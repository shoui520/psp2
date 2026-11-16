/*
 * SceNpWebApi: the push notification library (Sony's np_push/src: np_push_lib_context.cpp and
 * friends). The names are ours where the strings give none.
 */

#include "np_webapi_internal.h"

NpPushLibContext *g_pushLibContext;		/* 0x81010038 */
static int s_pushCallbackParam[4];		/* 0x81014080 */

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
	s_pushCallbackParam[0] = param[0];
	s_pushCallbackParam[1] = param[1];
	s_pushCallbackParam[2] = param[2];
	s_pushCallbackParam[3] = param[3];
	return s_pushCallbackParam[3];
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
