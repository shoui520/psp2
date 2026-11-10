/*
 * paf::ui::Supplier: a plane that stands in for another widget, its target, passing the
 * events it gets on to it and following its position and size. The class, TypeName and the
 * export NIDs are Sony's; the members' names are ours.
 */
#ifndef PAF_WIDGET_W_SUPPLIER_H
#define PAF_WIDGET_W_SUPPLIER_H

#include <stdint.h>
#include <math/ngp/v4.h>
#include <widget/core/event.h>
#include <widget/w_plane.h>

namespace paf {
namespace ui {

class Supplier : public Plane {
public:
	static const char m_widget_type[];
	static const char *TypeName();

	Supplier(Widget *parent, CreateParam *param) : Plane(parent, param)
	{
		m_target = NULL;
		m_follow_size = false;
		m_follow_pos = false;
		m_pos_offset = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
		m_size_offset = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
		SetMetaAlpha(0.3f);
		if (m_plane != NULL)
			m_plane->SetVisible(false);
	}

	/* the target let go, its delete callback dropped */
	virtual ~Supplier()
	{
		ScePafWidget_2EBD646E(NULL, true);
	}

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;

	/* the target set: the old one let go (its delete callback dropped where remove_cb),
	   the new one's event types taken over (name ours) */
	int32_t ScePafWidget_2EBD646E(Widget *target, bool remove_cb);
	/* the event passed on to the target's HandlerProc as supplied, and consumed here (export
	   C0A4DDDE, Sony's vtable slot of HandlerProc) */
	virtual int32_t HandlerProc(Event *e);
	/* the same, through the target's EventExecDirect (export 6591E40B, the slot of
	   EventExecDirect) */
	virtual int32_t EventExecDirect(Event *e);
	/* its position and size made the target's, with its offsets */
	virtual void UpdateAdjustSize();
	/* the target's delete callback: the target let go */
	static void ScePafWidget_C79983DE(Widget *widget, void *data);

	bool m_follow_size : 1;
	bool m_follow_pos : 1;
	/* the widget it supplies (name ours) */
	Widget *m_target;
	/* added to the target's position and size where it follows them (names ours) */
	math::v4 m_pos_offset;
	math::v4 m_size_offset;
};

}
}

#endif
