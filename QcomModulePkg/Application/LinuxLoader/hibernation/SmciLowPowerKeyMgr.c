/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "SmciLowPowerKeyMgr.h"

EFI_STATUS
ILowPowerKeyManagerRelease(SMCI_OBJECT Self)
{
    return smci_object_invoke(Self, SMCI_OBJECT_OP_RELEASE, 0, 0);
}

EFI_STATUS
ILowPowerKeyManagerRetain(SMCI_OBJECT Self)
{
    return smci_object_invoke(Self, SMCI_OBJECT_OP_RETAIN, 0, 0);
}

EFI_STATUS
ILowPowerKeyManagerGetKey(
    SMCI_OBJECT Self,
    UINT32 EventVal,
    VOID *KeyOutPtr,
    UINTN KeyOutLen,
    UINTN *KeyOutLenOut
)
{
    EFI_STATUS Result = EFI_SUCCESS;
    union smci_object_arg A[2] = {{{0, 0}}};

    A[0].b = (struct smci_object_buf){ &EventVal, sizeof(UINT32) };
    A[1].b = (struct smci_object_buf){ KeyOutPtr, KeyOutLen };

    Result = smci_object_invoke(Self, ILOWPOWERKEYMANAGER_OP_GETKEY, A,
        SMCI_OBJECT_COUNTS_PACK(1, 1, 0, 0));

    *KeyOutLenOut = A[1].b.size;

    return Result;
}

EFI_STATUS
ILowPowerKeyManagerPrepare(
    SMCI_OBJECT Self,
    UINT32 EventVal,
    CONST ILOWPOWERKEYMANAGER_KEY_INFO *KeyInfoPtr
)
{
    union smci_object_arg A[1] = {{{0, 0}}};
    struct {
        UINT32 Event;
        ILOWPOWERKEYMANAGER_KEY_INFO KeyInfo;
    } Input = {0};

    Input.Event = EventVal;
    Input.KeyInfo = *KeyInfoPtr;

    A[0].b = (struct smci_object_buf){ &Input, sizeof(Input) };

    return smci_object_invoke(Self, ILOWPOWERKEYMANAGER_OP_PREPARE, A,
        SMCI_OBJECT_COUNTS_PACK(1, 0, 0, 0));
}

