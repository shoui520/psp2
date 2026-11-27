/*
 * paf::ui::WebRenderer: bridges DocumentWidget/JsDocument coordinates with
 * WebCore rendering surfaces (WebObj and WebBGObj) in 2014 PAF.
 */
#ifndef PAF_WIDGET_WEB_RENDERER_H
#define PAF_WIDGET_WEB_RENDERER_H

#include <stdint.h>
#include <vectormath.h>
#include <math/ngp/v2.h>
#include <widget/web_core.h>
#include <widget/web_rect.h>

namespace paf {

namespace graph {
class WebObj;
class WebBGObj;
}

namespace ui {

class WebRenderer : public WebRenderClient, public WebSurfaceHost {
public:
	WebRenderer();
	virtual ~WebRenderer();

	virtual int32_t CreateRenderSurface(int32_t width, int32_t height,
					    WebTileObserver **out_observer);
	virtual int32_t OnDocumentSizeChanged(int32_t width, int32_t height);
	virtual int32_t ResetSurface();

	int32_t UpdateTransform();
	int32_t SetWidgetSize(math::v2 size);
	int32_t Initialize(DocumentSurfaceClient *document_client, math::v2 init_size,
			   WebRect surface_rect, WebRect view_rect,
			   int32_t tex_width, int32_t tex_height,
			   WebCoreView *view, WebCoreViewport *viewport,
			   WebRenderClient **out_render_client, WebCoreFrame *frame,
			   WebSurfaceHost **out_surface_host, graph::WebObj *web_obj);
	void Finalize();
	math::v2 ClampZoomPlaneCenter(math::v2 center, math::v2 size);
	void UpdateZoomPlane(const WebRect &view_doc_rect);
	int32_t SetVisibleRect(const WebRect &widget_rect, WebRect *out_doc_rect, bool commit);
	int32_t SetRenderedRect(const WebRect &widget_rect, WebRect *out_doc_rect,
				bool commit, bool skip_clear);
	int32_t SuspendSurface();
	int32_t ResumeSurface();
	int32_t HandleMouseEvent(int32_t event_type, int32_t pos_x, int32_t pos_y,
				 int32_t button, int32_t click_count,
				 int32_t modifiers, int32_t extra);
	int32_t HandleKeyEvent(int32_t key_code);
	graph::WebObj *ResizeWebObj(math::v2 size);
	void SetMaxSurfaceSize(math::v2 max_size);

	sce::Vectormath::Simd::Aos::Matrix3 m_widget_to_doc;
	sce::Vectormath::Simd::Aos::Matrix3 m_doc_to_widget;
	sce::Vectormath::Simd::Aos::Matrix3 m_doc_scale;
	math::v2 m_doc_size;
	math::v2 m_widget_size;
	DocumentSurfaceClient *m_document_client;
	graph::WebObj *m_web_obj;
	graph::WebBGObj *m_bg_obj;
	WebCoreViewport *m_viewport;
	WebCoreFrame *m_frame;
	WebCoreView *m_view;
	math::v2 m_max_surface_size;
	WebRect m_last_surface_rect;
};

}
}

#endif
