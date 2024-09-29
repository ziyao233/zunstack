// SPDX-License-Identifier: MPL-2.0
/*
 *	zunstack
 *	Unwind stack on SIGSEGV.
 *	Copyright (c) 2024 Yao Zi <ziyao@disroot.org>. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <ucontext.h>

static
void sighandler(int signo, siginfo_t *info, void *ctx)
{
	(void)signo; (void)info; (void)ctx;
	puts("Catch you, SIGSEGV!");

	struct sigaction dfl = { .sa_handler = SIG_DFL };
	sigaction(SIGABRT, &dfl, NULL);
	abort();
}

int
zunstack_init(void)
{
	struct sigaction act = {
					.sa_flags = SA_SIGINFO,
					.sa_sigaction = sighandler
			       };

	int ret = sigaction(SIGSEGV, &act, NULL);
	if (ret < 0)
		return ret;

	return sigaction(SIGABRT, &act, NULL);
}
