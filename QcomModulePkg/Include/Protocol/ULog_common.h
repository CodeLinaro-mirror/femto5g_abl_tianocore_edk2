/*=============================================================================
  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  SPDX-License-Identifier: BSD-3-Clause-Clear
=============================================================================*/

#ifndef ULOG_COMMON_H
#define ULOG_COMMON_H

#include "DALStdDef.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @cond */
/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/
typedef  void *   ULogHandle;
typedef  int32    ULogResult;


//ULog constants
#define ULOG_MAX_NAME_SIZE 23
/** @endcond */

// Error Codes
// These Error codes are return values from ULog functions.
// ULog functions additionally use the standard DAL return codes.
// DAL_SUCCESS is the return value to test for success in most operations.
// DAL_ERROR and the values below are returned by functions.
// Error values 10,000 to 20,000 are reserved for ULog Diag Plugin usage.
typedef enum
{
  ULOG_ERR_INVALIDNAME = 1,     /**< Name is invalid. */
  ULOG_ERR_INVALIDPARAMETER,    /**< Parameter is invalid. */
  ULOG_ERR_ALREADYCONNECTED,    /**< Handle is already connected to a log. */
  ULOG_ERR_ALREADYCREATED,      /**< Log has already been created. */
  ULOG_ERR_NOTCONNECTED,        /**< Handle is not connected to a log. */
  ULOG_ERR_ALREADYENABLED,      /**< Log is already enabled. */
  ULOG_ERR_INITINCOMPLETE,      /**< Initialization is incomplete. */
  ULOG_ERR_READONLY,            /**< Data is read only. */
  ULOG_ERR_INVALIDHANDLE,       /**< Handle is invalid. */
  ULOG_ERR_MALLOC,              /**< Memory allocation failed. */
  ULOG_ERR_ASSIGN,              /**< Error occurred setting memory in the
                                     ULog. */
  ULOG_ERR_INSUFFICIENT_BUFFER, /**< Buffer provided to a read is insufficient;
                                     not even a single item can be copied into
                                     the buffer. */
  ULOG_ERR_NAMENOTFOUND,        /**< Cannot find a log with the specified
                                     name. */
  ULOG_ERR_MISC,                /**< Miscellaneous category for errors that
                                     do not fit the other categories. */
  ULOG_ERR_OVERRUN,             /**< Overrun condition detected while reading */
  ULOG_ERR_NOTSUPPORTED,        /**< Request is not supported. */
} ULOG_ERRORS;

/** @cond */
// Buffer Sizes that can be queried in order to manually
// allocate memory for a log.
// See: ULogCore_ValueQuery
typedef enum
{
  // These queries do not require a log handle
  ULOG_VALUE_BUFFER,
  ULOG_VALUE_SHARED_HEADER,
  ULOG_VALUE_READCORE,
  ULOG_VALUE_WRITECORE,
  ULOG_VALUE_LOCKCORE,

  // Below here the queries require a log handle
  ULOG_VALUE_LOG_BASE = 0x100,
  ULOG_VALUE_LOG_READFLAGS = ULOG_VALUE_LOG_BASE,
} ULOG_VALUE_ID;


// Lock Types
// Each log can select the locking mechanism used to protect data
// accesses.  It is up to the log definer to select the appropriate
// lock type depending on the use cases.
typedef enum
{
  ULOG_LOCK_NONE = 0,              // No locking is provided for this
                                   // log.  The user is responsible for
                                   // providing mutual exclusion if logging
                                   // from multiple threads.

  ULOG_LOCK_OS,                    // An OS lock is provided.  The log can
                                   // be written from more than one thread.
                                   // OS locks are assumed to work across
                                   // cores in the case where one OS is
                                   // managing all cores.

  ULOG_LOCK_SPIN,                  // Spinlocks are NOT currently supported.
                                   // An error will be returned.

  ULOG_LOCK_INT,                   // An interrupt lock is provided.  The log can
                                   // be written from more than one thread.
                                   // Interrupt locks are NOT multi-core safe.

  ULOG_LOCK_UINT32 = 0x7FFFFFFF    // force enum to 32-bits
} ULOG_LOCK_TYPE;


// Memory Type
// Control how memory is provided for the log.
typedef enum
{
  ULOG_ALLOC_LOCAL = 0x01,         // Memory will be allocated using
                                   // DALSYS_Malloc.

  ULOG_ALLOC_MANUAL = 0x04,        // Memory allocation is handled
                                   // manually, and is provided to ULog
                                   // through the ULog_MemoryAssign
                                   // function.

  ULOG_ALLOC_TYPE_MASK = 0xFF,     // Mask for all possible allocation bit
                                   // types.

  ULOG_ALLOC_UINT32 = 0x7FFFFFFF    // force enum to 32-bits
} ULOG_ALLOC_TYPE;


// Memory Usage
// Control the amount and type of memory access available to the log.
typedef enum
{
  ULOG_MEMORY_USAGE_FULLACCESS = 0x0300,    // The processor is expected to read and
                                            // write from this log.

  ULOG_MEMORY_USAGE_READABLE = 0x0100,      // The processor will only read from this
                                            // log.

  ULOG_MEMORY_USAGE_WRITEABLE = 0x0200,     // The processor will only write to this
                                            // log.

  ULOG_MEMORY_USAGE_UINT32 = 0x7FFFFFFF    // force enum to 32-bits
} ULOG_MEMORY_USAGE_TYPE;


// Memory Configuration
// Control if the memory is configured and used as shared
typedef enum
{
  ULOG_MEMORY_CONFIG_LOCAL = 0x010000,        // Header information is stored in local
                                              // memory.  Because the log is used only
                                              // by one processor (or a symmetrical
                                              // multi-processing system), there is no
                                              // need to store the log in any other
                                              // location.

  ULOG_MEMORY_CONFIG_SHARED = 0x020000,       // Header information is stored in the
                                              // allocated buffer memory. This configuration
                                              // is not currently supported and will
                                              // return ULOG_ERR_NOTSUPPORTED

  ULOG_MEMORY_CONFIG_UINT32 = 0x7FFFFFFF    // force enum to 32-bits
} ULOG_MEMORY_CONFIG_TYPE;


// Memory type
// Memory type is a parameter specifying the allocation, usage, and configuration
// of the memory used by the log.  Each log must specify one of each of the
// ULOG_ALLOC_TYPE, ULOG_MEMORY_USAGE_TYPE, and ULOG_MEMORY_CONFIG_TYPE values.
// It is up to the log creator to select the appropriate memory type.
// Standard memory allocation types are provided below.
// Most logs will be created as local logs.
#define ULOG_MEMORY_LOCAL        (ULOG_MEMORY_CONFIG_LOCAL | ULOG_MEMORY_USAGE_FULLACCESS | ULOG_ALLOC_LOCAL)
#define ULOG_MEMORY_LOCAL_MANUAL (ULOG_MEMORY_CONFIG_LOCAL | ULOG_MEMORY_USAGE_FULLACCESS | ULOG_ALLOC_MANUAL)

//Shared Memory type logs are not supported and will return ULOG_ERR_NOTSUPPORTED
#define ULOG_MEMORY_SHARED       (ULOG_MEMORY_CONFIG_SHARED | ULOG_MEMORY_USAGE_FULLACCESS | ULOG_ALLOC_MANUAL)


//Optional timestamp source selection, if NULL default will be used
typedef uint64(*ULOG_ALT_TS_SRC)(void);		

// Interface Types
// Interface Types are used by the logging core.
// Generally logging into a log is only of one type, but as of 7/2012 messages
// of either type (Raw or RealTime) can be written to a ULog.
// Data should only be logged into the buffer using the Frontend APIs.
typedef enum
{
  ULOG_INTERFACE_INVALID = 0,      // Invalid Log Type

  ULOG_INTERFACE_RAW,              // Raw data logging provided in
                                   // ULogFront.h

  ULOG_INTERFACE_REALTIME,         // Real-Time data logging provided in
                                   // ULogFront.h.

  ULOG_INTERFACE_UINT32 = 0x7FFFFFFF    // force enum to 32-bits
} ULOG_INTERFACE_TYPE;


// ULog Message Format Types
// These values are stored within every message to indicate what the message
// type being recorded is.
typedef enum
{
  ULOG_REALTIME_SUBTYPE_RESERVED_FOR_RAW = 0,  /* RAW logs set the msg format to 0*/
  ULOG_REALTIME_SUBTYPE_PRINTF,
  ULOG_REALTIME_SUBTYPE_BYTEDATA,
  ULOG_REALTIME_SUBTYPE_STRINGDATA,
  ULOG_REALTIME_SUBTYPE_WORDDATA,
  ULOG_REALTIME_SUBTYPE_CSVDATA,
  ULOG_REALTIME_SUBTYPE_VECTOR,
  ULOG_REALTIME_SUBTYPE_MULTIPART,
  ULOG_REALTIME_SUBTYPE_MULTIPART_STREAM_END,  /*only used for streaming QDSS or ALT data streams*/

  ULOG_SUBTYPE_REALTIME_TOKENIZED_STRING,
  ULOG_SUBTYPE_RESERVED1,
  ULOG_SUBTYPE_RESERVED2,
  ULOG_SUBTYPE_RESERVED3,
  ULOG_SUBTYPE_RAW8,                /* Raw output formatted as uint8s  */
  ULOG_SUBTYPE_RAW16,               /* Raw output formatted as uint16s */
  ULOG_SUBTYPE_RAW32,               /* Raw output formatted as uint32s */

  ULOG_REALTIME_SUBTYPE_UINT32 = 0x7FFFFFFF    // force enum to 32-bits
} ULOG_REALTIME_SUBTYPES;


//the 1st word of a msg is len/arg bitfield/Format string size indicator bit/fmt
/*sga -- remove ULOG_LEN_MASK?? -- not referenced in any code, could see justification for 
  keeping it around since other masks are not publicily available, and persay
  new users (F3 diag for micro ulog) want to do some parsing of there own,
  they may require this field */
#define ULOG_LEN_MASK 0xffff0000
#define ULOG_FMT_MASK 0x0000000f 

//adding the below MASKs for new values stored in the 1st word (added under same
//rational as described in the above comment
#define ULOG_ARG_BF_MASK         0x0000FFC0
#define ULOG_FRMT_STR_SIZE_MASK  0x00000020


// Log Status Indicators
// Log status is returned as the first word from the ULog_Read functions.
typedef enum
{
  ULOG_LOG_WRAPPED = 0x0001,        // Not an error condition. Just an
                                    // indicator that the log has wrapped
                                    // at least once.

  ULOG_ERR_LARGEMSG = 0x0002,        // A message too large for the buffer
                                     // was logged, and therefore was dropped.
                                     // This indicates the message was too
                                     // large for the entire buffer.

  ULOG_ERR_LARGEMSGOUT = 0x0004,     // A message too large for the output
                                     // buffer was dropped because it could
                                     // not fit.

  ULOG_ERR_RESET = 0x0008,           // The log has been reset at least once.

  ULOG_ERR_UINT32 = 0x7FFFFFFF    // force enum to 32-bits
} ULOG_WRITER_STATUS_TYPE;


// ReadFlags Indicators
// ReadFlags is returned from a query.
typedef enum
{
  ULOG_RD_FLAG_REWIND = 0x0001,        // The read pointer was rewound
                                       // by the Autorewind feature.
  ULOG_RD_FLAG_UINT32 = 0x7FFFFFFF    // force enum to 32-bits
} ULOG_RD_FLAG_TYPE;


// Attribute Types
// Logs can be customized with the following attributes.
typedef enum
{
  ULOG_ATTRIBUTE_READ_AUTOREWIND = 0x1,        // Rewind the read pointer for this log
                                               // to the earliest message after the entire
                                               // log has been read, and zero bytes have
                                               // been transferred.

  ULOG_ATTRIBUTE_UINT32 = 0x7FFFFFFF    // force enum to 32-bits
} ULOG_ATTRIBUTE_TYPE;

// ULog Core VTable Struct
// Different types of write operations (Tee, STM, alternate) can be defined
// using different functions in this table.
typedef struct
{
  ULogResult (*write) (ULogHandle h, uint32 firstMsgCount, const char * firstMsgContent, uint32 secondMsgCount, const char * secondMsgContent, ULOG_INTERFACE_TYPE interfaceType);
  DALBOOL (*multipartMsgBegin) (ULogHandle h);
  void (*multipartMsgEnd) (ULogHandle h);
} ULOG_CORE_VTABLE;

typedef struct {
  CHAR8 separator;          // Separator between bootcount, timestamp, and message
  BOOLEAN PrintTimestamp;   // Used to enable or disable bootcount+timestamp printing
  BOOLEAN TrimNewline;      // Used to enable or disable replacing a terminating \n\n with \n
} ULOG_CONFIG_TYPE;

// The following ULog Diag plugin ID's are explicitly reserved for current
// and future use. 
#define ULOG_DIAG_PLUGIN_ID_NPA         1
#define ULOG_DIAG_PLUGIN_ID_MCA         2
#define ULOG_DIAG_PLUGIN_ID_RESERVED3   3
#define ULOG_DIAG_PLUGIN_ID_RESERVED4   4
#define ULOG_DIAG_PLUGIN_ID_RESERVED5   5
#define ULOG_DIAG_PLUGIN_ID_RESERVED6   6
#define ULOG_DIAG_PLUGIN_ID_RESERVED7   7
#define ULOG_DIAG_PLUGIN_ID_RESERVED8   8
/** @endcond */

#ifdef __cplusplus
}
#endif

#endif // ULOG_COMMON_H

