/*
 * paf::ui::Environment, as far as it is converted: the display area scale the
 * root surface shows at. The rest of its functions are still ScePafWidget's.
 */
#include <widget/core/environment.h>
#include <std/algorithm>
#include <inputdevice/touchscreen/touchscreen.h>
#include <inputdevice/inputlistener.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/drawobj/draw_env.h>
#include <graphics/surface/surface_display.h>
#include <graphics/gutil.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/effects/wave_switch.h>
#include <graphics/effects/custom_trail.h>
#include <system/system.h>
#include <timer/timer.h>
#include <widget/core/widget.h>
#include <widget/core/uiinputlistener.h>
#include <widget/w_camera.h>
#include <widget/w_scene.h>

extern "C" int ScePafMisc_862C8916(void);
extern "C" int sceAppMgrGetRecommendedScreenOrientation(int32_t *orientation);
extern "C" int sceAppMgrSetShellScreenOrientation(int32_t orientation);
extern "C" int sceAppMgrSetRecommendedScreenOrientationActivated(int32_t activated);
extern "C" int vshMotionNoiseFilterIsAvailable(void);

namespace paf {

/* Sony has the lookup in the scene entries inline. */
template<>
inline __attribute__((always_inline)) _Tree<_Tmap_traits<int32_t, ui::Widget *, less<int32_t> > >::iterator
_Tree<_Tmap_traits<int32_t, ui::Widget *, less<int32_t> > >::find(const int32_t &key)
{
	return iterator(_Find(key));
}

/* Sony has the lookup in the widget groups inline too. */
template<>
inline __attribute__((always_inline)) _Tree<_Tmap_traits<uint32_t, ui::ModalWidgetList, less<uint32_t> > >::iterator
_Tree<_Tmap_traits<uint32_t, ui::ModalWidgetList, less<uint32_t> > >::find(const uint32_t &key)
{
	return iterator(_Find(key));
}

/* Sony has the unlinking of a texture request inline. */
template<>
inline __attribute__((always_inline)) list<ui::TextureRequest>::iterator
list<ui::TextureRequest>::erase(iterator where)
{
	_Node *node = where.node;
	_Node *next = node->next;

	node->next->prev = node->prev;
	node->prev->next = node->next;
	destroy(&node->value);
	operator delete(node);
	--count;
	return iterator(next);
}

/* Sony's AddSceneEntry calls the pending list's push_back rather than inlining it. */
template<>
inline __attribute__((noinline)) void list<ui::EntryRequest>::push_back(const ui::EntryRequest &val)
{
	if (head == NULL)
		return;

	_Node *node = (_Node *)operator new(sizeof(_Node));

	if (node == NULL)
		ScePafStdc_D608246A();
	if (node != NULL)
		new (&node->value) ui::EntryRequest(val);
	else
		node = head;
	if (node != head) {
		node->prev = head->prev;
		node->next = head;
		head->prev->next = node;
		head->prev = node;
		++count;
	}
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

int32_t Environment::ReleaseTouchCapture(TouchCapture *capture)
{
	if (capture->m_widget == NULL)
		return -1;
	capture->m_port = -1;
	capture->m_id = -1;
	capture->m_widget = NULL;
	capture->m_touch = NULL;
	return 0;
}

extern "C" DragState *ScePafWidget_DE6905E2(Environment *env, TouchCapture *capture)
{
	if (capture == NULL)
		return NULL;
	if (capture->m_widget == NULL)
		return NULL;

	DragState *state = &env->m_drag_state[capture->m_port][capture->m_id];

	if (state->m_widget != NULL)
		return NULL;
	state->Reset();
	state->m_port = capture->m_port;
	state->m_id = capture->m_id;
	state->m_widget = capture->m_widget;
	state->m_capture = capture;
	return state;
}

void Environment::ReleaseFocusBeyond(Widget *widget)
{
	if (!reserved4_345)
		return;

	uint32_t group = 0xffffffff;

	if (widget != NULL)
		group = widget->m_scene->m_entry;

	OrderEntry *it = m_focus_candidates.m_entries.ptr;

	while (it != m_focus_candidates.m_entries.ptr + m_focus_candidates.m_entries.count) {
		if (it->m_widget != NULL) {
			if ((uint32_t)it->m_group >= group)
				return;

			Widget *root = ScePafWidget_CFA1EB7A(it->m_widget);

			if (root != NULL && root->m_focus_root != NULL && root->m_focus_root->m_state != 0)
				((Widget *)root->m_focus_root->m_state)->ReleaseFocus();
		}
		it++;
	}
}

void Environment::UpdateOrderedTop()
{
	OrderEntry *begin = m_ordered_widgets.m_entries.ptr;
	int32_t count = m_ordered_widgets.m_entries.count;

	if (m_ordered_widgets.m_unsorted && count != 0)
		sort(m_ordered_widgets.m_entries.begin(), m_ordered_widgets.m_entries.end());
	begin = m_ordered_widgets.m_entries.ptr;
	m_ordered_widgets.m_unsorted = false;
	m_display_area_scene = NULL;
	m_second_modal_widget = NULL;
	for (OrderEntry *it = begin; it != m_ordered_widgets.m_entries.ptr + m_ordered_widgets.m_entries.count;
	     ++it) {
		Widget *widget = it->m_widget;

		if (widget->IsInputTarget() &&
		    (widget->m_color_alpha * widget->m_meta_a_sum) * widget->m_show_a_sum >= 0.95f) {
			m_display_area_scene = widget->m_scene;
			m_second_modal_widget = widget;
			m_ordered_widgets.m_widget = widget;
			break;
		}
	}
}

void Environment::UpdateModalWidgets()
{
	OrderEntry *begin = m_modal_widgets.m_entries.ptr;
	int32_t count = m_modal_widgets.m_entries.count;

	if (m_modal_widgets.m_unsorted && count != 0)
		sort(m_modal_widgets.m_entries.begin(), m_modal_widgets.m_entries.end());
	begin = m_modal_widgets.m_entries.ptr;
	m_modal_widgets.m_unsorted = false;

	bool cancel_found = false;

	for (vector<OrderEntry>::iterator it = begin; it != m_modal_widgets.m_entries.end(); ++it) {
		Widget *widget = it->m_widget;

		if (widget->IsInputTarget() || widget->m_show_a_sum >= 0.02f || widget->m_transition_in ||
		    widget->m_transition_out) {
			int32_t mark = it->m_mark;

			if (mark == 0) {
				Widget *top = m_top_modal_widget;

				if (top == NULL) {
					inputdevice::touchscreen::Lock(1);
					widget = it->m_widget;
					top = m_top_modal_widget;
				}
				if (top != widget) {
					ReleaseTouchesBehind(widget);
					widget = it->m_widget;
				}
				m_top_modal_widget = widget;
				m_modal_widgets.m_widget = widget;
				goto done;
			}
			if (mark == 1 && !cancel_found) {
				m_cancel_widget = widget;
				cancel_found = true;
				m_modal_widgets.m_widget = widget;
			}
		}
	}
	if (m_top_modal_widget != NULL) {
		inputdevice::touchscreen::Unlock(1);
		m_top_modal_widget = NULL;
		m_modal_widgets.m_widget = NULL;
	}
done:
	if (!cancel_found && m_cancel_widget != NULL) {
		m_cancel_widget = NULL;
		m_modal_widgets.m_widget = NULL;
	}
}

int32_t Environment::AddTextureRequest(Widget *widget, const intrusive_ptr<TextureSource> *source,
				       uint8_t obj, uint8_t bit)
{
	if (widget == NULL || widget->m_hold_count > 0)
		return -1;

	TextureRequest request;

	request.m_source = *source;
	request.m_widget = widget;
	request.m_obj = obj;
	request.m_bit = bit;
	if (m_texture_requests_busy)
		m_texture_requests_add.push_back(request);
	else
		m_texture_requests.push_back(request);
	return 0;
}

int32_t Environment::RemoveTextureRequest(Widget *widget, int32_t obj, int32_t bit)
{
	if (widget == NULL)
		return -1;

	if (m_texture_requests_busy) {
		for (list<TextureRequest>::iterator it = m_texture_requests_add.begin();
		     it != m_texture_requests_add.end(); ++it) {
			if (it->m_widget == widget && it->m_obj == obj && it->m_bit == bit) {
				m_texture_requests_add.erase(it);
				return 0;
			}
		}
	}
	for (list<TextureRequest>::iterator it = m_texture_requests.begin(); it != m_texture_requests.end();
	     ++it) {
		if (it->m_widget == widget && it->m_obj == obj && it->m_bit == bit) {
			if (m_texture_requests_busy) {
				TextureRequest removed;

				removed.m_widget = widget;
				removed.m_obj = obj;
				removed.m_bit = bit;
				m_texture_requests_remove.push_back(removed);
			} else {
				m_texture_requests.erase(it);
			}
			return 0;
		}
	}
	return -1;
}

intrusive_ptr<TextureSource> Environment::FindTextureRequest(Widget *widget, int32_t obj, int32_t bit)
{
	if (widget == NULL || m_texture_requests.size() == 0)
		return intrusive_ptr<TextureSource>();

	for (list<TextureRequest>::iterator it = m_texture_requests.begin(); it != m_texture_requests.end();
	     ++it) {
		if (it->m_widget == widget && it->m_obj == obj && it->m_bit == bit)
			return it->m_source;
	}
	if (m_texture_requests_busy) {
		for (list<TextureRequest>::iterator it = m_texture_requests_add.begin();
		     it != m_texture_requests_add.end(); ++it) {
			if (it->m_widget == widget && it->m_obj == obj && it->m_bit == bit)
				return it->m_source;
		}
	}
	return intrusive_ptr<TextureSource>();
}

int32_t Environment::RemoveTextureRequests(Widget *widget)
{
	if (widget != NULL) {
		if (m_texture_requests_busy) {
			list<TextureRequest>::iterator it = m_texture_requests_add.begin();

			while (it != m_texture_requests_add.end()) {
				if (it->m_widget == widget)
					it = m_texture_requests_add.erase(it);
				else
					++it;
			}
		}

		list<TextureRequest>::iterator it = m_texture_requests.begin();

		while (it != m_texture_requests.end()) {
			if (it->m_widget == widget) {
				if (m_texture_requests_busy) {
					TextureRequest removed;

					removed.m_widget = widget;
					removed.m_obj = it->m_obj;
					removed.m_bit = it->m_bit;
					m_texture_requests_remove.push_back(removed);
					++it;
				} else {
					it = m_texture_requests.erase(it);
				}
			} else {
				++it;
			}
		}
	}
	return -1;
}

void Environment::ApplyResize()
{
	if (m_resize_pending && Screen::s_root_surface != NULL) {
		graph::Surface *surface = m_screen->m_surface.get();

		if (m_resize_width != surface->GetWidth() || m_resize_height != surface->GetHeight()) {
			bool resized = Screen::s_root_surface->Resize(m_resize_width, m_resize_height) == 0;

			surface = m_screen->m_surface.get();
			if (resized)
				m_display_area_changed = true;
			m_resize_width = surface->GetWidth();
			m_resize_height = surface->GetHeight();
		}
		m_resize_pending = false;
	}
}

void Environment::UpdateTextureRequests()
{
	if (m_texture_requests.size() == 0 && m_texture_requests_add.size() == 0)
		return;

	m_texture_requests_busy = true;

	list<TextureRequest>::iterator it = m_texture_requests.begin();

	while (it != m_texture_requests.end()) {
		if (it->m_source.get() != NULL && it->m_source->GetType() == 2) {
			int32_t result = it->m_widget->OnTextureSource(&it->m_source, it->m_obj, it->m_bit);

			if (result != 0 && result != (int32_t)0x80AF5022) {
				++it;
			} else {
				it->m_source = intrusive_ptr<TextureSource>();
				it = m_texture_requests.erase(it);
			}
		} else {
			++it;
		}
	}
	m_texture_requests_busy = false;

	while (m_texture_requests_remove.size() != 0) {
		TextureRequest request = m_texture_requests_remove.front();

		RemoveTextureRequestOf(request);
		m_texture_requests_remove.erase(m_texture_requests_remove.begin());
	}
	while (m_texture_requests_add.size() != 0) {
		TextureRequest request = m_texture_requests_add.front();

		m_texture_requests.push_back(request);
		m_texture_requests_add.erase(m_texture_requests_add.begin());
	}
}

int32_t Environment::RemoveTextureRequestOf(const TextureRequest &request)
{
	for (list<TextureRequest>::iterator it = m_texture_requests.begin(); it != m_texture_requests.end();
	     ++it) {
		if (it->m_widget == request.m_widget && it->m_obj == request.m_obj &&
		    it->m_bit == request.m_bit) {
			m_texture_requests.erase(it);
			return 0;
		}
	}
	return -1;
}

void Environment::Init()
{
	for (int32_t port = 0; port < 2; port++) {
		for (int32_t id = 0; id < 8; id++) {
			m_touch_state[port][id].m_id = id;
			m_touch_state[port][id].m_port = port;
		}
	}
	m_scene_entries.clear();
	sce_paf_memset(m_pad_capture_dir, 0, 20);
	sce_paf_memset(m_pad_capture_button, 0, 20);
	if (m_focus_candidates.m_entries.limit < 16)
		m_focus_candidates.m_entries.reserve(16);
	m_ev_doevent.clear();
	m_ev_subrouting.clear();
	m_ev_queue.Clear();
	m_callbacks.clear();
	m_touch_region_mask = 0;
	m_touch_region_mask_pre = 0;
	sce_paf_memset(m_touch_region, 0, 24);
	Widget::SetBaseDestroyCount(2);
	if (m_entry_ranges.limit < 4)
		m_entry_ranges.reserve(4);
}

Environment::Environment(Resolution *env_param, int32_t screen_orientation_mode)
	: m_focus_history_on(true), m_callbacks_busy(false), m_scene_restruct(false),
	  m_layer_weight_changed(false), m_display_area_scale(1.0f), m_display_area_applied(false),
	  m_display_area_changed(false), m_texture_requests_busy(false),
	  m_reserved_word_e(0), m_screen_orientation_mode(screen_orientation_mode), m_orientation(4),
	  m_env_param(env_param), m_reserved_flag7(false), m_root_widget(NULL),
	  m_root_camera(NULL), m_camera_roll_mode(0), m_orientation_dirty(false), m_frame_state(0),
	  m_draw_frame(false), m_term_requested(false), m_suspended(false),
	  m_draw_when_suspended(false), m_env_option(true), m_sound_callback(NULL),
	  m_sound_callback_data(NULL), m_create_widget_callback(NULL), m_cancel_widget(NULL),
	  m_top_modal_widget(NULL), m_display_area_scene(NULL), m_second_modal_widget(NULL),
	  m_focus_widget(NULL), m_focus_mode(0), m_focus_auto_off(false), reserved4_345(true),
	  m_focus_enabled(true), m_ev_queue(32), m_screen(NULL), m_reserved_flag_a(false),
	  m_reserved_flag_b(true), m_finish_after_draw(false), m_reserved_word_a(1),
	  m_motion_noise_filter(false)
{
	m_reserved_flag_c[0] = false;
	m_reserved_flag_c[1] = false;
	m_reserved_flag_c[2] = false;
	m_reserved_flag_c[3] = false;
	m_renderer_param[0] = 0;
	m_renderer_param[1] = 0;

	Init();
	thread::ThreadIDCache::SetCurrentType(thread::ThreadIDCache::Type_Main);
	SetScreen(new Screen());

	uint32_t resolution = SelectResolution();
	SceneResolution *res = ScePafWidget_CDC90BA2(m_env_param, resolution);

	if (res != NULL) {
		m_screen->vp = math::rect(0.0f, 0.0f, (float)res->m_viewport_width,
					  (float)res->m_viewport_height);
		m_screen->sp = math::rect(0.0f, 0.0f, (float)res->m_scissor_width,
					  (float)res->m_scissor_height);
	}

	/* the input listeners: pad, both touch panels, motion, the Bluetooth and HID remotes, and a
	   plain listener of type 10 */
	m_pad_gesture_listener = common::SharedPtr<inputdevice::InputListener>(new UIPadListener(this));
	m_touch_gesture_listener_front =
		common::SharedPtr<inputdevice::InputListener>(new UITouchListener(this, 0));
	m_touch_gesture_listener_rear =
		common::SharedPtr<inputdevice::InputListener>(new UITouchRearListener(this));
	m_motion_gesture_listener =
		common::SharedPtr<inputdevice::InputListener>(new UIMotionListener(this));
	m_btrm_gesture_listener =
		common::SharedPtr<inputdevice::InputListener>(new UIBtrmListener(this));
	m_hidrm_gesture_listener =
		common::SharedPtr<inputdevice::InputListener>(new UIHidrmListener(this));
	m_keyboard_listener = common::SharedPtr<inputdevice::InputListener>(new inputdevice::InputListener());
	m_keyboard_listener->m_type = 10;
	inputdevice::AddInputListener(m_touch_gesture_listener_front);

	/* the root scene, of the root screen's size, and its camera */
	m_root_widget = new Scene(this, 0, NULL, m_env_param, 128);

	Scene *root = (Scene *)m_root_widget;

	root->m_screen->vp = math::rect(Screen::s_root_screen->vp);
	((Scene *)m_root_widget)->m_screen->sp = math::rect(Screen::s_root_screen->sp);
	root = (Scene *)m_root_widget;
	root->m_scissor_to_scene = false;
	root->m_display_area_mode = 2;
	ScePafWidget_83AD517E(root->m_camera, root->m_camera);
	m_root_widget->m_name = IDParam(string("env_top_screen"));
	m_root_widget->ChangeResolution_core(resolution, true, true);
	if (!m_root_widget->m_drag_begun)
		m_root_widget->m_focus_target = false;
	m_root_camera = new Camera(m_root_widget, NULL);
	m_root_camera->SetRollScreen(0.0f, NULL);

	/* the debug texture: 8 by 8, white, with a clear 4 by 4 middle */
	m_debug_texture = intrusive_ptr<graph::Surface>(new graph::Surface(
		gutil::GetDefaultSurfacePool(), 8, 8, ImageMode_RGBA8888, ImageOrder_Linear, 1, 1,
		false));

	uint32_t *pixels = (uint32_t *)m_debug_texture->Lock(0);

	sce_paf_memset(pixels, 0xFF, m_debug_texture->GetPageSize());
	for (int32_t y = 2; y < 6; y++) {
		for (int32_t x = 2; x < 6; x++)
			pixels[y * 8 + x] = 0x00FFFFFF;
	}
	m_debug_texture->Unlock();
	m_debug_texture->SetDivideMode(3);

	for (int32_t port = 0; port < 2; port++)
		(&m_focus_cursor)[port].CreateCursor(m_root_widget);
	m_motion_noise_filter = vshMotionNoiseFilterIsAvailable() != 0;
	sce_paf_printf_dummy();
}

/* the GPU waited for, the focus frame textures and the input listeners given up, the screen
   deleted and the event and callback lists emptied; the members then end in their order */
Environment::~Environment()
{
	ScePafGraphics_E7B32741(graph::arl::Gl());
	ScePafWidget_B6C64486(intrusive_ptr<graph::Surface>(), 0);

	inputdevice::DelInputListener(m_pad_gesture_listener);
	inputdevice::DelInputListener(m_touch_gesture_listener_front);
	inputdevice::DelInputListener(m_touch_gesture_listener_rear);
	inputdevice::DelInputListener(m_motion_gesture_listener);
	inputdevice::DelInputListener(m_btrm_gesture_listener);
	inputdevice::DelInputListener(m_hidrm_gesture_listener);
	inputdevice::DelInputListener(m_keyboard_listener);

	delete m_screen;

	m_ev_doevent.clear();
	m_ev_subrouting.clear();
	m_ev_queue.Clear();
	m_callbacks.clear();
}

extern "C" int32_t ScePafWidget_094DF388(Environment *env, const EntryRange *range)
{
	uint32_t first = range->m_first;
	uint32_t last = range->m_last;

	if (first > 0xFFFF || last > 0xFFFF || last < first)
		return 0x80AF410F;
	for (vector<EntryRange>::iterator it = env->m_entry_ranges.begin(); it != env->m_entry_ranges.end(); ++it) {
		if (first >= it->m_first && first <= it->m_last)
			return 0x80AF410F;
		if (last >= it->m_first && last <= it->m_last)
			return 0x80AF410F;
		if (first <= it->m_first && last >= it->m_last)
			return 0x80AF410F;
	}
	env->m_entry_ranges.push_back(*range);
	for (vector<OrderEntry>::iterator entry = env->m_focus_candidates.m_entries.begin();
	     entry != env->m_focus_candidates.m_entries.end(); ++entry) {
		uint32_t priority = entry->m_widget->m_scene->m_scene_priority;
		uint32_t group = priority;

		for (vector<EntryRange>::iterator it = env->m_entry_ranges.begin(); it != env->m_entry_ranges.end();
		     ++it) {
			if (priority >= it->m_first && priority <= it->m_last) {
				group = it->m_first;
				break;
			}
		}
		entry->m_group = group;

		list<Widget *>::iterator top = NULL;
		list<Widget *>::iterator end = NULL;

		env->GetSceneTop(entry->m_widget->m_scene->m_scene_priority, top);
		env->GetSceneEnd(entry->m_widget->m_scene->m_scene_priority, end);
		for (; top != end; ++top)
			static_cast<Scene *>(*top)->m_entry = entry->m_group;
	}
	return 0;
}

extern "C" int32_t ScePafWidget_E8F38C84(Environment *env, uint16_t width, uint16_t height)
{
	graph::DisplaySurface *root = Screen::s_root_surface;

	if (root == NULL)
		return 0x80AF0001;
	if (root->m_frame_buffer_mode == 4 || root->m_frame_buffer_mode == 2)
		return 0x80AF0004;
	if ((float)width > (float)root->m_max_width || (float)height > (float)root->m_max_height)
		return 0x80AF008F;
	env->m_resize_width = width;
	env->m_resize_height = height;
	env->m_resize_pending = true;
	return 0;
}

extern "C" int32_t ScePafWidget_E6D215ED(Environment *env, Widget *scene, int32_t entry)
{
	if (entry < 0 || entry > 0xFFFF)
		return 0x80AF410F;

	env->m_pending.push_back(EntryRequest(entry, scene));
	if (env->m_scene_entries.find(entry) == env->m_scene_entries.end()) {
		Widget *root = new Widget(NULL, NULL);

		env->m_scene_entries[entry] = root;
		root->SetEnvironment(env);
		root->m_name.SetID(string("root_widget"));
	}
	return 0;
}

extern "C" int32_t ScePafWidget_F5842A6B(Environment *env, uint32_t priority, const EntryRange **range)
{
	for (vector<EntryRange>::iterator it = env->m_entry_ranges.begin(); it != env->m_entry_ranges.end(); ++it) {
		if (priority >= it->m_first && priority <= it->m_last) {
			*range = &*it;
			return 0;
		}
	}
	return 0x80AF008F;
}

int32_t Environment::SelectResolution()
{
	if (m_env_param == NULL)
		m_env_param = new Resolution((vector<uint32_t> *)NULL);

	Resolution *param = m_env_param;
	Screen *screen = m_screen;
	int32_t count = param->GetResolutionIdHD(screen->m_surface->GetWidth(),
						 screen->m_surface->GetHeight(), 0) != 255;
	uint32_t found = param->GetResolutionIdHD(screen->m_surface->GetWidth(),
						  screen->m_surface->GetHeight(), 2);

	if (found != 255)
		count++;
	if (param->GetResolutionIdHD(screen->m_surface->GetWidth(), screen->m_surface->GetHeight(), 1) !=
	    255)
		count++;
	if (param->GetResolutionIdHD(screen->m_surface->GetWidth(), screen->m_surface->GetHeight(), 3) !=
	    255)
		count++;
	if (count > 1)
		sceAppMgrSetRecommendedScreenOrientationActivated(1);

	int32_t orientation;

	if (m_screen_orientation_mode == 4) {
		int32_t recommended = 1;
		int32_t mode;

		sceAppMgrGetRecommendedScreenOrientation(&recommended);
		if (recommended > 2) {
			if (recommended <= 3)
				mode = 2;
			else if (recommended <= 4)
				mode = 3;
			else
				mode = 4;
		} else if (recommended > 0) {
			mode = recommended > 1;
		} else {
			mode = 4;
		}
		m_screen_orientation_mode = mode;
		orientation = recommended;
	} else {
		int32_t mode = m_screen_orientation_mode;

		if (mode <= 1) {
			if (mode >= 0)
				orientation = mode <= 0 ? 1 : 2;
			else
				orientation = -1;
		} else if (mode <= 2) {
			orientation = 3;
		} else if (mode > 3) {
			orientation = -1;
		} else {
			orientation = 4;
		}
	}

	uint32_t id = param->GetResolutionIdHD(m_screen->m_surface->GetWidth(),
					       m_screen->m_surface->GetHeight(),
					       m_screen_orientation_mode);

	if (id == 255) {
		id = m_env_param->GetResolutionIdHD(m_screen->m_surface->GetWidth(),
						    m_screen->m_surface->GetHeight(), 0);
		if (id != 255) {
			m_screen_orientation_mode = 0;
			orientation = 1;
		} else {
			id = m_env_param->GetResolutionIdHD(m_screen->m_surface->GetWidth(),
							    m_screen->m_surface->GetHeight(), 2);
			if (id != 255) {
				m_screen_orientation_mode = 2;
				orientation = 3;
			} else {
				id = m_env_param->GetResolutionIdHD(m_screen->m_surface->GetWidth(),
								    m_screen->m_surface->GetHeight(), 1);
				if (id != 255) {
					m_screen_orientation_mode = 1;
					orientation = 2;
				} else {
					id = m_env_param->GetResolutionIdHD(
						m_screen->m_surface->GetWidth(),
						m_screen->m_surface->GetHeight(), 3);
					if (id != 255) {
						m_screen_orientation_mode = 3;
						orientation = 4;
					} else {
						common_internal::on_error();
						switch (m_screen_orientation_mode) {
						case 0:
							orientation = 1;
							id = 255;
							break;
						case 1:
							orientation = 2;
							id = 255;
							break;
						case 2:
							orientation = 3;
							id = 255;
							break;
						case 3:
							orientation = 4;
							id = 255;
							break;
						default:
							orientation = -1;
							id = 255;
							break;
						}
					}
				}
			}
		}
	}
	sceAppMgrSetShellScreenOrientation(orientation);
	return id;
}

void Environment::UpdateOrientation()
{
	int32_t recommended = 1;
	int32_t orientation;

	sceAppMgrGetRecommendedScreenOrientation(&recommended);
	if (recommended > 2) {
		if (recommended <= 3)
			orientation = 2;
		else if (recommended <= 4)
			orientation = 3;
		else
			orientation = 4;
	} else if (recommended <= 0) {
		orientation = 4;
	} else {
		orientation = recommended > 1;
	}

	for (map<int32_t, Widget *>::iterator it = m_scene_entries.begin(); it != m_scene_entries.end();
	     ++it) {
		Widget *entry = it->second;

		for (list<Widget *>::iterator child = entry->m_children.begin();
		     child != entry->m_children.end(); ++child) {
			if ((*child)->m_scene->IsOrientationAvailable(orientation)) {
				sceAppMgrSetShellScreenOrientation(recommended);
				m_orientation = orientation;
				m_orientation_dirty = false;
				return;
			}
		}
	}
}

int32_t Environment::ReleaseTouchesBehind(Widget *widget)
{
	if (widget == NULL)
		return -1;
	ScePafWidget_ABF05727(this, 1, false, NULL);

	OrderEntry limit;
	OrderEntry entry;

	limit.Init(widget, 0);
	for (int32_t i = 0; i < 8; i++) {
		Widget *held = m_touch_capture[0][i].m_widget;

		if (held != NULL) {
			entry.Init(held, 0);
			if (limit < entry)
				ScePafWidget_931AD41E(held, true);
		}
	}
	return 0;
}

void Environment::UpdateDisplayArea()
{
	graph::DisplaySurface *surface = Screen::s_root_surface;

	if (surface != NULL && surface->m_frame_buffer_mode == gutil::FrameBuffer_Given) {
		if (surface->UsesDisplayArea()) {
			if (!m_display_area_applied) {
				m_display_area_applied = true;
				ReadDisplayAreaSettings(this);
			}
		} else {
			float scale = m_display_area_scale;

			m_display_area_applied = false;
			if (scale != 1.0f) {
				m_display_area_scale = 1.0f;
				Screen::s_root_surface->SetFullDisplay();
				m_display_area_changed = true;
			}
		}
	}
	if (m_display_area_changed) {
		for (list<Widget *>::iterator it = m_scenes_onupdate.begin(); it != m_scenes_onupdate.end();
		     ++it) {
			Scene *scene = static_cast<Scene *>(*it);

			if (scene != m_root_widget)
				scene->UpdateScreenSize(NULL);
		}
		m_display_area_changed = false;
	}
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

int32_t Environment::AddScene(Widget *scene, int32_t entry, Widget *before)
{
	if (scene == NULL)
		return -1;
	if (entry < 0 || entry > 0xFFFF)
		return -1;

	map<int32_t, Widget *>::iterator it = m_scene_entries.find(entry);
	Widget *root;

	if (it == m_scene_entries.end()) {
		root = new Widget(NULL, NULL);
		m_scene_entries[entry] = root;
		root->SetEnvironment(this);
		root->m_name.SetID(string("root_widget"));
	} else {
		root = it->second;
	}
	scene->SetParent_core(root, before);

	Scene *added = static_cast<Scene *>(scene);
	uint32_t priority = added->m_scene_priority;
	uint32_t group = priority;

	for (vector<EntryRange>::iterator range = m_entry_ranges.begin(); range != m_entry_ranges.end();
	     ++range) {
		if (priority >= range->m_first && priority <= range->m_last) {
			group = range->m_first;
			break;
		}
	}
	added->m_entry = group;
	m_scene_restruct = true;
	return 0;
}

int32_t Environment::RemoveScene(Widget *scene)
{
	int32_t entry = static_cast<Scene *>(scene)->m_scene_priority;

	if (entry < 0)
		return ENV_ERROR_NO_ENTRY;

	map<int32_t, Widget *>::iterator it = m_scene_entries.find(entry);

	if (it == m_scene_entries.end()) {
		for (it = m_scene_entries.begin(); it != m_scene_entries.end(); ++it) {
			if (it->second->FindChild(scene, 2) != NULL) {
				it->second->RemoveChild(scene);
				m_scene_restruct = true;
				return 0;
			}
		}
		return -2135998204;
	}
	it->second->RemoveChild(scene);
	m_scene_restruct = true;
	return 0;
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

void ModalWidgetList::Refresh()
{
	for (vector<OrderEntry>::iterator it = m_entries.begin(); it != m_entries.end(); ++it)
		it->Init(it->m_widget, it->m_mark);
	if (m_entries.count != 0) {
		m_unsorted = true;
		sort(m_entries.begin(), m_entries.end());
	}
	m_unsorted = false;
}

int32_t Environment::RefreshOrderLists(bool force)
{
	if (force || m_layer_weight_changed) {
		m_modal_widgets.Refresh();
		m_ordered_widgets.Refresh();
		m_focus_candidates.Refresh();
		if (m_widget_groups.size() != 0) {
			for (map<uint32_t, ModalWidgetList>::iterator it = m_widget_groups.begin();
			     it != m_widget_groups.end(); ++it)
				it->second.Refresh();
		}
		m_layer_weight_changed = false;
		return 0;
	}
	return m_layer_weight_changed;
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

void ScePafWidget_BA5195CA(Environment *env)
{
	vector<Screen *> &targets = Screen::s_target_list;

	if (targets.ptr != NULL) {
		operator delete(targets.ptr);
		targets.ptr = NULL;
		targets.count = 0;
		targets.limit = 0;
	}

	Screen *root = Screen::s_root_screen;

	if (root != NULL) {
		graph::arl::ArlInstance *gl = graph::arl::Gl();

		intrusive_ptr<graph::Surface> scene_surface = root->m_surface;

		gl->SetSceneData(scene_surface);

		graph::DisplaySurface *display = Screen::s_root_surface;

		if (display->m_frame_buffer_mode <= 1) {
			if (ScePafGraphics_DD2002B4()) {
				if (env->m_draw_frame)
					display->MarkCleared();
			} else {
				Widget *scene = env->m_display_area_scene;
				bool scaled = scene != NULL;

				if (scene != NULL && env->m_display_area_applied &&
				    env->m_display_area_scale != 1.0f &&
				    static_cast<Scene *>(scene)->IsDisplayAreaScaled())
					scaled = false;
				if (env->m_env_option && !scaled) {
					intrusive_ptr<graph::Surface> surface = root->m_surface;
					bool dirty = surface->IsDirty();

					if (dirty) {
						if (root->OnTarget() == 0)
							root->OnClear();
						root->OnFinish();
					} else {
						intrusive_ptr<graph::Surface> target = root->m_surface;

						target->MarkCleared();
					}
				} else {
					intrusive_ptr<graph::Surface> target = root->m_surface;

					target->MarkCleared();
				}
			}
		}
	}
	if (env->m_draw_frame)
		ScePafGraphics_BE4C5403();

	graph::DrawEnv *draw_env = graph::DrawCalls().get();

	draw_env->m_counted = false;
	draw_env->m_count++;
	env->m_draw_frame = false;
}

void ScePafWidget_851911CB(Environment *env)
{
	graph::arl::ArlInstance *gl = graph::arl::Gl();

	ScePafGraphics_E7B32741(gl);
	if (env->m_draw_frame) {
		ScePafGraphics_47F42806();
		ScePafGraphics_3AFA1144();
		ScePafGraphics_9409C495();
	}

	graph::DisplaySurface *surface = Screen::s_root_surface;
	int32_t state = 0;

	if (surface != NULL) {
		surface->EndFrame();
		surface->Swap();
		state = surface->m_frame_state;
	}
	env->m_frame_state = state;
	env->ApplyResize();
	if (env->m_finish_after_draw)
		ScePafGraphics_E4B3B1D2(graph::arl::Gl());
	ScePafWidget_BA5195CA(env);
}

static inline inputdevice::InputGestureListener *GestureListenerOf(Environment *env, uint32_t port)
{
	if (port == 0 || port == 1)
		return static_cast<inputdevice::InputGestureListener *>(
			env->GetTouchGestureListener(port).get());
	return NULL;
}

int32_t ScePafWidget_ABF05727(Environment *env, uint32_t port, bool release, Widget *keep)
{
	if (port >= 2)
		return -1;
	for (uint32_t id = 0; id < 8; id++) {
		Widget *holder = env->m_touch_capture[port][id].m_widget;

		if (holder != NULL && holder != keep) {
			inputdevice::InputGestureListener *listener = GestureListenerOf(env, port);

			if (listener != NULL)
				ScePafMisc_CB4DB658(listener, id, holder);
			ScePafWidget_5ABB3393(holder, port, id, release);
		}
	}
	return 0;
}

int32_t ScePafWidget_364E6985(Environment *env, uint32_t port, uint32_t id, bool apply)
{
	if (port >= 2 || id >= 8)
		return -1;

	Widget *holder = env->m_touch_capture[port][id].m_widget;

	if (holder == NULL)
		return -1;

	inputdevice::InputGestureListener *listener = GestureListenerOf(env, port);

	if (listener != NULL)
		ScePafMisc_CB4DB658(listener, id, holder);
	return ScePafWidget_5ABB3393(holder, port, id, apply);
}

TouchCapture *ScePafWidget_451DE355(Environment *env, uint32_t port, uint32_t id, Widget *widget)
{
	if (widget == NULL)
		return NULL;
	if (port >= 2 || id >= 8)
		return NULL;

	TouchCapture *capture = &env->m_touch_capture[port][id];
	Widget *old = capture->m_widget;

	if (old != NULL && old != widget) {
		inputdevice::InputGestureListener *listener = GestureListenerOf(env, port);

		if (listener != NULL)
			ScePafMisc_CB4DB658(listener, id, old);
		ScePafWidget_5ABB3393(old, port, id, false);
	}
	capture->m_port = -1;
	capture->m_id = -1;
	capture->m_widget = NULL;
	capture->m_touch = &env->m_touch_state[port][id];
	capture->m_port = capture->m_touch->m_port;
	capture->m_id = capture->m_touch->m_id;
	capture->m_widget = widget;
	return capture;
}

Widget *ScePafWidget_0775F78F(Environment *env, uint32_t id)
{
	if (env->m_widget_groups.size() == 0)
		return NULL;

	map<uint32_t, ModalWidgetList>::iterator it = env->m_widget_groups.find(id);

	if (it == env->m_widget_groups.end())
		return NULL;
	return it->second.m_widget;
}

int32_t ScePafWidget_D131E927(Environment *env, uint32_t id, Widget *widget)
{
	map<uint32_t, ModalWidgetList>::iterator it = env->m_widget_groups.find(id);

	if (it == env->m_widget_groups.end())
		return 0;

	int32_t result = 0;

	if (widget != NULL)
		result = it->second.Remove(widget);
	else
		it->second.m_entries.clear();
	if (widget != NULL && widget->m_hold_count < 0 && it->second.m_widget == widget)
		it->second.m_widget = NULL;
	return result;
}

int32_t ScePafWidget_66960CBF(Environment *env, uint32_t id, Widget *widget)
{
	if (widget == NULL)
		return -1;

	map<uint32_t, ModalWidgetList>::iterator it = env->m_widget_groups.find(id);

	if (it == env->m_widget_groups.end()) {
		env->m_widget_groups[id] = ModalWidgetList();
		it = env->m_widget_groups.find(id);
	}
	it->second.Sort();
	return it->second.Add(widget, -1) != NULL ? 0 : -1;
}

graph::Renderer *ScePafWidget_1955B85D(Environment *env, const uint32_t *info)
{
	env->m_renderer_param[0] = info[0];
	env->m_renderer_param[1] = info[1];

	graph::Renderer *renderer = ScePafGraphics_4E038C05()->m_ptr;

	if (renderer != NULL)
		renderer->m_effect_renderer_factory = (graph::EffectRenderer *(*)())info[2];
	return renderer;
}

graph::Renderer *ScePafWidget_CE744F88(Environment *env, const uint32_t *factory)
{
	graph::Renderer *renderer = ScePafGraphics_4E038C05()->m_ptr;

	if (renderer != NULL)
		renderer->m_custom_renderer_factory = (graph::CustomRenderer *(*)())factory[0];
	return renderer;
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
