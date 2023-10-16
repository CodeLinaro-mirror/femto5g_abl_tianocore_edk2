/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <Library/SailLib.h>
#include <Library/SailHeaders.h>
#include <Library/SailCrc.h>

#include <Protocol/EFIMailbox.h>

#define SAIL_BUFFER_ADDRESS 0x90E00000
#define SAIL_IMAGE_DATA_OFFSET 0x800
#define SAIL_IMAGE_SIZE_OFFSET 0xA40
#define SAIL_PART_NAME_LEN 8

EfiMailboxProtocol *MboxProt;

UINT64 FlashStartTime, FlashEndTime;
BOOLEAN FlashStatus = FALSE;
BOOLEAN BootStatus = FALSE;
UINT32 SailStatus = -1;

STATIC CONST CHAR8 *SailPartitions[] = {
    "SAIL_SW1", "SAIL_SW2", "SAIL_SW3", "SAIL_SW4", "SAIL_HYP",
};

BOOLEAN
CheckSailPartition (CONST CHAR8 *Arg)
{
  UINTN Part = 0;
  UINTN SailPartCnt = sizeof (SailPartitions) / sizeof (SailPartitions[0]);
  CHAR8 Argument[SAIL_UPD_IMG_NAME_LEN];
  UINTN ArgLength = AsciiStrLen (Arg);
  UINTN Iter = 0;

  while ( *Arg != '\0') {
          Argument[Iter] = AsciiCharToUpper (*Arg);
          Iter++;
          Arg++;
  }
  Argument[Iter] = '\0';

  if (AsciiStrStr (Argument, "_A") ||
      AsciiStrStr (Argument, "_B")) {
          ArgLength = ArgLength - 2;
  }

  ArgLength = (ArgLength > SAIL_PART_NAME_LEN) ? ArgLength : SAIL_PART_NAME_LEN;

  for (Part = 0; Part < SailPartCnt; Part++) {
    CONST CHAR8 *Partition =  SailPartitions[Part];
    if (!(AsciiStrnCmp (Argument, Partition, ArgLength))) {
          return TRUE;
    }
  }
  return FALSE;
}

STATIC EFI_STATUS
GetSailBaseAddr (UINT64 *SailBufferAddr)
{
  EFI_STATUS Status = EFI_FAILURE;
  UINTN DataSize = sizeof (*SailBufferAddr);

  Status = gRT->GetVariable ((CHAR16 *)L"SailBaseAddr",
                             &gQcomTokenSpaceGuid,
                             NULL,
                             &DataSize,
                             SailBufferAddr);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_VERBOSE, "Failed to get SAIL Buffer Address, %r\n", Status));
  } else {
    *SailBufferAddr = SAIL_BUFFER_ADDRESS;
    DEBUG ((EFI_D_VERBOSE,
              "Using SAIL Buffer Address:%llx\n", *SailBufferAddr ));
  }
  return EFI_SUCCESS;
}

STATIC VOID
SailFlashCb (VOID)
{
  INT32 NumOfItems = 0;
  EFI_STATUS Status = EFI_SUCCESS;
  INT32 Ret;

  Status = MboxProt->MailboxGetValidItemNum (MAILBOX_OTA, &NumOfItems);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Failed to get any valid items from Mailbox\n"));
    return;
  }

  if (NumOfItems) {
     while (NumOfItems != 0) {
       NumOfItems--;
       sailUpdaterMsgHeaderType *CbBuf =
                        AllocateZeroPool (sizeof (sailUpdaterMsgHeaderType));
       if (!CbBuf) {
           return;
       }

       Status = MboxProt->MailboxRead (MAILBOX_OTA, 1, (UINT8 *)CbBuf, &Ret);
       if (Status != EFI_SUCCESS) {
         DEBUG ((EFI_D_ERROR, "Error Mailboxread ret:%d\n", Ret));
       }
        SailStatus = CbBuf->Status;
        DEBUG ((EFI_D_ERROR, "SAIL Status:%d\n", SailStatus));
      }
       FlashEndTime = GetTimerCountms ();
       FlashStatus = TRUE;
     } else {
          DEBUG ((EFI_D_ERROR, "No valid item found\n"));
       }
}

STATIC EFI_STATUS
SendToSailMailBox (IN CONST CHAR8 *Partition,
                   IN VOID *BufferAddr,
                   IN UINT32 Size,
                   IN CONST UINT32 PartitionId)
{
  INT32         Ret = 0;
  EFI_STATUS    Status = EFI_FAILURE;
  UINT64        StartTime, EndTime;
  UINT32        NumItems = 1;
  UINT32        BufAdr = 0;
  EFI_EVENT     TimeoutEvent;
  UINTN         Timeout = 0;
  UINTN         EventIndex =0;

  Status = gBS->CreateEvent (EVT_TIMER, TPL_CALLBACK,
                                    NULL, NULL, &TimeoutEvent);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Timeout Event Creation Failed:%r\n", Status));
    return Status;
  }

  FlashStatus = FALSE;

  sailUpdaterMsgHeaderType __attribute__ ((aligned (32)))  SourceBuf = {0};
  SourceBuf.HeaderSize = sizeof (sailUpdaterMsgHeaderType);
  SourceBuf.MsgId = SAIL_UPD_MSG_ID_FLASH_IMAGE;
  SourceBuf.Direction = 0x0;
  SourceBuf.Status = 0x0;
  SourceBuf.HeaderCrc = 0x0;

  AsciiStrnCpyS ((CHAR8 *) SourceBuf.flashImg.imgName,
                      SAIL_UPD_IMG_NAME_LEN, Partition, SAIL_PART_NAME_LEN);

  SourceBuf.flashImg.FlashPartition = PartitionId;
  SourceBuf.flashImg.FlashGptId = 0x0;

  BufAdr = (UINT32_MAX & (UINT64)BufferAddr);
  SourceBuf.flashImg.BufAddr = BufAdr;
  SourceBuf.flashImg.BufLen = Size;

  StartTime = GetTimerCountms ();
  // Buffer CRC
  Status = XCrc32Generate (1, (UINT8 *) BufferAddr, Size,
                                    &SourceBuf.flashImg.BufCrc);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Buffer CRC Calculation Failed:%r\n", Status));
    return Status;
  }

  EndTime = GetTimerCountms ();
  DEBUG ((EFI_D_ERROR,
          "Buffer CRC Calculatin took %lums\n", EndTime - StartTime));

  StartTime = GetTimerCountms ();
  // Header CRC
  Status = XCrc32Generate (1, (UINT8 *)&SourceBuf,
         sizeof (sailUpdaterMsgHeaderType), (UINT32 *)&SourceBuf.HeaderCrc);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Header CRC Calculation Failed:%r\n", Status));
    return Status;
  }
  EndTime = GetTimerCountms ();
  DEBUG ((EFI_D_ERROR,
           "Header CRC Calculatin took %lums\n", EndTime - StartTime));

  DEBUG ((EFI_D_VERBOSE, "\nMailbox write...\n"));
  DEBUG ((EFI_D_VERBOSE, "Header Crc:%llx \n", SourceBuf.HeaderCrc));
  DEBUG ((EFI_D_VERBOSE, "Header Size:%llx \n", SourceBuf.HeaderSize));
  DEBUG ((EFI_D_VERBOSE, "MsgId:%d \n", SourceBuf.MsgId));
  DEBUG ((EFI_D_VERBOSE, "Direction:%d \n", SourceBuf.Direction));
  DEBUG ((EFI_D_VERBOSE, "Status:%d\n", SourceBuf.Status));
  DEBUG ((EFI_D_VERBOSE, "ImgName: %a\n", SourceBuf.bootImg.imgName));
  DEBUG ((EFI_D_VERBOSE,
             "BootPartition: %d\n", SourceBuf.bootImg.BootPartition));
  DEBUG ((EFI_D_VERBOSE, "BootGptId: %d\n", SourceBuf.bootImg.BootGptId));
  DEBUG ((EFI_D_VERBOSE, "BufAddr: 0x%llx\n", SourceBuf.bootImg.BufAddr));
  DEBUG ((EFI_D_VERBOSE, "BufLen: 0x%llx\n", SourceBuf.bootImg.BufLen));
  DEBUG ((EFI_D_VERBOSE, "BufCrc: 0x%llx\n\n", SourceBuf.bootImg.BufCrc));

  FlashStartTime = GetTimerCountms ();
  Status = MboxProt->MailboxWrite (MAILBOX_OTA,
                         NumItems, (UINT8 *)&SourceBuf, &Ret);
  if (Status != EFI_SUCCESS) {
     DEBUG ((EFI_D_ERROR, "Error Mailboxwrite ret:%d\n", Ret));
     return Status ;
  }
  EndTime = GetTimerCountms ();
  DEBUG ((EFI_D_ERROR, "MailBox Write took %lums\n", EndTime - StartTime));

  Status = gBS->SetTimer (TimeoutEvent, TimerPeriodic, 30 * 1000 * 1000);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Set Timer Failed:%r\n", Status));
    return Status;
  }

  while (TRUE) {
   if (FlashStatus == TRUE) {
     DEBUG ((EFI_D_ERROR, "MailBox Write to Partition:%a for PartitionId:%d"
                 " (Status:%d) took %lums\n\n", SourceBuf.flashImg.imgName,
                  PartitionId, SailStatus,  FlashEndTime - FlashStartTime));
      break;
   }

   Status = gBS->WaitForEvent (1, &TimeoutEvent, &EventIndex);
   if (Timeout++ == 6) {
     SailStatus = EFI_FAILURE;
     DEBUG ((EFI_D_ERROR, "SAIL Flashing Timed OUT\n"));
     break;
    }
  }
  if (!SailStatus) {
    Status = EFI_SUCCESS;
    }
  else {
    Status = EFI_FAILURE;
   }
  return Status;
}

/* Handle Sail Query Command */
EFI_STATUS
SailFlash (IN CONST CHAR8 *Arg, IN VOID *Data, IN UINT32 Size)
{
  EFI_STATUS Status = EFI_FAILURE;
  UINT64 StartTime = 0, EndTime = 0;
  UINT64 BufAddr = 0x0;

  if (!Size ||
      !Arg ||
      !Data ) {
         DEBUG ((EFI_D_ERROR, "Invalid Input\n"));
         return Status;
  }

  Status = GetSailBaseAddr (&BufAddr);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Failed to get the SAIL Buffer Address\n"));
    return Status;
  }

  UINT32 *BufferAddr = (UINT32 *)BufAddr;

  Status = gBS->LocateProtocol (&gEfiMailboxProtocolGuid,
                               NULL, (VOID **)&MboxProt);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Error locating Mailbox protocol\n"));
    return Status;
  }

  StartTime = GetTimerCountms ();
  Status = MboxProt->MailBoxInit ();
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Failed to Enable Mailbox\n"));
  }
  EndTime = GetTimerCountms ();
  DEBUG ((EFI_D_ERROR, "MailBox init took %lums\n", EndTime - StartTime));

  StartTime = GetTimerCountms ();
  Status = MboxProt->MailboxClientReg (MAILBOX_OTA, &SailFlashCb);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Error in Mailbox Client Registration\n"));
    return Status;
  }

  EndTime = GetTimerCountms ();
  DEBUG ((EFI_D_ERROR,
         "MailBox Client Registration took %lums\n", EndTime - StartTime));

  // Copy fastboot buffer to the Sail buffer
  gBS->CopyMem ((VOID *)BufferAddr, Data, Size);

  UINT8 Iter = 0;
  CHAR8 *Argument = AllocateZeroPool (SAIL_UPD_IMG_NAME_LEN);
  for (Iter = 0;  Iter < SAIL_UPD_IMG_NAME_LEN ||
                                Arg[Iter] != '\0'; Iter++) {
        Argument[Iter] = AsciiCharToUpper (Arg[Iter]);
  }

  CHAR8 *Delimt = "_";
  CHAR8 *Input = NULL;
  Input = AsciiStrStr (Arg, Delimt);
  if (Input) {
        Input++;
        Input = AsciiStrStr (Input, Delimt);
  }

  if ( !Input) {
   Status =  SendToSailMailBox (Argument, (VOID *) BufferAddr, Size, 0x0);
   if (Status != EFI_SUCCESS) {
        return Status;
   }
        Status =   SendToSailMailBox (Argument, (VOID *) BufferAddr, Size, 0x1);
  } else if (!AsciiStrCmp (Input, "_a")) {
     Status =  SendToSailMailBox (Argument, (VOID *) BufferAddr, Size, 0x0);
   } else if (!AsciiStrCmp (Input, "_b")) {
    Status =  SendToSailMailBox (Argument, (VOID *) BufferAddr, Size, 0x1);
   } else {
        DEBUG ((EFI_D_ERROR, "Invalid Input:%a\n", Input));
   }
  return Status;
}


STATIC VOID
SailBootCb (VOID)
{
  INT32 NumOfItems = 0;
  EFI_STATUS Status = EFI_FAILURE;
  INT32 Ret;

  Status = MboxProt->MailboxGetValidItemNum (MAILBOX_OTA, &NumOfItems);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Error Mailbox_get_validitemnum\n"));
    return;
  }

  if (NumOfItems) {
    while (NumOfItems != 0) {
      NumOfItems--;
      sailUpdaterMsgHeaderType *CbBuf =
                 AllocateZeroPool (sizeof (sailUpdaterMsgHeaderType));
      if (!CbBuf) {
        return;
      }

      Status = MboxProt->MailboxRead (MAILBOX_OTA, 1, (UINT8 *)CbBuf, &Ret);
      if (Status != EFI_SUCCESS) {
        DEBUG ((EFI_D_ERROR, "Error Mailboxread ret:%d\n", Ret));
      }
      SailStatus = CbBuf->Status;
      DEBUG ((EFI_D_ERROR, "SAIL Status:%d\n", SailStatus));
    }
    BootStatus = TRUE;
  } else {
    DEBUG ((EFI_D_ERROR, "No valid item found\n"));
  }
}

EFI_STATUS
SailBoot (IN VOID *Data, IN UINT32 Size, BOOLEAN Fastboot)
{
  EFI_STATUS Status = EFI_FAILURE;

  UINT32        NumItems = 1;
  INT32         Ret = 0;
  UINT64        BufAddr = 0x0;
  EFI_EVENT     TimeoutEvent;
  UINTN         Timeout = 0;
  UINTN         EventIndex =0;


  if (!Size ||
      !Data ) {
    DEBUG ((EFI_D_ERROR, "Invalid Input\n"));
    return Status;
  }

  if (Fastboot) {
    Data  += SAIL_IMAGE_DATA_OFFSET;
    Size  -= SAIL_IMAGE_SIZE_OFFSET;
  }

  Status = gBS->CreateEvent (EVT_TIMER, TPL_CALLBACK,
                                       NULL, NULL, &TimeoutEvent);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Timeout Event Creation Failed:%r\n", Status));
    return Status;
  }

  BootStatus = FALSE;


  Status = GetSailBaseAddr (&BufAddr);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Failed to get the SAIL Buffer Address\n"));
    return Status;
  }

  UINT32 *BufferAddr = (UINT32 *)BufAddr;

  Status = gBS->LocateProtocol (&gEfiMailboxProtocolGuid,
                                               NULL, (VOID **)&MboxProt);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Error locating Mailbox protocol\n"));
    return Status;
  }

  Status = MboxProt->MailBoxInit ();
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Failed to Enable Mailbox\n"));
    return Status;
  }

  Status = MboxProt->MailboxClientReg (MAILBOX_OTA, &SailBootCb);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Error in Mailbox client registration\n"));
    return Status;
  }

  sailUpdaterMsgHeaderType __attribute__ ((aligned (32))) SourceBuf = {0};
  SourceBuf.HeaderSize = sizeof (sailUpdaterMsgHeaderType);
  SourceBuf.MsgId = SAIL_UPD_MSG_ID_BOOT_IMAGE;
  SourceBuf.Direction = 0x0;
  SourceBuf.Status = 0x0;

  AsciiStrnCpyS ((CHAR8 *) SourceBuf.bootImg.imgName, SAIL_UPD_IMG_NAME_LEN,
                       SAIL_UPD_IMG_NAME_SW1, sizeof (SAIL_UPD_IMG_NAME_SW1));

  SourceBuf.bootImg.BootPartition = 0x0;
  SourceBuf.bootImg.BootGptId = 0x0;

  SourceBuf.bootImg.BufAddr = BufAddr;
  SourceBuf.bootImg.BufLen = Size;

  // Copy fastboot buffer to the Sail buffer. Fastboot host tool converts image
  // to boot image format. Hence consider the offset for the actual sail image.

  gBS->CopyMem ((VOID *)BufferAddr, Data, Size);

  // Buffer CRC
  Status = XCrc32Generate (1, (UINT8 *) BufferAddr, Size,
                                       &SourceBuf.bootImg.BufCrc);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Buffer CRC calculation failed:%r\n", Status));
    return Status;
  }

  // Header CRC
  Status = XCrc32Generate (1, (UINT8 *)&SourceBuf,
           sizeof (sailUpdaterMsgHeaderType), (UINT32 *)&SourceBuf.HeaderCrc);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Header CRC calculation failed:%r\n", Status));
    return Status;
  }

  DEBUG ((EFI_D_VERBOSE, "\nMailbox write...\n"));
  DEBUG ((EFI_D_VERBOSE, "Header Crc:%llx \n", SourceBuf.HeaderCrc));
  DEBUG ((EFI_D_VERBOSE, "Header Size:%llx \n", SourceBuf.HeaderSize));
  DEBUG ((EFI_D_VERBOSE, "MsgId:%d \n", SourceBuf.MsgId));
  DEBUG ((EFI_D_VERBOSE, "Direction:%d \n", SourceBuf.Direction));
  DEBUG ((EFI_D_VERBOSE, "Status:%d\n", SourceBuf.Status));
  DEBUG ((EFI_D_VERBOSE, "ImgName: %a\n", SourceBuf.bootImg.imgName));
  DEBUG ((EFI_D_VERBOSE,
             "BootPartition: %d\n", SourceBuf.bootImg.BootPartition));
  DEBUG ((EFI_D_VERBOSE, "BootGptId: %d\n", SourceBuf.bootImg.BootGptId));
  DEBUG ((EFI_D_VERBOSE, "BufAddr: 0x%llx\n", SourceBuf.bootImg.BufAddr));
  DEBUG ((EFI_D_VERBOSE, "BufLen: 0x%llx\n", SourceBuf.bootImg.BufLen));
  DEBUG ((EFI_D_VERBOSE, "BufCrc: 0x%llx\n\n", SourceBuf.bootImg.BufCrc));


  Status = MboxProt->MailboxWrite (MAILBOX_OTA,
                                       NumItems, (UINT8 *)&SourceBuf, &Ret);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Error Mailbox Write(%d)\n", Ret));
    return Status;
  }

  Status = gBS->SetTimer (TimeoutEvent, TimerPeriodic, 30 * 1000 * 1000);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Failed to set timer:%r\n", Status));
    return Status;
  }
  while (TRUE) {
   if (BootStatus == TRUE) {
      break;
   }

   Status = gBS->WaitForEvent (1, &TimeoutEvent, &EventIndex);
   if (Timeout++ == 6) {
     SailStatus = EFI_FAILURE;
     DEBUG ((EFI_D_ERROR, "SAIL Booting Timed Out.\n"));
     break;
    }
  }

  if (!SailStatus) {
    Status = EFI_SUCCESS;
  }
  else {
    Status = EFI_FAILURE;
  }

  return Status;

}
