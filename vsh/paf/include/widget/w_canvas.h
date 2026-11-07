/*
 * paf::ui::Canvas: a plane that shows what is drawn on it with the front touch. Its painter
 * (canvas_painter.h) keeps the strokes in a picture whose texture the plane shows; a touch down
 * begins a stroke, a move continues it and the touch up ends it. The class and its constructors,
 * destructors and vtable are Sony's (export NIDs); the members' names are ours.
 */
#ifndef PAF_WIDGET_W_CANVAS_H
#define PAF_WIDGET_W_CANVAS_H

#include <stdint.h>
#include <std/memory>
#include <math/ngp/v4.h>
#include <widget/canvas_painter.h>
#include <widget/w_plane.h>

namespace paf {

namespace graph {
class Surface;
}

namespace ui {

class Canvas : public Plane {
public:
	/* the touch events its strokes follow (names ours) */
	enum {
		CB_TOUCH_DOWN = 0x1000B,
		CB_TOUCH_UP = 0x1000C,
		CB_TOUCH_MOVE = 0x1000D
	};

	Canvas(Widget *parent, CreateParam *param);
	virtual ~Canvas();

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;

	static const char m_widget_type[];

	CanvasPainter *m_painter;
	/* the painter's picture as a surface, the plane's texture */
	intrusive_ptr<graph::Surface> m_surface;
};

}
}

/* No Sony name for these exports: */
extern "C" {
/* the colour of the strokes */
paf::math::v4 ScePafWidget_ED270613(paf::ui::Canvas *canvas);
void ScePafWidget_DBB1C54C(paf::ui::Canvas *canvas, const paf::math::v4 &color);
/* the diameter of the strokes' points */
float ScePafWidget_5D806B1E(paf::ui::Canvas *canvas);
void ScePafWidget_D844AC3F(paf::ui::Canvas *canvas, float size);
/* the painter's two functions that do nothing */
void ScePafWidget_FD3E7E8A(paf::ui::Canvas *canvas);
void ScePafWidget_710ECEC2(paf::ui::Canvas *canvas);
}

#endif
