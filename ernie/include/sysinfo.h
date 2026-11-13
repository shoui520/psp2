/*
 * The system's records (app/sysinfo.c): the hang counter the RTC's interrupt and INTTM11
 * keep, the reset flags taken at the start, and the id records the state machine reads.
 */
#ifndef ERNIE_SYSINFO_H
#define ERNIE_SYSINFO_H

#include "ernie.h"

/* the records' variables: near (saddr page) since 0x01000107, in the far data before */
#if FW_SINCE(0x01000107)
#define SYSINFO_NEAR		__near
#else
#define SYSINFO_NEAR
#endif

/* hangState */
#define HANG_NONE		0
#define HANG_COUNTING		1
#define HANG_RESET		2	/* the system reset with cause 0x30 */

extern SYSINFO_NEAR unsigned char hangState;
extern SYSINFO_NEAR unsigned char hangCount;
extern SYSINFO_NEAR unsigned char resetFlags;
#if FW_SINCE(0x01000107)
extern SYSINFO_NEAR unsigned int idRecordsLength;
extern SYSINFO_NEAR unsigned long idRecord1;
#endif

extern int hangLimitReached(void);
extern int hangDetected(void);
extern int hangStarted(void);
extern int hangTimerRunning(void);
extern unsigned char getResetFlags(void);
#if FW_SINCE(0x01000107)
extern unsigned long getIdRecord1(void);
extern unsigned int getIdRecordsLength(void);
#endif

#endif
