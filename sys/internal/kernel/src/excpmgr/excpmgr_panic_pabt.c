/* Terminal pabt exception policy. */
#include "excpmgr_panic.h"

void excpPanicPabt(SceExcpmgrContext *context, unsigned int arg)
{
	excpPanicDispatch(context, arg, sceKernelPanicHandlerPabtCfunc);
}
