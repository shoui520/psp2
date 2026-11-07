/*
 * paf::ui::ProgressBar: a bar filled from its minimum to its maximum value, one or two bars
 * deep (the fore bar over the back one), with a label of its rate. Names after the exports
 * (SetObjColor, GetObjColor, SetBarMode, SetMaxValue, SetMaxValueAsync, SetMinValue,
 * SetMinValueAsync, SetValue, SetValueAsync, GetValue, SetBarAnim, AttachLabelWidget,
 * DetachLabelWidget, GetLabelWidget, SetLabelMode, SetLabelPosMode, SetLabelMargin,
 * GetPartsWidget, Sync, CheckDirtyFlag, UpdateBaseShadow, UpdateBar, UpdateLabel,
 * UpdateLabelPos) and ScePaf-RE (the enums, the getters and the integer overloads the
 * vtable holds beside them, inlined in their callers). The virtuals only: the class is not
 * written in C yet, and its members are laid out with it.
 */
#ifndef PAF_WIDGET_W_PROGRESS_BAR_H
#define PAF_WIDGET_W_PROGRESS_BAR_H

#include <stdint.h>
#include <math/ngp/v4.h>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

class ProgressBar : public Widget {
public:
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
		LABEL_MODE_MANUAL = 2
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

	static const char m_widget_type[];
	static const char *TypeName();

	ProgressBar(Widget *parent, CreateParam *param);
	virtual ~ProgressBar();

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;

	/* the slots after Widget's, from 0x170 (index 92) */
	virtual int32_t SetObjColor(const math::v4 &color, int32_t obj);
	virtual int32_t GetObjColor(math::v4 &color, int32_t obj);
	virtual int32_t SetBarMode(BarMode mode);
	virtual BarMode GetBarMode();
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
	virtual float GetMaxValue();
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
	virtual float GetMinValue();
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
	virtual LabelMode GetLabelMode();
	virtual int32_t SetLabelPosMode(LabelPosMode mode);
	virtual LabelPosMode GetLabelPosMode();
	virtual int32_t SetLabelMargin(float margin);
	virtual float GetLabelMargin();
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
};

}
}

#endif
