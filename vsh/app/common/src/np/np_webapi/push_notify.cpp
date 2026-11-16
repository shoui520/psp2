/*
 * SceNpWebApi: the push library's registry of users it listens for (Sony's np_push/src:
 * np_push_lib_context.cpp). StopNotification is Sony's name (its assertion); the others are ours.
 */

#include "np_webapi_internal.h"


void *operator new(unsigned int size, void *where)
{
	return where;
}

static NpPushUserNode *npPushUserNodeCreate(void *allocator, const NpPushUserValue *value)
{
	void *memory;
	NpPushUserNode *node;
	NpPushUserNode *result;

	result = NULL;
	if (allocator != NULL) {
		memory = SceNpCommonPs4_AA980113(allocator, 36);
		if (memory != NULL) {
			node = (NpPushUserNode *)operator new(36, memory);
			if (node != NULL) {
				SceNpCommonPs4_AEC0DAC5(&node->key, &value->key);
				result = node;
				node->refCount = value->refCount;
			}
		}
	}
	return result;
}

/* the list's insert: a ring around a sentinel node made when the first node comes */
void npPushUserListInsert(NpPushUserNode **pResult, NpPushUserList *list, const NpPushUserValue *value)
{
	NpPushUserValue copy;
	NpPushUserValue sentinelValue;
	NpPushUserNode *sentinel;
	NpPushUserNode *node;
	int count;

	if (list->sentinel == NULL) {
		SceNpCommonPs4_B0BAB9BE(&sentinelValue.key);
		list->sentinel = npPushUserNodeCreate(list->allocator, &sentinelValue);
		SceNpCommonPs4_1543D7EE(&sentinelValue.key);
		sentinel = list->sentinel;
		if (sentinel == NULL) {
			*pResult = NULL;
			return;
		}
		sentinel->prev = sentinel;
		sentinel->next = sentinel;
	}
	SceNpCommonPs4_AEC0DAC5(&copy.key, &value->key);
	copy.refCount = value->refCount;
	node = npPushUserNodeCreate(list->allocator, &copy);
	SceNpCommonPs4_1543D7EE(&copy.key);
	if (node == NULL) {
		*pResult = NULL;
		return;
	}
	node->prev = list->sentinel->prev;
	node->next = list->sentinel;
	sentinel = list->sentinel;
	sentinel->prev->next = node;
	count = list->count + 1;
	list->sentinel->prev = node;
	list->count = count;
	*pResult = node;
}

int npPushUserListErase(NpPushUserNode **pOut, NpPushUserList *list, NpPushUserNode **pAt)
{
	NpPushUserNode *node;
	NpPushUserNode *next;
	NpPushUserNode *prev;
	NpPushUserNode *result;
	int count;

	node = *pAt;
	result = node->next;
	next = result;
	if (node == list->sentinel) {
		SceNpCommonPs4_F50BD96A(sceClibPrintf("Assertion in %s(%s:%d): %s(%s)\n", "erase",
			"../../../../../app/common/src/np/include/np_common_ps4/npext/list.h", 364, "HALT_ASSERT",
			"at != end()"));
		next = node->next;
	}
	prev = node->prev;
	next->prev = prev;
	prev->next = node->next;
	if (list->allocator != NULL) {
		SceNpCommonPs4_1543D7EE(&node->key);
		SceNpCommonPs4_6B428829(list->allocator, node);
	}
	count = list->count - 1;
	list->count = count;
	*pOut = result;
	return count;
}

int npPushLibContextAcquireUser(NpPushLibContext *ctx, const NpPushKey *key, int *pCount)
{
	NpPushUserNode *sentinel;
	NpPushUserNode *node;
	NpPushUserNode *iterator;
	NpPushUserValue value;
	const unsigned char *src;
	unsigned char *dst;
	int i;
	int result;

	sentinel = ctx->users.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	iterator = ctx->users.sentinel;
	if (node != sentinel) {
		while (SceNpCommonPs4_21B2C0B7(&node->key, key) == 0) {
			node = node->next;
			iterator = ctx->users.sentinel;
			if (node == iterator)
				goto insert;
		}
		node->refCount++;
		*pCount = node->refCount;
		return 0;
	}
insert:
	SceNpCommonPs4_B0BAB9BE(&value.key);
	src = key->id;
	dst = value.key.id;
	for (i = 5; i != 0; i--) {
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		dst[3] = src[3];
		src += 4;
		dst += 4;
	}
	value.refCount = 1;
	npPushUserListInsert(&iterator, &ctx->users, &value);
	if (iterator != NULL) {
		*pCount = value.refCount;
		SceNpCommonPs4_1543D7EE(&value.key);
		result = 0;
	} else {
		SceNpCommonPs4_1543D7EE(&value.key);
		result = -2141871612;
	}
	return result;
}

int npPushLibContextReleaseUser(NpPushLibContext *ctx, const NpPushKey *key, int *pCount)
{
	NpPushUserNode *sentinel;
	NpPushUserNode *node;
	NpPushUserNode *result;
	NpPushUserNode *at;
	int count;

	sentinel = ctx->users.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	if (node == sentinel)
		return -2141871616;
	while (SceNpCommonPs4_21B2C0B7(&node->key, key) == 0) {
		node = node->next;
		if (node == ctx->users.sentinel)
			return -2141871616;
	}
	count = node->refCount - 1;
	node->refCount = count;
	*pCount = count;
	if (count <= 0) {
		at = node;
		npPushUserListErase(&result, &ctx->users, &at);
	}
	return 0;
}

int npPushLibContextStartNotification(NpPushLibContext *ctx, const NpPushKey *key)
{
	int count;
	int result;

	SceNpCommonPs4_A2A8493B(ctx->mutex);
	if ((ctx->flags & 1) == 0) {
		result = SceNpCommonPs4_D84BF4B7(0);
		if (result < 0) {
			SceNpCommonPs4_6BA5A164(ctx->mutex);
			return result;
		}
		ctx->flags |= 1;
	}
	count = 0;
	if (npPushLibContextAcquireUser(ctx, key, &count) == 0 && count == 1
	    && (result = npPushIpcRegisterId(SceNpCommonPs4_A0599F24(), key->id)) < 0) {
		npPushLibContextReleaseUser(ctx, key, &count);
		SceNpCommonPs4_6BA5A164(ctx->mutex);
		return result;
	}
	SceNpCommonPs4_6BA5A164(ctx->mutex);
	return 0;
}

int npPushLibContextStopNotification(NpPushLibContext *ctx, const NpPushKey *key)
{
	int count;
	int result;
	int ret;

	SceNpCommonPs4_A2A8493B(ctx->mutex);
	count = 0;
	ret = npPushLibContextReleaseUser(ctx, key, &count);
	if (ret != 0)
		SceNpCommonPs4_F50BD96A(sceClibPrintf("Assertion in %s(%s:%d): %s(%s)\n", "StopNotification",
			"W:\\Build\\J01509658\\vsh\\app\\common\\src\\np\\np_push\\src\\np_push_lib_context.cpp", 668,
			"ASSERT", "ret == 0"));
	if ((count | ret) != 0 || (result = npPushIpcSendId(SceNpCommonPs4_A0599F24(), key->id)) >= 0) {
		SceNpCommonPs4_6BA5A164(ctx->mutex);
		return 0;
	}
	SceNpCommonPs4_6BA5A164(ctx->mutex);
	return result;
}
