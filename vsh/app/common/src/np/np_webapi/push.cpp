/*
 * SceNpWebApi: the push notification library (Sony's np_push/src: np_push_lib_context.cpp and
 * friends). The names are ours where the strings give none.
 */

#include "np_webapi_internal.h"

NpPushLibContext *g_pushLibContext;		/* 0x81010038 */
int g_pushAllocator[4];				/* 0x81014080 */

void npPushLibContextDelete(void)
{
	if (g_pushLibContext != NULL)
		((void (*)(NpPushLibContext *))g_pushLibContext->vtable[1])(g_pushLibContext);
	g_pushLibContext = NULL;
}

NpPushLibContext *npPushLibContextGet(void)
{
	return g_pushLibContext;
}

int npPushSetCallbackParam(const int *param)
{
	g_pushAllocator[0] = param[0];
	g_pushAllocator[1] = param[1];
	g_pushAllocator[2] = param[2];
	g_pushAllocator[3] = param[3];
	return g_pushAllocator[3];
}

/* the name of a filter (its text at +32) when it has one or its ID is set; none otherwise */
const char *npPushFilterName(const unsigned char *filter)
{
	const char *name;

	name = (const char *)(filter + 32);
	if (sceClibStrnlen(name, 0xFFFFFFFF) != 0 || *(const int *)(filter + 68) != -1)
		return name;
	return NULL;
}

/* tells the service (command 0x10001) the 20-byte ID the library listens for */
int npPushIpcSendId(NpWebApiIpmiClient *client, const void *data)
{
	NpWebApiIpmiIn in[1];
	int ret;
	int result;

	ret = 0;
	in[0].data = data;
	in[0].size = 20;
	result = client->vtbl->invoke(client, 0x10001, in, 1, &ret, NULL, 0);
	if (result == 0)
		result = ret;
	return result;
}

/* tells the service (command 0x10000) the 20-byte ID a user registers with */
int npPushIpcRegisterId(NpWebApiIpmiClient *client, const void *data)
{
	NpWebApiIpmiIn in[1];
	int ret;
	int result;

	ret = 0;
	in[0].data = data;
	in[0].size = 20;
	result = client->vtbl->invoke(client, 0x10000, in, 1, &ret, NULL, 0);
	if (result == 0)
		result = ret;
	return result;
}

/* starts the notifications of a user; the library context must exist */
int npPushRegisterUser(SceNpOnlineId *onlineId)
{
	NpPushLibContext *ctx;
	NpPushKey key;
	int result;

	ctx = npPushLibContextGet();
	if (ctx == NULL)
		return -2141871614;
	result = npPushLibContextStartNotification(ctx, SceNpCommonPs4_D0787194(&key, onlineId));
	SceNpCommonPs4_1543D7EE(&key);
	if (result >= 0)
		return 0;
	return result;
}

/* stops them */
int npPushUnregisterUser(SceNpOnlineId *onlineId)
{
	NpPushLibContext *ctx;
	NpPushKey key;
	int result;

	ctx = npPushLibContextGet();
	if (ctx == NULL)
		return -2141871614;
	result = npPushLibContextStopNotification(ctx, SceNpCommonPs4_D0787194(&key, onlineId));
	SceNpCommonPs4_1543D7EE(&key);
	if (result >= 0)
		return 0;
	return result;
}
