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

/* INTTM11 counts a hang from 0x00090206 until 0x0100060B; 0x00090009 has no hang counter */
#define HANG_TIMER		(FW_SINCE(0x00090203) && !FW_SINCE(0x0100060B))

extern SYSINFO_NEAR unsigned char hangState;
extern SYSINFO_NEAR unsigned char hangCount;
extern SYSINFO_NEAR unsigned char resetFlags;
#if FW_SINCE(0x01000107)
extern SYSINFO_NEAR unsigned int idRecordsLength;
extern SYSINFO_NEAR unsigned long idRecord1;
#endif

extern int hangLimitReached(void);
#if FW_SINCE(0x01000104)
extern int hangDetected(void);
#define HANG_DETECTED()		(hangDetected() != 0)
#else
/* before 0x01000104 a hang is P11.5 and P4.6 low, tested in place */
#include "chip.h"
#define HANG_DETECTED()		(!P11.5 && !P4.6)
#endif
#if !FW_SINCE(0x0100060B) && (FW_SINCE(0x00090804) || !FW_SINCE(0x00090502))
extern int hangStarted(void);
#endif
#if FW_SINCE(0x01000104) && !FW_SINCE(0x0100060B)
extern int hangTimerRunning(void);
#endif
extern unsigned char getResetFlags(void);
#if FW_SINCE(0x01000107)
extern unsigned long getIdRecord1(void);
extern unsigned int getIdRecordsLength(void);
#endif

#endif
