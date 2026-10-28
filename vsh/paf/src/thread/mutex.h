/* Layouts of paf::thread::Mutex and RMutex on Vita. */
#ifndef PAF_THREAD_MUTEX_H
#define PAF_THREAD_MUTEX_H

#include "../compiler.h"
#include <kernel/threadmgr.h>

typedef struct Mutex {
	SceKernelLwMutexWork m_mutex;
} Mutex;

typedef struct RMutex {
	SceKernelLwMutexWork m_mutex;
} RMutex;

/* Existing ScePafThread exports, kept under their current link names. */
RMutex *ScePafThread_C4BD9753(RMutex *mutex, const char *name, int option);
void ScePafThread_F9FFA0BE(RMutex *mutex); /* RMutex::Lock */
void ScePafThread_95F9C45F(RMutex *mutex); /* RMutex::Unlock */
void ScePafThread_8CCECAC9(RMutex *mutex); /* RMutex::~RMutex */
void ScePafThread_4EA3A3D5(Mutex *mutex);  /* Mutex::Lock */
void ScePafThread_9100D580(Mutex *mutex);  /* Mutex::Unlock */

#endif
