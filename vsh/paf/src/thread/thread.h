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

typedef struct ThreadClass {
	int offset_to_top;
	const void *type_info;
	ThreadVTable methods;
} ThreadClass;

extern const ThreadClass threadClass;

typedef struct ThreadOpt {
	SceUInt32 attr;
	SceInt32 cpuAffinityMask;
	SceUInt32 stackMemoryType;
} ThreadOpt;

enum {
	ThreadPriority_Normal = 0x10000100,
	ThreadAffinity_AllCores = 0x000f0000,
	SCE_PAF_ERROR_THREAD_THREAD_ALREADY_STARTED = -2136013054 /* 0x80AF0702 */
};

struct Thread {
	const ThreadVTable *vtable;
	SceUID m_thid;
	unsigned char m_started;
	unsigned char m_alive;
	unsigned char m_canceled;
};

typedef char ThreadSizeCheck[sizeof(Thread) == 12 ? 1 : -1];
typedef char ThreadCancelOffsetCheck[offsetof(Thread, m_canceled) == 10 ? 1 : -1];
typedef char ThreadClassSizeCheck[sizeof(ThreadClass) == 20 ? 1 : -1];
typedef char ThreadOptSizeCheck[sizeof(ThreadOpt) == 12 ? 1 : -1];

#define ThreadConstruct ScePafThread_6E829BD8
#define ThreadConstructBase ScePafThread_1740FB30
#define ThreadDestroy ScePafThread_52C62CD1
#define ThreadDestroyBase ScePafThread_7EA6B501
#define ThreadDelete ScePafThread_1013F3C0
#define ThreadStart ScePafThread_60910895
#define ThreadJoin ScePafThread_C5F24067
#define ThreadCancel ScePafThread_F95B2895
#define ThreadIsCanceled ScePafThread_4CE6D88B
#define ThreadIsAlive ScePafThread_318AD6B7
#define ThreadGetID ScePafThread_7E669E6E
#define ThreadChangePriority ScePafThread_FCCF62D6
#define ThreadInternalThreadEntry ScePafThread_918D89FC
#define ThreadOptConstruct ScePafThread_5712350E
#define ThreadOptConstructBase ScePafThread_CC65DF88

Thread *ThreadConstruct(Thread *self, SceInt32 priority, SceSize stackSize,
			const char *name, const ThreadOpt *opt);
Thread *ThreadConstructBase(Thread *self, SceInt32 priority, SceSize stackSize,
			    const char *name, const ThreadOpt *opt);
Thread *ThreadDestroy(Thread *self);
Thread *ThreadDestroyBase(Thread *self);
void ThreadDelete(Thread *self);
SceInt32 ThreadStart(Thread *self);
void ThreadJoin(Thread *self);
void ThreadCancel(Thread *self);
unsigned char ThreadIsCanceled(Thread *self);
unsigned char ThreadIsAlive(Thread *self);
SceUID ThreadGetID(Thread *self);
void ThreadChangePriority(Thread *self, SceInt32 priority);
SceInt32 ThreadInternalThreadEntry(SceSize argSize, void *argBlock);
ThreadOpt *ThreadOptConstruct(ThreadOpt *self);
ThreadOpt *ThreadOptConstructBase(ThreadOpt *self);
SceUInt32 ThreadSetDefaultOpt(const ThreadOpt *opt);
ThreadOpt *ThreadDefaultOptInitialize(void);

#endif
