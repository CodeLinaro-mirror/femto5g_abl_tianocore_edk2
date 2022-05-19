/* Copyright (c) 2021, The Linux Foundation. All rights reserved.
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
 * Changes from Qualcomm Innovation Center are provided under the following license:
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#if HIBERNATION_SUPPORT_INSECURE

#include <Library/DeviceInfo.h>
#include <Library/DrawUI.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PartitionTableUpdate.h>
#include <Library/ShutdownServices.h>
#include <Library/StackCanary.h>
#include "Hibernation.h"

#define BUG(fmt, ...) {\
		printf("Fatal error " fmt, ##__VA_ARGS__);\
		while(1);\
	}

/* Reserved some free memory for UEFI use */
#define RESERVE_FREE_SIZE	1024*1024*10

struct free_ranges {
	UINT64 start;
	UINT64 end;
};

/* Holds free memory ranges read from UEFI memory map */
static struct free_ranges free_range_buf[100];
static int free_range_count;

/* number of data pages to be copied from swap */
static unsigned int nr_copy_pages;
/* number of meta pages or pages which hold pfn indexes */
static unsigned int nr_meta_pages;

static unsigned long bounced_pages;

UINT64 relocation_base_addr;
static struct swsusp_header *swsusp_header;
static struct arch_hibernate_hdr *resume_hdr;
static struct swsusp_info *swsusp_info;

struct pfn_block {
	unsigned long base_pfn;
	int available_pfns;
};

struct kernel_pfn_iterator {
	unsigned long *pfn_array;
	int cur_index;
	int max_index;
	struct pfn_block cur_block;
};
static struct kernel_pfn_iterator kernel_pfn_iterator;

struct bounce_pfn_entry {
	UINT64 dst_pfn;
	UINT64 src_pfn;
};

/*
 * Size of the buffer where disk IO is performed.If any kernel pages are
 * destined to be here, they will be bounced.
 */
#define	DISK_BUFFER_SIZE	64*1024*1024
#define	DISK_BUFFER_PAGES	(DISK_BUFFER_SIZE / PAGE_SIZE)

#define	OUT_OF_MEMORY	-1

#define	BOUNCE_TABLE_ENTRY_SIZE	sizeof(struct bounce_pfn_entry)
#define	ENTRIES_PER_TABLE	(PAGE_SIZE / BOUNCE_TABLE_ENTRY_SIZE) - 1

/*
 * Bounce tables are discontinuous pages linked by the last element
 * of the page.
 *
 *       ---------          	      ---------
 * 0   | dst | src |	----->	0   | dst | src |
 * 1   | dst | src |	|	1   | dst | src |
 * 2   | dst | src |	|	2   | dst | src |
 * .   |	   |	|	.   |           |
 * .   |	   |	|	.   |           |
 * 256 | addr|     |-----	256 |addr |     |------>
 *       --------- 		      ---------
 */
struct bounce_table {
	struct bounce_pfn_entry bounce_entry[ENTRIES_PER_TABLE];
	unsigned long next_bounce_table;
	unsigned long padding;
};

/*
 * Base bounce table of bounced entries that will be passed to
 * relocation code.
 */
struct bounce_table *base_bounce_table;

struct bounce_table_iterator {
	struct bounce_table *cur_table;
	/* next available free table entry */
	int cur_index;
};
struct bounce_table_iterator bti;

/*
 * target_addr  : address where page allocation is needed
 *
 * return	: 1 if address falls in free range
 * 		  0 if address is not in free range
 */
static int CheckFreeRanges (UINT64 target_addr)
{
	int i = 0;
	while (i < free_range_count) {
		if (target_addr >= free_range_buf[i].start &&
			target_addr < free_range_buf[i].end)
		return 1;
		i++;
	}
	return 0;
}

static int memcmp(const void *s1, const void *s2, int n)
{
	const unsigned char *us1 = s1;
	const unsigned char *us2 = s2;

	if (n == 0 )
		return 0;

	while(n--) {
		if(*us1++ ^ *us2++)
			return 1;
	}
	return 0;
}

static void copyPage(unsigned long src_pfn, unsigned long dst_pfn)
{
	unsigned long *src = (unsigned long*)(src_pfn << PAGE_SHIFT);
	unsigned long *dst = (unsigned long*)(dst_pfn << PAGE_SHIFT);

	gBS->CopyMem (dst, src, PAGE_SIZE);
}

static void init_kernel_pfn_iterator(unsigned long *array)
{
	struct kernel_pfn_iterator *iter = &kernel_pfn_iterator;
	iter->pfn_array = array;
	iter->max_index = nr_copy_pages;
}

static int find_next_available_block(struct kernel_pfn_iterator *iter)
{
	int available_pfns;

	do {
		unsigned long cur_pfn, next_pfn;
		iter->cur_index++;
		if (iter->cur_index >= iter->max_index)
			BUG("index maxed out. Line %d\n", __LINE__);
		cur_pfn = iter->pfn_array[iter->cur_index];
		next_pfn = iter->pfn_array[iter->cur_index + 1];
		available_pfns = next_pfn - cur_pfn - 1;
	} while (!available_pfns);

	iter->cur_block.base_pfn = iter->pfn_array[iter->cur_index];
	iter->cur_block.available_pfns = available_pfns;
	return 0;
}

static unsigned long get_unused_kernel_pfn(void)
{
	struct kernel_pfn_iterator *iter = &kernel_pfn_iterator;

	if (!iter->cur_block.available_pfns)
		find_next_available_block(iter);

	iter->cur_block.available_pfns--;
	return iter->cur_block.base_pfn++;
}

/*
 * get a pfn which is unused by kernel and UEFI.
 *
 * unused pfns are pnfs which doesn't overlap with image kernel pages
 * or UEFI pages. These pfns are used for bounce pages, bounce tables
 * and relocation code.
 */
static unsigned long get_unused_pfn()
{
	unsigned long pfn;

	do {
		pfn = get_unused_kernel_pfn();
	} while(!CheckFreeRanges(pfn << PAGE_SHIFT));

	return pfn;
}

/*
 * Preallocation is done for performance reason. We want to map memory
 * as big as possible. So that UEFI can create bigger page table mappings.
 * We have seen mapping single page is taking time in terms of few ms.
 * But we cannot preallocate every free page, becasue that causes allocation
 * failures for UEFI. Hence allocate most of the free pages but some(10MB)
 * are kept unallocated for UEFI to use. If kernel has any destined pages in
 * this region, that will be bounced.
 */
static void preallocate_free_ranges(void)
{
	int i = 0, ret;
	int reservation_done = 0;
	UINT64 alloc_addr, range_size;
	UINT64 num_pages;

	for (i = free_range_count - 1; i >= 0 ; i--) {
		range_size = free_range_buf[i].end - free_range_buf[i].start;
		if (!reservation_done && range_size > RESERVE_FREE_SIZE) {
			/*
			 * We have more buffer. Remove reserved buf and allocate
			 * rest in the range.
			 */
			reservation_done = 1;
			alloc_addr = free_range_buf[i].start + RESERVE_FREE_SIZE;
			range_size -=  RESERVE_FREE_SIZE;
			num_pages = range_size/PAGE_SIZE;
			printf("Reserved range = 0x%lx - 0x%lx\n", free_range_buf[i].start,
								alloc_addr - 1);
			/* Modify the free range start */
			free_range_buf[i].start = alloc_addr;
		} else {
			alloc_addr = free_range_buf[i].start;
			num_pages = range_size/PAGE_SIZE;
		}

		ret = gBS->AllocatePages(AllocateAddress, EfiBootServicesData,
				num_pages, &alloc_addr);
		if(ret)
			printf("Fatal error alloc LINE %d alloc_addr = 0x%lx\n",
							__LINE__, alloc_addr);
	}
}

static int get_uefi_memory_map(void)
{
	EFI_MEMORY_DESCRIPTOR	*MemMap;
	EFI_MEMORY_DESCRIPTOR	*MemMapPtr;
	UINTN			MemMapSize;
	UINTN			MapKey, DescriptorSize;
	UINTN			Index;
	UINT32			DescriptorVersion;
	EFI_STATUS		Status;
	int index = 0;

	MemMapSize = 0;
	MemMap     = NULL;

	Status = gBS->GetMemoryMap (&MemMapSize, MemMap, &MapKey,
				&DescriptorSize, &DescriptorVersion);
	if (Status != EFI_BUFFER_TOO_SMALL) {
		DEBUG ((EFI_D_ERROR, "ERROR: Undefined response get memory map\n"));
		return -1;
	}
	if (CHECK_ADD64 (MemMapSize, EFI_PAGE_SIZE)) {
		DEBUG ((EFI_D_ERROR, "ERROR: integer Overflow while adding additional"
					"memory to MemMapSize"));
		return -1;
	}
	MemMapSize = MemMapSize + EFI_PAGE_SIZE;
	MemMap = AllocateZeroPool (MemMapSize);
	if (!MemMap) {
		DEBUG ((EFI_D_ERROR,
			"ERROR: Failed to allocate memory for memory map\n"));
		return -1;
	}
	MemMapPtr = MemMap;
	Status = gBS->GetMemoryMap (&MemMapSize, MemMap, &MapKey,
				&DescriptorSize, &DescriptorVersion);
	if (EFI_ERROR (Status)) {
		DEBUG ((EFI_D_ERROR, "ERROR: Failed to query memory map\n"));
		FreePool (MemMapPtr);
		return -1;
	}
	for (Index = 0; Index < MemMapSize / DescriptorSize; Index ++) {
		if (MemMap->Type == EfiConventionalMemory) {
			free_range_buf[index].start = MemMap->PhysicalStart;
			free_range_buf[index].end =  MemMap->PhysicalStart + MemMap->NumberOfPages * PAGE_SIZE;
			DEBUG ((EFI_D_ERROR, "Free Range 0x%lx --- 0x%lx\n",free_range_buf[index].start,
					free_range_buf[index].end));
			index++;
		}
		MemMap = (EFI_MEMORY_DESCRIPTOR *)((UINTN)MemMap + DescriptorSize);
	}
	free_range_count = index;
	FreePool (MemMapPtr);
	return 0;
}

static int read_image(unsigned long offset, VOID *Buff, int nr_pages) {

	int Status;
	EFI_BLOCK_IO_PROTOCOL *BlockIo = NULL;
	EFI_HANDLE *Handle = NULL;

	Status = PartitionGetInfo (L"system_b", &BlockIo, &Handle);
	if (Status != EFI_SUCCESS)
		return Status;

	if (!Handle) {
		DEBUG ((EFI_D_ERROR, "EFI handle for system_b is corrupted\n"));
		return -1;
	}

	if (CHECK_ADD64 (BlockIo->Media->LastBlock, 1)) {
		DEBUG ((EFI_D_ERROR, "Integer overflow while adding LastBlock and 1\n"));
		return -1;
	}

	if ((MAX_UINT64 / (BlockIo->Media->LastBlock + 1)) <
			(UINT64)BlockIo->Media->BlockSize) {
		DEBUG ((EFI_D_ERROR,
			"Integer overflow while multiplying LastBlock and BlockSize\n"));
		return -1;
	}

	/* Check what is the block size of the mmc and scale the offset accrodingly
	 * right now blocksize = page_size = 4096 */

	Status = BlockIo->ReadBlocks (BlockIo,
			BlockIo->Media->MediaId,
			offset,
			EFI_PAGE_SIZE * nr_pages,
			(VOID*)Buff);
	if (Status != EFI_SUCCESS) {
		printf("Read image failed Line = %d\n", __LINE__);
		return Status;
	}

	return 0;
}

static void update_bounce_entry(UINT64 dst_pfn, UINT64 src_pfn)
{
	if (bti.cur_index == ENTRIES_PER_TABLE) {
		/* Allocate and chain next bounce table */
		bti.cur_table->next_bounce_table = get_unused_pfn() << PAGE_SHIFT;
		bti.cur_table = (struct bounce_table *) bti.cur_table->next_bounce_table;
		bti.cur_index = 0;
	}

	bti.cur_table->bounce_entry[bti.cur_index].dst_pfn = dst_pfn;
	bti.cur_table->bounce_entry[bti.cur_index++].src_pfn = src_pfn;
	bounced_pages++;
}

/*
 * Copy page to destination if page is free and is not in reserved area.
 * Bounce page otherwise.
 */
static void copy_page_to_dst(unsigned long src_pfn, unsigned long dst_pfn)
{
	UINT64 target_addr = dst_pfn << PAGE_SHIFT;

	if (CheckFreeRanges(target_addr)) {
		copyPage(src_pfn, dst_pfn);
	} else {
		unsigned long bounce_pfn = get_unused_pfn();
		copyPage(src_pfn, bounce_pfn);
		update_bounce_entry(dst_pfn, bounce_pfn);
	}
}

static void print_kernel_details(struct swsusp_info *info)
{
	/*TODO: implement printing of kernel details here*/
	return;
}

static int check_swap_map_page(unsigned long offset)
{
	return !((offset -1) % PFNS_PER_PAGE);
}

static int swsusp_read(void)
{
	struct swsusp_info *info;
	int ret;
	unsigned long start_ms, temp, disk_read_ms = 0;
	unsigned long copy_page_ms = 0;
	unsigned long offset;
	unsigned long src_pfn, dst_pfn;
	unsigned int pending_pages, nr_read_pages;
	unsigned long *kernel_pfns;
	unsigned long pfn_index = 0;
	unsigned long MBs, MBPS, DDR_MBPS;
	unsigned long read_size;
	unsigned long read_meta_pages, rem_meta_pages;
	void *disk_read_buffer;

	start_ms = GetTimerCountms();

	disk_read_buffer =  AllocatePages (DISK_BUFFER_PAGES);
	if (!disk_read_buffer) {
		printf("Disk buffer alloc failed\n");
		return -1;
	} else {
		printf("Disk buffer alloction at 0x%p - 0x%p\n", disk_read_buffer,
				disk_read_buffer + DISK_BUFFER_SIZE - 1);
	}

	info = AllocatePages(1);
	if (!info) {
		printf("Failed to allocate memory for swsusp_info %d\n",__LINE__);
		FreePages(disk_read_buffer, DISK_BUFFER_PAGES);
		return -1;
	}

	/* read swsusp_info struct at offset 2 */
	ret = read_image(SWAP_INFO_OFFSET, info, 1);
	if (ret) {
		printf("Failed to read swsusp_info %d\n", __LINE__);
		return -1;
	}

	resume_hdr = (struct arch_hibernate_hdr *)info;
	swsusp_info = info;
	nr_meta_pages = info->pages - info->image_pages - 1;
	nr_copy_pages = info->image_pages;
	printf("Total pages to copy = %lu Total meta pages = %lu\n", nr_copy_pages, nr_meta_pages);
	offset = SWAP_INFO_OFFSET + 1 ;

	/* Allocate memory for kernel pfn indexes */
	kernel_pfns = AllocatePages(nr_meta_pages);
	if (!kernel_pfns) {
		printf("Failed to allocate memory for storing pfn meta data %d\n",
			__LINE__);
		return -1;
	}

	/* First swap_map page can have max (PFNS_PER_PAGE - 2) pfn indexes */
	read_size = nr_meta_pages < (PFNS_PER_PAGE - 2) ? nr_meta_pages : PFNS_PER_PAGE - 2;
	ret = read_image(offset, kernel_pfns, read_size);
	if (ret) {
		printf("Failed to read meta pages from disk %d\n", __LINE__);
		return -1;
	}
	offset += read_size;
	read_meta_pages = read_size;

	if (nr_meta_pages >= PFNS_PER_PAGE - 2) {
		/* skip swap_map page */
		offset++;
		while (nr_meta_pages != read_meta_pages) {
			rem_meta_pages = nr_meta_pages - read_meta_pages;
			read_size = rem_meta_pages > PFNS_PER_PAGE - 1 ? PFNS_PER_PAGE - 1 : rem_meta_pages;
			ret = read_image(offset, kernel_pfns + read_meta_pages * PFNS_PER_PAGE, read_size);
			if (ret) {
				printf("Failed to read meta pages from disk %d\n", __LINE__);
				return -1;
			}
			read_meta_pages += read_size;
			offset += read_size;

			/* skip swap_map page */
			if (read_size == PFNS_PER_PAGE - 1)
				offset++;
		}
	}

	if (read_meta_pages != nr_meta_pages) {
		printf("Mismatch in reading nr_meta_pages\n");
		return -1;
	}

	print_kernel_details(info);
	get_uefi_memory_map();
	/*
	 * No dynamic allocation beyond this point. If not honored it will
	 * result in corruption of pages.
	 */
	preallocate_free_ranges();

	populate_unused_pfn_array(kernel_pfns);
	reset_upa_index();

	base_bounce_table = (struct bounce_table *)(get_unused_pfn() << PAGE_SHIFT);
	bti.cur_table = base_bounce_table;

	pending_pages = nr_copy_pages;
	printf("Reading pages:     ");
	while (pending_pages > 0) {
		/* read pages in chunks to improve disk read performance */
		nr_read_pages = pending_pages > DISK_BUFFER_PAGES ? DISK_BUFFER_PAGES : pending_pages;
		temp = GetTimerCountms();
		ret = read_image(offset, disk_read_buffer, nr_read_pages);
		disk_read_ms += (GetTimerCountms() - temp);
		if (ret < 0) {
			printf("Failed to read data pages from disc %d\n", __LINE__);
			break;
		}
		src_pfn = (unsigned long) disk_read_buffer >> PAGE_SHIFT;
		while (nr_read_pages > 0) {
			/* Skip swap_map pages */
			if (!check_swap_map_page(offset)) {
				dst_pfn = kernel_pfns[pfn_index++];
				pending_pages--;
				temp = GetTimerCountms();
				copy_page_to_dst(src_pfn, dst_pfn);
				copy_page_ms += (GetTimerCountms() - temp);
			}
			src_pfn++;
			nr_read_pages--;
			offset++;
		}
	}
	printf("Done. \n");
	if (ret < 0) {
		printf("error swsusp_read\n");
		return -1;
	}

	MBs = (nr_copy_pages*PAGE_SIZE)/(1024*1024);
	MBPS = (MBs*1000)/disk_read_ms;
	DDR_MBPS = (MBs*1000)/copy_page_ms;
	printf("Image size = %lu MBs\n", MBs);
	printf("Time loading image (excluding bounce buffers) = %lu msecs\n", (GetTimerCountms() - start_ms));
	printf("Time spend - disk IO = %lu msecs (BW = %llu MBps)\n", disk_read_ms, MBPS);
	printf("Time spend - DDR copy = %llu msecs (BW = %llu MBps)\n", copy_page_ms, DDR_MBPS);

	return 0;
}

static int restore_snapshot_image(void)
{
	int ret;
	void *disk_read_buffer;
	unsigned long start_ms, offset;
	unsigned long *kernel_pfn_indexes;
	struct bounce_table_iterator *bti = &table_iterator;

	start_ms = GetTimerCountms();
	ret = read_swap_info_struct();
	if (ret < 0)
		return ret;

	kernel_pfn_indexes = read_kernel_image_pfn_indexes(&offset);
	if (!kernel_pfn_indexes)
		return -1;
	init_kernel_pfn_iterator(kernel_pfn_indexes);

	disk_read_buffer =  AllocatePages(DISK_BUFFER_PAGES);
	if (!disk_read_buffer) {
		printf("Memory alloc failed Line %d\n", __LINE__);
		return -1;
	} else {
		printf("Disk buffer alloction at 0x%p - 0x%p\n", disk_read_buffer,
				disk_read_buffer + DISK_BUFFER_SIZE - 1);
	}

	/*
	 * No dynamic allocation beyond this point. If not honored it will
	 * result in corruption of pages.
	 */
	get_uefi_memory_map();
	preallocate_free_ranges();

	bti->first_table = (struct bounce_table *)(get_unused_pfn() << PAGE_SHIFT);
	bti->cur_table = bti->first_table;

	ret = read_data_pages(kernel_pfn_indexes, offset, disk_read_buffer);
	if (ret < 0) {
		printf("error in restore_snapshot_image\n");
		goto err;
	}

	printf("Time loading image (excluding bounce buffers) = %lu msecs\n", (GetTimerCountms() - start_ms));
	printf("Image restore Completed...\n");
	printf("Total bounced Pages = %d (%lu MBs)\n", bounced_pages, (bounced_pages*PAGE_SIZE)/(1024*1024));
	return 0;
}

static void copy_bounce_and_boot_kernel(UINT64 relocateAddress)
{
	int Status;
	unsigned long cpu_resume = (unsigned long )resume_hdr->phys_reenter_kernel;

	/* TODO:
	 * We are not relocating the jump routine for now so avoid this copy
	 * gBS->CopyMem ((VOID*)relocateAddress, (VOID*)&JumpToKernel, PAGE_SIZE);
	 */

	/*
	 * The restore routine "JumpToKernel" copies the bounced pages after iterating
	 * through the bounce entry table and passes control to hibernated kernel after
	 * calling PreparePlatformHarware
	 */

	printf("Disable UEFI Boot services\n");
	printf("Kernel entry point = 0x%lx\n", cpu_resume);

	/*Shut down UEFI boot services*/
	Status = ShutdownUefiBootServices ();
	if (EFI_ERROR (Status)) {
		DEBUG ((EFI_D_ERROR,
			"ERROR: Can not shutdown UEFI boot services. Status=0x%X\n",
			Status));
		return;
	}

	asm __volatile__ (
		"mov x18, %[table_base]\n"
		"mov x19, %[count]\n"
		"mov x20, %[bounce_base]\n"
		"mov x21, %[resume]\n"
		"mov x22, %[disable_cache]\n"
		"b JumpToKernel"
		:
		:[table_base] "r" (base_bounce_table),
		[count] "r" (bounced_pages),
		[resume] "r" (cpu_resume),
		[disable_cache] "r" (PreparePlatformHardware)
		:"x18", "x19", "x20", "x21", "x22", "memory");
}

void BootIntoHibernationImage(BootInfo *Info)
{
	int ret;

	printf("===============================\n");
	printf("Entrying Hibernation restore\n");


	reset_upa_index();

	swsusp_header = AllocatePages(1);
	if(!swsusp_header) {
		printf("AllocatePages failed\n");
		goto free_upa;
	}

	ret = read_image(0, swsusp_header, 1);
	if (ret) {
		printf("Failed to read image at offset 0\n");
		goto read_image_error;
	}

	if(!memcmp(HIBERNATE_SIG, swsusp_header->sig, 10)) {
		printf("Signature found. Proceeding with disk read...\n");
	} else {
		printf("Signature not found. Aborting hibernation\n");
		goto read_image_error;
	}

	ret = swsusp_read();
	if (ret) {
		printf("Failed swsusp_read \n");
		goto read_image_error;
	printf("Signature found. Proceeding with disk read...\n");
	return 0;

read_image_error:
	FreePages(swsusp_header, 1);
	return -1;
}
#endif
