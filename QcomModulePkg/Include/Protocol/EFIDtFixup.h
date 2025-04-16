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
 * EFI Device Tree Fixup Protocol
 *
 * This protocol provides a mechanism for the EFI firmware to fix up the
 * Device Tree Blob (DTB) before passing it to the operating system.
 *
 * This is a protocol proposed by U-Boot and being used by the Kernel UEFI stub.
 * For more details, see:
 * https://github.com/U-Boot-EFI/EFI_DT_FIXUP_PROTOCOL
 */

#ifndef __EFI_DT_FIXUP_PROTOCOL_H__
#define __EFI_DT_FIXUP_PROTOCOL_H__

#include <Uefi.h>

#define EFI_DT_FIXUP_PROTOCOL_REVISION 0x00010000

/** @ingroup efi_dt_fixup_protocol */
#define EFI_DT_FIXUP_PROTOCOL_GUID \
  { \
    0xe617d64c, 0xfe08, 0x46da, { 0xf4, 0xdc, 0xbb, 0xd5, 0x87, 0x0c, 0x73, 0x00 } \
  }

extern EFI_GUID gEfiDtFixupProtocolGuid;

/**
  EFI Device Tree Fixup Protocol Interface.
**/
typedef struct _EFI_DT_FIXUP_PROTOCOL EFI_DT_FIXUP_PROTOCOL;

/**
  Fix up the Device Tree Blob (DTB).

  This function allows the firmware to apply necessary modifications to the DTB
  before it is handed off to the operating system. Modifications can include
  adding, removing, or altering nodes and properties within the device tree.

  @param[in]      This        Pointer to the EFI_DT_FIXUP_PROTOCOL instance.
  @param[in, out] Fdt         Pointer to the base of the DTB to fix up.
  @param[in, out] BufferSize  On input, size of the DTB buffer. On output,
                              size of the modified DTB.
  @param[in]      Flags       For additional device tree handling on the FW side.

  @retval EFI_SUCCESS           The DTB was successfully fixed up.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_BUFFER_TOO_SMALL  The buffer is too small to hold the modified DTB;
                                BufferSize has been updated with the required size.
  @retval EFI_UNSUPPORTED       The fixup operation is not supported.
  @retval EFI_DEVICE_ERROR      An error occurred during the fixup process.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DT_FIXUP)(
  IN EFI_DT_FIXUP_PROTOCOL *This,
  IN OUT VOID              *Fdt,
  IN OUT UINTN             *BufferSize,
  IN UINT32                Flags
  );

/**
  EFI Device Tree Fixup Protocol structure.
**/
struct _EFI_DT_FIXUP_PROTOCOL {
  UINT64       Revision;
  EFI_DT_FIXUP Fixup;
};

#endif /* __EFI_DT_FIXUP_PROTOCOL_H__ */