/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <Protocol/EFIScm.h>
#include <Protocol/EFIQseecom.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/Debug.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include "ISecretKeeper.h"
#include "Secretkeeper.h"

#define CAppClient_UID 0x97U
// CBOR response header that needs to be ignored until the start of the
// COSE public key.
#define CosePubKeyDeserializeOffset 8

// Command ID for Secretkeeper process bootloader
#define SK_CMD_PROCESS_BOOTLOADER 1

typedef struct {
  UINT32 CmdId;
  UINT8 Request[4];
} __attribute__ ((packed)) SKProcessBootloaderReq;

typedef struct {
  UINT32 PubKeyLen;
  UINT8 PubKeyRsp[SK_MAX_PUB_KEY_SIZE];
} __attribute__ ((packed)) SKProcessBootloaderRes;

// Global handle to secretkeeper TA
static Object AppObj = Object_NULL;

// Load app through Qseecom
static EFI_STATUS
SecretkeeperLoadApp (VOID)
{
  EFI_STATUS Status = EFI_SUCCESS;

  QCOM_QSEECOM_PROTOCOL *QseeComProtocol;
  UINT32 AppId = 0;

  Status = gBS->LocateProtocol (&gQcomQseecomProtocolGuid, NULL,
                                (VOID **)&QseeComProtocol);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Unable to locate QSEECom protocol: %r\n",
           Status));
    return Status;
  }

  Status = QseeComProtocol->QseecomStartApp (
      QseeComProtocol, "secretkeeper_a", &AppId);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR,
            "SecretkeeperLoadApp: Could not load form _a, status: %r\n",
           Status));
    Status = QseeComProtocol->QseecomStartApp (
        QseeComProtocol, "secretkeeper_b", &AppId);
    if (Status != EFI_SUCCESS) {
      DEBUG ((EFI_D_ERROR,
            "SecretkeeperLoadApp: Could not load form _b, status: %r\n",
             Status));
    }
  }

  return Status;
}

EFI_STATUS
SecretkeeperStartApp (VOID)
{
  EFI_STATUS Status = EFI_SUCCESS;

  Object ClientEnvObj = Object_NULL;
  Object AppClientObj = Object_NULL;

  if (!Object_isNull (AppObj)) {
    return EFI_SUCCESS;
  }

  SecretkeeperLoadApp ();
  CONST CHAR8 *skAppName = "secretkeeper";

  // Get TA app handle through SMCInvoke
  QCOM_SCM_PROTOCOL *pQcomScmProtocol = NULL;
  // Locate QCOM_SCM_PROTOCOL.
  Status = gBS->LocateProtocol (&gQcomScmProtocolGuid, NULL,
                                (VOID **)&pQcomScmProtocol);
  if (Status != EFI_SUCCESS ||
      (pQcomScmProtocol == NULL)) {
    DEBUG ((EFI_D_ERROR,
            "SecretkeeperStartApp: Locate SCM Pcol failed, Status: (0x%x)\n",
            Status));
    Status = -1;
    return Status;
  }

  Status = pQcomScmProtocol->ScmGetClientEnv (pQcomScmProtocol, &ClientEnvObj);
  if (Object_isERROR (Status) ||
      Object_isNull (ClientEnvObj)) {
    DEBUG ((EFI_D_ERROR,
            "SecretkeeperStartApp: Failed to get Client Env, Status: (0x%x)\n",
            Status));
    goto out;
  }

  Status = IClientEnvOpen (ClientEnvObj, CAppClient_UID, &AppClientObj);
  if (Object_isERROR (Status) ||
      Object_isNull (AppClientObj)) {
    DEBUG ((EFI_D_ERROR,
            "SecretkeeperStartApp: Failed to get App Client, Status: (0x%x)\n",
            Status));
    goto out;
  }

  // Get already loaded secretkeeper TA handle from AppClient.
  Status = IAppClientGetAppObject (AppClientObj, skAppName,
                                   AsciiStrLen (skAppName), &AppObj);
  if (Object_isERROR (Status) ||
      Object_isNull (AppObj)) {
    DEBUG (
        (EFI_D_ERROR,
         "SecretkeeperStartApp: Failed to get App Object, Status: (0x%x)\n",
         Status));
    goto out;
  }

  DEBUG ((EFI_D_INFO, "Secretkeeper app is loaded and ready to be used\n"));

  Status = EFI_SUCCESS;
  goto out_success;

out:
  DEBUG ((EFI_D_ERROR, "Secretkeeper app is not loaded\n"));
  Object_ASSIGN_NULL (AppObj);

out_success:
  Object_ASSIGN_NULL (ClientEnvObj);
  Object_ASSIGN_NULL (AppClientObj);

  return Status;
}

/* Get public key from TA using QSEECOM interface */
STATIC EFI_STATUS
SecretkeeperGetCosePublicKeyQseecom (UINT8 *CosePubKey, UINT32 CosePubKeyLen,
                                     UINT32 *RspLenOut, UINT32 *Offset)
{
  EFI_STATUS Status = EFI_SUCCESS;
  QCOM_QSEECOM_PROTOCOL *QseeComProtocol = NULL;
  UINT32 AppId = 0;
  SKProcessBootloaderReq Req = {0};
  SKProcessBootloaderRes Rsp = {0};

  /* CBOR encoded request for GetIdentity Request */
  UINT8 Request[4] = {0x00, 0x00, 0x00, 0x1};

  /* Locate QSEECOM protocol */
  Status = gBS->LocateProtocol (&gQcomQseecomProtocolGuid, NULL,
                                (VOID **)&QseeComProtocol);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_VERBOSE, "Unable to locate QSEECom protocol: %r\n", Status));
    return Status;
  }

  /* Load secretkeeper TA */
  Status = QseeComProtocol->QseecomStartApp (
      QseeComProtocol, "secretkeeper_a", &AppId);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_VERBOSE,
            "SecretkeeperGetCosePublicKeyQseecom: Could not load form _a, status: %r\n",
            Status));
    Status = QseeComProtocol->QseecomStartApp (
        QseeComProtocol, "secretkeeper_b", &AppId);
    if (Status != EFI_SUCCESS) {
      DEBUG ((EFI_D_VERBOSE,
              "SecretkeeperGetCosePublicKeyQseecom: Could not load form _b, status: %r\n",
              Status));
      return Status;
    }
  }

  DEBUG ((EFI_D_INFO, "Secretkeeper app loaded via QSEECOM\n"));

  /* Prepare request */
  Req.CmdId = SK_CMD_PROCESS_BOOTLOADER;
  CopyMem (Req.Request, Request, sizeof (Request));

  /* Send command to TA */
  Status = QseeComProtocol->QseecomSendCmd (
      QseeComProtocol, AppId, (UINT8 *)&Req, sizeof (Req),
      (UINT8 *)&Rsp, sizeof (Rsp));

  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR,
            "SecretkeeperGetCosePublicKeyQseecom: QseecomSendCmd failed, status: %r\n",
            Status));
    return Status;
  }

  /* Validate response */
  if (Rsp.PubKeyLen == 0 || Rsp.PubKeyLen > SK_MAX_PUB_KEY_SIZE) {
    DEBUG ((EFI_D_ERROR,
            "SecretkeeperGetCosePublicKeyQseecom: Invalid PubKeyLen: %d\n",
            Rsp.PubKeyLen));
    return EFI_DEVICE_ERROR;
  }

  if (Rsp.PubKeyLen > CosePubKeyLen) {
    DEBUG ((EFI_D_ERROR,
            "SecretkeeperGetCosePublicKeyQseecom: Buffer too small. Need: %d, Have: %d\n",
            Rsp.PubKeyLen, CosePubKeyLen));
    return EFI_BUFFER_TOO_SMALL;
  }

  /* Copy response to output buffer */
  CopyMem (CosePubKey, Rsp.PubKeyRsp, Rsp.PubKeyLen);
  *RspLenOut = Rsp.PubKeyLen;
  *Offset = CosePubKeyDeserializeOffset;

  DEBUG ((EFI_D_INFO,
          "SecretkeeperGetCosePublicKeyQseecom: Success, PubKeyLen: %d\n",
          Rsp.PubKeyLen));

  return EFI_SUCCESS;
}

/* Get public key from TA using SMC invoke method */
STATIC EFI_STATUS
SecretkeeperGetCosePublicKeySmc (UINT8 *CosePubKey, UINT32 CosePubKeyLen,
                                 UINT32 *RspLenOut, UINT32 *Offset)
{
  EFI_STATUS Status = EFI_SUCCESS;
  /* CBOR encoded request for GetIdentity Request */
  UINT8 Request[4] = {0x00, 0x00, 0x00, 0x1};
  SizeT LenOut = 0;

  Status = SecretkeeperStartApp ();
  if ((Status != EFI_SUCCESS) ||
        Object_isNull (AppObj)) {
    DEBUG ((EFI_D_ERROR, "App Obj is NULL, or Status = 0x%x\n", Status));
    return EFI_NOT_FOUND;
  }

  Status = ISecretKeeper_process_bootloader (AppObj,
                                             Request, sizeof (Request),
                                             CosePubKey, CosePubKeyLen,
                                             &LenOut);
  if (Status) {
    DEBUG ((EFI_D_ERROR,
           "Failed to get public key from the TA via SMC: Status = 0x%x\n",
           Status));
    return Status;
  }

  *RspLenOut = LenOut;
  *Offset = CosePubKeyDeserializeOffset;

  DEBUG ((EFI_D_INFO,
          "SecretkeeperGetCosePublicKeySmc: Success, RspLen: %d\n",
          LenOut));

  return EFI_SUCCESS;
}

/* Get public key from TA - tries QSEECOM first, falls back to SMC */
EFI_STATUS
SecretkeeperGetCosePublicKey (UINT8 *CosePubKey, UINT32 CosePubKeyLen,
                              UINT32 *RspLenOut, UINT32 *Offset)
{
  EFI_STATUS Status = EFI_SUCCESS;

  if (CosePubKey == NULL || RspLenOut == NULL || Offset == NULL) {
    DEBUG ((EFI_D_ERROR, "SecretkeeperGetCosePublicKey: Invalid parameters\n"));
    return EFI_INVALID_PARAMETER;
  }

  /* Try QSEECOM interface first */
  DEBUG ((EFI_D_INFO, "SecretkeeperGetCosePublicKey: Trying QSEECOM interface\n"));
  Status = SecretkeeperGetCosePublicKeyQseecom (CosePubKey, CosePubKeyLen,
                                                RspLenOut, Offset);
  if (Status == EFI_SUCCESS) {
    DEBUG ((EFI_D_INFO, "SecretkeeperGetCosePublicKey: QSEECOM method succeeded\n"));
    return EFI_SUCCESS;
  }

  /* Fall back to SMC invoke method */
  DEBUG ((EFI_D_INFO,
          "SecretkeeperGetCosePublicKey: QSEECOM failed (%r), falling back to SMC\n",
          Status));
  Status = SecretkeeperGetCosePublicKeySmc (CosePubKey, CosePubKeyLen,
                                            RspLenOut, Offset);
  if (Status == EFI_SUCCESS) {
    DEBUG ((EFI_D_INFO, "SecretkeeperGetCosePublicKey: SMC method succeeded\n"));
    return EFI_SUCCESS;
  }

  DEBUG ((EFI_D_ERROR,
          "SecretkeeperGetCosePublicKey: Both QSEECOM and SMC methods failed\n"));
  return Status;
}
