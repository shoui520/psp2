#ifndef PAF_MEMORY_DLMALLOC_H
#define PAF_MEMORY_DLMALLOC_H

#include <stddef.h>

typedef void *mspace;

struct mallinfo {
	size_t arena;
	size_t ordblks;
	size_t smblks;
	size_t hblks;
	size_t hblkhd;
	size_t usmblks;
	size_t fsmblks;
	size_t uordblks;
	size_t fordblks;
	size_t keepcost;
};

mspace create_mspace_with_base(void *base, size_t capacity, int locked,
			       size_t alignment);
size_t destroy_mspace(mspace heap);
struct mallinfo mspace_mallinfo(mspace heap);
void *mspace_malloc(mspace heap, size_t bytes, unsigned int tag);
void mspace_free(mspace heap, void *mem, unsigned int tag);
void *mspace_realloc(mspace heap, void *mem, size_t bytes, unsigned int tag);
void *mspace_memalign(mspace heap, size_t alignment, size_t bytes,
			unsigned int tag);

#endif
