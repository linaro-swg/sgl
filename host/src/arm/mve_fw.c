// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2017-2018, ARM Limited
 */
#define LOG_TAG "SEDGET_VIDEO"
#include <cutils/log.h>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "sedget_video.h"
#include "tee_service.h"

/* TBD: hard code secure firmpath now */
#define SEC_FW_PATH     "/lib/firmware/"

#define SIZE_4M         0x400000
#define SIZE_1M         0x100000

#define COUNT_ELEM(ar)	(sizeof(ar) / sizeof(ar[0]))

/* role v.s. secure firmware name */
struct firmware_list_item
{
	const char *role;
	const char *filename;
};

static struct firmware_list_item firmware_list[] = {
	{ "video_decoder.avc",      SEC_FW_PATH "h264dec.efwb" },
	{ "video_encoder.avc",      SEC_FW_PATH "h264enc.efwb" },
	{ "video_decoder.hevc",     SEC_FW_PATH "hevcdec.efwb" },
	{ "video_encoder.hevc",     SEC_FW_PATH "hevcenc.efwb" },
	{ "video_decoder.h264",     SEC_FW_PATH "h264dec.efwb" },
	{ "video_decoder.vp8",      SEC_FW_PATH "vp8dec.efwb" },
	{ "video_encoder.vp8",      SEC_FW_PATH "vp8enc.efwb" },
	{ "video_decoder.vp9",      SEC_FW_PATH "vp9dec.efwb" },
	{ "video_encoder.vp9",      SEC_FW_PATH "vp9enc.efwb" },
	{ "video_decoder.rv",       SEC_FW_PATH "rvdec.efwb" },
	{ "video_decoder.mpeg2",    SEC_FW_PATH "mpeg2dec.efwb" },
	{ "video_decoder.mpeg4",    SEC_FW_PATH "mpeg4dec.efwb" },
	{ "video_decoder.h263",     SEC_FW_PATH "mpeg4dec.efwb" },
	{ "video_decoder.vc1",      SEC_FW_PATH "vc1dec.efwb" },
	{ "video_encoder.jpeg",     SEC_FW_PATH "jpegenc.efwb" },
	{ "video_decoder.jpeg",     SEC_FW_PATH "jpegdec.efwb" }
};

static size_t get_firmware_file_size(const char *filename)
{
	struct stat fw_stat;

	if (stat(filename, &fw_stat) == -1)
		return 0;

	return (size_t)fw_stat.st_size;
}

static size_t load_firmware(const char *filename, int shm_fd, size_t shm_len,
			    void *fw_secure_desc, int fw_desc_size,
			    uint32_t ncores)
{
	FILE *fw_fp = NULL;
	size_t res = 0;
	unsigned char *fw_buf = NULL;
	size_t fw_size = 0;

	fw_size = get_firmware_file_size(filename);
	if (fw_size == 0 || fw_size > SIZE_1M) {
		ALOGE("Invalid fw size.");
		goto exit;
	}

	fw_fp = fopen(filename, "rb");
	if (fw_fp == NULL) {
		ALOGE("Firmware open error");
		goto exit;
	}

	fw_buf = malloc(fw_size);
	if (fw_buf == NULL) {
		ALOGE("Malloc buffer error");
		goto exit;
	}

	if (fread(fw_buf, 1, fw_size, fw_fp) != fw_size) {
		ALOGE("Firmware read error");
		goto exit;
	}

	if (tee_service_load_firmware(fw_buf, fw_size, shm_fd, shm_len,
				      fw_secure_desc, fw_desc_size, ncores))
		goto exit;

	res = fw_size;

exit:
	if (fw_buf)
		free(fw_buf);

	if (fw_fp)
		fclose(fw_fp);

	return res;
}


sedget_protected_buffer *sedget_load_prot_firmware(const char *role,
						   int num_cores,
						   void *out,
						   size_t out_size)
{
	int mem_fd = -1;
	sedget_protected_buffer *prot_buf = NULL;
	struct firmware_list_item *p_fw_item;
	uint32_t i, elem_count;

	if (role == NULL || out == NULL || out_size == 0)
		return NULL;

	elem_count = COUNT_ELEM(firmware_list);

	/* find matching firmware for role */
	for (i = 0; i < elem_count; i++)
		if (!strcmp(role, firmware_list[i].role))
			break;

	if (i == elem_count) {
		ALOGE("Failed to find matching role");
		return NULL;
	}

	p_fw_item = &firmware_list[i];

	/* prepare ion buffer -- firmware BSS needs more buffers;
	 * 4M is maxium size for L2 pte of firmware MMU */
	prot_buf = sedget_alloc_prot_buf(SIZE_4M, SEDGET_BUF_FIRMWARE);
	if (prot_buf == NULL) {
		ALOGE("Failed to allocate ion buffer");
		return NULL;
	}

	mem_fd = sedget_get_mem_fd(prot_buf);
	if (mem_fd < 0)
		goto error_out;

	memset(out, 0x0, out_size);

	if (load_firmware(p_fw_item->filename, mem_fd, SIZE_4M,
				out, out_size, num_cores) == 0) {
		ALOGE("Failed to load firmware");
		goto error_out;
	}

	ALOGD("Secure Firmware loaded with size: %zu", out_size);

	return prot_buf;

error_out:
	sedget_free_prot_buf(prot_buf);
	return NULL;
}
