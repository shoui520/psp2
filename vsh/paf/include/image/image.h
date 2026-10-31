/* paf::image::Image: a decoded image (declared as far as converted code uses it). */
#ifndef PAF_IMAGE_IMAGE_H
#define PAF_IMAGE_IMAGE_H

#include <image/image_types.h>
#include <image/image_region.h>

namespace paf {
namespace image {

class Image {
public:
	ImageMode GetMode() const;
	ImageOrder GetOrder() const;
	ImageExtent GetExtent() const;

	static void SetErrorType(int error);
};

}
}

#endif
