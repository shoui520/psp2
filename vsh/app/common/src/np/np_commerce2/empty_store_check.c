/*
 * SceNpCommerce2: the empty-store check, which asks the store whether a category or a product
 * has content, on a context and a request of its own.
 */

#include "np_commerce2_internal.h"

SceUInt32 g_checkCtxId = 0;		/* 0x81026010 */
SceUInt32 g_checkReqId = 0;		/* 0x81026014 */
int g_checkAborted = 0;			/* 0x81026018 */

int sceNpCommerce2StopEmptyStoreCheck(void)
{
	if (g_checkReqId != 0)
		sceNpCommerce2AbortReq(g_checkReqId);
	g_checkAborted = 1;
	return 0;
}
