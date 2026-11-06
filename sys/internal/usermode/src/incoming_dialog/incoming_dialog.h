/*
 * SceIncomingDialog's parameters (no SDK header has them: the library is the system's own). The
 * shell reads them whole; this module knows only the SDK version and where the text it sends
 * with them, for SDK 3.00 and later, starts. The names are ours.
 */
#ifndef INCOMING_DIALOG_INCOMING_DIALOG_H
#define INCOMING_DIALOG_INCOMING_DIALOG_H

#include <scetypes.h>

/* the parameters' only checked value: an argument the library does not take */
#define SCE_INCOMING_DIALOG_ERROR_INVALID_ARGUMENT	-2146409982	/* 0x80106202 */

/* from this SDK version on, the parameters end with a text the shell receives separately */
#define INCOMING_DIALOG_TEXT_SDK_VERSION	0x03000000

typedef struct SceIncomingDialogParam {
	SceUInt32 sdkVersion;
	SceUInt8 dialog[0x256];	/* the dialog the shell shows, read by the shell only */
	char text[2];		/* SDK 3.00 and later: sent as a string of its own */
} SceIncomingDialogParam;

_Static_assert(sizeof(SceIncomingDialogParam) == 0x25c, "SceIncomingDialogParam");

#endif
