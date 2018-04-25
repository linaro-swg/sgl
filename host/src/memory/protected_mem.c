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
#include <cutils/native_handle.h>

#include "sedget_video.h"

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

sedget_protected_buffer *sedget_alloc_prot_buf(size_t mem_size,
					       sedget_buf_type type)
{
	native_handle_t *native_h = native_handle_create(1, 0);
	int mem_fd = -1;

	if (type < SEDGET_BUF_INPUT || type > SEDGET_BUF_FIRMWARE){
		ALOGE("%s: Invalid buffer type", __FUNCTION__);
		return NULL;
	}

	if (!native_h) {
		ALOGE("%s: failed to create native handle", __FUNCTION__);
		return NULL;
	}

	mem_fd = allocate_secure_buffer(mem_size, type);
	if(mem_fd >= 0) {
		native_h->data[0] = mem_fd;
	}
out:
	return native_h;
}

int sedget_free_prot_buf(sedget_protected_buffer *prot_buf)
{
	if(NULL == prot_buf) {
		ALOGE("%s Not a sedget memory object", __FUNCTION__);
		return -EINVAL;
	}

	native_handle_close((native_handle_t *)prot_buf);
	native_handle_delete((native_handle_t *)prot_buf);

	return 0;
}

int sedget_get_mem_fd(sedget_protected_buffer *prot_buf)
{
	if(NULL == prot_buf) {
		ALOGE("%s Not a sedget memory object", __FUNCTION__);
		return -EINVAL;
	}

	return ((native_handle_t *)prot_buf)->data[0];
}
