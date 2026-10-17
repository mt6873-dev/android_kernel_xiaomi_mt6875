// SPDX-License-Identifier: GPL-2.0
/*
 * Disposable boot-image test entry point, selected with rdinit=.
 * Mount only temporary early-boot filesystems, run bounded cgroup tests,
 * undo the mounts/controller activation, then exec Android's original /init.
 * Offline 4.14 memcg references can prevent a later v1 rebind. Always
 * restore the normal boot image after testing. Never use this wrapper
 * in a production ramdisk.
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int logfd = -1;

static int control(const char *group, const char *file, const char *value)
{
	char path[4096];
	int fd, ret;

	snprintf(path, sizeof(path), "%s/%s", group, file);
	fd = open(path, O_WRONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;
	ret = write(fd, value, strlen(value)) == (ssize_t)strlen(value) ? 0 : -1;
	close(fd);
	return ret;
}

static void clean_group(const char *path)
{
	DIR *dir;
	struct dirent *de;
	char child[4096];

	control(path, "cgroup.freeze", "0");
	control(path, "cgroup.kill", "1");
	dir = opendir(path);
	if (!dir)
		return;
	while ((de = readdir(dir))) {
		struct stat st;

		if (de->d_name[0] == '.')
			continue;
		snprintf(child, sizeof(child), "%s/%s", path, de->d_name);
		if (!lstat(child, &st) && S_ISDIR(st.st_mode))
			clean_group(child);
	}
	closedir(dir);
	rmdir(path);
}

static int run_tests(void)
{
	pid_t pid;
	int status, i;

	pid = fork();
	if (!pid) {
		setpgid(0, 0);
		dup2(logfd, STDOUT_FILENO);
		dup2(logfd, STDERR_FILENO);
		execl("/cgroup17-test", "/cgroup17-test", (char *)NULL);
		_exit(127);
	}
	if (pid < 0)
		return -1;
	setpgid(pid, pid);
	for (i = 0; i < 450; i++) {
		pid_t ret = waitpid(pid, &status, WNOHANG);

		if (ret == pid)
			return WIFEXITED(status) && !WEXITSTATUS(status) ? 0 : -1;
		if (ret < 0)
			break;
		usleep(100000);
	}
	kill(-pid, SIGKILL);
	waitpid(pid, NULL, 0);
	return -1;
}

int main(void)
{
	const char *root = "/sys/fs/cgroup";
	const char *enable = "+cpu +cpuset +memory +io +pids";
	const char *disable = "-cpu -cpuset -memory -io -pids";
	int result = -1, status, i;
	bool proc_mounted = false, sys_mounted = false, dev_mounted = false;
	bool cg_mounted = false;
	pid_t reporter;

	if (getpid() != 1) {
		fprintf(stderr, "This is a disposable ramdisk PID 1 test wrapper.\n");
		return 64;
	}
	mkdir("/dev", 0755);
	mkdir("/proc", 0755);
	mkdir("/sys", 0755);
	dev_mounted = !mount("tmpfs", "/dev", "tmpfs", MS_NOSUID, "mode=0755");
	mknod("/dev/kmsg", S_IFCHR | 0600, makedev(1, 11));
	logfd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
	if (logfd < 0)
		goto done;
	dprintf(logfd, "CGROUP17_EARLY_BEGIN\n");
	proc_mounted = !mount("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL);
	sys_mounted = !mount("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL);
	if (!proc_mounted || !sys_mounted)
		goto done;
	mkdir(root, 0755);
	cg_mounted = !mount("none", root, "cgroup2", MS_NOSUID | MS_NODEV | MS_NOEXEC,
			    "memory_recursiveprot");
	if (!cg_mounted || control(root, "cgroup.subtree_control", enable))
		goto done;
	result = run_tests();
done:
	if (cg_mounted) {
		DIR *dir;
		struct dirent *de;

		/* Remove only groups owned by the test, including timeout leftovers. */
		for (i = 0; i < 20; i++) {
			while (waitpid(-1, &status, WNOHANG) > 0)
				;
			dir = opendir(root);
			if (!dir)
				break;
			while ((de = readdir(dir))) {
				char child[4096];

				if (strncmp(de->d_name, "cg_android17_", 13))
					continue;
				snprintf(child, sizeof(child), "%s/%s", root, de->d_name);
				clean_group(child);
			}
			closedir(dir);
			usleep(10000);
		}
		if (control(root, "cgroup.subtree_control", disable) || umount(root))
			result = -1;
	}
	if (sys_mounted && umount("/sys"))
		result = -1;
	if (proc_mounted && umount("/proc"))
		result = -1;
	/* kmsg stays open for the bounded result reporter. */
	if (dev_mounted && umount2("/dev", MNT_DETACH))
		result = -1;
	if (logfd >= 0) {
		dprintf(logfd, "CGROUP17_EARLY_DONE result=%s\n", result ? "FAIL" : "PASS");
		/*
		 * Repeat only this summary for 90 seconds so noisy vendor boot logs
		 * cannot hide the result before adbd starts. The helper then exits.
		 */
		reporter = fork();
		if (!reporter) {
			for (i = 0; i < 6; i++) {
				sleep(15);
				dprintf(logfd, "CGROUP17_EARLY_RESULT %s\n",
					result ? "FAIL" : "PASS");
			}
			_exit(0);
		}
	}
	execl("/init", "/init", (char *)NULL);
	if (logfd >= 0)
		dprintf(logfd, "CGROUP17 Android init exec failed errno=%d\n", errno);
	for (;;)
		pause();
}
