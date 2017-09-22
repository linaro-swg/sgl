/*
 * drivers/staging/android/uapi/ion_ext.h
 *
 * Copyright (C) 2017 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _UAPI_LINUX_ION_EXT_H
#define _UAPI_LINUX_ION_EXT_H

#define ION_HEAP_ID_SYSTEM		ION_HEAP_TYPE_SYSTEM
#define ION_HEAP_ID_SYSTEM_CONTIG	ION_HEAP_TYPE_SYSTEM_CONTIG
#define ION_HEAP_ID_DMA			ION_HEAP_TYPE_DMA
#define ION_HEAP_ID_MVE_PRIVATE		(ION_HEAP_TYPE_CUSTOM + 0)
#define ION_HEAP_ID_MVE_PROTECTED	(ION_HEAP_TYPE_CUSTOM + 1)
#define ION_HEAP_ID_MULTIMEDIA_PROTECTED	(ION_HEAP_TYPE_CUSTOM + 2)

#define ION_HEAP_MVE_PRIVATE_MASK	(1 << ION_HEAP_ID_MVE_PRIVATE)
#define ION_HEAP_MVE_PROTECTED_MASK	(1 << ION_HEAP_ID_MVE_PROTECTED)
#define ION_HEAP_MULTIMEDIA_PROTECTED_MASK	\
				(1 << ION_HEAP_ID_MULTIMEDIA_PROTECTED)

#endif /* _UAPI_LINUX_ION_EXT_H */
