/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 *  with the distribution.
 *   * Neither the name of The Linux Foundation nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Changes from Qualcomm Technologies, Inc. are provided under the
 * following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __PARTIALGOODS_H__
#define __PARTIALGOODS_H__

#include <Library/Board.h>
#define MAX_CPU_CLUSTER 4
#define SUBSET_PART_CHIPINFO_BASE_REVISION 0x0000000000010002
#define SLC_LOWER_HALF_MASK 0x0000FFFF
#define SLC_UPPER_HALF_MASK 0xFFFF0000

struct SubNodeListNew {
  CONST CHAR8 *SubNodeName;  /* Subnode name */
  CONST CHAR8 *PropertyName; /* Property name */
  CONST CHAR8 *PropertyStr;  /* Property string value */
  CONST CHAR8 *ReplaceStr;   /* Replace string */
};

struct PartialGoods {
  UINT32 Val;                    /* Value for the subset */
  CONST CHAR8 *ParentNode;       /* Parent Node name*/
  struct SubNodeListNew SubNode; /* Sub node name list*/
};

struct LabelStruct {
  CONST CHAR8 *LabelName;  /* Label name */
  CONST CHAR8 *PropertyName; /* Property name */
  CONST CHAR8 *ReplaceStr;   /* Replace string */
};

struct PartialGoodsWithLabel {
  UINT32 Val;                    /* Value for the subset */
  struct LabelStruct LabelRef; /* Labels list */
};

struct PartialGoodsDelNode {
  UINT32      Val;       /* Value for the subset */
  CONST CHAR8 *NodePath; /* Full absolute path of node to delete */
};

struct PgHit {
  CONST CHAR8 *ParentPath; /* parent node path, e.g. "/soc" or "/cpus"     */
  CONST CHAR8 *NodeName;   /* table base name, e.g. "qcom,kgsl-3d0"        */
  CONST CHAR8 *PropName;   /* property to set, e.g. "status"               */
  CONST CHAR8 *Val;        /* replacement value, e.g. "no"                 */
  UINT32       TableVal;   /* entry's defect bits (for the wlan special case)*/
  INT32        Offset;     /* resolved node offset, -1 until phase 1 finds it*/
};

 STATIC CONST char *ChipInfoPartTypeStr[] = {
  [EFICHIPINFO_PART_UNKNOWN]   = "unknown",
  [EFICHIPINFO_PART_GPU]       = "gpu",
  [EFICHIPINFO_PART_VIDEO]     = "video",
  [EFICHIPINFO_PART_CAMERA]    = "camera",
  [EFICHIPINFO_PART_DISPLAY]   = "display",
  [EFICHIPINFO_PART_AUDIO]     = "audio",
  [EFICHIPINFO_PART_MODEM]     = "modem",
  [EFICHIPINFO_PART_WLAN]      = "wlan",
  [EFICHIPINFO_PART_COMP]      = "comp",
  [EFICHIPINFO_PART_SENSORS]   = "sensors",
  [EFICHIPINFO_PART_NPU]       = "npu",
  [EFICHIPINFO_PART_SPSS]      = "spss",
  [EFICHIPINFO_PART_NAV]       = "nav",
  [EFICHIPINFO_PART_COMPUTE_1] = "comp1",
  [EFICHIPINFO_PART_DISPLAY_1] = "display1",
  [EFICHIPINFO_PART_NSP]       = "nsp",
  [EFICHIPINFO_PART_EVA]       = "eva",
  [EFICHIPINFO_PART_PCIE]      = "pcie",
  [EFICHIPINFO_PART_CPU]       = "cpu",
  [EFICHIPINFO_PART_DDR]       = "ddr",
  [EFICHIPINFO_PART_SLC]       = "slc",
};

EFI_STATUS
UpdatePartialGoodsNode (VOID *fdt);

EFI_STATUS
GetPartialGoodsMMValue (VOID);

BOOLEAN
IsNodeMarkedForDeletion (CONST CHAR8 *NodePath);

EFI_STATUS
ReadMMPartialGoods (EFI_CHIPINFO_PROTOCOL *pChipInfoProtocol, UINT32 *Value);
#endif
