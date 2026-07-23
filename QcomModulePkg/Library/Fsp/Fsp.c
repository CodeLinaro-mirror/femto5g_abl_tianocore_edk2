/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <Protocol/EFIScm.h>
#include <Protocol/EFIQseecom.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/SmciInvokeUtils.h>
#include "IFSPApp.h"
#include "Fsp.h"

#define CAppClient_UID  0x97U

#define FSP_WRAP_KEY_IS_ENCRYPTED  1U

STATIC Object gAppObj = Object_NULL;

/**
  Load the FSP TA from the pkcs11 (or pkcs11_b) partition via QSEECom.

  It locates the QSEECom protocol and calls QseecomStartApp, falling back to the "_b" slot
  on failure.

  @retval EFI_SUCCESS           TA loaded successfully.
  @retval EFI_NOT_FOUND         QSEECom protocol not present in the system.
  @retval EFI_PROTOCOL_ERROR    Protocol pointer or function pointer is NULL.
  @retval Other                 Error returned by QseecomStartApp.
**/
STATIC
EFI_STATUS
FspLoadApp (
  VOID
  )
{
  EFI_STATUS             Status          = EFI_DEVICE_ERROR;
  QCOM_QSEECOM_PROTOCOL *QseeComProtocol = NULL;
  UINT32                 AppId           = 0;

  Status = gBS->LocateProtocol (
                  &gQcomQseecomProtocolGuid,
                  NULL,
                  (VOID **)&QseeComProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR,
            "FspLoadApp: Unable to locate QSEECom protocol: %r\n", Status));
    return Status;
  }

  if ((QseeComProtocol == NULL) ||
      (QseeComProtocol->QseecomStartApp == NULL)) {
    DEBUG ((EFI_D_ERROR,
            "FspLoadApp: QSEECom protocol or QseecomStartApp is NULL\n"));
    return EFI_PROTOCOL_ERROR;
  }

  /* Attempt primary partition slot */
  Status = QseeComProtocol->QseecomStartApp (
                              QseeComProtocol,
                              "pkcs11",
                              &AppId
                              );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR,
            "FspLoadApp: Could not load from pkcs11 partition: %r\n", Status));

    /* Fallback to the _b slot */
    Status = QseeComProtocol->QseecomStartApp (
                                QseeComProtocol,
                                "pkcs11_b",
                                &AppId
                                );
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR,
              "FspLoadApp: Could not load from pkcs11_b partition: %r\n",
              Status));
    }
  }

  return Status;
}

/**
  Load the FSP TA and acquire its SMCInvoke Object handle

  Steps:
    1. Calls FspLoadApp() to load the TA from the pkcs11 partition
    2. Locates the SCM protocol
    3. Calls ScmGetClientEnv / IClientEnvOpen / IAppClientGetAppObject
       to obtain the TA Object handle stored in the module-level gAppObj

  gAppObj is cached and in subsequent calls return EFI_SUCCESS immediately
  without repeating the load sequence

  @retval EFI_SUCCESS           FSP TA loaded and gAppObj acquired
  @retval EFI_PROTOCOL_ERROR    SCM protocol not available or NULL
  @retval Other                 Error during TA loading or handle acquisition
**/
STATIC
EFI_STATUS
FspStartAppSmc (
  VOID
  )
{
  EFI_STATUS          Status           = EFI_DEVICE_ERROR;
  INT32               ObjectStatus     = 0;
  CONST CHAR8        *FspAppName       = "fspapp";
  Object              ClientEnvObj     = Object_NULL;
  Object              AppClientObj     = Object_NULL;
  QCOM_SCM_PROTOCOL  *pQcomScmProtocol = NULL;

  if (!Object_isNull (gAppObj)) {
    DEBUG ((EFI_D_INFO,
            "FspStartAppSmc: FSP TA is already loaded\n"));
    return EFI_SUCCESS;
  }

  Status = FspLoadApp ();
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR,
            "FspStartAppSmc: FspLoadApp failed: %r\n", Status));
    return Status;
  }

  Status = gBS->LocateProtocol (
                  &gQcomScmProtocolGuid,
                  NULL,
                  (VOID **)&pQcomScmProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR,
            "FspStartAppSmc: Locate SCM Protocol failed, Status: (0x%x)\n",
            Status));
    return Status;
  }

  if ((pQcomScmProtocol == NULL) ||
      (pQcomScmProtocol->ScmGetClientEnv == NULL)) {
    DEBUG ((EFI_D_ERROR,
            "FspStartAppSmc: SCM Protocol or ScmGetClientEnv is NULL\n"));
    return EFI_PROTOCOL_ERROR;
  }

  ObjectStatus = pQcomScmProtocol->ScmGetClientEnv (pQcomScmProtocol,
                                                   &ClientEnvObj);
  if (Object_isERROR (ObjectStatus) || Object_isNull (ClientEnvObj)) {
    DEBUG ((EFI_D_ERROR,
            "FspStartAppSmc: Failed to get Client Env, Status: (0x%x)\n",
            ObjectStatus));
    Status = EFI_PROTOCOL_ERROR;
    goto out;
  }

  ObjectStatus = IClientEnvOpen (ClientEnvObj, CAppClient_UID, &AppClientObj);
  if (Object_isERROR (ObjectStatus) || Object_isNull (AppClientObj)) {
    DEBUG ((EFI_D_ERROR,
            "FspStartAppSmc: Failed to get App Client, Status: (0x%x)\n",
            ObjectStatus));
    Status = EFI_PROTOCOL_ERROR;
    goto out;
  }

  ObjectStatus = IAppClientGetAppObject (
                   AppClientObj,
                   FspAppName,
                   AsciiStrLen (FspAppName),
                   &gAppObj
                   );
  if (Object_isERROR (ObjectStatus) || Object_isNull (gAppObj)) {
    DEBUG ((EFI_D_ERROR,
            "FspStartAppSmc: Failed to get App Object, Status: (0x%x)\n",
            ObjectStatus));
    Status = EFI_PROTOCOL_ERROR;
    goto out;
  }

  DEBUG ((EFI_D_INFO,
          "FspStartAppSmc: FSP app is loaded and ready\n"));

  Status = EFI_SUCCESS;
  goto out_success;

out:
  DEBUG ((EFI_D_ERROR, "FspStartAppSmc: FSP app is not loaded\n"));
  Object_ASSIGN_NULL (gAppObj);

out_success:
  Object_ASSIGN_NULL (ClientEnvObj);
  Object_ASSIGN_NULL (AppClientObj);

  return Status;
}

/**
  Unwrap a wrapped key blob

  Steps:
    1. Populates IFSPAPP_WrapKeyBlob with:
         feature_id   = FSP_WRAP_KEY_FEATURE_ID
         is_encrypted = FSP_WRAP_KEY_IS_ENCRYPTED
         wrapkey[]    = caller-supplied WrapKey bytes
         wrapkey_size = WrapKeyLength
    2. Calls IFSPApp_unwrap_wrapped_key(): Normal World proxy that issues
       an SMC into TrustZone

  @param[in]  WrapKey         Pointer to the wrapped key material
  @param[in]  WrapKeyLength   Length in bytes
  @param[out] CekKey          Output buffer for the unwrapped CEK key
  @param[in]  CekKeySize      Size of CekKey buffer in bytes
  @param[out] CekKeyLenOut    Receives the number of bytes written to CekKey

  @retval EFI_SUCCESS           Key unwrapped successfully
  @retval EFI_INVALID_PARAMETER WrapKeyLength exceeds the maximum blob size
  @retval EFI_PROTOCOL_ERROR    SCM protocol not available or NULL
  @retval EFI_DEVICE_ERROR      TA invocation returned a non-zero error code
  @retval Other                 Error during TA loading or handle acquisition
**/
STATIC
EFI_STATUS
FspUnwrapWrappedKeySmc (
  IN  CONST UINT8  *WrapKey,
  IN  UINT64        WrapKeyLength,
  OUT VOID         *CekKey,
  IN  UINTN         CekKeySize,
  OUT UINTN        *CekKeyLenOut
  )
{
  IFSPAPP_WrapKeyBlob WrappedKeyBlob;
  INT32               TaResult;
  UINTN               CekKeyLenOutLocal = 0;

  if (Object_isNull (gAppObj)) {
    DEBUG ((EFI_D_ERROR,
            "FspUnwrapWrappedKeySmc: gAppObj is NULL\n"));
    return EFI_NOT_READY;
  }

  if (WrapKeyLength > sizeof (WrappedKeyBlob.wrapkey)) {
    DEBUG ((EFI_D_ERROR,
            "FspUnwrapWrappedKeySmc: WrapKeyLength (%lu) exceeds "
            "maximum (%u bytes)\n",
            WrapKeyLength, (UINT32)sizeof (WrappedKeyBlob.wrapkey)));
    return EFI_INVALID_PARAMETER;
  }

  SetMem (&WrappedKeyBlob, sizeof (WrappedKeyBlob), 0);
  WrappedKeyBlob.feature_id   = IFSPApp_FSP_WRAP_KEY_FEATURE_ID;
  WrappedKeyBlob.is_encrypted = FSP_WRAP_KEY_IS_ENCRYPTED;
  WrappedKeyBlob.wrapkey_size = WrapKeyLength;
  CopyMem (WrappedKeyBlob.wrapkey, WrapKey, (UINTN)WrapKeyLength);

  TaResult = IFSPApp_unwrap_wrapped_key (
               gAppObj,
               &WrappedKeyBlob,
               CekKey,
               CekKeySize,
               &CekKeyLenOutLocal
               );
  SetMem (&WrappedKeyBlob, sizeof (WrappedKeyBlob), 0);
  if (Object_isERROR (TaResult)) {
    DEBUG ((EFI_D_ERROR,
            "FspUnwrapWrappedKeySmc: IFSPApp_unwrap_wrapped_key "
            "failed with TA error: %d\n", TaResult));
    return EFI_DEVICE_ERROR;
  }

  *CekKeyLenOut = CekKeyLenOutLocal;

  DEBUG ((EFI_D_INFO,
          "FspUnwrapWrappedKeySmc: Key unwrapped successfully. "
          "CekKeyLen: %u bytes\n", (UINT32)CekKeyLenOutLocal));

  return EFI_SUCCESS;
}

EFI_STATUS
FspLoadTa (
  VOID
  )
{
  EFI_STATUS Status = EFI_DEVICE_ERROR;

  Status = FspStartAppSmc ();
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR,
            "FspLoadTa: Failed to load FSP TA: %r\n", Status));
    return Status;
  }

  DEBUG ((EFI_D_INFO, "FspLoadTa: FSP TA loaded successfully\n"));
  return EFI_SUCCESS;
}

EFI_STATUS
FspUnwrapWrappedKey (
  IN  CONST UINT8  *WrapKey,
  IN  UINT64        WrapKeyLength,
  OUT VOID         *CekKey,
  IN  UINTN         CekKeySize,
  OUT UINTN        *CekKeyLenOut
  )
{
  EFI_STATUS Status = EFI_DEVICE_ERROR;

  if ((WrapKey == NULL) ||
      (WrapKeyLength == 0) ||
      (CekKey == NULL) ||
      (CekKeySize == 0) ||
      (CekKeyLenOut == NULL) ||
      (WrapKeyLength > FSP_WRAP_KEY_MAX_SIZE) ||
      (CekKeySize > FSP_CEK_KEY_MAX_SIZE)) {
    DEBUG ((EFI_D_ERROR,
            "FspUnwrapWrappedKey: Invalid input parameters\n"));
    return EFI_INVALID_PARAMETER;
  }

  Status = FspUnwrapWrappedKeySmc (WrapKey, WrapKeyLength,
                                   CekKey, CekKeySize, CekKeyLenOut);
  if (Status == EFI_SUCCESS) {
    DEBUG ((EFI_D_INFO, "FspUnwrapWrappedKey: Succeeded\n"));
    return EFI_SUCCESS;
  }
  DEBUG ((EFI_D_ERROR,
          "FspUnwrapWrappedKey: Failed: %r\n",
          Status));
  return Status;
}
