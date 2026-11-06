/*
 * DrawEnv::FlushLayers and DrawLayer::Draw: the layers of the draw order that took draws this
 * frame drawn, each its queued calls in the order of their keys (libpaf's build emits
 * FlushLayers once, 0x81220060 TOOL, for ui::Scene::DrawScene, the wave switch and the panel
 * objects). Apart from draw_env.h so that only their callers read the front end's header; the
 * file's and every name here are ours.
 */
#ifndef PAF_GRAPHICS_DRAWOBJ_DRAW_ENV_FLUSH_H
#define PAF_GRAPHICS_DRAWOBJ_DRAW_ENV_FLUSH_H

#include <stdint.h>
#include <common/string_util.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/drawobj/draw_env.h>
#include <std/algorithm>
#include <std/string>
#include <src/common/frame_count.h>

namespace paf {
namespace graph {

inline void DrawLayer::Draw(int32_t index, bool log)
{
	m_last = index;
	if (m_entries.size() == 0)
		return;

	m_drawn = true;
	if ((m_mode & MODE_UNSORTED) == 0)
		sort(m_entries.begin(), m_entries.end());
	if (log) {
		string line = string("[REN] ") + common::FormatString("frame=%i", common::FrameCount());
	}

	if (m_entries.size() != 0) {
		if (m_mode & MODE_MARKER) {
			arl::ArlInstance *gl = arl::Gl();

			ScePafGraphics_199BD5F2(gl, 0);
			gl->PushMarker(m_name.c_str());
		}
		for (uint32_t i = 0; i != m_entries.size(); i++) {
			m_entries[i].m_call->Exec();
			m_entries[i].m_call->m_queued = false;
		}
		if (m_mode & MODE_MARKER) {
			arl::ArlInstance *gl = arl::Gl();

			gl->PopMarker();
			ScePafGraphics_29F5AA8B(gl, NULL, false);
		}
	}
	m_last_count = m_entries.size();
	if ((m_mode & MODE_KEEP) == 0) {
		for (uint32_t i = 0; i != m_entries.size(); i++)
			m_entries[i].m_call->~DrawLayerCall();
		m_entries.resize(0, Entry(0, NULL));
	}
	m_last_used = m_used;
	m_used = 0;
}

inline void DrawEnv::FlushLayers(DrawLayer *layer)
{
	if (layer->m_state != 1)
		return;

	layer->m_first = m_index;

	DrawCallList *end = m_cursor;

	while (end != &m_lists[1] && DrawLayer::FromOrder(end)->m_first == m_index)
		end = end->m_next;
	if (m_cursor == end)
		return;

	while (m_cursor != end) {
		int32_t index = m_index;

		if (DrawLayer::FromOrder(m_cursor)->m_entries.size() == 0) {
			DrawLayer::FromOrder(m_cursor)->m_last = index;
		} else {
			DrawLayer::FromOrder(m_cursor)->Draw(index, m_counted);
			m_used_total += DrawLayer::FromOrder(m_cursor)->m_last_used;
			m_drawn_total += DrawLayer::FromOrder(m_cursor)->m_last_count;
		}
		m_cursor = m_cursor->m_next;
	}
}

inline void DrawEnv::SetPriority(DrawLayer *layer, int32_t priority)
{
	layer->m_priority = priority;
}

inline void DrawEnv::SetDrawMode(DrawLayer *layer, int32_t mode)
{
	layer->m_draw_mode = mode;
}

}
}

#endif
