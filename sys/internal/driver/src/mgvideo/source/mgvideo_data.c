/* Descriptive names: the original contains no source-file debug records. */
#include "mgvideo_private.h"
#include <psp2kern/kernel/proc_event.h>

const MgVideoProtectionInfoBox protectionInfoTemplate = {
	.header = { { 0, 0, 0, 116 }, { 's', 'i', 'n', 'f' } },
	.scheme = {
		.header = { { 0, 0, 0, 18 }, { 's', 'c', 'h', 'm' } },
		.schemeType = { 'p', 'r', 'o', 't' },
		.schemeVersion = { 1, 0 },
	},
	.information = {
		.header = { { 0, 0, 0, 90 }, { 's', 'c', 'h', 'i' } },
		.keySystem = {
			.header = { { 0, 0, 0, 36 }, { 'k', 's', 'i', 'd' } },
		},
		.key = {
			.header = { { 0, 0, 0, 14 }, { 'k', 'y', 'i', 'd' } },
		},
		.encryption = {
			.header = { { 0, 0, 0, 32 }, { 'e', 'a', 'l', 'g' } },
			.algorithm = { 0, 0, 0, 8 },
		},
	},
};

/* The keyring reader compares each eight-byte entry with this empty marker. */
const uint64_t emptyKeyringEntry = UINT64_MAX;

const char keyringLabel[13] = "KEYRING     ";

const MgVideoCipherBlock zeroVideoBlock = { { 0, 0, 0, 0 } };

const SceProcEventHandler videoProcessEventCallbacks = {
	.size = sizeof(SceProcEventHandler),
	.exit = onVideoProcessExit,
	.kill = onVideoProcessKill,
};

const char videoRecordMagic[5] = "MLST";
const uint16_t videoRecordStateMarker = 0x0923;

const MgVideoAvOut defaultVideoAvOut = { .fields = { .controlFlags = 1 } };

const MgVideoPolicyList videoPlaybackPolicies = { videoPlaybackPolicyLabels, 9 };
const MgVideoPolicyList videoPlaybackClockPolicies = { videoPlaybackClockPolicyLabels, 10 };

const MgVideoEncodedTime earliestVideoTime = {
	.yearHigh = 0x00, .yearLow = 0x01, .month = 0x01, .day = 0x01,
	.hour = 0x00, .minute = 0x00, .second = 0x00, .hundredth = 0x00,
};

const MgVideoEncodedTime latestVideoTime = {
	.yearHigh = 0x99, .yearLow = 0x99, .month = 0x12, .day = 0x31,
	.hour = 0x23, .minute = 0x59, .second = 0x59, .hundredth = 0x99,
};

const MgVideoPlaybackValue emptyVideoPlaybackValue = { .encodedValue = { 0 } };

const int videoPolicyRecordRequirement[4] = { 0, -2, 1, 1 };

const char *videoPlaybackClockPolicyLabels[10] = {
	"P_PlayAVOUT    ", "P_PlayAVOUT2   ", "P_PlayPeriod1  ",
	"P_PlayPeriod2  ", "T_ClockType1   ", "T_ClockType2   ",
	"T_MGR_HDD      ", "T_MagicGateV   ", "T_SVR          ",
	"V_SCEI_PSV     ",
};

const char *videoPlaybackRecordLabels[4] = {
	"!REFID      ", "PLAY_L      ", "PLAY_P      ", "PLAY_S      ",
};

const char *videoReferenceBindingRecordLabels[2] = { "!REFID      ", "BIND_L      " };

const char *videoProtectionRecordLabels[6] = {
	"!CID        ", "!REFID      ", "PLAY_L      ",
	"PLAY_P      ", "PLAY_S      ", 0,
};

const char *videoPlaybackPeriodRecordLabels[4] = {
	"!REFID      ", "PLAY_L      ", "PLAY_P      ", "PLAY_S      ",
};

const char *videoBindingRecordLabels[2] = { "!REFID      ", "BIND_L      " };

const char *videoPlaybackPolicyLabels[9] = {
	"P_PlayAVOUT    ", "P_PlayAVOUT2   ", "P_PlayPeriod1  ",
	"P_PlayPeriod2  ", "T_ClockType1   ", "T_MGR_HDD      ",
	"T_MagicGateV   ", "T_SVR          ", "V_SCEI_PSV     ",
};

MgVideoKeyringRecord keyringRecord;
int videoReferences;
uint64_t videoRandomChallenge;
MgVideoProcessReference processReferences[8];
int driverMutex;
int processEventHandler;
int contextHeaps[2];
MgVideoContext *videoContexts[2];
MgVideoRecordStream recordStreams[2];
