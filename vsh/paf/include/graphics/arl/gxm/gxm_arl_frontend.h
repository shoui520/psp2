/*
 * paf::graph::arl's GL-like front end over GXM: the ArlInstance Gl() returns, as far as
 * converted code uses it. Names from the mangled export names and ScePaf-RE's
 * gxm_arl_frontend.h; the scene stack's names ours.
 */
#ifndef PAF_GRAPHICS_ARL_GXM_GXM_ARL_FRONTEND_H
#define PAF_GRAPHICS_ARL_GXM_GXM_ARL_FRONTEND_H

#include <stdint.h>
#include <graphics/arl/arl_defines.h>

namespace paf {
namespace graph {
namespace arl {

class FrameBufferObject;

/* the scenes drawn into, innermost last: the bottom one stays */
class SceneDataStack {
public:
	enum {
		MAX_DEPTH = 8
	};

	SceneDataStack();
	~SceneDataStack();

	void Push();
	void Set(FrameBufferObject *fbo);
	void Pop();

	uint32_t m_depth;
	FrameBufferObject *m_scene[MAX_DEPTH];
};

class ArlInstance {
public:
	void SetSceneData(FrameBufferObject *fbo);
};

ArlInstance *Gl();

}
}
}

#endif
