/* Adapted from the public-domain dlmalloc 2.8.4 in contrib. */
#include "dlmalloc_internal.h"
#include "../std/string.h"
#include <kernel/processmgr.h>

static struct malloc_params mparams;

static inline int init_mparams(void)
{
	if (!mparams.magic) {
		mparams.page_size = 4096;
		mparams.granularity = 65536;
		mparams.mmap_threshold = SIZE_MAX;
		mparams.trim_threshold = 2097152;
		mparams.default_mflags = USE_NONCONTIGUOUS_BIT;
		mparams.magic = ((sceKernelGetProcessTimeLow() ^ 0x55555555U) |
				8U) & ~(size_t)7;
	}
	return 0;
}

mstate init_user_mstate(char *tbase, size_t tsize, size_t alignment)
{
	size_t msize = pad_request(sizeof(struct malloc_state));
	mchunkptr msp = (mchunkptr)(tbase + align_offset(tbase + CHUNK_OVERHEAD));
	mstate m = chunk2mem(msp);
	mchunkptr mn;
	unsigned int index = 0;
	unsigned int flags;
	size_t psize, offset;

	sce_paf_memset(m, 0, msize);
	m->least_addr = tbase;
	m->seg.base = tbase;
	msp->head = msize | INUSE_BITS;
	m->seg.size = tsize;
	m->footprint = tsize;
	m->max_footprint = tsize;
	m->release_checks = MAX_RELEASE_CHECK_RATE;
	m->magic = mparams.magic;
	flags = mparams.default_mflags;
	m->extp = NULL;
	m->exts = 0;
	if (!alignment)
		alignment = 8388608;
	else if (alignment < 1024)
		alignment = 1024;
	else if (alignment > 8388608)
		alignment = 8388608;
	if (alignment & (alignment - 1)) {
		size_t rounded = 2048;

		while (rounded < alignment)
			rounded <<= 1;
		alignment = rounded;
	}
	m->alignment = alignment;
	if (alignment >> 8) {
		unsigned int bits = alignment >> 8;

		index = NTREEBINS - 1;
		if (bits <= 65535) {
			unsigned int high = 31 - __builtin_clz(bits);

			index = (high << 1) | ((alignment >> (high + 7)) & 1);
		}
	}
	m->alignment_index = index;
	m->mflags = flags | USE_NONCONTIGUOUS_BIT;
	for (unsigned int i = 0; i < NSMALLBINS; i++) {
		mchunkptr bin = (mchunkptr)&m->smallbins[i * 2];

		bin->fd = bin;
		bin->bk = bin;
	}
	mn = chunk_plus_offset(mem2chunk(m), chunksize(msp));
	psize = (uintptr_t)tbase + tsize - (uintptr_t)mn;
	offset = align_offset(chunk2mem(mn));
	psize -= offset + TOP_FOOT_SIZE;
	m->topsize = psize;
	m->top = chunk_plus_offset(mn, offset);
	m->top->head = psize | PINUSE_BIT;
	chunk_plus_offset(mn, (uintptr_t)tbase + tsize - (uintptr_t)mn -
			  TOP_FOOT_SIZE)->head = TOP_FOOT_SIZE;
	m->trim_check = mparams.trim_threshold;
	return m;
}

mspace create_mspace_with_base(void *base, size_t capacity, int locked,
			       size_t alignment)
{
	mstate m = NULL;
	size_t overhead = pad_request(sizeof(struct malloc_state)) + TOP_FOOT_SIZE;

	if (!mparams.magic)
		init_mparams();
	if (capacity > overhead && capacity < -(overhead + mparams.page_size)) {
		m = init_user_mstate(base, capacity, alignment);
		m->seg.sflags = EXTERN_BIT;
	}
	return m;
}

size_t destroy_mspace(mspace heap)
{
	(void)mparams.magic;
	return 0;
}

static inline void mark_inuse_foot(mchunkptr p, size_t size, unsigned int tag,
				  size_t padding)
{
	mchunkptr next = chunk_plus_offset(p, size);

	next->prev_foot = tag ^ FOOTER_MAGIC;
	if (padding) {
		sce_paf_memset((char *)next - (MALLOC_ALIGNMENT - padding),
			       0xAF, CHUNK_ALIGN_MASK - padding);
		((unsigned char *)next)[-1] = ~(CHUNK_ALIGN_MASK - padding);
		p->head |= FLAG4_BIT;
	}
}

void *internal_realloc(mstate m, void *oldmem, size_t bytes, unsigned int tag)
{
	mchunkptr oldp, next, newp = NULL;
	void *extra = NULL;
	size_t head, oldsize, nb;

	if (bytes >= MAX_REQUEST)
		return NULL;
	oldp = mem2chunk(oldmem);
	head = oldp->head;
	oldsize = head & ~(size_t)7;
	next = chunk_plus_offset(oldp, oldsize);
	if ((uintptr_t)oldp < (uintptr_t)m->least_addr ||
	    (head & INUSE_BITS) == PINUSE_BIT ||
	    (uintptr_t)oldp >= (uintptr_t)next || !(next->head & PINUSE_BIT))
		return NULL;
	nb = request2size(bytes);
	if (!(head & INUSE_BITS)) {
		if ((nb >> 3) >= NSMALLBINS && oldsize >= nb + sizeof(size_t) &&
		    oldsize - nb <= 2 * mparams.granularity)
			newp = oldp;
	} else if (oldsize >= nb) {
		size_t rsize = oldsize - nb;

		newp = oldp;
		if (rsize >= MIN_CHUNK_SIZE) {
			mchunkptr remainder = chunk_plus_offset(oldp, nb);
			mchunkptr end = chunk_plus_offset(remainder, rsize);

			oldp->head = (oldp->head & PINUSE_BIT) | nb | CINUSE_BIT;
			remainder->head |= PINUSE_BIT;
			mark_inuse_foot(oldp, nb, tag,
					(bytes + CHUNK_OVERHEAD) & CHUNK_ALIGN_MASK);
			remainder->head = rsize | INUSE_BITS;
			end->head |= PINUSE_BIT;
			mark_inuse_foot(remainder, rsize, tag, rsize & CHUNK_ALIGN_MASK);
			extra = chunk2mem(remainder);
		}
	} else if (next == m->top) {
		size_t newsize = oldsize + m->topsize;

		if (newsize > nb) {
			size_t newtopsize = newsize - nb;
			mchunkptr newtop = chunk_plus_offset(oldp, nb);

			oldp->head = (head & PINUSE_BIT) | nb | CINUSE_BIT;
			newtop->head |= PINUSE_BIT;
			mark_inuse_foot(oldp, nb, tag,
					(bytes + CHUNK_OVERHEAD) & CHUNK_ALIGN_MASK);
			newtop->head = newtopsize | PINUSE_BIT;
			m->top = newtop;
			m->topsize = newtopsize;
			newp = oldp;
		}
	}
	if (newp) {
		if (extra)
			mspace_free(m, extra, tag);
		return chunk2mem(newp);
	} else {
		void *newmem = mspace_malloc(m, bytes, tag);

		if (newmem) {
			size_t count = bytes;

			if (count > oldsize - CHUNK_OVERHEAD)
				count = oldsize - CHUNK_OVERHEAD;
			sce_paf_memcpy(newmem, oldmem, count);
			mspace_free(m, oldmem, tag);
		}
		return newmem;
	}
}

void *mspace_realloc(mspace heap, void *mem, size_t bytes, unsigned int tag)
{
	if (!mem)
		return mspace_malloc(heap, bytes, tag);
	return internal_realloc(heap, mem, bytes, tag);
}

struct mallinfo mspace_mallinfo(mspace heap)
{
	mstate m = heap;
	struct mallinfo info = { 0 };
	mchunkptr top;

	(void)mparams.magic;
	if (!mparams.magic)
		init_mparams();
	top = m->top;
	if (top) {
		size_t top_size = m->topsize;
		size_t footprint = m->footprint;
		size_t maximum = m->max_footprint;
		size_t nfree = 1;
		size_t mfree = top_size + TOP_FOOT_SIZE;
		size_t sum = mfree;
		struct malloc_segment *s = &m->seg;

		while (s) {
			uintptr_t base = (uintptr_t)s->base;
			struct malloc_segment *next = s->next;
			uintptr_t address = base + align_offset(base + CHUNK_OVERHEAD);

			while (address >= base && address < base + s->size &&
			       (mchunkptr)address != top) {
				mchunkptr q = (mchunkptr)address;
				size_t head = q->head;
				size_t size;

				if (head == FENCEPOST_HEAD)
					break;
				size = head & ~(size_t)7;
				sum += size;
				if ((head & INUSE_BITS) == PINUSE_BIT) {
					mfree += size;
					nfree++;
				}
				address += size;
			}
			s = next;
		}
		info.arena = sum;
		info.ordblks = nfree;
		info.hblkhd = footprint - sum;
		info.usmblks = maximum;
		info.uordblks = footprint - mfree;
		info.fordblks = mfree;
		info.keepcost = top_size;
	}
	return info;
}
