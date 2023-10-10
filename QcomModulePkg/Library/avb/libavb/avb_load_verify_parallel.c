/*
 *
 * Changes from Qualcomm Innovation Center are provided under the following license:
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 * GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "avb_slot_verify.h"
#include "avb_chain_partition_descriptor.h"
#include "avb_footer.h"
#include "avb_hash_descriptor.h"
#include "avb_kernel_cmdline_descriptor.h"
#include "avb_sha.h"
#include "avb_util.h"
#include "avb_vbmeta_image.h"
#include "avb_version.h"
#include "BootStats.h"
#include "Board.h"
#include <Library/ThreadStack.h>
#include <Protocol/EFIKernelInterface.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

STATIC EFI_KERNEL_PROTOCOL  *KernIntf = NULL;
Mutex* mxLock;
UINT64 MxId = 2;
static Semaphore* SemMainThread;
UINT64 SemMainThreadID = 1;
static Semaphore* SemLoadFirst;
static Semaphore* SemLoadSecond;
UINT64 SemLoadFirstID = 3;
UINT64 SemLoadSecondID = 4;
UINT64 Semcnt = 0;

typedef struct {
  bool IsFinal;
  bool Sha256HashCheck;
  UINT32 ThreadId;
  AvbSlotVerifyResult Status;
  AvbOps* ops;
  CONST uint8_t* DescDigest;
  uint8_t* image_buf;
  char* part_name;
  uint32_t DescDigestLen;
  uint64_t ImageOffset;
  uint64_t SplitImageSize;
  uint64_t RemainImageSize;
  VOID* HashCtx;
} LoadVerifyInfo;

static AvbSlotVerifyResult VerifyPartitionSha256 (
    AvbSHA256Ctx* Sha256Ctx,
    char* part_name,
    CONST uint8_t* DescDigest,
    uint32_t DescDigestLen,
    uint8_t* image_buf,
    uint64_t ImageSize,
    bool IsFinal) {
  uint8_t *digest = NULL ;
  size_t  digest_len = 0;

  AvbSlotVerifyResult Ret = AVB_SLOT_VERIFY_RESULT_OK;

  avb_sha256_update (Sha256Ctx, image_buf, ImageSize);
  if (IsFinal == true) {
    digest = avb_sha256_final (Sha256Ctx);
    digest_len = AVB_SHA256_DIGEST_SIZE;
    if (digest_len != DescDigestLen) {
      avb_errorv (
          part_name, ": Digest in descriptor not of expected size.\n", NULL);
      Ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
      goto out;
    }

    if (avb_safe_memcmp (digest, DescDigest, digest_len) != 0) {
      avb_errorv (part_name,
                 ": Hash of data does not match digest in descriptor.\n",
                 NULL);
      Ret = AVB_SLOT_VERIFY_RESULT_ERROR_VERIFICATION;
      goto out;
    } else {
      avb_debugv (part_name, ": success: Image verification completed\n", NULL);
      goto out;
    }
  } else {
      avb_debugv (part_name, ": success: Image verification in parts\n", NULL);
      Ret = AVB_SLOT_VERIFY_RESULT_OK;
      goto out;
  }

out:
  return Ret;
}

static AvbSlotVerifyResult VerifyPartitionSha512 (
    AvbSHA512Ctx* Sha512Ctx,
    char* part_name,
    CONST uint8_t* DescDigest,
    uint32_t DescDigestLen,
    uint8_t* image_buf,
    uint64_t ImageSize,
    bool IsFinal) {
  uint8_t *digest = NULL ;
  size_t  digest_len = 0;
  AvbSlotVerifyResult Ret = AVB_SLOT_VERIFY_RESULT_OK ;

  avb_sha512_update (Sha512Ctx, image_buf, ImageSize);
  if (IsFinal == true) {
    digest = avb_sha512_final (Sha512Ctx);
    digest_len = AVB_SHA512_DIGEST_SIZE;
    if (digest_len != DescDigestLen) {
      avb_errorv (
          part_name, ": Digest in descriptor not of expected size.\n", NULL);
      Ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
      goto out;
    }
    if (avb_safe_memcmp (digest, DescDigest, digest_len) != 0) {
      avb_errorv (part_name,
                 ": Hash of data does not match digest in descriptor.\n",
                 NULL);
      Ret = AVB_SLOT_VERIFY_RESULT_ERROR_VERIFICATION;
      goto out;
    } else {
      avb_debugv (part_name, ": success: Image verification completed\n", NULL);
      goto out;
    }
  } else {
      avb_debugv (part_name, ": success: Image verification in parts\n", NULL);
      Ret = AVB_SLOT_VERIFY_RESULT_OK;
      goto out;
  }

out:
  return Ret;
}

/* Get the approximate optimal chunksize of an image, which is determined
 * by |read_speed| bytes/ms, one IO call time |timeCost|, |PageSize| and |ImageSize|.
 *
 * the optimal chunksize is sqrt(|read_speed| * |timeCost| * |ImageSize|) / |PageSize| * |PageSize|.
 */
static uint64_t GetChunkSize(uint64_t read_speed, uint64_t timeCost, uint64_t PageSize, uint64_t ImageSize) {
  uint64_t product = 1;
  uint64_t chunkSize = ImageSize;

  /*ImageSize is smaller than 1ms readsize or pagesize*/
  if(ImageSize <= read_speed || ImageSize < PageSize)
    return chunkSize;

  if(!avb_safe_mutiply_to(&product,timeCost) ||
     !avb_safe_mutiply_to(&product,read_speed) ||
     !avb_safe_mutiply_to(&product,ImageSize)) {
    avb_error("Overflow while mutiplying.\n");
    chunkSize = MAX_UINT32 - (MAX_UINT32 % PageSize);
    goto out;
  }
  product = avb_int_sqrt(product);
  /* Considering page alignment, the chunkSize should be a multiple of the pagesize.
   *
   * floor(product / PageSize) * PageSize = product - (product % PageSize).
   */
  chunkSize = product - (product % PageSize);
out:
  return chunkSize;
}

static AvbSlotVerifyResult Load_partition_to_verify (
    AvbOps* ops,
    char* part_name,
    int64_t Offset,
    uint8_t* image_buf,
    uint64_t ImageSize) {
  AvbIOResult IoRet;
  AvbSlotVerifyResult Ret = AVB_SLOT_VERIFY_RESULT_OK;
  size_t PartNumRead = 0;

  IoRet = ops->read_from_partition (
      ops, part_name, Offset, ImageSize, (image_buf + Offset), &PartNumRead);
  if (IoRet == AVB_IO_RESULT_ERROR_OOM) {
    Ret = AVB_SLOT_VERIFY_RESULT_ERROR_OOM;
    goto out;
  } else if (IoRet != AVB_IO_RESULT_OK) {
    avb_errorv (part_name, ": Error loading data from partition.\n", NULL);
    Ret = AVB_SLOT_VERIFY_RESULT_ERROR_IO;
    goto out;
  }
  if (PartNumRead != ImageSize) {
    avb_errorv (part_name, ": Read fewer than requested bytes.\n", NULL);
    Ret = AVB_SLOT_VERIFY_RESULT_ERROR_IO;
    goto out;
  }

out:
  return Ret;
}


INT32 PartitionLoad(VOID* Arg) {
  AvbSlotVerifyResult Status;
  uint64_t ImageOffset;
  uint64_t CurrentChunkSize;
  uint64_t SplitImageSize;
  char* part_name;
  LoadVerifyInfo* ThreadLoad = (LoadVerifyInfo*) Arg;
  if ((NULL ==  ThreadLoad->ops) ||
      (NULL == ThreadLoad->DescDigest) ||
      (NULL ==  ThreadLoad->image_buf) ||
      (NULL == ThreadLoad->part_name) ||
      (NULL == ThreadLoad->HashCtx)) {
    Status = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_ARGUMENT;
    ThreadLoad->Status = Status;
    KernIntf->Sem->SemPost (SemMainThread,FALSE);
    KernIntf->Thread->ThreadExit (0);
    return Status;
  }
  Thread* CurrentThread = KernIntf->Thread->GetCurrentThread();
  ImageOffset = ThreadLoad->ImageOffset;
  SplitImageSize = ThreadLoad->SplitImageSize;
  CurrentChunkSize = SplitImageSize;
  part_name = ThreadLoad->part_name;

  if (avb_strncmp ("boot", part_name, 4) == 0) {
    BootStatsSetTimeStamp (BS_KERNEL_LOAD_BOOT_START);
  }

  /* First stage */

  /* One loop one chunk */
  while(ThreadLoad->RemainImageSize >  SplitImageSize ) {
    Status = Load_partition_to_verify(ThreadLoad->ops,
              part_name,
              ImageOffset,
              ThreadLoad->image_buf,
              CurrentChunkSize);
    if(Status != AVB_SLOT_VERIFY_RESULT_OK)
      return Status;

    ImageOffset += SplitImageSize;
    ThreadLoad->RemainImageSize -= SplitImageSize;
    KernIntf->Sem->SemPost(SemLoadFirst, FALSE);
  }

  /* Second stage */
  CurrentChunkSize = ThreadLoad->RemainImageSize;
  Status = Load_partition_to_verify(ThreadLoad->ops,
              part_name,
              ImageOffset,
              ThreadLoad->image_buf,
              CurrentChunkSize);
  if(Status != AVB_SLOT_VERIFY_RESULT_OK)
      return Status;
  KernIntf->Sem->SemPost(SemLoadSecond, FALSE);

  if (avb_strncmp ("boot", part_name, 4) == 0) {
    BootStatsSetTimeStamp (BS_KERNEL_LOAD_BOOT_END);
  }
  ThreadLoad->Status = Status;
  ThreadStackNodeRemove (CurrentThread);
  KernIntf->Thread->ThreadExit (0);
  return 0;
}

INT32 PartitionVerify(VOID* Arg) {
  AvbSlotVerifyResult Status = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_ARGUMENT;
  uint64_t ImageOffset;
  uint64_t CurrentChunkSize;
  uint64_t SplitImageSize;
  AvbSHA256Ctx *Sha256Ctx;
  AvbSHA512Ctx* Sha512Ctx;
  LoadVerifyInfo* ThreadVerify = (LoadVerifyInfo*) Arg;
  Thread* CurrentThread = KernIntf->Thread->GetCurrentThread();
  if ((NULL ==  ThreadVerify->ops) ||
      (NULL == ThreadVerify->DescDigest) ||
      (NULL ==  ThreadVerify->image_buf) ||
      (NULL == ThreadVerify->part_name) ||
      (NULL == ThreadVerify->HashCtx)) {
    Status = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_ARGUMENT;
    goto out;
  }
  if(ThreadVerify->Sha256HashCheck == true)
  {
    Sha256Ctx = (AvbSHA256Ctx*) ThreadVerify->HashCtx;
    Sha512Ctx = NULL;
  }
  else
  {
    Sha256Ctx = NULL;
    Sha512Ctx = (AvbSHA512Ctx*) ThreadVerify->HashCtx;
  }

  ImageOffset = ThreadVerify->ImageOffset;
  SplitImageSize = ThreadVerify->SplitImageSize;
  CurrentChunkSize = SplitImageSize;
  /* First stage */
  while(ThreadVerify->RemainImageSize >  SplitImageSize) {
    KernIntf->Sem->SemWait (SemLoadFirst);
    if(ThreadVerify->Sha256HashCheck == true)
    {
    Status = VerifyPartitionSha256 (Sha256Ctx,
                                    ThreadVerify->part_name,
                                    ThreadVerify->DescDigest,
                                    ThreadVerify->DescDigestLen,
                                    ThreadVerify->image_buf + ImageOffset,
                                    CurrentChunkSize,
                                    ThreadVerify->IsFinal);
    }
    else{
    Status = VerifyPartitionSha512 (Sha512Ctx,
                                    ThreadVerify->part_name,
                                    ThreadVerify->DescDigest,
                                    ThreadVerify->DescDigestLen,
                                    ThreadVerify->image_buf + ImageOffset,
                                    CurrentChunkSize,
                                    ThreadVerify->IsFinal);
    }
    ThreadVerify->RemainImageSize -= ThreadVerify->SplitImageSize;
    ImageOffset += CurrentChunkSize;
    if(Status != AVB_SLOT_VERIFY_RESULT_OK)
        goto out;
  }

  /* Second stage */
  ThreadVerify->IsFinal = true;
  CurrentChunkSize = ThreadVerify->RemainImageSize;
  KernIntf->Sem->SemWait (SemLoadSecond);

  if(ThreadVerify->Sha256HashCheck == true)
  {
     if (!Sha256Ctx) {
        Status = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_ARGUMENT;
        goto out;
     }

     Status = VerifyPartitionSha256 (Sha256Ctx,
                                  ThreadVerify->part_name,
                                  ThreadVerify->DescDigest,
                                  ThreadVerify->DescDigestLen,
                                  ThreadVerify->image_buf + ImageOffset,
                                  CurrentChunkSize,
                                  ThreadVerify->IsFinal);
  }
  else
  {
     if (!Sha256Ctx) {
        Status = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_ARGUMENT;
        goto out;
     }
     Status = VerifyPartitionSha512 (Sha512Ctx,
                                  ThreadVerify->part_name,
                                  ThreadVerify->DescDigest,
                                  ThreadVerify->DescDigestLen,
                                  ThreadVerify->image_buf + ImageOffset,
                                  CurrentChunkSize,
                                  ThreadVerify->IsFinal);
  }

out:
  ThreadVerify->Status = Status;
  KernIntf->Sem->SemPost(SemMainThread, FALSE);
  ThreadStackNodeRemove (CurrentThread);
  KernIntf->Thread->ThreadExit (0);
  return Status;
}

EFI_STATUS CreateReaderThreads(LoadVerifyInfo *ThreadLoadInfo, LoadVerifyInfo *ThreadVerifyInfo)
{
  EFI_STATUS Status = EFI_SUCCESS;
  Thread* LoadThread = NULL;
  Thread* VerifyThread = NULL;
  int corenum = 4;

  LoadThread = KernIntf->Thread->ThreadCreate ("Executethreadwrapper_1",
                                        PartitionLoad, (VOID*)ThreadLoadInfo,
                                                UEFI_THREAD_PRIORITY, DEFAULT_STACK_SIZE);
  if (LoadThread == NULL) {
	    DEBUG ((EFI_D_INFO, "CreateReaderThreads: ThreadCreate failed\n"));
	    return EFI_NOT_READY;
  }
  KernIntf->Thread->ThreadSetPinnedCpu(LoadThread, corenum);
  AllocateUnSafeStackPtr (LoadThread);
  Status = KernIntf->Thread->ThreadResume (LoadThread);
  DEBUG ((EFI_D_INFO, "Thread 1 created with Thread ID: %d Status : %d\n", ThreadLoadInfo->ThreadId,Status));

  corenum = 7;
  VerifyThread = KernIntf->Thread->ThreadCreate ("Executethreadwrapper_2",
                                        PartitionVerify, (VOID*)ThreadVerifyInfo,
                                                UEFI_THREAD_PRIORITY, DEFAULT_STACK_SIZE);
  if (VerifyThread == NULL) {
	DEBUG ((EFI_D_INFO, "CreateReaderThreads: ThreadCreate failed\n"));
	return EFI_NOT_READY;
  }
  DEBUG ((EFI_D_INFO, "Thread 2 created with Thread ID: %d\n", ThreadVerifyInfo->ThreadId));

  KernIntf->Thread->ThreadSetPinnedCpu(VerifyThread, corenum);
  AllocateUnSafeStackPtr (VerifyThread);
  Status = KernIntf->Thread->ThreadResume (VerifyThread);
  return Status;
}

VOID InitReadMultiThreadEnv ()
{
   EFI_STATUS Status = EFI_SUCCESS;
   Status = gBS->LocateProtocol (&gEfiKernelProtocolGuid,
                                 NULL, (VOID **)&KernIntf);

   if ((Status != EFI_SUCCESS) ||
     (KernIntf == NULL)) {
     DEBUG ((EFI_D_INFO,
         "InitReadMultiThreadEnv: Multi thread is not supported\n"));

     return;
   }

   DEBUG ((EFI_D_INFO,
   "InitReadMultiThreadEnv: UEFI protocol header Version: %d : \n",
    KernIntf->Version));

   mxLock = KernIntf->Mutex->MutexInit (MxId);

   if (mxLock == NULL) {
      DEBUG ((EFI_D_INFO,
          "InitReadMultiThreadEnv: Mutex Initialization error\n"));
   }

   SemMainThread = KernIntf->Sem->SemInit (SemMainThreadID, Semcnt);
   SemLoadFirst = KernIntf->Sem->SemInit (SemLoadFirstID, Semcnt);
   SemLoadSecond = KernIntf->Sem->SemInit (SemLoadSecondID, Semcnt);

   DEBUG ((EFI_D_INFO,
   "InitMultiThreadEnv successful, Loading kernel image through threads\n"));
}

AvbSlotVerifyResult LoadAndVerifyHashPartitionInParallel (
    AvbOps* ops,
    AvbHashDescriptor HashDesc,
    char* part_name,
    CONST uint8_t* DescDigest,
    CONST uint8_t* DescSalt,
    uint8_t* image_buf,
    uint64_t ImageSize) {
  AvbSHA256Ctx Sha256Ctx;
  AvbSHA512Ctx Sha512Ctx;
  AvbSlotVerifyResult Status;
  uint64_t ImagePartLoop = 0;
  uint64_t ImageOffset = 0;
  uint64_t SplitImageSize = 0;
  uint64_t RemainImageSize = 0;
  uint32_t PageSize = 0;
  bool Sha256Hash = false;
  EFI_STATUS TStatus = EFI_SUCCESS;

  /*sequential read speed of images - 1000MB/s = 1MB/ms. */
  uint64_t ReadSpeed = (1 << 20);
  /* one IO call time - 1ms. */
  uint64_t IoCallTime = 1ULL;

  if (image_buf == NULL) {
    Status = AVB_SLOT_VERIFY_RESULT_ERROR_OOM;
    goto out;
  }

  InitReadMultiThreadEnv ();

  if (avb_strncmp ( (CONST CHAR8*)HashDesc.hash_algorithm, "sha256",
                 avb_strlen ("sha256")) == 0) {
    Sha256Hash  = true;
    avb_sha256_init (&Sha256Ctx);
    avb_sha256_update (&Sha256Ctx, DescSalt, HashDesc.salt_len);
  } else if (avb_strncmp ( (CONST CHAR8*)HashDesc.hash_algorithm, "sha512",
                  avb_strlen ("sha512")) == 0) {
    avb_sha512_init (&Sha512Ctx);
    avb_sha512_update (&Sha512Ctx, DescSalt, HashDesc.salt_len);
  } else {
    avb_errorv (part_name, ": Unsupported hash algorithm.\n", NULL);
    Status = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
    goto out;
  }

  GetPageSize(&PageSize);
  /*Setting SplitImageSize*/
  SplitImageSize = GetChunkSize(ReadSpeed,IoCallTime,PageSize,ImageSize);

  RemainImageSize = ImageSize;
  ImageOffset = 0;

  LoadVerifyInfo* ThreadLoadInfo = AllocateZeroPool (sizeof (LoadVerifyInfo));
  LoadVerifyInfo* ThreadVerifyInfo = AllocateZeroPool (sizeof (LoadVerifyInfo));

  if (!ThreadLoadInfo ||
      !ThreadVerifyInfo) {
    Status = AVB_SLOT_VERIFY_RESULT_ERROR_OOM;
    goto out;
  }

  ThreadLoadInfo->part_name = part_name;
  ThreadLoadInfo->ops = ops;
  ThreadLoadInfo->image_buf = image_buf;
  ThreadLoadInfo->DescDigest = DescDigest;
  ThreadLoadInfo->DescDigestLen = HashDesc.digest_len;
  ThreadLoadInfo->ThreadId = ImagePartLoop;
  ThreadLoadInfo->ImageOffset = ImageOffset;
  ThreadLoadInfo->SplitImageSize = SplitImageSize;
  ThreadLoadInfo->IsFinal = false;
  ThreadLoadInfo->RemainImageSize = RemainImageSize;


  if (Sha256Hash == true) {
     ThreadLoadInfo->HashCtx = &Sha256Ctx;
  } else {
     ThreadLoadInfo->HashCtx = &Sha512Ctx;
  }
  ThreadLoadInfo->Sha256HashCheck = Sha256Hash;

  ThreadVerifyInfo->part_name = part_name;
  ThreadVerifyInfo->ops = ops;
  ThreadVerifyInfo->image_buf = image_buf;
  ThreadVerifyInfo->DescDigest = DescDigest;
  ThreadVerifyInfo->DescDigestLen = HashDesc.digest_len;
  /*Initialize thread args before multithreading*/
  ThreadVerifyInfo->ThreadId = 1;
  ThreadVerifyInfo->ImageOffset = ImageOffset;
  ThreadVerifyInfo->SplitImageSize = SplitImageSize;
  ThreadVerifyInfo->IsFinal = false;
  ThreadVerifyInfo->RemainImageSize = RemainImageSize;
  if (Sha256Hash == true) {
     ThreadVerifyInfo->HashCtx = &Sha256Ctx;
  } else {
     ThreadVerifyInfo->HashCtx = &Sha512Ctx;
  }
  ThreadVerifyInfo->Sha256HashCheck = Sha256Hash;

  TStatus = CreateReaderThreads (ThreadLoadInfo, ThreadVerifyInfo);
  if (TStatus) {
    Status = TStatus;
    DEBUG ((EFI_D_INFO,
        "avb_load_verify_parallel: CreateReaderThreads Error\n"));
    goto out;
  }

 /*Wait for threads to complete*/
  KernIntf->Sem->SemWait (SemMainThread);

  if (ThreadLoadInfo->Status != AVB_SLOT_VERIFY_RESULT_OK) {
    Status = ThreadLoadInfo->Status;
    goto out;
  }

  if (ThreadVerifyInfo->Status != AVB_SLOT_VERIFY_RESULT_OK) {
    Status = ThreadVerifyInfo->Status;
    goto out;
  }

  Status = AVB_SLOT_VERIFY_RESULT_OK;
  /*Free and assign NUL to ThreadLoadVerifyInfo_1*/
  gBS->FreePool (ThreadLoadInfo);
  gBS->FreePool (ThreadVerifyInfo);

out:
  return Status;
}
