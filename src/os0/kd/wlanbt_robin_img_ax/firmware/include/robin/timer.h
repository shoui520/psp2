#ifndef ROBIN_TIMER_H
#define ROBIN_TIMER_H

#include <stddef.h>
#include "robin/sys.h"

struct OsaTimerEntry {
	OsaTimerCallback callback;
	uint32_t argument;
	uint32_t remainingTicks;
	uint8_t flags;
	uint32_t identifier;
	struct OsaTimerEntry *next;
};

struct OsaTimerState {
	uint32_t lastIdentifier;
	struct OsaTimerEntry *active;
	struct OsaTimerEntry *free;
	uint32_t programmedTicks;
	uint32_t paused;
	uint64_t pausedAt;
};

enum { OSA_TIMER_CAPACITY = 30 };

typedef char OsaTimerEntrySize[(sizeof(struct OsaTimerEntry) == 24) ? 1 : -1];
typedef char OsaTimerEntryFlags[(offsetof(struct OsaTimerEntry, flags) == 12) ? 1 : -1];
typedef char OsaTimerEntryIdentifier[(offsetof(struct OsaTimerEntry, identifier) == 16) ? 1 : -1];
typedef char OsaTimerEntryNext[(offsetof(struct OsaTimerEntry, next) == 20) ? 1 : -1];
typedef char OsaTimerStateSize[(sizeof(struct OsaTimerState) == 32) ? 1 : -1];
typedef char OsaTimerStatePausedAt[(offsetof(struct OsaTimerState, pausedAt) == 24) ? 1 : -1];

extern struct OsaTimerEntry osaTimerEntries[OSA_TIMER_CAPACITY];
extern struct OsaTimerState osaTimerState;
extern struct OsaTimerState *osaTimers;
extern uint32_t osaTimerDeferredInterrupts;
typedef int (*OsaCreateTimerOverride)(OsaTimerCallback callback,
	uint32_t argument, uint32_t delay, uint32_t *timer,
	uint32_t flags, int *result);
typedef int (*OsaCancelTimerOverride)(uint32_t timer, int *result);
extern OsaCreateTimerOverride osaCreateTimerOverride;
extern OsaCancelTimerOverride osaCancelTimerOverride;
extern int (*osaTimerInterruptOverride)(void);
extern int (*osaPauseTimersOverride)(void);
extern int (*osaResumeTimersOverride)(void);
typedef int (*OsaTimerDeadlineOverride)(uint64_t *deadline, uint32_t flagMask,
					int keepPaused, uint32_t *remaining);
typedef int (*OsaTimerExpireOverride)(uint32_t threshold, int *result);
extern OsaTimerDeadlineOverride osaTimerDeadlineOverride;
extern OsaTimerExpireOverride osaTimerExpireOverride;
int robinNoOverride(void);
int osaPauseTimers(void);
int osaResumeTimers(void);
uint32_t osaGetNextTimerDeadline(uint64_t *deadline, uint32_t flagMask,
				 int keepPaused);
int osaExpireTimerWithin(uint32_t threshold);

struct OsaTimerState *osaRecycleTimer(struct OsaTimerEntry **link);
int osaAdvanceTimers(struct OsaTimerEntry *entry, uint32_t elapsed);
int osaDispatchExpiredTimers(struct OsaTimerEntry **head);
struct OsaTimerEntry **osaInsertTimer(struct OsaTimerEntry **link,
				    struct OsaTimerEntry *entry);

#endif /* ROBIN_TIMER_H */
