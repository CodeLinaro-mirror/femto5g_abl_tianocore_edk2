/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <stdint.h>
// TODO: Replace with actual SmcInvoke implementation.
#include "SmciInvokeUtils.h"

typedef Object ISecretKeeper;

#define ISecretKeeper_ERROR_NOT_SUPPORTED INT32_C(10)
#define ISecretKeeper_ERROR_CONTEXT_NOT_AVAILABLE INT32_C(11)

#define ISecretKeeper_OP_process_bootloader 1

/*
*
*  This function provides a landing point to forward requests from ABL to
*  SecretKeeper TA.
*
*  @param[in] request: Buffer containing get identity key request to be
*                      forwarded to secretkeeper_core::process_bootloader
*  @param[out] response: Buffer containing identity key to be forwarded to ABL
*
*/
static inline INT32 ISecretKeeper_process_bootloader (Object Self,
                                                      CONST VOID *RequestPtr,
                                                      SizeT RequestLen,
                                                      VOID *ResponsePtr,
                                                      SizeT ResponseLen,
                                                      SizeT *ResponseLenOut)
{
    ObjectArg ArgA[] = {
        {.ArgBi = (ObjectBufIn) { RequestPtr, RequestLen * sizeof (UINT8) } },
        {.ArgB = (ObjectBuf) { ResponsePtr, ResponseLen * sizeof (UINT8) } },
    };

    INT32 Result = ObjectInvokeFunc (Self, ISecretKeeper_OP_process_bootloader,
                                     ArgA, ObjectCounts_pack (1, 1, 0, 0));
    *ResponseLenOut = ArgA[1].ArgB.Size / sizeof (UINT8);

    return Result;
}
