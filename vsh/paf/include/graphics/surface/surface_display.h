/*
 * paf::graph::DisplaySurface: the surface the root screen draws to, the display's frame buffer.
 * Its frame buffer is the process's own (FrameBuffer_Normal), a shared one it makes
 * (FrameBuffer_Shared_Create) or opens (FrameBuffer_Shared_Share, _OffScreen), or one the
 * caller gives each frame (FrameBuffer_Given). Its own pages go to the display through GXM's
 * display queue (displayCallback); a shared frame buffer's go through SceSharedFb.
 */
#ifndef PAF_GRAPHICS_SURFACE_SURFACE_DISPLAY_H
#define PAF_GRAPHICS_SURFACE_SURFACE_DISPLAY_H

#include <stdint.h>
extern "C" {
#include <display/display.h>
}
#include <graphics/gutil.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_screen.h>
#include <thread/thread.h>
#include <src/paf_types.h>

namespace paf {
namespace graph {

class DisplaySurface : public ScreenSurface {
public:
	/* what the display queue gives displayCallback for each frame (names ours) */
	class CallbackData {
	public:
		/* NULL: nothing is shown */
		void *base;
		uint32_t pitch;
		uint32_t width;
		uint32_t height;
		/* the plane the frame buffer goes to */
		int32_t port;
		/* the display's resolution flags (DisplayFrameBufParam's) */
		uint32_t resolution;
		bool *pending;
		thread::Mutex *mutex;
		thread::Cond *cond;
		/* set again on the display, then cleared, when not all 0 */
		SceDisplayViewportConf *viewport;
	};

	/* what a shared frame buffer's update holds: the page shown and how (names ours) */
	class UpdateInfo {
	public:
		void *base;
		uint32_t reserved0;
		uint32_t depth_offset;
		uint32_t reserved1[4];
		uint32_t pitch;
		int32_t width;
		int32_t height;
		SceGxmColorFormat format;
		/* the page's GXM sync object (FrameBuffer_Shared_Create), or its page number */
		uint32_t sync_id;
		uint32_t reserved2[4];
	};

	/* the frame buffer a caller gives for a frame (FrameBuffer_Given; names ours) */
	class FrameBuffer {
	public:
		uint32_t pitch;
		int32_t width;
		int32_t height;
		SceGxmColorFormat format;
		SceGxmColorSurfaceType surface_type;
		void *base;
		uint32_t depth_offset;
		SceGxmSyncObject *sync_object;
	};

	DisplaySurface(gutil::FrameBufferMode mode, const Param *param, void *bp);
	virtual ~DisplaySurface();
	/* told of each draw into the surface: the page drawn has changed */
	virtual void SetDirty(bool dirty);

	virtual bool IsDirty()
	{
		return m_page_dirty[m_page_counter];
	}

	virtual void Clear(const math::v4 &color, uint32_t clear_mask);
	virtual void Swap();
	virtual void *Lock(int32_t page);
	virtual void Unlock();

	virtual int32_t GenTexture()
	{
		return SCE_PAF_ERROR_GRAPHICS_INVALID_SURFACE;
	}

	virtual int32_t Begin(uint32_t flags);
	/* a frame begun and ended: the surface's own steps, under the surface list's lock (names
	   ours) */
	virtual int32_t OnStartFrame();
	virtual int32_t OnEndFrame();

	/* the shared frame buffer's id */
	static SceUID GetFbId();
	/* GXM's display queue callback, given the data of the frame to show */
	static void displayCallback(const void *callback_data);
	/* the frame buffer a mode needs for a surface made as param says; the shared one's size
	   back in param (name ours) */
	static void *AllocFrameBuffer(gutil::FrameBufferMode mode, Param *param);
	/* the largest frame buffer the display takes (name ours) */
	static void GetMaximumResolution(int32_t *width, int32_t *height);

	/* NOT_SUPPORTED for a texture */
	int32_t StartFrame();
	int32_t EndFrame();

	/* FrameBuffer_Shared_Share or FrameBuffer_Shared_OffScreen only */
	int32_t SetFrameBufferMode(gutil::FrameBufferMode mode);
	int32_t ReleaseFrameBuffer(gutil::FrameBufferMode mode);
	/* the plane the frame buffer goes to: the shell's, for a shared one it made */
	int32_t GetOutputPort();
	/* whether the display shows the frame buffer in the display area the system sets */
	bool UsesDisplayArea();
	/* the sync objects and the render target, made once (name ours) */
	int32_t Init();
	/* the display shows the whole frame buffer, or in the display area (names ours) */
	void SetFullDisplay();
	void SetDisplayArea();
	/* the size and depth surface a shared frame buffer's update gives (name ours) */
	void Update(const UpdateInfo *info);
	/* until the frame queued is shown (name ours) */
	void WaitFrame();
	/* the frame buffer the caller gives for the next frame (FrameBuffer_Given): a
	   FrameBuffer, NULL for none (name ours) */
	int32_t SetFrameBuffer(const void *frame_buffer);
	/* the viewport the display shows the surface in */
	void SetDisplayViewport(uint32_t y, uint32_t x, uint32_t width, uint32_t height);
	/* the shared frame buffer's update for the page drawn (FrameBuffer_Shared_Create; name
	   ours) */
	int32_t GetUpdateInfo(UpdateInfo *info);

	static intrusive_ptr<Surface> s_offscreen;

	SceGxmSyncObject **m_sync_objects;
	/* per page: drawn since it was last shown */
	bool *m_page_dirty;
	/* per page: its sync object's id for the shell (FrameBuffer_Shared_Create) */
	uint32_t *m_vsh_sync_ids;
	CallbackData m_callback_data;
	/* the page shown */
	int32_t m_shown_page;
	/* show the page shown again, not the page drawn */
	bool m_hold;
	bool m_hold_page;
	/* the GPU clock is the power callback's: no change on a draw or a frame */
	bool m_keep_gpu_clock;
	/* show nothing */
	bool m_blank;
	/* the page drawn is to be shown: Swap turns the pages */
	bool m_swap;
	/* a frame queued that the display has not shown yet */
	bool m_pending;
	thread::Mutex m_mutex;
	thread::Cond m_cond;
	SceDisplayViewportConf m_viewport;
	gutil::FrameBufferMode m_frame_buffer_mode;
	bool m_initialized;
	/* a draw asked to wait for the next vblank (1), none did (0), not told (-1) */
	int8_t m_wait_vblank;
	/* the vblanks each frame lasts at least */
	uint32_t m_vblank_count;
	/* 1 while a frame begins, 2 when the shared frame buffer could not be begun, 0 after the
	   frame */
	int32_t m_frame_state;
	/* the colour surface type of the frame buffer given (FrameBuffer_Given) */
	SceGxmColorSurfaceType m_given_surface_type;
};

/* the frame's time in microseconds (names ours) */
extern uint32_t s_frame_time;
/* frames that wait for the vblank whatever the surface says */
extern int32_t s_wait_vblank_count;

/*
 * Set when a page opens; the shared frame buffer's begin reads it
 * (0x813122d4, name ours).
 */
extern bool s_page_opened;

}

/* the GPU's clock: its level from the frequency, a level set (255: back to the one saved), the
   frequency saved (another file's; names ours) */
int32_t GetGpuClockLevel();
int32_t SetGpuClockLevel(int32_t level);
int32_t SaveGpuClock();

}

#endif
