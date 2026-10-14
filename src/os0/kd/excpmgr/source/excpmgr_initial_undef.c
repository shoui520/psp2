/* Initial undef exception state and policy. */
#include "excpmgr_context_private.h"

void *excpUndefEntry(SceExcpmgrContext *context)
{
	unsigned int pc;

	excpSaveState(context, SCE_EXCP_UNDEF);
	excpEnterKernel();
	excpCallback(g_undefCallback, context);
	pc = PatchUserInstruction(context);
	if (pc == 0)
		return excpEnterNested();
	context->pc = pc;
	excpRestoreFaultState(context);
	excpLeave(context);
	excpRestoreDebugRegisters(context);
	excpRearmTrace();
	return NULL;
}
