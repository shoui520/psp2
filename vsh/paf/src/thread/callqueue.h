#ifndef PAF_THREAD_CALLQUEUE_H
#define PAF_THREAD_CALLQUEUE_H

#include "mutex.h"
#include "../std/string.h"

typedef void (*CallQueueFunction)(void *userdata);

typedef struct CallQueueRequest {
	PafString name;
	CallQueueFunction func;
	void *arg;
} CallQueueRequest;

typedef struct CallQueueRequestNode {
	struct CallQueueRequestNode *prev;
	struct CallQueueRequestNode *next;
	CallQueueRequest val;
} CallQueueRequestNode;

typedef struct CallQueueRequestList {
	CallQueueRequestNode *root_node;
	unsigned int count;
	unsigned char alloc_value;
	unsigned char alloc_node;
} CallQueueRequestList;

typedef struct CallQueue {
	Mutex m_mutex;
	CallQueueRequestList m_request_queue;
} CallQueue;

typedef char CallQueueSizeCheck[sizeof(CallQueue) == 48 ? 1 : -1];

#define CallQueueConstruct ScePafThread_5A72B9FC
#define CallQueueDestroy ScePafThread_BE9A9155
#define CallQueueCheck ScePafThread_98EA7399
#define CallQueueEnqueue ScePafThread_23B8F66F
#define CallQueueNumItems ScePafThread_9F2131F9

CallQueue *CallQueueConstruct(CallQueue *self, const char *name);
CallQueue *CallQueueDestroy(CallQueue *self);
void CallQueueCheck(CallQueue *self);
void CallQueueEnqueue(CallQueue *self, const char *name,
		      CallQueueFunction entry, void *userdata);
int CallQueueNumItems(CallQueue *self);

#endif
