/* Terminal undef exception policy. */
#include "excpmgr_panic.h"

void excpPanicUndef(SceExcpmgrContext *context, unsigned int arg)
{
	excpPanicDispatch(context, arg, sceKernelPanicHandlerUndefCfunc);
}
