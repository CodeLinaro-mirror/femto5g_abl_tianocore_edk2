/*
 * Copyright (C) 2025 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file
 * No-op implementation of EFI_DT_FIXUP_PROTOCOL.
 */

#include <Uefi.h>

#include "OEMPublicKey.h"
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BootLinux.h>
#include <Library/DebugLib.h>
#include <Library/DeviceInfo.h>
#include <Library/KeymasterClient.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/VerifiedBoot.h>

#include <Protocol/EFIGblAvbProtocol.h>

typedef struct {
  BOOLEAN IsUserKey;
  BOOLEAN IsMultiSlot;
  UINTN PublicKeyLen;
  CHAR8 PublicKey[MAX_USER_KEY_SIZE];
} AvbUserData;

STATIC AvbUserData UserData;

EFI_STATUS EFIAPI
MinimalBootAvbValidateVbmetaPublicKey (IN EFI_GBL_AVB_PROTOCOL *This,
                                       IN CONST UINT8 *PublicKeyData,
                                       IN UINTN PublicKeyLength,
                                       IN CONST UINT8 *PublicKeyMetadata,
                                       IN UINTN PublicKeyMetadataLength,
                                       OUT UINT32 *ValidationStatus)
{
  DEBUG (
      (EFI_D_INFO,
       "EFI_GBL_AVB_PROTOCOL: MinimalBootAvbValidateVbmetaPublicKey called\n"));

  CHAR8 *UserKeyBuffer = NULL;
  UINT32 UserKeyLength = 0;

  if (ValidationStatus == NULL || PublicKeyData == NULL) {
    DEBUG ((EFI_D_ERROR, "EFI_GBL_AVB_PROTOCOL: Invalid parameters\n"));
    *ValidationStatus = INVALID;
    return EFI_SUCCESS;
  }

  EFI_STATUS Status = GetUserKey (&UserKeyBuffer, &UserKeyLength);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "EFI_GBL_AVB_PROTOCOL: GetUserKey failed!, %r\n",
            Status));
    *ValidationStatus = INVALID;
    return EFI_SUCCESS;
  }

  UserData.IsUserKey = FALSE;
  if (PublicKeyLength == UserKeyLength &&
      CompareMem (PublicKeyData, UserKeyBuffer, PublicKeyLength) == 0) {
    *ValidationStatus = VALID_CUSTOM_KEY;
    DEBUG ((EFI_D_INFO, "EFI_GBL_AVB_PROTOCOL: VALID_CUSTOM_KEY\n"));
    UserData.IsUserKey = TRUE;
  } else if (PublicKeyLength == ARRAY_SIZE (OEMPublicKey) &&
             CompareMem (PublicKeyData, OEMPublicKey, PublicKeyLength) == 0) {
    *ValidationStatus = VALID;
    DEBUG ((EFI_D_INFO, "EFI_GBL_AVB_PROTOCOL: VALID\n"));
  } else {
    *ValidationStatus = INVALID;
    DEBUG ((EFI_D_INFO, "EFI_GBL_AVB_PROTOCOL: INVALID\n"));
    SetMem (UserData.PublicKey, ARRAY_SIZE (UserData.PublicKey), 0);
    UserData.PublicKeyLen = 0;
  }

  if (*ValidationStatus == VALID || *ValidationStatus == VALID_CUSTOM_KEY) {
    CopyMem (UserData.PublicKey, PublicKeyData, PublicKeyLength);
    UserData.PublicKeyLen = PublicKeyLength;
  }

  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
MinimalBootAvbReadIsDeviceUnlocked (IN EFI_GBL_AVB_PROTOCOL *This,
                                    OUT BOOLEAN *Unlocked)
{
  DEBUG ((EFI_D_INFO,
          "EFI_GBL_AVB_PROTOCOL: MinimalBootAvbReadIsDeviceUnlocked called\n"));

  *Unlocked = IsUnlocked ();

  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
MinimalBootAvbReadRollbackIndex (IN EFI_GBL_AVB_PROTOCOL *This,
                                 IN UINTN IndexLocation,
                                 OUT UINT64 *RollbackIndex)
{
  DEBUG ((EFI_D_INFO,
          "EFI_GBL_AVB_PROTOCOL: MinimalBootAvbReadRollbackIndex called\n"));

  EFI_STATUS Status = ReadRollbackIndex (IndexLocation, RollbackIndex);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR,
            "EFI_GBL_AVB_PROTOCOL: ReadRollbackIndex failed!, %r\n", Status));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
MinimalBootAvbWriteRollbackIndex (IN EFI_GBL_AVB_PROTOCOL *This,
                                  IN UINTN IndexLocation,
                                  IN UINT64 RollbackIndex)
{
  DEBUG ((EFI_D_INFO,
          "EFI_GBL_AVB_PROTOCOL: MinimalBootAvbWriteRollbackIndex called\n"));

  EFI_STATUS Status = WriteRollbackIndex (IndexLocation, RollbackIndex);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR,
            "EFI_GBL_AVB_PROTOCOL: WriteRollbackIndex failed! %r\n", Status));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
MinimalBootAvbHandleVerificationResult (
    IN EFI_GBL_AVB_PROTOCOL *This,
    IN CONST EFI_GBL_AVB_VERIFICATION_RESULT *VerificationResult)
{
  DEBUG ((EFI_D_INFO,
          "EFI_GBL_AVB_PROTOCOL: MinimalBootAvbHandleVerificationResult called "
          "digest: %a\n",
          VerificationResult->Digest));

  EFI_STATUS Status;
  KMRotAndBootState Data = {0};

  switch (VerificationResult->Color) {
  case EFI_GBL_AVB_BOOT_STATE_GREEN:
    Data.Color = GREEN;
    break;
  case EFI_GBL_AVB_BOOT_STATE_ORANGE:
    Data.Color = ORANGE;
    break;
  case EFI_GBL_AVB_BOOT_STATE_YELLOW:
    Data.Color = YELLOW;
    break;
  case EFI_GBL_AVB_BOOT_STATE_RED:
    Data.Color = RED;
    break;
  }
  Data.IsUnlocked = IsUnlocked ();

  if (VerificationResult->BootVersion != NULL &&
      VerificationResult->BootSecurityPatch != NULL &&
      VerificationResult->Digest != NULL) {
    Data.SystemVersion =
        ParseFooterOsVersion (VerificationResult->BootVersion,
                              AsciiStrLen (VerificationResult->BootVersion));
    Data.SystemSecurityLevel = ParseFooterSecPatch (
        VerificationResult->BootSecurityPatch,
        AsciiStrLen (VerificationResult->BootSecurityPatch));

    DEBUG ((EFI_D_INFO,
            "EFI_GBL_AVB_PROTOCOL: VerificationResult->BootVersion: %s\n",
            VerificationResult->BootVersion));
    DEBUG ((EFI_D_INFO,
            "EFI_GBL_AVB_PROTOCOL: VerificationResult->BootSecurityPatch: %s\n",
            VerificationResult->BootSecurityPatch));

    Data.PublicKey = UserData.PublicKey;
    Data.PublicKeyLength = UserData.PublicKeyLen;

    Status = KeyMasterSetRotAndBootState (&Data);
    if (Status != EFI_SUCCESS) {
      DEBUG (
          (EFI_D_ERROR,
           "EFI_GBL_AVB_PROTOCOL: Failed to KeyMasterSetRotAndBootState: %r\n",
           Status));
      return Status;
    }

    if (VerificationResult->Digest != NULL) {
      // QCOM API requires 32 bytes VHB even if vbmeta has 64 bit one.
      Status = SetVerifiedBootHash (VerificationResult->Digest, 32);
      if (Status != EFI_SUCCESS) {
        DEBUG ((EFI_D_ERROR,
                "EFI_GBL_AVB_PROTOCOL: Failed to SetVerifiedBootHash: %r\n",
                Status));
        return Status;
      }
    }
  }

  BootInfo Info = {0};
  Info.BootState = Data.Color;
  DisplayVerifiedBootScreen (&Info);

  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
MinimalBootReadPersistentValue (IN EFI_GBL_AVB_PROTOCOL *This,
                                IN CONST CHAR8 *Name,
                                OUT UINT8 *Value,
                                IN OUT UINTN *ValueSize)
{
  DEBUG ((EFI_D_INFO,
          "EFI_GBL_AVB_PROTOCOL: MinimalBootReadPersistentValue called\n"));
  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
MinimalBootWritePersistentValue (IN EFI_GBL_AVB_PROTOCOL *This,
                                 IN CONST CHAR8 *Name,
                                 IN CONST UINT8 *Value,
                                 IN UINTN ValueSize)
{
  DEBUG ((EFI_D_INFO,
          "EFI_GBL_AVB_PROTOCOL: MinimalBootWritePersistentValue called\n"));
  return EFI_SUCCESS;
}

// Define the protocol instance with the no-op implementations
EFI_GBL_AVB_PROTOCOL gMinimalBootAvbProtocol = {
    EFI_GBL_AVB_PROTOCOL_REVISION,      MinimalBootAvbValidateVbmetaPublicKey,
    MinimalBootAvbReadIsDeviceUnlocked, MinimalBootAvbReadRollbackIndex,
    MinimalBootAvbWriteRollbackIndex,   MinimalBootReadPersistentValue,
    MinimalBootWritePersistentValue,    MinimalBootAvbHandleVerificationResult};

VOID
InstallGblAvbProtocol ()
{
  EFI_STATUS Status = gBS->InstallProtocolInterface (
      &gImageHandle, &gEfiGblAvbProtocolGuid, EFI_NATIVE_INTERFACE,
      &gMinimalBootAvbProtocol);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "Failed to install GBL OS Configuration Protocol: %r\n",
            Status));
  } else {
    DEBUG ((DEBUG_INFO,
            "GBL OS Configuration Protocol installed successfully.\n"));
  }
}
