/*
 * paf::graph::DrawEnv: the draw calls queued for the renderer, in four intrusive lists; the
 * renderer that draws them; and the globals the draw objects find them by. DrawEnv is Sony's
 * name (ScePaf-RE's predeclarations); the renderer, the lists and every member name are ours.
 * Their code is not converted yet: what is here is what DrawObj's file builds and frees.
 */
#ifndef PAF_GRAPHICS_DRAWOBJ_DRAW_ENV_H
#define PAF_GRAPHICS_DRAWOBJ_DRAW_ENV_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <math/ngp/v4.h>

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

/* the camera the panels are seen by: its fourth virtual gives the eye's position (names ours) */
class Camera {
public:
	virtual void reserved0();
	virtual void reserved1();
	virtual void reserved2();
	virtual math::v4 GetEyePos() const;
};

/* draws the panel and bubble objects (Sony's name: the string its constructor 0x810bc564
   names it by); 0x1478 bytes */
class PanelRenderer {
public:
	uint8_t reserved[0x1420];
	Camera *m_camera;
	uint8_t reserved2[0x54];
};

class DrawObj;

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

	common::SharedPtr<PanelRenderer> m_panel_renderer;
	uint8_t reserved[0x590];
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
class DrawLayer;

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
}

#endif
