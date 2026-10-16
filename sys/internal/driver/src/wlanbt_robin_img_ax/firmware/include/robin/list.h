/* Intrusive doubly linked list used by the ROM-patch and driver glue. */
#ifndef ROBIN_LIST_H
#define ROBIN_LIST_H

#include <stdint.h>

struct RobinListNode {
	struct RobinListNode *next;	/* towards the tail */
	struct RobinListNode *prev;	/* towards the head */
};

struct RobinList {
	struct RobinListNode *head;
	struct RobinListNode *tail;
	uint32_t count;
};

void robinListInit(struct RobinList *list);
struct RobinListNode *robinListPopTail(struct RobinList *list);
void robinListPushHead(struct RobinList *list, struct RobinListNode *node);
struct RobinListNode *robinListRemove(struct RobinList *list, struct RobinListNode *node);

#endif /* ROBIN_LIST_H */
