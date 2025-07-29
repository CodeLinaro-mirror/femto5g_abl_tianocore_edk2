/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/* -----------------------------------------------------------------------
** Standard Types
** ----------------------------------------------------------------------- */

/* The following definitions are the same accross platforms.  This first
** group are the sanctioned types.
*/

/*----------------------------------------------------------------------------
* Common DataType Defines [dont use com_dtpyes.h for EDK complience]
*----------------------------------------------------------------------------*/
/** @defgroup efi_dal_sys_protocol EFI_DAL_SYS_PROTOCOL
 *  @ingroup SOCINFRA 
 */
 /** @defgroup  efi_dal_sys_protocol_prot PROTOCOL
 *  @ingroup efi_dal_sys_protocol 
 */ 
 /** @defgroup  efi_dal_sys_protocol_apis APIs
 *  @ingroup efi_dal_sys_protocol 
 */ 

/** @defgroup  efi_dal_sys_protocol_data DATA_STRUCTURES
 *  @ingroup efi_dal_sys_protocol 
 */ 

/** @ingroup efi_dal_sys_protocol_data
@{ */

#ifndef DALSTDDEF_H
#define DALSTDDEF_H

typedef	 UINT32      uint32_t;
typedef  UINT64      uint64_t;
typedef  INT32       int32_t;
typedef  INT64       int64_t;
typedef  UINT8       uint8_t;
typedef  INT8        int8_t; 

typedef  UINT32      uint32;
typedef  UINT64      uint64;
typedef  UINT16      uint16;
typedef  UINT8       uint8;
typedef  INT32       int32;
typedef  INT64       int64;
typedef  INT8        int8; 

typedef  BOOLEAN            boolean; 
typedef unsigned char  byte;
typedef __builtin_va_list va_list;
/** @} */ /* end_ingroup efi_dal_sys_protocol_data */
/*----------------------------------------------------------------------------*/

#if defined(__GNUC__)
#define __int64 long long
#endif

#ifndef _UINT64_DEFINED
typedef  unsigned __int64   uint64;      /* Unsigned 64 bit value */
#define _UINT64_DEFINED
#endif

#ifndef _INT64_DEFINED
typedef  __int64            int64;       /* Signed 64 bit value */
#define _INT64_DEFINED
#endif

/** @ingroup efi_dal_sys_protocol_data
@{ */
typedef uint32  DALBOOL;
typedef uint32  DALDEVICEID;
typedef uint32  DalPowerCmd;
typedef uint32  DalPowerDomain;
typedef uint32  DalSysReq;
typedef UINTN   DALHandle;
typedef int     DALResult;
typedef void  * DALEnvHandle;
typedef void  * DALSYSEventHandle;
typedef UINTN  DALSYSMemAddr;
typedef UINTN  DALMemAddr;

typedef uint32  DALInterfaceVersion;
/** @} */ /* end_ingroup efi_dal_sys_protocol_data */

// common macro for dalsys objects
#define INHERIT_DALSYS_BASE_OBJ()\
   uint32 dwObjInfo;\
   uint32 hOwnerProc;\
   DALSYSMemAddr thisVirtualAddr


/** @ingroup efi_dal_sys_protocol_data
  @par Summary
   DAL System Objects which may be communicated as DDI params
  @par Parameters
*/
typedef unsigned char * DALDDIParamPtr;

typedef struct DALEventObject DALEventObject;
/** @ingroup efi_dal_sys_protocol_data
  @par Summary
   DALEventObject struct.
  @par Parameters
*/
struct DALEventObject
{
    uint32 obj[8];
};
typedef DALEventObject * DALEventHandle;

/** @ingroup efi_dal_sys_protocol_data
  @par Summary
   DALMemObject struct.
  @par Parameters
*/
typedef struct _DALMemObject
{
   uint32        memAttributes;
   uint32        sysObjInfo[2];
   uint32        dwLen;
   uint32        ownerVirtAddr;
   uint32        virtAddr;
   uint32        physAddr;
}
DALMemObject;

/** @ingroup efi_dal_sys_protocol_data
  @par Summary
   DALDDIMemBufDesc struct.
  @par Parameters
*/
typedef struct _DALDDIMemBufDesc
{
   uint32       dwOffset;
   uint32       dwLen;
   uint32       dwUser;
}
DALDDIMemBufDesc;

/** @ingroup efi_dal_sys_protocol_data
  @par Summary
  lower 8-bits of dwFlags reserved.
  @par Parameters
*/
typedef struct _DALDDIMemDescList
{
   uint32          dwFlags;
   uint32          dwNumBufs;
   DALDDIMemBufDesc   bufList[1];  // variable length
}
DALDDIMemDescList;


/** @ingroup efi_dal_sys_protocol_data
  @par Summary
  The following is memory descriptor syntax which can be used
  within an environment. (i.e. shared dalsys)..
  @par Parameters
*/
typedef struct DALSysMemDescBuf DALSysMemDescBuf;
struct DALSysMemDescBuf
{
   DALSYSMemAddr VirtualAddr;
   DALSYSMemAddr PhysicalAddr;
   uint32 size;
   uint32 user;
};

/** @ingroup efi_dal_sys_protocol_data
  @par Summary
  DALSysMemDescList struct.
  @par Parameters
*/
typedef struct DALSysMemDescList DALSysMemDescList;
struct DALSysMemDescList
{
   INHERIT_DALSYS_BASE_OBJ();
   DALSYSMemAddr PhysicalAddr;
   DALSYSMemAddr VirtualAddr;
   uint32        dwCurBufIdx;
   uint32        dwNumDescBufs;
   DALSysMemDescBuf BufInfo[1];
};

/* -----------------------------------------------------------------------
** Some Constants
** ----------------------------------------------------------------------- */
#ifndef TRUE
#define TRUE   1   /* Boolean true value. */
#endif

#ifndef FALSE
#define FALSE  0   /* Boolean false value. */
#endif

#ifndef NULL
#define NULL  0
#endif

#define DALSYS_MEM_INVALID_ADDR  (DALSYSMemAddr)(~0)

#endif /* #ifndef DALSTDDEF_H */

