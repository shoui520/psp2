/*
 * SceShell musiccore_plugin: main worker thread, command dispatch, and converters
 * (0x811CE626 - 0x811D2030)
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/new>
#include <kernel.h>
#include <thread/thread.h>
#include <plugin.h>
#include <system/system.h>
#include <inputdevice/inputdevice.h>

#include "musiccore_ctrl_client.h"
#include "musiccore_main_thread.h"

extern "C" {
extern int sceAppMgrGetStatusByName(const char *name, void *status);
extern int sceAppMgrGetStatusByAppId(SceUID appId, void *status);
extern int sceAppMgrGetAppIdByAppId(int target);
extern int sceBtAvrcpSetPlayStatus(int status);
extern int sceBtAvrcpSetTitle(const char *title, SceSize len);
extern int SceThreadmgrCoredumpTime_sceKernelExitThread(int status);
}

extern void *musicCoreHeapAlloc(SceSize size);			/* 0x811D3490 */
extern void musicCoreHeapFree(void *ptr);			/* 0x811D34AA */
extern SceSize musicCoreQueueGetSize(SceSize itemSz, int count, int mode); /* 0x811D3A26 */
extern void *musicCoreQueueCreate(void *mem, const char *name, SceSize itemSz,
	int count, int mode, int *outErr, SceSize bufSize);	/* 0x811D3A50 */
extern int musicCoreQueueSend(void *queue, int seqId, SceUInt32 cmd,
	const void *payload, SceSize payloadSize);		/* 0x811D3C80 */
extern int musicCoreQueueRecv(void *queue, void **outMsg, SceUInt *timeout); /* 0x811D499E */
extern int musicCoreQueueRelease(void *queue, void *msg);	/* 0x811D4AAA */

struct MusicCoreAppSlot {
	int seqId;						/* +0 */
	SceUInt8 focusState;					/* +4 */
	SceUInt8 slotId;					/* +5 */
	SceUInt8 reserved[2];					/* +6 */
	int appId;						/* +8 */
	char titleId[32];					/* +12 */
	SceUInt8 active;					/* +44 */
	SceUInt8 bgmAllowed;					/* +45 */
	SceUInt8 lastCtrl;					/* +46 */
	SceUInt8 padTail;					/* +47 */
};

struct MusicCoreErrorRing {
	SceUInt32 writeIdx;					/* +0 */
	SceUInt32 readIdx;					/* +4 */
	int errors[10];						/* +8 */
};

struct MusicCorePlayTimeEntry {
	int cmdId;						/* +0 */
	int errCode;						/* +4 */
	int extra;						/* +8 */
};

struct MusicCoreMetaDesc {
	void *artistBuf;
	SceSize artistSize;
	void *titleBuf;
	SceSize titleSize;
	void *albumBuf;
	SceSize albumSize;
};

struct MusicCoreRawSnap {
	SceUInt8 channelId;					/* +0 */
	SceInt8 playerKind;					/* +1 */
	SceInt8 bgmStatus;					/* +2 */
	SceInt8 bgmReason;					/* +3 */
	int bgmPriority;					/* +4 */
	SceUInt8 playableFlag;					/* +8 */
	SceUInt8 padPlayable[3];				/* +9 */
	int seqNum;						/* +12 */
	SceInt16 notifyFlags;					/* +16 */
	SceInt16 padNotify;					/* +18 */
	int cmdParam;						/* +20 */
	int errCode;						/* +24 */
	int playState;						/* +28 */
	int contentType;					/* +32 */
	int padContent;						/* +36 */
	SceInt64 trackId;					/* +40 */
	SceInt64 trackItemId;					/* +48 */
	SceInt64 playlistId;					/* +56 */
	int trackIndex;						/* +64 */
	int totalTracks;					/* +68 */
	int currentTrackNum;					/* +72 */
	int durationMs;						/* +76 */
	int sampleRate;						/* +80 */
	int bitRate;						/* +84 */
	int channels;						/* +88 */
	int codecType;						/* +92 */
	int sourceType;						/* +96 */
	int repeatMode;						/* +100 */
	int shuffleMode;					/* +104 */
	int volumeMode;						/* +108 */
	int audioBalance;					/* +112 */
	int eqType;						/* +116 */
	int eqBandLow;						/* +120 */
	int eqBandMid;						/* +124 */
	int eqBandHigh;						/* +128 */
	int audioNormalizer;					/* +132 */
	int extraErr;						/* +136 */
	int padTail;						/* +140 */
};

struct MusicCoreTrackInfoRaw {
	char uri[0x804];					/* +0 */
	int listIndex;						/* +0x804 */
	int trackIndex;						/* +0x808 */
	int padTrackId;						/* +0x80C */
	SceInt64 trackId;					/* +0x810 */
	int shuffleFlag;					/* +0x818 */
	int contentKind;					/* +0x81C */
	int extraMeta;						/* +0x820 */
	int padTail;						/* +0x824 */
};

struct MusicCoreConvertedStatus {
	int channelType;					/* +0 */
	int cmdParam;						/* +4 */
	int errCode;						/* +8 */
	int reservedHead[2];					/* +12..+19 */
	int hasRingError;					/* +20 */
	int reservedMid[2];					/* +24..+31 */
	int playMode;						/* +32 */
	int bgmReason;						/* +36 */
	int bgmPriority;					/* +40 */
	int playableFlag;					/* +44 */
	SceInt64 trackId;					/* +48 */
	int trackIndex;						/* +56 */
	int playTime;						/* +60 */
	int durationMs;						/* +64 */
	int sampleRate;						/* +68 */
	int bitRate;						/* +72 */
	int channels;						/* +76 */
	int codecType;						/* +80 */
	int sourceType;						/* +84 */
	int currentTrackNum;					/* +88 */
	int totalTracks;					/* +92 */
	int audioBalance;					/* +96 */
	int shuffleMode;					/* +100 */
	int repeatMode;						/* +104 */
	int audioNormalizer;					/* +108 */
	int eqType;						/* +112 */
	int volumeMode;						/* +116 */
	SceInt64 trackItemId;					/* +120 */
	SceInt64 playlistId;					/* +128 */
	int capActive;						/* +136 */
	int seqNum;						/* +140 */
};

struct MusicCoreTimeCbData {
	SceUInt8 slotId;					/* +0 */
	SceInt8 channelType;					/* +1 */
	SceUInt8 padHeader[2];					/* +2 */
	int timeMs;						/* +4 */
	SceInt64 trackId;					/* +8 */
};

struct ImposeNoticeInterface {
	int (*showNotice)(const void *notice);
	int (*hideNotice)(const void *notice);
	int (*getTexture)(const paf::IDParam *iconId);
};

struct AppFocusChangePayload {
	int eventType;
	char titleId[32];
};

struct ExtraCbHeader {
	SceUInt8 slotId;
	SceInt8 channelType;
	SceUInt8 detailBytes[6];
};

static MusicCoreNotifyCb s_notifyCb = NULL;			/* 0x8156B938 */
static SceUInt8 s_capData[72];					/* 0x8156B93C */
static MusicCoreRawSnap s_rawStatusCh0;				/* 0x8156B988 */
static MusicCoreRawSnap s_rawStatusCh1;				/* 0x8156BA18 */
static MusicCoreRawSnap s_rawStatusCh2;				/* 0x8156BAA8 */
static int s_playTimeCh1 = 0;					/* 0x8156BB38 */
static int s_playTimeCh2 = 0;					/* 0x8156BB3C */
static SceUInt8 s_extraState[3][8];				/* 0x8156BB40..BB57 */
static SceUInt8 s_playTimeCh0Slot = 0;				/* 0x8156BB58 */
static int s_playTimeCh0Val = 0;				/* 0x8156BB5C */
static MusicCoreTrackInfoRaw s_trackInfoBuf;			/* 0x8156BB68 */
static SceUInt8 s_exOpenParam[20];				/* 0x8156C390 */
static char s_uriCh1[0x401];					/* 0x8156C3A4 */
static char s_uriCh0[0x401];					/* 0x8156C7A5 */
static char s_uriCh2[0x401];					/* 0x8156CBA6 */
static MusicCoreErrorRing s_errRingCh1;				/* 0x8156CFA8 */
static MusicCoreErrorRing s_errRingCh0[8];			/* 0x8156CFD8 */
static MusicCoreErrorRing s_errRingCh2;				/* 0x8156D158 */
static MusicCoreMetaDesc s_metaDesc;				/* 0x8156D188 */
static char s_metaArtist[2048];					/* 0x8156D1A0 */
static char s_metaTitle[2048];					/* 0x8156D9A0 */
static char s_metaAlbum[2048];					/* 0x8156E1A0 */
static MusicCorePlayTimeEntry s_ptCh1;				/* 0x8156E9A0 */
static MusicCorePlayTimeEntry s_ptCh1Prev;			/* 0x8156E9AC */
static MusicCorePlayTimeEntry s_ptCh0[8];			/* 0x8156E9B8 */
static MusicCorePlayTimeEntry s_ptCh2;				/* 0x8156EA18 */
static MusicCoreAppSlot s_bgmSlot;				/* 0x8156EA24 */
static MusicCoreAppSlot s_appSlots[8];				/* 0x8156EA54 */
static SceUInt8 s_exSlotId = 0;					/* 0x8156EBD9 */
static int s_exAppId = -1;					/* 0x8156EBDC */
static int s_avrcpLastPlayStatus = -1;				/* 0x8156EC04 */
static int s_avrcpLastTrackMode = -1;				/* 0x8156EC08 */
static int s_avrcpPlayStatus = 0;				/* 0x8156EC0C */
static char s_curAppTitle[32];					/* 0x8156EC10 */
static char s_prevAppTitle[32];					/* 0x8156EC30 */
static char s_curParentTitle[32];				/* 0x8156EC50 */
static char s_prevParentTitle[32];				/* 0x8156EC70 */
static SceUInt8 s_musicAppForeground = 0;			/* 0x8156EC90 */
static SceUInt8 s_errorRingSuppressed = 0;			/* 0x8156EC91 */
static SceUInt8 s_sysPauseFlag = 0;				/* 0x8156EC92 */
static SceUInt8 s_powerPauseFlag = 0;				/* 0x8156EC93 */
static SceUInt8 s_btrmOwned = 0;				/* 0x8156EC94 */
static SceUInt8 s_titleDirty = 0;				/* 0x8156EC95 */

/* Forward declarations for internal callbacks */
int musicCoreSetImposePausedNotice(int mode, const char *text, SceUInt8 show); /* 0x811D1740 */
static int musicCoreOnCapCb(const void *cap);			/* 0x811D16FE */
static int musicCoreOnStatusCb(const void *raw, SceUInt16 eventId, SceUInt32 cmd, int err); /* 0x811D18B0 */
static int musicCoreOnExtraCb(const void *data);		/* 0x811D1E06 */
static int musicCoreOnTimeCb(const void *data);			/* 0x811D1E82 */
static int musicCoreOnMediaCb(int kind, const void *rawStatus, const void *data); /* 0x811D1F18 */

/* 0x811CE626 */
MusicCoreMain::MusicCoreMain(int *outErr)
	: m_threadId(-1), m_semaId(-1), m_initResult((int)0x8FFF0002),
	  m_initAlignPad(0), m_state((int)0x8FFF0002), m_stateAlignPad(0),
	  m_mutex("SceShellMusicCorePluginMainAckWait", 0),
	  m_cond(m_mutex, "SceShellMusicCorePluginMainAckWait", 0),
	  m_ackVtbl(NULL), m_ackCond(&m_cond), m_ackMutex(&m_mutex),
	  m_ackState(0), m_queue(NULL), m_initialized(0), m_stopping(0)
{
	s_notifyCb = NULL;
	*outErr = (int)0x8FFF0002;
	s_rawStatusCh1.trackId = -1;

	SceUID sema = sceKernelCreateSema("SceShellMusicCorePluginMainWait", 0, 0, 1, NULL);
	if (sema < 0) {
		*outErr = sema;
		return;
	}
	m_semaId = sema;

	SceSize qSize = musicCoreQueueGetSize(2304, 4, 1);
	void *qMem = musicCoreHeapAlloc(qSize);
	if (qMem == NULL) {
		*outErr = (int)0x8FFF0004;
		return;
	}
	void *q = musicCoreQueueCreate(qMem, "SceShellMusicCorePluginMainThr", 2304, 4, 1, outErr, qSize);
	if (q == NULL) {
		*outErr = (int)0x8FFF0004;
		return;
	}
	if (*outErr != 0)
		return;
	m_queue = q;

	SceUID tid = sceKernelCreateThread("SceShellMusicCorePluginMain",
		(SceKernelThreadEntry)ThreadEntry, 0x100000E6, 0x4000, 0, 0, NULL);
	if (tid < 0) {
		*outErr = tid;
		return;
	}
	m_threadId = tid;
	MusicCoreMain *self = this;
	int st = sceKernelStartThread(tid, 4, &self);
	if (st != 0) {
		*outErr = st;
		return;
	}
	if (*outErr == 0) {
		int wr = sceKernelWaitSema(m_semaId, 1, NULL);
		*outErr = (wr >= 0) ? m_initResult : wr;
	}
}

/* 0x811CE796 */
int MusicCoreMain::SendCmd(SceUInt32 cmdTag, int seqId, SceUInt32 packedCmd,
	const void *payload, SceSize payloadSize)
{
	(void)cmdTag;
	if (m_threadId == -1 || m_queue == NULL)
		return (int)0x8FFF0002;
	if (payloadSize != 0 && payload == NULL)
		return (int)0x8FFF0005;
	int ret = musicCoreQueueSend(m_queue, seqId, packedCmd, payload, payloadSize);
	return (ret == 0) ? (int)0x8FFF0002 : ret;
}

/* 0x811CE882 */
MusicCoreMain::~MusicCoreMain()
{
	if (m_threadId != -1) {
		SceUInt8 exitPayload[20];
		SendCmd(0, -1, 0x80008000u, exitPayload, sizeof(exitPayload));
		if (sceKernelWaitThreadEnd(m_threadId, NULL, NULL) >= 0)
			sceKernelDeleteThread(m_threadId);
		m_threadId = -1;
	}
	if (m_queue != NULL) {
		typedef void (*DtorFn)(void *);
		(*(DtorFn **)(*(void **)m_queue))[1](m_queue);
		void *q = m_queue;
		m_queue = NULL;
		musicCoreHeapFree(q);
	}
	s_notifyCb = NULL;
	sceKernelDeleteSema(m_semaId);
}

/* 0x811CE940 */
int MusicCoreMain::InitController()
{
	sce_paf_memset(&s_errRingCh1, 0, sizeof(s_errRingCh1));
	sce_paf_memset(s_errRingCh0, 0, sizeof(s_errRingCh0));
	sce_paf_memset(&s_errRingCh2, 0, sizeof(s_errRingCh2));
	sce_paf_memset(&s_bgmSlot, 0, sizeof(s_bgmSlot));
	sce_paf_memset(s_appSlots, 0, sizeof(s_appSlots));
	s_bgmSlot.appId = -1;
	for (int i = 0; i < 8; ++i) {
		s_appSlots[i].appId = -1;
		s_appSlots[i].lastCtrl = 1;
	}

	sce_paf_memset(&s_ptCh1, 0, sizeof(s_ptCh1));
	sce_paf_memset(&s_ptCh1Prev, 0, sizeof(s_ptCh1Prev));
	sce_paf_memset(s_ptCh0, 0, sizeof(s_ptCh0));
	sce_paf_memset(&s_ptCh2, 0, sizeof(s_ptCh2));
	s_avrcpLastPlayStatus = -1;
	s_avrcpLastTrackMode = -1;
	s_musicAppForeground = 0;
	s_errorRingSuppressed = 0;
	s_sysPauseFlag = 0;
	s_powerPauseFlag = 0;
	s_btrmOwned = 0;
	s_titleDirty = 0;
	sce_paf_memset(&s_trackInfoBuf, 0, sizeof(s_trackInfoBuf));
	s_trackInfoBuf.shuffleFlag = -1;
	s_trackInfoBuf.contentKind = -1;
	sce_paf_memset(s_curAppTitle, 0, sizeof(s_curAppTitle));
	sce_paf_memset(s_prevAppTitle, 0, sizeof(s_prevAppTitle));
	sce_paf_strncpy(s_curAppTitle, "NPXS19999", sce_paf_strlen("NPXS19999"));
	sce_paf_strncpy(s_prevAppTitle, "NPXS19999", sce_paf_strlen("NPXS19999"));

	int ret = sce::MusicCore::ctrlInit();
	if (ret != 0) return ret;
	ret = sce::MusicCore::ctrlSetIdleCallback(0, (void *)musicCoreSetImposePausedNotice);
	if (ret != 0) return ret;
	ret = sce::MusicCore::ctrlSetCapCallback(0, (void *)musicCoreOnCapCb);
	if (ret != 0) return ret;
	ret = sce::MusicCore::ctrlSetStatusCallback(0, (void *)musicCoreOnStatusCb);
	if (ret != 0) return ret;
	ret = sce::MusicCore::ctrlSetExtraCallback(0, (void *)musicCoreOnExtraCb);
	if (ret != 0) return ret;
	ret = sce::MusicCore::ctrlSetMediaCallback(0, (void *)musicCoreOnMediaCb);
	if (ret != 0) return ret;
	ret = sce::MusicCore::ctrlSetTimeCallback(0, (void *)musicCoreOnTimeCb, 90000);
	if (ret != 0) return ret;

	m_initialized = 1;
	SceUInt8 slot = 0xFF;
	return sce::MusicCore::ctrlOpenSimple(0, 1, 128, &slot);
}

/* 0x811CEB88 */
int MusicCoreMain::SetCallback(MusicCoreNotifyCb cb)
{
	if (s_notifyCb == NULL)
		s_notifyCb = cb;
	return 0;
}

/* 0x811CEB9A */
int MusicCoreMain::ClearCallback(MusicCoreNotifyCb cb)
{
	(void)cb;
	s_notifyCb = NULL;
	return 0;
}

/* 0x811CEBA8 */
int MusicCoreMain::ParseMsgHeader(const MusicCoreQueueMsg *msg, int *outSeq, SceUInt8 *outCh)
{
	*outSeq = msg->seqId;
	if (msg->channel == 0) {
		*outCh = 1;
		return 0;
	}
	if (msg->channel == 1) {
		*outCh = 0;
		return 0;
	}
	if (msg->channel == 2) {
		*outCh = 2;
		return 0;
	}
	return (int)0x80100D01;
}

/* 0x811CEC06 */
int MusicCoreMain::ResolveSlot(SceUInt32 chType, int appId, SceUInt8 *outSlot)
{
	if (chType == 1) {
		*outSlot = 1;
		return 0;
	}
	if (chType == 2) {
		*outSlot = s_exSlotId;
		return 0;
	}
	if (chType != 0)
		return (int)0x80100D01;
	for (int i = 0; i < 8; ++i) {
		if (s_appSlots[i].appId == appId) {
			*outSlot = s_appSlots[i].slotId;
			return (i < 7) ? 0 : (int)0x80100D01;
		}
	}
	return (int)0x80100D01;
}

/* 0x811CEC72 */
int MusicCoreMain::SetSlotBgmAllowed(SceUInt32 chType, int appId, SceUInt8 allowed)
{
	if (chType == 1 || chType == 2)
		return 0;
	if (chType != 0)
		return (int)0x80100D01;
	for (int i = 0; i < 8; ++i) {
		if (s_appSlots[i].appId == appId) {
			s_appSlots[i].bgmAllowed = allowed;
			return (i < 7) ? 0 : (int)0x80100D01;
		}
	}
	return (int)0x80100D01;
}

/* 0x811CECCA */
int MusicCoreMain::SetSlotLastCtrl(SceUInt32 chType, int appId, SceUInt8 ctrl)
{
	if (chType == 1 || chType == 2)
		return 0;
	if (chType != 0)
		return (int)0x80100D01;
	for (int i = 0; i < 8; ++i) {
		if (s_appSlots[i].appId == appId) {
			s_appSlots[i].lastCtrl = ctrl;
			return (i < 7) ? 0 : (int)0x80100D01;
		}
	}
	return (int)0x80100D01;
}

/* 0x811CED22 */
int MusicCoreMain::CheckSlotPaused(SceUInt32 chType, int appId, SceUInt8 *outState)
{
	*outState = 18;
	if (chType == 1 || chType == 2)
		return 0;
	if (chType != 0)
		return (int)0x80100D01;
	for (int i = 0; i < 8; ++i) {
		if (s_appSlots[i].appId == appId) {
			if (s_appSlots[i].bgmAllowed == 0) {
				SceUInt8 lc = s_appSlots[i].lastCtrl;
				if (lc == 0 || (lc >= 11 && lc <= 16)) {
					*outState = (s_sysPauseFlag != 0 || s_powerPauseFlag != 0) ? 1 : 0;
				}
			}
			return (i < 7) ? 0 : (int)0x80100D01;
		}
	}
	return (int)0x80100D01;
}

/* 0x811CEDC4 */
int MusicCoreMain::HandleSetContent(const MusicCoreQueueMsg *msg)
{
	int seqId = 0;
	SceUInt8 ch = 0;
	int ret = ParseMsgHeader(msg, &seqId, &ch);
	if (ret != 0)
		return ret;

	struct SetContentMsg {
		char uri[0x804];
		int listIndex;
		int trackIndex;
		int padTrackId;
		SceInt64 trackId;
		SceUInt32 shuffleMode;
		SceUInt32 contentKind;
		SceUInt32 extraMeta;
		int padMeta;
		int pid;
		int appId;
	} contentMsg;
	sce_paf_memset(&contentMsg, 0, sizeof(contentMsg));
	sce_paf_memcpy(&contentMsg, msg->payload, msg->payloadSize);

	sce::MusicCore::TrackContentDesc trackDesc;
	sce_paf_memset(&trackDesc, 0, sizeof(trackDesc));
	trackDesc.trackId = contentMsg.trackId;
	trackDesc.trackIndex = contentMsg.trackIndex;
	s_ptCh1.extra = 117;
	sce_paf_strncpy(trackDesc.uri, contentMsg.uri, 0x800u);
	trackDesc.uri[0x800] = '\0';
	trackDesc.listIndex = contentMsg.listIndex;

	if (contentMsg.shuffleMode <= 1u) {
		trackDesc.shuffleFlag = (contentMsg.shuffleMode != 0u) ? 1 : 0;
	} else if (contentMsg.shuffleMode == 2u) {
		trackDesc.shuffleFlag = -1;
	} else {
		return (int)0x80100D01;
	}

	if (contentMsg.contentKind == 0u) {
		trackDesc.contentKind = 1;
	} else if (contentMsg.contentKind == 2u) {
		trackDesc.contentKind = 2;
	} else if (contentMsg.contentKind == 18u) {
		trackDesc.contentKind = -1;
	} else {
		return (int)0x80100D01;
	}

	if (contentMsg.extraMeta <= 1u) {
		trackDesc.extraMeta = (contentMsg.extraMeta != 0u) ? 1 : 0;
	} else if (contentMsg.extraMeta == 2u) {
		trackDesc.extraMeta = -1;
	} else {
		return (int)0x80100D01;
	}

	SceUInt8 slot = 0;
	ret = ResolveSlot(ch, contentMsg.appId, &slot);
	if (ret == 0)
		return sce::MusicCore::ctrlSetUriData((uint32_t)seqId, slot, &trackDesc);
	return ret;
}

/* 0x811CEF6C */
int MusicCoreMain::HandleControl(const MusicCoreQueueMsg *msg)
{
	sce::MusicCore::ControlParams ctrlParams;
	ctrlParams.seekTarget = -1;
	ctrlParams.speedValue = (int)0x80000000u;
	ctrlParams.trackIndex = -2;
	ctrlParams.seekMode = -2;
	ctrlParams.seekPositionMs = -2;

	int seqId = 0;
	SceUInt8 ch = 0;
	int ret = ParseMsgHeader(msg, &seqId, &ch);
	if (ret != 0)
		return ret;

	int payload[5];
	sce_paf_memset(payload, 0, sizeof(payload));
	sce_paf_memcpy(payload, msg->payload, msg->payloadSize);

	int appId = payload[4];
	ret = SetSlotLastCtrl(ch, appId, (SceUInt8)payload[0]);
	if (ret != 0)
		return ret;

	SceUInt8 paused = 0;
	CheckSlotPaused(ch, appId, &paused);
	SceUInt8 slot = 0;
	ret = ResolveSlot(ch, appId, &slot);
	if (ret != 0)
		return ret;

	if (seqId == 156) {
		if (s_musicAppForeground != 1)
			return 0;
		if (s_capData[32] == 0 && payload[0] != 17) {
			paf::Plugin *impose = paf::Plugin::Find("impose_plugin");
			if (impose != NULL) {
				ImposeNoticeInterface *iface = (ImposeNoticeInterface *)impose->GetInterface(1);
				if (iface != NULL) {
					SceUInt32 notice[16];
					sce_paf_memset(notice, 0, sizeof(notice));
					paf::IDParam iconId(0xA8CC07F6u);
					notice[11] = (SceUInt32)iface->getTexture(&iconId);
					notice[7] = 639083309u;
					iface->showNotice(notice);
				}
			}
			return 0;
		}
		if ((SceUInt64)s_rawStatusCh1.trackId >= 0xFFFFFFFFFFFFFFFEULL)
			return 0;
	}

	int fadeMs = payload[1];
	switch (payload[0]) {
	case 0:
		ctrlParams.speedValue = 1;
		ret = sce::MusicCore::ctrlControl((uint32_t)seqId, slot, &ctrlParams);
		if (ret == 0) {
			ret = sce::MusicCore::ctrlPlay((uint32_t)seqId, slot, fadeMs);
			if (paused == 1)
				ret = sce::MusicCore::ctrlPause((uint32_t)seqId, slot, 1, 0);
		}
		break;
	case 1:
		ret = sce::MusicCore::ctrlPause((uint32_t)seqId, slot, 1, fadeMs);
		break;
	case 2:
		ret = sce::MusicCore::ctrlPause((uint32_t)seqId, slot, 0, 0);
		break;
	case 3:
		ctrlParams.seekTarget = -3;
		ctrlParams.seekPositionMs = -1;
		ret = sce::MusicCore::ctrlControl((uint32_t)seqId, slot, &ctrlParams);
		break;
	case 4:
		if (seqId == 156 && (unsigned int)s_playTimeCh1 > 1000u) {
			ctrlParams.seekPositionMs = 0;
		} else {
			ctrlParams.seekTarget = -4;
			ctrlParams.seekPositionMs = -1;
		}
		ret = sce::MusicCore::ctrlControl((uint32_t)seqId, slot, &ctrlParams);
		break;
	case 5:
		ctrlParams.speedValue = 10;
		ret = sce::MusicCore::ctrlControl((uint32_t)seqId, slot, &ctrlParams);
		break;
	case 6:
		ctrlParams.speedValue = 30;
		ret = sce::MusicCore::ctrlControl((uint32_t)seqId, slot, &ctrlParams);
		break;
	case 7:
		ctrlParams.speedValue = 100;
		ret = sce::MusicCore::ctrlControl((uint32_t)seqId, slot, &ctrlParams);
		break;
	case 8:
		ctrlParams.speedValue = -10;
		ret = sce::MusicCore::ctrlControl((uint32_t)seqId, slot, &ctrlParams);
		break;
	case 9:
		ctrlParams.speedValue = -30;
		ret = sce::MusicCore::ctrlControl((uint32_t)seqId, slot, &ctrlParams);
		break;
	case 10:
		ctrlParams.speedValue = -100;
		ret = sce::MusicCore::ctrlControl((uint32_t)seqId, slot, &ctrlParams);
		break;
	case 11:
		ctrlParams.speedValue = 10;
		ret = sce::MusicCore::ctrlSeekAndPlay((uint32_t)seqId, slot, &ctrlParams, fadeMs);
		if (paused == 1)
			ret = sce::MusicCore::ctrlPause((uint32_t)seqId, slot, 1, 0);
		break;
	case 12:
		ctrlParams.speedValue = 30;
		ret = sce::MusicCore::ctrlSeekAndPlay((uint32_t)seqId, slot, &ctrlParams, fadeMs);
		if (paused == 1)
			ret = sce::MusicCore::ctrlPause((uint32_t)seqId, slot, 1, 0);
		break;
	case 13:
		ctrlParams.speedValue = 100;
		ret = sce::MusicCore::ctrlSeekAndPlay((uint32_t)seqId, slot, &ctrlParams, fadeMs);
		if (paused == 1)
			ret = sce::MusicCore::ctrlPause((uint32_t)seqId, slot, 1, 0);
		break;
	case 14:
		ctrlParams.speedValue = -10;
		ret = sce::MusicCore::ctrlSeekAndPlay((uint32_t)seqId, slot, &ctrlParams, fadeMs);
		if (paused == 1)
			ret = sce::MusicCore::ctrlPause((uint32_t)seqId, slot, 1, 0);
		break;
	case 15:
		ctrlParams.speedValue = -30;
		ret = sce::MusicCore::ctrlSeekAndPlay((uint32_t)seqId, slot, &ctrlParams, fadeMs);
		if (paused == 1)
			ret = sce::MusicCore::ctrlPause((uint32_t)seqId, slot, 1, 0);
		break;
	case 16:
		ctrlParams.speedValue = -100;
		ret = sce::MusicCore::ctrlSeekAndPlay((uint32_t)seqId, slot, &ctrlParams, fadeMs);
		if (paused == 1)
			ret = sce::MusicCore::ctrlPause((uint32_t)seqId, slot, 1, 0);
		break;
	case 17:
		ret = sce::MusicCore::ctrlStop((uint32_t)seqId, slot);
		break;
	default:
		return (int)0x80100D01;
	}

	if (s_capData[9] != 0)
		ret = 0;
	return ret;
}

/* 0x811CF388 */
int MusicCoreMain::HandleSetRepeat(const MusicCoreQueueMsg *msg)
{
	int seqId = 0;
	SceUInt8 ch = 0;
	int ret = ParseMsgHeader(msg, &seqId, &ch);
	if (ret != 0)
		return ret;
	SceUInt32 payload[5];
	sce_paf_memset(payload, 0, sizeof(payload));
	sce_paf_memcpy(payload, msg->payload, msg->payloadSize);
	int mappedRepeat = 0;
	if (payload[0] == 0u)
		mappedRepeat = 1;
	else if (payload[0] == 1u)
		mappedRepeat = 2;
	else if (payload[0] == 2u)
		mappedRepeat = 0;
	else
		return (int)0x80100D01;
	SceUInt8 slot = 0;
	ret = ResolveSlot(ch, (int)payload[4], &slot);
	if (ret == 0)
		return sce::MusicCore::ctrlSetRepeat((uint32_t)seqId, slot, mappedRepeat);
	return ret;
}

/* 0x811CF428 */
int MusicCoreMain::HandleSetShuffle(const MusicCoreQueueMsg *msg)
{
	int seqId = 0;
	SceUInt8 ch = 0;
	int ret = ParseMsgHeader(msg, &seqId, &ch);
	if (ret != 0)
		return ret;
	SceUInt32 payload[5];
	sce_paf_memset(payload, 0, sizeof(payload));
	sce_paf_memcpy(payload, msg->payload, msg->payloadSize);
	if (payload[0] >= 2)
		return (int)0x80100D01;
	SceUInt8 slot = 0;
	ret = ResolveSlot(ch, (int)payload[4], &slot);
	if (ret == 0)
		return sce::MusicCore::ctrlSetShuffle((uint32_t)seqId, slot, (int)payload[0]);
	return ret;
}

/* 0x811CF4B8 */
int MusicCoreMain::HandleSetAvls(const MusicCoreQueueMsg *msg)
{
	int seqId = 0;
	SceUInt8 ch = 0;
	int ret = ParseMsgHeader(msg, &seqId, &ch);
	if (ret != 0)
		return ret;
	SceUInt32 payload[5];
	sce_paf_memset(payload, 0, sizeof(payload));
	sce_paf_memcpy(payload, msg->payload, msg->payloadSize);
	if (payload[0] >= 2)
		return (int)0x80100D01;
	SceUInt8 slot = 0;
	ret = ResolveSlot(ch, (int)payload[4], &slot);
	if (ret == 0)
		return sce::MusicCore::ctrlSetAudioNormalizer((uint32_t)seqId, slot, (int)payload[0]);
	return ret;
}

/* 0x811CF548 */
int MusicCoreMain::HandleOpenChannel(const MusicCoreQueueMsg *msg)
{
	int seqId = 0;
	SceUInt8 ch = 0;
	int ret = ParseMsgHeader(msg, &seqId, &ch);
	if (ret != 0)
		return ret;
	int payload[5];
	sce_paf_memset(payload, 0, sizeof(payload));
	sce_paf_memcpy(payload, msg->payload, msg->payloadSize);
	int appId = payload[4];

	if (ch == 1 || (ch != 0 && ch != 2))
		return (int)0x80100D01;
	int priority = (ch == 2) ? 0x7FFFFFFF : 0;

	int freeIdx = -1;
	for (int i = 0; i < 8; ++i) {
		if (s_appSlots[i].appId == -1) {
			freeIdx = i;
			break;
		}
	}
	if (freeIdx < 0 || freeIdx >= 7)
		return (int)0x80100D01;

	SceUInt8 slotId = 0xFF;
	ret = sce::MusicCore::ctrlOpenSimple((uint32_t)seqId, ch, priority, &slotId);
	if (ret != 0)
		return ret;

	MusicCoreAppSlot *slot = &s_appSlots[freeIdx];
	slot->appId = appId;
	slot->slotId = slotId;

	int activeApp = sceAppMgrGetAppIdByAppId(-4);
	int fallbackApp = sceAppMgrGetAppIdByAppId(-2);
	SceUInt8 status[0x80];
	sce_paf_memset(status, 0, sizeof(status));
	ret = sceAppMgrGetStatusByAppId(appId, status);
	if (ret != 0)
		return ret;
	sce_paf_memcpy(slot->titleId, status + 12, 32);
	slot->active = 1;

	if (appId == activeApp || (activeApp == -1 && appId == fallbackApp)) {
		slot->focusState = 1;
		if (ch == 0)
			return sce::MusicCore::ctrlSetActiveSlot((uint32_t)seqId, slotId);
	} else {
		slot->focusState = 2;
	}
	return 0;
}

/* 0x811CF6A8 */
int MusicCoreMain::HandleCloseChannel(const MusicCoreQueueMsg *msg)
{
	int seqId = 0;
	SceUInt8 ch = 0;
	int ret = ParseMsgHeader(msg, &seqId, &ch);
	if (ret != 0)
		return ret;
	int payload[5];
	sce_paf_memset(payload, 0, sizeof(payload));
	sce_paf_memcpy(payload, msg->payload, msg->payloadSize);
	int appId = payload[4];

	if (ch != 0 && ch != 2)
		return (int)0x80100D01;
	SceUInt8 slotId = 0;
	ret = ResolveSlot(ch, appId, &slotId);
	if (ret != 0)
		return ret;

	for (int i = 0; i < 8; ++i) {
		if (s_appSlots[i].appId == appId) {
			ret = sce::MusicCore::ctrlClose((uint32_t)seqId, slotId);
			if (ret != 0)
				return ret;
			s_appSlots[i].appId = -1;
			s_appSlots[i].seqId = 0;
			s_appSlots[i].slotId = 0xFF;
			s_appSlots[i].focusState = 0;
			sce_paf_memset(s_appSlots[i].titleId, 0, 32);
			s_appSlots[i].active = 0;
			s_appSlots[i].bgmAllowed = 0;
			s_appSlots[i].lastCtrl = 1;
			return (i < 7) ? 0 : (int)0x80100D01;
		}
	}
	return (int)0x80100D01;
}

/* 0x811CF7AC */
int MusicCoreMain::HandleSetEq(const MusicCoreQueueMsg *msg)
{
	int seqId = 0;
	SceUInt8 ch = 0;
	int ret = ParseMsgHeader(msg, &seqId, &ch);
	if (ret != 0)
		return ret;
	int payload[5];
	sce_paf_memset(payload, 0, sizeof(payload));
	sce_paf_memcpy(payload, msg->payload, msg->payloadSize);
	if (payload[0] < 0 || payload[0] > 4)
		return (int)0x80100D01;
	SceUInt8 slot = 0;
	ret = ResolveSlot(ch, payload[4], &slot);
	if (ret == 0)
		return sce::MusicCore::ctrlSetEqualizer((uint32_t)seqId, slot, payload[0], 0, 0, 0);
	return ret;
}

/* 0x811CF850 */
int MusicCoreMain::HandleDbQuery(const MusicCoreQueueMsg *msg)
{
	int seqId = 0;
	SceUInt8 ch = 0;
	int ret = ParseMsgHeader(msg, &seqId, &ch);
	if (ret != 0)
		return ret;
	struct DbQueryMsg {
		int queryType;
		int padAlign;
		SceUInt8 data[160];
		SceUInt32 opt;
		SceUInt8 tail[12];
	} buf;
	sce_paf_memset(&buf, 0, sizeof(buf));
	sce_paf_memcpy(&buf, msg->payload, msg->payloadSize);
	if (buf.opt > 2)
		return (int)0x80100D01;
	return sce::MusicCore::ctrlDbQuery((uint32_t)seqId, buf.data, buf.queryType, (int)buf.opt);
}

static void applyPauseToTitleSlots(MusicCoreMain *self, bool forceStop)
{
	int foundIdx = -1;
	for (int i = 0; i < 8; ++i) {
		if (sce_paf_strncmp(s_curAppTitle, s_appSlots[i].titleId, sce_paf_strlen(s_curAppTitle)) == 0) {
			foundIdx = i;
			break;
		}
	}
	if (foundIdx >= 0) {
		SceUInt8 st = 0;
		self->CheckSlotPaused(0, s_appSlots[foundIdx].appId, &st);
		if (forceStop) {
			if (st == 0 || st == 1)
				sce::MusicCore::ctrlPause((uint32_t)s_appSlots[foundIdx].seqId, s_appSlots[foundIdx].slotId, 0, 0);
		} else {
			if (st == 0)
				sce::MusicCore::ctrlPlay((uint32_t)s_appSlots[foundIdx].seqId, s_appSlots[foundIdx].slotId, 0);
			if (st == 1)
				sce::MusicCore::ctrlPause((uint32_t)s_appSlots[foundIdx].seqId, s_appSlots[foundIdx].slotId, 1, 0);
		}
	}
	if (foundIdx < 0 || foundIdx >= 7) {
		for (int i = 0; i < 8; ++i) {
			if (sce_paf_strncmp(s_curParentTitle, s_appSlots[i].titleId, sce_paf_strlen(s_curParentTitle)) == 0) {
				SceUInt8 st = 0;
				self->CheckSlotPaused(0, s_appSlots[i].appId, &st);
				if (forceStop) {
					if (st == 0 || st == 1)
						sce::MusicCore::ctrlPause((uint32_t)s_appSlots[i].seqId, s_appSlots[i].slotId, 0, 0);
				} else {
					if (st == 0)
						sce::MusicCore::ctrlPlay((uint32_t)s_appSlots[i].seqId, s_appSlots[i].slotId, 0);
					if (st == 1)
						sce::MusicCore::ctrlPause((uint32_t)s_appSlots[i].seqId, s_appSlots[i].slotId, 1, 0);
				}
				break;
			}
		}
	}
}

/* 0x811CF8E2 */
int MusicCoreMain::HandleAppFocusChange(const MusicCoreQueueMsg *msg)
{
	AppFocusChangePayload focusMsg;
	sce_paf_memset(&focusMsg, 0, sizeof(focusMsg));
	sce_paf_memcpy(&focusMsg, msg->payload, msg->payloadSize);

	int evType = focusMsg.eventType;
	char title[32];
	sce_paf_memcpy(title, focusMsg.titleId, 32);

	char parentBuf[128];
	sce_paf_memset(parentBuf, 0, sizeof(parentBuf));
	char *parentTitle = parentBuf + 80;
	if (evType == 2) {
		sce_paf_memset(parentTitle, 0, 32);
	} else if (evType == 3) {
		sce_paf_strncpy(parentTitle, "NPXS19999", sce_paf_strlen("NPXS19999"));
	} else {
		int ret = sceAppMgrGetStatusByName(title, parentBuf);
		if (ret != 0)
			return ret;
	}

	sce_paf_memcpy(s_prevAppTitle, s_curAppTitle, 32);
	sce_paf_memcpy(s_curAppTitle, title, 32);
	sce_paf_memcpy(s_prevParentTitle, s_curParentTitle, 32);
	sce_paf_memcpy(s_curParentTitle, parentTitle, 32);

	if (sce_paf_strncmp(s_prevAppTitle, "NPXS10009", sce_paf_strlen(s_prevAppTitle)) == 0 &&
	    sce_paf_strncmp(s_curAppTitle, "NPXS19999", sce_paf_strlen(s_curAppTitle)) == 0 &&
	    (SceUInt64)s_rawStatusCh1.trackId >= 0xFFFFFFFFFFFFFFFEULL) {
		sce::MusicCore::ctrlPause(0, 1, 0, 0);
	}

	int ret = 0;
	if (sce_paf_strncmp(s_prevAppTitle, s_bgmSlot.titleId, sce_paf_strlen(s_prevAppTitle)) == 0)
		ret = sce::MusicCore::ctrlSuspendAudio(512, 1);

	if (sce_paf_strncmp(s_prevAppTitle, s_curAppTitle, sce_paf_strlen(s_curAppTitle)) != 0) {
		for (int i = 0; i < 8; ++i) {
			if (sce_paf_strncmp(s_prevAppTitle, s_appSlots[i].titleId, sce_paf_strlen(s_prevAppTitle)) == 0) {
				s_appSlots[i].focusState = 2;
				SceUInt8 sl = 0xFF;
				if (ResolveSlot(0, s_appSlots[i].appId, &sl) == 0)
					ret = sce::MusicCore::ctrlSetActiveSlot(257, 255);
				break;
			}
		}
		for (int i = 0; i < 8; ++i) {
			if (sce_paf_strncmp(s_curAppTitle, s_appSlots[i].titleId, sce_paf_strlen(s_curAppTitle)) == 0) {
				SceUInt8 st = 0;
				CheckSlotPaused(0, s_appSlots[i].appId, &st);
				if (st == 0)
					sce::MusicCore::ctrlPlay((uint32_t)s_appSlots[i].seqId, s_appSlots[i].slotId, 0);
				if (st == 1)
					sce::MusicCore::ctrlPause((uint32_t)s_appSlots[i].seqId, s_appSlots[i].slotId, 0, 0);
				s_appSlots[i].focusState = 1;
				SceUInt8 sl = 0;
				int resolveRet = ResolveSlot(0, s_appSlots[i].appId, &sl);
				if (resolveRet == 0)
					return sce::MusicCore::ctrlSetActiveSlot(256, sl);
				return resolveRet;
			}
		}
	}
	return ret;
}

/* 0x811CFE7C */
int MusicCoreMain::HandleSystemPause(const MusicCoreQueueMsg *msg)
{
	SceUInt32 payload[5];
	sce_paf_memset(payload, 0, sizeof(payload));
	sce_paf_memcpy(payload, msg->payload, msg->payloadSize);
	if (payload[0] > 1)
		return (int)0x80100D01;
	s_sysPauseFlag = (SceUInt8)payload[0];
	applyPauseToTitleSlots(this, false);
	return 0;
}

/* 0x811D0016 */
int MusicCoreMain::HandlePowerSuspend(const MusicCoreQueueMsg *msg)
{
	SceUInt32 payload[5];
	sce_paf_memset(payload, 0, sizeof(payload));
	sce_paf_memcpy(payload, msg->payload, msg->payloadSize);
	s_powerPauseFlag = 1;
	applyPauseToTitleSlots(this, true);
	return 0;
}

/* 0x811D015C */
int MusicCoreMain::HandlePowerResume(const MusicCoreQueueMsg *msg)
{
	SceUInt32 payload[5];
	sce_paf_memset(payload, 0, sizeof(payload));
	sce_paf_memcpy(payload, msg->payload, msg->payloadSize);
	s_powerPauseFlag = 0;
	applyPauseToTitleSlots(this, false);
	return 0;
}

/* 0x811D02D2 */
int MusicCoreMain::GetStatus(const MusicCoreClientReq *req, void *outRaw)
{
	if (req->channel == 0) {
		sce_paf_memcpy(outRaw, &s_rawStatusCh1, sizeof(s_rawStatusCh1));
		return 0;
	}
	if (req->channel == 1) {
		SceUInt8 slot = 0;
		int ret = ResolveSlot(0, req->appId, &slot);
		if (ret != 0)
			return ret;
		if (slot == s_rawStatusCh0.channelId) {
			sce_paf_memcpy(outRaw, &s_rawStatusCh0, sizeof(s_rawStatusCh0));
			return 0;
		}
		sce::MusicCore::PlaybackStatusOut statusOut;
		int tm = 0;
		SceUInt8 valid = 0;
		ret = sce::MusicCore::ctrlGetPlaybackStatus(slot, &statusOut, &tm, &valid);
		if (ret == 0)
			sce_paf_memcpy(outRaw, &statusOut, 144);
		return ret;
	}
	if (req->channel == 2) {
		sce_paf_memcpy(outRaw, &s_rawStatusCh2, sizeof(s_rawStatusCh2));
		return 0;
	}
	return (int)0x80100D01;
}

static int popRingError(MusicCoreErrorRing *ring)
{
	int err = ring->errors[ring->readIdx];
	ring->errors[ring->readIdx] = 0;
	ring->readIdx++;
	if (ring->readIdx >= 10)
		ring->readIdx = 0;
	return err;
}

/* 0x811D038A */
int MusicCoreMain::GetError(const MusicCoreClientReq *req, int *outErr)
{
	if (req->channel == 0) {
		*outErr = popRingError(&s_errRingCh1);
		return 0;
	}
	if (req->channel == 1) {
		SceUInt8 slot = 0;
		int ret = ResolveSlot(0, req->appId, &slot);
		if (ret != 0)
			return ret;
		for (int i = 0; i < 8; ++i) {
			if (s_appSlots[i].slotId == slot) {
				*outErr = popRingError(&s_errRingCh0[i]);
				return 0;
			}
		}
		return (int)0x80100D01;
	}
	if (req->channel == 2) {
		*outErr = popRingError(&s_errRingCh2);
		return 0;
	}
	return (int)0x80100D01;
}

/* 0x811D04AC */
int MusicCoreMain::GetTrackInfo(const MusicCoreClientReq *req, void *outTrack, void *outRaw)
{
	if (req->channel == 0) {
		sce_paf_memcpy(outTrack, &s_trackInfoBuf, sizeof(s_trackInfoBuf));
		return GetStatus(req, outRaw);
	}
	if (req->channel > 2)
		return (int)0x80100D01;
	return 0;
}

/* 0x811D04FA */
int MusicCoreMain::GetUri(const MusicCoreClientReq *req, void *outUri)
{
	if (req->channel == 0) {
		sce_paf_memcpy(outUri, s_uriCh1, 0x401);
		return 0;
	}
	if (req->channel == 1) {
		SceUInt8 slot = 0;
		int ret = ResolveSlot(0, req->appId, &slot);
		if (ret != 0)
			return ret;
		if (slot == s_rawStatusCh0.channelId) {
			sce_paf_memcpy(outUri, s_uriCh0, 0x401);
			return 0;
		}
		int kind = 0;
		SceUInt8 buf[0x828];
		SceUInt8 valid = 0;
		ret = sce::MusicCore::ctrlGetPlaybackMode(slot, 0, &kind, buf, &valid);
		if (ret == 0 && kind == 1)
			sce_paf_memcpy(outUri, buf, 0x401);
		return ret;
	}
	if (req->channel == 2) {
		sce_paf_memcpy(outUri, s_uriCh2, 0x401);
		return 0;
	}
	return (int)0x80100D01;
}

/* 0x811D05CE */
int MusicCoreMain::GetPlayTimeFlag(const MusicCoreClientReq *req, int *outVal)
{
	if (req->channel == 0) {
		*outVal = s_playTimeCh1;
		return 0;
	}
	if (req->channel == 1) {
		SceUInt8 slot = 0;
		int ret = ResolveSlot(0, req->appId, &slot);
		if (ret != 0)
			return ret;
		if (slot == s_playTimeCh0Slot) {
			*outVal = s_playTimeCh0Val;
			return 0;
		}
		sce::MusicCore::PlaybackStatusOut statusOut;
		int tm = 0;
		SceUInt8 valid = 0;
		ret = sce::MusicCore::ctrlGetPlaybackStatus(slot, &statusOut, &tm, &valid);
		if (ret == 0)
			*outVal = (valid != 0) ? tm : 0;
		return ret;
	}
	if (req->channel == 2) {
		*outVal = s_playTimeCh2;
		return 0;
	}
	return (int)0x80100D01;
}

/* 0x811D068A */
int MusicCoreMain::GetCap(const MusicCoreClientReq *req, void *outCap)
{
	(void)req;
	sce_paf_memcpy(outCap, s_capData, sizeof(s_capData));
	return 0;
}

/* 0x811D06A0 */
int MusicCoreMain::GetMeta(const MusicCoreClientReq *req, char *outMeta)
{
	if (req->channel == 0) {
		sce_paf_memcpy(outMeta, s_metaArtist, 2048);
		sce_paf_memcpy(outMeta + 2048, s_metaTitle, 2048);
		sce_paf_memcpy(outMeta + 4096, s_metaAlbum, 2048);
		return 0;
	}
	if (req->channel > 2)
		return (int)0x80100D01;
	return 0;
}

/* 0x811D06FE */
int MusicCoreMain::GetPlayTimeEntry(const MusicCoreClientReq *req, void *outEntry)
{
	if (req->channel == 0) {
		sce_paf_memcpy(outEntry, &s_ptCh1, sizeof(s_ptCh1));
		s_ptCh1Prev.errCode = s_ptCh1.errCode;
		s_ptCh1.errCode = 0;
		return 0;
	}
	if (req->channel == 1) {
		SceUInt8 slot = 0;
		int ret = ResolveSlot(0, req->appId, &slot);
		if (ret != 0)
			return ret;
		for (int i = 0; i < 8; ++i) {
			if (s_appSlots[i].slotId == slot) {
				sce_paf_memcpy(outEntry, &s_ptCh0[i], sizeof(MusicCorePlayTimeEntry));
				return 0;
			}
		}
		return (int)0x80100D01;
	}
	if (req->channel == 2) {
		sce_paf_memcpy(outEntry, &s_ptCh2, sizeof(s_ptCh2));
		return 0;
	}
	return (int)0x80100D01;
}

/* 0x811D07D6 */
int MusicCoreMain::GetAllAppSlots(int unused, void *outSlots)
{
	(void)unused;
	sce_paf_memcpy(outSlots, s_appSlots, sizeof(s_appSlots));
	return 0;
}

/* 0x811D07EE */
int MusicCoreMain::GetBgmSlot(int unused, void *outSlot)
{
	(void)unused;
	sce_paf_memcpy(outSlot, &s_bgmSlot, sizeof(s_bgmSlot));
	return 0;
}

/* 0x811D0804 */
int MusicCoreMain::SetAvrcpActive(const MusicCoreClientReq *req, SceUInt8 enable)
{
	(void)req;
	SceUInt8 prev = 0;
	return sce::MusicCore::ctrlGetState(enable, &prev);
}

/* 0x811D082E */
int MusicCoreMain::SetErrorSuppression(const MusicCoreClientReq *req, SceUInt8 mode)
{
	(void)req;
	if (mode == 0)
		s_musicAppForeground = 1;
	else if (mode == 1)
		s_errorRingSuppressed = 1;
	else
		return (int)0x80100D01;
	sce_paf_memset(&s_errRingCh1, 0, sizeof(s_errRingCh1));
	return 0;
}

/* 0x811D087C */
int MusicCoreMain::ClearErrorSuppression(const MusicCoreClientReq *req, SceUInt8 mode)
{
	(void)req;
	sce_paf_memset(&s_errRingCh1, 0, sizeof(s_errRingCh1));
	if (mode == 0)
		s_musicAppForeground = 0;
	else if (mode == 1)
		s_errorRingSuppressed = 0;
	else
		return (int)0x80100D01;
	return 0;
}

/* 0x811D08CC */
void musicCoreMainNoop(void)
{
}

/* 0x811D08CE */
int MusicCoreMain::DispatchMsg(const MusicCoreQueueMsg *msg)
{
	s_ptCh1.extra = 0;
	int seqId = 0;
	SceUInt8 ch = 0;
	SceUInt8 slot = 0;
	int payload[7];

	switch (msg->cmdId) {
	case 1:  return HandleSetContent(msg);
	case 2:  return HandleControl(msg);
	case 3:
		if (ParseMsgHeader(msg, &seqId, &ch) != 0) return (int)0x80100D01;
		sce_paf_memset(payload, 0, 20);
		sce_paf_memcpy(payload, msg->payload, msg->payloadSize);
		if (ResolveSlot(ch, payload[4], &slot) != 0) return (int)0x80100D01;
		return sce::MusicCore::ctrlSetVolume((uint32_t)seqId, slot, payload[0]);
	case 4:  return HandleSetRepeat(msg);
	case 5:  return HandleSetShuffle(msg);
	case 6: {
		sce::MusicCore::ControlParams ctrlParams = { -1, (int)0x80000000u, -2, -2, -2 };
		int ret = ParseMsgHeader(msg, &seqId, &ch);
		if (ret != 0) return ret;
		sce_paf_memset(payload, 0, 20);
		sce_paf_memcpy(payload, msg->payload, msg->payloadSize);
		ctrlParams.seekPositionMs = payload[0];
		ret = ResolveSlot(ch, payload[4], &slot);
		if (ret != 0) return ret;
		return sce::MusicCore::ctrlControl((uint32_t)seqId, slot, &ctrlParams);
	}
	case 7: {
		sce::MusicCore::ControlParams ctrlParams = { -1, (int)0x80000000u, -2, -2, -2 };
		int ret = ParseMsgHeader(msg, &seqId, &ch);
		if (ret != 0) return ret;
		sce_paf_memset(payload, 0, 20);
		sce_paf_memcpy(payload, msg->payload, msg->payloadSize);
		ctrlParams.trackIndex = payload[0];
		ret = ResolveSlot(ch, payload[4], &slot);
		if (ret != 0) return ret;
		return sce::MusicCore::ctrlControl((uint32_t)seqId, slot, &ctrlParams);
	}
	case 8: {
		sce::MusicCore::ControlParams ctrlParams = { -1, (int)0x80000000u, -2, -2, -2 };
		int ret = ParseMsgHeader(msg, &seqId, &ch);
		if (ret != 0) return ret;
		sce_paf_memset(payload, 0, 20);
		sce_paf_memcpy(payload, msg->payload, msg->payloadSize);
		ctrlParams.seekMode = payload[0];
		ret = ResolveSlot(ch, payload[4], &slot);
		if (ret != 0) return ret;
		return sce::MusicCore::ctrlControl((uint32_t)seqId, slot, &ctrlParams);
	}
	case 9:  return HandleSetAvls(msg);
	case 10: return HandleOpenChannel(msg);
	case 11: return HandleCloseChannel(msg);
	case 12: {
		int ret = ParseMsgHeader(msg, &seqId, &ch);
		if (ret != 0) return ret;
		struct OpenUriMsg {
			char uri[1028];
			int uriFlags;
			SceUInt8 bgmAllowed;
			SceUInt8 optRest[11];
			int pid;
			int appId;
		} uriPayload;
		sce_paf_memset(&uriPayload, 0, sizeof(uriPayload));
		sce_paf_memcpy(&uriPayload, msg->payload, msg->payloadSize);
		char uriCopy[1028];
		sce_paf_strncpy(uriCopy, uriPayload.uri, 0x400u);
		uriCopy[0x400] = '\0';
		int appId = uriPayload.appId;
		s_ptCh1.extra = 119;
		ret = SetSlotBgmAllowed(ch, appId, uriPayload.bgmAllowed);
		if (ret != 0) return ret;
		ret = ResolveSlot(ch, appId, &slot);
		if (ret != 0) return ret;
		return sce::MusicCore::ctrlSetUriString((uint32_t)seqId, slot, uriCopy, uriPayload.uriFlags);
	}
	case 13: return HandleSetEq(msg);
	case 14: {
		sce::MusicCore::ControlParams ctrlParams = { -1, (int)0x80000000u, -2, -2, -2 };
		int ret = ParseMsgHeader(msg, &seqId, &ch);
		if (ret != 0) return ret;
		sce_paf_memset(payload, 0, 20);
		sce_paf_memcpy(payload, msg->payload, msg->payloadSize);
		if (payload[0] != -1)
			ctrlParams.seekPositionMs = payload[0];
		if (payload[1] != -1)
			ctrlParams.trackIndex = payload[1];
		if (payload[2] != -1)
			ctrlParams.seekMode = payload[2];
		ret = ResolveSlot(ch, payload[4], &slot);
		if (ret != 0) return ret;
		return sce::MusicCore::ctrlControl((uint32_t)seqId, slot, &ctrlParams);
	}
	case 15:
		if (ParseMsgHeader(msg, &seqId, &ch) != 0) return (int)0x80100D01;
		return sce::MusicCore::ctrlDbOpen((uint32_t)seqId);
	case 16:
		if (ParseMsgHeader(msg, &seqId, &ch) != 0) return (int)0x80100D01;
		sce_paf_memset(payload, 0, 20);
		sce_paf_memcpy(payload, msg->payload, msg->payloadSize);
		return sce::MusicCore::ctrlDbSuspend((uint32_t)seqId, (payload[0] != 0) ? 1 : 0);
	case 17:
	case 18:
		if (ParseMsgHeader(msg, &seqId, &ch) != 0) return (int)0x80100D01;
		return sce::MusicCore::ctrlDbClose((uint32_t)seqId);
	case 19: return HandleDbQuery(msg);
	case 20: {
		int ret = ParseMsgHeader(msg, &seqId, &ch);
		if (ret != 0) return ret;
		sce_paf_memset(payload, 0, 20);
		sce_paf_memcpy(payload, msg->payload, msg->payloadSize);
		SceUInt8 st[0x80];
		sce_paf_memset(st, 0, sizeof(st));
		ret = sceAppMgrGetStatusByAppId(payload[4], st);
		if (ret == 0) {
			sce_paf_memcpy(s_bgmSlot.titleId, st + 12, 32);
			s_bgmSlot.appId = payload[4];
			s_bgmSlot.seqId = payload[3];
			s_bgmSlot.focusState = 1;
		}
		return ret;
	}
	case 21:
		if (ParseMsgHeader(msg, &seqId, &ch) != 0) return (int)0x80100D01;
		sce_paf_memset(s_bgmSlot.titleId, 0, 32);
		s_bgmSlot.seqId = 0;
		s_bgmSlot.focusState = 0;
		s_bgmSlot.appId = -1;
		return 0;
	case 22: {
		int ret = ParseMsgHeader(msg, &seqId, &ch);
		if (ret != 0) return ret;
		sce_paf_memset(payload, 0, sizeof(payload));
		sce_paf_memcpy(payload, msg->payload, msg->payloadSize);
		sce_paf_memcpy(s_exOpenParam, payload, 20);
		SceUInt8 exSlot = 0xFF;
		ret = sce::MusicCore::ctrlOpenWithParams(
			(uint32_t)seqId, 2, 0x7FFFFFFF, &exSlot,
			reinterpret_cast<const sce::MusicCore::OpenParams *>(s_exOpenParam));
		if (ret == 0) {
			s_exAppId = payload[6];
			s_exSlotId = exSlot;
		}
		return ret;
	}
	case 23: {
		int ret = ParseMsgHeader(msg, &seqId, &ch);
		if (ret != 0) return ret;
		sce_paf_memset(payload, 0, sizeof(payload));
		sce_paf_memcpy(payload, msg->payload, msg->payloadSize);
		ret = ResolveSlot(ch, payload[6], &slot);
		if (ret == 0)
			return sce::MusicCore::ctrlClose((uint32_t)seqId, slot);
		return ret;
	}
	case 24: return HandleAppFocusChange(msg);
	case 25: return sce::MusicCore::ctrlSuspendAudio(512, 1);
	case 26: return HandleSystemPause(msg);
	case 27: return HandlePowerSuspend(msg);
	case 28: return HandlePowerResume(msg);
	case 29: return sce::MusicCore::ctrlResumeAudio(512, 1);
	case 30:
		if (paf::system::Model::GetType() == 1 ||
		    (SceUInt64)s_rawStatusCh1.trackId >= 0xFFFFFFFFFFFFFFFEULL)
			return HandleControl(msg);
		return 0;
	case 31: return sce::MusicCore::ctrlClearAudio(512, 1);
	default: return 0;
	}
}

/* 0x811D0FBA */
int MusicCoreMain::MainLoop()
{
	SceInt64 lastTrackId = -1;
	InitController();
	m_initResult = 0;
	if (sceKernelSignalSema(m_semaId, 1) >= 0) {
		m_stopping = 0;
		do {
			SceInt64 curTrackId = s_rawStatusCh1.trackId;
			if (curTrackId == -1 || curTrackId == -2) {
				if (s_btrmOwned != 0)
					paf::inputdevice::ReleaseBtrmOwnership();
				s_btrmOwned = 0;
			} else {
				sceBtAvrcpSetPlayStatus(s_avrcpPlayStatus);
				if (curTrackId != lastTrackId || s_btrmOwned == 0) {
					paf::inputdevice::GetBtrmOwnership();
					sceBtAvrcpSetTitle(s_metaTitle, sce_paf_strlen(s_metaTitle));
				}
				s_btrmOwned = 1;
			}
			lastTrackId = curTrackId;

			void *msgPtr = NULL;
			SceUInt timeout = 100000;
			int r = musicCoreQueueRecv(m_queue, &msgPtr, &timeout);
			if (r != 0 && r != (int)0x8FFF0003 && r != (int)0x8FFF000E)
				break;
			if (msgPtr != NULL) {
				MusicCoreQueueMsg *msg = (MusicCoreQueueMsg *)msgPtr;
				if ((msg->cmdId & 0x8000u) == 0) {
					DispatchMsg(msg);
					musicCoreQueueRelease(m_queue, msgPtr);
				} else if (msg->cmdId == 0x8000u) {
					m_stopping = 1;
					musicCoreQueueRelease(m_queue, msgPtr);
				}
			}
		} while (m_stopping != 1);
	}
	m_initialized = 0;
	sce::MusicCore::ctrlTerm();
	m_initResult = (int)0x8FFF0002;
	return 0;
}

/* 0x811D1174 */
int MusicCoreMain::ThreadEntry(SceSize args, void *argp)
{
	(void)args;
	if (argp != NULL)
		(*(MusicCoreMain **)argp)->MainLoop();
	SceThreadmgrCoredumpTime_sceKernelExitThread(1);
	return 0;
}

/* 0x811D1188 */
int musicCoreConvertPlayMode(SceInt8 raw, int *outMode)
{
	switch (raw) {
	case 0: *outMode = 3; return 0;
	case 1: *outMode = 2; return 0;
	case 2: *outMode = 1; return 0;
	case 3: *outMode = 0; return 0;
	default: return (int)0x80100D01;
	}
}

/* 0x811D11C6 */
int musicCoreConvertSampleRate(int raw, int *outRate)
{
	switch (raw) {
	case 8:  *outRate = 1; break;
	case 9:  *outRate = 2; break;
	case 12: *outRate = 3; break;
	case 13: *outRate = 4; break;
	case 14: *outRate = 5; break;
	case 29: *outRate = 6; break;
	default: *outRate = 0; break;
	}
	return 0;
}

/* 0x811D1214 */
int musicCoreConvertRepeat(int raw, int *outVal)
{
	if (raw == 0) { *outVal = 2; return 0; }
	if (raw == 1) { *outVal = 0; return 0; }
	if (raw == 2) { *outVal = 1; return 0; }
	return (int)0x80100D01;
}

/* 0x811D1248 */
int musicCoreConvertShuffle(SceUInt32 raw, int *outVal)
{
	if (raw > 1)
		return (int)0x80100D01;
	*outVal = (int)raw;
	return 0;
}

/* 0x811D126C */
int musicCoreConvertEq(int raw, int *outVal)
{
	if (raw == 0 || raw == 1) {
		*outVal = raw;
		return 0;
	}
	return (int)0x80100D01;
}

/* 0x811D1298 */
int musicCoreConvertAvls(int raw, int unusedMode, int unusedRate, int unusedFlags, int *outVal)
{
	(void)unusedMode; (void)unusedRate; (void)unusedFlags;
	if (raw < 0 || raw > 4)
		return (int)0x80100D01;
	*outVal = raw;
	return 0;
}

/* 0x811D12E2 */
int musicCoreConvertVolumeMode(SceUInt32 raw, int *outVal)
{
	if (raw <= 2) {
		*outVal = (int)raw;
		return 0;
	}
	if (raw == (SceUInt32)-1) {
		*outVal = 0;
		return 0;
	}
	return (int)0x80100D01;
}

/* 0x811D131E */
void musicCoreConvertNoop(void)
{
}

/* 0x811D1320 */
int musicCoreConvertStatus(const void *rawPtr, void *outStatusPtr, int playTime, const SceUInt8 *cap)
{
	const MusicCoreRawSnap *raw = (const MusicCoreRawSnap *)rawPtr;
	MusicCoreConvertedStatus *outStatus = (MusicCoreConvertedStatus *)outStatusPtr;
	SceInt8 chType = raw->playerKind;
	if (chType == 0)
		outStatus->channelType = 1;
	else if (chType == 1)
		outStatus->channelType = 0;
	else if (chType == 2)
		outStatus->channelType = 2;
	else
		return (int)0x80100D01;

	outStatus->cmdParam = raw->cmdParam;
	outStatus->errCode = raw->errCode;
	outStatus->hasRingError = 0;

	if (outStatus->channelType == 0) {
		for (int i = 0; i < 10; ++i) {
			if (s_errRingCh1.errors[i] != 0) {
				outStatus->hasRingError = 1;
				break;
			}
		}
	} else if (outStatus->channelType == 1) {
		int idx = -1;
		for (int i = 0; i < 8; ++i) {
			if (s_appSlots[i].slotId == raw->channelId) {
				idx = i;
				break;
			}
		}
		if (idx < 0)
			return (int)0x80100D01;
		for (int i = 0; i < 10; ++i) {
			if (s_errRingCh0[idx].errors[i] != 0) {
				outStatus->hasRingError = 1;
				break;
			}
		}
	} else {
		for (int i = 0; i < 10; ++i) {
			if (s_errRingCh2.errors[i] != 0) {
				outStatus->hasRingError = 1;
				break;
			}
		}
	}

	int ret = musicCoreConvertPlayMode(raw->bgmStatus, &outStatus->playMode);
	if (ret != 0) return ret;
	outStatus->bgmReason = raw->bgmReason;
	outStatus->bgmPriority = raw->bgmPriority;
	outStatus->playableFlag = raw->playableFlag;
	outStatus->trackId = raw->trackId;
	outStatus->trackIndex = raw->trackIndex;
	outStatus->playTime = playTime;
	outStatus->durationMs = raw->durationMs;
	outStatus->sampleRate = raw->sampleRate;
	outStatus->bitRate = raw->bitRate;
	outStatus->channels = raw->channels;
	outStatus->codecType = raw->codecType;
	outStatus->sourceType = raw->sourceType;
	outStatus->currentTrackNum = raw->currentTrackNum;
	outStatus->totalTracks = raw->totalTracks;
	outStatus->audioBalance = raw->audioBalance;

	ret = musicCoreConvertSampleRate(raw->sampleRate, &outStatus->sampleRate);
	if (ret != 0) return ret;
	ret = musicCoreConvertRepeat(raw->repeatMode, &outStatus->repeatMode);
	if (ret != 0) return ret;
	ret = musicCoreConvertShuffle((SceUInt32)raw->shuffleMode, &outStatus->shuffleMode);
	if (ret != 0) return ret;
	ret = musicCoreConvertEq(raw->audioNormalizer, &outStatus->audioNormalizer);
	if (ret != 0) return ret;
	if (raw->eqType < 0 || raw->eqType > 4)
		return (int)0x80100D01;
	outStatus->eqType = raw->eqType;
	ret = musicCoreConvertVolumeMode((SceUInt32)raw->contentType, &outStatus->volumeMode);
	if (ret != 0) return ret;

	outStatus->trackItemId = raw->trackItemId;
	outStatus->playlistId = raw->playlistId;
	outStatus->seqNum = raw->seqNum;
	if (outStatus->channelType == 0)
		outStatus->capActive = (cap[50] != 0);
	else if (outStatus->channelType == 1)
		outStatus->capActive = (cap[30] != 0);
	else
		outStatus->capActive = (cap[70] != 0);
	return 0;
}

/* 0x811D1608 */
int musicCoreConvertTrackInfo(const char *rawTrackPtr, const void *rawStatusPtr, char *outInfoPtr)
{
	const MusicCoreTrackInfoRaw *src = (const MusicCoreTrackInfoRaw *)rawTrackPtr;
	const MusicCoreRawSnap *st = (const MusicCoreRawSnap *)rawStatusPtr;
	MusicCoreTrackInfoRaw *dst = (MusicCoreTrackInfoRaw *)outInfoPtr;
	sce_paf_strncpy(dst->uri, src->uri, 0x801);
	dst->listIndex = src->listIndex;
	dst->trackIndex = st->trackIndex;
	dst->trackId = st->trackId;

	SceUInt32 shuffleFlag = (SceUInt32)src->shuffleFlag;
	if (shuffleFlag <= 1)
		dst->shuffleFlag = (int)shuffleFlag;
	else if (shuffleFlag == (SceUInt32)-1)
		dst->shuffleFlag = 2;
	else
		return (int)0x80100D01;

	SceUInt32 contentKind = (SceUInt32)src->contentKind;
	if (contentKind == 1)
		dst->contentKind = 0;
	else if (contentKind == 2)
		dst->contentKind = 2;
	else if (contentKind == (SceUInt32)-1)
		dst->contentKind = 18;
	else
		return (int)0x80100D01;
	return 0;
}

/* 0x811D16B2 */
int musicCoreConvertUri(const char *rawUri, char *outUri)
{
	sce_paf_strncpy(outUri, rawUri, 0x401);
	return 0;
}

/* 0x811D16C6 */
int musicCoreConvertCapFlag(SceInt8 capByte, int *outVal)
{
	switch (capByte) {
	case 0: case 4: *outVal = 0; return 0;
	case 1: *outVal = 1; return 0;
	case 2: *outVal = 3; return 0;
	case 3: *outVal = 2; return 0;
	default: return (int)0x80100D01;
	}
}

/* 0x811D16FE */
static int musicCoreOnCapCb(const void *cap)
{
	sce_paf_memcpy(s_capData, cap, sizeof(s_capData));
	return 0;
}

/* 0x811D1740 */
int musicCoreSetImposePausedNotice(int mode, const char *text, SceUInt8 show)
{
	paf::Plugin *impose = paf::Plugin::Find("impose_plugin");
	if (impose == NULL)
		return (int)0x80100D00;
	ImposeNoticeInterface *iface = (ImposeNoticeInterface *)impose->GetInterface(1);
	if (iface == NULL)
		return (int)0x80100D00;
	if (mode != 0)
		return (int)0x80100D01;
	if (text == NULL)
		return (int)0x80100D01;

	SceUInt32 notice[14];
	sce_paf_memset(notice, 0, sizeof(notice));
	notice[0] = (SceUInt32)(SceUIntPtr)"SceShellMusicCorePaused";
	int ok = 0;
	if (show == 1) {
		paf::IDParam iconId(0xA11603D2u);
		notice[10] = (SceUInt32)iface->getTexture(&iconId);
		notice[8] = (SceUInt32)(SceUIntPtr)text;
		notice[9] = 1;
		ok = iface->showNotice(notice);
	} else {
		ok = iface->hideNotice(notice);
	}
	return (ok == 1) ? 0 : (int)0x80100D00;
}

static void pushRingError(MusicCoreErrorRing *ring, int err)
{
	if (err == 0)
		return;
	SceUInt32 prev = (ring->writeIdx == 0) ? 9 : (ring->writeIdx - 1);
	if (ring->errors[prev] != err) {
		ring->errors[ring->writeIdx] = err;
		ring->writeIdx++;
		if (ring->writeIdx >= 10)
			ring->writeIdx = 0;
	}
}

/* 0x811D18B0 */
static int musicCoreOnStatusCb(const void *rawPtr, SceUInt16 eventId, SceUInt32 cmd, int err)
{
	const MusicCoreRawSnap *raw = (const MusicCoreRawSnap *)rawPtr;
	SceInt8 chType = raw->playerKind;
	MusicCoreConvertedStatus converted;
	sce_paf_memset(&converted, 0, sizeof(converted));
	MusicCoreClientReq ctx;
	sce_paf_memset(&ctx, 0, sizeof(ctx));
	ctx.seqId = (int)cmd;
	ctx.channel = (SceUInt8)chType;

	if (chType == 1) {
		SceInt64 prevTrack = s_rawStatusCh1.trackId;
		SceInt64 newTrack = raw->trackId;
		if (cmd == 0 && s_errorRingSuppressed == 0) {
			if (s_musicAppForeground == 0 || err == (int)0x80100D06 || err == (int)0x80100D07)
				pushRingError(&s_errRingCh1, err);
		}
		if ((cmd >> 16) == 1) {
			s_ptCh1.cmdId = (int)cmd;
			if (s_ptCh1Prev.errCode != err)
				s_ptCh1.errCode = err;
		}
		if (newTrack != prevTrack) {
			sce_paf_memset(&s_metaDesc, 0, sizeof(s_metaDesc));
			sce_paf_memset(s_metaArtist, 0, 3 * 2048);
			s_metaDesc.artistBuf = s_metaArtist;
			s_metaDesc.artistSize = 2048;
			s_metaDesc.titleBuf = s_metaTitle;
			s_metaDesc.titleSize = 2048;
			s_metaDesc.albumBuf = s_metaAlbum;
			s_metaDesc.albumSize = 2048;
			int ret = sce::MusicCore::ctrlGetContentInfo(
				(SceUInt8)chType,
				reinterpret_cast<sce::MusicCore::ContentInfoBuffers *>(&s_metaDesc));
			if (ret != 0)
				return ret;
			s_titleDirty = 1;
		}
		if (newTrack == -1 && s_titleDirty != 0) {
			char blank[4];
			sce_paf_strncpy(blank, " ", 1);
			sceBtAvrcpSetTitle(blank, 1);
			s_titleDirty = 0;
		}
		s_rawStatusCh1 = *raw;
		int ret = musicCoreConvertStatus(&s_rawStatusCh1, &converted, s_playTimeCh1, s_capData);
		if (ret == 0 && s_notifyCb != NULL)
			s_notifyCb(&ctx, eventId, &err, &converted);
		return ret;
	}
	if (chType == 2) {
		if (cmd == 0)
			pushRingError(&s_errRingCh2, err);
		if ((cmd >> 16) == 2 || (cmd >> 16) == 3) {
			s_ptCh2.cmdId = (int)cmd;
			s_ptCh2.errCode = err;
		}
		s_rawStatusCh2 = *raw;
		int ret = musicCoreConvertStatus(&s_rawStatusCh2, &converted, s_playTimeCh2, s_capData);
		if (ret == 0 && s_notifyCb != NULL)
			s_notifyCb(&ctx, eventId, &err, &converted);
		return ret;
	}
	if (chType == 0) {
		int idx = -1;
		for (int i = 0; i < 8; ++i) {
			if (s_appSlots[i].slotId == raw->channelId) {
				idx = i;
				break;
			}
		}
		if (idx < 0)
			return (int)0x80100D01;
		if (cmd == 0)
			pushRingError(&s_errRingCh0[idx], err);
		if ((cmd >> 16) == 2 || (cmd >> 16) == 3) {
			s_ptCh0[idx].cmdId = (int)cmd;
			s_ptCh0[idx].errCode = err;
		}
		s_rawStatusCh0 = *raw;
		int ret = musicCoreConvertStatus(&s_rawStatusCh0, &converted, s_playTimeCh0Val, s_capData);
		if (ret == 0 && s_notifyCb != NULL)
			s_notifyCb(&ctx, eventId, &err, &converted);
		return ret;
	}
	return (int)0x80100D01;
}

/* 0x811D1E06 */
static int musicCoreOnExtraCb(const void *data)
{
	const ExtraCbHeader *hdr = (const ExtraCbHeader *)data;
	SceInt8 chType = hdr->channelType;
	if (chType < 0 || chType > 2)
		return (int)0x80100D01;
	sce_paf_memcpy(s_extraState[chType], data, 8);
	return 0;
}

/* 0x811D1E82 */
static int musicCoreOnTimeCb(const void *data)
{
	const MusicCoreTimeCbData *cb = (const MusicCoreTimeCbData *)data;
	SceInt8 chType = cb->channelType;
	int timeVal = cb->timeMs;
	SceInt64 trackId = cb->trackId;
	if (chType == 0) {
		s_playTimeCh0Slot = cb->slotId;
		s_playTimeCh0Val = timeVal;
		return 0;
	}
	if (chType == 1) {
		if (trackId == s_rawStatusCh1.trackId)
			s_playTimeCh1 = timeVal;
		return 0;
	}
	if (chType == 2) {
		s_playTimeCh2 = timeVal;
		return 0;
	}
	return (int)0x80100D01;
}

/* 0x811D1F18 */
static int musicCoreOnMediaCb(int kind, const void *rawStatusPtr, const void *data)
{
	const MusicCoreRawSnap *rawStatus = (const MusicCoreRawSnap *)rawStatusPtr;
	SceInt8 chType = rawStatus->playerKind;
	if (chType == 0) {
		if (kind == 1)
			sce_paf_memcpy(s_uriCh0, data, 0x401);
		return 0;
	}
	if (chType == 1) {
		if (kind == 2) {
			sce_paf_memcpy(&s_trackInfoBuf, data, sizeof(s_trackInfoBuf));
			s_trackInfoBuf.trackId = rawStatus->trackId;
			s_trackInfoBuf.trackIndex = rawStatus->trackIndex;
		} else if (kind == 1) {
			sce_paf_memcpy(s_uriCh1, data, 0x401);
		}
		return 0;
	}
	if (chType == 2) {
		if (kind == 1)
			sce_paf_memcpy(s_uriCh2, data, 0x401);
		return 0;
	}
	return (int)0x80100D01;
}
