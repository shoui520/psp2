/*
 * paf::ui::CheckBox (DEBUGGER 0x810e64b4..0x810e67fc): a StateButtonBase whose decide toggles
 * its check with its sound. Its focus cursor is 46 by 46 where none is sized, and follows the
 * image to the side the image is aligned to. No record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <widget/factory.h>
#include <widget/w_check_box.h>

namespace paf {
namespace ui {

CheckBox::CheckBox(Widget *parent, CreateParam *param) : StateButtonBase(parent, param)
{
	static math::v2 s_focus_size(46.0f, 46.0f);

	if (m_focus_shape != NULL) {
		if (m_focus_shape->m_size == math::v2(0.0f, 0.0f))
			m_focus_shape->m_size = s_focus_size;
	} else {
		FocusShape shape;

		shape.m_size = s_focus_size;
		ScePafWidget_13B246E0(this, &shape);
	}
}

const char CheckBox::m_widget_type[] = "check_box";

bool CheckBox::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return StateButtonBase::IsInherit(type);
}

char *CheckBox::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *CheckBox::TypeName()
{
	return m_widget_type;
}

void CheckBox::UpdateSize()
{
	ButtonBase::UpdateSize();

	FocusShape *shape = m_focus_shape;
	graph::PlaneObj *image = NULL;

	if (GetDrawObj(OBJ_IMAGE) != NULL &&
	    GetDrawObj(OBJ_IMAGE)->IsInherit(graph::PlaneObj::DrawObjType()))
		image = (graph::PlaneObj *)GetDrawObj(OBJ_IMAGE);

	if (shape == NULL || image == NULL)
		return;

	math::v2 size(GetSize(0)->getImpl().getXY());
	float offs_x = 0.0f;
	float offs_y = 0.0f;
	graph::PlaneObj::AlignMode align_x = image->m_align[0];
	graph::PlaneObj::AlignMode align_y = image->m_align[1];

	if (align_x == graph::PlaneObj::ALIGN_MINUS)
		offs_x = (size - shape->m_size).extract_x() * -0.5f;
	else if (align_x == graph::PlaneObj::ALIGN_PLUS)
		offs_x = (size - shape->m_size).extract_x() * 0.5f;
	if (align_y == graph::PlaneObj::ALIGN_MINUS)
		offs_y = (size - shape->m_size).extract_y() * -0.5f;
	else if (align_y == graph::PlaneObj::ALIGN_PLUS)
		offs_y = (size - shape->m_size).extract_y() * 0.5f;
	shape->m_offset = math::v3(offs_x, offs_y, 0.0f);
}

/* toggled with its sound, the image's whole animation played (a negative time) */
void CheckBox::OnButtonDecide()
{
	SetCheck(!m_is_checked, -0.1f, true);
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewCheckBox(Widget *parent)
{
	return new CheckBox(parent, NULL);
}

static WidgetParam *NewCheckBoxParam()
{
	return new StateButtonBaseParam;
}

static VersatileParam *NewCheckBoxLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewCheckBoxFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewCheckBoxStyleParam()
{
	return new StateButtonBaseStyleParam;
}

static VersatileParam *NewCheckBoxHandlerParam()
{
	return new ButtonBaseHandlerParam;
}

static VersatileParam *NewCheckBoxSoundParam()
{
	return new StateButtonBaseSoundParam;
}

FactoryFuncTable g_factoryCheckBox = {
	CheckBox::TypeName(),
	NewCheckBox,
	NewCheckBoxParam,
	NewCheckBoxLayoutParam,
	NewCheckBoxFocusParam,
	NewCheckBoxStyleParam,
	NewCheckBoxHandlerParam,
	NewCheckBoxSoundParam,
};
