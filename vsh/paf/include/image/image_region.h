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

	int32_t w;
	int32_t h;
};

class ImagePoint {
public:
	ImagePoint() : x(0), y(0)
	{
	}

	int32_t x;
	int32_t y;
};

class ImageRect {
public:
	ImagePoint origin;
	ImageExtent extent;
};

}
}

#endif
