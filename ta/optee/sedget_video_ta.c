// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2017-2018, ARM Limited
 */

#include <string.h>
#include <tee_api.h>
#include <tee_internal_api_extensions.h>
#include <tee_internal_api.h>
#include <tee_ta_api.h>
#include <trace.h>
#include <sdp_pa.h>

#include "sedget_video_ta.h"
#include "mve_fw_mmu.h"

#define FIRMWARE_SIGNATURE_LEN		32

/* TODO: remove this test purpose key into formal formal process */
static const uint8_t fw_encryption_key[] = {
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* 01234567 */
	0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, /* 89ABCDEF */
};

static TEE_Result decrypt_firmware(void *srcdata, size_t srclen,
		   void *destdata, uint32_t *destlen)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	TEE_ObjectHandle trans_key;
	TEE_Attribute attrs;
	TEE_OperationHandle op;
	uint32_t maxkeylen = 16;

	attrs.attributeID = TEE_ATTR_SECRET_VALUE;
	attrs.content.ref.buffer = (void *)fw_encryption_key;
	attrs.content.ref.length = sizeof(fw_encryption_key);

	res = TEE_AllocateOperation(&op,
				    TEE_ALG_AES_ECB_NOPAD,
				    TEE_MODE_DECRYPT,
				    (maxkeylen * 8));
	if (res != TEE_SUCCESS) {
		EMSG("Can not allocate operation (0x%x)", res);
		goto out;
	}

	res = TEE_AllocateTransientObject(TEE_TYPE_AES,
					  (maxkeylen * 8),
					  &trans_key);
	if (res != TEE_SUCCESS) {
		EMSG("Can not allocate transient object 0x%x", res);
		goto out1;
	}

	res = TEE_PopulateTransientObject(trans_key, &attrs, 1);
	if (res != TEE_SUCCESS) {
		EMSG("Populate transient object error");
		goto out2;
	}
	res = TEE_SetOperationKey(op, trans_key);
	if (res != TEE_SUCCESS) {
		EMSG("Can not set operation key");
		goto out2;
	}

	TEE_CipherInit(op, NULL, 0);
	res = TEE_CipherDoFinal(op, srcdata, srclen, destdata, destlen);
	if (res != TEE_SUCCESS) {
		EMSG("Can not do AES %x", res);
		goto out2;
	}
out2:
	TEE_FreeTransientObject(trans_key);
out1:
	TEE_FreeOperation(op);
out:
	return res;
}

static TEE_Result verify_firmware_signature(const void *chunk,
			uint32_t chunkLen)
{
	TEE_Result res = TEE_SUCCESS;
	TEE_OperationHandle op = TEE_HANDLE_NULL;
	void *hash = NULL;
	uint32_t hashlen = FIRMWARE_SIGNATURE_LEN;

	res = TEE_AllocateOperation(&op, TEE_ALG_SHA1, TEE_MODE_DIGEST, 0);
	if (res != TEE_SUCCESS)
		goto out;

	hash = TEE_Malloc(hashlen, 0);
	if (!hash) {
		EMSG("Malloc memory failed!\n");
		res = TEE_ERROR_OUT_OF_MEMORY;
		goto out;
	}

	res = TEE_DigestDoFinal(op, chunk, chunkLen - FIRMWARE_SIGNATURE_LEN,
			hash, &hashlen);
	if (res != TEE_SUCCESS) {
		EMSG("Digest failed!");
		goto out;
	}

	if (TEE_MemCompare((char *)chunk + chunkLen -
			FIRMWARE_SIGNATURE_LEN,
			hash, hashlen) != 0 ) {
		EMSG("Verify firmware hash failed!");
		res = TEE_ERROR_CORRUPT_OBJECT;
	}
out:
	if (op)
		TEE_FreeOperation(op);

	if (hash)
		TEE_Free(hash);
	return res;
}

static TEE_Result decrypt_video_firmware(void *srcdata, size_t srclen,
		void *destdata, uint32_t *destlen)
{
	TEE_Result res = TEE_SUCCESS;

	res = decrypt_firmware(srcdata, srclen, destdata, destlen);
	if (res != TEE_SUCCESS) {
		EMSG("Decrypt firmware failed (0x%x)", res);
		return res;
	}

	res = verify_firmware_signature(destdata, *destlen);
	if (res != TEE_SUCCESS) {
		EMSG("Verify firmware signature failed (0x%x)", res);
		return res;
	}

	return res;
}

/* A TA have no reason to retrieve physical address of buffers;
 * call helper PTA to retrieve this */
static uint8_t *get_phys_address(TEE_Param *param)
{
	TEE_Result rc;
	TEE_UUID pta_uuid = PTA_SDP_PA_UUID;
	TEE_TASessionHandle sess = TEE_HANDLE_NULL;
	uint32_t param_types;
	TEE_Param p[TEE_NUM_PARAMS];
	param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
				      TEE_PARAM_TYPE_VALUE_OUTPUT,
				      TEE_PARAM_TYPE_NONE,
				      TEE_PARAM_TYPE_NONE);
	p[0].memref.buffer = param->memref.buffer;
	p[0].memref.size = param->memref.size;
	rc = TEE_OpenTASession(&pta_uuid, 0, 0, NULL, &sess, NULL);
	if (rc == TEE_SUCCESS) {
		rc = TEE_InvokeTACommand(sess, 0, PTA_CMD_SDP_VIRT_TO_PHYS,
					 param_types, p, NULL);
		if(rc != TEE_SUCCESS) {
			return NULL;
		}
		TEE_CloseTASession(sess);
	}

	/* LIMIT: current mve mmu cares for 32bit address */
	return (uint8_t *)(uintptr_t)p[1].value.a;
}

/*
 * Basic Secure Data Path access test commands:
 * - command INJECT: copy from non secure input into secure output.
 */
static TEE_Result sedget_video_load_firmware(uint32_t types,
		TEE_Param params[TEE_NUM_PARAMS])
{
	TEE_Result rc;
	const int ns_idx = 0;       /* nonsecure buffer index */
	const int sec_idx = 1;      /* secure buffer index */
	const int fw_desc_idx = 2;  /* fw load descriptor buffer index */
	const int ncores_idx = 3;
	uint8_t *fw_addr, *fw_phys_addr;
	uint8_t *l2pages, *l2pages_phys;
	uint32_t len;
	struct mve_fw_secure_descriptor *fw_secure_desc;
	int ncores = 1;

	if (types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
				     TEE_PARAM_TYPE_MEMREF_OUTPUT,
				     TEE_PARAM_TYPE_MEMREF_OUTPUT,
				     TEE_PARAM_TYPE_VALUE_INPUT)) {
		EMSG("bad parameters types: %x", (unsigned)types);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	if (params[sec_idx].memref.size <
	    params[ns_idx].memref.size + MVE_MMU_PAGE_SIZE)
		return TEE_ERROR_SHORT_BUFFER;

	/*
	 * We could rely on the TEE to provide consistent buffer/size values
	 * to reference a buffer with a unique and consistent secure attribute
	 * value. Hence it is safe enough (and more optimal) to test only the
	 * secure attribute of a single byte of it. Yet, since the current
	 * test does not deal with performance, let check the secure attribute
	 * of each byte of the buffer.
	 */
	rc = TEE_CheckMemoryAccessRights(TEE_MEMORY_ACCESS_ANY_OWNER |
					 TEE_MEMORY_ACCESS_READ |
					 TEE_MEMORY_ACCESS_NONSECURE,
					 params[ns_idx].memref.buffer,
					 params[ns_idx].memref.size);
	if (rc != TEE_SUCCESS) {
		EMSG("TEE_CheckMemoryAccessRights(nsec) failed %x\n", rc);
		return rc;
	}

	rc = TEE_CheckMemoryAccessRights(TEE_MEMORY_ACCESS_ANY_OWNER |
					 TEE_MEMORY_ACCESS_WRITE |
					 TEE_MEMORY_ACCESS_SECURE,
					 params[sec_idx].memref.buffer,
					 params[sec_idx].memref.size);
	if (rc != TEE_SUCCESS) {
		EMSG("TEE_CheckMemoryAccessRights(secure) failed %x\n", rc);
		return rc;
	}

	rc = TEE_CheckMemoryAccessRights(TEE_MEMORY_ACCESS_ANY_OWNER |
					 TEE_MEMORY_ACCESS_READ |
					 TEE_MEMORY_ACCESS_NONSECURE,
					 params[fw_desc_idx].memref.buffer,
					 params[fw_desc_idx].memref.size);
	if (rc != TEE_SUCCESS) {
		EMSG("TEE_CheckMemoryAccessRights(nsec) failed %x\n", rc);
		return rc;
	}

	fw_phys_addr = get_phys_address(&params[sec_idx]);
	if(NULL == fw_phys_addr) {
		return TEE_ERROR_ACCESS_DENIED;
	}

	ncores = params[ncores_idx].value.a;

#ifdef CFG_CACHE_API
	rc = TEE_CacheInvalidate(params[sec_idx].memref.buffer,
				 params[sec_idx].memref.size);
	if (rc != TEE_SUCCESS) {
		EMSG("TEE_CacheFlush(%p, %x) failed: 0x%x\n",
		     params[sec_idx].memref.buffer,
		     params[sec_idx].memref.size, rc);
		return rc;
	}
#endif /* CFG_CACHE_API */

	fw_addr = params[sec_idx].memref.buffer;
	len = params[sec_idx].memref.size - (ncores * MVE_MMU_PAGE_SIZE);
	l2pages = fw_addr + len;
	l2pages_phys = fw_phys_addr + len;

	/* Empty sdp buffer */
	TEE_MemFill(fw_addr, 0x0,
			params[sec_idx].memref.size);

	rc = decrypt_video_firmware(params[ns_idx].memref.buffer,
				       params[ns_idx].memref.size,
				       fw_addr,
				       &len);
	if (rc != TEE_SUCCESS) {
		EMSG("decrypt_video_firmware failed: 0x%x\n", rc);
		return rc;
	}

	fw_secure_desc = (struct mve_fw_secure_descriptor *)
				params[fw_desc_idx].memref.buffer;

	fill_l2pages(fw_addr, fw_phys_addr, len, l2pages,
		     ncores,fw_secure_desc);
	fw_secure_desc->l2pages = (uint32_t)(uintptr_t)l2pages_phys;

#ifdef CFG_CACHE_API
	rc = TEE_CacheFlush(params[sec_idx].memref.buffer,
			    params[sec_idx].memref.size);
	if (rc != TEE_SUCCESS) {
		EMSG("TEE_CacheFlush(%p, %x) failed: 0x%x\n",
		     params[sec_idx].memref.buffer,
		     params[sec_idx].memref.size, rc);
		return rc;
	}
#endif /* CFG_CACHE_API */
#ifdef CFG_CACHE_API
	rc = TEE_CacheFlush(params[fw_desc_idx].memref.buffer,
			    params[fw_desc_idx].memref.size);
	if (rc != TEE_SUCCESS) {
		EMSG("TEE_CacheFlush(%p, %x) failed: 0x%x\n",
		     params[fw_desc_idx].memref.buffer,
		     params[fw_desc_idx].memref.size, rc);
		return rc;
	}
#endif /* CFG_CACHE_API */
	return rc;
}

TEE_Result TA_CreateEntryPoint(void)
{
	return TEE_SUCCESS;
}

void TA_DestroyEntryPoint(void)
{
}

TEE_Result TA_OpenSessionEntryPoint(uint32_t nParamTypes,
		TEE_Param pParams[TEE_NUM_PARAMS],
		void **ppSessionContext)
{
	(void)nParamTypes;
	(void)pParams;
	(void)ppSessionContext;
	return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void *pSessionContext)
{
	(void)pSessionContext;
}

TEE_Result TA_InvokeCommandEntryPoint(void *pSessionContext,
		uint32_t nCommandID, uint32_t nParamTypes,
		TEE_Param pParams[TEE_NUM_PARAMS])
{
	(void)pSessionContext;

	switch (nCommandID) {
	case SEDGET_VIDEO_TA_CMD_LOAD_FW:
		return sedget_video_load_firmware(nParamTypes, pParams);
	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}
}
