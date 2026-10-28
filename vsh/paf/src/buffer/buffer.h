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

typedef struct BufferClass {
	int offset_to_top;
	const void *type_info;
	BufferVTable methods;
} BufferClass;

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
typedef char BufferClassSizeCheck[sizeof(BufferClass) == 28 ? 1 : -1];
typedef char SubBufferSizeCheck[sizeof(SubBuffer) == 20 ? 1 : -1];
typedef char SubConstBufferSizeCheck[sizeof(SubConstBuffer) == 24 ? 1 : -1];
typedef char UnmanagedBufferSizeCheck[sizeof(UnmanagedBuffer) == 12 ? 1 : -1];
typedef char UnmanagedConstBufferSizeCheck[sizeof(UnmanagedConstBuffer) == 16 ? 1 : -1];
typedef char MallocBufferSizeCheck[sizeof(MallocBuffer) == 12 ? 1 : -1];

#define HeapAllocBufferCreate ScePafMisc_22F87132
#define HeapAllocBufferAllocate ScePafMisc_E9C6A564
#define HeapAllocBufferDestroy ScePafMisc_75CB07DC
#define HeapAllocBufferDelete ScePafMisc_5A9ACD28
#define HeapAllocBufferDestroyBase ScePafMisc_44C18EAD
#define SubBufferCreate ScePafMisc_7DE5F71A
#define SubConstBufferCreate ScePafMisc_86EC46F4
#define UnmanagedBufferCreate ScePafMisc_7FED0E58
#define UnmanagedConstBufferCreate ScePafMisc_DEB7A1CF
#define MallocBufferAllocate ScePafMisc_220C5418
#define MallocBufferCreate ScePafMisc_7ECAD6F8
#define MallocBufferDestroy ScePafMisc_D11D7CE3
#define MallocBufferDelete ScePafMisc_FA39CB2E
#define MallocBufferDestroyBase ScePafMisc_B7F83390
#define subBufferClass ScePafMisc_50950FCB
#define subConstBufferClass ScePafMisc_C817C2F9
#define unmanagedBufferClass ScePafMisc_E8362884
#define unmanagedConstBufferClass ScePafMisc_17F973DF
#define mallocBufferClass ScePafMisc_C1BBF9B4

extern const BufferClass subBufferClass;
extern const BufferClass subConstBufferClass;
extern const BufferClass unmanagedBufferClass;
extern const BufferClass unmanagedConstBufferClass;
extern const BufferClass mallocBufferClass;

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
void SubBufferCreate(BufferSharedPtr *result, void *addr, size_t size,
		     const BufferSharedPtr *buffer);
void SubConstBufferCreate(BufferSharedPtr *result, const void *addr, size_t size,
			  const BufferSharedPtr *buffer);
void UnmanagedBufferCreate(BufferSharedPtr *result, void *addr, size_t size);
void UnmanagedConstBufferCreate(BufferSharedPtr *result, const void *addr, size_t size);
void MallocBufferAllocate(BufferSharedPtr *result, size_t size);
void MallocBufferCreate(BufferSharedPtr *result, void *addr, size_t size);
Buffer *MallocBufferDestroy(Buffer *base);
Buffer *MallocBufferDestroyBase(Buffer *base);
void MallocBufferDelete(Buffer *base);
Buffer *SubBufferDestroy(Buffer *base);
Buffer *SubConstBufferDestroy(Buffer *base);
Buffer *UnmanagedBufferDestroy(Buffer *base);
Buffer *UnmanagedConstBufferDestroy(Buffer *base);
void SubBufferDelete(Buffer *base);
void SubConstBufferDelete(Buffer *base);
void UnmanagedBufferDelete(Buffer *base);
void UnmanagedConstBufferDelete(Buffer *base);
const void *SubConstBufferGetConstAddr(const Buffer *base);
const void *UnmanagedConstBufferGetConstAddr(const Buffer *base);
void SubBufferDefaultDelete(Buffer *ptr);
void SubConstBufferDefaultDelete(Buffer *ptr);
void MallocBufferDefaultDelete(Buffer *ptr);
void UnmanagedBufferDefaultDelete(Buffer *ptr);
void UnmanagedConstBufferDefaultDelete(Buffer *ptr);

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
