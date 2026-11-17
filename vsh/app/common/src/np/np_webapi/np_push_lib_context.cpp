/*
 * SceNpWebApi: the push library's context (Sony's np_push/src/np_push_lib_context.cpp and
 * np_push_data_filter.cpp): made once, with the locks, lists and buffers the receive path uses. The
 * names are ours where the asserts give none.
 */

#include "np_webapi_internal.h"

unsigned char g_pushReceiveBuffer[0x2000];	/* 0x81012080 */

void *npPushAllocatorGet(void)
{
	return g_pushAllocator;
}

void *npPushAlloc(int size)
{
	return SceNpCommonPs4_AA980113(g_pushAllocator, size);
}

int npPushFree(void *memory)
{
	return SceNpCommonPs4_6B428829(g_pushAllocator, memory);
}

const void *const g_pushLibContextVtable[] = {		/* 0x8100ef80 */
	(const void *)npPushLibContextDestruct,
	(const void *)npPushLibContextDeleteObject,
	NULL,
	NULL,
};


const void *const g_pushServiceListVtable[] = {		/* 0x8100f100 */
	(const void *)npPushServiceListDestruct,
	(const void *)npPushServiceListDelete,
	NULL,
	NULL,
};

NpPushLibContext *npPushLibContextConstruct(NpPushLibContext *ctx)
{
	void *allocator;

	ctx->vtable = (const void **)g_pushLibContextVtable;
	SceNpCommonPs4_4266D870(ctx->receiveLock);
	SceNpCommonPs4_4266D870(ctx->mutex);
	ctx->callbacks.allocator = npPushAllocatorGet();
	ctx->callbacks.sentinel = NULL;
	ctx->callbacks.count = 0;
	npPushServiceListConstruct(&ctx->services);
	allocator = npPushAllocatorGet();
	ctx->serviceCallbacks.sentinel = NULL;
	ctx->serviceCallbacks.count = 0;
	ctx->serviceCallbacks.allocator = allocator;
	allocator = npPushAllocatorGet();
	ctx->users.sentinel = NULL;
	ctx->users.allocator = allocator;
	ctx->users.count = 0;
	ctx->receiveBuffer = NULL;
	ctx->exDataBuffer = NULL;
	ctx->exDataNames = NULL;
	ctx->flags = 0;
	return ctx;
}

NpPushLibContext *npPushLibContextDestruct(NpPushLibContext *ctx)
{
	NpPushUserNode *userSentinel;
	NpPushUserNode *userNode;
	NpPushUserNode *userNext;
	NpPushServiceCallbackNode *serviceSentinel;
	NpPushServiceCallbackNode *serviceNode;
	NpPushServiceCallbackNode *serviceNext;
	NpPushCallbackNode *callbackSentinel;
	NpPushCallbackNode *callbackNode;
	NpPushCallbackNode *callbackNext;
	void *allocator;

	ctx->vtable = (const void **)g_pushLibContextVtable;
	npPushFree(ctx->receiveBuffer);
	npPushFree(ctx->exDataBuffer);
	if ((ctx->flags & 1) != 0)
		SceNpCommonPs4_1096EAD4(0);
	if (ctx->exDataNames != NULL)
		SceNpCommonPs4_93C97239(ctx->exDataNames);
	userSentinel = ctx->users.sentinel;
	if (userSentinel != NULL) {
		userNode = userSentinel->next;
		allocator = ctx->users.allocator;
		if (userNode != userSentinel) {
			do {
				userNext = userNode->next;
				if (allocator != NULL) {
					SceNpCommonPs4_1543D7EE(&userNode->key);
					SceNpCommonPs4_6B428829(ctx->users.allocator, userNode);
					allocator = ctx->users.allocator;
					userSentinel = ctx->users.sentinel;
				}
				userNode = userNext;
			} while (userNext != userSentinel);
		}
		if (allocator != NULL) {
			SceNpCommonPs4_1543D7EE(&userNode->key);
			SceNpCommonPs4_6B428829(ctx->users.allocator, userNode);
		}
	}
	serviceSentinel = ctx->serviceCallbacks.sentinel;
	if (serviceSentinel != NULL) {
		serviceNode = serviceSentinel->next;
		allocator = ctx->serviceCallbacks.allocator;
		if (serviceNode != serviceSentinel) {
			do {
				serviceNext = serviceNode->next;
				if (allocator != NULL) {
					SceNpCommonPs4_6B428829(allocator, serviceNode);
					allocator = ctx->serviceCallbacks.allocator;
					serviceSentinel = ctx->serviceCallbacks.sentinel;
				}
				serviceNode = serviceNext;
			} while (serviceNext != serviceSentinel);
		}
		if (allocator != NULL)
			SceNpCommonPs4_6B428829(allocator, serviceNode);
	}
	npPushServiceListDestruct(&ctx->services);
	callbackSentinel = ctx->callbacks.sentinel;
	if (callbackSentinel != NULL) {
		callbackNode = callbackSentinel->next;
		allocator = ctx->callbacks.allocator;
		if (callbackNode != callbackSentinel) {
			do {
				callbackNext = callbackNode->next;
				if (allocator != NULL) {
					SceNpCommonPs4_6B428829(allocator, callbackNode);
					allocator = ctx->callbacks.allocator;
					callbackSentinel = ctx->callbacks.sentinel;
				}
				callbackNode = callbackNext;
			} while (callbackNext != callbackSentinel);
		}
		if (allocator != NULL)
			SceNpCommonPs4_6B428829(allocator, callbackNode);
	}
	SceNpCommonPs4_294F0B1A(ctx->mutex);
	SceNpCommonPs4_294F0B1A(ctx->receiveLock);
	ctx->vtable = (const void **)g_baseVtable;
	return ctx;
}

int npPushLibContextDeleteObject(NpPushLibContext *ctx)
{
	npPushLibContextDestruct(ctx);
	return SceNpCommonPs4_BB5282C8(ctx);
}

NpWebApiExtdEntryList *npPushServiceListConstruct(NpWebApiExtdEntryList *list)
{
	list->reserved = (void *)g_pushServiceListVtable;
	list->tail = NULL;
	list->head = NULL;
	return list;
}

NpWebApiExtdEntryList *npPushServiceListDestruct(NpWebApiExtdEntryList *list)
{
	list->reserved = (void *)g_baseVtable;
	return list;
}

int npPushServiceListDelete(NpWebApiExtdEntryList *list)
{
	list->reserved = (void *)g_baseVtable;
	return SceNpCommonPs4_BB5282C8(list);
}

int npPushLibContextInit(NpPushLibContext *ctx)
{
	NpPushExDataNames *names;
	NpPushExDataNames *current;
	void *memory;
	int result;

	result = SceNpCommonPs4_8AA7E995(ctx->receiveLock, "SceNpPushCb", 0);
	if (result >= 0) {
		result = SceNpCommonPs4_8AA7E995(ctx->mutex, "SceNpPushReg", 0);
		if (result >= 0) {
			ctx->receiveBuffer = g_pushReceiveBuffer;
			if (g_pushReceiveBuffer != NULL) {
				ctx->exDataBuffer = npPushAlloc(120);
				if (ctx->exDataBuffer != NULL) {
					names = NULL;
					memory = SceNpCommonPs4_0C922423(16, npPushAllocatorGet());
					if (memory != NULL)
						names = npPushDataFilterConstruct((NpPushExDataNames *)memory);
					current = ctx->exDataNames;
					if (names != current) {
						if (current != NULL)
							SceNpCommonPs4_93C97239(current);
						current = names;
						ctx->exDataNames = names;
						if (names != NULL) {
							SceNpCommonPs4_64BAC888(names);
							current = ctx->exDataNames;
						}
					}
					if (current != NULL) {
						result = npPushDataFilterInit(ctx->exDataNames);
						if (result >= 0) {
							ctx->flags = 0;
							return 0;
						}
					} else {
						result = -2141871612;
					}
				} else {
					result = -2141871612;
				}
			} else {
				result = -2141871612;
			}
		}
	}
	if (ctx->exDataNames != NULL) {
		SceNpCommonPs4_93C97239(ctx->exDataNames);
		ctx->exDataNames = NULL;
	}
	npPushFree(ctx->exDataBuffer);
	ctx->receiveBuffer = NULL;
	ctx->exDataBuffer = NULL;
	if (SceNpCommonPs4_C78F5DF5(ctx->mutex) != 0)
		SceNpCommonPs4_63EED478(ctx->mutex);
	if (SceNpCommonPs4_C78F5DF5(ctx->receiveLock) != 0)
		SceNpCommonPs4_63EED478(ctx->receiveLock);
	return result;
}

int npPushLibContextInitInstance(void)
{
	NpPushLibContext *ctx;
	void *memory;
	int result;

	if (g_pushLibContext != NULL)
		SceNpCommonPs4_F50BD96A(sceClibPrintf("Assertion in %s(%s:%d): %s(%s)\n", "InitInstance",
			"W:\\Build\\" NP_WEBAPI_BUILD "\\vsh\\app\\common\\src\\np\\np_push\\src\\np_push_lib_context.cpp", 1124,
			"ASSERT", "s_instance == 0"));
	ctx = NULL;
	memory = SceNpCommonPs4_0C922423(92, npPushAllocatorGet());
	if (memory != NULL)
		ctx = npPushLibContextConstruct((NpPushLibContext *)memory);
	g_pushLibContext = ctx;
	if (ctx == NULL) {
		result = -2141871612;
	} else {
		result = npPushLibContextInit(ctx);
		if (result >= 0)
			return 0;
	}
	if (g_pushLibContext != NULL) {
		((void (*)(NpPushLibContext *))g_pushLibContext->vtable[1])(g_pushLibContext);
		g_pushLibContext = NULL;
	}
	return result;
}
