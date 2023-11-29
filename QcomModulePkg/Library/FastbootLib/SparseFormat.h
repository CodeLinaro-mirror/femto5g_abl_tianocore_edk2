/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 *
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Changes from Qualcomm Innovation Center are provided under the following license:
 *
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted (subject to the limitations in the
 *  disclaimer below) provided that the following conditions are met:
 *
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials provided
 *        with the distribution.
 *
 *      * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 *        contributors may be used to endorse or promote products derived
 *        from this software without specific prior written permission.
 *
 *  NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 *  GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 *  HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 *   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 *  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 *  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

typedef struct sparse_header {
  UINT32 magic;         /* 0xed26ff3a */
  UINT16 major_version; /* (0x1) - reject images with higher major versions */
  UINT16 minor_version; /* (0x0) - allow images with higer minor versions */
  UINT16 file_hdr_sz;   /* 28 bytes for first revision of the file format */
  UINT16 chunk_hdr_sz;  /* 12 bytes for first revision of the file format */
  UINT32 blk_sz;       /* block size in bytes, must be a multiple of 4 (4096) */
  UINT32 total_blks;   /* total blocks in the non-sparse output image */
  UINT32 total_chunks; /* total chunks in the sparse input image */
  UINT32
      image_checksum; /* CRC32 checksum of the original data, counting "don't
                         care" */
  /* as 0. Standard 802.3 polynomial, use a Public Domain */
  /* table implementation */
} sparse_header_t;

#define SPARSE_HEADER_MAGIC 0xed26ff3a

#define CHUNK_TYPE_RAW 0xCAC1
#define CHUNK_TYPE_FILL 0xCAC2
#define CHUNK_TYPE_DONT_CARE 0xCAC3
#define CHUNK_TYPE_CRC 0xCAC4

typedef struct chunk_header {
  UINT16 chunk_type; /* 0xCAC1 -> raw; 0xCAC2 -> fill; 0xCAC3 -> don't care */
  UINT16 reserved1;
  UINT32 chunk_sz; /* in blocks in output image */
  UINT32 total_sz; /* in bytes of chunk input file including chunk header and
                      data */
} chunk_header_t;

/* Following a Raw or Fill chunk is data.  For a Raw chunk, it's the data in
 * chunk_sz * blk_sz.
 *  For a Fill chunk, it's 4 bytes of the fill data.
 */

typedef struct BufferInfo {
  VOID *Buffer;
  UINT32 BytesWritten;
  UINT32 Size;
} BufferInfo_t;

typedef struct UbiFlasherInfo {
  UBI_FLASHER_HANDLE UbiFlasherHandle;
  EFI_UBI_FLASHER_PROTOCOL *Ubi;
  UINT32 UbiBlkSize;
  UINT32 UbiFrameNo;
} UbiFlasherInfo_t;

typedef struct SparseImgParams {
  UINT32 Chunk;
  UINT32 TotalBlocks;
  UINT64 ChunkDataSz;
  UINT64 ImageEnd;
  UINT64 WrittenBlockCount;
  UINT64 BlockCountFactor;
  UINT64 PartitionSize;
  EFI_BLOCK_IO_PROTOCOL *BlockIo;
  EFI_HANDLE *Handle;
  UbiFlasherInfo_t UbiFlasher;
  BufferInfo_t UbiInputBufferInfo;
} SparseImgParam;
