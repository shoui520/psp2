/*
 * The pixel layout of an image or surface (ImageMode, ImageOrder: global
 * types, as Sony's mangled names show), and how paf::image resizes.
 */
#ifndef PAF_IMAGE_IMAGE_TYPES_H
#define PAF_IMAGE_IMAGE_TYPES_H

#include <stdint.h>

enum ImageMode {
	ImageMode_NONE = -1
};

enum ImageOrder {
	ImageOrder_NONE = -1
};

namespace paf {
namespace image {

enum ImageResizeType {
	ImageResizeType_None = 0,
	ImageResizeType_NearestNeighbor = 1
};

}
}

#endif
