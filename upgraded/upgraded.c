/*
 * Copyright (C) 2013 Felix Fietkau <nbd@openwrt.org>
 * Copyright (C) 2013 John Crispin <blogic@openwrt.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define _GNU_SOURCE

#include <sys/reboot.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <libubox/uloop.h>

#include "../watchdog.h"

#ifndef O_PATH
#define O_PATH		010000000
#endif

static struct uloop_process upgrade_proc;
unsigned int debug = 2;

static void upgrade_proc_cb(struct uloop_process *proc, int ret)
{
	if (ret)
		fprintf(stderr, "sysupgrade aborted with return code: %d\n", ret);
	uloop_end();
}

static void sysupgrade(char *path, char *command)
{
	char *args[] = { "/lib/upgrade/stage2", path, command, NULL };

	upgrade_proc.cb = upgrade_proc_cb;
	upgrade_proc.pid = fork();
	if (upgrade_proc.pid < 0) {
		fprintf(stderr, "Failed to fork sysupgrade\n");
		return;
	}

	if (!upgrade_proc.pid) {
		/* Child */
		execvp(args[0], args);
		fprintf(stderr, "Failed to exec sysupgrade\n");
		_exit(EXIT_FAILURE);
	}

	uloop_process_add(&upgrade_proc);
	uloop_run();
}

int main(int argc, char **argv)
{
	pid_t p = getpid();

	if (p != 1) {
		fprintf(stderr, "this tool needs to run as pid 1\n");
		return 1;
	}

	int fd = open("/", O_DIRECTORY|O_PATH);
	if (fd < 0) {
		fprintf(stderr, "unable to open prefix directory: %m\n");
		return 1;
	}

	if (chroot(".") < 0) {
		fprintf(stderr, "failed to chroot: %m\n");
		return 1;
	}

	if (fchdir(fd) == -1) {
		fprintf(stderr, "failed to chdir to prefix directory: %m\n");
		return 1;
	}
	close(fd);

	if (argc != 3) {
		fprintf(stderr, "sysupgrade stage 2 failed, invalid command line\n");
		return 1;
	}

	uloop_init();
	watchdog_init(0);
	sysupgrade(argv[1], argv[2]);

	reboot(RB_AUTOBOOT);

	return 0;
}
