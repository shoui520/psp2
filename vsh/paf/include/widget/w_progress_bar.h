/*
 * paf::ui::ProgressBar: a bar filled from its minimum to its maximum value, one or two bars
 * deep (the fore bar over the back one), with a label of its rate. Names after the exports
 * (SetObjColor, GetObjColor, SetBarMode, SetMaxValue, SetMaxValueAsync, SetMinValue,
 * SetMinValueAsync, SetValue, SetValueAsync, GetValue, SetBarAnim, AttachLabelWidget,
 * DetachLabelWidget, GetLabelWidget, SetLabelMode, SetLabelPosMode, SetLabelMargin,
 * GetPartsWidget, Sync, CheckDirtyFlag, UpdateBaseShadow, UpdateBar, UpdateLabel,
 * UpdateLabelPos, SetGlowVisible, SetSignalAnim, IsSignalAnim, SetSignalAnimTime,
 * SetShadowVisible, SetShadowParam), the parameters' names ("bar_mode", "w_label", "anim_time",
 * "shadow_offset", ...) and ScePaf-RE (the enums, m_max, m_min, m_value, m_anim_time,
 * m_anim_func, m_dirty, the getters and the integer overloads the vtable holds beside them,
 * inlined in their callers); the other members' names are ours. With its resource and style
 * parameters.
 */
#ifndef PAF_WIDGET_W_PROGRESS_BAR_H
#define PAF_WIDGET_W_PROGRESS_BAR_H

#include <stdint.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/vector>
#include <thread/thread.h>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

class ProgressBar : public Widget {
public:
	/* its draw objects, after its style's ("base_obj", "bar_fore_obj", "bar_back_obj",
	   "signal_obj", "glow_obj", "base_shadow_obj", "bar_shadow_obj") */
	enum {
		OBJ_BASE = 0,
		OBJ_BAR_FORE = 1,
		OBJ_BAR_BACK = 2,
		OBJ_SIGNAL = 3,
		OBJ_GLOW = 4,
		OBJ_BASE_SHADOW = 5,
		OBJ_BAR_SHADOW = 6,
		OBJ_PROGRESSBAR_END = 7
	};

	/* what it tells its listeners: with LABEL_MODE_MANUAL, its label is to be set (name
	   ours) */
	enum {
		CB_PROGRESSBAR_LABEL = 0x20001000
	};

	/* the bits of m_dirty: what CheckDirtyFlag updates */
	enum {
		DIRTY_FLAG_BAR_FORE = 0,
		DIRTY_FLAG_BAR_BACK = 1,
		DIRTY_FLAG_LABEL = 2,
		DIRTY_FLAG_LABEL_POS = 3,
		DIRTY_FLAG_PROGRESSBAR_END = 4
	};

	/* its animations (names ours) */
	enum {
		ANIM_SIGNAL = 0x1000F,
		ANIM_GLOW = 0x10010
	};

	/* the shadows: the base's moved by the offset, and both grown by the margin
	   ("shadow_offset", "shadow_margin") */
	class ShadowParam {
	public:
		math::v2 m_offset;
		math::v2 m_margin;
	};

	enum BarMode {
		BAR_MODE_SINGLE = 0,
		BAR_MODE_DOUBLE = 1
	};

	enum BarId {
		BAR_FORE = 0,
		BAR_BACK = 1
	};

	enum LabelMode {
		LABEL_MODE_PERCENTAGE = 0,
		LABEL_MODE_VALUE = 1,
		LABEL_MODE_MANUAL = 2,
		/* the value plus one over the maximum, shown only for a maximum above one (name
		   ours) */
		LABEL_MODE_PAGE = 3
	};

	enum LabelPosMode {
		LABEL_POS_MODE_CENTER = 0,
		LABEL_POS_MODE_LEFT = 1,
		LABEL_POS_MODE_RIGHT = 2,
		LABEL_POS_MODE_UP = 3,
		LABEL_POS_MODE_DOWN = 4,
		LABEL_POS_MODE_LEFT_UP = 5,
		LABEL_POS_MODE_LEFT_DOWN = 6,
		LABEL_POS_MODE_RIGHT_UP = 7,
		LABEL_POS_MODE_RIGHT_DOWN = 8,
		LABEL_POS_MODE_MANUAL = 9
	};

	/* where each LabelPosMode puts the label: its offset in halves of the bar's size, in
	   margins, and its alignment */
	static const struct LabelPos {
		math::v2 m_pos;
		math::v2 m_margin;
		int32_t m_align_x;
		int32_t m_align_y;
	} c_labelPosTable[];

	/* the lock of what the Async setters leave for Sync (name ours) */
	static thread::Mutex s_async_lock;

	static const char m_widget_type[];
	static const char *TypeName();

	ProgressBar(Widget *parent, CreateParam *param);
	virtual ~ProgressBar();

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual uint32_t CheckState();
	virtual int32_t SetColor_core(const math::v4 &v);
	virtual void UpdateSize();
	virtual void Update();
	virtual common::SharedPtr<graph::DrawObj> SwapDrawObj(common::SharedPtr<graph::DrawObj> obj,
							      int32_t id);
	virtual graph::DrawObj *GetDrawObj(int32_t obj);
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);
	using Widget::SetTexture;

	/* the slots after Widget's, from 0x170 (index 92) */
	virtual int32_t SetObjColor(const math::v4 &color, int32_t obj);
	virtual int32_t GetObjColor(math::v4 &color, int32_t obj);
	virtual int32_t SetBarMode(BarMode mode);
	virtual BarMode GetBarMode()
	{
		return m_bar_mode;
	}
	virtual int32_t SetMaxValue(float value);
	virtual int32_t SetMaxValue(int32_t value)
	{
		return SetMaxValue((float)value);
	}
	virtual int32_t SetMaxValueAsync(float value);
	virtual int32_t SetMaxValueAsync(int32_t value)
	{
		return SetMaxValueAsync((float)value);
	}
	virtual float GetMaxValue()
	{
		return m_max;
	}
	virtual int32_t SetMinValue(float value);
	virtual int32_t SetMinValue(int32_t value)
	{
		return SetMinValue((float)value);
	}
	virtual int32_t SetMinValueAsync(float value);
	virtual int32_t SetMinValueAsync(int32_t value)
	{
		return SetMinValueAsync((float)value);
	}
	virtual float GetMinValue()
	{
		return m_min;
	}
	/* the bar's value, animated with anim */
	virtual int32_t SetValue(float value, bool anim = false, BarId id = BAR_FORE);
	virtual int32_t SetValue(int32_t value, bool anim = false, BarId id = BAR_FORE)
	{
		return SetValue((float)value, anim, id);
	}
	virtual int32_t SetValueAsync(float value, bool anim = false, BarId id = BAR_FORE);
	/* 0x1B4 (index 109) */
	virtual int32_t SetValueAsync(int32_t value, bool anim = false, BarId id = BAR_FORE)
	{
		return SetValueAsync((float)value, anim, id);
	}
	virtual float GetValue(BarId id = BAR_FORE);
	virtual int32_t SetBarAnim(float time, int32_t func);
	virtual int32_t AttachLabelWidget(Widget *widget);
	virtual int32_t DetachLabelWidget();
	virtual Widget *GetLabelWidget();
	virtual int32_t SetLabelMode(LabelMode mode);
	virtual LabelMode GetLabelMode()
	{
		return m_label_mode;
	}
	virtual int32_t SetLabelPosMode(LabelPosMode mode);
	virtual LabelPosMode GetLabelPosMode()
	{
		return m_label_pos_mode;
	}
	virtual int32_t SetLabelMargin(float margin);
	virtual float GetLabelMargin()
	{
		return m_label_margin;
	}
	virtual Widget *GetPartsWidget(int32_t obj, int32_t *child_obj);
	/* the value clamped and stored, the bar updated unless sync_later (export 46F99A4E,
	   name ours) */
	virtual int32_t SetValue_core(float value, bool sync_later, BarId id);
	virtual void Sync();
	virtual void CheckDirtyFlag();
	virtual void UpdateBaseShadow();
	virtual void UpdateBar(BarId id = BAR_FORE, bool anim = false);
	virtual void UpdateLabel();
	virtual void UpdateLabelPos();

	/* the back bar made where it has none, shown in BAR_MODE_DOUBLE (no name found) */
	void ScePafWidget_536C8866();
	/* the glow made where it has none (no name found) */
	void ScePafWidget_16ACDAD6();
	void SetGlowVisible(bool visible);
	void SetSignalAnim(bool on_off);
	bool IsSignalAnim();
	void SetSignalAnimTime(float time);
	/* the signal made where it has none, over the fore bar (no name found) */
	void ScePafWidget_7BBE5D41();
	/* the base's shadow and the bar's made where it has none, under the base and over its
	   shadow (no names found) */
	void ScePafWidget_1E040ECD();
	void ScePafWidget_1FD5D71C();
	void SetShadowVisible(bool visible);
	void SetShadowParam(const ShadowParam &param);

	void GetBarAnim(float *time, int32_t *func)
	{
		if (time != NULL)
			*time = m_anim_time;
		if (func != NULL)
			*func = m_anim_func;
	}

	/* the label widget */
	vector<Widget *> m_parts;
	BarMode m_bar_mode;
	LabelMode m_label_mode;
	LabelPosMode m_label_pos_mode;
	float m_label_margin;
	float m_max;
	float m_min;
	/* each bar's value */
	float m_value[2];
	float m_anim_time;
	int32_t m_anim_func;
	float m_signal_anim_time;
	ShadowParam m_shadow_param;
	/* the minimum's share of the maximum, and each bar's value's */
	float m_min_rate;
	float m_rate[2];
	uint32_t m_dirty;
	/* what the Async setters leave for Sync, under its lock: -1 for nothing */
	float m_async_max;
	float m_async_min;
	float m_async_value[2];
	bool m_async_anim[2];
	bool m_glow_visible;
	bool m_signal_anim;
	bool m_shadow_visible;
};

/* a ProgressBar's resource parameters: its textures, bar_mode, w_label, label_mode,
   label_pos_mode, label_margin and signal_anim */
class ProgressBarParam : public WidgetParam {
public:
	static const char m_param_name[];
	static const TextureRef c_textureList[];
	static const char *ParamName();

	ProgressBarParam();
	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);

	int32_t m_bar_mode;
	int32_t m_label_mode;
	int32_t m_label_pos_mode;
	float m_label_margin;
	bool m_signal_anim;
	IDParam m_label;
};

/* a ProgressBar's style: the bar's animation, the signal's, the shadows' offset and margin, and
   its seven planes */
class ProgressBarStyleParam : public WidgetStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	ProgressBarStyleParam();
	virtual const char *GetParamName() const;
	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(Widget *widget);
	virtual int32_t GetEach(Widget *widget);

	float m_anim_time;
	int32_t m_anim_func;
	float m_signal_anim_time;
	ProgressBar::ShadowParam m_shadow_param;
	graph::PlaneObjParam m_obj[ProgressBar::OBJ_PROGRESSBAR_END];
};

}
}

#endif
