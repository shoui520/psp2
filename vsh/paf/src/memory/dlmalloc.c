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

void unlink_large_large_chunk(mstate m, tchunkptr x)
{
	tchunkptr xp = x->parent;
	tchunkptr *h = &m->treebins[x->index];
	tchunkptr r;

	if (x->bk != x) {
		ScePafStdc_D608246A();
		return;
	}
	r = x->child[0];
	if (r) {
		if (x->child[1]) {
			tchunkptr *rp = &x->child[0];
			tchunkptr child;
			tchunkptr c0, c1;

			/* Replace the node with its predecessor in address order. */
			while (r->child[1]) {
				rp = &r->child[1];
				r = *rp;
			}
			child = r->child[0];
			*rp = child;
			if (child)
				child->parent = r->parent;
			if (ok_address(m, r)) {
				c0 = x->child[0];
				c1 = x->child[1];
				r->child[0] = c0;
				if (c0)
					c0->parent = r;
				r->child[1] = c1;
				if (c1)
					c1->parent = r;
			} else {
				ScePafStdc_D608246A();
			}
		}
	} else {
		r = x->child[1];
	}
	if (!xp) {
		ScePafStdc_D608246A();
	} else if (*h == x) {
		*h = r;
		if (!r)
			m->treemap &= ~idx2bit(x->index);
	} else if (!ok_address(m, xp)) {
		ScePafStdc_D608246A();
	} else if (xp->child[0] == x) {
		xp->child[0] = r;
	} else if (xp->child[1] == x) {
		xp->child[1] = r;
	} else {
		ScePafStdc_D608246A();
	}
	if (r) {
		if (ok_address(m, r))
			r->parent = xp;
		else
			ScePafStdc_D608246A();
	}
}

int sys_trim(mstate m, size_t pad)
{
	if (!mparams.magic)
		init_mparams();
	if (pad < MAX_REQUEST) {
		mchunkptr top = m->top;

		if (top) {
			size_t size = m->topsize;
			size_t trim_check = m->trim_check;

			if (size > pad + TOP_FOOT_SIZE) {
				struct malloc_segment *sp = &m->seg;

				while ((uintptr_t)top < (uintptr_t)sp->base ||
				       (uintptr_t)top >= (uintptr_t)sp->base + sp->size) {
					sp = sp->next;
					if (!sp)
						break;
				}
			}
			if (size > trim_check)
				m->trim_check = SIZE_MAX;
		}
	}
	return 0;
}

size_t release_unused_segments(mstate m)
{
	m->release_checks = MAX_RELEASE_CHECK_RATE;
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

static inline void insert_small_chunk(mstate m, mchunkptr p, size_t size)
{
	unsigned int index = small_index(size);
	mchunkptr b = smallbin_at(m, index);
	mchunkptr f = b;

	if (!(m->smallmap & idx2bit(index)))
		m->smallmap |= idx2bit(index);
	else if (ok_address(m, b->fd))
		f = b->fd;
	else
		ScePafStdc_D608246A();
	b->fd = p;
	f->bk = p;
	p->fd = f;
	p->bk = b;
}

static inline void insert_large_chunk(mstate m, tchunkptr x, size_t size)
{
	unsigned int index = NTREEBINS - 1;
	int address_order = size >= m->alignment;
	tchunkptr *h, t;
	size_t k;

	if (!address_order) {
		unsigned int bits = size >> 8;

		if (!bits)
			index = 0;
		else if (bits <= 65535) {
			unsigned int high = 31 - __builtin_clz(bits);

			index = (high << 1) | ((size >> (high + 7)) & 1);
		}
	}
	h = &m->treebins[index];
	x->index = index;
	x->child[0] = NULL;
	x->child[1] = NULL;
	if (!(m->treemap & idx2bit(index))) {
		*h = x;
		m->treemap |= idx2bit(index);
		x->parent = (tchunkptr)h;
		x->fd = x;
		x->bk = x;
		return;
	}
	t = *h;
	k = size << (index == NTREEBINS - 1 ? 0 : 25 - (index >> 1));
	for (;;) {
		tchunkptr *c;

		if (!address_order && chunksize(t) == size) {
			tchunkptr f = t->fd;

			if (ok_address(m, t) && ok_address(m, f)) {
				f->bk = x;
				t->fd = x;
				x->fd = f;
				x->bk = t;
				x->parent = NULL;
			} else {
				ScePafStdc_D608246A();
			}
			return;
		}
		if (address_order)
			c = &t->child[(uintptr_t)x >= (uintptr_t)t];
		else {
			c = &t->child[k >> 31];
			k <<= 1;
		}
		if (*c) {
			t = *c;
		} else {
			if (ok_address(m, c)) {
				*c = x;
				x->parent = t;
				x->fd = x;
				x->bk = x;
			} else {
				ScePafStdc_D608246A();
			}
			return;
		}
	}
}

static inline void unlink_first_small_chunk(mstate m, mchunkptr b,
					    mchunkptr p, unsigned int index)
{
	mchunkptr f = p->fd;

	if (b == f)
		m->smallmap &= ~idx2bit(index);
	else if (ok_address(m, f)) {
		b->fd = f;
		f->bk = b;
	} else {
		ScePafStdc_D608246A();
	}
}

static inline void unlink_small_chunk(mstate m, mchunkptr p, size_t size)
{
	mchunkptr f = p->fd;
	mchunkptr b = p->bk;
	unsigned int index = small_index(size);
	mchunkptr bin = smallbin_at(m, index);

	if (f == b)
		m->smallmap &= ~idx2bit(index);
	else if ((f == bin || ok_address(m, f)) &&
		 (b == bin || ok_address(m, b))) {
		f->bk = b;
		b->fd = f;
	} else {
		ScePafStdc_D608246A();
	}
}

static inline void unlink_large_chunk(mstate m, tchunkptr x)
{
	tchunkptr xp, r;

	if (x->index == NTREEBINS - 1) {
		unlink_large_large_chunk(m, x);
		return;
	}
	xp = x->parent;
	r = x->bk;
	if (r != x) {
		tchunkptr f = x->fd;

		if (ok_address(m, f)) {
			f->bk = r;
			r->fd = f;
		} else {
			ScePafStdc_D608246A();
		}
	} else {
		tchunkptr *rp = &x->child[1];

		r = *rp;
		if (!r) {
			rp = &x->child[0];
			r = *rp;
		}
		if (r) {
			tchunkptr *cp;

			for (;;) {
				cp = &r->child[1];
				if (!*cp) {
					cp = &r->child[0];
					if (!*cp)
						break;
				}
				rp = cp;
				r = *rp;
			}
			if (ok_address(m, rp))
				*rp = NULL;
			else
				ScePafStdc_D608246A();
		}
	}
	if (xp) {
		tchunkptr *h = &m->treebins[x->index];

		if (*h == x) {
			*h = r;
			if (!r)
				m->treemap &= ~idx2bit(x->index);
		} else if (ok_address(m, xp)) {
			if (xp->child[0] == x)
				xp->child[0] = r;
			else
				xp->child[1] = r;
		} else {
			ScePafStdc_D608246A();
		}
		if (r) {
			if (ok_address(m, r)) {
				tchunkptr c0, c1;

				r->parent = xp;
				c0 = x->child[0];
				if (c0) {
					if (ok_address(m, c0)) {
						r->child[0] = c0;
						c0->parent = r;
					} else {
						ScePafStdc_D608246A();
					}
				}
				c1 = x->child[1];
				if (c1) {
					if (ok_address(m, c1)) {
						r->child[1] = c1;
						c1->parent = r;
					} else {
						ScePafStdc_D608246A();
					}
				}
			} else {
				ScePafStdc_D608246A();
			}
		}
	}
}

void mspace_free(mspace heap, void *mem, unsigned int tag)
{
	mstate m = heap;
	mchunkptr p, next;
	char *least_addr;
	size_t head, psize, size;

	(void)tag;
	if (!mem)
		return;
	p = mem2chunk(mem);
	least_addr = m->least_addr;
	if ((uintptr_t)p < (uintptr_t)least_addr)
		goto postaction;
	head = p->head;
	if ((head & INUSE_BITS) == PINUSE_BIT)
		goto postaction;
	psize = head & ~(size_t)7;
	next = chunk_plus_offset(p, psize);
	next->prev_foot = FOOTER_MAGIC;
	head = p->head;
	if (!(head & PINUSE_BIT)) {
		size_t prevsize = p->prev_foot;

		if (!(head & INUSE_BITS))
			goto postaction;
		p = chunk_plus_offset(p, -prevsize);
		psize += prevsize;
		if ((uintptr_t)p < (uintptr_t)least_addr)
			goto postaction;
		if (p != m->dv) {
			if (small_index(prevsize) < NSMALLBINS)
				unlink_small_chunk(m, p, prevsize);
			else
				unlink_large_chunk(m, (tchunkptr)p);
		} else if ((next->head & INUSE_BITS) == INUSE_BITS) {
			m->dvsize = psize;
			next->head &= ~PINUSE_BIT;
			p->head = psize | PINUSE_BIT;
			chunk_plus_offset(p, psize)->prev_foot = psize;
			goto postaction;
		}
	}
	if ((uintptr_t)p >= (uintptr_t)next)
		goto postaction;
	head = next->head;
	if (!(head & PINUSE_BIT))
		goto postaction;
	if (!(head & CINUSE_BIT)) {
		if (next == m->top) {
			size_t tsize = m->topsize + psize;
			mchunkptr dv = m->dv;
			size_t trim_check;

			m->topsize = tsize;
			m->top = p;
			p->head = tsize | PINUSE_BIT;
			trim_check = m->trim_check;
			if (p == dv) {
				m->dv = NULL;
				m->dvsize = 0;
			}
			if (tsize > trim_check)
				sys_trim(m, 0);
			goto postaction;
		}
		if (next == m->dv) {
			size_t dsize = m->dvsize + psize;

			m->dv = p;
			m->dvsize = dsize;
			p->head = dsize | PINUSE_BIT;
			chunk_plus_offset(p, dsize)->prev_foot = dsize;
			goto postaction;
		}
		size = head & ~(size_t)7;
		psize += size;
		if (small_index(size) < NSMALLBINS)
			unlink_small_chunk(m, next, size);
		else
			unlink_large_chunk(m, (tchunkptr)next);
		{
			mchunkptr dv = m->dv;

			p->head = psize | PINUSE_BIT;
			chunk_plus_offset(p, psize)->prev_foot = psize;
			if (p == dv) {
				m->dvsize = psize;
				goto postaction;
			}
		}
	} else {
		next->head = head & ~PINUSE_BIT;
		p->head = psize | PINUSE_BIT;
		chunk_plus_offset(p, psize)->prev_foot = psize;
	}
	if (small_index(psize) < NSMALLBINS) {
		insert_small_chunk(m, p, psize);
	} else {
		insert_large_chunk(m, (tchunkptr)p, psize);
		if (--m->release_checks == 0)
			release_unused_segments(m);
	}

postaction:
	/* PAF does not retain a designated victim larger than this threshold. */
	size = m->dvsize;
	if (size > m->alignment) {
		if (small_index(size) < NSMALLBINS) {
			unsigned int index = small_index(size);
			mchunkptr b = smallbin_at(m, index);
			mchunkptr f = b;

			if (!(m->smallmap & idx2bit(index)))
				m->smallmap |= idx2bit(index);
			else if (ok_address(m, b->fd))
				f = b->fd;
			else
				ScePafStdc_D608246A();
			b->fd = m->dv;
			f->bk = m->dv;
			m->dv->fd = f;
			m->dv->bk = b;
		} else {
			insert_large_chunk(m, (tchunkptr)m->dv, size);
		}
		m->dv = NULL;
		m->dvsize = 0;
	}
}

void *tmalloc_large_large(mstate m, size_t nb, size_t bytes, unsigned int tag)
{
	size_t dvs = m->dvsize;
	tchunkptr v = NULL;
	tchunkptr sentinel, t;
	mchunkptr p, r, next;
	size_t size, rsize;
	size_t padding = (bytes + CHUNK_OVERHEAD) & CHUNK_ALIGN_MASK;

	if (dvs == nb || !(m->treemap & idx2bit(NTREEBINS - 1)))
		return NULL;
	sentinel = (tchunkptr)&m->treebins[NTREEBINS - 1];
	t = m->treebins[NTREEBINS - 1];
	if (t) {
		while (t->child[1])
			t = t->child[1];
	}
	/* Visit chunks in descending address order until one fits. */
	while (t != sentinel) {
		if (chunksize(t) >= nb) {
			v = t;
			break;
		}
		if (t->child[0]) {
			t = t->child[0];
			while (t->child[1])
				t = t->child[1];
		} else {
			tchunkptr parent = t->parent;

			while (parent != sentinel) {
				if (parent->child[1] == t)
					break;
				t = parent;
				parent = t->parent;
			}
			t = parent;
		}
	}
	if (dvs > nb && (uintptr_t)m->dv > (uintptr_t)v) {
		p = m->dv;
		rsize = dvs - nb;
		if (rsize >= MIN_CHUNK_SIZE) {
			m->dvsize = rsize;
			p->head = rsize | PINUSE_BIT;
			r = chunk_plus_offset(p, rsize);
			r->prev_foot = rsize;
			r->head = nb | INUSE_BITS;
			next = chunk_plus_offset(r, nb);
			next->head |= PINUSE_BIT;
			mark_inuse_foot(r, nb, tag, padding);
			return chunk2mem(r);
		}
		m->dvsize = 0;
		p->head = dvs | INUSE_BITS;
		next = chunk_plus_offset(p, dvs);
		size = next->head;
		m->dv = NULL;
		next->head = size | PINUSE_BIT;
		mark_inuse_foot(p, dvs, tag, padding);
		return chunk2mem(p);
	}
	if (!v)
		return NULL;
	if (!ok_address(m, v))
		goto erroraction;
	size = chunksize(v);
	rsize = size - nb;
	unlink_large_chunk(m, v);
	p = (mchunkptr)v;
	if (rsize < MIN_CHUNK_SIZE) {
		p->head = size | INUSE_BITS;
		next = chunk_plus_offset(p, size);
		next->head |= PINUSE_BIT;
		mark_inuse_foot(p, size, tag, padding);
		return chunk2mem(p);
	}
	r = chunk_plus_offset(p, rsize);
	if ((uintptr_t)p >= (uintptr_t)r)
		goto erroraction;
	p->head = rsize | INUSE_BITS;
	r->head |= PINUSE_BIT;
	mark_inuse_foot(p, rsize, tag, rsize & CHUNK_ALIGN_MASK);
	r->head = nb | INUSE_BITS;
	next = chunk_plus_offset(r, nb);
	next->head |= PINUSE_BIT;
	mark_inuse_foot(r, nb, tag, padding);
	mspace_free(m, chunk2mem(p), tag);
	return chunk2mem(r);

erroraction:
	ScePafStdc_D608246A();
	return NULL;
}

void *tmalloc_small(mstate m, size_t nb, size_t bytes, unsigned int tag)
{
	unsigned int leastbit = m->treemap & -m->treemap;
	unsigned int index = leastbit ? 31 - __builtin_clz(leastbit) : 32;
	tchunkptr v = m->treebins[index];
	tchunkptr t = v;
	size_t rsize = chunksize(t) - nb;
	size_t padding = (bytes + CHUNK_OVERHEAD) & CHUNK_ALIGN_MASK;
	mchunkptr p, r;
	size_t size;

	if (index < m->alignment_index) {
		while ((t = t->child[0] ? t->child[0] : t->child[1]) != NULL) {
			size_t trem = chunksize(t) - nb;

			if (trem < rsize) {
				rsize = trem;
				v = t;
			}
		}
	} else {
		t = m->treebins[NTREEBINS - 1];
		if (!t)
			return NULL;
		v = t;
		while ((t = t->child[0] ? t->child[0] : t->child[1]) != NULL) {
			if ((uintptr_t)t < (uintptr_t)v)
				v = t;
		}
		rsize = chunksize(v) - nb;
	}
	if (!ok_address(m, v))
		goto erroraction;
	p = (mchunkptr)v;
	r = chunk_plus_offset(p, nb);
	if ((uintptr_t)p >= (uintptr_t)r)
		goto erroraction;
	unlink_large_chunk(m, v);
	if (rsize < MIN_CHUNK_SIZE) {
		size = nb + rsize;
		p->head = size | INUSE_BITS;
		chunk_plus_offset(p, size)->head |= PINUSE_BIT;
		mark_inuse_foot(p, size, tag, padding);
	} else {
		p->head = nb | INUSE_BITS;
		mark_inuse_foot(p, nb, tag, padding);
		r->head = rsize | PINUSE_BIT;
		chunk_plus_offset(r, rsize)->prev_foot = rsize;
		if (m->dvsize)
			insert_small_chunk(m, m->dv, m->dvsize);
		m->dvsize = rsize;
		m->dv = r;
	}
	/* The tree-allocation path bins a DV at the threshold as well. */
	size = m->dvsize;
	if (size >= m->alignment) {
		if (small_index(size) < NSMALLBINS) {
			unsigned int i = small_index(size);
			mchunkptr b = smallbin_at(m, i);
			mchunkptr f = b;

			if (!(m->smallmap & idx2bit(i)))
				m->smallmap |= idx2bit(i);
			else if (ok_address(m, b->fd))
				f = b->fd;
			else
				ScePafStdc_D608246A();
			b->fd = m->dv;
			f->bk = m->dv;
			m->dv->fd = f;
			m->dv->bk = b;
		} else {
			insert_large_chunk(m, (tchunkptr)m->dv, size);
		}
		m->dv = NULL;
		m->dvsize = 0;
	}
	return chunk2mem(p);

erroraction:
	ScePafStdc_D608246A();
	return NULL;
}

void *mspace_malloc(mspace heap, size_t bytes, unsigned int tag)
{
	mstate m = heap;
	size_t nb = pad_request(bytes);
	size_t padding = (bytes + CHUNK_OVERHEAD) & CHUNK_ALIGN_MASK;
	void *mem;

	if (m->magic != mparams.magic)
		return NULL;
	if (bytes < MAX_REQUEST && nb < (size_t)-255 && nb >= m->alignment) {
		size_t rounded = (nb + 255) & ~(size_t)255;

		/* Large allocations descend from the far end of the top chunk. */
		if (rounded + MIN_CHUNK_SIZE <= m->topsize) {
			mchunkptr oldtop = m->top;
			size_t rsize = m->topsize - MIN_CHUNK_SIZE - rounded;
			mchunkptr top = chunk_plus_offset(oldtop,
							 m->topsize - MIN_CHUNK_SIZE);
			mchunkptr p = chunk_plus_offset(oldtop, rsize);

			m->top = top;
			top->head = MIN_CHUNK_SIZE | PINUSE_BIT;
			m->topsize = MIN_CHUNK_SIZE;
			if (rsize >= MIN_CHUNK_SIZE) {
				oldtop->head = rsize | INUSE_BITS;
				p->head |= PINUSE_BIT;
				mark_inuse_foot(oldtop, rsize, tag,
						 rsize & CHUNK_ALIGN_MASK);
			}
			p->head = rounded | INUSE_BITS;
			top->head |= PINUSE_BIT;
			mark_inuse_foot(p, rounded, tag, padding);
			if (rsize >= MIN_CHUNK_SIZE)
				mspace_free(m, chunk2mem(oldtop), tag);
			return chunk2mem(p);
		}
		if (m->treemap &&
		    (mem = tmalloc_large_large(m, rounded, bytes, tag)) != NULL) {
			size_t size = m->dvsize;

			if (size) {
				if (small_index(size) < NSMALLBINS) {
					unsigned int index = small_index(size);
					mchunkptr b = smallbin_at(m, index);
					mchunkptr f = b;

					if (!(m->smallmap & idx2bit(index)))
						m->smallmap |= idx2bit(index);
					else if (ok_address(m, b->fd))
						f = b->fd;
					else
						ScePafStdc_D608246A();
					b->fd = m->dv;
					f->bk = m->dv;
					m->dv->fd = f;
					m->dv->bk = b;
				} else {
					insert_large_chunk(m, (tchunkptr)m->dv, size);
				}
				m->dv = NULL;
				m->dvsize = 0;
			}
			return mem;
		}
	}
	if (bytes <= MAX_SMALL_REQUEST) {
		unsigned int index;
		unsigned int smallbits;

		nb = request2size(bytes);
		index = small_index(nb);
		smallbits = m->smallmap >> index;
		if (smallbits & 3) {
			mchunkptr b, p, next;
			size_t size;

			index += ~smallbits & 1;
			b = smallbin_at(m, index);
			p = b->fd;
			size = small_index2size(index);
			unlink_first_small_chunk(m, b, p, index);
			p->head = size | INUSE_BITS;
			next = chunk_plus_offset(p, size);
			next->head |= PINUSE_BIT;
			mark_inuse_foot(p, size, tag, padding);
			return chunk2mem(p);
		}
		if (nb > m->dvsize) {
			if (smallbits) {
				unsigned int bit = idx2bit(index) << 1;
				unsigned int leftbits = (smallbits << index) & (bit | -bit);
				unsigned int i = 31 - __builtin_clz(leftbits & -leftbits);
				mchunkptr b = smallbin_at(m, i);
				mchunkptr p = b->fd;
				mchunkptr r = chunk_plus_offset(p, nb);
				size_t rsize = small_index2size(i) - nb;

				unlink_first_small_chunk(m, b, p, i);
				p->head = nb | INUSE_BITS;
				mark_inuse_foot(p, nb, tag, padding);
				r->head = rsize | PINUSE_BIT;
				chunk_plus_offset(r, rsize)->prev_foot = rsize;
				if (m->dvsize)
					insert_small_chunk(m, m->dv, m->dvsize);
				m->dvsize = rsize;
				m->dv = r;
				return chunk2mem(p);
			}
			if (m->treemap &&
			    (mem = tmalloc_small(m, nb, bytes, tag)) != NULL)
				return mem;
		}
	} else if (bytes >= MAX_REQUEST) {
		nb = SIZE_MAX;
	} else if (m->treemap &&
		   (mem = tmalloc_large(m, nb, bytes, tag)) != NULL) {
		return mem;
	}
	if (nb <= m->dvsize) {
		mchunkptr p = m->dv;
		size_t rsize = m->dvsize - nb;

		if (rsize >= MIN_CHUNK_SIZE) {
			mchunkptr r = chunk_plus_offset(p, nb);

			m->dvsize = rsize;
			m->dv = r;
			r->head = rsize | PINUSE_BIT;
			chunk_plus_offset(r, rsize)->prev_foot = rsize;
			p->head = nb | INUSE_BITS;
			mark_inuse_foot(p, nb, tag, padding);
		} else {
			size_t size = m->dvsize;
			mchunkptr next = chunk_plus_offset(p, size);
			size_t head;

			m->dvsize = 0;
			p->head = size | INUSE_BITS;
			head = next->head;
			m->dv = NULL;
			next->head = head | PINUSE_BIT;
			mark_inuse_foot(p, size, tag, padding);
		}
		return chunk2mem(p);
	}
	if (nb < m->topsize) {
		mchunkptr p = m->top;
		mchunkptr r = chunk_plus_offset(p, nb);
		size_t rsize = m->topsize - nb;

		m->topsize = rsize;
		m->top = r;
		r->head = rsize | PINUSE_BIT;
		p->head = nb | INUSE_BITS;
		mark_inuse_foot(p, nb, tag, padding);
		return chunk2mem(p);
	}
	if (!mparams.magic)
		init_mparams();
	return NULL;
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
