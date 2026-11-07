/*
 * paf::ui::SlideBar: a slider over its base, placed as the value is between 0 and the maximum,
 * with a bar filled up to it and three labels: the value at the slider and the minimum and the
 * maximum at the ends. A touch on the bar takes the slider there; dragged, or moved by the left
 * and right keys, it sets the value.
 * Names after the exports and the parameters' names; the members' names are ours.
 */
#ifndef PAF_WIDGET_W_SLIDEBAR_H
#define PAF_WIDGET_W_SLIDEBAR_H

#include <stdint.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v4.h>
#include <std/list>
#include <std/vector>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

class SlideBar : public Widget {
public:
	/* its draw objects, after its style's ("base_obj", "bar_obj", "slider_obj",
	   "slider_glow_obj"); the slider's and its glow are the slider widget's */
	enum {
		OBJ_BASE = 0,
		OBJ_BAR = 1,
		OBJ_SLIDER = 2,
		OBJ_SLIDER_GLOW = 3,
		OBJ_SLIDEBAR_END = 4
	};

	/* what a touch on the bar does: takes the slider to it, or nothing (Sony's type, the names
	   ours) */
	enum TouchMode {
		TOUCH_MODE_BAR = 0,
		TOUCH_MODE_SLIDER = 1
	};

	/* its labels, after its parameters' names ("w_slider_label", "w_min_label",
	   "w_max_label"; Sony's type, the names ours) */
	enum LabelId {
		LABEL_ID_SLIDER = 0,
		LABEL_ID_MIN = 1,
		LABEL_ID_MAX = 2
	};

	/* what a label shows: its number, or what the listeners of CB_SLIDEBAR_LABEL set (Sony's
	   type, the names ours) */
	enum LabelMode {
		LABEL_MODE_VALUE = 0,
		LABEL_MODE_EVENT = 1
	};

	/* where a label is: beside its end, above or below it, or nowhere it is moved to (Sony's
	   type, the names ours) */
	enum LabelPosMode {
		LABEL_POS_MODE_SIDE = 0,
		LABEL_POS_MODE_ABOVE = 1,
		LABEL_POS_MODE_BELOW = 2,
		LABEL_POS_MODE_NONE = 3
	};

	/* what it tells its listeners (names ours); with DRAG_BEGIN and DRAG_END, m_data[0] is 1
	   when a key moved it and m_data[1] whether the drag was cancelled; with CB_SLIDEBAR_LABEL,
	   m_data[0] is the label */
	enum {
		CB_SLIDEBAR_CHANGED = 0,
		CB_SLIDEBAR_DRAG_BEGIN = 1,
		CB_SLIDEBAR_DRAG_END = 2,
		CB_SLIDEBAR_LABEL = 0x20001000
	};

	/* its sounds, after its sound parameter's names */
	enum {
		SOUND_DRAG_BEGIN = 0,
		SOUND_DRAG_END = 1,
		SOUND_SLIDEBAR_END = 2
	};

	static const char m_widget_type[];
	static const char *TypeName();

	SlideBar(Widget *parent, CreateParam *param);
	virtual ~SlideBar();

	virtual int32_t HandleKeycodeEvent(Event *e);
	virtual int32_t HandlePointEvent(Event *e);
	virtual int32_t HandleTouchEvent(Event *e);
	/* the slider takes the focus given to the bar; a key drag ends when the bar loses it */
	virtual int32_t HandleFocusEvent(Event *e);
	virtual void EnableFocusEvent(bool on_off);
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
	virtual WidgetSoundParam::Sound *GetSound(int32_t id);
	virtual void AddSound(uint32_t index, WidgetSoundParam::Sound &sound);

	/* the slots after Widget's, from 0x170 (index 92) */
	virtual int32_t SetObjColor(const math::v4 &color, int32_t obj);
	virtual int32_t GetObjColor(math::v4 &color, int32_t obj);
	/* the size of the draw object's widget */
	virtual int32_t SetObjSize(const math::v4 &size, int32_t obj);
	virtual int32_t GetObjSize(math::v4 &size, int32_t obj);
	virtual int32_t SetTouchMode(TouchMode mode);
	virtual int32_t SetMaxValue(int32_t value);
	/* the value, the slider moved to it over its animation with anim */
	virtual int32_t SetValue(int32_t value, bool anim);
	virtual int32_t SetSliderAnim(float time, int32_t func);
	/* the widget shown as the label; the slider's label goes on the slider */
	virtual int32_t AttachLabelWidget(Widget *label, LabelId id);
	virtual int32_t DetachLabelWidget(LabelId id);
	virtual Widget *GetLabelWidget(LabelId id);
	virtual int32_t SetLabelMode(LabelMode mode, LabelId id);
	virtual int32_t SetLabelPosMode(LabelPosMode mode, LabelId id);
	/* how far the label is from its end */
	virtual int32_t SetLabelMargin(float margin, LabelId id);
	virtual void CreateSliderGlow();
	/* the widget holding the draw object, and the draw object's ID in it */
	virtual Widget *GetPartsWidget(int32_t obj, int32_t *child_obj);
	virtual void ShowGlow(int32_t obj, bool show);
	/* the value kept between 0 and the maximum, and its share of the maximum */
	virtual void AdjustParameter();
	virtual void UpdateSlider(bool anim);
	/* the label's text: the number it shows, or what the listeners of CB_SLIDEBAR_LABEL set */
	virtual void UpdateLabel(LabelId id);
	/* the label placed at its end by its position mode */
	virtual void UpdateLabelPos(LabelId id);
	/* a touch on the bar given to the slider */
	virtual void OnTouch(Event *e);
	virtual void OnDragBeginSlider(Event *e);
	virtual void OnDragMoveSlider(Event *e);
	virtual void OnDragEndSlider(Event *e, bool cancel);
	/* the value where the slider was dragged to, the bar filled up to it */
	virtual void OnDragSlider();

	/* the slider's drag events to the widget's handlers */
	static void OnDragSliderEvent(int32_t type, Handler *self, Event *e, void *data);

	/* the bar made, filled up to the slider, where the bar has none */
	void CreateBar();
	int32_t GetLabelMode(LabelMode &mode, LabelId id);
	int32_t GetLabelPosMode(LabelPosMode &mode, LabelId id);
	int32_t GetLabelMargin(float &margin, LabelId id);
	/* the bar in its color again, taking touches and keys */
	void Enable();
	/* the bar grayed out (m_disable_color, else a light gray), not taking touches or keys */
	void Disable();

	void GetSliderAnim(float *time, int32_t *func)
	{
		if (time != NULL)
			*time = m_slider_anim_time;
		if (func != NULL)
			*func = m_slider_anim_func;
	}

	/* the slider widget, then the labels by their LabelId */
	vector<Widget *> m_parts;
	TouchMode m_touch_mode;
	int32_t m_max_value;
	int32_t m_value;
	/* the slider's animation: its time and timer function */
	float m_slider_anim_time;
	int32_t m_slider_anim_func;
	LabelMode m_label_mode[3];
	LabelPosMode m_label_pos_mode[3];
	float m_label_margin[3];
	/* the value's share of the maximum */
	float m_value_rate;
	/* the touch that drags the slider, and the key that moves it: -1 for none */
	int32_t m_touch_id;
	int32_t m_key;
	/* how far a key press moves the value; 0 is 1 */
	int32_t m_key_step;
	bool m_enabled : 1;
	/* its color while enabled, kept by Disable; the color it is grayed out with */
	math::v4 *m_enable_color;
	math::v4 *m_disable_color;
	/* the sounds added to it, by index */
	list<WidgetSoundParam::Sound> m_sound_list;
};

/* a SlideBar's resource parameters: its textures, touch_mode, slider_size, and each label's
   widget, mode, position mode and margin */
class SlideBarParam : public WidgetParam {
public:
	static const char m_param_name[];
	static const TextureRef c_textureList[];
	static const char *ParamName();

	SlideBarParam();
	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);

	int32_t m_touch_mode;
	math::v4 m_slider_size;
	int32_t m_label_mode[3];
	int32_t m_label_pos_mode[3];
	float m_label_margin[3];
	/* the labels' widgets, by ID */
	IDParam m_label[3];
};

/* a SlideBar's style: the slider's animation, and its four planes */
class SlideBarStyleParam : public WidgetStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	SlideBarStyleParam();
	virtual const char *GetParamName() const;
	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(Widget *widget);
	virtual int32_t GetEach(Widget *widget);

	float m_anim_time;
	int32_t m_anim_func;
	graph::PlaneObjParam m_obj[SlideBar::OBJ_SLIDEBAR_END];
};

/* a SlideBar's two sounds */
class SlideBarSoundParam : public WidgetSoundParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	SlideBarSoundParam();
	virtual const char *GetParamName() const;
};

}
}

#endif
