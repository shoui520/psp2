/*
 * paf::ui::Text: a widget drawn as a text object (draw object 0) and, where it has a texture, a
 * plane object (1); its links, its auto scroll with its friends, its touch selection; its
 * resource, style and sound parameters. Names after the exports and ScePaf-RE (restored from
 * Sony's DWARF), the resource's parameter names ("selection_mode", "auto_link_detect", ...); the
 * functions and members no export names are ours.
 */
#ifndef PAF_WIDGET_W_TEXT_H
#define PAF_WIDGET_W_TEXT_H

#include <stdint.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <graphics/text/attributed_string.h>
#include <graphics/text/text_layout.h>
#include <graphics/text/text_style.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/string>
#include <std/vector>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>

/* a text not in a touch mode or not touched, or its auto scroll held by a long press or
   already running (name ours) */
#define SCE_PAF_ERROR_BUSY	(-2136014815)	/* 0x80AF0021 */

namespace paf {
namespace ui {

class ScrollView;
class ScrollViewBase;
class TextAutoScroll;
class TextBoxEditMenuListener;

class Text : public Widget {
public:
	enum {
		OBJ_TEXT = 0,
		OBJ_PLANE = 1
	};

	enum {
		CB_TEXT_SELECTION_BEGIN = 0x10000000,
		CB_TEXT_SELECTION_END = 0x10000001,
		CB_TEXT_SELECTION_POS_CHANGED = 0x10000002,
		/* a link tapped (ScePaf-RE's CB_TEXT_END; name ours) */
		CB_TEXT_LINK = 0x10000003
	};

	/* the text's animations (names after ScePaf-RE; 0x1000D, the auto scroll's fade back,
	   ours) */
	enum {
		ANIM_AUTO_SCROLL = 0x1000C,
		ANIM_AUTO_SCROLL_FADE = 0x1000D,
		ANIM_CARET_SCROLL = 0x1000E,
		ANIM_CARET_BLINK = 0x1000F
	};

	/* how StartAutoScroll scrolls: over and over, or (3 and 4, names ours) once, as long
	   as a long press holds it */
	enum ScrollType {
		SCROLL_TYPE_HORIZONTAL = 1,
		SCROLL_TYPE_VERTICAL = 2,
		SCROLL_TYPE_HORIZONTAL_ONCE = 3,
		SCROLL_TYPE_VERTICAL_ONCE = 4
	};

	/* the link styles: a link, an automatically found one, a pressed and a focused one
	   (names ours) */
	enum {
		LINK_STYLE_NORMAL = 0,
		LINK_STYLE_AUTO = 1,
		LINK_STYLE_PRESSED = 2,
		LINK_STYLE_FOCUSED = 3,
		LINK_STYLE_NUM = 4
	};

	/* what FriendAutoScroll does to each of its auto scroll's friends (names ours) */
	enum {
		FRIEND_START = 0,
		FRIEND_STOP = 1,
		FRIEND_IS_RUNNING = 2,
		FRIEND_SET_ORIGIN = 3,
		FRIEND_GET_SPEED_BY_POS = 4,
		FRIEND_SET_SPEED = 5
	};

	/* FRIEND_GET_SPEED_BY_POS's argument: TextAutoScroll::GetSpeedByPos's (name ours) */
	class FriendSpeedArg {
	public:
		math::v2 pos;
		float speed;
		bool set_origin;
		bool reverse;
	};

	/* a link in its string: the range, whether the auto link detection found it, and the URL
	   scheme bit it was found by (names ours) */
	class Link {
	public:
		Link(uint32_t index, uint32_t len, bool is_auto, uint16_t scheme)
			: m_index(index), m_len(len), m_auto(is_auto), m_scheme(scheme)
		{
		}

		virtual ~Link()
		{
		}

		uint32_t m_index;
		uint32_t m_len;
		bool m_auto;
		uint16_t m_scheme;
	};

	static const char m_widget_type[];
	static const char *TypeName();

	Text(Widget *parent, CreateParam *param);
	virtual ~Text();

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;

	/* whether its link event is on, with a link callback and links (name ours) */
	virtual bool IsTextEventEnabled()
	{
		bool links = m_links.size() != 0;
		bool on = false;

		if (m_map_e_listener.find(CB_TEXT_LINK) != m_map_e_listener.end() && links)
			on = true;
		return m_link_event && on;
	}

	/* sets the touch modes from its touch modes and its focused link (0x8123aa94, name
	   ours) */
	virtual void UpdateTouchMode()
	{
		int32_t mode = 4;

		if (m_focus_link != NULL || m_touch_modes == 0)
			mode = 1;
		if (m_touch_gesture_mask & 1)
			mode |= 1;
		else if (m_touch_gesture_mask & 4)
			mode |= 4;
		m_touch_gesture_mask = mode;
	}

	/* replaces len characters from index with the string (0x8113187e, name after
	   graph::TextObj's) */
	virtual int32_t ReplaceString(uint32_t index, uint32_t len, const wstring &str);
	virtual int32_t MoveCaretUp();
	virtual int32_t MoveCaretDown();
	virtual int32_t MoveCaretLeft();
	virtual int32_t MoveCaretRight();
	virtual int32_t SetCaretPos(const math::v2 &pos);
	virtual int32_t SetSelectionPos(const math::v2 &start, const math::v2 &end);
	virtual int32_t SetCaretIndex(uint32_t index);
	virtual int32_t SetSelectionRange(uint32_t index, uint32_t len);
	virtual int32_t SetCaretVisible(bool visible);
	virtual int32_t HandleTouchPress(Event *e);
	virtual int32_t HandleTouchRelease();
	/* a tap */
	virtual int32_t ScePafWidget_695200BA(Event *e);
	virtual int32_t UpdateTouch(const math::v2 &pos);
	virtual int32_t SetVisiblePos(const math::v2 &pos);
	virtual int32_t AdjustSize();

	virtual void DestroyWidget();
	virtual int32_t HandleTouchEvent(Event *e);
	virtual int32_t HandleGestureEvent(Event *e);

	/* the link styles deleted (0x8112ef16, name ours) */
	int32_t DeleteLinkStyles();
	/* its auto scroll made where it may have one, and started (0x8112f050, name ours) */
	int32_t RunAutoScroll(ScrollType type);
	/* the operation done to each friend of its auto scroll found among its parent's
	   children, other than itself: FRIEND_IS_RUNNING returns whether any one's runs,
	   FRIEND_GET_SPEED_BY_POS the first running one's (0x8112f114, name ours) */
	int32_t FriendAutoScroll(int32_t op, void *arg);
	/* the touch's position in its text's layout, the hit found on the touch's line where
	   it is not yet (0x8112f482, name ours) */
	int32_t GetTouchTextPos(uint32_t port, uint32_t id, math::v2 &pos);
	int32_t HandleTouchMove(Event *e);
	/* its touch begun at the position: the edit menu told, the selection begun, a long
	   press's auto scroll started (here and in its friends) */
	int32_t BeginTouch(const math::v2 &pos);
	/* its auto scroll there and running (inlined in Sony's code, name ours) */
	bool IsAutoScrollRunning();

	int32_t SetStyleAttribute(graph::TextStyleAttribute attr, uint32_t index, uint32_t len,
				  const math::v4 &val);
	int32_t SetStyleAttribute(graph::TextStyleAttribute attr, uint32_t index, uint32_t len,
				  bool val);
	int32_t SetLayoutAttribute(graph::TextLayoutAttribute attr, int32_t val);
	int32_t SetLayoutAttribute(graph::TextLayoutAttribute attr, bool val);
	int32_t GetLayoutAttribute(graph::TextLayoutAttribute attr, int32_t &val);
	/* its text object's bounds */
	int32_t GetBounds(math::v2 &bounds);
	/* only returns the error 0x80AF5004 */
	int32_t SetSelectionMode(bool mode);
	int32_t GetLength(uint32_t &len);
	/* its text object's string with its style runs; 0x80AF0022 without a text object */
	int32_t GetAttributedString(graph::AttributedString &str);
	int32_t GetCaretIndex(uint32_t &index);
	int32_t GetSelectionRange(uint32_t &index, uint32_t &len);
	int32_t GetSelectionMode(bool &mode);
	int32_t GetCharInfo(uint32_t index, graph::TextLayoutCharInfo &info);
	int32_t GetLineInfo(uint32_t line, graph::TextLayoutLineInfo &info);
	int32_t GetLineCount(uint32_t &count);
	int32_t GetStyleAttribute(graph::TextStyleAttribute attr, uint32_t index, float &val);
	int32_t GetLayoutAttribute(graph::TextLayoutAttribute attr, math::v2 &val);
	/* its auto scroll running, or its animation 2 (0x81132452, name ours) */
	bool IsScrolling();
	int32_t StartAutoScroll(ScrollType type);
	int32_t StopAutoScroll();
	/* sends the event with the rectangle, as integers (0x8113262e, name ours) */
	int32_t SendRectEvent(int32_t type, const math::v4 &rect);
	/* the style at the index of its text */
	int32_t GetStyle(uint32_t index, graph::TextStyle &style);
	/* a long press of the touch in the event: the touch begun at its position, the auto
	   scroll's origin set there where a touch moves it, and the touch held (0x8112f85c; name
	   ours) */
	int32_t HandleLongPress(Event *e);

	/* the plane, draw object 1, made where it is given a texture */
	graph::PlaneObj *m_plane;
	/* the text, draw object 0 */
	graph::TextObj *m_text;
	/* the scroll view it keeps its caret in sight in ("scroll_view"; name ours) */
	ScrollViewBase *m_scroll_view;
	union {
		struct {
			/* "selection_mode" */
			bool m_selection_mode : 1;
			/* "long_press_text_auto_scroll_mode": a long press scrolls it, its origin
			   following the touch (ScePafWidget_222D55C7 reads it; name ours) */
			bool m_touch_auto_scroll : 1;
			/* a text box's paste enabled and edit mode (names ours) */
			bool m_paste_enabled : 1;
			bool m_edit_mode : 1;
		};
		/* any of its touch modes (name ours) */
		int8_t m_touch_modes;
	};
	union {
		struct {
			/* a touch press holds it (HandleLongPress; name ours) */
			bool m_touch_held : 1;
			bool m_touch_state1 : 1;
			bool m_touch_state2 : 1;
		};
		int8_t m_touch_states;
	};
	/* where the touch began (BeginTouch; name ours) */
	math::v2 m_touch_pos;
	/* "auto_link_detect": bit 0 finds URLs, bit 1 (name ours) */
	uint8_t m_auto_link_detect;
	/* "url_scheme": the URL schemes found, a bit each */
	int16_t m_url_scheme;
	graph::TextStyle *m_link_styles[LINK_STYLE_NUM];
	/* the link the focus or touch is on (name ours) */
	Link *m_focus_link;
	vector<Link> m_links;
	/* not "disable_auto_scroll", and the link event on (names ours) */
	bool m_auto_scroll_enable : 1;
	bool m_link_event : 1;
	int32_t m_link_mode;
	TextAutoScroll *m_auto_scroll;
	/* what it tells about a selection or an edit (name ours) */
	TextBoxEditMenuListener *m_edit_menu_listener;
	/* its listener on its scene's event 0x80000A (name ours) */
	EventListener *m_scene_listener;
	uint32_t reserved;
};

/* a text's resource parameters (names after the resource's) */
class TextParam : public WidgetParam {
public:
	static const char *ParamName();

	TextParam();
	virtual const char *GetParamName() const;
	virtual ~TextParam();
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);

	bool m_selection_mode;
	int32_t m_auto_link_detect;
	int32_t m_url_scheme;
	int32_t m_auto_scroll;
	float m_auto_scroll_speed;
	IDParam m_placeholder;
	IDParam m_scroll_view;
	bool m_long_press_text_auto_scroll_mode;
	IDParam m_auto_scroll_target_friend;
	bool m_disable_auto_scroll;
};

/* a text's style: its plane and text objects' parameters and how it adjusts its size */
class TextStyleParam : public WidgetStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	TextStyleParam();
	virtual const char *GetParamName() const;
	virtual ~TextStyleParam();
	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(Widget *widget);
	virtual int32_t GetEach(Widget *widget);

	graph::PlaneObjParam m_plane_param;
	graph::TextObjParam m_text_param;
	AdjustParam *m_adjust;
};

/* a text's sounds: its link tapped (class name ours) */
class TextSoundParam : public WidgetSoundParam {
public:
	static const char *ParamName();

	TextSoundParam();
	virtual const char *GetParamName() const;
	virtual ~TextSoundParam();
};

}
}

extern "C" {
/* the friends of its auto scroll; 0x80AF0022 without one */
int32_t ScePafWidget_ADBA776F(paf::ui::Text *text, paf::vector<paf::IDParam> &friends);
/* the speed for a press at the position: its auto scroll's where it runs, else its first
   running friend's (TextAutoScroll::GetSpeedByPos) */
int32_t ScePafWidget_5B2F5119(paf::ui::Text *text, paf::math::v2 pos, float &speed,
			      bool set_origin, bool reverse);
/* the origin of its auto scroll and its friends' set to the position */
int32_t ScePafWidget_8A5AE1CD(paf::ui::Text *text, const paf::math::v2 &pos);
/* whether a long press scrolls it ("long_press_text_auto_scroll_mode") */
int32_t ScePafWidget_222D55C7(paf::ui::Text *text, bool *on);
/* its auto scroll's origin */
int32_t ScePafWidget_4608F7EF(paf::ui::Text *text, paf::math::v2 *origin);
/* its auto scroll's and its friends' speed (Text::SetAutoScrollSpeed(float), kept by its link
   name for store_browser's import) */
int32_t ScePafWidget_7C976DF2(paf::ui::Text *text, float speed);
/* its string buffer and its length (Text::GetStringBuffer, by graph::TextObj's) */
int32_t ScePafWidget_62604965(paf::ui::Text *text, const wchar_t **buf, uint32_t *len);
/* prototypes from common_gui_dialog's calls, not written in C yet (no name found) */
void ScePafWidget_B2A7DF66(paf::ui::Text *text, int32_t state, const paf::graph::TextStyle &style);
void ScePafWidget_4D5F35D5(paf::ui::Text *text, uint32_t a, uint32_t b);
}

#endif
