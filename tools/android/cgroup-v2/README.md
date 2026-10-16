# Android cgroup v2 and cached-app freezer

This backport supplies the real cgroup v2 freezer, not a `SIGSTOP` wrapper.
It retains the vendor cgroup v1 controllers used by FPSGO and Android 12
vendor services. It does not require CFI or LTO.

## Kernel support

* `cgroup.freeze` requests asynchronous freeze/thaw on non-root v2 groups.
* `cgroup.events` reports `frozen`, with poll notifications.
* Nested freeze inheritance, newly created children, migration, fork, vfork,
  exit, job-control stops, ptrace and fatal signals follow Linux 5.4 stable.
* The existing v1 `freezer.state` controller is preserved in `legacy_freezer.c`.
  `CONFIG_CGROUP_FREEZER` controls v1 only; v2 freezer is part of `CONFIG_CGROUPS`.
* Binder implements `BINDER_FREEZE`, `BINDER_GET_FROZEN_INFO`, and
  `BR_FROZEN_REPLY`. It drains existing transactions before freezing and
  reports pending synchronous work separately from received transactions.
* The cezanne defconfig explicitly enables cgroups, v1 freezer, and the
  existing PIDs controller. No PID limit is installed automatically.

Memory, PSI and cgroup BPF support were already present. CPU/cpuset/stune
remain on the vendor's v1 hierarchies. This is a hybrid Android setup, not
a conversion of every resource controller to v2. `cgroup.kill` is not added;
Android 15 has a `cgroup.procs`-based fallback.

## Required Android integration for system/uid_* paths

Linux does not assign UIDs to directory names. Android's libprocessgroup
creates UID/PID groups and must agree with init, process-group cleanup,
freezer task profiles, and process-group killing about their paths.
Flashing only the kernel cannot change a flat ROM's `/sys/fs/cgroup/uid_*`
layout into the following layout:

```
/sys/fs/cgroup/
  system/uid_0/pid_<pid>/
  system/uid_1000/pid_<pid>/
  apps/uid_10000/pid_<pid>/
```

`android15-system-core.patch` is a companion patch for **AOSP
android-15.0.0_r1 system/core**, not a kernel patch or a flashable ZIP.
It enables system/app isolation in both early setup and path generation,
and makes empty-group cleanup traverse both subtrees. UID < 10000 goes
under `system`; other UIDs go under `apps`. Legacy v1 memory cgroup paths
are deliberately kept flat, avoiding accidental double `apps` nesting.
ROMs with a materially different libprocessgroup need adaptation rather
than blindly applying or replacing shared libraries.

From the ROM's `system/core` checkout:

```
git apply --check /path/to/android15-system-core.patch
git apply /path/to/android15-system-core.patch
```

Rebuild the ROM's init/libprocessgroup consumers together. The setup library
is linked into init; replacing `libprocessgroup.so` alone is insufficient.
Do not relocate live system services or create symlink aliases for old UID
paths as a substitute for this change.

Use the ROM's existing cgroups/task profiles. The AOSP v2 hierarchy is
mounted at `/sys/fs/cgroup`, with the `freezer` profile controller at `.`;
`FreezerState` names `cgroup.freeze`, and `Frozen`/`Unfrozen` write 1/0.
Do not add `+freezer` to `cgroup.subtree_control`: v2 freezer is a core
feature and does not appear in `cgroup.controllers`.

The companion patch has been apply-checked against the stated AOSP tag;
it has not been integrated into or runtime-tested on the user's ROM.

## Validation

Upstream freezer regressions are in `tools/testing/selftests/cgroup`.
They cover hierarchy propagation, creation/removal, migration, ptrace,
stopped tasks, vfork, and concurrent fork. They must run as root on a
kernel containing this backport; running them under WSL tests WSL's kernel
instead and cannot validate this kernel.

Build just the userspace test with:

```
make -C tools/testing/selftests/cgroup
```

`test_binder_freezer --abi-only` checks the fixed-width ioctl ABI without
opening Binder. Without that option it opens only a disposable endpoint
for the test process and tests freeze/query/thaw; an optional argument
selects a Binder device path. It never freezes another process or cgroup.

Validation performed during this backport: the seven affected ARM64
objects compiled; both userspace test programs compiled; the Binder ABI
test passed; the Android companion patch applied cleanly to its reference
tag; nine boundary cases passed for the patched path helper with mocked
controller lookup. Neither the kernel runtime regressions nor a full
Android system build have been run.

On the target after a matching boot image and ROM are installed:

```
cat /proc/self/cgroup
cat /sys/fs/cgroup/system/uid_0/cgroup.events
cat /sys/fs/cgroup/system/uid_0/cgroup.freeze
```

Never write 1 to `system/cgroup.freeze` or `system/uid_0/cgroup.freeze`
as a smoke test; those groups contain essential services. Use only
disposable test groups/processes. No phone was attached to ADB while this
backport was prepared, so runtime validation is still outstanding.

## Sources

* [Linux v2 freezer introduction, 76f969e8948d](https://github.com/torvalds/linux/commit/76f969e8948d82e78e1bc4beb6b9465908e74873)
* [Linux 5.4 stable freezer and selftests](https://github.com/gregkh/linux/tree/linux-5.4.y/kernel/cgroup)
* [Binder freezer introduction, 432ff1e91694](https://github.com/torvalds/linux/commit/432ff1e91694e4c55a5bf6bc0574f4c254970232)
* [Linux 5.15 stable Binder drain/race handling](https://github.com/gregkh/linux/blob/linux-5.15.y/drivers/android/binder.c)
* [AOSP Android 15 path generation](https://android.googlesource.com/platform/system/core/+/refs/tags/android-15.0.0_r1/libprocessgroup/task_profiles.cpp)
