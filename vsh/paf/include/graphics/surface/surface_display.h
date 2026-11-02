/*
 * paf::graph::DisplaySurface, as far as converted C++ uses it: the surface the
 * root screen draws to, the display's frame buffer.
 */
#ifndef PAF_GRAPHICS_SURFACE_SURFACE_DISPLAY_H
#define PAF_GRAPHICS_SURFACE_SURFACE_DISPLAY_H

#include <stdint.h>
#include <graphics/gutil.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_screen.h>

namespace paf {
namespace graph {

class DisplaySurface : public ScreenSurface {
public:
	/* a frame begun and ended: the surface's own steps, under the surface list's lock (names
	   ours) */
	virtual int32_t OnStartFrame();
	virtual int32_t OnEndFrame();

	/* NOT_SUPPORTED for a texture */
	int32_t StartFrame();
	int32_t EndFrame();

	/* GXM's display queue callback, given the data of the frame to show */
	static void displayCallback(const void *callback_data);

	/* FrameBuffer_Shared_Share or FrameBuffer_Shared_OffScreen only */
	int32_t SetFrameBufferMode(gutil::FrameBufferMode mode);
	/* whether the display shows the frame buffer in the display area the system sets */
	bool UsesDisplayArea();

	uint8_t reserved[0x98];
	gutil::FrameBufferMode m_frame_buffer_mode;
};

}
}

namespace paf {
namespace graph {

/*
 * Set when a page opens; the shared frame buffer's begin reads it
 * (0x813122d4, name ours).
 */
extern bool s_page_opened;

}
}

/* Exports with no name found for their NIDs. */
extern "C" {
/* the frame buffer the caller gives for the next frame (FrameBufferMode_Given) */
void ScePafGraphics_F897407F(paf::graph::DisplaySurface *surface, void *buffer);
}

#endif
