/*
 * The system's records: the hang counter, which the RTC's interrupt and INTTM11 count up
 * while hangDetected holds and which resets the system at the settings' hangLimit (PCH-1000
 * detects one by four pins, the others nothing), the reset flags (RESF at the start), and the
 * id records.
 */
#pragma sfr

#include "board.h"
#include "config.h"
#include "sysinfo.h"

SYSINFO_NEAR unsigned char hangState;		/* HANG_* */
SYSINFO_NEAR unsigned char hangCount;
SYSINFO_NEAR unsigned char resetFlags;	/* RESF at the start */
#if FW_SINCE(0x01000107)
SYSINFO_NEAR unsigned int idRecordsLength;	/* the lengths of id records 0x0f, 0x10, 0x12 and 0x16 */
SYSINFO_NEAR unsigned long idRecord1;		/* id record 1 */
#endif

int hangLimitReached(void)
{
	return hangCount == (configGet()->hangLimit < 8 ? 8 : configGet()->hangLimit);
}

#if !FW_SINCE(0x01000104)
#elif BOARD_HANG_PINS && BOARD_CEM3000NE2 || BOARD_102603 && !HW_IS(0x00315200)
/* a hang: P11.5 and P4.6 both low (CEM-3000NE2 tests only these two) */
int hangDetected(void)
{
	return !P11.5 && !P4.6;
}
#elif BOARD_HANG_PINS && BOARD_USS1001
/* the slim's four pins: P7.4, P7.6, P7.0 and P7.3 all low */
int hangDetected(void)
{
	return !P7.4 && !P7.6 && !P7.0 && !P7.3;
}
#elif BOARD_HANG_PINS
/* a hang: P11.5, P4.6, P5.0 and P5.3 all low */
int hangDetected(void)
{
	return !P11.5 && !P4.6 && !P5.0 && !P5.3;
}
#else
int hangDetected(void)
{
	return 0;
}
#endif

#if !FW_SINCE(0x0100060B) && (FW_SINCE(0x00090804) || BOARD_STANDBY_STOP)
/* neither is in 0x0100060B, nor in 0x00090206 without the standby stop */
int hangStarted(void)
{
	return hangCount == 1;
}
#endif

#if FW_SINCE(0x01000104) && !FW_SINCE(0x0100060B)
/* timer 11 running (TE11) */
int hangTimerRunning(void)
{
	return (int)TE1L.1;
}
#endif

#if FW_SINCE(0x00090804)
unsigned char getResetFlags(void)
{
	return resetFlags;
}
#endif

#if FW_SINCE(0x01000107)
unsigned long getIdRecord1(void)
{
	return idRecord1;
}

unsigned int getIdRecordsLength(void)
{
	return idRecordsLength;
}
#endif
