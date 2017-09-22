// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2017-2018, ARM Limited
 */
#include <string.h>
#include <trace.h>
#include <tee_api.h>


#include "sedget_video_ta.h"
#include "mve_fw_mmu.h"

typedef uint32_t phys_addr_t;

static mve_mmu_entry_t mve_mmu_make_l1l2_entry(enum mve_mmu_attrib attrib,
					       phys_addr_t paddr,
					       enum mve_mmu_access access)
{
	return ((((mve_mmu_entry_t)attrib) << MVE_MMU_ATTRIBUTE_SHIFT) & MVE_MMU_ATTRIBUTE_MASK) |
		((mve_mmu_entry_t)((paddr >> (MVE_MMU_PAGE_SHIFT - MVE_MMU_PADDR_SHIFT)) & MVE_MMU_PADDR_MASK)) |
		((((mve_mmu_entry_t)access) << MVE_MMU_ACCESS_SHIFT) & MVE_MMU_ACCESS_MASK);
}

static uint8_t* write_pages(uint8_t* l2page, phys_addr_t paddr, uint32_t num_pages, bool bss) {
	uint32_t i;
	enum mve_mmu_access access = ACCESS_EXECUTABLE;
	if(true == bss) {
		access = ACCESS_READ_WRITE;
	}

	for(i=0; i<num_pages; i++) {
		mve_mmu_entry_t entry = mve_mmu_make_l1l2_entry(ATTRIB_PRIVATE, paddr + (i*MVE_MMU_PAGE_SIZE), access);
		*((uint32_t *)(void*)l2page) = entry;
		l2page += MVE_MMU_PAGE_TABLE_ENTRY_SIZE;
	}
	return l2page;
}

void fill_l2pages(uint8_t* fw_addr, uint8_t* fw_phys_addr, size_t fw_size, uint8_t *l2pages,
		uint32_t ncores, struct mve_fw_secure_descriptor *fw_secure_desc)
{
	uint32_t i, j;
	struct fw_header *header;
	uint32_t num_pages, num_text_pages, num_shared_pages, num_bss_pages;
	phys_addr_t data_start, shared_pages, bss_page;
	uint8_t *l2page;

	header = (struct fw_header *)(void*)fw_addr;
	fw_secure_desc->fw_version.major = header->protocol_major;
	fw_secure_desc->fw_version.minor = header->protocol_minor;

	num_pages = (fw_size + MVE_MMU_PAGE_SIZE - 1) >> MVE_MMU_PAGE_SHIFT;

	num_text_pages = (header->text_length + MVE_MMU_PAGE_SIZE - 1) / MVE_MMU_PAGE_SIZE;

	i = header->bss_start_address >> MVE_MMU_PAGE_SHIFT;
	num_shared_pages = 0;
	num_bss_pages = 0;

	/* dry run to get number of shared bss and non shared bss pages */
	for (j = 0; j < header->bss_bitmap_size; j++)
	{
		uint32_t word_idx = j >> 5;
		uint32_t bit_idx = j & 0x1f;
		uint32_t addr = i << MVE_MMU_PAGE_SHIFT;

		if (addr >= header->master_rw_start_address &&
				addr < header->master_rw_start_address + header->master_rw_size)
		{
			/* Shared pages can be shared between all cores running the same session. */
			num_shared_pages++;
		}
		else if ((header->bss_bitmap[word_idx] & (1 << bit_idx)) != 0)
		{
			num_bss_pages++;
		}
		i++;
	}

	data_start = (uintptr_t)fw_phys_addr;
	shared_pages = data_start + (num_pages << MVE_MMU_PAGE_SHIFT);
	bss_page = shared_pages + (num_shared_pages << MVE_MMU_PAGE_SHIFT);

	for(i=0; i<ncores;i++) {
		uint32_t bss_start_address = header->bss_start_address >> MVE_MMU_PAGE_SHIFT;
		phys_addr_t shared_page = shared_pages;
		l2page = l2pages + (i*MVE_MMU_PAGE_SIZE) + MVE_MMU_PAGE_TABLE_ENTRY_SIZE; /*Leave first mmu table entry blank*/
		l2page = write_pages(l2page, data_start, num_text_pages, false);
		for (j = 0; j < header->bss_bitmap_size; j++)
		{
			uint32_t word_idx = j >> 5;
			uint32_t bit_idx = j & 0x1f;
			uint32_t addr = bss_start_address << MVE_MMU_PAGE_SHIFT;

			/* Mark this page as either a BSS page or a shared page */
			if (addr >= header->master_rw_start_address &&
					addr < header->master_rw_start_address + header->master_rw_size)
			{
				/* Shared pages can be shared between all cores running the same session. */
				write_pages(l2page, shared_page, 1, true);
				shared_page += (1 << MVE_MMU_PAGE_SHIFT);
			}
			else if ((header->bss_bitmap[word_idx] & (1 << bit_idx)) != 0)
			{
				/* Non-shared BSS pages. These pages need to be allocated for each core
				 * and session. */
				write_pages(l2page, bss_page, 1, true);
				bss_page += (1 << MVE_MMU_PAGE_SHIFT);
			}
			else
			{
				DMSG("skip");
			}
			bss_start_address++;

			l2page += MVE_MMU_PAGE_TABLE_ENTRY_SIZE;
		}
	}
}
