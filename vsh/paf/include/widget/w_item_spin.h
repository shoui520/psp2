/*
 * paf::ui::ItemSpin: a spin widget of strings, its value the index of one. The class, TypeName and
 * Create are Sony's (export NIDs hash as them); m_items is ours.
 */
#ifndef PAF_WIDGET_W_ITEM_SPIN_H
#define PAF_WIDGET_W_ITEM_SPIN_H

#include <stdint.h>
#include <std/string>
#include <std/vector>
#include <widget/w_spin_base.h>

namespace paf {
namespace ui {

class ItemSpin : public SpinBase {
public:
	static const char m_widget_type[];
	static const char *TypeName();

	ItemSpin(Widget *parent, CreateParam *param);

	virtual ~ItemSpin()
	{
	}

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	/* the item at the value */
	virtual wstring GetValueString(int32_t value);

	/* its items, the value and loop it starts with */
	int32_t Create(const vector<wstring> &items, int32_t value, bool loop);

	vector<wstring> m_items;
};

}
}

#endif
