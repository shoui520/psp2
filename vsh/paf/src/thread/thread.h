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

#define ThreadConstruct _ZN3paf6thread6ThreadC2EijPKcPKNS1_6OptionE
#define ThreadConstructBase _ZN3paf6thread6ThreadC1EijPKcPKNS1_6OptionE
#define ThreadDestroy _ZN3paf6thread6ThreadD1Ev
#define ThreadDestroyBase _ZN3paf6thread6ThreadD2Ev
#define ThreadDelete _ZN3paf6thread6ThreadD0Ev
#define ThreadStart _ZN3paf6thread6Thread5StartEv
#define ThreadJoin _ZN3paf6thread6Thread4JoinEv
#define ThreadCancel _ZN3paf6thread6Thread6CancelEv
#define ThreadIsCanceled _ZN3paf6thread6Thread10IsCanceledEv
#define ThreadIsAlive _ZN3paf6thread6Thread7IsAliveEv
#define ThreadGetID _ZN3paf6thread6Thread5GetIDEv
#define ThreadChangePriority _ZN3paf6thread6Thread14ChangePriorityEi
#define ThreadInternalThreadEntry _ZN3paf6thread6Thread10ThreadFuncEjPv
#define ThreadOptConstruct _ZN3paf6thread6Thread6OptionC1Ev
#define ThreadOptConstructBase _ZN3paf6thread6Thread6OptionC2Ev

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
int _ZN3paf6thread5SleepEi(SceUInt32 milliseconds);
SceInt32 ThreadInternalThreadEntry(SceSize argSize, void *argBlock);
ThreadOpt *ThreadOptConstruct(ThreadOpt *self);
ThreadOpt *ThreadOptConstructBase(ThreadOpt *self);
SceUInt32 ThreadSetDefaultOpt(const ThreadOpt *opt);
ThreadOpt *ThreadDefaultOptInitialize(void);

#endif
