/*
 * DrawEnv::BeginFrame: a frame of the draw environment begun (libpaf's build emits it once,
 * 0x8123d34c TOOL, for ui::Environment's frame draw). Apart from draw_env.h because deleting a
 * layer instantiates the destructors of its vectors, which page_turn_renderer.h specializes;
 * the file's and every name here are ours.
 */
#ifndef PAF_GRAPHICS_DRAWOBJ_DRAW_ENV_BEGIN_H
#define PAF_GRAPHICS_DRAWOBJ_DRAW_ENV_BEGIN_H

#include <stdint.h>
#include <graphics/drawobj/draw_env.h>

namespace paf {
namespace graph {

inline void DrawEnv::BeginFrame(bool counted)
{
	DrawCallList *node;

	m_index = m_count;
	m_counted = counted || m_ended;
	m_ended = false;

	for (node = m_lists[1].m_next; node != &m_lists[1]; node = node->m_next)
		DrawLayer::FromOrder(node)->m_drawn = false;

	for (node = m_lists[0].m_next; node != &m_lists[0]; node = node->m_next) {
		DrawLayer *layer = static_cast<DrawLayer *>(static_cast<DrawListNode<0> *>(node));

		layer->m_frame_priority = layer->m_priority;
		if (layer->m_state == layer->m_draw_mode)
			continue;
		if (layer->m_draw_mode == 2)
			static_cast<DrawListNode<1> *>(layer)->InsertBefore(&m_lists[2]);
		if (layer->m_draw_mode == 3)
			static_cast<DrawListNode<1> *>(layer)->InsertBefore(&m_lists[3]);
		if (layer->m_draw_mode == 1)
			static_cast<DrawListNode<1> *>(layer)->InsertBefore(&m_lists[1]);
		layer->m_state = layer->m_draw_mode;
	}

	for (node = m_lists[1].m_next; node != &m_lists[1]; node = node->m_next)
		DrawLayer::FromOrder(node)->m_key_count = 0;

	while (m_lists[3].m_next != &m_lists[3])
		delete DrawLayer::FromOrder(m_lists[3].m_next);

	uint32_t count = 0;

	for (node = m_lists[1].m_next; node != &m_lists[1]; node = node->m_next)
		count++;
	if (count >= 2) {
		node = m_lists[1].m_next->m_next;
		while (node != &m_lists[1]) {
			DrawCallList *prev = node->m_prev;
			DrawCallList *next = node->m_next;
			DrawCallList *pos = node;

			while (DrawLayer::OrderLess(DrawLayer::FromOrder(*node),
						    DrawLayer::FromOrder(*prev))) {
				pos = prev;
				if (prev == m_lists[1].m_next)
					break;
				prev = prev->m_prev;
			}
			if (pos != node)
				static_cast<DrawListNode<1> *>(DrawLayer::FromOrder(node))
					->InsertBefore(static_cast<DrawListNode<1> *>(
						DrawLayer::FromOrder(pos)));
			node = next;
		}
	}
	m_cursor = m_lists[1].m_next;
	m_used_total = 0;
	m_drawn_total = 0;
}

}
}

#endif
