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

class DrawLayer;

class DrawEnv {
public:
	DrawEnv() : m_index(-1), m_count(0), m_counted(false), m_begin(0), m_end(0), m_ended(false)
	{
	}

	~DrawEnv();

	/* a layer named for a debug view, drawn in the order of its priority, which builds its
	   draws in a buffer of the size */
	DrawLayer *AddLayer(const string &name, int32_t priority, int32_t mode, size_t buffer_size);

	DrawCallList m_lists[4];
	/* not set when it is made */
	uint32_t m_spare;
	/* -1: none */
	int32_t m_index;
	/* counted up by ui::Environment, which clears m_counted with it */
	uint32_t m_count;
	bool m_counted;
	uint32_t m_begin;
	uint32_t m_end;
	bool m_ended;
};

/* draws the panel and bubble objects (Sony's name: the string its constructor 0x810bc564
   names it by); 0x1478 bytes */
class PanelRenderer {
public:
	/* 0x810bc564 */
	PanelRenderer();
	/* 0x810bcd6e */
	~PanelRenderer();

	/* 0x810bdca8: the cameras of the 3D and 2D views, the background and the draw calls */
	void Init(common::SharedPtr<Camera> camera, common::SharedPtr<Camera> camera_2d,
		  common::SharedPtr<Background> background, common::SharedPtr<DrawEnv> draw_env);
	/* 0x810bdef0 */
	void Update();
	/* 0x810bec18, 0x810becee: the matrices the panels are drawn with */
	math::matrix GetViewMatrix();
	math::matrix GetProjectionMatrix();
	/* 0x810bf36c: its resources loaded from the folder */
	void Load(const string &dir);

	uint8_t reserved[0x1420];
	Camera *m_camera;
	uint8_t reserved2[0x54];
};

class MotionListener;

/* the renderer of the waves (the name its constructor 0x810a54c2 gives it); 0x2090 bytes */
class WaveRenderer {
public:
	/* 0x810a54c2 */
	WaveRenderer();
	/* 0x811fe8e8: implicit, emitted with the renderer's code; declared until its members are */
	~WaveRenderer();

	/* 0x810a5ee2 */
	void Init(common::SharedPtr<Camera> camera, common::SharedPtr<Background> background,
		  common::SharedPtr<DrawEnv> draw_env, common::SharedPtr<PostEffects> post_effects,
		  common::SharedPtr<MotionListener> listener);
	/* 0x810a61c6 */
	void Load(const string &dir, int32_t mode);
	/* 0x810a76a0: the world of the 2D camera tilted with the waves, from the one it was reset
	   to */
	math::matrix GetTiltedWorld(const math::matrix &reset_world);

	uint8_t reserved[0x2078];
	/* the 2D camera follows the waves */
	bool m_tilt;
	uint8_t reserved2[0xb];
	/* what the custom renderer is given each frame */
	float m_wave_x;
	float m_wave_y;
	uint8_t reserved3[4];
};

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
	/* the panels' matrices, set every frame */
	math::matrix m_view;
	math::matrix m_projection;
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

class CustomRenderer {
public:
	virtual ~CustomRenderer();
	virtual void Init(common::SharedPtr<Camera> camera, common::SharedPtr<DrawEnv> draw_env);
	/* the folder twice, each with a size */
	virtual void Load(const string &dir, int32_t size, const string &dir2, int32_t size2);
	virtual void reserved4();
	virtual void reserved5();
	virtual CustomRendererOption *GetOption();

	uint8_t reserved[0x58];
	/* the waves' values (WaveRenderer::m_wave_x, m_wave_y), 0 without waves */
	float m_wave_x;
	float m_wave_y;
};

/* a renderer made by the function ScePafWidget_1955B85D gives the renderer (names ours) */
class EffectRenderer {
public:
	virtual ~EffectRenderer();
	virtual void Load(const string &dir);
	virtual void Update(float frame_time);
	virtual void Init(common::SharedPtr<Camera> camera, common::SharedPtr<Background> background,
			  common::SharedPtr<DrawEnv> draw_env,
			  common::SharedPtr<PostEffects> post_effects);
};

class DrawObj;
class Surface;
class PageTurnParam;
class PageTurnMesh;

/* turns a page with a texture on each side (Sony's name, the identifier PageTurnRenderer; its
   code is not converted: 0x810c89fc builds it); 0xa58 bytes */
class PageTurnRenderer {
public:
	/* 0x810c89fc: its resources loaded from the folder */
	PageTurnRenderer(const string &dir);
	/* 0x810ca076 */
	~PageTurnRenderer();

	/* 0x810cade8: draws the page from its parameters and mesh */
	void Draw(PageTurnParam *param, PageTurnMesh *mesh);
	/* 0x810cc0f6: lets go of the textures its shaders hold */
	void ReleaseTextures();
	/* 0x810cc42a */
	void Update();

	uint8_t reserved[0xa50];
	/* the back of the page (PageTurnObj::SetBgTexture) */
	intrusive_ptr<Surface> m_bg_texture;
	uint8_t reserved2[4];
};

/* where a widget's draw objects queue their draws: the order counted up per draw, under the
   widget's layer and sub-layer (names ours) */
class DrawInfo {
public:
	int32_t m_layer;
	int32_t m_reserved;
	int32_t m_order;
	int32_t m_sub_layer;
};

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

/* the draws of a frame: each is built in the layer's buffer and sorted by its key (names ours) */
class DrawLayer : public DrawCallList {
public:
	class Entry {
	public:
		Entry(uint32_t key, DrawLayerCall *call) : m_kind(0x40000000), m_call(call)
		{
			m_word = 0;
			m_key = key;
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
		: DrawCallList(NULL, NULL), m_reserved_word(0), m_reserved_word2(0), m_reserved_flag(false),
		  m_name(name), m_index(-1), m_priority(0), m_state(0), m_reserved_value(1),
		  m_buffer(buffer_size), m_used(0), m_first(-1), m_last(-1), m_mode(mode),
		  m_reserved_word3(0), m_reserved_word4(0), m_reserved_word5(0)
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

	uint32_t m_reserved_word;
	uint32_t m_reserved_word2;
	bool m_reserved_flag;
	string m_name;
	int32_t m_index;
	/* the order the layers are drawn in */
	int32_t m_priority;
	int32_t m_state;
	/* 1 when it is made; the renderer sets its layers' to 2 */
	int32_t m_reserved_value;
	vector<uint8_t> m_buffer;
	uint32_t m_used;
	vector<Entry> m_entries;
	int32_t m_first;
	int32_t m_last;
	int32_t m_mode;
	vector<uint32_t> m_reserved_list;
	uint32_t m_reserved_word3;
	uint32_t m_reserved_word4;
	uint32_t m_reserved_word5;
};

inline DrawLayer *DrawEnv::AddLayer(const string &name, int32_t priority, int32_t mode,
				    size_t buffer_size)
{
	DrawLayer *layer = new DrawLayer(name, mode, buffer_size);

	layer->InsertBefore(&m_lists[0]);
	layer->m_priority = priority;
	return layer;
}

/* the panel objects alive, by kind (0 and 2, 1), and the draws queued for the frame (0x810d5f88..;
   names ours) */
void RegisterPanelObj(DrawObj *obj, int32_t kind);
void UnregisterPanelObj(DrawObj *obj);
void DrawPanelObj(DrawObj *obj, int32_t kind, int32_t flags);

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
	common::SharedPtr<PostEffects> m_post_effects;
	Blur m_blur;
	/* how much of the blur is drawn (0 when it is made; nothing in the renderer sets it) */
	float m_blur_amount;
	bool m_reserved_flag;
	ScreenFilter m_filter;
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
