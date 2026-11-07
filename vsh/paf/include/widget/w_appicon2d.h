/*
 * paf::ui::AppIcon2d: an application icon drawn flat, its icon a bubble object. The class
 * and its functions are Sony's (the export NIDs hash as them).
 */
#ifndef PAF_WIDGET_W_APPICON2D_H
#define PAF_WIDGET_W_APPICON2D_H

#include <widget/w_appicon_base.h>

namespace paf {
namespace ui {

class AppIcon2d : public AppIconBase {
public:
	static const char m_widget_type[];
	static const char *TypeName();

	AppIcon2d(Widget *parent, CreateParam *param);
	virtual ~AppIcon2d();

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
};

}
}

#endif
