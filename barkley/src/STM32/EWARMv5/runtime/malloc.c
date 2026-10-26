/*
 * malloc: the first free cell big enough, searched from where the last search left off,
 * then from the head; the heap's one chunk added to the free list when none is. A cell
 * larger than asked is split, its rest left free.
 */

#include <stdlib.h>
#include "xalloc.h"
#include "xmtx.h"

_Altab _Aldata;

/* the link to a free cell of size bytes or more, or 0 */
static _Cell **findmem(size_t size)
{
	_Cell *q, **qb;

	for (;;) {
		if ((qb = _Aldata._Plast) == 0) {
			for (qb = &_Aldata._Head; *qb != 0; qb = &(*qb)->_Next)
				if (size <= (*qb)->_Size)
					return qb;
		} else {
			for (; *qb != 0; qb = &(*qb)->_Next)
				if (size <= (*qb)->_Size)
					return qb;
			q = *_Aldata._Plast;
			for (qb = &_Aldata._Head; *qb != q; qb = &(*qb)->_Next)
				if (size <= (*qb)->_Size)
					return qb;
		}
		if ((q = _GetMemChunk()) == 0)
			return 0;
		free((char *)q + CELL_OFF);
	}
}

void *malloc(size_t size_in)
{
	_Cell *q, **qb;
	size_t size = (size_in + (CELL_OFF + M_MASK)) & ~M_MASK;

	_Locksyslock(_LOCK_MALLOC);
	if (size <= size_in) {
		_Unlocksyslock(_LOCK_MALLOC);
		return 0;
	}
	if (size < SIZE_CELL)
		size = SIZE_CELL;
	if ((qb = findmem(size)) == 0) {
		_Unlocksyslock(_LOCK_MALLOC);
		return 0;
	}
	q = *qb;
	if (q->_Size - SIZE_CELL < size) {
		*qb = q->_Next;
	} else {
		*qb = (_Cell *)((char *)q + size);
		(*qb)->_Next = q->_Next;
		(*qb)->_Size = q->_Size - size;
		q->_Size = size;
	}
	_Aldata._Plast = *qb != 0 && (*qb)->_Next != 0 ? &(*qb)->_Next : 0;
	_Unlocksyslock(_LOCK_MALLOC);
	return (char *)q + CELL_OFF;
}
