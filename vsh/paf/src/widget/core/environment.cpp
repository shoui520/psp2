/*
 * paf::ui::Environment, as far as it is converted: the display area scale the
 * root surface shows at. The rest of its functions are still ScePafWidget's.
 */
#include <widget/core/environment.h>
#include <graphics/surface/surface_display.h>
#include <system/system.h>

namespace paf {
namespace ui {

enum {
	/* the system setting the display area scale is in, in billionths (name ours) */
	SYSTEM_PARAM_DISPLAY_AREA_SCALE = 3137460
};

/* without the setting, the display area is 86.5% of the screen */
#define DISPLAY_AREA_SCALE_DEFAULT	0.865f

/*
 * The scale from the system settings: the whole screen at 1, the display area
 * otherwise, a little under the setting. The root surface shows either.
 */
static inline void ReadDisplayAreaSettings(Environment *env)
{
	float scale = DISPLAY_AREA_SCALE_DEFAULT;
	int value;

	if (sceRegMgrSystemParamGetInt(SYSTEM_PARAM_DISPLAY_AREA_SCALE, &value) == 0)
		scale = (float)(uint32_t)value / 1000000000.0f;
	if (scale != env->m_display_area_scale) {
		graph::DisplaySurface *surface = Screen::s_root_surface;

		if (scale == 1.0f) {
			env->m_display_area_scale = 1.0f;
			surface->SetFullDisplay();
		} else {
			env->m_display_area_scale = scale - 0.0005f;
			surface->SetDisplayArea();
		}
		env->m_display_area_changed = true;
	}
}

void Environment::UpdateDisplayAreaSettings()
{
	ReadDisplayAreaSettings(this);
}

void Environment::ApplyDisplayAreaSettings()
{
	if (!m_display_area_applied) {
		m_display_area_applied = true;
		ReadDisplayAreaSettings(this);
	}
}

}
}
