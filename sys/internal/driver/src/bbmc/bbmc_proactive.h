#ifndef BBMC_PROACTIVE_H
#define BBMC_PROACTIVE_H

#include <stddef.h>
#include <scetypes.h>

typedef struct BbmcProactiveText {
	SceUInt32 encoding;
	SceUInt8 length;
	SceUInt8 reservedLength[3];
	char text[241];
} BbmcProactiveText;

typedef struct BbmcProactiveIcon {
	SceUInt32 qualifier;
	SceUInt32 codingScheme;
	SceUInt8 height;
	SceUInt8 width;
	SceUInt8 recordNumber;
	SceUInt16 length;
	SceUInt8 reservedLength[2];
	SceUInt8 data[255];
} BbmcProactiveIcon;

typedef struct BbmcProactiveItem {
	SceUInt8 identifier;
	SceUInt8 length;
	char text[254];
} BbmcProactiveItem;

typedef struct BbmcProactiveItems {
	SceUInt8 count;
	SceUInt8 reservedCount[3];
	BbmcProactiveItem items[30];
} BbmcProactiveItems;

typedef struct BbmcProactiveNextActions {
	SceUInt8 count;
	SceUInt32 actions[8];
} BbmcProactiveNextActions;

typedef struct BbmcProactiveIcons {
	SceUInt32 qualifier;
	SceUInt8 count;
	BbmcProactiveIcon icons[30];
} BbmcProactiveIcons;

typedef struct BbmcProactiveDuration {
	SceUInt32 unit;
	SceUInt8 interval;
} BbmcProactiveDuration;

typedef struct BbmcProactiveTextCommand {
	BbmcProactiveText text;
	SceUInt32 textOption;
	SceUInt32 responseOption;
	BbmcProactiveIcon icon;
	BbmcProactiveDuration duration;
	SceUInt32 textResponseOption;
	BbmcProactiveText defaultText;
} BbmcProactiveTextCommand;

typedef struct BbmcProactiveInputCommand {
	BbmcProactiveText text;
	SceUInt32 responseFormat;
	SceUInt32 responseOption;
	SceUInt8 minimumLength;
	SceUInt8 maximumLength;
	SceUInt32 inputOption;
	SceUInt32 inputPresentation;
	BbmcProactiveIcon icon;
	BbmcProactiveText defaultText;
} BbmcProactiveInputCommand;

typedef struct BbmcProactiveSelectItem {
	SceUInt32 selectionOption;
	SceUInt32 softKeyOption;
	BbmcProactiveItems items;
	SceUInt32 presentationOption;
	BbmcProactiveText title;
	SceUInt8 defaultItem;
	BbmcProactiveNextActions nextActions;
	BbmcProactiveIcon icon;
	BbmcProactiveIcons itemIcons;
	BbmcProactiveText text;
} BbmcProactiveSelectItem;

typedef struct BbmcProactiveSetupMenu {
	SceUInt32 selectionOption;
	SceUInt32 softKeyOption;
	BbmcProactiveItems items;
	BbmcProactiveText title;
	SceUInt8 defaultItem;
	BbmcProactiveNextActions nextActions;
	BbmcProactiveIcon icon;
	BbmcProactiveIcons itemIcons;
} BbmcProactiveSetupMenu;

typedef struct BbmcProactiveAddress {
	SceUInt8 length;
	SceUInt8 reservedLength[3];
	SceUInt8 data[255];
} BbmcProactiveAddress;

typedef struct BbmcProactiveSmsPayload {
	SceUInt32 format;
	SceUInt32 packing;
	SceUInt8 length;
	SceUInt8 data[200];
} BbmcProactiveSmsPayload;

typedef struct BbmcProactiveSendSms {
	SceUInt32 smsOption;
	SceUInt32 format;
	BbmcProactiveAddress address;
	BbmcProactiveSmsPayload payload;
	BbmcProactiveText text;
	BbmcProactiveIcon icon;
} BbmcProactiveSendSms;

typedef struct BbmcProactiveIdleText {
	BbmcProactiveText text;
	BbmcProactiveIcon icon;
} BbmcProactiveIdleText;

typedef struct BbmcProactiveScalarOptions {
	SceUInt32 option;
	SceUInt16 value;
} BbmcProactiveScalarOptions;

typedef union BbmcProactiveBody {
	BbmcProactiveTextCommand text;
	BbmcProactiveInputCommand input;
	BbmcProactiveSelectItem selectItem;
	BbmcProactiveSetupMenu setupMenu;
	BbmcProactiveSendSms sendSms;
	BbmcProactiveIdleText idleText;
	BbmcProactiveScalarOptions scalar;
} BbmcProactiveBody;

typedef struct BbmcProactiveCmd {
	SceUInt32 command;
	SceUInt32 reference;
	SceUInt32 validFields;
	SceUInt8 qualifier;
	BbmcProactiveBody body;
} BbmcProactiveCmd;

_Static_assert(sizeof(BbmcProactiveText) == 252, "proactive text size");
_Static_assert(offsetof(BbmcProactiveText, text) == 8, "proactive text bytes");
_Static_assert(sizeof(BbmcProactiveIcon) == 272, "proactive icon size");
_Static_assert(offsetof(BbmcProactiveIcon, length) == 12, "proactive icon length");
_Static_assert(offsetof(BbmcProactiveIcon, data) == 16, "proactive icon data");
_Static_assert(sizeof(BbmcProactiveItem) == 256, "proactive item size");
_Static_assert(sizeof(BbmcProactiveItems) == 7684, "proactive item list size");
_Static_assert(sizeof(BbmcProactiveNextActions) == 36, "proactive next actions");
_Static_assert(sizeof(BbmcProactiveIcons) == 8168, "proactive icon list size");
_Static_assert(sizeof(BbmcProactiveCmd) == 16696, "proactive command size");
_Static_assert(offsetof(BbmcProactiveCmd, body) == 16, "proactive command body");
_Static_assert(offsetof(BbmcProactiveCmd, body.text.icon) == 276,
	"proactive text icon");
_Static_assert(offsetof(BbmcProactiveCmd, body.text.duration) == 548,
	"proactive text duration");
_Static_assert(offsetof(BbmcProactiveCmd, body.text.defaultText) == 560,
	"proactive default text");
_Static_assert(offsetof(BbmcProactiveCmd, body.input.minimumLength) == 276,
	"proactive minimum input length");
_Static_assert(offsetof(BbmcProactiveCmd, body.input.icon) == 288,
	"proactive input icon");
_Static_assert(offsetof(BbmcProactiveCmd, body.idleText.icon) == 268,
	"proactive idle text icon");
_Static_assert(offsetof(BbmcProactiveCmd, body.selectItem.items.items) == 28,
	"proactive menu items");
_Static_assert(offsetof(BbmcProactiveCmd, body.selectItem.title) == 7712,
	"proactive select title");
_Static_assert(offsetof(BbmcProactiveCmd, body.selectItem.nextActions) == 7968,
	"proactive select next actions");
_Static_assert(offsetof(BbmcProactiveCmd, body.selectItem.icon) == 8004,
	"proactive select icon");
_Static_assert(offsetof(BbmcProactiveCmd, body.selectItem.itemIcons.icons) == 8284,
	"proactive select item icons");
_Static_assert(offsetof(BbmcProactiveCmd, body.selectItem.text) == 16444,
	"proactive select text");
_Static_assert(offsetof(BbmcProactiveCmd, body.setupMenu.title) == 7708,
	"proactive setup title");
_Static_assert(offsetof(BbmcProactiveCmd, body.setupMenu.icon) == 8000,
	"proactive setup icon");
_Static_assert(offsetof(BbmcProactiveCmd, body.sendSms.payload) == 284,
	"proactive SMS payload");
_Static_assert(offsetof(BbmcProactiveCmd, body.sendSms.text) == 496,
	"proactive SMS text");
_Static_assert(offsetof(BbmcProactiveCmd, body.sendSms.icon) == 748,
	"proactive SMS icon");

#endif
