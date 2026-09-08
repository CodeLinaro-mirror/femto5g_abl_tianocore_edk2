/** @file
 *
 *  Copyright (c) 2011-2015, ARM Limited. All rights reserved.
 *
 *  This program and the accompanying materials
 *  are licensed and made available under the terms and conditions of the BSD
 *License
 *  which accompanies this distribution.  The full text of the license may be
 *found at
 *  http://opensource.org/licenses/bsd-license.php
 *
 *  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR
 *IMPLIED.
 */

/*
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "ShutdownServices.h"

#include <FastbootLib/FastbootCmds.h>
#include <Guid/ArmMpCoreInfo.h>
#include <Guid/FileInfo.h>
#include <Guid/GlobalVariable.h>
#include <Library/ArmLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/HobLib.h>
#include <Library/LinuxLoaderLib.h>
#include <Library/PrintLib.h>
#include <Library/SerialPortLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

EFI_STATUS ShutdownUefiBootServices (VOID)
{
  EFI_STATUS Status;
  UINTN MemoryMapSize;
  EFI_MEMORY_DESCRIPTOR *MemoryMap;
  UINTN MapKey;
  UINTN DescriptorSize;
  UINT32 DescriptorVersion;
  UINTN Pages;

  WaitForFlashFinished ();

  MemoryMap = NULL;
  MemoryMapSize = 0;
  Pages = 0;

  do {
    Status = gBS->GetMemoryMap (&MemoryMapSize, MemoryMap, &MapKey,
                                &DescriptorSize, &DescriptorVersion);
    if (Status == EFI_BUFFER_TOO_SMALL) {

      Pages = EFI_SIZE_TO_PAGES (MemoryMapSize) + 1;
      MemoryMap = AllocatePages (Pages);
      if (!MemoryMap) {
        DEBUG ((EFI_D_ERROR, "Failed to allocate pages for memory map\n"));
        return EFI_OUT_OF_RESOURCES;
      }

      //
      // Get System MemoryMap
      //
      Status = gBS->GetMemoryMap (&MemoryMapSize, MemoryMap, &MapKey,
                                  &DescriptorSize, &DescriptorVersion);
    }

    // Don't do anything between the GetMemoryMap() and ExitBootServices()
    if (!EFI_ERROR (Status)) {
      Status = gBS->ExitBootServices (gImageHandle, MapKey);
      if (EFI_ERROR (Status)) {
        FreePages (MemoryMap, Pages);
        MemoryMap = NULL;
        MemoryMapSize = 0;
      }
    }
  } while (EFI_ERROR (Status));

  return Status;
}

#ifdef DISABLE_KERNEL_PROTOCOL
EFI_STATUS PreparePlatformHardware (VOID)
{
  ArmDisableBranchPrediction ();

  /* ArmDisableAllExceptions */
  ArmDisableInterrupts ();
  ArmDisableAsynchronousAbort ();

  ArmCleanInvalidateDataCache ();
  ArmCleanDataCache ();
  ArmInvalidateInstructionCache ();

  ArmDisableDataCache ();
  ArmDisableInstructionCache ();
  ArmDisableMmu ();
  ArmInvalidateTlb ();
  return EFI_SUCCESS;
}
#else
EFI_STATUS PreparePlatformHardware (EFI_KERNEL_PROTOCOL *KernIntf,
    VOID *KernelLoadAddr, UINTN KernelSizeActual, VOID *RamdiskLoadAddr,
    UINTN RamdiskSizeActual, VOID *DeviceTreeLoadAddr,
    UINTN DeviceTreeSizeActual, VOID *CallerStackCurrent, UINTN CallerStackBase)
{
  Thread *ThreadNum;
  VOID *StackBase;
  VOID **StackCurrent;

  if (KernIntf->Version >= EFI_KERNEL_PROTOCOL_VERSION) {
    ThreadNum = KernIntf->Thread->GetCurrentThread ();
    StackCurrent = KernIntf->Thread->ThreadGetUnsafeSPCurrent (ThreadNum);
    StackBase = KernIntf->Thread->ThreadGetUnsafeSPBase (ThreadNum);
  }

  ArmDisableBranchPrediction ();

  /* ArmDisableAllExceptions */
  ArmDisableInterrupts ();
  ArmDisableAsynchronousAbort ();

  // Clean, invalidate, disable data cache
  WriteBackInvalidateDataCacheRange (KernelLoadAddr, KernelSizeActual);
  WriteBackInvalidateDataCacheRange (RamdiskLoadAddr, RamdiskSizeActual);
  WriteBackInvalidateDataCacheRange (DeviceTreeLoadAddr, DeviceTreeSizeActual);
  if (KernIntf->Version >= EFI_KERNEL_PROTOCOL_VERSION) {
    WriteBackInvalidateDataCacheRange ((VOID *)StackCurrent,
                  (UINTN)StackBase - (UINTN)StackCurrent);
    WriteBackInvalidateDataCacheRange (CallerStackCurrent,
                  CallerStackBase - (UINTN)CallerStackCurrent);
  }

  ArmCleanDataCache ();
  ArmInvalidateInstructionCache ();

  ArmDisableDataCache ();
  ArmDisableInstructionCache ();
  ArmDisableMmu ();
  ArmInvalidateTlb ();
  return EFI_SUCCESS;
}
#endif

VOID
RebootDevice (UINT8 RebootReason)
{
  ResetDataType ResetData;
  EFI_STATUS Status = EFI_INVALID_PARAMETER;

  WaitForFlashFinished ();
  StrnCpyS (ResetData.DataBuffer, ARRAY_SIZE (ResetData.DataBuffer),
            (CONST CHAR16 *)STR_RESET_PARAM, ARRAY_SIZE (STR_RESET_PARAM) - 1);
  ResetData.Bdata = RebootReason;

  if (RebootReason == NORMAL_MODE ||
      RebootReason == FASTBOOT_MODE ||
      RebootReason == RECOVERY_MODE) {
    /* SDAM 0x7148 layout:
     * bit[7]    -> Unused/reserved
     * bit[6]    -> Intentional reboot flag (0 = unintentional, 1 = intentional)
     * bits[5:0] -> Reboot reason code
     * Set bit[6] for intentional reboot modes.
     */
    ResetData.Bdata = RebootReason | (1 << INTENT_BIT_SHIFT);
    DEBUG ((EFI_D_INFO,
            "Adding intentional bit RebootReason 0x%x\n",
            ResetData.Bdata));
    Status = EFI_SUCCESS;
  }
  if (RebootReason == EMERGENCY_DLOAD)
    gRT->ResetSystem (EfiResetPlatformSpecific, EFI_SUCCESS,
                      StrSize ((CONST CHAR16 *)STR_RESET_PLAT_SPECIFIC_EDL),
                      STR_RESET_PLAT_SPECIFIC_EDL);

  gRT->ResetSystem (EfiResetCold, Status, sizeof (ResetDataType),
                    (VOID *)&ResetData);
}

VOID ShutdownDevice (VOID)
{
  EFI_STATUS Status = EFI_INVALID_PARAMETER;

  WaitForFlashFinished ();

  gRT->ResetSystem (EfiResetShutdown, Status, 0, NULL);

  /* Flow never comes here and is fatal if it comes here.*/
  ASSERT (0);
}
