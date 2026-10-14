#ifndef BBMC_QMI_UIM_CARD_STATUS_H
#define BBMC_QMI_UIM_CARD_STATUS_H

#include "qmi_backend.h"

typedef struct BbmcPinFlags {
	SceUInt8 blocked;
	SceUInt8 enabled;
	SceUInt8 verified;
	SceUInt8 reserved;
} BbmcPinFlags;

typedef struct QmiUimPinStatus {
	SceUInt8 state;
	SceUInt8 verifyRetries;
	SceUInt8 unblockRetries;
} QmiUimPinStatus;

typedef struct QmiUimApplicationTail {
	SceUInt8 upinReplacesPin1;
	QmiUimPinStatus pin1;
	QmiUimPinStatus pin2;
} QmiUimApplicationTail;

typedef struct QmiUimApplicationPrefix {
	SceUInt8 type;
	SceUInt8 state;
	SceUInt8 personalizationState;
	SceUInt8 personalizationFeature;
	SceUInt8 personalizationRetries;
	SceUInt8 personalizationUnblockRetries;
	SceUInt8 aidLength;
	SceUInt8 aid[];
} QmiUimApplicationPrefix;

typedef struct QmiUimCardPrefix {
	SceUInt8 state;
	SceUInt8 upinState;
	SceUInt8 upinRetries;
	SceUInt8 upukRetries;
	SceUInt8 error;
	SceUInt8 applicationCount;
	SceUInt8 applications[];
} QmiUimCardPrefix;

typedef struct QmiUimCardStatusPrefix {
	SceUInt8 indexGwPrimary[2];
	SceUInt8 index1xPrimary[2];
	SceUInt8 indexGwSecondary[2];
	SceUInt8 index1xSecondary[2];
	SceUInt8 cardCount;
	SceUInt8 cards[];
} QmiUimCardStatusPrefix;

_Static_assert(sizeof(BbmcPinFlags) == 4, "PIN flag word size");
_Static_assert(sizeof(QmiUimPinStatus) == 3, "UIM PIN status size");
_Static_assert(sizeof(QmiUimApplicationTail) == 7, "UIM application tail size");
_Static_assert(sizeof(QmiUimApplicationPrefix) == 7, "UIM application prefix size");
_Static_assert(sizeof(QmiUimCardPrefix) == 6, "UIM card prefix size");
_Static_assert(sizeof(QmiUimCardStatusPrefix) == 9, "UIM card status prefix size");

#endif
