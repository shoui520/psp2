/*
 * paf::ui::Environment, as far as converted C++ uses it: the widget system's
 * state for one framework.
 */
#ifndef PAF_WIDGET_CORE_ENVIRONMENT_H
#define PAF_WIDGET_CORE_ENVIRONMENT_H

#include <stddef.h>
#include <stdint.h>
#include <std/memory>
#include <std/string>
#include <std/list>
#include <std/map>
#include <std/string.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <math/rect.h>
#include <versatileparam/idparam.h>
#include <graphics/surface/surface.h>
#include <widget/core/event.h>
#include <common/shared_ptr.h>
#include <inputdevice/inputlistener.h>
#include <graphics/gutil.h>
#include <std/vector>

namespace paf {
namespace graph {
class EffectRenderer;
}
}

namespace paf {
namespace graph {
class Surface;
class DisplaySurface;
class Renderer;
}

namespace gutil {
class WinHandle;
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
	Screen(intrusive_ptr<graph::Surface> surface);
	/* of the base's surface, viewport and scissor (NULL: none); the exports C0ABDE05 (C1) and
	   8A0CAAE7 (C2) */
	Screen(Screen *base);
	virtual ~Screen();

	static Screen *GetRootScreen()
	{
		return s_root_screen;
	}

	/* the root screen made of a surface of the window's size, with the viewport size given
	   (below 1: the surface's) (0x81148184; name ours), and given up (0x811483c8) */
	static int32_t Init(gutil::WinHandle whandle, int32_t vp_width, int32_t vp_height);
	static int32_t Term();
	/* the root screen given up for another, which is the one the root surface is of */
	static void SetRootScreen(Screen *screen);
	/* the frame on the root surface ended and shown (0x811484c2) */
	static int32_t Swap();
	/* the screen made the target of the draws that follow, and given up (0x81147e52,
	   0x81147ff4): 0, or 1 when it is the target already, -1 for none */
	static int32_t PushTarget(Screen *screen);
	static int32_t PopTarget(Screen *screen);

	/* the surface given in place of the one it had, which is returned (0x81147828) */
	intrusive_ptr<graph::Surface> SetSurface(intrusive_ptr<graph::Surface> surface);

	/* the surface made the target of the draws that follow, 0 when it is not cleared yet;
	   cleared; and the draws finished */
	int32_t OnTarget();
	void OnClear();
	void OnFinish();
	/* whether the surface is the size of the viewport (0x81147d50; name ours) */
	bool IsFullSize();
	/* the rectangle in the surface's coordinates, in its pixels (0x81147baa) and in the
	   pixels it shows at (0x81147c68) (names ours) */
	math::rect ToSurfaceRect(const math::rect &rect);
	math::rect ToPixelRect(const math::rect &rect);

	intrusive_ptr<graph::Surface> GetSurface()
	{
		return m_surface;
	}

	int32_t padding;
	math::v4 clear_color;
	bool is_auto_delete;
	bool is_cleared;
	math::rect vp;
	math::rect sp;
	int32_t clear_policy;
	intrusive_ptr<graph::Surface> m_surface;
	gutil::WinHandle m_whandle;
	/* the YUV conversion the surface is drawn with (ScreenSurface's, names ours) */
	uint32_t m_yuv_csc_index;
	int32_t m_yuv_profile;

	static Screen *s_root_screen;
	/* where widgets whose camera does not roll the screen are drawn from */
	static math::v4 s_draw_offset;
	/* the root screen's surface */
	static graph::DisplaySurface *s_root_surface;
	/* the screens the draws go to, the last the target */
	static vector<Screen *> s_target_list;
};

class Widget;
class Camera;
class SceneResolution;

/*
 * What the resource of a resolution is read with: the name of its strings' and
 * textures' variant and its value (names ours).
 */
class ResolutionResource {
public:
	ResolutionResource() : m_name(s_empty_name), m_value(0)
	{
	}

	string m_name;
	uint32_t m_value;
};

/*
 * The screen resolutions the layouts are made for and the ones to use (Sony's class name, by the
 * NID hash of its constructors and of Scene's; names of the rest ours): a table of resolutions
 * and the ids of the ones in use, each shared with the default or owned.
 */
class Resolution {
public:
	/* the ids in use made from a mask: bits 0 to 2 the sizes (qHD, HD, full HD), bits 24 to 27
	   the orientations (normal, left, right and the pad key sides) */
	Resolution(uint32_t mask);
	Resolution(const Resolution &obj);
	/* the table and the ids copied unless they are the defaults */
	Resolution(vector<SceneResolution> *table, vector<uint32_t> *ids);
	/* the default table, the ids copied unless they are the default */
	Resolution(vector<uint32_t> *ids = NULL);
	virtual ~Resolution();
	/* the layout resolution for a screen of 1920x1088 or more */
	virtual uint32_t GetResolutionIdHD(uint32_t width, uint32_t height, int32_t orientation);
	/* the layout resolution for a screen type below 4 */
	virtual uint32_t GetResolutionId(int32_t type, int32_t orientation);
	virtual Resolution *Clone() const
	{
		return new Resolution(*this);
	}

	/* the largest resolution the display fits */
	void GetScreenSize(uint32_t *width, uint32_t *height);

	/* the table and ids the defaults are made of, once (0x81146b20; name ours) */
	void Init(vector<SceneResolution> *table, vector<uint32_t> *ids);

	vector<SceneResolution> *m_resolutions;
	bool m_own_resolutions;
	vector<uint32_t> *m_ids;
	bool m_own_ids;

	/* the table of resolutions it is made with, and the ids it uses, until others are given
	   (names ours) */
	static vector<SceneResolution> s_default_table;
	static vector<uint32_t> s_default_ids;
};

/* whether the resolution is one in use (no name found) */
extern "C" __attribute__((noinline)) bool ScePafWidget_9F450B64(const Resolution *param, uint32_t id);
/* the resolution if it is in use, else 255 (no name found) */
extern "C" uint32_t ScePafWidget_730C0464(const Resolution *param, uint32_t id);
/* the resolution of the table with the ID, or NULL (no name found) */
extern "C" SceneResolution *ScePafWidget_CDC90BA2(const Resolution *param, uint32_t id);
/* the resolution whose resource variant, or its base's, has the ID's hash, or NULL (no name
   found) */
extern "C" SceneResolution *ScePafWidget_615501F0(const Resolution *param, const IDParam &id);
/* the ID of the resolution whose resource variant, or its base's, has the ID's hash; -1
   without one (no name found) */
extern "C" int32_t ScePafWidget_36505C86(const Resolution *param, const IDParam &id);
/* the resource variant of the resolution with the ID or whose base has it; an empty one without
   (no name found) */
extern "C" ResolutionResource ScePafWidget_E963EAEE(const Resolution *param, uint32_t id);
/* the display's largest frame buffer size (DisplaySurface::GetMaximumResolution; no name found
   for this export) */
extern "C" __attribute__((noinline)) void ScePafWidget_608BC317(int32_t *width, int32_t *height);

/* scenes whose priorities lie in the range are one group, their entry the range's first (names
   ours) */
class EntryRange {
public:
	uint32_t m_first;
	uint32_t m_last;
};

/* a widget in an ordered list of the environment's (32 bytes; names ours) */
class OrderEntry {
public:
	OrderEntry() : m_widget(NULL), m_mark(-1), m_order(-1), m_depth(-1), m_group(-1)
	{
	}

	/* the entry for the widget with the mark: its scene's priorities and its depth in the scene
	   (0x8113bcd4); -1 for no widget (the entry then has none of them) */
	int32_t Init(Widget *widget, int32_t mark);

	/* the scene's priority first, then the widget's depth: the entries are sorted by it */
	bool operator<(const OrderEntry &rhs) const
	{
		return m_order == rhs.m_order ? m_depth < rhs.m_depth : m_order < rhs.m_order;
	}

	Widget *m_widget;
	/* the mark the list's user gave it: UpdateFocus leaves 0x0f0f0f0f on the entry it chose */
	int32_t m_mark;
	/* 255 less the scene's widget priority, over the scene's priority shifted up a byte */
	uint32_t m_order;
	/* 64 bits: all ones less the widget's depth (GetLayerDepth), or all ones */
	uint64_t m_depth;
	/* the scene's entry */
	uint32_t m_group;
};

/* the environment keeps three of these: the modal widgets, the widgets of m_ordered and the
   focus candidates; entries sorted after one is added (names ours) */
class ModalWidgetList {
public:
	/* empty, with room for eight entries (Sony's map insert and Environment constructor make it
	   so) */
	ModalWidgetList() : m_widget(NULL), m_unsorted(false)
	{
		m_entries.reserve(8);
	}

	/* takes the widget out: -1 when it is not in the list (0x8113f74c) */
	int32_t Remove(Widget *widget);
	/* adds the widget with the mark, 0 where it has no order (0x8113f4e4) */
	OrderEntry *Add(Widget *widget, int32_t mark);
	/* sorts the entries where one was added or taken out (0x8113bbba) */
	void Sort();
	/* the entries made again from their widgets and sorted (0x8113bd48; name ours) */
	void Refresh();

	/* the widget the list is for (name ours, meaning open) */
	Widget *m_widget;
	bool m_unsorted;
	vector<OrderEntry> m_entries;
};

/* where a touch hit a widget (0x90 bytes, filled by Environment::GetHitStack; Sony's class
   name from the export, the fields' names ours) */
class HitInfo {
public:
	HitInfo() : m_widget_rot(0.0f, 0.0f, 0.0f, 1.0f)
	{
		sce_paf_memset(this, 0, sizeof(HitInfo));
	}

	/* where the touch is, as SpinBase's drum takes it when the touch goes down (name ours) */
	math::v4 m_pos;
	/* the touch's position, and where it hit (copied by AppIconBase at a touch down) */
	math::v2 m_touch_pos;
	math::v2 m_hit_pos;
	/* the two halves read as one position (the hit in the widget's space; name ours) */
	math::v4 &LocalPos()
	{
		return *reinterpret_cast<math::v4 *>(&m_touch_pos);
	}
	/* the hit taken through the widget's local matrix (CalcHitLineToPlane; name ours) */
	math::v4 m_local_hit;
	uint8_t reserved2[0x10];
	/* the widget's world position, world rotation, size and align offset when the hit was
	   found (names ours) */
	math::v4 m_widget_pos;
	math::v4 m_widget_rot;
	math::v4 m_widget_size;
	math::v4 m_widget_scale;
	/* set once the hit on the touch's line is found (Text's touch position finds it where it
	   is not; name ours) */
	uint32_t m_hit_valid;
	/* the touch panel, 0 front or 1 rear (name ours) */
	uint32_t m_port;
	/* the touch ID (the Environment's m_touch_hits; name ours) */
	uint32_t m_touch_id;
	Widget *m_widget;
};

/* 0x80 bytes, at 0x330 of the Environment (names ours) */
class TouchState {
public:
	__attribute__((always_inline)) TouchState() : m_port(-1), m_id(-1), m_has_line(false)
	{
	}

	/* the panel, 0 front or 1 rear, and the touch ID, set by the widget taking it */
	uint32_t m_port;
	uint32_t m_id;
	/* the touch's last event type (0x42 released) */
	uint32_t m_type;
	/* m_line_start and m_line_end are taken through a camera (ScrollViewBase::GetTrackPos) */
	bool m_has_line;
	/* where the touch is (DirectBar's drag out of its bar) */
	math::v2 m_pos;
	float reserved_b;
	/* the camera's distance the line is taken to */
	float m_depth;
	/* the line through the scene under the touch, from its start to its end (ImageSlideBar's
	   CalcHitLineToPlane) */
	math::v4 m_line_start;
	math::v4 m_line_end;
	/* the world matrix of the camera the line is taken through */
	math::matrix m_camera_matrix;
};

/* a widget's hold on a touch, kept by the Environment for each port and touch ID (names
   ours) */
class TouchCapture {
public:
	__attribute__((always_inline)) TouchCapture() : m_port(-1), m_id(-1), m_widget(NULL), m_touch(NULL)
	{
	}

	uint32_t m_port;
	uint32_t m_id;
	Widget *m_widget;
	/* the touch's state (the Environment's m_touch_state for the port and ID; the drum of
	   SpinBase reads its line end) */
	TouchState *m_touch;

};

/* a widget's drag by one touch (0xe0 bytes; the Environment keeps one for each port and touch ID
   and gives them out, a widget holds up to two; names ours). Sony resets it the same way in its
   constructor and destructor. */
class DragState {
public:
	__attribute__((always_inline)) DragState()
	{
		Reset();
	}

	__attribute__((always_inline)) ~DragState()
	{
		Reset();
	}

	/* made free again: no touch, widget or capture, the hit cleared, the vectors at their start
	   values (inline in Sony's callers) */
	__attribute__((always_inline)) void Reset()
	{
		m_port = -1;
		m_id = -1;
		m_widget = NULL;
		m_capture = NULL;
		sce_paf_memset(&m_hit, 0, sizeof(m_hit));
		m_delta = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
		m_axis = math::v4(0.0f, 0.0f, 1.0f, 0.0f);
		m_origin = math::v4(0.0f, 0.0f, 0.0f, 1.0f);
		m_threshold = math::v4(10.0f, 10.0f, 10.0f, 0.0f);
	}

	uint32_t m_port;
	uint32_t m_id;
	/* the widget dragged */
	Widget *m_widget;
	/* the touch's capture */
	TouchCapture *m_capture;
	HitInfo m_hit;
	math::v4 m_axis;
	math::v4 m_origin;
	math::v4 m_delta;
	/* how far the touch moves before the drag begins (10, 10, 10) */
	math::v4 m_threshold;
};

/* two blocks the Environment makes for the front and rear ports and clears again in its
   destructor; nothing else in libpaf reads or writes them (name ours) */
class PortReserved {
public:
	__attribute__((always_inline)) PortReserved() : reserved0(0), reserved1(0), reserved2(0), reserved4(0.0f, 0.0f, 0.0f, 1.0f)
	{
	}

	__attribute__((always_inline)) ~PortReserved()
	{
		reserved0 = 0;
		reserved2 = 0;
		reserved1 = 0;
	}

	uint32_t reserved0;
	uint32_t reserved1;
	uint32_t reserved2;
	uint8_t reserved3[100];
	math::v4 reserved4;
};

/* the focus: the widget drawn as the focus cursor and the widget it is on (Sony's class name,
   from its exports; the members' names ours) */
class FocusInfo {
public:
	/* no cursor yet, the colour and the time set, the focus shader made (0x811416a8) */
	FocusInfo();
	/* the focus shader and the texture given back (0x81141738) */
	~FocusInfo();
	/* the cursor made under the widget: a focus plane drawn with the focus shader, hidden, kept
	   under the widget when its own parent goes (0x81141750; name ours) */
	void CreateCursor(Widget *parent);

	/* whether the cursor is moving or resizing (0x81142952) */
	bool IsMoving();
	/* the cursor moved after the widget it is on, the time since the last frame given (0x81142982;
	   name ours) */
	void Update(float time);
	/* the cursor shown, where it may be (0x81143f5c) */
	void Show();
	/* the cursor hidden (0x81143fe2) */
	void Hide();
	/* the highlight of the widget the cursor is on and of the one it left, set (0x81141b28;
	   name ours) */
	void UpdateHighlight();
	/* the name of the widget the cursor is on written on the automatic tester's TTY (0x811438a0;
	   name ours) */
	void PrintTarget();
	/* the widget taken out of the focus history */
	void RemoveFocusHistory(Widget *widget);
	/* the widget the focus left for the one going the other way of the direction, NULL for
	   none */
	Widget *FindFocusHistory(Widget *widget, uint32_t dir);
	/* the move from the widget to the next one in the direction kept */
	void AddFocusHistory(Widget *widget, Widget *next, uint32_t dir);

	/* the widget the cursor goes under where it has no target (set by the cursor's creation,
	   0x81141750; name ours) */
	static Widget *s_cursor_parent;
	/* whether the cursor is moved on at once, without the move animation: the move modes 1, 6 and
	   7 (0x81141824; name ours) */
	bool IsMoveImmediate();
	/* the cursor's texture of the move mode, the default one for the modes below 2 (0x81141838;
	   name ours) */
	static void GetMoveTexture(intrusive_ptr<graph::Surface> &texture, int32_t mode);
	/* the focus frame textures, SetTexture's no (0x813152b0; name ours) */
	static intrusive_ptr<graph::Surface> s_textures[9];

	/* the cursor leaving its target for the next one (NULL for none): the widget groups told, the
	   position and shape of the target kept, the cursor sized to it and the target's cursor
	   state cleared; -1 where there is nothing to leave (0x8114439c; name ours) */
	int32_t ClearTarget(Widget *next);
	/* the cursor moved onto the widget: the old target left, the new one told, and the cursor
	   put under it (0x81144516; name ours) */
	int32_t SetTarget(Widget *next);

	/* a move of the focus: the widget it left, the one it went to and the direction (name ours) */
	class History {
	public:
		Widget *m_widget;
		Widget *m_next;
		uint32_t m_dir;
	};

	Widget *m_cursor;
	Widget *m_target;
	/* the widget the cursor was on before (UpdateHighlight; name ours) */
	Widget *m_previous;
	/* set to 0 by the constructor; no reader found */
	uint32_t reserved0;
	/* where the target was (its world position) when the cursor left it (name ours) */
	math::v4 m_target_pos;
	/* cleared with the other port's where a scroll view takes a touch (name ours, meaning open) */
	math::v4 m_touch_move;
	/* the cursor's colour, given to the focus shader (0, 191/255, 1, 1; name ours) */
	math::v4 m_color;
	/* the cursor's rotation where the target has no focus shape (Update; name ours) */
	math::v4 m_rot;
	/* the size of the target's focus shape (GetFocusShapeSize) when the cursor left it, which
	   the cursor is sized to (name ours) */
	math::v4 m_shape;
	/* set to (0, 0, 0, 1) by the constructor; no reader found */
	math::v4 reserved1a;
	/* how the cursor moved last; 1, 6 and 7 hide it at once (Hide; name ours, meaning open) */
	uint32_t m_move_mode;
	/* the move mode of the frame before (Update; name ours) */
	uint32_t m_move_mode_old;
	/* when the cursor left the last target for none, in microseconds (name ours) */
	uint64_t m_leave_time;
	/* the cursor is drawn with its widget only while it moves or the widget turns its focus
	   directions off (Camera's draw walk; name ours) */
	/* set while the cursor is shown; the shown state before it; set where the cursor may be shown
	   (Show, Hide, A6B659BB; names ours) */
	bool m_shown : 1;
	bool m_was_shown : 1;
	bool m_enabled : 1;
	/* m_target_pos and m_shape hold the last target's (name ours) */
	bool m_left_saved : 1;
	bool m_moving_only : 1;
	uint8_t reserved3 : 3;
	uint8_t reserved4[3];
	/* the last moves the focus made (one is kept) and how many */
	History m_history[1];
	uint32_t m_history_count;
	/* the texture the cursor is drawn with, released with it */
	graph::Surface *m_texture;
	/* the cursor's widget gave no texture and the default one is used (Show; name ours) */
	bool m_default_texture;
	uint8_t reserved5[3];
	/* the frames the shown state waited to change, up to five (Update; name ours) */
	int32_t m_show_wait;
};

/* a scene asked to be added to an entry, kept in the environment's pending list (names ours) */
class EntryRequest {
public:
	EntryRequest(int32_t entry, Widget *scene) : m_entry(entry), m_scene(scene), reserved(0)
	{
	}

	int32_t m_entry;
	/* the scene to be added to the entry at the next frame (OnUpdatePre) */
	Widget *m_scene;
	uint32_t reserved;
};

/* what a draw object's texture comes from while it loads: the surface it gives, its result and its
   type (2: a texture); reference counted through its virtual AddRef and Release (name ours, the
   methods after the vtable slots) */
class TextureSource {
public:
	virtual ~TextureSource()
	{
	}

	virtual void GetSurface(intrusive_ptr<graph::Surface> *surface) = 0;
	virtual int32_t GetResult() = 0;
	virtual int32_t GetType() = 0;
	virtual void Reserved5() = 0;
	virtual void AddRef() = 0;
	virtual void Release() = 0;
	virtual void Reserved8() = 0;
	virtual int32_t Reserved9() = 0;
};

/* a draw object's texture the environment waits for: its source, the widget and the object's
   index and bit (name ours) */
class TextureRequest {
public:
	TextureRequest() : m_widget(NULL), m_obj(0), m_bit(0)
	{
	}

	intrusive_ptr<TextureSource> m_source;
	Widget *m_widget;
	uint8_t m_obj;
	uint8_t m_bit;
};

/* what ScePafWidget_1955B85D takes: the functions the environment calls before and after the
   scenes' background layers are drawn, and the effect renderer's factory (names ours) */
class DrawHookParam {
public:
	void (*m_pre_draw)(void);
	void (*m_post_draw)(void);
	graph::EffectRenderer *(*m_effect_renderer_factory)();
};

/* an entry of the environment's callback list: a function that ends it (names ours) */
class EnvCallback {
public:
	void (*m_destroy)(EnvCallback *callback);

	~EnvCallback()
	{
		m_destroy(this);
	}
};

class Environment {
public:
	typedef int32_t (*SoundCallback)(const char *buffer1, uint32_t size1, const char *buffer2,
					 uint32_t size2, int32_t samplerate, float vol,
					 bool ignore_init, void *data);

	/* the focus cursor hidden on the port (-1: all) */
	void HideFocus(int32_t port);
	typedef Widget *(*CreateWidgetCallback)(Widget *parent, const char *type,
						const IDParam &id, const IDParam &style,
						void *data);

	/* the environment of the resolution set and the screen orientation mode: its screen, the
	   input listeners, the root scene "env_top_screen" with its camera, the debug texture and
	   the two focus cursors (export 393F0EF0) */
	Environment(Resolution *env_param, int32_t screen_orientation_mode);
	virtual ~Environment();

	/* the widget kept as a touch device region (up to 6), at the first free slot or at the index
	   (0x8113f3ce, 0x8113f3a2); taken off again (0x8113f40a) */
	/* the widget made one of the modal widgets, ordered with the others: -1 for a mode above 1
	   (0x8113f594; names ours) */
	int32_t RegisterModalWidget(Widget *widget, uint32_t mode);
	/* whether the widget is not behind the modal widgets (nor the second one): 1 where none is set
	   (0x8113fa84; name ours) */
	bool IsInFrontOfModal(Widget *widget);
	/* the ordered lists made again where the layer weights changed, or whatever (0x8113bdba; name
	   ours) */
	int32_t RefreshOrderLists(bool force);
	/* the focus released from the focus candidates in front of the widget's scene entry (0x8113f842;
	   name ours) */
	void ReleaseFocusBeyond(Widget *widget);
	/* the capture let go of: -1 and 0 again, -1 where it held nothing (0x8113fd14; name ours) */
	int32_t ReleaseTouchCapture(TouchCapture *capture);
	/* the ordered widgets sorted, the first one in front of its siblings and fully shown becomes
	   the second modal widget and its scene the display area scene (0x8113bbfe; name ours) */
	void UpdateOrderedTop();
	/* the modal widgets sorted, the touch panel locked while the first one that is shown (mark 0)
	   is on, and the first one with mark 1 taken as the cancel widget (0x8113d722; name ours) */
	void UpdateModalWidgets();
	/* the touch states numbered, the lists and queues emptied (0x81139cc4) */
	void Init();
	/* the touches of the front panel held by widgets behind the widget let go of (0x8113d64e;
	   name ours) */
	int32_t ReleaseTouchesBehind(Widget *widget);
	/* the texture request with the widget's draw object and bit added (0x811407aa); -1 for a
	   held widget (name ours) */
	int32_t AddTextureRequest(Widget *widget, const intrusive_ptr<TextureSource> *source,
				  uint8_t obj, uint8_t bit);
	/* the request taken off (0x8114082c); -1 for none */
	int32_t RemoveTextureRequest(Widget *widget, int32_t obj, int32_t bit);
	/* its source, none without a request (0x81140a44) */
	intrusive_ptr<TextureSource> FindTextureRequest(Widget *widget, int32_t obj, int32_t bit);
	/* the requests of the widget taken off (0x8114093e); always -1 */
	int32_t RemoveTextureRequests(Widget *widget);
	/* the resize of the root surface asked for made, and the display area said to have changed
	   where it worked (0x8113baf6; name ours) */
	void ApplyResize();
	/* each request whose source is a texture handed to its widget; those that are set or given up
	   let go, those taken off and added meanwhile made up (0x8113c886; name ours) */
	void UpdateTextureRequests();
	/* the request of the same widget, object and bit taken off (0x8113c826) */
	int32_t RemoveTextureRequestOf(const TextureRequest &request);
	int32_t RegisterTouchDeviceRegion(Widget *widget);
	int32_t RegisterTouchDeviceRegion(uint32_t index, Widget *widget);
	int32_t RemoveTouchDeviceRegion(Widget *widget);
	/* the widget forgotten as the hit of any touch (0x8113fecc) */
	void ClearHitInfo(Widget *widget);
	/* the number of widgets in the scene of the entry, or the iterators at the start and the
	   end of its children (0x8113ef94, 0x8113efda, 0x8113f032); the entry shown or hidden
	   (0x8113ec20, 0x8113eca4) */
	int32_t GetSceneNum(int32_t entry);
	int32_t GetSceneTop(int32_t entry, list<Widget *>::iterator &top);
	int32_t GetSceneEnd(int32_t entry, list<Widget *>::iterator &end);
	int32_t ShowSceneEntry(int32_t entry);
	int32_t HideSceneEntry(int32_t entry);
	/* whether nothing the widgets still run keeps the framework from ending */
	bool IsTermAvailable();
	/* lets the widgets end: m_term_requested once nothing keeps them */
	int32_t RequestTerm();
	/* lets the widgets end and runs frames until nothing keeps them (0x8113db36; name ours) */
	int32_t Term();
	/* a frame drawn: begun on the root surface, the scenes drawn, ended (0x8113c31c) */
	void OnDraw();
	/* the widget that holds the port's pad keys of the low group (0x1f) or the high group
	   (0x3e0) lets go of them: -1 where none holds them (0x8113f4be, 0x8113f470; names
	   ours) */
	/* the focus chosen again among the focus candidates in front of the modal limit, the cursor
	   put on it: the widget given first is taken where it is in the chosen group, the second
	   loses its mark (0x8113ce6e; names ours) */
	void UpdateFocus(Widget *prefer, Widget *drop);
	int32_t ReleasePadCaptureLow(uint32_t port);
	int32_t ReleasePadCaptureHigh(uint32_t port);
	/* the widget given the port's pad keys of the low group (0x8113f496) or the high group
	   (0x8113f448), the widget holding them let go first: -1 for a port of 5 or more */
	int32_t SetPadCaptureLow(uint32_t port, Widget *widget);
	int32_t SetPadCaptureHigh(uint32_t port, Widget *widget);
	/* a frame's steps, in the order the Framework runs them */
	void OnUpdatePre();
	void EventLoop();
	void OnUpdatePost();
	/* the scenes' restructures done (0x8113be56) */
	void UpdateRestructScene();
	/* the touch device regions of the widgets set on the panel (0x8113c3c2) */
	void OnUpdateTouchDeviceRegion();
	/* the screen the draws go to; the old one is deleted if it was auto-deleted (0x81139ca0) */
	int32_t SetScreen(Screen *screen);
	void ShowFocus(int32_t port);
	/* where the touch that hit the widget hit it: 0, or -1 when none did */
	int32_t GetHitStack(HitInfo *info, Widget *widget);
	/* the last hit stack of the touches, 0, or -1 when there is none (0x8113ff98) */
	int32_t GetLastHitStack(HitInfo *info);
	/* whether the widget is one of the hit stack's parents, or has a hit stack (0x8113dea6) */
	bool IsHitStackParent(Widget *widget);
	/* the display area scale from the system settings, once */
	void ApplyDisplayAreaSettings();
	/* the display area scale read again, once it has been applied (name ours) */
	void UpdateDisplayAreaSettings();
	/* the display area settings read while the surface uses them, the screen size of the scenes
	   made again where the area changed (0x8113c616; name ours) */
	void UpdateDisplayArea();
	/* nothing is drawn now (0x8113dcca; name ours) */
	bool IsDrawSkipped();
	/* the widget is not below the modal widgets in depth (0x8113fa84; name ours) */
	bool IsOverModal(Widget *widget);
	/* its camera's roll, 0 without one (0x8113dce2; name ours) */
	float GetRollScreen();
	/* the scene added to the scenes of the entry, before the widget (NULL: at the end), and
	   taken out of them (0x8113ed32, 0x8113eea2; names ours) */
	int32_t AddScene(Widget *scene, int32_t entry, Widget *before);
	int32_t RemoveScene(Widget *scene);
	/* the camera roll the screen orientation turns the scenes by (0x8113ca44; name ours) */
	float GetOrientationRoll(int32_t orientation);
	/* the layout resolution of the screen for the orientation mode (made from the system's
	   recommended orientation where the mode is 4), another orientation if the screen has
	   none in it, and the shell told the orientation (the environment's resolutions made when it
	   has none; 0x81139a38, name ours); 255 for none */
	int32_t SelectResolution();
	/* the orientation chosen from the system's recommended one, the first a scene of the entries
	   can be shown in (0x8113dbb2; name ours) */
	void UpdateOrientation();

	/* the gesture listener of the touch port, NULL for another port (inline in Sony's callers,
	   name ours) */
	common::SharedPtr<inputdevice::InputListener> GetTouchGestureListener(uint32_t port)
	{
		if (port == 0)
			return m_touch_gesture_listener_front;
		if (port == 1)
			return m_touch_gesture_listener_rear;
		return common::SharedPtr<inputdevice::InputGestureListener>();
	}

	/* set: the widgets the focus leaves are kept in its history (name ours) */
	bool m_focus_history_on;
	uint8_t reserved_5a[3];
	/* the groups of scene priorities, a vector of four to begin with (Init; name ours) */
	vector<EntryRange> m_entry_ranges;
	/* what the environment runs at its end, each ended with a call of its first word (name ours,
	   use not found) */
	list<EnvCallback> m_callbacks;
	/* (meaning open; name ours) */
	bool m_callbacks_busy;
	uint8_t reserved_37[3];
	/* the scenes by their entry (ShowSceneEntry, GetSceneNum; name ours) */
	map<int32_t, Widget *> m_scene_entries;
	/* the scenes asked to be added, with their entries (IsTermAvailable counts them; name ours) */
	list<EntryRequest> m_pending;
	/* set when scenes were taken out of the entries; UpdateRestructScene then orders the scenes' layers
	   again (RemoveScene; name ours) */
	bool m_scene_restruct;
	/* set when a scene's layer weights are made again (Scene::UpdateLayerWeight; name ours) */
	bool m_layer_weight_changed;
	uint8_t reserved_42[2];
	/* the display area scale the root surface shows at (names ours) */
	float m_display_area_scale;
	bool m_display_area_applied;
	bool m_display_area_changed;
	/* a resize of the root surface asked for (ScePafWidget_E8F38C84; names ours) */
	bool m_resize_pending;
	uint8_t reserved8a[1];
	uint16_t m_resize_width;
	uint16_t m_resize_height;
	/* the textures waited for; those added and those taken off while the first is walked
	   (UpdateTextureRequests; names ours) */
	list<TextureRequest> m_texture_requests;
	list<TextureRequest> m_texture_requests_add;
	list<TextureRequest> m_texture_requests_remove;
	bool m_texture_requests_busy;
	uint8_t reserved8c[3];
	/* the gesture listener of the pad (name ours) */
	common::SharedPtr<inputdevice::InputListener> m_pad_gesture_listener;
	/* the gesture listeners of the front (0) and rear (1) touch ports (names ours) */
	common::SharedPtr<inputdevice::InputListener> m_touch_gesture_listener_front;
	common::SharedPtr<inputdevice::InputListener> m_touch_gesture_listener_rear;
	/* the gesture listeners of the motion sensor, the Bluetooth remote, the HID remote and the
	   keyboard (names ours) */
	common::SharedPtr<inputdevice::InputListener> m_motion_gesture_listener;
	common::SharedPtr<inputdevice::InputListener> m_btrm_gesture_listener;
	common::SharedPtr<inputdevice::InputListener> m_hidrm_gesture_listener;
	common::SharedPtr<inputdevice::InputListener> m_keyboard_listener;
	/* set to 0 by the constructor; no reader found (name ours) */
	uint32_t m_reserved_word_e;
	/* the Framework's InitParam::screen_orientation_mode and env_param */
	int32_t m_screen_orientation_mode;
	/* the orientation UpdateOrientation chose from the system's recommended one (name ours) */
	int32_t m_orientation;
	Resolution *m_env_param;
	/* cleared by the constructor (meaning open; name ours) */
	bool m_reserved_flag7;
	uint8_t reserved7[3];
	/* the widget whose hit rect is the screen a rear touch scrolls within (TouchScrollSupplier's
	   rear mochi gesture; name ours) */
	Widget *m_root_widget;
	/* the root scene's camera (name ours) */
	Camera *m_root_camera;
	/* the scenes' cameras turn with the screen orientation in modes 0 and 1
	   (Scene::ResetCameraRoll; name ours) */
	uint8_t m_camera_roll_mode;
	/* cleared once UpdateOrientation chose an orientation (name ours, meaning open) */
	bool m_orientation_dirty;
	uint8_t reserved7g[2];
	/* the state the root surface was in after the frame began (DisplaySurface's m_frame_state;
	   name ours), and whether the frame is drawn: it was 1 (name ours) */
	int32_t m_frame_state;
	bool m_draw_frame;
	/* set by RequestTerm */
	bool m_term_requested;
	/* set by the power callback while the system suspends: the frame is not drawn unless
	   m_draw_when_suspended (name ours) */
	bool m_suspended;
	bool m_draw_when_suspended;
	/* the Framework's InitParam::env_option */
	bool m_env_option;
	uint8_t reserved2[3];
	SoundCallback m_sound_callback;
	void *m_sound_callback_data;
	CreateWidgetCallback m_create_widget_callback;
	void *m_create_widget_callback_data;
	/* a widget that takes the cancel key like the modal one on top does (HandleKeycodeEvent; name
	   ours, meaning open) */
	Widget *m_cancel_widget;
	/* the modal widget on top: a button below it in depth is not pressed while it is modal with
	   option 1 (name ours) */
	Widget *m_top_modal_widget;
	/* RegisterModalWidget's widgets (name ours) */
	ModalWidgetList m_modal_widgets;
	/* the scene the display area scale is applied to (Camera's DrawScene; name ours) */
	Widget *m_display_area_scene;
	/* a second modal widget: UpdateFocus takes the one of the two in front as the limit of the focus
	   (name ours, meaning open) */
	Widget *m_second_modal_widget;
	/* the widgets that have m_ordered (ScePafWidget 0253DC9C; name ours) */
	ModalWidgetList m_ordered_widgets;
	/* lists of widgets by ID, each with a widget it is for first (ScePafWidget 0775F78F and
	   D131E927; names ours, meaning open) */
	map<uint32_t, ModalWidgetList> m_widget_groups;
	/* the widget that has the focus (UpdateFocus; name ours) */
	Widget *m_focus_widget;
	/* the widgets that can take the focus, ordered for UpdateFocus (name ours) */
	ModalWidgetList m_focus_candidates;
	/* set before the focus is first shown */
	int32_t m_focus_mode;
	/* set: a scroll view does not take the focus by itself (name ours, meaning open) */
	bool m_focus_auto_off;
	bool reserved4_345;
	/* the buttons move the focus (ScePafWidget_75FB31F1; name ours) */
	bool m_focus_enabled;
	uint8_t reserved4[5];
	/* the focus cursor and the widget it is on, of the front and the rear panel */
	FocusInfo m_focus_cursor;
	FocusInfo m_focus_cursor_rear;
	/* the texture the scenes' debug cubes are drawn with (Scene::SetDebugMode; name ours) */
	intrusive_ptr<graph::Surface> m_debug_texture;
	/* the widget holding each port's pad keys of capture 0x17d (the keys other than the
	   directions and enter) and of 0x17c (the low group); five ports (names ours) */
	Widget *m_pad_capture_button[5];
	Widget *m_pad_capture_dir[5];
	uint8_t reserved4j[4];
	/* the events passed on at the next frame (names after ScePaf-RE) */
	EventQueue m_ev_queue;
	list<Event> m_ev_subrouting;
	list<Event> m_ev_doevent;
	/* the scenes, updated each frame (name after ScePaf-RE) */
	list<Widget *> m_scenes_onupdate;
	/* the screen SetScreen gives */
	Screen *m_screen;
	/* a state per port (the front panel's and the rear's) and touch ID: the widget taking a
	   touch sets its port and ID (names ours) */
	TouchState m_touch_state[2][8];
	/* where each touch ID last hit (name ours) */
	HitInfo m_touch_hit[2][8];
	/* the last hit of each touch of the front and rear panels (ClearHitInfo; name ours) */
	HitInfo m_touch_hit_last[2][8];
	/* the widget capturing each touch of the front and rear panels (name ours) */
	TouchCapture m_touch_capture[2][8];
	/* the drags given out, by the touch's port and ID (names ours) */
	DragState m_drag_state[2][8];
	PortReserved m_port_reserved[2];
	/* the widgets the touches are on (TouchScrollSupplier's checks; name ours) */
	list<HitInfo> m_touch_hits;
	/* the widgets IsHitStackParent takes as the hit stack's parents, cleared with
	   m_touch_hits (name after IsHitStackParent) */
	list<Widget *> m_hit_stack;
	/* the widgets kept as touch device regions and which of the six slots are used (name ours) */
	uint32_t m_touch_region_mask;
	/* the slots used when OnUpdateTouchDeviceRegion last set the regions (name ours) */
	uint32_t m_touch_region_mask_pre;
	Widget *m_touch_region[6];
	/* two flags set by the constructor, the second on (meaning open; names ours) */
	bool m_reserved_flag_a;
	bool m_reserved_flag_b;
	/* each frame waits for the GPU once it is drawn (Framework::Option_4) */
	bool m_finish_after_draw;
	/* set by the constructor (meaning open; names ours) */
	bool m_reserved_flag_c[4];
	uint8_t reserved6e[1];
	uint32_t m_reserved_word_a;
	/* three surfaces released by the destructor; no other reader or writer found (names ours) */
	intrusive_ptr<graph::Surface> m_reserved_surface_a;
	intrusive_ptr<graph::Surface> m_reserved_surface_b;
	intrusive_ptr<graph::Surface> m_reserved_surface_c;
	uint8_t reserved6j[16];
	/* the motion noise filter is available (vshMotionNoiseFilterIsAvailable; name ours) */
	bool m_motion_noise_filter;
	uint8_t reserved6k[3];
	/* called before and after the scenes' background layers are drawn (ScePafWidget_1955B85D
	   sets them; names ours) */
	void (*m_pre_draw)(void);
	void (*m_post_draw)(void);
	uint8_t reserved6b[4];
};

}
}

/* Exports with no name found for their NIDs. */
extern "C" {
/* the screen size of each resolution, qHD, HD and full HD (Resolution's rodata, defined with
   the stand-ins in state.c until its file is C; names ours) */
extern const uint32_t g_screen_width_qhd;
extern const uint32_t g_screen_height_qhd;
extern const uint32_t g_screen_width_hd;
extern const uint32_t g_screen_height_hd;
extern const uint32_t g_screen_width_full_hd;
extern const uint32_t g_screen_height_full_hd;

/* whether the buttons move the focus */
void ScePafWidget_75FB31F1(paf::ui::Environment *env, bool enable);
void ScePafWidget_B4A4660B(paf::ui::Environment *env, int32_t enable, int32_t port, int32_t mode);
/* whether the port's focus may be shown */
bool ScePafWidget_A6B659BB(paf::ui::Environment *env, int32_t port);
/* bit 0 of the port's focus state; for a port below 0, whether any port has it */
bool ScePafWidget_529FB877(paf::ui::Environment *env, int32_t port);
/* the touches of the port (0 front, 1 rear) let go of by the widgets that capture them, all
   but the widget's, their releases handled where release is set: -1 for another port (no
   name found) */
int32_t ScePafWidget_ABF05727(paf::ui::Environment *env, uint32_t port, bool release,
			      paf::ui::Widget *keep);
/* updates the input devices */
int32_t ScePafWidget_909850C4(void);
/* the touch of the port and ID let go of by the widget that holds it, its release handled where apply
   (no name found) */
int32_t ScePafWidget_364E6985(paf::ui::Environment *env, uint32_t port, uint32_t id, bool apply);
/* the touch of the port and ID given to the widget: its capture, 0 where it has none (no name found) */
paf::ui::TouchCapture *ScePafWidget_451DE355(paf::ui::Environment *env, uint32_t port, uint32_t id,
					     paf::ui::Widget *widget);
/* a drag state made free for the capture's touch, NULL where it is in use (no name found) */
paf::ui::DragState *ScePafWidget_DE6905E2(paf::ui::Environment *env, paf::ui::TouchCapture *capture);
/* the widget the list with the ID is for, 0 for none (no name found) */
paf::ui::Widget *ScePafWidget_0775F78F(paf::ui::Environment *env, uint32_t id);
/* a widget taken out of the list with the ID, or the list emptied with none (no name found) */
int32_t ScePafWidget_D131E927(paf::ui::Environment *env, uint32_t id, paf::ui::Widget *widget);
/* the widget put in the group (made where missing), kept sorted; -1 without a widget or where it
   does not fit (no name found) */
int32_t ScePafWidget_66960CBF(paf::ui::Environment *env, uint32_t id, paf::ui::Widget *widget);
/* the scene asked to be added to the entry at the next frame (kept in the pending list) and, where
   the entry has none, its root widget made and named "root_widget"; an error for an entry of
   0x10000 or more (no name found) */
int32_t ScePafWidget_E6D215ED(paf::ui::Environment *env, paf::ui::Widget *scene, int32_t entry);
/* the root surface asked to be resized to the size at the next frame; an error without a root
   surface (0x80af0001), for a frame buffer mode of 2 or 4 (0x80af0004) and for a size above its
   largest (0x80af008f) (no name found) */
int32_t ScePafWidget_E8F38C84(paf::ui::Environment *env, uint16_t width, uint16_t height);
/* a group of scene priorities added, 0 and the scenes' entries made again; an error where the
   range is no range or crosses another (no name found) */
int32_t ScePafWidget_094DF388(paf::ui::Environment *env, const paf::ui::EntryRange *range);
/* the range holding the priority, 0 and the range, or an error without one (no name found) */
int32_t ScePafWidget_F5842A6B(paf::ui::Environment *env, uint32_t priority,
			      const paf::ui::EntryRange **range);
/* sets the focus frame texture no (below 9); with none, reads all of them from the common resource */
void ScePafWidget_B6C64486(const paf::intrusive_ptr<paf::graph::Surface> &texture, int32_t no);
/* the draw hooks kept and the effect renderer's factory set (no name found); the renderer */
paf::graph::Renderer *ScePafWidget_1955B85D(paf::ui::Environment *env,
					    const paf::ui::DrawHookParam *param);
/* the custom renderer's factory set to the word; the renderer (no name found) */
paf::graph::Renderer *ScePafWidget_CE744F88(paf::ui::Environment *env, const uint32_t *factory);
/* begins a frame on the root surface */
void ScePafWidget_9D4A5A0E(paf::ui::Environment *env);
void ScePafWidget_DC1B19DB(paf::ui::Environment *env);
/* ends the frame on the root surface */
void ScePafWidget_851911CB(paf::ui::Environment *env);
/* the screen's targets let go of, the scene data of the root screen's surface set, and the frame's
   draw call count begun again (no name found) */
void ScePafWidget_BA5195CA(paf::ui::Environment *env);
/* shows the frame drawn; below 0 on failure */
int32_t ScePafWidget_F67517A0(paf::ui::Environment *env);
void ScePafWidget_8E61FE0C(paf::ui::Environment *env);
/* always 0 */
int32_t ScePafWidget_998F8F48(void);
/* begins a frame on the root screen, and ends it (0x81148456, 0x81148418; the Environment's
   versions are F67517A0 and 8E61FE0C) */
int32_t ScePafWidget_0313D591(void);
int32_t ScePafWidget_323CEF70(void);
/* loads the effects whose bits are set from the folder */
int32_t ScePafWidget_BA6EB0C6(uint32_t effects, const paf::string &dir);
}

#endif
