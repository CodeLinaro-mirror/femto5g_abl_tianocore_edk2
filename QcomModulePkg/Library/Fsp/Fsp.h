/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __FSP_H__
#define __FSP_H__

#include <Uefi.h>

#define FSP_WRAP_KEY_MAX_SIZE  12288U
#define FSP_CEK_KEY_MAX_SIZE   512U

/**
  Load the FSP Trusted Application and acquire its SMCInvoke Object handle.

  This function must be called once before FspUnwrapWrappedKey() to ensure
  the FSP TA is resident in TrustZone and its AppObj handle is acquired.

  @retval EFI_SUCCESS           FSP TA loaded and AppObj handle acquired.
  @retval EFI_PROTOCOL_ERROR    SCM protocol not available or NULL.
  @retval Other                 Error during TA loading or handle acquisition.
**/
EFI_STATUS
FspLoadTa (
  VOID
  );

/**
  Unwrap a wrapped key blob using the FSP Trusted Application.

  This is the primary public API for the FSP library.

  @param[in]  WrapKey         Pointer to the wrapped key material buffer.
                              Must not be NULL.
  @param[in]  WrapKeyLength   Number of valid bytes in WrapKey.
                              Must be > 0 and <= FSP_WRAP_KEY_MAX_SIZE.
  @param[out] CekKey          Caller-allocated output buffer that receives
                              the unwrapped CEK key bytes. Must not be NULL.
  @param[in]  CekKeySize      Size of the CekKey buffer in bytes.
                              Must be > 0 and <= FSP_CEK_KEY_MAX_SIZE.
  @param[out] CekKeyLenOut    Pointer to a UINTN that receives the number of
                              bytes actually written to CekKey.
                              Must not be NULL.

  @retval EFI_SUCCESS           CEK key unwrapped and written to CekKey.
  @retval EFI_INVALID_PARAMETER Any pointer parameter is NULL, WrapKeyLength
                                is 0, or CekKeySize is 0.
  @retval EFI_NOT_READY         FspLoadTa() has not been called successfully;
                                gAppObj handle is not yet acquired.
  @retval EFI_PROTOCOL_ERROR    Required protocol (SCM or QSEECom) not found.
  @retval EFI_DEVICE_ERROR      TA returned a non-zero error code.
  @retval Other                 Error during TA loading or invocation.
**/
EFI_STATUS
FspUnwrapWrappedKey (
  IN  CONST UINT8  *WrapKey,
  IN  UINT64        WrapKeyLength,
  OUT VOID         *CekKey,
  IN  UINTN         CekKeySize,
  OUT UINTN        *CekKeyLenOut
  );

#endif /* __FSP_H__ */
