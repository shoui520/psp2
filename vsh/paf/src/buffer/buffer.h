#ifndef PAF_BUFFER_BUFFER_H
#define PAF_BUFFER_BUFFER_H

#include "../memory/heap_allocator.h"
#include "../std/string.h"
#include <sce_atomic.h>

typedef struct Buffer Buffer;

typedef struct BufferVTable {
	void *(*GetAddr)(Buffer *self);
	const void *(*GetConstAddr)(const Buffer *self);
	size_t (*GetSize)(const Buffer *self);
	Buffer *(*Destroy)(Buffer *self);
	void (*Delete)(Buffer *self);
} BufferVTable;

struct Buffer {
	const BufferVTable *vtable;
	void *m_addr;
	size_t m_size;
};

typedef struct HeapAllocBuffer {
	Buffer base;
	HeapAllocator *m_allocator;
} HeapAllocBuffer;

typedef struct BufferRefCounter {
	Buffer *m_ptr;
	int32_t m_counter;
	int32_t m_weak_counter;
	void (*m_deleter)(Buffer *ptr);
} BufferRefCounter;

typedef struct BufferSharedPtr {
	Buffer *m_ptr;
	BufferRefCounter *m_ref_counter;
} BufferSharedPtr;

typedef char BufferSizeCheck[sizeof(Buffer) == 12 ? 1 : -1];
typedef char BufferVTableSizeCheck[sizeof(BufferVTable) == 20 ? 1 : -1];
typedef char HeapAllocBufferSizeCheck[sizeof(HeapAllocBuffer) == 16 ? 1 : -1];
typedef char BufferRefCounterSizeCheck[sizeof(BufferRefCounter) == 16 ? 1 : -1];
typedef char BufferSharedPtrSizeCheck[sizeof(BufferSharedPtr) == 8 ? 1 : -1];

#define HeapAllocBufferCreate ScePafMisc_22F87132
#define HeapAllocBufferAllocate ScePafMisc_E9C6A564
#define HeapAllocBufferDestroy ScePafMisc_75CB07DC
#define HeapAllocBufferDelete ScePafMisc_5A9ACD28
#define HeapAllocBufferDestroyBase ScePafMisc_44C18EAD

void *BufferGetAddr(Buffer *self);
const void *BufferGetConstAddr(const Buffer *self);
size_t BufferGetSize(const Buffer *self);
Buffer *BufferDestroy(Buffer *self);
void BufferDelete(Buffer *self);
void BufferDefaultDelete(Buffer *ptr);
void HeapAllocBufferCreate(BufferSharedPtr *result, void *addr, size_t size,
			   HeapAllocator *allocator);
void HeapAllocBufferAllocate(BufferSharedPtr *result, size_t size,
			     HeapAllocator *allocator);
Buffer *HeapAllocBufferDestroy(Buffer *base);
Buffer *HeapAllocBufferDestroyBase(Buffer *base);
void HeapAllocBufferDelete(Buffer *base);

static inline __attribute__((always_inline))
void BufferSharedPtrCopy(BufferSharedPtr *self, const BufferSharedPtr *src)
{
	self->m_ptr = src->m_ptr;
	self->m_ref_counter = src->m_ref_counter;
	if (self->m_ptr)
		sceAtomicIncrement32AcqRel(&self->m_ref_counter->m_counter);
}

static inline __attribute__((always_inline))
void BufferSharedPtrRelease(const BufferSharedPtr *self)
{
	if (self->m_ptr &&
	    sceAtomicDecrement32AcqRel(&self->m_ref_counter->m_counter) == 1) {
		self->m_ref_counter->m_deleter(self->m_ptr);
		if (sceAtomicDecrement32AcqRel(&self->m_ref_counter->m_weak_counter) == 1)
			ScePafStdc_1B77082E(self->m_ref_counter);
	}
}

#endif
