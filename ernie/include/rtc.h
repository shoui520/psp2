/*
 * The real-time clock. The clock counts the RTC's constant-period interrupts, one every
 * 0.5 s, in the RAM kept over a reset; Kermit reads it with command 0x0011
 * (sceSysconGetClock) and sets it with 0x0081. The alarm (commands 0x0012 and 0x0082) wakes
 * the system when the clock reaches it.
 */
#ifndef RTC_H
#define RTC_H

/* a clock value as the commands carry it, least significant byte first */
union RtcTick {
	unsigned long	value;
	unsigned char	byte[4];
};

#define RTC_ALARM_NONE	0xffffffff

extern void		rtcInit(void);
extern void		rtcStart(void);
extern union RtcTick	getClock(void);
extern void		setClock(unsigned long clock);
extern union RtcTick	getAlarm(void);
extern void		setAlarm(unsigned long alarm);
extern int		isAlarmReached(void);
extern unsigned int	sub_11118(void);
extern void		setAlarmMode(unsigned char mode);
extern unsigned char	getAlarmMode(void);

#endif
