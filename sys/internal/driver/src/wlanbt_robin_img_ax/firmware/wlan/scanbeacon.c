/*
 * A beacon or probe response the scan receives (0x4cb10..0x4cc00): checked
 * against what the scan asks for and handed to the scan result list
 * (wlan/scanresult.c). Marvell published no source; the names say what the
 * code does and the file name is inferred.
 */
#include <stdint.h>
#include "robin/ie.h"
#include "robin/mactx.h"
#include "robin/phy.h"
#include "robin/scan.h"
#include "robin/wlanrx.h"

/* The capability read as a word, as the original loads it. */
typedef uint32_t __attribute__((may_alias)) WlanCapabilityWord;

/* Not converted yet; provisional prototype. */
void macMgmtAdjustTable(const WlanWirelessFrame *frame, int kind);	/* 0x37c72 */

/* The OUI and type of the WPS element: an active scan's channel skips the beacons that carry one. */
uint8_t wlanScanWpsOui[4] __attribute__((section(".itcm.rw"))) = { 0x00, 0x50, 0xf2, 0x04 };

/* Whether a BSS's capability fits the BSS type a scan asks for. */
int fmCoexNotifyBuffer(uint32_t capability, unsigned int bssType, int p2p)
{
	switch (bssType) {
	case 1:
		return (capability & 1) != 0;
	case 2:
		return (capability & 2) != 0;
	case 3:
		return (capability & 3) != 0 || p2p == 1;
	default:
		return 0;
	}
}

/*
 * A beacon or probe response heard on the scan's channel. The capability is
 * read as a word from the body (the original's load), of which only the low
 * bits count.
 */
void fmCoexFormatBuffer(uintptr_t context, WlanWirelessFrame *frame, uintptr_t argument,
			struct WlanScan *scan, unsigned int channelIndex)
{
	const struct WlanRxStatus *status = (const struct WlanRxStatus *)argument;
	int found = 0;
	unsigned int snr;
	unsigned int rssi;

	if ((scan->channels[channelIndex].flags & 1) == 0 && ((frame->frameControl[0] >> 2) & 3) == 0 &&
	    (frame->frameControl[0] >> 4) == 8 &&
	    wlanFindIe(frame, IE_VENDOR, WLAN_FRAME_BEACON, wlanScanWpsOui, 4) != NULL)
		return;
	if (fmCoexNotifyBuffer(*(const WlanCapabilityWord *)&frame->body[10], scan->bssType,
			       p2pSqramParseContext(context)) == 0)
		return;
	if (fmCoexClearEntry(frame->address3, scan->bssid) == 0)
		return;
	fmCoexInspectEntry(frame, scan, &found);
	p2pSqramDispatchContext(context, frame);
	if (!found)
		return;
	macMgmtAdjustTable(frame, WLAN_FRAME_BEACON);
	if ((frame->bodyLength[0] | frame->bodyLength[1] << 8) == 0 || scan == NULL)
		return;
	snr = (uint8_t)wlanClampSnr((int8_t)status->rssi - (int8_t)status->noise);
	rssi = (uint8_t)wlanClampRssi((int8_t)status->rssi);
	fmCoexSetupEntry(frame, argument, scan, channelIndex, snr, rssi);
}
