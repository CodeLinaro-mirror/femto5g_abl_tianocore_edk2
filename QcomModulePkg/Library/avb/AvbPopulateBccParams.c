/*
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved. SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "AvbPopulateBccParams.h"

/* SetDummyBccParams will set the BccParams all 0s.
 */
STATIC void
SetDummyBccParams (BccParams_t *bcc_params)
{
#ifndef USE_OPENDICE_UDS_DERIVATION
    avb_memset ((void *)bcc_params, 0, sizeof (*bcc_params));
    DEBUG ((EFI_D_INFO, "VB: Setting Dummy DICE params\n"));
#endif
    /* AVF debug policy requires mode to be in debug */
    if (IsUnlocked ()) {
         bcc_params->Mode = kDiceModeDebug;
    }
}

// Convert a date string in format "YYYY-MM-DD" (Vbmeta), to integer YYYYMMDD (OpenDICE).
// Returns 0 on success, -1 on error. Result is written to *out_val.
static int ConvertPatchLevelDateToInt(const char *patchlevel, uint64_t *out_val)
{
    int year, month, day;

    if (!patchlevel || !out_val)
        return -1;

    /* Basic format check */
    if (patchlevel[4] != '-' ||
        patchlevel[7] != '-')
        return -1;

    /* Convert digits manually */
    year  = (patchlevel[0] - '0') * 1000 +
            (patchlevel[1] - '0') * 100 +
            (patchlevel[2] - '0') * 10  +
            (patchlevel[3] - '0');

    month = (patchlevel[5] - '0') * 10 +
            (patchlevel[6] - '0');

    day   = (patchlevel[8] - '0') * 10 +
            (patchlevel[9] - '0');

    /* Optional sanity checks */
    if (month < 1 || month > 12 ||
        day   < 1 || day   > 31)
        return -1;

    *out_val = (uint64_t)(year * 10000 + month * 100 + day);
    return 0;
}

// This is an helper function, to lookup a key-value pairs stored as property descriptor in Vbmeta.
static int GetVbmetaProp(AvbSlotVerifyData *slot_data, const char *prop_key, const char **prop_val)
{
	size_t n;
	size_t prop_val_size;

	// Go over all vbmeta's in the vbmeta chain
	for (n = 0; n < slot_data->num_vbmeta_images; n++)
	{
		*prop_val = avb_property_lookup(slot_data->vbmeta_images[n].vbmeta_data,
										slot_data->vbmeta_images[n].vbmeta_size,
										prop_key,
										avb_strlen(prop_key),
										&prop_val_size);
		if (*prop_val)
		{
			break;
		}
	}

	// The prop_key wasn't found
	if (n == slot_data->num_vbmeta_images)
	{
		DEBUG ((EFI_D_ERROR, "GetVbmetaProp: Key not found"));
		return -1;
	}

	return 0;
}

/**
 * Update the BCC params with the "component version" attribue, as retrieved from vbmeta.
 * The component version is stored in a dedicated property descriptor in vbmeta.
 *
 * @param [in]  SlotData      Pointer to slot data from AVB
 * @param [out] bcc_params	  BCC input parameters structure
 *
 * @returns 0 on success or -1 on error.
 */
static int PopulateComponentVersion(AvbSlotVerifyData *SlotData, BccParams_t *bcc_params)
{
	const char *prop_val;

	// Lookup property key "com.android.build.system.os_version".
	if (GetVbmetaProp(SlotData, "com.android.build.system.os_version", &prop_val))
	{
		DEBUG ((EFI_D_ERROR, "PopulateComponentVersion: Not found"));
		return -1;
	}

	// Returned value is a string, convert to integer.
	bcc_params->ChildImage.ComponentVersion = AsciiStrDecimalToUint64(prop_val);

	DEBUG ((EFI_D_INFO, "PopulateComponentVersion: component_version=%ld \n", bcc_params->ChildImage.ComponentVersion));

	return 0;
}

/**
 * Update the BCC params with the "security version" attribue, as retrieved from vbmeta.
 * The security version is stored in a dedicated property descriptor in vbmeta.
 *
 * @param [in]  SlotData      Pointer to slot data from AVB
 * @param [out] bcc_params	  BCC input parameters structure
 *
 * @returns 0 on success or -1 on error.
 */
static int PopulateSecurityVersion(AvbSlotVerifyData *SlotData, BccParams_t *bcc_params)
{
	const char *prop_val;
	uint64_t ret = 0;

	// Lookup property key "com.android.build.system.security_patch".
	if (GetVbmetaProp(SlotData, "com.android.build.system.security_patch", &prop_val))
	{
		DEBUG ((EFI_D_ERROR, "PopulateSecurityVersion: Not found"));
		return -1;
	}

	// Returned value is a string, convert to YYYYMMDD integer format.
	if (ConvertPatchLevelDateToInt(prop_val, &ret) != 0)
	{
		DEBUG ((EFI_D_ERROR, "PopulateSecurityVersion: Failed in string to int conversion"));
		return -1;
	}

	bcc_params->ChildImage.SecurityVersion = ret;

	DEBUG ((EFI_D_INFO, "PopulateSecurityVersion: security_version=%ld \n", bcc_params->ChildImage.SecurityVersion));

	return 0;
}

/**
 * Update the BCC params with the "component instance name" attribue.
 * The component instance name should be customized by OEM.
 *
 * @param [in]  SlotData      Pointer to slot data from AVB
 * @param [out] bcc_params	  BCC input parameters structure
 *
 * @returns 0 on success or -1 on error.
 */
static int PopulateComponentInstanceName(AvbSlotVerifyData *SlotData, BccParams_t *bcc_params)
{
	const char* component_instance_name = "la";
	uint32_t StringLen = 0;

	// As a reference, use hard coded "la" to represent LA GVM
	StringLen = avb_strlen(component_instance_name);

	avb_memcpy (bcc_params->ChildImage.ComponentInstanceName,
				component_instance_name,
				StringLen);
	bcc_params->ChildImage.ComponentInstanceName[StringLen] = '\0';

	DEBUG ((EFI_D_INFO, "PopulateComponentInstanceName: component_instance_name=%a \n", bcc_params->ChildImage.ComponentInstanceName));

	return 0;
}

/**
 * Update the BCC params with the "Verified Boot State" attribue.
 * The verified boot state is calculated based on the AVB boot state.
 *
 * @param [in]  SlotData      Pointer to slot data from AVB
 * @param [out] bcc_params	  BCC input parameters structure
 *
 * @returns 0 on success or -1 on error.
 */
static int PopulateVerifiedBootState(AvbSlotVerifyData *SlotData, BccParams_t *bcc_params, BootInfo *Info)
{
	const char* verified_boot_state;
	uint32_t StringLen = 0;

	switch (Info->BootState) {
		case RED:
		verified_boot_state = "red";
		break;

		case GREEN:
		verified_boot_state = "green";
		break;

		case YELLOW:
		verified_boot_state = "yellow";
		break;

		case ORANGE:
		verified_boot_state = "orange";
		break;

		default:
		verified_boot_state = "unknown";
	}

	StringLen = avb_strlen(verified_boot_state);

	avb_memcpy (bcc_params->ChildImage.VerifiedBootState,
				verified_boot_state,
				StringLen);
	bcc_params->ChildImage.VerifiedBootState[StringLen] = '\0';

	DEBUG ((EFI_D_INFO, "PopulateVerifiedBootState: verified_boot_state=%a \n", bcc_params->ChildImage.VerifiedBootState));

	return 0;
}

/**
 * Update the BCC params with the "vendor security version" attribue, as retrieved from vbmeta.
 * The vendor security version is stored in a dedicated property descriptor in vbmeta.
 *
 * @param [in]  SlotData      Pointer to slot data from AVB
 * @param [out] bcc_params	  BCC input parameters structure
 *
 * @returns 0 on success or -1 on error.
 */
static int PopulateVendorSecurityVersion(AvbSlotVerifyData *SlotData, BccParams_t *bcc_params)
{
	const char *prop_val;
	uint64_t ret = 0;

	// Lookup property key "com.android.build.vendor.security_patch".
	if (GetVbmetaProp(SlotData, "com.android.build.vendor.security_patch", &prop_val))
	{
		DEBUG ((EFI_D_ERROR, "PopulateVendorSecurityVersion: Not found"));
		return -1;
	}

	// Returned value is a string, convert to YYYYMMDD integer format.
	if (ConvertPatchLevelDateToInt(prop_val, &ret) != 0)
	{
		DEBUG ((EFI_D_ERROR, "PopulateVendorSecurityVersion: Failed in string to int conversion"));
		return -1;
	}

	bcc_params->ChildImage.VendorSecurityVersion = ret;

	DEBUG ((EFI_D_INFO, "PopulateVendorSecurityVersion: vendor_security_version=%ld \n", bcc_params->ChildImage.VendorSecurityVersion));

	return 0;
}

/**
 * Update the BCC params with the "boot security version" attribue, as retrieved from vbmeta.
 * The boot security version is stored in a dedicated property descriptor in vbmeta.
 *
 * @param [in]  SlotData      Pointer to slot data from AVB
 * @param [out] bcc_params	  BCC input parameters structure
 *
 * @returns 0 on success or -1 on error.
 */
static int PopulateBootSecurityVersion(AvbSlotVerifyData *SlotData, BccParams_t *bcc_params)
{
	const char *prop_val;
	uint64_t ret = 0;

	// Lookup property key "com.android.build.boot.security_patch".
	if (GetVbmetaProp(SlotData, "com.android.build.boot.security_patch", &prop_val))
	{
		DEBUG ((EFI_D_ERROR, "PopulateBootSecurityVersion: Not found"));
		return -1;
	}

	// Returned value is a string, convert to YYYYMMDD integer format.
	if (ConvertPatchLevelDateToInt(prop_val, &ret) != 0)
	{
		DEBUG ((EFI_D_ERROR, "PopulateBootSecurityVersion: Failed in string to int conversion"));
		return -1;
	}

	bcc_params->ChildImage.BootSecurityVersion = ret;

	DEBUG ((EFI_D_INFO, "PopulateBootSecurityVersion: boot_security_version=%ld \n", bcc_params->ChildImage.BootSecurityVersion));

	return 0;
}

/**
 * Update the BCC params with the "product security version" attribue, as retrieved from vbmeta.
 * The product security version is stored in a dedicated property descriptor in vbmeta.
 *
 * @param [in]  SlotData      Pointer to slot data from AVB
 * @param [out] bcc_params	  BCC input parameters structure
 *
 * @returns 0 on success or -1 on error.
 */
static int PopulateProductSecurityVersion(AvbSlotVerifyData *SlotData, BccParams_t *bcc_params)
{
	const char *prop_val;
	uint64_t ret = 0;

	// Lookup property key "com.android.build.product.security_patch".
	if (GetVbmetaProp(SlotData, "com.android.build.product.security_patch", &prop_val))
	{
		DEBUG ((EFI_D_ERROR, "PopulateProductSecurityVersion: Not found"));
		return -1;
	}

	// Returned value is a string, convert to YYYYMMDD integer format.
	if (ConvertPatchLevelDateToInt(prop_val, &ret) != 0)
	{
		DEBUG ((EFI_D_ERROR, "PopulateProductSecurityVersion: Failed in string to int conversion"));
		return -1;
	}

	bcc_params->ChildImage.ProductSecurityVersion = ret;

	DEBUG ((EFI_D_INFO, "PopulateProductSecurityVersion: product_security_version=%ld \n", bcc_params->ChildImage.ProductSecurityVersion));

	return 0;
}

/**
 * Update the BCC params with the "system_ext security version" attribue, as retrieved from vbmeta.
 * The product security version is stored in a dedicated property descriptor in vbmeta.
 *
 * @param [in]  SlotData      Pointer to slot data from AVB
 * @param [out] bcc_params	  BCC input parameters structure
 *
 * @returns 0 on success or -1 on error.
 */
static int PopulateSystemExtSecurityVersion(AvbSlotVerifyData *SlotData, BccParams_t *bcc_params)
{
	const char *prop_val;
	uint64_t ret = 0;

	// Lookup property key "com.android.build.system_ext.security_patch".
	if (GetVbmetaProp(SlotData, "com.android.build.system_ext.security_patch", &prop_val))
	{
		DEBUG ((EFI_D_ERROR, "PopulateSystemExtSecurityVersion: Not found"));
		return -1;
	}

	// Returned value is a string, convert to YYYYMMDD integer format.
	if (ConvertPatchLevelDateToInt(prop_val, &ret) != 0)
	{
		DEBUG ((EFI_D_ERROR, "PopulateSystemExtSecurityVersion: Failed in string to int conversion"));
		return -1;
	}

	bcc_params->ChildImage.SystemExtSecurityVersion = ret;

	DEBUG ((EFI_D_INFO, "PopulateSystemExtSecurityVersion: system_ext_security_version=%ld \n", bcc_params->ChildImage.SystemExtSecurityVersion));

	return 0;
}

/**
 * Update the BCC params with the "build fingerprint" attribue, as retrieved from vbmeta.
 * The build fingerprint is stored in a dedicated property descriptor in vbmeta.
 *
 * @param [in]  SlotData      Pointer to slot data from AVB
 * @param [out] bcc_params	  BCC input parameters structure
 *
 * @returns 0 on success or -1 on error.
 */
static int PopulateBuildFingerprint(AvbSlotVerifyData *SlotData, BccParams_t *bcc_params)
{
	const char *prop_val;
	uint32_t StringLen = 0;

	if (GetVbmetaProp(SlotData, "com.android.build.system.fingerprint", &prop_val)) {
		DEBUG ((EFI_D_ERROR, "PopulateBuildFingerprint: Not found"));
		return -1;
	}

	StringLen = avb_strlen(prop_val);

	if (StringLen >= BCC_BUILD_FINGERPRINT_BUFFER_MAX_SIZE) {
		DEBUG ((EFI_D_ERROR, "BuildFingerprint too long: %u\n", StringLen));
		return -1;
	}

	avb_memcpy (bcc_params->ChildImage.BuildFingerprint,
                prop_val,
                StringLen);
    bcc_params->ChildImage.BuildFingerprint[StringLen] = '\0';

	DEBUG ((EFI_D_INFO, "PopulateBuildFingerprint: build_fingerprint=%a \n", bcc_params->ChildImage.BuildFingerprint));

	return 0;
}

/**
 * Update the BCC params with the "SDV Boot Mode" attribue.
 * The SDV boot mode should be runtime settable by OEM.
 *
 * @param [in]  SlotData      Pointer to slot data from AVB
 * @param [out] bcc_params	  BCC input parameters structure
 *
 * @returns 0 on success or -1 on error.
 */
static int PopulateSdvBootMode(AvbSlotVerifyData *SlotData, BccParams_t *bcc_params)
{
	const char* sdv_boot_mode;
	uint32_t StringLen = 0;

	// As a reference only, set the "SDV Boot Mode" to the "AVB Boot Mode".
	// This is not per the spec, the actual "SDV Boot Mode" should be settable
	// in runtime by the OEM.
	sdv_boot_mode = IsUnlocked() ? "unlocked" : "locked";

	StringLen = avb_strlen(sdv_boot_mode);

	avb_memcpy (bcc_params->ChildImage.SdvBootMode,
				sdv_boot_mode,
				StringLen);
	bcc_params->ChildImage.SdvBootMode[StringLen] = '\0';

	DEBUG ((EFI_D_INFO, "PopulateSdvBootMode: sdv_boot_mode=%a \n", bcc_params->ChildImage.SdvBootMode));

	return 0;
}

/* PopulateAuthorityHash will Populate the Authority Hash for BCC Params.
 * The authority hash will be hash of public key of vbmeta and
 * vbmeta_system
 */

STATIC EFI_STATUS
PopulateAuthorityHash (AvbSlotVerifyData *SlotData, BccParams_t *bcc_params)
{
    EFI_STATUS Status = EFI_SUCCESS;
    const uint8_t* PkData = NULL;
    size_t PkLen = 0;
    AvbSHA512Ctx Ctx = {{0}};
    size_t Index = 0;
    uint8_t* authoritydigest = NULL;
    AvbVBMetaImageHeader VbmetaHeader = {{0}};

    avb_sha512_init (&Ctx);
    for (Index = 0; Index < SlotData->num_vbmeta_images; Index++) {
        /* Authority hash includes hash of vbmeta's public key
         * and vbmeta_system's public key
         */
        if (avb_strcmp
            (SlotData->vbmeta_images[Index].partition_name, "vbmeta")
            == 0 ||
           avb_strcmp
            (SlotData->vbmeta_images[Index].partition_name, "vbmeta_system")
            == 0) {
            if (SlotData->vbmeta_images[Index].vbmeta_data == NULL) {
                Status = EFI_INVALID_PARAMETER;
                goto out;
            }
            avb_vbmeta_image_header_to_host_byte_order (
            (AvbVBMetaImageHeader*)(SlotData->vbmeta_images[Index].vbmeta_data),
                &VbmetaHeader);
            PkData = SlotData->vbmeta_images[Index].vbmeta_data +
                     sizeof (AvbVBMetaImageHeader) +
                     VbmetaHeader.authentication_data_block_size +
                     VbmetaHeader.public_key_offset;
            PkLen = VbmetaHeader.public_key_size;
            avb_sha512_update (&Ctx, PkData, PkLen);
        }
    }
    if (&Ctx == NULL ||
        bcc_params->ChildImage.AuthorityHash == NULL) {
        Status = EFI_INVALID_PARAMETER;
        goto out;
    }

    authoritydigest = avb_sha512_final (&Ctx);
    if (authoritydigest == NULL) {
        Status = EFI_INVALID_PARAMETER;
        goto out;
    }
    avb_memcpy (bcc_params->ChildImage.AuthorityHash, authoritydigest,
                DICE_HASH_SIZE);
out:
    return Status;
}

/* PopulateBccImgParams will populate Image measurements like image name,
 * code hash and authority hash. For now, only pvmfw image measurement is
 * populated.
 */
STATIC EFI_STATUS
PopulateBccImgParams (AvbSlotVerifyData *SlotData, BccParams_t *bcc_params,
                      BootInfo *Info, uint32_t PartitionIndex)
{
    EFI_STATUS Status = EFI_SUCCESS;
    AvbSHA512Ctx CodeCtx = {{0}};
    uint8_t* CodeDigest = NULL;
    uint32_t PnameLen = 0;
    static char* SdvDiceComponentNamePvm = "PVM";
    static char* SdvDiceComponentNameGvm = "GVM";
    uint32_t VbmetaIndex;
    char** partition_name = NULL;

    Status = PopulateAuthorityHash (SlotData, bcc_params);
    if (Status != EFI_SUCCESS) {
        DEBUG ((EFI_D_ERROR, "VB: PopulateAuthorityHash: failed with Status:%r",
              Status));
        goto out;
    }

    if (SlotData->loaded_partitions[PartitionIndex].partition_name == NULL ||
        bcc_params->ChildImage.ComponentName == NULL ||
        SlotData->loaded_partitions[PartitionIndex].data == NULL ||
        bcc_params->ChildImage.CodeHash == NULL) {
        Status = EFI_INVALID_PARAMETER;
        goto out;
    }

    if (!Info->HasSdvDiceEnabled) {
        partition_name = &SlotData->loaded_partitions[PartitionIndex].partition_name;
    } else {
        /* For SDV, the ComponentName is "PVM"/"GVM", and not the image name. */
        if (Info->SdvDiceLeaf)
            partition_name = &SdvDiceComponentNameGvm;
        else
            partition_name = &SdvDiceComponentNamePvm;
    }

    PnameLen = avb_strlen(*partition_name);
    if (PnameLen >= BCC_COMPONENT_NAME_BUFFER_MAX_SIZE) {
      Status = EFI_BUFFER_TOO_SMALL;
      goto out;
    }

    avb_memcpy (bcc_params->ChildImage.ComponentName,
                *partition_name,
                PnameLen);
    bcc_params->ChildImage.ComponentName[PnameLen] = '\0';

    avb_sha512_init (&CodeCtx);
    avb_sha512_update (&CodeCtx,
                       SlotData->loaded_partitions[PartitionIndex].data,
                       SlotData->loaded_partitions[PartitionIndex].data_size);

    /* For SDV, Vbmeta images are also included in the CodeDigest. */
    if (Info->HasSdvDiceEnabled) {
      for (VbmetaIndex = 0; VbmetaIndex < SlotData->num_vbmeta_images; VbmetaIndex++) {
            if (SlotData->vbmeta_images[VbmetaIndex].vbmeta_data != NULL) {
              avb_sha512_update (&CodeCtx,
                SlotData->vbmeta_images[VbmetaIndex].vbmeta_data,
                SlotData->vbmeta_images[VbmetaIndex].vbmeta_size);
            }
      }
    }

    CodeDigest = avb_sha512_final (&CodeCtx);
    if (CodeDigest == NULL) {
        Status = EFI_INVALID_PARAMETER;
        goto out;
    }
    avb_memcpy (bcc_params->ChildImage.CodeHash, CodeDigest, DICE_HASH_SIZE);

out:
    return Status;
}

/* PopulateBccParams will populate BCC measurements for DICE Engine, which
 * includes Authority hash, Code Hash and Mode.
 */
EFI_STATUS
PopulateBccParams (AvbSlotVerifyData *SlotData,
                   BootInfo *Info, BccParams_t *bcc_params)
{
    EFI_STATUS Status = EFI_SUCCESS;
    char* partition_measured_pvmfw = "pvmfw";
    char* partition_measured_sdv = "boot";
    char** partition_measured = NULL;

    if (SlotData == NULL ||
        bcc_params == NULL || Info == NULL) {
        DEBUG ((EFI_D_ERROR, "VB: PopulateBccParams: Parameter received"
                "is NULL"));
        Status = EFI_INVALID_PARAMETER;
        goto out;
    }

    if (Info->HasSdvDiceEnabled) {
       partition_measured = &partition_measured_sdv;
    }
    else {
       partition_measured = &partition_measured_pvmfw;
    }

    // Set the DICE mode
    if (Info->BootIntoRecovery) {
         bcc_params->Mode = kDiceModeMaintenance;
    } else if (IsUnlocked ()) {
         bcc_params->Mode = kDiceModeDebug;
    } else {
         bcc_params->Mode = kDiceModeNormal;
    }

    Status = KeyMasterGetFRSAndUDS (bcc_params);
    if (Status != EFI_SUCCESS) {
        DEBUG ((EFI_D_ERROR, "VB: AvbPopulateBccParams: failed with"
                " Status:%r\n", Status));

         SetDummyBccParams (bcc_params);
         goto out;
    }

    for (UINTN LoadedIndex = 0; LoadedIndex < SlotData->num_loaded_partitions;
        LoadedIndex++) {
        DEBUG ((EFI_D_ERROR, "Loaded Partition: %a\n",
                SlotData->loaded_partitions[LoadedIndex].partition_name));
        if (avb_strcmp (SlotData->loaded_partitions[LoadedIndex].partition_name,
                        *partition_measured) == 0 ) {
            if (SlotData->loaded_partitions[LoadedIndex].verify_result ==
                AVB_SLOT_VERIFY_RESULT_OK) {
                Status = PopulateBccImgParams (SlotData, bcc_params,
                                               Info, LoadedIndex);
                if (Status != EFI_SUCCESS) {
                    DEBUG ((EFI_D_ERROR, "VB: PopulateBccImgParams: failed with"
                            " Status:%r\n", Status));
                    goto out;
                }
                DEBUG ((EFI_D_INFO, "VB: Bcc Params populated\n"));
            } else {
                SetDummyBccParams (bcc_params);
            }
            break;
        }
    }

	if (Info->HasSdvDiceEnabled && Info->SdvDiceLeaf) {
		Status = PopulateComponentVersion (SlotData, bcc_params);
		if (Status != EFI_SUCCESS) {
			DEBUG ((EFI_D_ERROR, "VB: PopulateComponentVersion: failed with Status:%r",
				Status));
			goto out;
		}

		Status = PopulateSecurityVersion (SlotData, bcc_params);
		if (Status != EFI_SUCCESS) {
			DEBUG ((EFI_D_ERROR, "VB: PopulateSecurityVersion: failed with Status:%r",
				Status));
			goto out;
		}

		Status = PopulateComponentInstanceName (SlotData, bcc_params);
		if (Status != EFI_SUCCESS) {
			DEBUG ((EFI_D_ERROR, "VB: PopulateComponentInstanceName: failed with Status:%r",
				Status));
			goto out;
		}

		Status = PopulateVerifiedBootState (SlotData, bcc_params, Info);
		if (Status != EFI_SUCCESS) {
			DEBUG ((EFI_D_ERROR, "VB: PopulateVerifiedBootState: failed with Status:%r",
				Status));
			goto out;
		}

		Status = PopulateVendorSecurityVersion (SlotData, bcc_params);
		if (Status != EFI_SUCCESS) {
			DEBUG ((EFI_D_ERROR, "VB: PopulateVendorSecurityVersion: failed with Status:%r",
				Status));
			goto out;
		}

		Status = PopulateBootSecurityVersion (SlotData, bcc_params);
		if (Status != EFI_SUCCESS) {
			DEBUG ((EFI_D_ERROR, "VB: PopulateBootSecurityVersion: failed with Status:%r",
				Status));
			goto out;
		}

		Status = PopulateProductSecurityVersion (SlotData, bcc_params);
		if (Status != EFI_SUCCESS) {
			DEBUG ((EFI_D_ERROR, "VB: PopulateProductSecurityVersion: failed with Status:%r",
				Status));
			goto out;
		}

		Status = PopulateSystemExtSecurityVersion (SlotData, bcc_params);
		if (Status != EFI_SUCCESS) {
			DEBUG ((EFI_D_ERROR, "VB: PopulateSystemExtSecurityVersion: failed with Status:%r",
				Status));
			goto out;
		}

		Status = PopulateBuildFingerprint (SlotData, bcc_params);
		if (Status != EFI_SUCCESS) {
			DEBUG ((EFI_D_ERROR, "VB: PopulateBuildFingerprint: failed with Status:%r",
				Status));
			goto out;
		}

		Status = PopulateSdvBootMode (SlotData, bcc_params);
		if (Status != EFI_SUCCESS) {
			DEBUG ((EFI_D_ERROR, "VB: PopulateSdvBootMode: failed with Status:%r",
				Status));
			goto out;
		}
	}

out:
  return Status;
}
