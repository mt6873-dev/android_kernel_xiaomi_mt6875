Xiaomi Millet on cezanne
=======================

Source and protocol
-------------------

This backport is derived from MiCode/Xiaomi_Kernel_OpenSource,
``dijun-v-oss``, commit ``0b426219a9a24b56c1593ebc84edb8a3f3b897b0``,
``xring-modules/drivers/mihw/millet``. That tree uses Linux 6.6.30.

The protocol is **Millet 1.0**: the source initializes ``millet_v`` to
``VERSION_1_0`` and reports ``1`` through ``/proc/millet/version``.
The presence of ``VERSION_2_0`` in the upstream enum does not enable 2.0.
Netlink protocol 30, type IDs, commands, message sizes and field offsets are
preserved, including the original native-word-size wire format. A client
must use the kernel's word size (64 bits for cezanne).

Supported paths
---------------

* Core Netlink transport, per-channel loopback registration, handshake and
  ``/proc/millet/millet_stat`` counters.
* Binder transactions, replies, pending-reply waits and low asynchronous
  buffer-space warnings for frozen processes.
* UID-based Binder busy/idle queries. The upstream ``millet_binder_switch``
  parameter selects synchronous-transaction-only or legacy worklist checks.
* SIGKILL, SIGTERM, SIGABRT and SIGQUIT notifications for frozen processes.
* One-shot TCP receive notifications for up to 64 registered application
  UIDs, over IPv4 and IPv6. Packets are always accepted.
* Both the legacy freezer and the already-backported cgroup v2 freezer.
  The OEM v1 attachment permission callback is installed at compile time;
  it does not replace the existing freezer migration callback. The v2
  hierarchy and its normal permission checks are unchanged.

The upstream MEM type has no implementation in this release and is not
advertised as an initialized channel. Millet reports wakeup reasons to a
userspace policy service; it does not freeze or thaw applications itself.
Without a registered client, event paths return before allocation, Binder
state inspection or socket-owner inspection. No scheduler or GPU policy is
added by enabling Millet.

4.14 adaptations
----------------

The core and the event modules are built in. Their original object names
are retained for ``/sys/module/millet_*/parameters`` compatibility. The core
initializes first; callbacks are connected directly to native signal,
Binder and allocator call sites. No GKI trace-hook infrastructure or 6.6
Binder structure definitions are imported, and ``task_struct`` and the
vendor module ABI are not changed.

Proc entries use 4.14 ``file_operations``. Frozen-state checks use
``PF_FROZEN`` together with the v1/v2 freezer helpers, rather than the 6.6
``TASK_FROZEN`` state. The Binder query walks native structures while holding
their existing locks; wait notifications snapshot transaction fields under
those locks before sending.

Received messages must fit the actual skb, use a supported initialized
channel, carry the expected protocol IDs, and come from UID 0..1000. Replies
use the transport-authenticated Netlink port ID. Outgoing records are zeroed
and copied field by field; kernel task addresses and padding are not exposed.
Statistics count successful zero-return sends, and reset input is parsed
using the bounded 4.14 userspace conversion API.

Validation
----------

Use ``./build.sh`` with the cezanne defconfig. All ``CONFIG_MILLET_*``
components are enabled by default when ``CONFIG_MILLET=y`` and their
dependencies are present. CFI and LTO are not required.

On a booted image, read ``/proc/millet/version`` and
``/proc/millet/millet_stat``. Check the original userspace client completes
its loopback handshake before testing notifications. A diagnostic client
must not replace an active system client's channel registration. Check v1
and v2 frozen-process events with disposable test processes, and always thaw
and remove those groups afterwards.
