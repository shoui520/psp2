/* Linked-list unlinking, draining and destruction. */
#include <stdint.h>
#include "../std/stdlib.h"
#include "../std/new.h"

typedef struct PafListNode {
	struct PafListNode *previous;
	struct PafListNode *next;
	void *payload;
} PafListNode;

typedef struct PafListCursor {
	uint32_t reserved;
	PafListNode *head;
	int32_t count;
} PafListCursor;

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
