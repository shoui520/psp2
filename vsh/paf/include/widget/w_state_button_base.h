/*
 * paf::ui::StateButtonBase: a button that is checked or not, the base of the check box and the
 * radio button. It has an image (0) whose texture animation shows the check, a text label (1),
 * a highlight (2) and a glow (3); it sizes itself to them in two dimensions where it adjusts, and
 * plays its "check" and "uncheck" sounds as the check changes. With its resource, style and sound
 * parameters and the style animator that lays its label out. Names after ScePaf-RE (restored
 * from Sony's DWARF) and the exports (CreateDrawObj and the sound parameter's by NID hash); the
 * members and functions Sony's records do not name are ours.
 */
#ifndef PAF_WIDGET_W_STATE_BUTTON_BASE_H
#define PAF_WIDGET_W_STATE_BUTTON_BASE_H

#include <stdint.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <math/ngp/v2.h>
#include <widget/core/layouter.h>
#include <widget/w_button_base.h>

namespace paf {
namespace ui {

class StateButtonBase : public ButtonBase {
public:
	enum {
		OBJ_IMAGE = 0,
		OBJ_STBTN_END = 1,
		/* the label, the highlight and the glow (names ours) */
		OBJ_LABEL = 1,
		OBJ_HIGHLIGHT = 2,
		OBJ_GLOW = 3
	};

	enum {
		CB_STBTN_BEGIN = 0x1000000A,
		CB_STBTN_VALUE_CHANGED = 0x1000000B,
		CB_STBTN_END = 0x1000000C
	};

	/* the indexes of the sounds its sound parameter adds, "check" and "uncheck" (names ours) */
	enum {
		SOUND_CHECK = 6,
		SOUND_UNCHECK = 7
	};

	static const char m_widget_type[];
	static const char *TypeName();

	StateButtonBase(Widget *parent, CreateParam *param);

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void UpdateAdjustSize();

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

	/* Widget's, or only down while its focus directions are off */
	virtual int32_t GetFocusDirectionMask()
	{
		int32_t mask = Widget::GetFocusDirectionMask();

		if (mask != 0)
			return mask;
		return 2;
	}

	virtual bool IsTextEventEnabled()
	{
		return true;
	}

	/* ButtonBase's, and decided as it is pressed (tuck) or decided */
	virtual int32_t DoButtonEvent(int32_t type, Event *e);
	virtual int32_t SetTextAutoScrollOrigin(const math::v2 &pos);
	virtual int32_t GetTextAutoScrollOrigin(math::v2 &pos);
	virtual graph::DrawObj *CreateDrawObj(int32_t obj);
	/* checked or not, the image's animation played over the time, and its sound played with
	   ch_anim */
	virtual void SetCheck(bool onoff, float time = 0.0f, bool ch_anim = false);
	virtual void OnButtonDecide();
	/* its size to its draw objects and children, in x and y (Button's names) */
	virtual void UpdateButtonSize();
	/* its label laid out: false while the label's bounds are not ready */
	virtual bool UpdateObjsSize();

	/* the image's texture animation, forward as it is checked */
	void DoTexAnim(bool onoff, float time);
	/* an event of the type sent with the four values */
	void SendEvent(int32_t type, int32_t v0, int32_t v1, int32_t v2, int32_t v3);

	int32_t GetFrameNum()
	{
		return m_frame_num;
	}

	float GetFrameTime()
	{
		return m_frame_time;
	}

	bool IsChecked()
	{
		return m_is_checked;
	}

	/* the size on the axes adjusted to fit (2 and 3) for the fit size and offset given, and for
	   its children and its draw objects with the margins (names ours) */
	math::v2 GetLayoutSize(const math::v2 &fit_size, const math::v2 &offs) const;
	math::v2 GetChildrenFitSize();
	math::v2 GetFitSize() const;

	bool m_is_checked;
	/* the image's texture animation: its frames and the time of each (the style's "frame_num",
	   "frame_time") */
	int32_t m_frame_num;
	float m_frame_time;
	/* decided as it is pressed, not as it is released (the parameter's "tuck") */
	bool m_tuck;
	/* where the text auto scroll's press began (name ours) */
	math::v2 m_text_auto_scroll_origin;
};

/* a StateButtonBase's resource parameters: its label, and whether it is decided as it is
   pressed (class name ours, after its widget's) */
class StateButtonBaseParam : public ButtonBaseParam {
public:
	static const char m_param_name[];

	StateButtonBaseParam();
	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);

	bool m_tuck;
};

/* a StateButtonBase's style: its four draw objects, its adjust and its texture animation */
class StateButtonBaseStyleParam : public ButtonBaseStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	StateButtonBaseStyleParam();
	virtual const char *GetParamName() const;
	virtual ~StateButtonBaseStyleParam();
	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(Widget *widget);
	virtual int32_t GetEach(Widget *widget);

	graph::PlaneObjParam m_image_obj;
	graph::TextObjParam m_label_obj;
	graph::PlaneObjParam m_highlight_obj;
	graph::PlaneObjParam m_glow_obj;
	AdjustParam *m_adjust;
	int32_t m_frame_num;
	float m_frame_time;
};

/* a ButtonBase's sounds and its own two, "check" and "uncheck" */
class StateButtonBaseSoundParam : public ButtonBaseSoundParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	StateButtonBaseSoundParam();
	virtual const char *GetParamName() const;
};

/* lays a state button's label out when its adjust changes or it is marked dirty (class name
   ours) */
class StateButtonBaseStyleAnimator : public Animator {
public:
	StateButtonBaseStyleAnimator() : Animator(NULL, NULL)
	{
	}

	virtual ~StateButtonBaseStyleAnimator()
	{
	}

	virtual int32_t OnUpdate_core()
	{
		uint32_t adjust_x = m_widget->m_adjust_x;
		uint32_t adjust_y = m_widget->m_adjust_y;

		if (adjust_x != m_adjust_x || adjust_y != m_adjust_y) {
			m_adjust_x = adjust_x;
			m_adjust_y = adjust_y;
			SetDirty(1);
		}
		if (m_dirty != 0) {
			graph::DrawObj::EnterSafety();
			if (((StateButtonBase *)m_widget)->UpdateObjsSize())
				m_dirty = 0;
			graph::DrawObj::FinishSafety();
		}
		m_first = 0;
		return 0;
	}

	/* the widget's adjust as it was at the last layout (names ours) */
	uint32_t m_adjust_x;
	uint32_t m_adjust_y;
};

}
}

#endif
