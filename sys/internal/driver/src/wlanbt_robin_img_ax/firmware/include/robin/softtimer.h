/*
 * The software timers of the MAC code: a list of timers counted down by one
 * system timer slot (wlan/softtimer.c). A timer counts in ticks, once or
 * periodically, and the ROM's expiry hook runs its callback.
 */
#ifndef ROBIN_SOFTTIMER_H
#define ROBIN_SOFTTIMER_H

#include <stdint.h>

struct WlanSoftTimer {
	struct WlanSoftTimer *next;	/* the list of running timers */
	uint32_t running;		/* set while the timer is on the list */
	uint32_t unstarted;		/* 1 from initialisation until the first start */
	uint16_t ticks;			/* ticks left before the timer expires */
	uint16_t period;		/* ticks of a periodic timer, 0 for a one-shot */
	uint32_t argument;		/* passed to the callback */
	void (*callback)(uint32_t argument);
};

_Static_assert(sizeof(struct WlanSoftTimer) == 24, "software timer");

void wlanSoftTimerInit(struct WlanSoftTimer *timer);					/* 0x2c0c0 */
void wlanSoftTimerStart(struct WlanSoftTimer *timer, uint32_t running,
			void (*callback)(uint32_t), uint32_t argument,
			uint16_t ticks);						/* 0x2c0e2 */
void wlanSoftTimerStartOnce(struct WlanSoftTimer *timer, uint32_t running,
			    void (*callback)(uint32_t), uint32_t argument,
			    uint16_t ticks);						/* 0x2c12a */
void wlanSoftTimerStartPeriodic(struct WlanSoftTimer *timer, uint32_t running,
				void (*callback)(uint32_t), uint32_t argument,
				uint16_t ticks);					/* 0x2c154 */
int wlanSoftTimerStop(struct WlanSoftTimer *timer);					/* 0x2c17c */
int wlanSoftTimerTick(uint32_t index, uint32_t argument);				/* 0x2c1ba */

#endif /* ROBIN_SOFTTIMER_H */
