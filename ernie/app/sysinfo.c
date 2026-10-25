/*
 * The system's records: the hang counter, which the RTC's interrupt and INTTM11 count up
 * while hangDetected holds and which resets the system at the settings' hangLimit (in this
 * build nothing is detected), the reset flags (RESF at the start), and the id records.
 */
#pragma sfr

#include "config.h"
#include "sysinfo.h"

__near unsigned char hangState;		/* HANG_* */
__near unsigned char hangCount;
__near unsigned char resetFlags;	/* RESF at the start */
__near unsigned int idRecordsLength;	/* the lengths of id records 0x0f, 0x10, 0x12 and 0x16 */
__near unsigned long idRecord1;		/* id record 1 */

int hangLimitReached(void)
{
	return hangCount == (configGet()->hangLimit < 8 ? 8 : configGet()->hangLimit);
}

int hangDetected(void)
{
	return 0;
}

int hangStarted(void)
{
	return hangCount == 1;
}

/* timer 11 running (TE11) */
int hangTimerRunning(void)
{
	return (int)TE1L.1;
}

unsigned char getResetFlags(void)
{
	return resetFlags;
}

unsigned long getIdRecord1(void)
{
	return idRecord1;
}

unsigned int getIdRecordsLength(void)
{
	return idRecordsLength;
}
