/*
 * The effects' parameters (the file's name ours): a ParamSet's values moved between keys, the
 * keys' file names and paths, and the manager that edits an effect's sets together.
 */
#include <stddef.h>
#include <stdint.h>
#include <graphics/color.h>
#include <graphics/effects/param.h>
#include <math/ngp/quaternion.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <std/map>
#include <std/set>
#include <std/string>
#include <std/string.h>
#include <std/vector>

namespace paf {
namespace graph {

/* radians to degrees and back: an angle edited in degrees keeps their precision */
#define PARAM_DEGREES_PER_RADIAN	57.29578f
#define PARAM_RADIANS_PER_DEGREE	0.017453292f

string ParamSet::GetLocalName(const string &name)
{
	size_t pos = name.rfind(':');

	if (pos == string::npos)
		return name;
	return string(name, pos + 1, name.length() - pos - 1);
}

void ParamSet::CopyKey(int32_t dst, int32_t src)
{
	for (uint32_t i = 0; i != m_params.size(); i++)
		m_params[i]->CopyKey(dst, src);
}

void ParamSet::StartTransition(float end, int32_t mode, float x)
{
	m_transition.end = end;
	m_transition_mode = mode;
	m_transition_x = x;
	m_transition.time = 0.0f;
	m_transition.start = 0.0f;
	m_transition_active = true;
	for (uint32_t i = 0; i != m_params.size(); i++)
		m_params[i]->CopyKey(PARAM_KEY_START, PARAM_KEY_CURRENT);
}

bool ParamSet::UpdateTransition(float dt)
{
	if (!m_transition_active)
		return false;

	m_transition.time += dt;
	for (uint32_t i = 0; i != m_params.size(); i++)
		m_params[i]->Update(&m_transition, m_transition_mode, m_transition_x);
	if (m_transition.time >= m_transition.end)
		m_transition_active = false;
	return true;
}

void ParamSet::Lerp(float t)
{
	for (uint32_t i = 0; i != m_params.size(); i++)
		m_params[i]->Lerp(t);
}

void ParamSet::findParamAndSet(const char *name, float x)
{
	ParamBase *p = findParam(name);

	if (p != NULL)
		p->setData(x);
}

void ParamSet::findParamAndSet(const char *name, float x, float y)
{
	ParamBase *p = findParam(name);

	if (p != NULL)
		p->setData(x, y);
}

void ParamSet::findParamAndSet(const char *name, float x, float y, float z)
{
	ParamBase *p = findParam(name);

	if (p != NULL)
		p->setData(x, y, z);
}

void ParamSet::findParamAndSet(const char *name, float x, float y, float z, float w)
{
	ParamBase *p = findParam(name);

	if (p != NULL)
		p->setData(x, y, z, w);
}

void ParamSetKeysBase::Clear()
{
	for (KeyMap::iterator it = m_keys.begin(); it != m_keys.end(); ++it) {
		if (it->second != NULL)
			delete it->second;
	}
	m_keys.clear();
}

string ParamSetKeysBase::GetFileName()
{
	string suffix = m_variant.length() != 0 ? "_" + m_variant : string();
	string base = m_name + suffix;
	string file = base + ".txt";
	string name = file;

	for (size_t i = 0; i != name.length(); i++) {
		char c = name[i];

		if (c >= 'A' && c <= 'Z')
			name[i] = c + ('a' - 'A');
	}
	return name;
}

ParamSetKeysManager::ParamSetKeysManager(const string &name)
	: m_name(name), m_editing(false), m_morphing(false), m_anim_time(2.0f)
{
}

void ParamSetKeysManager::Add(ParamSetKeysBase *keys)
{
	m_list.PushBack(keys);
}

void ParamSetKeysManager::AddVariant(const string &variant)
{
	if (m_variants.find(variant) == m_variants.end())
		m_variants.insert(variant);
	for (ParamLink *link = m_list.m_next; link != &m_list; link = link->m_next)
		static_cast<ParamSetKeysBase *>(link)->get_key(variant, true);
}

void ParamSetKeysManager::StartTransition(const string &variant, float time, int32_t mode,
					   float x)
{
	for (ParamLink *link = m_list.m_next; link != &m_list; link = link->m_next)
		static_cast<ParamSetKeysBase *>(link)->StartTransition(variant, time, mode, x);
}

void ParamSetKeysManager::SetPaths(const string &dir, const string &sub)
{
	for (ParamLink *link = m_list.m_next; link != &m_list; link = link->m_next) {
		ParamSetKeysBase *keys = static_cast<ParamSetKeysBase *>(link);

		keys->m_path = dir + "/" + sub + "/" + keys->GetFileName();
		keys->m_rel_path = sub + "/" + keys->GetFileName();
	}
}

void ParamSetKeysManager::SetVariant(const string &variant)
{
	for (ParamLink *link = m_list.m_next; link != &m_list; link = link->m_next)
		static_cast<ParamSetKeysBase *>(link)->SetKey(PARAM_KEY_CURRENT, variant);
}

void ParamSetKeysManager::Update(float dt)
{
	ParamLink *link = m_list.m_next;

	if (m_edit_variant.length() != 0) {
		for (; link != &m_list; link = link->m_next)
			static_cast<ParamSetKeysBase *>(link)->SetKey(PARAM_KEY_CURRENT,
								      m_edit_variant);
	} else {
		for (; link != &m_list; link = link->m_next)
			static_cast<ParamSetKeysBase *>(link)->UpdateTransition(dt);
	}
}

template<>
void ParamKeys<math::quaternion>::Normalize()
{
}

template<>
void ParamKeys<float>::Normalize()
{
	float &key = m_key[PARAM_KEY_EDIT];

	if ((m_flags & PARAM_FLAG_BOOL) != 0)
		key = key > 0.5f ? 1.0f : 0.0f;
	else if ((m_flags & PARAM_FLAG_INT) != 0)
		key = (float)(int32_t)key;
	else if ((m_flags & PARAM_FLAG_INDEX) != 0)
		key = (float)(int32_t)key;
	else if ((m_flags & PARAM_FLAG_DEGREES) != 0)
		key = key * PARAM_DEGREES_PER_RADIAN * PARAM_RADIANS_PER_DEGREE;
}

template<>
void ParamKeys<math::v2>::Normalize()
{
	bool vector = (m_flags & PARAM_FLAG_VECTOR) != 0;

	if (vector)
		m_flags |= PARAM_FLAG_VECTOR;
	else
		m_flags &= ~PARAM_FLAG_VECTOR;
	if (vector && (m_flags & PARAM_FLAG_DEGREES) != 0)
		m_key[PARAM_KEY_EDIT] = m_key[PARAM_KEY_EDIT] * PARAM_DEGREES_PER_RADIAN *
					PARAM_RADIANS_PER_DEGREE;
}

template<>
void ParamKeys<math::v3>::Normalize()
{
	if ((m_flags & PARAM_FLAG_VECTOR) != 0)
		m_flags |= PARAM_FLAG_VECTOR;
	else
		m_flags &= ~PARAM_FLAG_VECTOR;
}

/* a turning point of a curve, where its slope is 0 (names ours) */
struct ParamCurveRoot {
	math::v2 t;
	bool valid;
};

/* a Hermite curve's keys (p0, p1, m0, m1), its polynomial and its bounds (names ours) */
struct ParamCurve {
	math::v4 keys;
	math::v4 coef;
	math::v2 lo;
	math::v2 hi;
};

/*
 * A curve's keys are a Hermite curve's: its bounds over t in [0, 1] are found from its ends and
 * its turning points, and the keys are kept as they are.
 */
template<>
void ParamKeys<math::v4>::Normalize()
{
	bool vector = (m_flags & PARAM_FLAG_VECTOR) != 0;

	if (vector)
		m_flags |= PARAM_FLAG_VECTOR;
	else
		m_flags &= ~PARAM_FLAG_VECTOR;
	if (!vector || (m_flags & PARAM_FLAG_CURVE) == 0)
		return;

	ParamCurve curve;

	curve.keys = m_key[PARAM_KEY_EDIT];

	float p0 = curve.keys.extract_x();
	float p1 = curve.keys.extract_y();
	float m0 = curve.keys.extract_z();
	float m1 = curve.keys.extract_w();

	/* the curve as a t^3 + b t^2 + c t + d */
	curve.coef = math::v4(2.0f * p0 - 2.0f * p1 + m0 + m1, -3.0f * p0 + 3.0f * p1 - 2.0f * m0 - m1,
			      m0, p0);

	float a = curve.coef.extract_x();
	float b = curve.coef.extract_y();
	float c = curve.coef.extract_z();
	float d = curve.coef.extract_w();
	/* its slope 3a t^2 + 2b t + c is 0 at the turning points */
	float slope_a = 3.0f * a;
	float slope_b = 2.0f * b;
	float disc = slope_b * slope_b - 4.0f * slope_a * c;
	ParamCurveRoot roots[2];

	roots[0].valid = false;
	roots[1].valid = false;
	if (disc >= 0.0f) {
		float scale = 1.0f / (2.0f * slope_a);

		if (disc <= 0.0f) {
			roots[0].t = math::v2(-slope_b * scale, -slope_b * scale);
			roots[0].valid = true;
		} else {
			float root = sce::Vectormath::Simd::sqrtf(
				sce::Vectormath::Simd::floatInVec(disc)).getAsFloat();

			roots[0].t = math::v2((-slope_b + root) * scale, (-slope_b + root) * scale);
			roots[0].valid = true;
			roots[1].t = math::v2((-slope_b - root) * scale, (-slope_b - root) * scale);
			roots[1].valid = true;
		}
	}

	float lo_y = p0 < p1 ? p0 : p1;
	float hi_y = p0 > p1 ? p0 : p1;

	curve.lo = math::v2(0.0f, lo_y);
	curve.hi = math::v2(1.0f, hi_y);
	for (int32_t i = 0; i < 2; i++) {
		if (!roots[i].valid)
			continue;

		float t = roots[i].t.extract_x();
		float value = ((a * t + b) * t + c) * t + d;

		if (t < 0.0f || t > 1.0f)
			continue;
		if (value < curve.lo.extract_y())
			curve.lo.set_y(value);
		if (value > curve.hi.extract_y())
			curve.hi.set_y(value);
	}
	m_key[PARAM_KEY_EDIT] = curve.keys;
}

}
}
