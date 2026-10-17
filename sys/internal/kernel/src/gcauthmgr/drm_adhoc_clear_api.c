/* Adhoc session syscall exit. */
#include <sysmem/cpu.h>
#include <intrmgr/syscall.h>

int GcAuthAdhocClearState(void);

int GcAuthAdhocClearStateForUser(void)
{
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	result = GcAuthAdhocClearState();
	sceSyscallLeave(state);
	return result;
}
