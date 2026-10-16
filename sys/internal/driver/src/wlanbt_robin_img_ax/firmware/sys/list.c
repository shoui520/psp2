/*
 * Doubly linked list with interrupts masked around every change. The list
 * is first-in first-out: nodes enter at the head and leave at the tail. The
 * filename is inferred.
 */
#include <stddef.h>
#include "robin/list.h"
#include "robin/threadx.h"

void robinListInit(struct RobinList *list)
{
	if (list == NULL)
		return;
	list->head = NULL;
	list->tail = NULL;
	list->count = 0;
}

/* Removes and returns the oldest node, or NULL when the list is empty. */
struct RobinListNode *robinListPopTail(struct RobinList *list)
{
	unsigned int posture = tx_interrupt_control(TX_INT_DISABLE);
	struct RobinListNode *node;
	struct RobinListNode *previous;

	if (list == NULL || list->count == 0) {
		tx_interrupt_control(posture);
		return NULL;
	}
	node = list->tail;
	previous = node->prev;
	if (previous != NULL) {
		list->tail = previous;
		previous->next = NULL;
	} else {
		list->tail = NULL;
		list->head = NULL;
	}
	node->prev = NULL;
	node->next = NULL;
	--list->count;
	tx_interrupt_control(posture);
	return node;
}

void robinListPushHead(struct RobinList *list, struct RobinListNode *node)
{
	unsigned int posture;

	if (list == NULL || node == NULL)
		return;
	posture = tx_interrupt_control(TX_INT_DISABLE);
	node->next = list->head;
	node->prev = NULL;
	if (list->head != NULL)
		list->head->prev = node;
	else
		list->tail = node;
	list->head = node;
	++list->count;
	tx_interrupt_control(posture);
}

/* Unlinks a node from anywhere in the list; returns NULL if there is none to unlink. */
struct RobinListNode *robinListRemove(struct RobinList *list, struct RobinListNode *node)
{
	unsigned int posture;
	struct RobinListNode *previous;

	if (list == NULL || node == NULL || list->count == 0)
		return NULL;
	posture = tx_interrupt_control(TX_INT_DISABLE);
	previous = node->prev;
	if (previous != NULL) {
		if (node->next != NULL) {
			previous->next = node->next;
			node->next->prev = node->prev;
		} else {
			previous->next = NULL;
			list->tail = node->prev;
		}
	} else if (node->next != NULL) {
		node->next->prev = NULL;
		list->head = node->next;
	} else {
		list->tail = NULL;
		list->head = NULL;
	}
	node->next = NULL;
	node->prev = NULL;
	--list->count;
	tx_interrupt_control(posture);
	return node;
}
