/*
 * paf::ui::ScrollBar2d: a slider over its base on both axes, as large against the base as the
 * visible amount is against the maximum and placed as the value is; dragged, it sets the value.
 * Names after the exports and the parameters' names; the members' names are ours.
 */
#ifndef PAF_WIDGET_W_SCROLL_BAR2D_H
#define PAF_WIDGET_W_SCROLL_BAR2D_H

#include <stdint.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/vector>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

class ScrollBar2d : public Widget {
public:
	/* its draw objects, after its style's ("base_obj", "slider_obj"); the slider's is the
	   slider widget's */
	enum {
		OBJ_BASE = 0,
		OBJ_SLIDER = 1,
		OBJ_SCROLLBAR2D_END = 2
	};

	/* what it tells its listeners (names ours, after ImageSlideBar's) */
	enum {
		CB_SCROLLBAR2D_CHANGED = 0x10000000,
		CB_SCROLLBAR2D_DRAG_BEGIN = 0x10000001,
		CB_SCROLLBAR2D_DRAG_END = 0x10000002
	};

	static const char m_widget_type[];
	static const char *TypeName();

	ScrollBar2d(Widget *parent, CreateParam *param);
	virtual ~ScrollBar2d();

	virtual int32_t HandlePointEvent(Event *e);
	virtual int32_t HandleTouchEvent(Event *e);
	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual uint32_t CheckState();
	virtual int32_t SetColor_core(const math::v4 &v);
	virtual void UpdateSize();
	virtual graph::DrawObj *GetDrawObj(int32_t obj);
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);
	using Widget::SetTexture;

	/* the slots after Widget's, from 0x170 (index 92) */
	virtual int32_t SetObjColor(const math::v4 &color, int32_t obj);
	virtual int32_t GetObjColor(math::v4 &color, int32_t obj);
	virtual int32_t SetMaxValue(const math::v2 &value);
	virtual int32_t SetVisibleAmount(const math::v2 &value);
	/* the value, the slider moved to it over its animation with anim */
	virtual int32_t SetValue(const math::v2 &value, bool anim);
	virtual int32_t SetSliderAnim(float time, int32_t func);
	/* the widget holding the draw object, and the draw object's ID in it */
	virtual Widget *GetPartsWidget(int32_t obj, int32_t *child_obj);
	virtual void UpdateSlider(bool anim);
	/* a touch on it gives the slider the touch */
	virtual void OnTouch(Event *e);
	virtual void OnDragBeginSlider(Event *e);
	virtual void OnDragMoveSlider(Event *e);
	virtual void OnDragEndSlider(Event *e, bool cancel);
	/* the value where the slider was dragged to */
	virtual void OnDragSlider();

	/* the slider's drag events to the widget's handlers */
	static void OnDragSliderEvent(int32_t type, Handler *self, Event *e, void *data);

	/* the visible amount and the value kept in the maximum, and their shares of it */
	void AdjustParameter(bool anim);

	void GetSliderAnim(float *time, int32_t *func)
	{
		if (time != NULL)
			*time = m_slider_anim_time;
		if (func != NULL)
			*func = m_slider_anim_func;
	}

	/* the bar itself and the slider widget */
	vector<Widget *> m_parts;
	math::v2 m_max_value;
	math::v2 m_visible_amount;
	math::v2 m_value;
	/* the slider's animation: its time and timer function */
	float m_slider_anim_time;
	int32_t m_slider_anim_func;
	/* the visible amount's and the value's shares of the maximum, and the slider's offset */
	math::v2 m_visible_rate;
	math::v2 m_value_rate;
	math::v2 m_offset_rate;
	/* the touch that drags the slider, -1 for none */
	int32_t m_touch_id;
};

/* a ScrollBar2d's resource parameters: its slider's texture ("texture_slider") */
class ScrollBar2dParam : public WidgetParam {
public:
	static const char m_param_name[];
	static const TextureRef c_textureList[];
	static const char *ParamName();

	ScrollBar2dParam();
	virtual const char *GetParamName() const;
};

/* a ScrollBar2d's style: the slider's animation, and its two planes */
class ScrollBar2dStyleParam : public WidgetStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	ScrollBar2dStyleParam();
	virtual const char *GetParamName() const;
	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(Widget *widget);
	virtual int32_t GetEach(Widget *widget);

	float m_anim_time;
	int32_t m_anim_func;
	graph::PlaneObjParam m_base_obj;
	graph::PlaneObjParam m_slider_obj;
};

}
}

#endif
