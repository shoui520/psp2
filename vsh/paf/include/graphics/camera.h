/*
 * The camera the panels are seen by: its projection, where its eye is, and the matrices it
 * loads into the front end's stacks while it draws. Its code is 0x8109b314..0x8109c554 in
 * 3.65; no record or string names it, so every name here is ours.
 */
#ifndef PAF_GRAPHICS_CAMERA_H
#define PAF_GRAPHICS_CAMERA_H

#include <stdint.h>
#include <vectormath.h>
#include <common/shared_ptr.h>
#include <math/ngp/matrix.h>
#include <math/ngp/quaternion.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <src/std/math.h>

namespace paf {
namespace graph {

/* a position and a rotation */
class Pose {
public:
	Pose()
		: m_position(sce::Vectormath::Simd::Aos::Vector4::wAxis()),
		  m_rotation(math::quaternion::Identity())
	{
	}

	/* the position and the rotation of a matrix */
	Pose(const math::matrix &m) : m_position(m.w()), m_rotation(Normalize(math::quaternion(m)))
	{
	}

	static math::quaternion Normalize(math::quaternion q)
	{
		return math::quaternion(q.getImpl() *
					sce::Vectormath::Simd::recipf(length(q.getImpl())));
	}

	math::v4 m_position;
	math::quaternion m_rotation;
};

/* a pose and the matrix it makes */
class Transform {
public:
	/* Sony's build keeps it out of line */
	__attribute__((noinline)) Transform() : m_world(math::matrix::Identity())
	{
		Update();
	}

	/* the inverse and the pose taken from the world matrix */
	void Update()
	{
		m_inverse = m_world.inverse1();
		m_pose = Pose(m_world);
	}

	void SetWorld(const math::matrix &world)
	{
		m_world = world;
		Update();
	}

	Pose m_pose;
	math::matrix m_world;
	math::matrix m_inverse;
};

/* the point a transform looks at, and the transform it turns to look */
class CameraTarget {
public:
	CameraTarget()
		: m_center(0.0f, 0.0f, -5.0f, 1.0f), m_mode(0),
		  m_transform(new Transform()), m_y_up(false)
	{
	}

	/* the transform placed at eye, turned to look at center (0x81193760) */
	void LookAt(const math::v4 &eye, const math::v4 &center, const math::v4 &up);

	math::v4 m_center;
	/* 1 once LookAt has placed the transform */
	int32_t m_mode;
	common::SharedPtr<Transform> m_transform;
	/* the up of the last LookAt was the y axis, else the z axis is; a byte (Sony's LookAt
	   reads its arguments again after storing it, as after a character store) */
	uint8_t m_y_up;
};

/* what moves the camera: the point it looks at and the steps it moves by */
class CameraControl {
public:
	/* flag 1: the z axis is up; unit: the camera's unit of length */
	CameraControl(uint32_t flags, float unit);

	bool m_moving;
	CameraTarget m_target;
	float m_turn_step;
	/* 0.01 of the camera's unit on each axis */
	math::v4 m_move_step;
	Pose m_pose;
	bool m_input[5];
};

/* a line from the eye through a point of the screen, and what it hits first */
class Ray {
public:
	Ray(const math::v4 &origin, const math::v4 &direction)
		: m_origin(origin), m_direction(direction.unit()), m_distance(0.0f), m_hit(false),
		  m_hit_index(-1)
	{
	}

	math::v4 m_origin;
	math::v4 m_direction;
	math::v4 m_hit_point;
	sce::Vectormath::Simd::floatInVec m_distance;
	bool m_hit;
	int32_t m_hit_index;
};

class Camera {
public:
	enum {
		/* the 2D view: units are pixels */
		FLAG_2D = 2
	};

	/* a perspective projection from the field of view along an axis (1 the vertical, else
	   the horizontal) and the aspect */
	class Projection {
	public:
		Projection() : m_fixed(false)
		{
		}

		/* the projection made from the field of view */
		void Perspective(float fov, int32_t fov_axis, float aspect, float near_z, float far_z)
		{
			float left;
			float right;
			float bottom;
			float top;
			math::matrix matrix;

			if (fov_axis == 1) {
				float t = sce_paf_tanf(fov * 0.5f);

				top = near_z * t;
				bottom = -(near_z * t);
				right = top * aspect;
				left = -(top * aspect);
				matrix = math::matrix::Frustum(left, right, bottom, top, near_z, far_z);
			} else {
				float t = sce_paf_tanf(fov * 0.5f);

				right = near_z * t;
				left = -(near_z * t);
				top = right / aspect;
				bottom = -top;
				matrix = math::matrix::Frustum(left, right, bottom, top, near_z, far_z);
			}

			m_matrix = matrix;
			m_near = near_z;
			m_far = far_z;
			m_left = left;
			m_right = right;
			m_bottom = bottom;
			m_top = top;
			m_fov = fov;
			m_fov_axis = fov_axis;
			m_aspect = aspect;
			m_orthographic = false;
			m_fixed = false;
		}

		/* made again for an aspect, unless it is fixed */
		__attribute__((always_inline)) void SetAspect(float aspect)
		{
			if (m_fixed)
				return;

			float fov = m_fov;
			float near_z = m_near;
			int32_t fov_axis = m_fov_axis;
			float far_z = m_far;

			Perspective(fov, fov_axis, aspect, near_z, far_z);
		}

		/* the horizontal field of view */
		float GetFovX() const
		{
			if (m_fov_axis != 0)
				return sce_paf_atanf(sce_paf_tanf(m_fov * 0.5f) * m_aspect) * 2.0f;
			return m_fov;
		}

		/* the vertical field of view */
		float GetFovY() const
		{
			if (m_fov_axis == 0)
				return sce_paf_atanf(sce_paf_tanf(m_fov * 0.5f) / m_aspect) * 2.0f;
			return m_fov;
		}

		math::matrix m_matrix;
		float m_near;
		float m_far;
		float m_left;
		float m_right;
		float m_bottom;
		float m_top;
		float m_fov;
		int32_t m_fov_axis;
		float m_aspect;
		/* the rectangle is seen without perspective */
		bool m_orthographic;
		/* not made again for a new aspect */
		bool m_fixed;
	};

	Camera(uint32_t flags);

	virtual ~Camera()
	{
	}

	/* the projection by the view, turned by the roll */
	virtual math::matrix GetViewProjection() const;
	virtual math::v4 GetEyePos() const;
	/* the projection and view loaded into the front end's stacks, over what was there */
	virtual void PushMatrices();
	virtual void PopMatrices();
	/* the unit of length: a pixel in the 2D view */
	virtual float GetUnit();

	/* the projection set for the 2D or the 3D view in an aspect, and the eye placed */
	void Reset(float aspect);
	/* the eye placed at a position, looking at the origin */
	void SetPosition(const math::v4 &position);
	/* the projection made again for the viewport's aspect */
	void UpdateAspect();
	/* the projection and view loaded into the tops of the front end's stacks */
	void LoadMatrices();
	/* a draw that does nothing (queued by 0x8109c648) */
	void DrawNothing();
	/* the ray through a point of the screen, from -1 to 1 on both axes */
	Ray GetRay(const math::v2 &point);
	math::matrix &GetResetWorld();
	void SetRoll(float roll);

	float m_unit;
	/* the world matrix Reset kept */
	math::matrix m_reset_world;
	Transform m_transform;
	Projection m_projection;
	uint32_t m_flags;
	/* the turn about the view axis, in radians */
	float m_roll;
	CameraControl m_control;
};

}
}

#endif
