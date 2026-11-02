/*
 * paf::graph::DisplaySurface: the surface the root screen draws to, the display's frame buffer
 * (the file's name ours). Its own pages go to the display through GXM's display queue, which
 * calls displayCallback when a page is to be shown; a shared frame buffer's pages are begun and
 * ended through SceSharedFb each frame. ScreenSurface's empty constructor sits ahead of it.
 */
#include <stddef.h>
#include <stdint.h>
#include <display.h>
#include <gxm.h>
extern "C" {
#include <appmgr/sharedfb.h>
#include <display/display.h>
}
#include <common/common_internal.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/graphmem/graphmem.h>
#include <graphics/gutil.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_display.h>
#include <graphics/surface/surface_screen.h>
#include <std/new>
#include <std/string.h>
#include <system/system.h>
#include <thread/thread.h>
#include <timer/timer.h>
#include <src/paf_types.h>

/* the resolution flags of the frame buffer shown: the whole display, or in the display area
   the system sets (names ours) */
#define DISPLAY_RESOLUTION_FULL		0x100000
#define DISPLAY_RESOLUTION_DISPLAY_AREA	0x40000

/* the shell's GXM sync objects, by id (SceGxmInternalForVsh; no header in the tree) */
extern "C" {
int sceGxmVshSyncObjectCreate(uint32_t id, SceGxmSyncObject *sync_object);
int sceGxmVshSyncObjectDestroy(uint32_t id);
int sceGxmVshSyncObjectOpen(uint32_t id, SceGxmSyncObject **sync_object);
int sceGxmVshSyncObjectClose(uint32_t id, SceGxmSyncObject *sync_object);
/* SceDisplayUser's (no header in the tree) */
int sceDisplayGetMaximumFrameBufResolution(int *width, int *height);
/* SceVshBridge's, through its stub library */
int vshDisplaySetViewportConf(SceUInt32 head, SceUInt32 plane, const SceDisplayViewportConf *conf);
}

namespace paf {
namespace graph {

/* the frame buffer a frame of a shared or given one shows: its update first (names ours) */
static SharedFbInfo s_fb_info;
static SceUID s_fb_id = 0;

static inline DisplaySurface::UpdateInfo *GetUpdate()
{
	return (DisplaySurface::UpdateInfo *)s_fb_info.update;
}

/* when the frame began (names ours) */
class FrameClock {
public:
	FrameClock() : m_start(get_time_internal())
	{
	}

	SceUInt64 m_start;
};

/* 32 by 32; nothing reads it (names ours) */
class DefaultExtent {
public:
	DefaultExtent() : width(32), height(32)
	{
	}

	int32_t width;
	int32_t height;
};

static DefaultExtent s_default_extent;

intrusive_ptr<Surface> DisplaySurface::s_offscreen(NULL, true);
bool s_page_opened;
uint32_t s_frame_time;
int32_t s_wait_vblank_count;
/* the display queue showed nothing last (name ours) */
static bool s_blank;
static FrameClock s_frame_clock;

ScreenSurface::ScreenSurface()
	: Surface(0, 0, ImageMode_NONE, ImageOrder_Linear, TypeRenderTarget, 1, NULL, 0, 1, false)
{
}

void ScreenSurface::GetParam(Param *param)
{
	if (param != NULL) {
		param->width = m_width;
		param->height = m_height;
		param->format = m_format;
		param->order = m_order;
		param->num_page = m_num_page;
		if (m_depth.get() != NULL) {
			param->depth_format = m_depth->m_format;
			param->depth_order = m_depth->GetOrder();
		}
		param->scale_mode = m_scale_mode;
		param->full_size = m_full_size;
	}
}

int32_t DisplaySurface::Init()
{
	s_fb_info.base = NULL;
	if (m_frame_buffer_mode >= gutil::FrameBuffer_Normal &&
	    m_frame_buffer_mode <= gutil::FrameBuffer_Shared_Create && GetMemory() == NULL)
		return SCE_PAF_ERROR_GRAPHICS_NO_SPACE;
	if (m_format != ImageMode_RGBA8888)
		return SCE_PAF_ERROR_GRAPHICS_NOT_SUPPORTED;
	if (m_initialized)
		return 0;

	int32_t ret;

	switch (m_frame_buffer_mode) {
	case gutil::FrameBuffer_Normal:
	case gutil::FrameBuffer_Shared_Create:
		ret = InitColorSurface();
		if (ret < 0)
			return ret;
		if (m_depth.get() != NULL) {
			ret = InitDepthSurface();
			if (ret < 0)
				return ret;
		}
		for (int32_t i = 0; i < m_num_page; i++) {
			ret = sceGxmSyncObjectCreate(&m_sync_objects[i]);
			if (ret < 0)
				return ret;
			if (m_frame_buffer_mode == gutil::FrameBuffer_Shared_Create) {
				m_vsh_sync_ids[i] = i + 1;
				ret = sceGxmVshSyncObjectCreate(m_vsh_sync_ids[i], m_sync_objects[i]);
				if (ret < 0)
					return ret;
			}
		}
		break;
	case gutil::FrameBuffer_Shared_Share:
	case gutil::FrameBuffer_Shared_OffScreen:
		for (int32_t i = 0; i < m_num_page; i++) {
			ret = sceGxmVshSyncObjectOpen(i + 1, &m_sync_objects[i]);
			if (ret < 0)
				return ret;
		}
		break;
	case gutil::FrameBuffer_Given:
		break;
	}

	ret = CreateRenderTarget(&m_render_target, 0);
	if (ret < 0)
		return ret;
	m_initialized = true;
	return 0;
}

void DisplaySurface::SetFullDisplay()
{
	m_callback_data.resolution = DISPLAY_RESOLUTION_FULL;
}

DisplaySurface::DisplaySurface(gutil::FrameBufferMode mode, const Param *param, void *bp)
	: ScreenSurface(param, bp, TypeDisplay), m_sync_objects(NULL), m_page_dirty(NULL),
	  m_vsh_sync_ids(NULL), m_callback_data(), m_shown_page(0), m_hold(false), m_hold_page(false),
	  m_keep_gpu_clock(false), m_blank(false), m_swap(false), m_pending(false),
	  m_mutex("ScePafDisplaySurfaceMutex", 0), m_cond(m_mutex, "ScePafDisplaySurfaceCond", 0)
{
	m_frame_buffer_mode = mode;
	m_vblank_count = 0;
	m_wait_vblank = -1;
	m_initialized = false;
	m_frame_state = 1;

	size_t num_page = param->num_page;

	if (num_page == 0)
		common_internal::on_error();
	m_sync_objects = new SceGxmSyncObject *[num_page];
	sce_paf_memset(m_sync_objects, 0, sizeof(SceGxmSyncObject *) * num_page);
	switch (m_frame_buffer_mode) {
	case gutil::FrameBuffer_Normal:
		m_page_dirty = new bool[num_page];
		sce_paf_memset(m_page_dirty, 0xff, num_page);
		break;
	case gutil::FrameBuffer_Shared_Create:
		m_page_dirty = new bool[num_page];
		sce_paf_memset(m_page_dirty, 0xff, num_page);
		m_vsh_sync_ids = new uint32_t[num_page];
		sce_paf_memset(m_vsh_sync_ids, 0, sizeof(uint32_t) * num_page);
		break;
	case gutil::FrameBuffer_Shared_Share:
	case gutil::FrameBuffer_Shared_OffScreen:
	case gutil::FrameBuffer_Given:
		break;
	}
	if (num_page > 1)
		ActivatePage(1);

	m_viewport.size = sizeof(m_viewport);
	m_viewport.x = 0;
	m_viewport.y = 0;
	m_viewport.width = 960;
	m_viewport.height = 544;

	m_callback_data.base = NULL;
	m_callback_data.pitch = (m_stride * 8) >> m_pixel_bits;
	m_callback_data.width = m_width;
	m_callback_data.height = m_height;
	m_callback_data.pending = &m_pending;
	m_callback_data.mutex = &m_mutex;
	m_callback_data.cond = &m_cond;
	m_callback_data.viewport = &m_viewport;
	m_callback_data.port = 0;
	SetFullDisplay();
	Init();
}

int32_t DisplaySurface::Begin(uint32_t flags)
{
	if (GetRawContext() == NULL)
		return SCE_PAF_ERROR_GRAPHICS_INVALID_POINTER;

	int32_t ret = ScePafGraphics_751FC8E6(this, m_render_target,
					      m_sync_objects[m_page_counter],
					      &m_color_surfaces[m_page_counter],
					      m_depth_stencil_surface, flags);

	if (ret < 0)
		return ret;
	return 0;
}

void DisplaySurface::SetDirty(bool dirty)
{
	if (m_page_dirty != NULL)
		m_page_dirty[m_page_counter] = true;
	if (dirty) {
		if (m_wait_vblank == -1)
			m_wait_vblank = 1;
	} else {
		m_wait_vblank = 0;
	}
	if (!m_keep_gpu_clock && m_frame_buffer_mode != gutil::FrameBuffer_Given)
		SetGpuClockLevel(255);
}

void DisplaySurface::Clear(const math::v4 &color, uint32_t clear_mask)
{
	if (m_cleared)
		return;
	if (m_page_dirty != NULL) {
		if (m_page_dirty[m_page_counter]) {
			Surface::Clear(color, clear_mask);
			m_page_dirty[m_page_counter] = false;
		} else {
			m_cleared = 1;
		}
	} else {
		Surface::Clear(color, clear_mask);
	}
	m_wait_vblank = -1;
}

void DisplaySurface::Swap()
{
	switch (m_frame_buffer_mode) {
	case gutil::FrameBuffer_Normal:
	case gutil::FrameBuffer_Shared_Create:
		if (m_swap)
			Surface::Swap();
		break;
	case gutil::FrameBuffer_Shared_Share:
	case gutil::FrameBuffer_Shared_OffScreen:
		Surface::Swap();
		break;
	default:
		break;
	}
}

void DisplaySurface::WaitFrame()
{
	switch (m_frame_buffer_mode) {
	case gutil::FrameBuffer_Normal:
	case gutil::FrameBuffer_Shared_Create:
		if (!m_pending) {
			sceDisplayWaitVblankStart();
			break;
		}
		m_mutex.Lock();
		if (m_pending)
			m_cond.Wait(0);
		m_mutex.Unlock();
		break;
	case gutil::FrameBuffer_Shared_Share:
	case gutil::FrameBuffer_Shared_OffScreen:
		sceDisplayWaitVblankStart();
		break;
	default:
		break;
	}
}

void DisplaySurface::displayCallback(const void *callback_data)
{
	const CallbackData *data = (const CallbackData *)callback_data;
	DisplayFrameBufParam param;
	SceDisplayFrameBuf *frame_buf = NULL;
	int32_t plane = 0;

	if (data->base != NULL) {
		param.frameBuf.size = sizeof(param);
		param.frameBuf.base = data->base;
		param.frameBuf.pitch = data->pitch;
		param.frameBuf.pixelformat = 0;
		param.frameBuf.width = data->width;
		param.frameBuf.height = data->height;
		param.resolution = data->resolution;
		if (((uintptr_t)data->base & 0xff) != 0)
			common_internal::on_error();
		s_blank = false;
		frame_buf = &param.frameBuf;
	} else {
		s_blank = true;
	}
	sceDisplaySetFrameBufInternal(0, data->port, frame_buf, 1);
	sceDisplaySetFrameBufInternal(1, data->port, frame_buf, 1);
	if (data->port == 1) {
		SceDisplayViewportConf *viewport = data->viewport;

		if (viewport->y != 0 || viewport->x != 0 || viewport->width != 0 ||
		    viewport->height != 0) {
			do {
				vshDisplaySetViewportConf(0, plane, viewport);
				if (system::Model::GetType() == system::Model::Type_Dolce)
					vshDisplaySetViewportConf(1, plane, viewport);
				plane++;
			} while (plane <= 1);
			viewport->x = 0;
			viewport->y = 0;
			viewport->width = 0;
			viewport->height = 0;
		}
	}
	if (GetGpuClockLevel() == 1) {
		SaveGpuClock();
		SetGpuClockLevel(2);
	}
	sceDisplayWaitVblankStart();
	data->mutex->Lock();
	*data->pending = false;
	data->cond->Notify();
	data->mutex->Unlock();
	SetGpuClockLevel(255);
}

SceUID DisplaySurface::GetFbId()
{
	return s_fb_id;
}

int32_t DisplaySurface::SetFrameBufferMode(gutil::FrameBufferMode mode)
{
	if (mode != gutil::FrameBuffer_Shared_Share && mode != gutil::FrameBuffer_Shared_OffScreen)
		return SCE_PAF_ERROR_GRAPHICS_NO_ENTRY;
	m_frame_buffer_mode = mode;
	return 0;
}

void *DisplaySurface::Lock(int32_t page)
{
	switch (m_frame_buffer_mode) {
	case gutil::FrameBuffer_Normal:
	case gutil::FrameBuffer_Shared_Create:
		return Surface::Lock(page);
	case gutil::FrameBuffer_Shared_Share:
	case gutil::FrameBuffer_Shared_OffScreen:
	case gutil::FrameBuffer_Given: {
		m_lock_mutex.Lock();
		if (m_locked) {
			m_lock_mutex.Unlock();
			return NULL;
		}
		m_locked = 1;

		void *base = GetUpdate()->base;

		m_lock_mutex.Unlock();
		return base;
	}
	default:
		return NULL;
	}
}

void DisplaySurface::Unlock()
{
	switch (m_frame_buffer_mode) {
	case gutil::FrameBuffer_Normal:
	case gutil::FrameBuffer_Shared_Create:
		Surface::Unlock();
		break;
	case gutil::FrameBuffer_Shared_Share:
	case gutil::FrameBuffer_Shared_OffScreen:
		m_lock_mutex.Lock();
		if (m_locked) {
			m_locked = 0;
			if (GetUpdate()->base != NULL)
				Surface::GenTexture(&m_tex_pages[m_page_counter], GetUpdate()->base);
		}
		m_lock_mutex.Unlock();
		break;
	case gutil::FrameBuffer_Given:
		m_lock_mutex.Lock();
		if (m_locked)
			m_locked = 0;
		m_lock_mutex.Unlock();
		break;
	default:
		break;
	}
}

void DisplaySurface::SetDisplayArea()
{
	m_callback_data.resolution = DISPLAY_RESOLUTION_FULL | DISPLAY_RESOLUTION_DISPLAY_AREA;
}

void DisplaySurface::Update(const UpdateInfo *info)
{
	bool resized = false;
	int32_t width = info->width;
	int32_t height = info->height;

	if (m_width != width || m_height != height) {
		m_width = width;
		m_height = height;
		if (m_depth.get() != NULL) {
			m_depth->Resize(m_width, m_height);
			resized = true;
		}
		if (m_render_target != NULL && CreateRenderTarget(&m_render_target, 0) != 0)
			common_internal::on_error();
	}

	uint32_t depth_offset = info->depth_offset;

	if (resized || m_depth_offset != depth_offset) {
		m_depth_offset = depth_offset;
		if (InitDepthSurface() != 0)
			common_internal::on_error();
	}
}

int32_t DisplaySurface::SetFrameBuffer(const void *frame_buffer)
{
	const FrameBuffer *buffer = (const FrameBuffer *)frame_buffer;

	if (buffer != NULL) {
		UpdateInfo *update = GetUpdate();

		s_fb_info.base = buffer->base;
		update->base = s_fb_info.base;
		update->depth_offset = buffer->depth_offset;
		update->pitch = buffer->pitch;
		update->width = buffer->width;
		update->height = buffer->height;
		update->format = buffer->format;
		m_given_surface_type = buffer->surface_type;
		m_full_size = false;
		m_reserved0 = 0;
		m_sync_objects[m_page_counter] = buffer->sync_object;
		Update(update);
	} else {
		s_fb_info.base = NULL;
	}
	return 0;
}

void DisplaySurface::GetMaximumResolution(int32_t *width, int32_t *height)
{
	if (width != NULL && height != NULL)
		sceDisplayGetMaximumFrameBufResolution(width, height);
}

void *DisplaySurface::AllocFrameBuffer(gutil::FrameBufferMode mode, Param *param)
{
	if (param == NULL)
		return NULL;

	int32_t width = param->width;
	int32_t height = param->height;
	ImageMode format = param->format;
	ImageOrder order = param->order;
	int32_t num_page = param->num_page;
	ImageMode depth_format = param->depth_format;
	ImageOrder depth_order = param->depth_order;
	void *base = NULL;

	switch (mode) {
	case gutil::FrameBuffer_Normal:
	case gutil::FrameBuffer_Shared_Create: {
		uint32_t size = ScePafGraphics_48FBF58D(2, width, height, format, order, 1, NULL, 0,
							num_page) *
				num_page;

		if (ConvertDepthFormat(depth_format) != -1)
			size += GetDepthSize(width, height, depth_format, depth_order);
		size = (size + 0x3ffff) & ~0x3ffff;
		if (mode == gutil::FrameBuffer_Normal) {
			base = GraphMem::AllocOSMemory(GraphMem::DeviceType_VideoMemory, size,
						       "ScePafFrameBufferMemory", NULL);
		} else {
			SceSharedFbCreate create;
			SharedFbInfo info;

			sce_paf_memset(&create, 0, sizeof(create));
			create.AllocMemSize = size;
			s_fb_id = sceSharedFbCreate(1, &create);
			if (s_fb_id >= 0 && sceSharedFbGetInfo(s_fb_id, &info) == 0) {
				int32_t ret = sceGxmMapMemory(info.base, info.size,
							      SCE_GXM_MEMORY_ATTRIB_READ |
							      SCE_GXM_MEMORY_ATTRIB_WRITE);

				if (ret != 0)
					common_internal::on_error();
				if (ret >= 0)
					base = info.base;
			}
		}
		break;
	}
	case gutil::FrameBuffer_Shared_Share:
	case gutil::FrameBuffer_Shared_OffScreen: {
		SharedFbInfo info;

		s_fb_id = _sceSharedFbOpen(1, SCE_PSP2_SDK_VERSION);
		if (s_fb_id < 0 || sceSharedFbGetInfo(s_fb_id, &info) < 0) {
			common_internal::on_error();
		} else {
			int32_t ret = sceGxmMapMemory(info.base, info.size,
						      SCE_GXM_MEMORY_ATTRIB_READ |
						      SCE_GXM_MEMORY_ATTRIB_WRITE);

			if (ret != 0)
				common_internal::on_error();
			if (ret < 0)
				common_internal::on_error();
		}

		const UpdateInfo *update = (const UpdateInfo *)info.update;

		if (update->width != 0 && update->height != 0) {
			param->width = update->width;
			param->height = update->height;
		}
		break;
	}
	case gutil::FrameBuffer_Given:
		break;
	}
	return base;
}

int32_t DisplaySurface::GetOutputPort()
{
	int32_t port = 0;

	if (m_frame_buffer_mode == gutil::FrameBuffer_Shared_Create) {
		port = sceSharedFbGetShellRenderPort();
		if (port < 0)
			return 0;
	}
	return port;
}

bool DisplaySurface::UsesDisplayArea()
{
	DisplayFrameBufParam param;

	param.frameBuf.size = sizeof(param);
	return sceDisplayGetFrameBufInternal(1, GetOutputPort(), &param.frameBuf, 1) == 0 &&
	       param.frameBuf.size >= sizeof(param) &&
	       (param.resolution & DISPLAY_RESOLUTION_DISPLAY_AREA) != 0;
}

/* the viewport's x and y come in the other order */
void DisplaySurface::SetDisplayViewport(uint32_t y, uint32_t x, uint32_t width, uint32_t height)
{
	m_viewport.x = x;
	m_viewport.y = y;
	m_viewport.width = width;
	m_viewport.height = height;
}

int32_t DisplaySurface::GetUpdateInfo(UpdateInfo *info)
{
	if (info == NULL)
		return SCE_PAF_ERROR_GRAPHICS_INVALID_POINTER;
	if (m_frame_buffer_mode != gutil::FrameBuffer_Shared_Create)
		return SCE_PAF_ERROR_GRAPHICS_INVALID_SURFACE;
	info->base = GetPagePtr(m_page_counter);
	info->reserved0 = 0;
	info->depth_offset = m_depth_offset;
	info->pitch = (m_stride * 8) >> m_pixel_bits;
	info->width = m_width;
	info->height = m_height;
	info->format = GetColorSurfaceFormat();
	info->sync_id = m_vsh_sync_ids[m_page_counter];
	sce_paf_memset(info->reserved1, 0, sizeof(info->reserved1));
	sce_paf_memset(info->reserved2, 0, sizeof(info->reserved2));
	return 0;
}

int32_t DisplaySurface::ReleaseFrameBuffer(gutil::FrameBufferMode mode)
{
	int32_t ret = 0;
	int32_t err;

	switch (mode) {
	case gutil::FrameBuffer_Normal:
		GraphMem::FreeOSMemory(GraphMem::DeviceType_VideoMemory, m_memory);
		break;
	case gutil::FrameBuffer_Shared_Create: {
		SharedFbInfo info;

		err = sceSharedFbGetInfo(s_fb_id, &info);
		if (err < 0) {
			ret = err;
			break;
		}
		err = sceGxmUnmapMemory(info.base);
		if (err < 0) {
			ret = err;
			break;
		}
		ret = sceSharedFbDelete(s_fb_id);
		break;
	}
	case gutil::FrameBuffer_Shared_Share:
	case gutil::FrameBuffer_Shared_OffScreen: {
		SharedFbInfo info;

		err = sceSharedFbBegin(s_fb_id, &info);
		if (err < 0) {
			ret = err;
			break;
		}
		err = sceGxmUnmapMemory(info.base);
		if (err < 0) {
			ret = err;
			break;
		}
		err = sceSharedFbEnd(s_fb_id);
		if (err < 0) {
			ret = err;
			break;
		}
		if (sceSharedFbClose(s_fb_id) != 0)
			common_internal::on_error();
		break;
	}
	case gutil::FrameBuffer_Given:
		break;
	}
	return ret;
}

DisplaySurface::~DisplaySurface()
{
	switch (m_frame_buffer_mode) {
	case gutil::FrameBuffer_Normal:
		sceGxmFinish(GetRawContext());
		sceGxmDisplayQueueFinish();
		for (int32_t i = 0; i < m_num_page; i++)
			sceGxmSyncObjectDestroy(m_sync_objects[i]);
		delete[] m_page_dirty;
		break;
	case gutil::FrameBuffer_Shared_Create:
		for (int32_t i = 0; i < m_num_page; i++) {
			sceGxmVshSyncObjectDestroy(m_vsh_sync_ids[i]);
			sceGxmSyncObjectDestroy(m_sync_objects[i]);
		}
		delete[] m_page_dirty;
		delete[] m_vsh_sync_ids;
		break;
	case gutil::FrameBuffer_Shared_Share:
	case gutil::FrameBuffer_Shared_OffScreen:
		for (int32_t i = 0; i < m_num_page; i++)
			sceGxmVshSyncObjectClose(i + 1, m_sync_objects[i]);
		break;
	default:
		break;
	}
	delete[] m_sync_objects;
	m_sync_objects = NULL;
	ReleaseFrameBuffer(m_frame_buffer_mode);
}

int32_t DisplaySurface::OnStartFrame()
{
	m_frame_state = 1;
	s_frame_time = 0;
	s_frame_clock.m_start = get_time_internal();

	switch (m_frame_buffer_mode) {
	case gutil::FrameBuffer_Normal:
		return 0;
	case gutil::FrameBuffer_Shared_Create: {
		UpdateInfo info;
		uint32_t count;
		int32_t ret;

		GetUpdateInfo(&info);
		count = 0;
		ret = sceSharedFbUpdateProcessBegin(s_fb_id, &info, &count);
		if (ret == 0) {
			if ((int32_t)count > 0)
				ScePafGraphics_E7B32741(arl::Gl());
			for (int32_t i = 0; i < (int32_t)count; i++) {
				if (sceSharedFbUpdateProcess() >= 0)
					SetDirty(false);
			}
		}
		sceSharedFbUpdateProcessEnd();
		m_cleared = 1;
		return 0;
	}
	case gutil::FrameBuffer_Shared_Share:
	case gutil::FrameBuffer_Shared_OffScreen: {
		int32_t ret = sceSharedFbBegin(s_fb_id, &s_fb_info);

		if (ret != 0) {
			m_frame_state = 2;
			return ret;
		}

		UpdateInfo *update = GetUpdate();

		Update(update);
		if (s_page_opened)
			s_fb_info.vsync = 1;
		if (m_frame_buffer_mode == gutil::FrameBuffer_Shared_Share)
			m_cleared = 1;
		else if (m_frame_buffer_mode == gutil::FrameBuffer_Shared_OffScreen)
			m_cleared = 0;

		void *base = update->base;

		m_page_counter = update->sync_id - 1;
		Surface::GenTexture(&m_tex_pages[m_page_counter], base);

		SceGxmColorSurfaceType type = GetColorSurfaceOrder();

		if (type == (SceGxmColorSurfaceType)-1)
			return SCE_PAF_ERROR_GRAPHICS_NOT_SUPPORTED;
		ret = SetColorSurface(m_page_counter, update->format, type, update->pitch, base);
		if (ret < 0)
			return ret;
		return 0;
	}
	case gutil::FrameBuffer_Given: {
		if (s_fb_info.base == NULL)
			return SCE_PAF_ERROR_GRAPHICS_INVALID_SURFACE;
		m_cleared = 1;

		UpdateInfo *update = GetUpdate();
		int32_t ret = SetColorSurface(m_page_counter, update->format, m_given_surface_type,
					      update->pitch, update->base);

		if (ret < 0)
			return ret;
		return 0;
	}
	default:
		return SCE_PAF_ERROR_GRAPHICS_INVALID_SURFACE;
	}
}

int32_t DisplaySurface::OnEndFrame()
{
	int32_t ret = 0;
	float time = (float)(get_time_internal() - s_frame_clock.m_start);
	int32_t wait = 0;

	s_frame_time = (uint32_t)(time / 1000000.0f * 1000.0f * 1000.0f);
	if (s_wait_vblank_count != 0 || !s_page_opened || m_wait_vblank == 1)
		wait = 1;
	if (m_vblank_count > 1)
		wait = m_vblank_count;
	if (wait != 0) {
		uint32_t frames = s_frame_time /
				  (uint32_t)(int32_t)(Timer::s_tick_unit * 1000.0f * 1000.0f);

		int32_t left = wait - frames;

		if (left > 0) {
			do {
				ret = sceDisplayWaitVblankStart();
				left--;
			} while (left != 0);
		}
	}

	m_swap = false;
	switch (m_frame_buffer_mode) {
	case gutil::FrameBuffer_Normal:
	case gutil::FrameBuffer_Shared_Create: {
		int32_t shown = m_shown_page;
		int32_t port = 0;
		int32_t page = m_page_counter;

		if (m_frame_buffer_mode == gutil::FrameBuffer_Shared_Create) {
			port = sceSharedFbGetShellRenderPort();
			if (port < 0)
				port = 0;
		}

		if (m_page_dirty[shown] || m_page_dirty[page] || m_callback_data.port != port) {
			ScePafGraphics_E7B32741(arl::Gl());
			if (s_page_opened) {
				m_callback_data.port = port;
				m_pending = true;
				if (!m_hold && !m_hold_page)
					m_swap = true;
				if (m_blank) {
					m_callback_data.base = NULL;
				} else if (m_hold || m_hold_page) {
					m_callback_data.base = GetPagePtr(shown);
				} else {
					m_callback_data.height = m_height;
					m_callback_data.pitch = (m_stride * 8) >> m_pixel_bits;
					m_callback_data.width = m_width;
					m_callback_data.base = GetPagePtr(page);
					m_shown_page = page;
				}
				ret = sceGxmDisplayQueueAddEntry(m_sync_objects[shown],
								 m_sync_objects[page],
								 &m_callback_data);
			}
		} else {
			if (s_page_opened && !s_blank) {
				m_callback_data.port = port;
				m_pending = true;
				m_callback_data.base = NULL;
				ret = sceGxmDisplayQueueAddEntry(m_sync_objects[shown],
								 m_sync_objects[shown],
								 &m_callback_data);
			}
			if (!m_keep_gpu_clock && GetGpuClockLevel() != 2) {
				SaveGpuClock();
				SetGpuClockLevel(2);
			}
		}
		break;
	}
	case gutil::FrameBuffer_Shared_Share:
	case gutil::FrameBuffer_Shared_OffScreen:
		if (m_frame_state != 2) {
			ScePafGraphics_E7B32741(arl::Gl());
			if (GetGpuClockLevel() == 1) {
				SaveGpuClock();
				SetGpuClockLevel(2);
			}
			ret = sceSharedFbEnd(s_fb_id);
		}
		break;
	default:
		ScePafGraphics_E7B32741(arl::Gl());
		break;
	}
	m_frame_state = 0;
	return ret;
}

}
}
