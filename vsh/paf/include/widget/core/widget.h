/*
 * paf::ui::Widget, as far as converted C++ uses it: a node of the widget tree,
 * with its position, size and rotation, the animations that move them and the
 * objects it draws.
 *
 * Names after ScePaf-RE (restored from Sony's DWARF), the virtual functions in
 * its order, which is the order of Widget's vtable; the functions after
 * GetDrawObj are not declared yet. Where no name is known the name says what
 * the function does (namecheck lists them).
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
#include <math/ngp/quaternion.h>
#include <versatileparam/idparam.h>
#include <versatileparam/versatileparam.h>
#include <widget/core/handler.h>
#include <widget/core/layouter.h>

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
class Camera;
class AnimListener;
class AnimQueue;
typedef void (*AnimCB)(Timer *t, AnimListener *listener, void *data1, void *data2,
		       void *data3);

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
		const char *name;
		int32_t index;
		IDParam id;
	};

	static const char m_param_name[];
	static const char *s_names[];
	static const char *ParamName();

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

	WidgetLayoutParam()
		: m_widget(NULL), m_timer(NULL), m_pos_array(NULL), m_size_array(NULL),
		  m_rot_array(NULL)
	{
		Init();
	}

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
	/* a sound of the parameter: its element, its ID and its waveforms (names ours) */
	class Sound {
	public:
		const char *m_name;
		uint32_t reserved;
		IDParam m_id;
		/* -1 until played */
		int32_t m_handle;
		uint32_t reserved2;
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

		bool m_create_drawobj : 1;
		bool m_parent_protected : 1;
		bool m_under_template : 1;
		uint32_t reserved : 29;
	};

	/* the widgets being destroyed; Plugin::Exit waits for them */
	static int32_t s_destroying_count;

	virtual ~Widget();

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void DestroyWidget();
	virtual uint32_t CheckState();
	virtual uint32_t ChangeState(uint32_t state, int32_t ev_type);
	virtual int32_t SetEnvironment(Environment *env);
	virtual int32_t SetScene_core(Scene *scene);
	virtual bool RemoveChild(Widget *obj);
	virtual void RegisterChild(Widget *obj, Widget *sibling);
	virtual bool ReorderChild(Widget *obj, Widget *sibling);
	virtual Widget *GetInternalWidget();
	virtual int32_t DoTransition_core(float parameter, bool set);
	virtual int32_t DoTransitionReverse_core(float parameter, bool set);
	virtual int32_t SetColor_core(const math::v4 &v);
	virtual int32_t SetSize_core_force(const math::v4 &v, uint32_t notify_layout,
					   bool force);
	virtual void UpdateSize();
	/* Text keeps the flag only while it fits in its scene */
	virtual void SetFitToScene(bool on_off);
	virtual void Update();
	virtual void UpdateAfterChildren();
	virtual void UpdatePost();
	virtual int32_t OnDraw(graph::DrawInfo &info);
	/* Button hands its draw object the widget's state */
	virtual int32_t ApplyDrawObjState(int32_t arg);
	virtual void UpdateContent();
	virtual void UpdateGesture();
	virtual math::v4 calc_align_offset(const math::v4 &size, const math::v4 &aoffs,
					   uint32_t ax, uint32_t ay, uint32_t az);
	virtual int32_t SetOrigin(uint32_t ax, uint32_t ay, uint32_t az, Timer *t);
	virtual void GetOrigin(uint32_t &ax, uint32_t &ay, uint32_t &az);
	virtual math::v4 calc_origin_offset(const math::v4 &size, const math::v4 &offs,
					    uint32_t ax, uint32_t ay, uint32_t az);
	virtual int32_t SetAnchor(uint32_t ax, uint32_t ay, uint32_t az, Timer *t);
	virtual void GetAnchor(uint32_t &ax, uint32_t &ay, uint32_t &az);
	virtual math::v4 calc_anchor_offset(const math::v4 &size, const math::v4 &offs,
					    uint32_t ax, uint32_t ay, uint32_t az);
	/* the hit region at 0x210, made on first use */
	virtual void *GetHitRegion();
	virtual int32_t SetHitRegion(const math::v4 &region);
	virtual void UpdateLayoutAttribute();
	virtual void UpdateAdjustSize();
	virtual void UpdateAdjustChildSize(Widget *child);
	/* gives the surface to draw and returns the one drawn before (Plane, Canvas) */
	virtual intrusive_ptr<graph::Surface> SetSurface(const intrusive_ptr<graph::Surface> &surf,
							 int32_t obj);
	virtual graph::DrawObj *GetDrawObj(int32_t obj);
	/* the texture of the draw object (name after ScePaf-RE) */
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf);
	virtual int32_t GetTexture(intrusive_ptr<graph::Surface> &surf, int32_t obj, uint32_t index);
	virtual int32_t GetTexture(intrusive_ptr<graph::Surface> &surf);
	/*
	 * Three slots no source names (ScePafWidget C3D53C98, AC3B6830, FD48BD49): they set
	 * and read a bit of a draw object's 4-bit mask through the draw manager. Not
	 * converted; declared for the slots after them.
	 */
	virtual int32_t SetDrawObjMaskBit(const void *param, int32_t obj);
	virtual int32_t SetDrawObjMaskBit(const void *param, int32_t obj, uint32_t bit);
	virtual void *GetDrawObjMaskBit(int32_t obj);
	virtual int32_t SetString(const wstring &str, int32_t obj);
	virtual int32_t SetString(const wstring &str);
	virtual int32_t GetString(wstring &str, int32_t obj);
	virtual int32_t GetString(wstring &str);
	/* the sound of its sound parameter with the ID (names ours, the slots to SetFocusDirection) */
	virtual WidgetSoundParam::Sound *GetSound(int32_t id);
	virtual void DoNothing();
	virtual void SetFocusRootState(uint32_t state);
	virtual int32_t SetFocus(const void *param);
	/* the widget the focus moves to in the direction (0 up, 1 down, 2 left, 3 right) */
	virtual void SetFocusDirection(int32_t dir, IDParam id);
	virtual void SetFocusDirectionWidget(int32_t dir, Widget *widget);
	virtual void SetFocusSearchRect(const math::rect &rect);

	/* whether it is the widget its focus root focuses first (0x8114fd64, name ours) */
	bool IsInitFocus();

	math::v4 *GetSize(int32_t opt);
	math::v4 GetPos(int32_t opt, Widget *ref, const math::v4 &off_pos);
	math::v4 GetPosCenter(int32_t opt, Widget *ref, const math::v4 &off_pos);
	math::v4 GetRot(int32_t opt, Widget *ref, const math::v4 &off_rot, int32_t option);
	math::v4 GetAlignOffsetToCenter(int32_t opt);
	Camera *GetCamera() const;

	int32_t SetPos(const math::v4 &pos, Timer *t = NULL, int32_t id = ANIM_MOVE,
		       int32_t opt = 0, float delay = 0.0f, AnimCB cb_fin = NULL,
		       void *usrdata = NULL);
	/* the values for each resolution, in the array */
	int32_t SetPos(const ResolutionData *pos, uint32_t num, Timer *t, int32_t id, int32_t opt,
		       float delay, AnimCB cb_fin, void *usrdata);
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

	ResolutionData *get_resolution_data(ResolutionData *array, uint32_t num, uint32_t id);

	math::v4 m_pos;
	math::v4 m_scale;
	math::v4 m_size;
	math::v4 m_rot;
	uint8_t reserved[0x90];
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
	uint8_t reserved3[8];
	/* 0xff before a reverse transition (name after ScePaf-RE) */
	uint8_t m_transition;
	uint8_t reserved3a[3];
	/* the keys bound to it while m_bound_keycode (names after ScePaf-RE) */
	uint32_t m_keycode;
	uint8_t reserved3x[8];
	/* its parent writes it: WriteWidget writes only its children (names after ScePaf-RE) */
	bool m_parent_protected : 1;
	/* made by a template (TemplateOpen); WriteWidget leaves it out */
	bool m_under_template : 1;
	bool reserved3r : 1;
	bool reserved3s : 1;
	bool reserved3t : 1;
	bool reserved3u : 1;
	bool reserved3v : 1;
	bool reserved3w : 1;
	bool reserved3b : 1;
	bool reserved3c : 1;
	bool reserved3d : 1;
	/* not destroyed when its page closes (name ours) */
	bool m_destroy_protected : 1;
	bool reserved3e : 1;
	bool reserved3f : 1;
	bool reserved3g : 1;
	bool reserved3h : 1;
	bool reserved3i : 1;
	bool reserved3j : 1;
	bool reserved3k : 1;
	bool reserved3l : 1;
	bool m_bound_keycode : 1;
	bool reserved3n : 1;
	bool reserved3o : 1;
	/* cleared before a reverse transition (name after ScePaf-RE) */
	bool m_transition_complete : 1;
	uint8_t reserved3p[0xd];
	/* the alpha SetMetaAlpha sets (name ours) */
	float m_meta_alpha;
	uint8_t reserved3q[4];
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
	uint8_t reserved5[0x2a];
	uint32_t m_align_x : 4;
	uint32_t m_align_y : 4;
	uint32_t m_align_z : 4;
	uint32_t m_origin_x : 4;
	uint32_t m_origin_y : 4;
	uint32_t m_origin_z : 4;
	uint32_t reserved6 : 8;
	uint32_t reserved6a : 16;
	/* what SetAdjust sets (names after the resource's adjust_x and adjust_y) */
	uint32_t m_adjust_x : 4;
	uint32_t m_adjust_y : 4;
	uint32_t m_adjust_z : 4;
	uint32_t reserved6b : 4;
	uint8_t reserved7[0x10];
	/* lays the widget out each update (name after ScePaf-RE) */
	Layouter *m_layouter;
	uint8_t reserved7a[4];
	/* its focus state, its focus cursor and the focus of its children (names ours) */
	Focuser *m_focuser;
	FocusShape *m_focus_shape;
	uint8_t reserved7c[0x10];
	FocusRoot *m_focus_root;
	uint8_t reserved7d[0x24];
	/* what is called when it is deleted (names after DrawObj's, Sony's DWARF) */
	vector<DeleteCBInfo> m_list_delete_cb;
	uint8_t reserved7b[0xb];
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
}

#endif
