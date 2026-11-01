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
#include <std/list>
#include <std/memory>
#include <std/string>
#include <std/vector>
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

/* a widget's parameters as its resource gives them */
class WidgetParam : public VersatileParam {
public:
	uint8_t reserved[4];
	/* its widget's style and sound parameter IDs (names ours) */
	IDParam m_style_id;
	uint8_t reserved2[0x10];
	IDParam m_sound_id;
};

/* a widget type's style, read from the resource's "style_<type>" parameter */
class WidgetStyleParam : public VersatileParam {
public:
	virtual int32_t SetEach(Widget *widget);

	int32_t AddWidget(Widget *widget);

	uint8_t reserved[0x30];
	/* the ID of the style's sound parameter (name ours) */
	IDParam m_sound_id;
};

/* a widget type's handlers, read from its element (the factory table's handler factory) */
class WidgetHandlerParam : public VersatileParam {
public:
	int32_t AttachWidget(Widget *widget);
};

/* a widget type's layout, read from the "layout_" children of its element */
class WidgetLayoutParam : public VersatileParam {
public:
	/* makes room for num resolutions */
	int32_t InitResolution(int32_t num);
	/* takes the widget's position, size and rotation arrays (0x81064afc, name ours) */
	int32_t GetEach(Widget *widget);
	/* reads it for the widget */
	int32_t Attach(Widget *widget);
	/* the timer its Set animates with; NULL sets at once */
	int32_t SetTimer(Timer *timer);

	/* the widget it is read for (name ours) */
	Widget *m_widget;
};

/* the sounds of a widget type, read from the resource's "sound_<type>" parameter (names after
   ScePaf-RE) */
class WidgetSoundParam : public VersatileParam {
public:
	/* a sound of the parameter (names ours) */
	class Sound;

	int32_t AddWidget(Widget *widget);

	/* the widgets it gave its sounds (names ours) */
	list<Widget *> m_widget_list;
	Sound *m_sounds;
	uint32_t m_sound_num;
	uint32_t m_sound_capacity;
};

/*
 * A widget's key focus, read from the "focus_" children of its element (names ours; its
 * functions are ScePafResource's, not converted yet).
 */
class FocusParam : public VersatileParam {
public:
	/* makes room for num entries of each focus array (0x8106693e) */
	int32_t Alloc(int32_t num);

	/* the widget it is read for */
	Widget *m_widget;
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
	int32_t SetSize(const ResolutionData *size, uint32_t num, Timer *t, int32_t id,
			int32_t opt, float delay, AnimCB cb_fin, void *usrdata);
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
	int32_t SetAlignOffset(const math::v4 &offs, Timer *t, int32_t id, int32_t opt,
			       float delay, AnimCB cb_fin, void *usrdata);
	int32_t SetAlign(uint32_t ax, uint32_t ay, uint32_t az, Timer *t);
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
	uint8_t reserved3a[0xf];
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
	bool reserved3m : 1;
	bool reserved3n : 1;
	bool reserved3o : 1;
	/* cleared before a reverse transition (name after ScePaf-RE) */
	bool m_transition_complete : 1;
	uint8_t reserved3p[0x21];
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
	uint8_t reserved5[0x2c];
	uint32_t m_align_x : 4;
	uint32_t m_align_y : 4;
	uint32_t m_align_z : 4;
	uint32_t m_origin_x : 4;
	uint32_t m_origin_y : 4;
	uint32_t m_origin_z : 4;
	uint32_t reserved6 : 8;
	uint8_t reserved7[0x14];
	/* lays the widget out each update (name after ScePaf-RE) */
	Layouter *m_layouter;
	uint8_t reserved7a[0x44];
	/* what is called when it is deleted (names after DrawObj's, Sony's DWARF) */
	vector<DeleteCBInfo> m_list_delete_cb;
	uint8_t reserved7b[0xb];
	/* the sounds of its sound parameter (names ours) */
	uint8_t m_sound_num;
	WidgetSoundParam::Sound *m_sounds;
};

}
}

extern "C" {
/* gives the widget parameter its widget (the widget deletes it with itself) */
int32_t ScePafResource_190E99A6(paf::ui::WidgetParam *param, paf::ui::Widget *widget);
/* puts the widget into its transition: auto_delete deletes it once the time ends */
int32_t ScePafWidget_8AF26B1F(paf::ui::Widget *widget, bool auto_delete, float time);
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
