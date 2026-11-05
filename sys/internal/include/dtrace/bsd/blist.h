/*
 * Bitmap block lists (Sony's dtrace/bsd/blist.c, xnu's bsd/dev/dtrace/blist.c
 * from FreeBSD): the ID arenas of DTrace's vmem are built on them.
 */
#pragma once

#include <stdint.h>

typedef int32_t daddr_t;
typedef unsigned int u_daddr_t;

#define SWAPBLK_NONE	((daddr_t)-1)

/*
 * xnu's blist.h, with LOG2(16) and LOG2(32) as its LOG2() macro computes them.
 * Sony's BLIST_BMAP_RADIX is an int (xnu's is sizeof(u_daddr_t) * 8): its
 * blst_leaf_alloc compares count with it signed and knows 32 - count is not
 * negative.
 */
#define BLIST_META_RADIX	16
#define BLIST_META_RADIX_SHIFT	4
#define BLIST_BMAP_RADIX	32
#define BLIST_BMAP_RADIX_SHIFT	5

#define BLIST_MAX_ALLOC		BLIST_BMAP_RADIX

typedef struct blmeta {
	union {
		daddr_t bmu_avail;	/* space available under us */
		u_daddr_t bmu_bitmap;	/* bitmap if we are a leaf */
	} u;
	daddr_t bm_bighint;		/* biggest contiguous block hint */
} blmeta_t;

typedef struct blist {
	daddr_t bl_blocks;		/* area of coverage */
	daddr_t bl_radix;		/* coverage radix */
	daddr_t bl_skip;		/* starting skip */
	daddr_t bl_free;		/* number of free blocks */
	blmeta_t *bl_root;		/* root of radix tree */
	daddr_t bl_rootblks;		/* daddr_t blks allocated for tree */
} *blist_t;

blist_t blist_create(daddr_t blocks);
void blist_destroy(blist_t blist);
daddr_t blist_alloc(blist_t blist, daddr_t count);
void blist_free(blist_t blist, daddr_t blkno, daddr_t count);
void blist_resize(blist_t *pblist, daddr_t count, int freenew);
