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
#define PARAM_FLAG_RAW		0x00020	/* never normalized */
#define PARAM_FLAG_INT		0x00040	/* a whole number */
#define PARAM_FLAG_BOOL		0x00080	/* 0 or 1 */
#define PARAM_FLAG_INDEX	0x00100	/* a whole number, an index */
#define PARAM_FLAG_DEGREES	0x01000	/* an angle, edited in degrees */
#define PARAM_FLAG_CURVE	0x10000	/* four keys of a Hermite curve */
#define PARAM_FLAG_VECTOR	0x40000	/* a vector's components */

/* a parameter's keys */
#define PARAM_KEY_CURRENT	0
#define PARAM_KEY_MIN		2
#define PARAM_KEY_MAX		3
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

/* a parameter's keys, shared by every ParamSet of a variant (names ours) */
template<class T>
class ParamKeys {
public:
	/* the current key, kept in the range */
	void Set(const T &value);
	/* the edited key made a valid value */
	void Normalize();

	const char *m_name;
	uint32_t m_flags;
	T m_key[PARAM_KEY_NUM];
};

template<>
inline void ParamKeys<float>::Set(const float &value)
{
	float set = value;

	if ((m_flags & PARAM_FLAG_RANGED) != 0 && (m_flags & PARAM_FLAG_UNCLAMPED) == 0) {
		if (set < m_key[PARAM_KEY_MIN])
			set = m_key[PARAM_KEY_MIN];
		else if (set > m_key[PARAM_KEY_MAX])
			set = m_key[PARAM_KEY_MAX];
	}
	if (m_key[PARAM_KEY_CURRENT] != set)
		m_flags |= PARAM_FLAG_CHANGED;
	m_key[PARAM_KEY_CURRENT] = set;
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

class ParamSet {
public:
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

/* a ParamSetKeysBase's place in its manager's list (names ours) */
class ParamLink {
public:
	ParamLink()
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
