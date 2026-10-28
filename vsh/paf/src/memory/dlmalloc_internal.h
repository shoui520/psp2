/* dlmalloc 2.8.4 layouts with PAF's space-local allocation policy. */
#ifndef PAF_MEMORY_DLMALLOC_INTERNAL_H
#define PAF_MEMORY_DLMALLOC_INTERNAL_H

#include "../compiler.h"
#include "dlmalloc.h"
#include <stdint.h>

#define NSMALLBINS 32U
#define NTREEBINS 32U
#define MALLOC_ALIGNMENT 16U
#define CHUNK_ALIGN_MASK (MALLOC_ALIGNMENT - 1)
#define CHUNK_OVERHEAD (2 * sizeof(size_t))
#define PINUSE_BIT 1U
#define CINUSE_BIT 2U
#define INUSE_BITS (PINUSE_BIT | CINUSE_BIT)
#define FENCEPOST_HEAD (INUSE_BITS | sizeof(size_t))
#define USE_NONCONTIGUOUS_BIT 4U
#define EXTERN_BIT 8U
#define MAX_RELEASE_CHECK_RATE SIZE_MAX

struct malloc_chunk {
	size_t prev_foot;
	size_t head;
	struct malloc_chunk *fd;
	struct malloc_chunk *bk;
};

typedef struct malloc_chunk *mchunkptr;

struct malloc_segment {
	char *base;
	size_t size;
	struct malloc_segment *next;
	unsigned int sflags;
};

struct malloc_tree_chunk;

struct malloc_state {
	unsigned int smallmap;
	unsigned int treemap;
	size_t dvsize;
	size_t topsize;
	char *least_addr;
	mchunkptr dv;
	mchunkptr top;
	size_t trim_check;
	size_t release_checks;
	size_t magic;
	mchunkptr smallbins[(NSMALLBINS + 1) * 2];
	struct malloc_tree_chunk *treebins[NTREEBINS];
	size_t footprint;
	size_t max_footprint;
	unsigned int mflags;
	struct malloc_segment seg;
	void *extp;
	size_t exts;
	size_t alignment;
	unsigned int alignment_index;
};

typedef struct malloc_state *mstate;

struct malloc_params {
	volatile size_t magic;
	size_t page_size;
	size_t granularity;
	size_t mmap_threshold;
	size_t trim_threshold;
	unsigned int default_mflags;
};

#define pad_request(req) (((req) + CHUNK_OVERHEAD + CHUNK_ALIGN_MASK) & \
			 ~CHUNK_ALIGN_MASK)
#define align_offset(mem) ((-(uintptr_t)(mem)) & CHUNK_ALIGN_MASK)
#define chunk2mem(p) ((void *)((char *)(p) + 2 * sizeof(size_t)))
#define mem2chunk(mem) ((mchunkptr)((char *)(mem) - 2 * sizeof(size_t)))
#define chunksize(p) ((p)->head & ~(size_t)7)
#define chunk_plus_offset(p, size) ((mchunkptr)((char *)(p) + (size)))
#define TOP_FOOT_SIZE (align_offset(chunk2mem(0)) + \
		      pad_request(sizeof(struct malloc_segment)) + \
		      sizeof(struct malloc_chunk))

_Static_assert(sizeof(struct malloc_state) == 476, "malloc_state size");
_Static_assert(offsetof(struct malloc_state, magic) == 36, "malloc_state magic");
_Static_assert(offsetof(struct malloc_state, footprint) == 432,
	       "malloc_state footprint");
_Static_assert(offsetof(struct malloc_state, seg) == 444, "malloc_state segment");
_Static_assert(offsetof(struct malloc_state, alignment) == 468,
	       "malloc_state alignment");
_Static_assert(pad_request(sizeof(struct malloc_state)) == 496,
	       "malloc_state chunk size");
_Static_assert(TOP_FOOT_SIZE == 56, "malloc_state top foot");

mstate init_user_mstate(char *tbase, size_t tsize, size_t alignment);

#endif
