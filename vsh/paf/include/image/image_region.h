/* paf::image: an image's size, a point in it and a rectangle of it. */
#ifndef PAF_IMAGE_IMAGE_REGION_H
#define PAF_IMAGE_IMAGE_REGION_H

#include <stdint.h>

namespace paf {
namespace image {

class ImageExtent {
public:
	ImageExtent() : w(0), h(0)
	{
	}

	ImageExtent(int32_t _w, int32_t _h) : w(_w), h(_h)
	{
	}

	int32_t w;
	int32_t h;
};

class ImagePoint {
public:
	ImagePoint() : x(0), y(0)
	{
	}

	ImagePoint(int32_t _x, int32_t _y) : x(_x), y(_y)
	{
	}

	int32_t x;
	int32_t y;
};

class ImageRect {
public:
	ImageRect()
	{
	}

	ImageRect(const ImagePoint &p, const ImageExtent &e)
	{
		origin = p;
		extent = e;
	}

	ImageRect(int32_t x, int32_t y, int32_t w, int32_t h) : origin(x, y), extent(w, h)
	{
	}

	ImagePoint origin;
	ImageExtent extent;
};

}
}

#endif
