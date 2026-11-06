/*
 * The custom renderer's trail (DEBUGGER 0x810d6934..0x810d7db0): a point the moves queued here
 * move, around a rectangle, held or along a panel's outline, each until its timer finishes,
 * written each frame into the renderer's ring of points while the trail is on. No record names
 * the file or anything in it: every name is ours.
 */
#include <stdint.h>
#include <gxm.h>
#include <vectormath.h>
#include <graphics/drawobj/effect_path.h>
#include <graphics/color.h>
#include <graphics/arl/arl_defines.h>
#include <math/math.h>
#include <src/std/math.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/drawobj/draw_env_flush.h>
#include <graphics/drawobj/panel_obj.h>
#include <graphics/effects/custom_trail.h>
#include <graphics/effects/panel_renderer.h>
#include <graphics/signed_angle.h>
#include <std/list>

using sce::Vectormath::Simd::floatInVec;
using sce::Vectormath::Simd::recipf;
using sce::Vectormath::Simd::Aos::Vector2;
using sce::Vectormath::Simd::Aos::Vector3;
using sce::Vectormath::Simd::Aos::Vector4;

namespace paf {
namespace graph {

/* the moves queued, the first running; the timer run while there are none */
list<CustomTrailMove> s_trail_moves;
Timer s_trail_idle_timer(5000.0f);

/* the custom renderer made; its presets the "iboot" ones; the trail on; GetOption's switch */
bool s_trail_ready;
bool s_trail_iboot;
bool s_trail_on;
bool s_custom_on = true;

/* the custom renderer */
static inline CustomRenderer *TrailRenderer()
{
	return ScePafGraphics_4E038C05()->m_ptr->GetCustomRenderer(false).get();
}

/* x to the power, in each component, keeping its sign (an inline function Sony's link keeps
   once; name ours) */
inline __attribute__((noinline)) math::v2 SignedPow(const math::v2 &x, float power)
{
	Vector2 value = x.getImpl();
	Vector2 result(sce_vectormath_powf(absPerElem(value).get64(), Vector2(power).get64()));

	return math::v2(mulPerElem(copySignPerElem(Vector2(1.0f), value), result));
}

/* a corner of a panel's outline, and the way the outline leaves it (names ours) */
class PanelOutlineCorner {
public:
	Vector2 m_corner;
	Vector2 m_dir;
};

/* a point of a panel's outline, and the outline's normal there (names ours) */
class PanelOutlinePoint {
public:
	math::v3 m_pos;
	math::v3 m_normal;
};

/* the point uv of the outline of the panel param draws: x the way round from the top right
   corner, y from its front face (0) to its back (1) (an inline function Sony's link keeps once;
   name ours) */
inline __attribute__((noinline)) PanelOutlinePoint GetPanelOutlinePoint(const PanelParam &param, const math::v2 &uv)
{
	static const PanelOutlineCorner s_corners[8] = {
		{ Vector2(1.0f, 1.0f), Vector2(1.0f, 0.0f) },
		{ Vector2(1.0f, 1.0f), Vector2(0.0f, 1.0f) },
		{ Vector2(-1.0f, 1.0f), Vector2(0.0f, 1.0f) },
		{ Vector2(-1.0f, 1.0f), Vector2(-1.0f, 0.0f) },
		{ Vector2(-1.0f, -1.0f), Vector2(-1.0f, 0.0f) },
		{ Vector2(-1.0f, -1.0f), Vector2(0.0f, -1.0f) },
		{ Vector2(1.0f, -1.0f), Vector2(0.0f, -1.0f) },
		{ Vector2(1.0f, -1.0f), Vector2(1.0f, 0.0f) }
	};
	float around = uv.extract_x() * 8.0f;
	int32_t corner = (int32_t)around;
	float frac = around - (float)corner;
	const PanelOutlineCorner &from = s_corners[corner & 7];
	const PanelOutlineCorner &to = s_corners[(corner + 1) & 7];
	float face_angle = uv.extract_y() * 1.5707964f;
	math::v2 face(sce_paf_cosf(face_angle), sce_paf_sinf(face_angle));
	floatInVec from_length = length(from.m_dir);
	floatInVec to_length = length(to.m_dir);
	math::v2 from_dir(from.m_dir * recipf(from_length));
	math::v2 to_dir(to.m_dir * recipf(to_length));
	float turn = SignedAngle(from_dir, to_dir) * frac;
	float sin = sce_paf_sinf(turn);
	float cos = sce_paf_cosf(turn);
	Vector2 dir(cos * from_dir.extract_x() - sin * from_dir.extract_y(),
		    sin * from_dir.extract_x() + cos * from_dir.extract_y());

	dir = dir * recipf(length(dir)) *
	      (from_length + (to_length - from_length) * floatInVec(frac)).getAsFloat();

	Vector2 edge(param.m_edge);
	Vector2 pos = mulPerElem(lerp(frac, from.m_corner, to.m_corner),
				 Vector2(vget_low_f32(param.m_scale)) * 0.5f - edge);
	Vector3 extent(edge, vgetq_lane_f32(param.m_scale, 2));
	float pair_y = param.m_pair_y;
	float pair_x = param.m_pair_x;
	PanelOutlinePoint point;
	math::v2 bend = SignedPow(face, pair_y);
	math::v2 sweep = SignedPow(math::v2(dir), pair_x);

	point.m_pos = Vector3(sweep.getImpl() * bend.extract_x(), bend.extract_y());
	bend = SignedPow(face, 2.0f - pair_y);
	sweep = SignedPow(math::v2(dir), 2.0f - pair_x);

	Vector3 normal = divPerElem(Vector3(sweep.getImpl() * bend.extract_x(), bend.extract_y()),
				    extent);

	point.m_pos = Vector3(pos, 0.0f) + mulPerElem(point.m_pos.getImpl(), extent);
	point.m_normal = normal * recipf(length(normal));
	return point;
}

}
}

using namespace paf;
using namespace paf::graph;

void ScePafGraphics_23095DE2(void)
{
	if (!s_trail_ready) {
		ScePafGraphics_4E038C05()->m_ptr->GetCustomRenderer(true);
		s_trail_ready = true;
	}
	s_custom_on = true;
	s_trail_on = true;
}

void ScePafGraphics_0EC5770E(void)
{
	if (!s_trail_ready)
		return;
	s_custom_on = false;
	s_trail_on = false;
}

void ScePafGraphics_9409C495(void)
{
	if (!s_trail_ready)
		return;
	if (ScePafGraphics_4E038C05()->m_ptr->GetCustomRenderer(false).get() != NULL)
		TrailRenderer()->Update(arl::Gl()->m_frame_time);
}

bool ScePafGraphics_4CA96E38(math::v4 *point)
{
	if (s_trail_moves.size() == 0) {
		point->set_w(0.0f);
		if (s_trail_idle_timer.IsFinished())
			return false;
		s_trail_idle_timer.DoTick();
		return true;
	}

	CustomTrailMove move = s_trail_moves.front();
	Timer *timer = move.m_timer;

	if (move.m_kind == CUSTOM_TRAIL_MOVE_PANEL) {
		CustomTrailMove &running = s_trail_moves.front();
		CustomTrailPath *path = running.m_path.get();
		int32_t count = path->m_keys.size();
		float distance = running.m_distance + running.m_size.extract_x() / 20.0f;
		math::v3 pos;

		running.m_distance = distance;
		if (count != 0) {
			float total = path->m_keys[count - 1].m_start + path->m_keys[count - 1].m_length;
			int32_t laps = (int32_t)(distance / total);

			if (distance / total < 0.0f)
				laps--;
			distance -= (float)laps * total;
			if (distance < 0.0f || total <= distance)
				distance = 0.0f;
			running.m_distance = distance;

			int32_t index = 0;

			while (index < count &&
			       distance > path->m_keys[index].m_start + path->m_keys[index].m_length)
				index++;
			if (index == count)
				pos = path->m_keys[count - 1].m_pos;
			else
				pos = path->GetPoint(index) +
				      (path->GetPoint(index + 1) - path->GetPoint(index)) *
					      ((distance - path->m_keys[index].m_start) /
					       path->m_keys[index].m_length);
		}
		*point = math::v4(Vector4(pos.getImpl(), 1.0f) +
				  Vector4(running.m_pos.getImpl().getXYZ(), 0.0f));
	} else {
		float t = timer->m_mapped_x;

		point->set_w(1.0f);
		if (move.m_kind == CUSTOM_TRAIL_MOVE_RECT) {
			float x = move.m_pos.extract_x();
			float y = move.m_pos.extract_y();
			float width = move.m_size.extract_x();
			float height = move.m_size.extract_y();
			float half_width = width * 0.5f;
			float half_height = height * 0.5f;
			float side = height / (height + width);

			if (t < 0.5f) {
				float along = t * 2.0f;
				float up = along / side;

				if (up < 1.0f) {
					point->set_x(x - half_width);
					point->set_y((y - half_height) + height * up);
				} else {
					point->set_x((x - half_width) +
						     ((along - side) / (1.0f - side)) * width);
					point->set_y(y + half_height);
				}
			} else {
				float along = (t - 0.5f) * 2.0f;
				float up = along / side;

				if (up < 1.0f) {
					point->set_x(x + half_width);
					point->set_y((y + half_height) - height * up);
				} else {
					point->set_x((x + half_width) -
						     ((along - side) / (1.0f - side)) * width);
					point->set_y(y - half_height);
				}
			}
			point->set_z(move.m_pos.extract_z());
			point->set_w(1.0f);
		}
	}
	if (timer->IsFinished()) {
		s_trail_moves.pop_front();
		if (timer != NULL)
			delete timer;
		if (s_trail_moves.size() == 0)
			s_trail_idle_timer.ResetTimer();
	} else if (timer != NULL) {
		timer->DoTick();
	}
	return true;
}

void ScePafGraphics_0C2B6324(void)
{
	if (!s_trail_ready)
		return;

	common::SharedPtr<CustomRenderer> &renderer =
		ScePafGraphics_4E038C05()->m_ptr->GetCustomRenderer(false);

	if (renderer.get() == NULL)
		return;

	math::v4 point = renderer->m_trail.m_points[renderer->m_trail.m_last];

	renderer->m_trail_moved = false;
	ScePafGraphics_4CA96E38(&point);
	if (s_trail_on && !renderer->m_trail_moved) {
		if (point.extract_w() > 0.01f) {
			common::SharedPtr<Camera> camera =
				TrailRenderer()->m_camera;

			point = math::v4(Vector4((point / camera->GetUnit()).getImpl().getXYZ(), 1.0f));
		} else {
			point = math::v4(mulPerElem(point.getImpl(), Vector4(1.0f, 1.0f, 1.0f, 0.0f)));
		}

		CustomTrailPoints &trail = renderer->m_trail;
		uint32_t index = trail.m_next;
		uint32_t next = (index + 1) % trail.m_count;

		trail.m_last = index;
		trail.m_points[index] = point;
		trail.m_next = next;
	}
	renderer->GetOption()->SetEnabled(s_custom_on);
	renderer->GetTrailOption()->SetEnabled(s_trail_on);
	ScePafGraphics_4E038C05();
	renderer->m_rs_custom->m_draw_mode = s_custom_on ? 1 : 2;
	ScePafGraphics_4E038C05();
	renderer->m_rs_trail->m_draw_mode = s_trail_on ? 1 : 2;
}

void ScePafGraphics_67C25F27(void)
{
	if (!s_trail_ready)
		return;

	if (TrailRenderer()->m_rs_custom->m_state == 1)
		ScePafGraphics_4E038C05()->m_ptr->m_draw_env->FlushLayers(
			TrailRenderer()->m_rs_custom);
	if (TrailRenderer()->m_rs_trail->m_state == 1)
		ScePafGraphics_4E038C05()->m_ptr->m_draw_env->FlushLayers(
			TrailRenderer()->m_rs_trail);
}

void ScePafGraphics_9569E74C(int32_t iboot, float time)
{
	if (!s_trail_ready || s_trail_iboot == iboot)
		return;

	s_trail_iboot = iboot != 0;
	if (iboot == 1) {
		TrailRenderer()->m_manager.StartTransition("iboot", time, 0, 0.09f);
		TrailRenderer()->m_trail_manager.StartTransition("iboot", time, 0, 0.09f);
	} else {
		TrailRenderer()->m_manager.StartTransition("default", time, 0, 0.09f);
		TrailRenderer()->m_trail_manager.StartTransition("default", time, 0, 0.09f);
	}
}

void ScePafGraphics_DF94063C(const math::v4 &pos, const math::v4 &size, Timer *timer)
{
	if (timer == NULL)
		return;

	CustomTrailMove move;

	move.m_kind = CUSTOM_TRAIL_MOVE_RECT;
	move.m_pos = pos;
	move.m_size = size;
	move.m_timer = timer;
	s_trail_moves.push_back(move);
}

void ScePafGraphics_E5CF8441(const math::v4 &pos, float value, Timer *timer)
{
	if (timer == NULL)
		return;

	CustomTrailMove move;

	move.m_kind = CUSTOM_TRAIL_MOVE_HOLD;
	move.m_pos = pos;
	move.m_size = math::v4(Vector4(value));
	move.m_timer = timer;
	s_trail_moves.push_back(move);
}

void ScePafGraphics_6226C883(const CustomTrailMove &move)
{
	if (move.m_timer != NULL)
		s_trail_moves.push_back(move);
}

CustomTrailMove::CustomTrailMove(const math::v4 &pos, const math::v4 &size, const math::v2 &edge,
				 const math::v2 &pair, Timer *timer)
	: m_kind(CUSTOM_TRAIL_MOVE_PANEL), m_pos(pos), m_size(size), m_edge(edge), m_pair(pair),
	  m_timer(timer), m_distance(0.0f), m_path(new CustomTrailPath)
{
	PanelParam param;

	param.m_pos = Vector4(m_pos.getImpl().getXYZ(), 1.0f).get128();
	param.m_scale = m_size.getImpl().get128();
	param.m_edge = m_edge.getImpl().get64();
	param.m_pair_y = m_pair.extract_x();
	param.m_pair_x = m_pair.extract_y();
	for (int32_t side = 0; side < 8; side++) {
		for (int32_t i = 0; i < 6; i++) {
			PanelOutlinePoint point = GetPanelOutlinePoint(
				param, math::v2(((float)i / 5.0f + (float)side) * 0.125f, 0.5f));
			vector<CustomTrailKey> &keys = m_path->m_keys;
			int32_t count = keys.size();
			float distance = 0.0f;
			float start = 0.0f;

			if (count != 0) {
				CustomTrailKey &last = keys[count - 1];
				float step = length(point.m_pos.getImpl() - last.m_pos.getImpl())
						     .getAsFloat();

				if (step < 0.0001f)
					continue;
				last.m_length = step;
				start = last.m_start + step;
				if (m_path->m_loop)
					distance = length(point.m_pos.getImpl() -
							  keys[0].m_pos.getImpl())
							   .getAsFloat();
			}
			keys.push_back(CustomTrailKey(point.m_pos, distance, start));
		}
	}
}

