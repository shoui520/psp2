/* Adhoc session syscall entry. */
#include <sysmem/cpu.h>
#include <intrmgr/syscall.h>

int GcAuthAdhocInitializeState(void);

int GcAuthAdhocInitializeStateForUser(void)
{
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	result = GcAuthAdhocInitializeState();
	sceSyscallLeave(state);
	return result;
}
