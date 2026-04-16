/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
 *
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 *@file  EFIChargerEx.h
 *@brief Charger Extended Protocol for UEFI open source.
 */

#ifndef __EFICHARGEREX_H__
#define __EFICHARGEREX_H__

/*===========================================================================
MACRO DECLARATIONS
===========================================================================*/

/**
Protocol version.
*/

/** @defgroup efi_chargerex_protocol EFI_CHARGEREX_PROTOCOL
 *  @ingroup POWER
 */
 /** @defgroup  efi_chargerex_protocol_prot PROTOCOL
 *  @ingroup efi_chargerex_protocol
 */
 /** @defgroup  efi_chargerex_protocol_apis APIs
 *  @ingroup efi_chargerex_protocol
 */

/** @defgroup  efi_chargerex_protocol_data DATA_STRUCTURES
 *  @ingroup efi_chargerex_protocol
 */

/** @ingroup efi_chargerex_protocol_data */
#define CHARGER_EX_REVISION_10003 0x0000000000010003
#define CHARGER_EX_REVISION_10004 0x0000000000010004

#define CHARGER_EX_REVISION_10005 0x0000000000010005

/*  Protocol GUID definition */
/** @ingroup efi_chargerex_protocol_data */
#define EFI_CHARGER_EX_PROTOCOL_GUID \
    { 0x6edc8a6d, 0x2663, 0x43cd, { 0x90, 0xff, 0x46, 0x21, 0xff, 0xd1, 0x0d, 0xf5 } }

/** @cond */
/*===========================================================================
EXTERNAL VARIABLES
===========================================================================*/
/**
External reference to the Charger Extended Protocol GUID.
*/
extern EFI_GUID gChargerExProtocolGuid;

/*===========================================================================
TYPE DEFINITIONS
===========================================================================*/
/**
Protocol declaration.
*/
typedef struct _EFI_CHARGER_EX_PROTOCOL EFI_CHARGER_EX_PROTOCOL;
/** @endcond */

/** @ingroup efi_chargerex_protocol_data
  @par Summary
  EFI_CHARGER_EX_POWER_TYPE enum.
  @par Parameters
*/
typedef enum {
  EFI_CHARGER_EX_POWER_FLASH_BATTERY_VOLTAGE_TYPE,
  EFI_CHARGER_EX_POWER_TYPE_INVALID = 0x7FFFFFFF
}EFI_CHARGER_EX_POWER_TYPE;

/** @ingroup efi_chargerex_protocol_data
  @par Summary
  EFI_CHARGER_EX_FLASH_INFO struct.
  @par Parameters
*/
typedef struct {
  BOOLEAN bCanFlash;
  UINT32  BattCurrVoltage;
  UINT32  BattRequiredVoltage;
}EFI_CHARGER_EX_FLASH_INFO;

/** @ingroup efi_chargerex_protocol_data
  @par Summary
  EFI_CHARGER_EX_FW_UPDATE_MODE_TYPE enum.
  @par Parameters
*/
typedef enum {
  EFI_CHARGER_EX_FW_UPDATE_MODE_EXIT,
  EFI_CHARGER_EX_FW_UPDATE_MODE_ENTER,
  EFI_CHARGER_EX_FW_UPDATE_MODE_INVALID = 0x7FFFFFFF
}EFI_CHARGER_EX_FW_UPDATE_MODE_TYPE;

/**
Protocol declaration.
*/
typedef struct _EFI_CHARGER_EX_PROTOCOL   EFI_CHARGER_EX_PROTOCOL;
/** @endcond */




/*===========================================================================
FUNCTION DEFINITIONS
===========================================================================*/

/* EFI_CHARGER_EX_GET_CHARGER_PRESENCE */
/** @ingroup efi_chargerex_protocol_apis
  @par Summary
  This API returns charger presence status

  @param[out]  pChargerPresent   TRUE:Charger Present, FALSE:Charger not present

  @return
  EFI_SUCCESS            -- Function completed successfully.
  EFI_DEVICE_ERROR       -- Physical device reported an error.
*/
typedef
EFI_STATUS (EFIAPI *EFI_CHARGER_EX_GET_CHARGER_PRESENCE)(
  OUT BOOLEAN  *pChargerPresent
);


/* EFI_CHARGER_EX_GET_BATTERY_VOLTAGE */
/** @ingroup efi_chargerex_protocol_apis
  @par Summary
  This API returns battery voltage in mV

  @param[out]  pBatteryVoltagemV   Battery voltage in mV

  @return
  EFI_SUCCESS            -- Function completed successfully.
  EFI_DEVICE_ERROR       -- Physical device reported an error.
*/
typedef
EFI_STATUS (EFIAPI *EFI_CHARGER_EX_GET_BATTERY_VOLTAGE)(
  OUT UINT32  *pBatteryVoltagemV
);


/* EFI_CHARGER_EX_GET_CHARGING_STATUS */
/** @ingroup efi_chargerex_protocol_apis
  @par Summary
  This API returns Device Charging status

  @param[out]  pBatterySOC   Battery SOC in %level
  @param[out]  IsChargerAttached   Is charger present

  @return
  EFI_SUCCESS            -- Function completed successfully.
  EFI_DEVICE_ERROR       -- Physical device reported an error.
*/
typedef
EFI_STATUS (EFIAPI *EFI_CHARGER_EX_GET_CHARGING_STATUS)(
  OUT UINT32  *pBatterySOC,
  OUT BOOLEAN  *IsChargerAttached
);


/* EFI_CHARGER_EX_GET_BATTERY_PRESENCE */
/** @ingroup efi_chargerex_protocol_apis
  @par Summary
  This API returns battery presence status

  @param[out]  pBatteryPresent   TRUE:Battery Present, FALSE:Battery not present

  @return
  EFI_SUCCESS            -- Function completed successfully.
  EFI_DEVICE_ERROR       -- Physical device reported an error.
*/
typedef
EFI_STATUS (EFIAPI * EFI_CHARGER_EX_GET_BATTERY_PRESENCE)(
  OUT BOOLEAN  *pBatteryPresent
);


/* EFI_CHARGER_EX_IS_OFFMODE_CHARGING */
/** @ingroup efi_chargerex_protocol_apis
  @par Summary
  This API returns if device needs to off mode charging

  @param[out]  bOffModeCharging   TRUE:OffMode Charging is needed, Otherwise FALSE

  @return
  EFI_SUCCESS            -- Function completed successfully.
  EFI_DEVICE_ERROR       -- Physical device reported an error.
*/
typedef
EFI_STATUS ( EFIAPI * EFI_CHARGER_EX_IS_OFFMODE_CHARGING)(
  OUT BOOLEAN *bOffModeCharging
);


/* EFI_CHARGER_EX_IS_POWER_OK */
/** @ingroup efi_chargerex_protocol_apis
  @par Summary
  This API returns if device battery voltage is good for SW image flash

  @param[in]   FlashType     Refer EFI_CHARGER_EX_POWER_TYPE 
  @param[out]  PowerTypeInfo Refer required queried structure

  @return
  EFI_SUCCESS            -- Function completed successfully.
  EFI_DEVICE_ERROR       -- Physical device reported an error.
*/
typedef
EFI_STATUS ( EFIAPI * EFI_CHARGER_EX_IS_POWER_OK)(
  IN  EFI_CHARGER_EX_POWER_TYPE PowerType,
  OUT VOID                     *PowerTypeInfo
);



/* EFI_CHARGER_EX_PROCESS_BATT_FW_UPDATE */
/** @ingroup efi_chargerex_protocol_apis
  @par Summary
  This API returns battery SOC in %level

  @param[in]  FwUpdateMode   Process action for Batt Fw update

  @return
  EFI_SUCCESS            -- Function completed successfully.
  EFI_DEVICE_ERROR       -- Physical device reported an error.
*/
typedef
EFI_STATUS (EFIAPI *EFI_CHARGER_EX_SET_FW_UPDATE_MODE)(
  IN  EFI_CHARGER_EX_FW_UPDATE_MODE_TYPE  FwUpdateMode
);

/* EFI_CHARGER_EX_GET_CHARGING_STATUS */
/** @ingroup efi_chargerex_protocol_apis
  @par Summary
  This API returns Device Charging status

  @param[out]  pBatterySOC   Battery SOC in %level
  @param[out]  IsChargerAttached   Is charger present

  @return
  EFI_SUCCESS            -- Function completed successfully.
  EFI_DEVICE_ERROR       -- Physical device reported an error.
*/
typedef
EFI_STATUS (EFIAPI *EFI_CHARGER_EX_GET_CHARGING_STATUS)(
  OUT UINT32  *pBatterySOC,
  OUT BOOLEAN  *IsChargerAttached
);

/* EFI_CHARGER_EX_IS_CPU_FRQ_MITIG_REQ */
/** @ingroup efi_chargerex_protocol_apis
  @par Summary
  This API returns if device needs to off mode charging

  @param[out]  bIsWeakChg   TRUE:OffMode Charging is needed, Otherwise FALSE

  @return
  EFI_SUCCESS            -- Function completed successfully.
  EFI_DEVICE_ERROR       -- Physical device reported an error.
*/
typedef
EFI_STATUS ( EFIAPI * EFI_CHARGER_EX_IS_CPU_FRQ_MITIG_REQ)(
  OUT BOOLEAN *bmitigateCpuFreq
);

/*===========================================================================
PROTOCOL INTERFACE
===========================================================================*/
/** @ingroup efi_chargerex_protocol_prot
  @par Summary
  PMIC Charger Protocol interface.

  @par Parameters
  @inputprotoparams{pmic_charger_proto_params.tex}
*/

struct _EFI_CHARGER_EX_PROTOCOL {
  UINT64                                Revision;
  EFI_CHARGER_EX_GET_CHARGER_PRESENCE   GetChargerPresence;    // Returns if charger is present
  EFI_CHARGER_EX_GET_BATTERY_PRESENCE   GetBatteryPresence;    // Returns if battery is present
  EFI_CHARGER_EX_GET_BATTERY_VOLTAGE    GetBatteryVoltage;     // Returns battery voltage
  EFI_CHARGER_EX_IS_OFFMODE_CHARGING    IsOffModeCharging;
  EFI_CHARGER_EX_IS_POWER_OK            IsPowerOk;
  EFI_CHARGER_EX_IS_CPU_FRQ_MITIG_REQ   IsCpuFreqMitigationReq;
  EFI_CHARGER_EX_GET_CHARGING_STATUS    GetChargingStatus;
  EFI_CHARGER_EX_SET_FW_UPDATE_MODE     SetFirmwareUpdateMode;
};

#endif /* __EFICHARGEREX_H__ */
