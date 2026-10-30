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

#define MutexConstruct ScePafThread_A5D3F1B5
#define MutexConstructBase ScePafThread_BC109843
#define MutexDestroy ScePafThread_AD04A33D
#define MutexDestroyBase ScePafThread_9BA75E36
#define MutexLock ScePafThread_4EA3A3D5
#define MutexUnlock ScePafThread_9100D580
#define MutexTryLock ScePafThread_E09598ED

Mutex *MutexConstruct(Mutex *self, const char *name, int32_t option);
Mutex *MutexConstructBase(Mutex *self, const char *name, int32_t option);
Mutex *MutexDestroy(Mutex *self);
Mutex *MutexDestroyBase(Mutex *self);
void MutexLock(Mutex *self);
void MutexUnlock(Mutex *self);
unsigned char MutexTryLock(Mutex *self);

#define RWLockConstruct ScePafThread_02C1347F
#define RWLockConstructBase ScePafThread_1ECE442B
#define RWLockDestroy ScePafThread_221624F8
#define RWLockDestroyBase ScePafThread_6FBDD800
#define RWLockLockShared ScePafThread_AFB836B3
#define RWLockUnlockShared ScePafThread_38D35DEF
#define RWLockLockExclusive ScePafThread_39B3F3CA
#define RWLockUnlockExclusive ScePafThread_AEE8496E

RWLock *RWLockConstruct(RWLock *self, const char *name, int32_t option);
RWLock *RWLockConstructBase(RWLock *self, const char *name, int32_t option);
RWLock *RWLockDestroy(RWLock *self);
RWLock *RWLockDestroyBase(RWLock *self);
void RWLockLockShared(RWLock *self);
void RWLockUnlockShared(RWLock *self);
void RWLockLockExclusive(RWLock *self);
void RWLockUnlockExclusive(RWLock *self);

/* Existing ScePafThread exports, kept under their current link names. */
RMutex *ScePafThread_C4BD9753(RMutex *mutex, const char *name, int option);
void ScePafThread_F9FFA0BE(RMutex *mutex); /* RMutex::Lock */
void ScePafThread_95F9C45F(RMutex *mutex); /* RMutex::Unlock */
RMutex *ScePafThread_8CCECAC9(RMutex *mutex); /* RMutex::~RMutex */

#endif
