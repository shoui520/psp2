/* Complete SceNetPs thread and runtime owners, shared by both variants. */
#ifndef SCE_NETPS_THREAD_H
#define SCE_NETPS_THREAD_H

#include <scetypes.h>
#include <stddef.h>

typedef struct SceNetPsMutex {
	SceUID eventFlag;
	unsigned int available;
	unsigned int waiters;
} SceNetPsMutex;

typedef struct SceNetPsSync {
	struct SceNetPsSync *next;
	struct SceNetPsSync *prev;
	SceUID processId;
	SceUID eventFlag;
	unsigned int events;
	unsigned int waiters;
} SceNetPsSync;

typedef struct SceNetPsSyncList {
	SceNetPsSync *head;
	SceNetPsSync *tail;
} SceNetPsSyncList;

_Static_assert(sizeof(SceNetPsSyncList) == 8, "SceNetPsSyncList size");

typedef struct SceNetPsThread {
	struct SceNetPsThread *next;
	struct SceNetPsThread *prev;
	SceUID processId;
	SceUID threadId;
	SceNetPsSync sync;
	char name[32];
	const void *waitChannel;
	unsigned int flags;
	int mode;
	int error;
} SceNetPsThread;

typedef struct SceNetPsPendingWakeup {
	struct SceNetPsPendingWakeup *next;
	struct SceNetPsPendingWakeup *prev;
	const void *waitChannel;
	unsigned int events;
} SceNetPsPendingWakeup;

typedef struct SceNetPsThreadInfo {
	SceUID processId;
	SceUID threadId;
} SceNetPsThreadInfo;

typedef struct SceNetPsSleepCount {
	unsigned int identifier;
	unsigned int reserved;
	unsigned int count;
} SceNetPsSleepCount;

/* NetBSD's (sys/time.h) */
struct timeval;

typedef struct SceNetPsThreadParam {
	void (*entry)(void *arg);
	void *arg;
	int initPriority;
	SceSize stackSize;
	unsigned int cpuAffinityMask;
	const char *name;
} SceNetPsThreadParam;

typedef struct SceNetPsRuntime {
	SceNetPsSync sync;
	SceNetPsThread *threadHead;
	SceNetPsThread *threadTail;
	SceNetPsPendingWakeup *pendingHead;
	SceNetPsPendingWakeup *pendingTail;
	SceUID softThread;
	unsigned int flags;
	SceNetPsSleepCount sleepCounts[4];
} SceNetPsRuntime;

_Static_assert(sizeof(SceNetPsMutex) == 12, "SceNetPsMutex size");
_Static_assert(sizeof(SceNetPsSync) == 24, "SceNetPsSync size");
_Static_assert(sizeof(SceNetPsThread) == 88, "SceNetPsThread size");
_Static_assert(sizeof(SceNetPsRuntime) == 96, "SceNetPsRuntime size");
_Static_assert(sizeof(SceNetPsThreadParam) == 24, "SceNetPsThreadParam size");

extern int gNetPsSoftTick;
extern SceNetPsMutex gNetPsJumboMutex;
extern SceNetPsMutex gNetPsDevSendMutex;
extern SceNetPsMutex gNetPsDevRecvMutex;
extern SceNetPsMutex gNetPsEmuMutex;
extern SceNetPsMutex gNetPsPktMutex;
extern SceNetPsMutex gNetPsMemMutex;
extern SceNetPsMutex gNetPsThreadMutex;
extern SceUID gNetPsInitialProcessId;
extern SceNetPsRuntime gNetPsRuntime;
extern SceNetPsThreadParam gNetPsSoftThreadParam;

int sceNetPsWakeup(const void *waitChannel, unsigned int events);
int sceNetPsSoftThread(void);
SceNetPsThread *sceNetPsGetThread(SceUID threadId, int create);
int sceNetPsRetireThread(SceUID threadId);
SceUID sceNetPsGetSoftThread(void);
int sceNetPsIsSoftThread(void);
int sceNetPsGetThreadError(SceUID threadId, int *error);
int sceNetPsSetThreadError(SceUID threadId, int error);
int sceNetPsGetThreadMode(SceUID threadId, int *mode);
int sceNetPsSetThreadMode(SceUID threadId, int mode);
int sceNetPsGetThreadList(SceNetPsThreadInfo *info, int capacity);
int sceNetPsGetSleepCount(SceNetPsSleepCount *count, int index);
int sceNetPsRecordSleep(unsigned int identifier, int clear);
int sceNetPsIsApplicationProgram(int *error);
int sceNetPsSleep(const void *waitChannel, unsigned int flags,
		 const char *message, int delay);
int sceNetPsConsumeWakeup(const void *waitChannel, unsigned int events);
int sceNetPsQueueWakeup(const void *waitChannel, unsigned int events);
int sceNetPsWakeupNormal(const void *waitChannel);
int sceNetPsWakeupCanceled(const void *waitChannel);
int sceNetPsWakeupInterrupted(const void *waitChannel);
int sceNetPsWakeupAddressChanged(const void *waitChannel);
struct timeval *sceNetPsGetTime(struct timeval *value);
int ppsratecheck(struct timeval *lasttime, int *curpps, int maxpps);
int sceNetPsScheduleProtocols(void);
int sceNetPsScheduleReceive(void);
int sceNetPsNormalizeTimeout(int timeout);
int sceNetPsInitializeSync(void);
int sceNetPsStartStack(const SceNetPsThreadParam *param);

#endif

