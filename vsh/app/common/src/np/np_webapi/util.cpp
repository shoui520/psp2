/*
 * SceNpWebApi: small helpers: the error the service's codes become, the system parameter the
 * library reads at start, and the accessors of its map nodes. The names are ours.
 */

#include "np_webapi_internal.h"

volatile int g_systemParam = -1;	/* 0x81010010 */

int npWebApiConvertError(int error)
{
	if (error == -2141911417)
		return -2141872383;
	if (error == -2141911416)
		return -2141902572;
	return error;
}

int npWebApiLoadSystemParam(void)
{
	return sceRegMgrSystemParamGetInt(226562, (int *)&g_systemParam);
}

NpWebApiMapNode *npWebApiMapNodeNext(NpWebApiMapNode *node)
{
	return node->next;
}

void *npWebApiMapNodeValue(NpWebApiMapNode *node)
{
	return node->value;
}
