/*
 * paf::ui::Plane: a widget drawn as one plane object (draw object 0). The class and its
 * constructor are Sony's (export 1A678CB4); the member's name is ours. Only what consumers use
 * is declared: its overrides come with its C.
 */
#ifndef PAF_WIDGET_W_PLANE_H
#define PAF_WIDGET_W_PLANE_H

#include <graphics/drawobj/plane_obj.h>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

class Plane : public Widget {
public:
	Plane(Widget *parent, CreateParam *param);

	/* the plane, draw object 0 */
	graph::PlaneObj *m_plane;
};

}
}

#endif
