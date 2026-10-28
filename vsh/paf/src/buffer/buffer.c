#include "buffer.h"
#include "../std/stdlib.h"

typedef struct BufferClass {
	int offset_to_top;
	const void *type_info;
	BufferVTable methods;
} BufferClass;

static const BufferClass bufferClass = {
	0, NULL, {
		BufferGetAddr, BufferGetConstAddr, BufferGetSize,
		BufferDestroy, BufferDelete
	}
};

static const BufferClass heapAllocBufferClass = {
	0, NULL, {
		BufferGetAddr, BufferGetConstAddr, BufferGetSize,
		HeapAllocBufferDestroy, HeapAllocBufferDelete
	}
};

static inline __attribute__((always_inline))
void HeapAllocBufferInitialize(HeapAllocBuffer *self, void *addr, size_t size,
			       HeapAllocator *allocator)
{
	self->base.m_size = 0;
	self->base.m_addr = NULL;
	self->base.vtable = &heapAllocBufferClass.methods;
	self->base.m_addr = addr;
	self->m_allocator = allocator;
	self->base.m_size = size;
}

static inline __attribute__((always_inline))
void HeapAllocBufferShare(BufferSharedPtr *result, HeapAllocBuffer *buffer)
{
	BufferRefCounter *counter;

	result->m_ptr = &buffer->base;
	counter = ScePafStdc_FC5CD359(sizeof(*counter));
	if (counter) {
		result->m_ref_counter = counter;
		counter->m_ptr = &buffer->base;
		counter = result->m_ref_counter;
		counter->m_counter = 1;
		counter->m_weak_counter = 1;
		counter->m_deleter = BufferDefaultDelete;
	} else {
		/* Counter allocation failure leaves the allocated buffer alive. */
		result->m_ptr = NULL;
		result->m_ref_counter = NULL;
	}
}

void HeapAllocBufferAllocate(BufferSharedPtr *result, size_t size,
			     HeapAllocator *allocator)
{
	HeapAllocBuffer *buffer;
	void *addr;

	if (allocator) {
		if (size < 64)
			addr = scePafMallocWithContext(allocator, size);
		else
			addr = scePafMallocAlignWithContext(allocator, 64, size);
	} else {
		if (size < 64)
			addr = sce_paf_malloc(size);
		else
			addr = sce_paf_memalign(64, size);
	}
	if (!addr) {
		result->m_ptr = NULL;
		result->m_ref_counter = NULL;
		return;
	}
	buffer = ScePafStdc_FC5CD359(sizeof(*buffer));
	if (buffer) {
		HeapAllocBufferInitialize(buffer, addr, size, allocator);
		HeapAllocBufferShare(result, buffer);
	} else {
		if (allocator)
			scePafFreeWithContext(allocator, addr);
		else
			sce_paf_free(addr);
		result->m_ptr = NULL;
		result->m_ref_counter = NULL;
	}
}

void HeapAllocBufferCreate(BufferSharedPtr *result, void *addr, size_t size,
			   HeapAllocator *allocator)
{
	HeapAllocBuffer *buffer = ScePafStdc_FC5CD359(sizeof(*buffer));

	if (buffer) {
		HeapAllocBufferInitialize(buffer, addr, size, allocator);
		HeapAllocBufferShare(result, buffer);
	} else {
		result->m_ptr = NULL;
		result->m_ref_counter = NULL;
	}
}

Buffer *HeapAllocBufferDestroy(Buffer *base)
{
	HeapAllocBuffer *self = (HeapAllocBuffer *)base;
	HeapAllocator *allocator = self->m_allocator;
	void *addr = base->m_addr;

	base->vtable = &heapAllocBufferClass.methods;
	if (allocator)
		scePafFreeWithContext(allocator, addr);
	else
		sce_paf_free(addr);
	base->vtable = &bufferClass.methods;
	return base;
}

void HeapAllocBufferDelete(Buffer *base)
{
	HeapAllocBufferDestroy(base);
	ScePafStdc_1B77082E(base);
}

Buffer *HeapAllocBufferDestroyBase(Buffer *base)
{
	return HeapAllocBufferDestroy(base);
}

void *BufferGetAddr(Buffer *self)
{
	return self->m_addr;
}

const void *BufferGetConstAddr(const Buffer *self)
{
	return self->m_addr;
}

size_t BufferGetSize(const Buffer *self)
{
	return self->m_size;
}

Buffer *BufferDestroy(Buffer *self)
{
	self->vtable = &bufferClass.methods;
	return self;
}

void BufferDelete(Buffer *self)
{
	self->vtable = &bufferClass.methods;
	ScePafStdc_1B77082E(self);
}

void BufferDefaultDelete(Buffer *ptr)
{
	if (ptr)
		ptr->vtable->Delete(ptr);
}
