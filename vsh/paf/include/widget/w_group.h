/*
 * paf::ui::Group: a widget that only holds others, with Widget's parameters. Names after the
 * exports.
 */
#ifndef PAF_WIDGET_W_GROUP_H
#define PAF_WIDGET_W_GROUP_H

#include <widget/core/widget.h>

namespace paf {
namespace ui {

class Group : public Widget {
public:
	Group(Widget *parent, CreateParam *param) : Widget(parent, param)
	{
	}

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;

	static const char m_widget_type[];
	static const char *TypeName();
};

}
}

#endif
