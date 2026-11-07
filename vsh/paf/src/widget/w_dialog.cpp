/*
 * paf::ui::Dialog (DEBUGGER 0x810e92a8..0x810e9668): a box that consumes the point and touch
 * events it is sent, with its parts by index. No record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <versatileparam/idparam.h>
#include <widget/core/event.h>
#include <widget/factory.h>
#include <widget/w_dialog.h>

namespace paf {
namespace ui {

/* what GetPartsWidgetId gives for a part it does not have (name ours) */
static IDParam s_null_id("");

Dialog::Dialog(Widget *parent) : Box(parent, NULL)
{
	m_hit_detect_mode = 0;
	m_hit_detect_b_mode = 0;
}

const char Dialog::m_widget_type[] = "dialog";

bool Dialog::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Box::IsInherit(type);
}

char *Dialog::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *Dialog::TypeName()
{
	return m_widget_type;
}

Dialog::~Dialog()
{
}

int32_t Dialog::HandlePointEvent(Event *e)
{
	e->Consume();
	return 0;
}

int32_t Dialog::HandleTouchEvent(Event *e)
{
	e->Consume();
	return 0;
}

void Dialog::SetPartsList(const PartsList *list)
{
	for (; list->index != 0; list++)
		m_parts[list->index] = list->id;
}

const IDParam &Dialog::GetPartsWidgetId(int32_t index)
{
	map<int32_t, IDParam>::iterator it = m_parts.find(index);

	if (it == m_parts.end())
		return s_null_id;
	return it->second;
}

Widget *Dialog::GetPartsWidget(int32_t index)
{
	IDParam id = GetPartsWidgetId(index);
	Widget *widget = NULL;

	if (id.m_idhash != s_null_id.m_idhash)
		widget = FindChild(id, 0);
	return widget;
}

int32_t Dialog::SetPartsString(int32_t index, const wstring &str)
{
	Widget *widget = GetPartsWidget(index);

	if (widget == NULL)
		return -1;
	return widget->SetString(str);
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewDialog(Widget *parent)
{
	return new Dialog(parent);
}

static WidgetParam *NewDialogParam()
{
	return new BoxParam;
}

static VersatileParam *NewDialogLayoutParam()
{
	return new BoxLayoutParam;
}

static VersatileParam *NewDialogFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewDialogStyleParam()
{
	return new PlaneStyleParam;
}

static VersatileParam *NewDialogHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewDialogSoundParam()
{
	return new WidgetSoundParam;
}

FactoryFuncTable g_factoryDialog = {
	Dialog::TypeName(),
	NewDialog,
	NewDialogParam,
	NewDialogLayoutParam,
	NewDialogFocusParam,
	NewDialogStyleParam,
	NewDialogHandlerParam,
	NewDialogSoundParam,
};
