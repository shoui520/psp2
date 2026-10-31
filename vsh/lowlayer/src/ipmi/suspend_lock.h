#ifndef IPMI_SUSPEND_LOCK_H
#define IPMI_SUSPEND_LOCK_H

#include <kernel.h>
#include "assert.h"

namespace IPMI {
namespace impl {

/* Sony's SuspendLock (suspend_lock.h): keeps the system awake while it is in scope. */
class SuspendLock {
public:
	SuspendLock()
	{
		IPMI_ASSERT_SUCCESS(sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT));
	}

	~SuspendLock()
	{
		IPMI_ASSERT_SUCCESS(sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT));
	}
};

}
}

#endif
