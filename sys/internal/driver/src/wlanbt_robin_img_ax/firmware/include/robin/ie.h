/* 802.11 information element helpers working on WlanWirelessFrame bodies. */
#ifndef ROBIN_IE_H
#define ROBIN_IE_H

#include <stdint.h>
#include "robin/buffer.h"
#include "robin/wlan.h"

enum {
	IE_SSID = 0,
	IE_SUPPORTED_RATES = 1,
	IE_FH_PARAMETER = 2,
	IE_DS_PARAMETER = 3,
	IE_CF_PARAMETER_SET = 4,
	IE_TIM = 5,
	IE_IBSS_PARAMETER = 6,
	IE_COUNTRY = 7,
	IE_EDCA_PARAMETER_SET = 12,
	IE_POWER_CAPABILITY = 33,
	IE_ERP_INFORMATION = 42,
	IE_HT_CAPABILITY = 45,
	IE_QOS_CAPABILITY = 46,
	IE_RSN = 48,
	IE_AP_CHANNEL_REPORT = 51,
	IE_MOBILITY_DOMAIN = 54,
	IE_EXTENDED_RATES = 50,
	IE_HT_INFORMATION = 61,
	IE_WAPI = 68,
	IE_20_40_BSS_COEXISTENCE = 72,
	IE_OBSS_SCAN_PARAMETERS = 74,
	IE_EXTENDED_CAPABILITIES = 127,
	IE_VENDOR = 221,
};

/* The kind of a vendor element wlanWmmElementType() and wlanWpaElementType() name. */
enum {
	WLAN_VENDOR_NONE = 0,
	WLAN_VENDOR_WMM_INFORMATION = 1,
	WLAN_VENDOR_WMM_PARAMETER = 2,
	WLAN_VENDOR_WPA = 3,
	WLAN_VENDOR_WMM_TSPEC = 5,
};

/* The largest association ID a TIM element covers. */
enum { WLAN_AID_MAX = 2007 };

enum {
	WLAN_FRAME_PROBE_REQUEST = 4,
	WLAN_FRAME_PROBE_RESPONSE = 5,
	WLAN_FRAME_BEACON = 8,
};

enum {
	WLAN_HT_CAP_40MHZ = 1 << 1,	/* supported channel width set */
};

/* HT Capabilities element (802.11n 7.3.2.56), from its id byte */
typedef struct __attribute__((packed)) WlanHtCapabilityIe {
	uint8_t id;
	uint8_t length;
	union {
		uint16_t capabilities;
		struct __attribute__((packed)) {	/* 802.11n 7.3.2.56.2 */
			uint16_t ldpcCoding : 1;
			uint16_t supportedChannelWidth : 1;	/* WLAN_HT_CAP_40MHZ */
			uint16_t smPowerSave : 2;
			uint16_t greenfield : 1;
			uint16_t shortGi20 : 1;
			uint16_t shortGi40 : 1;
			uint16_t txStbc : 1;
			uint16_t rxStbc : 2;
			uint16_t delayedBlockAck : 1;
			uint16_t maxAmsduLength : 1;
			uint16_t dsssCck40 : 1;
			uint16_t reserved : 1;
			uint16_t fortyMhzIntolerant : 1;
			uint16_t lsigTxopProtection : 1;
		} capability;
	};
	uint8_t ampduParameters;
	uint8_t mcsSet[16];
	uint16_t extendedCapabilities;
	uint32_t beamforming;
	uint8_t antennaSelection;
} WlanHtCapabilityIe;

/* one access category of the WMM parameter element */
typedef struct __attribute__((packed)) WlanWmmAcParameter {
	uint8_t aciAifsn;		/* AIFSN 0-3, ACM 4, ACI 5-6 */
	uint8_t ecw;			/* ECWmin 0-3, ECWmax 4-7 */
	uint16_t txopLimit;
} WlanWmmAcParameter;

/* WMM Parameter element (WMM 2.2.2), from its id byte */
typedef struct __attribute__((packed)) WlanWmmParameterIe {
	uint8_t id;
	uint8_t length;
	uint8_t oui[3];
	uint8_t ouiType;
	uint8_t ouiSubtype;
	uint8_t version;
	uint8_t qosInfo;
	uint8_t reserved;
	WlanWmmAcParameter ac[4];
} WlanWmmParameterIe;

_Static_assert(sizeof(WlanWmmParameterIe) == 26, "WMM parameter element");

/*
 * The elements of a frame body by kind (Marvell's IEPointers_t, parser_rom.h,
 * with three more of this firmware's at the end), filled by the parser at
 * 0x3e8cc.
 */
typedef struct WlanIePointers {
	const uint8_t *pSsid;
	const uint8_t *pTim;
	const uint8_t *pWpa;
	const uint8_t *pWmmInfo;
	const uint8_t *pWmmParam;
	const uint8_t *pDsParam;
	const uint8_t *pSupportedRates;
	const uint8_t *pExtSupportedRates;
	uint8_t *pErpInfo;
	const uint8_t *pIbssParam;
	const uint8_t *pCountry;
	const uint8_t *pMdie;
	const uint8_t *pRsn;
	const WlanHtCapabilityIe *pHtCap;
	const uint8_t *pHtInfo;
	const uint8_t *p2040Coexist;
	const uint8_t *pHtScanParam;
	const uint8_t *pExtCap;
	const uint8_t *pWps;
	const uint8_t *pWapi;
	const uint8_t *pVendor;		/* the second vendor element kind (0x3e894) */
	const uint8_t *pQosCapability;
	const uint8_t *pEdcaParam;
} WlanIePointers;

_Static_assert(sizeof(WlanIePointers) == 92, "element pointers");

/*
 * The elements a station keeps of its access point's frames, filled by the
 * parser at 0x3e94a; the pointers share the names of WlanIePointers.
 */
typedef struct WlanBeaconIePointers {
	const uint8_t *pSsid;
	const uint8_t *pDsParam;
	const uint8_t *pCountry;
	const uint8_t *pApChannelReport;	/* element 51, set by the parser itself */
	const uint8_t *reserved4;
	const uint8_t *pSupportedRates;
	const uint8_t *pExtSupportedRates;
	const uint8_t *pWpa;
	const uint8_t *pWmmInfo;
	const uint8_t *pWmmParam;
	const uint8_t *reserved10;
	const uint8_t *pRsn;
	const uint8_t *pHtInfo;
	const WlanHtCapabilityIe *pHtCap;
	const uint8_t *p2040Coexist;
	const uint8_t *pHtScanParam;
	const uint8_t *pExtCap;
} WlanBeaconIePointers;

_Static_assert(sizeof(WlanBeaconIePointers) == 68, "beacon element pointers");

/* ERP Information element flags */
enum {
	WLAN_ERP_NON_ERP_PRESENT = 1 << 0,
	WLAN_ERP_USE_PROTECTION = 1 << 1,
	WLAN_ERP_BARKER_PREAMBLE = 1 << 2,
};

typedef void (*WlanMoveFn)(uint8_t *destination, const uint8_t *source, uint32_t length);
extern WlanMoveFn wlanIeMoveForward;
extern WlanMoveFn wlanIeMoveBackward;
extern uint16_t wlanIeLimitProbe;
extern uint16_t wlanIeLimitBeacon;
extern uint8_t *wlanBeaconTemplateEnd;
extern uint8_t *wlanProbeTemplateEnd;
int wlanIeInit(void);
int wlanChannelHooksInit(void);

uint8_t *wlanFindIe(WlanWirelessFrame *frame, int ieId, int frameKind,
		    const void *match, unsigned int matchLength);
void wlanSetIe(WlanWirelessFrame *frame, const uint8_t *ie, uint8_t *end,
	       uint8_t *removed, uint8_t *added);
int wlanPutErpIe(uint8_t *buffer, int nonErpPresent, int useProtection, int barker);
int wlanPutDsParameterIe(uint8_t *buffer, int channel);
unsigned int wlanHtRateBitmap(const WlanHtCapabilityIe *ht);
unsigned int wlanRateBitmapAll(const uint8_t *rates, unsigned int count,
			       const uint8_t *extendedRates, unsigned int extendedCount,
			       const WlanHtCapabilityIe *ht);
unsigned int wlanPutSsidIe(const void *ssid, uint8_t *buffer);
unsigned int wlanPutRatesIe(const uint8_t *rates, uint8_t *buffer, int extended);
unsigned int wlanInsertIeSorted(const uint8_t *ie, uint8_t *start, uint8_t *end);
unsigned int wlanCopyIes(int ieId, uint8_t *destination, int frameKind,
			 uint8_t *base, int ieOffset, const uint8_t *source,
			 unsigned int sourceLength);
unsigned int wlanAppendIe(WlanWirelessFrame *frame, uint8_t id, const void *data,
			  unsigned int length);
unsigned int wlanAppendIeList(WlanWirelessFrame *frame, int offset,
			      const uint8_t *ies, unsigned int length);
unsigned int wlanRemoveIe(const uint8_t *ie, uint8_t *start, uint8_t *end);
unsigned int wlanRemoveIeList(WlanWirelessFrame *frame, int offset,
			      const uint8_t *ies, unsigned int length);
int wlanClearBodyLength(uint16_t *length);
unsigned int wlanRateBitmapBasic(const uint8_t *rates, unsigned int count);
unsigned int wlanRateBitmap(const uint8_t *rates, unsigned int count);
int wlanTimeDelta(unsigned int now, unsigned int last);

/* The elements the host's TLVs became (HT Capabilities, HT Information, 20/40 BSS Coexistence, OBSS Scan Parameters). */
struct WlanHtTlvElements {
	uint8_t *capability;
	uint8_t *information;
	uint8_t *coexistence;
	uint8_t *obssScan;
	uint8_t *reserved16;		/* stored with the others by macMgmtConfigureEntry(); nothing reads it */
};

/* A growing run of elements in a pool buffer (wlan/iebuffer.c). */
struct WlanIeBuffer {
	OsaBuffer *buffer;
	uint16_t length;		/* bytes kept, from the buffer's data offset */
	uint16_t size;			/* bytes the buffer holds */
};

unsigned int wlanIeBufferAppend(struct WlanIeBuffer *ies, const void *data,
				int length);				/* 0x2a790 */
/* A host command TLV's header (Marvell's MrvlIEtypesHeader_t), little-endian and unaligned. */
struct __attribute__((packed)) MrvlIEtypesHeader {
	uint16_t type;
	uint16_t length;
};

unsigned int wlanIeBufferAppendTlv(struct WlanIeBuffer *ies, const uint8_t *tlv); /* 0x2a7d2 */
unsigned int wlanIeBufferGet(struct WlanIeBuffer *ies, uint8_t **data);	/* 0x2a83e */
void wlanIeBufferReset(struct WlanIeBuffer *ies);				/* 0x2a84a */
int wlanIeBufferFree(struct WlanIeBuffer *ies);				/* 0x2a850 */
int wlanIeBufferAlloc(struct WlanIeBuffer *ies);				/* 0x2a866 */
void wlanIeBufferInit(void);							/* 0x34c78: the buffers' pool */
int wlanParseIes(const uint8_t *ies, int length, WlanIePointers *out);	/* 0x3e8cc: the bytes left over; nonzero: the list was cut short */
int wlanPsBuildDescriptor(const uint8_t *oui);					/* 0x3e8b0: 4 for a Simple Configuration element */
int wlanPsParseDescriptor(const uint8_t *oui);					/* 0x3e894: 7 for a Wi-Fi Direct element */
int wlanPsQueryDescriptor(const uint8_t *ies, int length, WlanBeaconIePointers *out);	/* 0x3e94a */
struct WlanConnection;
unsigned int wlanPsResetDescriptor(struct WlanConnection *connection, const uint8_t *tim);	/* 0x3e98a */
void wlanPsSyncDescriptor(void);						/* 0x3e9d4 */
int wlanWmmElementType(const uint8_t *oui);					/* 0x2c21c */
int wlanWpaElementType(const uint8_t *oui);					/* 0x2c276 */
int wlanIePointersRecord(const uint8_t *ie, WlanIePointers *ies);		/* 0x2c294 */
int wlanBeaconIePointersRecord(const uint8_t *ie, WlanBeaconIePointers *ies);	/* 0x2c342 */
int wlanTimCheck(const uint8_t *tim, uint16_t aid, int *groupTraffic,
		 int *unicastTraffic);						/* 0x2c400 */
unsigned int wlanTlvChannelListParse(const uint8_t *tlv, uint8_t *channels,
				     uint32_t *channel);			/* 0x2c4c6 */

/* The TLV parsers of the host commands (wlan/hostcmdtlv.c). */
unsigned int wlanTlvCopyOfType(unsigned int type, const uint8_t *tlv, void *destination,
			       unsigned int maximum);				/* 0x2c480 */
unsigned int wlanTlvNumProbesParse(const uint8_t *tlv, uint8_t *numProbes);	/* 0x2c534 */
unsigned int wlanTlvTerminate(uint8_t *out);					/* 0x2c5a0 */
unsigned int wlanTlvParseThreshold(const uint8_t *tlv, uint8_t *threshold,
				   uint8_t *frequency);				/* 0x2c60e */
unsigned int wlanTlvCopyValue(const uint8_t *tlv, void *value);			/* 0x2c662 */

/* The first element with the id in ies[0..length) (0x2c3d8), or NULL. */
const uint8_t *wlanIeFind(int id, const uint8_t *ies, unsigned int length);

#endif /* ROBIN_IE_H */
