/* SPDX-License-Identifier: GPL-2.0-only OR BSD-2-Clause */
/*
 * Kernel-facing LZ4 declarations. The vendored upstream entry points are
 * renamed in lz4_deps.h; remove those aliases here and use the kernel API.
 */

#undef LZ4_compress_fast
#undef LZ4_compress_default
#undef LZ4_compress_destSize
#undef LZ4_resetStream
#undef LZ4_loadDict
#undef LZ4_saveDict
#undef LZ4_compress_fast_continue

#undef LZ4_decompress_safe
#undef LZ4_decompress_safe_partial
#undef LZ4_decompress_fast
#undef LZ4_setStreamDecode
#undef LZ4_decompress_safe_continue
#undef LZ4_decompress_fast_continue
#undef LZ4_decompress_safe_usingDict
#undef LZ4_decompress_fast_usingDict

#undef LZ4_compress_HC
#undef LZ4_resetStreamHC
#undef LZ4_loadDictHC
#undef LZ4_compress_HC_continue
#undef LZ4_saveDictHC

#include <linux/lz4.h>
