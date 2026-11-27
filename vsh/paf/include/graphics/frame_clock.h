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
#ifdef PAF_BUILD_2014
	__attribute__((always_inline)) FrameClock() : m_start(get_time_internal())
#else
	FrameClock() : m_start(get_time_internal())
#endif
	{
	}

	SceUInt64 m_start;
};

}
}

#endif
