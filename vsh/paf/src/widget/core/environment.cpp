/*
 * paf::ui::Environment, as far as it is converted: the display area scale the
 * root surface shows at. The rest of its functions are still ScePafWidget's.
 */
#include <widget/core/environment.h>
#include <std/algorithm>
#include <inputdevice/touchscreen/touchscreen.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/surface/surface_display.h>
#include <graphics/gutil.h>
#include <system/system.h>
#include <timer/timer.h>
#include <widget/core/widget.h>
#include <widget/w_camera.h>
#include <widget/w_scene.h>

extern "C" int ScePafMisc_862C8916(void);

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

int32_t Environment::GetHitStack(HitInfo *info, Widget *widget)
{
	list<HitInfo>::iterator it = m_touch_hits.begin();

	while (it != m_touch_hits.end()) {
		if (it->m_widget == widget) {
			*info = *it;
			return 0;
		}
		++it;
	}
	return -1;
}

int32_t Environment::GetLastHitStack(HitInfo *info)
{
	if (m_touch_hits.begin() == m_touch_hits.end())
		return -1;
	*info = m_touch_hits.back();
	return 0;
}

bool Environment::IsHitStackParent(Widget *widget)
{
	list<Widget *>::iterator it = m_hit_stack.begin();

	while (it != m_hit_stack.end()) {
		if (*it == widget)
			return true;
		++it;
	}

	HitInfo info;

	return GetHitStack(&info, widget) == 0;
}

bool Environment::IsTermAvailable()
{
	return m_scene_entries.size() == 0 && m_pending.size() == 0 && m_term_requested;
}

int32_t Environment::RequestTerm()
{
	if (m_scene_entries.size() != 0 || m_pending.size() != 0 || !m_term_requested) {
		m_ev_doevent.clear();
		m_ev_subrouting.clear();
		m_ev_queue.Clear();
		m_callbacks.clear();
		for (map<int32_t, Widget *>::iterator it = m_scene_entries.begin();
		     it != m_scene_entries.end(); ++it) {
			Widget *scene = it->second;

			list<Widget *>::iterator child = scene->m_children.begin();

			while (child != scene->m_children.end()) {
				Widget *widget = *child;

				child++;
				widget->DestroyWidget();
			}
		}
		m_term_requested = true;
		m_suspended = true;
		return 1;
	}
	return m_term_requested;
}


int32_t Environment::SetScreen(Screen *screen)
{
	Screen *old = m_screen;

	if (old != NULL && old->is_auto_delete)
		delete old;
	m_screen = screen;
	if (screen != NULL)
		screen->is_auto_delete = false;
	return 0;
}

void Environment::OnUpdateTouchDeviceRegion()
{
	uint32_t mask = m_touch_region_mask;

	if (mask == 0 && m_touch_region_mask_pre == 0)
		return;

	uint32_t bit = 1;

	for (uint32_t i = 0; i < 6; i++, bit *= 2) {
		Widget *widget = m_touch_region[i];

		if ((mask & bit) != 0) {
			if (widget->m_show_a_sum >= 0.02f) {
				math::rect clip = widget->CalcClipRect();

				inputdevice::touchscreen::SetRegion(0, i, clip, true,
								    widget->m_touch_region_option);
				mask = m_touch_region_mask;
				continue;
			}
			inputdevice::touchscreen::SetRegion(0, i, *(const math::rect *)NULL, false,
							    false);
			mask = m_touch_region_mask;
		} else if ((m_touch_region_mask_pre & bit) != 0) {
			inputdevice::touchscreen::SetRegion(0, i, *(const math::rect *)NULL, false,
							    false);
			mask = m_touch_region_mask;
		}
	}
	m_touch_region_mask_pre = mask;
}

void Environment::OnUpdatePost()
{
	for (list<Widget *>::iterator it = m_scenes_onupdate.begin(); it != m_scenes_onupdate.end(); it++)
		(*it)->OnUpdate();
	graph::DrawObj::SetDirtyCount(2);
	UpdateRestructScene();
	if (m_scene_entries.size() != 0 || m_pending.size() != 0 || !m_term_requested) {
		m_focus_cursor.Update(0.0f);
		m_focus_cursor_rear.Update(0.0f);
	}
	UpdateDisplayAreaSettings();
	for (list<Widget *>::iterator it = m_scenes_onupdate.begin(); it != m_scenes_onupdate.end(); it++)
		(*it)->OnUpdatePost();
	OnUpdateTouchDeviceRegion();
	graph::DrawObj::SetDirtyCount(1);
}

void Environment::HideFocus(int32_t port)
{
	if ((m_focus_enabled || ScePafMisc_862C8916()) && m_focus_cursor.m_enabled && port < 2) {
		if (port >= 0) {
			(&m_focus_cursor)[port].Hide();
		} else {
			m_focus_cursor.Hide();
			m_focus_cursor_rear.Hide();
		}

		Event e(0x2004, 0x200000, 0x40000, 0, 0, 0, 0, 0);

		e.m_data[0] = 1;
		m_ev_queue.PushBack(e);
	}
}

void Environment::ShowFocus(int32_t port)
{
	if ((m_focus_enabled || ScePafMisc_862C8916()) && m_focus_cursor.m_enabled && port < 2) {
		if (port >= 0) {
			(&m_focus_cursor)[port].Show();
		} else {
			m_focus_cursor.Show();
			m_focus_cursor_rear.Show();
		}

		Event e(0x2004, 0x200000, 0x40000, 0, 0, 0, 0, 0);

		e.m_data[0] = 0;
		m_ev_queue.PushBack(e);
	}
}

int32_t OrderEntry::Init(Widget *widget, int32_t mark)
{
	if (widget == NULL) {
		m_order = -1;
		m_group = -1;
		m_depth = -1;
		return -1;
	}

	Scene *scene = widget->m_scene;
	uint64_t depth = -1;

	m_widget = widget;
	m_mark = mark;
	m_order = (255 - scene->m_priority) | (scene->m_scene_priority << 8);
	if (scene != NULL && widget->m_parent != NULL)
		depth = -1 - widget->GetLayerDepth();
	m_depth = depth;
	m_group = widget->m_scene->m_entry;
	return m_order;
}

int32_t ModalWidgetList::Remove(Widget *widget)
{
	if (widget == NULL)
		return -1;
	if (m_entries.count == 0)
		return -1;

	vector<OrderEntry>::iterator it = m_entries.begin();
	vector<OrderEntry>::iterator end = m_entries.end();

	for (; it != end; ++it) {
		if (it->m_widget == widget) {
			vector<OrderEntry>::iterator next = it;

			if (next.m_ptr != NULL)
				next = it + 1;
			m_entries.erase(it, next);
			m_unsorted = true;
			return 0;
		}
	}
	return -1;
}

OrderEntry *ModalWidgetList::Add(Widget *widget, int32_t mark)
{
	OrderEntry entry;

	if (widget != NULL) {
		entry.m_mark = mark;
		if (entry.Init(widget, mark) >= 0) {
			m_entries.push_back(entry);
			m_unsorted = true;
			return &m_entries[m_entries.count - 1];
		}
	}
	return NULL;
}

void ModalWidgetList::Sort()
{
	if (m_unsorted && m_entries.count != 0)
		sort(m_entries.begin(), m_entries.end());
	m_unsorted = false;
}

/* the end of the focus candidates, taken from the list again each time as Sony's code does */
#define End()	(m_focus_candidates.m_entries.ptr + m_focus_candidates.m_entries.count)

bool Environment::IsInFrontOfModal(Widget *widget)
{
	if (m_top_modal_widget == NULL && m_second_modal_widget == NULL)
		return true;

	OrderEntry limit;

	if (m_top_modal_widget != NULL)
		limit.Init(m_top_modal_widget, 0);
	if (m_second_modal_widget != NULL) {
		OrderEntry other;

		other.Init(m_second_modal_widget, 0);
		if (other < limit)
			limit = other;
	}

	OrderEntry entry;

	entry.Init(widget, 0);
	return !(limit < entry);
}

int32_t Environment::RegisterModalWidget(Widget *widget, uint32_t mode)
{
	if (mode < 2) {
		if (m_modal_widgets.Add(widget, mode) != NULL) {
			if (m_modal_widgets.m_unsorted && m_modal_widgets.m_entries.count != 0)
				sort(m_modal_widgets.m_entries.begin(), m_modal_widgets.m_entries.end());
			m_modal_widgets.m_unsorted = false;
			return 0;
		}
	}
	return -1;
}

void Environment::UpdateFocus(Widget *prefer, Widget *drop)
{
	Widget *previous = m_focus_widget;

	if (!(m_focus_enabled || ScePafMisc_862C8916()) || !m_focus_cursor.m_enabled)
		return;
	if (m_focus_candidates.m_unsorted && m_focus_candidates.m_entries.count != 0)
		sort(m_focus_candidates.m_entries.begin(), m_focus_candidates.m_entries.end());
	m_focus_candidates.m_unsorted = false;
	m_focus_widget = NULL;
	if (m_focus_auto_off)
		return;

	int32_t prefer_group = -1;
	OrderEntry best;

	if (prefer != NULL)
		prefer_group = prefer->m_scene->m_entry;
	if (m_top_modal_widget != NULL)
		best.Init(m_top_modal_widget, 0);
	if (m_second_modal_widget != NULL) {
		OrderEntry other;

		other.Init(m_second_modal_widget, 0);
		if (other < best)
			best = other;
	}

	OrderEntry *it = m_focus_candidates.m_entries.ptr;
	OrderEntry *chosen = it + m_focus_candidates.m_entries.count;
	OrderEntry *begin = it;

	if (drop != NULL) {
		for (OrderEntry *e = begin; e != chosen; e++) {
			if (e->m_widget == drop) {
				e->m_mark = 0;
				break;
			}
		}
	}

	int32_t chosen_group = -1;

	for (; it != End(); it++) {
		Widget *widget = it->m_widget;

		if (widget->IsInputTarget()) {
			Widget *root = ScePafWidget_CFA1EB7A(widget);
			Widget *state = root != NULL && root->m_focus_root != NULL ?
				(Widget *)root->m_focus_root->m_state : NULL;

			if (ScePafWidget_DB0FFD33(state) && !(best < *it)) {
				if (m_focus_widget == NULL) {
					m_focus_widget = it->m_widget;
					chosen_group = it->m_group;
					chosen = it;
				}
				break;
			}
		}
	}
	if (it != End()) {
		for (;;) {
			if (it == End()) {
				chosen->m_mark = 0x0f0f0f0f;
				break;
			}
			if (chosen_group != it->m_group) {
				chosen->m_mark = 0x0f0f0f0f;
				break;
			}
			if (!(best < *it)) {
				Widget *root = ScePafWidget_CFA1EB7A(it->m_widget);
				Widget *state = root != NULL && root->m_focus_root != NULL ?
					(Widget *)root->m_focus_root->m_state : NULL;

				if (ScePafWidget_DB0FFD33(state)) {
					Widget *widget = it->m_widget;

					if (prefer != NULL && prefer_group == chosen_group) {
						if (prefer == widget) {
							m_focus_widget = widget;
							it->m_mark = 0x0f0f0f0f;
							break;
						}
					} else if (previous == widget) {
						if (it->m_mark == 0x0f0f0f0f) {
							m_focus_widget = widget;
							break;
						}
					} else {
						it++;
						continue;
					}
				}
			}
			it->m_mark = 0;
			it++;
		}
	}
	if (m_focus_widget != NULL) {
		Widget *root = ScePafWidget_CFA1EB7A(m_focus_widget);

		if (root != NULL && root->m_focus_root != NULL && root->m_focus_root->m_state != 0)
			m_focus_cursor.SetTarget((Widget *)root->m_focus_root->m_state);
		else
			m_focus_cursor.SetTarget(m_focus_widget);

		FocusRoot *focus_root = m_focus_widget->m_focus_root;

		if (focus_root != NULL && focus_root->m_pending)
			focus_root->m_pending = false;
	} else {
		m_focus_cursor.ClearTarget(NULL);
		m_focus_cursor_rear.ClearTarget(NULL);
	}
}

#undef End

extern "C" bool ScePafWidget_A6B659BB(Environment *env, int32_t port)
{
	if (!env->m_focus_enabled && !ScePafMisc_862C8916())
		return false;
	if ((uint32_t)port < 2)
		return (&env->m_focus_cursor)[port].m_enabled;
	return false;
}

extern "C" bool ScePafWidget_529FB877(Environment *env, int32_t port)
{
	if (port >= 2)
		return false;
	if (port >= 0)
		return (&env->m_focus_cursor)[port].m_shown;

	bool shown = false;

	for (int32_t i = 0; i < 2; i++) {
		if ((&env->m_focus_cursor)[i].m_shown)
			return true;
	}
	return shown;
}

extern "C" SceneResolution *ScePafWidget_CDC90BA2(const EnvironmentParam *param, uint32_t id)
{
	SceneResolution *result = NULL;
	SceneResolution *it = param->m_resolution_list->m_data;
	SceneResolution *end = it + param->m_resolution_list->m_num;

	while (it != end) {
		if (it->m_id == id) {
			result = it;
			break;
		}
		if (it != NULL)
			it++;
	}
	return result;
}

extern "C" SceneResolution *ScePafWidget_615501F0(const EnvironmentParam *param, const IDParam &id)
{
	SceneResolution *result = NULL;
	SceneResolution *it = param->m_resolution_list->m_data;
	SceneResolution *end = it + param->m_resolution_list->m_num;

	if (it != end) {
		uint32_t idhash = id.m_idhash;

		while (it != end) {
			if (it->m_resource.m_value == idhash || it->m_base_resource.m_value == idhash) {
				result = it;
				break;
			}
			if (it != NULL)
				it++;
		}
	}
	return result;
}

extern "C" int32_t ScePafWidget_36505C86(const EnvironmentParam *param, const IDParam &id)
{
	SceneResolution *it = param->m_resolution_list->m_data;
	SceneResolution *end = it + param->m_resolution_list->m_num;

	if (it == end)
		return -1;

	uint32_t idhash = id.m_idhash;

	while (it != end) {
		if (it->m_resource.m_value == idhash)
			return it->m_id;
		if (it->m_base_resource.m_value == idhash)
			return it->m_base_id;
		if (it != NULL)
			it++;
	}
	return -1;
}

extern "C" {

void ScePafWidget_9D4A5A0E(Environment *env)
{
	graph::DisplaySurface *surface = Screen::s_root_surface;
	int32_t state = 0;

	if (surface != NULL) {
		surface->StartFrame();
		state = surface->m_frame_state;
	}

	bool suspended = env->m_suspended;

	env->m_frame_state = state;
	env->m_draw_frame = state == 1;
	if (suspended && !env->m_draw_when_suspended)
		env->m_draw_frame = false;
}

void ScePafWidget_75FB31F1(Environment *env, bool enable)
{
	env->m_focus_enabled = enable;
	env->m_focus_widget = NULL;
	env->UpdateFocus(NULL, NULL);
	if (!env->IsTermAvailable()) {
		env->m_focus_cursor.Update(0.0f);
		env->m_focus_cursor_rear.Update(0.0f);
	}
}

int32_t ScePafWidget_F67517A0(Environment *env)
{
	int32_t result;

	ScePafMisc_4357D271();
	if (Screen::s_root_screen == NULL)
		return -1;
	if (Screen::s_root_screen->m_whandle.width > 0) {
		result = ScePafGraphics_27416D74(Screen::s_root_screen->m_whandle.width,
						 Screen::s_root_screen->m_whandle.height,
						 Screen::s_root_screen->m_whandle.callback);
		if (result < 0)
			return result;
	}
	if (Screen::s_root_surface == NULL)
		return -1;
	Screen::s_root_surface->WaitFrame();
	return 0;
}

void ScePafWidget_8E61FE0C(Environment *env)
{
	ScePafGraphics_C5504119();
	ScePafMisc_EF1C440B();
}

}

int32_t Environment::Term()
{
	Widget::SetBaseDestroyCount(2);
	RequestTerm();
	while (!IsTermAvailable()) {
		OnUpdatePre();
		OnUpdatePost();
		OnDraw();
		ScePafMisc_4357D271();
		if (Screen::s_root_screen != NULL) {
			if (Screen::s_root_screen->m_whandle.width <= 0 ||
			    ScePafGraphics_27416D74(Screen::s_root_screen->m_whandle.width,
						    Screen::s_root_screen->m_whandle.height,
						    Screen::s_root_screen->m_whandle.callback) >= 0) {
				if (Screen::s_root_surface != NULL)
					Screen::s_root_surface->WaitFrame();
			}
		}
		ScePafGraphics_C5504119();
		ScePafMisc_EF1C440B();
	}
	return m_term_requested;
}

int32_t Environment::SetPadCaptureHigh(uint32_t port, Widget *widget)
{
	if (port >= 5)
		return -1;

	Widget *old = m_pad_capture_button[port];

	if (old != NULL)
		old->ReleasePadCaptureButton(255, true);
	m_pad_capture_button[port] = widget;
	return 0;
}

int32_t Environment::ReleasePadCaptureHigh(uint32_t port)
{
	if (port >= 5)
		return -1;

	Widget *widget = m_pad_capture_button[port];

	if (widget != NULL)
		return widget->ReleasePadCaptureButton(255, true);
	return -1;
}

int32_t Environment::SetPadCaptureLow(uint32_t port, Widget *widget)
{
	if (port >= 5)
		return -1;

	Widget *old = m_pad_capture_dir[port];

	if (old != NULL)
		old->ReleasePadCaptureDir(255, true);
	m_pad_capture_dir[port] = widget;
	return 0;
}

int32_t Environment::ReleasePadCaptureLow(uint32_t port)
{
	if (port >= 5)
		return -1;

	Widget *widget = m_pad_capture_dir[port];

	if (widget != NULL)
		return widget->ReleasePadCaptureDir(255, true);
	return -1;
}

float Environment::GetRollScreen()
{
	if (m_root_camera != NULL)
		return m_root_camera->GetRollScreen(0);
	return 0.0f;
}

float Environment::GetOrientationRoll(int32_t orientation)
{
	float roll = 0.0f;

	if (orientation != 0) {
		if (orientation == 2)
			roll = 1.5707964f;
		else if (orientation == 1)
			roll = 3.1415927f;
		else if (orientation == 3)
			roll = -1.5707964f;
	}
	return roll;
}

}
}
