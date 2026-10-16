// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "ksft_compat.h"
#include "../../../../include/uapi/linux/android/binder.h"

/* The ABI uses fixed-width fields on both 32-bit and 64-bit Android. */
_Static_assert(sizeof(struct binder_freeze_info) == 12, "freeze ABI size");
_Static_assert(sizeof(struct binder_frozen_status_info) == 12, "status ABI size");
_Static_assert(BINDER_FREEZE == 0x400c620e, "freeze ioctl ABI");
_Static_assert(BINDER_GET_FROZEN_INFO == 0xc00c620f, "status ioctl ABI");
_Static_assert(BR_FROZEN_REPLY == 0x7212, "frozen reply ABI");

int main(int argc, char **argv)
{
	const char *device = argc > 1 ? argv[1] : "/dev/binder";
	struct binder_freeze_info freeze = {
		.pid = getpid(),
		.enable = 1,
		.timeout_ms = 1000,
	};
	struct binder_frozen_status_info status = { .pid = getpid() };
	bool frozen = false;
	int fd, ret = KSFT_FAIL;

	ksft_print_header();
	ksft_test_result_pass("fixed-width Binder freezer ABI\n");
	if (argc > 1 && !strcmp(argv[1], "--abi-only"))
		return ksft_exit_pass();

	fd = open(device, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		ksft_test_result_skip("Binder device unavailable: %s\n", strerror(errno));
		ksft_print_cnts();
		return KSFT_SKIP;
	}

	/* Freeze only this disposable test process's Binder endpoint. */
	if (ioctl(fd, BINDER_GET_FROZEN_INFO, &status) < 0)
		goto out;
	if (status.sync_recv || status.async_recv)
		goto out;
	if (ioctl(fd, BINDER_FREEZE, &freeze) < 0)
		goto out;
	frozen = true;
	if (ioctl(fd, BINDER_GET_FROZEN_INFO, &status) < 0)
		goto out;
	if (status.sync_recv || status.async_recv)
		goto out;
	freeze.enable = 0;
	if (ioctl(fd, BINDER_FREEZE, &freeze) < 0)
		goto out;
	frozen = false;
	ksft_test_result_pass("freeze/query/thaw a disposable Binder endpoint\n");
	ret = KSFT_PASS;
out:
	if (frozen) {
		freeze.enable = 0;
		ioctl(fd, BINDER_FREEZE, &freeze);
	}
	close(fd);
	if (ret != KSFT_PASS)
		return ksft_exit_fail_msg("Binder freeze/query/thaw failed\n");
	return ksft_exit_pass();
}
