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
#include <scetypes.h>
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
#include <graphics/text/text_style.h>

namespace paf {

class Timer;

namespace inputdevice {
class GestureListener;
class GestureParam;
}

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
class HitInfo;
class TouchInfo;
class DragState;
class PinchState;
class Camera;
class AnimListener;
class AnimQueue;
class TextureSource;
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

	/* a texture the widget type's resource names, and the draw object it is for (names
	   ours) */
	class TextureRef {
	public:
		const char *name;
		int32_t index;
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

}

/* Sony's build calls the push of a sound everywhere (0x81224cf0) rather than inlining it. */
template<>
inline __attribute__((noinline)) int
vector<ui::WidgetSoundParam::Sound>::push_back(const ui::WidgetSoundParam::Sound &val)
{
	int result;

	if (count + 1 <= limit || (result = reserve(limit + 10)) != 0) {
		new (&ptr[count]) ui::WidgetSoundParam::Sound(val);
		result = ++count;
	}
	return result;
}

namespace ui {

/*
 * The focus cursor of a widget: its shape, the offset, size and rotation it
 * takes, for each resolution in the arrays, and its texture (names ours).
 */
class FocusShape {
public:
	FocusShape()
		: m_shape(0), m_offset(0.0f, 0.0f, 0.0f), m_size(0.0f, 0.0f), m_rot(0.0f, 0.0f, 0.0f),
		  m_use_parts(false), m_changed(true)
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
	/* the cursor is drawn as the widget's part shapes (its m_focus_parts; a text's focused
	   link sets it); a part changed since the cursor took it (FocusInfo::UpdateParts; names
	   ours) */
	bool m_use_parts;
	bool m_changed;
};

}

/* Sony's code calls these two out of line for the focus shapes (the COMDATs w_text.cpp gives,
   0x8123a26c and 0x8123a458 in 3.650.011-TOOL); defined in w_text.cpp */
template<>
void vector<ui::FocusShape>::clear();
template<>
int vector<ui::FocusShape>::push_back(const ui::FocusShape &val);

namespace ui {

/* where the focus moves from a widget, and the rectangle it searches (names ours) */
class FocusHint {
public:
	__attribute__((always_inline)) FocusHint() : m_search_rect_enable(true)
	{
	}

	/* the IDs the focus moves to up, down, left and right (four members: Sony's destructor
	   destroys them one by one; names ours) */
	IDParam m_up;
	IDParam m_down;
	IDParam m_left;
	IDParam m_right;
	math::rect m_search_rect;
	/* whether the search rect is searched (EnableFocusSearchRect) */
	bool m_search_rect_enable;
};

class Focuser;

/* the widget the focus moves to in each direction: up, down, left and right (names ours) */
class FocusDirectionWidget {
public:
	FocusDirectionWidget()
	{
		m_widget[0] = NULL;
		m_widget[1] = NULL;
		m_widget[2] = NULL;
		m_widget[3] = NULL;
	}

	/* declared: Sony's ~Focuser tests it for NULL before deleting it */
	~FocusDirectionWidget()
	{
	}

	Widget *m_widget[4];
};

/* what a Focuser calls to move the focus: the focuser, its widget, and what Focus was given
   (name ours) */
typedef int32_t (*FocusCB)(Focuser *focuser, Widget *widget, void *data, void *move);

/* the callback moving the focus to the nearest widget the way it moves (0x81146794 TOOL; name
   ours) */
int32_t DefaultFocusCB(Focuser *focuser, Widget *widget, void *data, void *move);

/* a widget's focus: the function that moves it, and the hint where it goes (names ours) */
class Focuser {
public:
	Focuser(FocusCB cb) : m_widget(NULL), m_cb(cb), m_hint(NULL), m_data(NULL)
	{
	}

	virtual ~Focuser()
	{
		delete m_hint;
		delete m_data;
	}

	/* the focus moved by the callback, given the search it moves by (the export's NID names
	   it; "Focus" ours) */
	virtual int32_t ScePafWidget_97020684(void *move);

	void BindWidget(Widget *w)
	{
		m_widget = w;
	}

	/* the widget its hint names for the focus direction (0 up, 1 down, 2 left, 3 right): by
	   ID under its widget's focus root, else the hint's own widget (0x811460e8; name ours) */
	Widget *GetHintWidget(int32_t dir);
	/* the ID its hint names for the focus direction, where it has a widget and a hint
	   (0x811461da; name ours) */
	void GetHintID(int32_t dir, IDParam *id);

	/* its hint, made where it has none (name ours) */
	__attribute__((always_inline)) FocusHint *GetHint()
	{
		if (m_hint == NULL)
			m_hint = new FocusHint;
		return m_hint;
	}

	/* the ID of the focus direction in its hint (name ours) */
	void SetHintID(int32_t dir, IDParam id)
	{
		FocusHint *hint = GetHint();
		IDParam value = id;

		switch (dir) {
		case 0:
			hint->m_up = value;
			break;
		case 1:
			hint->m_down = value;
			break;
		case 2:
			hint->m_left = value;
			break;
		case 3:
			hint->m_right = value;
			break;
		}
	}

	/* (name ours; no user found, a pointer in the data) */
	static Focuser *s_enter_focuser;

	/* its direction widgets, made where it has none (name ours) */
	FocusDirectionWidget *GetDirectionWidget()
	{
		if (m_data == NULL)
			m_data = new FocusDirectionWidget;
		return m_data;
	}

	Widget *m_widget;
	FocusCB m_cb;
	FocusHint *m_hint;
	/* the widgets the focus moves to in each direction (SetFocusDirectionWidget) */
	FocusDirectionWidget *m_data;
};

/* the widgets whose focus directions name a widget (SetFocusDirectionWidget; names ours) */
class FocusReferrers {
public:
	FocusReferrers()
	{
	}

	/* the widgets whose focus roots search this one (FocusRoot's Members; DestroyWidget takes it
	   out of them; name ours) */
	vector<Widget *> m_roots;
	vector<Widget *> m_referrers;
};

/* the focus of the widgets under one that holds it (names ours) */
class FocusRoot {
public:
	/* what SetFocusRootState sets; the widget under it that has the focus (ReleaseFocus,
	   ScrollViewBase's focus moves) */
	uint32_t m_state;
	/* the widget that takes the focus first */
	Widget *m_init_focus;
	/* the frames a scroll view under it has waited to move the focus, cleared where it moves
	   it by its own search (name ours) */
	uint32_t m_focus_moved;
	/* nonzero: a scroll view under it does not take the focus (name ours, meaning open) */
	uint32_t m_focus_hold;
	uint32_t reserved16;
	/* the widgets SearchFocus searches with its children, as a vector's array, count, limit
	   and allocator; given back and the three set to -1 as a vector's destructor does (names
	   ours; no constructor or destructor here, as one in this class moves SNC's stack slots
	   in PlaneObj::Draw) */
	void *m_array;
	int32_t m_array_count;
	int32_t m_array_limit;
	uint32_t m_array_alloc;
	/* cleared by UpdateFocus where it is set (name ours, meaning open) */
	bool m_pending;

	/* m_array and its count as the vector they are (DestroyWidget; name ours) */
	vector<Widget *> &Members()
	{
		return *reinterpret_cast<vector<Widget *> *>(&m_array);
	}
};

/*
 * A widget's key focus, read from the "focus_" children of its element: whether
 * it takes focus and first, its direct keys, the widgets up, down, left and
 * right, its focus cursor and the rectangle it searches (members' names after
 * the attributes).
 */
class WidgetFocusParam : public VersatileParam {
public:
	static const char m_param_name[];
	static const char *const s_names[];

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

/* a move of a widget under a parent, kept until the next update makes it (name ours) */
class RestructInfo {
public:
	Widget *m_parent;
	Widget *m_widget;
	/* the sibling it goes before; NULL: last */
	Widget *m_sibling;
	/* 0 before the sibling under the parent, 1 behind the sibling, 2 in front of it, 3 and 4 a step
	   back and forward, 5 and 6 the back and the front (UpdateRestruct) */
	int32_t m_option;
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

	/* an animation ID's bit: the animation loops (name after ScePaf-RE) */
	enum {
		ANIM_OPTION_LOOP = 0x10000000
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

		DrawObjEntry(const common::SharedPtr<graph::DrawObj> &obj, uint32_t id)
			: m_obj(obj), m_attr(id)
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
	/* the widgets made and not yet destroyed (ScePafWidget exports it) */
	static int32_t s_instance_count;
	/* how often UpdateMatrix made the matrices again, the local one (L) and the world one (W)
	   (counters; no reader found) */
	static uint32_t s_upmat_count_L;
	static uint32_t s_upmat_count_W;

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
	static const char m_widget_type[];
	static const char *TypeName();

	virtual uint32_t CheckState()
	{
		return m_state;
	}

	virtual uint32_t ChangeState(uint32_t state, int32_t ev_type);
	virtual int32_t SetEnvironment(Environment *env);
	/* the widget taken off its environment: its holds on the focus, touches, pad keys and modal list let go
	   (0x811514e6; name Sony's) */
	void detach_environment();
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

	/* the gesture listener of the touch, made by the environment's gesture priority for the
	   gestures mask enables, with the parameter set where there is one (0x8115c05c) */
	common::SharedPtr<inputdevice::GestureListener> ApplyTouchGesture(uint32_t port, uint32_t id,
									    uint32_t mask,
									    const inputdevice::GestureParam *param);
	/* the same for the pad port's keys, the gestures of the type bits (0x8115c25c's pad twin
	   0x81158e98) */
	common::SharedPtr<inputdevice::GestureListener> ApplyPadGesture(uint32_t port, uint32_t type,
									 const inputdevice::GestureParam *param);
	/* the gestures of the type bits made for the touch (0x8115bd7a) and for the pad port
	   (0x81158d98), each by its own listener (0x811589ba tap, 0x811585c8 long press) */
	void apply_touch_gesture_mask(uint32_t port_id, uint32_t touch_id, uint32_t type);
	void apply_pad_gesture_mask(uint32_t port_id, uint32_t type);
	common::SharedPtr<inputdevice::GestureListener> apply_pad_gesture_tap(uint32_t port_id,
									       uint32_t type);
	common::SharedPtr<inputdevice::GestureListener> apply_pad_gesture_longpress(uint32_t port_id,
										     uint32_t type);

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
	/* its adjust parameters (at 0x210), made on first use */
	virtual AdjustParam *GetHitRegion();
	/* the adjust parameters set to the values (their reference count kept), made first where it
	   has none (0x81221bbc) */
	virtual int32_t SetHitRegion(const AdjustParam &param);

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

	/* the texture source (a const intrusive_ptr<TextureSource> *) requested through the
	   environment for the draw object's mask bit (bit 0; ScePafWidget C3D53C98, slot names
	   ours) */
	virtual int32_t SetDrawObjMaskBit(const void *param, int32_t obj);
	/* the same for the bit: one requested before dropped first (ScePafWidget AC3B6830) */
	virtual int32_t SetDrawObjMaskBit(const void *param, int32_t obj, uint32_t bit);
	/* the texture requested for the draw object's lowest mask bit set, NULL where none
	   (ScePafWidget FD48BD49) */
	virtual intrusive_ptr<TextureSource> GetDrawObjMaskBit(int32_t obj);
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

	/* nonzero to be found first in the focus search in the direction (CornerButton's: up and
	   down) */
	virtual int32_t GetFocusPriority(int32_t dir)
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
	/* the event done as the listeners of its type ask: run now, or kept for the environment's
	   next frame (0x81161baa) */
	int32_t EventExecDo(Event *e);
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
	/* makes it its widget group's current widget or not; a widget that stops being it lets go
	   of its pad button capture (0x811608f2, name ours) */
	void SetGroupCurrent(bool current);
	/* captures the pad port's key: a direction key (one m_keycode does not bind) as its pad
	   capture 0x17c, any other as 0x17d; with reset the holder of the other group is let go
	   first and a capture it already holds has its gestures reset, with reset_touch the
	   touches of both ports are let go (0x8115d4de, names ours) */
	int32_t CapturePad(uint32_t port, uint32_t key, bool reset, bool reset_touch);
	/* takes the port's pad keys of 0x17d where it is the widget of the key, or of 0x17c where
	   it has the focus; another widget that is lets go of them with reset (0x8115d29a,
	   0x8115d3be; names ours) */
	int32_t CapturePadButton(uint32_t port, uint32_t key, bool reset);
	int32_t CapturePadDir(uint32_t port, uint32_t key, bool reset);
	/* its depth among its scene's layers, from its parents' down (0x81162db2, name ours) */
	uint64_t GetLayerDepth();
	/* the draw object added, the surface drawn as the widget's, and the size updated with
	   update (export 7A682C7B, name ours) */
	void AddDrawObj(const DrawObjEntry &entry, bool update);
	/* the draw object with the entry's ID replaced by the entry's (0x81160032, name ours) */
	void ReplaceDrawObj(const DrawObjEntry &entry, bool update);
	/* whether its draw objects are ready to draw (export 9B9A9406) */
	bool IsPrepareObjs();
	/* where the line from start to end crosses the plane through pos with the normal, the hit
	   in the widget's space filled in: 1, or -1 for a line along the plane or no info (argument
	   names ours) */
	int32_t CalcHitLineToPlane(const math::v4 &start, const math::v4 &end, const math::v4 &pos,
				   const math::v4 &normal, HitInfo *info);
	/* the distance from the point to the line through line_a and line_b, along the normal of
	   the plane through the three; the foot of it, in the widget's space, filled in
	   (0x81157460; names ours) */
	float CalcDistPointToLine(const math::v4 &point, const math::v4 &line_a,
				  const math::v4 &line_b, HitInfo *info);
	/* the hit of the line from start to end on its hit box of the size, filled in (export
	   416CF405; argument names ours) */
	int32_t CalcHitBoxToLine(const math::v4 &size, const math::v4 &start, const math::v4 &end,
				 HitInfo *info);
	/* how far the corner of its hit box of the size farthest against the plane through pos with
	   the normal lies before it, in dist: 1 where it crosses the plane, else 0, -1 without
	   dist; the corner's hit filled in (export of the mangled name; argument names ours) */
	int32_t CalcHitBoxToPlane(const math::v4 &size, const math::v4 &pos, const math::v4 &normal,
				  float *dist, HitInfo *info);
	/* whether the line from line_a to line_b hits it with the event's touch, in the mode (1 the
	   point of the line nearest its hit box center only, 0 also the hit box, 2 and 3 never):
	   1 for a press, move or release it takes, 2 for the other events, 0 for no hit; the hit
	   filled in where the mode asks for it (0x811576d0; argument names ours) */
	int32_t HitDetect_core(uint32_t mode, Event *e, uint32_t mask, const math::v4 &line_b,
			       const math::v4 &line_a, HitInfo *hit);
	/* the widget the event's touch line hits, its children first (by depth from the camera
	   where it sorts by depth, else from the last), then itself: 2 with the hit filled in, 0 for
	   no hit, -1 without an environment or while held (export FECECAD1; argument names
	   ours) */
	int32_t HitDetectRouting(Event *e, const math::v4 &line_b, const math::v4 &line_a,
				 TouchInfo *touch, HitInfo *hit);
	int32_t GrabEventRouting(Event *e, const math::v4 &touch_p, const math::v4 &cam_p,
				 TouchInfo *t_info, HitInfo *h_info);
	/* a touch event ended at the widget and sent back to its ancestors (0x81161d70, name
	   ours) */
	int32_t RouteTouchEvent(Event *e);
	/* exports 7A3A7304, 0FCCC833, B94694E3 */
	void SetDragMode(uint32_t mode);
	/* a drag state let go of: the second becomes the first, the layout of it and its parent
	   marked dirty, the drag mode put back (0x8114e258; name ours) */
	int32_t ClearDragState(DragState *state);
	/* the touch the capture holds let go of: its drag cleared, the capture freed and taken out
	   of the map, the touch mask and gesture state reset where apply, the widget told
	   (0x8114e458; name ours) */
	int32_t ReleaseTouchCapture(map<uint32_t, TouchCapture *>::iterator it, TouchCapture *capture,
				    bool apply);
	void SetPinchMode(uint32_t mode);
	/* sets the steps its drag moves it in (name ours) */
	void SetDragQuant(uint8_t x, uint8_t y, uint8_t z)
	{
		m_drag_quant_x = x;
		m_drag_quant_y = y;
		m_drag_quant_z = z;
	}

	/* the smallest and the largest position its drag keeps it in (names ours) */
	void SetDragLimitMin(const math::v4 &pos)
	{
		if (m_drag_limit == NULL)
			m_drag_limit = new SceFVector3[2];
		m_drag_limit[0].x = pos.extract_x();
		m_drag_limit[0].y = pos.extract_y();
		m_drag_limit[0].z = pos.extract_z();
	}

	void SetDragLimitMax(const math::v4 &pos)
	{
		if (m_drag_limit == NULL)
			m_drag_limit = new SceFVector3[2];
		m_drag_limit[1].x = pos.extract_x();
		m_drag_limit[1].y = pos.extract_y();
		m_drag_limit[1].z = pos.extract_z();
	}
	void KillObjAnim(int32_t obj, int32_t id);
	/* the draw object with the ID, or the end (name ours) */
	vector<DrawObjEntry>::iterator FindDrawObj(int32_t id);
	/* the gestures its touches are given; with update, m_hit_check follows whether there are
	   any */
	void SetGestureMask(uint32_t mask, bool update);
	/* the layouter and animator of the parent and of itself told that kinds of change came
	   (0x8114c7e2, 0x8114c68a) */
	void NotifyLayoutDirty(uint32_t kind);
	void NotifyAnimatorDirty(uint32_t kind);
	/* the flags of the creation parameter taken over (0x81150052) */
	void InitCreateParam(const CreateParam *param);
	/* the count of base widgets destroyed, kept for the environment's start (0x81162e2a) */
	static int32_t GetBaseDestroyCount();
	static void SetBaseDestroyCount(int32_t count);
	static int32_t s_base_destroy_count;
	/* the size, on the axes adjusted to the scene its scene's, passed on to SetSize_core_force */
	int32_t SetSize_core(const math::v4 &size, uint32_t notify_layout, bool force);
	/* the position (w taken as 1), kept apart from the align offset's center; with store, the entry
	   of m_pos_array for the scene's resolution is set to it too */
	int32_t SetPos_core(const math::v4 &pos, uint32_t notify_layout, bool store);
	int32_t SetRot_core(const math::v4 &rot, int32_t option, uint32_t notify_layout);
	int32_t SetQuat_core(const math::quaternion &quat, uint32_t notify_layout);
	int32_t SetScale_core(const math::v4 &scale, uint32_t notify_layout);
	int32_t SetAlignOffset_core(const math::v4 &offs, uint32_t notify_layout);
	int32_t SetOriginOffset_core(const math::v4 &offs, uint32_t notify_layout);
	int32_t SetAnchorOffset_core(const math::v4 &offs, uint32_t notify_layout);
	int32_t SetMetaAlpha_core(float alpha);
	math::quaternion GetQuat(int32_t opt, Widget *ref, const math::quaternion &off_quat);
	math::v4 GetOriginOffset(int32_t opt);
	/* the property of the draw object (ID 0x201xx move, 0x202xx size, 0x203xx color, 0x204xx
	   rotation, 0x206xx texture region, 0x207xx revolution) animated to the value over the
	   timer; -1 for no such object or property */
	int32_t SetObjAnim(int32_t obj, int32_t id, const math::v4 &value, Timer *t, int32_t opt);

	/* room for num draw objects (name ours) */
	void ReserveDrawObj(uint32_t num)
	{
		if (m_drawobjs.capacity() < num)
			m_drawobjs.reserve(num);
	}

	/* whether it is the widget its focus root focuses first (0x8114fd64, name ours) */
	bool IsInitFocus();
	/* the focus cursor coming onto it or leaving it: the state set and the focus event 0x401
	   or 0x402 sent with the widget the cursor came from and the one it went to; leaving also
	   lets go of the direction keys it holds (0x81160920; name ours) */
	void SetCursorOn(bool on, Widget *from, Widget *to);
	/* the same for the focus events 0x403 and 0x404 of a widget that takes its children's events
	   first (0x81160a0c; name ours) */
	void NotifyCursorGroup(bool on, Widget *from, Widget *to);

	math::v4 *GetSize(int32_t opt);
	math::v4 *GetScale(int32_t opt);
	/* the bounds of a box of the size (up to the size in each axis) turned by the rotation:
	   the most each axis reaches (0x81156574; name ours) */
	math::v4 CalcBound(const math::v4 &size, const math::quaternion &quat);
	/* the bounds of its size turned by its rotation, relative to the widget (0x81156726; name
	   ours) */
	math::v4 GetSizeBound(int32_t opt, Widget *ref);
	/* the rect its touches hit it in */
	math::rect CalcHitRect();
	/* its world matrix turns it about one of the axes asked by more than the limit (0x81154586;
	   name ours) */
	bool IsRotated(bool x, bool y, bool z, float limit);
	/* the distance along the plane's normal (made a unit vector first unless is_unit) from the
	   point to the plane through the plane's point (0x811573b0; names ours) */
	float32x2_t CalcDistPointToPlane(const math::v4 &point, const math::v4 &plane_point,
					 const math::v4 &normal, int32_t is_unit);
	/* the closest points of two lines, each a point and a direction, and the distance
	   between them (0x8114d0ea; names ours) */
	float CalcDistLineToLine(const math::v4 &point_a, const math::v4 &dir_a,
				const math::v4 &point_b, const math::v4 &dir_b,
				math::v4 *closest_a, math::v4 *closest_b);
	/* where its hit box's center is, and its hit box's size (with opt 1 the target of a
	   running size animation) */
	math::v4 GetHitBoxCenter();
	math::v4 GetHitBoxSize(int32_t opt);
	/* the layout of the resolution taken at once, its children's too where children
	   (0x8114f9c2; names ours) */
	int32_t ChangeResolution_core(uint32_t id, bool children, bool update);
	/* the world matrix made again from its position, scale and rotation and its parent's,
	   returned */
	const math::matrix &UpdateMatrix();
	/* its matrix and its descendants' made again (0x811553b0 TOOL; name ours) */
	void UpdateMatrixTree();

	/* the world matrix, made again first where m_update_flag asks for it (inline in Sony's
	   callers, name ours) */
	const math::matrix &GetWorldMatrix()
	{
		if ((m_update_flag & 7) != 0)
			UpdateMatrix();
		return m_world_matrix;
	}

	/* the world rotation, made again first where m_update_flag asks for it (inline in Sony's
	   callers, name ours) */
	const math::quaternion &GetWorldRot()
	{
		if ((m_update_flag & 7) != 0)
			UpdateMatrix();
		return m_world_rot;
	}

	/* the world matrix moved back to its origin, where it has one (Sony's copy out of line,
	   0x812353b4; name ours) */
	__attribute__((noinline)) math::matrix GetOriginWorldMatrix()
	{
		math::v4 offs = m_origin_offs;

		if ((m_origin_x | m_origin_y | m_origin_z) == 0 &&
		    offs == math::v4(0.0f, 0.0f, 0.0f, 0.0f))
			return GetWorldMatrix();

		math::v4 offset(-offs.getImpl());

		offset.set_w(1.0f);
		return GetWorldMatrix() * math::matrix(math::v4(1.0f, 0.0f, 0.0f, 0.0f),
						       math::v4(0.0f, 1.0f, 0.0f, 0.0f),
						       math::v4(0.0f, 0.0f, 1.0f, 0.0f), offset);
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
	/* turned to face the point, the second look-at point (or the z axis) up (0x81154f18) */
	int32_t LookAt(const math::v4 &target, const math::v4 &up);
	/* the same with the points as the widgets' positions, their offsets given */
	int32_t LookAt(Widget *target, const math::v4 &target_offs, Widget *up, const math::v4 &up_offs);
	/* turned to the facing and up vectors, in a reference widget's rotation where it is given
	   (0x81155072) */
	int32_t SetPosture(const math::v4 &facing, const math::v4 &up, Widget *ref);
	/* the rotation that faces the first vector with the second up, made into the quaternion
	   (0x81154cee; name ours) */
	static void MakeLookAtQuat(math::v4 *facing, math::v4 *up, math::quaternion *quat);
	/* the rectangle it clips its children to */
	math::rect CalcClipRect();
	math::v4 GetAlignOffsetToCenter(int32_t opt);
	/* the same turned by its local matrix, as UpdateMatrix made it (0x8114c9c0) */
	math::v4 GetAlignOffsetToRotatedCenter(int32_t opt);
	Camera *GetCamera() const;

	int32_t SetPos(const math::v4 &pos, Timer *t = NULL, int32_t id = ANIM_MOVE,
		       int32_t opt = 0, float delay = 0.0f, AnimCB cb_fin = NULL,
		       void *usrdata = NULL);
	/* the values for each resolution, in the array */
	int32_t SetPos(const ResolutionData *pos, uint32_t num, Timer *t, int32_t id, int32_t opt,
		       float delay, AnimCB cb_fin, void *usrdata);
	/* the alpha it is shown with, kept within 0 and 1, set at once (0x8114d9f0; name ours,
	   after SetShowAlpha) */
	void SetShowAlpha_core(float alpha);
	/* its show alpha multiplied up from its parent's, and its children's after it (0x8114d9ac;
	   name ours) */
	void UpdateShowAlphaSum(float parent_sum);
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
	int32_t SetOriginOffset(const math::v4 &offs, Timer *t, int32_t id, int32_t opt, float delay,
				AnimCB cb_fin, void *usrdata);
	int32_t SetAnchorOffset(const math::v4 &offs, Timer *t, int32_t id, int32_t opt, float delay,
				AnimCB cb_fin, void *usrdata);
	int32_t SetAlign(uint32_t ax, uint32_t ay, uint32_t az, Timer *t);
	int32_t SetAdjust(uint32_t ax, uint32_t ay, uint32_t az);
	int32_t SetPosCenter(const math::v4 &v);
	/* the values of a resolution array kept (copied; num entries), the first three set at once
	   by SetPos, SetSize and SetRot of an array (0x81153bfe, 0x8115446a, 0x81154908) */
	/* the resolution it is in (id), the one it comes from (pre; -1 for none), and how far the
	   change has run (the timer's time, its mapped value; 1 and 1 without one); whether there is
	   a change (0x81162c50; names ours) */
	bool IsMorphResolution(uint32_t &id, uint32_t &pre, float &time, float &rate);
	/* the draw objects drawn to a surface or not, and whether that changed anything (0x81152bdc;
	   name ours) */
	bool UpdateSurfaceDraw(bool on);
	/* what its parent passes on taken over, and its children's after it: input, surface draw,
	   transition and destroy protection flags, the layer, and the parent's show and meta alpha
	   sums (0x81152c60; name ours) */
	void ApplyParentState(bool input_target, bool surface_draw, bool transition_in,
			      bool destroy_protected, uint32_t layer, float parent_show_sum,
			      float parent_meta_sum);
	void SetResolutionPos_core(const ResolutionData *pos, int32_t num);
	void SetResolutionSize_core(const ResolutionData *size, int32_t num);
	void SetResolutionRot_core(const ResolutionData *rot, int32_t num);
	/* SetPos with the tangents the position goes through as a hermite curve (the start's and
	   the target's, or NULL for none) (0x8114f80a; name ours) */
	int32_t SetPosHermite(const math::v4 &pos, const math::v4 *const *tangents, Timer *t,
			      int32_t id, int32_t opt, float delay, AnimCB cb_fin, void *usrdata);
	/* puts the widget behind the other in its parent's draw order */
	int32_t SetBehind(Widget *w);
	/* puts the widget in front of the other in its parent's draw order */
	int32_t SetForward(Widget *w);
	/* moves the widget under the parent, before the sibling (NULL: last), queued on
	   s_list_restruct for the next update; -1 for no parent or while it is held (name Sony's,
	   the parameters ours) */
	int32_t SetParent(Widget *parent, Widget *sibling, int32_t option);
	/* the widget made a child of the parent at once, before the sibling (NULL: last) (0x8114fd9c) */
	int32_t SetParent_core(Widget *parent, Widget *sibling);
	/* the texture the source gives set on the draw object, the events 0x100005 (set) and 0x100006
	   (not) sent; 0x100007 too where the source holds no more; the request let go of when it is
	   set or the source gives up (0x80af5022); the result of the source, -1 without an
	   environment or for a source that is no texture (0x81160596; name ours) */
	int32_t OnTextureSource(const intrusive_ptr<TextureSource> *source, int32_t obj, int32_t bit);
	/* it taken out of its scene's layer count and surface list, and of the debug focus
	   (0x8115164a; name ours) */
	void detach_scene();
	/* moves within the parent's children now, to behind or in front of the widget, a step, or
	   to the back or the front (0x81153030 to 0x81153192) */
	int32_t SetBehind_core(Widget *w);
	int32_t SetForward_core(Widget *w);
	int32_t SetBehindOneStep_core();
	int32_t SetForwardOneStep_core();
	int32_t SetBehindMost_core();
	int32_t SetForwardMost_core();
	/* the moves SetParent, SetBehind and SetForward kept (name Sony's) */
	static list<RestructInfo> s_list_restruct;
	/* the moves kept made, the layer weights of the scenes they were in made again (0x8115605a) */
	static void UpdateRestruct();
	/* the widgets waiting to be destroyed, with their children, each frame (0x81155f1e) */
	int32_t UpdateDestroy();
	/* the widget and its children updated each frame (0x81155cc8) */
	void OnUpdate();
	/* the resolution timers run, the layout morphed to the next resolution (0x81155696) */
	void UpdateResolutionTimer();
	/* whether an animation with the ID is running in it or a widget above it (0x811536c2; name
	   ours) */
	bool IsAnimRunning(int32_t id);
	/* the animations it runs given a tick (0x811553e4; name ours) */
	void UpdateAnims();
	/* what it holds back for a show let go of, in it and (where recursive) in its children
	   (0x81152394; name ours) */
	void ReleaseShowHolds(bool recursive);
	/* the show timer run each update, and taken out once it is out (0x81152554; name ours) */
	void UpdateShowTimer();
	void OnUpdatePost();

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
	/* the widget where it is one of its descendants, else NULL */
	Widget *FindChild(Widget *child, int32_t option);
	/* the first child of the type the check takes (NULL for any), searched in its children
	   only (mode 2) or below them too; mode 0 returns the one found deepest, else the child
	   it is under (0x81152902 TOOL; name after FindChild) */
	Widget *FindChild(const char *type, int32_t mode, bool (*check)(Widget *));
	/* the focus taken off it: -1 where it has none */
	int32_t ReleaseFocus();
	/* the target made the widget its focus root holds the focus with (NULL: none), and the
	   environment told (0x81150b6c; name ours, the flag meaning open) */
	int32_t SetFocusRootFocus(Widget *target, bool notify);
	/* touch and key input may reach it and, set the same, its descendants (0x8124267c; name
	   ours) */
	void SetInputTarget(bool on);
	/* whether the focus may be given to it: the cursor's widget and its focus root's focused
	   widget are not held (as DB0FFD33 says) and its own focus root has no move pending (0x81155196;
	   name ours) */
	bool IsFocusFree();
	/* the best widget among its children and theirs for the focus search; the search's
	   m_distance gets its score (0x81160ba4; name ours) */
	Widget *SearchFocusChildren(FocusSearchInfo *info, Widget *skip);
	/* the best widget for the focus search from it: the one its hint names, its children and
	   its focus root's (mode 0 and 1), then up through its parent or, with none, the other
	   scenes of its entry (mode 0 and above 1); the search's m_distance gets its score
	   (0x81160d82; name ours) */
	Widget *SearchFocus(FocusSearchInfo *info, Widget *from, uint32_t mode);
	/* a touch or key may reach it: an input target, shown and in no transition, and not
	   destroy protected (inline in Sony's code: ButtonBase's press, TouchScrollSupplier's rear
	   mochi gesture; name ours) */
	bool IsInputTarget()
	{
		return m_input_target && m_show_a_sum >= 0.95f && !m_transition_in &&
		       !m_transition_out && !m_destroy_protected;
	}
	/* the child at the index in m_children */
	Widget *GetChild(int32_t index) const;
	AnimQueue *FindAnim(int32_t id);
	/* the count of children that can take the focus changed by delta, up to the scene (0x81152ac0) */
	void CountFocusEnableChildren(int32_t delta);
	/* the children not ready to draw counted, up to the scene; at 0 the event 0x1008 is queued
	   (0x8114c5e2) */
	void CountPrepareChildren(int32_t delta);
	int32_t KillAnimType(int32_t type);
	/* the animation's listeners stopped where they are */
	int32_t KillAnim(int32_t id);
	/* the listener queued for the animation */
	int32_t SetAnimListener(int32_t id, AnimListener *listener, int32_t slot);
	/* the same, the animation starting after the delay */
	int32_t SetAnimListener_withDelay(int32_t id, AnimListener *listener, float delay,
					  int32_t slot);

	ResolutionData *get_resolution_data(ResolutionData *array, uint32_t num, uint32_t id) const;
	/* the entries of the array for the two resolutions (or their bases), 0 where both are found
	   (0x81162cd2) */
	int32_t get_resolution_data2(const ResolutionData *array, uint32_t num, uint32_t id_a,
				     uint32_t id_b, const ResolutionData **a,
				     const ResolutionData **b) const;

	math::v4 m_pos;
	math::v4 m_scale;
	math::v4 m_size;
	math::v4 m_rot;
	/* its rotation combined with its parents', as UpdateMatrix made it (name ours) */
	math::quaternion m_world_rot;
	/* its matrix in its parent's space, which UpdateMatrix multiplies by the parent's (name
	   ours) */
	math::matrix m_local_matrix;
	/* its world matrix, as UpdateMatrix made it (name ours) */
	math::matrix m_world_matrix;
	math::v4 m_align_offs;
	math::v4 m_anchor_offs;
	math::v4 m_origin_offs;
	/* the matrix GetPos takes a position to the widget's space with, made when another widget asks
	   (name ours) */
	math::matrix *m_inverse_matrix;
	uint8_t reserved2a[4];
	/* the rotation LookAt and SetPosture make, from a facing and an up vector (name ours) */
	math::quaternion m_quat;
	IDParam m_name;
	uint32_t m_state;
	void *m_user_data[2];
	UserData *m_user_data2[2];
	Environment *m_env;
	Widget *m_parent;
	Scene *m_scene;
	list<Widget *> m_children;
	/* the pad port that holds its direction keys (capture 0x17c) and its other keys (0x17d),
	   negative for none (names ours): the signs are the bits AppIconBase::OnDraw reads, which
	   dims an icon held in its mode 1 with both set (names from that use) */
	union {
		int8_t m_pad_dir_port;
		struct {
			uint8_t reserved3b0 : 7;
			bool m_dim_held : 1;
		};
	};
	union {
		int8_t m_pad_button_port;
		struct {
			uint8_t reserved3c0 : 7;
			bool m_dim_held_enable : 1;
		};
	};
	/* its children not ready to draw yet, as CountPrepareChildren counted them; a box lays
	   out only at 0 (name ours, after CountPrepareChildren) */
	uint16_t m_prepare_count;
	/* nonzero: a scroll view takes the focus while it has none (ScrollViewBase's focus update;
	   name ours, meaning open) */
	uint16_t m_focus_count;
	/* the children below it (and itself) that take the rear touch event, counted up to the scene
	   (EnableTouchRearEvent; name ours) */
	uint8_t m_touch_rear_children;
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
	/* the layer it had before the last ApplyParentState (name ours) */
	uint32_t m_layer_old : 4;
	/* how it is modal (export 42C5A4D4): 0 or 1 as Environment::RegisterModalWidget's mode, 2
	   not modal; and the option given with it (names ours) */
	uint32_t m_modal_mode : 4;
	uint32_t m_modal_option : 4;
	/* the show timers and holds it has waiting, counted (ScePafWidget AC0C81D5 adds one, A96A0B4A
	   adds or takes off; name ours) */
	uint32_t m_show_hold : 4;
	/* the texture requests it holds with the environment, counted (SetDrawObjMaskBit;
	   detach_environment takes them out; name ours) */
	uint32_t m_texture_request_count : 4;
	/* how the camera clips it and its children (1 the clip stack; 2 and 3 the scissor
	   snapped inward and to the nearest pixel, else outward), and where it is drawn (1
	   into a surface of its scene's list) (Camera's draw walk; names ours) */
	uint32_t m_clip_mode : 4;
	uint32_t reserved3x3 : 4;
	uint32_t m_draw_target : 4;
	/* the axes it takes positions in the opposite way (GetPos; names ours) */
	uint32_t m_flip_x : 1;
	uint32_t m_flip_y : 1;
	uint32_t m_flip_z : 1;
	uint32_t reserved3x4 : 1;
	/* what the touch event slots set (names after them) */
	bool m_touch_front_event : 1;
	bool m_touch_rear_event : 1;
	/* the focus cursor is on it: SetCursorOn sets it with the focus event 0x401 and clears it
	   with 0x402 (Text::UpdateSize lays its focused link out as the cursor; name ours) */
	bool m_cursor_on : 1;
	/* it holds the focus (SetFocus sets it, ReleaseFocus and the environment clear it; name ours) */
	bool m_focused : 1;
	/* the focus moves to it within its group (ButtonBaseStyleParam's "focus_in_group"; name
	   after it) */
	bool m_focus_in_group : 1;
	/* events are dispatched to it first, before its descendants (EventExecDispatch; name ours) */
	bool m_pre_dispatch : 1;
	bool reserved3y5 : 1;
	/* a list view's drag has begun (ListView::BeginDrag sets it; name ours) */
	bool m_drag_begun : 1;
	/* none of its focus directions (name ours) */
	bool m_focus_direction_off : 1;
	/* the focus cursor is not drawn on it while it rests there (FocusInfo::Update; name ours) */
	bool m_cursor_hidden : 1;
	/* the focus code takes it as a focus target (Widget::HandleFocusSwitchEvent; name ours) */
	bool m_focus_target : 1;
	/* a scene or camera, which the touch walk passes over (name ours) */
	bool m_not_hit_target : 1;
	/* a composite button takes its focus switch events itself (CompositeButton's ctor and
	   EnableFocusEvent; name ours) */
	bool m_composite_focus : 1;
	/* a scroll view searches the focus from it (ScrollViewBase::SearchFocus; name ours) */
	bool m_focus_searching : 1;
	/* a list item (ListItem's ctor; name ours) */
	bool m_list_item : 1;
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
	/* touch and key input may reach it (ButtonBase's press, ScrollViewBase's rear mochi
	   gesture; name ours) */
	bool m_input_target : 1;
	/* its draw objects draw to a surface (name after SurfaceDrawParam's) */
	bool m_surface_draw : 1;
	/* registered with its environment as a touch device region (A1193AAB; names ours) */
	bool m_touch_region_registered : 1;
	bool m_touch_region_option : 1;
	/* taps are taken (AppIconSimple::EnableTap; AppIconBase sets it; name after the export) */
	bool m_enable_tap : 1;
	/* animation listeners set on it are kept on ScePafWidget_8A0DA7BF until it takes them
	   (SetAnimListener; name ours, the setter not found) */
	bool m_anim_deferred : 1;
	/* a timer for it is waiting in the widget timers' map (SetTransitionIn and its like; name ours) */
	bool m_timer_pending : 1;
	/* not destroyed when its page closes (name ours) */
	bool m_destroy_protected : 1;
	/* its transitions in and out are under way: it takes no input and its timers wait
	   (Widget::OnUpdate, ScePafWidget 5A9793B0; names ours) */
	bool m_transition_in : 1;
	bool m_transition_out : 1;
	/* m_rot holds the Euler angles the rotation was set with (SetRot_core sets it, SetQuat_core
	   clears it; name ours) */
	bool m_rot_euler : 1;
	/* outside the camera's view, not drawn (Camera::add_obj; name ours) */
	bool m_culled : 1;
	/* what SetFitToScene sets */
	bool m_fit_to_scene : 1;
	/* drawn by the camera also when it is culled (name ours) */
	bool m_draw_always : 1;
	/* it is in its environment's list of ordered widgets (ScePafWidget 0253DC9C; name ours) */
	bool m_ordered : 1;
	/* a timer for it is waiting in the show timers' map (name ours) */
	bool m_show_timer_pending : 1;
	bool m_bound_keycode : 1;
	/* the current widget of its widget group (Environment::UpdateWidgetGroups; name ours) */
	bool m_group_current : 1;
	/* it takes the keys its m_keycode binds as pad captures (HandleKeycodeEvent; name ours, meaning open) */
	bool m_keycode_capture : 1;
	/* cleared before a reverse transition (name after ScePaf-RE) */
	bool m_transition_complete : 1;
	/* set where UpdateResolutionTimer morphs the layout and cleared where it ends (name ours) */
	bool m_resolution_morph : 1;
	/* the pad taps are counted at the press (apply_pad_gesture_tap gives it to the listener;
	   name ours) */
	bool m_pad_tap_on_press : 1;
	uint8_t reserved3p2 : 6;
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
	/* where it is in its parent, its size and a pixel more, as a rich text keeps it after its
	   layout (made by RichText::Update; no other user found; name ours) */
	math::rect *m_layout_rect;
	Timer *m_resolution_timer;
	Timer *m_resolution_timer_next;
	ResolutionData *m_pos_array;
	ResolutionData *m_size_array;
	ResolutionData *m_rot_array;
	uint8_t m_resolution_id;
	uint8_t m_resolution_id_pre;
	/* the resolution it changes to over m_resolution_timer_next, 0xff for none (name ours) */
	uint8_t m_resolution_id_next;
	/* the entries of m_pos_array */
	uint8_t m_pos_num;
	/* the entries of m_size_array and m_rot_array (names ours) */
	uint8_t m_size_num;
	uint8_t m_rot_num;
	uint8_t reserved5[2];
	/* the touches it holds, by port << 8 | touch ID (name ours) */
	map<uint32_t, TouchCapture *> m_touch_captures;
	/* the drag states of the touches dragging it, the first and the second; the second
	   becomes the first when the first is released (0x8114e258; names ours) */
	DragState *m_drag_state[2];
	/* its pinch by the two touches dragging it, the Environment's (cleared when the second drag
	   ends; name ours) */
	PinchState *m_pinch_state;
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
	uint32_t m_hit_check : 8;
	/* what SetDragMode sets */
	uint32_t m_drag_mode : 16;
	uint32_t reserved5a : 8;
	/* what SetPinchMode sets (0x400: the pinch limits are the widget's size) */
	uint16_t m_pinch_mode;
	/* the drag mode kept while it is dragged by a touch, -1 for none (name ours) */
	int16_t m_saved_drag_mode;
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
	/* the steps its drag moves it in, as a scene's layout debug mode sets them (Scene's
	   m_debug_quant_x, y and z; names ours) */
	uint8_t m_drag_quant_x;
	uint8_t m_drag_quant_y;
	uint8_t m_drag_quant_z;
	uint8_t reserved7;
	/* the bounds its drag keeps it in, its smallest and largest position (or, pinched, its
	   scale), made by SetDragMode (names ours) */
	SceFVector3 *m_drag_limit;
	/* the smallest and the largest scale its pinch keeps it in, made by SetPinchMode (name ours) */
	SceFVector3 *m_pinch_limit;
	/* how it is sized to its contents where it adjusts, shared by reference count; NULL for
	   the default, ScePafWidget_BE1CEA2E (name ours) */
	AdjustParam *m_adjust_param;
	/* lays the widget out each update, and animates it (names after ScePaf-RE) */
	Layouter *m_layouter;
	Animator *m_animator;
	/* its focus state, its focus cursor and the focus of its children (names ours) */
	Focuser *m_focuser;
	FocusShape *m_focus_shape;
	/* the shapes the cursor is drawn as where its shape uses parts: a text's focused link,
	   a shape per line (name ours) */
	vector<FocusShape> m_focus_parts;
	FocusRoot *m_focus_root;
	/* the widgets whose focus directions name this one, made by the first */
	FocusReferrers *m_focus_referrers;
	/* the layout changes it tells its parent's layouter of (NotifyLayoutDirty): the low byte
	   the kinds, the high byte the ones it passes on; a scroll view's child passes 0x300 on
	   (ScrollView::OnChildRegistered; name ours) */
	uint16_t m_layout_notify_mask;
	/* the same for the animator (NotifyAnimatorDirty; name ours) */
	uint16_t m_animator_notify_mask;
	/* the animations running, by their ID (the ID's low 24 bits; name after ScePaf-RE) */
	map<int32_t, AnimQueue *> m_anims;
	/* its draw objects, drawn in order */
	vector<DrawObjEntry> m_drawobjs;
	/* what is called when it is deleted (names after DrawObj's, Sony's DWARF) */
	vector<DeleteCBInfo> m_list_delete_cb;
	/* asked, when the widget is destroyed with its parent, for a widget to move it under instead
	   (NULL: none), with its argument; the focus cursor's goes under the cursor parent (names
	   ours) */
	Widget *(*m_keep_cb)(Widget *widget, void *arg);
	void *m_keep_arg;
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

PAF_LIST_INLINE_DTOR(ui::WidgetParam::IndexedRef)

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
/* the widget given a focus root of its own, the focus taken from the root above it
   (no name found) */
/* how many touches of the port the widget holds (no name found) */
int32_t ScePafWidget_54DFB062(paf::ui::Widget *widget, int32_t port);
/* where it is drawn set (1: into a surface of its scene's list), its draw objects told (no name
   found) */
void ScePafWidget_CB95F8F2(paf::ui::Widget *widget, int32_t target);
/* the keys of the mask taken off its key binding (no name found) */
int32_t ScePafWidget_164BFFA5(paf::ui::Widget *widget, uint32_t mask);
/* the sound of the index played through the environment's sound callback: -1 where it has none
   or no callback (the widget's sound mask; no name found) */
int32_t ScePafWidget_2B1A1493(paf::ui::Widget *widget, int32_t index);
int32_t ScePafWidget_00AA6519(paf::ui::Widget *widget);
/* the widget the focuser's direction widgets name for the direction (0 up, 1 down, 2 left,
   3 right; up for any other), NULL where it has no widget or none (no name found) */
paf::ui::Widget *ScePafWidget_71349B04(paf::ui::Focuser *focuser, int32_t dir);
/* the widget added to the shared list the file of AnimQueue keeps, whose object (0x2b0 bytes)
   is made on first use, with a delete callback dropping it again (no name found) */
int32_t ScePafWidget_82A61BBC(paf::ui::Widget *widget);
/* puts the widget into its transition: auto_delete deletes it once the time ends */
int32_t ScePafWidget_8AF26B1F(paf::ui::Widget *widget, bool auto_delete, float time);
/* the widget holds the touch of the port and ID, dragged in the mode: 0 once held, -1 if not */
int32_t ScePafWidget_DE592034(paf::ui::Widget *widget, uint32_t port, uint32_t id,
			      uint32_t drag_mode);
/* the widget lets go of the touch of the port and ID it holds, its release handled when
   release is set: -1 if it does not hold it */
int32_t ScePafWidget_093F1C89(paf::ui::Widget *widget, uint32_t port, uint32_t id, bool release);
/* the drag of the touch the capture holds: begun where the widget has none and drags, made its
   second (a pinch) where it has one and pinches, or found again, its hit taken from start or
   (NULL) found again on the touch's line; NULL where none is given (no name found) */
paf::ui::DragState *ScePafWidget_9697762E(paf::ui::Widget *widget, paf::ui::TouchCapture *capture,
					  const paf::ui::HitInfo *start);
/* captures the touch of the port, its gestures applied (names ours) */
int32_t ScePafWidget_D3D0A7C3(paf::ui::Widget *widget, uint32_t port, uint32_t id, bool apply,
			      bool reset_pad);
/* lets go of every touch it holds */
int32_t ScePafWidget_931AD41E(paf::ui::Widget *widget, bool release);
/* lets go of the touch of the port and ID it holds, its release handled when release is set:
   -1 if it does not hold it */
int32_t ScePafWidget_5ABB3393(paf::ui::Widget *widget, uint32_t port, uint32_t id, bool release);
/* takes the widget off the list of widgets (0x81315084 in DEBUGGER) it was put on (no name
   found) */
void ScePafWidget_A89BE22F(paf::ui::Widget *widget);
/* the vblank waits counted up and down, with paf::graph::s_wait_vblank_count (state.c; no
   name found) */
uint32_t *ScePafWidget_10BEC7CF(uint32_t *count);
uint32_t *ScePafWidget_DB993F51(uint32_t *count);
/* prototypes from common_gui_dialog's calls, not written in C yet (no name found) */
/* adds delta to the count of its children (itself included) that take the rear touch event, up
   to the scene (0x81152a82; no name found) */
int32_t ScePafWidget_D4759650(paf::ui::Widget *widget, int32_t delta);
/* the widget's draw objects and its children's drawn to a surface (on) or not (0x811538d0; no
   name found) */
void ScePafWidget_66847A11(paf::ui::Widget *widget, bool on);
/* the widget's adjust parameter let go, its size updated where update (0x81150600; no name
   found) */
void ScePafWidget_622193BD(paf::ui::Widget *widget, bool update);
/* the widget registered with its environment as a touch device region (on) or taken off it; the
   option kept (0x81162ab8; no name found) */
int32_t ScePafWidget_A1193AAB(paf::ui::Widget *widget, bool on, bool option);
/* plays its sound of the index where its sound mask has it: -1 when none is played */
int32_t ScePafWidget_2B1A1493(paf::ui::Widget *widget, int32_t index);
/* sets the style attribute of its text from pos for len characters, its size where attr is
   TextStyleAttribute_Point, laying the text out again where that changes it; NULL text
   object 0x80AF0022 (argument names ours, after TextObj::SetStyleAttribute) */
int32_t scePafWidgetSetFontSize(paf::ui::Widget *widget, float size,
				paf::graph::TextStyleAttribute attr, uint32_t pos, uint32_t len);
/* the hit box size for the size: its height at least the minimum (ImageSlideBar's; no name
   found) */
paf::math::v4 ScePafWidget_301AA80F(paf::ui::Widget *widget, const paf::math::v4 &size);
/* binds the keys of the mask to the widget (SetKeycode after ScePaf-RE) */
int32_t ScePafWidget_21125870(paf::ui::Widget *widget, uint32_t keycode);
/* gives the widget its focus cursor: a copy of the shape, its offset, size and rotation
   taken from the widget's own where the shape has none */
void ScePafWidget_13B246E0(paf::ui::Widget *widget, paf::ui::FocusShape *shape);
/* makes the widget (on) the default focus of the nearest ancestor with a focus root at 0x234, or
   stops it being that (off) (0x8114fcfa TOOL; no name found) */
void ScePafWidget_EC6175D6(paf::ui::Widget *widget, bool on);
/* the drag state of the widget made again for the position (name ours) */
/* the drag state's touch state filled from the hit line of the touch (no name found) */
int32_t ScePafWidget_0E453608(paf::ui::Widget *widget, paf::ui::DragState *drag,
			      paf::ui::TouchInfo *touch);
/* the plane the drag moves the widget on, for drag modes 272 (its parent's xy plane through the
   hit), 273 (the screen's, at the hit's depth) and 274 (facing the camera through the hit) (no
   name found) */
int32_t ScePafWidget_4E47E55A(paf::ui::Widget *widget, uint32_t mode, paf::ui::DragState *drag);
/* the axis the drag moves the widget along, for drag modes 288 to 293: its parent's x, y or z
   axis through the hit, or the world's (no name found) */
int32_t ScePafWidget_E322D1F6(paf::ui::Widget *widget, uint32_t mode, paf::ui::DragState *drag);
/* the drag's plane made its parent's xy plane through the second drag's hit (no name found) */
int32_t ScePafWidget_BF92FEF0(paf::ui::Widget *widget, uint32_t mode, paf::ui::DragState *drag,
			      paf::ui::DragState *second);
/* the widget sized by the drag of its edge (drag modes 513 to 518): the touch's line taken again
   through the camera and hit on the drag's plane, the size grown by the way the touch moved,
   stepped by the drag quanta and kept within the drag limits; 1 where the limits held it (no
   name found) */
int32_t ScePafWidget_1E0A2904(paf::ui::Widget *widget, paf::ui::DragState *drag);
/* nothing to do (focuser.cpp; no name found) */
void ScePafWidget_8A132DF7(paf::ui::Event *e, paf::ui::Widget *widget, bool on);
void ScePafWidget_727885B3(paf::ui::Event *e);
/* the widget pinched by its two drags: its size scaled by how far apart the touches are and kept
   within the pinch limits, turned with them where the pinch mode has 0x400 or 0x800, and moved
   to keep the touches on the same points within the drag limits; 1 where the drag limits held
   it, what they held back put in overflow (no name found) */
int32_t ScePafWidget_8BDEB047(paf::ui::Widget *widget, paf::ui::DragState *first,
			      paf::ui::DragState *second, paf::math::v4 *overflow);
/* the widget moved by its drag's touch, on the drag's plane (drag modes 272 to 274) or along its
   axis (288 to 293), stepped by the drag quanta and kept within the drag limits, rate of the way
   there at once: 1 where the limits held it, what they held back put in overflow, -1 for no
   touch, camera or hit (no name found) */
int32_t ScePafWidget_E17C2B53(paf::ui::Widget *widget, paf::ui::DragState *drag,
			      paf::math::v4 *overflow, float rate);
/* the layout of the resolution taken over the timer's time, at once without one, its
   children's too where children */
int32_t ScePafWidget_11D537AF(paf::ui::Widget *widget, uint32_t id, paf::Timer *t,
			      bool children);
/* Hide(Timer *, int32_t opt, float delay, AnimCB): hides the widget, over the timer's time */
int32_t ScePafWidget_4588699E(paf::ui::Widget *widget, paf::Timer *t, int32_t opt, float delay,
			      paf::ui::AnimCB cb_fin);
/* Show(Timer *, int32_t opt, float delay, AnimCB): shows the widget, over the timer's time */
int32_t ScePafWidget_FF5BC957(paf::ui::Widget *widget, paf::Timer *t, int32_t opt, float delay,
			      paf::ui::AnimCB cb_fin);
/* SetPosHermite with the start's and the target's tangents given apart (no name found) */
int32_t ScePafWidget_026C9E8C(paf::ui::Widget *widget, const paf::math::v4 &pos,
			      const paf::math::v4 *start, const paf::math::v4 *target,
			      paf::Timer *t, int32_t id, int32_t opt, float delay,
			      paf::ui::AnimCB cb_fin, void *usrdata);
/* the widget's transition ends after the time */
int32_t ScePafWidget_AC0C81D5(paf::ui::Widget *widget, float time);
/* the widget's transition ends after the time, and the widget is deleted */
int32_t ScePafWidget_5A9793B0(paf::ui::Widget *widget, float time);
/* the widget (and its children) set in a transition in or out, and let go of the timer waiting for
   it (no name found) */
void ScePafWidget_C7962202(paf::ui::Widget *widget, bool on, bool force);
/* the same for the destroy protection (no name found) */
void ScePafWidget_83214DB9(paf::ui::Widget *widget, bool on, bool force);
/* the widget made a focus root for its children, the focus of the root above moving to it
   (no name found) */
int32_t ScePafWidget_00AA6519(paf::ui::Widget *widget);
/* its focus root taken away again, the focus released (no name found) */
int32_t ScePafWidget_929C3278(paf::ui::Widget *widget);
/* it put in or taken out of its environment's list of ordered widgets, made a focus root first
   (no name found) */
int32_t ScePafWidget_0253DC9C(paf::ui::Widget *widget, bool on);
/* the timer waiting for the widget run out: the transition ended, or the widget destroyed (no
   name found) */
void ScePafWidget_78C4DBA4(paf::ui::Widget *widget);
/* every destroy timer run (no name found) */
void ScePafWidget_CEA2BE4E(void);
/* the frame's end: the panel's edge correction read, and the draw objects SwapDrawObj took out
   let go (no name found) */
void ScePafWidget_D2E7815D(void);
/* adds to the count kept in bits 24 to 27 of the widget's flags (the first hold lets go of the
   show holds first) */
void ScePafWidget_A96A0B4A(paf::ui::Widget *widget, int32_t add);
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
/* the touch's hit kept as port and ID's, and where it moved off one widget onto another (or
   onto none), the leave event put in the routed events for the old widget and the enter
   event, or the press, at the front of the queue for the new one: 1 where it moved, 0 where it
   did not, 2 where there is none, -1 without an environment (0x81158040; no name found) */
int32_t ScePafWidget_08CE1C4B(paf::ui::Widget *widget, paf::ui::Event *e, uint32_t port,
			      uint32_t id, paf::ui::HitInfo *hit);
/* the event sent to the widget and on, in the widget's routing (0x8116158e; no name found) */
int32_t ScePafWidget_EB1D1CBC(paf::ui::Widget *widget, paf::ui::Event *e);
/* the touch event taken by the widget's gestures (0x8116132e; no name found) */
int32_t ScePafWidget_38CDFD1D(paf::ui::Widget *widget, paf::ui::Event *e);
/* the first widget of the nearest focus root at or above the widget, up to its scene, NULL for
   none (0x81150cac; name ours) */
paf::ui::Widget *FindRootFocus(paf::ui::Widget *widget);
/* the widget is shown, so the focus may go to it (no name found) */
bool ScePafWidget_DB0FFD33(paf::ui::Widget *widget);
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

PAF_VECTOR_INLINE_DTOR(ui::ResolutionData)
PAF_VECTOR_INLINE_DTOR(ui::Widget::DeleteCBInfo)

#endif
