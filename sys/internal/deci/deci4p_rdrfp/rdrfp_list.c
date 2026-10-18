/*
 * SceDeci4pRDrfp lists (Sony's rdrfp_list.c): elements on the heap pointing
 * at their data, guarded by a mutex.
 */
#include <threadmgr/threadmgr.h>
#include <string.h>
#include "rdrfp.h"

int rdrfpInitList(RdrfpList *list)
{
	int ret;

	list->head.next = &list->head;
	list->head.prev = &list->head;
	ret = sceKernelCreateMutex("SceDeci4pRDrfp", 0x2000, 0, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 38, "CreateMutex failed (0x%08X)\n", ret);
		return ret;
	}
	list->mutex = ret;
	list->count = 0;
	return 0;
}

int rdrfpDeleteList(RdrfpList *list)
{
	sceKernelDeleteMutex(list->mutex);
	return 0;
}

RdrfpListElem *rdrfpAddToList(RdrfpList *list, void *data)
{
	RdrfpListElem *elem = rdrfpAlloc(sizeof(*elem));
	int count;

	if (elem == NULL)
		return NULL;
	memset(elem, 0, sizeof(*elem));
	elem->link.next = &elem->link;
	elem->link.prev = &elem->link;
	elem->data = data;
	sceKernelLockMutex(list->mutex, 1, NULL);
	SCE_DBG_ASSERT_LEVEL(0, &elem->link == elem->link.next, 81);
	count = list->count;
	elem->link.next = &list->head;
	elem->link.prev = list->head.prev;
	list->head.prev = &elem->link;
	elem->link.prev->next = &elem->link;
	list->count = count + 1;
	sceKernelUnlockMutex(list->mutex, 1);
	return elem;
}

void rdrfpRemoveFromList(RdrfpList *list, RdrfpListElem *elem)
{
	sceKernelLockMutex(list->mutex, 1, NULL);
	if (&elem->link != elem->link.next) {
		elem->link.next->prev = elem->link.prev;
		elem->link.prev->next = elem->link.next;
		elem->link.next = &elem->link;
		elem->link.prev = &elem->link;
		list->count--;
	}
	sceKernelUnlockMutex(list->mutex, 1);
	rdrfpFree(elem);
}

RdrfpListElem *rdrfpGetListElem(RdrfpList *list, int index)
{
	RdrfpLink *link;
	int i;

	if (list->count == 0 || index >= list->count)
		return NULL;
	sceKernelLockMutex(list->mutex, 1, NULL);
	link = list->head.next;
	for (i = 0; i < index; i++)
		link = link->next;
	sceKernelUnlockMutex(list->mutex, 1);
	SCE_DBG_ASSERT_LEVEL(0, link != &list->head, 121);
	return (RdrfpListElem *)link;
}

/* the first element whose data compares equal (0) to the key */
RdrfpListElem *rdrfpFindListElem(RdrfpListCompare compare, RdrfpList *list, const void *key)
{
	RdrfpLink *link;

	if (list->count == 0)
		return NULL;
	sceKernelLockMutex(list->mutex, 1, NULL);
	for (link = list->head.next; link != &list->head; link = link->next) {
		if (compare(((RdrfpListElem *)link)->data, key) == 0)
			goto out;
	}
	link = NULL;
out:
	sceKernelUnlockMutex(list->mutex, 1);
	return (RdrfpListElem *)link;
}
