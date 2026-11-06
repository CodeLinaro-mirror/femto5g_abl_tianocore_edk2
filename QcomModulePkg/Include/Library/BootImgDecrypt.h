/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/*
 * Boot image AES-256-GCM decryption support.
 *
 * On-flash format (ENCB header):
 *
 *   [ "ENCB"(4B) ][ mbn_size(4B LE) ][ cipher_size(4B LE) ]
 *   [ Wrapped MBN(M B) ][ IV(12B) ][ GCM tag(16B) ]
 *   [ Ciphertext(N B) ]
 *
 *   All fields at fixed or sequentially-derivable offsets from start:
 *     offset  0       : ENCB magic        (4B, fixed)
 *     offset  4       : mbn_size = M      (4B LE, fixed)
 *     offset  8       : cipher_size = N   (4B LE, fixed)
 *     offset 12       : Wrapped MBN       (M bytes)
 *     offset 12+M     : IV                (12B, raw)
 *     offset 12+M+12  : GCM tag           (16B, raw)
 *     offset 12+M+28  : Ciphertext        (N bytes)
 *
 *   Wrapped MBN is passed to FspLoadTa()/FspUnwrapWrappedKey() to obtain
 *   the clear AES-256 key from TrustZone.
 *
 * Detection:
 *   "ENCB"     -> encrypted (this format)
 *   "ANDROID!" -> plain (unencrypted), no-op
 *   other      -> legacy encrypted (no header)
 */

#ifndef __BOOT_IMG_DECRYPT_H__
#define __BOOT_IMG_DECRYPT_H__

#include <Uefi.h>

/* AES-GCM parameters */
#define BOOT_IMG_AES_GCM_IV_RAW_SIZE    12
#define BOOT_IMG_AES_GCM_TAG_RAW_SIZE   16
#define BOOT_IMG_AES_KEY_SIZE           32

/* IV (12B) + GCM tag (16B) = 28 bytes */
#define BOOT_IMG_ENC_METADATA_SIZE \
  (BOOT_IMG_AES_GCM_IV_RAW_SIZE + BOOT_IMG_AES_GCM_TAG_RAW_SIZE)

/* ENCB header: magic(4) + mbn_size(4) + cipher_size(4) = 12 bytes */
#define BOOT_IMG_ENC_MAGIC            "ENCB"
#define BOOT_IMG_ENC_MAGIC_SIZE       4
#define BOOT_IMG_ENC_MBN_SIZE_OFF     4   /* offset of mbn_size field    */
#define BOOT_IMG_ENC_CIPHER_SIZE_OFF  8   /* offset of cipher_size field */
#define BOOT_IMG_ENC_HDR_SIZE         12  /* magic(4)+mbn_size(4)+cipher_size(4) */

/* mkbootimg magic -- present in all plain Qualcomm boot images */
#define ANDROID_BOOT_MAGIC       "ANDROID!"
#define ANDROID_BOOT_MAGIC_SIZE  8

/*
 * IsBootImageEncrypted() - returns TRUE if image is encrypted.
 */
BOOLEAN
IsBootImageEncrypted (
  IN CONST VOID  *ImageBuffer,
  IN UINTN        ImageSize
  );

/*
 * DecryptBootImage() - AES-256-GCM decrypt boot image in-place.
 *
 * Reads mbn_size and cipher_size from the ENCB header, locates the
 * wrapped MBN, IV, TAG, and ciphertext sequentially from offset 0.
 * Calls FspLoadTa() and FspUnwrapWrappedKey() to obtain the clear
 * AES-256 key from TrustZone, decrypts, and verifies the GCM tag.
 *
 * On success: ImageBuffer holds plaintext, *ImageSize = cipher_size.
 * Returns EFI_SECURITY_VIOLATION on tag mismatch -- caller must abort.
 */
EFI_STATUS
DecryptBootImage (
  IN OUT VOID   *ImageBuffer,
  IN OUT UINTN  *ImageSize
  );

/*
 * LoadAndDecryptBootImage() - Load boot partition, detect encryption,
 * decrypt if needed, and populate Info->Images[0] before LoadImageAndAuth().
 * Plain image: no-op. Encrypted: loads, decrypts, verifies GCM tag.
 * VerifiedBoot.c is not modified; auth always runs on plain image.
 */
EFI_STATUS
LoadAndDecryptBootImage (
  IN OUT BootInfo  *Info
  );

VOID
FreeDecryptedBootImage (
  IN OUT BootInfo  *Info
  );

#endif /* __BOOT_IMG_DECRYPT_H__ */
