# SPDX-License-Identifier: GPL-2.0
CFLAGS += -Wall -g
TEST_GEN_PROGS := test_freezer test_binder_freezer test_android17
include ../lib.mk
$(OUTPUT)/test_freezer: cgroup_util.c
$(OUTPUT)/test_android17: cgroup_util.c
