/*
 * 802.11 defragmentation: the fragments of one MSDU are copied into one pool
 * buffer behind the first fragment's header, WEP fragments decrypted on the way.
 */
#ifndef ROBIN_DEFRAG_H
#define ROBIN_DEFRAG_H

#include <stdint.h>

#include "robin/buffer.h"
#include "robin/icverr.h"
#include "robin/wlan.h"

#define WLAN_FC1_MORE_FRAGMENTS		0x04

#define WLAN_DEFRAG_BUFFER_SIZE		2000
#define WLAN_DEFRAG_WEP_BUFFER_SIZE	1580

typedef struct WlanDefragContext {
	OsaBuffer *buffer;		/* the MSDU being reassembled */
	uint8_t *writePtr;		/* where the next fragment's body goes */
	uint32_t startTime;		/* SOC_MAC_TIMER at the first fragment */
	uint32_t reserved;		/* not used by the defragmenter */
	uint16_t sequenceNumber;
	uint8_t lastFragmentNumber;	/* 0xff: none yet */
} WlanDefragContext;

_Static_assert(sizeof(WlanDefragContext) == 20, "defragmentation context");

/* mlan_fw.h: the 802.11 statistics up to the beacon counters (newer chips append more) */
typedef struct __attribute__((packed)) HostCmd_DS_802_11_GET_LOG {
	uint32_t mcast_tx_frame;
	uint32_t failed;
	uint32_t retry;
	uint32_t multiretry;
	uint32_t frame_dup;
	uint32_t rts_success;
	uint32_t rts_failure;
	uint32_t ack_failure;
	uint32_t rx_frag;
	uint32_t mcast_rx_frame;
	uint32_t fcs_error;
	uint32_t tx_frame;
	uint32_t reserved;
	uint32_t wep_icv_err_cnt[4];	/* WEP ICV errors per key */
	uint32_t bcn_rcv_cnt;
	uint32_t bcn_miss_cnt;
} HostCmd_DS_802_11_GET_LOG;

/* the WEP engine (0x4fb0..0x5614, not converted) */
int wlanWepDecrypt(uint8_t *output, const uint8_t *input, uint32_t length,
		   const uint8_t *seed, uint8_t isWep104, WlanWirelessFrame *frame);

void wlanDefragReset(WlanDefragContext *context);
OsaBuffer *wlanDefragment(OsaBuffer *rxBuffer, WlanWirelessFrame *frame,
			  OsaBuffer **reassembly, WlanDefragContext *context,
			  uint8_t *discarding);
OsaBuffer *wlanDefragmentWep(OsaBuffer *rxBuffer, const uint8_t *seed, uint8_t isWep104,
			     uint8_t keyIndex, OsaBuffer **reassembly,
			     WlanDefragContext *context, WepIcvErrList *icvErrList,
			     HostCmd_DS_802_11_GET_LOG *statistics);

#endif /* ROBIN_DEFRAG_H */
