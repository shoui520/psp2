/*
 * paf::common::RefObj.
 */
#include <common/ref_obj.h>

namespace paf {
namespace common {

RefObj::RefObj()
{
	m_ref_count = 0;
}

int32_t RefObj::AddRef()
{
	return sceAtomicIncrement32AcqRel(&m_ref_count) + 1;
}

int32_t RefObj::Release()
{
	int32_t count = sceAtomicDecrement32AcqRel(&m_ref_count) - 1;

	if (count == 0)
		delete this;
	return count;
}

}
}
