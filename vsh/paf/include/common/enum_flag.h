/*
 * paf::common::EnumFlag: a set of an enum's bits, each enumerator the index of its bit (the
 * name from the mangled export names, its members ours).
 */
#ifndef PAF_COMMON_ENUM_FLAG_H
#define PAF_COMMON_ENUM_FLAG_H

#include <stdint.h>

namespace paf {
namespace common {

template<typename T>
class EnumFlag {
public:
	EnumFlag() : m_value(0)
	{
	}

	explicit EnumFlag(uint32_t value) : m_value(value)
	{
	}

	bool IsSet(T bit) const
	{
		return (m_value & (1 << bit)) != 0;
	}

	EnumFlag &Set(T bit)
	{
		m_value |= 1 << bit;
		return *this;
	}

	uint32_t m_value;
};

}
}

#endif
