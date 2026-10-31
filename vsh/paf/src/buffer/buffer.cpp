#include <buffer/buffer.h>

namespace paf {

/* A range of buffer, only one inside it. */
common::SharedPtr<SubBuffer> SubBuffer::Create(void *addr, size_t size,
					       common::SharedPtr<Buffer> buffer)
{
	if (addr == NULL || buffer.get() == NULL || buffer->GetAddr() == NULL)
		return common::SharedPtr<SubBuffer>();
	if (addr < buffer->GetAddr() ||
	    (char *)addr + size > (char *)buffer->GetAddr() + buffer->GetSize())
		return common::SharedPtr<SubBuffer>();

	SubBuffer *sub = new SubBuffer;

	if (sub == NULL)
		return common::SharedPtr<SubBuffer>();
	sub->m_addr = addr;
	sub->m_size = size;
	sub->m_buffer = buffer;
	return common::SharedPtr<SubBuffer>(sub);
}

common::SharedPtr<SubConstBuffer> SubConstBuffer::Create(const void *addr, size_t size,
							 common::SharedPtr<const Buffer> buffer)
{
	if (addr == NULL || buffer.get() == NULL || buffer->GetAddr() == NULL)
		return common::SharedPtr<SubConstBuffer>();
	if (addr < buffer->GetAddr() ||
	    (const char *)addr + size > (const char *)buffer->GetAddr() + buffer->GetSize())
		return common::SharedPtr<SubConstBuffer>();

	SubConstBuffer *sub = new SubConstBuffer;

	if (sub == NULL)
		return common::SharedPtr<SubConstBuffer>();
	sub->m_addr = NULL;
	sub->m_const_addr = addr;
	sub->m_size = size;
	sub->m_buffer = buffer;
	return common::SharedPtr<SubConstBuffer>(sub);
}

common::SharedPtr<UnmanagedBuffer> UnmanagedBuffer::Create(void *addr, size_t size)
{
	if (addr == NULL)
		return common::SharedPtr<UnmanagedBuffer>();

	UnmanagedBuffer *buffer = new UnmanagedBuffer;

	if (buffer == NULL)
		return common::SharedPtr<UnmanagedBuffer>();
	buffer->m_addr = addr;
	buffer->m_size = size;
	return common::SharedPtr<UnmanagedBuffer>(buffer);
}

common::SharedPtr<UnmanagedConstBuffer> UnmanagedConstBuffer::Create(const void *addr, size_t size)
{
	if (addr == NULL)
		return common::SharedPtr<UnmanagedConstBuffer>();

	UnmanagedConstBuffer *buffer = new UnmanagedConstBuffer;

	if (buffer == NULL)
		return common::SharedPtr<UnmanagedConstBuffer>();
	buffer->m_addr = NULL;
	buffer->m_const_addr = addr;
	buffer->m_size = size;
	return common::SharedPtr<UnmanagedConstBuffer>(buffer);
}

/* Below 64 bytes from the paf heap, else 64-byte aligned. */
common::SharedPtr<MallocBuffer> MallocBuffer::Allocate(size_t size)
{
	void *addr;

	if (size < 64)
		addr = sce_paf_malloc(size);
	else
		addr = sce_paf_memalign(64, size);
	if (addr == NULL)
		return common::SharedPtr<MallocBuffer>();

	MallocBuffer *buffer = new MallocBuffer;

	if (buffer == NULL) {
		sce_paf_free(addr);
		return common::SharedPtr<MallocBuffer>();
	}
	buffer->m_addr = addr;
	buffer->m_size = size;
	return common::SharedPtr<MallocBuffer>(buffer);
}

common::SharedPtr<MallocBuffer> MallocBuffer::Create(void *addr, size_t size)
{
	if (addr == NULL)
		return common::SharedPtr<MallocBuffer>();

	MallocBuffer *buffer = new MallocBuffer;

	if (buffer == NULL)
		return common::SharedPtr<MallocBuffer>();
	buffer->m_addr = addr;
	buffer->m_size = size;
	return common::SharedPtr<MallocBuffer>(buffer);
}

MallocBuffer::~MallocBuffer()
{
	sce_paf_free(m_addr);
}

/* As MallocBuffer::Allocate, from allocator when there is one. */
common::SharedPtr<HeapAllocBuffer> HeapAllocBuffer::Allocate(size_t size,
							     memory::HeapAllocator *allocator)
{
	void *addr;

	if (allocator != NULL) {
		if (size < 64)
			addr = allocator->sce_paf_malloc(size);
		else
			addr = allocator->sce_paf_memalign(64, size);
	} else {
		if (size < 64)
			addr = sce_paf_malloc(size);
		else
			addr = sce_paf_memalign(64, size);
	}
	if (addr == NULL)
		return common::SharedPtr<HeapAllocBuffer>();

	HeapAllocBuffer *buffer = new HeapAllocBuffer;

	if (buffer == NULL) {
		if (allocator != NULL)
			allocator->sce_paf_free(addr);
		else
			sce_paf_free(addr);
		return common::SharedPtr<HeapAllocBuffer>();
	}
	buffer->m_addr = addr;
	buffer->m_allocator = allocator;
	buffer->m_size = size;
	return common::SharedPtr<HeapAllocBuffer>(buffer);
}

common::SharedPtr<HeapAllocBuffer> HeapAllocBuffer::Create(void *addr, size_t size,
							   memory::HeapAllocator *allocator)
{
	HeapAllocBuffer *buffer = new HeapAllocBuffer;

	if (buffer == NULL)
		return common::SharedPtr<HeapAllocBuffer>();
	buffer->m_addr = addr;
	buffer->m_allocator = allocator;
	buffer->m_size = size;
	return common::SharedPtr<HeapAllocBuffer>(buffer);
}

HeapAllocBuffer::~HeapAllocBuffer()
{
	if (m_allocator != NULL)
		m_allocator->sce_paf_free(m_addr);
	else
		sce_paf_free(m_addr);
}

}
