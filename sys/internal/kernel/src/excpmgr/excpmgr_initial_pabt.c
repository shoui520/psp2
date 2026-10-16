/* Initial pabt exception state and policy. */
#include "excpmgr_context_private.h"

void *excpPabtEntry(SceExcpmgrContext *context)
{
	excpSaveState(context, SCE_EXCP_PABT);
	excpEnterKernel();
	excpCallback(g_pabtCallback, context);
	return excpEnterNested();
}
