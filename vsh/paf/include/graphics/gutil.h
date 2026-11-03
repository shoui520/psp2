/*
 * paf::gutil: the graphics utilities, which set up GXM, the graphics memory and the renderer
 * for the Framework, and keep the default surface pool and shader. Names from the mangled
 * export names (the InitParam and DevInitParam constructors, InitParam::Free, Init and
 * LoadResource by NID); the InitParam fields after ui::wutil::InitParam's.
 */
#ifndef PAF_GRAPHICS_GUTIL_H
#define PAF_GRAPHICS_GUTIL_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <std/string>

namespace paf {

namespace graph {
class Shader;
class SurfacePool;
class TextInitParam;

namespace arl {
class ArlInstance;

/* the GXM state (src/graphics/state.c, still C) */
ArlInstance *Gl();
}
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

/* what Init sets the screen, the camera and the default surface pool up with */
class InitParam {
public:
	InitParam();

	/* the memory debug::Init allocated given back */
	void Free();

	void *surf_dev_mem_addr;
	uint32_t surf_dev_mem_size;
	void *surf_def_pool_addr;
	uint32_t surf_def_pool_size;
	int32_t screen_width;
	int32_t screen_height;
	float fov;
	/* the text set up with it when not NULL */
	const graph::TextInitParam *text_param;
};

/* what GXM is initialized with */
class DevInitParam {
public:
	DevInitParam();

	uint32_t parameter_buffer_size;
	uint32_t parameter_buffer_location;
	WinHandle whandle;
	uint32_t graph_heap_size_on_main_memory;
	uint32_t graph_heap_size_on_video_memory;
	uint32_t display_list_size;
};

int32_t Init(const InitParam &param);
void Term();
/* the renderers the flags name made (1 the wave, 2 the panel, 4 the page turn renderer), with
   the folder their resources are loaded from */
int32_t LoadResource(uint32_t flags, const string &path);
int32_t DevInit(const DevInitParam &param);
void DevTerm();
int32_t Swap(WinHandle whandle);

/* the shader draw objects draw with when they are given none */
common::SharedPtr<graph::Shader> *GetDefaultShader();
graph::SurfacePool *GetDefaultSurfacePool();
void SetFrameBufferMode(FrameBufferMode mode);
/* the mode SetFrameBufferMode set (name ours) */
FrameBufferMode GetFrameBufferMode();
/* initializes GXM, or waits for the shell's when the frame buffer is given */
int32_t InitGxm(const DevInitParam &param);

namespace debug {
/* a heap and a default surface pool for a program that runs without the Framework */
InitParam Init(int32_t width, int32_t height);
void Term(InitParam &param);
WinHandle InitWindow(int32_t width, int32_t height, void *callback);
DevInitParam DevInit(int32_t width, int32_t height, void *window);
void DevTerm();
void WaitDummyVSync();

/* the functions the debug GUI draws with each frame, each with its userdata */
void AddDebugGuiFunc(void (*func)(void *), void *userdata);
void RemoveDebugGuiFunc(void (*func)(void *), void *userdata);
void RemoveDebugGuiFuncs(void (*func)(void *));
void CallDebugGuiFuncs();

/* the mouse and keyboard of a devkit's host, read once a frame (0x81191822..; their code is
   not converted, names ours) */
/* a button, counted a frame at a time */
class MouseButton {
public:
	bool m_down;
	/* cleared by the constructor and read nowhere */
	bool m_unread_flag;
	/* m_down of the frame before */
	bool m_last_down;
	/* the frames it has been down, or up */
	int32_t m_down_frames;
	int32_t m_up_frames;
	/* down in this frame and the one before; down in this one only; up in this one only */
	bool m_held;
	bool m_pressed;
	bool m_released;
};

/* a pointer: where it was the frame before, where it is, and how far it moved */
class MousePointer {
public:
	int32_t m_last_x;
	int32_t m_last_y;
	int32_t m_x;
	int32_t m_y;
	int32_t m_move_x;
	int32_t m_move_y;
};

class Mouse {
public:
	static Mouse *GetInstance();

	/* the buttons and the pointer's positions read for the frame */
	void Update();

	/* the three buttons (the first down while the screen is touched, the third while a second
	   finger is), and the pointers of the first two touches */
	MouseButton m_buttons[3];
	MousePointer m_pointers[2];
	/* the size of the viewport the pointer moves in */
	int32_t m_screen_width;
	int32_t m_screen_height;
};

class Keyboard {
public:
	static Keyboard *GetInstance();

	void Read();
	/* the 256 keys' states counted on */
	void Update();
};
}

}
}

extern "C" {
/* waits for the GXM context of a Gl state to finish (sceGxmFinish) */
void ScePafGraphics_E4B3B1D2(paf::graph::arl::ArlInstance *gl);
/* returns 0 */
int32_t ScePafGraphics_C5504119(void);
/* the frame counted and timed, and once a frame the debug mouse and keyboard read, the mouse
   given the viewport's size */
int32_t ScePafGraphics_27416D74(paf::gutil::WinHandle whandle);
}

#endif
