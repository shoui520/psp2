/* Adhoc session syscall entry. */
#include <sysmem/cpu.h>
#include <intrmgr/syscall.h>
#include "adhoc.h"

int GcAuthAdhocInitializeStateForUser(void)
{
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	result = GcAuthAdhocInitializeState();
	sceSyscallLeave(state);
	return result;
}
