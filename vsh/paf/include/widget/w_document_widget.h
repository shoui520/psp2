/*
 * paf::ui::DocumentWidget: renders a WebCore document inside a ScrollViewBase
 * via WebRenderer, forwarding touch and gesture events to its observer.
 */
#ifndef PAF_WIDGET_W_DOCUMENT_WIDGET_H
#define PAF_WIDGET_W_DOCUMENT_WIDGET_H

#include <stdint.h>
#include <std/memory>
#include <graphics/surface/surface.h>
#include <math/ngp/v2.h>
#include <widget/core/widget.h>
#include <widget/web_core.h>
#include <widget/web_rect.h>

namespace paf {
namespace graph {
class ScreenCopyObj;
}
namespace ui {

class WebRenderer;

/* Event observer notified by DocumentWidget on touch and gesture events. */
class DocumentEventObserver {
public:
	virtual ~DocumentEventObserver()
	{
	}

	virtual void OnDocumentEvent(int32_t event_type, math::v2 touch_pos) = 0;
};

class DocumentWidget : public Widget, public DocumentSurfaceClient {
public:
	static const char m_widget_type[];
	static const char *TypeName();

	DocumentWidget(Widget *parent, CreateParam *param);
	virtual ~DocumentWidget();

	virtual int32_t HandleTouchEvent(Event *e);
	virtual int32_t HandleGestureEvent(Event *e);
	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void DestroyWidget();
	virtual void UpdateSize();

	virtual int32_t OnDocumentSizeChanged(math::v2 size);
	virtual int32_t OnSurfaceReset();
	virtual int32_t AttachDrawObj(graph::DrawObj *obj, int32_t obj_id);

	int32_t Initialize(WebCoreView *view, WebCoreViewport *viewport,
			   WebRenderClient **out_render_client, WebCoreFrame *frame,
			   WebSurfaceHost **out_surface_host);
	int32_t UpdateRenderedRect(WebRect visible_rect, WebRect *out_rect,
				   bool commit, bool skip_clear);
	bool CheckScrollNeedsRender(math::v2 velocity);
	int32_t UpdateVisibleRect(WebRect *out_rect, bool commit);
	int32_t SyncScrollPosition(math::v2 velocity, bool force_render);
	int32_t SyncScrollPosition(math::v2 velocity);
	int32_t GetViewportSize(math::v2 *out_size);
	int32_t DocRectToWidgetRect(math::v2 top_left, math::v2 bottom_right,
				    math::v2 *out_points);
	int32_t WidgetRectToDocRect(math::v2 top_left, math::v2 bottom_right,
				    math::v2 *out_points);
	void ResumeSurface();
	void SuspendSurface();
	void SetZoomState(int32_t state);
	void SetBlitEnabled(bool enabled);
	math::v2 CalcCurrentTouchPos();
	math::v2 GetTouchPoint();
	int32_t ScrollParentToOffset(math::v2 offset);
	int32_t ResetScrollAndRect();
	void CreateCheckerSurface(intrusive_ptr<graph::Surface> &out_surface);
	void SetMaxSurfaceSize(math::v2 max_size);

	DocumentEventObserver *m_observer;
	WebRenderer *m_renderer;
	bool m_pinching;
	math::v2 m_last_scroll_pos;
	bool m_out_of_bounds;
	bool m_has_velocity;
	math::v2 m_surface_overflow;
	math::v2 m_view_padding;
	WebRect m_rendered_rect;
	math::v2 m_min_size;
	bool m_suspended;
	bool m_suspend_enabled;
	intrusive_ptr<graph::Surface> m_surface;
	graph::ScreenCopyObj *m_screen_copy;
};

}
}

#endif
