/*
 * paf::ui::Environment, as far as converted C++ uses it: the widget system's
 * state for one framework.
 */
#ifndef PAF_WIDGET_CORE_ENVIRONMENT_H
#define PAF_WIDGET_CORE_ENVIRONMENT_H

#include <stdint.h>
#include <std/memory>
#include <math/ngp/v4.h>
#include <math/rect.h>

namespace paf {
namespace graph {
class Surface;
}

namespace ui {

/*
 * A render target: its clear colour, viewport and scissor, and its surface. The
 * viewport and scissor are rectangles here, where older builds kept four ints
 * each (ScePaf-RE's vp_* and sp_*).
 */
class Screen {
public:
	Screen();
	virtual ~Screen();

	static Screen *GetRootScreen()
	{
		return s_root_screen;
	}

	int32_t padding;
	math::v4 clear_color;
	bool is_auto_delete;
	bool is_cleared;
	math::rect vp;
	math::rect sp;
	int32_t clear_policy;
	intrusive_ptr<graph::Surface> m_surface;
	uint8_t m_whandle[0xc];
	int32_t reserved[2];

	static Screen *s_root_screen;
};

class EnvironmentParam {
public:
	virtual ~EnvironmentParam();
};

class Environment {
public:
	virtual ~Environment();

	/* whether nothing the widgets still run keeps the framework from ending */
	bool IsTermAvailable();
};

}
}

#endif
