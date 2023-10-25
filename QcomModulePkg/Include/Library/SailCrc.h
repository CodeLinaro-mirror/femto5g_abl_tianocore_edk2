/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2023 Bastian Molkenthin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *THE SOFTWARE.
 */

#ifndef __SAILCRC_H__
#define __SAILCRC_H__

#include <Library/DebugLib.h>
#include <Library/Debug.h>
#include <Library/DeviceInfo.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>

#define crcGEN_CHANNEL_CRC8     0U
#define crcETH_CHANNEL_CRC32    1U

/* Type definition for the crc configuration */
typedef struct {
  UINT8         UcChannelId;
  CONST UINT8   *pCRCtable;
  UINT8         ucInitialValue;
  UINT8         ucFinalXORValue;
  BOOLEAN       InputDataReflected;
  BOOLEAN       ResultDataReflected;
}crc8ConfigType;

typedef struct {
  UINT8         UcChannelId;
  CONST UINT32  *pCRCtable;
  UINT32        ulInitialValue;
  UINT32        ulFinalXORValue;
  BOOLEAN       InputDataReflected;
  BOOLEAN       ResultDataReflected;
}crc32ConfigType;

typedef struct {
  union {
    CONST crc8ConfigType    *Ptr8;
    CONST crc32ConfigType   *Ptr32;
  }Crc;
}crcCoreConfigType;

/*----------------------------------------------------------------------------
 *                      PRIVATE Config macros
 *--------------------------------------------------------------------------*/
#define crcMAX_CH_ID            2U
#define crcMAX_ELEMENT_CRC8     256U
#define crcMAX_ELEMENT_CRC32    256U
/*----------------------------------------------------------------------------
 *                      PRIVATE Config Lib Data
 *--------------------------------------------------------------------------*/
/**
 * CRC8 SAE J1850 pre-calculated Table-Driven table uses following parameters:
 * CRC result width         8 bits
 * Initial value            0xFF
 * Input data reflected     False
 * Result data reflected    False
 * XOR value                0xFF
 * Polynomial               0x7
 */
STATIC CONST UINT8 Crc8_Table[crcMAX_ELEMENT_CRC8] =
{
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31,
    0x24, 0x23, 0x2A, 0x2D, 0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
    0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D, 0xE0, 0xE7, 0xEE, 0xE9,
    0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1,
    0xB4, 0xB3, 0xBA, 0xBD, 0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
    0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA, 0xB7, 0xB0, 0xB9, 0xBE,
    0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16,
    0x03, 0x04, 0x0D, 0x0A, 0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
    0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A, 0x89, 0x8E, 0x87, 0x80,
    0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8,
    0xDD, 0xDA, 0xD3, 0xD4, 0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
    0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44, 0x19, 0x1E, 0x17, 0x10,
    0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F,
    0x6A, 0x6D, 0x64, 0x63, 0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
    0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13, 0xAE, 0xA9, 0xA0, 0xA7,
    0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF,
    0xFA, 0xFD, 0xF4, 0xF3
};

/**
 * CRC32 ETH pre-calculated Table-Driven table uses following parameters:
 * CRC result width         32 bits
 * Initial value            0xFFFFFFFF
 * Input data reflected     true
 * Result data reflected    true
 * XOR value                0xFFFFFFFF
 * Polynomial               0x04C11DB7
 */
STATIC UINT32 U32Bit_Ethernet[256] =
{
    0x00000000U, 0x04C11DB7U, 0x09823B6EU, 0x0D4326D9U, 0x130476DCU,
    0x17C56B6BU, 0x1A864DB2U, 0x1E475005U, 0x2608EDB8U, 0x22C9F00FU,
    0x2F8AD6D6U, 0x2B4BCB61U, 0x350C9B64U, 0x31CD86D3U, 0x3C8EA00AU,
    0x384FBDBDU, 0x4C11DB70U, 0x48D0C6C7U, 0x4593E01EU, 0x4152FDA9U,
    0x5F15ADACU, 0x5BD4B01BU, 0x569796C2U, 0x52568B75U, 0x6A1936C8U,
    0x6ED82B7FU, 0x639B0DA6U, 0x675A1011U, 0x791D4014U, 0x7DDC5DA3U,
    0x709F7B7AU, 0x745E66CDU, 0x9823B6E0U, 0x9CE2AB57U, 0x91A18D8EU,
    0x95609039U, 0x8B27C03CU, 0x8FE6DD8BU, 0x82A5FB52U, 0x8664E6E5U,
    0xBE2B5B58U, 0xBAEA46EFU, 0xB7A96036U, 0xB3687D81U, 0xAD2F2D84U,
    0xA9EE3033U, 0xA4AD16EAU, 0xA06C0B5DU, 0xD4326D90U, 0xD0F37027U,
    0xDDB056FEU, 0xD9714B49U, 0xC7361B4CU, 0xC3F706FBU, 0xCEB42022U,
    0xCA753D95U, 0xF23A8028U, 0xF6FB9D9FU, 0xFBB8BB46U, 0xFF79A6F1U,
    0xE13EF6F4U, 0xE5FFEB43U, 0xE8BCCD9AU, 0xEC7DD02DU, 0x34867077U,
    0x30476DC0U, 0x3D044B19U, 0x39C556AEU, 0x278206ABU, 0x23431B1CU,
    0x2E003DC5U, 0x2AC12072U, 0x128E9DCFU, 0x164F8078U, 0x1B0CA6A1U,
    0x1FCDBB16U, 0x018AEB13U, 0x054BF6A4U, 0x0808D07DU, 0x0CC9CDCAU,
    0x7897AB07U, 0x7C56B6B0U, 0x71159069U, 0x75D48DDEU, 0x6B93DDDBU,
    0x6F52C06CU, 0x6211E6B5U, 0x66D0FB02U, 0x5E9F46BFU, 0x5A5E5B08U,
    0x571D7DD1U, 0x53DC6066U, 0x4D9B3063U, 0x495A2DD4U, 0x44190B0DU,
    0x40D816BAU, 0xACA5C697U, 0xA864DB20U, 0xA527FDF9U, 0xA1E6E04EU,
    0xBFA1B04BU, 0xBB60ADFCU, 0xB6238B25U, 0xB2E29692U, 0x8AAD2B2FU,
    0x8E6C3698U, 0x832F1041U, 0x87EE0DF6U, 0x99A95DF3U, 0x9D684044U,
    0x902B669DU, 0x94EA7B2AU, 0xE0B41DE7U, 0xE4750050U, 0xE9362689U,
    0xEDF73B3EU, 0xF3B06B3BU, 0xF771768CU, 0xFA325055U, 0xFEF34DE2U,
    0xC6BCF05FU, 0xC27DEDE8U, 0xCF3ECB31U, 0xCBFFD686U, 0xD5B88683U,
    0xD1799B34U, 0xDC3ABDEDU, 0xD8FBA05AU, 0x690CE0EEU, 0x6DCDFD59U,
    0x608EDB80U, 0x644FC637U, 0x7A089632U, 0x7EC98B85U, 0x738AAD5CU,
    0x774BB0EBU, 0x4F040D56U, 0x4BC510E1U, 0x46863638U, 0x42472B8FU,
    0x5C007B8AU, 0x58C1663DU, 0x558240E4U, 0x51435D53U, 0x251D3B9EU,
    0x21DC2629U, 0x2C9F00F0U, 0x285E1D47U, 0x36194D42U, 0x32D850F5U,
    0x3F9B762CU, 0x3B5A6B9BU, 0x0315D626U, 0x07D4CB91U, 0x0A97ED48U,
    0x0E56F0FFU, 0x1011A0FAU, 0x14D0BD4DU, 0x19939B94U, 0x1D528623U,
    0xF12F560EU, 0xF5EE4BB9U, 0xF8AD6D60U, 0xFC6C70D7U, 0xE22B20D2U,
    0xE6EA3D65U, 0xEBA91BBCU, 0xEF68060BU, 0xD727BBB6U, 0xD3E6A601U,
    0xDEA580D8U, 0xDA649D6FU, 0xC423CD6AU, 0xC0E2D0DDU, 0xCDA1F604U,
    0xC960EBB3U, 0xBD3E8D7EU, 0xB9FF90C9U, 0xB4BCB610U, 0xB07DABA7U,
    0xAE3AFBA2U, 0xAAFBE615U, 0xA7B8C0CCU, 0xA379DD7BU, 0x9B3660C6U,
    0x9FF77D71U, 0x92B45BA8U, 0x9675461FU, 0x8832161AU, 0x8CF30BADU,
    0x81B02D74U, 0x857130C3U, 0x5D8A9099U, 0x594B8D2EU, 0x5408ABF7U,
    0x50C9B640U, 0x4E8EE645U, 0x4A4FFBF2U, 0x470CDD2BU, 0x43CDC09CU,
    0x7B827D21U, 0x7F436096U, 0x7200464FU, 0x76C15BF8U, 0x68860BFDU,
    0x6C47164AU, 0x61043093U, 0x65C52D24U, 0x119B4BE9U, 0x155A565EU,
    0x18197087U, 0x1CD86D30U, 0x029F3D35U, 0x065E2082U, 0x0B1D065BU,
    0x0FDC1BECU, 0x3793A651U, 0x3352BBE6U, 0x3E119D3FU, 0x3AD08088U,
    0x2497D08DU, 0x2056CD3AU, 0x2D15EBE3U, 0x29D4F654U, 0xC5A92679U,
    0xC1683BCEU, 0xCC2B1D17U, 0xC8EA00A0U, 0xD6AD50A5U, 0xD26C4D12U,
    0xDF2F6BCBU, 0xDBEE767CU, 0xE3A1CBC1U, 0xE760D676U, 0xEA23F0AFU,
    0xEEE2ED18U, 0xF0A5BD1DU, 0xF464A0AAU, 0xF9278673U, 0xFDE69BC4U,
    0x89B8FD09U, 0x8D79E0BEU, 0x803AC667U, 0x84FBDBD0U, 0x9ABC8BD5U,
    0x9E7D9662U, 0x933EB0BBU, 0x97FFAD0CU, 0xAFB010B1U, 0xAB710D06U,
    0xA6322BDFU, 0xA2F33668U, 0xBCB4666DU, 0xB8757BDAU, 0xB5365D03U,
    0xB1F740B4U
};

STATIC CONST crc8ConfigType XCrc8ChannelCfg =
{
    .UcChannelId = crcGEN_CHANNEL_CRC8,
    .pCRCtable = Crc8_Table,
    .ucInitialValue = 0xFF,
    .ucFinalXORValue = 0x0,
    .InputDataReflected = 0x0,
    .ResultDataReflected = 0x0,
};

STATIC CONST crc32ConfigType XCrc32ChannelCfg =
{
    .UcChannelId = crcETH_CHANNEL_CRC32,
    .pCRCtable = U32Bit_Ethernet,
    .ulInitialValue = 0xFFFFFFFFU,
    .ulFinalXORValue = 0xFFFFFFFFU,
    .InputDataReflected = 0x1,
    .ResultDataReflected = 0x1,
};

STATIC CONST crcCoreConfigType XCrcCoreConfig[] =
{
    {
        .Crc.Ptr8  = &XCrc8ChannelCfg
    },
    {
        .Crc.Ptr32 = &XCrc32ChannelCfg
    },
};
/*----------------------------------------------------------------------------
 *                                PUBLIC APIs
 *--------------------------------------------------------------------------*/
/*==============================================================================
 @Service name        XCrc8Generate()
 @Description         This API is used to generate a CRC value for specific
                      data and length.
 @param[in]           UcChannelId: channel ID
 @param[in]           pucBuffer  : Input data buffer pointer
 @param[in]           UcLength   : length of the data buffer
 @param[out]          pucCrcData : OUTPUT parameter that will contain the value
                      of generated CRC result
 @param[in, out]      NA
 @return              CRC_SUCCESS:   generate CRC successfully
                      CRC_INVALID_PARAMETER:  parameter invalid
 @Pre                 NA
 @Post                NA
 @Requirements IDs    -
 @Design IDs          ->
 @service ID          -
 @Sync/Async          Synchronous function
 @Reentrancy          No
 @Note                -
==============================================================================*/
EFI_STATUS XCrc8Generate (UINT8 UcChannelId, UINT8 *pucBuffer,
                                UINT32 UcLength, UINT8 *pucCrcData )
{
    UINT8 UcCrc8;
    UINT32 UcCount = 0U;
    EFI_STATUS XStatus = EFI_SUCCESS;
    CONST UINT8 *puBufPtr = pucBuffer;
    CONST crc8ConfigType *CfgChptr = NULL;
    if ((pucBuffer == NULL) ||
        (pucCrcData == NULL) ||
        (UcLength == 0U) ||
        ( crcMAX_CH_ID <= UcChannelId) ) {
        XStatus = EFI_INVALID_PARAMETER;
    }
    else {
        CfgChptr = XCrcCoreConfig[UcChannelId].Crc.Ptr8;
        UcCrc8 = CfgChptr->ucInitialValue;
        for (UcCount = 0; UcCount < UcLength; UcCount++) {
            UcCrc8 = CfgChptr->pCRCtable[(UcCrc8 ^
                             (UINT8)*(puBufPtr++)) & 0xFFU];
        }
        *pucCrcData = (UcCrc8 ^ CfgChptr->ucFinalXORValue);
    }
    return XStatus;
}


/*----------------------------------------------------------------------------
 *                     CRC32 Helper APIs
 *--------------------------------------------------------------------------*/
STATIC UINT8 UReflect (UINT8 Val)
{
    UINT8 Res = 0;
    UINT8 Iter;
    for (Iter = 0; Iter < 8; Iter++)
    {
        if ((Val & ((UINT8)1 << Iter)) != (UINT8)0) {
            Res |= (UINT8)((UINT8)1 << (7U - Iter));
        }
    }
    return Res;
}

STATIC UINT32 UlReflect (UINT32 Val)
{
    UINT32 Res = 0U;
    UINT8 Iter;
    for (Iter = 0; Iter < 32; Iter++)
    {
        if ((Val & ((UINT32)1U << Iter)) != 0U) {
            Res |= (UINT32)((UINT32)1U << (31U - Iter));
        }
    }
    return Res;
}

/*==============================================================================
 @Service name        XCrc32Generate()
 @Description         This API is used to generate a CRC value for specific
                      data and length.
 @param[in]           UcChannelId: channel ID
 @param[in]           pucBuffer  : Input data buffer pointer
 @param[in]           UsLength   : length of the data buffer
 @param[out]          pusCrcData : OUTPUT parameter that will contain the value
                      of generated CRC result
 @param[in, out]      NA
 @return              CRC_SUCCESS:   generate CRC successfully
                      CRC_INVALID_PARAMETER:  parameter invalid
 @Pre                 NA
 @Post                NA
 @Requirements IDs    -
 @Design IDs          ->
 @service ID          -
 @Sync/Async          Synchronous function
 @Reentrancy          No
 @Note                -
==============================================================================*/
EFI_STATUS XCrc32Generate ( UINT8 UcChannelId, UINT8 *pucBuffer,
                            UINT32 UsLength, UINT32 *pusCrcData )
{
    CONST crc32ConfigType *CfgChptr = NULL;
    EFI_STATUS XStatus = EFI_SUCCESS;
    UINT32 BufId = 0U;
    UINT32 UcCrc32= 0U;
    UINT8  LutId = 0U;
    UINT8  Tmp = 0U;
    if ((pucBuffer == NULL) ||
        (pusCrcData == NULL)||
        (UsLength == 0U) ||
        ( crcMAX_CH_ID <= UcChannelId) ) {
        XStatus = EFI_INVALID_PARAMETER;
    }
    else {
        CfgChptr = XCrcCoreConfig[UcChannelId].Crc.Ptr32;
        UcCrc32  = CfgChptr->ulInitialValue;
        for (BufId = 0U; BufId < UsLength; BufId++)
        {
            Tmp = ((CfgChptr->InputDataReflected)?
                        UReflect (pucBuffer[BufId]): pucBuffer[BufId]);
            LutId  = (UINT8)((UcCrc32 ^ (Tmp << 24U)) >> 24U);
            UcCrc32 = ((UINT32)((UcCrc32 << 8U) ^
                         (UINT32)(CfgChptr->pCRCtable[LutId])));
        }
        UcCrc32 = ((CfgChptr->ResultDataReflected) ?
                             UlReflect (UcCrc32) : UcCrc32);
        *pusCrcData = (UcCrc32 ^ CfgChptr->ulFinalXORValue);
    }
    return XStatus;
}
#endif
