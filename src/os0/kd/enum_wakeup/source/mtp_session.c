/* SceEnumWakeUp: MTP session state. */
#include "ew_internal.h"

int mtpIsSessionOpen(const MtpContext *ctx)
{
	return ctx->sessionId != 0;
}
