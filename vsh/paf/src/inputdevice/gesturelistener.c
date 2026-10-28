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
