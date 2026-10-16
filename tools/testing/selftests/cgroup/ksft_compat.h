/* SPDX-License-Identifier: GPL-2.0 */
#include "../kselftest.h"

/* This tree's old harness aliases skip to pass; keep the v2 suite distinct. */
#if KSFT_SKIP == KSFT_PASS
#undef KSFT_SKIP
#define KSFT_SKIP 4
#endif
