/*
 * paf::ui::RadioBox: the box around a group of radio buttons. It keeps the one checked, the
 * others unchecked, and moves its slider widget over the checked button; the arrow keys move
 * the focus among its radio buttons. The classes, the exported functions and the strings of the
 * resource parameters are Sony's (export NIDs, ScePaf-RE's header after them, the parameter
 * names); the other functions' and the members' names are ours.
 */
#ifndef PAF_WIDGET_W_RADIO_BOX_H
#define PAF_WIDGET_W_RADIO_BOX_H

#include <stdint.h>
#include <math/ngp/v2.h>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

class RadioButton;

class RadioBox : public Widget {
public:
	static const char m_widget_type[];
	static const char *TypeName();

	RadioBox(Widget *parent, CreateParam *param);

	virtual int32_t HandleKeycodeEvent(Event *e);
	virtual int32_t HandleFocusEvent(Event *e);
	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void DestroyWidget();
	virtual void Update();

	/* left and right too, where the base takes none */
	virtual int32_t GetFocusDirectionMask()
	{
		int32_t mask = Widget::GetFocusDirectionMask();

		return mask != 0 ? mask : 6;
	}

	/* the widget moved over the checked button, hidden until one is checked */
	virtual void SetSlider(Widget *widget);

	/* the focus comes back to the radio button it last left */
	virtual void SetKeepFocus(bool keep)
	{
		m_keep_focus = keep;
	}

	/* the button checked or unchecked: the others unchecked, the slider moved over it */
	virtual void OnValueChanged(RadioButton *radio_button);
	/* the checked button destroyed: the slider hidden */
	virtual void OnDestroyChecked(Widget *widget);
	/* the focus events 0x401 to 0x404 (HandleFocusEvent), as ScrollViewBase's */
	virtual bool OnFocusIn(Event *e);
	virtual bool OnFocusEnter(Event *e);
	virtual bool OnFocusLeave(Event *e);
	virtual bool OnFocusOut(Event *e);
	/* an arrow key pressed: the focus moved to the radio button that way */
	virtual bool OnDirKey(Event *e);

	/* the event sent to its listeners with the four values */
	void SendEvent(int32_t type, int32_t v0, int32_t v1, int32_t v2, int32_t v3);
	/* the widget the focus goes to the direction, from from or the focus */
	Widget *SearchFocus(const math::v2 &dir, Widget *from);

	static void CBDestroyChecked(Widget *widget, void *data);

	Widget *m_slider;
	RadioButton *m_checked;
	/* the radio button the focus last left, or the one checked */
	Widget *m_last_focus;
	bool m_keep_focus;
};

class RadioBoxParam : public WidgetParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	RadioBoxParam();

	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);

	/* the slider ("w_slider") */
	IDParam m_slider;
};

}
}

#endif
