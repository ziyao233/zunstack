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

#include <elf.h>
#include <link.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>

static int pipeFds[2];
typedef unsigned long int Word;

typedef struct {
	Elf64_Addr addr;
	Elf64_Addr base;
	const char *name;
	const char *dsoName;
} Symbol;

#define to_linear_ptr(dsoBase, addr) ((void *)((Word)(dsoBase) + (Word)(addr)))
#define to_linear_addr(dsoBase, addr) (Elf64_Addr)(to_linear_ptr(dsoBase, addr))

static void *
dl_search_tag(Word dsoBase, const Elf64_Phdr *dynPhdr, Word tag)
{
	Elf64_Dyn *dynamic = to_linear_ptr(dsoBase, dynPhdr->p_vaddr);
	for (size_t i = 0; i < dynPhdr->p_memsz / sizeof(Elf64_Dyn); i++)
		if (dynamic[i].d_tag == tag)
			return (void *)dynamic[i].d_un.d_ptr;
	return NULL;
}

#pragma packed(push, 1)

typedef struct {
	Elf64_Word nb_buckets;
	Elf64_Word nb_chains;
	Elf64_Word data[];
} Elf64_Hash;

#pragma packed(pop)

static Elf64_Sym *
symtab_search(Elf64_Sym *symtab, Elf64_Hash *ht,
	      Elf64_Addr dsoBase, Elf64_Addr addr)
{
	Elf64_Word *buckets = (Elf64_Word *)&ht->data;
	Elf64_Word *chains = (Elf64_Word *)(&ht->data[ht->nb_buckets]);

	for (Elf64_Word j = 0; j < ht->nb_buckets; j++) {
		for (Elf64_Word k = buckets[j];
		     k != STN_UNDEF;
		     k = chains[k]) {
			Elf64_Addr linear = to_linear_addr(dsoBase,
							   symtab[k].st_value);
			if (linear <= addr &&
			    addr < linear + symtab[k].st_size)
				return symtab + k;
		}
	}
	return NULL;
}

static int
dl_do_search(struct dl_phdr_info *info, size_t size, void *data)
{
	Symbol *s = data;
	Elf64_Addr base = info->dlpi_addr;
	if (info->dlpi_addr > s->addr)
		return 0;

	for (unsigned int i = 0; i < info->dlpi_phnum; i++) {
		const Elf64_Phdr *phdr = info->dlpi_phdr + i;
		if (phdr->p_type == PT_DYNAMIC) {
			s->dsoName = info->dlpi_name;

			char *strtab = dl_search_tag(base, phdr, DT_STRTAB);
			Elf64_Sym *symtab = dl_search_tag(base, phdr,
							  DT_SYMTAB);
			Elf64_Hash *ht = dl_search_tag(base, phdr, DT_HASH);
			if (!strtab || !symtab || !ht)
				return 0;

			strtab = to_linear_ptr(base, strtab);
			symtab = to_linear_ptr(base, symtab);
			ht = to_linear_ptr(base, ht);

			Elf64_Sym *sym = symtab_search(symtab, ht,
						       base, s->addr);
			if (!sym)
				return 0;

			s->name = strtab + sym->st_name;
			return 1;
		}
	}

	return 0;
}

static int
dl_lookup_address(Symbol *sym)
{
	return !dl_iterate_phdr(dl_do_search, sym);
}

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
	Symbol s;

	for (int i = 0;
	     is_valid_addr(frame);
	     frame = next_frame(frame, &pc)) {
		printf("Frame #%d (0x%p): PC = 0x%lx\n", i, frame, pc);
		i++;

		s.addr = (Elf64_Addr)pc;
		if (!dl_lookup_address(&s))
			printf("\tsymbol %s in %s\n", s.name, s.dsoName);
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
