/*
 * Stations whose WEP frames failed the ICV check, each reported once to the
 * host as Marvell's EVENT_WEP_ICV_ERR and forgotten after 500 ageing ticks
 * or when a frame from them decrypts.
 */
#ifndef ROBIN_ICVERR_H
#define ROBIN_ICVERR_H

#include <stdint.h>

#include "robin/buffer.h"
#include "robin/wlan.h"

struct WlanConnection;

#define EVENT_WEP_ICV_ERR		0x46
#define MAX_WEP_KEY_SIZE		13
#define WEP_ICV_ERR_ENTRIES		15
#define WEP_ICV_ERR_AGE_TICKS		500
#define WLAN_EVENT_WEP_ICV_ERR_PENDING	(1u << 20)	/* wlanPostConnectionEvent */

/* mlan_fw.h */
typedef struct __attribute__((packed)) Event_WEP_ICV_ERR {
	uint16_t reason_code;
	uint8_t src_mac_addr[6];
	uint8_t wep_key_index;		/* the key the decryption used */
	uint8_t wep_key_length;
	uint8_t key[MAX_WEP_KEY_SIZE];
} Event_WEP_ICV_ERR;

typedef struct WepIcvErrEntry {
	uint16_t ageTicks;		/* forgotten when it reaches 0 */
	uint16_t reported;		/* the host has the event */
	Event_WEP_ICV_ERR event;
	uint8_t padding;		/* sent with the event: 24 bytes */
	struct WepIcvErrEntry *next;
} WepIcvErrEntry;

typedef struct WepIcvErrList {
	WepIcvErrEntry *head;
	WepIcvErrEntry *tail;
	WepIcvErrEntry *freeList;
} WepIcvErrList;

_Static_assert(sizeof(WepIcvErrEntry) == 32, "WEP ICV error entry");

/*
 * Services the RAM firmware gives the ROM code, filled at start-up by
 * cbProcHandleHeader (0x1f328).
 */
struct WlanRomServices {
	int (*postConnectionEvent)(struct WlanConnection *connection, uint32_t event);
	int (*sendHostEvent)(struct WlanConnection *connection, uint16_t eventId,
			     const void *data, uint16_t length);
	int (*releaseDmaSpan)(uint32_t start, uint32_t end, unsigned int queue);
	/* sysSubmitBbpCommand: the defragmenter copies with it (target, source, length) */
	uint32_t (*submitBbpCommand)(uint32_t target, uint32_t value, uint32_t command);
	void (*resetBufferDataOffset)(OsaBuffer *buffer);
	void (*onWepIcvError)(struct WlanConnection *connection);
	void (*onWepDecrypted)(OsaBuffer *rxBuffer);
	uint32_t timerOffset;		/* added to SOC_MAC_TIMER */
	struct OsaBufferPool *packetPool;
};

extern struct WlanRomServices wlanRomServices;

void wlanWepIcvErrInit(WepIcvErrEntry *entries, WepIcvErrList *list);
void wlanWepIcvErrRemove(WepIcvErrEntry *prev, WepIcvErrEntry *entry, WepIcvErrList *list);
void wlanWepIcvErrForget(WlanWirelessFrame *frame, WepIcvErrList *list);
void wlanWepIcvErrRefresh(const uint8_t *address, WepIcvErrList *list);
void wlanWepIcvErrRecord(struct WlanConnection *connection, WlanWirelessFrame *frame,
			 uint8_t keyIndex, int isWep104, const uint8_t *key,
			 WepIcvErrList *list);
void wlanWepIcvErrAge(WepIcvErrList *list);
void wlanWepIcvErrReport(struct WlanConnection *connection, WepIcvErrList *list);

#endif /* ROBIN_ICVERR_H */
