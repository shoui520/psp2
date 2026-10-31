/* paf::IDParam: a resource ID, its name and the name's hash. */
#ifndef PAF_VERSATILEPARAM_IDPARAM_H
#define PAF_VERSATILEPARAM_IDPARAM_H

#include <stdint.h>
#include <std/string>

namespace paf {

class IDParam {
public:
	IDParam(const char *id, uint32_t idhash) : m_id(id), m_idhash(idhash)
	{
	}

	uint32_t GetIDHash() const
	{
		return m_idhash;
	}

	string m_id;
	uint32_t m_idhash;
};

}

#endif
