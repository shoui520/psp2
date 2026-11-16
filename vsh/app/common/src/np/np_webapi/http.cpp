/*
 * SceNpWebApi: HTTP helpers: the library's error for an HTTP(S) failure, the services whose names
 * the library treats specially, and a list of objects of a library context. The names are ours.
 */

#include "np_webapi_internal.h"

int npWebApiConvertHttpError(int id, int error)
{
	int errNum;
	unsigned int detail;
	unsigned int code;
	int sslError;
	int result;

	errNum = 0;
	detail = 0;
	sslError = sceHttpsGetSslError(id, &errNum, &detail);
	result = error;
	if (sslError >= 0) {
		code = errNum;
		if (errNum == -2143072160) {
			sslError = -2141902333;
			if ((detail & 4) == 0) {
				sslError = -2141902332;
				if ((detail & 0x20) == 0) {
					sslError = -2141902331;
					if ((detail & 8) == 0) {
						sslError = -2141902330;
						if ((detail & 0x10) == 0) {
							sslError = -2141902328;
							if ((detail & 2) != 0)
								sslError = -2141902329;
						}
					}
				}
			}
			result = sslError;
		} else if (errNum == -2143072158) {
			if (detail != 0)
				code = detail | 0x80410100;
			result = code;
		} else if (errNum < 0) {
			result = errNum;
		}
	}
	return result;
}

int npWebApiIsUserProfileService(const char *service)
{
	if (service == NULL)
		return 0;
	if (sceClibStrncmp(service, "userProfile", sceClibStrnlen("userProfile", 0xFFFFFFFF)) != 0
	    && sceClibStrncmp(service, "sdk:userProfile", sceClibStrnlen("sdk:userProfile", 0xFFFFFFFF)) != 0
	    && sceClibStrncmp(service, "sessionInvitation", sceClibStrnlen("sessionInvitation", 0xFFFFFFFF)) != 0
	    && sceClibStrncmp(service, "sdk:sessionInvitation", sceClibStrnlen("sdk:sessionInvitation", 0xFFFFFFFF)) != 0)
		return 0;
	return 1;
}

int npWebApiIsAccountsService(const char *service)
{
	if (service == NULL)
		return 0;
	if (sceClibStrncmp(service, "accounts", sceClibStrnlen("accounts", 0xFFFFFFFF)) != 0
	    && sceClibStrncmp(service, "sdk:identityMapper", sceClibStrnlen("sdk:identityMapper", 0xFFFFFFFF)) != 0)
		return 0;
	return 1;
}

NpWebApiObjectList *npWebApiObjectListConstruct(NpWebApiObjectList *list, NpWebApiLibContext *libCtx)
{
	list->libContext = libCtx;
	list->vtable = g_objectListVtable;
	list->count = 0;
	list->head = NULL;
	return list;
}

NpWebApiObjectList *npWebApiObjectListDestruct(NpWebApiObjectList *list)
{
	NpWebApiLibContext *libCtx;
	NpWebApiObjectNode *node;
	NpWebApiObjectNode *next;

	libCtx = list->libContext;
	list->vtable = g_objectListVtable;
	npWebApiLibContextLock(libCtx);
	node = list->head;
	libCtx = list->libContext;
	if (node != NULL) {
		do {
			next = node->next;
			npWebApiLibContextDestroyPool(libCtx);
			libCtx = list->libContext;
			node = next;
		} while (next != NULL);
	}
	list->count = 0;
	npWebApiLibContextUnlock(libCtx);
	list->vtable = g_baseVtable;
	return list;
}

int npWebApiObjectListDeleteThis(NpWebApiObjectList *list)
{
	NpWebApiLibContext *libCtx;
	NpWebApiObjectNode *node;
	NpWebApiObjectNode *next;

	libCtx = list->libContext;
	list->vtable = g_objectListVtable;
	npWebApiLibContextLock(libCtx);
	node = list->head;
	libCtx = list->libContext;
	if (node != NULL) {
		do {
			next = node->next;
			npWebApiLibContextDestroyPool(libCtx);
			libCtx = list->libContext;
			node = next;
		} while (next != NULL);
	}
	list->count = 0;
	npWebApiLibContextUnlock(libCtx);
	list->vtable = g_baseVtable;
	return SceNpCommonPs4_BB5282C8(list);
}
