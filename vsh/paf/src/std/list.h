#ifndef PAF_STD_LIST_H
#define PAF_STD_LIST_H

#include <stdint.h>

typedef struct PafList {
	void *root_node;
	int32_t count;
	unsigned char alloc_value;
	unsigned char alloc_node;
} PafList;

typedef char PafListSizeCheck[sizeof(PafList) == 12 ? 1 : -1];

#endif
