/*
 * EnvProbeBackground: the light probes' cube maps drawn around the eye. The probes are loaded
 * from the background folder the first time the background is used, each one twice: its map,
 * and its spherical-harmonics projection. The pointer turns them with a trackball; reflections
 * sample the chosen probe's angular map and refractions its projection's. The file's and every
 * name here are ours ("env name:" is the message Sony's build composes and does not print).
 */
#include <stdint.h>
#include <gxm.h>
#include <vectormath.h>
#include <graphics/drawobj/effect_path.h>
#include <graphics/color.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/effects/background.h>
#include <graphics/gutil.h>
#include <math/math.h>
#include <math/ngp/quaternion.h>
#include "../../std/math.h"

using sce::Vectormath::Simd::floatInVec;
using sce::Vectormath::Simd::Aos::Vector2;
using sce::Vectormath::Simd::Aos::Vector3;

namespace paf {

/* Sony's build calls the probe list's push_back out of line */
template<>
inline __attribute__((noinline)) int
vector<common::SharedPtr<graph::EnvProbe> >::push_back(
	const common::SharedPtr<graph::EnvProbe> &val)
{
	int result;

	if (count + 1 <= limit || (result = reserve(limit + 10)) != 0) {
		new (&ptr[count]) common::SharedPtr<graph::EnvProbe>(val);
		result = ++count;
	}
	return result;
}

namespace graph {

EnvProbe::EnvProbe(const string &name, bool sh) : m_name(name)
{
	arl::ArlInstance *gl = arl::Gl();

	m_angular_map.Load(m_name + (sh ? ".sh_proj" : "") + string(".ang.dds"), false);
	m_cube.SetDirectory(m_name + (sh ? ".sh" : ""));
	m_cube.LoadFaces(gl->m_default_allocator);
	m_cube.Init();
}

/* the probes of the background folder, each as its map and its projection: the debug probe,
   and the others when all */
static void LoadProbes(vector<common::SharedPtr<EnvProbe> > &probes, bool all)
{
	string dir = s_background_path + "/";

	probes.push_back(common::SharedPtr<EnvProbe>(new EnvProbe(dir + "debug", false)));
	probes.push_back(common::SharedPtr<EnvProbe>(new EnvProbe(dir + "debug", true)));
	if (all) {
		probes.push_back(common::SharedPtr<EnvProbe>(
			new EnvProbe(dir + "grace_probe.float.gz", false)));
		probes.push_back(common::SharedPtr<EnvProbe>(
			new EnvProbe(dir + "grace_probe.float.gz", true)));
		probes.push_back(common::SharedPtr<EnvProbe>(
			new EnvProbe(dir + "beach_probe.float.gz", false)));
		probes.push_back(common::SharedPtr<EnvProbe>(
			new EnvProbe(dir + "beach_probe.float.gz", true)));
		probes.push_back(common::SharedPtr<EnvProbe>(new EnvProbe(dir + "sky_03", false)));
		probes.push_back(common::SharedPtr<EnvProbe>(new EnvProbe(dir + "sky_03", true)));
		probes.push_back(common::SharedPtr<EnvProbe>(new EnvProbe(dir + "ls01.tga", false)));
		probes.push_back(common::SharedPtr<EnvProbe>(new EnvProbe(dir + "ls01.tga", true)));
	}
}

EnvProbeBackground::EnvProbeBackground(bool all)
	: m_index(0), m_rotation(math::matrix::Identity()), m_all(all), m_loaded(false)
{
}

inline void EnvProbeBackground::Prepare()
{
	if (m_loaded)
		return;
	LoadProbes(m_probes, m_all);
	m_rotation.assign(math::matrix(
		math::quaternion(0.0809269994f, 0.847034991f, 0.503383994f, 0.150284007f)));
	m_loaded = true;
}

inline common::SharedPtr<EnvProbe> EnvProbeBackground::GetProbe(int32_t index)
{
	int32_t count = (int32_t)m_probes.size();
	int32_t i = index % count;

	if (i < 0)
		i += count;
	return m_probes[i];
}

void EnvProbeBackground::Init(common::SharedPtr<Camera> camera,
			      common::SharedPtr<SharedTexture> texture)
{
	m_camera = camera;
	m_shared_texture = texture;
}

void EnvProbeBackground::Load(const string &dir)
{
}

void EnvProbeBackground::Draw()
{
	Prepare();

	arl::ArlInstance *gl = arl::Gl();

	gl->CullMode(arl::CULL_CW);
	gl->ColorMask(true, true, true, true);
	gl->DepthMask(false, arl::FRONT_AND_BACK);
	gl->Disable(arl::BLEND);
	gl->Disable(arl::DEPTH_TEST);
	m_camera->PushMatrices();
	ScePafGraphics_199BD5F2(gl, 0);

	/* the model matrix pushed (MatrixStack::PushTop, its first entry the identity) */
	arl::MatrixStack &stack = gl->m_model;

	if (stack.m_depth == 0) {
		stack.m_depth++;

		arl::MatrixStack::Entry &top = stack.m_stack[stack.m_depth - 1];

		top.m_matrix = math::matrix::Identity();
		top.m_inverse = math::matrix::Identity();
		top.m_inverse_valid = true;
		top.m_inverse_entry = stack.m_depth - 1;
		top.m_rigid = true;
	} else {
		arl::MatrixStack::Entry &below = stack.m_stack[stack.m_depth - 1];
		arl::MatrixStack::Entry &top = stack.m_stack[stack.m_depth];

		top.m_matrix.assign(below.m_matrix);
		top.m_inverse_valid = below.m_inverse_valid;
		top.m_inverse_entry = below.m_inverse_entry;
		top.m_rigid = below.m_rigid;
		stack.m_depth++;
	}

	/* turned by the rotation, about the eye (MatrixStack::MultMatrix) */
	{
		math::matrix turn(m_rotation.x(), m_rotation.y(), m_rotation.z(), m_camera->GetEyePos());
		arl::MatrixStack::Entry &top = stack.m_stack[stack.m_depth - 1];

		top.m_matrix = top.m_matrix * turn;
		top.m_inverse_valid = false;
		top.m_inverse_entry = stack.m_depth - 1;
		top.m_rigid &= true;
	}

	/* the cube just past the near plane */
	Camera::Projection &projection = m_camera->m_projection;
	float scale = projection.m_near + (projection.m_far - projection.m_near) * 0.1f;

	{
		common::SharedPtr<EnvProbe> probe = GetProbe(m_index);

		probe->m_cube.Draw(scale);
	}
	stack.Pop();
	ScePafGraphics_29F5AA8B(gl, NULL, false);
	m_camera->PopMatrices();
}

void EnvProbeBackground::Update()
{
	Prepare();

	arl::ArlInstance *gl = arl::Gl();
	gutil::debug::Mouse *mouse = gutil::debug::Mouse::GetInstance();
	Vector2 point((float)mouse->m_pointers[0].m_x * 2.0f / (float)mouse->m_screen_width - 1.0f,
		      1.0f - (float)mouse->m_pointers[0].m_y * 2.0f / (float)mouse->m_screen_height);
	arl::Rect &viewport = gl->m_viewport.Top().m_viewport;
	float aspect = (float)(viewport.m_x1 - viewport.m_x0) / (float)(viewport.m_y1 - viewport.m_y0);
	math::v3 center(0.0f);
	math::v3 origin(Vector3(mulPerElem(point, Vector2(aspect, 1.0f)), 2.0f));
	math::v3 direction(-Vector3::zAxis());

	m_trackball.Update(center, origin, direction,
			   gutil::debug::Mouse::GetInstance()->m_buttons[0].m_pressed,
			   gutil::debug::Mouse::GetInstance()->m_buttons[0].m_down, m_rotation, 1.0f);
}

void EnvProbeBackground::Reset(bool force)
{
	Prepare();

	string name = m_probes[m_index]->m_name;

	/* the message, printed by no build */
	"env name:" + name;
}

math::matrix EnvProbeBackground::GetUVTransform()
{
	Prepare();
	return m_rotation;
}

void EnvProbeBackground::SetUVTransform(const math::matrix &transform)
{
	Prepare();
	m_rotation.assign(transform);
}

arl::TextureSource EnvProbeBackground::GetReflectionTexture()
{
	Prepare();

	common::SharedPtr<EnvProbe> probe = GetProbe(m_index + ((m_index & 1) != 0 ? -1 : 0));

	return probe->m_angular_map;
}

arl::TextureSource EnvProbeBackground::GetRefractionTexture()
{
	Prepare();

	common::SharedPtr<EnvProbe> probe = GetProbe(m_index + ((m_index & 1) == 0));

	return probe->m_angular_map;
}

common::SharedPtr<arl::Texture2D> EnvProbeBackground::GetSharedTexture()
{
	Prepare();

	SharedTexture *shared = m_shared_texture.get();
	arl::DefaultTextures *textures = arl::Gl();

	if (shared->get() != NULL)
		return *shared;
	return textures->m_white;
}

inline bool Trackball::Hit(math::v3 center, math::v3 origin, math::v3 direction, math::v3 &point,
			   float radius)
{
	Vector3 dir = direction.getImpl();

	point = math::v3(origin.getImpl() + dir * dot(dir, center.getImpl() - origin.getImpl()) /
						    dot(dir, dir));

	float left = radius * radius - lengthSqr(point.getImpl() - center.getImpl()).getAsFloat();

	if (left < 0.0f)
		return false;
	point = math::v3(point.getImpl() -
			 dir * sce::Vectormath::Simd::recipf(length(dir)) * __builtin_sqrtf(left));
	return true;
}

/* the turn by the angle about the unit axis */
static inline __attribute__((always_inline)) math::matrix AxisRotation(Vector3 axis, float c,
									 float s)
{
	float x = axis.getX();
	float y = axis.getY();
	float z = axis.getZ();
	float t = 1.0f - c;

	return math::matrix(math::v4(x * x * t + c, x * y * t + z * s, x * z * t - y * s, 0.0f),
			    math::v4(y * x * t - z * s, y * y * t + c, y * z * t + x * s, 0.0f),
			    math::v4(z * x * t + y * s, z * y * t - x * s, z * z * t + c, 0.0f),
			    math::v4(0.0f, 0.0f, 0.0f, 1.0f));
}

/* GetSignedAngle, which Sony's build inlines here */
static inline __attribute__((always_inline)) float SignedAngle(const math::v2 &from,
							       const math::v2 &to)
{
	Vector2 a = from.getImpl() * sce::Vectormath::Simd::recipf(length(from.getImpl()));
	Vector2 b = to.getImpl() * sce::Vectormath::Simd::recipf(length(to.getImpl()));
	float angle = sce_paf_acosf(math::ClampUnit(dot(b, a).getAsFloat()));
	floatInVec cross = floatInVec(to.extract_y() * from.extract_x() -
				      to.extract_x() * from.extract_y());

	return angle * sce::Vectormath::Simd::select(floatInVec(-1.0f), floatInVec(1.0f),
						     cross > floatInVec(0.0f))
			       .getAsFloat();
}

inline void Trackball::Update(const math::v3 &center, const math::v3 &origin,
			      const math::v3 &direction, bool pressed, bool down,
			      math::matrix &rotation, float radius)
{
	math::v3 point(0.0f);
	bool on_sphere = Hit(center, origin, direction, point, radius);

	if (!down)
		return;
	if (pressed) {
		m_start_point = point;
		m_start_on_sphere = on_sphere;
		m_start_rotation.assign(rotation);
	}

	Vector3 to = point.getImpl() - center.getImpl();
	floatInVec to_length = length(to);

	if (!(to_length > floatInVec(0.00001f)).getAsBool())
		return;

	Vector3 from = m_start_point.getImpl() - center.getImpl();
	floatInVec from_length = length(from);

	if (!(from_length > floatInVec(0.00001f)).getAsBool())
		return;

	Vector3 a = to * sce::Vectormath::Simd::recipf(to_length);
	Vector3 b = from * sce::Vectormath::Simd::recipf(from_length);
	float angle = -sce_paf_acosf(sce::Vectormath::Simd::fminf(
		sce::Vectormath::Simd::fmaxf(dot(b, a), floatInVec(-1.0f)), floatInVec(1.0f))
					     .getAsFloat());

	if (__builtin_fabsf(angle) <= 0.001f)
		return;

	math::matrix turn;

	if (m_start_on_sphere) {
		Vector3 normal = cross(a, b);
		Vector3 axis = normal * sce::Vectormath::Simd::recipf(length(normal));
		float c = sce_paf_cosf(angle);
		float s = sce_paf_sinf(angle);

		turn = AxisRotation(axis, c, s);
	} else {
		float roll = -SignedAngle(math::v2(a.getX(), a.getY()), math::v2(b.getX(), b.getY()));
		float s = sce_paf_sinf(roll);
		float c = sce_paf_cosf(roll);

		turn = math::matrix(math::v4(c, s, 0.0f, 0.0f), math::v4(-s, c, 0.0f, 0.0f),
				    math::v4(0.0f, 0.0f, 1.0f, 0.0f), math::v4(0.0f, 0.0f, 0.0f, 1.0f));
	}
	rotation = turn * m_start_rotation;

	/* kept a rotation: the turn's drift taken out through a quaternion */
	math::v4 translation = rotation.w();
	math::matrix unit = math::quaternion(rotation).toMatrix(0.0001f);

	rotation = math::matrix(unit.x(), unit.y(), unit.z(), translation);
}

}
}
