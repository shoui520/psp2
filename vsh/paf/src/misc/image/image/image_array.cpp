/* paf::image::ImageArray: the buffers of an image's mipmap levels and frames. */
#include <image/image.h>
#include <image/image_array.h>
#include <image/image_attribute.h>
#include <memory/heap_allocator.h>
#include <std/new>
#include <std/stdlib.h>
#include <std/string.h>
extern "C" {
#include "../../../paf_types.h"
}

using namespace paf;
using namespace paf::image;
using paf::memory::HeapAllocator;

uint32_t ImageArray::GetMax() const
{
	return m_arraysize.x * m_arraysize.y;
}

ImageBuffer *ImageArray::GetBuffer(uint32_t index) const
{
	if (index < GetMax())
		return &m_array[index];
	return NULL;
}

bool ImageArray::Init(uint32_t max_x, uint32_t max_y)
{
	if (m_array != NULL || (max_x == 0 && max_y != 0) || (max_x != 0 && max_y == 0))
		return false;

	m_arraysize.x = max_x;
	m_arraysize.y = max_y;
	m_needfree = false;
	m_empty = true;
	if (max_x != 0 || max_y != 0) {
		m_array = new ImageBuffer[max_x * max_y];
		for (uint32_t i = 0; i < GetMax(); i++) {
			ImageBuffer *buffer = GetBuffer(i);
			buffer->buff = NULL;
			buffer->size = 0;
		}
	}
	return true;
}

bool ImageArray::Init(const ImagePoint &index)
{
	return Init(index.x, index.y);
}

ImagePoint ImageArray::GetArraySize() const
{
	return m_arraysize;
}

/* GetBuffer(const ImagePoint &), inlined: Sony's SetSize inlines it, SNC here inlines it only from a static function. */
static inline ImageBuffer *BufferAt(ImageBuffer *array, const ImagePoint &arraysize,
				    const ImagePoint &index)
{
	ImageBuffer *buffer = NULL;

	if (index.x >= 0 && index.y >= 0 && index.x < arraysize.x && index.y < arraysize.y)
		buffer = &array[index.x + index.y * arraysize.x];
	return buffer;
}

ImageBuffer *ImageArray::GetBuffer(const ImagePoint &index) const
{
	ImageBuffer *buffer = NULL;

	if (index.x >= 0 && index.y >= 0 && index.x < m_arraysize.x && index.y < m_arraysize.y)
		buffer = &m_array[index.x + index.y * m_arraysize.x];
	return buffer;
}

bool ImageArray::SetSize(uint32_t index, size_t size)
{
	if (index >= GetMax())
		return false;
	GetBuffer(index)->size = size;
	return true;
}

bool ImageArray::SetSize(const ImagePoint &index, uint32_t size)
{
	if (BufferAt(m_array, m_arraysize, index) == NULL)
		return false;
	BufferAt(m_array, m_arraysize, index)->size = size;
	return true;
}

bool ImageArray::SetSize(const ImageAttribute *attr)
{
	for (int x = 0; x < m_arraysize.x; x++) {
		uint32_t size = attr->GetMemSize(x);
		for (int y = 0; y < m_arraysize.y; y++) {
			ImagePoint index;
			index.x = x;
			index.y = y;
			SetSize(index, size);
		}
	}
	return true;
}

bool ImageArray::SetCLUTSize(const ImageAttribute *attr)
{
	size_t size = attr->GetCLUTPixelSizeInBytes() * attr->GetCLUTSize();
	for (uint32_t i = 0; i < GetMax(); i++) {
		ImageBuffer *buffer = GetBuffer(i);
		if (buffer != NULL)
			buffer->size = size;
	}
	return true;
}

bool ImageArray::IsEmpty() const
{
	return m_empty;
}

bool ImageArray::Alloc()
{
	if (!m_empty)
		return false;
	if (GetMax() == 0)
		return true;

	uint32_t i;
	for (i = 0; i < GetMax(); i++) {
		ImageBuffer *buffer = GetBuffer(i);
		if (m_allocator != NULL)
			buffer->buff = m_allocator->sce_paf_malloc(buffer->size);
		else
			buffer->buff = sce_paf_malloc(buffer->size);
		if (buffer->buff == NULL) {
			Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
			break;
		}
	}
	if (i == GetMax()) {
		m_empty = false;
		m_needfree = true;
		return true;
	}

	for (uint32_t j = 0; j < i; j++) {
		ImageBuffer *buffer = GetBuffer(j);
		if (m_allocator != NULL)
			m_allocator->sce_paf_free(buffer->buff);
		else
			sce_paf_free(buffer->buff);
		buffer->buff = NULL;
	}
	m_needfree = false;
	m_empty = true;
	return false;
}

bool ImageArray::Free()
{
	if (!m_empty) {
		if (m_needfree) {
			for (uint32_t i = 0; i < GetMax(); i++) {
				ImageBuffer *buffer = GetBuffer(i);
				if (m_allocator != NULL)
					m_allocator->sce_paf_free(buffer->buff);
				else
					sce_paf_free(buffer->buff);
				buffer->size = 0;
				buffer->buff = NULL;
			}
		} else {
			for (uint32_t i = 0; i < GetMax(); i++) {
				ImageBuffer *buffer = GetBuffer(i);
				buffer->buff = NULL;
				buffer->size = 0;
			}
		}
		m_needfree = false;
		m_empty = true;
	}
	return true;
}

bool ImageArray::Reset()
{
	Free();
	delete[] m_array;
	m_array = NULL;
	return true;
}

bool ImageArray::SetLink()
{
	if (!m_empty)
		return false;
	if (GetMax() != 0) {
		m_empty = false;
		m_needfree = false;
	}
	return true;
}

ImageArray &ImageArray::operator=(const ImageArray &s)
{
	if (this != &s) {
		Free();
		delete[] m_array;
		m_array = new ImageBuffer[s.GetMax()];
		m_allocator = s.m_allocator;
		m_arraysize = s.m_arraysize;
		m_empty = true;
		for (uint32_t i = 0; i < s.GetMax(); i++) {
			ImageBuffer *buffer = GetBuffer(i);
			buffer->buff = NULL;
			buffer->size = s.GetBuffer(i)->size;
		}
		if (!s.m_empty) {
			Alloc();
			for (uint32_t i = 0; i < s.GetMax(); i++) {
				const ImageBuffer *src = s.GetBuffer(i);
				sce_paf_memcpy(GetBuffer(i)->buff, src->buff, src->size);
			}
		}
	}
	return *this;
}

ImageArray::ImageArray(HeapAllocator *allocator)
	: m_array(NULL), m_allocator(allocator), m_needfree(false), m_empty(true)
{
}

ImageArray::~ImageArray()
{
	Free();
	delete[] m_array;
	m_array = NULL;
}
