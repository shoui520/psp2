/*
 * paf::ui::Button: a button with a background plane, a text label, a highlight and a glow; it
 * sizes itself to its label and children where it adjusts, plays a texture animation as it is
 * pressed and released, and scrolls its label on a long press. With its resource, style and
 * handler parameters and the style animator that lays its objects out. Names after ScePaf-RE
 * (restored from Sony's DWARF) and the exports; the members and the virtual functions Sony's
 * records do not name are ours, after the style's parameters where they have one.
 */
#ifndef PAF_WIDGET_W_BUTTON_H
#define PAF_WIDGET_W_BUTTON_H

#include <stdint.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <widget/core/layouter.h>
#include <widget/text_auto_scroll.h>
#include <widget/w_button_base.h>

namespace paf {
namespace ui {

class Button : public ButtonBase {
public:
	enum {
		OBJ_BG = 0,
		OBJ_LABEL = 1,
		OBJ_BTN_END = 2,
		/* the highlight and the glow (names ours) */
		OBJ_HIGHLIGHT = 67,
		OBJ_GLOW = 68
	};

	enum StyleType {
		MIN_STYLE_TYPE = 0,
		STYLE_MANUAL = 0,
		STYLE_AUTO = 1,
		NUM_STYLE_TYPES = 2,
		MAX_STYLE_TYPE = 1
	};

	static const char m_widget_type[];
	static const char *TypeName();

	Button(Widget *parent, CreateParam *param);
	virtual ~Button();

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void UpdateAdjustSize();
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf);

	/* the label's */
	virtual int32_t SetString(const wstring &str)
	{
		return SetString(str, OBJ_LABEL);
	}

	using ButtonBase::SetString;

	virtual int32_t GetString(wstring &str)
	{
		return GetString(str, OBJ_LABEL);
	}

	using ButtonBase::GetString;

	virtual int32_t SetTextAutoScrollOrigin(const math::v2 &pos);
	virtual int32_t GetTextAutoScrollOrigin(math::v2 &pos);
	virtual int32_t GetTextAutoScrollSpeedByPos(const math::v2 &pos, float &speed,
						    bool set_origin, bool reverse);
	virtual int32_t StartTextAutoScroll(Text::ScrollType type);
	virtual int32_t StopTextAutoScroll();
	virtual int32_t SetTextAutoScrollSpeed(float speed);
	virtual graph::DrawObj *CreateDrawObj(int32_t obj);
	virtual uint32_t ChangeButtonState(ButtonState state);
	virtual bool UpdateObjsSize();
	virtual void UpdateButtonSize();
	virtual void SetStyleAnimator(StyleType style_type);

	/* the texture animation of the background, forward as it is pressed (export 81FC2E6A) */
	void DoTexAnim(bool forward);
	/* whether its label or a text among its children scrolls (export AC592A89) */
	int32_t IsTextAutoScroll();
	void GetChildrenFitGeometry(math::v4 *size, math::v4 *offs);
	void GetFitGeometry(math::v4 *size, math::v4 *offs) const;
	math::v4 GetLayoutSize(const math::v4 &fit_size, const math::v4 &offs) const;

	StyleType GetStyleType() const
	{
		return m_style_type;
	}

	/* cleared by the constructor; nothing in Button's file reads them (names ours) */
	uint32_t m_unused0 : 4;
	uint32_t m_unused1 : 4;
	uint32_t m_unused2 : 4;
	StyleType m_style_type;
	/* the background's texture animation: its frames and the time of each (the style's
	   "frame_num", "frame_time") */
	int32_t m_frame_num;
	float m_frame_time;
	/* the label's auto scroll, made when it first starts */
	TextAutoScroll *m_text_auto_scroll;
};

/* a Button's resource parameters: the long press that scrolls its label */
class ButtonParam : public ButtonBaseParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	ButtonParam();
	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);

	bool m_enable_long_press_text_auto_scroll;
	/* -1: 500 */
	int32_t m_long_press_text_auto_scroll_begin_msec;
};

/* a Button's style: its four draw objects, how it lays them out and its texture animation */
class ButtonStyleParam : public ButtonBaseStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	ButtonStyleParam();
	virtual const char *GetParamName() const;
	virtual ~ButtonStyleParam();
	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(Widget *widget);
	virtual int32_t GetEach(Widget *widget);

	int32_t Init();

	graph::PlaneObjParam m_bg_obj;
	graph::TextObjParam m_label_obj;
	graph::PlaneObjParam m_highlight_obj;
	graph::PlaneObjParam m_glow_obj;
	Button::StyleType m_style_type;
	AdjustParam *m_adjust;
	int32_t m_frame_num;
	float m_frame_time;
};

class ButtonHandlerParam : public ButtonBaseHandlerParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	ButtonHandlerParam()
	{
	}

	virtual const char *GetParamName() const;
};

/* lays a widget out when its adjust changes or its animator is marked dirty */
class StyleAnimator : public Animator {
public:
	StyleAnimator() : Animator(NULL, NULL)
	{
	}

	virtual ~StyleAnimator()
	{
	}

	virtual int32_t OnUpdate_core();
	/* nonzero once laid out (name ours) */
	virtual int32_t Layout() = 0;

	/* the widget's adjust, and as it was at the last layout (names ours) */
	uint32_t m_adjust_x;
	uint32_t m_adjust_y;
	uint32_t m_adjust_z;
	uint32_t m_last_adjust_x;
	uint32_t m_last_adjust_y;
	uint32_t m_last_adjust_z;
};

/* a button's: its objects sized to its label (class name ours) */
class ButtonStyleAnimator : public StyleAnimator {
public:
	ButtonStyleAnimator()
	{
	}

	virtual ~ButtonStyleAnimator()
	{
	}

	virtual int32_t Layout()
	{
		int32_t ret = 1;
		Button *button = m_widget != NULL && m_widget->IsInherit(Button::m_widget_type) ?
					 (Button *)m_widget :
					 NULL;

		if (button != NULL) {
			graph::DrawObj::EnterSafety();
			ret = button->UpdateObjsSize();
			graph::DrawObj::FinishSafety();
		}
		return ret;
	}
};

}
}

extern "C" {
/* a text's auto scroll origin, speed by position and speed (Text's; no names found; 7C976DF2
   is Text::SetAutoScrollSpeed(float), kept by its link name for store_browser's import) */
int32_t ScePafWidget_8A5AE1CD(paf::ui::Text *text, const paf::math::v2 &pos);
int32_t ScePafWidget_5B2F5119(paf::ui::Text *text, paf::math::v2 pos, float &speed,
			      bool set_origin, bool reverse);
int32_t ScePafWidget_7C976DF2(paf::ui::Text *text, float speed);
}

#endif
