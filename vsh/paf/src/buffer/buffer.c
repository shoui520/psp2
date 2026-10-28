#include "buffer.h"
#include "../std/stdlib.h"

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

const BufferClass subBufferClass = {
	0, NULL, {
		BufferGetAddr, BufferGetConstAddr, BufferGetSize,
		SubBufferDestroy, SubBufferDelete
	}
};

const BufferClass subConstBufferClass = {
	0, NULL, {
		BufferGetAddr, SubConstBufferGetConstAddr, BufferGetSize,
		SubConstBufferDestroy, SubConstBufferDelete
	}
};

const BufferClass unmanagedBufferClass = {
	0, NULL, {
		BufferGetAddr, BufferGetConstAddr, BufferGetSize,
		UnmanagedBufferDestroy, UnmanagedBufferDelete
	}
};

const BufferClass unmanagedConstBufferClass = {
	0, NULL, {
		BufferGetAddr, UnmanagedConstBufferGetConstAddr, BufferGetSize,
		UnmanagedConstBufferDestroy, UnmanagedConstBufferDelete
	}
};

const BufferClass mallocBufferClass = {
	0, NULL, {
		BufferGetAddr, BufferGetConstAddr, BufferGetSize,
		MallocBufferDestroy, MallocBufferDelete
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
void BufferSharedPtrInitialize(BufferSharedPtr *result, Buffer *buffer,
			       void (*deleter)(Buffer *ptr))
{
	BufferRefCounter *counter;

	result->m_ptr = buffer;
	counter = ScePafStdc_FC5CD359(sizeof(*counter));
	if (counter) {
		result->m_ref_counter = counter;
		counter->m_ptr = buffer;
		counter = result->m_ref_counter;
		counter->m_counter = 1;
		counter->m_weak_counter = 1;
		counter->m_deleter = deleter;
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
		BufferSharedPtrInitialize(result, &buffer->base, BufferDefaultDelete);
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
		BufferSharedPtrInitialize(result, &buffer->base, BufferDefaultDelete);
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

void SubBufferCreate(BufferSharedPtr *result, void *addr, size_t size,
		     const BufferSharedPtr *buffer)
{
	SubBuffer *self;
	uintptr_t start;
	size_t parent_size;

	if (!addr || !buffer->m_ptr ||
	    !buffer->m_ptr->vtable->GetAddr(buffer->m_ptr))
		goto empty;
	if ((uintptr_t)addr < (uintptr_t)buffer->m_ptr->vtable->GetAddr(buffer->m_ptr))
		goto empty;
	start = (uintptr_t)buffer->m_ptr->vtable->GetAddr(buffer->m_ptr);
	parent_size = buffer->m_ptr->vtable->GetSize(buffer->m_ptr);
	if ((uintptr_t)addr + size > start + parent_size)
		goto empty;
	self = ScePafStdc_FC5CD359(sizeof(*self));
	if (!self)
		goto empty;
	self->base.vtable = &subBufferClass.methods;
	self->base.m_addr = NULL;
	self->m_buffer.m_ptr = NULL;
	self->m_buffer.m_ref_counter = NULL;
	self->base.m_size = 0;
	self->base.m_addr = addr;
	self->base.m_size = size;
	BufferSharedPtrAssign(&self->m_buffer, buffer);
	BufferSharedPtrInitialize(result, &self->base, SubBufferDefaultDelete);
	return;
empty:
	result->m_ptr = NULL;
	result->m_ref_counter = NULL;
}

void SubConstBufferCreate(BufferSharedPtr *result, const void *addr, size_t size,
			  const BufferSharedPtr *buffer)
{
	SubConstBuffer *self;
	uintptr_t start;
	size_t parent_size;

	if (!addr || !buffer->m_ptr ||
	    !buffer->m_ptr->vtable->GetConstAddr(buffer->m_ptr))
		goto empty;
	if ((uintptr_t)addr < (uintptr_t)buffer->m_ptr->vtable->GetConstAddr(buffer->m_ptr))
		goto empty;
	start = (uintptr_t)buffer->m_ptr->vtable->GetConstAddr(buffer->m_ptr);
	parent_size = buffer->m_ptr->vtable->GetSize(buffer->m_ptr);
	if ((uintptr_t)addr + size > start + parent_size)
		goto empty;
	self = ScePafStdc_FC5CD359(sizeof(*self));
	if (!self)
		goto empty;
	self->base.m_size = 0;
	self->base.m_addr = NULL;
	self->base.vtable = &subConstBufferClass.methods;
	self->m_buffer.m_ptr = NULL;
	self->m_buffer.m_ref_counter = NULL;
	self->m_const_addr = addr;
	self->base.m_size = size;
	BufferSharedPtrAssign(&self->m_buffer, buffer);
	BufferSharedPtrInitialize(result, &self->base, SubConstBufferDefaultDelete);
	return;
empty:
	result->m_ptr = NULL;
	result->m_ref_counter = NULL;
}

void UnmanagedBufferCreate(BufferSharedPtr *result, void *addr, size_t size)
{
	UnmanagedBuffer *self;

	if (!addr)
		goto empty;
	self = ScePafStdc_FC5CD359(sizeof(*self));
	if (!self)
		goto empty;
	self->base.m_size = 0;
	self->base.m_addr = NULL;
	self->base.vtable = &unmanagedBufferClass.methods;
	self->base.m_addr = addr;
	self->base.m_size = size;
	BufferSharedPtrInitialize(result, &self->base, UnmanagedBufferDefaultDelete);
	return;
empty:
	result->m_ptr = NULL;
	result->m_ref_counter = NULL;
}

void UnmanagedConstBufferCreate(BufferSharedPtr *result, const void *addr, size_t size)
{
	UnmanagedConstBuffer *self;

	if (!addr)
		goto empty;
	self = ScePafStdc_FC5CD359(sizeof(*self));
	if (!self)
		goto empty;
	self->base.m_size = 0;
	self->base.m_addr = NULL;
	self->base.vtable = &unmanagedConstBufferClass.methods;
	self->m_const_addr = addr;
	self->base.m_size = size;
	BufferSharedPtrInitialize(result, &self->base, UnmanagedConstBufferDefaultDelete);
	return;
empty:
	result->m_ptr = NULL;
	result->m_ref_counter = NULL;
}

void MallocBufferAllocate(BufferSharedPtr *result, size_t size)
{
	MallocBuffer *self;
	void *addr;

	if (size < 64)
		addr = sce_paf_malloc(size);
	else
		addr = sce_paf_memalign(64, size);
	if (!addr)
		goto empty;
	self = ScePafStdc_FC5CD359(sizeof(*self));
	if (!self) {
		sce_paf_free(addr);
		goto empty;
	}
	self->base.m_size = 0;
	self->base.m_addr = NULL;
	self->base.vtable = &mallocBufferClass.methods;
	self->base.m_addr = addr;
	self->base.m_size = size;
	BufferSharedPtrInitialize(result, &self->base, MallocBufferDefaultDelete);
	return;
empty:
	result->m_ptr = NULL;
	result->m_ref_counter = NULL;
}

void MallocBufferCreate(BufferSharedPtr *result, void *addr, size_t size)
{
	MallocBuffer *self;

	if (!addr)
		goto empty;
	self = ScePafStdc_FC5CD359(sizeof(*self));
	if (!self)
		goto empty;
	self->base.m_size = 0;
	self->base.m_addr = NULL;
	self->base.vtable = &mallocBufferClass.methods;
	self->base.m_addr = addr;
	self->base.m_size = size;
	BufferSharedPtrInitialize(result, &self->base, MallocBufferDefaultDelete);
	return;
empty:
	result->m_ptr = NULL;
	result->m_ref_counter = NULL;
}

Buffer *MallocBufferDestroy(Buffer *base)
{
	void *addr = base->m_addr;

	base->vtable = &mallocBufferClass.methods;
	sce_paf_free(addr);
	return BufferDestroy(base);
}

void MallocBufferDelete(Buffer *base)
{
	MallocBufferDestroy(base);
	ScePafStdc_1B77082E(base);
}

Buffer *MallocBufferDestroyBase(Buffer *base)
{
	return MallocBufferDestroy(base);
}

Buffer *SubBufferDestroy(Buffer *base)
{
	SubBuffer *self = (SubBuffer *)base;

	if (self->m_buffer.m_ptr) {
		base->vtable = &subBufferClass.methods;
		BufferSharedPtrRelease(&self->m_buffer);
	}
	return BufferDestroy(base);
}

Buffer *SubConstBufferDestroy(Buffer *base)
{
	SubConstBuffer *self = (SubConstBuffer *)base;

	if (self->m_buffer.m_ptr) {
		base->vtable = &subConstBufferClass.methods;
		BufferSharedPtrRelease(&self->m_buffer);
	}
	return BufferDestroy(base);
}

Buffer *UnmanagedBufferDestroy(Buffer *base)
{
	return BufferDestroy(base);
}

Buffer *UnmanagedConstBufferDestroy(Buffer *base)
{
	return BufferDestroy(base);
}

void SubBufferDelete(Buffer *base)
{
	SubBufferDestroy(base);
	ScePafStdc_1B77082E(base);
}

void SubConstBufferDelete(Buffer *base)
{
	SubConstBufferDestroy(base);
	ScePafStdc_1B77082E(base);
}

void UnmanagedBufferDelete(Buffer *base)
{
	BufferDelete(base);
}

void UnmanagedConstBufferDelete(Buffer *base)
{
	BufferDelete(base);
}

const void *SubConstBufferGetConstAddr(const Buffer *base)
{
	return ((const SubConstBuffer *)base)->m_const_addr;
}

const void *UnmanagedConstBufferGetConstAddr(const Buffer *base)
{
	return ((const UnmanagedConstBuffer *)base)->m_const_addr;
}

void SubBufferDefaultDelete(Buffer *ptr)
{
	BufferDefaultDelete(ptr);
}

void SubConstBufferDefaultDelete(Buffer *ptr)
{
	BufferDefaultDelete(ptr);
}

void MallocBufferDefaultDelete(Buffer *ptr)
{
	BufferDefaultDelete(ptr);
}

void UnmanagedBufferDefaultDelete(Buffer *ptr)
{
	BufferDefaultDelete(ptr);
}

void UnmanagedConstBufferDefaultDelete(Buffer *ptr)
{
	BufferDefaultDelete(ptr);
}
