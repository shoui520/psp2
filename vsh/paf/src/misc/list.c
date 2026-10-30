/* Linked-list unlinking, draining and destruction. */
#include <stdint.h>
#include "../std/stdlib.h"
#include "../std/new.h"

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

static PafList *ConstructList(PafList *list)
{
	PafListNode *head;

	list->count = 0;
	head = PafNew(sizeof(*head));
	if (!head)
		ScePafStdc_D608246A();
	list->head = head;
	if (head) {
		head->previous = head;
		head->next = head;
	}
	return list;
}

PafList *ScePafMisc_45755C06(PafList *list)
{
	return ConstructList(list);
}

PafList *ScePafMisc_BEA27D34(PafList *list)
{
	return ConstructList(list);
}

uint32_t ScePafMisc_0078AC09(PafList *list, void *payload)
{
	PafListNode *head;
	PafListNode *node;

	if (!payload)
		return 0x80af5002U;
	head = list->head;
	node = head ? head->next : (PafListNode *)0;
	while (node != head) {
		if (node->payload == payload) {
			PafListNode *previous = node->previous;
			node->next->previous = previous;
			previous->next = node->next;
			PafDelete(node);
			list->count--;
			return 0;
		}
		if (node)
			node = node->next;
	}
	return 0;
}

void *ScePafMisc_EB1C375C(PafListCursor *list)
{
	PafListNode *head, *node;
	void *payload;
	int32_t count;

	count = list->count;
	if (!count)
		return 0;
	head = list->head;
	node = head->previous;
	payload = node->payload;
	if (payload)
		return payload;
	do {
		if (head) {
			PafListNode *previous;
			node = head->previous;
			previous = node->previous;
			node->next->previous = previous;
			previous->next = node->next;
			PafDelete(node);
			count = list->count - 1;
			head = list->head;
			list->count = count;
		}
	} while (count);
	return 0;
}

static PafList *DestroyList(PafList *list)
{
	PafListNode *head = list->head;

	if (head) {
		PafListNode *node = head->next;

		while (node != list->head) {
			PafListNode *next = node->next;

			PafDelete(node);
			node = next;
		}
		PafDelete(list->head);
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
	int32_t count = list->count;

	if (!count)
		return 0;
	head = list->head;
	node = head->next;
	payload = node->payload;
	while (!payload) {
		if (head) {
			PafListNode *previous = node->previous;
			node->next->previous = previous;
			previous->next = node->next;
			PafDelete(node);
			count = list->count - 1;
			head = list->head;
			list->count = count;
		}
		if (!count)
			return 0;
		node = head->next;
		payload = node->payload;
	}
	return payload;
}
