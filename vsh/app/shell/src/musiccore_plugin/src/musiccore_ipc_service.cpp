/*
 * SceShell musiccore_plugin - IPMI IPC services for MusicApp (0xD0000..0xD000E)
 * and MusicCore (0x30000..0x3001B).
 * Original address range (3.65-CEX): 0x81209B58 - 0x8120CEB3
 */

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/string.h>
#include <plugin.h>
#include <ipmi/session.h>

extern "C" {
extern int shellSvcLiveAreaSyncDefault(IPMI::Session *session, SceUInt32 reqId,
	SceUInt32 methodId, const IPMI::DataInfo *inBufs, SceUInt32 inCount); /* 0x8115B554 */
}

namespace {

static const int MUSICAPP_ERR_INVALID_ARG = (int)0x80101900;
static const int MUSICAPP_ERR_NOT_AVAILABLE = (int)0x80101901;
static const int MUSICAPP_ERR_GENERAL = (int)0x801019FF;

static const int MUSICCORE_ERR_INVALID_ARG = (int)0x80100E00;
static const int MUSICCORE_ERR_NOT_AVAILABLE = (int)0x80100E01;
static const int MUSICCORE_ERR_GENERAL = (int)0x80100EFF;

static const int MUSICCORE_IPMI_UNSUPPORTED = (int)0x800205C1;

struct MusicCoreIpcClientCtx {
	int seqId;						/* +0 */
	SceUInt8 channel;					/* +4 */
	SceUInt8 subMode;					/* +5 */
	SceUInt8 padChannel[2];					/* +6 */
	int pid;						/* +8 */
	int appId;						/* +12 */
};

struct MusicCoreIpcHandle {
	int handleId;						/* +0 */
	SceUInt8 channelKind;					/* +4 */
	SceUInt8 padKind[3];					/* +5 */
	int priority;						/* +8 */
};

struct MusicCoreRawStatus {
	SceUInt32 channelType;					/* +0 */
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
	SceUInt32 volume;					/* +116 */
	SceInt64 trackItemId;					/* +120 */
	SceInt64 playlistId;					/* +128 */
	SceUInt32 isPlayable;					/* +136 */
	int seqNum;						/* +140 */
};

struct MusicAppIpcStatus {
	SceUInt8 channelType;					/* +0 */
	SceUInt8 padChannel[3];					/* +1 */
	int state;						/* +4 */
	int eventType;						/* +8 */
	int errorCode;						/* +12 */
	SceUInt8 playMode;					/* +16 */
	SceUInt8 padPlayMode[7];				/* +17 */
	SceInt64 duration;					/* +24 */
	int posLo;						/* +32 */
	int posHi;						/* +36 */
	int sampleRate;						/* +40 */
	int artistId;						/* +44 */
	int albumId;						/* +48 */
	int audioBalance;					/* +52 */
	SceUInt8 repeatMode;					/* +56 */
	SceUInt8 shuffleMode;					/* +57 */
	SceUInt8 eqMode;					/* +58 */
	SceUInt8 avlsMode;					/* +59 */
	SceUInt8 padTail[4];					/* +60 */
};

struct MusicCoreIpcStatus {
	int headerLow;						/* +0 */
	int headerHigh;						/* +4 */
	int errorCode;						/* +8 */
	int reservedHeader;					/* +12 */
	int playMode;						/* +16 */
	int bgmReason;						/* +20 */
	int bgmPriority;					/* +24 */
	int padDuration;					/* +28 */
	SceInt64 duration;					/* +32 */
	int posLo;						/* +40 */
	int posHi;						/* +44 */
	int sampleRate;						/* +48 */
	int artistId;						/* +52 */
	int albumId;						/* +56 */
	int audioBalance;					/* +60 */
	int shuffleMode;					/* +64 */
	int repeatMode;						/* +68 */
	int eqMode;						/* +72 */
	int avlsMode;						/* +76 */
	int volume;						/* +80 */
	int padTrackId;						/* +84 */
	SceInt64 trackItemId;					/* +88 */
	SceInt64 playlistId;					/* +96 */
	int isPlayable;						/* +104 */
	int seqNum;						/* +108 */
};

struct MusicCoreContentParam {
	char uri[2052];						/* +0 */
	int contentIdLow;					/* +2052 */
	int contentIdHigh;					/* +2056 */
	int contentFlags;					/* +2060 */
	int trackIndex;						/* +2064 */
	int trackTotal;						/* +2068 */
	SceUInt32 shuffleMode;					/* +2072 */
	SceUInt32 playPauseMode;				/* +2076 */
	SceUInt32 repeatMode;					/* +2080 */
	int reservedTail;					/* +2084 */
};

struct MusicCoreDbQueryParam {
	SceUInt32 count;					/* +0 */
	SceUInt32 reservedCount;				/* +4 */
	SceUInt64 itemIds[20];					/* +8 */
	SceUInt32 queryMode;					/* +168 */
	SceUInt32 reservedTail;					/* +172 */
};

struct MusicCoreChannelError {
	int eventCode;						/* +0 */
	int errorCode;						/* +4 */
	int commandTag;						/* +8 */
};

struct MusicAppErrorOut {
	int mappedError;					/* +0 */
	int detailError;					/* +4 */
};

struct MusicCorePlayTimeOut {
	int mappedEvent;					/* +0 */
	int detailError;					/* +4 */
	int rawEvent;						/* +8 */
};

struct MusicCoreRawMetadata {
	char artist[2048];					/* +0 */
	char title[2048];					/* +2048 */
	char album[2048];					/* +4096 */
};

struct MusicCoreMetadataOut {
	int durationLo;						/* +0 */
	int durationHi;						/* +4 */
	int sampleRate;						/* +8 */
	char title[2048];					/* +12 */
	char artist[2048];					/* +2060 */
	char album[2048];					/* +4108 */
	int reservedTail;					/* +6156 */
};

struct MusicCoreAppSlot {
	int active;						/* +0 */
	int pid;						/* +4 */
	int appId;						/* +8 */
	SceUInt32 slotWords[9];					/* +12..+47 */
};

struct MusicCoreSeekParam {
	int mode;						/* +0 */
	int offset;						/* +4 */
	int flags;						/* +8 */
};

struct MusicAppControlReq {
	SceUInt8 controlCode;					/* +0 */
	SceUInt8 padControl[3];					/* +1 */
	int controlArg;						/* +4 */
};

struct MusicCoreControlReq {
	int controlCode;					/* +0 */
	int controlArg;						/* +4 */
};

struct MusicCorePluginInterface {
	int (*registerCallback)(int unused, void *cb);					/* +0 */
	int (*unregisterCallback)(int unused, void *cb);				/* +4 */
	int (*setContent)(const MusicCoreIpcClientCtx *ctx, const void *content);	/* +8 */
	int (*getTrackInfo)(const MusicCoreIpcClientCtx *ctx, void *outInfo);		/* +12 */
	int (*sendControlCmd)(const MusicCoreIpcClientCtx *ctx, int ctrlCode, int arg);	/* +16 */
	int (*queryStatus)(const MusicCoreIpcClientCtx *ctx, MusicCoreRawStatus *out);	/* +20 */
	int (*setParam)(const MusicCoreIpcClientCtx *ctx, int paramId, const void *val);/* +24 */
	int (*getParam)(const MusicCoreIpcClientCtx *ctx, int paramId, int *outVal);	/* +28 */
	int (*dbSync)(const MusicCoreIpcClientCtx *ctx);				/* +32 */
	int (*noop)(const MusicCoreIpcClientCtx *ctx);					/* +36 */
	int (*openChannel)(const MusicCoreIpcClientCtx *ctx);				/* +40 */
	int (*closeChannel)(const MusicCoreIpcClientCtx *ctx);				/* +44 */
	int (*openUri)(const MusicCoreIpcClientCtx *ctx, const char *uri,
		const void *optBuf);							/* +48 */
	int (*getCurrentUri)(const MusicCoreIpcClientCtx *ctx, char *outUri);		/* +52 */
	int (*getChannelError)(const MusicCoreIpcClientCtx *ctx,
		MusicCoreChannelError *outErr);						/* +56 */
	int (*dbOpen)(const MusicCoreIpcClientCtx *ctx);				/* +60 */
	int (*dbSuspend)(const MusicCoreIpcClientCtx *ctx, SceUInt8 flag);		/* +64 */
	int (*dbClose)(const MusicCoreIpcClientCtx *ctx);				/* +68 */
	int (*dbDetach)(const MusicCoreIpcClientCtx *ctx);				/* +72 */
	int (*dbQuery)(const MusicCoreIpcClientCtx *ctx, const void *query);		/* +76 */
	int (*getCapability)(const MusicCoreIpcClientCtx *ctx, SceUInt32 *outCap);	/* +80 */
	int (*getMetadata)(const MusicCoreIpcClientCtx *ctx,
		MusicCoreRawMetadata *outMeta);						/* +84 */
	int (*getBgmSlot)(const MusicCoreIpcClientCtx *ctx, MusicCoreAppSlot *outSlot);	/* +88 */
	int (*getAllAppSlots)(const MusicCoreIpcClientCtx *ctx,
		MusicCoreAppSlot *outSlots);						/* +92 */
	int (*resetChannel)(const MusicCoreIpcClientCtx *ctx);				/* +96 */
	int (*getPlayTime)(const MusicCoreIpcClientCtx *ctx,
		MusicCoreChannelError *outTime);					/* +100 */
	int (*exOpen)(const MusicCoreIpcClientCtx *ctx, const void *openParam);		/* +104 */
	int (*exClose)(const MusicCoreIpcClientCtx *ctx);				/* +108 */
	int (*setAvrcp)(const MusicCoreIpcClientCtx *ctx, SceUInt8 enable);		/* +112 */
	int (*setNoticeSuppress)(const MusicCoreIpcClientCtx *ctx, SceUInt8 mute);	/* +116 */
	int (*clearNoticeSuppress)(const MusicCoreIpcClientCtx *ctx, SceUInt8 mute);	/* +120 */
	int (*setAudioFocus)(const MusicCoreIpcClientCtx *ctx, SceUInt8 mode);		/* +124 */
};

static SceUInt8 s_musicAppLastChannel = 0;			/* 0x8156F404 */

static MusicCorePluginInterface *findMusicCoreInterface()
{
	paf::Plugin *plugin = paf::Plugin::Find("musiccore_plugin");
	if (plugin == NULL)
		return NULL;
	return static_cast<MusicCorePluginInterface *>(plugin->GetInterface(1));
}

static int initMusicAppClientCtx(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	MusicCoreIpcClientCtx *outCtx)
{
	sce_paf_memset(outCtx, 0, sizeof(*outCtx));
	outCtx->seqId = handle->handleId;
	if (handle->channelKind == 0)
		outCtx->channel = 1;
	else if (handle->channelKind == 1)
		outCtx->channel = 2;
	else
		return MUSICAPP_ERR_INVALID_ARG;
	outCtx->subMode = static_cast<SceUInt8>(handle->priority);
	outCtx->pid = session->getProcessId();
	outCtx->appId = session->getAppId();
	return 0;
}

/* 0x81209B58 */
static int mapMusicAppControlCode(int ipcCtrl, int *outCtrl, int *outSeqId)
{
	switch (ipcCtrl) {
	case 0:  *outCtrl = 0;  *outSeqId = 88;  return 0;
	case 1:  *outCtrl = 2;  *outSeqId = 89;  return 0;
	case 2:  *outCtrl = 1;  *outSeqId = 90;  return 0;
	case 3:  *outCtrl = 3;  *outSeqId = 91;  return 0;
	case 4:  *outCtrl = 4;  *outSeqId = 92;  return 0;
	case 5:  *outCtrl = 5;  *outSeqId = 93;  return 0;
	case 6:  *outCtrl = 6;  *outSeqId = 94;  return 0;
	case 7:  *outCtrl = 7;  *outSeqId = 95;  return 0;
	case 8:  *outCtrl = 8;  *outSeqId = 96;  return 0;
	case 9:  *outCtrl = 9;  *outSeqId = 97;  return 0;
	case 10: *outCtrl = 10; *outSeqId = 98;  return 0;
	case 11: *outCtrl = 11; *outSeqId = 99;  return 0;
	case 12: *outCtrl = 12; *outSeqId = 100; return 0;
	case 13: *outCtrl = 13; *outSeqId = 101; return 0;
	case 14: *outCtrl = 14; *outSeqId = 102; return 0;
	case 15: *outCtrl = 15; *outSeqId = 103; return 0;
	case 16: *outCtrl = 16; *outSeqId = 104; return 0;
	case 17: *outCtrl = 17; *outSeqId = 105; return 0;
	default:
		return MUSICAPP_ERR_INVALID_ARG;
	}
}

/* 0x81209C48 */
static int mapMusicAppPlayMode(SceUInt32 rawPlayMode, SceUInt8 *outPlayMode)
{
	if (rawPlayMode <= 3) {
		*outPlayMode = static_cast<SceUInt8>(rawPlayMode);
		return 0;
	}
	return MUSICAPP_ERR_INVALID_ARG;
}

/* 0x81209C84 */
static int mapMusicAppState(int rawState, int *outState)
{
	switch (rawState) {
	case 'S': *outState = 0;  break;
	case 'T': *outState = 1;  break;
	case 'U': *outState = 2;  break;
	case 'V': *outState = 3;  break;
	case 'X': *outState = 4;  break;
	case 'Y': *outState = 5;  break;
	case 'Z': *outState = 6;  break;
	case '[': *outState = 7;  break;
	case '\\': *outState = 8; break;
	case ']': *outState = 9;  break;
	case '^': *outState = 10; break;
	case '_': *outState = 11; break;
	case '`': *outState = 12; break;
	case 'a': *outState = 13; break;
	case 'b': *outState = 14; break;
	case 'c': *outState = 21; break;
	case 'd': *outState = 22; break;
	case 'e': *outState = 23; break;
	case 'f': *outState = 24; break;
	case 'g': *outState = 25; break;
	case 'h': *outState = 26; break;
	case 'i': *outState = 27; break;
	case 'k': *outState = 15; break;
	case 'l': *outState = 16; break;
	case 'm': *outState = 17; break;
	case 'n': *outState = 18; break;
	case 'o': *outState = 19; break;
	case 'p': *outState = 20; break;
	default:
		break;
	}
	return 0;
}

/* 0x81209D74 */
static int mapMusicAppErrorCode(int eventCode, int rawErr, int commandTag, MusicAppErrorOut *outErr)
{
	(void)eventCode;
	(void)commandTag;
	if (rawErr == 0) {
		outErr->mappedError = 0;
		outErr->detailError = 0;
		return 0;
	}
	if (rawErr >= (int)0x80100D02 && rawErr <= (int)0x80100D0A) {
		int mapped = (int)0x80101904 + (rawErr - (int)0x80100D02);
		outErr->mappedError = mapped;
		outErr->detailError = mapped;
		return 0;
	}
	outErr->detailError = rawErr;
	outErr->mappedError = MUSICAPP_ERR_GENERAL;
	return 0;
}

/* 0x81209E54 */
static int mapMusicAppPlayTimeError(int eventCode, int rawErr, int commandTag, MusicAppErrorOut *outErr)
{
	(void)eventCode;
	(void)commandTag;
	if (rawErr == 0) {
		outErr->detailError = 0;
		return 0;
	}
	if (rawErr == (int)0x80100D00) {
		outErr->detailError = MUSICAPP_ERR_GENERAL;
		return 0;
	}
	if (rawErr >= (int)0x80100D02 && rawErr <= (int)0x80100D0A) {
		outErr->detailError = (int)0x80101904 + (rawErr - (int)0x80100D02);
		return 0;
	}
	outErr->detailError = rawErr;
	return 0;
}

/* 0x81209F24 */
static int musicAppOpenChannel(IPMI::Session *session, const MusicCoreIpcHandle *handle)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICAPP_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicAppClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	s_musicAppLastChannel = ctx.channel;
	ctx.seqId = 84;
	return iface->openChannel(&ctx);
}

/* 0x81209FE0 */
static int musicAppCloseChannel(IPMI::Session *session, const MusicCoreIpcHandle *handle)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICAPP_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicAppClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 85;
	return iface->closeChannel(&ctx);
}

/* 0x8120A08E */
static int musicAppOpenUri(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	const char *uri, const void *openOptions)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICAPP_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicAppClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 86;

	char uriBuf[1028];
	SceUInt8 optBuf[16];
	sce_paf_memset(uriBuf, 0, 1025);
	sce_paf_strncpy(uriBuf, uri, 1025);
	sce_paf_memset(optBuf, 0, sizeof(optBuf));
	sce_paf_memcpy(optBuf, openOptions, sizeof(optBuf));
	return iface->openUri(&ctx, uriBuf, optBuf);
}

/* 0x8120A18C */
static int musicAppGetCurrentUri(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	char *outUri)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICAPP_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicAppClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 87;

	char uriBuf[1028];
	sce_paf_memset(uriBuf, 0, 1025);
	ret = iface->getCurrentUri(&ctx, uriBuf);
	if (ret != 0)
		return ret;
	sce_paf_strncpy(outUri, uriBuf, 1025);
	return 0;
}

/* 0x8120A272 */
static int musicAppSendControl(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	SceUInt8 ipcCtrl, int controlArg)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICAPP_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicAppClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	int ctrlCode = 1;
	ret = mapMusicAppControlCode(ipcCtrl, &ctrlCode, &ctx.seqId);
	if (ret != 0)
		return ret;
	return iface->sendControlCmd(&ctx, ctrlCode, controlArg);
}

/* 0x8120A33E */
static int musicAppGetStatus(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	MusicAppIpcStatus *outStatus)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICAPP_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicAppClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 106;

	MusicCoreRawStatus rawStatus;
	sce_paf_memset(&rawStatus, 0, sizeof(rawStatus));
	ret = iface->queryStatus(&ctx, &rawStatus);
	if (ret != 0)
		return ret;

	if (rawStatus.channelType == 1)
		outStatus->channelType = 0;
	else if (rawStatus.channelType == 2)
		outStatus->channelType = 1;
	else
		return MUSICAPP_ERR_INVALID_ARG;

	ret = mapMusicAppState(rawStatus.state, &outStatus->state);
	if (ret != 0)
		return ret;

	outStatus->eventType = rawStatus.eventType;
	outStatus->errorCode = rawStatus.errorCode;
	ret = mapMusicAppPlayMode(rawStatus.playMode, &outStatus->playMode);
	if (ret != 0)
		return ret;

	outStatus->duration = rawStatus.duration;
	outStatus->posLo = rawStatus.posLo;
	outStatus->posHi = rawStatus.posHi;
	outStatus->sampleRate = rawStatus.sampleRate;
	outStatus->artistId = rawStatus.artistId;
	outStatus->albumId = rawStatus.albumId;
	outStatus->audioBalance = rawStatus.audioBalance;

	if (rawStatus.repeatMode > 2)
		return MUSICAPP_ERR_INVALID_ARG;
	outStatus->repeatMode = static_cast<SceUInt8>(rawStatus.repeatMode);

	if (rawStatus.shuffleMode > 1)
		return MUSICAPP_ERR_INVALID_ARG;
	outStatus->shuffleMode = static_cast<SceUInt8>(rawStatus.shuffleMode);

	if (rawStatus.avlsMode > 1)
		return MUSICAPP_ERR_INVALID_ARG;
	outStatus->avlsMode = static_cast<SceUInt8>(rawStatus.avlsMode);

	if (rawStatus.eqMode < 0 || rawStatus.eqMode > 4)
		return MUSICAPP_ERR_INVALID_ARG;
	outStatus->eqMode = static_cast<SceUInt8>(rawStatus.eqMode);
	return 0;
}

/* 0x8120A54A */
static int musicAppSetVolume(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	int volume)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICAPP_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicAppClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 107;
	return iface->setParam(&ctx, 0, &volume);
}

/* 0x8120A608 */
static int musicAppSetRepeat(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	SceUInt8 repeatMode)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICAPP_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicAppClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	if (repeatMode > 2)
		return MUSICAPP_ERR_INVALID_ARG;
	int value = repeatMode;
	ctx.seqId = 108;
	return iface->setParam(&ctx, 1, &value);
}

/* 0x8120A6FE */
static int musicAppSetShuffle(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	SceUInt8 shuffleMode)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICAPP_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicAppClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	if (shuffleMode > 1)
		return MUSICAPP_ERR_INVALID_ARG;
	int value = shuffleMode;
	ctx.seqId = 109;
	return iface->setParam(&ctx, 2, &value);
}

/* 0x8120A7E4 */
static int musicAppSetAvls(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	SceUInt8 avlsMode)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICAPP_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicAppClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	if (avlsMode > 1)
		return MUSICAPP_ERR_INVALID_ARG;
	int value = avlsMode;
	ctx.seqId = 111;
	return iface->setParam(&ctx, 6, &value);
}

/* 0x8120A8C6 */
static int musicAppSetEq(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	SceInt8 eqMode)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICAPP_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicAppClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	if (eqMode < 0 || eqMode > 4)
		return MUSICAPP_ERR_INVALID_ARG;
	int value = eqMode;
	ctx.seqId = 110;
	return iface->setParam(&ctx, 7, &value);
}

/* 0x8120A9C2 */
static int musicAppSetSeek(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	int seekMode, int seekOffset, int seekFlags)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICAPP_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicAppClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 112;
	MusicCoreSeekParam seek;
	sce_paf_memset(&seek, 0, sizeof(seek));
	seek.mode = seekMode;
	seek.offset = seekOffset;
	seek.flags = seekFlags;
	return iface->setParam(&ctx, 8, &seek);
}

/* 0x8120AAA0 */
static int musicAppGetError(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	MusicAppErrorOut *outErr)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICAPP_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicAppClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 113;

	MusicCoreChannelError rawErr;
	sce_paf_memset(&rawErr, 0, sizeof(rawErr));
	ret = iface->getChannelError(&ctx, &rawErr);
	if (ret != 0)
		return ret;
	return mapMusicAppErrorCode(rawErr.eventCode, rawErr.errorCode, rawErr.commandTag, outErr);
}

/* 0x8120AB76 */
static int musicAppOpenUriWithSeq(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	const char *uri, const void *openOptions, SceUInt16 seqNum)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICAPP_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicAppClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = static_cast<int>((2u << 16) | seqNum);

	char uriBuf[1028];
	SceUInt8 optBuf[16];
	sce_paf_memset(uriBuf, 0, 1025);
	sce_paf_strncpy(uriBuf, uri, 1025);
	sce_paf_memset(optBuf, 0, sizeof(optBuf));
	sce_paf_memcpy(optBuf, openOptions, sizeof(optBuf));
	return iface->openUri(&ctx, uriBuf, optBuf);
}

/* 0x8120AC7E */
static int musicAppGetPlayTime(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	MusicAppErrorOut *outErr)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICAPP_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicAppClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 113;

	MusicCoreChannelError rawTime;
	sce_paf_memset(&rawTime, 0, sizeof(rawTime));
	ret = iface->getPlayTime(&ctx, &rawTime);
	if (ret != 0)
		return ret;
	outErr->mappedError = rawTime.eventCode;
	return mapMusicAppPlayTimeError(rawTime.eventCode, rawTime.errorCode, rawTime.commandTag, outErr);
}

/* 0x8120AE32 */
static int musicAppDispatchSyncImpl(IPMI::Session *session, SceUInt32 methodId,
	const IPMI::DataInfo *inBufs, SceUInt32 inCount,
	IPMI::BufferInfo *outBufs, SceUInt32 outCount)
{
	(void)inCount;
	int ret;
	switch (methodId) {
	case 0xD0000:
		ret = musicAppOpenChannel(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0xD0001:
		ret = musicAppCloseChannel(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0xD0002:
		ret = musicAppOpenUri(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[0].pData),
			static_cast<const char *>(inBufs[1].pData),
			inBufs[2].pData);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0xD0003:
		ret = musicAppGetCurrentUri(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[0].pData),
			static_cast<char *>(outBufs[0].pBuffer));
		outBufs[0].outSize = 1025;
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0xD0004: {
		const MusicAppControlReq *req = static_cast<const MusicAppControlReq *>(inBufs[0].pData);
		ret = musicAppSendControl(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[1].pData),
			req->controlCode, req->controlArg);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	}
	case 0xD0005:
		ret = musicAppGetStatus(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[0].pData),
			static_cast<MusicAppIpcStatus *>(outBufs[0].pBuffer));
		outBufs[0].outSize = 64;
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0xD0006:
		ret = musicAppSetVolume(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[1].pData),
			*static_cast<const int *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0xD0007:
		ret = musicAppSetRepeat(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[1].pData),
			*static_cast<const SceUInt8 *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0xD0008:
		ret = musicAppSetShuffle(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[1].pData),
			*static_cast<const SceUInt8 *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0xD0009:
		ret = musicAppSetEq(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[1].pData),
			*static_cast<const SceInt8 *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0xD000A:
		ret = musicAppSetAvls(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[1].pData),
			*static_cast<const SceUInt8 *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0xD000B: {
		const MusicCoreSeekParam *seek = static_cast<const MusicCoreSeekParam *>(inBufs[0].pData);
		ret = musicAppSetSeek(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[1].pData),
			seek->mode, seek->offset, seek->flags);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	}
	case 0xD000C:
		ret = musicAppGetError(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[0].pData),
			static_cast<MusicAppErrorOut *>(outBufs[0].pBuffer));
		outBufs[0].outSize = 8;
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0xD000D:
		ret = musicAppOpenUriWithSeq(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[1].pData),
			static_cast<const char *>(inBufs[2].pData),
			inBufs[3].pData,
			*static_cast<const SceUInt16 *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0xD000E:
		ret = musicAppGetPlayTime(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[0].pData),
			static_cast<MusicAppErrorOut *>(outBufs[0].pBuffer));
		outBufs[0].outSize = 8;
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	default:
		return MUSICCORE_IPMI_UNSUPPORTED;
	}
}

/* 0x8120B038 */
static int convertMusicCoreHandle(const MusicCoreIpcHandle *handle, MusicCoreIpcClientCtx *outCtx)
{
	outCtx->seqId = handle->handleId;
	if (handle->channelKind != 1)
		return MUSICCORE_ERR_INVALID_ARG;
	outCtx->channel = 0;
	switch (handle->priority) {
	case 8:   outCtx->subMode = 4; return 0;
	case 16:  outCtx->subMode = 5; return 0;
	case 32:  outCtx->subMode = 6; return 0;
	case 64:  outCtx->subMode = 7; return 0;
	case 128: outCtx->subMode = 8; return 0;
	default:
		return MUSICCORE_ERR_INVALID_ARG;
	}
}

static int initMusicCoreClientCtx(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	MusicCoreIpcClientCtx *outCtx)
{
	sce_paf_memset(outCtx, 0, sizeof(*outCtx));
	int ret = convertMusicCoreHandle(handle, outCtx);
	if (ret != 0)
		return ret;
	outCtx->pid = session->getProcessId();
	outCtx->appId = session->getAppId();
	return 0;
}

/* 0x8120B092 */
static int mapMusicCoreControlCode(int ipcCtrl, int *outCtrl, int *outSeqId)
{
	switch (ipcCtrl) {
	case 1:  *outCtrl = 0;  *outSeqId = 121; return 0;
	case 2:  *outCtrl = 2;  *outSeqId = 122; return 0;
	case 3:  *outCtrl = 3;  *outSeqId = 124; return 0;
	case 4:  *outCtrl = 4;  *outSeqId = 125; return 0;
	case 5:  *outCtrl = 5;  *outSeqId = 126; return 0;
	case 6:  *outCtrl = 6;  *outSeqId = 127; return 0;
	case 7:  *outCtrl = 7;  *outSeqId = 128; return 0;
	case 8:  *outCtrl = 8;  *outSeqId = 129; return 0;
	case 9:  *outCtrl = 9;  *outSeqId = 130; return 0;
	case 10: *outCtrl = 10; *outSeqId = 131; return 0;
	case 11: *outCtrl = 11; *outSeqId = 132; return 0;
	case 12: *outCtrl = 12; *outSeqId = 133; return 0;
	case 13: *outCtrl = 13; *outSeqId = 134; return 0;
	case 14: *outCtrl = 14; *outSeqId = 135; return 0;
	case 15: *outCtrl = 15; *outSeqId = 136; return 0;
	case 16: *outCtrl = 16; *outSeqId = 137; return 0;
	case 17: *outCtrl = 17; *outSeqId = 138; return 0;
	default:
		return MUSICCORE_ERR_INVALID_ARG;
	}
}

/* 0x8120B178 */
static int mapMusicCorePlayMode(SceUInt32 rawPlayMode, int *outPlayMode)
{
	switch (rawPlayMode) {
	case 0: *outPlayMode = 1;  return 0;
	case 1: *outPlayMode = 2;  return 0;
	case 2: *outPlayMode = 0;  return 0;
	case 3: *outPlayMode = 10; return 0;
	default:
		return MUSICCORE_ERR_INVALID_ARG;
	}
}

/* 0x8120B1B4 */
static int mapMusicCoreErrorCode(int eventCode, int rawErr, int commandTag, MusicAppErrorOut *outErr)
{
	(void)eventCode;
	(void)commandTag;
	if (rawErr == 0) {
		outErr->mappedError = 0;
		outErr->detailError = 0;
		return 0;
	}
	if (rawErr >= (int)0x80100D02 && rawErr <= (int)0x80100D0A) {
		int mapped = (int)0x80100E02 + (rawErr - (int)0x80100D02);
		outErr->mappedError = mapped;
		outErr->detailError = mapped;
		return 0;
	}
	outErr->detailError = rawErr;
	outErr->mappedError = MUSICCORE_ERR_GENERAL;
	return 0;
}

/* 0x8120B296 */
static int mapMusicCoreCapability(SceUInt32 rawCap, SceUInt32 *outCap)
{
	switch (rawCap) {
	case 1: *outCap = 2; return 0;
	case 2: *outCap = 0; return 0;
	case 3: *outCap = 1; return 0;
	default:
		return MUSICCORE_ERR_INVALID_ARG;
	}
}

/* 0x8120B2CA */
static int mapMusicCorePlayTimeError(int eventCode, int rawErr, int commandTag, MusicCorePlayTimeOut *outTime)
{
	(void)eventCode;
	(void)commandTag;
	if (rawErr == 0) {
		outTime->detailError = 0;
		return 0;
	}
	if (rawErr == (int)0x80100D00) {
		outTime->detailError = MUSICCORE_ERR_GENERAL;
		return 0;
	}
	if (rawErr >= (int)0x80100D02 && rawErr <= (int)0x80100D0A) {
		outTime->detailError = (int)0x80100E02 + (rawErr - (int)0x80100D02);
		return 0;
	}
	outTime->detailError = rawErr;
	return 0;
}

/* 0x8120B39A */
static int mapMusicCoreRawPlayPauseMode(SceUInt32 rawMode, SceUInt32 *outMode)
{
	if (rawMode == 18) {
		*outMode = 0;
		return 0;
	}
	if (rawMode == 0) {
		*outMode = 1;
		return 0;
	}
	if (rawMode == 2) {
		*outMode = 2;
		return 0;
	}
	return MUSICCORE_ERR_INVALID_ARG;
}

static int convertContentParamToInternal(const MusicCoreContentParam *inParam,
	MusicCoreContentParam *outParam)
{
	sce_paf_memset(outParam, 0, sizeof(*outParam));
	sce_paf_strncpy(outParam->uri, inParam->uri, 2049);
	outParam->contentIdLow = inParam->contentIdLow;
	outParam->contentIdHigh = inParam->contentIdHigh;
	outParam->trackIndex = inParam->trackIndex;
	outParam->trackTotal = inParam->trackTotal;

	if (inParam->shuffleMode == 0)
		outParam->shuffleMode = 2;
	else if (inParam->shuffleMode == 1)
		outParam->shuffleMode = 0;
	else if (inParam->shuffleMode == 2)
		outParam->shuffleMode = 1;
	else
		return MUSICCORE_ERR_INVALID_ARG;

	if (inParam->playPauseMode == 0)
		outParam->playPauseMode = 18;
	else if (inParam->playPauseMode == 1)
		outParam->playPauseMode = 0;
	else if (inParam->playPauseMode == 2)
		outParam->playPauseMode = 2;
	else
		return MUSICCORE_ERR_INVALID_ARG;

	if (inParam->repeatMode == 0)
		outParam->repeatMode = 2;
	else if (inParam->repeatMode == 1)
		outParam->repeatMode = 0;
	else if (inParam->repeatMode == 2)
		outParam->repeatMode = 1;
	else
		return MUSICCORE_ERR_INVALID_ARG;

	return 0;
}

/* 0x8120B3CC */
static int musicCoreIpcDbSync(IPMI::Session *session, const MusicCoreIpcHandle *handle)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 115;
	return iface->dbSync(&ctx);
}

/* 0x8120B454 */
static int musicCoreIpcNoop(IPMI::Session *session, const MusicCoreIpcHandle *handle)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 116;
	return iface->noop(&ctx);
}

/* 0x8120B4DC */
static int musicCoreIpcSetContent(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	const MusicCoreContentParam *inContent)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 117;

	MusicCoreContentParam internalContent;
	ret = convertContentParamToInternal(inContent, &internalContent);
	if (ret != 0)
		return ret;
	return iface->setContent(&ctx, &internalContent);
}

/* 0x8120B65A */
static int musicCoreIpcGetTrackInfo(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	MusicCoreContentParam *outContent)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 118;

	MusicCoreContentParam internalContent;
	sce_paf_memset(&internalContent, 0, sizeof(internalContent));
	ret = iface->getTrackInfo(&ctx, &internalContent);
	if (ret != 0)
		return ret;

	sce_paf_strncpy(outContent->uri, internalContent.uri, 2049);
	outContent->contentIdLow = internalContent.contentIdLow;
	outContent->contentIdHigh = internalContent.contentIdHigh;
	outContent->trackIndex = internalContent.trackIndex;
	outContent->trackTotal = internalContent.trackTotal;

	if (internalContent.shuffleMode == 0)
		outContent->shuffleMode = 1;
	else if (internalContent.shuffleMode == 1)
		outContent->shuffleMode = 2;
	else if (internalContent.shuffleMode == 2)
		outContent->shuffleMode = 0;
	else
		return MUSICCORE_ERR_INVALID_ARG;

	return mapMusicCoreRawPlayPauseMode(internalContent.playPauseMode, &outContent->playPauseMode);
}

/* 0x8120B778 */
static int musicCoreIpcOpenUri(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	const char *uri, const void *openOptions)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 119;

	char uriBuf[1028];
	SceUInt8 optBuf[16];
	sce_paf_memset(uriBuf, 0, 1025);
	sce_paf_strncpy(uriBuf, uri, 1025);
	sce_paf_memset(optBuf, 0, sizeof(optBuf));
	sce_paf_memcpy(optBuf, openOptions, sizeof(optBuf));
	return iface->openUri(&ctx, uriBuf, optBuf);
}

/* 0x8120B846 */
static int musicCoreIpcGetCurrentUri(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	char *outUri)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 120;

	char uriBuf[1028];
	sce_paf_memset(uriBuf, 0, 1025);
	ret = iface->getCurrentUri(&ctx, uriBuf);
	if (ret != 0)
		return ret;
	sce_paf_strncpy(outUri, uriBuf, 1025);
	return 0;
}

/* 0x8120B900 */
static int musicCoreIpcSendControl(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	int ipcCtrl, int controlArg)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	int ctrlCode = 1;
	ret = mapMusicCoreControlCode(ipcCtrl, &ctrlCode, &ctx.seqId);
	if (ret != 0)
		return ret;
	return iface->sendControlCmd(&ctx, ctrlCode, controlArg);
}

/* 0x8120B9A4 */
static int musicCoreIpcGetStatus(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	MusicCoreIpcStatus *outStatus)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 139;

	MusicCoreRawStatus rawStatus;
	sce_paf_memset(&rawStatus, 0, sizeof(rawStatus));
	ret = iface->queryStatus(&ctx, &rawStatus);
	if (ret != 0)
		return ret;

	outStatus->errorCode = rawStatus.errorCode;
	ret = mapMusicCorePlayMode(rawStatus.playMode, &outStatus->playMode);
	if (ret != 0)
		return ret;

	outStatus->bgmReason = rawStatus.bgmReason;
	outStatus->bgmPriority = rawStatus.bgmPriority;
	outStatus->duration = rawStatus.duration;
	outStatus->posLo = rawStatus.posLo;
	outStatus->posHi = rawStatus.posHi;
	outStatus->sampleRate = rawStatus.sampleRate;
	outStatus->artistId = rawStatus.artistId;
	outStatus->albumId = rawStatus.albumId;
	outStatus->audioBalance = rawStatus.audioBalance;

	if (rawStatus.repeatMode == 0)
		outStatus->repeatMode = 0;
	else if (rawStatus.repeatMode == 1)
		outStatus->repeatMode = 1;
	else if (rawStatus.repeatMode == 2)
		outStatus->repeatMode = 3;
	else
		return MUSICCORE_ERR_INVALID_ARG;

	if (rawStatus.shuffleMode == 0)
		outStatus->shuffleMode = 1;
	else if (rawStatus.shuffleMode == 1)
		outStatus->shuffleMode = 2;
	else if (rawStatus.shuffleMode == 2)
		outStatus->shuffleMode = 0;
	else
		return MUSICCORE_ERR_INVALID_ARG;

	if (rawStatus.avlsMode > 1)
		return MUSICCORE_ERR_INVALID_ARG;
	outStatus->avlsMode = static_cast<int>(rawStatus.avlsMode);

	if (rawStatus.eqMode < 0 || rawStatus.eqMode > 4)
		return MUSICCORE_ERR_INVALID_ARG;
	outStatus->eqMode = rawStatus.eqMode;

	if (rawStatus.volume > 2)
		return MUSICCORE_ERR_INVALID_ARG;
	outStatus->volume = static_cast<int>(rawStatus.volume);

	outStatus->trackItemId = rawStatus.trackItemId;
	outStatus->playlistId = rawStatus.playlistId;
	outStatus->seqNum = rawStatus.seqNum;

	if (rawStatus.isPlayable > 1)
		return MUSICCORE_ERR_INVALID_ARG;
	outStatus->isPlayable = static_cast<int>(rawStatus.isPlayable);
	return 0;
}

/* 0x8120BBB6 */
static int musicCoreIpcSetVolume(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	int volume)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 140;
	return iface->setParam(&ctx, 0, &volume);
}

/* 0x8120BC48 */
static int musicCoreIpcSetRepeat(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	SceUInt32 repeatMode)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	int value;
	if (repeatMode == 0)
		value = 0;
	else if (repeatMode == 1)
		value = 1;
	else if (repeatMode == 3)
		value = 2;
	else
		return MUSICCORE_ERR_INVALID_ARG;
	ctx.seqId = 141;
	return iface->setParam(&ctx, 1, &value);
}

/* 0x8120BD0E */
static int musicCoreIpcSetShuffle(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	SceUInt32 shuffleMode)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	int value;
	if (shuffleMode == 0)
		value = 2;
	else if (shuffleMode == 1)
		value = 0;
	else if (shuffleMode == 2)
		value = 1;
	else
		return MUSICCORE_ERR_INVALID_ARG;
	ctx.seqId = 142;
	return iface->setParam(&ctx, 2, &value);
}

/* 0x8120BDD4 */
static int musicCoreIpcSetTrackIndex(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	int trackIndex)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 145;
	return iface->setParam(&ctx, 3, &trackIndex);
}

/* 0x8120BE66 */
static int musicCoreIpcSetTrackTotal(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	int trackTotal)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 146;
	return iface->setParam(&ctx, 4, &trackTotal);
}

/* 0x8120BEF8 */
static int musicCoreIpcSetAudioBalance(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	int balance)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 147;
	return iface->setParam(&ctx, 5, &balance);
}

/* 0x8120BF8A */
static int musicCoreIpcSetSeek(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	int seekMode, int seekOffset, int seekFlags)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 148;
	MusicCoreSeekParam seek;
	sce_paf_memset(&seek, 0, sizeof(seek));
	seek.mode = seekMode;
	seek.offset = seekOffset;
	seek.flags = seekFlags;
	return iface->setParam(&ctx, 8, &seek);
}

/* 0x8120C03C */
static int musicCoreIpcSetAvls(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	SceUInt32 avlsMode)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	if (avlsMode > 1)
		return MUSICCORE_ERR_INVALID_ARG;
	int value = static_cast<int>(avlsMode);
	ctx.seqId = 144;
	return iface->setParam(&ctx, 6, &value);
}

/* 0x8120C0F2 */
static int musicCoreIpcSetEq(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	int eqMode)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	if (eqMode < 0 || eqMode > 4)
		return MUSICCORE_ERR_INVALID_ARG;
	ctx.seqId = 143;
	return iface->setParam(&ctx, 7, &eqMode);
}

/* 0x8120C1C0 */
static int musicCoreIpcGetError(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	MusicAppErrorOut *outErr)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 149;

	MusicCoreChannelError rawErr;
	sce_paf_memset(&rawErr, 0, sizeof(rawErr));
	ret = iface->getChannelError(&ctx, &rawErr);
	if (ret != 0)
		return ret;
	return mapMusicCoreErrorCode(rawErr.eventCode, rawErr.errorCode, rawErr.commandTag, outErr);
}

/* 0x8120C26A */
static int musicCoreIpcDbOpen(IPMI::Session *session, const MusicCoreIpcHandle *handle)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 150;
	return iface->dbOpen(&ctx);
}

/* 0x8120C2F2 */
static int musicCoreIpcDbSuspend(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	SceUInt8 suspendFlag)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 151;
	return iface->dbSuspend(&ctx, suspendFlag);
}

/* 0x8120C380 */
static int musicCoreIpcDbClose(IPMI::Session *session, const MusicCoreIpcHandle *handle)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 152;
	return iface->dbClose(&ctx);
}

/* 0x8120C408 */
static int musicCoreIpcDbDetach(IPMI::Session *session, const MusicCoreIpcHandle *handle)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 153;
	return iface->dbDetach(&ctx);
}

/* 0x8120C490 */
static int musicCoreIpcDbQuery(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	const MusicCoreDbQueryParam *inQuery)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;

	MusicCoreDbQueryParam queryCopy;
	sce_paf_memset(&queryCopy, 0, sizeof(queryCopy));
	queryCopy.count = inQuery->count;
	for (SceUInt32 idx = 0; idx < inQuery->count; ++idx)
		queryCopy.itemIds[idx] = inQuery->itemIds[idx];

	if (inQuery->queryMode > 2)
		return MUSICCORE_ERR_INVALID_ARG;
	queryCopy.queryMode = inQuery->queryMode;

	ctx.seqId = 154;
	return iface->dbQuery(&ctx, &queryCopy);
}

/* 0x8120C582 */
static int musicCoreIpcGetCapability(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	SceUInt32 *outCap)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 155;
	SceUInt32 rawCap = 0;
	ret = iface->getCapability(&ctx, &rawCap);
	if (ret != 0)
		return ret;
	return mapMusicCoreCapability(rawCap, outCap);
}

/* 0x8120C61C */
static int musicCoreIpcSetContentWithSeq(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	const MusicCoreContentParam *inContent, SceUInt16 seqNum)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = static_cast<int>((1u << 16) | seqNum);

	MusicCoreContentParam internalContent;
	ret = convertContentParamToInternal(inContent, &internalContent);
	if (ret != 0)
		return ret;
	return iface->setContent(&ctx, &internalContent);
}

/* 0x8120C7A6 */
static int musicCoreIpcOpenUriWithSeq(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	const char *uri, const void *openOptions, SceUInt16 seqNum)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = static_cast<int>((1u << 16) | seqNum);

	char uriBuf[1028];
	SceUInt8 optBuf[16];
	sce_paf_memset(uriBuf, 0, 1025);
	sce_paf_strncpy(uriBuf, uri, 1025);
	sce_paf_memset(optBuf, 0, sizeof(optBuf));
	sce_paf_memcpy(optBuf, openOptions, sizeof(optBuf));
	return iface->openUri(&ctx, uriBuf, optBuf);
}

/* 0x8120C880 */
static int musicCoreIpcGetPlayTime(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	MusicCorePlayTimeOut *outTime)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 149;

	MusicCoreChannelError rawTime;
	sce_paf_memset(&rawTime, 0, sizeof(rawTime));
	ret = iface->getPlayTime(&ctx, &rawTime);
	if (ret != 0)
		return ret;

	if (rawTime.commandTag == 117)
		outTime->mappedEvent = 3;
	else if (rawTime.commandTag == 119)
		outTime->mappedEvent = 24;
	else
		outTime->mappedEvent = rawTime.eventCode;

	ret = mapMusicCorePlayTimeError(rawTime.eventCode, rawTime.errorCode,
		rawTime.commandTag, outTime);
	if (ret == 0)
		outTime->rawEvent = rawTime.eventCode;
	return ret;
}

/* 0x8120C954 */
static int musicCoreIpcGetMetadata(IPMI::Session *session, const MusicCoreIpcHandle *handle,
	MusicCoreMetadataOut *outMeta)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return MUSICCORE_ERR_NOT_AVAILABLE;
	MusicCoreIpcClientCtx ctx;
	int ret = initMusicCoreClientCtx(session, handle, &ctx);
	if (ret != 0)
		return ret;
	ctx.seqId = 139;

	MusicCoreRawStatus rawStatus;
	sce_paf_memset(&rawStatus, 0, sizeof(rawStatus));
	ret = iface->queryStatus(&ctx, &rawStatus);
	if (ret != 0)
		return ret;

	MusicCoreRawMetadata rawMeta;
	sce_paf_memset(&rawMeta, 0, sizeof(rawMeta));
	ret = iface->getMetadata(&ctx, &rawMeta);
	if (ret != 0)
		return ret;

	outMeta->durationLo = static_cast<int>(static_cast<SceUInt64>(rawStatus.duration) & 0xFFFFFFFFu);
	outMeta->durationHi = static_cast<int>((static_cast<SceUInt64>(rawStatus.duration) >> 32) & 0xFFFFFFFFu);
	outMeta->sampleRate = rawStatus.sampleRate;
	sce_paf_memcpy(outMeta->title, rawMeta.title, sizeof(outMeta->title));
	sce_paf_memcpy(outMeta->artist, rawMeta.artist, sizeof(outMeta->artist));
	sce_paf_memcpy(outMeta->album, rawMeta.album, sizeof(outMeta->album));
	return 0;
}

/* 0x8120CB24 */
static int musicCoreDispatchSyncImpl(IPMI::Session *session, SceUInt32 methodId,
	const IPMI::DataInfo *inBufs, SceUInt32 inCount,
	IPMI::BufferInfo *outBufs, SceUInt32 outCount)
{
	(void)inCount;
	int ret;
	switch (methodId) {
	case 0x30000:
		ret = musicCoreIpcDbSync(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x30001:
		ret = musicCoreIpcNoop(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x30002:
		ret = musicCoreIpcSetContent(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[0].pData),
			static_cast<const MusicCoreContentParam *>(inBufs[1].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x30003:
		ret = musicCoreIpcGetTrackInfo(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[0].pData),
			static_cast<MusicCoreContentParam *>(outBufs[0].pBuffer));
		outBufs[0].outSize = 2088;
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x30004:
		ret = musicCoreIpcOpenUri(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[0].pData),
			static_cast<const char *>(inBufs[1].pData),
			inBufs[2].pData);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x30005:
		ret = musicCoreIpcGetCurrentUri(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[0].pData),
			static_cast<char *>(outBufs[0].pBuffer));
		outBufs[0].outSize = 1025;
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x30006: {
		const MusicCoreControlReq *req = static_cast<const MusicCoreControlReq *>(inBufs[0].pData);
		ret = musicCoreIpcSendControl(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[1].pData),
			req->controlCode, req->controlArg);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	}
	case 0x30007:
		ret = musicCoreIpcGetStatus(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[0].pData),
			static_cast<MusicCoreIpcStatus *>(outBufs[0].pBuffer));
		outBufs[0].outSize = 112;
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x30008:
		ret = musicCoreIpcSetVolume(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[1].pData),
			*static_cast<const int *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x30009:
		ret = musicCoreIpcSetRepeat(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[1].pData),
			*static_cast<const SceUInt32 *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x3000A:
		ret = musicCoreIpcSetShuffle(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[1].pData),
			*static_cast<const SceUInt32 *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x3000B:
		ret = musicCoreIpcSetTrackIndex(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[1].pData),
			*static_cast<const int *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x3000C:
		ret = musicCoreIpcSetTrackTotal(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[1].pData),
			*static_cast<const int *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x3000D:
		ret = musicCoreIpcSetAudioBalance(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[1].pData),
			*static_cast<const int *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x3000E: {
		const MusicCoreSeekParam *seek = static_cast<const MusicCoreSeekParam *>(inBufs[0].pData);
		ret = musicCoreIpcSetSeek(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[1].pData),
			seek->mode, seek->offset, seek->flags);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	}
	case 0x3000F:
		ret = musicCoreIpcSetAvls(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[1].pData),
			*static_cast<const SceUInt32 *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x30010:
		ret = musicCoreIpcSetEq(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[1].pData),
			*static_cast<const int *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x30011:
		ret = musicCoreIpcGetError(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[0].pData),
			static_cast<MusicAppErrorOut *>(outBufs[0].pBuffer));
		outBufs[0].outSize = 8;
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x30012:
		ret = musicCoreIpcDbOpen(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x30013:
		ret = musicCoreIpcDbSuspend(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[1].pData),
			*static_cast<const SceUInt8 *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x30014:
		ret = musicCoreIpcDbClose(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x30015:
		ret = musicCoreIpcDbDetach(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x30016:
		ret = musicCoreIpcDbQuery(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[0].pData),
			static_cast<const MusicCoreDbQueryParam *>(inBufs[1].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x30017:
		ret = musicCoreIpcGetCapability(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[0].pData),
			static_cast<SceUInt32 *>(outBufs[0].pBuffer));
		outBufs[0].outSize = 4;
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x30018:
		ret = musicCoreIpcSetContentWithSeq(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[1].pData),
			static_cast<const MusicCoreContentParam *>(inBufs[2].pData),
			*static_cast<const SceUInt16 *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x30019:
		ret = musicCoreIpcOpenUriWithSeq(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[1].pData),
			static_cast<const char *>(inBufs[2].pData),
			inBufs[3].pData,
			*static_cast<const SceUInt16 *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x3001A:
		ret = musicCoreIpcGetPlayTime(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[0].pData),
			static_cast<MusicCorePlayTimeOut *>(outBufs[0].pBuffer));
		outBufs[0].outSize = 12;
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	case 0x3001B:
		ret = musicCoreIpcGetMetadata(session,
			static_cast<const MusicCoreIpcHandle *>(inBufs[0].pData),
			static_cast<MusicCoreMetadataOut *>(outBufs[0].pBuffer));
		outBufs[0].outSize = 6160;
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	default:
		return MUSICCORE_IPMI_UNSUPPORTED;
	}
}

} /* anonymous namespace */

/* 0x8120AD56 */
int shellSvcDisp_0D_Sync(IPMI::Session *session, SceUInt32 methodId,
	const IPMI::DataInfo *inBufs, SceUInt32 inCount,
	IPMI::BufferInfo *outBufs, SceUInt32 outCount)
{
	return musicAppDispatchSyncImpl(session, methodId, inBufs, inCount, outBufs, outCount);
}

/* 0x8120AD6A */
int shellSvcDisp_0D_Async(IPMI::Session *session, SceUInt32 reqId, SceUInt32 methodId,
	const IPMI::DataInfo *inBufs, SceUInt32 inCount)
{
	return shellSvcLiveAreaSyncDefault(session, reqId, methodId, inBufs, inCount);
}

/* 0x8120AD7A */
void shellSvcDisp_0D_Disconn(IPMI::Session *session)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return;
	MusicCoreIpcClientCtx ctx;
	sce_paf_memset(&ctx, 0, sizeof(ctx));
	ctx.seqId = 255;
	ctx.channel = s_musicAppLastChannel;
	ctx.subMode = 0;
	ctx.pid = session->getProcessId();
	ctx.appId = session->getAppId();

	MusicCoreAppSlot slots[8];
	sce_paf_memset(slots, 0, sizeof(slots));
	if (iface->getAllAppSlots(&ctx, slots) != 0)
		return;
	for (int idx = 0; idx < 8; ++idx) {
		if (ctx.appId == slots[idx].appId) {
			iface->closeChannel(&ctx);
			break;
		}
	}
}

/* 0x8120CA64 */
int shellSvcDisp_03_Sync(IPMI::Session *session, SceUInt32 methodId,
	const IPMI::DataInfo *inBufs, SceUInt32 inCount,
	IPMI::BufferInfo *outBufs, SceUInt32 outCount)
{
	return musicCoreDispatchSyncImpl(session, methodId, inBufs, inCount, outBufs, outCount);
}

/* 0x8120CA78 */
int shellSvcDisp_03_Async(IPMI::Session *session, SceUInt32 reqId, SceUInt32 methodId,
	const IPMI::DataInfo *inBufs, SceUInt32 inCount)
{
	return shellSvcLiveAreaSyncDefault(session, reqId, methodId, inBufs, inCount);
}

/* 0x8120CA88 */
void shellSvcDisp_03_Disconn(IPMI::Session *session)
{
	MusicCorePluginInterface *iface = findMusicCoreInterface();
	if (iface == NULL)
		return;
	MusicCoreIpcClientCtx ctx;
	sce_paf_memset(&ctx, 0, sizeof(ctx));
	ctx.seqId = 255;
	ctx.channel = 0;
	ctx.subMode = 0;
	ctx.pid = session->getProcessId();
	ctx.appId = session->getAppId();

	MusicCoreAppSlot bgmSlot;
	sce_paf_memset(&bgmSlot, 0, sizeof(bgmSlot));
	if (iface->getBgmSlot(&ctx, &bgmSlot) == 0 && ctx.appId == bgmSlot.appId)
		iface->resetChannel(&ctx);
}
