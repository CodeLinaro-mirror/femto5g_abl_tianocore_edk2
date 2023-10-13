/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __SAIL_HEADERS_H__
#define __SAIL_HEADERS_H__

#define SAIL_UPD_IMG_NAME_HYP             "SAIL_HYP"
#define SAIL_UPD_IMG_NAME_SW1             "SAIL_SW1"
#define SAIL_UPD_IMG_NAME_SW2             "SAIL_SW2"
#define SAIL_UPD_IMG_NAME_SW3             "SAIL_SW3"
#define SAIL_UPD_IMG_NAME_SW4             "SAIL_SW4"
#define SAIL_UPD_IMG_NAME_LEN             32

/* Hello MSG for handshake after bootup */
#define SAIL_UPD_MSG_ID_HELLO             0

/* Query GPT header and GPT partition integrity */
#define SAIL_UPD_MSG_ID_CHECK_GPT         1

/* Read GPT header and GPT partition entry */
#define SAIL_UPD_MSG_ID_READ_GPT          2

/* Write new GPT header and GPT partition to fix integrity error */
#define SAIL_UPD_MSG_ID_WRITE_GPT         3

/* Fix GPT using the other good GPT table as reference */
#define SAIL_UPD_MSG_ID_FIX_GPT           4

/* Update GPT header and GPT partition entry for reboot to take effect */
#define SAIL_UPD_MSG_ID_UPDATE_GPT        5

/* Query Image boot Information of images specified */
#define SAIL_UPD_MSG_ID_QUERY_IMAGES      6

/* Read image from SPI NOR flash memory to DDR memory */
#define SAIL_UPD_MSG_ID_READ_IMAGE        7

/* Flash image from DDR memory to SPI NOR flash memory at given GPT header's
    partition */
#define SAIL_UPD_MSG_ID_FLASH_IMAGE       8

/* Boot image to SAIL RAM (Directly load the ELF from DDR to SAIL RAM */
#define SAIL_UPD_MSG_ID_BOOT_IMAGE        9

/* Boot image to SAIL RAM (Directly load the ELF from DDR to SAIL RAM */
#define SAIL_UPD_MSG_ID_BOOT_CONTINUE     10

/* Command consolidates Check GPT and Query Image in single command */
#define SAIL_UPD_MSG_ID_GET_BOOTINFO      11

/* Gets the OTA metadata information stored on the Flash memory */
#define SAIL_UPD_MSG_ID_GET_OTA_METADATA  12

/* Writes the OTA metadata information to the Flash memory */
#define SAIL_UPD_MSG_ID_SET_OTA_METADATA  13

#define SAIL_UPD_STATUS_SUCCESS           0
#define SAIL_UPD_STATUS_ERROR             1

#define SAIL_UPD_MD2SAIL                  0
#define SAIL_UPD_SAIL2MD                  1

#define SAIL_UPD_GPT_ID_PRIMARY           0
#define SAIL_UPD_GPT_ID_SECONDARY         1
#define SAIL_UPD_GPT_ID_MAX               2

#define SAIL_UPD_PARTITION_ID_PRIMARY     0
#define SAIL_UPD_PARTITION_ID_SECONDARY   1
#define SAIL_UPD_PARTITION_ID_MAX         2

typedef struct {
  /* Must be "HELLO" with NULL termination. It is case sensitive */
  UINT8  hello[SAIL_UPD_IMG_NAME_LEN];
  /* Must be 0x1 */
  UINT32 MailboxVersionMajor;
  /* Must be 0x0 */
  UINT32 MailboxVersionMinor;
} __attribute__ ((packed)) sailUpdaterHelloMsgType;

typedef struct {
  /* 0 - error, 1 - pass */
  UINT32 PrimaryGptHeaderCrcStatus;
  /* 0 - error, 1 - pass */
  UINT32 PrimaryGptEntryCrcStatus;
  /* in bytes            */
  UINT32 PrimaryGptSize;
  /* 0 - error, 1 - pass */
  UINT32 SecondaryGptHeaderCrcStatus;
  /* 0 - error, 1 - pass */
  UINT32 SecondaryGptEntryCrcStatus;
  /* in bytes            */
  UINT32 SecondaryGptSize;
} __attribute__ ((packed)) sailUpdaterCheckGptMsgType;

typedef struct {
  /* GPT ID: 0 - primary, 1 - secondary */
  UINT32 Id;
  /* DDR buffer address */
  UINT32 BufAddr;
  /* DDR buffer length. Length has to be cacheline size Aligned */
  UINT32 BufLen;
  /* DDR buffer CRC using IEEE-802.3 CRC32 Ethernet Standard */
  UINT32 BufCrc;
} __attribute__ ((packed)) sailUpdaterReadGptMsgType;

typedef struct {
  /* GPT ID: 0 - primary, 1 - secondary */
  UINT32 Id;
  /* DDR buffer address */
  UINT32 BufAddr;
  /* DDR buffer length. Length has to be cacheline size Aligned */
  UINT32 BufLen;
  /* DDR buffer CRC using IEEE-802.3 CRC32 Ethernet Standard */
  UINT32 BufCrc;
} __attribute__ ((packed)) sailUpdaterWriteGptMsgType;

typedef struct {
  /* GPT ID: 0 - primary, 1 - secondary */
  UINT32 Id;
  /* DDR buffer address which is used as work buffer in fix GPT command */
  UINT32 BufAddr;
  /* DDR buffer length. Length has to be cacheline size Aligned.
   * The size must be equal or larger than 2 times the GPT table size
   */
  UINT32 BufLen;

} __attribute__ ((packed)) sailUpdaterFixGptMsgType;

typedef struct {
  /* including NULL termination. It is case sensitive */
  UINT8  imgName[SAIL_UPD_IMG_NAME_LEN];
  /* Image partition swapping type: 0 - A and B offset swapping,
   * other value are reserved
   */
  UINT32 PartitionSwapType;
} __attribute__ ((packed)) sailUpdaterMsgUpdateGptEntryType;

typedef struct {
  /* GPT ID: 0 - primary, 1 - secondary */
  UINT32 Id;
  /* Number of partitions to be swapped between A and B */
  UINT32 Num;
  /* DDR buffer address. Cast to sailUpdaterMsgUpdateGptEntryType
   * to get the array of entries
   */
  UINT32 BufAddr;
  /* DDR buffer length. Length has to be cacheline size Aligned Buffer length
   * should atleast (num*sizeof(sailUpdaterMsgUpdateGptEntryType) + 24K used for
   * Work buffer)
   */
  UINT32 BufLen;
  /* DDR buffer CRC using IEEE-802.3 CRC32 Ethernet Standard.
   * bufCrc is ran by (num*sizeof(sailUpdaterMsgUpdateGptEntryType)).
   * Padding zero is needed.
   */
  UINT32 BufCrc;

} __attribute__ ((packed)) sailUpdaterUpdateGptMsgType;

typedef struct {
  /* including NULL termination */
  UINT8  imgName[SAIL_UPD_IMG_NAME_LEN];
  /* Boot partition entry ID (A or B) : 0 - A partition, 1 - B partition.
   * If the partition is not A, OTA update should not be attempted.
   */
  UINT32 BootPartition;
  /* Boot GPT ID: 0 - Primary, 1 - secondary.
   * If bootGptId is not 0 (primary), OTA update should not be attempted.
   */
  UINT32 BootGptId;
  /* the maximum size of partition A in bootGptId table */
  UINT32 PartitionSizeA;
  /* the maximum size of partition B in bootGptId table */
  UINT32 PartitionSizeB;
  /* 0 - error,
   * 1 - Primary GPT table's two entries match secondary GPT table's two entries
   */
  UINT32 IsTwoGptTableEntriesMatching;
} __attribute__ ((packed)) sailUpdaterImageEntryType;

typedef struct {
  UINT32 Num_Images;
  /* DDR buffer address. Cast to sailUpdaterImageEntryType
   * to get the array of entries. Address has to be cacheline Aligned
   */
  UINT32 BufAddr;
  /* DDR buffer length. Length has to be cacheline Aligned*/
  UINT32 BufLen;
  /* DDR buffer CRC using IEEE-802.3 CRC32 Ethernet Standard.
   * bufCrc is ran by bufLen. Padding zero is needed.
   */
  UINT32 BufCrc;
} __attribute__ ((packed)) sailUpdaterQueryImageMsgType;

typedef struct {
  /* including NULL termination */
  UINT8  imgName[SAIL_UPD_IMG_NAME_LEN];
  /* Read partition entry ID (A or B) : 0 - A partition, 1 - B partition */
  UINT32 ReadPartition;
  /* Read GPT ID: 0 - Primary, 1 - secondary */
  UINT32 ReadGptId;
  /* DDR image address */
  UINT32 BufAddr;
  /* DDR image length. Length has to be cacheline size Aligned */
  UINT32 BufLen;
  /* DDR image CRC using IEEE-802.3 CRC32 Ethernet Standard */
  UINT32 BufCrc;
} __attribute__ ((packed)) sailUpdaterReadImgMsgType;

typedef struct {
  /* including NULL termination */
  UINT8  imgName[SAIL_UPD_IMG_NAME_LEN];
  /* Flash partition entry ID (A or B) : 0 - A partition, 1 - B partition */
  UINT32 FlashPartition;
  /* Flash GPT ID: 0 - Primary, 1 - secondary */
  UINT32 FlashGptId;
  /* DDR image address */
  UINT32 BufAddr;
  /* DDR image length. Length has to be cacheline size Aligned */
  UINT32 BufLen;
  /* DDR image CRC using IEEE-802.3 CRC32 Ethernet Standard */
  UINT32 BufCrc;
} __attribute__ ((packed)) sailUpdaterFlashImgMsgType;

typedef struct {
  /* 0 - error, 1 - pass */
  UINT32 PrimaryGptHeaderCrcStatus;
  /* 0 - error, 1 - pass */
  UINT32 PrimaryGptEntryCrcStatus;
  /* in bytes */
  UINT32 PrimaryGptSize;
  /* 0 - error, 1 - pass */
  UINT32 SecondaryGptHeaderCrcStatus;
  /* 0 - error, 1 - pass */
  UINT32 SecondaryGptEntryCrcStatus;
  /* in bytes */
  UINT32 SecondaryGptSize;
  /* Boot Information of the images */
  sailUpdaterQueryImageMsgType ImgInfo;
} __attribute__ ((packed)) sailUpdaterGetBootInfoMsgType;

typedef struct {
  /* DDR image address */
  UINT32 BufAddr;
  /* DDR image length. Length has to be cacheline size Aligned */
  UINT32 BufLen;
  /* DDR image CRC using IEEE-802.3 CRC32 Ethernet Standard */
  UINT32 BufCrc;
} __attribute__ ((packed)) sailUpdaterGetOTAMetaDataMsgType;

typedef struct {
  /* DDR image address */
  UINT32 BufAddr;
  /* DDR image length. Length has to be cacheline size Aligned */
  UINT32 BufLen;
  /* DDR image CRC using IEEE-802.3 CRC32 Ethernet Standard */
  UINT32 BufCrc;
} __attribute__ ((packed)) sailUpdaterSetOTAMetaDataMsgType;

typedef struct {
  /* including NULL termination */
  UINT8  imgName[SAIL_UPD_IMG_NAME_LEN];
  /* Boot partition entry ID (A or B) : 0 - A partition, 1 - B partition */
  UINT32 BootPartition;
  /* Boot GPT ID: 0 - Primary, 1 - secondary */
  UINT32 BootGptId;
  /* DDR image address */
  UINT32 BufAddr;
  /* DDR image length. Length has to be cacheline size Aligned*/
  UINT32 BufLen;
  /* DDR image CRC using IEEE-802.3 CRC32 Ethernet Standard */
  UINT32 BufCrc;
} __attribute__ ((packed)) sailUpdaterBootImgMsgType;

typedef struct {
  /* message CRC using IEEE-802.3 CRC32 Ethernet Standard */
  UINT32 HeaderCrc;
  /* message header size in bytes including msgCRC */
  UINT32 HeaderSize;
  /* SAIL_UPDATE_MB_MSG_ID_<X_Y> */
  UINT32 MsgId;
  /* 0 - TO_SAIL, 1 - FROM_SAIL */
  UINT32 Direction;
  /* response status code such as SAIL_UPDATE_MB_STATUS_SUCCESS */
  UINT32 Status;
  union {
    sailUpdaterHelloMsgType            hello;
    sailUpdaterCheckGptMsgType         checkGpt;
    /*  Only for debug purpose. Disabled in mission mode */
    sailUpdaterReadGptMsgType          readGpt;
    /*  Only for debug purpose. Disabled in mission mode */
    sailUpdaterWriteGptMsgType         writeGpt;
    sailUpdaterFixGptMsgType           fixGpt;
    sailUpdaterUpdateGptMsgType        updateGpt;
    sailUpdaterQueryImageMsgType       queryImg;
    sailUpdaterGetBootInfoMsgType      bootInfo;
    sailUpdaterReadImgMsgType          readImg;
    sailUpdaterFlashImgMsgType         flashImg;
    sailUpdaterGetOTAMetaDataMsgType   getMetaData;
    sailUpdaterSetOTAMetaDataMsgType   setMetaData;
    sailUpdaterBootImgMsgType          bootImg;
  };
} __attribute__ ((packed)) sailUpdaterMsgHeaderType;

#endif
