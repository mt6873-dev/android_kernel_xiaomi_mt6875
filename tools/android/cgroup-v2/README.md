# Android cgroup v2 and cached-app freezer

For the newer Android 17 work, see [compatibility scope](android17-compatibility.md)
and [backport status](6x-backport-status.md). The account below describes the
earlier freezer-only implementation and its Android 15 integration.

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

The initial flash was tested with Android 12 / SDK 31. Later checks on
2026-10-16 found Android 13 / SDK 33 and then Android 15 / SDK 35 on the
same phone, still running build #99. These ROMs need separate integration
checks; kernel test results do not establish their directory layout.

On the latest Android 15 installation, `/sys/fs/cgroup` is a real cgroup2
mount, but its UID groups are still flat. Disassembly of that ROM's
`libprocessgroup.so` shows `ConvertUidToPath` directly formatting
`%s/uid_%u`, without a system/app branch. Its library SHA-256 is
`034a4c46af9bf630317ec85e3e027402306632ea1d5522da9e2cc84682964e04`.
The companion patch has not been built into this ROM. The requested
`system/uid_*` and `apps/uid_*` layout therefore remains unimplemented
on the phone, even though the kernel freezer tests pass.

AOSP Android 15 also has the product build option
`PRODUCT_CGROUP_V2_SYS_APP_ISOLATION_ENABLED := true`. This selects
`CGROUP_V2_SYS_APP_ISOLATION` at compilation; it is not a `setprop` or
`device_config` runtime switch. A vendor library lacking that branch needs
a matching source rebuild. The companion patch additionally keeps v1
memory paths flat in this hybrid setup.

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

## MIUI v1 mount conflict

The Android 13 ROM inspected on this phone had
`persist.sys.millet.cgroup1=true`. Its `cgroups_v1.json` selected
`/dev/cg2_bpf` for cgroup2, while `init.milletmonitor.rc` mounted tmpfs
over `/sys/fs/cgroup` and a legacy freezer at `/sys/fs/cgroup/freezer`.
The boot-completed action then created a regular tmpfs file named
`frozen/cgroup.freeze`; the name alone did not make it a freezer interface.
The read-only mount and init-file checks identified this as a ROM setup
conflict, with the same #99 boot partition hash still present.

For a ROM that implements this selector, disabling it before the next boot
selects its v2 setup. Verify the actual mounts after reboot rather than
assuming every MIUI release uses the same property. The subsequent
Android 15 installation had no such property and mounted cgroup2 at
`/sys/fs/cgroup` directly; no change to that property is needed there.

Inspect these as root:

```
getprop ro.build.version.sdk
getprop persist.sys.millet.cgroup1
cat /proc/1/mountinfo | grep -E 'cgroup|cg2_bpf'
cat /sys/fs/cgroup/uid_0/cgroup.events
cat /sys/fs/cgroup/uid_0/cgroup.freeze
```

The v2 root itself has no `cgroup.freeze` or `cgroup.events`. Check a
non-root group. Neither a successful mount nor a functional freezer
changes Android's UID path generation.

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
controller lookup. A full Android system build has not been run.

On 2026-10-16, `./build.sh` completed and build #99 was flashed to the
cezanne boot partition. The running version and full partition SHA-256
were verified after Android completed boot. The image preserves the
previous boot image's ramdisk and official DTB. KernelSU root and the
vendor fpsgo module remained available.

All ten upstream freezer tests passed on the phone: simple, tree,
concurrent fork, mkdir, rmdir, migration, ptrace, stopped, ptraced, and
vfork. The Binder ABI and disposable endpoint freeze/query/thaw tests
also passed. Test groups were removed; the captured kernel log contained
no panic, Oops, BUG, or WARNING. The ROM's automatic cached-app freezer
setting still reports `use_freezer=false`; it was not changed by testing.

After the phone changed to Android 15 / SDK 35, both test binaries were
copied back and run again on the live `/sys/fs/cgroup` cgroup2 mount.
All ten freezer tests and both Binder checks passed again, and no `cg_*`
test groups remained. This confirms the kernel interfaces on that ROM;
it does not validate system/app isolation or automatic app freezing.

Tested boot image SHA-256:
`d30ff1a4f40e1e677ca8716438b8e9a1d5af3c1a89d8cce622dc3c0490a3576c`.
Its code base is `d04cfc3b3b0f` plus the pre-existing local driver/probe
changes used by the previous test kernel; the saved flash manifest also
records the working-tree diff hash. It is not a clean-checkout image.

On the target after a matching boot image and ROM are installed:

```
cat /proc/self/cgroup
cat /sys/fs/cgroup/system/uid_0/cgroup.events
cat /sys/fs/cgroup/system/uid_0/cgroup.freeze
```

Never write 1 to `system/cgroup.freeze` or `system/uid_0/cgroup.freeze`
as a smoke test; those groups contain essential services. Use only
disposable test groups/processes. The runtime tests above validate the
kernel interfaces; they do not validate the unapplied Android companion
patch or replace a system-side hierarchy migration.

## Sources

* [Linux v2 freezer introduction, 76f969e8948d](https://github.com/torvalds/linux/commit/76f969e8948d82e78e1bc4beb6b9465908e74873)
* [Linux 5.4 stable freezer and selftests](https://github.com/gregkh/linux/tree/linux-5.4.y/kernel/cgroup)
* [Binder freezer introduction, 432ff1e91694](https://github.com/torvalds/linux/commit/432ff1e91694e4c55a5bf6bc0574f4c254970232)
* [Linux 5.15 stable Binder drain/race handling](https://github.com/gregkh/linux/blob/linux-5.15.y/drivers/android/binder.c)
* [AOSP Android 15 path generation](https://android.googlesource.com/platform/system/core/+/refs/tags/android-15.0.0_r1/libprocessgroup/task_profiles.cpp)
* [AOSP Android 15 compile-time isolation option](https://android.googlesource.com/platform/system/core/+/refs/tags/android-15.0.0_r1/libprocessgroup/Android.bp)
