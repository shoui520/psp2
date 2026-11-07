/*
 * paf::ui::ScrollBar: a slider over its base, as long against the base as the visible amount is
 * against the maximum, and placed as the value is; with a jet tail stretched behind the slider
 * and a glow over it. ScrollBarTouch, the one moved by touch, as far as converted C++ uses it.
 * Names after the exports and the parameters' names; the members' names are ours.
 */
#ifndef PAF_WIDGET_W_SCROLL_BAR_H
#define PAF_WIDGET_W_SCROLL_BAR_H

#include <stdint.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v4.h>
#include <std/vector>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

class ScrollBar : public Widget {
public:
	/* its draw objects, after its style's ("base_obj", "slider_obj", "jet_tail_obj",
	   "slider_glow_obj"); all but the base are the slider widget's */
	enum {
		OBJ_BASE = 0,
		OBJ_SLIDER = 1,
		OBJ_JET_TAIL = 2,
		OBJ_SLIDER_GLOW = 3,
		OBJ_SCROLLBAR_END = 4
	};

	/* which way it scrolls (names ours) */
	enum ScrollType {
		SCROLL_TYPE_VERTICAL = 0,
		SCROLL_TYPE_HORIZONTAL = 1
	};

	static const char m_widget_type[];
	static const char *TypeName();

	ScrollBar(Widget *parent, CreateParam *param);
	virtual ~ScrollBar();

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual uint32_t CheckState();
	virtual int32_t SetColor_core(const math::v4 &v);
	virtual void UpdateSize();
	virtual common::SharedPtr<graph::DrawObj> SwapDrawObj(common::SharedPtr<graph::DrawObj> obj,
							      int32_t id);
	virtual graph::DrawObj *GetDrawObj(int32_t obj);
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);
	using Widget::SetTexture;

	/* the slots after Widget's, from 0x170 (index 92) */
	virtual int32_t SetObjColor(const math::v4 &color, int32_t obj);
	virtual int32_t GetObjColor(math::v4 &color, int32_t obj);
	/* the jet tail made, hidden, where the slider has none (name ours, after
	   CreateSliderGlow) */
	virtual void CreateJetTail();
	/* the jet tail shown or hidden (name ours, after ShowGlow) */
	virtual void ShowJetTail(bool show);
	/* the shortest the jet tail is stretched to (name ours) */
	virtual void SetJetTailMinLength(float length);
	virtual void CreateSliderGlow();
	virtual void ShowGlow(int32_t obj, bool show);
	virtual int32_t SetScrollType(ScrollType type);
	virtual int32_t SetSliderSize(const math::v4 &size);
	virtual int32_t SetSliderMinSize(float size);
	virtual int32_t SetMaxValue(float value);
	virtual int32_t SetMaxValue(int32_t value)
	{
		return SetMaxValue((float)value);
	}
	virtual int32_t SetVisibleAmount(float value);
	virtual int32_t SetVisibleAmount(int32_t value)
	{
		return SetVisibleAmount((float)value);
	}
	/* the value, the slider moved to it over its animation with anim */
	virtual int32_t SetValue(float value, bool anim);
	virtual int32_t SetValue(int32_t value, bool anim)
	{
		return SetValue((float)value, anim);
	}
	virtual float GetValue()
	{
		return m_value;
	}
	virtual int32_t SetSliderAnim(float time, int32_t func);
	/* the widget holding the draw object, and the draw object's ID in it */
	virtual Widget *GetPartsWidget(int32_t obj, int32_t *child_obj);
	virtual void UpdateSlider(bool anim);
	/* the jet tail stretched from the slider toward the end it moved from (name ours) */
	virtual void UpdateJetTail();

	/* the visible amount and the value kept in the maximum, and their shares of it */
	void AdjustParameter(bool anim);
	/* the shares made again so the slider is not shorter than its minimum size */
	void AdjustSliderSize();

	void GetSliderAnim(float *time, int32_t *func)
	{
		if (time != NULL)
			*time = m_slider_anim_time;
		if (func != NULL)
			*func = m_slider_anim_func;
	}

	/* the slider widget */
	vector<Widget *> m_parts;
	ScrollType m_scroll_type;
	/* the slider's size; an axis 0 takes the bar's */
	math::v4 m_slider_size;
	float m_slider_min_size;
	float m_max_value;
	float m_visible_amount;
	float m_value;
	/* the slider's animation: its time and timer function */
	float m_slider_anim_time;
	int32_t m_slider_anim_func;
	float m_jet_tail_min_length;
	/* the range the slider moves over, and the visible amount's, the value's and the slider's
	   offset's shares of it */
	float m_range;
	float m_visible_rate;
	float m_value_rate;
	float m_offset_rate;
};

/* a ScrollBar's resource parameters: its slider's texture ("texture_slider"), scroll_type and
   slider_size */
class ScrollBarParam : public WidgetParam {
public:
	static const char m_param_name[];
	static const TextureRef c_textureList[];
	static const char *ParamName();

	ScrollBarParam();
	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);

	int32_t m_scroll_type;
	math::v4 m_slider_size;
};

/* a ScrollBar's style: the slider's minimum size and animation, and its four planes */
class ScrollBarStyleParam : public WidgetStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	ScrollBarStyleParam();
	virtual const char *GetParamName() const;
	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(Widget *widget);
	virtual int32_t GetEach(Widget *widget);

	float m_min_size;
	float m_anim_time;
	int32_t m_anim_func;
	graph::PlaneObjParam m_obj[ScrollBar::OBJ_SCROLLBAR_END];
};

class ScrollBarTouch : public ScrollBar {
public:
	static const char *TypeName();

	ScrollBarTouch(Widget *parent, CreateParam *param);
};

}
}

#endif
