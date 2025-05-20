/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __EFIQCOMCHARGER_H__
#define __EFIQCOMCHARGER_H__

/*===========================================================================
INCLUDE FILES
===========================================================================*/
//#include "EFIULog.h"
/*===========================================================================
MACRO DECLARATIONS
===========================================================================*/
/**
  Protocol version.
*/
#define QCOM_CHARGER_REVISION_12 0x0000000000010012
#define QCOM_CHARGER_REVISION_13 0x0000000000010013
#define QCOM_CHARGER_REVISION_14 0x0000000000010014
#define QCOM_CHARGER_REVISION QCOM_CHARGER_REVISION_14
/** @} */ /* end_addtogroup efi_pmicfg_constants */

/*  Protocol GUID definition */
/** @ingroup efi_qcomcharger_protocol */
#define EFI_QCOM_CHARGER_PROTOCOL_GUID \
  { 0x21bfb78b, 0x02ea, 0x4ab7, { 0x99, 0x40, 0xb5, 0x8a, 0x2a, 0xd6, 0x58, 0x29 } }

/*===========================================================================
EXTERNAL VARIABLES
===========================================================================*/
/**
External reference to the PMIC Fg UEFI Protocol GUID.
*/

extern EFI_GUID gQcomChargerProtocolGuid;


/*===========================================================================
TYPE DEFINITIONS
===========================================================================*/
/**
Protocol declaration.
*/
typedef struct _EFI_QCOM_CHARGER_PROTOCOL  EFI_QCOM_CHARGER_PROTOCOL;

/*===========================================================================*/
typedef enum
{
  EFI_QCOM_CHARGER_STATUS_GOOD_TO_BOOT = 0,
  /**< Device is good to boot to HLOS */
  EFI_QCOM_CHARGER_STATUS_CHARGING,
  /**< Charging is on-going */
  EFI_QCOM_CHARGER_STATUS_ERROR,
  /**< Error occurred */
  EFI_QCOM_CHARGER_PLATFORM_STATUS_PROFILE_LOAD,
  /**< Profile load is needed  */
  EFI_QCOM_CHARGER_STATUS_DEBUG_BOARD_GOOD_TO_BOOT,
  /**< Detected debug board good to boot */
  EFI_QCOM_CHARGER_STATUS_INVALID = 0x7FFFFFFF,
  /**< Error occurred */
}EFI_QCOM_CHARGER_STATUS_TYPE;

typedef enum
{
  EFI_QCOM_CHARGER_ACTION_CONTINUE,
  /**< Continue charging */
  EFI_QCOM_CHARGER_ACTION_START_CHARGING,
  /**< Start Charging */
  EFI_QCOM_CHARGER_ACTION_STOP_CHARGING,
  /**<  Stop Charging */
  EFI_QCOM_CHARGER_ACTION_GOOD_TO_BOOT,
  /**< Boot to HLOS */
  EFI_QCOM_CHARGER_ACTION_CRITICAL,
  /**< Action for critical error */
  EFI_QCOM_CHARGER_ACTION_SHUTDOWN,
  /**< Shutdown device */
  EFI_QCOM_CHARGER_ACTION_NO_CHARGE_WAIT,
  /**< Stop charging and wait */
  EFI_QCOM_CHARGER_PLATFORM_ACTION_PROFILE_LOAD,
  /**< Profile load Action */
  EFI_QCOM_CHARGER_ACTION_SHUTDOWN_USB_DC_PON_DISABLED,
  /**< Action required for shutdown disable PON1 */
  EFI_QCOM_CHARGER_ACTION_DEBUG_BOARD_GOOD_TO_BOOT,
  /**< debug board Boot to HLOS */
  EFI_QCOM_CHARGER_ACTION_TSENSE_HIGH_WAIT,
  /**< action for tsense high*/
  EFI_QCOM_CHARGER_ACTION_BATTERY_UNKNOWN,
  /**< Stop charging and wait */
  EFI_QCOM_CHARGER_ACTION_EXIT,
  /**< Clean up Charger */
  EFI_QCOM_CHARGER_ACTION_DEBUG_BOARD_WAIT,
  /**< debug board stay in uefi */
  EFI_QCOM_CHARGER_ACTION_LOAD_CHARGER_FW,
  /**< Load ADSP Charger FW */
  EFI_QCOM_CHARGER_ACTION_CONNECT_CHARGER_FW,
  /**< Connect/Link to ADSP Charger FW (GLink)*/
  EFI_QCOM_CHARGER_ACTION_RESET_COLD,
  /**< Reset the device */

  EFI_QCOM_CHARGER_ACTION_RECOVERY,
  /**< Chg Fw recovery */
  EFI_QCOM_CHARGER_ACTION_FACTORY_MODE_BOOT_TO_HLOS,
  /**< Factory Mode Detected, Boot to HLOS */

  EFI_QCOM_CHARGER_ACTION_INVALID = 0x7FFFFFFF,
  /**< invalid action */
}EFI_QCOM_CHARGER_ACTION_TYPE;


typedef enum {
  EFI_QCOM_CHARGER_ERROR_NONE = 0,
  /**< No status. */

  EFI_QCOM_CHARGER_ERROR_BATTERY_NOT_DETECTED,
  /**< Battery is not detected. */

  EFI_QCOM_CHARGER_ERROR_VBATT_OUTSIDE_RANGE,
  /**< Charging logic detected the voltage to be out of
    the operational range. */

  EFI_QCOM_CHARGER_ERROR_CHARGING_TIMEOUT,
  /**< Charging logic detected that the battery is not being charged within
    a reasonable time. */

  EFI_QCOM_CHARGER_ERROR_BATT_TEMP_OUTSIDE_OPERATIONAL_RANGE,
  /**< Battery temperature is too hot to charge. */

  EFI_QCOM_CHARGER_ERROR_BATT_TEMP_OUTSIDE_CHARGING_RANGE,
  /**< Battery temperature is too hot to continue charging. */

  EFI_QCOM_CHARGER_ERROR_TSENSE_CRITICAL,
  /**< Battery is getting too hot to charge. */

  EFI_QCOM_CHARGER_ERROR_TSENSE_HOT,
  /**< Battery is too cold to continue charging. */

  EFI_QCOM_CHARGER_ERROR_BATTERY_CHARGING_NOT_SUPPORTED,
  /**< Battery does not support the charging operation. */

  EFI_QCOM_CHARGER_ERROR_CHARGING_SOURCE_NOT_DETECTED,
  /**< Phone is not attached to a charging source and cannot continue with
    the charging operation. */

  EFI_QCOM_CHARGER_ERROR_ADC_ERROR,
  /**< ADC error was detected during the charging operation. */

  EFI_QCOM_CHARGER_ERROR_FG_ERROR,
  /**< Fuel gauge error detected. */

  //EFI_QCOM_CHARGER_ERROR_NO_CHARGE_WAIT,
  /**< Battery temperature outside charging range */

  EFI_QCOM_CHARGER_ERROR_LOW_VBATT,
  /**< Battery Voltage below the emergency shutdown limit. */

  EFI_QCOM_CHARGER_ERROR_UNKNOWN_BATTERY,
  /**< Battery Voltage below the emergency shutdown limit. */

  EFI_QCOM_CHARGER_ERROR_DEBUG_BOARD_GOOD_TO_BOOT,
  /**< Detected debug board */

  EFI_QCOM_CHARGER_ERROR_CHARGER_FW_LOAD_ERROR,
  /**< ADSP CHARGER FW Load (GLink) Error  */

  EFI_QCOM_CHARGER_ERROR_CHARGER_FW_LINK_ERROR,
  /**< ADSP CHARGER Link/Connect Error  */

  EFI_QCOM_CHARGER_ERROR_INVALID = 0x7FFFFFFF,
  /**< invalid action */
} /** @cond */EFI_QCOM_CHARGER_ERROR_TYPES/** @endcond */;

typedef enum {
  EFI_QCOM_CHARGER_DISP_IMAGE_NONE,
  EFI_QCOM_CHARGER_DISP_IMAGE_NOBATTERY,
  EFI_QCOM_CHARGER_DISP_IMAGE_NOCHARGER,
  EFI_QCOM_CHARGER_DISP_IMAGE_ABOVE_THRESHOLD,
  EFI_QCOM_CHARGER_DISP_IMAGE_LOWBATTERYCHARGING,
  EFI_QCOM_CHARGER_DISP_IMAGE_LOWBATTERY,
  EFI_QCOM_CHARGER_DISP_IMAGE_TSENS_THERMAL_SYMBOL,
  EFI_QCOM_CHARGER_DISP_IMAGE_TSENS_CRITICAL_SYMBOL,
  EFI_QCOM_CHARGER_DISP_IMAGE_DEBUG_BOOT_SYMBOL,
  EFI_QCOM_CHARGER_DISP_IMAGE_DEBUG_LOW_SYMBOL,
}EFI_QCOM_CHARGER_DISP_IMAGE_TYPE;


typedef struct
{
  UINT32 StateOfCharge;
  INT32  ChargeCurrent;
  INT32  BatteryVoltage;
  INT32  BatteryTemperature;
  UINT32 Ocv;
  UINT32 StateOfChargeCentiPct;
}EFI_QCOM_CHARGER_BATT_STATUS_INFO;

typedef enum
{
  EFI_QCOM_CHARGER_PROFILE_LOAD,
  EFI_QCOM_CHARGER_PROFILE_RESTART,
  EFI_QCOM_CHARGER_PROFILE_INVALID
}EFI_QCOM_CHARGER_BATT_PROFILE_STATE;

typedef enum
{
  EFI_QCOM_CHARGER_CHARGING_LED_OFF,
  EFI_QCOM_CHARGER_CHARGING_LED_ON,
  EFI_QCOM_CHARGER_CHARGING_LED_TOGGLE,
  EFI_QCOM_CHARGER_CHARGING_LED_INVALID
}EFI_QCOM_CHARGER_CHARGING_LED_CONFIG_TYPE;

typedef enum
{
   EFI_QCOM_CHARGER_ATTACHED_NONE,    /**< No charger. */
   EFI_QCOM_CHARGER_ATTACHED_BATT,    /**< Battery. */
   EFI_QCOM_CHARGER_ATTACHED_USB,     /**< USB path. */
   EFI_QCOM_CHARGER_ATTACHED_DCIN,    /**< DCIN path. */
} EFI_QCOM_CHARGER_ATTACHED_TYPE;

typedef struct
{
  BOOLEAN bChargerSrcPresent;
  UINT32  DispSignOfLifeMaxThresholdMv;
  EFI_QCOM_CHARGER_BATT_STATUS_INFO         BatteryStatusInfo;
  EFI_QCOM_CHARGER_ATTACHED_TYPE            ChargerAttached;
  EFI_QCOM_CHARGER_BATT_PROFILE_STATE       ProfState;
  EFI_QCOM_CHARGER_CHARGING_LED_CONFIG_TYPE LedConfigType;
  BOOLEAN bPowerKeyPressed;
}EFI_QCOM_CHARGER_ACTION_INFO;


//typedef struct
//{
  /*Charger Log File Handled */
  /*ULogHandle gChargerLogHandle;
  BOOLEAN    bPrintChargerAppDbgMsg;
  BOOLEAN    bPrintChargerAppDbgMsgToFile;
  UINT32     fileLogLevelMask;
}EFI_QCOM_CHARGER_LOG_INFO;*/


typedef struct
{
  UINT32 HARD_RESET : 1;     /* Hard reset event trigger */
  UINT32 SMPL : 1;           /* SMPL trigger */
  UINT32 RTC : 1;            /* RTC trigger */
  UINT32 DC_CHG : 1;         /* DC Charger trigger */
  UINT32 USB_CHG : 1;        /* USB Charger trigger */
  UINT32 PON1 : 1;           /* PON1 trigger */
  UINT32 CBLPWR : 1;         /* CBL_PWR1_N trigger */
  UINT32 KPDPWR : 1;         /* KPDPWR_N trigger */
}EFI_QCOM_CHARGER_PWRON_REASON_TYPE;

/*===========================================================================
  FUNCTION DEFINITIONS
===========================================================================*/
/* EFI_QCOM_CHARGER_ENABLE_CHARGING */
/** @ingroup efi_qcomchargerEnableCharging
* @par Summary
* Enables battery charging
*
* @param[in] Enable: TRUE: Enable Charging
*                    FALSE: Disable Charging
*
*@return
* EFI_SUCCESS           -- Function completed successfully. \n
* EFI_INVALID_PARAMETER -- Parameter is invalid. \n
* EFI_DEVICE_ERROR      -- Physical device reported an error. \n
* EFI_NOT_READY         -- Physical device is busy or not ready to
*                          process this request.
*/
typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_ENABLE_CHARGING) (
  IN BOOLEAN Enable
);

/* EFI_QCOM_CHARGER_GET_MAX_USB_CURRENT */
/** @ingroup efi_qcomchargerGetMaxUsbCurrent
* @par Summary
* Gets Max USB current from CFG file
*
* @param[out] pMaxCurrent: value of current in mA
*
*@return
* EFI_SUCCESS           -- Function completed successfully. \n
* EFI_INVALID_PARAMETER -- Parameter is invalid. \n
* EFI_DEVICE_ERROR      -- Physical device reported an error. \n
* EFI_NOT_READY         -- Physical device is busy or not ready to
*                          process this request.
*/
typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_GET_MAX_USB_CURRENT) (
  OUT UINT32 *pMaxCurrent
);

/* EFI_QCOM_CHARGER_SET_MAX_USB_CURRENT */
/** @ingroup efi_qcomchargerSetMaxUsbCurrent
* @par Summary
* Enables battery charging
*
* @param[in] MaxCurrent: value of current in mA
*
*@return
* EFI_SUCCESS           -- Function completed successfully. \n
* EFI_INVALID_PARAMETER -- Parameter is invalid. \n
* EFI_DEVICE_ERROR      -- Physical device reported an error. \n
* EFI_NOT_READY         -- Physical device is busy or not ready to
*                          process this request.
*/
typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_SET_MAX_USB_CURRENT) (
  IN UINT32 MaxCurrent
);


/* EFI_QCOMCHARGER_GET_CHARGING_STATUS */
/** @ingroup efi_qcomchargerGetChargingAction
  @par Summary
  Returns required charging action

  @param[out] pChargingAction: Required action type
                               refer to EFI_QCOM_CHARGER_ACTION_TYPE for details
  @param[out] pChargerActionInfo: information such as battery voltage, soc and charger present
                               refer to EFI_QCOM_CHARGER_ACTION_INFO for details
  @param[in] vbattChecking: if low vbatt checking for emergency shutdown will be done.

 @return
  EFI_SUCCESS           -- Function completed successfully. \n
  EFI_INVALID_PARAMETER -- Parameter is invalid. \n
  EFI_DEVICE_ERROR      -- Physical device reported an error. \n
  EFI_NOT_READY         -- Physical device is busy or not ready to
                           process this request.
*/
typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_GET_CHARGING_ACTION) (
  OUT EFI_QCOM_CHARGER_ACTION_TYPE  *pChargingAction,
  OUT EFI_QCOM_CHARGER_ACTION_INFO  *pChargerActionInfo,
  IN  BOOLEAN                       vbattChecking
);

/* EFI_QCOMCHARGER_TAKE_ACTION */
/** @ingroup efi_qcomchargerTakeAction
  @par Summary
  Get Charge Current

  @param[in] ChargingAction: Required action type
                              refer to EFI_QCOM_CHARGER_CHARGING_ACTION_TYPE for details
 @param[in] pChargerActionInfo: Required action info
                             refer to EFI_QCOM_CHARGER_ACTION_INFO for details

 @return
  EFI_SUCCESS           -- Function completed successfully. \n
  EFI_INVALID_PARAMETER -- Parameter is invalid. \n
  EFI_DEVICE_ERROR      -- Physical device reported an error. \n
  EFI_NOT_READY         -- Physical device is busy or not ready to
                           process this request.
*/
typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_TAKE_ACTION) (
  IN EFI_QCOM_CHARGER_ACTION_TYPE ChargingAction,
  IN CONST EFI_QCOM_CHARGER_ACTION_INFO *pChargerActionInfo
);

/* EFI_QCOMCHARGER_TAKE_ACTION */
/** @ingroup efi_qcomchargerTakeAction
  @par Summary
  Get Charge Current

  @param[in] ChargingAction: Required action type
                              refer to EFI_QCOM_CHARGER_CHARGING_ACTION_TYPE for details

 @return
  EFI_SUCCESS           -- Function completed successfully. \n
  EFI_INVALID_PARAMETER -- Parameter is invalid. \n
  EFI_DEVICE_ERROR      -- Physical device reported an error. \n
  EFI_NOT_READY         -- Physical device is busy or not ready to
                           process this request.
*/
typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_DISPLAY_IMAGE) (
  IN EFI_QCOM_CHARGER_DISP_IMAGE_TYPE ImageType,
  IN BOOLEAN ClearScreen
);


/* EFI_QCOM_CHARGER_EXIT */
/** @ingroup efi_qcomchargerExit
@par Summary
Clean up Qcom charger dxe on charging exit or critical error

 @return
  EFI_SUCCESS           -- Function completed successfully. \n
  EFI_INVALID_PARAMETER -- Parameter is invalid. \n
  EFI_DEVICE_ERROR      -- Physical device reported an error. \n
  EFI_NOT_READY         -- Physical device is busy or not ready to
                           process this request.
*/
typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_DEINIT) (
  IN EFI_QCOM_CHARGER_ACTION_TYPE ChargingAction
);


/* EFI_QCOM_CHARGER_GET_BATTERY_PRESENCE */
/** @ingroup efi_qcomchargerGetBatteryPresence
  @par Summary
  Returns battery presence status

  @param[out] *pBatteryPresence: TRUE: Battery is Present
                                 FALSE: Battery is not present

 @return
  EFI_SUCCESS           -- Function completed successfully. \n
  EFI_INVALID_PARAMETER -- Parameter is invalid. \n
  EFI_DEVICE_ERROR      -- Physical device reported an error. \n
  EFI_NOT_READY         -- Physical device is busy or not ready to
                           process this request.
*/
typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_GET_BATTERY_PRESENCE) (
  OUT BOOLEAN *pBatteryPresence
);

/* EFI_QCOM_CHARGER_GET_BATTERY_VOLTAGE */
/** @ingroup efi_qcomchargerGetBatteryVoltage
  @par Summary
  Returns battery voltage

  @param[out] *pBatteryVoltage: TRUE: Battery is Present
                                 FALSE: Battery is not present

 @return
  EFI_SUCCESS           -- Function completed successfully. \n
  EFI_INVALID_PARAMETER -- Parameter is invalid. \n
  EFI_DEVICE_ERROR      -- Physical device reported an error. \n
  EFI_NOT_READY         -- Physical device is busy or not ready to
                           process this request.
*/
typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_GET_BATTERY_VOLTAGE) (
  OUT UINT32 *pBatteryVoltage
);

/* EFI_QCOM_CHARGER_GET_FILE_LOG_INFO */
/** @ingroup efi_getFileLogInfo
  @par Summary
  Returns File Log Information as File log handle

  @param[out] *pFileLogInfo: File Log informtion as in File log handle

 @return
  EFI_SUCCESS           -- Function completed successfully. \n
  EFI_INVALID_PARAMETER -- Parameter is invalid. \n
  EFI_DEVICE_ERROR      -- Physical device reported an error. \n
  EFI_NOT_READY         -- Physical device is busy or not ready to
                           process this request.
*/
/*typedef
EFI_STATUS(EFIAPI *EFI_QCOM_CHARGER_GET_LOG_INFO)(
  OUT EFI_QCOM_CHARGER_LOG_INFO *pFileLogInfo
);*/

/* EFI_QCOM_CHARGER_GET_FILE_LOG_INFO */
/** @ingroup efi_getFileLogInfo
  @par Summary
  Returns File Log Information as File log handle

  @param[out] *pFileLogInfo: File Log informtion as in File log handle

 @return
  EFI_SUCCESS           -- Function completed successfully. \n
  EFI_INVALID_PARAMETER -- Parameter is invalid. \n
  EFI_DEVICE_ERROR      -- Physical device reported an error. \n
  EFI_NOT_READY         -- Physical device is busy or not ready to
                           process this request.
*/
typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_DUMP_PERIPHERAL) (
);

/* EFI_QCOM_CHARGER_IS_DCIN_VALID */
/** @ingroup efi_isDcInValid
@par Summary
Returns if dcin is valid or not

@param[out] *pIsValid: if dcin is valid or not

@return
EFI_SUCCESS           -- Function completed successfully. \n
EFI_INVALID_PARAMETER -- Parameter is invalid. \n
EFI_DEVICE_ERROR      -- Physical device reported an error. \n
EFI_NOT_READY         -- Physical device is busy or not ready to
process this request.
*/
typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_IS_DCIN_VALID) (
OUT BOOLEAN* pIsValid
);


typedef enum
{
  /* Key to retrive battery minimum voltage for allowing SW image flash */
  EFI_QCOM_CHARGER_CONFIG_KEY_SW_FLASH_VOLTAGE,
  /* Key to retrieve if we are in offmode Power testing mode */
  EFI_QCOM_CHARGER_CONFIG_KEY_OFFMODE_POWER_TESTING_MODE,
  /* Key to retrieve Charger FW (enable/disable) info */
  EFI_QCOM_CHARGER_CONFIG_KEY_CHARGER_FW_INFO,
  /* Key to retrieve FW update support info */
  EFI_QCOM_CHARGER_CONFIG_KEY_FW_UPDATE_SUPPORT,
  /* Key to retrieve FW update support info */
  EFI_QCOM_CHARGER_CONFIG_KEY_BATT_ID_INFO,
  /* Key to retrieve if the last shutdown was because of ship mode */
  EFI_QCOM_CHARGER_CONFIG_SHIP_MODE_EXIT,
  /* Key to retrive battery minimum soc for allowing SW image flash */
  EFI_QCOM_CHARGER_CONFIG_KEY_SW_FLASH_SOC,
  /* Key to retrieve minimum charger power for no battery boot */
  EFI_QCOM_CHARGER_CONFIG_KEY_BATT_MISSING_BOOT_MIN_CHG_POWER,
  /* Key to retrieve battery missing action */
  EFI_QCOM_CHARGER_CONFIG_KEY_BATT_MISSING_ACTION,
  /* Key to retrieve Battery Status icon display support (enable/disable) info */
  EFI_QCOM_CHARGER_CONFIG_KEY_ENABLE_IMAGE_DISPLAY,
  /* Key to retrive battery minimum and Maximum Soc for displaying battery status icon */
  EFI_QCOM_CHARGER_CONFIG_KEY_IMAGE_DISPLAY_SOC_VAL,
  /* Key invalid */
  EFI_QCOM_CHARGER_CONFIG_KEY_INVALID = 0x7FFFFFFF,
}EFI_QCOM_CHARGER_CONFIG_KEY;
/* EFI_QCOM_CHARGER_GET_CHARGER_CONFIG */
/** @ingroup efi_qcom_charger_get_charger_config
@par Summary
Returns if dcin is valid or not

@param[out] *pIsValid: if dcin is valid or not

@return
EFI_SUCCESS           -- Function completed successfully. \n
EFI_INVALID_PARAMETER -- Parameter is invalid. \n
EFI_DEVICE_ERROR      -- Physical device reported an error. \n
EFI_NOT_READY         -- Physical device is busy or not ready to
process this request.
*/
typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_GET_CHARGER_CONFIG) (
IN EFI_QCOM_CHARGER_CONFIG_KEY  ChargerCfgKey,
OUT  UINT32 *KeyValue
);

/* EFI_QCOM_CHARGER_IS_CHARGING_SUPPORTED */
/** @ingroup efi_isChargingSupported
@par Summary
Returns if charging is supported or not

@param[out] *pIsChargingSupported: if charging is supported or not

@return
EFI_SUCCESS           -- Function completed successfully. \n
EFI_INVALID_PARAMETER -- Parameter is invalid. \n
EFI_DEVICE_ERROR      -- Physical device reported an error. \n
EFI_NOT_READY         -- Physical device is busy or not ready to
process this request.
*/
typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_IS_CHARGING_SUPPORTED) (
OUT BOOLEAN* pIsChargingSupported
);


/* EFI_QCOM_CHARGER_GET_DISPLAY_IMAGE_TYPE */
/** @ingroup efi_qcomchargerGetDisplayImageType
  @par Summary
  Returns the image to be diplayed

  @param[out] pDispImage: Image to be displayed on screen

 @return
  EFI_SUCCESS           -- Function completed successfully. \n
  EFI_INVALID_PARAMETER -- Parameter is invalid. \n
  EFI_DEVICE_ERROR      -- Physical device reported an error. \n
  EFI_NOT_READY         -- Physical device is busy or not ready to
                           process this request.
*/
typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_GET_DISPLAY_IMAGE_TYPE) (
  OUT  EFI_QCOM_CHARGER_DISP_IMAGE_TYPE *pDispImage
);

/* EFI_QCOM_CHARGER_GET_PON_REASON */
/** @ingroup efi_getPonReason
@par Summary
Returns pon reason

@param[out] *pPwrOnReason: power on reason

@return
EFI_SUCCESS           -- Function completed successfully. \n
EFI_INVALID_PARAMETER -- Parameter is invalid. \n
EFI_DEVICE_ERROR      -- Physical device reported an error. \n
EFI_NOT_READY         -- Physical device is busy or not ready to
process this request.
*/
typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_GET_PON_REASON) (
OUT EFI_QCOM_CHARGER_PWRON_REASON_TYPE *pPwrOnReason
);

typedef enum {
  EFI_CHARGER_CONTROL_ACTION_CONTINUE_CHARGING = EFI_QCOM_CHARGER_ACTION_CONTINUE,
  EFI_CHARGER_CONTROL_ACTION_BOOT_TO_HLOS = EFI_QCOM_CHARGER_ACTION_GOOD_TO_BOOT,
  EFI_CHARGER_CONTROL_ACTION_SHUTDOWN = EFI_QCOM_CHARGER_ACTION_SHUTDOWN,
  EFI_CHARGER_CONTROL_ACTION_THERMAL_WAIT = EFI_QCOM_CHARGER_ACTION_TSENSE_HIGH_WAIT,
  EFI_CHARGER_CONTROL_ACTION_RESET_COLD = EFI_QCOM_CHARGER_ACTION_RESET_COLD,
  EFI_CHARGER_CONTROL_ACTION_INVALID = EFI_QCOM_CHARGER_ACTION_INVALID,
}EFI_CHARGER_CONTROL_ACTION_TYPE;

/* EFI_QCOM_CHARGER_CONTROL_GET_ACTION */
/** @ingroup efi_control_get_action
@par Summary
Returns user set boot flow action

@param[out] *pControlAction: external set boot flow action

@return
EFI_SUCCESS           -- Function completed successfully. \n
EFI_INVALID_PARAMETER -- Parameter is invalid. \n
EFI_DEVICE_ERROR      -- Physical device reported an error. \n
EFI_NOT_READY         -- Physical device is busy or not ready to
process this request.
*/
typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_CONTROL_GET_ACTION) (
OUT EFI_CHARGER_CONTROL_ACTION_TYPE *pControlAction
);

/* EFI_QCOM_CHARGER_CONTROL_SET_ACTION */
/** @ingroup efi_control_set_action
@par Summary
Returns status

@param[in] controlAction: external set boot flow action

@return
EFI_SUCCESS           -- Function completed successfully. \n
EFI_INVALID_PARAMETER -- Parameter is invalid. \n
EFI_DEVICE_ERROR      -- Physical device reported an error. \n
EFI_NOT_READY         -- Physical device is busy or not ready to
process this request.
*/
typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_CONTROL_SET_ACTION) (
IN EFI_CHARGER_CONTROL_ACTION_TYPE controlAction
);

/* EFI_QCOM_CHARGER_GET_BATTERY_TYPE */
/** @ingroup efi_get_battery_type
@par Summary
Returns status

@param[out] pBatteryType: pointer to battery type

@return
EFI_SUCCESS           -- Function completed successfully. \n
EFI_INVALID_PARAMETER -- Parameter is invalid. \n
EFI_DEVICE_ERROR      -- Physical device reported an error. \n
EFI_NOT_READY         -- Physical device is busy or not ready to
process this request.
*/

typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_GET_BATTERY_TYPE) (
OUT UINT32  *pBatteryType
);

/* EFI_QCOM_CHARGER_GET_BATTERY_SOC */
/** @ingroup efi_get_battery_soc
@par Summary
Returns status

@param[out] pBatterySOC: pointer to battery SOC value

@return
EFI_SUCCESS           -- Function completed successfully. \n
EFI_INVALID_PARAMETER -- Parameter is invalid. \n
EFI_DEVICE_ERROR      -- Physical device reported an error. \n
EFI_NOT_READY         -- Physical device is busy or not ready to
process this request.
*/

typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_GET_BATTERY_SOC) (
OUT UINT32 *pBatterySOC
);

/* EFI_QCOM_CHARGER_GET_BARREL_CHARGER_STATUS */
/** @ingroup efi_get_barrel_charger_status
@par Summary
Returns status

@param[out] pConnected: pointer to charger connection
@param[out] pCharger_max_voltage_mv: pointer to charger input voltage
@param[out] pCharger_max_current_ma: pointer to charger input current

@return
EFI_SUCCESS           -- Function completed successfully. \n
EFI_INVALID_PARAMETER -- Parameter is invalid. \n
EFI_DEVICE_ERROR      -- Physical device reported an error. \n
EFI_NOT_READY         -- Physical device is busy or not ready to
process this request.
*/
typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_GET_BARREL_CHARGER_STATUS) (
  OUT BOOLEAN* pConnected,
  OUT UINT16*  pCharger_max_voltage_mv,
  OUT UINT16*  pCharger_max_current_ma
);

/* EFI_QCOM_CHARGER_LOGGING_TIMER_EXIT */
/** @ingroup efi_qcom_charger_logging_timer_exit
@par Summary
Closes chargerPD logging timer event, if enabled

 @return
  EFI_SUCCESS           -- Function completed successfully. \n
  EFI_INVALID_PARAMETER -- Parameter is invalid. \n
  EFI_DEVICE_ERROR      -- Physical device reported an error. \n
  EFI_NOT_READY         -- Physical device is busy or not ready to
                           process this request.
*/
typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_LOGGING_TIMER_EXIT) (
  IN EFI_QCOM_CHARGER_ACTION_TYPE ChargingAction
);

/* EFI_QCOM_CHARGER_IS_OFFMODE */
/** @ingroup efi_qcom_charger_is_offmode
@par Summary
Closes chargerPD logging timer event, if enabled

 @return
  EFI_SUCCESS           -- Function completed successfully. \n
  EFI_INVALID_PARAMETER -- Parameter is invalid. \n
  EFI_DEVICE_ERROR      -- Physical device reported an error. \n
  EFI_NOT_READY         -- Physical device is busy or not ready to
                           process this request.
*/
typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_IS_OFFMODE) (
  OUT BOOLEAN* lpm_offmode,
  OUT BOOLEAN* offmode
);

/* EFI_QCOM_CHARGER_GET_MAX_NEGO_PWR */
/** @ingroup efi_qcom_charger_get_max_Pwr
@par Summary
Returns status

 @return
  EFI_SUCCESS           -- Function completed successfully. \n
  EFI_INVALID_PARAMETER -- Parameter is invalid. \n
  EFI_DEVICE_ERROR      -- Physical device reported an error. \n
  EFI_NOT_READY         -- Physical device is busy or not ready to
                           process this request.
*/
typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_GET_MAX_NEGO_PWR) (
  UINT8* pConfig, 
  UINT8* pNegotiatedPwr
);

/* EFI_QCOM_CHARGER_GET_ACTIVE_CHARGER_INFO */
/** @ingroup efi_qcom_charger_get_active_charger_info
@par Summary
Returns status

 @return
  EFI_SUCCESS           -- Function completed successfully. \n
  EFI_INVALID_PARAMETER -- Parameter is invalid. \n
  EFI_DEVICE_ERROR      -- Physical device reported an error. \n
  EFI_NOT_READY         -- Physical device is busy or not ready to
                           process this request.
*/
typedef
EFI_STATUS (EFIAPI *EFI_QCOM_CHARGER_GET_ACTIVE_CHARGER_INFO) (
  UINT32* CurrentChargerPowerUW, 
  UINT32* GoodChargerThresholdUW,
  UINT8* ChargerIndex,
  BOOLEAN* ActiveChargerPresent,
  BOOLEAN* DCINChargerPresent,
  BOOLEAN* WeakCharger
);

/*===========================================================================
  PROTOCOL INTERFACE
===========================================================================*/
/** @ingroup efi_qcomCharger_protocol
  @par Summary
  Qualcomm Charger Protocol interface.

  @par Parameters
  @inputprotoparams{qcom_charger_proto_params.tex}
*/
struct _EFI_QCOM_CHARGER_PROTOCOL
{
  UINT64                                 Revision;
  EFI_QCOM_CHARGER_ENABLE_CHARGING       EnableCharging;
  EFI_QCOM_CHARGER_GET_MAX_USB_CURRENT   GetMaxUsbCurrent;
  EFI_QCOM_CHARGER_SET_MAX_USB_CURRENT   SetMaxUsbCurrent;
  EFI_QCOM_CHARGER_GET_CHARGING_ACTION   GetChargingAction;
  EFI_QCOM_CHARGER_TAKE_ACTION           TakeAction;
  EFI_QCOM_CHARGER_DISPLAY_IMAGE         DisplayImage;
  EFI_QCOM_CHARGER_GET_BATTERY_PRESENCE  GetBatteryPresence;
  EFI_QCOM_CHARGER_GET_BATTERY_VOLTAGE   GetBatteryVoltage;
  EFI_QCOM_CHARGER_DEINIT                ChargerDeInit;
  //EFI_QCOM_CHARGER_GET_LOG_INFO          GetLogInfo;
  EFI_QCOM_CHARGER_DUMP_PERIPHERAL       DumpPeripheral;
  EFI_QCOM_CHARGER_IS_DCIN_VALID         IsDcInValid;
  EFI_QCOM_CHARGER_GET_CHARGER_CONFIG    GetChargerConfig;
  EFI_QCOM_CHARGER_IS_CHARGING_SUPPORTED IsChargingSupported;
  EFI_QCOM_CHARGER_GET_DISPLAY_IMAGE_TYPE GetDisplayImageType;
  EFI_QCOM_CHARGER_GET_PON_REASON        GetPonReason;
  EFI_QCOM_CHARGER_CONTROL_GET_ACTION    GetControlAction;
  EFI_QCOM_CHARGER_CONTROL_SET_ACTION    SetControlAction;
  EFI_QCOM_CHARGER_GET_BATTERY_TYPE      GetBatteryType;
  EFI_QCOM_CHARGER_GET_BATTERY_SOC       GetBatterySOC;
  EFI_QCOM_CHARGER_GET_BARREL_CHARGER_STATUS GetBarrelChargerStatus;
  EFI_QCOM_CHARGER_LOGGING_TIMER_EXIT    ChargerPDLogTimerExit;
  EFI_QCOM_CHARGER_IS_OFFMODE            IsOffmode;
  //added in QCOM_CHARGER_REVISION_13
  EFI_QCOM_CHARGER_GET_MAX_NEGO_PWR      GetMaxNegotiatedPwr;
  EFI_QCOM_CHARGER_GET_ACTIVE_CHARGER_INFO GetActiveChargerInfo;
};

#endif  /* __EFIQCCHARGER_H__*/
