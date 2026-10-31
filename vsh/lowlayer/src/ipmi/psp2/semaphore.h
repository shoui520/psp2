#ifndef IPMI_PSP2_SEMAPHORE_H
#define IPMI_PSP2_SEMAPHORE_H

#include <stddef.h>
#include <kernel.h>

namespace IPMI {
namespace impl {
namespace psp2 {

class Semaphore {
public:
	Semaphore();
	~Semaphore();

	int create(const char *name, int maxCount, int initCount);
	int open(const char *name);
	int close();
	int wait(SceUInt32 *timeout);
	int waitCB(SceUInt32 *timeout);
	int tryWait();
	int signal();
	int cancel(int setCount);
	int destroy();

private:
	SceUID m_id;
	bool m_owner;
};

}
}
}

#endif
