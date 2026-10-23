/*	$NetBSD: pool.h,v 1.42.2.1 2004/06/22 08:59:04 tron Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg; by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SYS_POOL_H_
#define _SYS_POOL_H_

#ifdef _KERNEL
#define	__POOL_EXPOSE
#endif

#if defined(_KERNEL_OPT)
#include "opt_pool.h"
#endif

#define	PCG_NOBJECTS		16

#define	POOL_PADDR_INVALID	((paddr_t) -1)

#ifdef __POOL_EXPOSE
/*
 * SCE: the psp2 pool has no pages, cache groups or locks. pool_get takes an object from the
 * pool's free list or allocates one from the network heap, pool_put keeps at most 32 objects
 * on the list (net_ps_glue.c). struct pool keeps the fields its functions set, struct pool_cache
 * its pool and object hooks.
 */
struct pool_allocator;

struct SceNetPsPoolFreeItem {
	struct SceNetPsPoolFreeItem *next;
};

struct pool_cache {
	struct pool	*pc_pool;	/* parent pool */
	int		(*pc_ctor)(void *, void *, int);
	void		(*pc_dtor)(void *, void *);
	void		*pc_arg;
};

struct pool {
	unsigned int	pr_size;	/* Size of item */
	unsigned int	pr_align;	/* Requested alignment, must be 2^n */
	unsigned int	pr_itemoffset;	/* Align this offset in item */
	int		pr_flags;	/* r/w flags */
#define	PR_NOWAIT	0x00		/* for symmetry */
#define PR_WAITOK	0x02
#define PR_WANTED	0x04
#define PR_PHINPAGE	0x40
#define PR_LOGGING	0x80
#define PR_LIMITFAIL	0x100	/* even if waiting, fail if we hit limit */
#define PR_RECURSIVE	0x200	/* pool contains pools, for vmstat(8) */
	const char	*pr_wchan;	/* tsleep(9) identifier */
	struct pool_allocator *pr_alloc;/* back-end allocator */

	/* Drain hook. */
	void		(*pr_drain_hook)(void *, int);
	void		*pr_drain_hook_arg;

	int		pr_hardlimit;	/* hard limit to number of allocated
					   items */
	const char	*pr_hardlimit_warning;
	int		pr_hardlimit_ratecap;	/* seconds */
	int		pr_minitems;	/* minimum # of items to keep */
	struct SceNetPsPoolFreeItem *freeList;
	int		freeCount;
};
#endif /* __POOL_EXPOSE */

#ifdef _KERNEL
/*
 * pool_allocator_kmem is the default that all pools get unless
 * otherwise specified.  pool_allocator_nointr is provided for
 * pools that know they will never be accessed in interrupt
 * context.
 */
extern struct pool_allocator pool_allocator_kmem;
extern struct pool_allocator pool_allocator_nointr;

void		pool_init(struct pool *, size_t, u_int, u_int,
		    int, const char *, struct pool_allocator *);
void		pool_destroy(struct pool *);

void		pool_set_drain_hook(struct pool *,
		    void (*)(void *, int), void *);

void		*pool_get(struct pool *, int);
void		pool_put(struct pool *, void *);
int		pool_reclaim(struct pool *);

#ifdef POOL_DIAGNOSTIC
/*
 * These versions do reentrancy checking.
 */
void		*_pool_get(struct pool *, int, const char *, long);
void		_pool_put(struct pool *, void *, const char *, long);
int		_pool_reclaim(struct pool *, const char *, long);
#define		pool_get(h, f)	_pool_get((h), (f), __FILE__, __LINE__)
#define		pool_put(h, v)	_pool_put((h), (v), __FILE__, __LINE__)
#define		pool_reclaim(h)	_pool_reclaim((h), __FILE__, __LINE__)
#endif /* POOL_DIAGNOSTIC */

int		pool_prime(struct pool *, int);
void		pool_setlowat(struct pool *, int);
void		pool_sethiwat(struct pool *, int);
void		pool_sethardlimit(struct pool *, int, const char *, int);
void		pool_drain(void *);

/*
 * Debugging and diagnostic aides.
 */
void		pool_print(struct pool *, const char *);
void		pool_printit(struct pool *, const char *,
		    void (*)(const char *, ...));
int		pool_chk(struct pool *, const char *);

/*
 * Pool cache routines.
 */
void		pool_cache_init(struct pool_cache *, struct pool *,
		    int (*)(void *, void *, int),
		    void (*)(void *, void *),
		    void *);
void		pool_cache_destroy(struct pool_cache *);
void		*pool_cache_get_paddr(struct pool_cache *, int, paddr_t *);
#define		pool_cache_get(pc, f) pool_cache_get_paddr((pc), (f), NULL)
/* SCE: an object goes straight back to its cache's pool (m_free, m_freem) */
#define		pool_cache_put_paddr(pc, o, pa) \
			pool_put(((struct pool_cache *)(pc))->pc_pool, (o))
#define		pool_cache_put(pc, o) pool_cache_put_paddr((pc), (o), \
				          POOL_PADDR_INVALID)
void		pool_cache_destruct_object(struct pool_cache *, void *);
void		pool_cache_invalidate(struct pool_cache *);
#endif /* _KERNEL */

#endif /* _SYS_POOL_H_ */
