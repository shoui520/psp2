/*
 * SceNpCommerce2: the table of contexts and requests, a linked list of objects behind one
 * lightweight mutex. An object lives while someone holds a reference to it.
 */

#include "np_commerce2_internal.h"

SceKernelLwMutexWork g_tableLock;		/* 0x81027080 */

/* Init's settings, and the lock of the object table */
int npCommerce2InitTable(const char *environment, int npCommonValue, int environmentType)
{
	int ret;

	ret = npCommerce2SetNpEnvironment(environment);
	if (ret >= 0) {
		ret = npCommerce2SetNpCommonValue(npCommonValue);
		if (ret >= 0) {
			ret = npCommerce2SetEnvironmentType(environmentType);
			if (ret >= 0) {
				sceClibMemset(&g_tableLock, 0, 0x20);
				ret = sceKernelCreateLwMutex(&g_tableLock, "SceNpCommerce2", 2, 0, NULL);
				if (ret >= 0)
					return 0;
			}
		}
	}
	return ret;
}

/* Term: every object is released, then the lock goes */
int npCommerce2TermTable(void)
{
	npCommerce2_8101D9AC(&g_tableLock);
	return sceKernelDeleteLwMutex(&g_tableLock);
}
