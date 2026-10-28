#include "bufferedfile.h"
#include "../std/stdlib.h"

void BufferedFileCreateDefaultBuffer(BufferSharedPtr *result)
{
	BufferSharedPtr buffer;
	void *addr;

	result->m_ptr = NULL;
	result->m_ref_counter = NULL;
	addr = sce_paf_memalign(64, 4096);
	if (addr) {
		HeapAllocBufferCreate(&buffer, addr, 4096, NULL);
		BufferSharedPtrAssign(result, &buffer);
		BufferSharedPtrRelease(&buffer);
	}
}
