/*
 * paf::graph::arl's stack of the scenes drawn into, each change passed on to the
 * ArlInstance (the file's name ours).
 */
#include <stddef.h>
#include <stdint.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>

namespace paf {
namespace graph {
namespace arl {

SceneDataStack::SceneDataStack() : m_depth(1)
{
	m_scene[0] = NULL;
}

void SceneDataStack::Unused()
{
}

void SceneDataStack::Push()
{
	if (m_depth < MAX_DEPTH) {
		m_scene[m_depth] = m_scene[m_depth - 1];
		m_depth++;
	}
}

void SceneDataStack::Set(FrameBufferObject *fbo)
{
	ArlInstance *gl = Gl();

	m_scene[m_depth - 1] = fbo;
	gl->SetSceneData(fbo);
}

void SceneDataStack::Pop()
{
	if (m_depth > 1)
		m_depth--;
	Set(m_scene[m_depth - 1]);
}

}
}
}
