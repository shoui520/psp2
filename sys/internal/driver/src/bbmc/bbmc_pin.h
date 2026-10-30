#ifndef BBMC_PIN_H
#define BBMC_PIN_H

#include "qmi_backend.h"

typedef struct BbmcPinStatus {
	SceUInt32 pin1Status;
	SceUInt32 pin2Status;
	SceUInt8 pin1VerifyRetries;
	SceUInt8 pin1UnblockRetries;
	SceUInt8 pin2VerifyRetries;
	SceUInt8 pin2UnblockRetries;
} BbmcPinStatus;

typedef struct BbmcUnblockPinParam {
	char puk[36];
	char pin[12];
	SceUInt8 reserved;
	SceUInt8 pukLength;
	SceUInt8 pinLength;
	SceUInt8 reservedTail;
} BbmcUnblockPinParam;

/* name unknown: a control key, copied whole */
typedef struct BbmcControlKey {
	char digits[20];
} BbmcControlKey;

typedef struct BbmcControlKeyParam {
	SceUInt32 featureId;
	SceUInt8 controlKeyLength;
	SceUInt8 reserved[3];
	BbmcControlKey controlKey;
} BbmcControlKeyParam;

_Static_assert(sizeof(BbmcPinStatus) == 12, "PIN status size");
_Static_assert(offsetof(BbmcPinStatus, pin2Status) == 4, "PIN2 status");
_Static_assert(offsetof(BbmcPinStatus, pin1VerifyRetries) == 8,
	"PIN1 verification retries");
_Static_assert(offsetof(BbmcPinStatus, pin1UnblockRetries) == 9,
	"PIN1 unblock retries");
_Static_assert(offsetof(BbmcPinStatus, pin2VerifyRetries) == 10,
	"PIN2 verification retries");
_Static_assert(offsetof(BbmcPinStatus, pin2UnblockRetries) == 11,
	"PIN2 unblock retries");
_Static_assert(sizeof(BbmcUnblockPinParam) == 52, "unblock PIN parameter size");
_Static_assert(offsetof(BbmcUnblockPinParam, pin) == 36, "new PIN bytes");
_Static_assert(offsetof(BbmcUnblockPinParam, pukLength) == 49, "PUK length");
_Static_assert(offsetof(BbmcUnblockPinParam, pinLength) == 50, "new PIN length");
_Static_assert(sizeof(BbmcControlKeyParam) == 28, "control key parameter size");
_Static_assert(offsetof(BbmcControlKeyParam, controlKeyLength) == 4,
	"control key length");
_Static_assert(offsetof(BbmcControlKeyParam, controlKey) == 8, "control key bytes");

#endif
