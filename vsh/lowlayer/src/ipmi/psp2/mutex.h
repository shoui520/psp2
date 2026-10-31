#ifndef IPMI_PSP2_MUTEX_H
#define IPMI_PSP2_MUTEX_H

#include <stddef.h>
#include <kernel.h>

namespace IPMI {
namespace impl {
namespace psp2 {

class Mutex {
public:
	Mutex();

	int create();
	int finalize();
	int lock();
	int unlock();
	int destroy();

private:
	SceUID m_id;
};

}
}
}

#endif
