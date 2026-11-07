/*
 * paf::graph::DrawEnv: the draw calls queued for the renderer, in four intrusive lists; the
 * renderer that draws them; and the globals the draw objects find them by. DrawEnv is Sony's
 * name (ScePaf-RE's predeclarations); the renderer, the lists and every member name are ours.
 * Their code is not converted yet: what is here is what DrawObj's file builds and frees.
 */
#ifndef PAF_GRAPHICS_DRAWOBJ_DRAW_ENV_H
#define PAF_GRAPHICS_DRAWOBJ_DRAW_ENV_H

#include <stdint.h>
#include <common/common_internal.h>
#include <common/shared_ptr.h>
#include <graphics/camera.h>
#include <graphics/drawobj/draw_info.h>
#include <graphics/effects/background.h>
#include <graphics/effects/fft_wave_surface.h>
#include <graphics/effects/param.h>
#include <graphics/effects/post_effects.h>
#include <graphics/surface/surface.h>
#include <inputdevice/inputlistener.h>
#include <inputdevice/motionsensor/motionsensor.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v4.h>
#include <std/memory>
#include <std/new>
#include <std/string>
#include <std/vector>

namespace paf {
namespace graph {

/* the head of an intrusive list: empty, it links to itself; a node of one starts unlinked */
class DrawCallList {
public:
	DrawCallList() : m_prev(this), m_next(this)
	{
	}

	DrawCallList(DrawCallList *prev, DrawCallList *next) : m_prev(prev), m_next(next)
	{
	}

	void Unlink()
	{
		if (m_next != NULL && m_prev != NULL) {
			m_next->m_prev = m_prev;
			m_prev->m_next = m_next;
		}
		m_prev = NULL;
		m_next = NULL;
	}

	/* the node linked in front of another, taken out of any list it was in */
	void InsertBefore(DrawCallList *node)
	{
		if (node == this)
			return;
		Unlink();
		m_next = node;
		m_prev = node->m_prev;
		node->m_prev->m_next = this;
		node->m_prev = this;
	}

	DrawCallList *m_prev;
	DrawCallList *m_next;
};

/* a DrawCallList a class derives from once for each list it is in, N the list (name
   ours): a layer is in its draw environment's lists 0 and 1, the second 8 bytes in */
template<int N>
class DrawListNode : public DrawCallList {
public:
	DrawListNode(DrawCallList *prev, DrawCallList *next) : DrawCallList(prev, next)
	{
	}
};

class DrawLayer;

class DrawEnv {
public:
	DrawEnv()
		: m_index(-1), m_count(0), m_counted(false), m_used_total(0), m_drawn_total(0),
		  m_ended(false)
	{
	}

	~DrawEnv();

	/* a layer named for a debug view, drawn in the order of its priority, which builds its
	   draws in a buffer of the size */
	DrawLayer *AddLayer(const string &name, int32_t priority, int32_t mode, size_t buffer_size);

	/* the layers of m_lists[1] from the cursor on that took draws this frame drawn, once
	   the layer takes draws (draw_env_flush.h; names ours) */
	void FlushLayers(DrawLayer *layer);
	void SetPriority(DrawLayer *layer, int32_t priority);
	void SetDrawMode(DrawLayer *layer, int32_t mode);

	DrawCallList m_lists[4];
	/* the next layer of m_lists[1] FlushLayers draws; not set when it is made */
	DrawCallList *m_cursor;
	/* -1: none */
	int32_t m_index;
	/* counted up by ui::Environment, which clears m_counted with it */
	uint32_t m_count;
	/* FlushLayers makes its "[REN] frame=" line, which it prints nowhere */
	bool m_counted;
	/* the bytes of draws and the draws FlushLayers drew, added up */
	uint32_t m_used_total;
	uint32_t m_drawn_total;
	bool m_ended;
};

class PanelRenderer;

class WaveRenderer;

/* the waves' surface computed by FFT (its parameters' set is named "FFTWave"); nothing in
   libpaf makes one */
class FFTWave {
public:
	/* 0x8109f3e0 */
	void Update();

	FFTWaveSurface m_surface;
	uint8_t reserved4[0x438];
	/* the frame of the last update */
	int32_t m_frame;
	uint32_t m_reserved_word;
	/* the time the waves are at, back to 0 past the period over 2 pi */
	float m_time;
	float m_period;
	uint8_t reserved[9];
	bool m_paused;
	uint8_t reserved2[10];
	/* the parameters' keys, moved on by the frame time */
	ParamSetKeysManager m_param_keys;
	/* the panels' bubble light, packed as the lighting programs take it, set every frame
	   (names ours) */
	math::matrix m_point_light_pack1;
	math::matrix m_point_light_pack2;
	uint8_t reserved3[0x30c];
	/* how fast the time goes, times the frame time */
	float m_speed;
};

/* a renderer made by a function the renderer is given: its seventh virtual returns an object
   whose third switches it (names ours) */
class CustomRendererOption {
public:
	virtual void reserved0();
	virtual void reserved1();
	virtual void SetEnabled(bool enabled);
};

/* the points the custom renderer's trail went through, a ring of m_count: the last at m_last,
   the next written at m_next (names ours) */
class CustomTrailPoints {
public:
	math::v4 *m_points;
	uint32_t m_reserved;
	uint32_t m_count;
	uint32_t m_last;
	uint32_t m_next;
};

/* the renderer's class is the factory's (another module's); libpaf reads only these */
class CustomRenderer {
public:
	virtual ~CustomRenderer();
	virtual void Init(common::SharedPtr<Camera> camera, common::SharedPtr<DrawEnv> draw_env);
	/* the folder twice, each with a size */
	virtual void Load(const string &dir, int32_t size, const string &dir2, int32_t size2);
	/* the frame's time on (ScePafGraphics_9409C495) */
	virtual void Update(float frame_time);
	virtual void reserved5();
	virtual CustomRendererOption *GetOption();
	/* the trail's switch (name ours) */
	virtual CustomRendererOption *GetTrailOption();

	/* the camera Init was given */
	common::SharedPtr<Camera> m_camera;
	uint8_t reserved[0x50];
	/* the waves' values (WaveRenderer::m_wave_x, m_wave_y), 0 without waves */
	float m_wave_x;
	float m_wave_y;
	uint8_t reserved2[4];
	/* the presets, moved to "iboot" or "default" by ScePafGraphics_9569E74C, and the trail's */
	ParamSetKeysManager m_manager;
	uint8_t reserved3[0x173c - 0x68 - sizeof(ParamSetKeysManager)];
	/* drawn by the scene while GetOption's switch is on, the trail's while the trail is on */
	DrawLayer *m_rs_custom;
	uint8_t reserved4[0x177c - 0x1740];
	ParamSetKeysManager m_trail_manager;
	uint8_t reserved6[0x2800 - 0x177c - sizeof(ParamSetKeysManager)];
	CustomTrailPoints m_trail;
	uint8_t reserved7[0x2c18 - 0x2814];
	/* set by the renderer when it moved the trail itself this frame, cleared before the
	   moves are run */
	bool m_trail_moved;
	DrawLayer *m_rs_trail;
};

/* a renderer made by the function ScePafWidget_1955B85D gives the renderer (names ours) */
class EffectRenderer {
public:
	virtual ~EffectRenderer();
	virtual void Load(const string &dir);
	virtual void Update(float frame_time);
	virtual void Init(common::SharedPtr<Camera> camera, common::SharedPtr<Background> background,
			  common::SharedPtr<DrawEnv> draw_env,
			  common::SharedPtr<TextureOps> texture_ops);
};

class DrawObj;
class Surface;
class PageTurnParam;
class PageTurnMesh;

class PageTurnRenderer;


/* a draw queued in a layer, which the layer calls when it draws (names ours) */
class DrawLayerCall {
public:
	DrawLayerCall() : m_queued(false)
	{
	}

	virtual ~DrawLayerCall()
	{
	}

	virtual void Exec() = 0;

	bool m_queued;
};

/* calls a function with the argument it keeps */
template<class Arg>
class DrawLayerFuncCall : public DrawLayerCall {
public:
	DrawLayerFuncCall(void (*func)(const Arg &arg), const Arg &arg) : m_func(func), m_arg(arg)
	{
	}

	virtual void Exec()
	{
		m_func(m_arg);
	}

	void (*m_func)(const Arg &arg);
	Arg m_arg;
};

/* calls a member function of the object it keeps (names ours) */
template<class T>
class DrawLayerMemberCall : public DrawLayerCall {
public:
	DrawLayerMemberCall(void (T::*func)(), T *object) : m_func(func), m_object(object)
	{
	}

	virtual void Exec()
	{
		(m_object->*m_func)();
	}

	void (T::*m_func)();
	T *m_object;
};

/* calls a member function of the object it keeps with one argument, kept by value (names ours) */
template<class T, class A1>
class DrawLayerMemberCall1 : public DrawLayerCall {
public:
	DrawLayerMemberCall1(void (T::*func)(const A1 &), T *object, const A1 &arg1)
		: m_func(func), m_object(object), m_arg1(arg1)
	{
	}

	virtual void Exec()
	{
		(m_object->*m_func)(m_arg1);
	}

	void (T::*m_func)(const A1 &);
	T *m_object;
	A1 m_arg1;
};

/* the same, with two arguments (names ours) */
template<class T, class A1, class A2>
class DrawLayerMemberCall2 : public DrawLayerCall {
public:
	DrawLayerMemberCall2(void (T::*func)(const A1 &, const A2 &), T *object, const A1 &arg1,
			     const A2 &arg2)
		: m_func(func), m_object(object), m_arg1(arg1), m_arg2(arg2)
	{
	}

	virtual void Exec()
	{
		(m_object->*m_func)(m_arg1, m_arg2);
	}

	void (T::*m_func)(const A1 &, const A2 &);
	T *m_object;
	A1 m_arg1;
	A2 m_arg2;
};

/* the same, with three arguments (names ours) */
template<class T, class A1, class A2, class A3>
class DrawLayerMemberCall3 : public DrawLayerCall {
public:
	DrawLayerMemberCall3(void (T::*func)(const A1 &, const A2 &, const A3 &), T *object,
			     const A1 &arg1, const A2 &arg2, const A3 &arg3)
		: m_func(func), m_object(object), m_arg1(arg1), m_arg2(arg2), m_arg3(arg3)
	{
	}

	virtual void Exec()
	{
		(m_object->*m_func)(m_arg1, m_arg2, m_arg3);
	}

	void (T::*m_func)(const A1 &, const A2 &, const A3 &);
	T *m_object;
	A1 m_arg1;
	A2 m_arg2;
	A3 m_arg3;
};

/* the draws of a frame: each is built in the layer's buffer and sorted by its key (names ours) */
class DrawLayer : public DrawListNode<0>, public DrawListNode<1> {
public:
	/* m_mode's bits (names ours) */
	enum {
		/* drawn between a GPU marker of its name */
		MODE_MARKER = 1,
		/* the draws kept after they are drawn */
		MODE_KEEP = 2,
		/* the draws drawn in the order they were queued, not by their keys */
		MODE_UNSORTED = 4
	};

	/* a key of two words, as the entries are sorted by */
	class Key {
	public:
		Key(uint32_t kind, uint32_t word) : m_kind(kind), m_word(word)
		{
		}

		bool operator==(const Key &other) const
		{
			return m_kind == other.m_kind && m_word == other.m_word;
		}

		uint32_t m_kind;
		uint32_t m_word;
	};

	class Entry {
	public:
		Entry(uint32_t key, DrawLayerCall *call) : m_kind(0x40000000), m_call(call)
		{
			m_word = 0;
			m_key = key;
		}

		/* by the first word, then the second */
		bool operator<(const Entry &other) const
		{
			if (m_kind == other.m_kind)
				return m_word < other.m_word;
			return m_kind < other.m_kind;
		}

		Entry(uint32_t kind, uint32_t word, DrawLayerCall *call)
			: m_kind(kind), m_word(word), m_call(call)
		{
		}

		uint32_t m_kind;
		union {
			struct {
				uint32_t m_key : 31;
				uint32_t m_reserved : 1;
			};
			uint32_t m_word;
		};
		DrawLayerCall *m_call;
	};

	DrawLayer(const string &name, int32_t mode, size_t buffer_size)
		: DrawListNode<0>(NULL, NULL), DrawListNode<1>(NULL, NULL), m_drawn(false),
		  m_name(name), m_index(-1), m_priority(0), m_state(0), m_draw_mode(1),
		  m_buffer(buffer_size), m_used(0), m_first(-1), m_last(-1), m_mode(mode),
		  m_key_count(0), m_last_used(0), m_last_count(0)
	{
	}

	/* only while the layer takes draws (state 0 or 1) */
	template<class Arg>
	void Push(uint32_t key, void (*func)(const Arg &arg), const Arg &arg)
	{
		if (m_state != 1 && m_state != 0)
			return;
		if (m_used + sizeof(DrawLayerFuncCall<Arg>) > m_buffer.size())
			common_internal::on_error();

		DrawLayerFuncCall<Arg> *call =
			new (&m_buffer[m_used]) DrawLayerFuncCall<Arg>(func, arg);

		call->m_queued = true;
		m_entries.push_back(Entry(key, call));
		m_used += sizeof(DrawLayerFuncCall<Arg>);
	}

	/* a function called with the argument, under a key of two words */
	template<class Arg>
	void Push(uint32_t kind, uint32_t word, void (*func)(const Arg &arg), const Arg &arg)
	{
		if (m_state != 1 && m_state != 0)
			return;
		if (m_used + sizeof(DrawLayerFuncCall<Arg>) > m_buffer.size())
			common_internal::on_error();

		DrawLayerFuncCall<Arg> *call =
			new (&m_buffer[m_used]) DrawLayerFuncCall<Arg>(func, arg);

		call->m_queued = true;
		m_entries.push_back(Entry(kind, word, call));
		m_used += sizeof(DrawLayerFuncCall<Arg>);
	}

	/* a member function of an object called, under a key of two words */
	template<class T>
	void Push(uint32_t kind, uint32_t word, void (T::*func)(), T *const &object)
	{
		if (m_state != 1 && m_state != 0)
			return;
		if (m_used + sizeof(DrawLayerMemberCall<T>) > m_buffer.size())
			common_internal::on_error();

		DrawLayerMemberCall<T> *call =
			new (&m_buffer[m_used]) DrawLayerMemberCall<T>(func, object);

		call->m_queued = true;
		m_entries.push_back(Entry(kind, word, call));
		m_used += sizeof(DrawLayerMemberCall<T>);
	}

	template<class T, class A1>
	void Push(uint32_t kind, uint32_t word, void (T::*func)(const A1 &), T *const &object,
		  const A1 &arg1)
	{
		if (m_state != 1 && m_state != 0)
			return;
		if (m_used + sizeof(DrawLayerMemberCall1<T, A1>) > m_buffer.size())
			common_internal::on_error();

		DrawLayerMemberCall1<T, A1> *call =
			new (&m_buffer[m_used]) DrawLayerMemberCall1<T, A1>(func, object, arg1);

		call->m_queued = true;
		m_entries.push_back(Entry(kind, word, call));
		m_used += sizeof(DrawLayerMemberCall1<T, A1>);
	}

	template<class T, class A1, class A2>
	void Push(uint32_t kind, uint32_t word, void (T::*func)(const A1 &, const A2 &),
		  T *const &object, const A1 &arg1, const A2 &arg2)
	{
		if (m_state != 1 && m_state != 0)
			return;
		if (m_used + sizeof(DrawLayerMemberCall2<T, A1, A2>) > m_buffer.size())
			common_internal::on_error();

		DrawLayerMemberCall2<T, A1, A2> *call = new (&m_buffer[m_used])
			DrawLayerMemberCall2<T, A1, A2>(func, object, arg1, arg2);

		call->m_queued = true;
		m_entries.push_back(Entry(kind, word, call));
		m_used += sizeof(DrawLayerMemberCall2<T, A1, A2>);
	}

	template<class T, class A1, class A2, class A3>
	void Push(uint32_t kind, uint32_t word, void (T::*func)(const A1 &, const A2 &, const A3 &),
		  T *const &object, const A1 &arg1, const A2 &arg2, const A3 &arg3)
	{
		if (m_state != 1 && m_state != 0)
			return;
		if (m_used + sizeof(DrawLayerMemberCall3<T, A1, A2, A3>) > m_buffer.size())
			common_internal::on_error();

		DrawLayerMemberCall3<T, A1, A2, A3> *call = new (&m_buffer[m_used])
			DrawLayerMemberCall3<T, A1, A2, A3>(func, object, arg1, arg2, arg3);

		call->m_queued = true;
		m_entries.push_back(Entry(kind, word, call));
		m_used += sizeof(DrawLayerMemberCall3<T, A1, A2, A3>);
	}

	/* the key added to those queued this frame, unless it is there: whether it was not */
	bool AddKey(const Key &key)
	{
		for (uint32_t i = 0; i < m_key_count; i++)
			if (m_keys[i] == key)
				return false;
		if (m_keys.size() == (int)m_key_count)
			m_keys.push_back(key);
		m_keys[m_key_count++] = key;
		return true;
	}

	/* the layer of its node in the draw order (name ours) */
	static DrawLayer *FromOrder(DrawCallList *node)
	{
		return static_cast<DrawLayer *>(static_cast<DrawListNode<1> *>(node));
	}

	/* the frame's draws drawn, sorted by their keys unless MODE_UNSORTED, then dropped
	   unless MODE_KEEP; index is the environment's, log whether to make the "[REN]" line
	   (draw_env_flush.h; name ours) */
	void Draw(int32_t index, bool log);

	/* set by Draw once it has drawn */
	bool m_drawn;
	string m_name;
	int32_t m_index;
	/* the order the layers are drawn in */
	int32_t m_priority;
	int32_t m_state;
	/* 1 when it is made; the renderers set their layers' to 2, the wave switch 1 or 2 as
	   it is on or off (name ours) */
	int32_t m_draw_mode;
	vector<uint8_t> m_buffer;
	uint32_t m_used;
	vector<Entry> m_entries;
	int32_t m_first;
	int32_t m_last;
	int32_t m_mode;
	/* the keys of the state changes queued this frame, each queued once, and how many are */
	vector<Key> m_keys;
	uint32_t m_key_count;
	/* the bytes of draws and the draws of the last Draw */
	uint32_t m_last_used;
	uint32_t m_last_count;
};

inline DrawLayer *DrawEnv::AddLayer(const string &name, int32_t priority, int32_t mode,
				    size_t buffer_size)
{
	DrawLayer *layer = new DrawLayer(name, mode, buffer_size);

	static_cast<DrawListNode<0> *>(layer)->InsertBefore(&m_lists[0]);
	layer->m_priority = priority;
	return layer;
}

/* the panel objects alive, by kind (0 and 2, 1), and the draws queued for the frame (0x810d5f88..;
   names ours) */
void RegisterPanelObj(DrawObj *obj, int32_t kind);
void UnregisterPanelObj(DrawObj *obj);
void DrawPanelObj(DrawObj *obj, int32_t kind, int32_t flags);
/* the panel objects' layers set to be drawn this frame or not, and drawn (called by
   ScePafWidget_DC1B19DB; names ours) */
void UpdatePanelLayers();
void FlushPanelLayers();
void FlushPanelReflections();

/* hands the motion sensor's data of every frame to the waves, with the sensor's rotation
   turned from its axes to the view's (names ours) */
class MotionListener : public inputdevice::InputListener {
public:
	MotionListener()
		: inputdevice::InputListener(inputdevice::DEVICE_TYPE_PAD),
		  m_rotation(math::matrix::Identity())
	{
	}

	virtual void OnUpdate(const inputdevice::Data *data)
	{
		if (data->m_motion_data != NULL) {
			m_motion = *data->m_motion_data;

			math::matrix rotation = m_motion.rotation_matrix.transpose();

			m_rotation = math::matrix(rotation.getImpl().getCol0(),
						  -rotation.getImpl().getCol2(),
						  rotation.getImpl().getCol1(),
						  rotation.getImpl().getCol3());
		}
	}

	inputdevice::motionsensor::Data m_motion;
	math::matrix m_rotation;
};

/* the textures loaded for the panels, by name, from a folder of the resources (names ours) */
class TextureCache {
public:
	class Entry {
	public:
		~Entry()
		{
			if (m_surface != NULL)
				m_surface->Release();
		}

		string m_name;
		common::SharedPtr<arl::Texture2D> m_texture;
		Surface *m_surface;
	};

	TextureCache() : m_dir("/budge_texture/")
	{
	}

	string m_dir;
	vector<Entry> m_entries;
};

/* the renderers of the effects, the backgrounds they are drawn over, and the layers of the
   draw calls they draw in (names ours) */
class Renderer {
public:
	Renderer(common::SharedPtr<DrawEnv> draw_env);

	/* the layers' draws */
	void DrawDummyBackground();
	void CaptureBlur();
	void DrawBlur();
	/* 0x8109c646: no caller */
	void DrawNothing();

	/* each made at the first call that asks for it */
	common::SharedPtr<PanelRenderer> &GetPanelRenderer(bool create);
	common::SharedPtr<WaveRenderer> &GetWaveRenderer(bool create);
	/* by the function the renderer is given */
	common::SharedPtr<CustomRenderer> &GetCustomRenderer(bool create);
	common::SharedPtr<PageTurnRenderer> &GetPageTurnRenderer(bool create);

	void SetRoll(float roll);
	void Update();

	/* the layer's draw mode set (name ours) */
	void SetDrawMode(DrawLayer *layer, int32_t mode)
	{
		layer->m_draw_mode = mode;
	}

	common::SharedPtr<PanelRenderer> m_panel_renderer;
	common::SharedPtr<FFTWave> m_fft_wave;
	common::SharedPtr<WaveRenderer> m_wave_renderer;
	common::SharedPtr<CustomRenderer> m_custom_renderer;
	common::SharedPtr<PageTurnRenderer> m_page_turn_renderer;
	/* made by ScePafGraphics_71E2CB77 */
	common::SharedPtr<EffectRenderer> m_effect_renderer;
	/* nothing in libpaf makes these */
	common::SharedPtr<EffectRenderer> m_reserved_renderer;
	common::SharedPtr<EffectRenderer> m_reserved_renderer2;
	/* the folder the renderers' resources are loaded from (gutil::LoadResource) */
	string m_resource_path;
	/* the frame of the last Update */
	int32_t m_frame;
	int32_t m_reserved_frame;
	TextureCache m_texture_cache;
	common::SharedPtr<BackgroundList> m_backgrounds;
	common::SharedPtr<DrawEnv> m_draw_env;
	common::SharedPtr<EnvAuto> m_env;
	/* the background drawn */
	common::SharedPtr<Background> m_background;
	/* the cameras of the 3D and the 2D view, set up by gutil::Init */
	common::SharedPtr<Camera> m_camera;
	common::SharedPtr<Camera> m_camera_2d;
	common::SharedPtr<MotionListener> m_listener;
	common::SharedPtr<SharedTexture> m_shared_texture;
	common::SharedPtr<TextureOps> m_texture_ops;
	Blur m_blur;
	/* how much of the blur is drawn (0 when it is made; nothing in the renderer sets it) */
	float m_blur_amount;
	bool m_reserved_flag;
	DepthOfField m_depth_of_field;
	/* the flag ScePafGraphics_505CD74E sets; nothing in libpaf reads it */
	bool m_flag_505CD74E;
	DrawLayer *m_rs_world_grid;
	DrawLayer *m_rs_dummy_background;
	DrawLayer *m_rs_capture_reflections;
	DrawLayer *m_rs_copy_screen_for_refractions;
	DrawLayer *m_rs_capture_blur;
	uint32_t m_reserved_word;
	/* set by ScePafWidget_1955B85D */
	EffectRenderer *(*m_effect_renderer_factory)();
	CustomRenderer *(*m_custom_renderer_factory)();
	uint32_t m_reserved_word2;
};

/* a pointer kept in a global: the static initializer sets it to NULL */
template<class T>
class GlobalPtr {
public:
	GlobalPtr(T *ptr) : m_ptr(ptr)
	{
	}

	T *m_ptr;
};

/* what the scene draws into: the scene sets these while it draws (ui::Scene::DrawScene) */

extern GlobalPtr<DrawLayer> g_scene_draw_layer;
extern GlobalPtr<DrawLayer> g_scene_draw_layer2;

}
}

extern "C" {
/* the renderer, made with the draw calls by ScePafGraphics_1C294786 and deleted by 0B4F6C24 */
paf::graph::GlobalPtr<paf::graph::Renderer> *ScePafGraphics_4E038C05();
void ScePafGraphics_1C294786();
void ScePafGraphics_0B4F6C24();
void ScePafGraphics_505CD74E(bool flag);
/* the renderer's effect renderer, made at the first call that asks for it (the flag 8 of
   gutil::LoadResource) */
paf::common::SharedPtr<paf::graph::EffectRenderer> &
ScePafGraphics_71E2CB77(paf::graph::Renderer *renderer, bool create);
}

#endif
