/*
 * The rates a connection transmits at, read from the rate set it keeps with
 * its highest and lowest rate of each class (wlanRateSetLimits()), and the
 * MAC operation the channel change waits for. See robin/macentry.h. The
 * file name is inferred.
 */
#include <stddef.h>
#include "robin/macentry.h"
#include "robin/macrate.h"
#include "robin/mac.h"
#include "robin/socpower.h"
#include "robin/sys.h"
#include "robin/txpower.h"

/* Not converted yet; provisional names. */
int macMgmtParseContext(void);					/* 0x356b4 */

/* MAC_650_OPERATION bit 2 as it was before wlanMacOperationWait() (SQRAM 0xc0011c04). */
extern uint32_t wlanMacOperationSaved;

/* Takes a new rate set and finds the limits of its classes. */
void wlanConnectionRateSetUpdate(struct WlanConnection *connection, uint32_t rateSet)
{
	wlanRateSetLimits(&connection->rateSet, rateSet, &connection->ofdmHigh, &connection->ofdmLow,
			  &connection->cckHigh, &connection->cckLow);
}

/* The first rate at or below start in the CCK range of the rate set. */
uint8_t wlanTxLowRate(struct WlanConnection *connection, unsigned int start)
{
	return wlanFindSetBitDown(connection->rateSet, start, connection->cckHigh, connection->cckLow);
}

unsigned int wlanConnectionResponseRate(struct WlanConnection *connection, unsigned int rate)
{
	return wlanRateResponseSelect(rate, connection->rateSet);
}

void wlanTxHookIdle(void)
{
}

/* Saves the MAC operation's bit 2 and waits for the operation to finish. */
int wlanMacOperationWait(uint32_t timeout, uint32_t *status)
{
	wlanMacOperationSaved = MAC_650_OPERATION & 4;
	return wlanMacPollOperation(timeout, status, (void (*)(int))sysDelayUs);
}

/* Starts the operation again with bit 2 as it was. */
void wlanMacOperationRestore(void)
{
	MAC_650_OPERATION = (MAC_650_OPERATION >> 2) << 2;
	MAC_650_OPERATION |= wlanMacOperationSaved;
}

int wlanSlotTableSize(int table)
{
	if (table != 0)
		return 16;
	return 8;
}

uint32_t wlanMacConfigApply(uint32_t value)
{
	uint8_t config = wlanTxPowerRateClass(value);

	return wlanMacProgramConfig(value, config);
}

/* The lowest rate the connection's rate set has: CCK, else OFDM, else the rate of the band. */
uint8_t wlanTxControlRate(struct WlanConnection *connection)
{
	unsigned int result = connection->cckLow;

	if (connection->cckHigh < result) {
		result = connection->ofdmLow;
		if (connection->ofdmHigh < result) {
			if (macMgmtParseContext() == 2)
				return 5;
			return 0;
		}
	}
	return result;
}

/* The rate an access point sends group frames at: its highest OFDM rate, else its highest CCK rate. */
unsigned int wlanTxApGroupRate(struct WlanConnection *connection)
{
	unsigned int result;

	if (connection->ofdmHigh >= connection->ofdmLow)
		return connection->ofdmHigh;
	result = connection->cckHigh;
	if (result < connection->cckLow) {
		if (macMgmtParseContext() != 0)
			return 9;
		return 3;
	}
	return result;
}
