/*
 * The channel the radio is on, as the access point and IBSS code keep it: the channel flags
 * (WlanChannelFlags: the band in bits 0-1, the secondary channel in bits 4-5) and number, the
 * ones to return to (what the radio had when the connection took the channel), and the transmit
 * power limit of the link. Marvell published no source for it; the names are the firmware's own
 * placeholder names (macMgmt*Context), kept, and the file name is inferred.
 */
#include <stddef.h>
#include "robin/adhoc.h"
#include "robin/apsta.h"
#include "robin/conn.h"
#include "robin/domain.h"
#include "robin/htba.h"
#include "robin/mgmt.h"
#include "robin/rom.h"

struct WlanChannelContext {
	uint8_t reserved0;
	uint8_t savedFlags;		/* the flags macMgmtCheckContext() found the radio on */
	uint8_t linkPowerLimit;		/* wlanTxPowerGetLinkLimit() then */
	uint8_t flags;			/* the flags the radio is on */
	uint8_t reserved4[8];
	uint32_t savedChannel;		/* 0: none yet */
	uint32_t channel;		/* the channel the radio is on */
};

struct WlanChannelContext wlanChannelContext __attribute__((section(".itcm.rw")));	/* 0x55158 */

/* 0x355b0: the channel's secondary channel is set from the HT operation element of the connection. */
void macMgmtProcessContext(struct WlanConnection *connection, uint8_t *channel, unsigned int number)
{
	uint8_t *htOperation = (uint8_t *)wlanConnectionHtOperation(connection);
	uint8_t index;

	wlanSetSecondaryChannel(channel, number, htOperation, wlanDomainChannelIndex(12, &index));
}

/* 0x355d2: the radio goes to the channel, unless it is there. */
int macMgmtUpdateContext(uint8_t channelFlags, unsigned int channel)
{
	uint8_t flags = channelFlags;

	if (channel != wlanChannelContext.channel || flags != wlanChannelContext.flags) {
		wlanSetChannel(channel, &flags, 1);
		wlanChannelContext.flags = flags;
		wlanChannelContext.channel = channel;
		if (wlanChannelContext.savedChannel == 0) {
			wlanChannelContext.savedFlags = flags;
			wlanChannelContext.savedChannel = channel;
		}
	}
	return 0;
}

/* 0x3560c: the radio's secondary channel follows the HT operation element. */
void macMgmtConfigureContext(const uint8_t *htOperation)
{
	uint8_t index;

	if (((wlanChannelContext.flags >> 4) & 3) != (htOperation[3] & 3)) {
		wlanSetSecondaryChannel(&wlanChannelContext.flags, wlanChannelContext.channel & 0xff,
					(uint8_t *)htOperation, wlanDomainChannelIndex(12, &index));
		wlanSetChannelFlags(&wlanChannelContext.flags);
	}
}

/* 0x3564a: what the radio is on is what it is to return to. */
void macMgmtCheckContext(void)
{
	wlanChannelContext.savedFlags = wlanChannelContext.flags;
	wlanChannelContext.savedChannel = wlanChannelContext.channel;
	wlanChannelContext.linkPowerLimit = wlanTxPowerGetLinkLimit();
}

/* 0x3565e */
void macMgmtValidateContext(void)
{
	wlanChannelContext.linkPowerLimit = wlanTxPowerGetLinkLimit();
}
