/*
 * SceNpCommerce2: the library's own test flow (signing up a test account and buying with it, on the
 * networks that are not the production one), and the request it keeps to be able to abort it.
 */

#include "np_commerce2_internal.h"

SceUInt32 g_priceReqId = 0;		/* 0x81026004: the request the flow waits for */
int g_priceAborted = 0;			/* 0x81026008 */

/* aborts the flow: the request it waits for, then the flow itself at its next step */
int SceNpCommerce2_785095D9(void)
{
	if (g_priceReqId != 0)
		sceNpCommerce2AbortReq(g_priceReqId);
	g_priceAborted = 1;
	return 0;
}
