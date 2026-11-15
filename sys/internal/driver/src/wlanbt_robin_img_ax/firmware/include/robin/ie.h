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
	IE_POWER_CAPABILITY = 33,
	IE_ERP_INFORMATION = 42,
	IE_HT_CAPABILITY = 45,
	IE_RSN = 48,
	IE_EXTENDED_RATES = 50,
	IE_HT_INFORMATION = 61,
	IE_20_40_BSS_COEXISTENCE = 72,
	IE_OBSS_SCAN_PARAMETERS = 74,
	IE_VENDOR = 221,
};

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
	uint16_t capabilities;
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
void wlanAppendIe(WlanWirelessFrame *frame, uint8_t id, const void *data, unsigned int length);
unsigned int wlanAppendIeList(WlanWirelessFrame *frame, int offset,
			      const uint8_t *ies, unsigned int length);
unsigned int wlanRemoveIe(const uint8_t *ie, uint8_t *start, uint8_t *end);
unsigned int wlanRemoveIeList(WlanWirelessFrame *frame, int offset,
			      const uint8_t *ies, unsigned int length);
int wlanClearBodyLength(uint16_t *length);
unsigned int wlanRateBitmapBasic(const uint8_t *rates, unsigned int count);
unsigned int wlanRateBitmap(const uint8_t *rates, unsigned int count);
int wlanTimeDelta(unsigned int now, unsigned int last);

/* A growing run of elements in a pool buffer (wlan/iebuffer.c). */
struct WlanIeBuffer {
	OsaBuffer *buffer;
	uint16_t length;		/* bytes kept, from the buffer's data offset */
	uint16_t size;			/* bytes the buffer holds */
};

unsigned int wlanIeBufferAppend(struct WlanIeBuffer *ies, const void *data,
				int length);				/* 0x2a790 */
unsigned int wlanIeBufferAppendTlv(struct WlanIeBuffer *ies, const uint8_t *tlv); /* 0x2a7d2 */
unsigned int wlanIeBufferGet(struct WlanIeBuffer *ies, uint8_t **data);	/* 0x2a83e */
void wlanIeBufferReset(struct WlanIeBuffer *ies);				/* 0x2a84a */
int wlanIeBufferFree(struct WlanIeBuffer *ies);				/* 0x2a850 */
int wlanIeBufferAlloc(struct WlanIeBuffer *ies);				/* 0x2a866 */
void wlanIeBufferInit(void);							/* 0x34c78: the buffers' pool */
int cbProcRecordBuffer(int first, int argument);				/* 0x1e12a */
void wlanParseIes(const uint8_t *ies, int length, WlanIePointers *out);	/* 0x3e8cc */
int wpaSuppSelectDescriptor(const uint8_t *oui);				/* 0x2c21c */
int wpaSuppAdjustDescriptor(const uint8_t *oui);				/* 0x2c276 */
unsigned int wpaSuppConfigureQueue(const uint8_t *tlv, uint8_t *channels,
				   uint32_t *channel);				/* 0x2c4c6 */

/* The first element with the id in ies[0..length) (0x2c3d8), or NULL. */
const uint8_t *wlanIeFind(int id, const uint8_t *ies, unsigned int length);

#endif /* ROBIN_IE_H */
