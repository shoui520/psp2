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

#define CallQueueConstruct _ZN3paf6thread9CallQueueC1EPKc
#define CallQueueDestroy _ZN3paf6thread9CallQueueD1Ev
#define CallQueueCheck _ZN3paf6thread9CallQueue5CheckEv
#define CallQueueEnqueue _ZN3paf6thread9CallQueue7EnqueueEPKcPFvPvES4_
#define CallQueueNumItems _ZN3paf6thread9CallQueue8NumItemsEv

CallQueue *CallQueueConstruct(CallQueue *self, const char *name);
CallQueue *CallQueueDestroy(CallQueue *self);
void CallQueueCheck(CallQueue *self);
void CallQueueEnqueue(CallQueue *self, const char *name,
		      CallQueueFunction entry, void *userdata);
int CallQueueNumItems(CallQueue *self);

#endif
