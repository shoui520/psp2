/*
 * paf::ui::AppIconBase: an application's icon, a plane under a label, the label's text
 * object kept below the icon, and the modes it is shown in. The class, TypeName and the
 * virtual functions' names are Sony's (the export NIDs hash as them); the members, and the
 * names of the exports no hash was found for, are ours.
 */
#ifndef PAF_WIDGET_W_APPICON_BASE_H
#define PAF_WIDGET_W_APPICON_BASE_H

#include <stdint.h>
#include <std/string>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <graphics/drawobj/text_obj.h>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

class AppIconBase : public Widget {
public:
	/* what is called with the icon mode SetIconMode sets and the one before (names ours;
	   export 72A7F51A sets them) */
	typedef void (*IconCB)(int32_t mode, int32_t prev, void *data);
	/* what Update calls with the icon (names ours; export A50C6B30 sets it) */
	typedef void (*UpdateCB)(AppIconBase *icon, void *data);

	static const char m_widget_type[];
	static const char *TypeName();

	AppIconBase(Widget *parent, CreateParam *param);
	virtual ~AppIconBase();

	virtual int32_t HandleKeycodeEvent(Event *e);
	virtual int32_t HandleTouchEvent(Event *e);
	virtual int32_t HandleGestureEvent(Event *e);
	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void DestroyWidget();
	virtual uint32_t CheckState();
	virtual void UpdateSize();
	virtual void Update();
	virtual int32_t OnDraw(graph::DrawInfo &info);
	/* export E686D954 (name ours) */
	virtual void OnTouchCaptureChanged(uint32_t port, uint32_t id, int32_t arg);
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);
	virtual int32_t SetString(const wstring &str, int32_t obj);
	virtual int32_t GetString(wstring &str, int32_t obj);

	/* every direction while its focus directions are on; else up and down, or only one way
	   when something listens for 0x10000001 or 0x10000002 */
	virtual int32_t GetFocusDirectionMask()
	{
		if (!m_focus_direction_off)
			return 7;
		if (m_map_e_listener.find(0x10000001) != m_map_e_listener.end())
			return 2;
		if (m_map_e_listener.find(0x10000002) != m_map_e_listener.end())
			return 2;
		return 3;
	}


	/* the widget's size for an icon of the size: the label's height added */
	virtual math::v4 CalcIconSize(const math::v4 &size) const;
	virtual int32_t SetIconSize(const math::v4 &size, Timer *t, int32_t id, int32_t opt);
	/* the texture of the plane over the icon */
	virtual int32_t SetInfoTexture(const intrusive_ptr<graph::Surface> &surf);
	virtual void SetLabelString(const wstring &str);
	virtual void SetIconMode(int32_t mode);
	virtual void SetDispMode(int32_t mode);

	/* exports A50C6B30 and 72A7F51A (names ours) */
	void SetUpdateCB(UpdateCB func, void *data);
	void SetIconCB(IconCB func, void *data);

	/* the icon's size: the widget's, the label's height taken off (export DD6CE2DD, name
	   ours) */
	math::v4 GetIconSize() const;
	/* the icon state, its move back to the icon size ended (export E8941D16, name ours) */
	int32_t SetIconState(int32_t state, int32_t arg);

	void SendEvent(int32_t type, int32_t d0, int32_t d1, int32_t d2, int32_t d3);

	/* the touch event in icon state 0 to 4 (HandleTouchEvent's; exports C351C6BA, 9D7E9E6D,
	   F23A1CF1, 8B6FD3F0, 1FD7EB26; names ours), and export CBA54017, which does nothing */
	void HandleTouchIdle(Event *e);
	void HandleTouchPressed(Event *e);
	void HandleTouchHeld(Event *e);
	void HandleTouchState3(Event *e);
	int32_t HandleTouchState4(Event *e);
	int32_t HandleTouchNone();

	wstring m_label;
	/* the label's size, larger with the system's enlarged font */
	float m_label_width;
	float m_label_height;
	/* what SetIconState sets */
	int32_t m_icon_state;
	/* what SetIconMode sets */
	int32_t m_icon_mode;
	/* what SetDispMode sets */
	int32_t m_disp_mode;
	bool reserved1;
	/* how long a touch holds the icon before it is raised, in milliseconds (name ours) */
	float m_hold_time;
	/* what Update calls while nothing holds the icon */
	UpdateCB m_update_cb;
	void *m_update_cb_data;
	IconCB m_icon_cb;
	void *m_icon_cb_data;
	/* the info plane (draw object 1) drawn (name ours) */
	bool m_show_info;
	uint8_t reserved4[3];
	/* the touch's position and hit at the touch down that held it (names ours) */
	math::v2 m_touch_pos;
	math::v2 m_hit_pos;
	int32_t m_drag_value;
	bool m_dragging;
	/* the label */
	graph::TextObj *m_text_obj;
};

/* an AppIconBase's resource parameters: WidgetParam's, nothing of its own */
class AppIconBaseParam : public WidgetParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	AppIconBaseParam();
	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);
};

/* an AppIconBase's style: WidgetStyleParam's, nothing of its own */
class AppIconBaseStyleParam : public WidgetStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	AppIconBaseStyleParam();
	virtual const char *GetParamName() const;
};

}
}

#endif
