/*
 * paf::Buffer and its kinds: a memory range behind a SharedPtr. A SubBuffer
 * keeps a range of another buffer and holds that buffer; an UnmanagedBuffer
 * does not own its memory; MallocBuffer and HeapAllocBuffer free it with the
 * buffer, the paf heap's or a HeapAllocator's. The const kinds keep a const
 * address, which only the const GetAddr returns.
 */
#ifndef PAF_BUFFER_BUFFER_H
#define PAF_BUFFER_BUFFER_H

#include <stddef.h>
#include <stdint.h>
#include <std/new>
#include <std/stdlib.h>
#include <common/shared_ptr.h>
#include <memory/heap_allocator.h>

namespace paf {

class Buffer {
public:
	Buffer() : m_addr(NULL), m_size(0)
	{
	}

	virtual void *GetAddr()
	{
		return m_addr;
	}

	virtual const void *GetAddr() const
	{
		return m_addr;
	}

	virtual size_t GetSize() const
	{
		return m_size;
	}

	virtual ~Buffer()
	{
	}

protected:
	void *m_addr;
	size_t m_size;
};

class SubBuffer : public Buffer {
public:
	virtual ~SubBuffer()
	{
	}

	static common::SharedPtr<SubBuffer> Create(void *addr, size_t size,
						   common::SharedPtr<Buffer> buffer);

private:
	common::SharedPtr<Buffer> m_buffer;
};

class SubConstBuffer : public Buffer {
public:
	virtual const void *GetAddr() const
	{
		return m_const_addr;
	}

	virtual ~SubConstBuffer()
	{
	}

	static common::SharedPtr<SubConstBuffer> Create(const void *addr, size_t size,
							common::SharedPtr<const Buffer> buffer);

private:
	const void *m_const_addr;
	common::SharedPtr<const Buffer> m_buffer;
};

class UnmanagedBuffer : public Buffer {
public:
	virtual ~UnmanagedBuffer()
	{
	}

	static common::SharedPtr<UnmanagedBuffer> Create(void *addr, size_t size);
};

class UnmanagedConstBuffer : public Buffer {
public:
	virtual const void *GetAddr() const
	{
		return m_const_addr;
	}

	virtual ~UnmanagedConstBuffer()
	{
	}

	static common::SharedPtr<UnmanagedConstBuffer> Create(const void *addr, size_t size);

private:
	const void *m_const_addr;
};

class MallocBuffer : public Buffer {
public:
	virtual ~MallocBuffer();

	static common::SharedPtr<MallocBuffer> Allocate(size_t size);
	static common::SharedPtr<MallocBuffer> Create(void *addr, size_t size);
};

class HeapAllocBuffer : public Buffer {
public:
	virtual ~HeapAllocBuffer();

	static common::SharedPtr<HeapAllocBuffer> Allocate(size_t size,
							   memory::HeapAllocator *allocator);
	static common::SharedPtr<HeapAllocBuffer> Create(void *addr, size_t size,
							 memory::HeapAllocator *allocator);

private:
	memory::HeapAllocator *m_allocator;
};

}

#endif
