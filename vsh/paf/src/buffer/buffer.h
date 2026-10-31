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

typedef struct SubBuffer {
	Buffer base;
	BufferSharedPtr m_buffer;
} SubBuffer;

typedef struct SubConstBuffer {
	Buffer base;
	const void *m_const_addr;
	BufferSharedPtr m_buffer;
} SubConstBuffer;

typedef struct UnmanagedBuffer {
	Buffer base;
} UnmanagedBuffer;

typedef struct UnmanagedConstBuffer {
	Buffer base;
	const void *m_const_addr;
} UnmanagedConstBuffer;

typedef struct MallocBuffer {
	Buffer base;
} MallocBuffer;

typedef char BufferSizeCheck[sizeof(Buffer) == 12 ? 1 : -1];
typedef char BufferVTableSizeCheck[sizeof(BufferVTable) == 20 ? 1 : -1];
typedef char HeapAllocBufferSizeCheck[sizeof(HeapAllocBuffer) == 16 ? 1 : -1];
typedef char BufferRefCounterSizeCheck[sizeof(BufferRefCounter) == 16 ? 1 : -1];
typedef char BufferSharedPtrSizeCheck[sizeof(BufferSharedPtr) == 8 ? 1 : -1];
typedef char SubBufferSizeCheck[sizeof(SubBuffer) == 20 ? 1 : -1];
typedef char SubConstBufferSizeCheck[sizeof(SubConstBuffer) == 24 ? 1 : -1];
typedef char UnmanagedBufferSizeCheck[sizeof(UnmanagedBuffer) == 12 ? 1 : -1];
typedef char UnmanagedConstBufferSizeCheck[sizeof(UnmanagedConstBuffer) == 16 ? 1 : -1];
typedef char MallocBufferSizeCheck[sizeof(MallocBuffer) == 12 ? 1 : -1];

/*
 * The C view of paf::Buffer (include/buffer/buffer.h) for the C files not yet
 * converted: the factories by their link names. A factory's SharedPtr result
 * is returned through the pointer in r0; a SharedPtr argument by value is
 * passed as a pointer to the caller's copy.
 */
#define SubBufferCreate _ZN3paf9SubBuffer6CreateEPvjNS_6common9SharedPtrINS_6BufferEEE
#define SubConstBufferCreate _ZN3paf14SubConstBuffer6CreateEPKvjNS_6common9SharedPtrIKNS_6BufferEEE
#define UnmanagedBufferCreate _ZN3paf15UnmanagedBuffer6CreateEPvj
#define UnmanagedConstBufferCreate _ZN3paf20UnmanagedConstBuffer6CreateEPKvj
#define MallocBufferAllocate _ZN3paf12MallocBuffer8AllocateEj
#define MallocBufferCreate _ZN3paf12MallocBuffer6CreateEPvj
#define HeapAllocBufferAllocate _ZN3paf15HeapAllocBuffer8AllocateEjPNS_6memory13HeapAllocatorE
#define HeapAllocBufferCreate _ZN3paf15HeapAllocBuffer6CreateEPvjPNS_6memory13HeapAllocatorE

void SubBufferCreate(BufferSharedPtr *result, void *addr, size_t size,
		     const BufferSharedPtr *buffer);
void SubConstBufferCreate(BufferSharedPtr *result, const void *addr, size_t size,
			  const BufferSharedPtr *buffer);
void UnmanagedBufferCreate(BufferSharedPtr *result, void *addr, size_t size);
void UnmanagedConstBufferCreate(BufferSharedPtr *result, const void *addr, size_t size);
void MallocBufferAllocate(BufferSharedPtr *result, size_t size);
void MallocBufferCreate(BufferSharedPtr *result, void *addr, size_t size);
void HeapAllocBufferAllocate(BufferSharedPtr *result, size_t size,
			     HeapAllocator *allocator);
void HeapAllocBufferCreate(BufferSharedPtr *result, void *addr, size_t size,
			   HeapAllocator *allocator);

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

static inline __attribute__((always_inline))
void BufferSharedPtrAssign(BufferSharedPtr *self, const BufferSharedPtr *src)
{
	Buffer *ptr = src->m_ptr;
	BufferRefCounter *counter = src->m_ref_counter;

	if (ptr != self->m_ptr) {
		BufferSharedPtrRelease(self);
		self->m_ptr = ptr;
		self->m_ref_counter = counter;
		if (ptr)
			sceAtomicIncrement32AcqRel(&counter->m_counter);
	}
}

#endif
