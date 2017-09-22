// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2017-2018, ARM Limited
 */

#ifndef __SEDGET_VIDEO_H__
#define __SEDGET_VIDEO_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque type of protected memory */
typedef struct _sedget_protected_buffer sedget_protected_buffer;

/*
 * Protected memory types:
 *  SEDGET_BUF_INPUT		: video codec input buffer(s) holding bitstream
 *  SEDGET_BUF_INTERMEDIATE	: video codec engine working buffer(s)
 *  SEDGET_BUF_FIRMWARE		: video codec firmware working buffer(s)
 *
 * Buffer type 'SEDGET_BUF_INTERMEDIATE' and 'SEDGET_BUF_FIRMWARE'
 * allocation depends on hardware codec types. These two buffer types
 * are optional for codecs who don't support firmware and intermediate.
 */
typedef enum _sedget_buf_type {
	SEDGET_BUF_INPUT,
	SEDGET_BUF_INTERMEDIATE,
	SEDGET_BUF_FIRMWARE
} sedget_buf_type;

/*
 * Allocate protected buffer for video codec
 *
 * @param mem_size Protected buffer size in bytes
 * @param type     Protected buffer type
 *
 * @return Pointer to 'sedget_protected_buffer' object is returned
 *         NULL indicates a failure and errno is set.
 */
sedget_protected_buffer *sedget_alloc_prot_buf(size_t mem_size,
					       sedget_buf_type type);

/*
 * Load 'role' specified firmware into secure memory and return result in
 * user provided buffer
 *
 * @param role		Indicates firmware codec type to be loaded
 * @param num_cores	Number of cores of MVE in the hardware
 * @param out		Pagetable items are created and returned in 'out' after
 *			firmware is loaded into specified position
 * @param out_size	Size in bytes of memory pointed by out. An out of
 * 			memory error is returned if out_size is not big enough
 *
 * @return Pointer to 'sedget_protected_buffer' object is returned. NULL
 * 	indicates a failure and errno is set.
 */
sedget_protected_buffer *sedget_load_prot_firmware(const char *role,
						   int num_cores,
						   void *out,
						   size_t out_size);

/*
 * Free secure memory allocated by sedget_alloc_prot_buf and
 * sedget_load_prot_firmware.
 *
 * @param prot_buf	Pointer to 'sedget_protected_buffer' object previously
 * 			allocated
 *
 * @return 0 on success or an negative errno indicates error occured.
 */
int sedget_free_prot_buf(sedget_protected_buffer *prot_buf);

/*
 * Return fd for the dma/ion memory encapsulated in sedget_protected_buffer
 *
 * @param prot_buf	Pointer to 'sedget_protected_buffer' object previously
 * 			allocated.
 *
 * @return a file describitor on success or negative errno indicates errors.
 */
int sedget_get_mem_fd(sedget_protected_buffer *prot_buf);

#ifdef __cplusplus
}
#endif

#endif
