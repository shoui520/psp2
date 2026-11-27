/*
 * paf::ui::WebRect: axis-aligned rectangle (left, right, top, bottom) used by the
 * 2014 PAF web/map rendering pipeline (WebObj, MapObj, WebBGObj, WebRenderer,
 * DocumentWidget, JsDocument).
 */
#ifndef PAF_WIDGET_WEB_RECT_H
#define PAF_WIDGET_WEB_RECT_H

#include <stdint.h>

namespace paf {
namespace ui {

class WebRect {
public:
	WebRect();
	~WebRect();

	void Set(float left, float bottom, float right, float top);
	void SetCenterSize(float center_x, float center_y, float width, float height);
	void Intersect(const WebRect &other);
	void ClampMin(const WebRect &bounds);
	void ClampInside(const WebRect &bounds);
	bool ApproxEquals(const WebRect &other) const;

	float Width() const
	{
		return m_right - m_left;
	}

	float Height() const
	{
		return m_top - m_bottom;
	}

	float m_left;
	float m_right;
	float m_top;
	float m_bottom;
};

}
}

#endif
