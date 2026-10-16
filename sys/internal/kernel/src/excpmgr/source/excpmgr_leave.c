/* Leave the terminal exception chain in the interrupted thread. */
#include "excpmgr_context_private.h"

void __attribute__((noreturn)) excpPanicLeave(SceExcpmgrContext *context)
{
	unsigned int cpu = excpCpu();
	ThreadCB *thread;

	if (g_excpData.nestCount[cpu] - 1 != 0)
		for (;;)
			;
	g_excpData.nestCount[cpu] = 0;
	writeDfsr(0);
	writeIfsr(0);
	writeAdfsr(0);
	writeAifsr(0);
	writeDfar(0);
	writeIfar(0);
	thread = context->thread;
	writeTpidrprw((unsigned int)thread);
	sceKernelSysrootReturnFromExcpToThread(context, thread);
	for (;;)
		;
}
