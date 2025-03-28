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

// This is a custom protocol introduced by GBL.
// See gbl/docs/gbl_efi_avb_protocol.md for details.

#ifndef __EFI_GBL_AVB_PROTOCOL_H__
#define __EFI_GBL_AVB_PROTOCOL_H__

#include <Uefi.h>

/**
 * Revision Number
 *
 * Note: revision 0 means the protocol is not yet stable and may change in
 * backwards-incompatible ways.
 */
#define EFI_GBL_AVB_PROTOCOL_REVISION 0x0000000000010000

//
// Protocol GUID
//
// 6bc66b9a-d5c9-4c02-9da9-50af198d912c
//
#define EFI_GBL_AVB_PROTOCOL_GUID                                              \
  {0x6bc66b9a, 0xd5c9, 0x4c02, {0x9d, 0xa9, 0x50, 0xaf, 0x19, 0x8d, 0x91, 0x2c}}

extern EFI_GUID gEfiGblAvbProtocolGuid;

/**
  EFI GBL AVB Protocol Interface.
**/
typedef struct _EFI_GBL_AVB_PROTOCOL EFI_GBL_AVB_PROTOCOL;

//
// EFI_GBL_AVB_BOOT_STATE_COLOR
//
// OS boot state color.
//
// https://source.android.com/docs/security/features/verifiedboot/boot-flow#communicating-verified-boot-state-to-users
//
typedef enum {
  EFI_GBL_AVB_BOOT_STATE_GREEN,
  EFI_GBL_AVB_BOOT_STATE_YELLOW,
  EFI_GBL_AVB_BOOT_STATE_ORANGE,
  EFI_GBL_AVB_BOOT_STATE_RED_EIO,
  EFI_GBL_AVB_BOOT_STATE_RED,
} EFI_GBL_AVB_BOOT_STATE_COLOR;

//
// EFI_GBL_AVB_KEY_VALIDATION_STATUS
//
// Vbmeta key validation status.
//
// https://source.android.com/docs/security/features/verifiedboot/boot-flow#locked-devices-with-custom-root-of-trust
//
typedef enum {
  VALID,
  VALID_CUSTOM_KEY,
  INVALID,
} EFI_GBL_AVB_KEY_VALIDATION_STATUS;

//
// EFI_GBL_AVB_VERIFICATION_RESULT
//
// Contains verification result and OS version/security patch information.
//
typedef struct {
  UINT32 Color; // EFI_GBL_AVB_BOOT_STATE_COLOR

  // Pointer to zero terminated digest calculated by libavb.
  CONST CHAR8 *Digest;

  // Pointers to zero-terminated OS versions and security patches for different
  // boot components. NULL is provided if the value isn't present in the boot
  // artifacts or in case of a fatal AVB failure.
  // https://source.android.com/docs/core/architecture/bootloader/version-info-avb
  CONST CHAR8 *BootVersion;
  CONST CHAR8 *BootSecurityPatch;
  CONST CHAR8 *SystemVersion;
  CONST CHAR8 *SystemSecurityPatch;
  CONST CHAR8 *VendorVersion;
  CONST CHAR8 *VendorSecurityPatch;
} EFI_GBL_AVB_VERIFICATION_RESULT;

//
// EFI_GBL_AVB_VALIDATE_VBMETA_PUBLIC_KEY
//
// Prototype for the ValidateVbmetaPublicKey function.
//
typedef EFI_STATUS (EFIAPI *EFI_GBL_AVB_VALIDATE_VBMETA_PUBLIC_KEY) (
    IN EFI_GBL_AVB_PROTOCOL *This,
    IN CONST UINT8 *PublicKeyData,
    IN UINTN PublicKeyLength,
    IN CONST UINT8 *PublicKeyMetadata,
    IN UINTN PublicKeyMetadataLength,
    /* GBL_EFI_AVB_KEY_VALIDATION_STATUS */ OUT UINT32 *ValidationStatus);

//
// EFI_GBL_AVB_READ_IS_DEVICE_UNLOCKED
//
// Prototype for the ReadIsDeviceUnlocked function.
//
typedef EFI_STATUS (EFIAPI *EFI_GBL_AVB_READ_IS_DEVICE_UNLOCKED) (
    IN EFI_GBL_AVB_PROTOCOL *This,
    OUT BOOLEAN *IsUnlocked);

//
// EFI_GBL_AVB_READ_ROLLBACK_INDEX
//
// Prototype for the ReadRollbackIndex function.
//
typedef EFI_STATUS (EFIAPI *EFI_GBL_AVB_READ_ROLLBACK_INDEX) (
    IN EFI_GBL_AVB_PROTOCOL *This,
    IN UINTN IndexLocation,
    OUT UINT64 *RollbackIndex);

//
// EFI_GBL_AVB_WRITE_ROLLBACK_INDEX
//
// Prototype for the WriteRollbackIndex function.
//
typedef EFI_STATUS (EFIAPI *EFI_GBL_AVB_WRITE_ROLLBACK_INDEX) (
    IN EFI_GBL_AVB_PROTOCOL *This,
    IN UINTN IndexLocation,
    IN UINT64 RollbackIndex);

//
// EFI_GBL_AVB_HANDLE_VERIFICATION_RESULT
//
// Prototype for the HandleVerificationResult function.
//
typedef EFI_STATUS (EFIAPI *EFI_GBL_AVB_HANDLE_VERIFICATION_RESULT) (
    IN EFI_GBL_AVB_PROTOCOL *This,
    IN CONST EFI_GBL_AVB_VERIFICATION_RESULT *Result);

//
// EFI_GBL_AVB_READ_PERSISTENT_VALUE
//
// Prototype for the ReadPersistentValue function.
//
typedef EFI_STATUS (EFIAPI *EFI_GBL_AVB_READ_PERSISTENT_VALUE) (
    IN EFI_GBL_AVB_PROTOCOL *This,
    IN CONST CHAR8 *Name,
    OUT UINT8 *Value,
    IN OUT UINTN *ValueSize);

//
// EFI_GBL_AVB_WRITE_PERSISTENT_VALUE
//
// Prototype for the WritePersistentValue function.
//
typedef EFI_STATUS (EFIAPI *EFI_GBL_AVB_WRITE_PERSISTENT_VALUE) (
    IN EFI_GBL_AVB_PROTOCOL *This,
    IN CONST CHAR8 *Name,
    IN CONST UINT8 *Value,
    IN UINTN ValueSize);

//
// EFI_GBL_AVB_PROTOCOL
//
// GBL EFI AVB Protocol structure.
//
typedef struct _EFI_GBL_AVB_PROTOCOL {
  UINT64 Revision;
  EFI_GBL_AVB_VALIDATE_VBMETA_PUBLIC_KEY ValidateVbmetaPublicKey;
  EFI_GBL_AVB_READ_IS_DEVICE_UNLOCKED ReadIsDeviceUnlocked;
  EFI_GBL_AVB_READ_ROLLBACK_INDEX ReadRollbackIndex;
  EFI_GBL_AVB_WRITE_ROLLBACK_INDEX WriteRollbackIndex;
  EFI_GBL_AVB_READ_PERSISTENT_VALUE ReadPersistentValue;
  EFI_GBL_AVB_WRITE_PERSISTENT_VALUE WritePersistentValue;
  EFI_GBL_AVB_HANDLE_VERIFICATION_RESULT HandleVerificationResult;
} EFI_GBL_AVB_PROTOCOL;

#endif /* __EFI_GBL_AVB_PROTOCOL_H__ */