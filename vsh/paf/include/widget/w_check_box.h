/*
 * paf::ui::CheckBox: a state button that toggles its check as it is decided, its focus cursor
 * 46 by 46 unless one is given, placed by the image's alignment. Names after ScePaf-RE and the
 * exports.
 */
#ifndef PAF_WIDGET_W_CHECK_BOX_H
#define PAF_WIDGET_W_CHECK_BOX_H

#include <widget/w_state_button_base.h>

namespace paf {
namespace ui {

class CheckBox : public StateButtonBase {
public:
	static const char m_widget_type[];
	static const char *TypeName();

	CheckBox(Widget *parent, CreateParam *param);

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	/* ButtonBase's, and the focus cursor moved to the image's side */
	virtual void UpdateSize();
	virtual void OnButtonDecide();
};

}
}

#endif
