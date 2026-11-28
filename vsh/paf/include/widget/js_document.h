/*
 * paf::ui::JsDocument ("js_document"): WebCore map document widget translating
 * touch, drag, and pinch gestures into MapCore zoom/pan commands and MapObj blits.
 */
#ifndef PAF_WIDGET_JS_DOCUMENT_H
#define PAF_WIDGET_JS_DOCUMENT_H

#include <stdint.h>
#include <math/ngp/v2.h>
#include <widget/core/widget.h>
#include <widget/map_bg_loader.h>
#include <widget/web_core.h>
#include <widget/web_rect.h>
#include <widget/w_mapview.h>

namespace paf {

namespace graph {
class MapObj;
}

namespace ui {

class MapCore;
class WebRenderer;

/* Touch movement tracker embedded in JsDocument (44 bytes). */
struct MapTouchTracker {
	int32_t ComputeDelta(int32_t *out_dx, int32_t *out_dy);

	int32_t m_down_x;
	int32_t m_down_y;
	int32_t m_move_x;
	int32_t m_move_y;
	int32_t m_drag_start_x;
	int32_t m_drag_start_y;
	int32_t m_drag_x;
	int32_t m_drag_y;
	int32_t m_delta_x;
	int32_t m_delta_y;
	int32_t m_phase;
};

class JsDocument : public Widget, public DocumentSurfaceClient {
public:
	static const char m_widget_type[];
	static const char *TypeName();

	JsDocument(Widget *parent, CreateParam *param);
	virtual ~JsDocument();

	virtual int32_t HandleTouchEvent(Event *e);
	virtual int32_t HandleGestureEvent(Event *e);
	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void DestroyWidget();
	virtual void UpdateSize();

	/* DocumentSurfaceClient overrides. */
	virtual int32_t OnDocumentSizeChanged(math::v2 size);
	virtual int32_t OnSurfaceReset();
	virtual int32_t AttachDrawObj(graph::DrawObj *obj, int32_t obj_id);

	int32_t Initialize(WebCoreView *view, WebCoreViewport *viewport,
			   WebRenderClient **out_render_client, WebCoreFrame *frame,
			   WebSurfaceHost **out_surface_host, MapCore *map_core);
	int32_t UpdateRenderedRect(WebRect visible_rect, WebRect *out_rect);
	int32_t UpdateVisibleRect(WebRect *out_rect);
	int32_t StartRender();

	int32_t OnTouchMove();
	void OnTouchEndNotify();
	int32_t ForwardMouseEvent(int32_t phase, int32_t pos_x, int32_t pos_y);
	int32_t OnTouchUp();
	void NotifyTouchBegin(Event *e, uint32_t touch_index);
	int32_t OnTouchDown(Event *e);
	int32_t OnSingleTap();
	void ConvertToCssCoord(const MapPixelPoint *in_pos, MapPixelPoint *out_css);
	int32_t DispatchZoomGesture(int32_t zoom_action, int32_t zoom_level,
				    const MapPixelPoint *old_center,
				    const MapPixelPoint *new_center);
	int32_t CommitPinchZoom();
	int32_t OnPinchEnd();
	int32_t OnDragEnd(Event *e);
	int32_t OnDragMove();
	int32_t OnDragBegin();
	int32_t ResetScrollPosition();
	void SetMapBlitEnabled(bool enabled);

	uint8_t padding[16];
	int32_t m_pinch_origin_x;
	int32_t m_pinch_origin_y;
	uint8_t reserved[16];
	MapTouchTracker m_touch;
	bool m_pinching;
	bool m_dragging;
	uint64_t m_last_touch_tick;
	WebRenderer *m_renderer;
	graph::MapObj *m_map_obj;
	MapBackgroundLoader *m_bg_loader;
	MapCore *m_map_core;
	bool m_initial_render_done;
	uint32_t m_touch_user_data;
	uint8_t tail_padding[8];
};

}
}

#endif
