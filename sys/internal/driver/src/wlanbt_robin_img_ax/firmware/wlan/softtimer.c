/*
 * The software timers of the MAC code (include/robin/softtimer.h): a singly
 * linked list of running timers, changed with interrupts disabled and
 * counted down by wlanSoftTimerTick(), which the system timer slot of
 * wlaninit.c runs. The ROM's expiry hook (0x54e98) runs the callback of a
 * timer that reaches zero.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/softtimer.h"
#include "robin/threadx.h"

struct WlanSoftTimer *wlanSoftTimerList __attribute__((section(".itcm.bss")));				/* 0x54e94 */
void (*wlanSoftTimerExpireHook)(struct WlanSoftTimer *timer) __attribute__((section(".itcm.bss")));	/* 0x54e98 */

void wlanSoftTimerInit(struct WlanSoftTimer *timer)
{
	UINT posture;

	posture = tx_interrupt_control(TX_INT_DISABLE);
	timer->running = 0;
	timer->unstarted = 1;
	timer->callback = NULL;
	timer->ticks = 1;
	timer->argument = 0;
	timer->period = 0;
	timer->next = NULL;
	tx_interrupt_control(posture);
}

void wlanSoftTimerStart(struct WlanSoftTimer *timer, uint32_t running,
			void (*callback)(uint32_t), uint32_t argument,
			uint16_t ticks)
{
	struct WlanSoftTimer *entry;
	UINT posture;

	posture = tx_interrupt_control(TX_INT_DISABLE);
	if (timer->running == 0) {
		timer->running = running;
		timer->ticks = ticks;
		timer->unstarted = 0;
		timer->callback = callback;
		timer->argument = argument;
		for (entry = wlanSoftTimerList; entry != NULL; entry = entry->next) {
			if (entry == timer) {
				tx_interrupt_control(posture);
				return;
			}
		}
		timer->next = wlanSoftTimerList;
		wlanSoftTimerList = timer;
	}
	tx_interrupt_control(posture);
}

void wlanSoftTimerStartOnce(struct WlanSoftTimer *timer, uint32_t running,
			    void (*callback)(uint32_t), uint32_t argument,
			    uint16_t ticks)
{
	UINT posture;

	posture = tx_interrupt_control(TX_INT_DISABLE);
	timer->period = 0;
	tx_interrupt_control(posture);
	wlanSoftTimerStart(timer, running, callback, argument, ticks);
}

void wlanSoftTimerStartPeriodic(struct WlanSoftTimer *timer, uint32_t running,
				void (*callback)(uint32_t), uint32_t argument,
				uint16_t ticks)
{
	UINT posture;

	posture = tx_interrupt_control(TX_INT_DISABLE);
	timer->period = ticks;
	tx_interrupt_control(posture);
	wlanSoftTimerStart(timer, running, callback, argument, ticks);
}

int wlanSoftTimerStop(struct WlanSoftTimer *timer)
{
	struct WlanSoftTimer *entry;
	struct WlanSoftTimer *previous;
	UINT posture;

	if (timer == NULL)
		return 0;
	if (timer->running == 0)
		return 0;
	posture = tx_interrupt_control(TX_INT_DISABLE);
	timer->running = 0;
	if (timer == wlanSoftTimerList) {
		wlanSoftTimerList = timer->next;
	} else {
		previous = wlanSoftTimerList;
		for (entry = wlanSoftTimerList; entry != NULL; entry = entry->next) {
			if (entry == timer)
				previous->next = entry->next;
			previous = entry;
		}
	}
	return tx_interrupt_control(posture);
}

int wlanSoftTimerTick(uint32_t index, uint32_t argument)
{
	struct WlanSoftTimer *timer;
	struct WlanSoftTimer *previous;
	UINT posture;

	(void)index;
	(void)argument;
	posture = tx_interrupt_control(TX_INT_DISABLE);
	previous = timer = wlanSoftTimerList;
	while (timer != NULL) {
		if (--timer->ticks == 0) {
			if (timer->period != 0) {
				previous = timer;
				timer->ticks = timer->period;
			} else {
				if (timer == wlanSoftTimerList) {
					previous = timer->next;
					wlanSoftTimerList = timer->next;
				} else {
					previous->next = timer->next;
				}
				timer->running = 0;
			}
			if (timer->unstarted == 0)
				wlanSoftTimerExpireHook(timer);
		} else {
			previous = timer;
		}
		timer = timer->next;
	}
	return tx_interrupt_control(posture);
}
