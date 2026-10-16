/* Terminal dabt exception policy. */
#include "excpmgr_panic.h"

void excpPanicDabt(SceExcpmgrContext *context, unsigned int arg)
{
	excpPanicDispatch(context, arg, sceKernelPanicHandlerDabtCfunc);
}
