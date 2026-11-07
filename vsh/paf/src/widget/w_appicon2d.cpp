/*
 * paf::ui::AppIcon2d (DEBUGGER 0x810d8b10..0x810d8cc4): an application icon whose icon is a
 * bubble object, lit, in place of the base's first draw object.
 */
#include <graphics/drawobj/bubble_obj.h>
#include <graphics/drawobj/plane_obj.h>
#include <widget/w_appicon2d.h>

namespace paf {
namespace ui {

const char AppIcon2d::m_widget_type[] = "app_icon_2d";

bool AppIcon2d::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return AppIconBase::IsInherit(type);
}

char *AppIcon2d::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *AppIcon2d::TypeName()
{
	return m_widget_type;
}

AppIcon2d::AppIcon2d(Widget *parent, CreateParam *param) : AppIconBase(parent, param)
{
	ReserveDrawObj(3);

	graph::DrawStyle style = graph::PlaneObjStyle();

	style.lighting = 1;
	ReplaceDrawObj(DrawObjEntry(new graph::BubbleObj(style), 0), false);
}

AppIcon2d::~AppIcon2d()
{
}

}
}
