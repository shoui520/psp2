/*
 * ScebXCe's node list (the SDK's sce::Xml::Dom::NodeList): the nodes in a doubly linked list
 * of entries taken from the heap.
 */
#include "bxce_internal.h"

BxceNodeList *bxceNodeListInitialize(BxceNodeList *list, const BxceDocument *doc)
{
	list->doc = doc;
	list->tail = NULL;
	list->length = 0;
	list->head = NULL;
	return list;
}

/* clear: frees every entry; the tail and the length are left as they are */
void ScebXCe_1F59F0C4(BxceNodeList *list)
{
	BxceNodeListEntry *entry;

	if (list->head == NULL)
		return;
	do {
		entry = list->head;
		list->head = entry->next;
		ScebXCe_EC560647(entry);
	} while (list->head != NULL);
}

/* insertFirst */
BxceNodeId bxceNodeListInsertFirst(BxceNodeList *list, BxceNodeId node)
{
	BxceNodeListEntry *entry, *head;

	entry = (BxceNodeListEntry *)ScebXCe_B8DDE741(sizeof(*entry));
	if (entry == NULL)
		return 0;

	bxceMemset(entry, 0, sizeof(*entry));
	entry->node = node;
	head = list->head;
	if (head != NULL) {
		head->prev = entry;
		entry->next = head;
	} else {
		list->tail = entry;
	}
	list->head = entry;
	list->length++;
	return node;
}

/* insertLast */
BxceNodeId bxceNodeListInsertLast(BxceNodeList *list, BxceNodeId node)
{
	BxceNodeListEntry *entry, *tail;

	entry = (BxceNodeListEntry *)ScebXCe_B8DDE741(sizeof(*entry));
	if (entry == NULL)
		return 0;

	bxceMemset(entry, 0, sizeof(*entry));
	entry->node = node;
	tail = list->tail;
	if (tail != NULL) {
		tail->next = entry;
		entry->prev = tail;
	} else {
		list->head = entry;
	}
	list->tail = entry;
	list->length++;
	return node;
}

/* removeItem: the node, or 0 when the list does not hold it */
BxceNodeId bxceNodeListRemoveItem(BxceNodeList *list, BxceNodeId node)
{
	BxceNodeListEntry *entry;

	entry = list->head;
	if (entry == NULL)
		return 0;

	while (entry != NULL) {
		BxceNodeId found = entry->node;

		if (node == found) {
			if (entry->prev != NULL)
				entry->prev->next = entry->next;
			else
				list->head = entry->next;
			if (entry->next != NULL)
				entry->next->prev = entry->prev;
			else
				list->tail = entry->prev;

			ScebXCe_EC560647(entry);
			list->length--;
			return found;
		}
		entry = entry->next;
	}

	return 0;
}

/* findItem by name: the first node whose name is name */
BxceNodeId bxceNodeListFindItemByName(const BxceNodeList *list, const char *name)
{
	BxceNodeListEntry *entry;

	for (entry = list->head; entry != NULL; entry = entry->next) {
		BxceNodeId node = entry->node;
		const char *nodeName = ScebXCe_1189A2B1(list->doc, node);

		if (nodeName != NULL && bxceStrncmpTag(name, nodeName, BXCE_ATTR_STRING_MAX) == 0)
			return node;
	}

	return 0;
}

/* findItem: the node when the list holds it, else 0 */
BxceNodeId bxceNodeListFindItem(const BxceNodeList *list, BxceNodeId node)
{
	BxceNodeListEntry *entry;

	for (entry = list->head; entry != NULL; entry = entry->next) {
		if (entry->node == node)
			return entry->node;
	}

	return 0;
}

/* item: the index'th node, 0 past the end */
BxceNodeId ScebXCe_6C46D786(const BxceNodeList *list, SceUInt32 index)
{
	BxceNodeListEntry *entry;

	if (index >= (SceUInt32)list->length)
		return 0;

	for (entry = list->head; index != 0; index--)
		entry = entry->next;

	return entry->node;
}
