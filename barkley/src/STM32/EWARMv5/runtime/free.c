/*
 * free: the block's cell back into the free list, in address order, merged with the
 * free cells it touches; a pointer whose header is not a cell's, or a cell that overlaps
 * a free one, is left alone.
 */

#include <stdlib.h>
#include "xalloc.h"
#include "xmtx.h"

void free(void *ptr)
{
	_Cell *q, *qp;

	if (ptr == 0)
		return;
	q = (_Cell *)((char *)ptr - CELL_OFF);
	if (q->_Size < SIZE_CELL || (q->_Size & M_MASK) != 0)
		return;
	_Locksyslock(_LOCK_MALLOC);
	if (_Aldata._Head == 0 || q < _Aldata._Head) {
		q->_Next = _Aldata._Head;
		_Aldata._Head = q;
	} else {
		for (qp = _Aldata._Head; qp->_Next != 0 && qp->_Next < q; qp = qp->_Next)
			;
		if (q < (_Cell *)((char *)qp + qp->_Size)) {
			_Unlocksyslock(_LOCK_MALLOC);
			return;
		}
		if ((_Cell *)((char *)qp + qp->_Size) == q) {
			qp->_Size += q->_Size;
			q = qp;
		} else if (qp->_Next != 0 && qp->_Next < (_Cell *)((char *)q + q->_Size)) {
			_Unlocksyslock(_LOCK_MALLOC);
			return;
		} else {
			q->_Next = qp->_Next;
			qp->_Next = q;
		}
	}
	if (q->_Next != 0 && (_Cell *)((char *)q + q->_Size) == q->_Next) {
		_Aldata._Plast = 0;
		q->_Size += q->_Next->_Size;
		q->_Next = q->_Next->_Next;
	}
	_Unlocksyslock(_LOCK_MALLOC);
}
