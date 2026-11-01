/*
 * The buffered file a resource is read through (names ours): ResourceObj's files share them.
 */
#ifndef PAF_RESOURCE_RESOURCE_FILE_H
#define PAF_RESOURCE_RESOURCE_FILE_H

#include <stddef.h>
#include <stdint.h>
#include <buffer/buffer.h>
#include <common/shared_ptr.h>
#include <file/bufferedfile.h>
#include <memory/heap_allocator.h>
#include <std/stdlib.h>

namespace paf {
namespace resource {

/* a buffer on the allocator, else the paf heap (name ours) */
static inline __attribute__((always_inline)) common::SharedPtr<Buffer>
CreateBuffer(size_t size, memory::HeapAllocator *const &allocator)
{
	common::SharedPtr<Buffer> buffer;
	void *addr;

	if (allocator != NULL)
		addr = allocator->sce_paf_memalign(64, size);
	else
		addr = sce_paf_memalign(64, size);
	if (addr != NULL) {
		if (allocator != NULL)
			buffer = HeapAllocBuffer::Create(addr, size, allocator);
		else
			buffer = HeapAllocBuffer::Create(addr, size, NULL);
	}
	return buffer;
}

/* the file opened through a buffer of its own (name ours) */
static inline __attribute__((always_inline)) common::SharedPtr<BufferedFile>
OpenBufferedFile(common::SharedPtr<File> file, int32_t *error, common::SharedPtr<Buffer> buffer)
{
	return BufferedFile::Open(file, error, buffer);
}

}
}

#endif
