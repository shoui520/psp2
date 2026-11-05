/*
 * paf::graph's 2D FFTs over grids of complex values (math::v2: real, imaginary), for the FFT wave
 * surface (the header's name ours; the functions' names Sony's).
 */
#ifndef PAF_GRAPHICS_EFFECTS_FFT_H
#define PAF_GRAPHICS_EFFECTS_FFT_H

#include <stdint.h>
#include <math/ngp/v2.h>
#include <std/vector>

namespace paf {
namespace graph {

/* 0x811944c0: the 2D FFT of src into dst, inverse when asked */
void FFT2D(int32_t width, int32_t height, bool inverse, const vector<math::v2> &src,
	   vector<math::v2> &dst);
/* 0x811949ea: the inverse 2D FFT of 32 by 32 */
void FFT2D32x32(const vector<math::v2> &src, vector<math::v2> &dst);

}
}

#endif
