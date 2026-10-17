# Cgroup v2 6.x backport status

Work in progress. Do not describe this branch as a complete 6.6 backport or
as phone-validated.

Reference: MiCode/Xiaomi_Kernel_OpenSource, dada-v-oss, commit
afb5a488a44e7e980b2ef7b4c145703cfcb7aac5 (Linux 6.6.30).
Baseline: de6a05d23e41476f0e90c0ceae024ea4856b3e4f.
Working branch: codex/cgroup-v2-6x. The dev branch is unchanged.

Implemented in the working tree:
* The reference cpuset v2 masks, effective masks, root/isolated partitions,
  invalidation and partition notifications.
* Scheduler-domain and Deadline bandwidth dependencies adapted to 4.14.
* A real nonblocking CPU hotplug read-lock API.
* Retained cpuset filesystem alias and MT6885 legacy mask inheritance.
* cgroup.kill with descendant traversal and concurrent-fork signaling.

Validation: cpuset/core/cpu/scheduler/deadline/topology object compilation
passed with implicit declarations and incompatible pointer types as errors,
including the cgroup.kill addition (cpuset-build-8.log). Full build and phone
tests remain pending.

Work still required:
* CPU v2 control and real CPU accounting/statistics.
* Group IO, writeback and throttling.
* Memory/core interface parity inventory against the pinned reference.
* CLONE_INTO_CGROUP and modern BPF kfunc dependencies require explicit work;
  the first cpuset adaptation retains the inherited-cgroup fork callback.
* Full build.sh build and disposable upstream cgroup selftests on the phone.
* ROM init/libprocessgroup integration for system/uid_* paths.

No flash or remote push has been performed for this branch.
