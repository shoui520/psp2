/*
 * paf::graph::DisplaySurface, as far as converted C++ uses it: the surface the
 * root screen draws to, the display's frame buffer.
 */
#ifndef PAF_GRAPHICS_SURFACE_SURFACE_DISPLAY_H
#define PAF_GRAPHICS_SURFACE_SURFACE_DISPLAY_H

#include <graphics/gutil.h>

namespace paf {
namespace graph {

class DisplaySurface {
public:
	static void SetFrameBufferMode(gutil::FrameBufferMode mode);
	/* whether the display shows the frame buffer in the display area the system sets */
	bool UsesDisplayArea();
};

}
}

#endif
