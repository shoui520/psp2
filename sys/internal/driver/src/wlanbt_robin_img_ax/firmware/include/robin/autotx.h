/*
 * Automatic transmission of a periodic frame: Marvell's HostCmd_CMD_AUTO_TX
 * (host command 0x82). The host gives one Ethernet frame and an interval; the
 * firmware sends it to the host's own transmit path every interval while the
 * station is associated, also when the host sleeps.
 */
#ifndef ROBIN_AUTOTX_H
#define ROBIN_AUTOTX_H

#include <stdint.h>
#include "robin/buffer.h"

struct WlanConnection;

#define WLAN_AUTO_TX_TLV_TYPE		0x0118	/* the frame's TLV (TLV_TYPE_AUTO_TX_FRAME) */
#define WLAN_AUTO_TX_TAG		90	/* TX_DATA_TAG of the frames it builds */

/* The host's TLV as the host command gives it (action, header, AutoTx_MacFrame_t). */
struct __attribute__((packed)) WlanAutoTxFrame {
	uint16_t action;
	uint16_t type;			/* WLAN_AUTO_TX_TLV_TYPE */
	uint16_t length;		/* bytes after this field */
	uint16_t interval;		/* seconds; 0: no frame is configured */
	uint8_t priority;		/* user priority, used when WMM is on */
	uint8_t reserved;
	uint16_t frameLength;		/* the bytes from destination on */
	uint8_t destination[6];		/* zero: the connection's local address */
	uint8_t source[6];		/* zero: the interface address */
	uint8_t payload[98];		/* the rest of the record */
};

/* The configured frame (0x5745e) and the time left until it is due. */
struct __attribute__((packed)) WlanAutoTxRecord {
	uint8_t remaining[8];		/* unaligned 64-bit count, milliseconds */
	union {
		struct WlanAutoTxFrame frame;
		uint8_t bytes[122];
	};
};

_Static_assert(sizeof(struct WlanAutoTxRecord) == 130, "auto transmission record");

unsigned int hostCmdAutoTxSet(const uint8_t *body);	/* 0x2fcc2: NULL clears it */
unsigned int hostCmdAutoTxGet(uint8_t *body);		/* 0x2fd28 */
void wlanAutoTxInit(void);				/* 0x2fbcc */
int wlanAutoTxConfigured(void);				/* 0x2fefa */
int wlanAutoTxWanted(const struct WlanConnection *connection);	/* 0x2ff0c */
int wlanAutoTxTick(void);				/* 0x2fbe8 */
OsaBuffer *wlanAutoTxNextFrame(void);			/* 0x2fe92 */

#endif /* ROBIN_AUTOTX_H */
