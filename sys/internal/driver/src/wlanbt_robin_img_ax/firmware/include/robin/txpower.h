/*
 * The transmit power the channel allows and the host asks for (wlan/txpower.c, 0x3b35a..0x3b7d6),
 * the host's channel power table (hostCmd 0x00fb) and rate power table (hostCmd 0x00d1), and the
 * connection's MAC filter (0x3b14c..0x3b1e2), whose hooks are installed with the template hooks
 * (0x3b0ea..0x3b14c, wlan/aptemplate.c). The firmware's placeholder names (macMgmt*Header,
 * hostCmdXXXX) are kept; the file name, the structures and their fields are inferred.
 */
#ifndef ROBIN_TXPOWER_H
#define ROBIN_TXPOWER_H

#include <stdint.h>
#include "robin/conn.h"

#define WLAN_TX_POWER_RATES	8	/* the rate classes whose power is limited */
#define WLAN_TX_POWER_CHANNELS	14
#define WLAN_TX_POWER_INDICES	10	/* the power entries of a channel's row (row byte 0 is the channel) */

/* dBm, read and written unaligned (__aeabi_uread4(), __aeabi_uwrite4()). */
struct WlanTxPowerLimit {
	int32_t maximum;
	int32_t minimum;
	int32_t spread;		/* the most the maximum may be above the minimum */
};

/* From 0x40007ba, one packed block. */
struct __attribute__((packed)) WlanTxPowerState {
	uint8_t channel;		/* the channel the limits were computed for */
	uint8_t policy[16];		/* the row entries macMgmtInspectHeader() uses: [rate] and [rate + 8] */
	uint8_t rates[11][3];		/* hostCmd00d1Get(): the rate class, its first and last rate; 8..10 are 40 MHz */
	struct WlanTxPowerLimit limit[WLAN_TX_POWER_RATES];	/* what the channel allows (0x40007ec) */
	struct WlanTxPowerLimit request[WLAN_TX_POWER_RATES];	/* what the host asked for (0x400084c) */
	struct WlanTxPowerLimit defaults[WLAN_TX_POWER_RATES];	/* request after macMgmtSyncHeader() (0x40008ac) */
	uint8_t table[WLAN_TX_POWER_CHANNELS][WLAN_TX_POWER_INDICES + 1];	/* per channel: its number, 10 powers (0x400090c) */
};

_Static_assert(sizeof(struct WlanTxPowerState) == 492, "transmit power state");

extern struct WlanTxPowerState wlanTxPower;	/* 0x40007ba */
extern uint8_t wlanTxPowerIncludeFirst;		/* 0x5541d: hostCmd00d1Get() starts at the first rate class */

/* The hooks the template and the MAC filter code are installed in. */
extern void (*wlanSqramFrameHook)(struct WlanConnection *connection);		/* SQRAM 0xc0011bb0 */
extern void (*wlanHtOperationHook)(struct WlanConnection *connection);		/* 0x54e64 */
extern void (*wlanApTemplateHook)(struct WlanConnection *connection);		/* 0x54e84 */
extern void *(*wlanSqramMacFilterGetHook)(char code, int count, const void *list, uint8_t *out); /* SQRAM 0xc0011bb8 */
extern void *(*wlanSqramMacFilterSetHook)(uint8_t *code, uint8_t *count, void *list, const uint8_t *in); /* SQRAM 0xc0011bbc */


void macMgmtAdjustFrame(struct WlanConnection *connection);			/* 0x3b0ea */
void macMgmtRecordFrame(struct WlanConnection *connection);			/* 0x3b0fa */
void macMgmtFilterFrame(struct WlanConnection *connection);			/* 0x3b110 */
void macMgmtHandleHeader(void);							/* 0x3b120 */
void macMgmtProcessHeader(struct WlanConnectionInfo *info);			/* 0x3b14c */
void *macMgmtUpdateHeader(char code, int count, const void *list, uint8_t *out);	/* 0x3b156 */
void *macMgmtConfigureHeader(uint8_t *code, uint8_t *count, void *list, const uint8_t *in);	/* 0x3b16e */
int macMgmtCheckHeader(struct WlanConnection *connection, const uint8_t *address);	/* 0x3b18c */
void macMgmtValidateHeader(void);						/* 0x3b1e2 */
int macMgmtPrepareHeader(uint8_t channel, int band, unsigned int index, uint8_t power);	/* 0x3b200 */
int macMgmtDispatchHeader(const uint8_t *tlvs, unsigned int length);		/* 0x3b226 */
int macMgmtParseHeader(uint8_t *tlvs, uint16_t *length);			/* 0x3b2b6 */
int hostCmd00fb(struct WlanConnection *connection, unsigned int action, uint8_t *data,
		uint16_t *length);						/* 0x3b338 */
void macMgmtComputeHeader(void);						/* 0x3b35a */
int macMgmtQueryHeader(unsigned int channel, int force, int *changed);		/* 0x3b384 */
int macMgmtResetHeader(int unused, unsigned int channel, int force);		/* 0x3b4b6 */
int macMgmtSyncHeader(struct WlanConnection *connection);			/* 0x3b4d2 */
int macMgmtApplyHeader(void);							/* 0x3b4f4 */
int macMgmtResolveHeader(struct WlanConnection *connection, int power);		/* 0x3b516 */
void hostCmd0016Oid17(void);							/* 0x3b55c */
int hostCmd00d1Set(struct WlanConnection *connection, uint8_t *body);		/* 0x3b5d8 */
unsigned int hostCmd00d1Get(uint8_t *body);					/* 0x3b6a2 */
int macMgmtInspectHeader(unsigned int channel, unsigned int band, unsigned int rate);	/* 0x3b73e */
int macMgmtTrackHeader(unsigned int channel, unsigned int band, unsigned int rate);	/* 0x3b784 */

/* Not written yet. */
void macTxNotifyClearContext(unsigned int index, int32_t value);		/* 0x30c70 */
unsigned int macTxNotifyAdjustContext(unsigned int index);			/* 0x30cfa */
int macTxNotifySetupContext(unsigned int index);				/* 0x30c5e */
uint8_t macTxNotifyReleaseContext(uint32_t index);				/* 0x30c58 */

#endif /* ROBIN_TXPOWER_H */
