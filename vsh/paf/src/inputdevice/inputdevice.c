#include "inputdevice.h"
#include "../std/string.h"
#include "../std/stdc++/atexit.h"

static InputTimerList inputTimers;

InputTimerList *InputTimerListConstruct(InputTimerList *self,
				       const void *allocator)
{
	InputTimerNode *root;

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

InputTimerList *InputTimerListDestroy(InputTimerList *self)
{
	InputTimerNode *root = self->root_node;

	if (root) {
		InputTimerNode *node = root->next;

		while (node != root) {
			InputTimerNode *next = node->next;

			node->val.vtable->Destroy(&node->val);
			PafDelete(node);
			root = self->root_node;
			node = next;
		}
		PafDelete(root);
	}
	self->root_node = (InputTimerNode *)-1;
	self->count = -1;
	return self;
}

void InputDeviceTimerListInitialize(void)
{
	extern void *__dso_handle;
	unsigned char allocator[4];

	InputTimerListConstruct(&inputTimers, allocator);
	ScePafStdc_C95732BB(&inputTimers,
			  (void (*)(void *))InputTimerListDestroy, &__dso_handle);
}
