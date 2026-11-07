/*
 * paf::ui::ButtonBase: what every button shares, its states, the events it sends, its glow and
 * the text auto scroll a long press starts; and its resource, style and handler parameters.
 * Names after ScePaf-RE (restored from Sony's DWARF) and the exports; the virtual functions and
 * members Sony's records do not name are ours, after what its file (w_button_base.cpp) does with
 * them.
 */
#ifndef PAF_WIDGET_W_BUTTON_BASE_H
#define PAF_WIDGET_W_BUTTON_BASE_H

#include <stdint.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/list>
#include <std/string>
#include <widget/core/event.h>
#include <widget/core/widget.h>
#include <widget/w_text_box.h>

/* a text auto scroll a long press holds, or one already running (name ours) */
#define SCE_PAF_ERROR_BUTTON_BUSY	(-2136014815)	/* 0x80AF0021 */

namespace paf {
namespace ui {

class ButtonBase : public Widget {
public:
	enum {
		CB_BTN_BEGIN = 0x10000000,
		CB_BTN_NORMAL = 0x10000001,
		CB_BTN_DISABLE = 0x10000002,
		CB_BTN_PRESS = 0x10000003,
		CB_BTN_PRESS_OUT = 0x10000004,
		CB_BTN_PRESS_REPEAT = 0x10000005,
		CB_BTN_PRESS_REPEAT_END = 0x10000006,
		CB_BTN_LONG_PRESS = 0x10000007,
		CB_BTN_DECIDE = 0x10000008,
		CB_BTN_MULTI_TAP = 0x10000009,
		CB_BTN_END = 0x1000000a
	};

	enum {
		BTN_MODE_BASIC = 0,
		BTN_MODE_PRESSOVER = 1
	};

	enum ButtonState {
		ST_BTN_NORMAL = 0,
		ST_BTN_DISABLE = 1,
		ST_BTN_PRESS = 2,
		ST_BTN_PRESS_OUT = 3,
		ST_BTN_PRESS_REPEAT = 4
	};

	/* the indexes of its sounds (its sound parameter's names: "press", "move_out",
	   "long_press", "decide", "multi_tap"; names ours) */
	enum {
		SOUND_PRESS = 1,
		SOUND_MOVE_OUT = 2,
		SOUND_LONG_PRESS = 3,
		SOUND_DECIDE = 4,
		SOUND_MULTI_TAP = 5
	};

	/* the glow's animations (SetGlowAnim; names ours, after what starts them): faded out, hidden
	   at once, up to m_glow_max as it is pressed, to half as it repeats, dimmed as it is
	   pressed out, flashed as it is decided, flashed twice, and the two pulses */
	enum GlowAnim {
		GLOW_OFF = 0,
		GLOW_HIDE = 1,
		GLOW_PRESS = 2,
		GLOW_REPEAT = 3,
		GLOW_PRESS_OUT = 4,
		GLOW_DECIDE = 5,
		GLOW_FLASH = 6,
		GLOW_PULSE = 7,
		GLOW_BREATHE = 8
	};

	static const char m_widget_type[];
	static const char *TypeName();

	ButtonBase(Widget *parent, CreateParam *param);
	virtual ~ButtonBase();

	virtual int32_t HandleKeycodeEvent(Event *e);
	virtual int32_t HandleTouchEvent(Event *e);
	virtual int32_t HandleGestureEvent(Event *e);
	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual uint32_t CheckState();
	virtual void UpdateSize();
	virtual void Update();
	virtual int32_t OnDraw(graph::DrawInfo &info);
	virtual int32_t ApplyDrawObjState(graph::DrawInfo &info);
	virtual void OnTouchCaptureChanged(uint32_t port, uint32_t id, int32_t arg);
	virtual void OnPadCaptureChanged(bool captured, int32_t group, uint32_t port, uint32_t key);
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);
	using Widget::SetTexture;
	virtual int32_t GetTexture(intrusive_ptr<graph::Surface> &surf, int32_t obj, uint32_t index);
	using Widget::GetTexture;
	virtual int32_t SetString(const wstring &str, int32_t obj);
	using Widget::SetString;
	virtual int32_t GetString(wstring &str, int32_t obj);
	using Widget::GetString;
	virtual WidgetSoundParam::Sound *GetSound(int32_t id);
	virtual void AddSound(uint32_t index, WidgetSoundParam::Sound &sound);

	/* it takes the text events where a decide or press handler is set */
	virtual bool IsTextEventEnabled()
	{
		return m_map_e_listener.find(CB_BTN_DECIDE) != m_map_e_listener.end() ||
		       m_map_e_listener.find(CB_BTN_PRESS) != m_map_e_listener.end();
	}

	/* the event sent, as a command dispatched forward without one */
	virtual int32_t DoButtonEvent(int32_t type, Event *e)
	{
		if (e != NULL)
			return DoEvent(type, e);

		Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, 0, 0, 0, 0);

		return DoEvent(type, &ev);
	}

	/* a long press of msec scrolls its text (Button's "enable_long_press_text_auto_scroll") */
	virtual void EnableLongPressTextAutoScroll(uint32_t msec)
	{
		m_enable_long_press = true;
		m_long_press_text_auto_scroll = true;
		m_long_press_msec = msec;
		m_long_press_flag = true;
		SetGestureMask(m_touch_gesture_mask | 4, false);
	}

	virtual void DisableLongPressTextAutoScroll()
	{
		m_enable_long_press = false;
		m_long_press_text_auto_scroll = false;
		SetGestureMask(m_touch_gesture_mask & ~4, false);
	}

	virtual bool IsLongPressTextAutoScrollEnabled()
	{
		return m_long_press_text_auto_scroll;
	}

	/* the text auto scroll started, stopped or sped up, unless a long press holds it */
	virtual int32_t RequestTextAutoScrollStart(Text::ScrollType type)
	{
		int32_t ret = SCE_PAF_ERROR_BUTTON_BUSY;

		if (!IsTextAutoScrollLocked())
			return StartTextAutoScroll(type);
		return ret;
	}

	virtual int32_t RequestTextAutoScrollStop()
	{
		int32_t ret = SCE_PAF_ERROR_BUTTON_BUSY;

		if (!IsTextAutoScrollLocked())
			return StopTextAutoScroll();
		return ret;
	}

	virtual int32_t RequestTextAutoScrollSpeed(float speed)
	{
		int32_t ret = SCE_PAF_ERROR_BUTTON_BUSY;

		if (!IsTextAutoScrollLocked())
			return SetTextAutoScrollSpeed(speed);
		return ret;
	}

	/* where the scroll's press began (exports EC9CD401, 3C06E3D4, 26D767FE on Button) */
	virtual int32_t SetTextAutoScrollOrigin(const math::v2 &pos)
	{
		return 0;
	}

	virtual int32_t GetTextAutoScrollOrigin(math::v2 &pos)
	{
		return 0;
	}

	/* the speed for a press at the position (TextAutoScroll::GetSpeedByPos) */
	virtual int32_t GetTextAutoScrollSpeedByPos(const math::v2 &pos, float &speed,
						    bool set_origin, bool reverse)
	{
		return 0;
	}

	/* a long press holds the scroll */
	virtual bool IsTextAutoScrollLocked()
	{
		return m_text_auto_scroll_locked;
	}

	/* the long press that scrolls the text begins, moves and ends (exports F9B56FFF, A2369014,
	   60E99392) */
	virtual int32_t BeginLongPressTextAutoScroll(Event *e);
	virtual int32_t UpdateLongPressTextAutoScroll(Event *e);
	virtual int32_t EndLongPressTextAutoScroll(Event *e);

	/* exports E297B7B4, A05F07F2, A7D379AD on Button */
	virtual int32_t StartTextAutoScroll(Text::ScrollType type)
	{
		return 0;
	}

	virtual int32_t StopTextAutoScroll()
	{
		return 0;
	}

	virtual int32_t SetTextAutoScrollSpeed(float speed)
	{
		return 0;
	}

	/* the draw object of the ID made (export A97A1993, Sony's name from Button's D941B8C3) */
	virtual graph::DrawObj *CreateDrawObj(int32_t obj);
	virtual uint32_t ChangeButtonState(ButtonState state);

	void SetButtonMode(uint32_t mode);
	void EnableRepeat(uint32_t begin_msec, uint32_t repeat_msec);
	/* the fade to or from the disable color done at once with immediate */
	void Enable(bool immediate);
	void Disable(bool immediate);
	/* the draw object that glows when it is focused, -1 for none (export F02C5605) */
	void SetGlowObj(int32_t obj);
	/* the glow's animation of the type started, kept to restart once it ends with store
	   (export 7602488B, name ours) */
	void SetGlowAnim(int32_t type, bool store);
	/* normal while enabled, else disabled, unless it is already (export F08F954E, name
	   ours) */
	void UpdateButtonState(bool enabled);

	/* enabled, and not below the top modal widget */
	bool IsButtonEnabled() const
	{
		return m_enable && m_active;
	}

	/* the draw object that glows */
	int32_t m_glow_obj;
	/* the glow's largest alpha (the style's "glow_max") */
	float m_glow_max;
	uint32_t m_btn_mode;
	ButtonState m_btn_state;
	/* the color it fades to while disabled: (1, 1, 1, 0.3) without one */
	math::v4 *m_disable_color;
	bool m_enable : 1;
	/* not below the top modal widget (Update) */
	bool m_active : 1;
	/* a long press is reported after m_long_press_msec */
	bool m_enable_long_press : 1;
	bool m_enable_repeat : 1;
	/* stays pressed when the touch leaves it */
	bool m_keep_press : 1;
	bool m_long_press_flag : 1;
	/* the fade to the disable color, 0 to 200 */
	uint32_t m_disable_fade : 8;
	uint32_t reserved : 16;
	uint32_t reserved1 : 2;
	uint16_t m_long_press_msec;
	uint16_t m_repeat_begin;
	uint16_t m_repeat_msec;
	/* set to 5 as a repeat begins */
	uint16_t m_repeat_count;
	/* the glow animation SetGlowAnim restarts as one ends */
	int32_t m_glow_anim;
	/* the sounds given it, by index (AddSound) */
	list<WidgetSoundParam::Sound> m_sound_list;
	/* a long press scrolls the text, and one holds the scroll now (names ours) */
	bool m_long_press_text_auto_scroll : 1;
	bool m_text_auto_scroll_locked : 1;
};

/* a ButtonBase's resource parameters: whether it is enabled ("enable") */
class ButtonBaseParam : public WidgetParam {
public:
	static const char m_param_name[];

	ButtonBaseParam();
	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);

	bool m_enable;
};

/* a ButtonBase's style: its glow, and how the focus and the direct keys find it */
class ButtonBaseStyleParam : public WidgetStyleParam {
public:
	ButtonBaseStyleParam() : m_glow_max(0.85f), m_key_focus(true), m_focus_in_group(true),
				 m_direct_key(0)
	{
	}

	static const char m_param_name[];

	virtual const char *GetParamName() const;
	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(Widget *widget);
	virtual int32_t GetEach(Widget *widget);

	float m_glow_max;
	bool m_key_focus;
	bool m_focus_in_group;
	/* 1 to 5: the key that decides it directly */
	int32_t m_direct_key;
};

/* a ButtonBase's five sounds */
class ButtonBaseSoundParam : public WidgetSoundParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	ButtonBaseSoundParam();
	virtual const char *GetParamName() const;
};

class ButtonBaseHandlerParam : public WidgetHandlerParam {
public:
	static const char m_param_name[];
	static const EventList c_eventList[];
	static const char *ParamName();

	ButtonBaseHandlerParam();
	virtual const char *GetParamName() const;
};

}
}

#endif
