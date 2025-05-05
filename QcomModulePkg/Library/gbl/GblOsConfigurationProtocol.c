/*
 * Copyright (C) 2025 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/LinuxLoaderLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UpdateCmdLine.h>
#include <Library/LocateDeviceTree.h>
#include <Library/Board.h>

#include <Protocol/EFIGblOsConfigurationProtocol.h>

// Copied from UpdateCmdLine.c
STATIC VOID
GetDisplayCmdline (CHAR8 *Dst, UINTN *Size)
{
  EFI_STATUS Status;

  Status = gRT->GetVariable ((CHAR16 *)L"DisplayPanelConfiguration",
                             &gQcomTokenSpaceGuid, NULL, Size, Dst);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Unable to get Panel Config, %r\n", Status));
  }
}

// Minimal implementation of FixupKernelCmdline to boot with GBL
// Fixup provided:
// 1. Display configuration (not required to haver success boot)
EFI_STATUS
EFIAPI
EfiGblFixupKernelCommandlineMinimalBoot (
    IN EFI_GBL_OS_CONFIGURATION_PROTOCOL *This,
    IN CONST CHAR8 *CommandLine,
    OUT CHAR8 *Fixup,
    IN OUT UINTN *FixupBufferSize)
{
  DEBUG ((EFI_D_INFO, "EFI_GBL_OS_CONFIGURATION_PROTOCOL: "
                      "EfiGblFixupKernelCommandlineMinimalBoot called\n"));

  UINTN FixupBufferSizeInput = *FixupBufferSize;
  CHAR8 DisplayCmdLine[256];
  UINTN DisplayCmdLineLen = sizeof (DisplayCmdLine);
  GetDisplayCmdline (DisplayCmdLine, &DisplayCmdLineLen);

  UINTN RequiredLen = AsciiStrLen (DisplayCmdLine) + 1;
  if (FixupBufferSizeInput < RequiredLen) {
    *FixupBufferSize = RequiredLen;
    return EFI_BUFFER_TOO_SMALL;
  }

  return AsciiStrCpyS (Fixup, RequiredLen, DisplayCmdLine);
}

// Minimal implementation of FixupBootconfig to boot with GBL (subset of
// UpdateCmdLineParams). Fixups provided:
// 1. Provide bootdevice/boot_device to successfully boot.
// 2. Provide serialno to have working ADB.
EFI_STATUS
EFIAPI
EfiGblFixupBootconfigMinimalBoot (IN EFI_GBL_OS_CONFIGURATION_PROTOCOL *This,
                                  IN CONST CHAR8 *BootConfig,
                                  IN UINTN BootConfigSize,
                                  OUT CHAR8 *Fixup,
                                  IN OUT UINTN *FixupBufferSize)
{
  DEBUG ((EFI_D_INFO, "EFI_GBL_OS_CONFIGURATION_PROTOCOL: "
                      "EfiGblFixupBootconfigMinimalBoot called\n"));

  UINTN FixupBufferSizeInput = *FixupBufferSize;
  UINT32 BootConfigLen = 0;
  LIST_ENTRY *BootConfigListHead =
      (LIST_ENTRY *)AllocateZeroPool (sizeof (LIST_ENTRY));
  if (BootConfigListHead == NULL) {
    DEBUG ((EFI_D_ERROR, "BootConfigListHead: Out of resources\n"));
    return EFI_OUT_OF_RESOURCES;
  }
  InitializeListHead (BootConfigListHead);

  CHAR8 StrSerialNum[SERIAL_NUM_SIZE];
  EFI_STATUS Status = BoardSerialNum (StrSerialNum, sizeof (StrSerialNum));
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Error Finding board serial num: %x\n", Status));
    return Status;
  }
  DEBUG ((EFI_D_INFO,
          "EFI_GBL_OS_CONFIGURATION_PROTOCOL: Get BoardSerialNum %a\n",
          StrSerialNum));

  CHAR8 BootDev[32];
  UINTN BootDevLen = sizeof (BootDev);
  GetBootDevice (BootDev, BootDevLen);
  DEBUG ((EFI_D_INFO, "EFI_GBL_OS_CONFIGURATION_PROTOCOL: GetBootDevice %a\n",
          BootDev));

  CONST CHAR8 *UsbSerialCmdLine = " androidboot.serialno=";
  CONST CHAR8 *BootDeviceCmdLine = " androidboot.bootdevice=";
  CONST CHAR8 *DynamicBootDeviceCmdLine = " androidboot.boot_devices=soc/";
  AddtoBootConfigList (TRUE, BootDeviceCmdLine, BootDev, BootConfigListHead,
                       AsciiStrLen (BootDeviceCmdLine), AsciiStrLen (BootDev));
  AddtoBootConfigList (
      TRUE, DynamicBootDeviceCmdLine, BootDev, BootConfigListHead,
      AsciiStrLen (DynamicBootDeviceCmdLine), AsciiStrLen (BootDev));
  AddtoBootConfigList (TRUE, UsbSerialCmdLine, StrSerialNum, BootConfigListHead,
                       AsciiStrLen (UsbSerialCmdLine),
                       AsciiStrLen (StrSerialNum));

  // +1 for =, +1 for \n
  BootConfigLen += AsciiStrLen (BootDeviceCmdLine) + AsciiStrLen (BootDev) + 2;
  BootConfigLen +=
      AsciiStrLen (DynamicBootDeviceCmdLine) + AsciiStrLen (BootDev) + 2;
  BootConfigLen +=
      AsciiStrLen (UsbSerialCmdLine) + AsciiStrLen (StrSerialNum) + 2;

  CHAR8 *FinalBootConfig = NULL;
  UINT32 FinalBootConfigLen = 0;
  EFI_STATUS BootConfigResult = UpdateBootConfigParams (
      BootConfigListHead, BootConfigLen, &FinalBootConfig, &FinalBootConfigLen);
  if (BootConfigResult != EFI_SUCCESS) {
    DEBUG ((EFI_D_INFO, "EFI_GBL_OS_CONFIGURATION_PROTOCOL: Failed to "
                        "UpdateBootConfigParams\n"));
    return BootConfigResult;
  }
  ClearBootConfigList (BootConfigListHead);

  DEBUG ((EFI_D_INFO,
          "EFI_GBL_OS_CONFIGURATION_PROTOCOL: FinalBootConfigLen %u\n",
          FinalBootConfigLen));
  DEBUG ((EFI_D_INFO, "EFI_GBL_OS_CONFIGURATION_PROTOCOL: FinalBootConfig %a\n",
          FinalBootConfig));

  // Ignore termination byte since it isn't the part of bootconfig spec
  *FixupBufferSize = FinalBootConfigLen - 1;
  if (FixupBufferSizeInput < FinalBootConfigLen) {
    return EFI_BUFFER_TOO_SMALL;
  }

  return AsciiStrCpyS (Fixup, FinalBootConfigLen, FinalBootConfig);
}

// No-op implementation for BuildDeviceTree
EFI_STATUS
EFIAPI
EfiGblSelectDeviceTreesMinimalBoot (
    IN EFI_GBL_OS_CONFIGURATION_PROTOCOL *This,
    IN OUT EFI_GBL_VERIFIED_DEVICE_TREE *DeviceTrees,
    IN UINTN NumDeviceTrees)
{
  BoardInit ();
  DEBUG ((EFI_D_INFO,
          "EFI_GBL_OS_CONFIGURATION_PROTOCOL: "
          "EfiGblSelectDeviceTreesMinimalBoot called. Amount to select: %u\n",
          NumDeviceTrees));

  UINTN SelectedBaseDeviceTreeIndex = 0;
  UINTN SelectedOverlayIndex = 0;

  UINTN CurrentBaseDeviceTreeIndex = 0;
  UINTN CurrentOverlayIndex = 0;
  UINTN DtbMatchVal = 0;
  UINTN DtboMatchVal = 0;

  for (UINTN i = 0; i < NumDeviceTrees; i++) {
    BOOLEAN IsBaseDeviceTree = DeviceTrees[i].Metadata.Type == DEVICE_TREE;
    BOOLEAN IsOverlay = DeviceTrees[i].Metadata.Type == OVERLAY;

    DtInfo CurDtbInfo = {0};
    DtInfo BestDtbInfo = {0};
    CurDtbInfo.Dtb = (void*) DeviceTrees[i].DeviceTree;

    if (IsBaseDeviceTree) {
        if (ReadDtbFindMatch (&CurDtbInfo, &BestDtbInfo, SOC_MATCH)) {
            CurrentBaseDeviceTreeIndex = i;
        }

        if (CurDtbInfo.DtMatchVal) {
            if (CurDtbInfo.DtMatchVal & BIT (SOC_MATCH)) {
                if ((CurDtbInfo.DtMatchVal & ALL_BITS_SET) == (ALL_BITS_SET)) {
                    DEBUG ((EFI_D_INFO, "Exact DTB match"
                            " found. DTBO search is not "
                            "required\n"));
                DeviceTrees[i].Selected = TRUE;
                SelectedBaseDeviceTreeIndex = CurrentBaseDeviceTreeIndex;
                DEBUG ((EFI_D_INFO,
                        "EFI_GBL_OS_CONFIGURATION_PROTOCOL: "
                        "Base device tree at index %u"
                        "(%u in the provided array) got "
                        "selected\n",
                        SelectedBaseDeviceTreeIndex, i));
                }
            }
            if (CurDtbInfo.DtMatchVal > DtbMatchVal) {
                DtbMatchVal = CurDtbInfo.DtMatchVal;
                SelectedBaseDeviceTreeIndex = CurrentBaseDeviceTreeIndex;
            }
        }
        CurrentBaseDeviceTreeIndex++;
    }
    if (IsOverlay) {
        if (ReadDtbFindMatch (&CurDtbInfo, &BestDtbInfo, VARIANT_MATCH)) {
            CurrentOverlayIndex = i;
        }
        if (CurDtbInfo.DtMatchVal >  DtboMatchVal) {
            DtboMatchVal = CurDtbInfo.DtMatchVal;
            SelectedOverlayIndex = CurrentOverlayIndex;
        }
        CurrentOverlayIndex++;
    }
  }

  DEBUG ((EFI_D_INFO,
          "EFI_GBL_OS_CONFIGURATION_PROTOCOL: "
          "Overlays at index %u got selected\n",
          SelectedOverlayIndex));
  DeviceTrees[SelectedOverlayIndex].Selected = TRUE;

  DEBUG ((EFI_D_INFO,
          "EFI_GBL_OS_CONFIGURATION_PROTOCOL: "
          "Base device tree at index %u got selected\n",
          SelectedBaseDeviceTreeIndex));
  DeviceTrees[SelectedBaseDeviceTreeIndex].Selected = TRUE;

  DEBUG (
      (EFI_D_INFO,
       "EFI_GBL_OS_CONFIGURATION_PROTOCOL: Total base device trees count: %u\n",
       CurrentBaseDeviceTreeIndex));

  DEBUG ((EFI_D_INFO,
          "EFI_GBL_OS_CONFIGURATION_PROTOCOL: Total overlays count: %u\n",
          CurrentOverlayIndex));

  return EFI_SUCCESS;
}

/**
 * Implementation of EFI_GBL_OS_CONFIGURATION_PROTOCOL to get a minimal boot
 * with GBL.
 */
EFI_GBL_OS_CONFIGURATION_PROTOCOL gMinimalBootGblOsConfigurationProtocol = {
    EFI_GBL_OS_CONFIGURATION_PROTOCOL_REVISION,
    EfiGblFixupKernelCommandlineMinimalBoot, EfiGblFixupBootconfigMinimalBoot,
    EfiGblSelectDeviceTreesMinimalBoot};

VOID
InstallGblOsConfigurationProtocol ()
{
  EFI_STATUS Status = gBS->InstallProtocolInterface (
      &gImageHandle, &gEfiGblOsConfigurationProtocolGuid, EFI_NATIVE_INTERFACE,
      &gMinimalBootGblOsConfigurationProtocol);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "Failed to install GBL OS Configuration Protocol: %r\n",
            Status));
  } else {
    DEBUG ((DEBUG_INFO,
            "GBL OS Configuration Protocol installed successfully.\n"));
  }
}
