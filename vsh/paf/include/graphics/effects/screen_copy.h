/*
 * paf::graph::ScreenCopy: surfaces each drawn over, while their object is on, with the screen (its
 * first page or the page shown), the offscreen display surface or a surface of their own, a quad
 * over the whole surface drawn with GateBlur's programs. The classes ScreenCopy and ScreenCopyObj
 * and the methods Initialize, Finalize, Draw, Draw_, CreateObj and DeleteObj are Sony's (the
 * export NIDs hash as them); the file's name, the fields and the enums are ours.
 */
#ifndef PAF_GRAPHICS_EFFECTS_SCREEN_COPY_H
#define PAF_GRAPHICS_EFFECTS_SCREEN_COPY_H

#include <stdint.h>
#include <std/list>
#include <std/memory>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/surface/surface.h>

namespace paf {
namespace graph {

/* what an object's surface is drawn over with */
enum ScreenCopySource {
	/* the screen's page shown */
	SCREEN_COPY_FRONT = 0,
	/* the offscreen display surface */
	SCREEN_COPY_OFFSCREEN = 1,
	/* the object's m_source */
	SCREEN_COPY_SURFACE = 2,
	/* the screen's first page */
	SCREEN_COPY_PAGE0 = 3
};

/* when an object's surface is drawn over */
enum ScreenCopyState {
	SCREEN_COPY_OFF = 0,
	/* every frame */
	SCREEN_COPY_ON = 1,
	/* this frame, then off */
	SCREEN_COPY_ONCE = 2,
	/* the next frame, then off */
	SCREEN_COPY_NEXT = 3
};

/* a surface drawn over (0x10 bytes) */
class ScreenCopyObj {
public:
	/* the surface, its source none, its state left as it is */
	__declspec(dllexport) ScreenCopyObj(intrusive_ptr<Surface> &target, int32_t source)
		: m_target(target), m_source_kind(source)
	{
	}

	__declspec(dllexport) ~ScreenCopyObj()
	{
	}

	intrusive_ptr<Surface> m_target;
	intrusive_ptr<Surface> m_source;
	/* a ScreenCopySource */
	int32_t m_source_kind;
	/* a ScreenCopyState, set by the object's owner */
	int32_t m_state;
};

/* the objects (0xc bytes) */
class ScreenCopy {
public:
	/* the instance and its quad made */
	ScreenCopy();
	~ScreenCopy();

	/* the instance made */
	static void Initialize();
	/* the instance deleted */
	static void Finalize();
	/* the instance drawn, when there is one */
	static void Draw();
	/* an object added, NULL without an instance */
	static ScreenCopyObj *CreateObj(intrusive_ptr<Surface> &target, int32_t source);
	/* an object removed and deleted, when it is one */
	static void DeleteObj(ScreenCopyObj *obj);

	/* the objects on drawn over */
	void Draw_();

	static ScreenCopy *s_instance;
	/* four vertices over the whole surface */
	static arl::VertexArray *s_quad;

	list<ScreenCopyObj *> m_objs;
};

}
}

#endif
