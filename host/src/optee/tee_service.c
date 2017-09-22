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

#include <tee_client_api_extensions.h>
#include <sedget_video_ta.h>


#include "tee_service.h"

/* types of context */
typedef struct _Tee_Inst {
	TEEC_Context ctx;
	TEEC_Session sess;
} Tee_Inst;

static int create_tee_instance(Tee_Inst *inst)
{
	TEEC_Result teerc;
	uint32_t err_origin;
	TEEC_UUID ta_uuid = SEDGET_VIDEO_TA_UUID;

	teerc = TEEC_InitializeContext(NULL, &inst->ctx);
	if (teerc != TEEC_SUCCESS)
		return -EACCES;

	teerc = TEEC_OpenSession(&inst->ctx, &inst->sess, &ta_uuid,
				 TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
	if (teerc != TEEC_SUCCESS)
		ALOGE("Error: open session failed %x %d", teerc, err_origin);

	return (teerc == TEEC_SUCCESS) ? 0 : -EFAULT;
}

static void finalize_tee_instance(Tee_Inst *inst)
{
	if (inst) {
		TEEC_CloseSession(&inst->sess);
		TEEC_FinalizeContext(&inst->ctx);
	}
}

static int tee_register_buffer(Tee_Inst *inst,
			       TEEC_SharedMemory *shm, int mem_fd)
{
	TEEC_Result teerc;

	shm->flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
	teerc = TEEC_RegisterSharedMemoryFileDescriptor(&inst->ctx,
							shm, mem_fd);
	if (teerc != TEEC_SUCCESS) {
		ALOGE("Error: TEEC_RegisterMemoryFileDescriptor failed %x",
		      teerc);
		return -EINVAL;
	}

	return 0;
}

static void tee_deregister_buffer(Tee_Inst *inst, void *shm)
{
	(void)inst;

	TEEC_ReleaseSharedMemory((TEEC_SharedMemory *)shm);
}

int tee_service_load_firmware(void *fw_data, size_t len,
			      int mem_fd, size_t mem_len,
			      void *fw_secure_desc, int fw_desc_size,
			      uint32_t ncores)
{
	TEEC_SharedMemory shm;
	TEEC_Result teerc = TEEC_ERROR_GENERIC;
	TEEC_Operation op;
	Tee_Inst tee_inst;
	uint32_t err_origin;
	int ret;

	ret = create_tee_instance(&tee_inst);
	if (ret != 0)
		return ret;

	ret = tee_register_buffer(&tee_inst, &shm, mem_fd);
	if (ret != 0)
		goto _finalize_exit;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
					 TEEC_MEMREF_PARTIAL_OUTPUT,
					 TEEC_MEMREF_TEMP_OUTPUT,
					 TEEC_VALUE_INPUT);

	op.params[0].tmpref.buffer = fw_data;
	op.params[0].tmpref.size = len;

	op.params[1].memref.parent = &shm;
	op.params[1].memref.size = mem_len;
	op.params[1].memref.offset = 0;

	op.params[2].tmpref.buffer = fw_secure_desc;
	op.params[2].tmpref.size = fw_desc_size;

	op.params[3].value.a = ncores;
	op.params[3].value.b = 0;

	teerc = TEEC_InvokeCommand(&tee_inst.sess,
				   SEDGET_VIDEO_TA_CMD_LOAD_FW,
				   &op, &err_origin);
	if (teerc != TEEC_SUCCESS) {
		ALOGE("TA Load firmware failed %#x - %d", teerc, err_origin);
		ret = -EINVAL;
		goto _deregister_exit;
	}

	ret = 0;

_deregister_exit:
	tee_deregister_buffer(&tee_inst, &shm);
_finalize_exit:
	finalize_tee_instance(&tee_inst);

	return ret;
}
