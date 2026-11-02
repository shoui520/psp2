/*
 * The camera the panels are seen by (the file's name ours): its projection, its eye, and the
 * matrices it loads into the front end's stacks.
 */
#include <stdint.h>
#include <vectormath.h>
#include <common/shared_ptr.h>
/* the colours' statics are made before the front end's */
#include <graphics/color.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/camera.h>
#include <math/ngp/matrix.h>
#include <math/ngp/quaternion.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <src/std/math.h>

namespace paf {
namespace graph {

void Camera::SetPosition(const math::v4 &position)
{
	math::v4 up = m_control.m_target.m_y_up ?
		math::v4(sce::Vectormath::Simd::Aos::Vector4::yAxis()) :
		math::v4(sce::Vectormath::Simd::Aos::Vector4::zAxis());

	m_control.m_target.LookAt(position, math::v4(0.0f, 0.0f, 0.0f, 0.0f), up);

	common::SharedPtr<Transform> transform = m_control.m_target.m_transform;
	math::matrix world(transform->m_pose.m_rotation);

	world.set_w(transform->m_pose.m_position);
	m_transform.SetWorld(world);
}

void Camera::Reset(float aspect)
{
	math::v4 z_axis(sce::Vectormath::Simd::Aos::Vector4::zAxis());

	if ((m_flags & FLAG_2D) != 0) {
		m_projection.Perspective(0.80285144f, 0, aspect, 100.0f, 10000.0f);
		SetPosition(z_axis * 480.0f / sce_paf_tanf(m_projection.m_fov * 0.5f));
	} else {
		m_projection.Perspective(0.7679449f, 1, aspect, 0.1f, 1000.0f);
		SetPosition(z_axis * 2.0f);
	}
	m_reset_world = m_transform.m_world;
}

CameraControl::CameraControl(uint32_t flags, float unit)
	: m_moving(false), m_turn_step(0.005f),
	  m_move_step(sce::Vectormath::Simd::Aos::Vector4(0.01f))
{
	m_pose = Pose(math::matrix::Identity());
	for (int32_t i = 0; i < 5; i++)
		m_input[i] = false;
	m_target.m_y_up = (flags & 1) == 0;
	m_move_step = m_move_step * unit;
}

Camera::Camera(uint32_t flags)
	: m_unit((flags & FLAG_2D) != 0 ? 500.0f : 1.0f), m_reset_world(math::matrix::Identity()),
	  m_flags(flags), m_roll(0.0f), m_control(flags, m_unit)
{
	Reset(1.0f);
}

math::matrix Camera::GetViewProjection() const
{
	math::matrix view_projection = m_projection.m_matrix * m_transform.m_inverse;
	float roll = m_roll;

	if (roll != 0.0f) {
		float s = sce_paf_sinf(roll);
		float c = sce_paf_cosf(roll);

		return view_projection * math::matrix(math::v4(c, s, 0.0f, 0.0f),
						      math::v4(-s, c, 0.0f, 0.0f),
						      math::v4(0.0f, 0.0f, 1.0f, 0.0f),
						      math::v4(0.0f, 0.0f, 0.0f, 1.0f));
	}
	return view_projection;
}

math::v4 Camera::GetEyePos() const
{
	return m_transform.m_world.w();
}

void Camera::LoadMatrices()
{
	arl::ArlInstance *gl = arl::Gl();

	gl->m_projection_changed = true;
	gl->m_projection.Load(m_projection.m_matrix);
	gl->m_view_changed = true;
	gl->m_view.LoadKeepInverse(m_transform.m_inverse);
	gl->m_model.LoadIdentity();
}

void Camera::UpdateAspect()
{
	arl::Rect viewport = arl::Gl()->m_viewport.Top().m_viewport;

	m_projection.SetAspect((float)(viewport.m_x1 - viewport.m_x0) /
			       (float)(viewport.m_y1 - viewport.m_y0));
}

void Camera::PushMatrices()
{
	arl::ArlInstance *gl = arl::Gl();

	gl->m_projection_changed = true;
	gl->m_projection.PushTop();
	gl->m_view_changed = true;
	gl->m_view.PushTop();
	gl->m_model.PushTop();
	UpdateAspect();
	LoadMatrices();
}

void Camera::PopMatrices()
{
	arl::ArlInstance *gl = arl::Gl();

	gl->m_view_changed = true;
	gl->m_projection_changed = true;
	gl->m_projection.Pop();
	gl->m_view.Pop();
	gl->m_model.Pop();
}

void Camera::DrawNothing()
{
}

Ray Camera::GetRay(const math::v2 &point)
{
	math::matrix view = m_transform.m_inverse;
	float near_z = m_projection.m_near;
	math::v2 screen = point;
	math::v4 on_near;

	if (!m_projection.m_orthographic) {
		float top = near_z * sce_paf_tanf(m_projection.GetFovY() * 0.5f);
		float right = m_projection.m_aspect * top;

		on_near = math::v4(screen.extract_x() * right, screen.extract_y() * top, -near_z, 1.0f);
	} else {
		float x = m_projection.m_left + (m_projection.m_right - m_projection.m_left) *
			((screen.extract_x() + 1.0f) * 0.5f);
		float y = m_projection.m_bottom + (m_projection.m_top - m_projection.m_bottom) *
			((screen.extract_y() + 1.0f) * 0.5f);

		on_near = math::v4(x, y, -near_z, 1.0f);
	}

	math::v4 direction;

	if (m_projection.m_orthographic)
		direction = math::v4(-sce::Vectormath::Simd::Aos::Vector4::zAxis());
	else
		direction = on_near - math::v4(sce::Vectormath::Simd::Aos::Vector4::wAxis());

	math::matrix world = view.inverse();

	return Ray(math::v4(world.getImpl() * on_near.getImpl()),
		   math::v4(world.getImpl() * direction.getImpl()).unit());
}

float Camera::GetUnit()
{
	return m_unit;
}

math::matrix &Camera::GetResetWorld()
{
	return m_reset_world;
}

void Camera::SetRoll(float roll)
{
	m_roll = roll;
}

}
}
