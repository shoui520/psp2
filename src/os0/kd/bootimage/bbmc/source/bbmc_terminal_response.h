#ifndef BBMC_TERMINAL_RESPONSE_H
#define BBMC_TERMINAL_RESPONSE_H

#include "qmi_backend.h"

typedef struct BbmcTerminalText {
	SceUInt32 dataCodingScheme;
	SceUInt8 length;
	SceUInt8 reserved[3];
	char text[244];
} BbmcTerminalText;

typedef struct BbmcGetInkeyExtraInfo {
	SceUInt32 units;
	SceUInt32 dataCodingScheme;
	SceUInt8 interval;
	SceUInt8 length;
	SceUInt8 reserved[2];
	char text[244];
} BbmcGetInkeyExtraInfo;

typedef struct BbmcTerminalResponse {
	SceUInt32 command;
	SceUInt32 referenceId;
	SceUInt32 generalResult;
	SceUInt8 commandNumber;
	SceUInt8 additionalInfoLength;
	SceUInt8 additionalInfo[10];
	SceUInt32 validFields;
	BbmcTerminalText text;
	BbmcGetInkeyExtraInfo getInkeyExtra;
	SceUInt32 userConfirmed;
	SceUInt32 iconIsDisplayed;
	SceUInt8 itemIdentifier;
	SceUInt16 language;
} BbmcTerminalResponse;

_Static_assert(sizeof(BbmcTerminalText) == 252, "terminal text size");
_Static_assert(offsetof(BbmcTerminalText, length) == 4, "terminal text length");
_Static_assert(offsetof(BbmcTerminalText, text) == 8, "terminal text bytes");
_Static_assert(sizeof(BbmcGetInkeyExtraInfo) == 256, "get inkey extra size");
_Static_assert(offsetof(BbmcGetInkeyExtraInfo, dataCodingScheme) == 4,
	"get inkey coding scheme");
_Static_assert(offsetof(BbmcGetInkeyExtraInfo, interval) == 8,
	"get inkey interval");
_Static_assert(offsetof(BbmcGetInkeyExtraInfo, length) == 9,
	"get inkey text length");
_Static_assert(offsetof(BbmcGetInkeyExtraInfo, text) == 12,
	"get inkey text bytes");
_Static_assert(sizeof(BbmcTerminalResponse) == 548, "terminal response size");
_Static_assert(offsetof(BbmcTerminalResponse, referenceId) == 4,
	"terminal response reference");
_Static_assert(offsetof(BbmcTerminalResponse, generalResult) == 8,
	"terminal response result");
_Static_assert(offsetof(BbmcTerminalResponse, commandNumber) == 12,
	"terminal response command number");
_Static_assert(offsetof(BbmcTerminalResponse, additionalInfoLength) == 13,
	"terminal response additional length");
_Static_assert(offsetof(BbmcTerminalResponse, additionalInfo) == 14,
	"terminal response additional bytes");
_Static_assert(offsetof(BbmcTerminalResponse, validFields) == 24,
	"terminal response valid fields");
_Static_assert(offsetof(BbmcTerminalResponse, text) == 28,
	"terminal response text");
_Static_assert(offsetof(BbmcTerminalResponse, getInkeyExtra) == 280,
	"terminal response get inkey");
_Static_assert(offsetof(BbmcTerminalResponse, userConfirmed) == 536,
	"terminal response confirmation");
_Static_assert(offsetof(BbmcTerminalResponse, iconIsDisplayed) == 540,
	"terminal response icon confirmation");
_Static_assert(offsetof(BbmcTerminalResponse, itemIdentifier) == 544,
	"terminal response item");
_Static_assert(offsetof(BbmcTerminalResponse, language) == 546,
	"terminal response language");

#endif
