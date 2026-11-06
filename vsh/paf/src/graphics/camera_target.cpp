/*
 * The camera's target (the file's name ours): LookAt places the target's transform at the
 * eye, turned to look at the center.
 */
#include <stddef.h>
#include <stdint.h>
#include <vectormath.h>
#include <common/shared_ptr.h>
#include <graphics/color.h>
#include <graphics/camera.h>
#include <math/ngp/matrix.h>
#include <math/ngp/quaternion.h>
#include <math/ngp/v4.h>

namespace paf {
namespace graph {

void CameraTarget::LookAt(const math::v4 &eye, const math::v4 &center, const math::v4 &up)
{
	Transform *transform = m_transform.get();

	if (transform == NULL)
		return;

	m_mode = 1;
	m_center = math::v4(sce::Vectormath::Simd::Aos::Vector4(center.getImpl().getXYZ(), 1.0f));
	m_y_up = up == math::v4(sce::Vectormath::Simd::Aos::Vector4::yAxis());

	math::matrix look = math::matrix::LookAt(
		math::v4(sce::Vectormath::Simd::Aos::Vector4(eye.getImpl().getXYZ(), 1.0f)),
		math::v4(sce::Vectormath::Simd::Aos::Vector4(center.getImpl().getXYZ(), 1.0f)),
		math::v4(sce::Vectormath::Simd::Aos::Vector4(up.getImpl().getXYZ(), 0.0f)), 0.9999f);
	Pose pose(look);
	math::matrix world(pose.m_rotation);

	world.set_w(pose.m_position);
	transform->SetWorld(world);
}

}
}
