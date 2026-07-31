/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "QcBcc.h"
#include "opendice-util.h"

#include <Library/DeviceInfo.h>
#include <LinuxLoaderLib.h>
#include <dice/android.h>
#include <dice/cbor_writer.h>
#include <dice/ops.h>
#include <dice/ops/trait/cose.h>
#include <dice/utils.h>

#include "SmciInvokeUtils.h"
#include "CRkpBCC.h"
#include "IRkpBCC.h"
#include <Protocol/EFIScm.h>

/* Max size of COSE_Sign1 including payload. */
#define BCC_MAX_CERTIFICATE_SIZE 512

/*
 * Size of a BCC artifacts handed over from root (without Bcc) is:
 * CBOR tags + Two CDIs = 71
 */
#define BCC_ARTIFACTS_WO_BCC_TOTAL_SIZE 71

/* Actual Size of BCC Configuration Descriptor field */
#define BCC_CONFIG_DESCRIPTOR_TOTAL_SIZE 48

/* Set of information required to derive DICE artifacts for the child node. */
typedef struct BccChildParams {
  UINT8 CodeHash[DICE_HASH_SIZE];      /* Code Hash */
  UINT8 AuthorityHash[DICE_HASH_SIZE]; /* Authority Hash */
  DiceAndroidConfigValues BccCfgDesc;  /* Bcc Config Descriptor */
} BccChildParams_t;

typedef struct BccRootState {
  /* Unique Device Secret */
  UINT8 Uds[DICE_CDI_SIZE]; /* Unique Device Secret */
  /* Public key of the key pair derived from a seed derived from Uds */
  UINT8 UdsPubKey[DICE_PUBLIC_KEY_BUFFER_SIZE];
  /* Secret with factory reset life time */
  UINT8 Frs[DICE_HIDDEN_SIZE];
  /* Device Mode */
  DiceMode Mode;
  /* Parameters of next stage/child Image */
  BccChildParams_t ChildImage;
} BccRoot_t;

/* Set of BCC artifacts (BCC Handover Format) passed on from one stage
   to the next */
typedef struct BCCArtifacts {
  UINT8 NextCDIAttest[DICE_CDI_SIZE];
  UINT8 NextCDISeal[DICE_CDI_SIZE];
  UINT8 NextBCC[BCC_MAX_CERTIFICATE_SIZE];
  size_t NextBCCSize;
} BCCArtifacts_t;

STATIC CONST INT64 KCdiAttestLabel = 1;
STATIC CONST INT64 KCdiSealLabel = 2;
STATIC Object AppClientObj = Object_NULL;
STATIC Object ClientEnvObj = Object_NULL;
STATIC QCOM_SCM_PROTOCOL *pQcomScmProtocol = NULL;
STATIC size_t NextBccEncodedCDIsSize = 0;

/* Data structure that holds details of root node and the parameters of
   the images that will be used to generated the final encoded BCC Artifacts */
static BccRoot_t BccRoot;

/* API fetches BCC size from BCC service*/
static EFI_STATUS
GetRkpBCCSize (VOID)
{
  EFI_STATUS Status = EFI_SUCCESS;
  RkpBCCInfo BccInfo = {0};

  // Locate QCOM_SCM_PROTOCOL.
  Status = gBS->LocateProtocol (&gQcomScmProtocolGuid, NULL,
                                (VOID **)&pQcomScmProtocol);
  if (Status != EFI_SUCCESS ||
      (pQcomScmProtocol == NULL)) {
    DEBUG ((EFI_D_ERROR,
            "GetRkpBCCSize: Locate SCM Protocol failed, Status: (0x%x)\n",
            Status));
    return Status;
  }

  if (pQcomScmProtocol->ScmGetClientEnv == NULL) {
    DEBUG ((EFI_D_ERROR, "GetRkpBCCSize: ScmGetClientEnv is NULL"));
    return EFI_NOT_FOUND;
  }

  Status = pQcomScmProtocol->ScmGetClientEnv (pQcomScmProtocol, &ClientEnvObj);
  if (Object_isERROR (Status) ||
      Object_isNull (ClientEnvObj)) {
    DEBUG ((EFI_D_ERROR,
            "GetRkpBCCSize: Failed to get Client Env, Status: (0x%x)\n",
            Status));
    goto out2;
  }

  Status = IClientEnvOpen (ClientEnvObj, CRkpBCCABL_UID, &AppClientObj);
  if (Object_isERROR (Status) ||
      Object_isNull (AppClientObj)) {
    DEBUG ((EFI_D_ERROR,
            "GetRkpBCCSize: Failed to get App Client, Status: (0x%x) UID=%d\n",
            Status, CRkpBCCABL_UID));
    goto out2;
  }

  Status = IRkpBCC_getRkpBCCInfo (AppClientObj, &BccInfo);
  if (Object_isERROR (Status)) {
    DEBUG ((EFI_D_ERROR,
            "GetRkpBCCSize: Failed to get App Opener Object, Status: (0x%x)\n",
            Status));
    goto out1;
  }
  /* Copy the size of BCC, received from BCC service*/
  NextBccEncodedCDIsSize = BccInfo.MinimumBccBufferSize;
  return Status;

out1:
  Object_ASSIGN_NULL (AppClientObj);
out2:
  Object_ASSIGN_NULL (ClientEnvObj);

  return Status;
}

/* This API fetches RkpBCC from BCC Service*/
static EFI_STATUS
GetRkpBCC (UINT8 *bcc, size_t *bccValidSize)
{
  EFI_STATUS Status = EFI_SUCCESS;
  Object BccNextStageSecret = Object_NULL;

  if (pQcomScmProtocol == NULL) {
    // Locate QCOM_SCM_PROTOCOL.
    Status = gBS->LocateProtocol (&gQcomScmProtocolGuid, NULL,
                                  (VOID **)&pQcomScmProtocol);
    if (Status != EFI_SUCCESS ||
        (pQcomScmProtocol == NULL)) {
      DEBUG ((EFI_D_ERROR,
              "GetRkpBCC: Locate SCM Protocol failed, Status: (0x%x)\n",
              Status));
      return Status;
    }
  }

  if (Object_isNull (ClientEnvObj)) {
    if (pQcomScmProtocol->ScmGetClientEnv == NULL) {
      DEBUG ((EFI_D_ERROR, "GetRkpBCC: ScmGetClientEnv is NULL"));
      return EFI_NOT_FOUND;
    }
    Status =
        pQcomScmProtocol->ScmGetClientEnv (pQcomScmProtocol, &ClientEnvObj);
    if (Object_isERROR (Status) ||
        Object_isNull (ClientEnvObj)) {
      DEBUG ((EFI_D_ERROR,
              "GetRkpBCC: Failed to get Client Env, Status: (0x%x)\n", Status));
      goto out;
    }
  }

  if (Object_isNull (AppClientObj)) {
    Status = IClientEnvOpen (ClientEnvObj, CRkpBCCABL_UID, &AppClientObj);
    if (Object_isERROR (Status) ||
        Object_isNull (AppClientObj)) {
      DEBUG ((EFI_D_ERROR,
              "GetRkpBCC: Failed to get App Client, Status: (0x%x) UID=%d\n",
              Status, CRkpBCCABL_UID));
      goto out;
    }
  }

  /* Fetch BCC from QTEE service*/
  Status =
      IRkpBCC_getRkpFinalBCC (AppClientObj, (void *)bcc, NextBccEncodedCDIsSize,
                              bccValidSize, &BccNextStageSecret);
  if (Object_isERROR (Status)) {
    DEBUG ((EFI_D_ERROR,
            "GetRkpBCC: Failed to get App Opener Object, Status: (0x%x)\n",
            Status));
  }

  Object_ASSIGN_NULL (BccNextStageSecret);
  Object_ASSIGN_NULL (AppClientObj);
out:
  Object_ASSIGN_NULL (ClientEnvObj);

  return Status;
}

/* This API fetches DICE FRS from DevInfo*/
static EFI_STATUS
GetDICEFRS ()
{
  EFI_STATUS Status = EFI_SUCCESS;
  DeviceInfo *Devinfo = NULL;

  Devinfo = AllocateZeroPool (sizeof (DeviceInfo));
  if (Devinfo == NULL) {
    DEBUG ((EFI_D_ERROR, "Failed to allocate zero pool for device info.\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  /* Read FRS from DevInfo*/
  Status =
      ReadWriteDeviceInfo (READ_CONFIG, (VOID *)Devinfo, sizeof (DeviceInfo));

  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR,
            "GetDICEFRS: Unable to Read Device Info:"
            " %r\n",
            Status));
    goto out;
  }

  /* Check if DICE FRS is not Generated*/
  if (Devinfo->Dice_frs_len == 0) {
    /* Generate FRS and maintain copy in DevInfo*/
    Status = GenerateDICEFRS (Devinfo);
    if (Status != EFI_SUCCESS) {
      DEBUG ((EFI_D_ERROR, "Unable to Generate DICEFRS: %r\n", Status));
      goto out;
    }
  }

  // Copy DICE FRS (Factory Reset Sequence)
  memcpy (BccRoot.Frs, Devinfo->Dice_frs, DICE_HIDDEN_SIZE);

out:
  if (Devinfo) {
    FreePool (Devinfo);
    Devinfo = NULL;
  }
  return Status;
}

/* This API is called when HW backed BCC is not supported.
 * It returns BCC artifacts in the handover format.
 */
DiceResult
#ifndef USE_DUMMY_BCC
GetSWBccArtifacts (UINT8 *FinalEncodedBccArtifacts,
                   size_t BccArtifactsBufferSize,
                   size_t *BccArtifactsValidSize,
                   BccParams_t BccParamsRecvdFromAVB)
#else
GetSWBccArtifacts (UINT8 *FinalEncodedBccArtifacts,
                   size_t BccArtifactsBufferSize,
                   size_t *BccArtifactsValidSize)
#endif
{
  UINT8 UdsPrivateKeySeed[DICE_PRIVATE_KEY_SEED_SIZE] = {0};
  UINT8 UdsPrivateKey[DICE_PRIVATE_KEY_BUFFER_SIZE] = {0};
  UINT8 BccEncodedConfigDesc[BCC_CONFIG_DESCRIPTOR_TOTAL_SIZE] = {0};
  UINT8 NextBccEncodedCDIs[BCC_ARTIFACTS_WO_BCC_TOTAL_SIZE] = {0};
  size_t BccEncodedConfigDescValidSize = 0;
  size_t NextBccEncodedCDIsValidSize = 0;
  DiceInputValues BccInputValues = {{0}};
  struct CborOut Out = {NULL, 0, 0};
  DiceResult Result = kDiceResultOk;

#ifdef USE_DUMMY_BCC
  // Fill some hard code values here for now. AVB team has to provide
  // the real values.
  BccParams_t BccParamsRecvdFromAVB = {{0}};
  memcpy ((void *)BccParamsRecvdFromAVB.ChildImage.ComponentName, "pvmfw", 5);
#endif

  assert (FinalEncodedBccArtifacts);
  assert (BccArtifactsValidSize);
  assert (BccArtifactsBufferSize >= BCC_ARTIFACTS_WITH_BCC_TOTAL_SIZE);

  //---------------------------------------------------------------------
  // Populate BCC Root Data Structure with parameters received from AVB
  //---------------------------------------------------------------------

  // Clear BCC Root State datastructure
  SetMem (&BccRoot, sizeof (BccRoot), 0);

  // Copy UDS (Unique Device Secret) received from AVB
  memcpy (BccRoot.Uds, BccParamsRecvdFromAVB.Uds, DICE_CDI_SIZE);

  // Copy FRS (Factory Reset Sequence) from DevInfo
  EFI_STATUS Status = GetDICEFRS ();
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Failed to Fetch FRS: %r\n", Status));
    return kDiceResultInvalidInput;
  }

  // Copy code hash received from AVB
  memcpy (BccRoot.ChildImage.CodeHash,
          BccParamsRecvdFromAVB.ChildImage.CodeHash, DICE_HIDDEN_SIZE);

  // Copy authority hash received from AVB
  memcpy (BccRoot.ChildImage.AuthorityHash,
          BccParamsRecvdFromAVB.ChildImage.AuthorityHash, DICE_HIDDEN_SIZE);

  // Update image component name
  BccRoot.ChildImage.BccCfgDesc.component_name =
      BccParamsRecvdFromAVB.ChildImage.ComponentName;

  // Copy image component version received from AVB
  BccRoot.ChildImage.BccCfgDesc.component_version =
      BccParamsRecvdFromAVB.ChildImage.ComponentVersion;

  // Copy device mode received from AVB
  BccRoot.Mode = BccParamsRecvdFromAVB.Mode;

  // Finally select Config Descriptor fields to include in BCC input
  BccRoot.ChildImage.BccCfgDesc.configs =
      DICE_ANDROID_CONFIG_COMPONENT_NAME |
      DICE_ANDROID_CONFIG_SECURITY_VERSION |
      DICE_ANDROID_CONFIG_COMPONENT_VERSION | DICE_ANDROID_CONFIG_RKP_VM_MARKER;

  //---------------------------------------------------------------------
  //                  Derive Private Key Seed from UDS
  //---------------------------------------------------------------------
  Result = DiceDeriveCdiPrivateKeySeed (NULL, BccRoot.Uds, UdsPrivateKeySeed);
  if (Result != kDiceResultOk) {
    DEBUG ((EFI_D_ERROR, "Failed to derive a seed for Uds key pair.\n"));
    return Result;
  }

  //---------------------------------------------------------------------
  //                       Derive UDS Key Pair
  //---------------------------------------------------------------------

  /* UDS public key is kept in root to construct the certificate
   * chain for the child nodes. UDS private key is derived in every
   * DICE operation which uses it.
   */
  Result =
      DiceKeypairFromSeed (NULL, kDicePrincipalAuthority, UdsPrivateKeySeed,
                           BccRoot.UdsPubKey, UdsPrivateKey);
  if (Result != kDiceResultOk) {
    DEBUG ((EFI_D_ERROR, "Failed to derive Uds key pair.\n"));
    return Result;
  }

  //---------------------------------------------------------------------
  //           CBOR Encode BCC Config Descriptor Parameters
  //---------------------------------------------------------------------
  Result = DiceAndroidFormatConfigDescriptor (
      &(BccRoot.ChildImage.BccCfgDesc), sizeof (BccEncodedConfigDesc),
      BccEncodedConfigDesc, &BccEncodedConfigDescValidSize);
  if (Result != kDiceResultOk) {
    DEBUG ((EFI_D_ERROR, "Failed to format config descriptor : %d\n", Result));
    return Result;
  }

  //---------------------------------------------------------------------
  //                  Initialize the DICE input values
  //---------------------------------------------------------------------
  // Initialize code hash
  memcpy (BccInputValues.code_hash, BccRoot.ChildImage.CodeHash,
          sizeof (BccRoot.ChildImage.CodeHash));

  // Initialize authority hash
  memcpy (BccInputValues.authority_hash, BccRoot.ChildImage.AuthorityHash,
          sizeof (BccRoot.ChildImage.AuthorityHash));

  // Initialize Factory reset secret being used
  memcpy (BccInputValues.hidden, BccRoot.Frs, sizeof (BccRoot.Frs));

  BccInputValues.config_type = kDiceConfigTypeDescriptor;
  BccInputValues.config_descriptor = BccEncodedConfigDesc;
  BccInputValues.config_descriptor_size = BccEncodedConfigDescValidSize;
  BccInputValues.mode = BccRoot.Mode;

#ifndef USE_RKP_ALIGNED_UDS_DERIVATION
  //---------------------------------------------------------------------
  // Non-degenerate DICE: derive NextCDIAttest from UDS + BccInputValues
  //---------------------------------------------------------------------
  BCCArtifacts_t BccCDIsOnly = {{0}};
  Result =
      DiceMainFlow (NULL, BccRoot.Uds, BccRoot.Uds, &BccInputValues, 0, NULL,
                    NULL, BccCDIsOnly.NextCDIAttest, BccCDIsOnly.NextCDISeal);
  if (Result != kDiceResultOk) {
    DEBUG ((EFI_D_ERROR, "Failed to derive DICE CDIs : %d\n", Result));
    return Result;
  }

#endif
  //---------------------------------------------------------------------
  // CBOR Encode Dice Artifacts (Without BCC) CDI-Attest/CDI-Sealing
  //---------------------------------------------------------------------
  CborOutInit (NextBccEncodedCDIs, BCC_ARTIFACTS_WO_BCC_TOTAL_SIZE, &Out);

  CborWriteMap (2, &Out);

  CborWriteInt (KCdiAttestLabel, &Out);
#ifdef USE_RKP_ALIGNED_UDS_DERIVATION
  CborWriteBstr (DICE_CDI_SIZE, BccRoot.Uds, &Out);  /* degenerate: UDS as CDI_Attest */
#else
  CborWriteBstr (DICE_CDI_SIZE, BccCDIsOnly.NextCDIAttest, &Out);
#endif

  CborWriteInt (KCdiSealLabel, &Out);
#ifdef USE_RKP_ALIGNED_UDS_DERIVATION
  CborWriteBstr (DICE_CDI_SIZE, BccRoot.Uds, &Out);  /* degenerate: UDS as CDI_Seal */
#else
  CborWriteBstr (DICE_CDI_SIZE, BccCDIsOnly.NextCDISeal, &Out);
#endif

  assert (!CborOutOverflowed (&Out));
  NextBccEncodedCDIsValidSize = CborOutSize (&Out);

  //---------------------------------------------------------------------
  // Generate Dice Artifacts With BCC (CDI-Attest, CDI-Sealing, BCC)
  //---------------------------------------------------------------------
  Result = DiceAndroidHandoverMainFlow (
      NULL /*context=*/, NextBccEncodedCDIs, NextBccEncodedCDIsValidSize,
      &BccInputValues, BccArtifactsBufferSize, FinalEncodedBccArtifacts,
      BccArtifactsValidSize);
  return Result;
}

/* This API is called when HW backed BCC is supported.
 * It returns BCC artifacts in the handover format.
 */
DiceResult
#ifndef USE_DUMMY_BCC
GetHWBccArtifacts (UINT8 *FinalEncodedBccArtifacts,
                   size_t BccArtifactsBufferSize,
                   size_t *BccArtifactsValidSize,
                   BccParams_t BccParamsRecvdFromAVB)
#else
GetHWBccArtifacts (UINT8 *FinalEncodedBccArtifacts,
                   size_t BccArtifactsBufferSize,
                   size_t *BccArtifactsValidSize)
#endif
{

  UINT8 BccEncodedConfigDesc[BCC_CONFIG_DESCRIPTOR_TOTAL_SIZE] = {0};
  UINT8 NextBccEncodedCDIs[BCC_ARTIFACTS_WITH_BCC_TOTAL_SIZE] = {0};
  EFI_STATUS Status = EFI_SUCCESS;
  size_t BccEncodedConfigDescValidSize = 0;
  size_t NextBccEncodedCDIsValidSize = 0;
  DiceInputValues BccInputValues = {{0}};
  DiceResult Result = kDiceResultOk;

#ifdef USE_DUMMY_BCC
  // Fill some hard code values here for now. AVB team has to provide
  // the real values.
  BccParams_t BccParamsRecvdFromAVB = {{0}};
  memcpy ((void *)BccParamsRecvdFromAVB.ChildImage.ComponentName, "pvmfw", 5);
#endif

  assert (FinalEncodedBccArtifacts);
  assert (BccArtifactsValidSize);
  assert (BccArtifactsBufferSize >= BCC_ARTIFACTS_WITH_BCC_TOTAL_SIZE);

  /* Fetches the size of BCC from RKP BCC Service */
  Status = GetRkpBCCSize ();
  switch (Status) {
    case IOpener_ERROR_NOT_FOUND:
    case IOpener_ERROR_PRIVILEGE:
    case IOpener_ERROR_NOT_SUPPORTED:
    case EFI_NOT_FOUND:
      return kDiceResultNotSupported;

    default:
      if (Status != EFI_SUCCESS) {
        DEBUG ((EFI_D_ERROR, "Failed to Fetch RkpBCC: %r\n", Status));
        return kDiceResultInvalidInput;
      }
  }

  // Fetch BCC from QTEE BCC service
  Status = GetRkpBCC (NextBccEncodedCDIs, &NextBccEncodedCDIsValidSize);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Failed to Fetch HW RkpBCC:%r. Generate SW BCC\n",
            Status));
#if 0
    Status =
        GetSWBccArtifacts (FinalEncodedBccArtifacts, BccArtifactsBufferSize,
                           BccArtifactsValidSize, BccParamsRecvdFromAVB);
    if (Status != EFI_SUCCESS) {
      DEBUG ((EFI_D_ERROR, "Failed to Fetch SW BCC: %r\n", Status));
      return kDiceResultInvalidInput;
    }
#endif
    return kDiceResultInvalidInput;
  }

  //---------------------------------------------------------------------
  // Populate BCC Root Data Structure with parameters received from AVB
  //---------------------------------------------------------------------

  // Clear BCC Root State datastructure
  SetMem (&BccRoot, sizeof (BccRoot), 0);

  // Copy Uds (Unique Device Secret) received from AVB
  memcpy (BccRoot.Uds, BccParamsRecvdFromAVB.Uds, DICE_CDI_SIZE);

  // Copy FRS (Factory Reset Sequence) from DevInfo
  Status = GetDICEFRS ();
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Failed to Fetch FRS: %r\n", Status));
    return kDiceResultInvalidInput;
  }

  // Copy code hash received from AVB
  memcpy (BccRoot.ChildImage.CodeHash,
          BccParamsRecvdFromAVB.ChildImage.CodeHash, DICE_HIDDEN_SIZE);

  // Copy authority hash received from AVB
  memcpy (BccRoot.ChildImage.AuthorityHash,
          BccParamsRecvdFromAVB.ChildImage.AuthorityHash, DICE_HIDDEN_SIZE);

  // Update image component name
  BccRoot.ChildImage.BccCfgDesc.component_name =
      BccParamsRecvdFromAVB.ChildImage.ComponentName;

  // Copy image component version received from AVB
  BccRoot.ChildImage.BccCfgDesc.component_version =
      BccParamsRecvdFromAVB.ChildImage.ComponentVersion;

  // Copy device mode received from AVB
  BccRoot.Mode = BccParamsRecvdFromAVB.Mode;

  // Finally select Config Descriptor fields to include in BCC input
  BccRoot.ChildImage.BccCfgDesc.configs =
      DICE_ANDROID_CONFIG_COMPONENT_NAME |
      DICE_ANDROID_CONFIG_SECURITY_VERSION |
      DICE_ANDROID_CONFIG_COMPONENT_VERSION | DICE_ANDROID_CONFIG_RKP_VM_MARKER;

  //---------------------------------------------------------------------
  //           CBOR Encode BCC Config Descriptor Parameters
  //---------------------------------------------------------------------
  Result = DiceAndroidFormatConfigDescriptor (
      &(BccRoot.ChildImage.BccCfgDesc), sizeof (BccEncodedConfigDesc),
      BccEncodedConfigDesc, &BccEncodedConfigDescValidSize);
  if (Result != kDiceResultOk) {
    DEBUG ((EFI_D_ERROR, "Failed to format config descriptor : %d\n", Result));
    return Result;
  }

  //---------------------------------------------------------------------
  //                  Initialize the DICE input values
  //---------------------------------------------------------------------
  // Initialize code hash
  memcpy (BccInputValues.code_hash, BccRoot.ChildImage.CodeHash,
          sizeof (BccRoot.ChildImage.CodeHash));

  // Initialize authority hash
  memcpy (BccInputValues.authority_hash, BccRoot.ChildImage.AuthorityHash,
          sizeof (BccRoot.ChildImage.AuthorityHash));

  // Initialize Factory reset secret being used
  memcpy (BccInputValues.hidden, BccRoot.Frs, sizeof (BccRoot.Frs));

  BccInputValues.config_type = kDiceConfigTypeDescriptor;
  BccInputValues.config_descriptor = BccEncodedConfigDesc;
  BccInputValues.config_descriptor_size = BccEncodedConfigDescValidSize;
  BccInputValues.mode = BccRoot.Mode;

  //---------------------------------------------------------------------
  // Generate Dice Artifacts With BCC (CDI-Attest, CDI-Sealing, BCC)
  //---------------------------------------------------------------------
  Result = DiceAndroidHandoverMainFlow (
      NULL /*context=*/, NextBccEncodedCDIs, NextBccEncodedCDIsValidSize,
      &BccInputValues, BccArtifactsBufferSize, FinalEncodedBccArtifacts,
      BccArtifactsValidSize);
  return Result;
}

/* Function that returns BCC artifacts in the handover format.*/
DiceResult
#ifndef USE_DUMMY_BCC
GetBccArtifacts (UINT8 *FinalEncodedBccArtifacts,
                 size_t BccArtifactsBufferSize,
                 size_t *BccArtifactsValidSize,
                 BccParams_t BccParamsRecvdFromAVB)
#else
GetBccArtifacts (UINT8 *FinalEncodedBccArtifacts,
                 size_t BccArtifactsBufferSize,
                 size_t *BccArtifactsValidSize)
#endif
{
  EFI_STATUS Status = EFI_SUCCESS;

  if (BccArtifactsBufferSize >= BCC_ARTIFACTS_WITH_BCC_TOTAL_SIZE) {
    /* Check to identify whether HW BCC is supported or not*/

    /* Note: This logic is subject to change, as the BCC service
     * currently provides size information only when BCC is available.
     */
#ifndef USE_DUMMY_BCC
    Status =
        GetHWBccArtifacts (FinalEncodedBccArtifacts, BccArtifactsBufferSize,
                           BccArtifactsValidSize, BccParamsRecvdFromAVB);
    if (Status == kDiceResultNotSupported) {
      DEBUG ((EFI_D_ERROR, "HWBCC Failed: Not Supported, Falling Back to SWBCC\n"));
      Status =
          GetSWBccArtifacts (FinalEncodedBccArtifacts, BccArtifactsBufferSize,
                             BccArtifactsValidSize, BccParamsRecvdFromAVB);
    }
#else
    Status = GetHWBccArtifacts (FinalEncodedBccArtifacts,
                                BccArtifactsBufferSize, BccArtifactsValidSize);
    if (Status == kDiceResultNotSupported) {
      DEBUG ((EFI_D_ERROR, "HWBCC Failed: Not Supported, Falling Back to SWBCC\n"));
      Status = GetSWBccArtifacts (FinalEncodedBccArtifacts,
                                  BccArtifactsBufferSize, BccArtifactsValidSize);
    }
#endif
    if (Status != EFI_SUCCESS) {
      DEBUG ((EFI_D_ERROR, "Failed to Generate BCC: %r\n", Status));
    }
  }
  DEBUG ((EFI_D_INFO, "BCC Generation is %r\n", Status));

  return Status;
}
