// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2017-2018, ARM Limited
 */
#ifndef __TEE_SERVICE_H_
#define __TEE_SERVICE_H_

int tee_service_load_firmware(void *fw_data, size_t len,
				int mem_fd, size_t mem_len,
				void *fw_secure_desc, int fw_desc_size,
				uint32_t ncores);

#endif
