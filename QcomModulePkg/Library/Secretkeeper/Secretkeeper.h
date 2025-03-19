/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __SECRETKEEPER_H__
#define __SECRETKEEPER_H__

#include <Uefi.h>

#define SK_MAX_PUB_KEY_SIZE 128

EFI_STATUS
SecretkeeperStartApp (VOID);

// This returns a CBOR encoded Cose Public Key with the COSE key material
// Offset: This is the response header size that has to be skipped to identify
// the start of the COSE Public key,
EFI_STATUS
SecretkeeperGetCosePublicKey (UINT8 *CosePubKey, UINT32 CosePubKeyLen,
                              UINT32 *RspLenOut, UINT32 *Offset);

#endif
