// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2017-2018, ARM Limited
 */
#ifndef __MVE_FW_MMU_H
#define __MVE_FW_MMU_H

/**
 * Firmware binary header.
 */
struct fw_header
{
    /** The RASC instruction for a jump to the "real" firmware (so we
     *  always can start executing from the first address). */
    uint32_t rasc_jmp;

    /** Host interface protocol version. */
    uint8_t protocol_minor;
    uint8_t protocol_major;

    /** Reserved for future use. Always 0. */
    uint8_t reserved[2];

    /** Human readable codec information. */
    uint8_t info_string[56];

    /** Part number. */
    uint8_t part_number[8];

    /** SVN revision */
    uint8_t svn_revision[8];

    /** Firmware version. */
    uint8_t version_string[16];

    /** Length of the read-only part of the firmware. */
    uint32_t text_length;

    /** Start address for BSS segment.  This is always page-aligned. */
    uint32_t bss_start_address;

    /** How to allocate pages for BSS segment is specified by a bitmap.
     *  The encoding of this is as for the "mem_map" in the protocol,
     *  as if you call it with:
     *    mem_map(bss_start_address, bss_bitmap_size, bss_bitmap_size,
     *            bss_bitmap); */
    uint32_t bss_bitmap_size;
    /** How to allocate pages for BSS segment is specified by a bitmap.
     *  The encoding of this is as for the "mem_map" in the protocol,
     *  as if you call it with:
     *    mem_map(bss_start_address, bss_bitmap_size, bss_bitmap_size,
     *            bss_bitmap); */
    uint32_t bss_bitmap[16];

    /** Defines a region of shared pages */
    uint32_t master_rw_start_address;
    /** Defines a region of shared pages */
    uint32_t master_rw_size;
};

struct mve_fw_version
{
    uint8_t major;                    /**< Firmware major version. */
    uint8_t minor;                    /**< Firmware minor version. */
};

struct mve_fw_secure_descriptor
{
    struct mve_fw_version fw_version; /**< FW protocol version */
    uint32_t l2pages;                 /**< Physical address of l2pages created by secure OS */
};

/* The following code assumes 4 kB pages and that the MVE uses a 32-bit
 * virtual address space. */

#define MVE_MMU_PAGE_SHIFT 12
#define MVE_MMU_PAGE_SIZE (1 << MVE_MMU_PAGE_SHIFT)

/* Size of the MVE MMU page table entry in bytes */
#define MVE_MMU_PAGE_TABLE_ENTRY_SIZE 4

enum mve_mmu_attrib
{
    ATTRIB_PRIVATE = 0,
    ATTRIB_REFFRAME = 1,
    ATTRIB_SHARED_RO = 2,
    ATTRIB_SHARED_RW = 3
};

enum mve_mmu_access
{
    ACCESS_NO = 0,
    ACCESS_READ_ONLY = 1,
    ACCESS_EXECUTABLE = 2,
    ACCESS_READ_WRITE = 3
};

typedef uint32_t mve_mmu_entry_t;

#define MVE_MMU_PADDR_MASK 0x3FFFFFFCLLU
#define MVE_MMU_PADDR_SHIFT 2
#define MVE_MMU_ATTRIBUTE_MASK 0xC0000000LLU
#define MVE_MMU_ATTRIBUTE_SHIFT 30
#define MVE_MMU_ACCESS_MASK 0x3
#define MVE_MMU_ACCESS_SHIFT 0


void fill_l2pages(uint8_t* fw_addr, uint8_t* fw_phys_addr, size_t fw_size, uint8_t *l2pages,
                         uint32_t ncores, struct mve_fw_secure_descriptor *fw_secure_desc);

#endif
