/*
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of The Linux Foundation nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "PartitionTableUpdate.h"
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/Debug.h>
#include <Library/LinuxLoaderLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ShutdownServices.h>
#include <Library/UefiBootServicesTableLib.h>

#ifdef ENABLE_SPINOR_SLOT_SYNC

/* SPINOR device root VENDOR-GUID; GetSpinorBlockIo() matches on it at runtime. */
extern EFI_GUID gEfiSpiNor0Guid;

/* XBL boot TYPE-GUID anchor: the _a/_b entry carrying it names the active slot. */
extern EFI_GUID gSpinorXblBootGuid;

/* Partition-entry byte offset of the 36 x CHAR16 partition name.
 * TypeGUID@0(16) UniqueGUID@16(16) StartLBA@32(8) EndLBA@40(8) Attr@48(8)
 * Name@56 (see MdePkg/Include/Uefi/UefiGpt.h EFI_PARTITION_ENTRY). */
#define SPINOR_PART_NAME_OFFSET 56

/* Number of CHAR16 in a GPT partition name field (72 bytes / 2). */
#define SPINOR_PART_NAME_CHARS  (MAX_GPT_NAME_SIZE / 2)

/* ======================================================================== *
 *  Small string / entry helpers                                            *
 * ======================================================================== */

/* Returns the trailing slot char (`L'a'`/`L'b'`/0) from the fixed 36-CHAR16 name field. */
STATIC CHAR16
IsAbEntry (IN CONST UINT8 *Entry)
{
  CONST CHAR16 *Name = (CONST CHAR16 *)(Entry + SPINOR_PART_NAME_OFFSET);
  UINT32 i;
  UINT32 Len = 0;

  for (i = 0; i < SPINOR_PART_NAME_CHARS; i++) {
    if (Name[i] == 0) {
      break;
    }
    Len++;
  }

  /* Need at least "_x" -> underscore followed by slot char. */
  if (Len < 2) {
    return 0;
  }
  if (Name[Len - 2] != L'_') {
    return 0;
  }
  if (Name[Len - 1] == L'a' || Name[Len - 1] == L'b') {
    return Name[Len - 1];
  }
  return 0;
}

/* TRUE if two entries share the base name minus the `_a`/`_b` suffix. */
STATIC BOOLEAN
SameBaseName (IN CONST UINT8 *EntryA, IN CONST UINT8 *EntryB)
{
  CONST CHAR16 *A = (CONST CHAR16 *)(EntryA + SPINOR_PART_NAME_OFFSET);
  CONST CHAR16 *B = (CONST CHAR16 *)(EntryB + SPINOR_PART_NAME_OFFSET);
  UINT32 i;

  for (i = 0; i < SPINOR_PART_NAME_CHARS; i++) {
    CHAR16 Ca = A[i];
    CHAR16 Cb = B[i];

    /* Compare up to (but not including) the final slot char.  When both hit
     * the last meaningful char, they must both be the '_' that precedes the
     * slot letter; the slot letter itself is allowed to differ. */
    if (Ca == 0 && Cb == 0) {
      return TRUE;
    }
    if (Ca == 0 || Cb == 0) {
      return FALSE;
    }
    /* At the underscore that starts the "_a"/"_b" suffix, stop comparing:
     * everything before matched. */
    if (Ca == L'_' && Cb == L'_') {
      CHAR16 Na = (i + 1 < SPINOR_PART_NAME_CHARS) ? A[i + 1] : 0;
      CHAR16 Nb = (i + 1 < SPINOR_PART_NAME_CHARS) ? B[i + 1] : 0;
      CHAR16 Ea = (i + 2 < SPINOR_PART_NAME_CHARS) ? A[i + 2] : 0;
      CHAR16 Eb = (i + 2 < SPINOR_PART_NAME_CHARS) ? B[i + 2] : 0;
      if ((Na == L'a' || Na == L'b') && Ea == 0 &&
          (Nb == L'a' || Nb == L'b') && Eb == 0) {
        return TRUE;
      }
    }
    if (Ca != Cb) {
      return FALSE;
    }
  }
  return TRUE;
}

/* Validate a caller-supplied slot suffix is exactly "_a" or "_b". */
STATIC BOOLEAN
IsValidSlotSuffix (IN CONST CHAR16 *Suffix)
{
  if (Suffix == NULL) {
    return FALSE;
  }
  if (Suffix[0] != L'_') {
    return FALSE;
  }
  if (Suffix[1] != L'a' && Suffix[1] != L'b') {
    return FALSE;
  }
  if (Suffix[2] != 0) {
    return FALSE;
  }
  return TRUE;
}


STATIC EFI_STATUS
GetSpinorBlockIo (OUT EFI_BLOCK_IO_PROTOCOL **BlockIo)
{
  EFI_STATUS Status;
  PartiSelectFilter HandleFilter;
  HandleInfo HandleInfoList[1];
  UINT32 MaxHandles = 1;
  UINT32 Attribs = 0;

  if (BlockIo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  /* SPINOR is a single whole-device root handle (like NVMe: NO_LUN). */
  Attribs |= BLK_IO_SEL_SELECT_ROOT_DEVICE_ONLY;
  HandleFilter.PartitionType = NULL;
  HandleFilter.VolumeName = NULL;
  HandleFilter.RootDeviceType = &gEfiSpiNor0Guid;

  Status = GetBlkIOHandles (Attribs, &HandleFilter, HandleInfoList, &MaxHandles);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "SpinorSlotSync: no SPINOR BlockIo handle: %r\n",
            Status));
    return Status;
  }
  if (MaxHandles != 1) {
    DEBUG ((EFI_D_ERROR,
            "SpinorSlotSync: expected 1 SPINOR handle, got %d\n", MaxHandles));
    return EFI_NO_MEDIA;
  }

  *BlockIo = HandleInfoList[0].BlkIo;
  if (*BlockIo == NULL) {
    return EFI_NO_MEDIA;
  }
  return EFI_SUCCESS;
}

/* ======================================================================== *
 *                                                                          *
 *  Reads the SPINOR GPT (primary + backup), swaps the 16-byte TYPE-GUID     *
 *  between each _a/_b partition pair in the in-buffer entries, recomputes    *
 *  the entry-array CRC32 then the header CRC32, and writes both copies back. *
 *  Block-size-agnostic (uses BlockIo->Media->BlockSize), so 4KB NOR sectors  *
 *  need no special handling.                                                 *
 *                                                                          *
 *  The swap is gated on UpdateType & PARTITION_GUID_MASK.  SPINOR carries    *
 *  no ACTIVE/PRIORITY attributes (PBL/XBL never read them); the active slot  *
 *  is signalled purely by which _a/_b entry holds the canonical XBL          *
 *  TYPE-GUID anchor (see ReadSpinorActiveSlot).                              *
 * ======================================================================== */

STATIC EFI_STATUS
UpdateSpinorPartitionAttributes (IN CONST CHAR16 *TargetSuffix,
                                 IN UINT32 UpdateType)
{
  EFI_STATUS Status;
  EFI_BLOCK_IO_PROTOCOL *BlockIo = NULL;
  UINT8 *GptHdr = NULL;
  UINT8 *GptHdrPtr = NULL;
  UINT8 *PtnArray;
  UINT64 DeviceDensity;
  UINT64 CardSizeSec;
  UINTN SpinorGptSzBytes;
  UINT32 BlkSz;
  UINT32 PartEntriesblocks;
  UINT32 Offset;
  UINT32 Iter;
  UINT32 MaxPtnCount;
  UINT32 PtnEntrySz;
  UINT32 CrcVal = 0;
  UINT32 HdrSz = GPT_HEADER_SIZE;
  UINT32 i;
  UINT32 j;

  if (!IsValidSlotSuffix (TargetSuffix)) {
    DEBUG ((EFI_D_ERROR, "SpinorSlotSync: invalid target suffix\n"));
    return EFI_INVALID_PARAMETER;
  }

  Status = GetSpinorBlockIo (&BlockIo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DeviceDensity = GetPartitionSize (BlockIo);
  if (!DeviceDensity) {
    return EFI_DEVICE_ERROR;
  }
  BlkSz = BlockIo->Media->BlockSize;
  if (!BlkSz) {
    return EFI_DEVICE_ERROR;
  }

  /* Bound SPINOR GPT I/O to the fixed 5-block extent (1 header + 4 entry blocks). */
  PartEntriesblocks = SPINOR_GPT_ENTRY_BLOCKS;
  SpinorGptSzBytes = SPINOR_GPT_TOTAL_BLOCKS * BlkSz;
  CardSizeSec = DeviceDensity / BlkSz;
  Offset = PRIMARY_HDR_LBA;

  /* Need room for both the primary GPT (front) and the backup GPT (tail). */
  if (CardSizeSec < (UINT64)(PRIMARY_HDR_LBA + 2 * SPINOR_GPT_TOTAL_BLOCKS)) {
    DEBUG ((EFI_D_ERROR, "SpinorSlotSync: SPINOR too small for GPT\n"));
    return EFI_VOLUME_CORRUPTED;
  }

  GptHdr = AllocateZeroPool (SpinorGptSzBytes);
  if (!GptHdr) {
    DEBUG ((EFI_D_ERROR, "SpinorSlotSync: alloc failed\n"));
    return EFI_OUT_OF_RESOURCES;
  }
  GptHdrPtr = GptHdr;

  /* Iterate twice: primary GPT (front), then backup GPT (tail). */
  for (Iter = 0; Iter < 2;
       Iter++, (Offset = CardSizeSec - SpinorGptSzBytes / BlkSz)) {

    GptHdr = GptHdrPtr;
    Status = BlockIo->ReadBlocks (BlockIo, BlockIo->Media->MediaId, Offset,
                                  SpinorGptSzBytes, GptHdr);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "SpinorSlotSync: GPT read failed (iter %d): %r\n",
              Iter, Status));
      goto Exit;
    }

    if (Iter == 0x1) {
      /* Backup GPT: entry array first, header follows the array. */
      PtnArray = GptHdr;
      GptHdr = GptHdr + (PartEntriesblocks * BlkSz);
    } else {
      /* Primary GPT: header at block 0 of the buffer, entries follow. */
      PtnArray = GptHdr + BlkSz;
    }

    MaxPtnCount = GET_LWORD_FROM_BYTE (&GptHdr[PARTITION_COUNT_OFFSET]);
    PtnEntrySz = GET_LWORD_FROM_BYTE (&GptHdr[PENTRY_SIZE_OFFSET]);
    /* Reject a too-small entry stride (would let name/GUID reads run off the
     * end of each entry) as well as an array that overruns the 4-block SPINOR
     * entry region we read. */
    if (PtnEntrySz < PARTITION_ENTRY_SIZE || MaxPtnCount == 0 ||
        ((UINT64)MaxPtnCount * PtnEntrySz) >
            ((UINT64)PartEntriesblocks * BlkSz)) {
      DEBUG ((EFI_D_ERROR,
              "SpinorSlotSync: bad GPT hdr MaxPtnCount=%x PtnEntrySz=%x\n",
              MaxPtnCount, PtnEntrySz));
      Status = EFI_VOLUME_CORRUPTED;
      goto Exit;
    }

    /* ---- Swap the TYPE-GUID between each _a/_b pair ---- */
    if (UpdateType & PARTITION_GUID_MASK) {
      for (i = 0; i < MaxPtnCount; i++) {
        UINT8 *EntryA = PtnArray + (i * PtnEntrySz);
        if (IsAbEntry (EntryA) != L'a') {
          continue; /* anchor the swap on each _a entry */
        }
        for (j = 0; j < MaxPtnCount; j++) {
          UINT8 *EntryB = PtnArray + (j * PtnEntrySz);
          UINT8 Tmp[GUID_SIZE];

          if (IsAbEntry (EntryB) != L'b') {
            continue;
          }
          if (!SameBaseName (EntryA, EntryB)) {
            continue;
          }
          /* Swap the 16-byte partition TYPE-GUID (entry offset 0). */
          CopyMem (Tmp, EntryA, GUID_SIZE);
          CopyMem (EntryA, EntryB, GUID_SIZE);
          CopyMem (EntryB, Tmp, GUID_SIZE);
          break;
        }
      }
    }

    /* Recompute entry-array CRC32, then header CRC32 (header CRC zeroed). */
    Status = gBS->CalculateCrc32 (PtnArray, (MaxPtnCount * PtnEntrySz), &CrcVal);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "SpinorSlotSync: array CRC32 failed: %r\n", Status));
      goto Exit;
    }
    PUT_LONG (&GptHdr[PARTITION_CRC_OFFSET], CrcVal);

    CrcVal = 0;
    PUT_LONG (&GptHdr[HEADER_CRC_OFFSET], CrcVal);
    Status = gBS->CalculateCrc32 (GptHdr, HdrSz, &CrcVal);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "SpinorSlotSync: header CRC32 failed: %r\n", Status));
      goto Exit;
    }
    PUT_LONG (&GptHdr[HEADER_CRC_OFFSET], CrcVal);

    if (Iter == 0x1) {
      /* Backup: write from the entry-array base at the tail offset. */
      Status = BlockIo->WriteBlocks (BlockIo, BlockIo->Media->MediaId, Offset,
                                     SpinorGptSzBytes, (VOID *)PtnArray);
    } else {
      Status = BlockIo->WriteBlocks (BlockIo, BlockIo->Media->MediaId, Offset,
                                     SpinorGptSzBytes, (VOID *)GptHdr);
    }
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "SpinorSlotSync: GPT write failed (iter %d): %r\n",
              Iter, Status));
      goto Exit;
    }
  }

  Status = BlockIo->FlushBlocks (BlockIo);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "SpinorSlotSync: flush failed: %r\n", Status));
    goto Exit;
  }

  Status = EFI_SUCCESS;

Exit:
  if (GptHdrPtr) {
    FreePool (GptHdrPtr);
    GptHdrPtr = NULL;
  }
  return Status;
}

BOOLEAN
SpinorSlotSyncEnabled (VOID)
{
  return TRUE;
}

/* Read the SPINOR active slot into ActiveSlot by locating the canonical XBL
 * boot TYPE-GUID (gSpinorXblBootGuid): the _a/_b entry currently carrying that
 * TYPE-GUID names the booted slot, mirroring the fixed-GPT signal PBL/XBL boots
 * from.  SPINOR has no ACTIVE/PRIORITY attributes to key off. */

EFI_STATUS
ReadSpinorActiveSlot (OUT Slot *ActiveSlot)
{
  EFI_STATUS Status;
  EFI_BLOCK_IO_PROTOCOL *BlockIo = NULL;
  UINT8 *GptHdr = NULL;
  UINT8 *PtnArray;
  UINT64 DeviceDensity;
  UINTN SpinorGptSzBytes;
  UINT32 BlkSz;
  UINT32 PartEntriesblocks;
  UINT32 MaxPtnCount;
  UINT32 PtnEntrySz;
  UINT32 i;
  CHAR16 ActiveLast = 0;

  if (ActiveSlot == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetSpinorBlockIo (&BlockIo);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR,
            "SpinorSlotSync: read: SPINOR BlockIo unavailable: %r\n", Status));
    goto Exit;
  }
  DeviceDensity = GetPartitionSize (BlockIo);
  if (!DeviceDensity) {
    DEBUG ((EFI_D_ERROR, "SpinorSlotSync: read: bad device density\n"));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }
  BlkSz = BlockIo->Media->BlockSize;
  if (!BlkSz) {
    DEBUG ((EFI_D_ERROR, "SpinorSlotSync: read: bad block size\n"));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }
  /* Bound to the fixed 5-block SPINOR GPT extent (LBA 1..5) */
  PartEntriesblocks = SPINOR_GPT_ENTRY_BLOCKS;
  SpinorGptSzBytes = SPINOR_GPT_TOTAL_BLOCKS * BlkSz;

  GptHdr = AllocateZeroPool (SpinorGptSzBytes);
  if (!GptHdr) {
    DEBUG ((EFI_D_ERROR, "SpinorSlotSync: read: alloc failed\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Status = BlockIo->ReadBlocks (BlockIo, BlockIo->Media->MediaId,
                                PRIMARY_HDR_LBA, SpinorGptSzBytes, GptHdr);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "SpinorSlotSync: read: GPT read failed: %r\n", Status));
    goto Exit;
  }

  MaxPtnCount = GET_LWORD_FROM_BYTE (&GptHdr[PARTITION_COUNT_OFFSET]);
  PtnEntrySz = GET_LWORD_FROM_BYTE (&GptHdr[PENTRY_SIZE_OFFSET]);

  if (PtnEntrySz < PARTITION_ENTRY_SIZE || MaxPtnCount == 0 ||
      ((UINT64)MaxPtnCount * PtnEntrySz) >
          ((UINT64)PartEntriesblocks * BlkSz)) {
    DEBUG ((EFI_D_ERROR,
            "SpinorSlotSync: read: bad GPT hdr MaxPtnCount=%x PtnEntrySz=%x\n",
            MaxPtnCount, PtnEntrySz));
    Status = EFI_VOLUME_CORRUPTED;
    goto Exit;
  }
  PtnArray = GptHdr + BlkSz;

  /* The active slot is whichever _a/_b entry holds the XBL boot TYPE-GUID. */
  for (i = 0; i < MaxPtnCount; i++) {
    UINT8 *Entry = PtnArray + (i * PtnEntrySz);

    if (!CompareGuid ((EFI_GUID *)Entry, &gSpinorXblBootGuid)) {
      continue;
    }
    ActiveLast = IsAbEntry (Entry);
    break;
  }

  if (ActiveLast == 0) {
    DEBUG ((EFI_D_ERROR,
            "SpinorSlotSync: read: XBL boot GUID not found on any _a/_b entry\n"));
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  ActiveSlot->Suffix[0] = L'_';
  ActiveSlot->Suffix[1] = ActiveLast;
  ActiveSlot->Suffix[2] = 0;
  Status = EFI_SUCCESS;

Exit:
  if (GptHdr) {
    FreePool (GptHdr);
  }
  return Status;
}

/* Make NewSlot the active SPINOR slot.  When the requested slot already matches
 * the current one, this is a no-op (a same-slot call would only rewrite
 * identical GPT data).  When the slot differs -- or the current slot cannot be
 * read -- swap the TYPE-GUID so the XBL anchor rides the requested slot. */

EFI_STATUS
SetSpinorActiveSlot (IN Slot *NewSlot)
{
  EFI_STATUS Status;
  Slot CurrentSlot = {{0}};
  UINT32 UpdateType = PARTITION_ATTRIBUTES_MASK;

  if (NewSlot == NULL || !IsValidSlotSuffix (NewSlot->Suffix)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = ReadSpinorActiveSlot (&CurrentSlot);
  if (!EFI_ERROR (Status)) {
    if (StrnCmp (CurrentSlot.Suffix, NewSlot->Suffix,
                 StrLen (NewSlot->Suffix)) == 0) {
      /* Already on the requested slot -> nothing to write. */
      DEBUG ((EFI_D_INFO,
              "SpinorSlotSync: SPINOR already on slot %s, skipping write\n",
              NewSlot->Suffix));
      return EFI_SUCCESS;
    }
    /* Slot actually changes -> also swap the TYPE-GUID. */
    UpdateType |= PARTITION_GUID_MASK;
  } else {
    /* Current slot unknown -> swap the TYPE-GUID to force the anchor over. */
    UpdateType |= PARTITION_GUID_MASK;
  }

  Status = UpdateSpinorPartitionAttributes (NewSlot->Suffix, UpdateType);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "SpinorSlotSync: SetSpinorActiveSlot(%s) failed: %r\n",
            NewSlot->Suffix, Status));
  } else {
    DEBUG ((EFI_D_INFO, "SpinorSlotSync: SPINOR active slot -> %s\n",
            NewSlot->Suffix));
  }
  return Status;
}

/* Boot-time heal: make the SPINOR slot match the UFS slot.
 * Fail-open: any read error leaves SPINOR untouched (no OTA regression while
 * the SPINOR driver may be absent).  On a genuine mismatch, roll SPINOR forward
 * and cold-reboot so the corrected GPT takes effect cleanly. */

VOID
CheckAndRestoreSlotConsistency (VOID)
{
  EFI_STATUS Status;
  Slot UfsSlot;
  Slot SpinorSlot = {{0}};

  UfsSlot = GetCurrentSlotSuffix ();
  if (!IsValidSlotSuffix (UfsSlot.Suffix)) {
    /* Not a multislot boot / no active UFS slot -> nothing to reconcile. */
    return;
  }

  Status = ReadSpinorActiveSlot (&SpinorSlot);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR,
            "SpinorSlotSync: heal skipped, SPINOR read failed: %r\n", Status));
    return; /* fail-open */
  }

  DEBUG ((EFI_D_INFO,
          "SpinorSlotSync: SPINOR active slot read as %s\n",
          SpinorSlot.Suffix));

  if (StrnCmp (UfsSlot.Suffix, SpinorSlot.Suffix,
               StrLen (UfsSlot.Suffix)) == 0) {
    return; /* already consistent */
  }

  DEBUG ((EFI_D_ERROR,
          "SpinorSlotSync: slot mismatch UFS=%s SPINOR=%s -> healing\n",
          UfsSlot.Suffix, SpinorSlot.Suffix));

  Status = SetSpinorActiveSlot (&UfsSlot);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "SpinorSlotSync: heal write failed: %r\n", Status));
    return; /* fail-open */
  }

  DEBUG ((EFI_D_ERROR, "SpinorSlotSync: heal complete, rebooting\n"));
  RebootDevice (NORMAL_MODE);
}

#else /* !ENABLE_SPINOR_SLOT_SYNC */

BOOLEAN
SpinorSlotSyncEnabled (VOID)
{
  return FALSE;
}

EFI_STATUS
ReadSpinorActiveSlot (OUT Slot *ActiveSlot)
{
  (VOID)ActiveSlot;
  return EFI_UNSUPPORTED;
}

EFI_STATUS
SetSpinorActiveSlot (IN Slot *NewSlot)
{
  (VOID)NewSlot;
  return EFI_UNSUPPORTED;
}

VOID
CheckAndRestoreSlotConsistency (VOID)
{
}

#endif /* ENABLE_SPINOR_SLOT_SYNC */

