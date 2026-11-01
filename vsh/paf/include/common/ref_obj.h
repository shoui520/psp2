/*
 * paf::common::RefObj: an object that counts its own references and deletes
 * itself through its virtual destructor when the last one is released. The
 * class and its members are Sony's names, by the export NIDs.
 */
#ifndef PAF_COMMON_REF_OBJ_H
#define PAF_COMMON_REF_OBJ_H

#include <stdint.h>
#include <sce_atomic.h>

namespace paf {
namespace common {

class RefObj {
public:
	RefObj();

	virtual ~RefObj()
	{
	}

	/* the count after this reference */
	int32_t AddRef();
	/* the count left; the object is gone when it is 0 */
	int32_t Release();

	volatile int32_t m_ref_count;
};

}
}

#endif
