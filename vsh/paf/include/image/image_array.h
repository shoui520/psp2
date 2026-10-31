/*
 * paf::image::ImageArray: a grid of image buffers, one per mipmap level (x)
 * and frame (y), sized from an ImageAttribute and allocated together.
 */
#ifndef PAF_IMAGE_IMAGE_ARRAY_H
#define PAF_IMAGE_IMAGE_ARRAY_H

#include <stddef.h>
#include <stdint.h>
#include <image/image_types.h>
#include <image/image_region.h>

namespace paf {

namespace memory {
class HeapAllocator;
}

namespace image {

class ImageAttribute;

class ImageArray {
public:
	ImageArray(memory::HeapAllocator *allocator);
	~ImageArray();

	/* Sizes the grid; both dimensions 0 leaves it without buffers. */
	bool Init(uint32_t max_x, uint32_t max_y);
	bool Init(const ImagePoint &index);
	/* Gives every buffer the size of the attribute's palette. */
	bool SetCLUTSize(const ImageAttribute *attr);
	/* Frees the buffers it allocated and forgets linked ones. */
	bool Free();
	/* Allocates every buffer at its size; on failure frees them again. */
	bool Alloc();
	/* Frees the buffers and the grid. */
	bool Reset();
	/* Marks the buffers as set by the caller, not to be freed. */
	bool SetLink();
	bool SetSize(uint32_t index, size_t size);
	/* Gives each mipmap level the attribute's memory size for it. */
	bool SetSize(const ImageAttribute *attr);
	bool SetSize(const ImagePoint &index, uint32_t size);
	ImageArray &operator=(const ImageArray &s);
	ImagePoint GetArraySize() const;
	uint32_t GetMax() const;
	bool IsEmpty() const;
	ImageBuffer *GetBuffer(uint32_t index) const;
	ImageBuffer *GetBuffer(const ImagePoint &index) const;

	memory::HeapAllocator *GetHeapAllocator() const
	{
		return m_allocator;
	}

private:
	ImageBuffer *m_array;
	memory::HeapAllocator *m_allocator;
	ImagePoint m_arraysize;
	bool m_needfree;
	bool m_empty;
};

}
}

#endif
