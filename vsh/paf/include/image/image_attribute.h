/* paf::image::ImageAttribute: an image's mode and size (declared as far as converted code uses it). */
#ifndef PAF_IMAGE_IMAGE_ATTRIBUTE_H
#define PAF_IMAGE_IMAGE_ATTRIBUTE_H

#include <stdint.h>
#include <image/image_types.h>

namespace paf {
namespace image {

class ImageAttribute {
public:
	/* Bytes for w x h pixels, rows padded to pitch_align bytes and the height to height_align. */
	static uint32_t CalcMemSize(uint32_t w, uint32_t h, uint16_t pitch_align,
				    uint16_t height_align, ImageMode mode, ImageFormat format);
	/* Bits per pixel. */
	static uint32_t GetPixelSize(ImageMode mode);
};

}
}

#endif
