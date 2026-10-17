# Android 17 cgroup compatibility

Scope: supply the kernel interfaces used by Android 17 init and
libprocessgroup, while retaining the existing MT6885 vendor v1 hierarchies.
This is not a complete Linux 6.6 subsystem replacement or an Android 17
boot certification.

## Fixed references

* AOSP system/core android17-release:
  [545d2487e38192a2ce25040897ced877cf6b4f53](https://android.googlesource.com/platform/system/core/+/545d2487e38192a2ce25040897ced877cf6b4f53/).
* AOSP frameworks/base android17-release:
  [94b4c163b7dfe5ce3607f7bb8456f9573f7de57d](https://android.googlesource.com/platform/frameworks/base/+/94b4c163b7dfe5ce3607f7bb8456f9573f7de57d/).
* Kernel donor: MiCode/Xiaomi_Kernel_OpenSource dada-v-oss,
  [afb5a488a44e7e980b2ef7b4c145703cfcb7aac5](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/afb5a488a44e7e980b2ef7b4c145703cfcb7aac5),
  Linux 6.6.30. APIs are adapted to the 4.14 scheduler, memory and cgroup core.

## Android-side contracts

| Android 17 caller/configuration | Kernel support |
| --- | --- |
| Default cgroups.json mounts v1 blkio, cpu, cpuset | Existing v1 interfaces retained; enable block groups, CFQ groups, throttling and writeback |
| init mounts cpuset with noprefix,cpuset_v2_mode | Existing parser retained; vendor CPU-mask inheritance retained |
| init first attempts v2 memory_recursiveprot, then falls back without it | Implement real hierarchical min/low protection and the mount flag |
| init creates apps and system, then activates memory to UID/PID depth | v2 hierarchy, subtree_control, process migration and memory controller |
| FreezerState writes cgroup.freeze; process-group removal polls populated | Existing v2 freezer, frozen/populated notifications and Binder freeze remain |
| processgroup.cpp uses cgroup.kill when available | Recursive process killing with concurrent-fork handling |
| memory task profiles use memory.events,stat,high,max,low,swap.max | Existing memory interfaces retained; add min, peak and events.local |
| CPU/cpuset may be moved to v2 by a product configuration | v2 CPU weights/quota/statistics and cpuset masks/partition interface available |

See the fixed AOSP
[cgroups.json](https://android.googlesource.com/platform/system/core/+/545d2487e38192a2ce25040897ced877cf6b4f53/libprocessgroup/profiles/cgroups.json),
[setup](https://android.googlesource.com/platform/system/core/+/545d2487e38192a2ce25040897ced877cf6b4f53/libprocessgroup/setup/cgroup_map_write.cpp),
[profiles](https://android.googlesource.com/platform/system/core/+/545d2487e38192a2ce25040897ced877cf6b4f53/libprocessgroup/profiles/task_profiles.json)
and [process groups](https://android.googlesource.com/platform/system/core/+/545d2487e38192a2ce25040897ced877cf6b4f53/libprocessgroup/processgroup.cpp).

Default Android 17 still uses v1 CPU/cpuset/blkio. Its memory v2 descriptor
is marked Optional. cgroup.kill has a cgroup.procs fallback, and setup
retries a mount without memory_recursiveprot. These are distinct from
unconditional boot dependencies. The extra kernel support removes those
fallbacks where the ROM selects the corresponding controller.

The Android 17
[path helper](https://android.googlesource.com/platform/system/core/+/545d2487e38192a2ce25040897ced877cf6b4f53/libprocessgroup/task_profiles.cpp)
uses system/uid_* for UIDs below 10000 and apps/uid_* for application UIDs.
Setup creates both subhierarchies unconditionally in this reference.
No Android 17 patch is needed for this layout in the pinned source.
A kernel alone cannot change an Android 15 library that still formats
flat uid_* paths. The Android 15 companion patch remains separate.

## Deliberately excluded from this scope

* Modern BTF kfuncs, CLONE_INTO_CGROUP, CPU idle groups/burst/v2 uclamp,
  iocost and ioprio policies are not backported.
* memory.reclaim (including swappiness=), memory.oom.group and other
  interfaces outside the boot profiles are not added. Android's Compact
  action probes memory.reclaim before using it; its presence is not a
  prerequisite for the pinned init's cgroup setup.
* The existing vendor SCHED_TUNE arrangement remains. No CFI/LTO changes.
* Controller ownership is global: a controller mounted by the vendor in
  v1 cannot also be activated in v2. The production boot image does not
  relocate vendor services or change ROM cgroups.json.

## Validation

Use ./build.sh for the kernel build. Focused compile logs and full-image
build/flash manifests are retained in the dated Downloads artifact folder.

test_android17 in tools/testing/selftests/cgroup exercises descendant
killing, frozen-subtree killing, bounded concurrent forks, populated poll
notifications, recursive CPU accounting after removal, and (when available)
memory charging/peak/tunables, cpuset effective masks, CPU weights and real
quota throttling. Every group is underneath a unique cg_android17_<pid>
test directory; essential services are never migrated or frozen.

On a hybrid Android boot, memory/cpuset/CPU tests report SKIP when the
controllers are owned by v1. Such a skip is not a pass for those controllers.
The existing ten freezer regressions and Binder ABI/freeze checks should
also run on the target kernel.

early-selftest-init.c is an optional, disposable ramdisk PID 1 wrapper.
Selected with rdinit=/cgroup17-early-init, it mounts temporary proc/sys/dev
filesystems and cgroup2 with memory_recursiveprot, activates CPU/cpuset/
memory/IO/PIDs before Android takes ownership, runs test_android17 with a
45-second deadline, removes its groups, disables the controllers, undoes
the mounts and executes the original /init. A short-lived helper repeats
the result in kmsg for 90 seconds to survive vendor log traffic. It does
not change a production ramdisk. A 4.14 memory controller can retain offline
references and fail a later v1 rebind after this exercise. The normal image
must be restored and vendor mounts checked independently after this test.

Runtime results are recorded separately. An Android 15 successful boot and
kernel interface tests cannot establish that an Android 17 ROM will boot:
that requires the actual ROM, its vendor overrides, task profiles and SELinux
policy. This document establishes the cgroup scope, not unrelated Android
17 kernel/syscall/driver compatibility.
