/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __SAILLIB_H__
#define __SAILLIB_H__

#include <Library/BootLinux.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/Debug.h>
#include <Uefi/UefiSpec.h>
#include <Library/LinuxLoaderLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Uefi.h>

BOOLEAN CheckSailPartition (CONST CHAR8 *Arg);
EFI_STATUS SailFlash (CONST CHAR8 *Arg, VOID *Data, UINT32 Sz);
EFI_STATUS SailBoot (VOID *Data, UINT32 Size, BOOLEAN Fastboot);

#endif
