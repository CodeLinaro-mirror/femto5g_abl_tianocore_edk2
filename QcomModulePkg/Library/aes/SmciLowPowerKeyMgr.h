/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __LOWPOWER_KEY_MANAGER_H__
#define __LOWPOWER_KEY_MANAGER_H__

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include "../avb/SmciInvokeUtils.h"

typedef struct {
  UINT32 KeySize;
  UINT32 Reserved;
} ILOWPOWERKEYMANAGER_KEY_INFO;

#define CLOWPOWERKEYMANAGER_UID 0x13B

#define ILOWPOWERKEYMANAGER_HIBERNATE 1
#define ILOWPOWERKEYMANAGER_HIBERNATE_WITH_ENCRYPTION 2

#define ILOWPOWERKEYMANAGER_ERROR_INVALID_EVENT 10
#define ILOWPOWERKEYMANAGER_ERROR_INVALID_OPERATION 11
#define ILOWPOWERKEYMANAGER_ERROR_INVALID_KEYSIZE 12
#define ILOWPOWERKEYMANAGER_ERROR_KEY_GENERATION 13
#define ILOWPOWERKEYMANAGER_ERROR_RPMB_OPERATION 14

#define ILOWPOWERKEYMANAGER_OP_GETKEY 0
#define ILOWPOWERKEYMANAGER_OP_PREPARE 1
#define ILOWPOWERKEYMANAGER_OP_RESERVED 2

EFI_STATUS
ILowPowerKeyManagerRelease(Object Self);

EFI_STATUS
ILowPowerKeyManagerRetain(Object Self);

EFI_STATUS
ILowPowerKeyManagerGetKey(
  Object Self,
  UINT32 EventVal,
  VOID *KeyOutPtr,
  UINTN KeyOutLen,
  UINTN *KeyOutLenOut
);

EFI_STATUS
ILowPowerKeyManagerPrepare(
  Object Self,
  UINT32 EventVal,
  CONST ILOWPOWERKEYMANAGER_KEY_INFO *KeyInfoPtr
);

#endif // __LOWPOWER_KEY_MANAGER_H__

