/* paf::IDParam: a resource ID, its name and the name's hash. */
#ifndef PAF_VERSATILEPARAM_IDPARAM_H
#define PAF_VERSATILEPARAM_IDPARAM_H

#include <stddef.h>
#include <stdint.h>
#include <std/string>

namespace paf {

/* the name of an ID known only by its hash (one object, in cache_image.cpp) */
extern const char s_empty_id[];
/* the other empty name, which the widget and page lookups build their IDParam from (plugin.cpp, name ours) */
extern const char s_empty_name[];

class IDParam {
public:
	IDParam(const char *id)
	{
		if (id == NULL)
			m_idhash = 0;
		else
			SetID(string(id));
	}

	IDParam(const string &id) : m_id(id), m_idhash(ID2IDHash(id))
	{
	}

	IDParam(const char *id, uint32_t idhash) : m_id(id), m_idhash(idhash)
	{
	}

	~IDParam()
	{
	}

	uint32_t ID2IDHash(const string &id);

	void SetID(const string &id)
	{
		m_id = id;
		m_idhash = ID2IDHash(id);
	}

	uint32_t GetIDHash() const
	{
		return m_idhash;
	}

	/* IDs order by their hash */
	bool operator<(const IDParam &right) const
	{
		return m_idhash < right.m_idhash;
	}

	string m_id;
	uint32_t m_idhash;
};

}

#endif
