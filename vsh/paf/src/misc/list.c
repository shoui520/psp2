/* Linked-list unlinking, draining and destruction. */
#include <stdint.h>
#include "../std/stdlib.h"

typedef struct PafListNode {
	struct PafListNode *previous;
	struct PafListNode *next;
	void *payload;
} PafListNode;

typedef struct PafList {
	PafListNode *head;
	int32_t count;
} PafList;

typedef struct PafListCursor {
	uint32_t reserved;
	PafListNode *head;
	int32_t count;
} PafListCursor;

static void UnlinkNode(PafListNode *node)
{
	PafListNode *previous = node->previous;
	PafListNode *next = node->next;

	next->previous = previous;
	previous->next = next;
	sce_paf_free(node);
}

uint32_t ScePafMisc_0078AC09(PafList *list, void *payload)
{
	PafListNode *head = list->head;
	PafListNode *node;

	if (!payload)
		return 0x80af5002U;
	node = head ? head->next : (PafListNode *)0;
	while (node != head) {
		PafListNode *next = node->next;

		if (node->payload == payload) {
			UnlinkNode(node);
			list->count--;
			return 0;
		}
		node = next;
	}
	return 0;
}

void *ScePafMisc_EB1C375C(PafListCursor *list)
{
	PafListNode *head, *node;
	void *payload;

	if (!list->count)
		return 0;
	head = list->head;
	node = head->previous;
	payload = node->payload;
	if (payload)
		return payload;
	while (list->count) {
		node = head->previous;
		UnlinkNode(node);
		list->count--;
	}
	return 0;
}

static PafList *DestroyList(PafList *list)
{
	PafListNode *head = list->head;

	if (head) {
		PafListNode *node = head->next;

		while (node != head) {
			PafListNode *next = node->next;

			sce_paf_free(node);
			node = next;
		}
		sce_paf_free(head);
	}
	list->head = (PafListNode *)(uintptr_t)UINT32_MAX;
	list->count = -1;
	return list;
}

PafList *ScePafMisc_04B4652F(PafList *list)
{
	return DestroyList(list);
}

PafList *ScePafMisc_600A1245(PafList *list)
{
	return DestroyList(list);
}

void *ScePafMisc_822EC852(PafListCursor *list)
{
	PafListNode *head;
	PafListNode *node;
	void *payload;

	if (!list->count)
		return 0;
	head = list->head;
	node = head->next;
	payload = node->payload;
	while (!payload) {
		UnlinkNode(node);
		list->count--;
		if (!list->count)
			return 0;
		node = head->next;
		payload = node->payload;
	}
	return payload;
}
