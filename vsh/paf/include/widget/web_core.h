/*
 * paf::ui::WebCore and WebCore/Wbc interface declarations for 2014 PAF WebView/MapView.
 */
#ifndef PAF_WIDGET_WEB_CORE_H
#define PAF_WIDGET_WEB_CORE_H

#include <stdint.h>
#include <std/string>
#include <std/vector>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <widget/web_rect.h>

namespace paf {

namespace graph {
class DrawObj;
class SurfacePool;
}

namespace ui {

class WebController;
class WebViewBase;

/* Abstract WebCore tile observer implemented by graph::WebObj / graph::MapObj. */
class WebTileObserver {
public:
	virtual ~WebTileObserver()
	{
	}

	virtual int32_t SetSurfaceOwner(void *owner) = 0;
	virtual int32_t Blit(float pos_x, float pos_y, float width, float height,
			     int32_t src_width, int32_t src_height, int32_t bytes_per_pixel,
			     const void *src_pixels) = 0;
	virtual int32_t NotifyViewportRect(float pos_x, float pos_y, float width, float height) = 0;
	virtual int32_t OnRenderEnd() = 0;
	virtual int32_t ReleaseSurface() = 0;
	virtual int32_t SetPixelSize(int32_t width, int32_t height) = 0;
};

/* Abstract surface owner notified when WebObj/MapObj finalizes. */
class WebSurfaceOwner {
public:
	virtual ~WebSurfaceOwner()
	{
	}

	virtual void OnSurfaceReleased() = 0;
};

/* Abstract client notified by WebRenderer of document size/reset/drawobj attachment. */
class DocumentSurfaceClient {
public:
	virtual ~DocumentSurfaceClient()
	{
	}

	virtual int32_t OnDocumentSizeChanged(math::v2 size) = 0;
	virtual int32_t OnSurfaceReset() = 0;
	virtual int32_t AttachDrawObj(graph::DrawObj *obj, int32_t obj_id) = 0;
};

/* Abstract render client interface implemented by WebRenderer (primary base). */
class WebRenderClient {
public:
	virtual ~WebRenderClient()
	{
	}

	virtual int32_t CreateRenderSurface(int32_t width, int32_t height,
					    WebTileObserver **out_observer) = 0;
	virtual int32_t OnDocumentSizeChanged(int32_t width, int32_t height) = 0;
	virtual int32_t ResetSurface() = 0;
};

/* Abstract surface host interface implemented by WebRenderer (secondary base). */
class WebSurfaceHost {
public:
	virtual ~WebSurfaceHost()
	{
	}

	virtual int32_t OnDocumentSizeChanged(int32_t width, int32_t height) = 0;
	virtual int32_t ResetSurface() = 0;
};

/* Abstract WebCore render view (WbcView) controlled by WebRenderer. */
class WebCoreView {
public:
	virtual ~WebCoreView()
	{
	}

	virtual int32_t SetRenderedRect(uint32_t tex_width, uint32_t tex_height,
					float pos_x, float pos_y, float width, float height) = 0;
	virtual int32_t SetVisibleRect(float pos_x, float pos_y, float width, float height) = 0;
	virtual int32_t HandleKeyEvent(int32_t event_type, int32_t key_code,
				       int32_t char_code, int32_t modifiers) = 0;
	virtual int32_t HandleMouseEvent(int32_t event_type, int32_t pos_x, int32_t pos_y,
					 int32_t button, int32_t click_count,
					 int32_t modifiers, int32_t extra) = 0;
	virtual int32_t ResumeSurface(int32_t width, int32_t height) = 0;
};

/* Abstract WebCore viewport/frame handle released when WebRenderer finalizes. */
class WebCoreViewport {
public:
	virtual ~WebCoreViewport()
	{
	}

	virtual void Release() = 0;
};

class WebCoreFrame {
public:
	virtual ~WebCoreFrame()
	{
	}

	virtual void Release() = 0;
};

/* Abstract WebCore lifecycle interfaces owned by WebCore singleton. */
class WebCoreEventListener {
public:
	virtual ~WebCoreEventListener()
	{
	}

	virtual void OnShutdown(class WebCore *core) = 0;
};

class WebCoreResourceClient {
public:
	virtual ~WebCoreResourceClient()
	{
	}

	virtual void Release() = 0;
	virtual void Finalize() = 0;
};

class WebCoreEngine {
public:
	virtual ~WebCoreEngine()
	{
	}

	virtual void Release() = 0;
	virtual void Shutdown() = 0;
};

class WebCoreProcess {
public:
	virtual ~WebCoreProcess()
	{
	}

	virtual void Release() = 0;
	virtual void Stop() = 0;
	virtual void Terminate() = 0;
};

/*
 * Global WebCore coordinator singleton (2014 PAF).
 * Tracks active WebController instances, surface pool, and lifecycle state.
 */
class WebCore {
public:
	static WebCore *GetInstance();

	int32_t Shutdown(bool notify_restart);
	void SetSuspendFlag(bool flag);
	bool GetSuspendFlag() const;
	void NotifyControllerWakeup(WebController *sender);
	int32_t RemoveController(WebController *controller);

	graph::SurfacePool *GetSurfacePool() const
	{
		return m_surface_pool;
	}

	int32_t GetPopupPolicy() const
	{
		return m_popup_policy;
	}

	static WebCore *s_instance;

	uint32_t m_vtable_word;
	uint32_t m_header_pad[3];
	WebCoreEventListener *m_event_listener;
	uint32_t m_config_pad[6];
	WebCoreResourceClient *m_resource_client;
	uint32_t m_resource_pad;
	WebCoreEngine *m_engine;
	WebCoreProcess *m_process;
	bool m_initialized;
	bool m_active;
	uint8_t m_active_pad[2];
	vector<WebController *> m_controllers;
	uint32_t m_pool_pad[6];
	graph::SurfacePool *m_surface_pool;
	uint32_t m_tail_pad[2];
	int32_t m_popup_policy;
	bool m_suspend_flag;
};

}
}

#endif
