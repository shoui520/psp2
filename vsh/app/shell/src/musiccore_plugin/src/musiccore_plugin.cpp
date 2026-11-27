/*
 * SceShell musiccore_plugin: plugin entry, public interface, and event hooks
 * (0x811CA5A2 - 0x811CE626)
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/list>
#include <std/string.h>
#include <std/new>
#include <framework.h>
#include <plugin.h>
#include <inputdevice/inputdevice.h>

#include "musiccore_main_thread.h"

extern "C" {
extern int sceAppMgrGetStatusByName(const char *name, void *status);
extern void SceShell_83D16CA8(void);
extern int SceShell_7FE4E732(int id, void *cb, void *arg);
extern int SceShell_D4C7D2B7(int id, void *cb, void *arg);
extern int SceShell_E3D3A162(int id, void *cb, void *arg);
}

typedef void (*ShellEventCallback)(int event, const void *data, SceSize size, void *arg);
extern int shellAppEventServerAddListener(int event, ShellEventCallback callback, void *arg); /* 0x810E17DE */
extern int shellAppEventServerRemoveListener(int event, ShellEventCallback callback, void *arg); /* 0x810E17E6 */

extern void musicCoreHeapInit(void);				/* 0x811D34C0 */
extern void musicCoreHeapFini(void);				/* 0x811D3552 */
extern void *musicCoreHeapAlloc(SceSize size);			/* 0x811D3490 */

typedef void (*MusicCoreEventCb)(int eventId, int eventData, int eventDetail, int eventFlags);

typedef MusicCoreClientReq MusicCoreClientCtx;

struct MusicCoreRawStatus {
	int channelType;					/* +0 */
	int state;						/* +4 */
	int eventType;						/* +8 */
	int statusExtraLow;					/* +12 */
	int statusExtraHigh;					/* +16 */
	int errorCode;						/* +20 */
	int errorDetailLow;					/* +24 */
	int errorDetailHigh;					/* +28 */
	SceUInt32 playMode;					/* +32 */
	int bgmReason;						/* +36 */
	int bgmPriority;					/* +40 */
	int mediaType;						/* +44 */
	SceInt64 duration;					/* +48 */
	int posLo;						/* +56 */
	int posHi;						/* +60 */
	int sampleRate;						/* +64 */
	int bitrate;						/* +68 */
	int channels;						/* +72 */
	int codec;						/* +76 */
	int trackIndex;						/* +80 */
	int trackTotal;						/* +84 */
	int artistId;						/* +88 */
	int albumId;						/* +92 */
	int audioBalance;					/* +96 */
	SceUInt32 shuffleMode;					/* +100 */
	SceUInt32 repeatMode;					/* +104 */
	SceUInt32 avlsMode;					/* +108 */
	int eqMode;						/* +112 */
	int volume;						/* +116 */
	SceInt64 trackItemId;					/* +120 */
	SceInt64 playlistId;					/* +128 */
	int isPlayable;						/* +136 */
	int seqNum;						/* +140 */
};

struct MusicCorePublicStatus {
	int state;						/* +0 */
	int eventType;						/* +4 */
	int errorCode;						/* +8 */
	SceUInt8 playMode;					/* +12 */
	SceUInt8 padPlayMode[3];				/* +13 */
	SceInt64 duration;					/* +16 */
	int posLo;						/* +24 */
	int posHi;						/* +28 */
	int sampleRate;						/* +32 */
	int bitrate;						/* +36 */
	int channels;						/* +40 */
	int codec;						/* +44 */
	int trackIndex;						/* +48 */
	int trackTotal;						/* +52 */
	int artistId;						/* +56 */
	int albumId;						/* +60 */
	SceUInt8 repeatMode;					/* +64 */
	SceUInt8 shuffleMode;					/* +65 */
	SceUInt8 eqMode;					/* +66 */
	SceUInt8 avlsMode;					/* +67 */
	int mediaType;						/* +68 */
};

struct MusicCoreImposeInfo {
	int volume;						/* +0 */
	SceUInt8 playMode;					/* +4 */
	SceUInt8 bgmReason;					/* +5 */
	SceUInt8 padBgmReason[2];				/* +6 */
	int bgmPriority;					/* +8 */
	char title[2048];					/* +12 */
	char artist[2048];					/* +2060 */
	char album[2048];					/* +4108 */
	SceUInt8 canPlay;					/* +6156 */
	SceUInt8 padCanPlay[3];					/* +6157 */
	int durationLo;						/* +6160 */
	int durationHi;						/* +6164 */
	int posLo;						/* +6168 */
	int volStep;						/* +6172 */
};

struct MusicCoreAppEventPayload {
	int type;						/* +0 */
	char titleId[32];					/* +4 */
};

static const int kMusicCoreVolumeTable[17] = {
	0x0000, 0x2027, 0x2341, 0x26A8, 0x2A63, 0x2E79, 0x32F5, 0x37E0,
	0x3D44, 0x432D, 0x49A8, 0x50C3, 0x588E, 0x6119, 0x6A77, 0x74BD,
	0x8000
};

static paf::Plugin *s_musicCorePlugin = NULL;			/* 0x81544E30 */
static MusicCoreMain *s_musicCoreMain = NULL;			/* 0x81544E34 */
static MusicCoreClientCtx s_bgmCtx;				/* 0x8156B8C8 */
static SceUInt8 s_bgmOpened = 0;				/* 0x8156B8D8 */
static SceUInt8 s_bgmLoaded = 0;				/* 0x8156B8D9 */
static SceUInt8 s_exOpened = 0;					/* 0x8156B8DA */
static SceUInt8 s_exLoaded = 0;					/* 0x8156B8DB */
static paf::list<MusicCoreEventCb> s_eventCallbacks;		/* 0x8156B8E0 */

/* Forward declarations */
static int musicCoreDispatchEventCb(int eventId, int eventData, int eventDetail, int eventFlags); /* 0x811CD8EA */
static void musicCoreOnShellEvent(int eventId, const void *info); /* 0x811CE316 */
static void musicCoreOnAppEvent(int eventType, const void *titleId); /* 0x811CE3AE */
static void musicCoreOnPowerEvent2(int eventId);		/* 0x811CE45A */
static void musicCoreOnPowerEvent3(int eventId);		/* 0x811CE4D2 */
static void musicCoreOnRemoteKey(int eventId, int handler, const void *ev); /* 0x811CE54A */

int musicCoreRegisterCallback(int unused, MusicCoreEventCb cb);
int musicCoreUnregisterCallback(int unused, MusicCoreEventCb cb);
int musicCoreCmdSetContent(const MusicCoreClientCtx *ctx, const void *content);
int musicCoreGetTrackInfo(const MusicCoreClientCtx *ctx, char *outInfo);
int musicCoreSendControlCmd(const MusicCoreClientCtx *ctx, SceUInt32 ctrlCode, int param);
int musicCoreQueryConvertedStatus(const MusicCoreClientCtx *ctx, MusicCoreRawStatus *outStatus);
int musicCoreSetParam(const MusicCoreClientCtx *ctx, int paramId, const int *values);
int musicCoreGetParam(const MusicCoreClientCtx *ctx, int paramId, int *outVal);
int musicCoreDbSync(const MusicCoreClientCtx *ctx);
static int musicCoreInterfaceNoop(void) { return 0; }		/* 0x81409E2C */
int musicCoreCmdOpen(const MusicCoreClientCtx *ctx);
int musicCoreCmdClose(const MusicCoreClientCtx *ctx);
int musicCoreSendOpenUri(const MusicCoreClientCtx *ctx, const void *uriBuf, const void *optBuf);
int musicCoreGetCurrentUri(const MusicCoreClientCtx *ctx, char *outUri);
int musicCoreGetChannelError(const MusicCoreClientCtx *ctx, int *outErr);
int musicCoreDbOpen(const MusicCoreClientCtx *ctx);
int musicCoreDbSuspend(const MusicCoreClientCtx *ctx, SceUInt8 flag);
int musicCoreDbClose(const MusicCoreClientCtx *ctx);
int musicCoreDbDetach(const MusicCoreClientCtx *ctx);
int musicCoreDbQuery(const MusicCoreClientCtx *ctx, const void *data);
int musicCoreGetCapability(const MusicCoreClientCtx *ctx, void *outCap);
int musicCoreGetMetadata(const MusicCoreClientCtx *ctx, void *outMeta);
int musicCoreGetBgmMute(int mode, void *outState);
int musicCoreSetBgmMute(int mode, int enable);
int musicCoreCmdResetChannel(const MusicCoreClientCtx *ctx);
int musicCoreGetPlayTime(const MusicCoreClientCtx *ctx, int *outTime);
int musicCoreCmdExOpen(const MusicCoreClientCtx *ctx, const void *openParam);
int musicCoreCmdExClose(const MusicCoreClientCtx *ctx);
int musicCoreCmdSetAvrcp(const MusicCoreClientCtx *ctx, SceUInt8 enable);
int musicCoreCmdSetNoticeSuppress(const MusicCoreClientCtx *ctx, SceUInt8 mute);
int musicCoreCmdClearNoticeSuppress(const MusicCoreClientCtx *ctx, SceUInt8 mute);
int musicCoreCmdSetAudioFocus(const MusicCoreClientCtx *ctx, SceUInt8 mode);
int musicCoreCmdSetSystemPause(const MusicCoreClientCtx *ctx, SceUInt8 pause);

static void *s_musicCoreInterface[33] = {			/* 0x81544E38 */
	reinterpret_cast<void *>(&musicCoreRegisterCallback),		/* [ 0] 0x811CD952 */
	reinterpret_cast<void *>(&musicCoreUnregisterCallback),		/* [ 1] 0x811CD98A */
	reinterpret_cast<void *>(&musicCoreCmdSetContent),		/* [ 2] 0x811CD9D8 */
	reinterpret_cast<void *>(&musicCoreGetTrackInfo),		/* [ 3] 0x811CDAAE */
	reinterpret_cast<void *>(&musicCoreSendControlCmd),		/* [ 4] 0x811CAF32 */
	reinterpret_cast<void *>(&musicCoreQueryConvertedStatus),	/* [ 5] 0x811CB1F6 */
	reinterpret_cast<void *>(&musicCoreSetParam),			/* [ 6] 0x811CB736 */
	reinterpret_cast<void *>(&musicCoreGetParam),			/* [ 7] 0x811CDB26 */
	reinterpret_cast<void *>(&musicCoreDbSync),			/* [ 8] 0x811CDC54 */
	reinterpret_cast<void *>(&musicCoreInterfaceNoop),		/* [ 9] 0x81409E2C */
	reinterpret_cast<void *>(&musicCoreCmdOpen),			/* [10] 0x811CD0E0 */
	reinterpret_cast<void *>(&musicCoreCmdClose),			/* [11] 0x811CD18E */
	reinterpret_cast<void *>(&musicCoreSendOpenUri),		/* [12] 0x811CAD60 */
	reinterpret_cast<void *>(&musicCoreGetCurrentUri),		/* [13] 0x811CD23C */
	reinterpret_cast<void *>(&musicCoreGetChannelError),		/* [14] 0x811CD2A6 */
	reinterpret_cast<void *>(&musicCoreDbOpen),			/* [15] 0x811CDD02 */
	reinterpret_cast<void *>(&musicCoreDbSuspend),			/* [16] 0x811CDDB0 */
	reinterpret_cast<void *>(&musicCoreDbClose),			/* [17] 0x811CDE6C */
	reinterpret_cast<void *>(&musicCoreDbDetach),			/* [18] 0x811CDF1A */
	reinterpret_cast<void *>(&musicCoreDbQuery),			/* [19] 0x811CDFC8 */
	reinterpret_cast<void *>(&musicCoreGetCapability),		/* [20] 0x811CE090 */
	reinterpret_cast<void *>(&musicCoreGetMetadata),		/* [21] 0x811CD384 */
	reinterpret_cast<void *>(&musicCoreGetBgmMute),			/* [22] 0x811CE1A2 */
	reinterpret_cast<void *>(&musicCoreSetBgmMute),			/* [23] 0x811CE1BA */
	reinterpret_cast<void *>(&musicCoreCmdResetChannel),		/* [24] 0x811CE1D2 */
	reinterpret_cast<void *>(&musicCoreGetPlayTime),		/* [25] 0x811CD310 */
	reinterpret_cast<void *>(&musicCoreCmdExOpen),			/* [26] 0x811CD3B8 */
	reinterpret_cast<void *>(&musicCoreCmdExClose),			/* [27] 0x811CD480 */
	reinterpret_cast<void *>(&musicCoreCmdSetAvrcp),		/* [28] 0x811CD52E */
	reinterpret_cast<void *>(&musicCoreCmdSetNoticeSuppress),	/* [29] 0x811CD556 */
	reinterpret_cast<void *>(&musicCoreCmdClearNoticeSuppress),	/* [30] 0x811CD610 */
	reinterpret_cast<void *>(&musicCoreCmdSetAudioFocus),		/* [31] 0x811CD6CA */
	reinterpret_cast<void *>(&musicCoreCmdSetSystemPause),		/* [32] 0x811CD84E */
};

/* 0x811CA5A2 */
void musiccorePluginInit(paf::Plugin *plugin)
{
	musicCoreHeapInit();
	s_musicCorePlugin = plugin;
	plugin->SetInterface(1, s_musicCoreInterface);
	s_eventCallbacks.clear();

	void *mem = musicCoreHeapAlloc(0x68);
	int err = 0;
	s_musicCoreMain = (mem != NULL) ? new (mem) MusicCoreMain(&err) : NULL;
	if (s_musicCoreMain != NULL) {
		err = s_musicCoreMain->SetCallback(reinterpret_cast<MusicCoreNotifyCb>(musicCoreDispatchEventCb));
		if (err == 0)
			return;
	}
	if (s_musicCoreMain != NULL) {
		s_musicCoreMain->ClearCallback(reinterpret_cast<MusicCoreNotifyCb>(musicCoreDispatchEventCb));
		if (s_musicCoreMain != NULL) {
			s_musicCoreMain->~MusicCoreMain();
			operator delete(s_musicCoreMain);
		}
		s_musicCoreMain = NULL;
	}
}

/* 0x811CA68A */
void musiccorePluginStart(paf::Plugin *plugin)
{
	SceShell_83D16CA8();
	SceShell_83D16CA8();
	SceShell_83D16CA8();
	shellAppEventServerAddListener(1, (ShellEventCallback)musicCoreOnAppEvent, NULL);
	SceShell_7FE4E732(2, (void *)musicCoreOnPowerEvent2, NULL);
	SceShell_7FE4E732(3, (void *)musicCoreOnPowerEvent3, NULL);

	paf::Plugin::PageOpenParam openParam;
	paf::IDParam pageId(0x5FCCD9CEu);
	plugin->PageOpen(pageId, openParam);
	paf::ui::Scene *root = plugin->PageRoot(pageId);
	if (root != NULL) {
		paf::ui::EventCBListener *cb = new paf::ui::EventCBListener(
			(paf::ui::HandlerCB)musicCoreOnRemoteKey, NULL);
		((paf::ui::Handler *)root)->AddEventListener(0x10013, cb, 0);
	}
}

/* 0x811CAB12 */
void musiccorePluginStop(paf::Plugin *plugin)
{
	SceShell_E3D3A162(5, (void *)musicCoreOnShellEvent, NULL);
	SceShell_E3D3A162(8, (void *)musicCoreOnShellEvent, NULL);
	SceShell_E3D3A162(7, (void *)musicCoreOnShellEvent, NULL);
	shellAppEventServerRemoveListener(1, (ShellEventCallback)musicCoreOnAppEvent, NULL);
	SceShell_D4C7D2B7(2, (void *)musicCoreOnPowerEvent2, NULL);
	SceShell_D4C7D2B7(3, (void *)musicCoreOnPowerEvent3, NULL);
	plugin->PageCloseAll();
}

/* 0x811CAB74 */
void musicCorePluginFini(void)
{
	if (s_musicCoreMain != NULL) {
		s_musicCoreMain->ClearCallback(reinterpret_cast<MusicCoreNotifyCb>(musicCoreDispatchEventCb));
		if (s_musicCoreMain != NULL) {
			s_musicCoreMain->~MusicCoreMain();
			operator delete(s_musicCoreMain);
		}
		s_musicCoreMain = NULL;
	}
	s_eventCallbacks.clear();
	s_musicCorePlugin = NULL;
	musicCoreHeapFini();
}

void musiccorePluginExit(paf::Plugin *plugin)
{
	(void)plugin;
	musicCorePluginFini();
}

/* 0x811CABEA */
int musicCoreBgmOpen(SceUInt8 mode)
{
	if (s_bgmOpened == 1)
		return (int)0x80101302;
	if (s_musicCorePlugin == NULL || s_musicCorePlugin->GetInterface(1) == NULL)
		return (int)0x80101301;

	sce_paf_memset(&s_bgmCtx, 0, sizeof(s_bgmCtx));
	s_bgmCtx.seqId = 2;
	if (mode == 2)
		s_bgmCtx.channel = 1;
	else if (mode == 3)
		s_bgmCtx.channel = 2;
	else
		return (int)0x80101300;
	s_bgmCtx.subMode = 0;

	SceUInt32 appStatus[32];
	int ret = sceAppMgrGetStatusByName(NULL, appStatus);
	if (ret != 0)
		return ret;
	s_bgmCtx.pid = (int)appStatus[13];
	s_bgmCtx.appId = (int)appStatus[12];

	if (s_bgmCtx.channel > 2)
		return (int)0x80100D01;

	SceUInt32 payload[5];
	sce_paf_memset(payload, 0, sizeof(payload));
	payload[3] = (SceUInt32)s_bgmCtx.pid;
	payload[4] = (SceUInt32)s_bgmCtx.appId;

	if (s_musicCoreMain != NULL) {
		SceUInt32 packed = ((SceUInt32)10 << 16) | s_bgmCtx.channel;
		ret = s_musicCoreMain->SendCmd(packed, s_bgmCtx.seqId, packed,
			payload, sizeof(payload));
		if (ret != 0)
			return ret;
	}
	s_bgmOpened = 1;
	return 0;
}

/* 0x811CAD60 */
int musicCoreSendOpenUri(const MusicCoreClientCtx *ctx, const void *uriBuf, const void *optBuf)
{
	if (ctx->channel > 2)
		return (int)0x80100D01;
	struct OpenUriPayload {
		char uri[1028];
		SceUInt8 opt[16];
		int pid;
		int appId;
	} payload;
	sce_paf_memset(&payload, 0, sizeof(payload));
	sce_paf_memcpy(payload.uri, uriBuf, 0x401);
	sce_paf_memcpy(payload.opt, optBuf, 16);
	payload.pid = ctx->pid;
	payload.appId = ctx->appId;
	if (s_musicCoreMain == NULL)
		return 0;
	SceUInt32 packed = ((SceUInt32)12 << 16) | ctx->channel;
	return s_musicCoreMain->SendCmd(packed, ctx->seqId, packed,
		&payload, sizeof(payload));
}

/* 0x811CAE46 */
int musicCoreBgmLoadUri(const char *uri, const SceUInt8 *opt)
{
	if (s_bgmOpened == 0)
		return (int)0x80101301;
	if (uri == NULL)
		return (int)0x80101300;
	if (opt[4] != 0 && opt[4] != 1)
		return (int)0x80101300;
	if (s_musicCorePlugin == NULL || s_musicCorePlugin->GetInterface(1) == NULL)
		return (int)0x80101301;

	s_bgmCtx.seqId = 4;
	char uriBuf[1028];
	sce_paf_memset(uriBuf, 0, 0x401);
	sce_paf_strncpy(uriBuf, uri, 0x401);
	SceUInt32 optBuf[4];
	sce_paf_memset(optBuf, 0, sizeof(optBuf));
	if (opt != NULL)
		sce_paf_memcpy(optBuf, opt, sizeof(optBuf));
	else
		optBuf[0] = 0;

	int ret = musicCoreSendOpenUri(&s_bgmCtx, uriBuf, optBuf);
	if (ret == 0)
		s_bgmLoaded = 1;
	return ret;
}

/* 0x811CAF32 */
int musicCoreSendControlCmd(const MusicCoreClientCtx *ctx, SceUInt32 ctrlCode, int param)
{
	if (ctx->channel > 2 || ctrlCode > 17)
		return (int)0x80100D01;
	SceUInt32 payload[5];
	sce_paf_memset(payload, 0, sizeof(payload));
	if (s_musicCoreMain == NULL)
		return 0;
	payload[0] = ctrlCode;
	payload[1] = (SceUInt32)param;
	payload[3] = (SceUInt32)ctx->pid;
	payload[4] = (SceUInt32)ctx->appId;
	SceUInt32 packed = ((SceUInt32)2 << 16) | ctx->channel;
	return s_musicCoreMain->SendCmd(20, ctx->seqId, packed,
		payload, sizeof(payload));
}

static SceUInt32 mapControlCode(SceUInt8 cmd)
{
	if (cmd == 1)
		return 2;
	if (cmd == 2)
		return 1;
	return cmd;
}

/* 0x811CB03A */
int musicCoreBgmControl(SceUInt8 cmd, int param)
{
	if (s_bgmOpened == 0)
		return (int)0x80101301;
	if (s_bgmLoaded == 0)
		return (int)0x80101303;
	if (s_musicCorePlugin == NULL || s_musicCorePlugin->GetInterface(1) == NULL)
		return (int)0x80101301;
	if (cmd > 17)
		return (int)0x80101300;
	s_bgmCtx.seqId = 6 + cmd;
	return musicCoreSendControlCmd(&s_bgmCtx, mapControlCode(cmd), param);
}

/* 0x811CB1F6 */
int musicCoreQueryConvertedStatus(const MusicCoreClientCtx *ctx, MusicCoreRawStatus *outStatus)
{
	if (ctx->channel > 2)
		return (int)0x80100D01;
	SceUInt8 rawBuf[144];
	SceUInt8 capBuf[72];
	int flag = 0;
	sce_paf_memset(rawBuf, 0, sizeof(rawBuf));
	sce_paf_memset(capBuf, 0, sizeof(capBuf));
	if (s_musicCoreMain == NULL)
		return 0;
	int ret = s_musicCoreMain->GetStatus(ctx, rawBuf);
	if (ret == 0) {
		ret = s_musicCoreMain->GetPlayTimeFlag(ctx, &flag);
		if (ret == 0) {
			ret = s_musicCoreMain->GetCap(ctx, capBuf);
			if (ret == 0)
				return musicCoreConvertStatus(rawBuf, outStatus, flag, capBuf);
		}
	}
	return ret;
}

/* 0x811CB342 */
int musicCoreBgmGetStatus(MusicCorePublicStatus *outStatus)
{
	if (s_bgmOpened == 0)
		return (int)0x80101301;
	if (outStatus == NULL)
		return (int)0x80101300;
	if (s_musicCorePlugin == NULL || s_musicCorePlugin->GetInterface(1) == NULL)
		return (int)0x80101301;

	s_bgmCtx.seqId = 24;
	MusicCoreRawStatus raw;
	sce_paf_memset(&raw, 0, sizeof(raw));
	int ret = musicCoreQueryConvertedStatus(&s_bgmCtx, &raw);
	if (ret != 0)
		return ret;

	switch (raw.state) {
	case 0: case 1: case 86: case 88: outStatus->state = 0; break;
	case 2:  outStatus->state = 1;  break;
	case 3:  outStatus->state = 2;  break;
	case 4:  outStatus->state = 3;  break;
	case 6:  outStatus->state = 4;  break;
	case 7:  outStatus->state = 5;  break;
	case 8:  outStatus->state = 6;  break;
	case 9:  outStatus->state = 7;  break;
	case 10: outStatus->state = 8;  break;
	case 11: outStatus->state = 9;  break;
	case 12: outStatus->state = 10; break;
	case 13: outStatus->state = 11; break;
	case 14: outStatus->state = 12; break;
	case 15: outStatus->state = 13; break;
	case 16: outStatus->state = 14; break;
	case 17: outStatus->state = 21; break;
	case 18: outStatus->state = 22; break;
	case 19: outStatus->state = 23; break;
	case 20: outStatus->state = 24; break;
	case 21: outStatus->state = 25; break;
	case 22: outStatus->state = 26; break;
	case 23: outStatus->state = 27; break;
	case 25: outStatus->state = 15; break;
	case 26: outStatus->state = 16; break;
	case 27: outStatus->state = 17; break;
	case 28: outStatus->state = 18; break;
	case 29: outStatus->state = 19; break;
	case 30: outStatus->state = 20; break;
	default: return (int)0x80101300;
	}

	outStatus->eventType = raw.eventType;
	outStatus->errorCode = raw.errorCode;
	if (raw.playMode > 3)
		return (int)0x80101300;
	outStatus->playMode = (SceUInt8)raw.playMode;
	outStatus->duration = raw.duration;
	outStatus->posLo = raw.posLo;
	outStatus->posHi = raw.posHi;
	outStatus->sampleRate = (raw.sampleRate >= 1 && raw.sampleRate <= 6) ? raw.sampleRate : 0;
	outStatus->bitrate = raw.bitrate;
	outStatus->channels = raw.channels;
	outStatus->codec = raw.codec;
	outStatus->trackIndex = raw.trackIndex;
	outStatus->trackTotal = raw.trackTotal;
	outStatus->artistId = raw.artistId;
	outStatus->albumId = raw.albumId;
	outStatus->mediaType = raw.mediaType;

	if (raw.repeatMode > 2 || raw.shuffleMode > 1 || raw.eqMode < 0 || raw.eqMode > 4 || raw.avlsMode > 1)
		return (int)0x80101300;
	outStatus->repeatMode = (SceUInt8)raw.repeatMode;
	outStatus->shuffleMode = (SceUInt8)raw.shuffleMode;
	outStatus->eqMode = (SceUInt8)raw.eqMode;
	outStatus->avlsMode = (SceUInt8)raw.avlsMode;
	return 0;
}

/* 0x811CB736 */
int musicCoreSetParam(const MusicCoreClientCtx *ctx, int paramId, const int *values)
{
	if (ctx->channel > 2)
		return (int)0x80100D01;

	SceUInt32 payload[5];
	sce_paf_memset(payload, 0, sizeof(payload));
	SceUInt32 cmdHi = 0;

	switch (paramId) {
	case 0:
		if (values[0] < 0 || values[0] > 0x8000)
			return (int)0x80100D01;
		cmdHi = 3;
		break;
	case 1:
		if (values[0] < 0 || values[0] > 2)
			return (int)0x80100D01;
		cmdHi = 4;
		break;
	case 2:
		if (values[0] != 0 && values[0] != 1)
			return (int)0x80100D01;
		cmdHi = 5;
		break;
	case 3:
		cmdHi = 6;
		break;
	case 4:
		cmdHi = 7;
		break;
	case 5:
		cmdHi = 8;
		break;
	case 6:
		if (values[0] != 0 && values[0] != 1)
			return (int)0x80100D01;
		cmdHi = 9;
		break;
	case 7:
		if (values[0] < 0 || values[0] > 4)
			return (int)0x80100D01;
		cmdHi = 13;
		break;
	case 8:
		if (s_musicCoreMain == NULL)
			return 0;
		payload[0] = (SceUInt32)values[0];
		payload[1] = (SceUInt32)values[1];
		payload[2] = (SceUInt32)values[2];
		payload[3] = (SceUInt32)ctx->pid;
		payload[4] = (SceUInt32)ctx->appId;
		return s_musicCoreMain->SendCmd((SceUInt32)values[2], ctx->seqId,
			((SceUInt32)14 << 16) | ctx->channel, payload, sizeof(payload));
	default:
		return (int)0x80100D01;
	}

	if (s_musicCoreMain == NULL)
		return 0;
	payload[0] = (SceUInt32)values[0];
	payload[3] = (SceUInt32)ctx->pid;
	payload[4] = (SceUInt32)ctx->appId;
	return s_musicCoreMain->SendCmd((SceUInt32)values[0], ctx->seqId,
		(cmdHi << 16) | ctx->channel, payload, sizeof(payload));
}

/* 0x811CBCD6 */
int musicCoreSetVolumeStep(int step)
{
	if (step <= 0 || step > 16)
		return (int)0x80101300;
	if (s_musicCorePlugin == NULL || s_musicCorePlugin->GetInterface(1) == NULL)
		return (int)0x80101301;
	MusicCoreClientCtx ctx;
	sce_paf_memset(&ctx, 0, sizeof(ctx));
	ctx.seqId = 32;
	int vol = kMusicCoreVolumeTable[step];
	return musicCoreSetParam(&ctx, 0, &vol);
}

/* 0x811CBD5E */
int musicPlayerSeek(int mode, int offset, int flags)
{
	if (s_musicCorePlugin == NULL || s_musicCorePlugin->GetInterface(1) == NULL)
		return (int)0x80101301;
	MusicCoreClientCtx ctx;
	sce_paf_memset(&ctx, 0, sizeof(ctx));
	ctx.seqId = 32;
	int args[3] = { mode, offset, flags };
	return musicCoreSetParam(&ctx, 8, args);
}

/* 0x811CBDE6 */
int musicCorePlayerControl(SceUInt8 cmd, int param)
{
	if (s_musicCorePlugin == NULL || s_musicCorePlugin->GetInterface(1) == NULL)
		return (int)0x80101301;
	if (cmd > 17)
		return (int)0x80101300;
	MusicCoreClientCtx ctx;
	sce_paf_memset(&ctx, 0, sizeof(ctx));
	ctx.seqId = 33 + cmd;
	return musicCoreSendControlCmd(&ctx, mapControlCode(cmd), param);
}

/* 0x811CBF10 */
int musicCoreGetImposeInfo(MusicCoreImposeInfo *outInfo)
{
	if (outInfo == NULL)
		return (int)0x80101300;
	if (s_musicCorePlugin == NULL || s_musicCorePlugin->GetInterface(1) == NULL)
		return (int)0x80101301;

	MusicCoreClientCtx ctx;
	sce_paf_memset(&ctx, 0, sizeof(ctx));
	ctx.seqId = 51;

	MusicCoreRawStatus raw;
	sce_paf_memset(&raw, 0, sizeof(raw));
	int ret = musicCoreQueryConvertedStatus(&ctx, &raw);
	if (ret != 0)
		return ret;

	SceUInt32 appStatus[32];
	if (sceAppMgrGetStatusByName("NPXS10009", appStatus) == 0 && appStatus[14] != 0)
		raw.errorCode = 0;

	char metaBuf[3 * 2048];
	sce_paf_memset(metaBuf, 0, sizeof(metaBuf));
	if (s_musicCoreMain != NULL) {
		ret = s_musicCoreMain->GetMeta(&ctx, metaBuf);
		if (ret != 0)
			return ret;
	}

	if (raw.playMode > 3)
		return (int)0x80101300;

	outInfo->volume = raw.volume;
	outInfo->playMode = (SceUInt8)raw.playMode;
	outInfo->bgmReason = (SceUInt8)raw.bgmReason;
	outInfo->bgmPriority = raw.bgmPriority;
	sce_paf_memcpy(outInfo->title, metaBuf + 2048, 2048);
	sce_paf_memcpy(outInfo->artist, metaBuf, 2048);
	sce_paf_memcpy(outInfo->album, metaBuf + 4096, 2048);
	outInfo->canPlay = (SceUInt8)raw.errorCode;
	outInfo->durationLo = (int)(raw.duration & 0xFFFFFFFF);
	outInfo->durationHi = (int)((SceUInt64)raw.duration >> 32);
	outInfo->posLo = raw.posHi;

	int step = 0;
	for (int i = 0; i < 16; ++i) {
		int lo = kMusicCoreVolumeTable[i];
		int hi = kMusicCoreVolumeTable[i + 1];
		if (lo == raw.volume) {
			step = i;
			break;
		}
		if (hi == raw.volume) {
			step = i + 1;
			break;
		}
		if (lo < raw.volume && hi > raw.volume) {
			step = (raw.volume - lo >= hi - raw.volume) ? (i + 1) : i;
			break;
		}
	}
	if (step == 0)
		step = 1;
	outInfo->volStep = step;
	return 0;
}

/* 0x811CC17C */
int musicCoreGetLastError(int *outErr)
{
	if (outErr == NULL)
		return (int)0x80101300;
	if (s_musicCorePlugin == NULL || s_musicCorePlugin->GetInterface(1) == NULL)
		return (int)0x80101301;

	MusicCoreClientCtx ctx;
	sce_paf_memset(&ctx, 0, sizeof(ctx));
	ctx.seqId = 52;

	int rawErr = 0;
	if (s_musicCoreMain != NULL) {
		int ret = s_musicCoreMain->GetError(&ctx, &rawErr);
		if (ret != 0)
			return ret;
	}

	if (rawErr == 0) {
		outErr[0] = 0;
		outErr[1] = 0;
		return 0;
	}
	if (rawErr >= (int)0x80100D02 && rawErr <= (int)0x80100D0A) {
		int mapped = (int)0x80101304 + (rawErr - (int)0x80100D02);
		outErr[0] = mapped;
		outErr[1] = mapped;
		return 0;
	}
	outErr[0] = (int)0x801013FF;
	outErr[1] = rawErr;
	return 0;
}

/* 0x811CC2F8 */
int musicCoreExOpen(const int *openParam)
{
	if (s_exOpened == 1)
		return (int)0x80101302;
	if (openParam == NULL)
		return (int)0x80101300;
	if (s_musicCorePlugin == NULL || s_musicCorePlugin->GetInterface(1) == NULL)
		return (int)0x80101301;

	MusicCoreClientCtx ctx;
	sce_paf_memset(&ctx, 0, sizeof(ctx));
	ctx.seqId = 53;
	ctx.channel = 2;

	SceUInt32 payload[7];
	sce_paf_memset(payload, 0, sizeof(payload));
	sce_paf_memcpy(payload, openParam, 20);
	payload[5] = (SceUInt32)ctx.pid;
	payload[6] = (SceUInt32)ctx.appId;

	if (s_musicCoreMain != NULL) {
		SceUInt32 packed = ((SceUInt32)22 << 16) | 2;
		int ret = s_musicCoreMain->SendCmd(packed, 53, packed,
			payload, sizeof(payload));
		if (ret != 0)
			return ret;
	}
	s_exOpened = 1;
	return 0;
}

/* 0x811CC420 */
int musicCoreExClose(void)
{
	if (s_exOpened == 0)
		return (int)0x80101301;
	if (s_musicCorePlugin == NULL || s_musicCorePlugin->GetInterface(1) == NULL)
		return (int)0x80101301;

	MusicCoreClientCtx ctx;
	sce_paf_memset(&ctx, 0, sizeof(ctx));
	ctx.seqId = 54;
	ctx.channel = 2;

	SceUInt32 payload[5];
	sce_paf_memset(payload, 0, sizeof(payload));
	payload[3] = (SceUInt32)ctx.pid;
	payload[4] = (SceUInt32)ctx.appId;

	if (s_musicCoreMain != NULL) {
		SceUInt32 packed = ((SceUInt32)23 << 16) | 2;
		int ret = s_musicCoreMain->SendCmd(packed, 54, packed,
			payload, sizeof(payload));
		if (ret != 0)
			return ret;
	}
	s_exLoaded = 0;
	s_exOpened = 0;
	return 0;
}

/* 0x811CC51A */
int musicCoreExLoadUri(const char *uri, const void *opt)
{
	if (s_exOpened == 0)
		return (int)0x80101301;
	if (uri == NULL)
		return (int)0x80101300;
	if (s_musicCorePlugin == NULL || s_musicCorePlugin->GetInterface(1) == NULL)
		return (int)0x80101301;

	MusicCoreClientCtx ctx;
	sce_paf_memset(&ctx, 0, sizeof(ctx));
	ctx.seqId = 55;
	ctx.channel = 2;

	char uriBuf[1028];
	sce_paf_memset(uriBuf, 0, 0x401);
	sce_paf_strncpy(uriBuf, uri, 0x401);
	SceUInt32 optBuf[4];
	sce_paf_memset(optBuf, 0, sizeof(optBuf));
	if (opt != NULL)
		sce_paf_memcpy(optBuf, opt, sizeof(optBuf));
	else
		optBuf[0] = 0;

	int ret = musicCoreSendOpenUri(&ctx, uriBuf, optBuf);
	if (ret == 0)
		s_exLoaded = 1;
	return ret;
}

/* 0x811CC5FC */
int musicCoreExControl(SceUInt8 cmd, int param)
{
	if (s_exOpened == 0)
		return (int)0x80101301;
	if (s_exLoaded == 0)
		return (int)0x80101303;
	if (s_musicCorePlugin == NULL || s_musicCorePlugin->GetInterface(1) == NULL)
		return (int)0x80101301;
	if (cmd > 17)
		return (int)0x80101300;
	MusicCoreClientCtx ctx;
	sce_paf_memset(&ctx, 0, sizeof(ctx));
	ctx.seqId = 57 + cmd;
	ctx.channel = 2;
	return musicCoreSendControlCmd(&ctx, mapControlCode(cmd), param);
}

/* 0x811CC754 */
int musicCoreExGetStatus(MusicCorePublicStatus *outStatus)
{
	if (s_exOpened == 0)
		return (int)0x80101301;
	if (outStatus == NULL)
		return (int)0x80101300;
	if (s_musicCorePlugin == NULL || s_musicCorePlugin->GetInterface(1) == NULL)
		return (int)0x80101301;

	MusicCoreClientCtx ctx;
	sce_paf_memset(&ctx, 0, sizeof(ctx));
	ctx.seqId = 75;
	ctx.channel = 2;

	MusicCoreRawStatus raw;
	sce_paf_memset(&raw, 0, sizeof(raw));
	int ret = musicCoreQueryConvertedStatus(&ctx, &raw);
	if (ret != 0)
		return ret;

	switch (raw.state) {
	case 1:  outStatus->state = 0;  break;
	case 2:  outStatus->state = 1;  break;
	case 3:  outStatus->state = 2;  break;
	case 55: outStatus->state = 3;  break;
	case 57: outStatus->state = 4;  break;
	case 58: outStatus->state = 5;  break;
	case 59: outStatus->state = 6;  break;
	case 60: outStatus->state = 7;  break;
	case 61: outStatus->state = 8;  break;
	case 62: outStatus->state = 9;  break;
	case 63: outStatus->state = 10; break;
	case 64: outStatus->state = 11; break;
	case 65: outStatus->state = 12; break;
	case 66: outStatus->state = 13; break;
	case 67: outStatus->state = 14; break;
	case 68: outStatus->state = 21; break;
	case 69: outStatus->state = 22; break;
	case 70: outStatus->state = 23; break;
	case 71: outStatus->state = 24; break;
	case 72: outStatus->state = 25; break;
	case 73: outStatus->state = 26; break;
	case 74: outStatus->state = 27; break;
	case 76: outStatus->state = 15; break;
	case 77: outStatus->state = 16; break;
	case 78: outStatus->state = 17; break;
	case 79: outStatus->state = 18; break;
	case 80: outStatus->state = 19; break;
	case 81: outStatus->state = 20; break;
	default: outStatus->state = raw.state; break;
	}

	outStatus->eventType = raw.eventType;
	outStatus->errorCode = raw.errorCode;
	if (raw.playMode > 3)
		return (int)0x80101300;
	outStatus->playMode = (SceUInt8)raw.playMode;
	outStatus->duration = raw.duration;
	outStatus->posLo = raw.posLo;
	outStatus->posHi = raw.posHi;
	outStatus->trackTotal = raw.trackTotal;
	outStatus->artistId = raw.artistId;
	outStatus->albumId = raw.albumId;
	outStatus->mediaType = raw.mediaType;

	if (raw.repeatMode > 2 || raw.shuffleMode > 1 || raw.eqMode < 0 || raw.eqMode > 4 || raw.avlsMode > 1)
		return (int)0x80101300;
	outStatus->repeatMode = (SceUInt8)raw.repeatMode;
	outStatus->shuffleMode = (SceUInt8)raw.shuffleMode;
	outStatus->eqMode = (SceUInt8)raw.eqMode;
	outStatus->avlsMode = (SceUInt8)raw.avlsMode;
	return 0;
}

/* 0x811CCABC */
int musicCoreExSetRepeat(SceUInt8 mode)
{
	if (s_exOpened == 0)
		return (int)0x80101301;
	if (s_musicCorePlugin == NULL || s_musicCorePlugin->GetInterface(1) == NULL)
		return (int)0x80101301;
	if (mode > 2)
		return (int)0x80101300;
	int val = mode;
	MusicCoreClientCtx ctx;
	sce_paf_memset(&ctx, 0, sizeof(ctx));
	ctx.seqId = 77;
	ctx.channel = 2;
	return musicCoreSetParam(&ctx, 1, &val);
}

/* 0x811CCB70 */
int musicCoreEnableAvrcp(void)
{
	if (s_musicCorePlugin == NULL || s_musicCorePlugin->GetInterface(1) == NULL)
		return (int)0x80101301;
	MusicCoreClientCtx ctx;
	sce_paf_memset(&ctx, 0, sizeof(ctx));
	return (s_musicCoreMain != NULL) ? s_musicCoreMain->SetAvrcpActive(&ctx, 1) : 0;
}

/* 0x811CCBD8 */
int musicCoreDisableAvrcp(void)
{
	if (s_musicCorePlugin == NULL || s_musicCorePlugin->GetInterface(1) == NULL)
		return (int)0x80101301;
	MusicCoreClientCtx ctx;
	sce_paf_memset(&ctx, 0, sizeof(ctx));
	return (s_musicCoreMain != NULL) ? s_musicCoreMain->SetAvrcpActive(&ctx, 0) : 0;
}

/* 0x811CCC40 */
int musicCoreSetMute1(SceUInt8 mute)
{
	if (s_musicCorePlugin == NULL || s_musicCorePlugin->GetInterface(1) == NULL)
		return (int)0x80101301;
	if (mute > 1)
		return (int)0x80101300;
	MusicCoreClientCtx ctx;
	sce_paf_memset(&ctx, 0, sizeof(ctx));
	SceUInt8 payload[20];
	sce_paf_memset(payload, 0, sizeof(payload));
	if (s_musicCoreMain == NULL)
		return 0;
	int ret = s_musicCoreMain->SetErrorSuppression(&ctx, mute);
	if (ret == 0)
		return s_musicCoreMain->SendCmd(ctx.seqId, ctx.seqId, 0x190000,
			payload, sizeof(payload));
	return ret;
}

/* 0x811CCD36 */
int musicCoreSetMute2(SceUInt8 mute)
{
	if (s_musicCorePlugin == NULL || s_musicCorePlugin->GetInterface(1) == NULL)
		return (int)0x80101301;
	if (mute > 1)
		return (int)0x80101300;
	MusicCoreClientCtx ctx;
	sce_paf_memset(&ctx, 0, sizeof(ctx));
	SceUInt8 payload[20];
	sce_paf_memset(payload, 0, sizeof(payload));
	if (s_musicCoreMain == NULL)
		return 0;
	int ret = s_musicCoreMain->ClearErrorSuppression(&ctx, mute);
	if (ret == 0)
		return s_musicCoreMain->SendCmd(ctx.seqId, ctx.seqId, 0x190000,
			payload, sizeof(payload));
	return ret;
}

/* 0x811CCE2C */
int musicCoreSetAudioFocus(SceUInt8 mode)
{
	if (s_musicCorePlugin == NULL || s_musicCorePlugin->GetInterface(1) == NULL)
		return (int)0x80101301;
	if (mode > 3)
		return (int)0x80101300;
	MusicCoreClientCtx ctx;
	sce_paf_memset(&ctx, 0, sizeof(ctx));
	SceUInt32 payload[5];
	sce_paf_memset(payload, 0, sizeof(payload));
	SceUInt32 packed = 0;
	if (mode == 0) {
		packed = 25u << 16;
	} else if (mode == 1) {
		packed = 29u << 16;
	} else if (mode == 2) {
		packed = 30u << 16;
		payload[0] = 2;
		payload[3] = (SceUInt32)ctx.pid;
		payload[4] = (SceUInt32)ctx.appId;
	} else {
		packed = 31u << 16;
	}
	if (s_musicCoreMain == NULL)
		return 0;
	return s_musicCoreMain->SendCmd((SceUInt32)(SceUIntPtr)payload, ctx.seqId,
		packed, payload, sizeof(payload));
}

int shellHomeUpdateRequest(SceUInt8 mode)
{
	return musicCoreSetAudioFocus(mode);
}

/* 0x811CD000 */
int musicCoreSetSystemPause(SceUInt8 pause)
{
	if (s_musicCorePlugin == NULL || s_musicCorePlugin->GetInterface(1) == NULL)
		return (int)0x80101301;
	if (pause > 1)
		return (int)0x80101300;
	MusicCoreClientCtx ctx;
	sce_paf_memset(&ctx, 0, sizeof(ctx));
	SceUInt32 payload[5];
	sce_paf_memset(payload, 0, sizeof(payload));
	if (s_musicCoreMain == NULL)
		return 0;
	payload[0] = pause;
	return s_musicCoreMain->SendCmd(0x1A0001, ctx.seqId, 0x1A0001,
		payload, sizeof(payload));
}

int shellMusicStart(int mode)
{
	return musicCoreBgmOpen(static_cast<SceUInt8>(mode));
}

int shellMusicSetBgm(bool on)
{
	return musicCoreSetSystemPause(on ? 1 : 0);
}

static int musicCoreSendSimpleCmd(const MusicCoreClientCtx *ctx, SceUInt16 cmdId)
{
	if (ctx->channel > 2)
		return (int)0x80100D01;
	SceUInt32 payload[5];
	sce_paf_memset(payload, 0, sizeof(payload));
	if (s_musicCoreMain == NULL)
		return 0;
	payload[3] = (SceUInt32)ctx->pid;
	payload[4] = (SceUInt32)ctx->appId;
	SceUInt32 packed = ((SceUInt32)cmdId << 16) | ctx->channel;
	return s_musicCoreMain->SendCmd(packed, ctx->seqId, packed,
		payload, sizeof(payload));
}

/* 0x811CD0E0 */
int musicCoreCmdOpen(const MusicCoreClientCtx *ctx) { return musicCoreSendSimpleCmd(ctx, 10); }
/* 0x811CD18E */
int musicCoreCmdClose(const MusicCoreClientCtx *ctx) { return musicCoreSendSimpleCmd(ctx, 11); }

/* 0x811CD23C */
int musicCoreGetCurrentUri(const MusicCoreClientCtx *ctx, char *outUri)
{
	char rawUri[1028];
	sce_paf_memset(rawUri, 0, 0x401);
	if (s_musicCoreMain == NULL)
		return 0;
	int ret = s_musicCoreMain->GetUri(ctx, rawUri);
	if (ret == 0)
		return musicCoreConvertUri(rawUri, outUri);
	return ret;
}

/* 0x811CD2A6 */
int musicCoreGetChannelError(const MusicCoreClientCtx *ctx, int *outErr)
{
	if (ctx->channel > 2)
		return (int)0x80100D01;
	if (s_musicCoreMain == NULL)
		return 0;
	int rawErr = 0;
	int ret = s_musicCoreMain->GetError(ctx, &rawErr);
	if (ret == 0) {
		outErr[0] = ctx->channel;
		outErr[1] = rawErr;
		outErr[2] = 0;
	}
	return ret;
}

/* 0x811CD310 */
int musicCoreGetPlayTime(const MusicCoreClientCtx *ctx, int *outTime)
{
	if (ctx->channel > 2)
		return (int)0x80100D01;
	int buf[3];
	sce_paf_memset(buf, 0, sizeof(buf));
	if (s_musicCoreMain == NULL)
		return 0;
	int ret = s_musicCoreMain->GetPlayTimeEntry(ctx, buf);
	if (ret == 0) {
		outTime[0] = buf[0];
		outTime[1] = buf[1];
		outTime[2] = buf[2];
	}
	return ret;
}

/* 0x811CD384 */
int musicCoreGetMetadata(const MusicCoreClientCtx *ctx, void *outMeta)
{
	if (ctx->channel > 2)
		return (int)0x80100D01;
	if (s_musicCoreMain == NULL)
		return 0;
	return s_musicCoreMain->GetMeta(ctx, static_cast<char *>(outMeta));
}

/* 0x811CD3B8 */
int musicCoreCmdExOpen(const MusicCoreClientCtx *ctx, const void *openParam)
{
	if (ctx->channel > 2)
		return (int)0x80100D01;
	SceUInt32 payload[7];
	sce_paf_memset(payload, 0, sizeof(payload));
	sce_paf_memcpy(payload, openParam, 20);
	payload[5] = (SceUInt32)ctx->pid;
	payload[6] = (SceUInt32)ctx->appId;
	if (s_musicCoreMain == NULL)
		return 0;
	SceUInt32 packed = ((SceUInt32)22 << 16) | ctx->channel;
	return s_musicCoreMain->SendCmd(packed, ctx->seqId, packed,
		payload, sizeof(payload));
}

/* 0x811CD480 */
int musicCoreCmdExClose(const MusicCoreClientCtx *ctx) { return musicCoreSendSimpleCmd(ctx, 23); }

/* 0x811CD52E */
int musicCoreCmdSetAvrcp(const MusicCoreClientCtx *ctx, SceUInt8 enable)
{
	if (s_musicCoreMain == NULL)
		return 0;
	return s_musicCoreMain->SetAvrcpActive(ctx, (enable == 1));
}

/* 0x811CD556 */
int musicCoreCmdSetNoticeSuppress(const MusicCoreClientCtx *ctx, SceUInt8 mute)
{
	if (mute > 1)
		return (int)0x80100D01;
	SceUInt8 payload[20];
	sce_paf_memset(payload, 0, sizeof(payload));
	if (s_musicCoreMain == NULL)
		return 0;
	int ret = s_musicCoreMain->SetErrorSuppression(ctx, mute);
	if (ret == 0)
		return s_musicCoreMain->SendCmd(ctx->seqId, ctx->seqId, 0x190000,
			payload, sizeof(payload));
	return ret;
}

/* 0x811CD610 */
int musicCoreCmdClearNoticeSuppress(const MusicCoreClientCtx *ctx, SceUInt8 mute)
{
	if (mute > 1)
		return (int)0x80100D01;
	SceUInt8 payload[20];
	sce_paf_memset(payload, 0, sizeof(payload));
	if (s_musicCoreMain == NULL)
		return 0;
	int ret = s_musicCoreMain->ClearErrorSuppression(ctx, mute);
	if (ret == 0)
		return s_musicCoreMain->SendCmd(ctx->seqId, ctx->seqId, 0x190000,
			payload, sizeof(payload));
	return ret;
}

/* 0x811CD6CA */
int musicCoreCmdSetAudioFocus(const MusicCoreClientCtx *ctx, SceUInt8 mode)
{
	if (mode > 3)
		return (int)0x80100D01;
	SceUInt32 payload[5];
	sce_paf_memset(payload, 0, sizeof(payload));
	SceUInt32 packed = 0;
	if (mode == 0) {
		packed = 25u << 16;
	} else if (mode == 1) {
		packed = 29u << 16;
	} else if (mode == 2) {
		packed = 30u << 16;
		payload[0] = 2;
		payload[3] = (SceUInt32)ctx->pid;
		payload[4] = (SceUInt32)ctx->appId;
	} else {
		packed = 31u << 16;
	}
	if (s_musicCoreMain == NULL)
		return 0;
	return s_musicCoreMain->SendCmd((SceUInt32)(SceUIntPtr)payload, ctx->seqId,
		packed, payload, sizeof(payload));
}

/* 0x811CD84E */
int musicCoreCmdSetSystemPause(const MusicCoreClientCtx *ctx, SceUInt8 pause)
{
	if (pause > 1)
		return (int)0x80100D01;
	SceUInt32 payload[5];
	sce_paf_memset(payload, 0, sizeof(payload));
	if (s_musicCoreMain == NULL)
		return 0;
	payload[0] = pause;
	return s_musicCoreMain->SendCmd(0x1A0001, ctx->seqId, 0x1A0001,
		payload, sizeof(payload));
}

/* 0x811CD8EA */
static int musicCoreDispatchEventCb(int eventId, int eventData, int eventDetail, int eventFlags)
{
	for (paf::list<MusicCoreEventCb>::iterator it = s_eventCallbacks.begin();
	     it != s_eventCallbacks.end(); ++it) {
		if (*it != NULL)
			(*it)(eventId, eventData, eventDetail, eventFlags);
	}
	return 0;
}

/* 0x811CD952 */
int musicCoreRegisterCallback(int unused, MusicCoreEventCb cb)
{
	(void)unused;
	if (cb != NULL)
		s_eventCallbacks.push_back(cb);
	return 0;
}

/* 0x811CD98A */
int musicCoreUnregisterCallback(int unused, MusicCoreEventCb cb)
{
	(void)unused;
	for (paf::list<MusicCoreEventCb>::iterator it = s_eventCallbacks.begin();
	     it != s_eventCallbacks.end(); ) {
		if (*it == cb)
			it = s_eventCallbacks.erase(it);
		else
			++it;
	}
	return 0;
}

/* 0x811CD9D8 */
int musicCoreCmdSetContent(const MusicCoreClientCtx *ctx, const void *content)
{
	if (ctx->channel > 2)
		return (int)0x80100D01;
	struct SetContentPayload {
		SceUInt8 content[0x828];
		int pid;
		int appId;
	} payload;
	sce_paf_memset(&payload, 0, sizeof(payload));
	sce_paf_memcpy(payload.content, content, 0x828);
	payload.pid = ctx->pid;
	payload.appId = ctx->appId;
	if (s_musicCoreMain == NULL)
		return 0;
	SceUInt32 packed = ((SceUInt32)1 << 16) | ctx->channel;
	return s_musicCoreMain->SendCmd(packed, ctx->seqId, packed,
		&payload, sizeof(payload));
}

/* 0x811CDAAE */
int musicCoreGetTrackInfo(const MusicCoreClientCtx *ctx, char *outInfo)
{
	char rawTrack[0x828];
	SceUInt8 rawStatus[152];
	sce_paf_memset(rawTrack, 0, sizeof(rawTrack));
	sce_paf_memset(rawStatus, 0, 144);
	if (s_musicCoreMain == NULL)
		return 0;
	int ret = s_musicCoreMain->GetTrackInfo(ctx, rawTrack, rawStatus);
	if (ret == 0)
		return musicCoreConvertTrackInfo(rawTrack, rawStatus, outInfo);
	return ret;
}

/* 0x811CDB26 */
int musicCoreGetParam(const MusicCoreClientCtx *ctx, int paramId, int *outVal)
{
	MusicCoreRawStatus raw;
	sce_paf_memset(&raw, 0, sizeof(raw));
	if (s_musicCoreMain == NULL)
		return 0;
	int ret = 0;
	switch (paramId) {
	case 0:
		ret = s_musicCoreMain->GetStatus(ctx, &raw);
		if (ret == 0)
			*outVal = raw.volume;
		break;
	case 1:
		ret = s_musicCoreMain->GetStatus(ctx, &raw);
		if (ret == 0) {
			int mapped = 0;
			ret = musicCoreConvertRepeat((int)raw.shuffleMode, &mapped);
			if (ret == 0)
				*outVal = mapped;
		}
		break;
	case 2:
		ret = s_musicCoreMain->GetStatus(ctx, &raw);
		if (ret == 0) {
			int mapped = 0;
			ret = musicCoreConvertShuffle(raw.repeatMode, &mapped);
			if (ret == 0)
				*outVal = mapped;
		}
		break;
	case 3:
		ret = s_musicCoreMain->GetStatus(ctx, &raw);
		if (ret == 0) {
			int mapped = 0;
			ret = musicCoreConvertEq((int)(raw.playlistId >> 32), &mapped);
			if (ret == 0)
				*outVal = mapped;
		}
		break;
	case 4:
		ret = s_musicCoreMain->GetStatus(ctx, &raw);
		if (ret == 0) {
			int mapped = 0;
			ret = musicCoreConvertAvls((int)raw.avlsMode, 0, 0, 0, &mapped);
			if (ret == 0)
				*outVal = mapped;
		}
		break;
	default:
		return (int)0x80100D01;
	}
	return ret;
}

/* 0x811CDC54 */
int musicCoreDbSync(const MusicCoreClientCtx *ctx) { return musicCoreSendSimpleCmd(ctx, 20); }
/* 0x811CDD02 */
int musicCoreDbOpen(const MusicCoreClientCtx *ctx) { return musicCoreSendSimpleCmd(ctx, 15); }

/* 0x811CDDB0 */
int musicCoreDbSuspend(const MusicCoreClientCtx *ctx, SceUInt8 flag)
{
	if (ctx->channel > 2)
		return (int)0x80100D01;
	SceUInt32 payload[5];
	sce_paf_memset(payload, 0, sizeof(payload));
	if (s_musicCoreMain == NULL)
		return 0;
	payload[0] = flag;
	payload[3] = (SceUInt32)ctx->pid;
	payload[4] = (SceUInt32)ctx->appId;
	SceUInt32 packed = ((SceUInt32)16 << 16) | ctx->channel;
	return s_musicCoreMain->SendCmd(packed, ctx->seqId, packed,
		payload, sizeof(payload));
}

/* 0x811CDE6C */
int musicCoreDbClose(const MusicCoreClientCtx *ctx) { return musicCoreSendSimpleCmd(ctx, 17); }
/* 0x811CDF1A */
int musicCoreDbDetach(const MusicCoreClientCtx *ctx) { return musicCoreSendSimpleCmd(ctx, 18); }

/* 0x811CDFC8 */
int musicCoreDbQuery(const MusicCoreClientCtx *ctx, const void *data)
{
	if (ctx->channel > 2)
		return (int)0x80100D01;
	struct DbQueryPayload {
		SceUInt8 data[0xB0];
		int pid;
		int appId;
	} payload;
	sce_paf_memset(&payload, 0, sizeof(payload));
	sce_paf_memcpy(payload.data, data, 0xB0);
	payload.pid = ctx->pid;
	payload.appId = ctx->appId;
	if (s_musicCoreMain == NULL)
		return 0;
	SceUInt32 packed = ((SceUInt32)19 << 16) | ctx->channel;
	return s_musicCoreMain->SendCmd(packed, ctx->seqId, packed,
		&payload, sizeof(payload));
}

/* 0x811CE090 */
int musicCoreGetCapability(const MusicCoreClientCtx *ctx, void *outCap)
{
	if (ctx->channel > 2)
		return (int)0x80100D01;
	SceUInt8 capBuf[72];
	sce_paf_memset(capBuf, 0, sizeof(capBuf));
	if (s_musicCoreMain == NULL)
		return 0;
	int ret = s_musicCoreMain->GetCap(ctx, capBuf);
	if (ret == 0)
		return musicCoreConvertCapFlag(static_cast<SceInt8>(capBuf[8]),
					       static_cast<int *>(outCap));
	return ret;
}

/* 0x811CE1A2 */
int musicCoreGetBgmMute(int mode, void *outState)
{
	return s_musicCoreMain->GetBgmSlot(mode, outState);
}

/* 0x811CE1BA */
int musicCoreSetBgmMute(int mode, int enable)
{
	return s_musicCoreMain->GetAllAppSlots(mode, reinterpret_cast<void *>(enable));
}

/* 0x811CE1D2 */
int musicCoreCmdResetChannel(const MusicCoreClientCtx *ctx)
{
	MusicCoreClientCtx copy = *ctx;
	if (copy.channel > 2)
		return (int)0x80100D01;
	int ret = musicCoreSendSimpleCmd(&copy, 17);
	if (ret != 0)
		return ret;
	return musicCoreSendSimpleCmd(ctx, 21);
}

struct ShellAppEventInfo {
	SceUInt32 eventWords[2];
	const void *titleIdPtr;
};

struct RemoteKeyEventInfo {
	SceUInt32 eventWords[2];
	SceUInt32 keyCode;
};

/* 0x811CE316 */
static void musicCoreOnShellEvent(int eventId, const void *info)
{
	MusicCoreAppEventPayload payload;
	sce_paf_memset(&payload, 0, sizeof(payload));
	if (eventId == 5)
		payload.type = 1;
	else if (eventId == 7)
		payload.type = 2;
	else if (eventId == 8)
		payload.type = 3;
	else
		return;
	const ShellAppEventInfo *appInfo = static_cast<const ShellAppEventInfo *>(info);
	sce_paf_memcpy(payload.titleId, appInfo->titleIdPtr, 32);
	if (s_musicCoreMain != NULL)
		s_musicCoreMain->SendCmd(36, 0, 24u << 16, &payload, sizeof(payload));
}

/* 0x811CE3AE */
static void musicCoreOnAppEvent(int eventType, const void *titleId)
{
	if (eventType == 1 &&
	    sce_paf_strncmp((const char *)titleId, "NPXS19999", sce_paf_strlen("NPXS19999")) == 0) {
		MusicCoreAppEventPayload payload;
		sce_paf_memset(&payload, 0, sizeof(payload));
		payload.type = 0;
		sce_paf_memcpy(payload.titleId, titleId, 32);
		if (s_musicCoreMain != NULL)
			s_musicCoreMain->SendCmd(0x180000, 0, 0x180000,
				&payload, sizeof(payload));
	}
}

/* 0x811CE45A */
static void musicCoreOnPowerEvent2(int eventId)
{
	if (eventId == 2) {
		SceUInt8 payload[20];
		sce_paf_memset(payload, 0, sizeof(payload));
		if (s_musicCoreMain != NULL)
			s_musicCoreMain->SendCmd(0x1B0001, 0, 0x1B0001,
				payload, sizeof(payload));
	}
}

/* 0x811CE4D2 */
static void musicCoreOnPowerEvent3(int eventId)
{
	if (eventId == 3) {
		SceUInt8 payload[20];
		sce_paf_memset(payload, 0, sizeof(payload));
		if (s_musicCoreMain != NULL)
			s_musicCoreMain->SendCmd(0x1C0001, 0, 0x1C0001,
				payload, sizeof(payload));
	}
}

/* 0x811CE54A */
static void musicCoreOnRemoteKey(int eventId, int handler, const void *ev)
{
	(void)eventId;
	(void)handler;
	MusicCoreClientCtx ctx;
	sce_paf_memset(&ctx, 0, sizeof(ctx));
	ctx.seqId = 156;
	const RemoteKeyEventInfo *keyEv = static_cast<const RemoteKeyEventInfo *>(ev);
	SceUInt8 keyCode = static_cast<SceUInt8>(keyEv->keyCode);
	SceUInt32 ctrlCode;
	switch (keyCode) {
	case 0: ctrlCode = 0; break;
	case 1: ctrlCode = 1; break;
	case 2: ctrlCode = 2; break;
	case 3: ctrlCode = 3; break;
	case 4: ctrlCode = 4; break;
	case 5: ctrlCode = 11; break;
	case 6: case 7: case 9: case 10: ctrlCode = 17; break;
	case 8: ctrlCode = 14; break;
	default: return;
	}
	musicCoreSendControlCmd(&ctx, ctrlCode, 0);
}
