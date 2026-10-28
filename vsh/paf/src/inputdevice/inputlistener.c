#include "inputlistener.h"
#include "../std/string.h"
#include "../std/stdc++/atexit.h"
#include <sce_atomic.h>

static InputListenerList inputListeners;

InputListenerList *InputListenerListConstruct(InputListenerList *self,
					     const void *allocator)
{
	InputListenerNode *root;

	self->count = 0;
	root = PafNew(sizeof(*root));
	if (!root)
		ScePafStdc_D608246A();
	self->root_node = root;
	if (root) {
		root->prev = root;
		root->next = root;
	}
	return self;
}

InputListenerList *InputListenerListDestroy(InputListenerList *self)
{
	InputListenerNode *root = self->root_node;

	if (root) {
		InputListenerNode *node = root->next;

		while (node != root) {
			InputListenerNode *next = node->next;
			InputListenerSharedPtr *item = &node->val;

			if (item->m_ptr && sceAtomicDecrement32AcqRel(
				    &item->m_ref_counter->m_counter) == 1) {
				item->m_ref_counter->m_deleter(item->m_ptr);
				if (sceAtomicDecrement32AcqRel(
					    &item->m_ref_counter->m_weak_counter) == 1)
					PafDelete(item->m_ref_counter);
			}
			PafDelete(node);
			root = self->root_node;
			node = next;
		}
		PafDelete(root);
	}
	self->root_node = (InputListenerNode *)-1;
	self->count = -1;
	return self;
}

void InputListenerListInitialize(void)
{
	extern void *__dso_handle;
	unsigned char allocator[4];

	InputListenerListConstruct(&inputListeners, allocator);
	ScePafStdc_C95732BB(&inputListeners,
			  (void (*)(void *))InputListenerListDestroy, &__dso_handle);
}
