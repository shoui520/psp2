/*
 * paf::ui::Supplier (TOOL 0x8112e22c..0x8112e80c): a plane standing in for its target widget,
 * passing its events on and following its position and size; its type and its factories. No
 * record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <math/ngp/v4.h>
#include <widget/core/event.h>
#include <widget/factory.h>
#include <widget/w_supplier.h>

namespace paf {
namespace ui {

const char Supplier::m_widget_type[] = "supplier";

bool Supplier::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Plane::IsInherit(type);
}

char *Supplier::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *Supplier::TypeName()
{
	return m_widget_type;
}

int32_t Supplier::ScePafWidget_2EBD646E(Widget *target, bool remove_cb)
{
	if (m_target != NULL && m_target != target) {
		if (remove_cb)
			m_target->RemoveDeleteCB(ScePafWidget_C79983DE, this);
		m_target = NULL;
		m_event_enable |= 0x1000;
	}
	if (target == NULL)
		return 0;

	target->AddDeleteCB(ScePafWidget_C79983DE, this);

	uint32_t enable = target->m_event_enable;

	if (!(m_event_enable & 0x400) && (enable & 0x400))
		EnableFocusEvent(true);
	m_target = target;
	m_event_enable |= enable;
	return 0;
}

int32_t Supplier::ScePafWidget_C0A4DDDE(Event *e)
{
	Widget *target = m_target;

	if (target == NULL)
		return 0;

	Event supplied(*e);

	supplied.SetTarget(target);
	supplied.m_type = (supplied.m_type & 0x0fffffff) | Event::DETAIL_SUPPLY;

	int32_t ret = target->HandlerProc(&supplied);

	e->Consume();
	return ret;
}

int32_t Supplier::ScePafWidget_6591E40B(Event *e)
{
	Widget *target = m_target;

	if (target == NULL)
		return 0;

	Event supplied(*e);

	supplied.SetTarget(target);
	supplied.m_type = (supplied.m_type & 0x0fffffff) | Event::DETAIL_SUPPLY;

	int32_t ret = target->EventExecDirect(&supplied);

	e->Consume();
	return ret;
}

void Supplier::ScePafWidget_AAE71C36()
{
	Widget *target = m_target;

	if (target == NULL || m_parent == NULL)
		return;

	if (m_follow_pos) {
		math::v4 pos = target->GetWorldMatrix().w();
		math::v4 parent_pos = m_parent->GetWorldMatrix().w();

		SetPos(pos - parent_pos + m_pos_offset);
	}
	if (m_follow_size)
		SetSize(*m_target->GetSize(0) + m_size_offset);
}

void Supplier::ScePafWidget_C79983DE(Widget *widget, void *data)
{
	((Supplier *)data)->ScePafWidget_2EBD646E(NULL, false);
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewSupplier(Widget *parent)
{
	return new Supplier(parent, NULL);
}

static WidgetParam *NewSupplierParam()
{
	return new PlaneParam;
}

static VersatileParam *NewSupplierLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewSupplierFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewSupplierStyleParam()
{
	return new PlaneStyleParam;
}

static VersatileParam *NewSupplierHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewSupplierSoundParam()
{
	return new WidgetSoundParam;
}

FactoryFuncTable g_factorySupplier = {
	Supplier::TypeName(),
	NewSupplier,
	NewSupplierParam,
	NewSupplierLayoutParam,
	NewSupplierFocusParam,
	NewSupplierStyleParam,
	NewSupplierHandlerParam,
	NewSupplierSoundParam,
};
