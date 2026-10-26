/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/LinuxLoaderLib.h>
#include <Library/PartitionTableUpdate.h>
#include <aes/aes_public.h>
#include "BootLinux.h"
#include <Library/BootImgDecrypt.h>
#include <Fsp/Fsp.h>
#include <Library/RecoveryInfo.h>

STATIC UINT32 gDecryptedImageEncSize = 0;

BOOLEAN
IsBootImageEncrypted (
  IN CONST VOID  *ImageBuffer,
  IN UINTN        ImageSize
  )
{
  if (ImageBuffer == NULL || ImageSize <= BOOT_IMG_ENC_HDR_SIZE) {
    return FALSE;
  }

  if (CompareMem (ImageBuffer,
                  BOOT_IMG_ENC_MAGIC,
                  BOOT_IMG_ENC_MAGIC_SIZE) == 0) {
    return TRUE;
  }

  /* Plain ANDROID! image -- not encrypted */
  if (CompareMem (ImageBuffer,
                  ANDROID_BOOT_MAGIC,
                  ANDROID_BOOT_MAGIC_SIZE) != 0) {
    /* Legacy encrypted format -- no header magic, treat as encrypted. */
    return TRUE;
  }

  return FALSE;
}

EFI_STATUS
DecryptBootImage (
  IN OUT VOID   *ImageBuffer,
  IN OUT UINTN  *ImageSize
  )
{
  EFI_STATUS          Status;
  UINT8              *Buf;
  UINTN               BufSize;
  UINT32              MbnSize;
  UINT32              CipherSize;
  UINT8              *MbnPtr;
  UINT8              *IvPtr;
  UINT8              *TagPtr;
  UINT8              *CipherPtr;
  UINT8               Iv[BOOT_IMG_AES_GCM_IV_RAW_SIZE];
  UINT8               StoredTag[BOOT_IMG_AES_GCM_TAG_RAW_SIZE];
  UINT8               ComputedTag[BOOT_IMG_AES_GCM_TAG_RAW_SIZE];
  UINT8              *PlainBuf    = NULL;
  UINT8               ClearKey[BOOT_IMG_AES_KEY_SIZE];
  UINTN               ClearKeyLen = 0;
  GcmAesStruct        CipherCtx;
  SW_CipherEncryptDir Dir  = SW_CIPHER_DECRYPT;
  SW_CipherModeType   Mode = SW_CIPHER_MODE_GCM;
  IovecListType       IoVecIn;
  IovecListType       IoVecOut;
  IovecType           IovecIn;
  IovecType           IovecOut;
  UINT32              Ret;

  if (ImageBuffer == NULL || ImageSize == NULL) {
    DEBUG ((EFI_D_ERROR, "DecryptBootImage: invalid parameter\n"));
    return EFI_INVALID_PARAMETER;
  }

  Buf     = (UINT8 *)ImageBuffer;
  BufSize = *ImageSize;

  /* Verify ENCB magic */
  if (BufSize < BOOT_IMG_ENC_HDR_SIZE ||
      CompareMem (Buf, BOOT_IMG_ENC_MAGIC, BOOT_IMG_ENC_MAGIC_SIZE) != 0) {
    DEBUG ((EFI_D_ERROR,
            "DecryptBootImage: missing ENCB magic\n"));
    return EFI_INVALID_PARAMETER;
  }

  /*
   * Parse fixed header fields (all at known offsets from start):
   *   [4..7]  mbn_size   = M
   *   [8..11] cipher_size = N
   */
  MbnSize    = (UINT32)Buf[BOOT_IMG_ENC_MBN_SIZE_OFF]
             | ((UINT32)Buf[BOOT_IMG_ENC_MBN_SIZE_OFF + 1] << 8)
             | ((UINT32)Buf[BOOT_IMG_ENC_MBN_SIZE_OFF + 2] << 16)
             | ((UINT32)Buf[BOOT_IMG_ENC_MBN_SIZE_OFF + 3] << 24);

  CipherSize = (UINT32)Buf[BOOT_IMG_ENC_CIPHER_SIZE_OFF]
             | ((UINT32)Buf[BOOT_IMG_ENC_CIPHER_SIZE_OFF + 1] << 8)
             | ((UINT32)Buf[BOOT_IMG_ENC_CIPHER_SIZE_OFF + 2] << 16)
             | ((UINT32)Buf[BOOT_IMG_ENC_CIPHER_SIZE_OFF + 3] << 24);

  if (MbnSize == 0 || CipherSize == 0) {
    DEBUG ((EFI_D_ERROR,
            "DecryptBootImage: zero mbn_size or cipher_size\n"));
    return EFI_INVALID_PARAMETER;
  }

  /* Validate total size fits in buffer */
  /* Cast each operand to UINTN to avoid UINT32 overflow before comparison. */
  if (BufSize < ((UINTN)BOOT_IMG_ENC_HDR_SIZE + (UINTN)MbnSize +
                  (UINTN)BOOT_IMG_ENC_METADATA_SIZE + (UINTN)CipherSize)) {
    DEBUG ((EFI_D_ERROR,
            "DecryptBootImage: buffer too small for declared sizes "
            "(mbn=%u cipher=%u buf=%lu)\n",
            MbnSize, CipherSize, BufSize));
    return EFI_INVALID_PARAMETER;
  }

  /*
   * Derive sequential field pointers:
   *   offset 12       : MBN
   *   offset 12+M     : IV  (12B, raw)
   *   offset 12+M+12  : GCM tag (16B, raw)
   *   offset 12+M+28  : Ciphertext
   */
  MbnPtr    = Buf + BOOT_IMG_ENC_HDR_SIZE;
  IvPtr     = MbnPtr    + MbnSize;
  TagPtr    = IvPtr     + BOOT_IMG_AES_GCM_IV_RAW_SIZE;
  CipherPtr = TagPtr    + BOOT_IMG_AES_GCM_TAG_RAW_SIZE;

  CopyMem (Iv,        IvPtr,  BOOT_IMG_AES_GCM_IV_RAW_SIZE);
  CopyMem (StoredTag, TagPtr, BOOT_IMG_AES_GCM_TAG_RAW_SIZE);

  DEBUG ((EFI_D_INFO, "DecryptBootImage: --- Start ---\n"));
  DEBUG ((EFI_D_VERBOSE,
          "DecryptBootImage: MBN=%u CipherSize=%u\n",
          MbnSize, CipherSize));
  DEBUG ((EFI_D_VERBOSE,
          "DecryptBootImage: IV =%02x%02x%02x%02x%02x%02x"
          "%02x%02x%02x%02x%02x%02x\n",
          Iv[0],  Iv[1],  Iv[2],  Iv[3],  Iv[4],  Iv[5],
          Iv[6],  Iv[7],  Iv[8],  Iv[9],  Iv[10], Iv[11]));
  DEBUG ((EFI_D_VERBOSE,
          "DecryptBootImage: TAG=%02x%02x%02x%02x%02x%02x%02x%02x\n",
          StoredTag[0], StoredTag[1], StoredTag[2], StoredTag[3],
          StoredTag[4], StoredTag[5], StoredTag[6], StoredTag[7]));
  DEBUG ((EFI_D_VERBOSE,
          "DecryptBootImage: TAG=%02x%02x%02x%02x%02x%02x%02x%02x\n",
          StoredTag[8],  StoredTag[9],  StoredTag[10], StoredTag[11],
          StoredTag[12], StoredTag[13], StoredTag[14], StoredTag[15]));

  /* Load FSP TA and unwrap the MBN to get the clear AES key */
  DEBUG ((EFI_D_INFO,
          "DecryptBootImage: loading FSP TA for key unwrap\n"));

  Status = FspLoadTa ();
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR,
            "DecryptBootImage: FspLoadTa failed: %r\n", Status));
    goto Cleanup;
  }

  SetMem (ClearKey, sizeof (ClearKey), 0);
  Status = FspUnwrapWrappedKey (MbnPtr,
                                (UINT64)MbnSize,
                                ClearKey,
                                sizeof (ClearKey),
                                &ClearKeyLen);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR,
            "DecryptBootImage: FspUnwrapWrappedKey failed: %r\n",
            Status));
    goto Cleanup;
  }

  if (ClearKeyLen != BOOT_IMG_AES_KEY_SIZE) {
    DEBUG ((EFI_D_ERROR,
            "DecryptBootImage: unexpected key length %lu\n",
            ClearKeyLen));
    Status = EFI_DEVICE_ERROR;
    goto Cleanup;
  }

  DEBUG ((EFI_D_INFO,
          "DecryptBootImage: dynamic key unwrapped (%lu bytes)\n",
          ClearKeyLen));

  /* AES-256-GCM decrypt */
  PlainBuf = AllocateZeroPool (CipherSize);
  if (PlainBuf == NULL) {
    DEBUG ((EFI_D_ERROR,
            "DecryptBootImage: failed to allocate %u bytes\n",
            CipherSize));
    gDecryptedImageEncSize = 0;
    Status = EFI_OUT_OF_RESOURCES;
    goto Cleanup;
  }

  IovecIn.pvBase  = (VOID *)CipherPtr;
  IovecIn.dwLen   = CipherSize;
  IoVecIn.iov     = &IovecIn;
  IoVecIn.size    = 1;

  IovecOut.pvBase = (VOID *)PlainBuf;
  IovecOut.dwLen  = CipherSize;
  IoVecOut.iov    = &IovecOut;
  IoVecOut.size   = 1;

  SetMem (&CipherCtx, sizeof (CipherCtx), 0);
  CipherCtx.InstanceId = 0;

  Ret = SW_Cipher_Init (SW_CIPHER_ALG_AES256, &CipherCtx);
  if (Ret != UC_E_SUCCESS) {
    DEBUG ((EFI_D_ERROR,
            "DecryptBootImage: SW_Cipher_Init failed (ret=%u)\n", Ret));
    Status = EFI_DEVICE_ERROR;
    goto Cleanup;
  }

  Ret = SW_Cipher_SetParam (SW_CIPHER_PARAM_DIRECTION,
                            &Dir, sizeof (Dir), &CipherCtx);
  if (Ret != UC_E_SUCCESS) {
    DEBUG ((EFI_D_ERROR,
            "DecryptBootImage: SetParam DIRECTION failed (ret=%u)\n", Ret));
    Status = EFI_DEVICE_ERROR;
    goto CleanupCtx;
  }

  Ret = SW_Cipher_SetParam (SW_CIPHER_PARAM_MODE,
                            &Mode, sizeof (Mode), &CipherCtx);
  if (Ret != UC_E_SUCCESS) {
    DEBUG ((EFI_D_ERROR,
            "DecryptBootImage: SetParam MODE failed (ret=%u)\n", Ret));
    Status = EFI_DEVICE_ERROR;
    goto CleanupCtx;
  }

  Ret = SW_Cipher_SetParam (SW_CIPHER_PARAM_KEY,
                            (CONST VOID *)ClearKey,
                            BOOT_IMG_AES_KEY_SIZE, &CipherCtx);
  if (Ret != UC_E_SUCCESS) {
    DEBUG ((EFI_D_ERROR,
            "DecryptBootImage: SetParam KEY failed (ret=%u)\n", Ret));
    Status = EFI_DEVICE_ERROR;
    goto CleanupCtx;
  }

  Ret = SW_Cipher_SetParam (SW_CIPHER_PARAM_IV,
                            (CONST VOID *)Iv,
                            BOOT_IMG_AES_GCM_IV_RAW_SIZE, &CipherCtx);
  if (Ret != UC_E_SUCCESS) {
    DEBUG ((EFI_D_ERROR,
            "DecryptBootImage: SetParam IV failed (ret=%u)\n", Ret));
    Status = EFI_DEVICE_ERROR;
    goto CleanupCtx;
  }

  Ret = SW_CipherData (IoVecIn, &IoVecOut, &CipherCtx);
  if (Ret != UC_E_SUCCESS) {
    DEBUG ((EFI_D_ERROR,
            "DecryptBootImage: SW_CipherData failed (ret=%u)\n", Ret));
    Status = EFI_DEVICE_ERROR;
    goto CleanupCtx;
  }

  SetMem (ComputedTag, sizeof (ComputedTag), 0);
  Ret = SW_Cipher_GetParam (SW_CIPHER_PARAM_TAG,
                            (VOID *)ComputedTag,
                            BOOT_IMG_AES_GCM_TAG_RAW_SIZE, &CipherCtx);
  if (Ret != UC_E_SUCCESS) {
    DEBUG ((EFI_D_ERROR,
            "DecryptBootImage: GetParam TAG failed (ret=%u)\n", Ret));
    Status = EFI_DEVICE_ERROR;
    goto CleanupCtx;
  }

  if (CompareMem (ComputedTag, StoredTag, BOOT_IMG_AES_GCM_TAG_RAW_SIZE) != 0) {
    DEBUG ((EFI_D_ERROR,
            "DecryptBootImage: GCM authentication tag mismatch\n"));
    DEBUG ((EFI_D_ERROR,
            "DecryptBootImage: Stored   TAG=%02x%02x%02x%02x%02x%02x%02x%02x\n",
            StoredTag[0], StoredTag[1], StoredTag[2], StoredTag[3],
            StoredTag[4], StoredTag[5], StoredTag[6], StoredTag[7]));
    DEBUG ((EFI_D_ERROR,
            "DecryptBootImage: Stored   TAG=%02x%02x%02x%02x%02x%02x%02x%02x\n",
            StoredTag[8],  StoredTag[9],  StoredTag[10], StoredTag[11],
            StoredTag[12], StoredTag[13], StoredTag[14], StoredTag[15]));
    DEBUG ((EFI_D_ERROR,
            "DecryptBootImage: Computed TAG=%02x%02x%02x%02x%02x%02x%02x%02x\n",
            ComputedTag[0], ComputedTag[1], ComputedTag[2], ComputedTag[3],
            ComputedTag[4], ComputedTag[5], ComputedTag[6], ComputedTag[7]));
    DEBUG ((EFI_D_ERROR,
            "DecryptBootImage: Computed TAG=%02x%02x%02x%02x%02x%02x%02x%02x\n",
            ComputedTag[8],  ComputedTag[9],  ComputedTag[10], ComputedTag[11],
            ComputedTag[12], ComputedTag[13], ComputedTag[14], ComputedTag[15]));
    Status = EFI_SECURITY_VIOLATION;
    goto CleanupCtx;
  }

  /* Copy plaintext to start of ImageBuffer */
  CopyMem (ImageBuffer, PlainBuf, CipherSize);
  *ImageSize = CipherSize;

  DEBUG ((EFI_D_INFO,
          "DecryptBootImage: --- Success --- %u plaintext bytes\n",
          CipherSize));

  Status = EFI_SUCCESS;

CleanupCtx:
  SW_Cipher_DeInit (&CipherCtx);

Cleanup:
  if (PlainBuf != NULL) {
    SetMem (PlainBuf, CipherSize, 0);
    FreePool (PlainBuf);
  }
  SetMem (ComputedTag, sizeof (ComputedTag), 0);
  SetMem (Iv,          sizeof (Iv),          0);
  SetMem (StoredTag,   sizeof (StoredTag),   0);
  SetMem (ClearKey,    sizeof (ClearKey),    0);

  return Status;
}

EFI_STATUS
LoadAndDecryptBootImage (
  IN OUT BootInfo  *Info
  )
{
  EFI_STATUS              Status;
  CHAR16                  Pname[MAX_GPT_NAME_SIZE];
  VOID                   *HdrBuf   = NULL;
  UINT32                  HdrSize  = BOOT_IMG_MAX_PAGE_SIZE;
  PartiSelectFilter       HandleFilter;
  HandleInfo              HandleInfoList[1];
  UINT32                  MaxHandles;
  EFI_BLOCK_IO_PROTOCOL  *BlkIo    = NULL;
  UINT64                  PartSize = 0;
  UINT32                  EncSize  = 0;
  VOID                   *ImageBuf = NULL;
  UINTN                   PlainSize;
  Slot                    CurrentSlot;

  if (Info == NULL) {
    DEBUG ((EFI_D_ERROR,
            "LoadAndDecryptBootImage: invalid parameter\n"));
    return EFI_INVALID_PARAMETER;
  }

  if (Info->MultiSlotBoot) {
    CurrentSlot = GetCurrentSlotSuffix ();
    if (Info->BootIntoRecovery &&
        !IsRecoveryHasNoKernel () &&
        IsDynamicPartitionSupport ()) {
      StrnCpyS (Pname, ARRAY_SIZE (Pname), L"recovery", StrLen (L"recovery"));
    } else {
      StrnCpyS (Pname, ARRAY_SIZE (Pname), L"boot", StrLen (L"boot"));
      if (!IsRecoveryInfo () ||
          (StrCmp (CurrentSlot.Suffix, L"_a") != 0) ||
          IsRecoveryInfoWithSlotA ()) {
        StrnCatS (Pname, ARRAY_SIZE (Pname),
                  CurrentSlot.Suffix, StrLen (CurrentSlot.Suffix));
      }
    }
  } else {
    if (Info->BootIntoRecovery && !IsRecoveryHasNoKernel ()) {
      StrnCpyS (Pname, ARRAY_SIZE (Pname), L"recovery", StrLen (L"recovery"));
    } else {
      StrnCpyS (Pname, ARRAY_SIZE (Pname), L"boot", StrLen (L"boot"));
    }
  }

  /* Read first page to detect encryption */
  HdrBuf = AllocatePages (ALIGN_PAGES (BOOT_IMG_MAX_PAGE_SIZE,
                                       ALIGNMENT_MASK_4KB));
  if (HdrBuf == NULL) {
    DEBUG ((EFI_D_ERROR,
            "LoadAndDecryptBootImage: failed to allocate header buffer\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  Status = LoadImageFromPartition (HdrBuf, &HdrSize, Pname);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR,
            "LoadAndDecryptBootImage: failed to read header "
            "from %s: %r\n", Pname, Status));
    goto FreeHdr;
  }

  /* Plain image -- no-op, LoadImageAndAuth() handles it */
  if (!IsBootImageEncrypted (HdrBuf, (UINTN)HdrSize)) {
    DEBUG ((EFI_D_INFO,
            "LoadAndDecryptBootImage: plain image on %s, "
            "skipping decryption\n", Pname));
    Status = EFI_SUCCESS;
    goto FreeHdr;
  }

  DEBUG ((EFI_D_INFO,
          "LoadAndDecryptBootImage: encrypted image detected on %s\n",
          Pname));

  /* Get partition size for full read */
  HandleFilter.RootDeviceType = NULL;
  HandleFilter.PartitionLabel = Pname;
  HandleFilter.VolumeName     = NULL;
  MaxHandles = ARRAY_SIZE (HandleInfoList);

  Status = GetBlkIOHandles (BLK_IO_SEL_PARTITIONED_GPT |
                            BLK_IO_SEL_PARTITIONED_MBR |
                            BLK_IO_SEL_MEDIA_TYPE_NON_REMOVABLE |
                            BLK_IO_SEL_MATCH_PARTITION_LABEL,
                            &HandleFilter, HandleInfoList, &MaxHandles);
  if (EFI_ERROR (Status) || MaxHandles == 0) {
    DEBUG ((EFI_D_ERROR,
            "LoadAndDecryptBootImage: partition %s not found\n", Pname));
    Status = EFI_NOT_FOUND;
    goto FreeHdr;
  }

  BlkIo    = HandleInfoList[0].BlkIo;
  PartSize = GetPartitionSize (BlkIo);
  if (PartSize == 0 || PartSize > MAX_UINT32) {
    DEBUG ((EFI_D_ERROR,
            "LoadAndDecryptBootImage: invalid partition size %lu\n",
            PartSize));
    Status = EFI_BAD_BUFFER_SIZE;
    goto FreeHdr;
  }
  EncSize = (UINT32)PartSize;
  gDecryptedImageEncSize = EncSize;

  ImageBuf = AllocatePages (
               ALIGN_PAGES (
                 ADD_OF (ROUND_TO_PAGE (EncSize, ALIGNMENT_MASK_4KB - 1),
                         BOOT_IMG_MAX_PAGE_SIZE),
                 ALIGNMENT_MASK_4KB));
  if (ImageBuf == NULL) {
    DEBUG ((EFI_D_ERROR,
            "LoadAndDecryptBootImage: failed to allocate %u bytes\n",
            EncSize));
    Status = EFI_OUT_OF_RESOURCES;
    goto FreeHdr;
  }

  Status = LoadImageFromPartition (ImageBuf, &EncSize, Pname);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR,
            "LoadAndDecryptBootImage: failed to read partition "
            "%s: %r\n", Pname, Status));
    goto FreeImg;
  }

  /*
   * mbn_size and cipher_size are in the ENCB header at fixed offsets.
   * DecryptBootImage() reads them directly -- no partition-size
   * dependency, no trimming needed.
   */
  PlainSize = (UINTN)EncSize;
  Status = DecryptBootImage (ImageBuf, &PlainSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR,
            "LoadAndDecryptBootImage: decryption failed: %r\n", Status));
    Status = EFI_SECURITY_VIOLATION;
    goto FreeImg;
  }

  Info->Images[0].ImageBuffer = ImageBuf;
  Info->Images[0].ImageSize   = PlainSize;

  Info->Images[0].Name = AllocateZeroPool (StrLen (Pname) + 1);
  if (Info->Images[0].Name == NULL) {
    DEBUG ((EFI_D_ERROR,
            "LoadAndDecryptBootImage: failed to allocate image name\n"));
    Status = EFI_OUT_OF_RESOURCES;
    Info->Images[0].ImageBuffer = NULL;
    Info->Images[0].ImageSize   = 0;
    goto FreeImg;
  }
  /* Use bounded UnicodeStrToAsciiStrS in place of deprecated API. */
  Status = UnicodeStrToAsciiStrS (Pname,
                                  Info->Images[0].Name,
                                  StrLen (Pname) + 1);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR,
            "LoadAndDecryptBootImage: UnicodeStrToAsciiStrS failed: %r\n",
            Status));
    FreePool (Info->Images[0].Name);
    Info->Images[0].Name        = NULL;
    Info->Images[0].ImageBuffer = NULL;
    Info->Images[0].ImageSize   = 0;
    Info->NumLoadedImages       = 0;
    goto FreeImg;
  }
  Info->NumLoadedImages = 1;

  DEBUG ((EFI_D_INFO,
          "LoadAndDecryptBootImage: decryption complete, "
          "%lu bytes ready for auth\n", PlainSize));

  Status = EFI_SUCCESS;
  goto FreeHdr;

FreeImg:
  gDecryptedImageEncSize = 0;
  FreePages (ImageBuf,
             ALIGN_PAGES (
               ADD_OF (ROUND_TO_PAGE (EncSize, ALIGNMENT_MASK_4KB - 1),
                       BOOT_IMG_MAX_PAGE_SIZE),
               ALIGNMENT_MASK_4KB));

FreeHdr:
  FreePages (HdrBuf,
             ALIGN_PAGES (BOOT_IMG_MAX_PAGE_SIZE, ALIGNMENT_MASK_4KB));
  return Status;
}

VOID
FreeDecryptedBootImage (
  IN OUT BootInfo  *Info
  )
{
  if (Info == NULL || Info->Images[0].ImageBuffer == NULL)
    return;
  if (gDecryptedImageEncSize == 0) {
    DEBUG ((EFI_D_ERROR,
            "FreeDecryptedBootImage: EncSize is 0, skipping free\n"));
    Info->Images[0].ImageBuffer = NULL;
    Info->Images[0].ImageSize   = 0;
    return;
  }

  FreePages (Info->Images[0].ImageBuffer,
             ALIGN_PAGES (
               ADD_OF (ROUND_TO_PAGE (gDecryptedImageEncSize,
                                      ALIGNMENT_MASK_4KB - 1),
                       BOOT_IMG_MAX_PAGE_SIZE),
             ALIGNMENT_MASK_4KB));
  Info->Images[0].ImageBuffer = NULL;
  Info->Images[0].ImageSize   = 0;
  gDecryptedImageEncSize      = 0;
  if (Info->Images[0].Name != NULL) {
    FreePool (Info->Images[0].Name);
    Info->Images[0].Name = NULL;
  }
}
