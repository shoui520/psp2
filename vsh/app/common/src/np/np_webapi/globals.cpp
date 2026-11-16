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
