/*
 * The RSSI bias that follows the radio's temperature (0x4cc00..0x4cca0).
 * Marvell published no source; the names say what the code does and the file
 * name is inferred.
 */
#include <stdint.h>
#include "robin/phy.h"
#include "robin/rom.h"
#include "robin/rssibias.h"
#include "robin/txlimit.h"
#include "robin/wlanrx.h"

struct RssiBias rssiBias __attribute__((section(".itcm.rw")));

/* The bias for a sample, by the slope of the antenna in use. */
__attribute__((noinline)) int8_t fmCoexInspectBuffer(int sample)
{
	int slope = wlanAntenna != 0 ? rssiBias.calibration.slope[1] : rssiBias.calibration.slope[0];

	return (int8_t)((sample - rssiBias.calibration.reference) * slope / 1000);
}

/*
 * Applies a bias when it changed or when asked to. The bias comes in unsigned
 * and the last one is kept signed, so a negative bias is applied again on
 * every sample, as in the original.
 */
__attribute__((noinline)) void fmCoexTrackBuffer(uint8_t bias, int force)
{
	if (rssiBias.last != bias || force) {
		wlanSetRssiBias((int8_t)bias);
		rssiBias.last = bias;
	}
}

/* The sampler's callback. */
void fmCoexManageBuffer(int sample, int force)
{
	fmCoexTrackBuffer((uint8_t)fmCoexInspectBuffer(sample), force);
}

/* Starts or stops following the temperature; stopping drops the bias. */
void fmCoexAcquireBuffer(int enable)
{
	if (enable) {
		if (rssiBias.registered == 0) {
			wlanPsFormatHeader(fmCoexManageBuffer);
			rssiBias.registered = 1;
			wlanPsScheduleHeader();
		}
	} else if (rssiBias.registered != 0) {
		wlanPsInspectHeader(fmCoexManageBuffer);
		rssiBias.registered = 0;
		wlanSetRssiBias(0);
	}
}

/* Keeps the channel's calibration quad. */
void fmCoexReleaseBuffer(const void *block)
{
	if (block != NULL)
		__aeabi_memcpy(&rssiBias.calibration, block, sizeof(rssiBias.calibration));
}
