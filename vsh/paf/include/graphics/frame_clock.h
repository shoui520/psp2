/*
 * paf::graph::FrameClock: a time taken when the clock is made, which the display's frame and an
 * animated glyph's frames are timed from (names ours).
 */
#ifndef PAF_GRAPHICS_FRAME_CLOCK_H
#define PAF_GRAPHICS_FRAME_CLOCK_H

#include <scetypes.h>
#include <timer/timer.h>

namespace paf {
namespace graph {

class FrameClock {
public:
	FrameClock() : m_start(get_time_internal())
	{
	}

	SceUInt64 m_start;
};

}
}

#endif
