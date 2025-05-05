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
 * GBL EFI OS Configuration Protocol
 *
 * This protocol provides a mechanism for the EFI firmware to update OS
 * configuration data:
 *
 * - Kernel command line
 * - Bootconfig
 * - Device tree
 *
 * GBL will load and verify the base data from disk, and then call these protocol
 * functions to give the firmware a chance to construct and adjust the data as needed
 * for the particular device.
 *
 * If no runtime modifications are necessary, this protocol may be left
 * unimplemented.
 */

#ifndef __EFI_GBL_OS_CONFIGURATION_PROTOCOL_H__
#define __EFI_GBL_OS_CONFIGURATION_PROTOCOL_H__

#include <Uefi.h>

/**
 * Revision Number
 *
 * Note: revision 0 means the protocol is not yet stable and may change in
 * backwards-incompatible ways.
 */
#define EFI_GBL_OS_CONFIGURATION_PROTOCOL_REVISION 0x0000000000010000

// {dda0d135-aa5b-42ff-85ac-e3ad6efb4619}
#define EFI_GBL_OS_CONFIGURATION_PROTOCOL_GUID \
  { \
    0xdda0d135, 0xaa5b, 0x42ff, { 0x85, 0xac, 0xe3, 0xad, 0x6e, 0xfb, 0x46, 0x19 } \
  }

extern EFI_GUID gEfiGblOsConfigurationProtocolGuid;

/**
  Enumeration of possible device tree component types.
**/
typedef enum {
  DEVICE_TREE,
  OVERLAY,
  PVM_DA_OVERLAY,
} EFI_GBL_DEVICE_TREE_TYPE;

/**
  Enumeration of possible device tree component sources.
**/
typedef enum {
  BOOT,
  VENDOR_BOOT,
  DTBO,
  DTB
} EFI_GBL_DEVICE_TREE_SOURCE;

/**
  Device tree metadata structure.
**/
typedef struct {
  UINT32 Source;    // EFI_GBL_DEVICE_TREE_SOURCE
  UINT32 Type;      // EFI_GBL_DEVICE_TREE_TYPE
  // Values are zeroed and must not be used in case of VENDOR_BOOT source
  UINT32 Id;
  UINT32 Rev;
  UINT32 Custom[4];
} EFI_GBL_DEVICE_TREE_METADATA;

/**
  Verified device tree structure.
**/
typedef struct {
  EFI_GBL_DEVICE_TREE_METADATA Metadata;
  // Device tree/overlay buffer, cannot be NULL, guaranteed to be 8-byte aligned
  CONST VOID *DeviceTree;
  // Indicates whether this device tree (or overlay) must be included in the
  // final device tree. Set to true by a FW if this component must be used
  BOOLEAN Selected;
} EFI_GBL_VERIFIED_DEVICE_TREE;

/**
  GBL EFI OS Configuration Protocol Interface.
**/
typedef struct _EFI_GBL_OS_CONFIGURATION_PROTOCOL EFI_GBL_OS_CONFIGURATION_PROTOCOL;

/**
  Provides runtime fixups to the kernel command line.

  @param[in]     This            A pointer to the EFI_GBL_OS_CONFIGURATION_PROTOCOL instance.
  @param[in]     CommandLine     A pointer to the ASCII null-terminated command line built by GBL.
  @param[out]    Fixup           Pointer to a pre-allocated buffer to store the generated command line fixup.
                                 GBL verifies and appends provided data into the final command line. Firmware may
                                 leave this unchanged if no fixup is required.

                                 The firmware implementation can generate a fixup with the following restrictions:
                                 - On return, the data must be valid ASCII encoding with null termination.
                                 - The data and termination byte must never exceed the provided FixupBufferSize.
                                 - No libavb arguments may be provided (see Security below).

  @param[in,out] FixupBufferSize On function call, this points to the fixup buffer size provided by Fixup.
                                 The implementation is free to provide fixup data up to this size, including the termination byte.

                                 If the buffer is not large enough to fit the fixups, the function should update
                                 FixupBufferSize with the required size and return EFI_BUFFER_TOO_SMALL; GBL will
                                 then allocate a larger buffer, discard all modifications, and repeat the
                                 FixupKernelCommandline call.

                                 FixupBufferSize does not need to be updated on success; GBL will determine the fixup command line data size via the null terminator.

  @retval EFI_SUCCESS            Command line fixup provided.
  @retval EFI_INVALID_PARAMETER  A parameter is invalid.
  @retval EFI_BUFFER_TOO_SMALL   The buffer is too small; FixupBufferSize has been updated with the required size.
  @retval EFI_DEVICE_ERROR       Internal error while providing the command line fixup.

  @note Ownership of all the parameters is loaned only for the duration of the function call and
        must not be retained by the protocol after returning.

  @par Security
  To ensure the integrity of verified boot data, this protocol will not allow appending any command line parameters provided by libavb.
  If any of these parameters are provided, GBL will treat this as a failed boot attempt:
  - androidboot.veritymode*
  - androidboot.vbmeta*
  - dm
  - root

  Additionally, all data used to apply fixups to the command line must be trusted.
  In particular, if the protocol loads any data from non-secure storage, it should
  verify that data before use.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_GBL_FIXUP_KERNEL_COMMAND_LINE)(
  IN EFI_GBL_OS_CONFIGURATION_PROTOCOL *This,
  IN CONST CHAR8 *CommandLine,
  OUT CHAR8 *Fixup,
  IN OUT UINTN *FixupBufferSize
  );

/**
  Provides runtime fixups to the bootconfig.

  @param[in]     This            A pointer to the EFI_GBL_OS_CONFIGURATION_PROTOCOL instance.
  @param[in]     BootConfig      Pointer to the bootconfig built by GBL. Trailing data isn't provided.
  @param[in]     BootConfigSize  Size of the bootconfig built by GBL.
  @param[out]    Fixup           Pointer to a pre-allocated buffer to store the generated bootconfig fixup.
                                 GBL verifies and appends provided data into the final bootconfig. Firmware may
                                 leave this unchanged if no fixup is required. FixupBufferSize must be updated to 0 in this case.

                                 The firmware implementation can generate a fixup with the following restrictions:
                                 - On return, the data must be valid bootconfig (trailer is optional).
                                 - The data must never exceed the provided FixupBufferSize.
                                 - No libavb arguments may be provided (see Security below).

  @param[in,out] FixupBufferSize On function call, this points to the fixup buffer size provided by Fixup.
                                 The implementation is free to provide fixup data up to this size.

                                 If the buffer is not large enough to fit the fixups, the function should update
                                 FixupBufferSize with the required size and return EFI_BUFFER_TOO_SMALL; GBL will
                                 then allocate a larger buffer, discard all modifications, and repeat the FixupBootConfig call.

                                 FixupBufferSize must be updated on success to let GBL determine the fixup data size.

  @retval EFI_SUCCESS            Boot configuration fixup provided.
  @retval EFI_INVALID_PARAMETER  A parameter is invalid.
  @retval EFI_BUFFER_TOO_SMALL   The buffer is too small; FixupBufferSize has been updated with the required size.
  @retval EFI_DEVICE_ERROR       Internal error while providing the bootconfig fixup.

  @note Ownership of all the parameters is loaned only for the duration of the function call and
        must not be retained by the protocol after returning.

  @par Security
  The security guidelines for this function are identical to those for FixupKernelCommandline; see those details.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_GBL_FIXUP_BOOTCONFIG)(
  IN EFI_GBL_OS_CONFIGURATION_PROTOCOL *This,
  IN CONST CHAR8 *BootConfig,
  IN UINTN BootConfigSize,
  OUT CHAR8 *Fixup,
  IN OUT UINTN *FixupBufferSize
  );

/**
  Inspects device trees and overlays loaded by GBL to determine which ones to use.

  @param[in]     This            A pointer to the EFI_GBL_OS_CONFIGURATION_PROTOCOL instance.
  @param[in,out] DeviceTrees     Pointer to an array of base device trees and overlays for selection.
                                 Base device trees and overlays are differentiated by the
                                 EFI_GBL_DEVICE_TREE_METADATA.Source field (BOOT, VENDOR_BOOT, DTB for base device trees,
                                 and DTBO for overlays).

                                 Selection is made by setting EFI_GBL_VERIFIED_DEVICE_TREE.Selected to TRUE.

                                 Setting Chosen to an incorrect FDT header or outside the provided DeviceTree will cause GBL
                                 to fail to boot.

  @param[in]     NumDeviceTrees  The number of base device trees and overlays in the DeviceTrees array.

  @retval EFI_SUCCESS            Base device tree and overlays have been selected.
  @retval EFI_INVALID_PARAMETER  A parameter is invalid (e.g., incorrect device trees, alignment).

  @note Ownership of all the parameters is loaned only for the duration of the function call and
        must not be retained by the protocol after returning.

  @par Description
  Android build artifacts provide multiple base device trees and overlays from the boot, vendor_boot, dtb, and dtbo partitions.
  These artifacts are reused across multiple SoCs, so the firmware typically selects a base device tree and overlays to construct the final tree.
  This method enables selection based on the loaded content.

  Only one base device tree and multiple overlays (no overlays is also allowed) can be selected.
  If more than one or no base device trees are selected, GBL will fail to boot.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_GBL_SELECT_DEVICE_TREES)(
  IN EFI_GBL_OS_CONFIGURATION_PROTOCOL *This,
  IN OUT EFI_GBL_VERIFIED_DEVICE_TREE *DeviceTrees,
  IN UINTN NumDeviceTrees
  );

/**
  GBL EFI OS Configuration Protocol structure.
**/
struct _EFI_GBL_OS_CONFIGURATION_PROTOCOL {
  UINT64 Revision;
  EFI_GBL_FIXUP_KERNEL_COMMAND_LINE FixupKernelCommandline;
  EFI_GBL_FIXUP_BOOTCONFIG FixupBootconfig;
  EFI_GBL_SELECT_DEVICE_TREES SelectDeviceTrees;
};

#endif  /* __EFI_GBL_OS_CONFIGURATION_PROTOCOL_H__ */
