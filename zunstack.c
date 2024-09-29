// SPDX-License-Identifier: MPL-2.0
/*
 *	zunstack
 *	Unwind stack on SIGSEGV.
 *	Copyright (c) 2024 Yao Zi <ziyao@disroot.org>. All rights reserved.
 */

#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <signal.h>
#include <ucontext.h>


static int pipeFds[2];
typedef unsigned long int Word;

/*
 *	On x86-64, %rbp points to the frame pointer
 */
static Word *
next_frame(Word *frame, Word *pc)
{
	*pc = frame[1];
	return (Word *)*frame;
}

static inline int
is_valid_addr(Word *frame) {
	return write(pipeFds[1], frame, 1) >= 0;
}

static void
unwind_stack(ucontext_t *ctx)
{
	Word *frame = (Word *)ctx->uc_mcontext.gregs[REG_RBP];
	Word pc = ctx->uc_mcontext.gregs[REG_RIP];

	for (int i = 0;
	     is_valid_addr(frame);
	     frame = next_frame(frame, &pc)) {
		printf("Frame #%d (0x%p): PC = 0x%lx\n", i, frame, pc);
		i++;
	}
}

static
void sighandler(int signo, siginfo_t *info, void *pCtx)
{
	(void)signo; (void)info;
	struct sigaction dfl = { .sa_handler = SIG_DFL };
	sigaction(SIGSEGV, &dfl, NULL);
	sigaction(SIGABRT, &dfl, NULL);

	unwind_stack(pCtx);

	abort();
}

int
zunstack_init(void)
{
	struct sigaction act = {
					.sa_flags = SA_SIGINFO,
					.sa_sigaction = sighandler
			       };

	if (pipe(pipeFds) < 0)
		return -1;

	if (sigaction(SIGSEGV, &act, NULL))
		return -1;

	return sigaction(SIGABRT, &act, NULL);
}
