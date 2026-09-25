/*
 * Copyright (C) 2016 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _CCU_MVA_H_
#define _CCU_MVA_H_

#include "mtk_ion.h"
#include "ion_drv.h"
#include <linux/iommu.h>
#ifdef CONFIG_MTK_IOMMU_V2
#include "mtk_iommu.h"
#include <dt-bindings/memory/mt6873-larb-port.h>
#else
#include "m4u.h"
#endif

int ccu_ion_init(void);
int ccu_ion_uninit(void);
int ccu_allocate_mva(uint32_t *mva, void *va,
	struct ion_handle **handle, int buffer_size);
int ccu_deallocate_mva(struct ion_handle **handle);
struct ion_handle *ccu_ion_import_handle(int fd);
void ccu_ion_free_import_handle(struct ion_handle *handle);

/*
 * 官核 struct CcuMemInfo = 40B (0x28)，布局(以官核反汇编访问偏移为准):
 *   shareFd       @0x00
 *   va            @0x08
 *   align_mva     @0x10
 *   mva           @0x14
 *   size          @0x18
 *   occupiedSize  @0x1c
 *   cached        @0x20 (官核按 u32 读写)
 *   ion_log       @0x24 (官核按 char 读: >10MB 分配/释放时打日志)
 */
struct CcuMemInfo {
	int shareFd;
	char *va;
	uint32_t align_mva;
	uint32_t mva;
	uint32_t size;
	uint32_t occupiedSize;
	uint32_t cached;
	bool ion_log;
};

/* 官核 sizeof(struct CcuMemHandle) = 48B (0x30) */
struct CcuMemHandle {
	struct ion_handle *ionHandleKd;
	struct CcuMemInfo meminfo;
};

int ccu_allocate_mem(struct CcuMemHandle *memHandle, int size, bool cached);
int ccu_deallocate_mem(struct CcuMemHandle *memHandle);
struct CcuMemInfo *ccu_get_binary_memory(void);

#endif
