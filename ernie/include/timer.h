/*
 * The software timers (app/timer.c): a timer runs its handler when its count of ticks runs
 * out.
 */
#ifndef ERNIE_TIMER_H
#define ERNIE_TIMER_H

#include "ernie.h"
#include "board.h"

struct TimerHandler {
	void		(*handler)(void);
};

/* a timer: armed (1) until its count of ticks reaches time, then its handler runs */
struct Timer {
	unsigned char	state;
#if !defined(ERNIE_UPDATER) && FW_SINCE(0x0100060B)
	unsigned char	handlerSet;	/* timerSetHandler gave it one: timerStart arms only then */
#endif
	unsigned int	time;
	void		(*handler)(void);
};

/* timers 0x00-0x14 count timerTicks, 0x15-0x1f timerRtcTicks (the RTC's, every 0.5 s); the
   updater's 0x15-0x1d count the clock. Before 0x01000306 the application has no timer 0x14
   (the power LED's fade step): its RTC timers are 0x14-0x1e, each one below its number since,
   which TIMER_RTC() gives. Before 0x01000104 power timers 9 and 10 have timers 0x0c and 0x0d,
   so the timers from 0x0c up are two later (TIMER()), the RTC's from 0x15 one later, and there
   are no 0x1e and 0x1f. Since 0x0100060B power bits 0 and 3 have no timer, so the timers from
   0x0c up are two earlier (TIMER(), POWER_TIMER()), and two new ones (0x15, power bit 14, and
   0x16) put the RTC's at 0x17-0x21.
   0x00090206 counts the clock from timer 0x13 (TIMER(0x11)) on, and has no power LED
   timer 0x1c: its RTC timers 0x14-0x1b are 0x13-0x1a, each one below its number */
#if !defined(ERNIE_UPDATER) && BOARD_USS1001 && SOURCE_0104
/* the slim's 0x0105 (USS-1002): no modem (bbmc.c's timers 1, 0x11, 0x16 and 0x1a of 0x0103)
   and no timers 0-2, so the RTC's are at 0x12-0x1b, four below their numbers, and five from
   0x19 (the modem's) on */
#define TIMER_RTC_FIRST		0x12
#define TIMER_RTC(id)		((id) < 0x19 ? (id) - 4 : (id) - 5)
#elif !defined(ERNIE_UPDATER) && BOARD_USS1001 && FW_SINCE(0x01030511)
/* the slim's: the RTC's at 0x16-0x20, one above their numbers (the charge's soon update, 0x16
   on the others, is 0x13) */
#define TIMER_RTC_FIRST		0x16
#define TIMER_RTC(id)		((id) + 1)
#elif !defined(ERNIE_UPDATER) && BOARD_DOL1001 && FW_SINCE(0x01040000)
/* DOL-1002's: the RTC's at 0x18-0x24, three above their numbers, with two more (0x20, 0x21) */
#define TIMER_RTC_FIRST		0x18
#define TIMER_RTC(id)		((id) + 3)
#elif !defined(ERNIE_UPDATER) && BOARD_USS1001
/* 0x01030505: the RTC's at 0x15-0x1f, at their numbers */
#define TIMER_RTC_FIRST		0x15
#define TIMER_RTC(id)		(id)
#elif !defined(ERNIE_UPDATER) && FW_SINCE(0x0100060B)
#define TIMER_RTC_FIRST		0x17
#define TIMER_RTC(id)		((id) + 2)
#elif defined(ERNIE_UPDATER) && FW_SINCE(0x0100060D)
/* the updater's since 0x0100060D: the RTC's at 0x14-0x1e */
#define TIMER_RTC_FIRST		0x14
#define TIMER_RTC(id)		((id) - 1)
#elif defined(ERNIE_UPDATER) || FW_SINCE(0x01000306)
#define TIMER_RTC_FIRST		0x15
#define TIMER_RTC(id)		(id)
#elif FW_SINCE(0x01000104)
#define TIMER_RTC_FIRST		0x14
#define TIMER_RTC(id)		((id) - 1)
#elif FW_SINCE(0x00090804)
#define TIMER_RTC_FIRST		0x15
#define TIMER_RTC(id)		((id) + 1)
#elif FW_SINCE(0x00090009)
#define TIMER_RTC_FIRST		0x13
#define TIMER_RTC(id)		((id) - 1)
#else
#define TIMER_RTC_FIRST		0x11
#define TIMER_RTC(id)		((id) - 3)
#endif
#if !defined(ERNIE_UPDATER) && BOARD_USS1001 && SOURCE_0104
/* five below, and six above the modem's 0x13 */
#define TIMER(id)		((id) < 0x13 ? (id) - 5 : (id) - 6)
#elif FW_SINCE(0x0100060B) && (!defined(ERNIE_UPDATER) || FW_SINCE(0x0100060D))
/* the application's since 0x0100060B, the updater's since 0x0100060D */
#define TIMER(id)		((id) - 2)
#elif defined(ERNIE_UPDATER) || FW_SINCE(0x01000104) || !FW_SINCE(0x00090009)
#define TIMER(id)		(id)
#else
#define TIMER(id)		((id) + 2)
#endif
#if defined(ERNIE_UPDATER) && FW_SINCE(0x0100060D)
#define TIMER_COUNT		0x1f
#elif defined(ERNIE_UPDATER)
#define TIMER_COUNT		0x1e
#elif BOARD_DOL1001 && FW_SINCE(0x01040000)
#define TIMER_COUNT		(TIMER_RTC(0x21) + 1)
#elif BOARD_USS1001 && SOURCE_0104
#define TIMER_COUNT		(TIMER_RTC(0x20) + 1)
#elif FW_SINCE(0x01000104)
#define TIMER_COUNT		(TIMER_RTC(0x1f) + 1)
#elif FW_SINCE(0x00090903)
#define TIMER_COUNT		0x1f
#elif FW_SINCE(0x00090804)
#define TIMER_COUNT		0x1e	/* 0x00090804: no timer 0x1e */
#elif FW_SINCE(0x00090009)
#define TIMER_COUNT		0x1b	/* 0x00090206: no power timers 9 and 10 (TIMER()), and
					   the RTC's end at 0x1c (TIMER_RTC()) */
#else
#define TIMER_COUNT		0x18	/* 0x00080504: timers 0x0c and up at their numbers, the RTC's
					   from 0x11 (TIMER_RTC()), no watchdog timer */
#endif

extern __near struct Timer timers[TIMER_COUNT];
extern __sreg unsigned int timerTicks;
#if FW_SINCE(0x01000107)
extern __sreg unsigned int timerRtcTicks;
#endif

extern int timerInit(void);
extern int timerSetHandler(unsigned char id, struct TimerHandler *handler);
extern int timerStart(unsigned char id, unsigned int ticks);
extern int timerStop(unsigned char id);
extern int timerGetTicks(unsigned int *ticks);
extern int timerSetTicks(unsigned int *ticks);
extern int timerArmed(void);
extern int timerSetRtcTicks(unsigned int *ticks);
extern int timerGetRtcTicks(unsigned int *ticks);
extern void timerTick(void);

#endif
