/*
 * paf::gutil, as far as converted C++ uses it: the graphics utilities, which
 * set up GXM and the display for the Framework.
 */
#ifndef PAF_GRAPHICS_GUTIL_H
#define PAF_GRAPHICS_GUTIL_H

#include <stdint.h>

namespace paf {

namespace graph {
class SurfacePool;
}

namespace gutil {

/* how the frame buffer is had: the Framework's mode decides it */
enum FrameBufferMode {
	FrameBuffer_Normal = 0,
	FrameBuffer_Shared_Create = 1,
	FrameBuffer_Shared_Share = 2,
	FrameBuffer_Shared_OffScreen = 3,
	FrameBuffer_Given = 4
};

/* the window: its size and the display callback */
class WinHandle {
public:
	int32_t width;
	int32_t height;
	void *callback;
};

/* what GXM is initialized with */
class DevInitParam {
public:
	uint32_t parameter_buffer_size;
	uint32_t parameter_buffer_location;
	WinHandle whandle;
	uint32_t graph_heap_size_on_main_memory;
	uint32_t graph_heap_size_on_video_memory;
	uint32_t display_list_size;
};

graph::SurfacePool *GetDefaultSurfacePool();
void SetFrameBufferMode(FrameBufferMode mode);
/* initializes GXM, or waits for the shell's when the frame buffer is given */
int32_t InitGxm(const DevInitParam &param);

}
}

#endif
