/*
 * paf::ui::Group (DEBUGGER 0x810f013c..0x810f0338): its type, and its factories, which make
 * Widget's parameters. No record or string names the file.
 */
#include <stddef.h>
#include <widget/factory.h>
#include <widget/w_group.h>

namespace paf {
namespace ui {

const char Group::m_widget_type[] = "group";

bool Group::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *Group::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *Group::TypeName()
{
	return m_widget_type;
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewGroup(Widget *parent)
{
	return new Group(parent, NULL);
}

static WidgetParam *NewGroupParam()
{
	return new WidgetParam;
}

static VersatileParam *NewGroupLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewGroupFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewGroupStyleParam()
{
	return new WidgetStyleParam;
}

static VersatileParam *NewGroupHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewGroupSoundParam()
{
	return new WidgetSoundParam;
}

FactoryFuncTable g_factoryGroup = {
	Group::TypeName(),
	NewGroup,
	NewGroupParam,
	NewGroupLayoutParam,
	NewGroupFocusParam,
	NewGroupStyleParam,
	NewGroupHandlerParam,
	NewGroupSoundParam,
};
