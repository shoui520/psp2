/*
 * paf::graph::DisplaySurface, as far as converted C++ uses it: the surface the
 * root screen draws to, the display's frame buffer.
 */
#ifndef PAF_GRAPHICS_SURFACE_SURFACE_DISPLAY_H
#define PAF_GRAPHICS_SURFACE_SURFACE_DISPLAY_H

#include <stdint.h>
#include <graphics/gutil.h>
#include <graphics/surface/surface.h>

namespace paf {
namespace graph {

class DisplaySurface : public Surface {
public:
	/* FrameBuffer_Shared_Share or FrameBuffer_Shared_OffScreen only */
	int32_t SetFrameBufferMode(gutil::FrameBufferMode mode);
	/* whether the display shows the frame buffer in the display area the system sets */
	bool UsesDisplayArea();

	uint8_t reserved[0xdc];
	gutil::FrameBufferMode m_frame_buffer_mode;
};

}
}

/* Exports with no name found for their NIDs. */
extern "C" {
/* the frame buffer the caller gives for the next frame (FrameBufferMode_Given) */
void ScePafGraphics_F897407F(paf::graph::DisplaySurface *surface, void *buffer);
}

#endif
