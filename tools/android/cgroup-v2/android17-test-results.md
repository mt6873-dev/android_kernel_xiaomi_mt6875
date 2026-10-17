# Android 17 cgroup compatibility: target results

Date: 2026-10-17. Target: Xiaomi cezanne / MT6885.
Installed ROM: Android 15, SDK 35, HyperOS 3.0.1. An Android 17 ROM was not
available, so Android 17 system boot is not claimed.

## Kernel and image

* Built through ./build.sh with clang/LLD 13; successful full link, image
  and five modules. The initial link caught the missing MT6885 cpuset
  boost hooks; these were restored before the successful build.
* Kernel source: 7ae5f67555965a7b86c75621be906d68bbddfbf5.
* Running kernel: 4.14.336-codex90-no-fpsbridge #103.
* Normal boot SHA-256:
  8623f1f3b09c3cedb877dc0ce20d449e8a78c5c8588904a4e520562ed73a3092.
* Rollback (#101) boot SHA-256:
  d41438e4f2cabb9c8c7683632d55bff321d6f0d9412b3f11779ed40e12a392b6.
* Normal image retains the original ramdisk and official DTB. AVB hash
  footer verification passed. The actual boot partition hash matched.
* The pre-existing untracked local Mali r32p1 sources were used by the
  build. They were not added or modified by this backport; their aggregate
  hash is retained in the artifact manifest. A Git commit alone does not
  capture that pre-existing build input.

Artifacts: C:\Users\Administor\Downloads\cezanne-cgroup6x-20261017.
The source/tests/docs commit after the kernel commit does not alter the
compiled kernel.

## Cmdline

The original ARM64 CMDLINE_EXTEND mode puts built-in arguments first.
An explicit CMDLINE_APPEND choice was added and selected, with
CONFIG_CMDLINE="printk.devkmsg=on".

Both tested images preserved the bootloader arguments and ended
/proc/cmdline with printk.devkmsg=on. The running
/proc/sys/kernel/printk_devkmsg value was on. The normal image contains no
rdinit test-wrapper argument.

## Full-controller test boot

The disposable early-test image mounted cgroup2 with memory_recursiveprot
and activated cpu, cpuset, memory, io and pids before Android init.
Seven tests passed, with no skips:

1. Kill descendants and receive populated notification.
2. Kill a frozen subtree.
3. Kill a bounded set of concurrently forking children.
4. Recursive CPU accounting remains after child cgroup removal.
5. Memory interfaces/tunables and real 8 MiB anonymous charging/peak.
6. Cpuset requested/effective masks and partition interface.
7. CPU weight conversion/rejection and actual CFS quota throttling.

Observed CPU accounting: leaf/parent 200087 usec; user 188082 and system
12005 usec. Memory current/peak were both 8519680 bytes.
The original result lines and CGROUP17_EARLY_DONE result=PASS are saved in
early-adb-dmesg.txt and early-selftest-excerpt.txt.

The early-test boot completed Android startup, but legacy memory did not
rebind to /dev/memcg after the temporary v2 exercise. This test is solely
for kernel interface verification; it is not a normal vendor boot image.
The normal image was subsequently flashed and its mounts verified
independently. Do not distribute the disposable image for daily use.

## Normal image and regressions

* Android boot_completed=1, KernelSU root, system_server and SurfaceFlinger.
* Vendor fpsgo and sla modules loaded.
* v1 CPU, cpuset, memory, schedtune and blkio mounted; v2 cgroup2 and the
  legacy freezer mount coexist.
* memory_recursiveprot and cpuset_v2_mode are present in mount options.
* The normal-image cgroup test passed its four controller-independent
  cases. The three controller cases correctly skipped because the
  corresponding controllers belong to v1; they had all passed in the
  separate early test.
* Freezer regression suite: 10/10 passed, including migration, job-control
  stops, ptrace, fork/vfork and nested groups.
* Binder fixed-width ABI check and disposable endpoint freeze/query/thaw:
  passed.
* No temporary cg_android17_*, cg_test_* or cg_forkbomb groups remained.

The first freezer regression invocation exposed harness problems: a renamed
binary failed the hard-coded test_freezer comm-name check, and the inotify
helper did not consume events or stop after reaching the desired state.
Use the original basename. The helper was corrected to consume events,
honor timeouts, inspect current state before waiting and stop on success;
the same ten behavioral assertions then passed. No kernel freezer behavior
was weakened to obtain this result.

No panic, Oops or BUG appeared in the captured new boot logs. The existing
flush_work NULL-handler warning during apexd device-mapper cleanup appeared
38 times, matching the saved #101 boot's 38 occurrences. It is not fixed by
this cgroup change. The ROM's recurring gpuservice failure also requires
separate work; recurring service activity was already visible in the
pre-change baseline.

## Remaining limits

The phone still uses the Android 15 flat uid_* layout. Android 17's pinned
init/libprocessgroup sources create system/uid_* and apps/uid_* themselves;
a kernel flash cannot change this ROM's path-generation code.
See android17-compatibility.md for exact upstream references and excluded
features. These results validate the tested cgroup interfaces and current
ROM startup, not every Android 17 kernel requirement or CTS/VTS.
