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

#include <Library/DebugLib.h>
#include <Library/FdtRw.h>
#include <Library/HypervisorMvCalls.h>
#include <Library/LocateDeviceTree.h>
#include <Library/UefiBootServicesTableLib.h>
#include <libufdt_sysdeps.h>
#include <ufdt_overlay.h>

#include <Protocol/EFIDtFixup.h>
#include <Protocol/EFIRamPartition.h>

// Copied from UpdateDeviceTree
STATIC EFI_STATUS
UpdateRamPartitionsDeviceTree (VOID *Fdt)
{
  EFI_STATUS Status = EFI_SUCCESS;
  RamPartitionEntry *RamPartitions = NULL;
  UINT32 NumPartitions = 0;

  Status = ReadRamPartitions (&RamPartitions, &NumPartitions);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Error returned from ReadRamPartitions %r\n", Status));
    return Status;
  }

  RamPartitionEntry UpdatedRamPartitions[NUM_NOMAP_REGIONS];
  UINT32 NumUpdPartitions = 0;
  Status = GetUpdatedRamPartitions (Fdt, RamPartitions, NumPartitions,
                                    UpdatedRamPartitions, &NumUpdPartitions);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Error returned from GetUpdatedRamPartitions %r\n",
            Status));
    return Status;
  }

  INT32 offset = FdtPathOffset (Fdt, "/memory");
  if (offset < 0) {
    DEBUG ((EFI_D_ERROR, "ERROR: Could not find memory node ...\n"));
    return EFI_NOT_FOUND;
  }

  DEBUG ((EFI_D_INFO, "Final RAM Partitions\r\n"));
  for (int i = 0; i < NumUpdPartitions; i++) {
    DEBUG ((EFI_D_INFO, "Add Base: 0x%016lx Available Length: 0x%016lx \n",
            UpdatedRamPartitions[i].Base,
            UpdatedRamPartitions[i].AvailableLength));
    INT32 ret =
        dev_tree_add_mem_infoV64 (Fdt, offset, UpdatedRamPartitions[i].Base,
                                  UpdatedRamPartitions[i].AvailableLength);
    if (ret) {
      DEBUG ((EFI_D_ERROR, "Add Base: 0x%016lx Length: 0x%016lx Fail\n",
              UpdatedRamPartitions[i].Base,
              UpdatedRamPartitions[i].AvailableLength));
    }
  }

  if (RamPartitions) {
    FreePool (RamPartitions);
  }

  return EFI_SUCCESS;
}

STATIC EFI_STATUS
InitHypOverlays (BootParamlist *BootParamlistPtr, UINTN *HypOverlaysSize)
{
  EFI_STATUS Status = CheckAndSetVmData (BootParamlistPtr);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to CheckAndSetVmData %r\n", Status));
    return Status;
  }

  for (UINT32 i = 0; i < BootParamlistPtr->NumHypDtbos; i++) {
    if (!BootParamlistPtr->HypDtboBaseAddr[i] ||
        fdt_check_header ((VOID *)BootParamlistPtr->HypDtboBaseAddr[i])) {
      DEBUG ((EFI_D_ERROR,
              "HypInfo: Not overlaying hyp dtbo"
              "Dtbo :%d is null or Bad DT header\n",
              i));
      continue;
    }

    HypOverlaysSize += fdt_totalsize (BootParamlistPtr->HypDtboBaseAddr[i]);
  }

  return Status;
}

STATIC EFI_STATUS
ApplyHypOverlays (VOID *Fdt, BootParamlist *BootParamlistPtr)
{
  struct fdt_entry_node *DtsList = NULL;

  for (UINT32 i = 0; i < BootParamlistPtr->NumHypDtbos; i++) {
    if (!BootParamlistPtr->HypDtboBaseAddr[i] ||
        fdt_check_header ((VOID *)BootParamlistPtr->HypDtboBaseAddr[i])) {
      DEBUG ((EFI_D_ERROR,
              "HypInfo: Not overlaying hyp dtbo"
              "Dtbo :%d is null or Bad DT header\n",
              i));
      continue;
    }

    if (!AppendToDtList (
            &DtsList, (fdt64_t)BootParamlistPtr->HypDtboBaseAddr[i],
            fdt_totalsize (BootParamlistPtr->HypDtboBaseAddr[i]))) {
      DEBUG ((EFI_D_ERROR,
              "Unable to Allocate buffer for HypOverlay DT num: %d\n", i));
      DeleteDtList (&DtsList);
      return EFI_OUT_OF_RESOURCES;
    }
  }

  void *overlay_buffer = pre_overlay_malloc ();
  if (!overlay_buffer) {
    DEBUG ((EFI_D_ERROR, "ApplyOverlay: pre_overlay_malloc failed\n"));
    return EFI_NOT_FOUND;
  }

  struct fdt_header *FinalDtbHdr =
      ufdt_apply_multi_overlay (Fdt, fdt_totalsize (Fdt), DtsList);
  DeleteDtList (&DtsList);
  if (!FinalDtbHdr) {
    DEBUG ((EFI_D_ERROR, "ApplyOverlay: ufdt apply overlay failed\n"));
    post_overlay_free ();
    return EFI_NOT_FOUND;
  }

  gBS->CopyMem (Fdt, FinalDtbHdr, fdt_totalsize (FinalDtbHdr));
  post_overlay_free ();

  return EFI_SUCCESS;
}

// Minimal implementation of FixupDeviceTree to boot with GBL (Subset of
// UpdateDeviceTree). Fixups applied:
// 1. Hyp Overlays
// 2. Ram Partitions
EFI_STATUS
EFIAPI
MinimalBootFixup (IN EFI_DT_FIXUP_PROTOCOL *This,
                  IN OUT VOID *Fdt,
                  IN OUT UINTN *BufferSize,
                  IN UINT32 Flags)
{
  DEBUG ((EFI_D_INFO, "EFI_DT_FIXUP_PROTOCOL: MinimalBootFixup called\n"));
  BootParamlist BootParamlist = {0};
  UINTN HypOverlaysSize = 0;

  INT32 ret = fdt_check_header (Fdt) || fdt_check_header_ext (Fdt);
  if (ret) {
    DEBUG ((EFI_D_ERROR, "ERROR: Invalid device tree header ...\n"));
    return EFI_INVALID_PARAMETER;
  }

  // Init hyp overlays first to calculate required buffer size
  EFI_STATUS Status = InitHypOverlays (&BootParamlist, &HypOverlaysSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to InitHypOverlays %r\n", Status));
    return Status;
  }

  UINTN RequiredBufferSize =
      fdt_totalsize (Fdt) + DTB_PAD_SIZE + HypOverlaysSize;
  if (*BufferSize < RequiredBufferSize) {
    DEBUG ((EFI_D_ERROR,
            "ERROR: Don't have enough space to apply device tree fixups\n"));
    *BufferSize = RequiredBufferSize;
    return EFI_BUFFER_TOO_SMALL;
  }

  // Apply hyp overlays
  Status = ApplyHypOverlays (Fdt, &BootParamlist);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to ApplyHypOverlays %r\n", Status));
    return Status;
  }

  // Re-open to extend
  ret = fdt_open_into (Fdt, Fdt, *BufferSize);
  if (ret) {
    DEBUG ((EFI_D_ERROR, "ERROR: Failed to fdt_open_into\n"));
    return EFI_INVALID_PARAMETER;
  }

  // Apply ram partitions
  Status = UpdateRamPartitionsDeviceTree (Fdt);
  if (EFI_ERROR (Status)) {
    DEBUG (
        (EFI_D_ERROR, "Failed to UpdateRamPartitionsDeviceTree %r\n", Status));
    return Status;
  }

  // Update device tree total size
  fdt_pack (Fdt);

  return EFI_SUCCESS;
}

/**
  Minimal boot implementation of EFI_DT_FIXUP_PROTOCOL.
**/
EFI_DT_FIXUP_PROTOCOL gMinimalBootDtFixupProtocol = {
    EFI_DT_FIXUP_PROTOCOL_REVISION, MinimalBootFixup};

/**
  Installs the EFI_DT_FIXUP_PROTOCOL with the no-op implementation.
**/
VOID
InstallDtFixupProtocol (VOID)
{
  EFI_STATUS Status;

  Status = gBS->InstallProtocolInterface (
      &gImageHandle, &gEfiDtFixupProtocolGuid, EFI_NATIVE_INTERFACE,
      &gMinimalBootDtFixupProtocol);

  if (EFI_ERROR (Status)) {
    DEBUG (
        (DEBUG_INFO, "Failed to install EFI_DT_FIXUP_PROTOCOL: %r\n", Status));
  } else {
    DEBUG ((DEBUG_INFO, "EFI_DT_FIXUP_PROTOCOL installed successfully.\n"));
  }
}