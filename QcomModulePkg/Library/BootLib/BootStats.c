/*
 * Copyright (c) 2016-2017, 2021, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 *  with the distribution.
 *   * Neither the name of The Linux Foundation nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 */


#include "BootStats.h"
#include "AutoGen.h"
#include "BootLinux.h"
#include "Reg.h"

#define BS_INFO_OFFSET (0x6B0)
/*
 * Fixed offsets within the Shared IMEM region where XBL writes pre-ABL
 * boot stage delta times.
 */
#define BS_PBL_DELTA_OFFSET  0x70
#define BS_XBL_DELTA_OFFSET  0x78
#define BS_UEFI_DELTA_OFFSET 0x80

STATIC UINT64 SharedImemAddress;
STATIC UINT64 MpmTimerBase;
STATIC UINT64 BsImemAddress;

VOID
BootStatsSetInitTimeStamp ()
{
  EFI_STATUS Status;

  UINTN DataSize = sizeof (SharedImemAddress);

  if (!SharedImemAddress) {
    Status =
        gRT->GetVariable ((CHAR16 *)L"Shared_IMEM_Base", &gQcomTokenSpaceGuid,
                          NULL, &DataSize, &SharedImemAddress);
    if (Status != EFI_SUCCESS) {
      DEBUG ((EFI_D_ERROR, "Failed to get Shared IMEM base, %r\n", Status));
      return;
    }
  }
    BsImemAddress = SharedImemAddress + BS_INFO_OFFSET;
    memset ((UINT64 *)BsImemAddress, 0, sizeof (UINT32) * BS_MAX);
}

VOID
BootStatsGetPreAblTime (CHAR8 *Buf, UINTN BufSize)
{
  EFI_STATUS Status;
  UINT64     PblDelta;
  UINT64     XblDelta;
  UINT64     UefiDelta;
  UINTN      DataSize = sizeof (SharedImemAddress);

  if (!Buf || BufSize == 0) {
    return;
  }
  Buf[0] = '\0';

  if (!SharedImemAddress) {
    Status =
        gRT->GetVariable ((CHAR16 *)L"Shared_IMEM_Base", &gQcomTokenSpaceGuid,
                          NULL, &DataSize, &SharedImemAddress);
    if (Status != EFI_SUCCESS) {
      DEBUG ((EFI_D_ERROR, "Failed to get Shared IMEM base, %r\n", Status));
      return;
    }
  }

  PblDelta  = *(volatile UINT64 *)(UINTN)(SharedImemAddress + BS_PBL_DELTA_OFFSET);
  XblDelta  = *(volatile UINT64 *)(UINTN)(SharedImemAddress + BS_XBL_DELTA_OFFSET);
  UefiDelta = *(volatile UINT64 *)(UINTN)(SharedImemAddress + BS_UEFI_DELTA_OFFSET);

  DEBUG ((EFI_D_VERBOSE,
          "BootStatsGetPreAblTime: pbl=%llu xbl=%llu uefi=%llu"
          " ms\n", PblDelta, XblDelta, UefiDelta));

  AsciiSPrint (Buf, BufSize,
               "1BLL:%llu,1BLE:%llu,UEFI:%llu",
               PblDelta, XblDelta, UefiDelta);
}

VOID
BootStatsSetTimeStamp (BS_ENTRY BootStatId)
{
  EFI_STATUS Status;
  UINT32 BootStatClockCount = 0;

  UINTN DataSize = sizeof (SharedImemAddress);

  if (!SharedImemAddress) {
    Status =
        gRT->GetVariable ((CHAR16 *)L"Shared_IMEM_Base", &gQcomTokenSpaceGuid,
                          NULL, &DataSize, &SharedImemAddress);
    if (Status != EFI_SUCCESS) {
      DEBUG ((EFI_D_ERROR, "Failed to get Shared IMEM base, %r\n", Status));
      return;
    }
  }

  DataSize = sizeof (MpmTimerBase);
  if (!MpmTimerBase) {
    Status =
        gRT->GetVariable ((CHAR16 *)L"MPM2_SLP_CNTR_ADDR", &gQcomTokenSpaceGuid,
                          NULL, &DataSize, &MpmTimerBase);
    if (Status != EFI_SUCCESS) {
      DEBUG (
          (EFI_D_ERROR, "Failed to get MPM Sleep counter base, %r\n", Status));
      return;
    }
  }

  if (SharedImemAddress && MpmTimerBase) {
    UINT64 BootStatImemAddress;
    BsImemAddress = SharedImemAddress + BS_INFO_OFFSET;

    if (BootStatId >= BS_MAX) {
      DEBUG (
          (EFI_D_ERROR, "Bad BootStat id: %u, Max: %u\n", BootStatId, BS_MAX));
      return;
    }

    BootStatImemAddress = BsImemAddress + (sizeof (UINT32) * BootStatId);
    BootStatClockCount = READL (MpmTimerBase);
    if (BootStatClockCount) {
      WRITEL (BootStatImemAddress, BootStatClockCount);
    }
  }
}
