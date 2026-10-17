/* Adhoc session syscall exit. */
#include <sysmem/cpu.h>

int GcAuthAdhocClearState(void);

int GcAuthAdhocClearStateForUser(void)
{
	unsigned int state;
	int result;

	ENTER_SYSCALL(state);
	result = GcAuthAdhocClearState();
	EXIT_SYSCALL(state);
	return result;
}
