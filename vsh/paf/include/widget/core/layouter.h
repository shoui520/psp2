/*
 * paf::ui::Layouter: what lays a widget out each update, by calling the
 * layout handler its resource names.
 *
 * Names after ScePaf-RE (restored from Sony's DWARF); the class and its
 * exports (OnUpdate_core, s_enter_layout, the vtable) are Sony's.
 */
#ifndef PAF_WIDGET_CORE_LAYOUTER_H
#define PAF_WIDGET_CORE_LAYOUTER_H

#include <stddef.h>
#include <stdint.h>

namespace paf {
namespace ui {

class Widget;
class Layouter;

/* a layout handler */
typedef int32_t (*LayoutCB)(Layouter *layouter, Widget *widget, void *data1, void *data2,
			    void *data3);

class Layouter {
public:
	Layouter(LayoutCB cb_layout, void *data)
		: m_widget(NULL), m_cb_layout(cb_layout), m_dirty(1), m_first(1),
		  m_reso_dirty_begin(false), m_reso_dirty_end(false)
	{
	}

	virtual ~Layouter()
	{
	}

	virtual int32_t OnUpdate_core();

	void BindWidget(Widget *w)
	{
		m_widget = w;
	}

	bool IsDirty(uint32_t val)
	{
		return (m_dirty & val) != 0;
	}

	void SetDirty(uint32_t val)
	{
		m_dirty |= val;
	}

	static bool s_enter_layout;

protected:
	Widget *m_widget;
	LayoutCB m_cb_layout;
	uint32_t m_dirty : 8;
	uint32_t m_first : 8;
	bool m_reso_dirty_begin : 1;
	bool m_reso_dirty_end : 1;
};

}
}

#endif /* PAF_WIDGET_CORE_LAYOUTER_H */
