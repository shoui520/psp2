#ifndef PAF_INPUTDEVICE_INPUTDEVICE_H
#define PAF_INPUTDEVICE_INPUTDEVICE_H

#include "../timer/timer.h"
#include <stdint.h>

typedef struct InputTimerNode {
	struct InputTimerNode *prev;
	struct InputTimerNode *next;
	Timer val;
} InputTimerNode;

typedef struct InputTimerList {
	InputTimerNode *root_node;
	int32_t count;
	unsigned char alloc_value;
	unsigned char alloc_node;
} InputTimerList;

typedef char InputTimerNodeSizeCheck[sizeof(InputTimerNode) == 36 ? 1 : -1];
typedef char InputTimerListSizeCheck[sizeof(InputTimerList) == 12 ? 1 : -1];

InputTimerList *InputTimerListConstruct(InputTimerList *self,
				       const void *allocator);
InputTimerList *InputTimerListDestroy(InputTimerList *self);

#endif
