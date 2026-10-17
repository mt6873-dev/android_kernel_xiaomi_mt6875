# Cgroup v2 backport status

Scope: Android 17 cgroup boot compatibility, adapted to the MT6885 4.14 tree.
See [compatibility scope](android17-compatibility.md) for pinned upstream
callers, and [target results](android17-test-results.md) for image hashes,
passed tests and limitations. This is not a complete Linux 6.6 backport.

Kernel donor: MiCode dada-v-oss, afb5a488a44e7e980b2ef7b4c145703cfcb7aac5,
Linux 6.6.30. Baseline: de6a05d23e41476f0e90c0ceae024ea4856b3e4f.

Implemented:
* Cpuset v2 masks/effective masks and root/isolated partition management;
  scheduler/Deadline dependencies and nonblocking hotplug read lock.
* MT6885 legacy mask inheritance and scheduling boost hooks retained.
* cgroup.kill and concurrent-fork signaling.
* Core recursive CPU accounting, cpu.weight/nice, cpu.max and throttle stats.
* Block group/CFQ/BFQ, writeback and device-throttling configurations.
* Hierarchical memory.min/low protection, memory_recursiveprot, peak and
  local events, including v1 non-hierarchical and offline lifetime handling.
* Existing v2/Binder freezer retained.
* Explicit ARM64 CMDLINE_APPEND for printk.devkmsg=on after bootloader args.

Validation: focused object builds, full ./build.sh build, seven early-boot
v2 controller tests, ten freezer regressions, Binder ABI/endpoint tests and
normal #103 phone boot passed. The early image is disposable and normal
vendor mounts were verified again after restoring the normal image.

Not included: CPU idle/burst/v2 uclamp, iocost/ioprio, CLONE_INTO_CGROUP,
modern BPF kfuncs or per-controller CSS rstat callback nodes. The rstat
adaptation provides core CPU statistics and preserves the existing CSS
layout. Actual Android 17 ROM boot and CTS/VTS have not been tested.
