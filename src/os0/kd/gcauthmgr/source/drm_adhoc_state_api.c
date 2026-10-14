/* Adhoc session syscall entry. */
#include <psp2kern/kernel/cpu.h>

int GcAuthAdhocInitializeState(void);

int GcAuthAdhocInitializeStateForUser(void)
{
	unsigned int state;
	int result;

	ENTER_SYSCALL(state);
	result = GcAuthAdhocInitializeState();
	EXIT_SYSCALL(state);
	return result;
}
