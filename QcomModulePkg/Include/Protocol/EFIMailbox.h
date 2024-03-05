/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __EFIMAILBOX_H__
#define __EFIMAILBOX_H__

typedef struct _EFI_MAILBOX_PROTOCOL EfiMailboxProtocol;

/*
  External reference to the Mailbox Protocol GUID.
 */
extern EFI_GUID gEfiMailboxProtocolGuid;


typedef enum
{
  MAILBOX_HLOSPMU = 0,
  MAILBOX_HLOSUSS,
  MAILBOX_HLOSTST,
  MAILBOX_CONSOLE,
  MAILBOX_OTA,
  MAILBOX_EBMAX,
} MailboxClientType;

typedef VOID (*OtaChannelCbPtr)(VOID);

typedef EFI_STATUS (EFIAPI *EFI_MAILBOXINIT)(IN);

typedef EFI_STATUS (EFIAPI *EFI_MAILBOXCLIENTREG)(
            IN MailboxClientType MbClientID,
            OUT OtaChannelCbPtr CallbackPtr
);

typedef EFI_STATUS (EFIAPI *EFI_MAILBOXWRITE)(
            IN MailboxClientType MbClientID,
            IN UINT32 NumItem,
            UINT8 *Buffer,
            IN OUT INT32 *Ret
);

typedef EFI_STATUS (EFIAPI *EFI_MAILBOXREAD)(
            IN MailboxClientType MbClientID,
            IN UINT32 NumItem,
            UINT8 *Buffer,
            IN OUT INT32 *Ret
);

typedef EFI_STATUS (EFIAPI *EFI_MAILBOX_GET_FREEITEMNUM)(
            IN MailboxClientType MbClientID,
            IN OUT INT32 *Ret
);

typedef EFI_STATUS (EFIAPI *EFI_MAILBOX_GET_VALIDITEMNUM)(
            IN MailboxClientType MbClientID,
            IN OUT INT32 *Ret
);

typedef EFI_STATUS (EFIAPI *EFI_MAILBOX_GET_STATUS)(
            IN EfiMailboxProtocol *This
);

/*===========================================================================
  PROTOCOL INTERFACE
===========================================================================*/

typedef struct _EFI_MAILBOX_PROTOCOL {
  UINT64                        Revision;
  EFI_MAILBOXINIT               MailBoxInit;
  EFI_MAILBOXCLIENTREG          MailboxClientReg;
  EFI_MAILBOXWRITE              MailboxWrite;
  EFI_MAILBOXREAD               MailboxRead;
  EFI_MAILBOX_GET_FREEITEMNUM   MailboxGetFreeItemNum;
  EFI_MAILBOX_GET_VALIDITEMNUM  MailboxGetValidItemNum;
  EFI_MAILBOX_GET_STATUS        MailboxGetStatus;
} EfiMailboxProtocol;

#endif /* __EFIMAILBOX_H__ */
