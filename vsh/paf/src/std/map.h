#ifndef PAF_STD_MAP_H
#define PAF_STD_MAP_H

#include <stddef.h>

/* The tree nodes retain their key/value-specific types at their users. */
typedef struct PafMap {
	void *_Myhead;
	size_t _Mysize;
	unsigned char _allocator;
	unsigned char _compare;
} PafMap;

typedef char PafMapSizeCheck[sizeof(PafMap) == 12 ? 1 : -1];

#endif
