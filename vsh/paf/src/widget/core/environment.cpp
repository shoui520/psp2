/*
 * paf::ui::Environment, as far as it is converted: the display area scale the
 * root surface shows at. The rest of its functions are still ScePafWidget's.
 */
#include <widget/core/environment.h>
#include <graphics/surface/surface_display.h>
#include <system/system.h>
#include <widget/core/widget.h>

namespace paf {

/* Sony has the lookup in the scene entries inline. */
template<>
inline __attribute__((always_inline)) _Tree<_Tmap_traits<int32_t, ui::Widget *, less<int32_t> > >::iterator
_Tree<_Tmap_traits<int32_t, ui::Widget *, less<int32_t> > >::find(const int32_t &key)
{
	return iterator(_Find(key));
}

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


enum {
	/* no such scene entry, and the error the touch device regions give when all six are used
	   (0x80AF410F; name ours) */
	ENV_ERROR_NO_ENTRY = -2135998193
};

int32_t Environment::RegisterTouchDeviceRegion(Widget *widget)
{
	uint32_t mask = m_touch_region_mask;
	int32_t i = 0;
	uint32_t bit = 1;

	do {
		if ((mask & bit) == 0) {
			m_touch_region[i] = widget;
			m_touch_region_mask = mask | bit;
			return 0;
		}
		i++;
		bit *= 2;
	} while (i < 6);
	return ENV_ERROR_NO_ENTRY;
}

int32_t Environment::RemoveTouchDeviceRegion(Widget *widget)
{
	uint32_t mask = m_touch_region_mask;

	if (mask != 0) {
		int32_t i = 0;
		uint32_t bit = 1;

		do {
			if ((mask & bit) != 0 && m_touch_region[i] == widget) {
				mask &= ~bit;
				m_touch_region[i] = NULL;
				m_touch_region_mask = mask;
			}
			i++;
			bit *= 2;
		} while (i < 6);
	}
	return 0;
}

void Environment::ClearHitInfo(Widget *widget)
{
	for (int32_t port = 0; port < 2; port++) {
		for (int32_t id = 0; id < 8; id++) {
			if (m_touch_hit[port][id].m_widget == widget)
				m_touch_hit[port][id].m_widget = NULL;
			if (m_touch_hit_last[port][id].m_widget == widget)
				m_touch_hit_last[port][id].m_widget = NULL;
		}
	}
}

int32_t Environment::GetSceneNum(int32_t entry)
{
	map<int32_t, Widget *>::iterator it = m_scene_entries.find(entry);

	if (it == m_scene_entries.end())
		return ENV_ERROR_NO_ENTRY;
	return it->second->m_children.size();
}

int32_t Environment::GetSceneTop(int32_t entry, list<Widget *>::iterator &top)
{
	map<int32_t, Widget *>::iterator it = m_scene_entries.find(entry);

	if (it == m_scene_entries.end())
		return ENV_ERROR_NO_ENTRY;
	top = it->second->m_children.begin();
	return 0;
}

int32_t Environment::GetSceneEnd(int32_t entry, list<Widget *>::iterator &end)
{
	map<int32_t, Widget *>::iterator it = m_scene_entries.find(entry);

	if (it == m_scene_entries.end())
		return ENV_ERROR_NO_ENTRY;
	end = it->second->m_children.end();
	return 0;
}

int32_t Environment::ShowSceneEntry(int32_t entry)
{
	map<int32_t, Widget *>::iterator it = m_scene_entries.find(entry);

	if (it == m_scene_entries.end())
		return ENV_ERROR_NO_ENTRY;

	Widget *scene = it->second;

	if (!scene->m_transition_complete) {
		scene->m_transition = 0xFF;
		scene->m_transition_complete = true;
		scene->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(scene, NULL, 2, 0.0f, NULL);
	return 0;
}

int32_t Environment::HideSceneEntry(int32_t entry)
{
	map<int32_t, Widget *>::iterator it = m_scene_entries.find(entry);

	if (it == m_scene_entries.end())
		return ENV_ERROR_NO_ENTRY;

	Widget *scene = it->second;

	scene->m_transition_complete = false;
	scene->m_transition = 0xFF;
	scene->DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(scene, NULL, 2, 0.0f, NULL);
	return 0;
}

int32_t Environment::RegisterTouchDeviceRegion(uint32_t index, Widget *widget)
{
	if (index >= 6)
		return ENV_ERROR_NO_ENTRY;
	m_touch_region_mask |= 1U << index;
	m_touch_region[index] = widget;
	return 0;
}
}
}
