/*
 * SceNpWebApi: the library's global state.
 */

#include "np_webapi_internal.h"

volatile int g_initState;		/* 0x81010000 */
volatile int g_initCount;		/* 0x81010004 */
volatile int g_defaultLibCtxId;		/* 0x81010008 */
SceUInt32 g_sdkVersion;			/* 0x8101000C */

SceUInt32 npWebApiGetSdkVersion(void)
{
	return g_sdkVersion;
}

/* the root of Sony's classes: an object's vtable becomes this one while its base is destroyed */
void *npWebApiBaseDestruct(void *object)
{
	*(const void *const **)object = g_baseVtable;
	return object;
}

int npWebApiBaseDelete(void *object)
{
	*(const void *const **)object = g_baseVtable;
	return SceNpCommonPs4_BB5282C8(object);
}

const void *g_baseVtable[] = {		/* 0x8100eb6c */
	(const void *)npWebApiBaseDestruct,
	(const void *)npWebApiBaseDelete,
	NULL,
	NULL,
};
