/*
 * Changes from Qualcomm Technologies, Inc. are provided
 * under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __AVB_POPULATE_BCC_PARAMS_H__
#define __AVB_POPULATE_BCC_PARAMS_H__

#include "libavb/avb_slot_verify.h"
//#include <Library/QcBcc.h>
#include "Library/KeymasterClient.h"

EFI_STATUS
PopulateBccParams (AvbSlotVerifyData *SlotData,
                   BOOLEAN BootIntoRecovery,
                   BccParams_t *bcc_params);

#endif /* __AVB_POPULATE_BCC_PARAMS_H__ */
