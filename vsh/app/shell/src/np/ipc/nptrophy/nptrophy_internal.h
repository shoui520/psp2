/*
 * nptrophy_internal.h: shared internal declarations for SceNpTrophyServer
 * (0x81226C10..0x8122ACC0, 0x814277E8..0x814288B8).
 */
#ifndef SHELL_NP_TROPHY_INTERNAL_H
#define SHELL_NP_TROPHY_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <ipmi/session.h>
#include <np/np_common.h>

/* SceNpTrophy server error codes */
#define NP_TROPHY_ERR_NOT_INIT			-2141907455	/* 0x80551601 */
#define NP_TROPHY_ERR_ALREADY_INIT		-2141907454	/* 0x80551602 */
#define NP_TROPHY_ERR_NO_MEMORY			-2141907453	/* 0x80551603 */
#define NP_TROPHY_ERR_ABORTED			-2141907449	/* 0x80551607 */
#define NP_TROPHY_ERR_CLIENT_CLOSED		-2141907267	/* 0x805516BD */
#define NP_TROPHY_ERR_CLIENT_KILLED		-2141907266	/* 0x805516BE */
#define NP_TROPHY_ERR_SERVER_STOPPED		-2141907257	/* 0x805516C7 */
#define NP_TROPHY_ERR_UNSUPPORTED_METHOD	-2147351103	/* 0x800205C1 */

/* SceNpTrophy IPC method IDs (0x090000..0x090046) */
enum NpTrophyIpcMethod {
	/* Synchronous methods */
	NP_TROPHY_CMD_CREATE_CONTEXT		= 0x090000,
	NP_TROPHY_CMD_DESTROY_CONTEXT		= 0x090001,
	NP_TROPHY_CMD_CREATE_TITLE_SESSION	= 0x090002,
	NP_TROPHY_CMD_DESTROY_TITLE_SESSION	= 0x090003,
	NP_TROPHY_CMD_GET_HANDLE_PROGRESS	= 0x090004,
	NP_TROPHY_CMD_REGISTER_CONTEXT_DATA	= 0x090005,
	NP_TROPHY_CMD_READ_CONTEXT_DATA		= 0x090006,
	NP_TROPHY_CMD_UPDATE_CONTEXT_DATA	= 0x090007,

	/* Asynchronous request / response pairs */
	NP_TROPHY_CMD_REMOVE_ALL_TITLES		= 0x090008,
	NP_TROPHY_CMD_REMOVE_ALL_TITLES_RES	= 0x090009,
	NP_TROPHY_CMD_CHECK_RECOVERY		= 0x09000A,
	NP_TROPHY_CMD_CHECK_RECOVERY_RES	= 0x09000B,
	NP_TROPHY_CMD_PERFORM_RECOVERY		= 0x09000C,
	NP_TROPHY_CMD_PERFORM_RECOVERY_RES	= 0x09000D,
	NP_TROPHY_CMD_GET_USER_FILE_STATUS	= 0x09000E,
	NP_TROPHY_CMD_GET_USER_FILE_STATUS_RES	= 0x09000F,
	NP_TROPHY_CMD_GET_USER_TITLE_INFO	= 0x090010,
	NP_TROPHY_CMD_GET_USER_TITLE_INFO_RES	= 0x090011,
	NP_TROPHY_CMD_GET_NEXT_TITLE_STATUS	= 0x090012,
	NP_TROPHY_CMD_GET_NEXT_TITLE_STATUS_RES	= 0x090013,
	NP_TROPHY_CMD_GET_TITLE_FOR_REMOVAL	= 0x090014,
	NP_TROPHY_CMD_GET_TITLE_FOR_REMOVAL_RES	= 0x090015,
	NP_TROPHY_CMD_CHECK_NET_SYNC		= 0x090016,
	NP_TROPHY_CMD_CHECK_NET_SYNC_RES	= 0x090017,
	NP_TROPHY_CMD_NET_SYNC_TITLES		= 0x090018,
	NP_TROPHY_CMD_NET_SYNC_TITLES_RES	= 0x090019,
	NP_TROPHY_CMD_GET_TITLE_STATUS_GAME	= 0x09001A,
	NP_TROPHY_CMD_GET_TITLE_STATUS_GAME_RES	= 0x09001B,
	NP_TROPHY_CMD_INSTALL_TITLE		= 0x09001C,
	NP_TROPHY_CMD_INSTALL_TITLE_RES		= 0x09001D,
	NP_TROPHY_CMD_UPDATE_TITLE		= 0x09001E,
	NP_TROPHY_CMD_UPDATE_TITLE_RES		= 0x09001F,
	NP_TROPHY_CMD_SETUP_TITLE		= 0x090020,
	NP_TROPHY_CMD_SETUP_TITLE_RES		= 0x090021,
	NP_TROPHY_CMD_REMOVE_TITLE		= 0x090022,
	NP_TROPHY_CMD_REMOVE_TITLE_RES		= 0x090023,
	NP_TROPHY_CMD_GET_TITLE_FILE_STATUS	= 0x090024,
	NP_TROPHY_CMD_GET_TITLE_FILE_STATUS_RES	= 0x090025,
	NP_TROPHY_CMD_GET_TITLE_SYNC_STATUS	= 0x090026,
	NP_TROPHY_CMD_GET_TITLE_SYNC_STATUS_RES	= 0x090027,
	NP_TROPHY_CMD_GET_UNLOCKED_FLAG_ARRAY	= 0x090028,
	NP_TROPHY_CMD_GET_UNLOCKED_FLAG_ARR_RES	= 0x090029,
	NP_TROPHY_CMD_GET_TROPHY_TITLE_DATA	= 0x09002A,
	NP_TROPHY_CMD_GET_TROPHY_TITLE_DATA_RES	= 0x09002B,
	NP_TROPHY_CMD_GET_TROPHY_GROUP_DATA	= 0x09002C,
	NP_TROPHY_CMD_GET_TROPHY_GROUP_DATA_RES	= 0x09002D,
	NP_TROPHY_CMD_GET_TROPHY_DATA		= 0x09002E,
	NP_TROPHY_CMD_GET_TROPHY_DATA_RES	= 0x09002F,
	NP_TROPHY_CMD_GET_GAME_INFO		= 0x090030,
	NP_TROPHY_CMD_GET_GAME_INFO_RES		= 0x090031,
	NP_TROPHY_CMD_GET_GROUP_INFO		= 0x090032,
	NP_TROPHY_CMD_GET_GROUP_INFO_RES	= 0x090033,
	NP_TROPHY_CMD_GET_TROPHY_INFO		= 0x090034,
	NP_TROPHY_CMD_GET_TROPHY_INFO_RES	= 0x090035,
	NP_TROPHY_CMD_DEBUG_UNLOCK_TROPHY	= 0x090036,
	NP_TROPHY_CMD_DEBUG_UNLOCK_TROPHY_RES	= 0x090037,
	NP_TROPHY_CMD_DEBUG_LOCK_TROPHY		= 0x090038,
	NP_TROPHY_CMD_DEBUG_LOCK_TROPHY_RES	= 0x090039,
	NP_TROPHY_CMD_UNLOCK_TROPHY		= 0x09003A,
	NP_TROPHY_CMD_UNLOCK_TROPHY_RES		= 0x09003B,
	NP_TROPHY_CMD_OPEN_TROPHY_CONF_STREAM	= 0x09003C,
	NP_TROPHY_CMD_OPEN_TROPHY_CONF_RES	= 0x09003D,
	NP_TROPHY_CMD_OPEN_TITLE_ICON_STREAM	= 0x09003E,
	NP_TROPHY_CMD_OPEN_TITLE_ICON_RES	= 0x09003F,
	NP_TROPHY_CMD_OPEN_GROUP_ICON_STREAM	= 0x090040,
	NP_TROPHY_CMD_OPEN_GROUP_ICON_RES	= 0x090041,
	NP_TROPHY_CMD_OPEN_TROPHY_ICON_STREAM	= 0x090042,
	NP_TROPHY_CMD_OPEN_TROPHY_ICON_RES	= 0x090043,
	NP_TROPHY_CMD_READ_STREAM		= 0x090044,
	NP_TROPHY_CMD_READ_STREAM_RES		= 0x090045,
	NP_TROPHY_CMD_CLOSE_STREAM		= 0x090046
};

/* Trophy grade values (matching SceNpTrophyGrade) */
enum NpTrophyGradeValue {
	NP_TROPHY_GRADE_PLATINUM	= 1,
	NP_TROPHY_GRADE_GOLD		= 2,
	NP_TROPHY_GRADE_SILVER		= 3,
	NP_TROPHY_GRADE_BRONZE		= 4
};

/* Standard SceNpTrophy payload structures matching IPC wire sizes */
struct NpTrophyGameDetails {
	SceSize size;			/* 1180 */
	SceUInt32 numGroups;
	SceUInt32 numTrophies;
	SceUInt32 numPlatinum;
	SceUInt32 numGold;
	SceUInt32 numSilver;
	SceUInt32 numBronze;
	char title[128];		/* +0x1C */
	char description[1024];		/* +0x9C */
};

struct NpTrophyGameData {
	SceSize size;			/* 28 */
	SceUInt32 unlockedTrophies;
	SceUInt32 unlockedPlatinum;
	SceUInt32 unlockedGold;
	SceUInt32 unlockedSilver;
	SceUInt32 unlockedBronze;
	SceUInt32 progressPercentage;
};

struct NpTrophyGroupDetails {
	SceSize size;			/* 1180 */
	int groupId;
	SceUInt32 numTrophies;
	SceUInt32 numPlatinum;
	SceUInt32 numGold;
	SceUInt32 numSilver;
	SceUInt32 numBronze;
	char title[128];
	char description[1024];
};

struct NpTrophyGroupData {
	SceSize size;			/* 32 */
	int groupId;
	SceUInt32 unlockedTrophies;
	SceUInt32 unlockedPlatinum;
	SceUInt32 unlockedGold;
	SceUInt32 unlockedSilver;
	SceUInt32 unlockedBronze;
	SceUInt32 progressPercentage;
};

struct NpTrophyDetails {
	SceSize size;			/* 1172 */
	int trophyId;			/* +0x04 */
	int trophyGrade;		/* +0x08 */
	int groupId;			/* +0x0C */
	int hidden;			/* +0x10 */
	char name[128];			/* +0x14 */
	char description[1024];		/* +0x94 */
};

struct NpTrophyData {
	SceSize size;			/* 24 */
	int trophyId;
	int unlocked;
	SceUInt32 reserved;
	SceUInt64 timestamp;
};

struct NpTrophyFlagArray {
	SceUInt32 flagBits[4];		/* 16 bytes (128 trophies) */
};

struct NpTrophyCommId {
	char commId[12];		/* 12 bytes */
};

/* Base trophy job runtime (32-byte parent at 0x812EB10C / 0x81432938) */
class NpTrophyQueue;

class NpTrophyRuntimeJob {
	friend class NpTrophyQueue;
public:
	NpTrophyRuntimeJob(const char *name);
	virtual ~NpTrophyRuntimeJob();
	virtual int Cancel(int cancelError);
	virtual int Run() = 0;
	virtual void Finish(int result) = 0;

protected:
	int m_requestId;		/* +0x04 */
	const char *m_name;		/* +0x08 */
	int m_state;			/* +0x0C */
	NpTrophyRuntimeJob *m_next;	/* +0x10 */
	NpTrophyRuntimeJob *m_prev;	/* +0x14 */
	NpTrophyQueue *m_queue;		/* +0x18 */
	int m_cancelError;		/* +0x1C */
};

/* SceNpTrophyServer IPC job base (48 bytes; vtable 0x81534DF4) */
class SceNpTrophyJobBase : public NpTrophyRuntimeJob {
public:
	SceNpTrophyJobBase(const char *name, IPMI::Session *session, SceUInt32 ipcReqId,
			   int handleId);
	virtual ~SceNpTrophyJobBase();
	virtual int Cancel(int cancelError);

	static void *operator new(size_t size);
	static void operator delete(void *ptr);

protected:
	IPMI::Session *m_session;	/* +0x20 */
	SceUInt32 m_ipcReqId;		/* +0x24 */
	SceUID m_clientPid;		/* +0x28 */
	int m_handleId;			/* +0x2C */
};

/* NetCtl process refcount exports (implemented in netctl_api.cpp) */
extern "C" {
extern int SceShell_6B8772A2(SceUID pid);
extern int SceShell_794BF5BE(unsigned int token);
extern int SceNpCommon_E426FEE8(const NpTrophyCommId *commId, char *outStr, SceSize outSize);
}

/* Trophy server internal heap & job queue helpers (0x812E9978..0x812EB550) */
extern int npTrophyHeapInit(void *baseAddr, SceSize size);				/* 0x812E9978 */
extern int npTrophyHeapTerm(void);							/* 0x812E99DE */
extern void *npTrophyHeapGetContext(void);						/* 0x812E9A76 */
extern void *npTrophyHeapAlloc(SceSize size);						/* 0x812E9A80 */
extern void npTrophyHeapFree(void *ptr);						/* 0x812E9AAA */
extern void *npTrophyHeapAllocObj(SceSize size, void *heapCtx);				/* 0x812E9AC0 */
extern void npTrophyHeapFreeObj(void *ptr);						/* 0x812E9ADC */

extern NpTrophyQueue *npTrophyQueueConstruct(void *mem);				/* 0x812EB1F8 */
extern int npTrophyQueueTerm(NpTrophyQueue *queue);					/* 0x812EB230 */
extern int npTrophyQueueCancelAll(NpTrophyQueue *queue, int errorCode);			/* 0x812EB26E */
extern int npTrophyQueueInit(NpTrophyQueue *queue, const char *name, SceUInt32 attr,
			     SceSize stackSize);					/* 0x812EB3B4 */
extern int npTrophyQueueStart(NpTrophyQueue *queue);					/* 0x812EB46A */
extern int npTrophyQueueEnqueue(NpTrophyQueue *queue, int handleId,
				SceNpTrophyJobBase *job);				/* 0x812EB4A4 */
extern int npTrophyQueueCancelHandle(NpTrophyQueue *queue, int handleId,
				     int cancelError);					/* 0x812EB550 */

/* Trophy core service functions (0x812C16B4..0x812C7DFC) */
extern int npTrophyCoreCreateContext(SceUID pid, IPMI::Session *session, int *outCtxId,
				     int titleIdMode, const void *commId, const void *commSign,
				     SceUInt64 options);				/* 0x812C16B4 */
extern int npTrophyCoreDestroyContext(SceUID pid, IPMI::Session *session, int ctxId);	/* 0x812C1832 */
extern int npTrophyCoreRegisterContextData(SceUID pid, IPMI::Session *session,
					   const void *commId, const void *buf,
					   SceSize bufSize);				/* 0x812C19B4 */
extern int npTrophyCoreReadContextData(SceUID pid, IPMI::Session *session,
				       const void *commId, void *outBuf,
				       SceSize bufSize);				/* 0x812C19DA */
extern int npTrophyCoreUpdateContextData(SceUID pid, IPMI::Session *session,
					 int ctxId, const void *buf,
					 SceSize bufSize);				/* 0x812C1A00 */
extern int npTrophyCoreCreateHandle(SceUID pid, IPMI::Session *session, int handleType,
				    int *outHandleId);					/* 0x812C1A28 */
extern int npTrophyCoreDestroyHandle(SceUID pid, IPMI::Session *session, int handleId);	/* 0x812C1B76 */
extern int npTrophyCoreAbortHandle(SceUID pid, IPMI::Session *session, int handleId);	/* 0x812C1DC8 */
extern int npTrophyCoreGetHandleProgress(SceUID pid, IPMI::Session *session, int handleId,
					 int *outProgress);				/* 0x812C1F22 */
extern int npTrophyCoreInit(void);							/* 0x812C2094 */
extern int npTrophyCoreTerm(void);							/* 0x812C2154 */
extern int npTrophyCoreCheckNetSync(SceUID pid, IPMI::Session *session, int handleId,
				    int ctxId, SceUInt64 options, int *outNeedSync);	/* 0x812C21AC */
extern int npTrophyCoreNetSyncTitles(SceUID pid, IPMI::Session *session, int handleId,
				     int ctxId, SceUInt64 options);			/* 0x812C236C */
extern int npTrophyCoreOnClientConnect(SceUID pid, IPMI::Session *session);		/* 0x812C2504 */
extern int npTrophyCoreOnClientDisconnect(SceUID pid, IPMI::Session *session,
					  int reason);					/* 0x812C252C */
extern int npTrophyCoreSuspend(void);							/* 0x812C2558 */
extern int npTrophyCoreResume(void);							/* 0x812C257C */
extern int npTrophyCoreCheckRecovery(SceUID pid, IPMI::Session *session, int handleId,
				     int ctxId, int *outRequired);			/* 0x812C25A0 */
extern int npTrophyCorePerformRecovery(SceUID pid, IPMI::Session *session, int handleId,
				       int ctxId);					/* 0x812C2738 */
extern int npTrophyCoreGetTitleForRemoval(SceUID pid, IPMI::Session *session, int handleId,
					  int ctxId, void *outInfo, void *outExtra,
					  NpTrophyGameDetails *outDetails);		/* 0x812C28C8 */
extern int npTrophyCoreRemoveTitle(SceUID pid, IPMI::Session *session, int handleId,
				   int ctxId, int titleHandle);				/* 0x812C2A60 */
extern int npTrophyCoreRemoveAllTitles(SceUID pid, IPMI::Session *session, int handleId,
				       int ctxId);					/* 0x812C2C80 */
extern int npTrophyCoreCreateTitleSession(SceUID pid, IPMI::Session *session,
					  int sessionFlags, int limit, int reserved,
					  SceUInt64 options, int *outTitleSession);	/* 0x812C2DF8 */
extern int npTrophyCoreDestroyTitleSession(SceUID pid, IPMI::Session *session,
					   int titleSession);				/* 0x812C307A */
extern int npTrophyCoreOpenConfStream(SceUID pid, IPMI::Session *session, int handleId,
				      int ctxId, int mode, SceUInt64 options,
				      int *outStreamId, SceUInt64 *outSize);		/* 0x812C31A0 */
extern int npTrophyCoreOpenTitleIconStream(SceUID pid, IPMI::Session *session, int handleId,
					   int ctxId, int mode, SceUInt64 options,
					   int *outStreamId, SceUInt64 *outSize);	/* 0x812C362E */
extern int npTrophyCoreOpenGroupIconStream(SceUID pid, IPMI::Session *session, int handleId,
					   int ctxId, int mode, int groupId,
					   SceUInt64 options, int *outStreamId,
					   SceUInt64 *outSize);				/* 0x812C3BB8 */
extern int npTrophyCoreOpenTrophyIconStream(SceUID pid, IPMI::Session *session, int handleId,
					    int ctxId, int mode, int trophyId,
					    SceUInt64 options, int *outStreamId,
					    SceUInt64 *outSize);			/* 0x812C41A2 */
extern int npTrophyCoreReadStream(SceUID pid, IPMI::Session *session, int handleId,
				  int streamId, void *buf, SceSize bufSize,
				  SceSize *outReadSize);				/* 0x812C4776 */
extern int npTrophyCoreCloseStream(SceUID pid, IPMI::Session *session, int streamId);	/* 0x812C4C10 */
extern int npTrophyCoreGetTrophyIconPath(const NpTrophyCommId *commId, int trophyId,
					 char *outPath, SceSize outPathSize);		/* 0x812C4D36 */
extern int npTrophyCoreGetTrophyTitleData(SceUID pid, IPMI::Session *session, int handleId,
					  int ctxId, int titleHandle, void *outInfo,
					  void *outStats);				/* 0x812C4DE0 */
extern int npTrophyCoreGetTrophyGroupData(SceUID pid, IPMI::Session *session, int handleId,
					  int ctxId, int titleHandle, int groupId,
					  void *outInfo, void *outStats);		/* 0x812C502E */
extern int npTrophyCoreGetTrophyData(SceUID pid, IPMI::Session *session, int handleId,
				     int ctxId, int titleHandle, int trophyId,
				     void *outInfo, void *outStats);			/* 0x812C5286 */
extern int npTrophyCoreGetNextTitleEntry(SceUID pid, IPMI::Session *session, int handleId,
					 int ctxId, int cursor, int *outNextCursor,
					 void *outEntry);				/* 0x812C54E0 */
extern int npTrophyCoreGetGameInfo(SceUID pid, IPMI::Session *session, int handleId,
				   int ctxId, int mode, SceUInt64 options,
				   NpTrophyGameDetails *outDetails,
				   NpTrophyGameData *outData);				/* 0x812C5678 */
extern int npTrophyCoreGetGroupInfo(SceUID pid, IPMI::Session *session, int handleId,
				    int ctxId, int mode, int groupId, SceUInt64 options,
				    NpTrophyGroupDetails *outDetails,
				    NpTrophyGroupData *outData);			/* 0x812C5A2A */
extern int npTrophyCoreGetTrophyInfo(SceUID pid, IPMI::Session *session, int handleId,
				     int ctxId, int mode, int trophyId, SceUInt64 options,
				     NpTrophyDetails *outDetails,
				     NpTrophyData *outData);				/* 0x812C5DB4 */
extern int npTrophyCoreGetTitleStatusForGame(SceUID pid, IPMI::Session *session, int handleId,
					     int titleSession, int ctxId,
					     int *outStatus);				/* 0x812C6180 */
extern int npTrophyCoreInstallTitle(SceUID pid, IPMI::Session *session, int handleId,
				    int titleSession, int ctxId, int *outResult);	/* 0x812C63D0 */
extern int npTrophyCoreUpdateTitle(SceUID pid, IPMI::Session *session, int handleId,
				   int titleSession, int ctxId, int *outResult);	/* 0x812C6686 */
extern int npTrophyCoreSetupTitle(SceUID pid, IPMI::Session *session, int handleId,
				  int titleSession, int ctxId, int *outResult);		/* 0x812C693C */
extern int npTrophyCoreGetTitleFileStatus(SceUID pid, IPMI::Session *session, int handleId,
					  int titleSession, int ctxId,
					  void *outStatus);				/* 0x812C6BF0 */
extern int npTrophyCoreGetTitleSyncStatus(SceUID pid, IPMI::Session *session, int handleId,
					  int titleSession, int ctxId,
					  void *outStatus);				/* 0x812C6F38 */
extern int npTrophyCoreUnlockTrophy(SceUID pid, IPMI::Session *session, int handleId,
				    int ctxId, int trophyId, int flags, int *outPlatinumId,
				    NpTrophyGameDetails *outGameDetails,
				    NpTrophyDetails *outTrophyDetails,
				    NpTrophyDetails *outPlatinumDetails,
				    NpTrophyCommId *outCommId, int *ioUnlockedFlag);	/* 0x812C7178 */
extern int npTrophyCoreDebugUnlockTrophy(SceUID pid, IPMI::Session *session, int handleId,
					 int ctxId, int trophyId, const SceUInt64 *timestamp,
					 int *outPlatinumId);				/* 0x812C74FC */
extern int npTrophyCoreDebugLockTrophy(SceUID pid, IPMI::Session *session, int handleId,
				       int ctxId, int trophyId, const SceUInt64 *timestamp,
				       int *outPlatinumId);				/* 0x812C775E */
extern int npTrophyCoreGetUnlockedFlagArray(SceUID pid, IPMI::Session *session, int handleId,
					    int ctxId, int mode,
					    NpTrophyFlagArray *outFlags,
					    SceUInt32 *outCount);			/* 0x812C79C0 */
extern int npTrophyCoreGetUserFileStatus(SceUID pid, IPMI::Session *session, int handleId,
					 int ctxId, void *outStatus);			/* 0x812C7C88 */
extern int npTrophyCoreGetUserTitleInfo(SceUID pid, IPMI::Session *session, int handleId,
					int ctxId, void *outInfo);			/* 0x812C7DFC */

/* Cross-file SceNpTrophyServer functions (nptrophy_ipc_server.cpp) */
extern int npTrophyQueueSubmitJob(SceNpTrophyJobBase *job);
extern int npTrophyQueueCancelByHandle(int handleId);

/* Async response senders (nptrophy_ipc_server.cpp: 0x812271F4..0x812279DC) */
extern int npTrophySendRemoveAllTitlesResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					       int result);
extern int npTrophySendCheckRecoveryResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					     int result, const int *outRequired);
extern int npTrophySendPerformRecoveryResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					       int result);
extern int npTrophySendGetUserFileStatusResponse(IPMI::Session *session, SceUInt32 ipcReqId,
						 int result, const void *outStatus);
extern int npTrophySendGetUserTitleInfoResponse(IPMI::Session *session, SceUInt32 ipcReqId,
						int result, const void *outInfo);
extern int npTrophySendGetNextTitleStatusResponse(IPMI::Session *session, SceUInt32 ipcReqId,
						  int result, const int *outNextCursor,
						  const void *outEntry);
extern int npTrophySendGetTitleForRemovalResponse(IPMI::Session *session, SceUInt32 ipcReqId,
						  int result, const void *outInfo,
						  const void *outExtra,
						  const NpTrophyGameDetails *outDetails);
extern int npTrophySendCheckNetSyncResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					    int result, const int *outNeedSync);
extern int npTrophySendNetSyncTitlesResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					     int result);
extern int npTrophySendGetTitleStatusForGameResponse(IPMI::Session *session, SceUInt32 ipcReqId,
						     int result, const int *outStatus);
extern int npTrophySendInstallTitleResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					    int result, const int *outResult);
extern int npTrophySendUpdateTitleResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					   int result, const int *outResult);
extern int npTrophySendSetupTitleResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					  int result, const int *outResult);
extern int npTrophySendRemoveTitleResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					   int result);
extern int npTrophySendGetTitleFileStatusResponse(IPMI::Session *session, SceUInt32 ipcReqId,
						  int result, const void *outStatus);
extern int npTrophySendGetTitleSyncStatusResponse(IPMI::Session *session, SceUInt32 ipcReqId,
						  int result, const void *outStatus);
extern int npTrophySendGetUnlockedFlagArrayResponse(IPMI::Session *session, SceUInt32 ipcReqId,
						    int result,
						    const NpTrophyFlagArray *outFlags,
						    const SceUInt32 *outCount);
extern int npTrophySendGetTrophyTitleDataResponse(IPMI::Session *session, SceUInt32 ipcReqId,
						  int result, const void *outInfo,
						  const void *outStats);
extern int npTrophySendGetTrophyGroupDataResponse(IPMI::Session *session, SceUInt32 ipcReqId,
						  int result, const void *outInfo,
						  const void *outStats);
extern int npTrophySendGetTrophyDataResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					     int result, const void *outInfo,
					     const void *outStats);
extern int npTrophySendGetGameInfoResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					   int result, const NpTrophyGameDetails *outDetails,
					   const NpTrophyGameData *outData);
extern int npTrophySendGetGroupInfoResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					    int result, const NpTrophyGroupDetails *outDetails,
					    const NpTrophyGroupData *outData);
extern int npTrophySendGetTrophyInfoResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					     int result, const NpTrophyDetails *outDetails,
					     const NpTrophyData *outData);
extern int npTrophySendDebugUnlockTrophyResponse(IPMI::Session *session, SceUInt32 ipcReqId,
						 int result, const int *outPlatinumId);
extern int npTrophySendDebugLockTrophyResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					       int result, const int *outPlatinumId);
extern int npTrophySendUnlockTrophyResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					    int result, const int *outPlatinumId,
					    const int *outFlag);
extern int npTrophySendOpenConfStreamResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					      int result, const int *outStreamId,
					      const SceUInt64 *outSize);
extern int npTrophySendOpenTitleIconStreamResponse(IPMI::Session *session, SceUInt32 ipcReqId,
						   int result, const int *outStreamId,
						   const SceUInt64 *outSize);
extern int npTrophySendOpenGroupIconStreamResponse(IPMI::Session *session, SceUInt32 ipcReqId,
						   int result, const int *outStreamId,
						   const SceUInt64 *outSize);
extern int npTrophySendOpenTrophyIconStreamResponse(IPMI::Session *session, SceUInt32 ipcReqId,
						    int result, const int *outStreamId,
						    const SceUInt64 *outSize);
extern int npTrophySendReadStreamResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					  int result, const SceSize *outReadSize,
					  const void *outBuf);

/* Async job enqueue entrypoints (nptrophy_job.cpp: 0x81227FB8..0x8122998C) */
extern int npTrophyEnqueueCheckNetSyncJob(IPMI::Session *session, SceUInt32 ipcReqId,
					  int handleId, int ctxId, SceUInt64 options);
extern int npTrophyEnqueueNetSyncTitlesJob(IPMI::Session *session, SceUInt32 ipcReqId,
					   int handleId, int ctxId, SceUInt64 options);
extern int npTrophyEnqueueCheckRecoveryJob(IPMI::Session *session, SceUInt32 ipcReqId,
					   int handleId, int ctxId);
extern int npTrophyEnqueuePerformRecoveryJob(IPMI::Session *session, SceUInt32 ipcReqId,
					     int handleId, int ctxId);
extern int npTrophyEnqueueGetTitleForRemovalJob(IPMI::Session *session, SceUInt32 ipcReqId,
						int handleId, int ctxId);
extern int npTrophyEnqueueRemoveTitleJob(IPMI::Session *session, SceUInt32 ipcReqId,
					 int handleId, int ctxId, int titleHandle);
extern int npTrophyEnqueueRemoveAllTitlesJob(IPMI::Session *session, SceUInt32 ipcReqId,
					     int handleId, int ctxId);
extern int npTrophyEnqueueGetUserFileStatusJob(IPMI::Session *session, SceUInt32 ipcReqId,
					       int handleId, int ctxId);
extern int npTrophyEnqueueGetUserTitleInfoJob(IPMI::Session *session, SceUInt32 ipcReqId,
					      int handleId, int ctxId);
extern int npTrophyEnqueueReadStreamJob(IPMI::Session *session, SceUInt32 ipcReqId,
					int handleId, int streamId, SceSize readSize);
extern int npTrophyEnqueueOpenConfStreamJob(IPMI::Session *session, SceUInt32 ipcReqId,
					    int handleId, int ctxId, int mode, SceUInt64 options,
					    int wantSize);
extern int npTrophyEnqueueGetTrophyTitleDataJob(IPMI::Session *session, SceUInt32 ipcReqId,
						int handleId, int ctxId, int titleHandle);
extern int npTrophyEnqueueGetTrophyGroupDataJob(IPMI::Session *session, SceUInt32 ipcReqId,
						int handleId, int ctxId, int titleHandle,
						int groupId);
extern int npTrophyEnqueueGetTrophyDataJob(IPMI::Session *session, SceUInt32 ipcReqId,
					   int handleId, int ctxId, int titleHandle, int trophyId);
extern int npTrophyEnqueueGetNextTitleStatusJob(IPMI::Session *session, SceUInt32 ipcReqId,
						int handleId, int ctxId, int cursor);
extern int npTrophyEnqueueOpenTitleIconStreamJob(IPMI::Session *session, SceUInt32 ipcReqId,
						 int handleId, int ctxId, int mode,
						 SceUInt64 options, int wantSize);
extern int npTrophyEnqueueOpenGroupIconStreamJob(IPMI::Session *session, SceUInt32 ipcReqId,
						 int handleId, int ctxId, int mode, int groupId,
						 SceUInt64 options, int wantSize);
extern int npTrophyEnqueueOpenTrophyIconStreamJob(IPMI::Session *session, SceUInt32 ipcReqId,
						  int handleId, int ctxId, int mode, int trophyId,
						  SceUInt64 options, int wantSize);
extern int npTrophyEnqueueGetGameInfoJob(IPMI::Session *session, SceUInt32 ipcReqId,
					 int handleId, int ctxId, int mode, SceUInt64 options);
extern int npTrophyEnqueueGetGroupInfoJob(IPMI::Session *session, SceUInt32 ipcReqId,
					  int handleId, int ctxId, int mode, int groupId,
					  SceUInt64 options);
extern int npTrophyEnqueueGetTrophyInfoJob(IPMI::Session *session, SceUInt32 ipcReqId,
					   int handleId, int ctxId, int mode, int trophyId,
					   SceUInt64 options);
extern int npTrophyEnqueueGetTitleStatusForGameJob(IPMI::Session *session, SceUInt32 ipcReqId,
						   int handleId, int titleSession, int ctxId);
extern int npTrophyEnqueueInstallTitleJob(IPMI::Session *session, SceUInt32 ipcReqId,
					  int handleId, int titleSession, int ctxId);
extern int npTrophyEnqueueUpdateTitleJob(IPMI::Session *session, SceUInt32 ipcReqId,
					 int handleId, int titleSession, int ctxId);
extern int npTrophyEnqueueSetupTitleJob(IPMI::Session *session, SceUInt32 ipcReqId,
					int handleId, int titleSession, int ctxId);
extern int npTrophyEnqueueGetTitleFileStatusJob(IPMI::Session *session, SceUInt32 ipcReqId,
						int handleId, int titleSession, int ctxId);
extern int npTrophyEnqueueGetTitleSyncStatusJob(IPMI::Session *session, SceUInt32 ipcReqId,
						int handleId, int titleSession, int ctxId);
extern int npTrophyEnqueueDebugUnlockTrophyJob(IPMI::Session *session, SceUInt32 ipcReqId,
					       int handleId, int ctxId, int trophyId,
					       const SceUInt64 *timestamp);
extern int npTrophyEnqueueDebugLockTrophyJob(IPMI::Session *session, SceUInt32 ipcReqId,
					     int handleId, int ctxId, int trophyId,
					     const SceUInt64 *timestamp);
extern int npTrophyEnqueueUnlockTrophyJob(IPMI::Session *session, SceUInt32 ipcReqId,
					  int handleId, int ctxId, int trophyId, int flags,
					  const int *inFlag);
extern int npTrophyEnqueueGetUnlockedFlagArrayJob(IPMI::Session *session, SceUInt32 ipcReqId,
						  int handleId, int ctxId, int mode);

/* Trophy unlock notifications (nptrophy_notify.cpp: 0x81229A58..0x8122ACC0) */
extern void npTrophyPostUnlockActivity(SceUID clientPid, const NpTrophyDetails *trophyDetails,
				       const NpTrophyGameDetails *gameDetails,
				       const NpTrophyCommId *commId);
extern void npTrophyPostUnlockNewEvent(const NpTrophyDetails *trophyDetails,
				       const NpTrophyCommId *commId);

#endif /* SHELL_NP_TROPHY_INTERNAL_H */
