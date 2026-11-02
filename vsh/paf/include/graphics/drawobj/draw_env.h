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
#include <math/ngp/matrix.h>
#include <math/ngp/v4.h>
#include <std/memory>
#include <std/new>
#include <std/string>
#include <std/vector>

namespace paf {
namespace graph {

/* the head of an intrusive list: empty, it links to itself */
class DrawCallList {
public:
	DrawCallList() : m_prev(this), m_next(this)
	{
	}

	DrawCallList *m_prev;
	DrawCallList *m_next;
};

class DrawEnv {
public:
	DrawEnv() : m_index(-1), m_count(0), m_counted(false), m_begin(0), m_end(0), m_ended(false)
	{
	}

	~DrawEnv();

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
	uint8_t reserved[0x1420];
	Camera *m_camera;
	uint8_t reserved2[0x54];
};

/* the renderer of the waves (the name its constructor 0x810a54c2 gives it); 0x2090 bytes */
class WaveRenderer;

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
	virtual void reserved0();
	virtual void reserved1();
	virtual void reserved2();
	virtual void reserved3();
	virtual void reserved4();
	virtual void reserved5();
	virtual CustomRendererOption *GetOption();
};

class DrawObj;
class Surface;
class PageTurnParam;
class PageTurnMesh;

/* turns a page with a texture on each side (Sony's name, the identifier PageTurnRenderer; its
   code is not converted: 0x810c89fc builds it); 0xa58 bytes */
class PageTurnRenderer {
public:
	/* 0x810cade8: draws the page from its parameters and mesh */
	void Draw(PageTurnParam *param, PageTurnMesh *mesh);
	/* 0x810cc0f6: lets go of the textures its shaders hold */
	void ReleaseTextures();

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

/* the draws of a frame: each is built in the layer's buffer and sorted by its key (names ours) */
class DrawLayer {
public:
	class Entry {
	public:
		Entry(uint32_t key, DrawLayerCall *call) : m_kind(0x40000000), m_call(call)
		{
			m_word = 0;
			m_key = key;
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

	/* only while the layer takes draws (state 0 or 1) */
	template<class Arg>
	void Push(uint32_t key, void (*func)(const Arg &arg), const Arg &arg)
	{
		if (m_state != 1 && m_state != 0)
			return;
		if (m_used + sizeof(DrawLayerFuncCall<Arg>) > m_buffer_size)
			common_internal::on_error();

		DrawLayerFuncCall<Arg> *call = new (m_buffer + m_used) DrawLayerFuncCall<Arg>(func, arg);

		call->m_queued = true;
		m_entries.push_back(Entry(key, call));
		m_used += sizeof(DrawLayerFuncCall<Arg>);
	}

	uint8_t reserved[40];
	int32_t m_state;
	uint8_t reserved2[4];
	uint8_t *m_buffer;
	uint32_t m_buffer_size;
	uint8_t reserved3[8];
	uint32_t m_used;
	vector<Entry> m_entries;
};

/* the panel objects alive, by kind (0 and 2, 1), and the draws queued for the frame (0x810d5f88..;
   names ours) */
void RegisterPanelObj(DrawObj *obj, int32_t kind);
void UnregisterPanelObj(DrawObj *obj);
void DrawPanelObj(DrawObj *obj, int32_t kind, int32_t flags);

/* draws a DrawEnv's draw calls */
class Renderer {
public:
	Renderer(common::SharedPtr<DrawEnv> draw_calls);
	~Renderer();

	/* made at the first call that asks for it (0x8109dbaa) */
	common::SharedPtr<PanelRenderer> &GetPanelRenderer(bool create);

	/* made at the first call that asks for it (0x8109e532) */
	common::SharedPtr<PageTurnRenderer> &GetPageTurnRenderer(bool create);

	/* made at the first call that asks for it (0x8109defe) */
	common::SharedPtr<WaveRenderer> &GetWaveRenderer(bool create);

	/* made at the first call that asks for it, by the function the renderer is given
	   (0x8109e2ce) */
	common::SharedPtr<CustomRenderer> &GetCustomRenderer(bool create);

	common::SharedPtr<PanelRenderer> m_panel_renderer;
	uint8_t reserved[8];
	common::SharedPtr<WaveRenderer> m_wave_renderer;
	common::SharedPtr<CustomRenderer> m_custom_renderer;
	common::SharedPtr<PageTurnRenderer> m_page_turn_renderer;
	uint8_t reserved1[0x18];
	/* the folder the renderers' resources are loaded from (gutil::LoadResource) */
	string m_resource_path;
	uint8_t reserved3[0x44];
	/* the cameras of the 3D and the 2D view, set up by gutil::Init */
	common::SharedPtr<Camera> m_camera;
	common::SharedPtr<Camera> m_camera_2d;
	uint8_t reserved4[0x4f8];
	/* the flag ScePafGraphics_505CD74E sets; nothing in libpaf reads it by this offset */
	bool m_flag_505CD74E;
	uint8_t reserved2[0x27];
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
/* the renderer made with the flag 8 of gutil::LoadResource */
void ScePafGraphics_71E2CB77(paf::graph::Renderer *renderer, bool create);
}

#endif
