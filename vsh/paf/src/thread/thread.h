#ifndef PAF_THREAD_THREAD_H
#define PAF_THREAD_THREAD_H

#include <stddef.h>
#include <scetypes.h>

typedef struct Thread Thread;

typedef struct ThreadVTable {
	Thread *(*Destroy)(Thread *self);
	void (*Delete)(Thread *self);
	void (*EntryFunction)(Thread *self);
} ThreadVTable;

struct Thread {
	const ThreadVTable *vtable;
	SceUID m_thid;
	unsigned char m_started;
	unsigned char m_alive;
	unsigned char m_canceled;
};

typedef char ThreadSizeCheck[sizeof(Thread) == 12 ? 1 : -1];
typedef char ThreadCancelOffsetCheck[offsetof(Thread, m_canceled) == 10 ? 1 : -1];

#define ThreadJoin ScePafThread_C5F24067
#define ThreadCancel ScePafThread_F95B2895
#define ThreadIsAlive ScePafThread_318AD6B7

void ThreadJoin(Thread *self);
void ThreadCancel(Thread *self);
unsigned char ThreadIsAlive(Thread *self);

#endif
