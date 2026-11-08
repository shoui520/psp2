/*
 * paf::ui::RadioButton (TOOL 0x8110cb70..0x8110cd0c): a StateButtonBase whose decide checks it,
 * and whose check change goes to the nearest radio box above it. No record or string names
 * the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <widget/factory.h>
#include <widget/w_radio_box.h>
#include <widget/w_radio_button.h>

namespace paf {
namespace ui {

RadioButton::RadioButton(Widget *parent, CreateParam *param) : StateButtonBase(parent, param)
{
}

const char RadioButton::m_widget_type[] = "radio_button";

bool RadioButton::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return StateButtonBase::IsInherit(type);
}

char *RadioButton::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *RadioButton::TypeName()
{
	return m_widget_type;
}

void RadioButton::SetCheck(bool onoff, float time, bool ch_anim)
{
	bool changed = m_is_checked != onoff;

	StateButtonBase::SetCheck(onoff, time, ch_anim);
	if (!changed)
		return;
	for (Widget *w = m_parent; w != NULL; w = w->m_parent) {
		if (w->IsInherit(RadioBox::TypeName())) {
			((RadioBox *)w)->OnValueChanged(this);
			break;
		}
	}
}

/* checked with its sound, the image's whole animation played (a negative time) */
void RadioButton::OnButtonDecide()
{
	SetCheck(true, -0.1f, true);
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewRadioButton(Widget *parent)
{
	return new RadioButton(parent, NULL);
}

static WidgetParam *NewRadioButtonParam()
{
	return new StateButtonBaseParam;
}

static VersatileParam *NewRadioButtonLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewRadioButtonFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewRadioButtonStyleParam()
{
	return new StateButtonBaseStyleParam;
}

static VersatileParam *NewRadioButtonHandlerParam()
{
	return new ButtonBaseHandlerParam;
}

static VersatileParam *NewRadioButtonSoundParam()
{
	return new StateButtonBaseSoundParam;
}

FactoryFuncTable g_factoryRadioButton = {
	RadioButton::TypeName(),
	NewRadioButton,
	NewRadioButtonParam,
	NewRadioButtonLayoutParam,
	NewRadioButtonFocusParam,
	NewRadioButtonStyleParam,
	NewRadioButtonHandlerParam,
	NewRadioButtonSoundParam,
};
