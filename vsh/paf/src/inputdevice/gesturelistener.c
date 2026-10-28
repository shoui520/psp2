#include "gesturelistener.h"
#include "../std/string.h"
#include "../std/stdc++/atexit.h"

static PafMap gestureUserData;

static inline __attribute__((always_inline))
GestureUserDataNode *GestureUserDataFind(const PafMap *self, void *data)
{
	GestureUserDataNode *head = self->_Myhead;
	GestureUserDataNode *node = NULL;

	if (head) {
		node = head->_Parent;
		while (!node->_Isnil) {
			uintptr_t key = (uintptr_t)node->_Myval.first;

			if ((uintptr_t)data < key)
				node = node->_Left;
			else if (key < (uintptr_t)data)
				node = node->_Right;
			else
				break;
		}
	}
	return node;
}

void GestureListenerAddUserDataList(void *data)
{
	GestureUserDataNode *node = GestureUserDataFind(&gestureUserData, data);

	if (node == gestureUserData._Myhead) {
		GestureUserDataEntry value = { data, 0 };

		node = GestureUserDataMapInsert(&gestureUserData, &value, NULL);
		node->_Myval.second = 1;
	}
}

void GestureListenerIncrUserDataList(void *data)
{
	GestureUserDataNode *node = GestureUserDataFind(&gestureUserData, data);

	if (node == gestureUserData._Myhead)
		GestureListenerAddUserDataList(data);
	else
		++node->_Myval.second;
}

unsigned char GestureListenerIsValidUserData(void *data)
{
	return GestureUserDataFind(&gestureUserData, data) != gestureUserData._Myhead;
}

PafMap *GestureUserDataMapClear(PafMap *self)
{
	GestureUserDataNode *head = self->_Myhead;
	GestureUserDataNode *node;

	if (!head)
		return self;
	node = head->_Left;
	while (!node->_Isnil) {
		if (!node->_Left->_Isnil) {
			node = node->_Left;
		} else if (!node->_Right->_Isnil) {
			node = node->_Right;
		} else {
			GestureUserDataNode *parent = node->_Parent;

			if (!parent->_Isnil) {
				if (parent->_Left == node)
					parent->_Left = head;
				else
					parent->_Right = head;
			}
			PafDelete(node);
			head = self->_Myhead;
			node = parent;
		}
	}
	head->_Parent = head;
	head->_Left = head;
	head->_Right = head;
	self->_Mysize = 0;
	return NULL;
}

static inline __attribute__((always_inline))
void GestureUserDataMapLrotate(GestureUserDataNode *head,
			       GestureUserDataNode *node)
{
	GestureUserDataNode *right = node->_Right;

	node->_Right = right->_Left;
	if (!right->_Left->_Isnil)
		right->_Left->_Parent = node;
	right->_Parent = node->_Parent;
	if (node->_Parent->_Isnil)
		head->_Parent = right;
	else if (node == node->_Parent->_Left)
		node->_Parent->_Left = right;
	else
		node->_Parent->_Right = right;
	right->_Left = node;
	node->_Parent = right;
}

static inline __attribute__((always_inline))
void GestureUserDataMapRrotate(GestureUserDataNode *head,
			       GestureUserDataNode *node)
{
	GestureUserDataNode *left = node->_Left;

	node->_Left = left->_Right;
	if (!left->_Right->_Isnil)
		left->_Right->_Parent = node;
	left->_Parent = node->_Parent;
	if (node->_Parent->_Isnil)
		head->_Parent = left;
	else if (node == node->_Parent->_Right)
		node->_Parent->_Right = left;
	else
		node->_Parent->_Left = left;
	left->_Right = node;
	node->_Parent = left;
}

GestureUserDataNode *GestureUserDataMapRebalance(PafMap *self,
					       GestureUserDataNode *node)
{
	GestureUserDataNode *head = self->_Myhead;

	if (!head)
		return (GestureUserDataNode *)self;
	node->_Color = 1;
	while (node != head->_Parent && node->_Parent->_Color) {
		GestureUserDataNode *parent = node->_Parent;
		GestureUserDataNode *grandparent = parent->_Parent;
		GestureUserDataNode *uncle;

		if (parent == grandparent->_Left) {
			uncle = grandparent->_Right;
			if (uncle->_Color) {
				parent->_Color = 0;
				uncle->_Color = 0;
				node->_Parent->_Parent->_Color = 1;
				node = node->_Parent->_Parent;
			} else {
				if (node == parent->_Right) {
					node = node->_Parent;
					GestureUserDataMapLrotate(head, node);
				}
				node->_Parent->_Color = 0;
				node->_Parent->_Parent->_Color = 1;
				GestureUserDataMapRrotate(head, node->_Parent->_Parent);
			}
		} else {
			uncle = grandparent->_Left;
			if (uncle->_Color) {
				parent->_Color = 0;
				uncle->_Color = 0;
				node->_Parent->_Parent->_Color = 1;
				node = node->_Parent->_Parent;
			} else {
				if (node == parent->_Left) {
					node = node->_Parent;
					GestureUserDataMapRrotate(head, node);
				}
				node->_Parent->_Color = 0;
				node->_Parent->_Parent->_Color = 1;
				GestureUserDataMapLrotate(head, node->_Parent->_Parent);
			}
		}
	}
	head->_Parent->_Color = 0;
	return head->_Parent;
}

GestureUserDataNode *GestureUserDataMapInsert(PafMap *self,
					    const GestureUserDataEntry *value,
					    unsigned char *inserted)
{
	GestureUserDataNode *parent = self->_Myhead;
	GestureUserDataNode *node;
	GestureUserDataNode *entry;
	int side = 0;

	if (!parent)
		return NULL;
	node = parent->_Parent;
	if (inserted)
		*inserted = 0;
	while (!node->_Isnil) {
		parent = node;
		if ((uintptr_t)value->first < (uintptr_t)node->_Myval.first) {
			node = node->_Left;
			side = -1;
		} else if ((uintptr_t)node->_Myval.first < (uintptr_t)value->first) {
			node = node->_Right;
			side = 1;
		} else {
			return node;
		}
	}
	node = PafNew(sizeof(*node));
	if (!node) {
		ScePafStdc_D608246A();
		return self->_Myhead;
	}
	entry = PafPlacementNew(sizeof(*entry), node);
	if (entry) {
		GestureUserDataNode *head = self->_Myhead;

		entry->_Parent = parent;
		entry->_Left = head;
		entry->_Right = head;
		entry->_Myval.first = value->first;
		entry->_Myval.second = value->second;
		entry->_Color = 1;
		entry->_Isnil = 0;
	}
	if (side < 0) {
		parent->_Left = node;
		if (parent == ((GestureUserDataNode *)self->_Myhead)->_Left)
			((GestureUserDataNode *)self->_Myhead)->_Left = node;
	} else if (side > 0) {
		parent->_Right = node;
		if (parent == ((GestureUserDataNode *)self->_Myhead)->_Right)
			((GestureUserDataNode *)self->_Myhead)->_Right = node;
	} else {
		GestureUserDataNode *head = self->_Myhead;

		head->_Parent = node;
		head->_Left = node;
		head->_Right = node;
	}
	++self->_Mysize;
	GestureUserDataMapRebalance(self, node);
	if (inserted)
		*inserted = 1;
	return node;
}

PafMap *GestureUserDataMapConstruct(PafMap *self, const void *allocator)
{
	GestureUserDataNode *head;

	self->_Mysize = 0;
	head = PafNew(sizeof(*head));
	if (!head)
		ScePafStdc_D608246A();
	self->_Myhead = head;
	if (head) {
		head->_Parent = head;
		head->_Left = head;
		head->_Right = head;
		head->_Color = 0;
		((GestureUserDataNode *)self->_Myhead)->_Isnil = 1;
	}
	return self;
}

PafMap *GestureUserDataMapDestroy(PafMap *self)
{
	GestureUserDataMapClear(self);
	if (self->_Myhead)
		PafDelete(self->_Myhead);
	self->_Myhead = (void *)-1;
	self->_Mysize = (size_t)-1;
	return self;
}

void GestureUserDataInitialize(void)
{
	extern void *__dso_handle;
	unsigned char allocator[4];

	GestureUserDataMapConstruct(&gestureUserData, allocator);
	ScePafStdc_C95732BB(&gestureUserData,
			  (void (*)(void *))GestureUserDataMapDestroy, &__dso_handle);
}
