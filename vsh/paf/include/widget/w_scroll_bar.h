/*
 * paf::ui::ScrollBar: a slider over its base, as long against the base as the visible amount is
 * against the maximum, and placed as the value is; with a jet tail stretched behind the slider
 * and a glow over it; and ScrollBarTouch, the one moved by touch.
 * Names after the exports and the parameters' names; the members' names are ours.
 */
#ifndef PAF_WIDGET_W_SCROLL_BAR_H
#define PAF_WIDGET_W_SCROLL_BAR_H

#include <stdint.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v4.h>
#include <std/list>
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

/* a ScrollBar moved by touch: the slider dragged, and a touch on the bar beside it scrolling it a
   page toward the touch, under a glow on the base */
class ScrollBarTouch : public ScrollBar {
public:
	/* its draw objects after ScrollBar's: the glow on the base, at the touch ("base_glow_obj") */
	enum {
		OBJ_BASE_GLOW = 4,
		OBJ_SCROLLBAR_TOUCH_END = 5
	};

	/* what a touch on the bar beside the slider does: nothing, moves the value a page, or sends
	   CB_SCROLLBAR_TOUCH_PAGE_SCROLL (Sony's type, the names ours) */
	enum PageScrollMode {
		PAGE_SCROLL_MODE_NONE = 0,
		PAGE_SCROLL_MODE_VALUE = 1,
		PAGE_SCROLL_MODE_EVENT = 2
	};

	/* what it tells its listeners (names ours, after ImageSlideBar's); with
	   CB_SCROLLBAR_TOUCH_PAGE_SCROLL, m_data[0] is 1 toward the end and -1 toward the start */
	enum {
		CB_SCROLLBAR_TOUCH_CHANGED = 0x10000000,
		CB_SCROLLBAR_TOUCH_DRAG_BEGIN = 0x10000001,
		CB_SCROLLBAR_TOUCH_DRAG_END = 0x10000002,
		CB_SCROLLBAR_TOUCH_PAGE_SCROLL = 0x20001000
	};

	/* its sounds, after its sound parameter's names */
	enum {
		SOUND_DRAG_BEGIN = 0,
		SOUND_DRAG_END = 1,
		SOUND_PAGE_SCROLL = 2,
		SOUND_PAGE_SCROLL_REPEAT = 3,
		SOUND_SCROLLBAR_TOUCH_END = 4
	};

	static const char m_widget_type[];
	static const char *TypeName();

	ScrollBarTouch(Widget *parent, CreateParam *param);
	virtual ~ScrollBarTouch();

	virtual int32_t HandlePointEvent(Event *e);
	virtual int32_t HandleTouchEvent(Event *e);
	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual WidgetSoundParam::Sound *GetSound(int32_t id);
	virtual void AddSound(uint32_t index, WidgetSoundParam::Sound &sound);
	virtual void CreateSliderGlow();
	virtual Widget *GetPartsWidget(int32_t obj, int32_t *child_obj);
	virtual void UpdateSlider(bool anim);

	/* the slots after ScrollBar's, from 0x1c4 (index 113) */
	virtual int32_t SetPageScrollMode(PageScrollMode mode);
	virtual void CreateBaseGlow();
	virtual void SetBaseGlowSize(const math::v3 &size);
	virtual math::v3 GetBaseGlowSize();
	/* a touch on the bar beside the slider follows it, scrolling a page now and then */
	virtual void OnTouch(Event *e);
	virtual void OnDragBeginSlider(Event *e);
	virtual void OnDragMoveSlider(Event *e);
	virtual void OnDragEndSlider(Event *e, bool cancel);
	/* the value where the slider was dragged to */
	virtual void OnDragSlider();

	/* the slider's drag events to the widget's handlers */
	static void OnDragSliderEvent(int32_t type, Handler *self, Event *e, void *data);

	/* the value moved a page toward the touch it follows, the base glow shown where the touch
	   is beside the slider */
	void PageScroll();

	PageScrollMode m_page_scroll_mode;
	/* the touch on the bar it follows, and the one dragging the slider: -1 for none */
	int32_t m_page_touch_id;
	int32_t m_drag_touch_id;
	/* the milliseconds the touch on the bar has been held since it last scrolled */
	float m_page_scroll_time;
	/* the sounds added to it, by index */
	list<WidgetSoundParam::Sound> m_sound_list;
};

/* a ScrollBarTouch's resource parameters: ScrollBar's and page_scroll */
class ScrollBarTouchParam : public ScrollBarParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	ScrollBarTouchParam();
	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);

	int32_t m_page_scroll;
};

/* a ScrollBarTouch's style: ScrollBar's, and the base glow's plane */
class ScrollBarTouchStyleParam : public ScrollBarStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	ScrollBarTouchStyleParam();
	virtual const char *GetParamName() const;
	virtual int32_t SetEach(Widget *widget);

	graph::PlaneObjParam m_base_glow_obj;
};

/* a ScrollBarTouch's four sounds */
class ScrollBarTouchSoundParam : public WidgetSoundParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	ScrollBarTouchSoundParam();
	virtual const char *GetParamName() const;
};

}
}

#endif
