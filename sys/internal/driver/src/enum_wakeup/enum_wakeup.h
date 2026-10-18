/*
 * SceEnumWakeUp: what the system runs when it is woken by USB enumeration
 * while off (charging over USB). It shows the device icon, lets the user
 * adjust the brightness and charges; a minimal MTP responder answers the
 * host's GetDeviceInfo / OpenSession / CloseSession and the battery level
 * and device icon properties so the device shows up. Pressing the power
 * button or unplugging powers the system off or on.
 */

#pragma once

#include <scetypes.h>
#include <stddef.h>

/* ===== display ===== */

#define EW_SCREEN_WIDTH		960
#define EW_SCREEN_HEIGHT	544
#define EW_FB_SIZE		0x1fe000	/* one 960x544x4 frame buffer, page aligned */

/* ===== MTP (PTP) codes ===== */

#define PTP_OC_GET_DEVICE_INFO		0x1001
#define PTP_OC_OPEN_SESSION		0x1002
#define PTP_OC_CLOSE_SESSION		0x1003
#define PTP_OC_GET_DEVICE_PROP_DESC	0x1014
#define PTP_OC_GET_DEVICE_PROP_VALUE	0x1015

#define PTP_RC_OK			0x2001
#define PTP_RC_GENERAL_ERROR		0x2002
#define PTP_RC_SESSION_NOT_OPEN		0x2003
#define PTP_RC_OPERATION_NOT_SUPPORTED	0x2005
#define PTP_RC_DEVICE_PROP_NOT_SUPPORTED 0x200a
#define PTP_RC_INVALID_PARAMETER	0x201d
#define PTP_RC_SESSION_ALREADY_OPEN	0x201e

#define PTP_DPC_BATTERY_LEVEL		0x5001
#define MTP_DPC_DEVICE_ICON		0xd405
#define MTP_DPC_SYNCHRONIZATION_PARTNER	0xd401
#define MTP_DPC_DEVICE_FRIENDLY_NAME	0xd402

/* datatype codes */
#define PTP_DTC_INT8			0x0001
#define PTP_DTC_UINT8			0x0002
#define PTP_DTC_INT16			0x0003
#define PTP_DTC_UINT16			0x0004
#define PTP_DTC_INT32			0x0005
#define PTP_DTC_UINT32			0x0006
#define PTP_DTC_INT64			0x0007
#define PTP_DTC_UINT64			0x0008
#define PTP_DTC_INT128			0x0009
#define PTP_DTC_UINT128			0x000a
#define PTP_DTC_AINT8			0x4001
#define PTP_DTC_AUINT8			0x4002
#define PTP_DTC_AINT16			0x4003
#define PTP_DTC_AUINT16			0x4004
#define PTP_DTC_AINT32			0x4005
#define PTP_DTC_AUINT32			0x4006
#define PTP_DTC_AINT64			0x4007
#define PTP_DTC_AUINT64			0x4008
#define PTP_DTC_STR			0xffff

/* DevicePropDesc form flags */
#define PTP_DPFF_NONE			0x00
#define PTP_DPFF_RANGE			0x01
#define PTP_DPFF_ENUMERATION		0x02
#define MTP_DPFF_FIXED_ARRAY		0x04
#define MTP_DPFF_REGULAR_EXPRESSION	0x05

/* SceMtpIfForDriver / responder errors (names unknown) */
#define MTP_ERROR_FAILED		0x80540001
#define MTP_ERROR_ALREADY		0x80540002
#define MTP_ERROR_INVALID_ARGUMENT	0x80540003
#define MTP_ERROR_NOT_STOPPED		0x80540004
#define MTP_ERROR_NO_MEMORY		0x80540007
#define MTP_ERROR_NOT_CONNECTED		0x80540008
#define MTP_ERROR_NOT_CACHED		0x80540011

/* a PTP container (request / response) */
typedef struct MtpContainer {
	SceUInt16 code;
	SceUInt16 padding;
	SceUInt32 sessionId;
	SceUInt32 transactionId;
	SceUInt32 params[5];
	SceUInt32 nparams;
} MtpContainer;

/* a counted array in a dataset */
typedef struct MtpArray {
	SceUInt32 count;
	const void *data;
} MtpArray;

/* the DeviceInfo dataset, as the responder keeps it */
typedef struct MtpDeviceInfo {
	SceUInt16 standardVersion;
	SceUInt32 vendorExtensionId;
	SceUInt16 vendorExtensionVersion;
	const char *vendorExtensionDesc;
	SceUInt16 functionalMode;
	MtpArray operations;			/* SceUInt16 */
	MtpArray events;			/* SceUInt16 */
	MtpArray deviceProperties;		/* SceUInt16 */
	MtpArray captureFormats;		/* SceUInt16 */
	MtpArray playbackFormats;		/* SceUInt16 */
	const char *manufacturer;
	const char *model;
	const char *deviceVersion;
	const char *serialNumber;
} MtpDeviceInfo;

/* a DevicePropDesc dataset; the values are 64 bits wide at most */
typedef struct MtpPropDesc {
	SceUInt16 code;
	SceUInt16 dataType;
	SceUInt8 getSet;
	SceUInt8 padding0[3];
	union {
		SceUInt8 u8;
		SceUInt16 u16;
		SceUInt32 u32;
		SceUInt32 u64[2];
		SceUInt32 u128[4];
		MtpArray array;
		const char *str;
	} defaultValue;
	SceUInt32 reserved0;
	SceUInt8 formFlag;			/* PTP_DPFF_* */
	SceUInt8 padding1[3];
	union {
		struct {
			SceUInt16 min, max, step;
		} range16;
		struct {
			SceUInt8 min, max, step;
		} range8;
		struct {
			SceUInt32 min, max, step;
		} range32;
		struct {
			SceUInt16 count;
			SceUInt16 padding;
			const void *values;
		} enumeration;
		const char *str;
	} form;
	SceUInt32 reserved1[3];
} MtpPropDesc;

/* the fixed head of the DeviceInfo dataset on the wire */
typedef struct MtpDeviceInfoHead {
	SceUInt16 standardVersion;
	SceUInt32 vendorExtensionId;
	SceUInt16 vendorExtensionVersion;
} __attribute__((packed)) MtpDeviceInfoHead;

/*
 * A DevicePropDesc dataset as it goes on the wire: packed little-endian
 * fields, the values sized by their datatype.
 */
typedef struct MtpDevicePropDescHead {
	SceUInt16 devicePropertyCode;
	SceUInt16 dataType;
	SceUInt8 getSet;
} __attribute__((packed)) MtpDevicePropDescHead;

/* dataset fields lie at any byte offset */
typedef SceUInt16 MtpUInt16 __attribute__((aligned(1)));
typedef SceUInt32 MtpUInt32 __attribute__((aligned(1)));

typedef union MtpWireValue {
	SceUInt8 u8;
	SceUInt16 u16;
	SceUInt32 u32;
	SceUInt32 u64[2];
	SceUInt32 u128[4];
} __attribute__((packed)) MtpWireValue;

typedef union MtpWireRange {
	struct {
		SceUInt8 min, max, step;
	} __attribute__((packed)) range8;
	struct {
		SceUInt16 min, max, step;
	} __attribute__((packed)) range16;
	struct {
		SceUInt32 min, max, step;
	} __attribute__((packed)) range32;
} __attribute__((packed)) MtpWireRange;

/* a counted array: the count, then the elements */
typedef struct MtpWireArray {
	SceUInt32 count;
	SceUInt8 elements[];
} MtpWireArray;

typedef struct MtpWireEnumeration {
	SceUInt16 count;
	SceUInt8 values[];			/* count values of the datatype */
} __attribute__((packed)) MtpWireEnumeration;

/* the device strings a provider fills */
typedef struct MtpProvidedStrings {
	char manufacturer[0x100];
	char model[0x100];
	char deviceVersion[0x100];
	char friendlyName[0x100];
	char syncPartner[0x100];
	char iconPath[0x100];
} MtpProvidedStrings;

/* the strings GetDeviceInfo reports */
typedef struct MtpDeviceStrings {
	char serialNumber[0x100];
	MtpProvidedStrings provided;
} MtpDeviceStrings;

typedef int (*MtpDeviceStringsProvider)(MtpProvidedStrings *strings, int reserved);

/* a file kept in memory for GetDevicePropValue (the icon) */
typedef struct MtpCacheEntry {
	char path[0x100];
	void *base;
	SceUID block;
	SceSize size;
} MtpCacheEntry;

#define MTP_CACHE_ENTRIES	16	/* a miss falls one past the end (Sony) */

/* request / data transfer helper thread */
typedef struct MtpXfer {
	void *buf;
	SceSize size;
	SceSize total;				/* the whole data phase */
	SceUInt32 reserved0;
	int result;
	SceUInt32 reserved1;
	SceUID evf;
	SceUID thread;
	struct MtpContext *ctx;
	SceUInt32 reserved2;
} MtpXfer;

typedef struct MtpContext {
	SceUInt32 flags;			/* MTP_CTX_* */
	int state;				/* MTP_STATE_* */
	int stateParam;			/* -1 with every state change */
	SceUInt32 sessionId;			/* 0: no session */
	const void *port;			/* SceMtpIfForDriver */
	MtpContainer request;
	MtpContainer response;
	SceUInt32 reserved0[7];
	MtpXfer *xfer;
	SceUInt32 reserved1;
	SceUInt32 sendObject[2];		/* cleared by anything but SendObject */
	SceUID sema;
	SceUID mutex;
	SceUID thread;
	MtpCacheEntry cache[MTP_CACHE_ENTRIES];
} MtpContext;

#define MTP_CTX_STOPPED		0x1000

#define MTP_STATE_NONE		0
#define MTP_STATE_STARTED	1
#define MTP_STATE_DISCONNECTED	2
#define MTP_STATE_BUSY		5

_Static_assert(sizeof(MtpContainer) == 0x24, "MTP container");
_Static_assert(offsetof(MtpContext, xfer) == 0x78, "MTP transfer");
_Static_assert(sizeof(MtpDeviceInfo) == 0x4c, "MTP DeviceInfo");
_Static_assert(sizeof(MtpPropDesc) == 0x38, "MTP DevicePropDesc");
_Static_assert(sizeof(MtpCacheEntry) == 0x10c, "MTP cache entry");
_Static_assert(offsetof(MtpContext, cache) == 0x94, "MTP cache");
_Static_assert(sizeof(MtpContext) == 0x1154, "MTP context");
