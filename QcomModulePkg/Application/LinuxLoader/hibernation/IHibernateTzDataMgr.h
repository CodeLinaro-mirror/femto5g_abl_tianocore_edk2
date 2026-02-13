/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifdef ENABLE_C_HEADER
#include <stdint.h>
#endif
#include "SmciInvokeUtils.h"

#define IHibernateTzDataMgr_ERROR_GENERIC INT32_C(10)
#define IHibernateTzDataMgr_ERROR_DATA_READ_FAIL INT32_C(11)
#define IHibernateTzDataMgr_ERROR_DATA_WRITE_FAIL INT32_C(12)
#define IHibernateTzDataMgr_ERROR_CB_REGISER_FAILED INT32_C(13)
#define IHibernateTzDataMgr_ERROR_DATA_NOT_FOUND INT32_C(14)
#define IHibernateTzDataMgr_ERROR_DATA_NOT_CACHED INT32_C(15)
#define IHibernateTzDataMgr_ERROR_INVALID_DATASIZE INT32_C(16)
#define IHibernateTzDataMgr_ERROR_DATA_ENC_FAILED INT32_C(17)
#define IHibernateTzDataMgr_ERROR_DATA_DEC_FAILED INT32_C(18)
#define IHibernateTzDataMgr_ERROR_KEYGEN_FAILED INT32_C(19)
#define IHibernateTzDataMgr_ERROR_DATA_PRESENT INT32_C(20)

#define IHibernateTzDataMgr_OP_getKey 4
#define IHibernateTzDataMgr_OP_getData 3

static inline int32_t
IHibernateTzDataMgr_release(Object self)
{
    return ObjectInvokeFunc(self, Object_OP_Release, 0, 0);
}

static inline int32_t
IHibernateTzDataMgr_retain(Object self)
{
    return ObjectInvokeFunc(self, Object_OP_Retain, 0, 0);
}

/*
*
*  This method will generate hw bound random key.
*
*  @param[out] buffer key data
*
*  @return
*  Object_OK on success.\n
*
*/
static inline int32_t IHibernateTzDataMgr_getKey(Object self, void *key_ptr, size_t key_len, size_t *key_lenout)
{
    ObjectArg a[] = {
        {.ArgB = (ObjectBuf) { key_ptr, key_len * sizeof(uint8_t) } },
    };

    int32_t result = ObjectInvokeFunc(self, IHibernateTzDataMgr_OP_getKey, a, ObjectCounts_pack(0, 1, 0, 0));

    *key_lenout = a[0].ArgB.Size / sizeof(uint8_t);
    return result;
}

/*
 *  This method will get the data for the requesting client which was saved before
 *  hibernate entry. This method is expected to be called after hibernate exit.
 *  Client ID is a unique identifier for each use case. TZ service can use its UID
 *  as client ID. HLOS service needs to make sure they are using unique ID which
 *  does not conflict with other use case.
 *
 *  @param[in] uint32 client ID
 *  @param[out] buffer key data
 *
 *  @return
 *  Object_OK on success.\n
 *
 */
static inline int32_t IHibernateTzDataMgr_getData(Object self,
              uint32_t client_ID_val, void *key_ptr, size_t key_len,
              size_t *key_lenout)
{
	ObjectArg a[] = {
		{.ArgB = (ObjectBuf) { &client_ID_val, sizeof(uint32_t)}
		  },
		{.ArgB = (ObjectBuf) { key_ptr, key_len * sizeof(uint8_t)}
		  },
	};

	int32_t result = ObjectInvokeFunc(self, IHibernateTzDataMgr_OP_getData, a,
			                  ObjectCounts_pack(1, 1, 0, 0));

	*key_lenout = a[1].ArgB.Size / sizeof(uint8_t);
	return result;
}
