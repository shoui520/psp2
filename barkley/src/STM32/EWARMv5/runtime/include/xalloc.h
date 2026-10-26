/*
 * xalloc.h: the heap of IAR's C library (DLib, Dinkumware's): its free cells in address
 * order, each a size (the cell's, header included) and the next free cell.
 */
#ifndef _XALLOC_H
#define _XALLOC_H

#include <stddef.h>

#define CELL_OFF	8		/* a block's header: the cell's size, and room for _Next */
#define M_MASK		7		/* blocks are 8-byte multiples */
#define SIZE_CELL	8		/* the smallest cell */

typedef struct _Cell {
	size_t _Size;
	struct _Cell *_Next;
} _Cell;

/* the free list, and where malloc left off in it (the _Next of the cell before the one
   it took from, or 0) */
typedef struct {
	_Cell **_Plast;
	_Cell *_Head;
} _Altab;

extern _Altab _Aldata;

_Cell *_GetMemChunk(void);

#endif
