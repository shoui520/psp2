/* Initial dabt exception state and policy. */
#include "excpmgr_context_private.h"

void *excpDabtEntry(SceExcpmgrContext *context)
{
	/* the unprivileged access helper stages its resume address in r12 */
	unsigned int resume = context->r[12];
	int res;

	excpSaveState(context, SCE_EXCP_DABT);
	excpEnterKernel();
	excpCallback(g_dabtCallback, context);
	res = ClassifyAbortInstruction(context->pc, context->cpsr);
	if (res == 0)
		return excpEnterNested();
	if (res == 12) {
		context->pc = resume;
	} else if (res < 0) {
		context->r[0] = res;
		context->pc += 4;
	}
	excpRestoreFaultState(context);
	excpRestoreDebugRegisters(context);
	excpRearmTrace();
	return NULL;
}
