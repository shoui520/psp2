/*
 * The system's records (app/sysinfo.c): the hang counter the RTC's interrupt and INTTM11
 * keep, the reset flags taken at the start, and the id records the state machine reads.
 */
#ifndef ERNIE_SYSINFO_H
#define ERNIE_SYSINFO_H

/* hangState */
#define HANG_NONE		0
#define HANG_COUNTING		1
#define HANG_RESET		2	/* the system reset with cause 0x30 */

extern __near unsigned char hangState;
extern __near unsigned char hangCount;
extern __near unsigned char resetFlags;
extern __near unsigned int idRecordsLength;
extern __near unsigned long idRecord1;

extern int hangLimitReached(void);
extern int hangDetected(void);
extern int hangStarted(void);
extern int hangTimerRunning(void);
extern unsigned char getResetFlags(void);
extern unsigned long getIdRecord1(void);
extern unsigned int getIdRecordsLength(void);

#endif
