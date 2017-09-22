// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2017-2018, ARM Limited
 */
#define LOG_TAG "SEDGET_VIDEO"
#include <cutils/log.h>

#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/ion.h>
#include "ion_ext.h"

#include "sedget_video.h"

/*
 * private structure
 */
#define SEDGET_MAGIC    0x5347534D  /*'SGSM'*/
struct _sedget_protected_buffer {
	uint32_t size;		/* size of actual secure_video_buffer */
	uint32_t magic;		/* 'SGSM' for checking if object is a
				Secure Gadget implemetation */
	sedget_buf_type type;	/* type of sedget buffer */
	int mem_fd;		/* file descriptor of protected memory */
};

static int allocate_secure_buffer(size_t size, sedget_buf_type type)
{
	int ion_fd, mem_fd = -1;
	struct ion_allocation_data alloc_data;
	struct ion_handle_data hdl_data;
	struct ion_fd_data fd_data;

	ion_fd = open("/dev/ion", O_RDWR);
	if (ion_fd < 0) {
		ALOGE("Failed to open ion device.");
		return -EACCES;
	}
	alloc_data.len = size;
	alloc_data.align = 0;
	alloc_data.flags = 0;

	switch (type) {
		case SEDGET_BUF_INPUT:
			alloc_data.heap_id_mask = ION_HEAP_MVE_PROTECTED_MASK;
			break;
		case SEDGET_BUF_INTERMEDIATE:
			alloc_data.heap_id_mask = ION_HEAP_MULTIMEDIA_PROTECTED_MASK;
			break;
		case SEDGET_BUF_FIRMWARE:
			alloc_data.heap_id_mask = ION_HEAP_MVE_PRIVATE_MASK;
			break;
		default:
			ALOGE("Invalid Buffer Type requested.");
			goto alloc_exit;
	}

	if (ioctl(ion_fd, ION_IOC_ALLOC, &alloc_data) == -1) {
		ALOGE("Failed ION_IOC_ALLOC: %d(%s).", errno, strerror(errno));
		goto alloc_exit;
	}

	/* get new created sharing fd for memory sharing with driver */
	fd_data.handle = alloc_data.handle;
	fd_data.fd = -1;
	if (ioctl(ion_fd, ION_IOC_MAP, &fd_data) != -1)
		mem_fd = fd_data.fd;

	/* just free handle here since it is useless; buffer don't free with
	 * handle freeing operation since they are individual items
	 */
	hdl_data.handle = alloc_data.handle;
	(void)ioctl(ion_fd, ION_IOC_FREE, &hdl_data);

alloc_exit:
	close(ion_fd);

	/* record correct errno */
	if (mem_fd == -1)
		return -errno;
	else
		return mem_fd;
}

static void fill_prot_buf(sedget_protected_buffer * prot_buf,
			  size_t mem_size,
			  sedget_buf_type type,
			  int mem_fd)
{
	prot_buf->size = mem_size;
	prot_buf->magic = SEDGET_MAGIC;
	prot_buf->type = type;
	prot_buf->mem_fd = mem_fd;
}

sedget_protected_buffer *sedget_alloc_prot_buf(size_t mem_size,
					       sedget_buf_type type)
{
	sedget_protected_buffer *prot_buf = NULL;
	int mem_fd = -1;

	if (type < SEDGET_BUF_INPUT || type > SEDGET_BUF_FIRMWARE){
		ALOGE("%s: Invalid buffer type", __FUNCTION__);
		return NULL;
	}

	mem_fd = allocate_secure_buffer(mem_size, type);
	if(mem_fd >= 0) {
		prot_buf = (sedget_protected_buffer *)malloc(
					sizeof(sedget_protected_buffer));
		if (prot_buf)
			fill_prot_buf(prot_buf, mem_size, type, mem_fd);
		else
			close(mem_fd);
	}
out:
	return prot_buf;
}

int sedget_free_prot_buf(sedget_protected_buffer *prot_buf)
{
	if(NULL == prot_buf || SEDGET_MAGIC != prot_buf->magic) {
		ALOGE("%s Not a sedget memory object", __FUNCTION__);
		return -EINVAL;
	}

	if(prot_buf->mem_fd >= 0) {
		close(prot_buf->mem_fd);
		prot_buf->mem_fd = -1;
	}

	free(prot_buf);

	return 0;
}

int sedget_get_mem_fd(sedget_protected_buffer *prot_buf)
{
	if(NULL == prot_buf || SEDGET_MAGIC != prot_buf->magic) {
		ALOGE("%s Not a sedget memory object", __FUNCTION__);
		return -EINVAL;
	}

	return prot_buf->mem_fd;
}
