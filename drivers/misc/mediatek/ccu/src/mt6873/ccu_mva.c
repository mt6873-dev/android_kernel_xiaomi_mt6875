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

#include "ccu_cmn.h"
#include "ccu_mva.h"
#include "ccu_platform_def.h"
#include <linux/timekeeping.h>
#include <linux/string.h>

/*
 * 官核依据:
 *   ccu_allocate_mem/ccu_deallocate_mem 中 meminfo+0x24 按 char 读,
 *   与 size > 0xa00000(10MB) 一起决定是否打印计时/大小日志
 */
#define ION_LOG_SIZE	(10*1024*1024)	/* 10M */
/* 官核: ion_alloc 的 flags = (cached?3:0) | 4 => cached 时 7, 否则 4 */
#define ION_FLAG_FREE_WITHOUT_DEFER	(4)

static struct ion_client *_ccu_ion_client;

static unsigned long get_ns_systemtime(void)
{
	struct timespec64 ts;

	ts.tv_sec = 0;
	ts.tv_nsec = 0;
	getnstimeofday64(&ts);
	return ((unsigned long)(ts.tv_sec)) * 1000000000 + (ts.tv_nsec);
}

int ccu_config_m4u_port(void);
static struct ion_handle *_ccu_ion_alloc(struct ion_client *client,
		unsigned int heap_id_mask, size_t align, unsigned int size,
		bool cached, bool ion_log);
static int _ccu_ion_get_mva(struct ion_client *client,
	struct ion_handle *handle,
		unsigned int *mva, bool cached);
static void _ccu_ion_free_handle(struct ion_client *client,
	struct ion_handle *handle);

int ccu_ion_init(void)
{
	MBOOL need_init = MFALSE;

	ccu_lock_ion_client_mutex();
	if (!_ccu_ion_client && g_ion_device) {
		LOG_INF_MUST("CCU ION_client need init\n");
		need_init = MTRUE;
	} else {
		LOG_INF_MUST("ION Client exist: 0x%p | Device NULL: 0x%p\n",
			_ccu_ion_client, g_ion_device);
	}

	if (need_init == MTRUE) {
		_ccu_ion_client = ion_client_create(g_ion_device, "ccu");
		LOG_INF_MUST("CCU ION_client create success: 0x%p\n",
			_ccu_ion_client);
	}

	ccu_unlock_ion_client_mutex();
	return 0;
}

int ccu_ion_uninit(void)
{
	MBOOL need_uninit = MFALSE;

	ccu_lock_ion_client_mutex();
	if (_ccu_ion_client && g_ion_device) {
		LOG_INF_MUST("CCU ION_client need uninit\n");
		need_uninit = MTRUE;
	}

	if (need_uninit == MTRUE) {
		ion_client_destroy(_ccu_ion_client);
		LOG_INF_MUST("CCU ION_client destroy done.\n");
		_ccu_ion_client = NULL;
	}

	ccu_unlock_ion_client_mutex();
	return 0;
}

int ccu_deallocate_mva(struct ion_handle **handle)
{
	LOG_DBG("X-:%s\n", __func__);
	if (_ccu_ion_client == NULL) {
		LOG_ERR("%s: _ccu_ion_client is null!\n", __func__);
		return -1;
	}

	if (*handle != NULL) {
		_ccu_ion_free_handle(_ccu_ion_client, *handle);
		*handle = NULL;
	}
	return 0;
}

/*
 * 官核 ccu_allocate_mva (0xffffff8008ca755c):
 *   _ccu_ion_client 为空 -> 报错返回
 *   ccu_config_m4u_port() 失败 -> 打印并返回该错误
 *   *handle == 0 -> 打印 "ion_alloc for size %d failed" 并返回 -1
 *   _ccu_ion_get_mva(client, *handle, mva, cached=0) 成功 -> 返回 0
 *   失败 -> 打印 + ion_free(nolock) + *handle = 0 + 返回 -1
 *   (官核不再自行 ion_alloc, 由上层持有 handle)
 */
int ccu_allocate_mva(uint32_t *mva, void *va,
	struct ion_handle **handle, int buffer_size)
{
	int ret = 0;

	if (_ccu_ion_client == NULL) {
		LOG_ERR("%s: _ccu_ion_client is null!\n", __func__);
		return -1;
	}

	ret = ccu_config_m4u_port();
	if (ret) {
		LOG_ERR("fail to config m4u port!\n");
		return ret;
	}

	if (*handle == NULL) {
		LOG_ERR("Fatal Error, ion_alloc for size %d failed\n",
			buffer_size);
		return -1;
	}

	ret = _ccu_ion_get_mva(_ccu_ion_client, *handle, mva, false);
	if (ret == 0)
		return 0;

	LOG_ERR("ccu ion_get_mva failed\n");
	_ccu_ion_free_handle(_ccu_ion_client, *handle);
	*handle = NULL;
	return -1;
}



int ccu_config_m4u_port(void)
{
	int ret = 0;

#if defined(CONFIG_MTK_M4U)
	struct M4U_PORT_STRUCT port;

	port.ePortID = M4U_PORT_CCU0;
	port.Virtuality = 1;
	port.Security = 0;
	port.domain = 3;
	port.Distance = 1;
	port.Direction = 0;

	ret = m4u_config_port(&port);
#endif
	return ret;
}

static struct ion_handle *_ccu_ion_alloc(struct ion_client *client,
		unsigned int heap_id_mask, size_t align, unsigned int size,
		bool cached, bool ion_log)
{
	unsigned long ts_start, ts_end;
	struct ion_handle *disp_handle = NULL;

	/* 官核 ccu_allocate_mem 内联展开: ion_log 时统计耗时至多打印一次 */
	if (ion_log)
		ts_start = get_ns_systemtime();

	disp_handle = ion_alloc(client, size, align, heap_id_mask,
		((cached) ? 3 : 0) | ION_FLAG_FREE_WITHOUT_DEFER);
	if (IS_ERR(disp_handle)) {
		LOG_ERR("disp_ion_alloc 1error %p\n", disp_handle);
		return NULL;
	} else {
		if ((ion_log) && (size > ION_LOG_SIZE)) {
			ts_end = get_ns_systemtime();
			LOG_INF_MUST("ion alloc size = %d, caller = CCU, costTime = %lu ns\n",
				size, (unsigned long)(ts_end - ts_start));
		}
	}

	LOG_DBG("disp_ion_alloc 1 %p\n", disp_handle);

	return disp_handle;

}

/*
 * 官核 _ccu_ion_get_mva (0xffffff8008ca77f4) 逐步对应:
 *   第一次 ion_kernel_ioctl(ION_CMD_MULTIMEDIA):
 *     mm_cmd = 8 (= ION_MM_GET_IOVA)
 *     kernel_handle = handle, security = 0, coherent = 1
 *     cached=0: module_id = 0x2c0 = M4U_PORT_L22_CCU0
 *               reserve_iova = 0x40000000 / 0x43ffffff
 *     cached=1: module_id = 0x2e0 = M4U_PORT_L23_CCU1
 *               reserve_iova = 0x44000000 / 0x47ffffff
 *     成功后 *mva = mm_data.get_phys_param.phy_addr
 *   第二次 ion_kernel_ioctl: mm_cmd = 1 (= ION_MM_SET_DEBUG_INFO),
 *     dbg_name = "CCU_BUFFER", value1 = 67(0x43), value2 = 97(0x61),
 *     value3 = 109(0x6d), value4 = 0; 失败仅打印, 不改返回值
 */
static int _ccu_ion_get_mva(struct ion_client *client,
	struct ion_handle *handle,
		unsigned int *mva, bool cached)
{
	struct ion_mm_data mm_data;
	int err;
	size_t count = 0;
	char const *ccu_bufferName = "CCU_BUFFER";

	mm_data.mm_cmd = ION_MM_GET_IOVA;
	mm_data.config_buffer_param.kernel_handle = handle;
	mm_data.config_buffer_param.security    = 0;
	mm_data.config_buffer_param.coherent    = 1;
	if (cached == false) {
		mm_data.config_buffer_param.module_id = M4U_PORT_L22_CCU0;
		mm_data.config_buffer_param.reserve_iova_start =
		CCU_DDR_BUF_MVA_LOWER_BOUND;
		mm_data.config_buffer_param.reserve_iova_end =
		CCU_DDR_BUF_MVA_UPPER_BOUND;
	} else {
		mm_data.config_buffer_param.module_id   = M4U_PORT_L23_CCU1;
		mm_data.config_buffer_param.reserve_iova_start =
		CCU_CTRL_BUFS_LOWER_BOUND;
		mm_data.config_buffer_param.reserve_iova_end =
		CCU_CTRL_BUFS_UPPER_BOUND;
	}

	if (ion_kernel_ioctl(client, ION_CMD_MULTIMEDIA,
		(unsigned long)&mm_data) < 0) {
		LOG_ERR("disp_ion_get_mva: config buffer failed.%p -%p\n",
			client, handle);

		ion_free(client, handle);
		return -1;
	}
	*mva = mm_data.get_phys_param.phy_addr;

	LOG_DBG_MUST("alloc mmu addr hnd=0x%p,mva=0x%08x\n",
		handle, (unsigned int)*mva);

	mm_data.mm_cmd = ION_MM_SET_DEBUG_INFO;
	mm_data.buf_debug_info_param.kernel_handle = handle;
	/* Check Length of "ccu_bufferName" */
	if (strlen(ccu_bufferName) < ION_MM_DBG_NAME_LEN)
		count = strlen(ccu_bufferName);
	else
		count = ION_MM_DBG_NAME_LEN - 1;
	strncpy(mm_data.buf_debug_info_param.dbg_name, ccu_bufferName, count);
	mm_data.buf_debug_info_param.dbg_name[count] = '\0';
	mm_data.buf_debug_info_param.value1 = 67;
	mm_data.buf_debug_info_param.value2 = 97;
	mm_data.buf_debug_info_param.value3 = 109;
	mm_data.buf_debug_info_param.value4 = 0;
	err = ion_kernel_ioctl(client, ION_CMD_MULTIMEDIA,
		(unsigned long)&mm_data);
	if (err)
		LOG_ERR("ion_kernel_ioctl(ION_MM_SET_DEBUG_INFO) returns %d, client %p",
			err, client);

	return 0;
}

static void _ccu_ion_free_handle(struct ion_client *client,
	struct ion_handle *handle)
{
	if (!client) {
		LOG_ERR("invalid ion client!\n");
		return;
	}
	if (!handle)
		return;

	ion_free(client, handle);

	LOG_DBG("free ion handle 0x%p\n", handle);
}

/* 官核: ccu_buffer_handle[2]，步长 0x30，按 meminfo.cached 索引 */
static struct CcuMemHandle ccu_buffer_handle[2];

struct CcuMemInfo *ccu_get_binary_memory(void)
{
	if (ccu_buffer_handle[0].meminfo.va != NULL)
		return &ccu_buffer_handle[0].meminfo;

	LOG_ERR("ccu ddr va not found!\n");
	return NULL;
}

/*
 * 官核 ccu_allocate_mem (0xffffff8008ca79c0):
 *   __ion_alloc(client, size, align=0, heap=0x400,
 *               flags=(cached ? 7 : 4), 0)
 *   -> ion_map_kernel -> _ccu_ion_get_mva(&mva, cached)
 *   -> 存入 ccu_buffer_handle[cached & 1]；不回写用户内存
 *   dump_time(meminfo+0x24) 且 size > 0xa00000(10MB) 时打印分配耗时
 */
int ccu_allocate_mem(struct CcuMemHandle *memHandle, int size, bool cached)
{
	int ret = 0;
	uint32_t idx = cached ? 1 : 0;

	LOG_DBG_MUST("_ccuAllocMem+\n");
	LOG_DBG_MUST("size(%d) cached(%d) memHandle->ionHandleKd(%p)\n",
		size, cached, memHandle->ionHandleKd);

	if (_ccu_ion_client == NULL) {
		LOG_ERR("%s: _ccu_ion_client is null!\n", __func__);
		return -EINVAL;
	}

	if (ccu_buffer_handle[idx].ionHandleKd != NULL) {
		LOG_ERR("idx %d handle %p is not empty\n", idx,
			ccu_buffer_handle[idx].ionHandleKd);
		return -EINVAL;
	}

	memHandle->ionHandleKd = _ccu_ion_alloc(_ccu_ion_client,
		ION_HEAP_MULTIMEDIA_MASK, 0, (size_t)size, cached,
		memHandle->meminfo.ion_log);
	if (!memHandle->ionHandleKd) {
		LOG_ERR("fail to get ion buffer handle (size=0x%lx)\n", size);
		return -1;
	}

	LOG_DBG_MUST("memHandle->ionHandleKd(%p)\n", memHandle->ionHandleKd);

	memHandle->meminfo.size = size;
	memHandle->meminfo.cached = cached;
	memHandle->meminfo.va = (char *)ion_map_kernel(_ccu_ion_client,
		memHandle->ionHandleKd);
	if (memHandle->meminfo.va == NULL) {
		LOG_ERR("fail to get buffer kernel virtual address");
		return -EINVAL;
	}
	LOG_DBG_MUST("memHandle->va(0x%lx)\n",
		(unsigned long)memHandle->meminfo.va);

	ret = _ccu_ion_get_mva(_ccu_ion_client, memHandle->ionHandleKd,
		&memHandle->meminfo.mva, cached);
	if (ret) {
		LOG_ERR("ccu ion_get_mva failed\n");
		return -1;
	}
	LOG_DBG_MUST("memHandle->mva(0x%lx)\n", memHandle->meminfo.mva);

	LOG_DBG_MUST("_ccuAllocMem-\n");

	ccu_buffer_handle[idx] = *memHandle;
	return (memHandle->ionHandleKd != NULL) ? 0 : -1;
}

/*
 * 官核 ccu_deallocate_mem (0xffffff8008ca7d90):
 *   取 ccu_buffer_handle[cached != 0].ionHandleKd
 *   -> ion_unmap_kernel -> ion_free
 *   -> memset 槽位(0x30)；dump_time 且 size > 10MB 打印释放日志
 *   官核不做 __close_fd / shareFd 释放
 */
int ccu_deallocate_mem(struct CcuMemHandle *memHandle)
{
	uint32_t idx = (memHandle->meminfo.cached != 0) ? 1 : 0;

	LOG_DBG_MUST("free idx(%d) mva(0x%x) fd(0x%x) cached(0x%x)\n", idx,
		ccu_buffer_handle[idx].meminfo.mva,
		ccu_buffer_handle[idx].meminfo.shareFd,
		memHandle->meminfo.cached);

	if (_ccu_ion_client == NULL) {
		LOG_ERR("%s: _ccu_ion_client is null!\n", __func__);
		return -EINVAL;
	}
	if (ccu_buffer_handle[idx].ionHandleKd == 0) {
		LOG_ERR("idx %d handle %p is empty\n", idx,
			ccu_buffer_handle[idx].ionHandleKd);
		return -EINVAL;
	}

	ion_unmap_kernel(_ccu_ion_client,
		ccu_buffer_handle[idx].ionHandleKd);
	ion_free(_ccu_ion_client,
		ccu_buffer_handle[idx].ionHandleKd);
	if ((memHandle->meminfo.ion_log) &&
		(memHandle->meminfo.size > ION_LOG_SIZE))
		LOG_INF_MUST("ion free size = %d, caller = CCU\n",
			memHandle->meminfo.size);

	memset(&(ccu_buffer_handle[idx]), 0,
		sizeof(struct CcuMemHandle));

	return 0;
}

void ccu_ion_free_import_handle(struct ion_handle *handle)
{
	if (!_ccu_ion_client) {
		LOG_ERR("invalid ion client!\n");
		return;
	}
	if (handle == NULL) {
		LOG_ERR("invalid ion handle!\n");
		return;
	}

	ion_free(_ccu_ion_client, handle);
}

struct ion_handle *ccu_ion_import_handle(int fd)
{
	struct ion_handle *handle = NULL;

	if (!_ccu_ion_client) {
		LOG_ERR("ccu invalid ion client!\n");
		return handle;
	}
	if (fd == -1) {
		LOG_ERR("ccu invalid ion fd!\n");
		return handle;
	}

	handle = ion_import_dma_buf_fd(_ccu_ion_client, fd);
	LOG_INF_MUST("ccu_ion_import_fd : %d, %s : 0x%p\n",
		fd, __func__, handle);
	if (!(handle)) {
		LOG_ERR("ccu import ion handle failed!\n");
		return NULL;
	}

	return handle;
}
