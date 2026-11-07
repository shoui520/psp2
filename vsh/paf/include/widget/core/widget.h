/*
 * paf::ui::Widget, as far as converted C++ uses it: a node of the widget tree,
 * with its position, size and rotation, the animations that move them and the
 * objects it draws.
 *
 * Names after ScePaf-RE (restored from Sony's DWARF), the virtual functions in
 * its order, which is the order of Widget's vtable; the virtual functions Sony
 * defined in the class (the copies the linker kept) are defined here. Where no
 * name is known the name says what the function does (namecheck lists them).
 */
#ifndef PAF_WIDGET_CORE_WIDGET_H
#define PAF_WIDGET_CORE_WIDGET_H

#include <stdint.h>
#include <arm_neon.h>
#include <std/list>
#include <std/memory>
#include <std/string>
#include <std/vector>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/rect.h>
#include <math/ngp/v4.h>
#include <math/ngp/matrix.h>
#include <math/ngp/quaternion.h>
#include <versatileparam/idparam.h>
#include <versatileparam/versatileparam.h>
#include <widget/core/handler.h>
#include <widget/core/layouter.h>
#include <common/transition.h>
#include <common/shared_ptr.h>
#include <timer/timer.h>

namespace paf {

class Timer;

namespace graph {
class DrawInfo;
class DrawObj;
class DrawObjParam;
class Surface;
}

namespace ui {

class Environment;
class Widget;
class Scene;
class TouchCapture;
class Camera;
class AnimListener;
class AnimQueue;
typedef void (*AnimCB)(Timer *t, AnimListener *listener, void *data1, void *data2,
		       void *data3);

/* the bit of a key code in a widget's m_keycode, 0 for a key that has none (0x811411f6, name
   ours) */
uint32_t KeycodeToMask(int32_t keycode);

/* a position, size or rotation for one screen resolution */
class ResolutionData {
public:
	uint32_t m_id;
	float m_value[3];
};

/*
 * A widget's parameters as its resource gives them: its style, texture and
 * sound, and the textures and strings of its draw objects, which the widget
 * types' parameters add to the lists (members' names ours).
 */
class WidgetParam : public VersatileParam {
public:
	/* a texture or string ID of the widget's draw object index (names ours) */
	class IndexedRef {
	public:
		IndexedRef(const char *name, int32_t index) : name(name), index(index)
		{
		}

		const char *name;
		int32_t index;
		IDParam id;
	};

	static const char m_param_name[];
	static const char *s_names[];
	static const char *ParamName();

	/* the IDs cleared (the widget types' parameters inline it) */
	WidgetParam() : m_widget(NULL)
	{
		Init();
	}
	virtual const char *GetParamName() const;
	/* below Widget: the widget forgets this parameter's delete callback */
	virtual ~WidgetParam();
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);
	virtual int32_t Write(common::SharedPtr<File> fp, uint32_t type, uint32_t indent);

	int32_t Init();
	const wchar_t *GetString(const IDParam &id);

	Widget *m_widget;
	IDParam m_style_id;
	IDParam m_texture_id;
	IDParam m_sound_id;
	list<IndexedRef> m_texture_list;
	list<IndexedRef> m_string_list;
	/* strings by their alias (ScePafToplevel_D8801CA4) */
	list<IndexedRef> m_alias_string_list;
};

/*
 * A widget type's style, read from the resource's "style_<type>" parameter:
 * the widgets that have it, the parameters of their draw objects, their color
 * and their sound (members' names ours).
 */
class WidgetStyleParam : public VersatileParam {
public:
	static const char m_param_name[];
	static const char *const s_names[];
	static const char *ParamName();

	WidgetStyleParam();
	virtual const char *GetParamName() const;

	/* each widget forgets this style's delete callback (below Widget) */
	virtual ~WidgetStyleParam();

	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);
	virtual int32_t Destroy(int32_t mode);
	virtual int32_t OnUpdate();
	virtual void OnDraw();
	virtual int32_t AttachPlugin(Plugin *plugin);
	virtual int32_t Read(const cxml::Element *elem);
	virtual int32_t Write(common::SharedPtr<File> fp, uint32_t type, uint32_t indent);
	virtual int32_t SetEach(Widget *widget);
	virtual int32_t GetEach(Widget *widget);
	virtual graph::DrawObjParam *GetDrawObjParam(int32_t index);
	/* the parameter, initialised, replaces the one of the draw object index (name ours) */
	virtual void SetDrawObjParam(const char *name, graph::DrawObjParam *param, int32_t index);

	int32_t Init();
	/* the parameter of the draw object index, or the end (name ours) */
	vector<graph::DrawObjParam *>::iterator FindDrawObjParam(int32_t index);
	int32_t AddWidget(Widget *widget);
	int32_t DelWidget(Widget *widget);
	static void delete_cb(Widget *widget, void *param);

	list<Widget *> m_widget_list;
	vector<graph::DrawObjParam *> m_drawobj_param_list;
	math::v4 m_color;
	/* the ID of the style's sound parameter */
	IDParam m_sound_id;
};

/*
 * A widget type's handlers, read from its element (the factory table's handler
 * factory): the names of its layout handler and of its event handlers, which
 * the plugin's handler tables turn into functions (members' names ours).
 */
class WidgetHandlerParam : public VersatileParam {
public:
	/* an event a handler can be named for (names after ScePaf-RE) */
	class EventList {
	public:
		const char *name;
		int32_t type;
	};

	/* the event and the name of its handler */
	class EventEntry {
	public:
		const char *m_name;
		int32_t m_type;
		const char *m_handler;
	};

	static const char m_param_name[];
	static const EventList c_eventList[];
	static const char *ParamName();

	WidgetHandlerParam();
	virtual const char *GetParamName() const;
	/* the widget forgets this parameter's delete callback (below Widget) */
	virtual ~WidgetHandlerParam();
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);

	int32_t Init();
	HandlerCB GetHandler(const char *name);
	int32_t SetHandler(int32_t type, const char *name);
	int32_t AttachWidget(Widget *widget);
	static void delete_cb(Widget *widget, void *param);

	Widget *m_widget;
	const char *m_layouter;
	list<EventEntry> m_event_list;
};

/*
 * A widget type's layout, read from the "layout_" children of its element: its
 * position, size and rotation, for each resolution the scene lays out for when
 * the element has "resolution" children, and its alpha, align, adjust, origin
 * and anchor (members' names after the attributes).
 */
class WidgetLayoutParam : public VersatileParam {
public:
	static const char m_param_name[];
	static const char *s_names[];
	static const char *ParamName();

	WidgetLayoutParam();

	virtual const char *GetParamName() const;
	virtual ~WidgetLayoutParam();
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);
	virtual int32_t Read(const cxml::Element *elem);
	virtual int32_t Write(common::SharedPtr<File> fp, uint32_t type, uint32_t indent);

	int32_t Init();
	/* makes room for num resolutions */
	int32_t InitResolution(int32_t num);
	/* takes the widget's position, size and rotation arrays (0x81064afc, name ours) */
	int32_t GetEach(Widget *widget);
	/* reads it for the widget */
	int32_t Attach(Widget *widget);
	/* the timer its Set animates with; NULL sets at once */
	int32_t SetTimer(Timer *timer);
	static void delete_cb(Widget *widget, void *param);

	/* the widget it is read for (name ours) */
	Widget *m_widget;
	Timer *m_timer;
	math::v3 m_pos;
	math::v3 m_size;
	math::v3 m_rot;
	float m_alpha;
	int32_t m_align[3];
	int32_t m_adjust[3];
	int32_t m_origin[3];
	int32_t m_anchor[3];
	/* the values of each resolution, m_resolution_num entries each (names ours) */
	ResolutionData *m_pos_array;
	ResolutionData *m_size_array;
	ResolutionData *m_rot_array;
	int32_t m_resolution_num;
	/* the widget's resolution and its base resolution (names ours) */
	uint32_t m_resolution_id;
	int32_t m_base_resolution_id;
};

/*
 * A widget type's sounds, read from the resource's "sound_<type>" parameter: the
 * widgets that have them and, for each sound, its ID and what the plugin's
 * sound table gives for it (members' names ours).
 */
class WidgetSoundParam : public VersatileParam {
public:
	/* a sound of the parameter: its element, its index, its ID and its waveforms (names ours) */
	class Sound {
	public:
		/* the element and index named, no ID or waveforms yet (ButtonBaseSoundParam's) */
		Sound(const char *name, uint32_t index)
			: m_name(name), m_index(index), m_handle(-1), reserved2(-1.0f), m_buffer1(NULL),
			  m_size1(0), m_buffer2(NULL), m_size2(0), m_samplerate(0), m_type(0)
		{
		}

		const char *m_name;
		/* the index a widget plays it by (PlaySound, GetSound) */
		uint32_t m_index;
		IDParam m_id;
		/* -1 until played */
		int32_t m_handle;
		/* made -1.0; its readers are not converted */
		float reserved2;
		const char *m_buffer1;
		uint32_t m_size1;
		const char *m_buffer2;
		uint32_t m_size2;
		int32_t m_samplerate;
		uint32_t m_type;
	};

	static const char m_param_name[];
	static const char *ParamName();

	WidgetSoundParam();
	virtual const char *GetParamName() const;
	/* each widget forgets this parameter's delete callback (below Widget) */
	virtual ~WidgetSoundParam();
	virtual int32_t Set();
	virtual int32_t Create(int32_t mode);
	virtual int32_t Read(const cxml::Element *elem);

	int32_t Init();
	int32_t AddWidget(Widget *widget);
	int32_t DelWidget(Widget *widget);
	static void delete_cb(Widget *widget, void *param);

	/* the widgets it gave its sounds */
	list<Widget *> m_widget_list;
	vector<Sound> m_sound_list;
};

/*
 * The focus cursor of a widget: its shape, the offset, size and rotation it
 * takes, for each resolution in the arrays, and its texture (names ours).
 */
class FocusShape {
public:
	FocusShape()
		: m_shape(0), m_offset(0.0f, 0.0f, 0.0f), m_size(0.0f, 0.0f), m_rot(0.0f, 0.0f, 0.0f),
		  reserved_a(false), reserved_b(true)
	{
	}

	vector<ResolutionData> m_offset_array;
	vector<ResolutionData> m_size_array;
	vector<ResolutionData> m_rot_array;
	int32_t m_shape;
	math::v3 m_offset;
	math::v2 m_size;
	math::v3 m_rot;
	intrusive_ptr<graph::Surface> m_texture;
	/* made false and true; their readers are not converted */
	bool reserved_a;
	bool reserved_b;
};

/* where the focus moves from a widget, and the rectangle it searches (names ours) */
class FocusHint {
public:
	/* up, down, left and right */
	IDParam m_direction_id[4];
	math::rect m_search_rect;
	bool reserved;
};

/* a widget's focus state; only the hint is converted (names ours) */
class Focuser {
public:
	uint8_t reserved[0xc];
	FocusHint *m_hint;
};

/* the focus of the widgets under one that holds it (names ours) */
class FocusRoot {
public:
	/* what SetFocusRootState sets */
	uint32_t m_state;
	/* the widget that takes the focus first */
	Widget *m_init_focus;
};

/*
 * A widget's key focus, read from the "focus_" children of its element: whether
 * it takes focus and first, its direct keys, the widgets up, down, left and
 * right, its focus cursor and the rectangle it searches (members' names after
 * the attributes).
 */
class WidgetFocusParam : public VersatileParam {
public:
	static const char *s_names[];

	WidgetFocusParam();

	static const char *ParamName();
	virtual const char *GetParamName() const;
	virtual ~WidgetFocusParam();
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);
	virtual int32_t Read(const cxml::Element *elem);

	int32_t Init();
	/* makes room for num resolutions (0x8106693e, name after WidgetLayoutParam's) */
	int32_t InitResolution(int32_t num);
	/* takes the widget's focus arrays (0x810663bc, name after WidgetLayoutParam's) */
	int32_t GetEach(Widget *widget);
	/* reads it for the widget */
	int32_t Attach(Widget *widget);
	static void delete_cb(Widget *widget, void *param);

	/* the widget it is read for (name ours) */
	Widget *m_widget;
	bool m_enable;
	bool m_init;
	uint32_t m_direct_key;
	IDParam m_up_id;
	IDParam m_down_id;
	IDParam m_left_id;
	IDParam m_right_id;
	int32_t m_focus_shape;
	math::v3 m_focus_offset;
	math::v2 m_focus_size;
	math::v3 m_focus_rot;
	IDParam m_focus_texture;
	math::v4 m_search_rect;
	/* the values of each resolution, m_resolution_num entries each (names ours) */
	ResolutionData *m_offset_array;
	ResolutionData *m_size_array;
	ResolutionData *m_rot_array;
	int32_t m_resolution_num;
	/* the widget's resolution and its base resolution (names ours) */
	uint32_t m_resolution_id;
	int32_t m_base_resolution_id;
};

/*
 * How a widget that adjusts to its contents is sized: the smallest and largest size it is given
 * and the margins around its contents, from the style's "adjust_min_size", "adjust_max_size" and
 * "*_margin" (Button's; the class name is ours). 0x38 bytes, shared by reference count.
 */
class AdjustParam {
public:
	AdjustParam()
		: m_ref_count(0), m_min_size(0.0f, 0.0f, 0.0f, 0.0f),
		  m_max_size(3.4028235e+18f, 3.4028235e+18f, 3.4028235e+18f, 3.4028235e+18f),
		  m_top_margin(0.0f), m_bottom_margin(0.0f), m_left_margin(0.0f),
		  m_right_margin(0.0f)
	{
	}

	int32_t m_ref_count;
	math::v4 m_min_size;
	math::v4 m_max_size;
	float m_top_margin;
	float m_bottom_margin;
	float m_left_margin;
	float m_right_margin;
};

class Widget : public Handler {
public:
	typedef void (*DeleteCB)(Widget *widget, void *data);

	/* a delete callback and its data (names after DrawObj's, Sony's DWARF) */
	class DeleteCBInfo {
	public:
		DeleteCBInfo(DeleteCB func, void *data) : func(func), data(data)
		{
		}

		DeleteCB func;
		void *data;
	};

	/* what a widget keeps for its owner (m_user_data2), deleted with it */
	class UserData {
	public:
		UserData()
		{
		}

		virtual ~UserData()
		{
		}
	};

	enum {
		ALIGN_CENTER = 0,
		ALIGN_MINUS = 1,
		ALIGN_PLUS = 2,
		ALIGN_LEFT = 1,
		ALIGN_RIGHT = 2,
		ALIGN_BOTTOM = 1,
		ALIGN_TOP = 2
	};

	enum {
		ANCHOR_CENTER = 0,
		ANCHOR_MINUS = 1,
		ANCHOR_PLUS = 2,
		ANCHOR_LEFT = 1,
		ANCHOR_RIGHT = 2,
		ANCHOR_BOTTOM = 1,
		ANCHOR_TOP = 2
	};

	enum {
		ANIM_MOVE = 0x10000,
		ANIM_COLOR = 0x10001,
		ANIM_SHOW_ALPHA = 0x10002,
		ANIM_META_ALPHA = 0x10003,
		ANIM_SIZE = 0x10004,
		ANIM_SCALE = 0x10005,
		ANIM_ROT = 0x10006,
		ANIM_QUAT = 0x10007,
		ANIM_ALIGN = 0x10008,
		ANIM_ORIGIN = 0x10009,
		ANIM_ANCHOR = 0x1000A
	};

	enum {
		GET_CURRENT = 0,
		GET_ANIMTARGET = 1,
		GET_ANIMNEXT = 2
	};

	/* how a widget is made (names after ScePaf-RE) */
	class CreateParam {
	public:
		CreateParam()
			: m_create_drawobj(true), m_parent_protected(false), m_under_template(false),
			  reserved(0)
		{
		}

		/* a static one is destroyed at exit (CompositeButton's) */
		~CreateParam()
		{
		}

		bool m_create_drawobj : 1;
		bool m_parent_protected : 1;
		bool m_under_template : 1;
		uint32_t reserved : 29;
	};

	/* how SetFocus moves the focus to it, and a search for where the focus goes
	   (widget/core/focus.h: defined outside, as a definition here moves SNC's code in
	   unrelated files) */
	class SetFocusInfo;
	class FocusSearchInfo;

	/* how a draw object of the widget is kept (names ours): three flags set when it is
	   made, its ID and a value made 0 */
	class DrawObjAttr {
	public:
		DrawObjAttr(uint32_t id) : m_flag0(true), m_flag1(true), m_flag2(true), m_id(id), m_value(0)
		{
		}

		/* copied field by field, as Sony's copies are */
		DrawObjAttr(const DrawObjAttr &other)
			: m_flag0(other.m_flag0), m_flag1(other.m_flag1), m_flag2(other.m_flag2),
			  m_id(other.m_id), m_value(other.m_value)
		{
		}

		DrawObjAttr &operator=(const DrawObjAttr &other)
		{
			m_flag0 = other.m_flag0;
			m_flag1 = other.m_flag1;
			m_flag2 = other.m_flag2;
			m_id = other.m_id;
			m_value = other.m_value;
			return *this;
		}

		bool m_flag0 : 1;
		bool m_flag1 : 1;
		bool m_flag2 : 1;
		uint32_t m_id : 8;
		uint32_t m_value : 4;
	};

	/* a draw object of the widget, found by its ID (names ours) */
	class DrawObjEntry {
	public:
		DrawObjEntry(graph::DrawObj *obj, uint32_t id) : m_obj(obj), m_attr(id)
		{
		}

		DrawObjEntry(const common::SharedPtr<graph::DrawObj> &obj, const DrawObjAttr &attr)
			: m_obj(obj), m_attr(attr)
		{
		}

		common::SharedPtr<graph::DrawObj> m_obj;
		DrawObjAttr m_attr;
	};

	/* the widgets being destroyed; Plugin::Exit waits for them */
	static int32_t s_destroying_count;

	Widget(Widget *parent, CreateParam *param);
	virtual ~Widget();

	virtual int32_t HandlerProc(Event *e);
	virtual int32_t HandleKeycodeEvent(Event *e);
	virtual int32_t HandleTouchEvent(Event *e);
	virtual int32_t HandleGestureEvent(Event *e);
	virtual int32_t HandleFocusSwitchEvent(Event *e);
	virtual void EnableFocusEvent(bool on_off);
	virtual int32_t EnableTouchRearEvent(bool on_off);

	virtual bool IsTouchRearEventEnabled()
	{
		return m_touch_rear_event;
	}

	virtual void EnableTouchFrontEvent(bool on_off)
	{
		m_touch_front_event = on_off;
	}

	virtual bool IsTouchFrontEventEnabled()
	{
		return m_touch_front_event;
	}

	virtual int32_t DoEvent(int32_t type, Event *e);

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void DestroyWidget();

	virtual uint32_t CheckState()
	{
		return m_state;
	}

	virtual uint32_t ChangeState(uint32_t state, int32_t ev_type);
	virtual int32_t SetEnvironment(Environment *env);
	virtual int32_t SetScene_core(Scene *scene);
	virtual bool RemoveChild(Widget *obj);
	virtual void RegisterChild(Widget *obj, Widget *sibling);
	virtual bool ReorderChild(Widget *obj, Widget *sibling);

	virtual Widget *GetInternalWidget()
	{
		return this;
	}

	/* shows it over 50 ticks after parameter (at once with set); returns when it is shown */
	virtual float DoTransition_core(float parameter, bool set)
	{
		float time = set ? 0.0f : 50.0f;

		SetShowAlpha(1.0f, new Timer(time), ANIM_SHOW_ALPHA, 2, parameter, NULL);
		return parameter + time;
	}

	/* hides it the same way */
	virtual float DoTransitionReverse_core(float parameter, bool set)
	{
		float time = set ? 0.0f : 50.0f;

		SetShowAlpha(0.0f, new Timer(time), ANIM_SHOW_ALPHA, 2, parameter, NULL);
		return parameter + time;
	}

	virtual int32_t SetColor_core(const math::v4 &v);
	virtual int32_t SetSize_core_force(const math::v4 &v, uint32_t notify_layout,
					   bool force);
	virtual void UpdateSize();
	/* Text keeps the flag only while it fits in its scene */
	virtual void SetFitToScene(bool on_off)
	{
		m_fit_to_scene = on_off;
	}

	virtual void Update();
	virtual void UpdateAfterChildren();

	virtual void UpdatePost()
	{
	}

	virtual int32_t OnDraw(graph::DrawInfo &info);
	/* called after its children are drawn, when OnDraw returned 1 (Camera's draw walk);
	   Button draws its glow there */
	virtual int32_t ApplyDrawObjState(graph::DrawInfo &info);

	/* a touch of the port and ID has been captured (arg 0) or let go (names ours) */
	virtual void OnTouchCaptureChanged(uint32_t port, uint32_t id, int32_t arg)
	{
	}

	/* a key of the pad port has been captured (captured) or let go; group 1 for the keys of
	   pad capture 0x17c, 0 for those of 0x17d (names ours) */
	virtual void OnPadCaptureChanged(bool captured, int32_t group, uint32_t port, uint32_t key)
	{
	}

	virtual math::v4 calc_align_offset(const math::v4 &size, const math::v4 &aoffs,
					   uint32_t ax, uint32_t ay, uint32_t az);
	virtual int32_t SetOrigin(uint32_t ax, uint32_t ay, uint32_t az, Timer *t);

	virtual void GetOrigin(uint32_t &ax, uint32_t &ay, uint32_t &az)
	{
		ax = m_origin_x;
		ay = m_origin_y;
		az = m_origin_z;
	}

	virtual math::v4 calc_origin_offset(const math::v4 &size, const math::v4 &offs,
					    uint32_t ax, uint32_t ay, uint32_t az);
	virtual int32_t SetAnchor(uint32_t ax, uint32_t ay, uint32_t az, Timer *t);

	virtual void GetAnchor(uint32_t &ax, uint32_t &ay, uint32_t &az)
	{
		ax = m_anchor_x;
		ay = m_anchor_y;
		az = m_anchor_z;
	}

	virtual math::v4 calc_anchor_offset(const math::v4 &size, const math::v4 &offs,
					    uint32_t ax, uint32_t ay, uint32_t az);
	/* the hit region at 0x210, made on first use */
	virtual void *GetHitRegion();
	virtual int32_t SetHitRegion(const math::v4 &region);

	virtual void UpdateLayoutAttribute()
	{
	}

	virtual void UpdateAdjustSize()
	{
	}

	virtual void UpdateAdjustChildSize(Widget *child);
	/* puts the draw object in place of the one with the ID and returns that one (an empty
	   pointer where it has none); an empty object deletes it (name Sony's, by NID hash) */
	virtual common::SharedPtr<graph::DrawObj> SwapDrawObj(common::SharedPtr<graph::DrawObj> obj,
							      int32_t id);
	virtual graph::DrawObj *GetDrawObj(int32_t obj);
	/* the texture of the draw object (name after ScePaf-RE) */
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index)
	{
		return 0x80AF4101;
	}

	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf)
	{
		return SetTexture(surf, 0, 0);
	}

	virtual int32_t GetTexture(intrusive_ptr<graph::Surface> &surf, int32_t obj, uint32_t index)
	{
		return 0x80AF4101;
	}

	virtual int32_t GetTexture(intrusive_ptr<graph::Surface> &surf)
	{
		return GetTexture(surf, 0, 0);
	}

	/*
	 * Three slots no source names (ScePafWidget C3D53C98, AC3B6830, FD48BD49): they set
	 * and read a bit of a draw object's 4-bit mask through the draw manager. Not
	 * converted; declared for the slots after them.
	 */
	virtual int32_t SetDrawObjMaskBit(const void *param, int32_t obj);
	virtual int32_t SetDrawObjMaskBit(const void *param, int32_t obj, uint32_t bit);
	virtual void *GetDrawObjMaskBit(int32_t obj);
	virtual int32_t SetString(const wstring &str, int32_t obj)
	{
		return 0x80AF4101;
	}

	virtual int32_t SetString(const wstring &str)
	{
		return SetString(str, 0);
	}

	virtual int32_t GetString(wstring &str, int32_t obj)
	{
		return 0x80AF4101;
	}

	virtual int32_t GetString(wstring &str)
	{
		return GetString(str, 0);
	}

	/* the sound of its sound parameter with the ID (names ours, the slots to SetFocusDirection) */
	virtual WidgetSoundParam::Sound *GetSound(int32_t id)
	{
		return FindSound(id);
	}

	/* a sound to take as the one of the index (names ours) */
	virtual void AddSound(uint32_t index, WidgetSoundParam::Sound &sound)
	{
	}

	virtual void SetFocusRootState(uint32_t state)
	{
		if (m_focus_root != NULL)
			m_focus_root->m_state = state;
	}

	virtual int32_t SetFocus(SetFocusInfo *info);
	/* the widget the focus moves to in the direction (0 up, 1 down, 2 left, 3 right) */
	virtual void SetFocusDirection(int32_t dir, IDParam id);
	virtual void SetFocusDirectionWidget(int32_t dir, Widget *widget);
	virtual void SetFocusSearchRect(const math::rect &rect);
	/* the flag kept with the focus directions and search rect (0x81222140; this slot to
	   GetFocusShapeSize named ours) */
	virtual void EnableFocusSearchRect(bool on_off);
	/* the focus moved by the search (a FocusSearchInfo, or NULL for none); the widget it
	   went to (export F7F7F682) */
	virtual Widget *SetFocusOption(const void *param, int32_t option);
	/* every direction, or none while its focus directions are off */
	virtual int32_t GetFocusDirectionMask()
	{
		return m_focus_direction_off ? 0 : 7;
	}

	virtual int32_t GetFocusPriority()
	{
		return 0;
	}

	/* its position, the focus shape's offset added (0x81222258) */
	virtual math::v4 GetFocusShapePos();
	/* its world position, the focus shape's offset added (0x812222c4) */
	virtual math::v4 GetFocusShapeWorldPos();
	/* its size, or the focus shape's where that is set (0x8122230c) */
	virtual math::v4 GetFocusShapeSize();
	virtual int32_t EventExecDispatch(Event *e);
	virtual int32_t EventExecBroadcast(Event *e);
	virtual int32_t EventExecDirect(Event *e);
	/* ui::Text's: whether it handles its text events (name ours) */
	virtual bool IsTextEventEnabled()
	{
		return true;
	}

	/* the sound with the ID (export 6811A0CF, name ours) */
	WidgetSoundParam::Sound *FindSound(int32_t id);
	/* the pad keys of its capture 0x17c (dir keys) or 0x17d (the others) let go, their gestures
	   reset with reset; key 255 for all of them (0x811513f0, 0x811510ce, names ours) */
	int32_t ReleasePadCaptureDir(uint32_t key, bool reset);
	int32_t ReleasePadCaptureButton(uint32_t key, bool reset);
	/* captures the pad port's key, its gestures applied (0x8115d4de, name ours) */
	int32_t CapturePad(uint32_t port, uint32_t key, bool apply, bool reset_touch);
	/* its depth among its scene's layers, from its parents' down (0x81162db2, name ours) */
	uint64_t GetLayerDepth();
	/* the draw object added, the surface drawn as the widget's, and the size updated with
	   update (export 7A682C7B, name ours) */
	void AddDrawObj(const DrawObjEntry &entry, bool update);
	/* the draw object with the entry's ID replaced by the entry's (0x81160032, name ours) */
	void ReplaceDrawObj(const DrawObjEntry &entry, bool update);
	/* whether its draw objects are ready to draw (export 9B9A9406) */
	bool IsPrepareObjs();
	/* exports 7A3A7304, 0FCCC833, B94694E3 */
	void SetDragMode(uint32_t mode);
	void KillObjAnim(int32_t obj, int32_t id);
	/* the draw object with the ID, or the end (name ours) */
	vector<DrawObjEntry>::iterator FindDrawObj(int32_t id);
	/* the gestures its touches are given; with update, m_hit_check follows whether there are
	   any */
	void SetGestureMask(uint32_t mask, bool update);
	/* the size, on the axes adjusted to the scene its scene's, passed on to SetSize_core_force */
	int32_t SetSize_core(const math::v4 &size, uint32_t notify_layout, bool force);
	int32_t SetPos_core(const math::v4 &pos, uint32_t notify_layout, bool force);
	void SetObjAnim(int32_t obj, int32_t id, const math::v4 &value, Timer *t, int32_t opt);

	/* room for num draw objects (name ours) */
	void ReserveDrawObj(uint32_t num)
	{
		if (m_drawobjs.capacity() < num)
			m_drawobjs.reserve(num);
	}

	/* whether it is the widget its focus root focuses first (0x8114fd64, name ours) */
	bool IsInitFocus();

	math::v4 *GetSize(int32_t opt);
	math::v4 *GetScale(int32_t opt);
	/* the world matrix made again from its position, scale and rotation and its parent's */
	void UpdateMatrix();

	/* the world matrix, made again first where m_update_flag asks for it (inline in Sony's
	   callers, name ours) */
	const math::matrix &GetWorldMatrix()
	{
		if ((m_update_flag & 7) != 0)
			UpdateMatrix();
		return m_world_matrix;
	}

	/* culled or not, its children with it (Sony's copy out of line, 0x81225744; name
	   ours) */
	void SetCulled(bool culled)
	{
		m_culled = culled;
		SetFitToScene(culled);
		for (list<Widget *>::iterator it = m_children.begin(); it != m_children.end(); ++it)
			(*it)->SetCulled(culled);
	}

	/* the hit box's mode, and its size and center offset unless both are zero (inline in
	   Sony's callers, name ours) */
	/* the gestures of the type taken off the pad port, or off the touch port's touch
	   (exports 0DB27A25, A74F495E) */
	void remove_pad_gesture_mask(uint32_t port_id, uint32_t type);
	void remove_touch_gesture_mask(uint32_t port_id, uint32_t touch_id, uint32_t type);

	void SetHitBox(uint8_t mode, const math::v4 &size, const math::v4 &center)
	{
		m_hit_box_mode = mode;
		/* only x, y and z count */
		if (!(math::v3(size.getImpl().getXYZ()) == math::v3(0.0f, 0.0f, 0.0f)) ||
		    !(math::v3(center.getImpl().getXYZ()) == math::v3(0.0f, 0.0f, 0.0f))) {
			if (m_hit_box == NULL)
				m_hit_box = new math::v4[2];
			m_hit_box[1] = size;
			m_hit_box[0] = center;
		}
	}
	math::v4 GetPos(int32_t opt, Widget *ref, const math::v4 &off_pos);
	math::v4 GetPosCenter(int32_t opt, Widget *ref, const math::v4 &off_pos);
	/* its anchor offset; with opt 1 the target of a running anchor animation */
	math::v4 GetAnchorOffset(int32_t opt);
	/* marks what its next update recalculates */
	void SetUpdateParam(uint32_t param);
	math::v4 GetRot(int32_t opt, Widget *ref, const math::v4 &off_rot, int32_t option);
	/* turned to look at the target's position plus the offset, with up taken from ref
	   (export 88A433CC; argument names ours) */
	int32_t LookAt(Widget *target, const math::v4 &offs, Widget *ref, const math::v4 &up);
	/* the rectangle it clips its children to */
	math::rect CalcClipRect();
	math::v4 GetAlignOffsetToCenter(int32_t opt);
	Camera *GetCamera() const;

	int32_t SetPos(const math::v4 &pos, Timer *t = NULL, int32_t id = ANIM_MOVE,
		       int32_t opt = 0, float delay = 0.0f, AnimCB cb_fin = NULL,
		       void *usrdata = NULL);
	/* the values for each resolution, in the array */
	int32_t SetPos(const ResolutionData *pos, uint32_t num, Timer *t, int32_t id, int32_t opt,
		       float delay, AnimCB cb_fin, void *usrdata);
	/* the alpha it is shown with (Sony's name from the export) */
	int32_t SetShowAlpha(float alpha, Timer *t, int32_t id, int32_t opt, float delay,
			     AnimCB cb_fin);
	int32_t SetSize(const math::v4 &size, Timer *t = NULL, int32_t id = ANIM_SIZE,
			int32_t opt = 0, float delay = 0.0f, AnimCB cb_fin = NULL,
			void *usrdata = NULL);
	int32_t SetSize(const ResolutionData *size, uint32_t num, Timer *t, int32_t id,
			int32_t opt, float delay, AnimCB cb_fin, void *usrdata);
	int32_t SetRot(const math::v4 &rot, int32_t option, Timer *t, int32_t id, int32_t opt,
		       float delay, AnimCB cb_fin, void *usrdata);
	int32_t SetRot(const ResolutionData *rot, uint32_t num, int32_t option, Timer *t,
		       int32_t id, int32_t opt, float delay, AnimCB cb_fin, void *usrdata);
	int32_t SetScale(const math::v4 &scale, Timer *t = NULL, int32_t id = ANIM_SCALE,
			 int32_t opt = 0, float delay = 0.0f, AnimCB cb_fin = NULL,
			 void *usrdata = NULL);
	int32_t SetQuat(const math::quaternion &quat, Timer *t = NULL, int32_t id = ANIM_QUAT,
			int32_t opt = 0, float delay = 0.0f, AnimCB cb_fin = NULL,
			void *usrdata = NULL);
	int32_t SetMetaAlpha(float alpha, Timer *t = NULL, int32_t id = ANIM_META_ALPHA,
			     int32_t opt = 0, float delay = 0.0f, AnimCB cb_fin = NULL,
			     void *usrdata = NULL);
	int32_t SetColor(const math::v4 &color, Timer *t = NULL, int32_t id = ANIM_COLOR,
			 int32_t opt = 0, float delay = 0.0f, AnimCB cb_fin = NULL,
			 void *usrdata = NULL);
	int32_t SetAlignOffset(const math::v4 &offs, Timer *t, int32_t id, int32_t opt,
			       float delay, AnimCB cb_fin, void *usrdata);
	int32_t SetAlign(uint32_t ax, uint32_t ay, uint32_t az, Timer *t);
	int32_t SetAdjust(uint32_t ax, uint32_t ay, uint32_t az);
	int32_t SetPosCenter(const math::v4 &v);
	/* puts the widget behind the other in its parent's draw order */
	int32_t SetBehind(Widget *w);
	/* moves the widget under the parent, before the sibling (NULL: last), queued on
	   s_list_restruct for the next update; -1 for no parent or while it is held (name Sony's,
	   the parameters ours) */
	int32_t SetParent(Widget *parent, Widget *sibling, int32_t option);

	/* the texture animation of the draw object: frames start to end of a grid of column_num by
	   row_num, over the timer's time */
	int32_t SetTexAnim(int32_t obj, int32_t column_num, int32_t row_num, int32_t start_frame,
			   int32_t end_frame, Timer *t, int32_t option, float delay, AnimCB cb_fin,
			   void *userdata);
	/* the descendants of the type, not held, added to the list; how many (0x811529e0, name
	   ours) */
	int32_t FindChildrenByType(const char *type, vector<Widget *> *out);

	/* the list cleared first; nothing for no type (inline in Sony's callers, name ours) */
	void GetChildrenByType(const char *type, vector<Widget *> &out)
	{
		if (type != NULL) {
			out.clear();
			FindChildrenByType(type, &out);
		}
	}

	/* the first of its draw objects with an ID from id on (0x81160486, name ours) */
	vector<DrawObjEntry>::iterator LowerBoundDrawObj(int32_t id);
	/* the draw object put before it, the surface drawn as the widget's, and the size updated
	   with update (0x8115ff82, name ours) */
	void InsertDrawObj(vector<DrawObjEntry>::iterator it, const DrawObjEntry &entry, bool update);

	/* deletes the widget's Animator and binds this one (name after ScePaf-RE) */
	int32_t SetAnimator(Animator *animator)
	{
		delete m_animator;
		m_animator = animator;
		if (m_animator != NULL)
			m_animator->BindWidget(this);
		return 0;
	}

	/* deletes the widget's Layouter and binds this one (name after ScePaf-RE) */
	int32_t SetLayouter(Layouter *layouter)
	{
		delete m_layouter;
		m_layouter = layouter;
		if (m_layouter != NULL)
			m_layouter->BindWidget(this);
		return 0;
	}

	/* its color: the RGB of its bytes and its alpha (inline in Sony's callers; name ours) */
	math::v4 GetColor() const
	{
		uint16x8_t rgba = vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(m_color)));
		math::v4 color(sce::Vectormath::Simd::Aos::Vector4(
			vmulq_f32(vcvtq_f32_u32(vmovl_u16(vget_low_u16(rgba))),
				  vdupq_n_f32(1.0f / 255.0f))));

		color.set_w(m_color_alpha);
		return color;
	}

	/* registers a delete callback (inline in Sony's callers; name ours) */
	void AddDeleteCB(DeleteCB func, void *data)
	{
		if (m_list_delete_cb.size() == 0 && m_list_delete_cb.capacity() < 1)
			m_list_delete_cb.reserve(1);
		m_list_delete_cb.push_back(DeleteCBInfo(func, data));
	}

	/* drops the first delete callback with the function and data (inline in Sony's callers; name ours) */
	void RemoveDeleteCB(DeleteCB func, void *data)
	{
		for (vector<DeleteCBInfo>::iterator it = m_list_delete_cb.begin();
		     it != m_list_delete_cb.end(); ++it) {
			if (it->func == func && it->data == data) {
				vector<DeleteCBInfo>::iterator first = it;
				vector<DeleteCBInfo>::iterator last = it + 1;

				m_list_delete_cb.erase(first, last);
				break;
			}
		}
	}

	/* the descendant with the ID */
	Widget *FindChild(const IDParam &id, int32_t option);
	AnimQueue *FindAnim(int32_t id);
	int32_t KillAnimType(int32_t type);
	/* the animation's listeners stopped where they are */
	int32_t KillAnim(int32_t id);
	/* the listener queued for the animation */
	int32_t SetAnimListener(int32_t id, AnimListener *listener, int32_t slot);

	ResolutionData *get_resolution_data(ResolutionData *array, uint32_t num, uint32_t id);

	math::v4 m_pos;
	math::v4 m_scale;
	math::v4 m_size;
	math::v4 m_rot;
	uint8_t reserved[0x50];
	/* its world matrix, as UpdateMatrix made it (name ours) */
	math::matrix m_world_matrix;
	math::v4 m_align_offs;
	math::v4 m_anchor_offs;
	math::v4 m_origin_offs;
	uint8_t reserved2[0x18];
	IDParam m_name;
	uint32_t m_state;
	void *m_user_data[2];
	UserData *m_user_data2[2];
	Environment *m_env;
	Widget *m_parent;
	Scene *m_scene;
	list<Widget *> m_children;
	uint8_t reserved3b0 : 7;
	/* both set, AppIconBase::OnDraw dims an icon held in its mode 1; no writer found (names ours,
	   from that use) */
	bool m_dim_held : 1;
	uint8_t reserved3c0 : 7;
	bool m_dim_held_enable : 1;
	/* its children not ready to draw yet, as CountPrepareChildren counted them; a box lays
	   out only at 0 (name ours, after CountPrepareChildren) */
	uint16_t m_prepare_count;
	uint8_t reserved3[3];
	/* what UpdateMatrix still has to make again: bits 0 to 2 (name after ScePaf-RE) */
	uint8_t m_update_flag;
	/* 0xff before a reverse transition (name after ScePaf-RE) */
	uint8_t m_transition;
	uint8_t reserved3a[3];
	/* the keys bound to it while m_bound_keycode (names after ScePaf-RE) */
	uint32_t m_keycode;
	/* its draw priority and the layer it is drawn in, its scene's weight of the layer times the
	   priority adding up to its depth (GetLayerDepth; names ours) */
	uint32_t m_priority : 8;
	uint32_t m_layer : 4;
	uint32_t reserved3x : 4;
	/* how it is modal (export 42C5A4D4): 0 or 1 as Environment::RegisterModalWidget's mode, 2
	   not modal; and the option given with it (names ours) */
	uint32_t m_modal_mode : 4;
	uint32_t m_modal_option : 4;
	uint32_t reserved3x1 : 8;
	/* how the camera clips it and its children (1 the clip stack; 2 and 3 the scissor
	   snapped inward and to the nearest pixel, else outward), and where it is drawn (1
	   into a surface of its scene's list) (Camera's draw walk; names ours) */
	uint32_t m_clip_mode : 4;
	uint32_t reserved3x3 : 4;
	uint32_t m_draw_target : 4;
	uint32_t reserved3x4 : 4;
	/* what the touch event slots set (names after them) */
	bool m_touch_front_event : 1;
	bool m_touch_rear_event : 1;
	bool reserved3y1 : 1;
	bool reserved3y2 : 1;
	/* the focus moves to it within its group (ButtonBaseStyleParam's "focus_in_group"; name
	   after it) */
	bool m_focus_in_group : 1;
	bool reserved3y4 : 1;
	bool reserved3y5 : 1;
	/* a list view's drag has begun (ListView::BeginDrag sets it; name ours) */
	bool m_drag_begun : 1;
	/* none of its focus directions (name ours) */
	bool m_focus_direction_off : 1;
	bool reserved3z1 : 1;
	/* the focus code takes it as a focus target (Widget::HandleFocusSwitchEvent; name ours) */
	bool m_focus_target : 1;
	/* a scene or camera, which the touch walk passes over (name ours) */
	bool m_not_hit_target : 1;
	/* a composite button takes its focus switch events itself (CompositeButton's ctor and
	   EnableFocusEvent; name ours) */
	bool m_composite_focus : 1;
	bool reserved3z5 : 1;
	bool reserved3z6 : 1;
	/* drawn before its children, not after (Camera's draw walk; name ours) */
	bool m_draw_before_children : 1;
	/* its parent writes it: WriteWidget writes only its children; a box does not lay it out
	   (its exclusive list) (names after ScePaf-RE) */
	bool m_parent_protected : 1;
	/* made by a template (TemplateOpen); WriteWidget leaves it out */
	bool m_under_template : 1;
	/* it takes every event while its scene is in debug mode 2 or 3 (GrabEventRouting; name
	   ours) */
	bool m_debug_events : 1;
	/* its clip and scissor rectangles pushed around its children (name ours) */
	bool m_clip_children : 1;
	bool reserved3t : 1;
	/* its draw objects draw to a surface (name after SurfaceDrawParam's) */
	bool m_surface_draw : 1;
	bool reserved3v : 1;
	bool reserved3w : 1;
	/* taps are taken (AppIconSimple::EnableTap; AppIconBase sets it; name after the export) */
	bool m_enable_tap : 1;
	bool reserved3c : 1;
	bool reserved3d : 1;
	/* not destroyed when its page closes (name ours) */
	bool m_destroy_protected : 1;
	bool reserved3e : 1;
	bool reserved3f : 1;
	bool reserved3g : 1;
	/* outside the camera's view, not drawn (Camera::add_obj; name ours) */
	bool m_culled : 1;
	/* what SetFitToScene sets */
	bool m_fit_to_scene : 1;
	/* drawn by the camera also when it is culled (name ours) */
	bool m_draw_always : 1;
	bool reserved3k : 1;
	bool reserved3l : 1;
	bool m_bound_keycode : 1;
	bool reserved3n : 1;
	bool reserved3o : 1;
	/* cleared before a reverse transition (name after ScePaf-RE) */
	bool m_transition_complete : 1;
	uint8_t reserved3p[1];
	/* the gestures a touch it captures is given (ScePafWidget D3D0A7C3 applies them; name
	   from that use) */
	uint32_t m_touch_gesture_mask;
	/* the alpha SetShowAlpha sets, and it multiplied by its parents' (names after ScePaf-RE) */
	float m_show_alpha;
	float m_show_a_sum;
	/* the alpha SetMetaAlpha sets (name ours) */
	float m_meta_alpha;
	/* m_meta_alpha multiplied by its parents' (name after ScePaf-RE) */
	float m_meta_a_sum;
	/* its color: the alpha as SetColor_core gave it, and the RGBA bytes (names ours) */
	float m_color_alpha;
	uint32_t m_color;
	uint8_t reserved3y[4];
	Timer *m_resolution_timer;
	Timer *m_resolution_timer_next;
	ResolutionData *m_pos_array;
	ResolutionData *m_size_array;
	ResolutionData *m_rot_array;
	uint8_t m_resolution_id;
	uint8_t m_resolution_id_pre;
	uint8_t reserved4;
	/* the entries of m_pos_array */
	uint8_t m_pos_num;
	/* the entries of m_size_array and m_rot_array (names ours) */
	uint8_t m_size_num;
	uint8_t m_rot_num;
	uint8_t reserved5[2];
	/* the touches it holds, by port << 8 | touch ID (name ours) */
	map<uint32_t, TouchCapture *> m_touch_captures;
	uint8_t reserved5b[0xc];
	/* the hit box GetHitBoxSize and GetHitBoxCenter give (names ours): its center offset and
	   its size, the size an addition to the widget's (mode 1), a scale of it (2) or itself (3);
	   mode 0 is the widget's own size */
	math::v4 *m_hit_box;
	uint8_t m_hit_box_mode;
	/* how a touch finds it and hits it (names after ScePaf-RE) */
	uint8_t m_hit_find_mode;
	uint8_t m_hit_detect_mode;
	uint8_t m_hit_detect_b_mode;
	/* whether touches are tested against it, set by AppIconBase (name ours) */
	uint8_t m_hit_check;
	uint8_t reserved5a[7];
	uint32_t m_align_x : 4;
	uint32_t m_align_y : 4;
	uint32_t m_align_z : 4;
	uint32_t m_origin_x : 4;
	uint32_t m_origin_y : 4;
	uint32_t m_origin_z : 4;
	/* what SetAnchor sets (names after its arguments) */
	uint32_t m_anchor_x : 8;
	uint32_t m_anchor_y : 8;
	uint32_t m_anchor_z : 8;
	/* what SetAdjust sets (names after the resource's adjust_x and adjust_y) */
	uint32_t m_adjust_x : 4;
	uint32_t m_adjust_y : 4;
	uint32_t m_adjust_z : 4;
	uint32_t reserved6b : 4;
	uint8_t reserved7[0xc];
	/* how it is sized to its contents where it adjusts, shared by reference count; NULL for
	   the default, ScePafWidget_BE1CEA2E (name ours) */
	AdjustParam *m_adjust_param;
	/* lays the widget out each update, and animates it (names after ScePaf-RE) */
	Layouter *m_layouter;
	Animator *m_animator;
	/* its focus state, its focus cursor and the focus of its children (names ours) */
	Focuser *m_focuser;
	FocusShape *m_focus_shape;
	uint8_t reserved7c[0x10];
	FocusRoot *m_focus_root;
	uint8_t reserved7d[0x14];
	/* its draw objects, drawn in order */
	vector<DrawObjEntry> m_drawobjs;
	/* what is called when it is deleted (names after DrawObj's, Sony's DWARF) */
	vector<DeleteCBInfo> m_list_delete_cb;
	uint8_t reserved7b[8];
	/* the indexes of the sounds it plays, a bit each (PlaySound; name ours) */
	uint32_t m_sound_mask : 24;
	/* the sounds of its sound parameter (names ours) */
	uint8_t m_sound_num;
	WidgetSoundParam::Sound *m_sounds;
};

inline WidgetSoundParam::~WidgetSoundParam()
{
	for (list<Widget *>::iterator it = m_widget_list.begin(); it != m_widget_list.end(); ++it)
		(*it)->RemoveDeleteCB(delete_cb, this);
}

inline WidgetHandlerParam::~WidgetHandlerParam()
{
	if (m_widget != NULL)
		m_widget->RemoveDeleteCB(delete_cb, this);
}

inline WidgetStyleParam::~WidgetStyleParam()
{
	for (list<Widget *>::iterator it = m_widget_list.begin(); it != m_widget_list.end(); ++it)
		(*it)->RemoveDeleteCB(delete_cb, this);
}

}
}

extern "C" {
/* gives the widget parameter its widget, dropping its delete callback from the old one */
int32_t ScePafResource_190E99A6(paf::ui::WidgetParam *param, paf::ui::Widget *widget);
/* the delete callback of a widget parameter's widget: the parameter forgets it */
void ScePafResource_2B873BCD(paf::ui::Widget *widget, void *param);
/* the texture of the ID from the widget parameter's plugin, NULL without a plugin */
paf::intrusive_ptr<paf::graph::Surface> ScePafResource_31C3B828(paf::ui::WidgetParam *param,
								 const paf::IDParam &id);
}

namespace paf {
namespace ui {

inline WidgetParam::~WidgetParam()
{
	if (m_widget != NULL)
		m_widget->RemoveDeleteCB(ScePafResource_2B873BCD, this);
}

}
}

extern "C" {
/* puts the widget into its transition: auto_delete deletes it once the time ends */
int32_t ScePafWidget_8AF26B1F(paf::ui::Widget *widget, bool auto_delete, float time);
/* the widget holds the touch of the port and ID, dragged in the mode: 0 once held, -1 if not */
int32_t ScePafWidget_DE592034(paf::ui::Widget *widget, uint32_t port, uint32_t id,
			      uint32_t drag_mode);
/* the widget lets go of the touch of the port and ID it holds, its release handled when
   release is set: -1 if it does not hold it */
int32_t ScePafWidget_093F1C89(paf::ui::Widget *widget, uint32_t port, uint32_t id, bool release);
/* captures the touch of the port, its gestures applied (names ours) */
int32_t ScePafWidget_D3D0A7C3(paf::ui::Widget *widget, uint32_t port, uint32_t id, bool apply,
			      bool reset_pad);
/* lets go of every touch it holds */
int32_t ScePafWidget_931AD41E(paf::ui::Widget *widget, bool release);
/* lets go of the touch of the port and ID it holds, its release handled when release is set:
   -1 if it does not hold it */
int32_t ScePafWidget_5ABB3393(paf::ui::Widget *widget, uint32_t port, uint32_t id, bool release);
/* plays its sound of the index where its sound mask has it: -1 when none is played */
int32_t ScePafWidget_2B1A1493(paf::ui::Widget *widget, int32_t index);
/* binds the keys of the mask to the widget (SetKeycode after ScePaf-RE) */
int32_t ScePafWidget_21125870(paf::ui::Widget *widget, uint32_t keycode);
/* gives the widget its focus cursor: a copy of the shape, its offset, size and rotation
   taken from the widget's own where the shape has none */
void ScePafWidget_13B246E0(paf::ui::Widget *widget, paf::ui::FocusShape *shape);
/* Hide(Timer *, int32_t opt, float delay, AnimCB): hides the widget, over the timer's time */
int32_t ScePafWidget_4588699E(paf::ui::Widget *widget, paf::Timer *t, int32_t opt, float delay,
			      paf::ui::AnimCB cb_fin);
/* Show(Timer *, int32_t opt, float delay, AnimCB): shows the widget, over the timer's time */
int32_t ScePafWidget_FF5BC957(paf::ui::Widget *widget, paf::Timer *t, int32_t opt, float delay,
			      paf::ui::AnimCB cb_fin);
/* the widget's transition ends after the time */
int32_t ScePafWidget_AC0C81D5(paf::ui::Widget *widget, float time);
/* the widget's transition ends after the time, and the widget is deleted */
int32_t ScePafWidget_5A9793B0(paf::ui::Widget *widget, float time);
/* adds to the count kept in bits 24 to 27 of the widget's flags */
int32_t ScePafWidget_A96A0B4A(paf::ui::Widget *widget, int32_t add);
/* Show(common::transition::Type, float delay, HandlerCB, void *): shows the widget with the
   transition (the NID's name, ScePaf-RE's NID list); returns the transition's time */
float ScePafWidget_44B22ACA(paf::ui::Widget *widget, paf::common::transition::Type type,
			    float delay, paf::ui::HandlerCB cb, void *data);
/* Hide(common::transition::Type, float delay, HandlerCB, void *): hides it the same way */
float ScePafWidget_F1176ACC(paf::ui::Widget *widget, paf::common::transition::Type type,
			    float delay, paf::ui::HandlerCB cb, void *data);
/* the widget put on its environment's modal list (on) or taken off it; 0 when it already was,
   -1 without an environment or when it cannot be added */
int32_t ScePafWidget_0253DC9C(paf::ui::Widget *widget, bool on);
/* the widget or the nearest of its parents below its scene that has a focus root, or NULL */
paf::ui::Widget *ScePafWidget_CFA1EB7A(paf::ui::Widget *widget);
/* makes the widget modal in the mode (0, 1), or not (2), with the option */
int32_t ScePafWidget_42C5A4D4(paf::ui::Widget *widget, int32_t mode, int32_t option);
/* the widget's adjust parameter, held by reference, and its size updated */
void ScePafWidget_55D83675(paf::ui::Widget *widget, paf::ui::AdjustParam *param);
/* the adjust parameter of the widgets that have none */
extern paf::ui::AdjustParam ScePafWidget_BE1CEA2E;
/* the widget's adjust parameter set to a copy of the one given and its adjust size updated where
   they differ; -1 where it has none, else 0 */
int32_t ScePafWidget_B1C23134(paf::ui::Widget *widget, const paf::ui::AdjustParam &param);
}

#endif
