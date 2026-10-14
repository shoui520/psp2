/* OSA hardware clock family; the filename is inferred, not attested. */
#include "robin/sys.h"
#include "robin/soc.h"
#include "robin/timer.h"
#include "robin/rom.h"

static volatile uint32_t osaTimerOverflowCount __attribute__((section(".itcm.rw"))) = 0;
static volatile uint32_t osaTimerLastCounter __attribute__((section(".itcm.rw"))) = 0;
struct OsaTimerState *osaTimers __attribute__((section(".itcm.rw"))) = 0;
uint32_t osaTimerClockReload __attribute__((section(".itcm.rw"))) = 0;
OsaTimerArmedHook osaTimerArmedHook __attribute__((section(".itcm.rw"))) = 0;
uint32_t osaTimerDeferredInterrupts __attribute__((section(".itcm.rw"))) = 0;
/* All three original slots use the same argument-ignoring default. */
OsaCreateTimerOverride osaCreateTimerOverride
	__attribute__((section(".itcm.rw"))) =
	(OsaCreateTimerOverride)robinNoOverride;
OsaCancelTimerOverride osaCancelTimerOverride
	__attribute__((section(".itcm.rw"))) =
	(OsaCancelTimerOverride)robinNoOverride;
int (*osaTimerInterruptOverride)(void)
	__attribute__((section(".itcm.rw"))) = robinNoOverride;
int (*osaPauseTimersOverride)(void)
	__attribute__((section(".itcm.rw"))) = robinNoOverride;
int (*osaResumeTimersOverride)(void)
	__attribute__((section(".itcm.rw"))) = robinNoOverride;

struct OsaTimerEntry osaTimerEntries[OSA_TIMER_CAPACITY]
	__attribute__((section(".itcm.bss")));
struct OsaTimerState osaTimerState __attribute__((section(".itcm.bss")));

void osaInitializeHardwareClock(void)
{
	osaTimerLastCounter = UINT32_MAX;
	SOC_TIMER1_RELOAD = UINT32_MAX;
	SOC_TIMER_CONTROL |= 0xb0;
	SOC_TIMER_INT_ENABLE |= 2;
	SOC_IRQ_ENABLE_SET |= 1u << 5;
}

uint64_t osaGetHardwareTime(void)
{
	UINT posture;
	uint32_t counter;
	uint32_t previous;
	uint32_t overflows;

	posture = tx_interrupt_control(TX_INT_DISABLE);
	counter = SOC_TIMER1_COUNTER;
	previous = osaTimerLastCounter;
	overflows = osaTimerOverflowCount;
	if (counter > previous)
		++overflows;
	tx_interrupt_control(posture);
	return ((uint64_t)overflows << 32) | (uint32_t)~counter;
}

void osaTimer1Isr(void)
{
	++osaTimerOverflowCount;
	osaTimerLastCounter = UINT32_MAX;
}

void osaTimer0Isr(void)
{
	uint32_t counter = SOC_TIMER1_COUNTER;

	if (counter <= osaTimerLastCounter)
		osaTimerLastCounter = counter;
}

uint32_t osaGetTimerElapsedTicks(void)
{
	uint32_t counter = SOC_TIMER3_COUNTER;

	return osaTimerClockReload - counter;
}

void osaRestartTimerClock(void)
{
	SOC_TIMER_CONTROL &= ~(1u << 13);
	SOC_TIMER3_RELOAD = osaTimerClockReload;
	SOC_TIMER_CONTROL |= 0xb000;
}

void osaDisableTimerInterrupt(void)
{
	SOC_TIMER_CONTROL &= ~(1u << 9);
	SOC_IRQ_ENABLE_CLEAR = 1u << 6;
	SOC_TIMER_INT_STATUS = ~4u;
}

uint32_t osaProgramTimerInterrupt(uint32_t delay)
{
	osaRestartTimerClock();
	if (delay < 200)
		delay = 200;
	if (delay >= osaTimerClockReload)
		delay = osaTimerClockReload;
	SOC_TIMER_INT_STATUS = ~4u;
	SOC_IRQ_ENABLE_SET |= 1u << 6;
	SOC_TIMER2_RELOAD = delay;
	SOC_TIMER_CONTROL |= 0xa00;
	osaTimerArmedHook();
	return delay;
}

struct OsaTimerState *osaRecycleTimer(struct OsaTimerEntry **link)
{
	struct OsaTimerEntry *entry = *link;
	struct OsaTimerState *state;

	*link = entry->next;
	entry->identifier = 0;
	state = osaTimers;
	entry->next = state->free;
	state->free = entry;
	return state;
}

int osaAdvanceTimers(struct OsaTimerEntry *entry, uint32_t elapsed)
{
	while (entry) {
		if (entry->remainingTicks < elapsed)
			entry->remainingTicks = 0;
		else
			entry->remainingTicks -= elapsed;
		entry = entry->next;
	}
	return 0;
}

int osaDispatchExpiredTimers(struct OsaTimerEntry **head)
{
	int dispatched = 0;

	while (*head && (*head)->remainingTicks == 0) {
		struct OsaTimerEntry *entry = *head;
		uint32_t argument = entry->argument;
		uint32_t identifier = entry->identifier;
		OsaTimerCallback callback = entry->callback;

		osaRecycleTimer(head);
		if (callback)
			callback(identifier, argument);
		dispatched = 1;
	}
	return dispatched;
}

struct OsaTimerEntry **osaInsertTimer(struct OsaTimerEntry **link,
				    struct OsaTimerEntry *entry)
{
	struct OsaTimerEntry *current = *link;

	while (current && entry->remainingTicks > current->remainingTicks) {
		link = &current->next;
		current = current->next;
	}
	entry->next = current;
	*link = entry;
	return link;
}

void osaTimer2Isr(void)
{
	uint32_t elapsed;

	if (osaTimerInterruptOverride())
		return;
	if (osaTimers->paused) {
		++osaTimerDeferredInterrupts;
		return;
	}
	osaDisableTimerInterrupt();
	elapsed = osaGetTimerElapsedTicks();
	osaRestartTimerClock();
	if (osaTimers->programmedTicks > elapsed)
		elapsed += osaTimerClockReload;
	osaAdvanceTimers(osaTimers->active, elapsed);
	osaDispatchExpiredTimers(&osaTimers->active);
	if (osaTimers->active) {
		elapsed = osaGetTimerElapsedTicks();
		osaAdvanceTimers(osaTimers->active, elapsed);
		elapsed = osaProgramTimerInterrupt(osaTimers->active->remainingTicks);
		osaTimers->programmedTicks = elapsed;
	} else {
		osaTimers->programmedTicks = 0;
	}
}

int osaCreateTimer(OsaTimerCallback callback, uint32_t argument, uint32_t delay,
		   uint32_t *timer, uint32_t flags)
{
	struct OsaTimerEntry *entry = 0;
	UINT posture;
	uint32_t elapsed;
	int result;

	if (osaCreateTimerOverride(callback, argument, delay, timer, flags, &result))
		return result;
	osaDisableTimerInterrupt();
	posture = tx_interrupt_control(TX_INT_DISABLE);
	if (osaTimers->free) {
		entry = osaTimers->free;
		osaTimers->free = entry->next;
		if (++osaTimers->lastIdentifier == 0)
			osaTimers->lastIdentifier = 1;
		entry->callback = callback;
		entry->argument = argument;
		entry->identifier = osaTimers->lastIdentifier;
		entry->next = 0;
		entry->remainingTicks = delay;
		entry->flags = flags;
		elapsed = osaGetTimerElapsedTicks();
		osaAdvanceTimers(osaTimers->active, elapsed);
		osaInsertTimer(&osaTimers->active, entry);
	}
	if (osaTimers->active && !osaTimers->paused) {
		elapsed = osaProgramTimerInterrupt(osaTimers->active->remainingTicks);
		osaTimers->programmedTicks = elapsed;
	}
	if (entry && entry->identifier) {
		if (timer)
			*timer = entry->identifier;
		result = 1;
	} else {
		if (timer)
			*timer = 0;
		result = 0;
	}
	tx_interrupt_control(posture);
	return result;
}

int osaCancelTimer(uint32_t timer)
{
	struct OsaTimerEntry **link = &osaTimers->active;
	struct OsaTimerEntry *entry;
	UINT posture;
	int result = 0;
	int found = 0;

	if (osaCancelTimerOverride(timer, &result) || !timer)
		return result;
	posture = tx_interrupt_control(TX_INT_DISABLE);
	while ((entry = *link) && !found) {
		if (entry->identifier == timer) {
			found = 1;
			result = 1;
		} else {
			link = &entry->next;
		}
	}
	if (result)
		osaRecycleTimer(link);
	if (!osaTimers->active)
		osaDisableTimerInterrupt();
	tx_interrupt_control(posture);
	return result;
}

int osaPauseTimers(void)
{
	int result = osaPauseTimersOverride();
	UINT posture;
	uint64_t timestamp;
	uint32_t elapsed;

	if (result)
		return result;
	posture = tx_interrupt_control(TX_INT_DISABLE);
	osaTimers->paused = 1;
	if (osaTimers->active) {
		osaDisableTimerInterrupt();
		timestamp = romGetTimerTimestamp();
		osaTimers->pausedAt = timestamp;
		elapsed = osaGetTimerElapsedTicks();
		osaAdvanceTimers(osaTimers->active, elapsed);
		osaRestartTimerClock();
	}
	return tx_interrupt_control(posture);
}

int osaResumeTimers(void)
{
	int result = osaResumeTimersOverride();
	UINT posture;
	uint64_t timestamp;
	uint32_t elapsed;
	uint32_t programmed;

	if (result)
		return result;
	posture = tx_interrupt_control(TX_INT_DISABLE);
	if (osaTimers->paused) {
		osaTimers->paused = 0;
		if (osaTimers->active) {
			timestamp = romGetTimerTimestamp();
			elapsed = timestamp - osaTimers->pausedAt;
			osaAdvanceTimers(osaTimers->active, elapsed);
			programmed =
				osaProgramTimerInterrupt(osaTimers->active->remainingTicks);
			osaTimers->programmedTicks = programmed;
		}
	}
	return tx_interrupt_control(posture);
}

