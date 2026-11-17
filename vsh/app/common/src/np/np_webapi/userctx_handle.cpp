/*
 * SceNpWebApi: the objects the user context creation allocates besides the user context itself. The
 * names are ours.
 */

#include "np_webapi_internal.h"

NpWebApiUserContextHandle *npWebApiLibContextCreateUserContextHandle(NpWebApiLibContext *libCtx)
{
	NpWebApiUserContextHandle *handle;
	NpWebApiUserContextHandle *result;

	result = NULL;
	handle = (NpWebApiUserContextHandle *)SceNpCommonPs4_D154D84F(12, (unsigned char *)libCtx->heapBlock + 20);
	if (handle != NULL) {
		handle->userContext = NULL;
		handle->reserved = 0;
		handle->vtable = g_userContextHandleVtable;
		result = handle;
		handle->userContext = libCtx->userContextHead;
		handle->reserved = 0;
	}
	return result;
}

NpWebApiUserObject *npWebApiUserObjectConstruct(NpWebApiUserObject *object, NpWebApiLibContext *libCtx,
	NpWebApiUserContext *userCtx)
{
	object->libContext = libCtx;
	object->vtable = g_userObjectVtable;
	object->onlineId = npWebApiUserContextOnlineId(userCtx);
	sceClibMemset(object->buffer16, 0, 0x108);
	sceClibMemset(object->buffer280, 0, 0x410);
	return object;
}

NpWebApiUserContextHandle *npWebApiUserContextHandleDestruct(NpWebApiUserContextHandle *handle)
{
	handle->vtable = g_baseVtable;
	return handle;
}

int npWebApiUserContextHandleDelete(NpWebApiUserContextHandle *handle)
{
	handle->vtable = g_baseVtable;
	return SceNpCommonPs4_BB5282C8(handle);
}

const void *g_userContextHandleVtable[] = {	/* 0x8100eb9c */
	(const void *)npWebApiUserContextHandleDestruct,
	(const void *)npWebApiUserContextHandleDelete,
};

NpWebApiUserObject *npWebApiUserObjectDestruct(NpWebApiUserObject *object)
{
	object->vtable = g_baseVtable;
	return object;
}

int npWebApiUserObjectDelete(NpWebApiUserObject *object)
{
	object->vtable = g_baseVtable;
	return SceNpCommonPs4_BB5282C8(object);
}

const void *g_userObjectVtable[] = {		/* 0x8100edcc */
	(const void *)npWebApiUserObjectDestruct,
	(const void *)npWebApiUserObjectDelete,
	NULL,
	NULL,
};
