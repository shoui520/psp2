/*
 * paf::graph::DrawObj, as far as converted C++ uses it: what a widget draws,
 * with the draw style that says how (BindDrawStyle reads it at 0x30).
 *
 * Names after ScePaf-RE (restored from Sony's DWARF); the fields before the
 * draw style are not converted yet.
 */
#ifndef PAF_GRAPHICS_DRAWOBJ_DRAW_OBJ_H
#define PAF_GRAPHICS_DRAWOBJ_DRAW_OBJ_H

#include <stdint.h>

namespace paf {
namespace graph {

class DrawStyle {
public:
	uint32_t blend : 4;
	uint32_t depth : 4;
	uint32_t lighting : 4;
	uint32_t face_cull : 2;
	uint32_t scissor : 1;
	uint32_t reserved1 : 17;
};

class DrawObj {
public:
	virtual bool IsInherit(const char *type) const;

	uint8_t reserved[0x2c];
	DrawStyle m_draw_style;
};

}
}

#endif
