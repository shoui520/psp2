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

void GestureListenerDelUserDataList(void *data)
{
	if (gestureUserData._Mysize) {
		GestureUserDataNode *node = GestureUserDataFind(&gestureUserData, data);

		if (node != gestureUserData._Myhead)
			GestureUserDataMapErase(&gestureUserData, &node);
	}
}

void GestureListenerDecrUserDataList(void *data)
{
	if (gestureUserData._Mysize) {
		GestureUserDataNode *node = GestureUserDataFind(&gestureUserData, data);

		if (node != gestureUserData._Myhead) {
			--node->_Myval.second;
			if (!node->_Myval.second)
				GestureListenerDelUserDataList(data);
		}
	}
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

uintptr_t GestureUserDataMapEraseRebalance(PafMap *self,
					 GestureUserDataNode *node,
					 GestureUserDataNode *parent)
{
	GestureUserDataNode *head = self->_Myhead;
	uintptr_t result = (uintptr_t)self;

	if (!head)
		return result;
	for (;;) {
		GestureUserDataNode *sibling;

		result = (int32_t)(signed char)parent->_Isnil;
		if (result)
			break;
		result = (int32_t)(signed char)node->_Color;
		if (result)
			break;
		if (node == parent->_Left) {
			sibling = parent->_Right;
			if (sibling->_Color) {
				sibling->_Color = 0;
				parent->_Color = 1;
				sibling = sibling->_Left;
				GestureUserDataMapLrotate(head, parent);
			}
			if (sibling->_Left->_Color || sibling->_Right->_Color) {
				if (!sibling->_Right->_Color) {
					GestureUserDataNode *left = sibling->_Left;

					left->_Color = 0;
					sibling->_Color = 1;
					GestureUserDataMapRrotate(head, sibling);
					sibling = left;
				}
				sibling->_Color = parent->_Color;
				parent->_Color = 0;
				sibling->_Right->_Color = 0;
				GestureUserDataMapLrotate(head, parent);
				node = head->_Parent;
				parent = node->_Parent;
			} else {
				node = parent;
				sibling->_Color = 1;
				parent = parent->_Parent;
			}
		} else {
			sibling = parent->_Left;
			if (sibling->_Color) {
				sibling->_Color = 0;
				parent->_Color = 1;
				sibling = sibling->_Right;
				GestureUserDataMapRrotate(head, parent);
			}
			if (sibling->_Left->_Color || sibling->_Right->_Color) {
				if (!sibling->_Left->_Color) {
					GestureUserDataNode *right = sibling->_Right;

					right->_Color = 0;
					sibling->_Color = 1;
					GestureUserDataMapLrotate(head, sibling);
					sibling = right;
				}
				sibling->_Color = parent->_Color;
				parent->_Color = 0;
				sibling->_Left->_Color = 0;
				GestureUserDataMapRrotate(head, parent);
				node = head->_Parent;
				parent = node->_Parent;
			} else {
				node = parent;
				sibling->_Color = 1;
				parent = parent->_Parent;
			}
		}
	}
	node->_Color = 0;
	return result;
}

uintptr_t GestureUserDataMapEraseNode(PafMap *self, GestureUserDataNode *node)
{
	GestureUserDataNode *head = self->_Myhead;
	GestureUserDataNode *successor;
	GestureUserDataNode *parent;
	GestureUserDataNode *fixparent;
	GestureUserDataNode *fixnode;
	unsigned char black;

	if (!head)
		return (uintptr_t)self;
	if (node->_Left->_Isnil || node->_Right->_Isnil) {
		successor = node;
		parent = node->_Parent;
		fixparent = node->_Parent;
	} else {
		successor = node->_Right;
		if (successor->_Left->_Isnil) {
			fixparent = successor;
		} else {
			while (!successor->_Left->_Isnil)
				successor = successor->_Left;
			fixparent = successor->_Parent;
		}
		parent = successor->_Parent;
	}
	if (successor->_Left->_Isnil)
		fixnode = successor->_Right;
	else
		fixnode = successor->_Left;
	if (!fixnode->_Isnil) {
		fixnode->_Parent = parent;
		parent = successor->_Parent;
	}
	if (parent->_Isnil)
		head->_Parent = fixnode;
	else if (parent->_Left == successor)
		parent->_Left = fixnode;
	else
		parent->_Right = fixnode;
	black = !successor->_Color;
	if (successor != node) {
		successor->_Parent = node->_Parent;
		if (node->_Parent->_Isnil)
			head->_Parent = successor;
		else if (node->_Parent->_Left == node)
			node->_Parent->_Left = successor;
		else
			node->_Parent->_Right = successor;
		successor->_Left = node->_Left;
		if (!node->_Left->_Isnil)
			node->_Left->_Parent = successor;
		successor->_Right = node->_Right;
		if (!node->_Right->_Isnil)
			node->_Right->_Parent = successor;
		successor->_Color = node->_Color;
	}
	if (black) {
		GestureUserDataMapEraseRebalance(self, fixnode, fixparent);
		head = self->_Myhead;
	}
	if (head->_Left == node) {
		GestureUserDataNode *first = fixparent;

		if (!fixnode->_Isnil) {
			first = fixnode;
			while (!first->_Left->_Isnil)
				first = first->_Left;
		}
		head->_Left = first;
	}
	if (head->_Right == node) {
		if (!fixnode->_Isnil) {
			fixparent = fixnode;
			while (!fixparent->_Right->_Isnil)
				fixparent = fixparent->_Right;
		}
		head->_Right = fixparent;
	}
	PafDelete(node);
	return --self->_Mysize;
}

uintptr_t GestureUserDataMapErase(PafMap *self,
				GestureUserDataNode *const *where)
{
	GestureUserDataNode *node = *where;
	GestureUserDataNode *head;
	GestureUserDataNode *found;

	if (!node)
		return (uintptr_t)self;
	head = self->_Myhead;
	if (!head)
		return (uintptr_t)self;
	found = head->_Parent;
	if (!found->_Isnil) {
		uintptr_t key = (uintptr_t)node->_Myval.first;

		while (!found->_Isnil) {
			uintptr_t entry = (uintptr_t)found->_Myval.first;

			if (key < entry)
				found = found->_Left;
			else if (entry < key)
				found = found->_Right;
			else
				break;
		}
	}
	if (found && !found->_Isnil)
		return GestureUserDataMapEraseNode(self, found);
	return (uintptr_t)self;
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
