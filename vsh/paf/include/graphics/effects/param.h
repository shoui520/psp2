/*
 * The effects' parameters: named values an effect animates between sets of keys. A ParamBase
 * is one value, a ParamSet the values of one effect, a ParamSetKeysBase an effect's sets, one
 * per variant, read from "<name>[_<variant>].txt" in the effect's psets folder. Sony's names
 * are ParamBase, ParamSet, ParamSetKeysBase, findParam, findParamAndSet, setData and get_key
 * (the 0.990 and 0.995 builds' generated-code strings); the rest are ours.
 */
#ifndef PAF_GRAPHICS_EFFECTS_PARAM_H
#define PAF_GRAPHICS_EFFECTS_PARAM_H

#include <stddef.h>
#include <stdint.h>
#include <math/math.h>
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

/* a parameter's flags (names ours) */
#define PARAM_FLAG_CHANGED	0x00001	/* the current key was set to another value */
#define PARAM_FLAG_RANGED	0x00002	/* the value has a minimum and a maximum */
#define PARAM_FLAG_UNCLAMPED	0x00004	/* the range is not enforced */
#define PARAM_FLAG_FINE		0x00008	/* edited in fine steps; nothing in libpaf reads it */
#define PARAM_FLAG_UNRANGED	0x00010	/* made without a range */
#define PARAM_FLAG_RAW		0x00020	/* never normalized */
#define PARAM_FLAG_INT		0x00040	/* a whole number */
#define PARAM_FLAG_BOOL		0x00080	/* 0 or 1 */
#define PARAM_FLAG_INDEX	0x00100	/* a whole number, an index */
#define PARAM_FLAG_COLOR	0x00400	/* a colour; nothing in libpaf reads it */
#define PARAM_FLAG_DEGREES	0x01000	/* an angle, edited in degrees */
#define PARAM_FLAG_ANGLE	0x02000	/* an angle, moved the shortest way round */
#define PARAM_FLAG_ANGLE_DEGREES 0x04000	/* an angle in degrees, moved the shortest way round */
#define PARAM_FLAG_CURVE	0x10000	/* four keys of a Hermite curve */
#define PARAM_FLAG_PROFILE	0x20000	/* four keys of a profile, kept without a range */
#define PARAM_FLAG_VECTOR	0x40000	/* a vector's components */

/* radians to degrees and back: an angle edited in degrees keeps their precision */
#define PARAM_DEGREES_PER_RADIAN	57.29578f
#define PARAM_RADIANS_PER_DEGREE	0.017453292f

/* a parameter's keys */
#define PARAM_KEY_CURRENT	0
#define PARAM_KEY_MIN		2
#define PARAM_KEY_MAX		3
#define PARAM_KEY_SAVED		4	/* the current key kept while another is set (name ours) */
#define PARAM_KEY_TARGET	5
#define PARAM_KEY_START		6
#define PARAM_KEY_LERP_FROM	7
#define PARAM_KEY_LERP_TO	8
#define PARAM_KEY_EDIT		9
#define PARAM_KEY_NUM		10

/* the time a set of keys moves from its start to its target in (names ours) */
struct ParamTransition {
	float time;
	float start;
	float end;
};

/* the keys a transition moves between (names ours) */
template<class T>
struct ParamTransitionKeys {
	T current;
	T start;
	T target;
};

/* the number of floats in a parameter's value (names ours) */
template<class T>
struct ParamComponents;

template<>
struct ParamComponents<float> {
	enum { NUM = 1 };
};

template<>
struct ParamComponents<math::v2> {
	enum { NUM = 2 };
};

template<>
struct ParamComponents<math::v3> {
	enum { NUM = 3 };
};

template<>
struct ParamComponents<math::v4> {
	enum { NUM = 4 };
};

template<>
struct ParamComponents<math::quaternion> {
	enum { NUM = 4 };
};

/* every component set to x (names ours from here to ParamBase) */
template<class T>
inline T ParamSplat(float x);

template<>
inline float ParamSplat<float>(float x)
{
	return x;
}

template<>
inline math::v2 ParamSplat<math::v2>(float x)
{
	return math::v2(sce::Vectormath::Simd::Aos::Vector2(x));
}

template<>
inline math::v3 ParamSplat<math::v3>(float x)
{
	return math::v3(sce::Vectormath::Simd::Aos::Vector3(x));
}

template<>
inline math::v4 ParamSplat<math::v4>(float x)
{
	return math::v4(sce::Vectormath::Simd::Aos::Vector4(x));
}

/* the value kept between min and max */
inline float ParamClamp(float value, float min, float max)
{
	float clamped = min > value ? min : value;

	return max > clamped ? clamped : max;
}

inline math::v2 ParamClamp(math::v2 value, math::v2 min, math::v2 max)
{
	return math::v2(maxPerElem(min.getImpl(), minPerElem(max.getImpl(), value.getImpl())));
}

inline math::v3 ParamClamp(math::v3 value, math::v3 min, math::v3 max)
{
	return math::v3(maxPerElem(minPerElem(value.getImpl(), max.getImpl()), min.getImpl()));
}

inline math::v4 ParamClamp(math::v4 value, math::v4 min, math::v4 max)
{
	return math::v4(maxPerElem(minPerElem(value.getImpl(), max.getImpl()), min.getImpl()));
}

/* from a to b by t, an angle the shortest way round */
template<class T>
inline T ParamLerp(T a, T b, float t, bool angle, bool degrees)
{
	if (angle) {
		if (degrees)
			return math::LerpAngle(a * PARAM_RADIANS_PER_DEGREE,
					       b * PARAM_RADIANS_PER_DEGREE, ParamSplat<T>(t)) *
			       PARAM_DEGREES_PER_RADIAN;
		return math::LerpAngle(a, b, ParamSplat<T>(t));
	}
	return a + (b - a) * t;
}

/* a rotation the shortest way round */
template<>
inline math::quaternion ParamLerp(math::quaternion a, math::quaternion b, float t, bool angle,
				  bool degrees)
{
	return math::quaternion::Slerp(a, b, t, 0.0001f);
}

/*
 * The value a transition is at: mode 1 moves from the current key to the target by x, the others
 * from the start key to the target over the transition's time, mode 0 easing in and out.
 */
template<class T>
T ParamInterpolate(const ParamTransition *transition, const ParamTransitionKeys<T> &keys,
		   int32_t mode, bool angle, bool degrees, float x)
{
	if (mode == 1)
		return ParamLerp(keys.current, keys.target, x, angle, degrees);
	if (transition->time >= transition->end)
		return keys.target;
	if (transition->time <= transition->start)
		return keys.start;

	float t = (transition->time - transition->start) / (transition->end - transition->start);

	if (mode == 0)
		t = t * t * (3.0f - t * 2.0f);
	return ParamLerp(keys.start, keys.target, t, angle, degrees);
}

/* a rotation's, which Sony inlines */
template<>
inline math::quaternion ParamInterpolate(const ParamTransition *transition,
					 const ParamTransitionKeys<math::quaternion> &keys,
					 int32_t mode, bool angle, bool degrees, float x)
{
	if (mode == 1)
		return ParamLerp(keys.current, keys.target, x, angle, degrees);
	if (transition->time >= transition->end)
		return keys.target;
	if (transition->time <= transition->start)
		return keys.start;

	float t = (transition->time - transition->start) / (transition->end - transition->start);

	if (mode == 0)
		t = t * t * (3.0f - t * 2.0f);
	return ParamLerp(keys.start, keys.target, t, angle, degrees);
}

/* a parameter's keys, shared by every ParamSet of a variant (names ours) */
template<class T>
class ParamKeys {
public:
	ParamKeys(const char *name, T value, T min, T max, uint32_t flags);

	ParamKeys(const char *name, T value, uint32_t flags)
		: m_name(name), m_flags(flags)
	{
		for (int32_t i = 0; i < PARAM_KEY_NUM; i++)
			m_key[i] = value;
		if ((flags & PARAM_FLAG_CURVE) == 0 && (flags & PARAM_FLAG_PROFILE) == 0)
			flags |= PARAM_FLAG_UNRANGED;
		m_flags = flags & ~PARAM_FLAG_RANGED;
	}

	/* the current key, kept in the range */
	void Set(T value);

	void SetCurrent(T value)
	{
		bool changed = !(value == m_key[PARAM_KEY_CURRENT]);

		m_key[PARAM_KEY_CURRENT] = value;
		if (changed)
			m_flags |= PARAM_FLAG_CHANGED;
	}

	/* key dst set to key src, the current key through the range */
	void CopyKey(int32_t dst, int32_t src)
	{
		if (dst != PARAM_KEY_CURRENT)
			m_key[dst] = m_key[src];
		else
			Set(m_key[src]);
	}

	/* the current key moved from the start key to the target */
	void Update(const ParamTransition *transition, int32_t mode, float x);

	/* the edited key made a valid value */
	void Normalize();

	const char *m_name;
	uint32_t m_flags;
	T m_key[PARAM_KEY_NUM];
};

/* Sony keeps a rotation's out of line */
template<>
inline __attribute__((noinline))
ParamKeys<math::quaternion>::ParamKeys(const char *name, math::quaternion value, uint32_t flags)
	: m_name(name), m_flags(flags)
{
	for (int32_t i = 0; i < PARAM_KEY_NUM; i++)
		m_key[i] = value;
	if ((flags & PARAM_FLAG_CURVE) == 0 && (flags & PARAM_FLAG_PROFILE) == 0)
		flags |= PARAM_FLAG_UNRANGED;
	m_flags = flags & ~PARAM_FLAG_RANGED;
}

/* Sony keeps these out of line, but for a float's */
template<typename T>
__attribute__((noinline)) ParamKeys<T>::ParamKeys(const char *name, T value, T min, T max,
						   uint32_t flags)
	: m_name(name), m_flags(flags)
{
	for (int32_t i = 0; i < PARAM_KEY_NUM; i++)
		m_key[i] = value;
	m_key[PARAM_KEY_MIN] = min;
	m_key[PARAM_KEY_MAX] = max;
	m_flags |= PARAM_FLAG_RANGED;
	if ((m_flags & PARAM_FLAG_BOOL) != 0) {
		m_key[PARAM_KEY_MIN] = ParamSplat<T>(0.0f);
		m_key[PARAM_KEY_MAX] = ParamSplat<T>(1.0f);
	}
}

template<>
inline ParamKeys<float>::ParamKeys(const char *name, float value, float min, float max,
				   uint32_t flags)
	: m_name(name), m_flags(flags)
{
	for (int32_t i = 0; i < PARAM_KEY_NUM; i++)
		m_key[i] = value;
	m_key[PARAM_KEY_MIN] = min;
	m_key[PARAM_KEY_MAX] = max;
	m_flags |= PARAM_FLAG_RANGED;
	if ((m_flags & PARAM_FLAG_BOOL) != 0) {
		m_key[PARAM_KEY_MIN] = ParamSplat<float>(0.0f);
		m_key[PARAM_KEY_MAX] = ParamSplat<float>(1.0f);
	}
}

template<typename T>
__attribute__((noinline)) void ParamKeys<T>::Update(const ParamTransition *transition, int32_t mode,
						     float x)
{
	if (!(m_key[PARAM_KEY_TARGET] == m_key[PARAM_KEY_CURRENT])) {
		ParamTransitionKeys<T> keys = { m_key[PARAM_KEY_CURRENT], m_key[PARAM_KEY_START],
						m_key[PARAM_KEY_TARGET] };

		Set(ParamInterpolate(transition, keys, mode, (m_flags & PARAM_FLAG_ANGLE) != 0,
				     (m_flags & PARAM_FLAG_ANGLE_DEGREES) != 0, x));
	}
}

template<>
inline void ParamKeys<float>::Update(const ParamTransition *transition, int32_t mode, float x)
{
	if (!(m_key[PARAM_KEY_TARGET] == m_key[PARAM_KEY_CURRENT])) {
		ParamTransitionKeys<float> keys = { m_key[PARAM_KEY_CURRENT], m_key[PARAM_KEY_START],
						m_key[PARAM_KEY_TARGET] };

		Set(ParamInterpolate(transition, keys, mode, (m_flags & PARAM_FLAG_ANGLE) != 0,
				     (m_flags & PARAM_FLAG_ANGLE_DEGREES) != 0, x));
	}
}

template<typename T>
__attribute__((noinline)) void ParamKeys<T>::Set(T value)
{
	if ((m_flags & PARAM_FLAG_RANGED) == 0 || (m_flags & PARAM_FLAG_UNCLAMPED) != 0)
		SetCurrent(value);
	else
		SetCurrent(ParamClamp(value, m_key[PARAM_KEY_MIN], m_key[PARAM_KEY_MAX]));
}

/* Sony keeps these two out of line */
template<>
inline __attribute__((noinline)) void ParamKeys<math::v2>::SetCurrent(math::v2 value)
{
	bool changed = !(value == m_key[PARAM_KEY_CURRENT]);

	m_key[PARAM_KEY_CURRENT] = value;
	if (changed)
		m_flags |= PARAM_FLAG_CHANGED;
}

template<>
inline __attribute__((noinline)) void
ParamKeys<math::quaternion>::SetCurrent(math::quaternion value)
{
	bool changed = !(value == m_key[PARAM_KEY_CURRENT]);

	m_key[PARAM_KEY_CURRENT] = value;
	if (changed)
		m_flags |= PARAM_FLAG_CHANGED;
}

/* a rotation is kept a unit quaternion and has no range */
template<>
inline void ParamKeys<math::quaternion>::Set(math::quaternion value)
{
	SetCurrent(math::quaternion(value.getImpl() / length(value.getImpl())));
}

/* Sony keeps a rotation's copy out of line */
template<>
inline __attribute__((noinline)) void ParamKeys<math::quaternion>::CopyKey(int32_t dst, int32_t src)
{
	if (dst != PARAM_KEY_CURRENT)
		m_key[dst] = m_key[src];
	else
		Set(m_key[src]);
}

template<> void ParamKeys<math::quaternion>::Normalize();
template<> void ParamKeys<float>::Normalize();
template<> void ParamKeys<math::v2>::Normalize();
template<> void ParamKeys<math::v3>::Normalize();
template<> void ParamKeys<math::v4>::Normalize();

class ParamBase {
public:
	/* the value's floats */
	struct Value {
		float *data;
		int32_t num;
	};

	virtual ~ParamBase()
	{
	}

	/* key dst set to key src */
	virtual void CopyKey(int32_t dst, int32_t src) = 0;
	/* the current key at t between the lerp keys */
	virtual void Lerp(float t) = 0;
	/* the current key moved from the start key to the target key */
	virtual void Update(const ParamTransition *transition, int32_t mode, float x) = 0;
	virtual const char *GetName() = 0;
	virtual Value GetValue() = 0;
	virtual void Normalize() = 0;
	virtual uint32_t GetFlags() = 0;

	void setData(float x)
	{
		Value value = GetValue();

		value.data[0] = x;
	}

	void setData(float x, float y)
	{
		Value value = GetValue();

		value.data[0] = x;
		value.data[1] = y;
	}

	void setData(float x, float y, float z)
	{
		Value value = GetValue();

		value.data[0] = x;
		value.data[1] = y;
		value.data[2] = z;
	}

	void setData(float x, float y, float z, float w)
	{
		Value value = GetValue();

		value.data[0] = x;
		value.data[1] = y;
		value.data[2] = z;
		value.data[3] = w;
	}
};

}

/* a set's list of its parameters: Sony's sets destroy it inline */
template<>
inline vector<graph::ParamBase *, allocator<graph::ParamBase *> >::~vector()
{
	if (ptr != NULL)
		alloc.deallocate(ptr, limit);
	ptr = (graph::ParamBase **)-1;
	count = -1;
	limit = -1;
}

namespace graph {
class ParamSet;
}

/* a key's set by its variant: Sony's sets find it inline */
template<>
inline graph::ParamSet *&map<string, graph::ParamSet *>::operator[](const string &key)
{
	return this->_Insert(value_type(key, mapped_type()), NULL)->value.second;
}

namespace graph {

class ParamSet {
public:
	ParamSet(const string &name)
		: m_transition_active(false), m_transition_mode(0), m_transition_x(0.9f),
		  m_name(GetLocalName(name))
	{
	}

	virtual ~ParamSet()
	{
	}

	/* every value copied into dst's (name ours) */
	virtual void CopyValues(ParamSet *dst) = 0;

	/* the name after its last ':' */
	string GetLocalName(const string &name);

	void CopyKey(int32_t dst, int32_t src);
	/* every parameter on its way from the current key to its target in end seconds */
	void StartTransition(float end, int32_t mode, float x);
	/* the transition dt seconds on; false when there is none */
	bool UpdateTransition(float dt);
	void Lerp(float t);

	ParamBase *findParam(const char *name)
	{
		for (uint32_t i = 0; i != m_params.size(); i++) {
			if (sce_paf_strcmp(m_params[i]->GetName(), name) == 0)
				return m_params[i];
		}
		return NULL;
	}

	void findParamAndSet(const char *name, float x);
	void findParamAndSet(const char *name, float x, float y);
	void findParamAndSet(const char *name, float x, float y, float z);
	void findParamAndSet(const char *name, float x, float y, float z, float w);

	vector<ParamBase *> m_params;
	bool m_transition_active;
	ParamTransition m_transition;
	int32_t m_transition_mode;
	float m_transition_x;
	string m_name;
};

/* a value of type T, its keys made when it has a set (names ours) */
template<class T>
class Param : public ParamBase {
public:
	__attribute__((noinline))
	Param(ParamSet *set, const char *name, T value, T min, T max, const uint32_t &flags)
		: m_keys(NULL)
	{
		if (set != NULL) {
			m_keys = new ParamKeys<T>(name, value, min, max, flags);
			set->m_params.push_back(this);
		}
		SetValue(value);
	}

	__attribute__((noinline)) Param(ParamSet *set, const char *name, T value, const uint32_t &flags)
		: m_keys(NULL)
	{
		if (set != NULL) {
			m_keys = new ParamKeys<T>(name, value, flags);
			set->m_params.push_back(this);
		}
		SetValue(value);
	}

	/* a copy has the value alone, without keys */
	Param(const Param &other) : m_value(other.m_value), m_keys(NULL)
	{
	}

	virtual ~Param()
	{
		if (m_keys != NULL)
			delete m_keys;
	}

	virtual void CopyKey(int32_t dst, int32_t src)
	{
		if (m_keys != NULL) {
			m_keys->CopyKey(dst, src);
			m_value = m_keys->m_key[PARAM_KEY_CURRENT];
		}
	}

	virtual void Lerp(float t)
	{
		if (m_keys != NULL) {
			T from = m_keys->m_key[PARAM_KEY_LERP_FROM];

			m_keys->Set(from + (m_keys->m_key[PARAM_KEY_LERP_TO] - from) * t);
			m_value = m_keys->m_key[PARAM_KEY_CURRENT];
		}
	}

	virtual void Update(const ParamTransition *transition, int32_t mode, float x)
	{
		if (m_keys != NULL) {
			m_keys->Update(transition, mode, x);
			m_value = m_keys->m_key[PARAM_KEY_CURRENT];
		}
	}

	virtual const char *GetName()
	{
		return m_keys != NULL ? m_keys->m_name : "";
	}

	virtual Value GetValue()
	{
		Value value;

		value.data = (float *)&m_value;
		value.num = ParamComponents<T>::NUM;
		return value;
	}

	virtual void Normalize()
	{
		if (m_keys != NULL && (m_keys->m_flags & PARAM_FLAG_RAW) == 0)
			m_keys->Normalize();
	}

	virtual uint32_t GetFlags()
	{
		return m_keys->m_flags;
	}

	Param &operator=(const Param &other)
	{
		SetValue(other.m_value);
		return *this;
	}

	/* the value, through the keys' range when there are keys */
	void SetValue(T value)
	{
		m_value = value;
		if (m_keys != NULL) {
			m_keys->Set(value);
			m_value = m_keys->m_key[PARAM_KEY_CURRENT];
		}
	}

	T m_value;
	ParamKeys<T> *m_keys;
};

/* Sony keeps a rotation's set out of line */
template<>
inline __attribute__((noinline)) void Param<math::quaternion>::SetValue(math::quaternion value)
{
	m_value = value;
	if (m_keys != NULL) {
		m_keys->Set(value);
		m_value = m_keys->m_key[PARAM_KEY_CURRENT];
	}
}

/* a rotation between the lerp keys the shortest way round */
template<>
inline void Param<math::quaternion>::Lerp(float t)
{
	if (m_keys != NULL) {
		m_keys->Set(math::quaternion::Slerp(m_keys->m_key[PARAM_KEY_LERP_FROM],
						    m_keys->m_key[PARAM_KEY_LERP_TO], t, 0.0001f));
		m_value = m_keys->m_key[PARAM_KEY_CURRENT];
	}
}

/* a ParamSetKeysBase's place in its manager's list (names ours) */
class ParamLink {
public:
	ParamLink() : m_prev(NULL), m_next(NULL)
	{
	}

	~ParamLink()
	{
		Unlink();
		m_prev = NULL;
		m_next = NULL;
	}

	void Unlink()
	{
		if (m_next != NULL && m_prev != NULL) {
			m_next->m_prev = m_prev;
			m_prev->m_next = m_next;
		}
	}

	ParamLink *m_prev;
	ParamLink *m_next;
};

/* a list's head: a link to itself when the list is empty (names ours) */
class ParamList : public ParamLink {
public:
	ParamList()
	{
		m_prev = this;
		m_next = this;
	}

	~ParamList()
	{
		Erase(m_next, this);
	}

	/* the links from first up to last taken out, each unlinked */
	void Erase(ParamLink *first, ParamLink *last)
	{
		if (first != last) {
			first->m_prev->m_next = last;
			last->m_prev = first->m_prev;
			while (first != last) {
				ParamLink *next = first->m_next;

				first->m_prev = NULL;
				first->m_next = NULL;
				first = next;
			}
		}
	}

	/* link put last, out of the list it was in */
	void PushBack(ParamLink *link)
	{
		if (link != this) {
			link->Unlink();

			ParamLink *last = m_prev;

			link->m_prev = last;
			link->m_next = this;
			last->m_next = link;
			m_prev = link;
		}
	}
};

class ParamSetKeysBase : public ParamLink {
public:
	typedef map<string, ParamSet *> KeyMap;

	ParamSetKeysBase(const string &name, const string &variant)
		: m_name(name), m_variant(variant)
	{
	}

	/* the set of variant, made when create is set */
	virtual ParamSet *get_key(const string &variant, bool create) = 0;
	/* the set every variant starts from (name ours) */
	virtual ParamSet *GetDefault() = 0;

	virtual ~ParamSetKeysBase()
	{
		Clear();
	}

	/* key of every set the variant's (names ours from here) */
	virtual void SetKey(int32_t key, const string &variant) = 0;
	virtual void StartTransition(const string &variant, float time, int32_t mode, float x) = 0;
	virtual void UpdateTransition(float dt) = 0;
	virtual void Lerp(float t) = 0;

	/* every variant's set deleted */
	void Clear();
	/* "<name>[_<variant>].txt", in lower case */
	string GetFileName();

	KeyMap m_keys;
	string m_name;
	string m_variant;
	string m_path;
	string m_rel_path;
};

/*
 * A ParamSetKeysBase of one kind of set S: its default set, a set per variant made when first
 * asked for, and the sets of S that follow its keys (names ours). S has a set's constructor
 * taking whether its parameters get keys, its values in m_values and a SetKey taking the
 * values of a variant.
 */
template<class S>
class ParamSetKeys : public ParamSetKeysBase {
public:
	/* out of line, one for each set (0x812020a4 for the material's) */
	__attribute__((noinline)) ParamSetKeys(const string &variant);

	virtual ParamSet *get_key(const string &variant, bool create)
	{
		return GetKey(variant, create);
	}

	virtual ParamSet *GetDefault()
	{
		return &m_default;
	}

	virtual ~ParamSetKeys()
	{
	}

	virtual void SetKey(int32_t key, const string &variant)
	{
		for (ParamLink *link = m_sets.m_next; link != &m_sets; link = link->m_next) {
			S *set = static_cast<S *>(link);

			set->SetKey(key, GetKey(variant, false)->m_values);
		}
	}

	virtual void StartTransition(const string &variant, float time, int32_t mode, float x)
	{
		SetKey(PARAM_KEY_TARGET, variant);
		for (ParamLink *link = m_sets.m_next; link != &m_sets; link = link->m_next) {
			S *set = static_cast<S *>(link);

			set->ParamSet::StartTransition(time, mode, x);
		}
	}

	virtual void UpdateTransition(float dt)
	{
		for (ParamLink *link = m_sets.m_next; link != &m_sets; link = link->m_next) {
			S *set = static_cast<S *>(link);

			set->ParamSet::UpdateTransition(dt);
		}
	}

	virtual void Lerp(float t)
	{
		for (ParamLink *link = m_sets.m_next; link != &m_sets; link = link->m_next) {
			S *set = static_cast<S *>(link);

			set->ParamSet::Lerp(t);
		}
	}

	/* the set of the variant, else (made when create is set) the default */
	__attribute__((noinline)) S *GetKey(const string &variant, bool create)
	{
		KeyMap::iterator it = m_keys.find(variant);

		if (it == m_keys.end()) {
			if (!create)
				return &m_default;

			S *set = new S(false);

			m_keys[variant] = set;
			return set;
		}
		return static_cast<S *>(it->second);
	}

	/* the set follows the keys */
	void Add(S *set)
	{
		m_sets.PushBack(set);
	}

	S m_default;
	ParamList m_sets;
};

template<class S>
inline __attribute__((noinline)) ParamSetKeys<S>::ParamSetKeys(const string &variant)
	: ParamSetKeysBase(S::TypeName(), variant), m_default(true)
{
}

/* the effect's ParamSetKeysBase, edited together (names ours) */
class ParamSetKeysManager {
public:
	ParamSetKeysManager(const string &name);

	void Add(ParamSetKeysBase *keys);
	/* a variant every member has a set of */
	void AddVariant(const string &variant);
	void StartTransition(const string &variant, float time, int32_t mode, float x);
	/* the members' files in dir/sub */
	void SetPaths(const string &dir, const string &sub);
	void SetVariant(const string &variant);
	/* the variant edited shown, else the transitions dt seconds on */
	void Update(float dt);

	ParamList m_list;
	string m_name;
	set<string> m_variants;
	string m_edit_variant;
	string m_copied_variant;
	bool m_editing;
	bool m_morphing;
	float m_anim_time;
};

}
}

#endif
