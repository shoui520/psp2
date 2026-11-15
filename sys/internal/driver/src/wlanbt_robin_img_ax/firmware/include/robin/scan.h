/*
 * The scan engine of the management layer (wlan/scan.c): the host's scan command (Marvell's
 * HostCmd_CMD_802_11_SCAN, 0x0006), the periodic background scan the host configures with command
 * 0x006b, and the channel loop both run on, one request to the channel scheduler (slot.h) per
 * scan. Marvell published no source for it; the names are the firmware's placeholder names
 * (macMgmt*Entry for the loop, macMgmt*Table for the background scan), kept, and the names of the
 * structures and fields are inferred from what the code does with them.
 */
#ifndef ROBIN_SCAN_H
#define ROBIN_SCAN_H

#include <stddef.h>
#include <stdint.h>
#include "robin/buffer.h"
#include "robin/conn.h"
#include "robin/ie.h"
#include "robin/sync.h"

enum {
	WLAN_SCAN_CHANNELS = 14,	/* the channels one scan loop visits */
	WLAN_SCAN_CONFIG_CHANNELS = 32,	/* the channels the background scan is configured with */
	WLAN_SCAN_SSIDS = 11,
	WLAN_SCAN_RATES = 14,
	WLAN_SCAN_RECORDS = 16,
};

/* One channel of a scan (Marvell's ChanScanParamSet_t, 7 bytes). */
struct WlanScanChannel {
	uint8_t channelFlags;		/* WlanChannelFlags: the band is in bits 0-1 */
	uint8_t channel;
	uint8_t flags;			/* bit 0: passive, no probe request is sent (Marvell's ScanType) */
	uint8_t minDwell[2];		/* little endian, ms (MinScanTime) */
	uint8_t maxDwell[2];		/* little endian, ms the scan stays on the channel (MaxScanTime) */
};

_Static_assert(sizeof(struct WlanScanChannel) == 7, "scan channel");

/* The SSID of a directed probe request: the first byte is 0 for one, nonzero for the wildcard. */
struct WlanScanSsid {
	uint8_t wildcard;
	uint8_t ssid[32];		/* an element: its id, length and bytes as the host sent it */
};

_Static_assert(sizeof(struct WlanScanSsid) == 33, "scan SSID");

/*
 * A scan as the host asked for it (convert) and as the loop runs it (the scan of
 * macMgmtVerifyEntry()), 520 bytes. wlanCommandScan (0x578d4) is the host command's, and
 * wlanBackgroundScan (0x590a4) the background scan's, which macMgmtPrepareTable() fills from the
 * configuration.
 */
struct WlanScan {
	uint8_t bssType;		/* 0: Marvell's BSS type (infrastructure, ad hoc, any) */
	uint8_t bssid[6];		/* all ones: any */
	struct WlanScanChannel channels[WLAN_SCAN_CHANNELS];
	uint8_t ssidCount;		/* 105 */
	struct WlanScanSsid ssids[WLAN_SCAN_SSIDS];
	uint8_t rates[WLAN_SCAN_RATES];	/* 469: the probe requests' supported rates */
	uint8_t reserved483;
	uint16_t probes;		/* 484: the probe requests sent on a channel (the number of probes TLV); 1 */
	uint8_t broadcast;		/* 486: a probe request with the wildcard SSID is sent on every channel */
	uint8_t reserved487;
	uint8_t policy;			/* 488: set by the background scan's configuration (flags bit 1) */
	uint8_t reserved489[3];
	uint8_t channelIndex;		/* 492: the channel the loop is on */
	uint8_t channelCount;		/* 493: the channels in the list */
	uint8_t reserved494[2];
	uint32_t slot;			/* 496: the request to the channel scheduler (wlanSlotSubmit()), 0: none */
	struct WlanIeBuffer ies;	/* 500: the elements the probe requests carry besides the SSID and the rates */
	uint32_t waited;		/* 508: the time the request waited for the channel (the scheduler's elapsed) */
	struct WlanScanResults *results;	/* 512: where the loop reports its end */
	uint32_t parameter;		/* 516: the command's parameter */
};

_Static_assert(sizeof(struct WlanScan) == 520, "scan");
_Static_assert(offsetof(struct WlanScan, probes) == 484, "scan probes");
_Static_assert(offsetof(struct WlanScan, channelIndex) == 492, "scan channel index");
_Static_assert(offsetof(struct WlanScan, slot) == 496, "scan slot");
_Static_assert(offsetof(struct WlanScan, results) == 512, "scan results");

/* What a scan found, as far as the host is told (258 bytes): the records and the end callback. */
struct WlanScanRecord {
	uint8_t reserved0[4];
	uint8_t timestamp[8];		/* the TSF of the beacon or probe response (TLV 0x0113) */
	uint8_t reserved12;
	uint8_t channelBand[2];		/* the channel and its band (TLV 0x012a) */
};

_Static_assert(sizeof(struct WlanScanRecord) == 15, "scan record");

struct WlanScanResults {
	uint8_t *data;			/* 0: the buffer the descriptors are written to */
	uint32_t length;		/* 4: the bytes written */
	uint32_t size;			/* 8: the bytes the buffer holds (1816) */
	uint8_t count;			/* 12: the descriptors written */
	uint8_t capacity;		/* 13: the records the structure holds (16) */
	struct WlanScanRecord records[WLAN_SCAN_RECORDS];	/* 14 */
	uint8_t callback[4];		/* 254: unaligned; the function the end of the scan calls with the results */
} __attribute__((packed, aligned(2)));	/* 258 bytes: the next object follows it directly */

_Static_assert(sizeof(struct WlanScanResults) == 0x102, "scan results");
_Static_assert(offsetof(struct WlanScanResults, callback) == 254, "scan results callback");

/* What the scan loop keeps of its run (wlanScanState, 0x578c0). */
struct WlanScanState {
	struct OsaSemaphore *semaphore;	/* 0: serialises the loop's callbacks */
	struct WlanScan *scan;		/* 4 */
	uint8_t completed;		/* 8: the probe requests the radio has sent */
	uint8_t queued;			/* 9: the probe requests handed to the radio */
	uint8_t allQueued;		/* 10: no probe request is left for the channel */
	uint8_t dwellDone;		/* 11: the channel's time is up */
	uint8_t broadcastSent;		/* 12: the wildcard probe request has been queued */
	uint8_t repeat;			/* 13: the probe requests queued for the SSID */
	uint8_t ssidIndex;		/* 14: the SSID the loop is on */
	uint8_t reserved15;
	struct WlanConnection *connection;	/* 16 */
};

_Static_assert(sizeof(struct WlanScanState) == 20, "scan state");

/*
 * The background scan's configuration (Marvell's host command 0x006b; two of them, 608 bytes each):
 * wlanScanSlots[0] is the one for the radio when a connection is up, wlanScanSlots[1] the other.
 */
struct WlanScanConfig {
	uint8_t enabled;		/* 0 */
	uint8_t bssType;		/* 1 */
	uint8_t channelsPerScan;	/* 2 */
	uint8_t reserved3;
	uint32_t interval;		/* 4: ms between two scans */
	uint32_t progress;		/* 8: the progress bits that complete a round; bit 1 sets the scan's policy */
	struct WlanScanSsid ssids[WLAN_SCAN_SSIDS];	/* 12 */
	struct WlanScanChannel channels[WLAN_SCAN_CONFIG_CHANNELS];	/* 375 */
	uint8_t channelCount;		/* 599 */
	uint8_t ssidCount;		/* 600 */
	uint8_t reserved601;
	uint16_t probes;		/* 602 */
	uint8_t thresholds[2];		/* 604: the RSSI threshold TLV 0x0104 and the one of 0x0105 */
	uint8_t flag;			/* 606: TLV 0x011e is not zero */
	uint8_t reserved607;
};

_Static_assert(sizeof(struct WlanScanConfig) == 0x260, "scan configuration");
_Static_assert(offsetof(struct WlanScanConfig, channels) == 375, "scan configuration channels");
_Static_assert(offsetof(struct WlanScanConfig, channelCount) == 599, "scan configuration channel count");

/* The background scan's run (wlanBackgroundScanState, 0x58f88): 284 bytes, then its scan. */
struct WlanBackgroundScanState {
	uint8_t mode;			/* 0: 0 off, 1..5 the modes macMgmtValidateTable() selects, 6 ended */
	uint8_t reserved1[3];
	uint32_t slot;			/* 4: the request to the channel scheduler */
	uint8_t position;		/* 8: the first channel of the configuration the next scan visits */
	uint8_t reserved9[3];
	uint32_t progress;		/* 12: the bits of the rounds that have ended (bit 31: wrapped around) */
	uint32_t completed;		/* 16: the progress bits the host asked for and got */
	struct WlanScanResults results;	/* 20 */
	uint8_t reserved278[2];
	struct WlanConnection *connection;	/* 280 */
};

_Static_assert(sizeof(struct WlanBackgroundScanState) == 0x11c, "background scan state");
_Static_assert(offsetof(struct WlanBackgroundScanState, results) == 20, "background scan results");

extern struct WlanScanState wlanScanState;			/* 0x578c0 */
extern struct WlanScan wlanCommandScan;				/* 0x578d4 */
extern struct WlanScanResults wlanCommandScanResults;		/* 0x57adc */
extern uint8_t wlanCommandScanWindow[2000];			/* 0x57bde */
extern uint8_t wlanBackgroundScanWindow[1816];			/* 0x583b0 */
extern struct WlanScanConfig wlanScanSlots[2];			/* 0x58ac8, 0x58d28 */
extern struct WlanScanConfig *wlanScanConfig;			/* 0x55178 */
extern struct WlanBackgroundScanState wlanBackgroundScanState;	/* 0x58f88 */
extern struct WlanScan wlanBackgroundScan;			/* 0x590a4 */

/* The ROM's. */
int mdevRomDispatchState(void *table, void *item, void *key);
void *mdevRomParseState(void *table, const void *key);
void *mdevRomPrepareState(void *table, const void *key);
int mdevRomBuildState(void *table, void *buckets, unsigned int count, int (*hash)(void *item),
		      int (*compare)(void *left, void *right));
int osaRomHandleState(void *pool, int (*release)(struct OsaPoolEntry *entry));

/* The converted ones (wlan/scan.c). */
int macMgmtFormatEntry(void);						/* 0x36ad8 */
int macMgmtInspectEntry(void);						/* 0x36ba2 */
int macMgmtTrackEntry(OsaBuffer *buffer);				/* 0x36bf0 */
int macMgmtManageEntry(void);						/* 0x36bf4 */
int macMgmtAcquireEntry(uint32_t handle, uint32_t event, OsaBuffer *buffer, uint32_t unused,
			uint32_t argument);				/* 0x36cba */
int macMgmtReleaseEntry(uint32_t handle, int event, uint32_t argument);	/* 0x36d12 */
OsaBuffer *macMgmtSetupEntry(struct WlanScanResults *results);		/* 0x36dd4 */
int macMgmtClearEntry(uint8_t *out, const struct WlanScanResults *results);	/* 0x36e08 */
int macMgmtConvertEntry(int unused, uint8_t *command, int16_t length, struct WlanScan *scan);	/* 0x36ef4 */
uint32_t macMgmtVerifyEntry(struct WlanConnection *connection, struct WlanScan *scan, uint32_t delay);	/* 0x37042 */
int hostCmd0006(struct WlanConnection *connection, uint8_t parameter, uint8_t *command, int16_t length);	/* 0x370f2 */
void macMgmtAdjustEntry(struct WlanConnection *connection);		/* 0x37176 */
int macMgmtRecordEntry(void);						/* 0x3719a */
int macMgmtFilterEntry(void);						/* 0x371b8 */
int macMgmtHandleTable(void);						/* 0x371c8 */
int macMgmtProcessTable(void);						/* 0x371d4 */
int macMgmtUpdateTable(void);						/* 0x371f0 */
void macMgmtConfigureTable(void);					/* 0x37206 */
int macMgmtCheckTable(void);						/* 0x37216 */
void macMgmtValidateTable(int event);					/* 0x3726c */
void macMgmtCompleteTable(struct WlanScanResults *results);		/* 0x3733a */
int macMgmtPrepareTable(void);						/* 0x3747c */
void *macMgmtDispatchTable(void);					/* 0x37614 */
void macMgmtParseTable(int mode);					/* 0x37634 */
int macMgmtBuildTable(unsigned int slot, const uint8_t *command, int length);	/* 0x376a0 */
int hostCmd006bSet(struct WlanConnection *connection, unsigned int slot, const uint8_t *command, int length);	/* 0x378b0 */
int hostCmd006bGet(void *out, int reset);				/* 0x378f0 */
int macMgmtResetTable(void);						/* 0x3791c */
int macMgmtSyncTable(void);						/* 0x37944 */
void macMgmtApplyTable(void);						/* 0x3795a */
int hostCmd001cResolve(void);						/* 0x37966 */
unsigned int macMgmtScheduleTable(unsigned int enabled);		/* 0x3796e */
int macMgmtNotifyTable(void);						/* 0x3797a */
int macMgmtFormatTable(const struct WlanScan *scan);			/* 0x37998 */
int macMgmtInspectTable(unsigned int channelIndex);			/* 0x379a8 */

/* Provisional prototypes of what is not converted yet (their addresses). */
int p2pSqramHandleState(struct WlanConnection *connection);		/* SQRAM, veneer 0x53254 */
int p2pSqramScheduleState(struct WlanConnection *connection, uint32_t slot);	/* SQRAM, veneer 0x53284 */
void p2pSqramProcessState(struct WlanConnection *connection, unsigned int state, uintptr_t argument,
			  int event);					/* SQRAM, veneer 0x5328c */
int p2pSqramNotifyState(void);						/* SQRAM, veneer 0x53294 */
int p2pDirectFormatContext(struct WlanConnection *connection);		/* 0x40da2 */
unsigned int fmCoexConvertEntry(unsigned int type, const uint8_t *tlv, uint8_t *ssid, uint8_t *count,
			unsigned int maximum);				/* 0x4ba8e */
void fmCoexFormatBuffer(uintptr_t frame, uint8_t *body, uintptr_t argument, struct WlanScan *scan,
			unsigned int channelIndex);			/* 0x4cb36 */
int fmCoexVerifyBuffer(struct WlanConnectionInfo *info, const uint8_t *tlv);	/* 0x4cdde */
unsigned int fmCoexVerifyEntry(struct WlanScanResults *results, int mode);	/* 0x4bb2c */
unsigned int fmCoexTrackEntry(void *record, int mode);			/* 0x4b7ae */
int fmCoexManageEntry(unsigned int record, unsigned int threshold, int mode);	/* 0x4b7c0 */

#endif /* ROBIN_SCAN_H */
