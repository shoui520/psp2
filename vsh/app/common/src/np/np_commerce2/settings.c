/*
 * SceNpCommerce2: the settings Init takes from SceNpCommon (the NP environment's name and
 * two values) and the library's heap.
 */

#include "np_commerce2_internal.h"

int g_npCommonValue = 0;		/* 0x8102601C */
int g_environmentType = 0;		/* 0x81026020 */

char g_npEnvironment[17];		/* 0x810270A0 */

int npCommerce2SetNpEnvironment(const char *environment)
{
	int ret = 0;

	if (environment == NULL)
		return NP_COMMERCE2_ERROR_NO_ENVIRONMENT;
	if (sceClibStrnlen(environment, 0x10) >= 0x11)
		return NP_COMMERCE2_ERROR_ENVIRONMENT_TOO_LONG;
	sceClibStrncpy(g_npEnvironment, environment, 0x11);
	g_npEnvironment[16] = '\0';
	return ret;
}

int npCommerce2SetNpCommonValue(int value)
{
	g_npCommonValue = value;
	return 0;
}

int npCommerce2GetEnvironment(void)
{
	return g_environmentType;
}

int npCommerce2SetEnvironmentType(int type)
{
	g_environmentType = type;
	return 0;
}

void *npCommerce2Alloc(SceSize size)
{
	return sceClibMspaceMalloc(g_heap, size);
}

void npCommerce2Free(void *ptr)
{
	sceClibMspaceFree(g_heap, ptr);
}
