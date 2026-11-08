/*
 * paf::ui::ItemSpin (DEBUGGER 0x810f3464..0x810f352c, vector<wstring>::operator= and the destructors
 * at 0x812290b4..0x8122927c): a spin widget of strings. No record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <widget/w_item_spin.h>
#include "../paf_types.h"

namespace paf {
namespace ui {

const char ItemSpin::m_widget_type[] = "item_spin";

bool ItemSpin::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return SpinBase::IsInherit(type);
}

char *ItemSpin::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *ItemSpin::TypeName()
{
	return m_widget_type;
}

ItemSpin::ItemSpin(Widget *parent, CreateParam *param) : SpinBase(parent, param)
{
}

int32_t ItemSpin::Create(const vector<wstring> &items, int32_t value, bool loop)
{
	m_items = items;

	int32_t max = 0;

	if (m_items.size() != 0)
		max = m_items.size() - 1;
	return InitSpin(0, max, value, loop);
}

wstring ItemSpin::GetValueString(int32_t value)
{
	return m_items[value];
}

}
}
