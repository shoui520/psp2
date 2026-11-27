/*
 * np_imps_svc.h: SceNpImpsSvc internal declarations (0x812F899C..0x812FFE9D).
 *
 * SceNpImpsSvc bridges SceNpImpsCache, SceNpRoster, SceNpImpsCore, and SceNpImpsUtil:
 *   - manages listener registration and event fanout (online status, service state,
 *     friend list, block list, presence changes, in-game data messages),
 *   - tracks local user presence (title metadata from LSDB/AppMgr, game presence,
 *     party presence) and publishes presence updates to SceNpImpsCore,
 *   - runs an async context manager worker thread ("SceNpImpsSvcCtxMgr") for roster
 *     mutations (request/accept/delete friend, add/delete block).
 */
#ifndef SHELL_NP_IMPS_SVC_H
#define SHELL_NP_IMPS_SVC_H

#include <stddef.h>
#include <scetypes.h>
#include <np/np_common.h>
#include <np/np_basic.h>
#include <common/ref_obj.h>
#include <memory/heap_allocator.h>
#include <datetime/datetime.h>
#include <thread/thread.h>

/* SceNpImpsSvc error codes (0x80551400 range) */
#define NP_IMPS_SVC_ERROR_INTERNAL		-2141906431	/* 0x80551401 */
#define NP_IMPS_SVC_ERROR_INVALID_ARGUMENT	-2141906430	/* 0x80551402 */
#define NP_IMPS_SVC_ERROR_OUT_OF_MEMORY		-2141906429	/* 0x80551403 */
#define NP_IMPS_SVC_ERROR_NOT_INITIALIZED	-2141906428	/* 0x80551404 */
#define NP_IMPS_SVC_ERROR_ALREADY_INITIALIZED	-2141906427	/* 0x80551405 */
#define NP_IMPS_SVC_ERROR_LISTENER_NOT_FOUND	-2141906426	/* 0x80551406 */
#define NP_IMPS_SVC_ERROR_NO_CALLBACK		-2141906425	/* 0x80551407 */
#define NP_IMPS_SVC_ERROR_SIGNED_OUT		-2141906424	/* 0x80551408 */
#define NP_IMPS_SVC_ERROR_OFFLINE		-2141906423	/* 0x80551409 */
#define NP_IMPS_SVC_ERROR_ABORTED		-2141906422	/* 0x8055140A */
#define NP_IMPS_SVC_ERROR_ALREADY_ABORTED	-2141906421	/* 0x8055140B */
#define NP_IMPS_SVC_ERROR_CONTEXT_LIMIT		-2141906420	/* 0x8055140C */
#define NP_IMPS_SVC_ERROR_NO_PRESENCE		-2141906417	/* 0x8055140F */
#define NP_IMPS_SVC_ERROR_NO_COMM_ID		-2141906415	/* 0x80551411 */
#define NP_IMPS_SVC_ERROR_QUEUE_FULL		-2141906413	/* 0x80551413 */
#define NP_IMPS_SVC_ERROR_NO_JOINABLE_COMM_ID	-2141906412	/* 0x80551414 */
#define NP_IMPS_SVC_ERROR_WRONG_PROCESS		-2141906411	/* 0x80551415 */

#define NP_PARTY_ERROR_NOT_IN_PARTY		-2141875964	/* 0x80559104 */

/* Service state values */
enum NpImpsSvcServiceState {
	NP_IMPS_SVC_STATE_SIGNED_OUT	= 1,
	NP_IMPS_SVC_STATE_SIGNED_IN	= 2,
	NP_IMPS_SVC_STATE_ONLINE	= 3
};

/* Online status values */
enum NpImpsSvcOnlineStatus {
	NP_IMPS_SVC_ONLINE_OFFLINE	= 1,
	NP_IMPS_SVC_ONLINE_AWAY		= 2,
	NP_IMPS_SVC_ONLINE_ONLINE	= 3
};

/* Platform values */
enum NpImpsSvcPlatform {
	NP_IMPS_SVC_PLATFORM_UNSPECIFIED	= 0,
	NP_IMPS_SVC_PLATFORM_PSP2		= 1,
	NP_IMPS_SVC_PLATFORM_PS3		= 2
};

/* Presence diff bitmask (0x812FA652) */
enum NpImpsSvcPresenceDiffMask {
	NP_IMPS_DIFF_ONLINE_STATUS	= 0x001,
	NP_IMPS_DIFF_PLATFORM		= 0x002,
	NP_IMPS_DIFF_COMMENT		= 0x004,
	NP_IMPS_DIFF_TITLE_PLATFORM	= 0x008,
	NP_IMPS_DIFF_LAST_ONLINE	= 0x010,
	NP_IMPS_DIFF_TITLE		= 0x020,
	NP_IMPS_DIFF_COMM_ID		= 0x040,
	NP_IMPS_DIFF_GAME_PRESENCE	= 0x080,
	NP_IMPS_DIFF_PARTY_PRESENCE	= 0x100,
	NP_IMPS_DIFF_CONTENT_INFO	= 0x200
};

/* Roster sync trigger codes passed to SceNpImpsCache (0x812FF4BC) */
enum NpImpsRosterSyncReason {
	NP_IMPS_ROSTER_SYNC_REQUEST_FRIEND	= 1,
	NP_IMPS_ROSTER_SYNC_ACCEPT_FRIEND	= 2,
	NP_IMPS_ROSTER_SYNC_DELETE_FRIEND	= 3,
	NP_IMPS_ROSTER_SYNC_ADD_BLOCK		= 4,
	NP_IMPS_ROSTER_SYNC_DELETE_BLOCK	= 5
};

/* App title metadata populated by NpImpsSvcMyPresence::GetAppTitleInfo (155 bytes) */
struct NpImpsAppTitleInfo {
	char titleId[10];		/* +0x000 */
	char longTitleName[128];	/* +0x00A */
	char shortTitleName[16];	/* +0x08A */
	SceUInt8 isBigAppGame;		/* +0x09A */
};

/* Game presence input payload from client apps (332 bytes) */
struct NpImpsGamePresenceInput {
	SceSize size;			/* +0x000 */
	int presenceType;		/* +0x004 */
	char status[192];		/* +0x008 */
	SceUInt8 gameData[128];		/* +0x0C8 */
	SceSize gameDataSize;		/* +0x148 */
};

/* Party presence payload (460 bytes = 0x1CC) */
struct NpImpsPartyPresence {
	SceSize size;			/* +0x000 (460) */
	int presenceType;		/* +0x004 */
	char title[128];		/* +0x008 */
	char status[192];		/* +0x088 */
	SceUInt8 partyData[128];	/* +0x148 */
	SceSize partyDataSize;		/* +0x1C8 */
};

/* Game presence with title (464 bytes = 0x1D0, matches SceNpBasicGamePresence) */
struct NpImpsGamePresenceWithTitle {
	SceSize size;			/* +0x000 (464) */
	char title[128];		/* +0x004 */
	NpImpsGamePresenceInput body;	/* +0x084 (332 bytes) */
};

/* Unified friend/self presence snapshot (1016 bytes = 0x3F8) */
struct NpImpsSvcPresenceSnapshot {
	int onlineStatus;			/* +0x000 */
	int platform;				/* +0x004 */
	int titlePlatform;			/* +0x008 */
	char titleName[128];			/* +0x00C */
	SceNpCommunicationId titleId;		/* +0x08C */
	SceNpCommunicationId commId;		/* +0x098 */
	char contentId[48];			/* +0x0A4 */
	SceNpCommunicationId joinableCommId;	/* +0x0D4 */
	NpImpsGamePresenceInput gamePresence;	/* +0x0E0 (332 bytes) */
	NpImpsPartyPresence partyPresence;	/* +0x22C (460 bytes) */
};

/* Presence diff flags array returned by NpImpsSvcPresenceData::Compare (11 bytes) */
struct NpImpsSvcPresenceDiffFlags {
	SceUInt8 effectiveStatusChanged;	/* [0] */
	SceUInt8 onlineStatusChanged;		/* [1] */
	SceUInt8 platformChanged;		/* [2] */
	SceUInt8 commentChanged;		/* [3] */
	SceUInt8 titlePlatformChanged;		/* [4] */
	SceUInt8 lastOnlineChanged;		/* [5] */
	SceUInt8 titleChanged;			/* [6] */
	SceUInt8 commIdChanged;			/* [7] */
	SceUInt8 gamePresenceChanged;		/* [8] */
	SceUInt8 partyPresenceChanged;		/* [9] */
	SceUInt8 contentInfoChanged;		/* [10] */
};

/* In-game data message payload passed to listeners (528 bytes) */
struct NpImpsSvcInGameDataPayload {
	SceNpCommunicationId commId;		/* +0x000 (12 bytes) */
	SceUInt8 data[512];			/* +0x00C */
	SceSize dataSize;			/* +0x20C */
};

/* SceNpImpsCore presence wire structures (passed to npImpsCoreUpdatePresence) */
struct NpImpsCoreTitleNode {
	NpImpsCoreTitleNode *next;		/* +0x000 */
	int platform;				/* +0x004 */
	SceNpCommunicationId titleId;		/* +0x008 */
	char status[201];			/* +0x014 */
	char titleIdStr[128];			/* +0x0DD */
	char titleName[128];			/* +0x15D */
	SceUInt8 padAfterTitle[3];		/* +0x1DD */
	int presenceType;			/* +0x1E0 */
	SceUInt8 isSystemTitle;			/* +0x1E4 */
	SceUInt8 padAfterSystem[7];		/* +0x1E5 */
	int updatePriority;			/* +0x1EC */
	SceNpCommunicationId joinableCommId;	/* +0x1F0 */
	char contentId[48];			/* +0x1FC */
};

struct NpRtCapsNode;
struct NpRtStoredExt;

struct NpImpsCoreDataNode {
	NpImpsCoreDataNode *next;		/* +0x000 */
	char *mimeType;				/* +0x004: <data type="..."> */
	int valid;				/* +0x008: inline format (1=base64, 2=uri, 3=fileid) */
	void *dataPtr;				/* +0x00C */
	SceSize dataSize;			/* +0x010 */
	SceUInt32 declaredSize;			/* +0x014: <data size="..."> */
	char *dataName;				/* +0x018: <data name="..."> */
	SceNpCommunicationId commId;		/* +0x01C: <x context="..."> */
	SceNpCommunicationId targetCommId;	/* +0x028: <origin> */
	char defaultName[128];			/* +0x034: <defaultName> */
	char localName[128];			/* +0x0B4: <localName> */
	SceUInt8 localLang;			/* +0x134: <localLang> */
	SceUInt8 padAfterLocalLang[3];		/* +0x135 */
	SceUInt32 padBeforeActive;		/* +0x138 */
	int activeState;			/* +0x13C: <x active="0"|"1"> */
};

struct NpImpsCorePresenceUpdate {
	int onlineStatus;			/* +0x000 */
	int platform;				/* +0x004 */
	char *comment;				/* +0x008 */
	NpImpsCoreTitleNode *titleList;		/* +0x00C */
	NpImpsCoreDataNode *dataList;		/* +0x010 */
	paf::rtc::Tick *lastOnlineTick;		/* +0x014 */
	NpRtCapsNode *capsList;			/* +0x018 */
	NpRtStoredExt *storedExt;		/* +0x01C */
};

/* Init configuration for SceNpImpsSvc (16 bytes) */
struct NpImpsSvcInitParam {
	int cacheThreadPriority;		/* +0x00 */
	SceSize cacheThreadStackSize;		/* +0x04 */
	int ctxMgrThreadPriority;		/* +0x08 */
	SceSize ctxMgrThreadStackSize;		/* +0x0C */
};

/* Forward declarations for SceNpImpsCache interfaces */
class NpImpsCacheFriendEntry;
class NpImpsCacheFriendStore;
class NpImpsCacheBlockStore;
class NpImpsCacheRosterSync;
class NpImpsCacheCore;
class NpImpsSvc;

/* Ref-counted handle wrapper around a cached friend entry */
typedef NpImpsCacheFriendEntry *NpImpsCacheFriendPtr;

/* Callback signatures for SceNpImpsSvc listeners (52 bytes) */
struct NpImpsSvcListenerCallbacks {
	void (*onServiceState)(int state, int errorCode, void *userArg);
	void (*onMyOnlineStatus)(int status, void *userArg);
	void (*onOnlineTransition)(int isOnline, void *userArg);
	void (*onFriendList)(int eventType, const SceNpId *npIds,
			     NpImpsCacheFriendPtr **handles, SceSize count, void *userArg);
	void (*onFriendPresence)(int eventType, NpImpsCacheFriendPtr *handle, const SceNpId *npId,
				 const NpImpsSvcPresenceSnapshot *curPresence,
				 const NpImpsSvcPresenceSnapshot *prevPresence,
				 SceUInt32 diffMask, void *userArg);
	void (*onBlockList)(int eventType, const SceNpId *npIds, SceSize count, void *userArg);
	void (*onInGameDataMessage)(int eventType, const SceNpId *npId,
				    const NpImpsSvcInGameDataPayload *payload, void *userArg);
	void (*onFriendRemoved)(const SceNpId *npIds, SceSize count, void *userArg);
	void (*onMyPresence)(int eventType, const NpImpsSvcPresenceSnapshot *curPresence,
			     const NpImpsSvcPresenceSnapshot *prevPresence, void *userArg);
	void (*onPlatformFriendPresence)(int eventType, NpImpsCacheFriendPtr *handle,
					 const SceNpId *npId,
					 const NpImpsSvcPresenceSnapshot *curPresence,
					 const NpImpsSvcPresenceSnapshot *prevPresence,
					 SceUInt32 diffMask, int platform, void *userArg);
	void (*onFriendContext)(int eventType, NpImpsCacheFriendPtr *handle, const SceNpId *npId,
				int contextState, void *extra, void *userArg);
	void (*onFriendRequestRemoved)(const SceNpId *npIds, SceSize count, void *userArg);
	void *userArg;
};

/* Async roster operation completion callback */
typedef void (*NpImpsSvcRosterCallback)(int result, SceUInt32 contextId, const SceNpId *npId);

/* Ref-counted in-game data message object (64 bytes, 0x812FE122..0x812FE337) */
class NpImpsSvcInGameDataMessage : public paf::common::RefObj {
public:
	NpImpsSvcInGameDataMessage();
	virtual ~NpImpsSvcInGameDataMessage();

	static int Create(NpImpsSvcInGameDataMessage **outMsg, const SceNpId *senderNpId,
			  const SceNpCommunicationId *commId, const void *payload, SceSize payloadSize);

	const SceNpId *GetSenderNpId() const { return &m_senderNpId; }
	const SceNpCommunicationId *GetCommId() const { return &m_commId; }
	const void *GetPayload() const { return m_payload; }
	SceSize GetPayloadSize() const { return m_payloadSize; }

private:
	SceNpId m_senderNpId;			/* +0x08 */
	SceNpCommunicationId m_commId;		/* +0x2C */
	void *m_payload;			/* +0x38 */
	SceSize m_payloadSize;			/* +0x3C */
};

/* Ref-counted presence state object (136 bytes, 0x812FE338..0x812FF2E7) */
class NpImpsSvcPresenceData : public paf::common::RefObj {
public:
	NpImpsSvcPresenceData();
	virtual ~NpImpsSvcPresenceData();

	void ClearPartyPresence();
	void ClearGamePresenceBody();
	void ClearGamePresenceAll();
	void ClearJoinableCommId();
	void ClearContentId();
	void ClearTitleAndGamePresence();

	void SetOnlineStatus(int status) { m_onlineStatus = status; }
	void SetPlatform(int platform) { m_platform = platform; }
	void SetTitlePlatform(int platform) { m_titlePlatform = platform; }
	int SetComment(const char *comment);
	void SetLastOnlineTick(const paf::rtc::Tick *tick) { m_lastOnlineTick = *tick; }
	int SetTitleInfo(const SceNpCommunicationId *titleId, const char *titleIdStr,
			 const char *titleName, SceUInt8 isSystemTitle);
	int SetCommId(const SceNpCommunicationId *commId);
	int SetGamePresence(int presenceType, const char *status,
			    const void *gameData, SceSize gameDataSize);
	int SetGamePresenceFromInput(const NpImpsGamePresenceInput *input);
	int SetPartyPresence(int presenceType, const char *title, const char *status,
			     const void *partyData, SceSize partyDataSize);
	int SetPartyPresenceFromStruct(const NpImpsPartyPresence *party);
	void SetJoinableCommId(const SceNpCommunicationId *commId);
	int SetContentId(const char *contentId);

	int GetOnlineStatus() const { return m_onlineStatus; }
	int GetEffectiveOnlineStatus() const;
	int GetPlatform() const { return m_platform; }
	const char *GetComment() const { return m_comment; }
	int GetTitlePlatform() const { return m_titlePlatform; }
	const paf::rtc::Tick *GetLastOnlineTick() const { return &m_lastOnlineTick; }
	bool HasTitle() const { return m_hasTitle != 0; }
	const SceNpCommunicationId *GetTitleId() const { return &m_titleId; }
	const char *GetTitleIdStr() const { return m_titleIdStr; }
	const char *GetTitleName() const { return m_titleName; }
	SceUInt8 IsSystemTitle() const { return m_isSystemTitle; }
	const char *GetContentId() const { return m_contentId; }
	const SceNpCommunicationId *GetJoinableCommId() const { return &m_joinableCommId; }
	bool HasCommId() const { return m_hasCommId != 0; }
	const SceNpCommunicationId *GetCommId() const { return &m_commId; }
	bool HasGamePresence() const { return m_hasGamePresence != 0; }
	int GetGamePresenceType() const { return m_gamePresenceType; }
	const char *GetGameStatus() const { return m_gameStatus; }
	SceSize GetGameDataSize() const { return m_gameDataSize; }
	int CopyGameData(void *dst, SceSize maxLen) const;
	bool HasPartyPresence() const { return m_hasPartyPresence != 0; }
	int GetPartyPresenceType() const { return m_partyPresenceType; }
	const char *GetPartyTitle() const { return m_partyTitle; }
	const char *GetPartyStatus() const { return m_partyStatus; }
	SceSize GetPartyDataSize() const { return m_partyDataSize; }
	int CopyPartyData(void *dst, SceSize maxLen) const;

	int Compare(const NpImpsSvcPresenceData *other, NpImpsSvcPresenceDiffFlags *outDiff) const;
	int CopyFrom(const NpImpsSvcPresenceData *src);

	static int CreateFromCoreUpdate(NpImpsSvcPresenceData **outData,
					const NpImpsCorePresenceUpdate *update);
	static int Clone(NpImpsSvcPresenceData **outData, const NpImpsSvcPresenceData *src);

private:
	int m_onlineStatus;			/* +0x08 */
	int m_platform;				/* +0x0C */
	char *m_comment;			/* +0x10 */
	int m_titlePlatform;			/* +0x14 */
	paf::rtc::Tick m_lastOnlineTick;	/* +0x18 */
	SceUInt8 m_hasTitle;			/* +0x20 */
	SceNpCommunicationId m_titleId;		/* +0x21 */
	char *m_titleIdStr;			/* +0x30 */
	char *m_titleName;			/* +0x34 */
	SceUInt8 m_isSystemTitle;		/* +0x38 */
	char *m_contentId;			/* +0x3C */
	SceNpCommunicationId m_joinableCommId;	/* +0x40 */
	SceUInt8 m_hasCommId;			/* +0x4C */
	SceNpCommunicationId m_commId;		/* +0x4D */
	SceUInt8 m_hasGamePresence;		/* +0x59 */
	int m_gamePresenceType;			/* +0x5C */
	char *m_gameStatus;			/* +0x60 */
	void *m_gameData;			/* +0x64 */
	SceSize m_gameDataSize;			/* +0x68 */
	SceUInt8 m_hasPartyPresence;		/* +0x6C */
	int m_partyPresenceType;		/* +0x70 */
	char *m_partyTitle;			/* +0x74 */
	char *m_partyStatus;			/* +0x78 */
	void *m_partyData;			/* +0x7C */
	SceSize m_partyDataSize;		/* +0x80 */
};

/* Observer interface notified when local user presence changes */
class NpImpsSvcMyPresenceObserver {
public:
	virtual ~NpImpsSvcMyPresenceObserver() {}
	virtual void OnMyPresenceUpdated(NpImpsSvcPresenceData **curPresence,
					 NpImpsSvcPresenceData **prevPresence) = 0;
};

/* Local user presence manager (196 bytes, 0x812FC140..0x812FDAA9) */
class NpImpsSvcMyPresence {
public:
	NpImpsSvcMyPresence(NpImpsSvcMyPresenceObserver *observer);
	virtual ~NpImpsSvcMyPresence();

	void Lock() { m_mutex.Lock(); }
	void Unlock() { m_mutex.Unlock(); }

	int GetSnapshot(NpImpsSvcPresenceSnapshot *outSnapshot);
	int PublishCurrentPresence();
	void SetActiveProcess(SceUID processId, int appId);
	int SetGameTitle(int appId);
	int ClearGameTitle();
	int SetCommId(SceUID processId, const SceNpCommunicationId *commId);
	int ClearCommId();
	bool HasCommId() const;
	int RepublishWithoutCommId();
	int SetGamePresence(SceUID processId, const NpImpsGamePresenceInput *input);
	int ClearGamePresence();
	int SetPartyPresence(const NpImpsPartyPresence *party);
	int ClearPartyPresence();
	int GetAppTitleInfo(int appId, NpImpsAppTitleInfo *outInfo);
	int IsBigAppGame(int appId);

private:
	int BuildTitleNode(NpImpsCorePresenceUpdate *update);
	int BuildGamePresenceNodes(NpImpsCorePresenceUpdate *update, SceUInt8 prioritize);
	int BuildPartyPresenceNodes(NpImpsCorePresenceUpdate *update, SceUInt8 prioritize);
	int BuildCoreUpdate(NpImpsCorePresenceUpdate *update, int priorityMode);
	static void FreeCoreUpdate(NpImpsCorePresenceUpdate *update);
	void EnsureTitleLoaded();
	static int CheckAppStatusIsBigAppGame(const void *appStatus);
	void NotifyObserver(NpImpsSvcPresenceData *curPresence, NpImpsSvcPresenceData *prevPresence);

	paf::thread::RMutex m_mutex;		/* +0x08 */
	SceUID m_processId;			/* +0x28 */
	int m_appId;				/* +0x2C */
	SceUInt8 m_titleLoaded;			/* +0x30 */
	NpImpsSvcPresenceData m_presence;	/* +0x38 */
	NpImpsSvcMyPresenceObserver *m_observer;/* +0xC0 */
};

/* Service/online state trackers (0x812FDB20..0x812FDC93) */
class NpImpsSvcOnlineStatusTracker {
public:
	NpImpsSvcOnlineStatusTracker(paf::thread::RMutex *mutex) : m_mutex(mutex), m_status(1) {}
	virtual ~NpImpsSvcOnlineStatusTracker() {}

	void Lock() { m_mutex->Lock(); }
	void Unlock() { m_mutex->Unlock(); }
	int GetStatus();
	void OnImpsCoreState(int coreState, SceUInt8 *outChanged);

private:
	paf::thread::RMutex *m_mutex;
	int m_status;
};

class NpImpsSvcSessionStateTracker {
public:
	NpImpsSvcSessionStateTracker(paf::thread::RMutex *mutex) : m_mutex(mutex), m_level(0) {}
	virtual ~NpImpsSvcSessionStateTracker() {}

	void Lock() { m_mutex->Lock(); }
	void Unlock() { m_mutex->Unlock(); }
	bool IsSignedIn();
	bool IsOnline();
	void SetSignedIn();
	void SetSignedOut();
	void OnImpsCoreState(int coreState, SceUInt8 *outChanged);

private:
	paf::thread::RMutex *m_mutex;
	int m_level;
};

/* Base class for async roster mutation contexts (108 bytes, 0x812FF338..0x812FF527) */
class NpImpsSvcContext : public paf::common::RefObj {
public:
	NpImpsSvcContext(SceUInt32 contextId);
	virtual ~NpImpsSvcContext();

	virtual void NotifyComplete(int result) = 0;
	virtual int Execute() = 0;

	int RunIfNotAborted();
	int Abort();
	void WaitComplete();
	void SignalComplete();

	SceUInt32 GetContextId() const { return m_contextId; }
	bool IsAborted() const { return m_aborted != 0; }

protected:
	paf::thread::RMutex m_rmutex;		/* +0x08 */
	int m_rosterReqId;			/* +0x28 */
	SceUInt32 m_contextId;			/* +0x2C */
	SceUInt8 m_aborted;			/* +0x30 */
	paf::thread::Mutex m_mutexToWait;	/* +0x38 */
	paf::thread::Cond m_condToWait;		/* +0x58 */
};

/* Context factory interface (0x812FDC94..0x812FDE93) */
class NpImpsSvcContextFactory {
public:
	virtual ~NpImpsSvcContextFactory() {}
	virtual int CreateRequestFriend(NpImpsSvcContext **outCtx, SceUInt32 ctxId,
					const SceNpId *npId, const char *comment,
					NpImpsSvcRosterCallback callback);
	virtual int CreateDeleteFriend(NpImpsSvcContext **outCtx, SceUInt32 ctxId,
				       const SceNpId *npId, NpImpsSvcRosterCallback callback);
	virtual int CreateAcceptFriend(NpImpsSvcContext **outCtx, SceUInt32 ctxId,
				       const SceNpId *npId, NpImpsSvcRosterCallback callback);
	virtual int CreateAddBlock(NpImpsSvcContext **outCtx, SceUInt32 ctxId,
				   const SceNpId *npId, NpImpsSvcRosterCallback callback);
	virtual int CreateDeleteBlock(NpImpsSvcContext **outCtx, SceUInt32 ctxId,
				      const SceNpId *npId, NpImpsSvcRosterCallback callback);
};

/* Async context queue & manager thread ("SceNpImpsSvcCtxMgr", 0x812FF528..0x812FFB3B) */
class NpImpsSvcContextQueue {
public:
	NpImpsSvcContextQueue();
	virtual ~NpImpsSvcContextQueue();

	int Enqueue(NpImpsSvcContext *ctx);
	void ProcessAll();
	int AbortContext(SceUInt32 contextId);
	void WaitContext(SceUInt32 contextId);

private:
	struct QueueNode {
		QueueNode *prev;
		QueueNode *next;
		NpImpsSvcContext *ctx;
	};

	SceUInt32 m_capacity;
	paf::thread::RMutex m_mutex;
	QueueNode *m_head;
	QueueNode *m_tail;
	SceUInt32 m_count;
};

class NpImpsSvcContextManager : public paf::thread::Thread {
public:
	NpImpsSvcContextManager(const NpImpsSvcInitParam *param);
	virtual ~NpImpsSvcContextManager();
	virtual void EntryFunction();

	int StartManager();
	int StopManager();
	int CreateContextId(SceUInt32 *outContextId);
	int DestroyContextId(SceUInt32 contextId);
	int AbortContext(SceUInt32 contextId);
	int WaitContext(SceUInt32 contextId);
	int EnqueueContext(NpImpsSvcContext *ctx);

private:
	bool HasContextIdLocked(SceUInt32 contextId) const;
	bool InsertContextIdLocked(SceUInt32 contextId);
	void EraseContextIdLocked(SceUInt32 contextId);

	paf::thread::Mutex m_waitMutex;
	paf::thread::Cond m_waitCond;
	NpImpsSvcContextQueue m_queue;
	SceUInt32 m_nextContextId;
	paf::thread::RMutex m_setMutex;
	SceUInt32 m_activeContextIds[16];
	SceUInt32 m_activeCount;
};

/* Interface for async roster operations */
class NpImpsSvcAsyncInterface {
public:
	virtual ~NpImpsSvcAsyncInterface() {}
	virtual int CreateContext(SceUInt32 *outContextId) = 0;
	virtual int DestroyContext(SceUInt32 contextId) = 0;
	virtual int AbortContext(SceUInt32 contextId) = 0;
	virtual int WaitContext(SceUInt32 contextId) = 0;
	virtual int RequestFriend(SceUInt32 contextId, const SceNpId *npId,
				  const char *comment, NpImpsSvcRosterCallback callback) = 0;
	virtual int AcceptFriend(SceUInt32 contextId, const SceNpId *npId,
				 NpImpsSvcRosterCallback callback) = 0;
	virtual int DeleteFriend(SceUInt32 contextId, const SceNpId *npId,
				 NpImpsSvcRosterCallback callback) = 0;
	virtual int AddBlock(SceUInt32 contextId, const SceNpId *npId,
			     NpImpsSvcRosterCallback callback) = 0;
	virtual int DeleteBlock(SceUInt32 contextId, const SceNpId *npId,
				NpImpsSvcRosterCallback callback) = 0;
	virtual int ForceRosterSync(int syncFlags) = 0;
};

/* Cache observer interface (SceNpImpsCache -> SceNpImpsSvc) */
class NpImpsCacheObserver {
public:
	virtual ~NpImpsCacheObserver() {}
	virtual void OnAccountSessionEvent(int eventType) = 0;
	virtual void OnImpsCoreStateEvent(int coreState, int errorCode) = 0;
	virtual void OnFriendListEvent(int eventType, SceSize count, NpImpsCacheFriendPtr *handles) = 0;
	virtual void OnFriendRemovedEvent(SceSize count, const SceNpId *npIds) = 0;
	virtual void OnFriendRequestRemovedEvent(SceSize count, const SceNpId *npIds) = 0;
	virtual void OnFriendContextEvent(NpImpsCacheFriendPtr *handle, int contextState, void *extra) = 0;
	virtual void OnFriendPresenceEvent(NpImpsCacheFriendPtr *handle,
					   NpImpsSvcPresenceData **curPresence,
					   NpImpsSvcPresenceData **prevPresence,
					   const NpImpsSvcPresenceDiffFlags *diffFlags) = 0;
	virtual void OnPlatformFriendPresenceEvent(NpImpsCacheFriendPtr *handle,
						   NpImpsSvcPresenceData **curPresence,
						   NpImpsSvcPresenceData **prevPresence,
						   const NpImpsSvcPresenceDiffFlags *diffFlags,
						   int platform) = 0;
	virtual void OnBlockListEvent(int eventType, SceSize count, const SceNpId *npIds) = 0;
	virtual void OnInGameDataMessageEvent(NpImpsSvcInGameDataMessage **msg) = 0;
};

/* Main SceNpImpsSvc class (0x812F899C..0x812FC13F) */
class NpImpsSvc : public NpImpsCacheObserver,
		  public NpImpsSvcAsyncInterface,
		  public NpImpsSvcMyPresenceObserver {
public:
	NpImpsSvc(const NpImpsSvcInitParam *param, paf::thread::RMutex *cacheStateMutex);
	virtual ~NpImpsSvc();

	static int Init(void *heapMem, SceSize heapSize, const NpImpsSvcInitParam *param);
	static int Term();
	static int GetInstance(NpImpsSvc **outInstance);

	int RegisterListener(const NpImpsSvcListenerCallbacks *callbacks, SceUInt32 *outListenerId);
	int UnregisterListener(SceUInt32 listenerId);
	int IsOnline(bool *outIsOnline);
	int GetServiceState(int *outState);
	int GetMyOnlineStatus(int *outStatus);
	int GetMyPresence(NpImpsSvcPresenceSnapshot *outSnapshot);

	int LockServiceState();
	int UnlockServiceState();
	int LockOnlineStatus();
	int UnlockOnlineStatus();
	int LockMyPresence();
	int UnlockMyPresence();
	int LockFriendStore();
	int UnlockFriendStore();
	int LockBlockStore();
	int UnlockBlockStore();

	int SetActiveProcess(SceUID processId, int appId);
	int SetGameTitle(int appId);
	int ClearGameTitle();
	int SetCommId(SceUID processId, const SceNpCommunicationId *commId);
	int ClearCommId();
	int HasCommId(bool *outHasCommId);
	int PublishMyPresence();
	int RepublishMyPresenceWithoutCommId();
	int SetGamePresence(SceUID processId, const NpImpsGamePresenceInput *input);
	int ClearGamePresence();
	int SetPartyPresence(const NpImpsPartyPresence *party);
	int ClearPartyPresence();
	int GetAppTitleInfo(int appId, NpImpsAppTitleInfo *outInfo);
	int IsBigAppGame(int appId);

	int GetBlockListCount(SceUInt32 *outCount, SceUInt8 *outLoaded);
	int GetBlockListEntries(SceUInt32 startIndex, SceNpId *outEntries, SceUInt32 maxCount,
				SceUInt32 *outCount, SceUInt8 *outLoaded);
	int IsBlocked(const SceNpId *npId, bool *outBlocked, SceUInt8 *outLoaded);

	int GetFriendCount(SceUInt32 *outCount);
	int GetFriendEntries(SceUInt32 startIndex, SceNpId *outEntries, SceUInt32 maxCount,
			     SceUInt32 *outCount);
	int GetFriendEntryNpId(NpImpsCacheFriendPtr *handle, SceNpId *outNpId);
	int GetFriendEntryRelation(NpImpsCacheFriendPtr *handle, int *outRelation);
	int GetFriendEntryDate(NpImpsCacheFriendPtr *handle, paf::rtc::Tick *outTick);
	int GetFriendEntryRequestDirection(NpImpsCacheFriendPtr *handle, int *outDirection);
	int GetFriendEntryIsNew(NpImpsCacheFriendPtr *handle, SceUInt8 *outIsNew);
	int SnapshotFriendHandles(NpImpsCacheFriendPtr ***outHandles, SceUInt32 *outCount,
				  void *(*allocFn)(SceSize));
	int ReleaseFriendHandles(NpImpsCacheFriendPtr **handles, SceUInt32 count,
				 void (*freeFn)(void *));

	int GetAcceptedFriendCount(SceUInt32 *outCount, SceUInt8 *outLoaded);
	int GetAcceptedFriendEntries(SceUInt32 startIndex, SceNpId *outEntries, SceUInt32 maxCount,
				     SceUInt32 *outCount, SceUInt8 *outLoaded);
	int GetPendingFriendCount(SceUInt32 *outCount, SceUInt8 *outLoaded);
	int GetPendingFriendEntries(SceUInt32 startIndex, SceNpId *outEntries, SceUInt32 maxCount,
				    SceUInt32 *outCount, SceUInt8 *outLoaded);
	int GetRequestedFriendCount(SceUInt32 *outCount, SceUInt8 *outLoaded);
	int GetRequestedFriendEntries(SceUInt32 startIndex, SceNpId *outEntries, SceUInt32 maxCount,
				      SceUInt32 *outCount, SceUInt8 *outLoaded);

	int GetFriendPresenceByNpId(const SceNpId *npId, NpImpsSvcPresenceSnapshot *outSnapshot);
	int GetFriendPresenceByHandle(NpImpsCacheFriendPtr *handle,
				      NpImpsSvcPresenceSnapshot *outSnapshot, int platform);
	int GetFriendContextByNpId(const SceNpId *npId, int *outContext);
	int GetFriendGamePresence(const SceNpId *npId, NpImpsGamePresenceWithTitle *outPresence);

	/* NpImpsCacheObserver overrides */
	virtual void OnAccountSessionEvent(int eventType);
	virtual void OnImpsCoreStateEvent(int coreState, int errorCode);
	virtual void OnFriendListEvent(int eventType, SceSize count, NpImpsCacheFriendPtr *handles);
	virtual void OnFriendRemovedEvent(SceSize count, const SceNpId *npIds);
	virtual void OnFriendRequestRemovedEvent(SceSize count, const SceNpId *npIds);
	virtual void OnFriendContextEvent(NpImpsCacheFriendPtr *handle, int contextState, void *extra);
	virtual void OnFriendPresenceEvent(NpImpsCacheFriendPtr *handle,
					   NpImpsSvcPresenceData **curPresence,
					   NpImpsSvcPresenceData **prevPresence,
					   const NpImpsSvcPresenceDiffFlags *diffFlags);
	virtual void OnPlatformFriendPresenceEvent(NpImpsCacheFriendPtr *handle,
						   NpImpsSvcPresenceData **curPresence,
						   NpImpsSvcPresenceData **prevPresence,
						   const NpImpsSvcPresenceDiffFlags *diffFlags,
						   int platform);
	virtual void OnBlockListEvent(int eventType, SceSize count, const SceNpId *npIds);
	virtual void OnInGameDataMessageEvent(NpImpsSvcInGameDataMessage **msg);

	/* NpImpsSvcAsyncInterface overrides */
	virtual int CreateContext(SceUInt32 *outContextId);
	virtual int DestroyContext(SceUInt32 contextId);
	virtual int AbortContext(SceUInt32 contextId);
	virtual int WaitContext(SceUInt32 contextId);
	virtual int RequestFriend(SceUInt32 contextId, const SceNpId *npId,
				  const char *comment, NpImpsSvcRosterCallback callback);
	virtual int AcceptFriend(SceUInt32 contextId, const SceNpId *npId,
				 NpImpsSvcRosterCallback callback);
	virtual int DeleteFriend(SceUInt32 contextId, const SceNpId *npId,
				 NpImpsSvcRosterCallback callback);
	virtual int AddBlock(SceUInt32 contextId, const SceNpId *npId,
			     NpImpsSvcRosterCallback callback);
	virtual int DeleteBlock(SceUInt32 contextId, const SceNpId *npId,
				NpImpsSvcRosterCallback callback);
	virtual int ForceRosterSync(int syncFlags);

	/* NpImpsSvcMyPresenceObserver override */
	virtual void OnMyPresenceUpdated(NpImpsSvcPresenceData **curPresence,
					 NpImpsSvcPresenceData **prevPresence);

	static void FillPresenceSnapshotFromData(NpImpsSvcPresenceSnapshot *outSnapshot,
						 const NpImpsSvcPresenceData *data);
	static void FillPresenceSnapshotFromFriend(NpImpsSvcPresenceSnapshot *outSnapshot,
						   NpImpsCacheFriendEntry *friendEntry,
						   int platform);
	static void BuildDiffMask(SceUInt32 *outMask, const NpImpsSvcPresenceDiffFlags *diffFlags);

private:
	struct ListenerNode {
		ListenerNode *next;
		SceUInt32 id;
		NpImpsSvcListenerCallbacks callbacks;
	};

	int ReplayInitialPlatformPresence(const NpImpsSvcListenerCallbacks *cb);
	int ReplayInitialFriendPresence(const NpImpsSvcListenerCallbacks *cb);
	int ReplayInitialFriendContext(const NpImpsSvcListenerCallbacks *cb);
	int ReplayInitialFriendList(const NpImpsSvcListenerCallbacks *cb);

	void DispatchServiceState(int state, int errorCode);
	void DispatchMyOnlineStatus(int status);
	void DispatchOnlineTransition(int isOnline);
	void DispatchFriendList(int eventType, NpImpsCacheFriendPtr *handles, SceSize count);
	void DispatchFriendRemoved(const SceNpId *npIds, SceSize count);
	void DispatchFriendRequestRemoved(const SceNpId *npIds, SceSize count);
	void DispatchFriendContext(int eventType, NpImpsCacheFriendPtr *handle,
				   int contextState, void *extra);
	void DispatchFriendPresence(int eventType, NpImpsCacheFriendPtr *handle,
				    NpImpsSvcPresenceData *curPresence,
				    NpImpsSvcPresenceData *prevPresence, SceUInt32 diffMask);
	void DispatchPlatformFriendPresence(int eventType, NpImpsCacheFriendPtr *handle,
					    NpImpsSvcPresenceData *curPresence,
					    NpImpsSvcPresenceData *prevPresence,
					    SceUInt32 diffMask, int platform);
	void DispatchBlockList(int eventType, const SceNpId *npIds, SceSize count);
	void DispatchInGameDataMessage(int eventType, const SceNpId *senderNpId,
				       const NpImpsSvcInGameDataPayload *payload);
	void DispatchMyPresence(int eventType, NpImpsSvcPresenceData *curPresence,
				NpImpsSvcPresenceData *prevPresence);

	NpImpsCacheCore *m_cacheCore;
	paf::thread::RMutex m_listenerMutex;
	ListenerNode *m_listenersHead;
	SceUInt32 m_listenerCount;
	SceUInt32 m_nextListenerId;
	NpImpsSvcSessionStateTracker m_sessionState;
	NpImpsSvcOnlineStatusTracker m_onlineStatus;
	NpImpsSvcContextManager m_ctxMgr;
	NpImpsSvcContextFactory m_ctxFactory;
	NpImpsSvcMyPresence m_myPresence;
};

/* Heap helpers for SceNpImpsSvc (0x812FF2E8..0x812FF337) & SceNpImpsCache (0x812FFE4C..0x812FFE9D) */
extern void npImpsSvcHeapSet(paf::memory::HeapAllocator *heap);
extern void npImpsSvcHeapClear(void);
extern void *npImpsSvcMalloc(SceSize size);
extern void npImpsSvcFree(void *ptr);

extern void npImpsCacheHeapSet(paf::memory::HeapAllocator *heap);
extern void npImpsCacheHeapClear(void);
extern void *npImpsCacheMalloc(SceSize size);
extern void npImpsCacheFree(void *ptr);

/* SceNpImpsCache bridges (implemented in np_imps_cache) */
class NpImpsCacheFriendEntry : public paf::common::RefObj {
public:
	virtual ~NpImpsCacheFriendEntry() {}
	virtual void UpdateRosterData(int relation, const paf::rtc::Tick *date) = 0;	/* +8 */
	virtual int UpdatePresenceDefault(NpImpsSvcPresenceData **newPresence,
					  NpImpsSvcPresenceDiffFlags *outDiff,
					  NpImpsSvcPresenceData **outPrev) = 0;		/* +12 */
	virtual int UpdatePresenceForPlatform(NpImpsSvcPresenceData **newPresence,
					      NpImpsSvcPresenceDiffFlags *outDiff,
					      NpImpsSvcPresenceData **outPrev,
					      int platform) = 0;			/* +16 */
	virtual void GetNpId(SceNpId *outNpId) = 0;					/* +28 */
	virtual int GetRelation() = 0;							/* +32 */
	virtual void GetDate(paf::rtc::Tick *outTick) = 0;				/* +36 */
	virtual int GetRequestDirection() = 0;						/* +40 */
	virtual SceUInt8 GetIsNew() = 0;						/* +44 */
	virtual void SetIsNew(SceUInt8 isNew) = 0;					/* +48 */
	virtual int GetFriendContext() = 0;						/* +52 */
	virtual int GetOnlineStatus(int platform) = 0;					/* +56 */
	virtual int GetPlatform(int platform) = 0;					/* +60 */
	virtual int CopyComment(char **outBuf, void *(*allocFn)(SceSize),
				int platform) = 0;					/* +64 */
	virtual int GetTitlePlatform(int platform) = 0;					/* +68 */
	virtual void GetLastOnlineTick(paf::rtc::Tick *outTick, int platform) = 0;	/* +72 */
	virtual void GetTitleId(SceNpCommunicationId *outId, int platform) = 0;		/* +76 */
	virtual void GetTitleIdStr(char *outBuf, int platform) = 0;			/* +80 */
	virtual void GetTitleName(char *outBuf, int platform) = 0;			/* +84 */
	virtual SceUInt8 IsSystemTitle(int platform) = 0;				/* +88 */
	virtual void GetJoinableCommId(SceNpCommunicationId *outId, int platform) = 0;	/* +92 */
	virtual void GetContentId(char *outBuf, int platform) = 0;			/* +96 */
	virtual int GetGamePresenceType(int platform) = 0;				/* +100 */
	virtual void GetCommId(SceNpCommunicationId *outId, int platform) = 0;		/* +104 */
	virtual void GetGameStatus(char *outBuf, int platform) = 0;			/* +108 */
	virtual SceSize GetGameDataSize(int platform) = 0;				/* +112 */
	virtual int CopyGameData(void *dst, SceSize maxLen, int platform) = 0;		/* +116 */
	virtual int GetPartyPresenceType(int platform) = 0;				/* +120 */
	virtual void GetPartyTitle(char *outBuf, int platform) = 0;			/* +124 */
	virtual void GetPartyStatus(char *outBuf, int platform) = 0;			/* +128 */
	virtual SceSize GetPartyDataSize(int platform) = 0;				/* +132 */
	virtual int CopyPartyData(void *dst, SceSize maxLen, int platform) = 0;		/* +136 */
	virtual int FreeAllocatedString(void *ptr, void (*freeFn)(void *)) = 0;		/* +140 */
};

class NpImpsCacheFriendStore {
public:
	virtual ~NpImpsCacheFriendStore() {}
	virtual void Lock() = 0;							/* +8 */
	virtual void Unlock() = 0;							/* +12 */
	virtual SceUInt32 GetCount() = 0;						/* +16 */
	virtual int CopyHandles(NpImpsCacheFriendPtr *outHandles, SceUInt32 count) = 0;	/* +20 */
	virtual SceUInt32 GetAcceptedCount(SceUInt8 *outLoaded) = 0;			/* +24 */
	virtual void CopyAcceptedNpIds(SceUInt32 startIndex, SceNpId *outEntries,
				       SceUInt32 maxCount, SceUInt32 *outCount,
				       SceUInt8 *outLoaded) = 0;			/* +28 */
	virtual SceUInt32 GetPendingCount(SceUInt8 *outLoaded) = 0;			/* +32 */
	virtual void CopyPendingNpIds(SceUInt32 startIndex, SceNpId *outEntries,
				      SceUInt32 maxCount, SceUInt32 *outCount,
				      SceUInt8 *outLoaded) = 0;				/* +36 */
	virtual SceUInt32 GetRequestedCount(SceUInt8 *outLoaded) = 0;			/* +40 */
	virtual void CopyRequestedNpIds(SceUInt32 startIndex, SceNpId *outEntries,
					SceUInt32 maxCount, SceUInt32 *outCount,
					SceUInt8 *outLoaded) = 0;			/* +44 */
	virtual int CopyAllNpIds(SceUInt32 startIndex, SceNpId *outEntries,
				 SceUInt32 maxCount, SceUInt32 *outCount) = 0;		/* +48 */
	virtual int GetRelation(const SceNpId *npId, int *outRelation) = 0;		/* +52 */
	virtual int GetDate(const SceNpId *npId, paf::rtc::Tick *outTick) = 0;		/* +56 */
	virtual int GetFriendContext(const SceNpId *npId, int *outContext) = 0;		/* +60 */
	virtual int GetOnlineStatus(const SceNpId *npId, int *outStatus) = 0;		/* +64 */
	virtual int GetTitlePlatform(const SceNpId *npId, int *outPlatform) = 0;	/* +68 */
	virtual int GetLastOnlineTick(const SceNpId *npId, paf::rtc::Tick *outTick,
				      bool *outHasTick) = 0;				/* +72 */
	virtual int GetTitleId(const SceNpId *npId, SceNpCommunicationId *outId) = 0;	/* +76 */
	virtual int GetTitleName(const SceNpId *npId, char *outTitleName) = 0;		/* +80 */
	virtual int IsSystemTitle(const SceNpId *npId, SceUInt8 *outIsSystem) = 0;	/* +84 */
	virtual int GetContentId(const SceNpId *npId, char *outContentId) = 0;		/* +88 */
	virtual int GetGamePresenceType(const SceNpId *npId, int *outType) = 0;		/* +92 */
	virtual int GetCommId(const SceNpId *npId, SceNpCommunicationId *outId) = 0;	/* +96 */
	virtual int GetGameStatus(const SceNpId *npId, char *outStatus) = 0;		/* +100 */
	virtual int GetGameData(const SceNpId *npId, void *outData) = 0;		/* +104 */
	virtual int GetGameDataSize(const SceNpId *npId, SceSize *outSize) = 0;		/* +108 */
	virtual int GetPartyPresenceType(const SceNpId *npId, int *outType) = 0;	/* +112 */
	virtual int GetPartyTitle(const SceNpId *npId, char *outTitle) = 0;		/* +116 */
	virtual int GetPartyStatus(const SceNpId *npId, char *outStatus) = 0;		/* +120 */
	virtual int GetPartyData(const SceNpId *npId, void *outData) = 0;		/* +124 */
	virtual int GetPartyDataSize(const SceNpId *npId, SceSize *outSize) = 0;	/* +128 */
	virtual bool Contains(const SceNpId *npId) = 0;					/* +132 */
	virtual int GetTitleIdForPlatform(const SceNpId *npId, SceNpCommunicationId *outId,
					  int platform) = 0;				/* +136 */
	virtual int GetTitleNameForPlatform(const SceNpId *npId, char *outTitleName,
					    int platform) = 0;				/* +140 */
	virtual int GetGamePresenceTypeForPlatform(const SceNpId *npId, int *outType,
						   int platform) = 0;			/* +144 */
	virtual int GetCommIdForPlatform(const SceNpId *npId, SceNpCommunicationId *outId,
					 int platform) = 0;				/* +148 */
	virtual int GetGameStatusForPlatform(const SceNpId *npId, char *outStatus,
					     int platform) = 0;				/* +152 */
	virtual int GetGameDataForPlatform(const SceNpId *npId, void *outData,
					   int platform) = 0;				/* +156 */
	virtual int GetGameDataSizeForPlatform(const SceNpId *npId, SceSize *outSize,
					       int platform) = 0;			/* +160 */
	virtual int GetNpIdAt(SceUInt32 index, SceNpId *outNpId) = 0;			/* +164 */
};

class NpImpsCacheBlockStore {
public:
	virtual ~NpImpsCacheBlockStore() {}
	virtual void Lock() = 0;							/* +8 */
	virtual void Unlock() = 0;							/* +12 */
	virtual SceUInt32 GetCount(SceUInt8 *outLoaded) = 0;				/* +16 */
	virtual void CopyEntries(SceUInt32 startIndex, SceNpId *outEntries,
				 SceUInt32 maxCount, SceUInt32 *outCount,
				 SceUInt8 *outLoaded) = 0;				/* +20 */
	virtual bool Contains(const SceNpId *npId, SceUInt8 *outLoaded) = 0;		/* +24 */
};

class NpImpsCacheRosterSync {
public:
	virtual ~NpImpsCacheRosterSync() {}
	virtual int TriggerRosterUpdate(const SceNpId *npId, int syncReason) = 0;	/* +8 */
	virtual int ForceFullSync(int syncFlags) = 0;					/* +12 */
};

class NpImpsCacheCore {
public:
	virtual ~NpImpsCacheCore() {}
	virtual int RegisterObserver(NpImpsCacheObserver *observer) = 0;	/* +8 */
	virtual int UnregisterObserver() = 0;					/* +12 */
	virtual NpImpsCacheFriendStore *GetFriendStore() = 0;			/* +16 */
	virtual NpImpsCacheBlockStore *GetBlockStore() = 0;			/* +20 */
	virtual NpImpsCacheRosterSync *GetRosterSync() = 0;			/* +24 */
	virtual const SceNpCommunicationId *GetPartyTitleId() = 0;		/* +28 */
	virtual paf::thread::RMutex *GetStateMutex() = 0;			/* +32 */
	virtual void Lock() = 0;						/* +36 */
	virtual void Unlock() = 0;						/* +40 */
};

/* External bridges implemented in SceNpImpsCache / SceNpRoster / SceNpImpsCore / SceNpParty */
extern int npImpsCacheInit(int threadPriority, SceSize stackSize,
			   paf::memory::HeapAllocator *heap);		/* 0x812FFF56 */
extern int npImpsCacheTerm(void);					/* 0x813000FC */
extern NpImpsCacheCore *npImpsCacheGetCore(void);			/* 0x812FE0BC */

extern int npRosterCreateRequest(void);					/* 0x81308442 */
extern int npRosterDestroyRequest(int reqId);				/* 0x81308460 */
extern int npRosterAbortRequest(int reqId);				/* 0x8130847E */
extern int npRosterRequestFriend(int reqId, const SceNpId *npId, const char *comment);	/* 0x8130849C */
extern int npRosterAcceptFriend(int reqId, const SceNpId *npId);	/* 0x813085AE */
extern int npRosterDeleteFriend(int reqId, const SceNpId *npId);	/* 0x813086B2 */
struct NpRosterBlockListEntry {
	SceNpId npId;
	int action;	/* 0 = add block, 1 = delete block */
};
extern int npRosterUpdateBlockList(int reqId, const NpRosterBlockListEntry *entries,
				   SceSize count);			/* 0x813088D6 */

extern int npImpsCoreUpdatePresence(const NpImpsCorePresenceUpdate *update);	/* 0x8131B69A */
extern int npPartyGetMemberGamePresence(const SceNpId *npId,
					NpImpsGamePresenceWithTitle *outPresence); /* 0x81385C72 */

/* SceNpImpsSvc C facade (implemented in np_imps_util_server.cpp, 0x812F7FD0..0x812F899B) */
extern int npImpsSvcInit(void *heapMem, SceSize heapSize, const NpImpsSvcInitParam *param);
extern int npImpsSvcTerm(void);
extern int npImpsSvcGetServiceState(int *outState);
extern int npImpsSvcGetFriendPresenceByNpId(const SceNpId *npId,
					    NpImpsSvcPresenceSnapshot *outSnapshot);
extern int npImpsSvcGetAppTitleInfo(SceUID appId, NpImpsAppTitleInfo *outInfo);

/* Platform check helper (0x812FE0D0) */
extern int npImpsSvcParsePlatformTag(const void *record);

#endif
