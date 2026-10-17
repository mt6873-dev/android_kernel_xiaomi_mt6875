// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "cgroup_util.h"
#include "ksft_compat.h"

static int put(const char *group, const char *file, const char *value)
{
	char path[4096];
	int fd, saved;
	ssize_t n;

	snprintf(path, sizeof(path), "%s/%s", group, file);
	fd = open(path, O_WRONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;
	n = write(fd, value, strlen(value));
	saved = errno;
	close(fd);
	errno = saved;
	return n == (ssize_t)strlen(value) ? 0 : -1;
}

static int await_key(const char *group, const char *file, const char *key, long want)
{
	int i;

	for (i = 0; i < 500; i++) {
		if (cg_read_key_long(group, file, key) == want)
			return 0;
		usleep(10000);
	}
	return -1;
}

static int reap(pid_t pid)
{
	int i, status;
	pid_t ret;

	for (i = 0; i < 500; i++) {
		ret = waitpid(pid, &status, WNOHANG);
		if (ret == pid)
			return WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL ? 0 : -1;
		if (ret < 0)
			return -1;
		usleep(10000);
	}
	return -1;
}

static pid_t sleeper(const char *group, bool fork_children)
{
	int ready[2];
	char ch;
	pid_t pid;

	if (pipe(ready))
		return -1;
	pid = fork();
	if (!pid) {
		int i;

		close(ready[0]);
		if (cg_enter_current(group))
			_exit(2);
		if (write(ready[1], "1", 1) != 1)
			_exit(3);
		close(ready[1]);
		/* At most twelve children; never an unbounded fork storm. */
		if (fork_children)
			for (i = 0; i < 12; i++) {
				pid_t child = fork();

				if (!child)
					break;
				if (child < 0)
					_exit(4);
				usleep(2000);
			}
		for (;;)
			pause();
	}
	close(ready[1]);
	if (pid > 0 && read(ready[0], &ch, 1) != 1) {
		kill(pid, SIGKILL);
		waitpid(pid, NULL, 0);
		pid = -1;
	}
	close(ready[0]);
	return pid;
}

static void cleanup(const char *group, pid_t pid)
{
	/* Only the test's own, newly created group can reach this helper. */
	put(group, "cgroup.freeze", "0");
	put(group, "cgroup.kill", "1");
	if (pid > 0) {
		kill(pid, SIGKILL);
		waitpid(pid, NULL, 0);
	}
	rmdir(group);
}

static int kill_case(const char *base, bool frozen, bool forks)
{
	char *group = cg_name(base, "kill");
	char *leaf = cg_name(group, "child");
	char path[4096];
	int ret = KSFT_FAIL, fd = -1;
	pid_t pid = -1;

	if (!group || !leaf || cg_create(group) || cg_create(leaf))
		goto out;
	pid = sleeper(leaf, forks);
	if (pid < 0 || await_key(group, "cgroup.events", "populated ", 1))
		goto out;
	if (frozen && (put(group, "cgroup.freeze", "1") ||
		      await_key(group, "cgroup.events", "frozen ", 1)))
		goto out;
	snprintf(path, sizeof(path), "%s/cgroup.events", group);
	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		goto out;
	{
		char buf[128];
		struct pollfd pollfd = { .fd = fd, .events = POLLPRI | POLLERR };

		if (pread(fd, buf, sizeof(buf), 0) < 0 ||
		    put(group, "cgroup.kill", "1") ||
		    poll(&pollfd, 1, 5000) <= 0 ||
		    await_key(group, "cgroup.events", "populated ", 0) ||
		    reap(pid))
			goto out;
	}
	pid = -1;
	ret = KSFT_PASS;
out:
	if (fd >= 0)
		close(fd);
	if (leaf)
		cleanup(leaf, pid);
	if (group)
		cleanup(group, -1);
	free(leaf);
	free(group);
	return ret;
}

static int cpu_stats(const char *base)
{
	char *parent = cg_name(base, "accounting");
	char *leaf = cg_name(parent, "child");
	pid_t pid = -1;
	long usage, user, system, ancestor;
	int status, ret = KSFT_FAIL;

	if (!parent || !leaf || cg_create(parent) || cg_create(leaf))
		goto out;
	pid = fork();
	if (!pid) {
		struct timespec start, now;

		if (cg_enter_current(leaf) || clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start))
			_exit(2);
		do {
			unsigned int i;
			volatile unsigned long sum = 0;

			for (i = 0; i < 10000; i++)
				sum += i;
			if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now))
				_exit(3);
		} while ((now.tv_sec - start.tv_sec) * 1000000000LL +
			 now.tv_nsec - start.tv_nsec < 200000000LL);
		_exit(0);
	}
	if (pid < 0 || waitpid(pid, &status, 0) != pid)
		goto out;
	pid = -1;
	if (!WIFEXITED(status) || WEXITSTATUS(status))
		goto out;
	usage = cg_read_key_long(leaf, "cpu.stat", "usage_usec ");
	user = cg_read_key_long(leaf, "cpu.stat", "user_usec ");
	system = cg_read_key_long(leaf, "cpu.stat", "system_usec ");
	ancestor = cg_read_key_long(parent, "cpu.stat", "usage_usec ");
	printf("# CPU usec: leaf=%ld user=%ld system=%ld parent=%ld\n",
	       usage, user, system, ancestor);
	if (usage < 180000 || user < 0 || system < 0 ||
	    labs(usage - user - system) > 20000 || ancestor < usage)
		goto out;
	if (rmdir(leaf) ||
	    cg_read_key_long(parent, "cpu.stat", "usage_usec ") < ancestor)
		goto out;
	ret = KSFT_PASS;
out:
	if (leaf)
		cleanup(leaf, pid);
	if (parent)
		cleanup(parent, -1);
	free(leaf);
	free(parent);
	return ret;
}

static int cpu_control(const char *base)
{
	char available[512];
	char *group = cg_name(base, "bandwidth");
	int ret = KSFT_FAIL, status;
	pid_t pid = -1;

	if (cg_read(base, "cgroup.controllers", available, sizeof(available)))
		goto out;
	if (!strstr(available, "cpu")) {
		printf("# cpu controller unavailable here (may be owned by v1)\n");
		ret = KSFT_SKIP;
		goto out;
	}
	if (put(base, "cgroup.subtree_control", "+cpu") || cg_create(group) ||
	    put(group, "cpu.weight", "200") ||
	    cg_read_long(group, "cpu.weight") != 200 ||
	    !put(group, "cpu.weight", "0") ||
	    !put(group, "cpu.weight", "10001") ||
	    put(group, "cpu.weight.nice", "0") ||
	    cg_read_long(group, "cpu.weight.nice") != 0 ||
	    put(group, "cpu.max", "20000 100000") ||
	    cg_read_strstr(group, "cpu.max", "20000 100000"))
		goto out;
	pid = fork();
	if (!pid) {
		struct timespec start, now;

		if (cg_enter_current(group) || clock_gettime(CLOCK_MONOTONIC, &start))
			_exit(2);
		do {
			volatile unsigned long n;

			for (n = 0; n < 100000; n++)
				;
			if (clock_gettime(CLOCK_MONOTONIC, &now))
				_exit(3);
		} while ((now.tv_sec - start.tv_sec) * 1000000000LL +
			 now.tv_nsec - start.tv_nsec < 500000000LL);
		_exit(0);
	}
	if (pid < 0 || waitpid(pid, &status, 0) != pid)
		goto out;
	pid = -1;
	if (!WIFEXITED(status) || WEXITSTATUS(status) ||
	    cg_read_key_long(group, "cpu.stat", "nr_throttled ") <= 0 ||
	    cg_read_key_long(group, "cpu.stat", "throttled_usec ") <= 0)
		goto out;
	ret = KSFT_PASS;
out:
	if (group)
		cleanup(group, pid);
	free(group);
	return ret;
}

static int memory_charge(const char *group)
{
	int ready[2], done[2], status, ret = -1;
	pid_t pid;
	char ch;

	if (pipe(ready))
		return -1;
	if (pipe(done)) {
		close(ready[0]);
		close(ready[1]);
		return -1;
	}
	pid = fork();
	if (!pid) {
		volatile char *buf;
		size_t i;

		close(ready[0]);
		close(done[1]);
		if (cg_enter_current(group))
			_exit(2);
		buf = malloc(8 * 1024 * 1024);
		if (!buf)
			_exit(3);
		for (i = 0; i < 8 * 1024 * 1024; i += 4096)
			buf[i] = 1;
		if (write(ready[1], "1", 1) != 1 || read(done[0], &ch, 1) != 1)
			_exit(4);
		free((void *)buf);
		_exit(0);
	}
	close(ready[1]);
	close(done[0]);
	if (pid > 0 && read(ready[0], &ch, 1) == 1) {
		long current = cg_read_long(group, "memory.current");
		long peak = cg_read_long(group, "memory.peak");

		printf("# memory bytes: current=%ld peak=%ld\n", current, peak);
		if (current >= 8 * 1024 * 1024 && peak >= current)
			ret = 0;
	}
	/* Close rather than signal a child that may already have exited. */
	if (pid > 0) {
		if (ret)
			kill(pid, SIGKILL);
		else if (write(done[1], "1", 1) != 1)
			ret = -1;
		waitpid(pid, &status, 0);
		if (!WIFEXITED(status) || WEXITSTATUS(status))
			ret = -1;
	}
	close(ready[0]);
	close(done[1]);
	return ret;
}

static int controller_files(const char *base, const char *controller)
{
	char available[512], cpus[512], mems[512];
	char *group = cg_name(base, controller);
	const char *memory_files[] = {
		"memory.current", "memory.min", "memory.low", "memory.high",
		"memory.max", "memory.stat", "memory.events", "memory.events.local",
		"memory.peak", "memory.swap.current", "memory.swap.max"
	};
	int ret = KSFT_FAIL, i;
	bool memory = !strcmp(controller, "memory");

	if (cg_read(base, "cgroup.controllers", available, sizeof(available)))
		goto out;
	if (!strstr(available, controller)) {
		printf("# %s unavailable here (may be owned by a v1 hierarchy)\n", controller);
		ret = KSFT_SKIP;
		goto out;
	}
	if (put(base, "cgroup.subtree_control", memory ? "+memory" : "+cpuset") ||
	    cg_create(group))
		goto out;
	if (memory) {
		char buf[4096];

		for (i = 0; i < (int)ARRAY_SIZE(memory_files); i++)
			if (cg_read(group, memory_files[i], buf, sizeof(buf)))
				goto out;
		if (put(group, "memory.min", "1048576") ||
		    put(group, "memory.low", "2097152") ||
		    put(group, "memory.high", "max") ||
		    put(group, "memory.max", "max") ||
		    cg_read_long(group, "memory.min") != 1048576 ||
		    cg_read_long(group, "memory.low") != 2097152 ||
		    memory_charge(group))
			goto out;
	} else {
		if (cg_read(group, "cpuset.cpus.effective", cpus, sizeof(cpus)) ||
		    cg_read(group, "cpuset.mems.effective", mems, sizeof(mems)) ||
		    !cpus[0] || cpus[0] == '\n' || !mems[0] || mems[0] == '\n' ||
		    put(group, "cpuset.cpus", cpus) || put(group, "cpuset.mems", mems) ||
		    cg_read_strstr(group, "cpuset.cpus.partition", "member"))
			goto out;
	}
	ret = KSFT_PASS;
out:
	if (group)
		rmdir(group);
	free(group);
	return ret;
}

int main(void)
{
	char root[4096], name[128];
	char *base;
	int ret = 0, i, result[7];
	const char *names[] = {
		"kill descendants and populated notification",
		"kill a frozen subtree",
		"kill while a bounded set of children forks",
		"recursive CPU accounting survives child removal",
		"v2 memory boot profile files",
		"v2 cpuset masks and partition interface",
		"v2 CPU weights and actual quota throttling"
	};

	signal(SIGPIPE, SIG_IGN);
	if (cg_find_unified_root(root, sizeof(root)))
		return KSFT_SKIP;
	snprintf(name, sizeof(name), "cg_android17_%d", getpid());
	base = cg_name(root, name);
	if (!base || cg_create(base))
		return KSFT_FAIL;
	/* Essential services remain in their original cgroups throughout. */
	result[0] = kill_case(base, false, false);
	result[1] = kill_case(base, true, false);
	result[2] = kill_case(base, false, true);
	result[3] = cpu_stats(base);
	result[4] = controller_files(base, "memory");
	result[5] = controller_files(base, "cpuset");
	result[6] = cpu_control(base);
	for (i = 0; i < 7; i++) {
		if (result[i] == KSFT_PASS)
			ksft_test_result_pass("%s\n", names[i]);
		else if (result[i] == KSFT_SKIP)
			ksft_test_result_skip("%s\n", names[i]);
		else {
			ksft_test_result_fail("%s (errno %d)\n", names[i], errno);
			ret = 1;
		}
	}
	if (rmdir(base)) {
		perror("test cgroup cleanup");
		ret = 1;
	}
	free(base);
	return ret;
}
