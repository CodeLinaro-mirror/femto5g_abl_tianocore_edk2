/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <Protocol/EFIScm.h>
#include <Protocol/EFIQseecom.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/Debug.h>
#include <Library/BaseLib.h>
#include "ISecretKeeper.h"
#include "Secretkeeper.h"

#define CAppClient_UID 0x97U
// CBOR response header that needs to be ignored until the start of the
// COSE public key.
#define CosePubKeyDeserializeOffset 8

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

/* Get public key from TA */
EFI_STATUS
SecretkeeperGetCosePublicKey (UINT8 *CosePubKey, UINT32 CosePubKeyLen,
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
           "Failed to get public key from the TA: Status = 0x%x\n",
           Status));
  }

  *RspLenOut = LenOut;
  *Offset = CosePubKeyDeserializeOffset;

  return Status;
}
