/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __ALLOWED_VBMETA_SYS_PUBLIC_KEYS_H__
#define __ALLOWED_VBMETA_SYS_PUBLIC_KEYS_H__

#include "keys/OEMVbmetaSysPublicKey.h"
#include "keys/GSIVbmetaSysPublicKeys.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) sizeof (a) / sizeof (*a)
#endif

// Index 0 should always be OEM key.
static const char* AllowedVbmetaSysPubKeys[] =
    { OEMVbmetaSysPublicKey, GSIVbmetaSysPublicKeyW };
static const unsigned int AllowedVbmetaSysPubKeysLen[] =
    { ARRAY_SIZE(OEMVbmetaSysPublicKey), ARRAY_SIZE(GSIVbmetaSysPublicKeyW) };
static const unsigned int AllowedVbmetaSysPubKeyCount =
    sizeof(AllowedVbmetaSysPubKeys) / sizeof(AllowedVbmetaSysPubKeys[0]);

#endif // __ALLOWED_VBMETA_SYS_PUBLIC_KEYS_H__
