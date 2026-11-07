/*
 * paf::ui::DirectBar: a bar of buttons, one per entry of its comma-separated string, held at its
 * launch button and swept by a touch or the direct keys: a balloon follows the entry under the
 * touch and the bar sends the entry chosen when the touch is let go. With its resource and style
 * parameters. Names after the exports (SendEvent, OrientDragOutState, OrientDragState,
 * OrientNormalState, GetPartsSize, GetHitNumber, UpdateDBarLayout, GetBtnNum, and Show, Hide,
 * SetLaunchBtnY and GetBtnString by NID hash) and ScePaf-RE (CB_DIRECT_BAR_BEGIN to _END, m_btnNum);
 * the parameter members after their resource names. The other members, the states and the
 * functions no export names are ours.
 */
#ifndef PAF_WIDGET_W_DIRECTBAR_H
#define PAF_WIDGET_W_DIRECTBAR_H

#include <stdint.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <math/ngp/v2.h>
#include <std/string>
#include <std/vector>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

class HitInfo;

class DirectBar : public Widget {
public:
	enum {
		/* the bar, its entries' text, the marks of its empty entries, the balloon, the
		   balloon's text and the launch button (the style's bg_obj, label_obj, balloon_obj
		   and launchbtn_obj; the other names ours) */
		OBJ_BG = 0,
		OBJ_LABEL = 1,
		OBJ_BLANK_MARK = 2,
		OBJ_BALLOON = 3,
		OBJ_BALLOON_LABEL = 4,
		OBJ_LAUNCHBTN = 5,
		OBJ_DIRECT_BAR_END = 6
	};

	enum {
		CB_DIRECT_BAR_BEGIN = 0x10000000,
		CB_DIRECT_BAR_DECIDE = 0x10000001,
		CB_DIRECT_BAR_PRESS = 0x10000002,
		CB_DIRECT_BAR_DRAG_OUT = 0x10000003,
		CB_DIRECT_BAR_END = 0x10000004,
		/* let go with no entry chosen, held by the direct keys, and let go (names ours) */
		CB_DIRECT_BAR_CANCEL = 0x10000005,
		CB_DIRECT_BAR_KEY_HOLD = 0x10000006,
		CB_DIRECT_BAR_RELEASE = 0x10000007
	};

	/* what m_state holds: shown, held, dragged out of the bar, closed to its launch button,
	   hidden, and not laid out yet (names ours) */
	enum {
		ST_DBAR_OPEN = 0,
		ST_DBAR_HOLD = 1,
		ST_DBAR_DRAG_OUT = 2,
		ST_DBAR_CLOSED = 3,
		ST_DBAR_HIDDEN = 4,
		ST_DBAR_NONE = 5
	};

	DirectBar(Widget *parent, CreateParam *param);

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;

	static const char m_widget_type[];
	static const char *TypeName();

	virtual int32_t HandleKeycodeEvent(Event *e);
	virtual int32_t HandleTouchEvent(Event *e);
	virtual int32_t HandleFocusEvent(Event *e);
	virtual void DestroyWidget();
	virtual uint32_t CheckState();
	virtual void UpdateSize();
	virtual void Update();
	virtual int32_t OnDraw(graph::DrawInfo &info);
	virtual void OnTouchCaptureChanged(uint32_t port, uint32_t id, int32_t arg);
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);
	virtual int32_t SetString(const wstring &str, int32_t obj);
	virtual int32_t GetString(wstring &str, int32_t obj);

	/* up and down always */
	virtual int32_t GetFocusDirectionMask()
	{
		int32_t mask = Widget::GetFocusDirectionMask();

		if (mask == 0)
			return 2;
		return mask;
	}

	virtual ~DirectBar();
	virtual int32_t SetObjColor(const math::v4 &color, int32_t obj);
	virtual int32_t GetObjColor(math::v4 &color, int32_t obj);
	/* shown or hidden over the time, open or closed by its bar type */
	virtual int32_t Show(float time);
	virtual int32_t Hide(float time);

	void SendEvent(int32_t type, int32_t v0, int32_t v1, int32_t v2, int32_t v3);
	void OrientDragOutState(Event *e);
	void OrientDragState(Event *e);
	void OrientNormalState(Event *e);
	math::v4 GetPartsSize();
	int32_t GetHitNumber(HitInfo *info);
	void UpdateDBarLayout();
	int32_t GetBtnNum();
	int32_t GetBtnString(wstring &str, int32_t index);
	void SetLaunchBtnY(float y);

	/* its draw objects animated into the state over the time (name ours) */
	void AnimDBarState(int32_t state, float time);

	/* the state, its draw objects animated into it once over the time (name ours) */
	void SetDBarState(int32_t state, float time = 240.0f)
	{
		if (m_state != state) {
			AnimDBarState(state, time);
			m_state = state;
		}
	}

	/* open while it has a bar type, else closed (name ours) */
	void SetDBarIdleState(float time = 240.0f)
	{
		SetDBarState((m_type & 0xf0) != 0 ? ST_DBAR_OPEN : ST_DBAR_CLOSED, time);
	}

	/* the event sent with nothing (name ours) */
	void SendDBarEvent(int32_t type)
	{
		Event e(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

		DoEvent(type, &e);
	}

	/* the event sent with an entry's index (name ours) */
	void SendDBarEvent(int32_t type, int32_t index)
	{
		Event e(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, index);

		DoEvent(type, &e);
	}

	/* the release event sent once the touch or the keys let go (name ours) */
	void ReleaseDBar()
	{
		if (m_touch_id >= 0)
			SendDBarEvent(CB_DIRECT_BAR_RELEASE);
		m_touch_id = -1;
	}

	/* where the balloon sits by the index-th entry (name ours) */
	math::v4 GetBalloonPos(int32_t index);
	/* whether the index-th entry can be chosen: its color not the disabled one (name ours) */
	bool IsBtnEnable(int32_t index);
	/* from the index, the nearest entry that can be chosen in the key's direction (name ours) */
	bool FindEnableBtn(int32_t *index, uint32_t key);
	/* the focus frame laid on the launch button, or on the bar (name ours) */
	void UpdateFocusShape();
	/* the touch down of a closed bar (name ours) */
	void OrientClosedState(Event *e);

	/* the balloon's move, kept beside the entry while it is clamped (name ours) */
	static void OnBalloonMove(Timer *t, AnimListener *listener, void *data1, void *data2,
				  void *data3);

	int32_t m_state;
	/* where the touch that holds it went down */
	math::v2 m_touch_pos;
	math::v2 m_hit_pos;
	/* the balloon type (the low four bits: 1 the balloon on the right) and the bar type (the
	   high four: 2 the bar sized to the launch button) */
	uint32_t m_type;
	/* the touch that holds it, -1 none */
	int32_t m_touch_id;
	float m_hit_margin;
	int32_t m_btnNum;
	/* the entry chosen, the first entry shown and the first empty entry, -1 none */
	int32_t m_select;
	int32_t m_top;
	int32_t m_blank;
	/* its entries, a line each */
	wstring m_label;
	/* the label's lines laid out over the bar once */
	bool m_layout_done;
	float m_launchbtn_y;
	/* the hit box while it is closed: its center offset, and its size */
	float m_closed_hit_x;
	float m_closed_hit_y;
	float m_closed_hit_height;
	float m_closed_hit_width;
	/* the color of an entry that cannot be chosen */
	math::v4 m_disable_color;
	/* the direct key presses repeated */
	int32_t m_repeat_count;
	/* the balloon held at the end of the bar */
	bool m_balloon_clamped;
	/* its entries */
	vector<wstring> m_btn_strings;
};

/* a DirectBar's resource parameters: its entries ("label"), its hit margin, the launch button's
   position, and its balloon and bar types */
class DirectBarParam : public WidgetParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	DirectBarParam();
	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);

	wstring m_label;
	float m_hitmargin;
	float m_launchbtn_y;
	uint32_t m_balloon_type;
	uint32_t m_bar_type;
};

/* a DirectBar's style: its bar, its label, the balloon and the launch button */
class DirectBarStyleParam : public WidgetStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	DirectBarStyleParam();
	virtual const char *GetParamName() const;
	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(Widget *widget);
	virtual int32_t GetEach(Widget *widget);

	graph::PlaneObjParam m_bg_obj;
	graph::TextObjParam m_label_obj;
	graph::PlaneObjParam m_balloon_obj;
	graph::PlaneObjParam m_launchbtn_obj;
};

}
}

extern "C" {
/* the balloon and bar types: the text, the balloon's side and the launch button laid out for
   them (no name found) */
void ScePafWidget_0926399C(paf::ui::DirectBar *bar, uint32_t type);
/* the index-th entry chosen: the balloon moved to it over the time, from a direct key (key)
   (no name found) */
void ScePafWidget_A0CEFD28(paf::ui::DirectBar *bar, int32_t index, bool key, float time);
/* the selection let go: the balloon faded out (no name found) */
void ScePafWidget_0E46CEE0(paf::ui::DirectBar *bar);
/* the balloon's text set to one line, the bar left idle (no name found) */
void ScePafWidget_7C2ABBCB(paf::ui::DirectBar *bar);
/* its entries (GetBtnNum's twin; no name found) */
int32_t ScePafWidget_8D86C86E(paf::ui::DirectBar *bar);
}

#endif
