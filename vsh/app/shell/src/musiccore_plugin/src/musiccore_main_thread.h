/*
 * SceShell musiccore_plugin - shared MusicCoreMain class and converter declarations.
 */

#ifndef SHELL_MUSICCORE_MAIN_THREAD_H
#define SHELL_MUSICCORE_MAIN_THREAD_H

#include <stddef.h>
#include <scetypes.h>
#include <thread/thread.h>

typedef void (*MusicCoreNotifyCb)(const void *ctx, SceUInt16 eventId, const int *err,
				  const void *status);

struct MusicCoreQueueMsg {
	int header[2];						/* +0 */
	int seqId;						/* +8 */
	SceUInt8 channel;					/* +12 */
	SceUInt8 padChannel;					/* +13 */
	SceUInt16 cmdId;					/* +14 */
	const void *payload;					/* +16 */
	SceSize payloadSize;					/* +20 */
};

struct MusicCoreClientReq {
	int seqId;						/* +0 */
	SceUInt8 channel;					/* +4 */
	SceUInt8 subMode;					/* +5 */
	SceUInt8 reserved[2];					/* +6 */
	int pid;						/* +8 */
	int appId;						/* +12 */
};

class MusicCoreMain {
public:
	MusicCoreMain(int *outErr);				/* 0x811CE626 */
	~MusicCoreMain();					/* 0x811CE882 */

	int SendCmd(SceUInt32 cmdTag, int seqId, SceUInt32 packedCmd,
		const void *payload, SceSize payloadSize);	/* 0x811CE796 */
	int InitController();					/* 0x811CE940 */
	int SetCallback(MusicCoreNotifyCb cb);			/* 0x811CEB88 */
	int ClearCallback(MusicCoreNotifyCb cb);		/* 0x811CEB9A */
	int ParseMsgHeader(const MusicCoreQueueMsg *msg, int *outSeq, SceUInt8 *outCh); /* 0x811CEBA8 */
	int ResolveSlot(SceUInt32 chType, int appId, SceUInt8 *outSlot); /* 0x811CEC06 */
	int SetSlotBgmAllowed(SceUInt32 chType, int appId, SceUInt8 allowed); /* 0x811CEC72 */
	int SetSlotLastCtrl(SceUInt32 chType, int appId, SceUInt8 ctrl); /* 0x811CECCA */
	int CheckSlotPaused(SceUInt32 chType, int appId, SceUInt8 *outState); /* 0x811CED22 */

	int HandleSetContent(const MusicCoreQueueMsg *msg);	/* 0x811CEDC4 */
	int HandleControl(const MusicCoreQueueMsg *msg);	/* 0x811CEF6C */
	int HandleSetRepeat(const MusicCoreQueueMsg *msg);	/* 0x811CF388 */
	int HandleSetShuffle(const MusicCoreQueueMsg *msg);	/* 0x811CF428 */
	int HandleSetAvls(const MusicCoreQueueMsg *msg);	/* 0x811CF4B8 */
	int HandleOpenChannel(const MusicCoreQueueMsg *msg);	/* 0x811CF548 */
	int HandleCloseChannel(const MusicCoreQueueMsg *msg);	/* 0x811CF6A8 */
	int HandleSetEq(const MusicCoreQueueMsg *msg);		/* 0x811CF7AC */
	int HandleDbQuery(const MusicCoreQueueMsg *msg);	/* 0x811CF850 */
	int HandleAppFocusChange(const MusicCoreQueueMsg *msg);	/* 0x811CF8E2 */
	int HandleSystemPause(const MusicCoreQueueMsg *msg);	/* 0x811CFE7C */
	int HandlePowerSuspend(const MusicCoreQueueMsg *msg);	/* 0x811D0016 */
	int HandlePowerResume(const MusicCoreQueueMsg *msg);	/* 0x811D015C */

	int GetStatus(const MusicCoreClientReq *req, void *outRaw); /* 0x811D02D2 */
	int GetError(const MusicCoreClientReq *req, int *outErr); /* 0x811D038A */
	int GetTrackInfo(const MusicCoreClientReq *req, void *outTrack, void *outRaw); /* 0x811D04AC */
	int GetUri(const MusicCoreClientReq *req, void *outUri); /* 0x811D04FA */
	int GetPlayTimeFlag(const MusicCoreClientReq *req, int *outVal); /* 0x811D05CE */
	int GetCap(const MusicCoreClientReq *req, void *outCap); /* 0x811D068A */
	int GetMeta(const MusicCoreClientReq *req, char *outMeta); /* 0x811D06A0 */
	int GetPlayTimeEntry(const MusicCoreClientReq *req, void *outEntry); /* 0x811D06FE */
	int GetAllAppSlots(int unused, void *outSlots);		/* 0x811D07D6 */
	int GetBgmSlot(int unused, void *outSlot);		/* 0x811D07EE */
	int SetAvrcpActive(const MusicCoreClientReq *req, SceUInt8 enable); /* 0x811D0804 */
	int SetErrorSuppression(const MusicCoreClientReq *req, SceUInt8 mode); /* 0x811D082E */
	int ClearErrorSuppression(const MusicCoreClientReq *req, SceUInt8 mode); /* 0x811D087C */

	int DispatchMsg(const MusicCoreQueueMsg *msg);		/* 0x811D08CE */
	int MainLoop();						/* 0x811D0FBA */
	static int ThreadEntry(SceSize args, void *argp);	/* 0x811D1174 */

	SceUID m_threadId;					/* +0 */
	SceUID m_semaId;					/* +4 */
	int m_initResult;					/* +8 */
	int m_initAlignPad;					/* +12 */
	int m_state;						/* +16 */
	int m_stateAlignPad;					/* +20 */
	paf::thread::Mutex m_mutex;				/* +24 */
	paf::thread::Cond m_cond;				/* +56 */
	void *m_ackVtbl;					/* +76 */
	paf::thread::Cond *m_ackCond;				/* +80 */
	paf::thread::Mutex *m_ackMutex;				/* +84 */
	int m_ackState;						/* +88 */
	void *m_queue;						/* +92 */
	SceUInt8 m_initialized;					/* +96 */
	SceUInt8 m_stopping;					/* +97 */
};

extern int musicCoreConvertRepeat(int raw, int *outVal);	/* 0x811D1214 */
extern int musicCoreConvertShuffle(SceUInt32 raw, int *outVal);	/* 0x811D1248 */
extern int musicCoreConvertEq(int raw, int *outVal);		/* 0x811D126C */
extern int musicCoreConvertAvls(int raw, int unusedMode, int unusedRate,
				int unusedFlags, int *outVal);	/* 0x811D1298 */
extern int musicCoreConvertStatus(const void *raw, void *outStatus, int playTime,
				  const SceUInt8 *cap);		/* 0x811D1320 */
extern int musicCoreConvertTrackInfo(const char *rawTrack, const void *rawStatus,
				     char *outInfo);		/* 0x811D1608 */
extern int musicCoreConvertUri(const char *rawUri, char *outUri); /* 0x811D16B2 */
extern int musicCoreConvertCapFlag(SceInt8 capByte, int *outVal); /* 0x811D16C6 */

#endif /* SHELL_MUSICCORE_MAIN_THREAD_H */
