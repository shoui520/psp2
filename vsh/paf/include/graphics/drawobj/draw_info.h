/* paf::graph::DrawInfo: where the draws of a widget's draw objects are queued. */
#ifndef PAF_GRAPHICS_DRAWOBJ_DRAW_INFO_H
#define PAF_GRAPHICS_DRAWOBJ_DRAW_INFO_H

#include <stdint.h>

namespace paf {
namespace graph {

/* where a widget's draw objects queue their draws: the order counted up per draw, under the
   widget's layer and sub-layer; the deepest layer the camera's walk reached, and a word the
   walk hands down unread (names ours) */
class DrawInfo {
public:
	int32_t m_layer;
	int32_t m_max_layer;
	int32_t m_order;
	int32_t m_sub_layer;
	int32_t m_reserved;
};

}
}

#endif
