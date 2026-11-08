/*
 * paf::ui::RadioButton: a state button its radio box keeps the only one checked; decided, it is
 * checked. Names after ScePaf-RE and the exports.
 */
#ifndef PAF_WIDGET_W_RADIO_BUTTON_H
#define PAF_WIDGET_W_RADIO_BUTTON_H

#include <widget/w_state_button_base.h>

namespace paf {
namespace ui {

class RadioButton : public StateButtonBase {
public:
	static const char m_widget_type[];
	static const char *TypeName();

	RadioButton(Widget *parent, CreateParam *param);

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	/* StateButtonBase's, and the radio box above it told when the check changed */
	virtual void SetCheck(bool onoff, float time = 0.0f, bool ch_anim = false);
	virtual void OnButtonDecide();
};

}
}

#endif
