#include "inputlistener.h"
#include "../std/string.h"
#include "../std/stdc++/atexit.h"
#include <sce_atomic.h>

static InputListenerList inputListeners;
/* The 56-byte input event is initialized by ScePafMisc_B51652DF. */
static uint8_t inputEvent[0x38];

int ScePafMisc_996A8C03(void)
{
	InputListenerNode *root = inputListeners.root_node;
	InputListenerNode *next = root ? root->next : NULL;
	InputListenerNode *node = next;

	while (node != inputListeners.root_node) {
		InputListener *item = node->val.m_ptr;
		void (**methods)(void) = *(void (***)(void))item;

		((void (*)(InputListener *, const void *))methods[3])(item, inputEvent);
		node = next;
		if (next) {
			next = next->next;
			node = next;
		}
	}
	return 0;
}

int ScePafMisc_26F81894(uint8_t state)
{
	InputListenerNode *root = inputListeners.root_node;
	InputListenerNode *next = root ? root->next : NULL;
	InputListenerNode *node = next;

	while (node != inputListeners.root_node) {
		InputListener *item = node->val.m_ptr;
		void (**methods)(void) = *(void (***)(void))item;

		((void (*)(InputListener *, const void *, int))methods[4])(
			item, inputEvent, state);
		node = next;
		if (next) {
			next = next->next;
			node = next;
		}
	}
	return 0;
}

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
