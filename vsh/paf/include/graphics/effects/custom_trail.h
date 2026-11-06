/*
 * The custom renderer's trail: a point moved by a queue of moves, each run by its timer, around
 * a rectangle, held, or along a panel's outline, and written each frame into the renderer's ring
 * of points while the trail is on. The exports' names are ScePafGraphics_<NID>; the classes, the
 * file's and every other name are ours.
 */
#ifndef PAF_GRAPHICS_EFFECTS_CUSTOM_TRAIL_H
#define PAF_GRAPHICS_EFFECTS_CUSTOM_TRAIL_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <std/vector>
#include <timer/timer.h>

namespace paf {
namespace graph {

/* a point of a panel's outline, and the outline's length up to it and past it */
class CustomTrailKey {
public:
	CustomTrailKey(const math::v3 &pos, float length, float start)
		: m_pos(pos), m_length(length), m_start(start)
	{
	}

	math::v3 m_pos;
	/* to the next point (to the first from the last of a loop) */
	float m_length;
	/* from the first point */
	float m_start;
};

/* the points of an outline, a loop or not */
class CustomTrailPath {
public:
	CustomTrailPath() : m_loop(false)
	{
	}

	/* the index-th point, around a loop, else the first or the last past them; unset while
	   there are none */
	math::v3 GetPoint(int32_t index) const
	{
		math::v3 pos;
		int32_t count = m_keys.size();

		if (count != 0) {
			if (m_loop) {
				int32_t wrapped = index % count;

				if (wrapped < 0)
					wrapped += count;
				pos = m_keys[wrapped].m_pos;
			} else {
				int32_t clamped = index < 0 ? 0 : index;

				pos = m_keys[count - 1 > clamped ? clamped : count - 1].m_pos;
			}
		}
		return pos;
	}

	vector<CustomTrailKey> m_keys;
	bool m_loop;
};

/* how a move moves the point */
enum CustomTrailMoveKind {
	/* around the rectangle of m_size about m_pos, from its bottom left corner up */
	CUSTOM_TRAIL_MOVE_RECT = 0,
	/* not at all */
	CUSTOM_TRAIL_MOVE_HOLD = 1,
	/* along the outline of the panel m_pos, m_size, m_edge and m_pair give, m_size's x a
	   twentieth of the way a frame */
	CUSTOM_TRAIL_MOVE_PANEL = 2
};

/* a move, until its timer finishes (0x48 bytes) */
class CustomTrailMove {
public:
	CustomTrailMove()
	{
	}

	/* along the outline of the panel at pos of size, edge and pair, its points made (exported
	   as NID 11A133FB, under this name) */
	CustomTrailMove(const math::v4 &pos, const math::v4 &size, const math::v2 &edge,
			const math::v2 &pair, Timer *timer);

	int32_t m_kind;
	math::v4 m_pos;
	math::v4 m_size;
	math::v2 m_edge;
	math::v2 m_pair;
	Timer *m_timer;
	/* how far along the outline */
	float m_distance;
	common::SharedPtr<CustomTrailPath> m_path;
};

}
}

extern "C" {
/* the trail turned on, the custom renderer made the first time (vitasdk: none) */
void ScePafGraphics_23095DE2(void);
/* the trail turned off (vitasdk: none) */
void ScePafGraphics_0EC5770E(void);
/* the custom renderer moved on by the frame's time (vitasdk: none) */
void ScePafGraphics_9409C495(void);
/* the first move run on point: false, with no move, when the idle timer has finished
   (vitasdk: none) */
bool ScePafGraphics_4CA96E38(paf::math::v4 *point);
/* the moves run, the point written into the renderer's ring, the switches and the layers set
   (vitasdk: none) */
void ScePafGraphics_0C2B6324(void);
/* the custom renderer's layers drawn now, those that take draws this frame (vitasdk: none) */
void ScePafGraphics_67C25F27(void);
/* the presets moved to "iboot" (iboot 1) or "default" over time seconds (vitasdk: none) */
void ScePafGraphics_9569E74C(int32_t iboot, float time);
/* a move around a rectangle, a hold, and a move queued (vitasdk: none) */
void ScePafGraphics_DF94063C(const paf::math::v4 &pos, const paf::math::v4 &size,
			     paf::Timer *timer);
void ScePafGraphics_E5CF8441(const paf::math::v4 &pos, float value, paf::Timer *timer);
void ScePafGraphics_6226C883(const paf::graph::CustomTrailMove &move);
}

#endif
