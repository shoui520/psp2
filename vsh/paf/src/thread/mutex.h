/* Layouts of paf::thread::Mutex and RMutex on Vita. */
#ifndef PAF_THREAD_MUTEX_H
#define PAF_THREAD_MUTEX_H

#include "../compiler.h"
#include <stdint.h>
#include <kernel/threadmgr.h>

typedef struct Mutex {
	SceKernelLwMutexWork m_mutex;
} Mutex;

typedef struct RMutex {
	SceKernelLwMutexWork m_mutex;
} RMutex;

typedef struct RWLock {
	SceUID m_evf;
	int32_t m_counter;
} RWLock;

typedef enum MutexOption {
	MutexOption_None = 0,
	MutexOption_FIFO = 1,
	MutexOption_Default = 0
} MutexOption;

#define MutexConstruct _ZN3paf6thread5MutexC1EPKci
#define MutexConstructBase _ZN3paf6thread5MutexC2EPKci
#define MutexDestroy _ZN3paf6thread5MutexD1Ev
#define MutexDestroyBase _ZN3paf6thread5MutexD2Ev
#define MutexLock _ZN3paf6thread5Mutex4LockEv
#define MutexUnlock _ZN3paf6thread5Mutex6UnlockEv
#define MutexTryLock _ZN3paf6thread5Mutex7TryLockEv

Mutex *MutexConstruct(Mutex *self, const char *name, int32_t option);
Mutex *MutexConstructBase(Mutex *self, const char *name, int32_t option);
Mutex *MutexDestroy(Mutex *self);
Mutex *MutexDestroyBase(Mutex *self);
void MutexLock(Mutex *self);
void MutexUnlock(Mutex *self);
unsigned char MutexTryLock(Mutex *self);

#define RWLockConstruct _ZN3paf6thread6RWLockC1EPKci
#define RWLockConstructBase _ZN3paf6thread6RWLockC2EPKci
#define RWLockDestroy _ZN3paf6thread6RWLockD1Ev
#define RWLockDestroyBase _ZN3paf6thread6RWLockD2Ev
#define RWLockLockShared _ZN3paf6thread6RWLock10ReaderLockEv
#define RWLockUnlockShared _ZN3paf6thread6RWLock12ReaderUnlockEv
#define RWLockLockExclusive _ZN3paf6thread6RWLock10WriterLockEv
#define RWLockUnlockExclusive _ZN3paf6thread6RWLock12WriterUnlockEv

RWLock *RWLockConstruct(RWLock *self, const char *name, int32_t option);
RWLock *RWLockConstructBase(RWLock *self, const char *name, int32_t option);
RWLock *RWLockDestroy(RWLock *self);
RWLock *RWLockDestroyBase(RWLock *self);
void RWLockLockShared(RWLock *self);
void RWLockUnlockShared(RWLock *self);
void RWLockLockExclusive(RWLock *self);
void RWLockUnlockExclusive(RWLock *self);

/* Existing ScePafThread exports, kept under their current link names. */
RMutex *_ZN3paf6thread6RMutexC1EPKci(RMutex *mutex, const char *name, int option);
void _ZN3paf6thread6RMutex4LockEv(RMutex *mutex); /* RMutex::Lock */
void _ZN3paf6thread6RMutex6UnlockEv(RMutex *mutex); /* RMutex::Unlock */
RMutex *_ZN3paf6thread6RMutexD1Ev(RMutex *mutex); /* RMutex::~RMutex */

#endif
