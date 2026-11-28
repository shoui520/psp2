/*
 * nptrophy_service.h: SceNpTrophy core service, refcounted client resource pools,
 * TRP archive & conf source, storage arbiter, user session, sync & recovery
 * controllers, and core API entrypoints (0x812C16B4..0x812D00BF).
 */
#ifndef SHELL_NP_TROPHY_SERVICE_H
#define SHELL_NP_TROPHY_SERVICE_H

#include <stdint.h>
#include <scetypes.h>
#include <appmgr.h>
#include <ipmi/session.h>
#include "nptrophy_runtime.h"
#include "nptrophy_io.h"
#include "nptrophy_sfm.h"
#include "nptrophy_storage.h"
#include "nptrophy_net.h"

extern "C" {
int sceAppMgrGetStatusById(SceUID pid, void *statusBuf);
int sceAppMgrTrophyMount(int mountId, SceUID pid, char *mountPoint);
int sceAppMgrTrophyMountById(int mountId, const char *commIdStr, char *mountPoint);
int sceAppMgrUmount(const char *mountPoint);
uint32_t sceNetNtohl(uint32_t netlong);
uint64_t sceNetNtohll(uint64_t netllong);
}

/* Additional SceNpTrophy service, TRP, and arbiter error codes */
#define NP_TROPHY_ERR_ALREADY_REGISTERED	-2141907450	/* 0x80551606 */
#define NP_TROPHY_ERR_INVALID_HANDLE		-2141907447	/* 0x80551609 */
#define NP_TROPHY_ERR_INVALID_COMM_SIGN_FORMAT	-2141907445	/* 0x8055160B */
#define NP_TROPHY_ERR_INVALID_COMM_SIGN		-2141907444	/* 0x8055160C */
#define NP_TROPHY_ERR_INVALID_CONTEXT		-2141907438	/* 0x80551612 */
#define NP_TROPHY_ERR_NOT_REGISTERED		-2141907437	/* 0x80551613 */
#define NP_TROPHY_ERR_TRP_NOT_FOUND		-2141907433	/* 0x80551617 */
#define NP_TROPHY_ERR_TRP_HASH_MISMATCH		-2141907431	/* 0x80551619 */
#define NP_TROPHY_ERR_TRP_INVALID_HEADER	-2141907430	/* 0x8055161A */
#define NP_TROPHY_ERR_TRP_UNSUPPORTED_VERSION	-2141907429	/* 0x8055161B */
#define NP_TROPHY_ERR_TRP_NOT_OPEN		-2141907304	/* 0x80551698 */
#define NP_TROPHY_ERR_TRP_STREAM_BUSY		-2141907303	/* 0x80551699 */
#define NP_TROPHY_ERR_TRP_ALREADY_OPEN		-2141907302	/* 0x8055169A */
#define NP_TROPHY_ERR_TRP_ENTRY_NOT_FOUND	-2141907301	/* 0x8055169B */
#define NP_TROPHY_ERR_INVALID_TITLE_SESSION	-2141907298	/* 0x8055169E */
#define NP_TROPHY_ERR_INVALID_STREAM		-2141907297	/* 0x8055169F */
#define NP_TROPHY_ERR_ARBITER_CLIENT_LIMIT	-2141907296	/* 0x805516A0 */
#define NP_TROPHY_ERR_ARBITER_PREEMPT_LOW	-2141907294	/* 0x805516A2 */
#define NP_TROPHY_ERR_ARBITER_PREEMPT_MID	-2141907293	/* 0x805516A3 */
#define NP_TROPHY_ERR_ARBITER_PREEMPT_HIGH	-2141907292	/* 0x805516A4 */
#define NP_TROPHY_ERR_ARBITER_NOT_FOUND		-2141907291	/* 0x805516A5 */
#define NP_TROPHY_ERR_ARBITER_PATH_CONFLICT	-2141907290	/* 0x805516A6 */
#define NP_TROPHY_ERR_CONTEXT_LIMIT		-2141907286	/* 0x805516AA */
#define NP_TROPHY_ERR_HANDLE_LIMIT		-2141907285	/* 0x805516AB */
#define NP_TROPHY_ERR_DEBUG_PARAM_NOT_FOUND	-2141907272	/* 0x805516B8 */
#define NP_TROPHY_ERR_CLIENT_NOT_FOUND		-2141907269	/* 0x805516BB */
#define NP_TROPHY_ERR_CLIENT_LIMIT		-2141907268	/* 0x805516BC */
#define NP_TROPHY_ERR_DATA_DIR_MISSING		-2141907251	/* 0x805516CD */
#define NP_TROPHY_ERR_USER_SESSION_NOT_OPEN	-2141907246	/* 0x805516D2 */

/* AppMgr mount IDs for trophy partitions */
#define TROPHY_MOUNT_ID_GAME_TRP		300
#define TROPHY_MOUNT_ID_USER_ROOT		302
#define TROPHY_MOUNT_ID_TITLE_DATA		303
#define TROPHY_MOUNT_ID_TITLE_BACKUP		304

/* Refcounted object ID bases */
#define TROPHY_ID_BASE_HANDLE			0x00020000
#define TROPHY_ID_BASE_CONTEXT			0x00040000
#define TROPHY_ID_BASE_TITLE_SESSION		0x00060000
#define TROPHY_ID_BASE_TITLE_STORAGE		0x00080000
#define TROPHY_ID_BASE_CONF_SOURCE		0x000A0000
#define TROPHY_ID_BASE_STREAM			0x000C0000
#define TROPHY_ID_SPAN				0x00020000

/* =========================================================================
 * CommSign verification & AppMgr mount helper (0x812CC4CC, 0x812CC774..0x812CC815)
 * ========================================================================= */
int npTrophyVerifyCommSign(const TrophyCommId *commId, const void *commSignBuf);
int npTrophyResolvePidTitleId(SceUID pid, char *outTitleId);

class TrophyMount : public TrophyHeapObject {
public:
	TrophyMount();
	virtual ~TrophyMount();

	int MountForPid(SceUID pid);
	void Unmount();
	bool IsMounted() const { return m_mounted != 0; }
	const char *GetMountPoint() const;

private:
	uint8_t m_mounted;
	char m_mountPoint[16];
};

/* =========================================================================
 * TROPHY.TRP archive & TRP conf source (0x812CB694..0x812CC4C8, 0x812CC816..0x812CDB8B)
 * ========================================================================= */
struct TrophyTrpHeader {
	uint32_t magic;
	uint32_t version;
	uint64_t fileSize;
	uint32_t entryCount;
	uint32_t entrySize;
	uint32_t flags;
	TrophySha1Digest sha1;
};

struct TrophyTrpEntry {
	char name[32];
	uint64_t offset;
	uint64_t size;
	uint32_t entryKind;
};

class TrophyTrpArchive;

class TrophyTrpFileStream : public TrophyFileBase {
public:
	TrophyTrpFileStream();
	virtual ~TrophyTrpFileStream();

	int OpenEntry(TrophyTrpArchive *archive, const TrophyTrpEntry *entry);
	int OpenByName(TrophyHandle *handle, TrophyTrpArchive *archive, const char *name);
	virtual int Close();
	virtual bool IsOpen() const;
	virtual int GetSize(SceOff *outSize);
	virtual int Seek(TrophyHandle *handle, SceOff offset, int whence, SceOff *outPos);
	virtual int Truncate(TrophyHandle *handle, SceOff length);
	virtual int Sync(TrophyHandle *handle);
	virtual int Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead);
	virtual int Write(TrophyHandle *handle, const void *srcBuf, SceSize size,
			  SceSize *outWritten);

private:
	TrophyTrpArchive *m_archive;
	uint64_t m_entryOffset;
	uint64_t m_entrySize;
	uint64_t m_cursor;
};

class TrophyTrpArchive : public TrophyHeapObject {
	friend class TrophyTrpFileStream;

public:
	TrophyTrpArchive();
	virtual ~TrophyTrpArchive();

	int Open(TrophyHandle *handle, const char *trpPath, bool verifySha1);
	int Close();
	bool IsOpen() const;
	const TrophyTrpHeader &GetHeader() const { return m_header; }
	uint32_t GetEntryCount() const { return m_header.entryCount; }
	int GetEntryByIndex(uint32_t index, TrophyTrpEntry *outEntry) const;
	int GetEntryPtr(uint32_t index, const TrophyTrpEntry **outEntry) const;
	int FindEntryByName(const char *name, TrophyTrpEntry *outEntry) const;
	int OpenEntryStream(const char *name, TrophyTrpFileStream *outStream);
	int OpenEntryStreamByIndex(uint32_t index, TrophyTrpFileStream *outStream);
	int VerifyArchiveSha1(TrophyHandle *handle);
	int EnsureHeaderAndGetDevFlag(TrophyHandle *handle, bool *outIsDev);
	int ReadDefaultConfOrExtract(TrophyHandle *handle, const TrophyCommId *commId,
				     TrophyConfMeta *outMeta, TrophyConfData **outConf);

private:
	int ReadAndParseHeader(TrophyHandle *handle);
	int LoadEntryTable(TrophyHandle *handle);
	void NotifyStreamClosed();

	TrophyFile m_file;
	TrophyTrpHeader m_header;
	TrophyTrpEntry *m_entries;
	int m_openStreamCount;
	uint8_t m_sha1Verified;
};

class TrophyTrpConfSource : public TrophyConfSourceBase {
public:
	TrophyTrpConfSource();
	virtual ~TrophyTrpConfSource();

	int Open(TrophyHandle *handle, const TrophyMount *mount, const TrophyCommId *commId,
		 bool verifySha1);
	virtual int IsOpen() const { return m_archive.IsOpen() ? 1 : 0; }
	virtual int Close();
	virtual int EstimateSpace(TrophyHandle *handle, const TrophyExtractOptions *options,
				  uint64_t *outUr0Bytes, uint64_t *outUx0Bytes);
	virtual int OpenOwnedAssetStream(TrophyHandle *handle, const char *assetName,
					 TrophyConfSourceStream **outStream);
	virtual int ExtractAssets(TrophyHandle *handle, const TrophyExtractOptions *options,
				  TrophyConfMeta *outMeta, TrophyExtractedAssetReport *outReport,
				  TrophyConfData **outDefaultConf,
				  TrophyConfHandle **outLocalizedConf);

	int IsDevFlagSet(bool *outIsDev) const;
	int ReadHeaderAndConf(TrophyHandle *handle, uint32_t parseOptions,
			      TrophyConfData **outConfData);
	int ReadDefaultConf(TrophyHandle *handle, TrophyConfData **outConfData);
	int ReadConfHandle(TrophyHandle *handle, int lang, TrophyConfHandle **outHandle);
	int ExtractTitleText(TrophyHandle *handle, const TrophyConfHandle *confHandle,
			     void *outTitleDetails);
	int ExtractGroupText(TrophyHandle *handle, const TrophyConfHandle *confHandle,
			     int groupId, void *outGroupDetails);
	int ExtractTrophyText(TrophyHandle *handle, const TrophyConfHandle *confHandle,
			      int trophyId, bool revealDetails, void *outTrophyDetails);
	int OpenSfmStream(TrophyHandle *handle, int lang, bool fallbackDefault,
			  TrophyTrpFileStream *outStream, SceSize *outHeaderSize);
	int OpenTitleIconStream(TrophyHandle *handle, int lang, bool fallbackDefault,
				TrophyTrpFileStream *outStream);
	int OpenOwnedTitleIconStream(TrophyHandle *handle, int lang, int smallFlag,
				     bool fallbackDefault, TrophyConfSourceStream **outStream);
	int OpenGroupIconStream(TrophyHandle *handle, int groupId, int lang, bool fallbackDefault,
				TrophyTrpFileStream *outStream);
	int OpenOwnedGroupIconStream(TrophyHandle *handle, int groupId, int lang, int smallFlag,
				     bool fallbackDefault, TrophyConfSourceStream **outStream);
	int OpenTrophyIconStream(TrophyHandle *handle, int trophyId,
				 TrophyTrpFileStream *outStream);

private:
	int OpenAssetWithLangFallback(const TrophyAssetDesc *requested, bool fallbackDefault,
				      TrophyTrpFileStream *outStream);

	TrophyTrpArchive m_archive;
};

/* =========================================================================
 * Storage arbiter, user session, sync & recovery controllers (0x812CDB98..0x812D00BF)
 * ========================================================================= */
class TrophyStorageArbiter;

class TrophyStorageArbiterClient : public TrophyHeapObject {
	friend class TrophyStorageArbiter;

public:
	TrophyStorageArbiterClient(TrophyStorageArbiter *arbiter, uint32_t accessFlags);
	virtual ~TrophyStorageArbiterClient();

	int Open(TrophyHandle *handle, uint32_t accessFlags);
	int CheckHandle(TrophyHandle *handle) const;
	int CheckPreempted() const;
	uint32_t GetAccessFlags() const { return m_accessFlags; }
	void SetPreemptError(int errorCode);
	void Detach();

private:
	TrophyStorageArbiter *m_arbiter;
	uint32_t m_accessFlags;
	TrophyHandle *m_boundHandle;
	int m_preemptError;
	uint8_t m_registered;
};

class TrophyMountBase {
public:
	TrophyMountBase();
	virtual ~TrophyMountBase();
	virtual int Unmount();

	const char *GetMountPoint() const;
	bool IsMounted() const { return m_mounted != 0; }

protected:
	uint8_t m_mounted;
	char m_mountPoint[16];
	int m_refCount;
};

class TrophyUserRootMount : public TrophyMountBase {
public:
	TrophyUserRootMount();
	virtual ~TrophyUserRootMount();
	int Mount();
	virtual int Unmount();
};

class TrophyTitleMount : public TrophyMountBase {
public:
	TrophyTitleMount();
	virtual ~TrophyTitleMount();
	int MatchesCommId(const TrophyCommId *commId) const;
	int MountData(const TrophyCommId *commId);
	int MountBackup(const TrophyCommId *commId);
	virtual int Unmount();

private:
	TrophyCommId m_commId;
};

class TrophyMountGuard {
public:
	TrophyMountGuard();
	~TrophyMountGuard();

	const char *GetPath() const;
	void Attach(TrophyStorageArbiterClient *client, TrophyMountBase **mountPtr);

private:
	TrophyStorageArbiterClient *m_client;
	TrophyMountBase **m_mountPtr;
};

struct TrophyMountSlot {
	int refCount;
	int mountKind;
	int userId;
	TrophyCommId commId;
	char mountPoint[256];

	void Clear();
	bool Matches(int kind, int uid, const TrophyCommId *id) const;
};

class TrophyStorageArbiter : public TrophyHeapObject {
public:
	TrophyStorageArbiter();
	virtual ~TrophyStorageArbiter();

	static void TermGlobal();
	static int GetGlobal(TrophyStorageArbiter **outArbiter);

	int Init();
	void Term();

	int AcquireClient(TrophyHandle *handle, uint32_t accessFlags,
			  TrophyStorageArbiterClient **outClient);
	void ReleaseClient(TrophyStorageArbiterClient *client);

	int LockUserRoot(int userId, char *outPath, SceSize outSize);
	int LockTitleData(int userId, const TrophyCommId *commId, char *outPath, SceSize outSize);
	int LockTitleBackup(int userId, const TrophyCommId *commId, char *outPath,
			    SceSize outSize);
	int UnlockPath(const char *path);

	static int PreemptErrorForPriority(uint32_t priority);

private:
	int LockMountSlot(int mountKind, int userId, const TrophyCommId *commId, char *outPath,
			  SceSize outSize);
	bool HasConflictLocked(uint32_t requestedFlags, int *outPreemptError,
			       bool *outCanPreemptOthers);

	TrophyLwMutex m_mutex;
	TrophyLwCond m_cond;
	TrophyStorageArbiterClient *m_clients[16];
	TrophyUserRootMount m_userRootMount;
	TrophyTitleMount m_titleDataMount;
	TrophyTitleMount m_titleBackupMount;
	TrophyMountSlot m_mounts[3];
	uint8_t m_initialized;
};

extern TrophyStorageArbiter g_trophyStorageArbiter;

class TrophyUserSession : public TrophyHeapObject {
public:
	TrophyUserSession();
	virtual ~TrophyUserSession();

	int Open(TrophyHandle *handle, TrophyStorageArbiter *arbiter, int userId,
		 uint32_t accessFlags);
	int Close();
	bool IsOpen() const;
	int EnsureIndexOpen(TrophyHandle *handle);
	int EnsureReady(TrophyHandle *handle, int requiredMode);

	int GetUserId() const { return m_userId; }
	TrophyStorageArbiter *GetArbiter() const { return m_arbiter; }
	TrophyUserIndexStorage *GetIndexStorage() const { return m_indexStorage; }
	TrophyNetClient *GetNetClient() const { return m_netClient; }
	const SceKernelOpenPsId *GetOpenPsId() const { return &m_openPsId; }
	int GetAccessMode() const;
	bool IsWritable() const;
	uint32_t GetAccessFlags() const { return m_accessFlags; }

	int PurgeTitle(TrophyHandle *handle, const TrophyCommId *commId);
	int RemoveAllTitles(TrophyHandle *handle);
	int RemoveAllTitlesData(TrophyHandle *handle);
	int FindOrRecoverTitleRecord(TrophyHandle *handle, const TrophyCommId *commId,
				     bool *outExists, TrophyTitleRecord *outRecord);
	int GetTitleInstallState(TrophyHandle *handle, const TrophyCommId *commId, int *outState);
	int CheckTitleAgainstTrp(TrophyHandle *handle, const TrophyCommId *commId,
				 TrophyTrpConfSource *trpSource, int *outState,
				 TrophyGradeBitsets *outBitsets);
	int GetTitleCount(TrophyHandle *handle, uint32_t *outCount);
	int GetInstalledCount(TrophyHandle *handle, uint32_t *outCount);
	int EnumerateTitleInfo(TrophyHandle *handle, uint32_t startIndex,
			       uint32_t *outFoundIndex, void *outTitleInfo);
	int GetOldestTitleInfo(TrophyHandle *handle, void *outTitleInfo, void *outSyncInfo,
			       void *outTitleDetails);
	static int DebugMarkTitleBroken(const TrophyCommId *commId, int brokenKind);

private:
	uint8_t m_isOpen;
	SceKernelOpenPsId m_openPsId;
	int m_userId;
	TrophyStorageArbiter *m_arbiter;
	TrophyStorageArbiterClient *m_arbiterClient;
	TrophyUserIndexStorage *m_indexStorage;
	TrophyNetClient *m_netClient;
	uint32_t m_accessFlags;
};

class TrophySyncController : public TrophyHeapObject {
public:
	explicit TrophySyncController(TrophyUserSession *session);
	virtual ~TrophySyncController();

	int CheckNetSync(TrophyHandle *handle, uint64_t options, int *outNeedSync);
	int SyncTitles(TrophyHandle *handle, uint64_t options);

private:
	int PrepareModifiedTitleFetcher(TrophyHandle *handle, const SceRtcTick *sinceUtc,
					TrophyProgress *progress);
	int UploadDirtyTitles(TrophyHandle *handle, uint32_t *outUploadedCount);
	int InstallTitleFromNet(TrophyHandle *handle, const TrophyCommId *commId, int lang);
	int DownloadModifiedTitles(TrophyHandle *handle, const SceNpId *npId,
				   const SceRtcTick *sinceTick, uint32_t *outUpdatedCount);
	int SyncIncremental(TrophyHandle *handle);
	int SyncFull(TrophyHandle *handle);

	TrophyUserSession *m_session;
	TrophyNetTitleListFetcher *m_titleListFetcher;
};

class TrophyRecoveryController : public TrophyHeapObject {
public:
	explicit TrophyRecoveryController(TrophyUserSession *session);
	virtual ~TrophyRecoveryController();

	int CheckRecoveryRequired(TrophyHandle *handle, bool *outRequired);
	int RecoverAllTitles(TrophyHandle *handle);
	int RecoverTitleIfExists(TrophyHandle *handle, const TrophyCommId *commId);
	int RecoverTitleEntry(TrophyHandle *handle, const TrophyCommId *commId);
	int ScanAndRecoverAllTitles(TrophyHandle *handle);

private:
	int RecoverSingleTitle(TrophyHandle *handle, const TrophyCommId *commId,
			       const TrophyTitleRecord *indexRecord);
	int ScanAndRebuildUserIndex(TrophyHandle *handle);

	TrophyUserSession *m_session;
};

/* =========================================================================
 * Service context, stream, refcounted pools & client state (0x812C7F70..0x812CB693)
 * ========================================================================= */
class TrophyContext : public TrophyHeapObject {
public:
	TrophyContext();
	virtual ~TrophyContext();

	int Init(SceUID pid, const TrophyCommId *commId, const void *commSign, uint64_t options);
	const TrophyCommId &GetCommId() const { return m_commId; }
	SceUID GetPid() const { return m_pid; }
	const void *GetCommSign() const { return m_commSign; }
	const char *GetTitleId() const { return m_titleId; }
	int GetInstallLang() const { return m_installLang; }
	int GetDisplayLang() const { return m_displayLang; }
	uint64_t GetOptions() const { return m_options; }

	bool HasCommSign() const { return m_commSign != NULL; }
	int GetServerTag(const void **outTag) const;
	int GetTrialFlag(uint8_t *outIsTrial) const;
	int GetUnlockAllowed(uint8_t *outAllowed) const;
	int GetOfflineFlag(uint8_t *outOffline) const;

	TrophyConfHandle *GetTrpConfHandle() const { return m_trpConfHandle; }
	TrophyConfHandle *GetStorageConfHandle() const { return m_storageConfHandle; }
	int SetTrpConfHandle(TrophyConfHandle **ioHandle);
	int SetStorageConfHandle(TrophyConfHandle **ioHandle);
	int SetGradeBitsets(const TrophyGradeBitsets *bitsets);
	const TrophyGradeBitsets &GetGradeBitsets() const { return m_gradeBitsets; }

	static bool IsTrialExemptTitleId(const char *titleId);

private:
	SceUID m_pid;
	TrophyCommId m_commId;
	uint8_t *m_commSign;
	char *m_titleId;
	TrophyConfHandle *m_trpConfHandle;
	TrophyConfHandle *m_storageConfHandle;
	int m_installLang;
	int m_displayLang;
	TrophyGradeBitsets m_gradeBitsets;
	uint64_t m_options;
};

class TrophyStreamWrapper : public TrophyHeapObject {
public:
	TrophyStreamWrapper();
	virtual ~TrophyStreamWrapper();

	int BindFile(TrophyFileBase *ownedFile, bool stripSfmHeader, SceSize headerSize);
	int Close();
	bool IsOpen() const { return m_file != NULL; }
	int GetReadableSize(SceUInt64 *outSize);
	int Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead);

private:
	TrophyFileBase *m_file;
	SceOff m_baseOffset;
	uint8_t m_positioned;
};

class TrophyPoolBase;

class TrophyPooledObject : public TrophyHeapObject {
	friend class TrophyPoolBase;

public:
	TrophyPooledObject();
	virtual ~TrophyPooledObject();

	virtual void OnAdded() {}
	virtual void OnRemoved() {}
	virtual void OnSuspended(int suspendError) { (void)suspendError; }

	int GetId() const { return m_id; }
	bool IsValid() const { return m_valid != 0; }
	bool IsAutoEvict() const { return m_autoEvict != 0; }
	int GetSuspendError() const { return m_suspendError; }
	void SetAutoEvict(bool autoEvict) { m_autoEvict = autoEvict ? 1 : 0; }
	void AddRef();
	void ReleaseRef();

protected:
	int m_id;
	uint8_t m_autoEvict;
	uint8_t m_valid;
	uint8_t m_suspendedNotified;
	int m_suspendError;
	uint8_t m_registered;
	int m_refCount;
	TrophyPoolBase *m_pool;
	TrophyPooledObject **m_slotPtr;
};

class TrophyPoolBase {
public:
	TrophyPoolBase(TrophyLwMutex *mutex, int idBase, int idSpan, uint32_t capacity,
		       TrophyPooledObject **slots);
	~TrophyPoolBase();

	int Add(TrophyPooledObject *obj, int *outId);
	int AcquireById(int id, TrophyPooledObject **outObj);
	void Release(TrophyPooledObject *obj);
	int RemoveById(int id);
	void RemoveAll();
	void SuspendAll(int suspendError);
	void ResumeAll();
	void EvictAutoEvictable();
	TrophyPooledObject *GetSlot(uint32_t index) const;
	uint32_t GetCapacity() const { return m_capacity; }

private:
	int AllocateIdLocked();
	void DestroyObjectLocked(TrophyPooledObject *obj);

	TrophyLwMutex *m_mutex;
	int m_idBase;
	int m_idSpan;
	int m_nextIdOffset;
	uint32_t m_capacity;
	TrophyPooledObject **m_slots;
};

class TrophyHandlePoolObj : public TrophyPooledObject {
public:
	TrophyHandlePoolObj();
	virtual ~TrophyHandlePoolObj();

	int Init(int handleType);
	virtual void OnSuspended(int suspendError);

	int GetHandleType() const { return m_handleType; }
	TrophyHandle *GetHandle() const { return m_handle; }

private:
	int m_handleType;
	TrophyHandle *m_handle;
};

class TrophyContextPoolObj : public TrophyPooledObject {
public:
	TrophyContextPoolObj();
	virtual ~TrophyContextPoolObj();

	int Init(int titleIdMode, SceUID pid, const TrophyCommId *commId, const void *commSign,
		 uint64_t options);

	TrophyContext *GetContext() const { return m_context; }
	int GetTitleIdMode() const { return m_titleIdMode; }
	bool CanWriteStorage() const;
	int MarkRegistered();
	int MarkTrpLoaded();
	bool IsRegisteredAndLoaded() const;
	int EnsureTrpConfLoaded(TrophyHandle *handle, TrophyTrpConfSource *trpSource);
	int EnsureStorageConfLoaded(TrophyHandle *handle, TrophyTitleStorage *titleStorage);

private:
	int m_titleIdMode;
	TrophyContext *m_context;
	uint8_t m_registeredFlag;
	uint8_t m_trpLoadedFlag;
};

class TrophyTitleStoragePoolObj;
class TrophyConfSourcePoolObj;

class TrophyTitleSessionPoolObj : public TrophyPooledObject {
public:
	TrophyTitleSessionPoolObj();
	virtual ~TrophyTitleSessionPoolObj();

	int Init(TrophyHandle *handle, TrophyStorageArbiter *arbiter, int userId,
		 uint32_t accessFlags);
	int OpenSession(TrophyHandle *handle, uint32_t accessFlags, int userId);
	int CloseSession();
	virtual void OnRemoved();

	TrophyUserSession *GetUserSession() const { return m_userSession; }
	TrophyTitleStoragePoolObj *GetBoundStorage() const { return m_boundStorage; }
	TrophyConfSourcePoolObj *GetBoundConfSource() const { return m_boundConfSource; }
	void SetBoundStorage(TrophyTitleStoragePoolObj *storage);
	void SetBoundConfSource(TrophyConfSourcePoolObj *source);

private:
	TrophyUserSession *m_userSession;
	TrophyTitleStoragePoolObj *m_boundStorage;
	TrophyConfSourcePoolObj *m_boundConfSource;
};

class TrophyTitleStoragePoolObj : public TrophyPooledObject {
public:
	TrophyTitleStoragePoolObj();
	virtual ~TrophyTitleStoragePoolObj();

	int Init(TrophyUserSession *session, const TrophyCommId *commId);
	virtual void OnRemoved();

	bool IsStorageOpen() const;
	bool IsOpenForMode(int openMode) const;
	int CloseStorage();
	int CloseIfOpen();
	int PurgeStorage(TrophyHandle *handle, TrophyUserSession *session);
	int OpenForMode(int openMode, TrophyHandle *handle, TrophyUserSession *session);
	int InstallFromTrp(int openMode, TrophyHandle *handle, TrophyUserSession *session,
			   TrophyContext *context, TrophyConfSource *source, int *outPlatinumId);
	int UpgradeFromTrp(int openMode, TrophyHandle *handle, TrophyUserSession *session,
			   TrophyContext *context, TrophyConfSource *source, int *outPlatinumId);
	bool MatchesCommId(const TrophyTitleStoragePoolObj *other) const;

	TrophyTitleStorage *GetTitleStorage() const { return m_titleStorage; }
	const TrophyCommId &GetCommId() const { return m_commId; }
	TrophyUserSession *GetOwnerSession() const { return m_ownerSession; }

private:
	TrophyTitleStorage *m_titleStorage;
	int m_openMode;
	TrophyCommId m_commId;
	TrophyUserSession *m_ownerSession;
};

class TrophyConfSourcePoolObj : public TrophyPooledObject {
public:
	TrophyConfSourcePoolObj();
	virtual ~TrophyConfSourcePoolObj();

	int Init(const TrophyCommId *commId);
	virtual void OnRemoved();

	bool IsSourceOpen() const;
	int CloseSource();
	int CloseIfOpen();
	bool MatchesCommId(const TrophyConfSourcePoolObj *other) const;

	TrophyTrpConfSource *GetConfSource() const { return m_confSource; }
	const TrophyCommId &GetCommId() const { return m_commId; }

private:
	TrophyTrpConfSource *m_confSource;
	TrophyCommId m_commId;
};

class TrophyStreamPoolObj : public TrophyPooledObject {
public:
	TrophyStreamPoolObj();
	virtual ~TrophyStreamPoolObj();

	int Init(TrophyStreamWrapper *stream, TrophyTitleStoragePoolObj *heldStorage,
		 TrophyConfSourcePoolObj *heldConfSource);
	virtual void OnRemoved();

	int OpenStorageSfmStream(TrophyHandle *handle, TrophyTitleStorage *storage,
				 SceOff *outSize);
	int OpenStorageTitleIconStream(TrophyHandle *handle, TrophyTitleStorage *storage,
				       SceOff *outSize);
	int OpenStorageGroupIconStream(TrophyHandle *handle, TrophyTitleStorage *storage,
				       int groupId, SceOff *outSize);
	int OpenStorageTrophyIconStream(TrophyHandle *handle, TrophyTitleStorage *storage,
					int trophyId, SceOff *outSize);
	int OpenTrpTitleIconStream(TrophyHandle *handle, TrophyTrpConfSource *trpSource,
				   int lang, SceOff *outSize);
	int OpenTrpGroupIconStream(TrophyHandle *handle, TrophyTrpConfSource *trpSource,
				   int groupId, int lang, SceOff *outSize);
	int OpenTrpTrophyIconStream(TrophyHandle *handle, TrophyTrpConfSource *trpSource,
				    int trophyId, SceOff *outSize);

	TrophyStreamWrapper *GetStream() const { return m_stream; }

private:
	TrophyStreamWrapper *m_stream;
	TrophyInputStream *m_activeInput;
	TrophyFileBase *m_ownedStorageFile;
	TrophyConfSourceStream *m_ownedTrpStream;
	TrophyTitleStoragePoolObj *m_heldStorage;
	TrophyConfSourcePoolObj *m_heldConfSource;
};

class TrophyClientState : public TrophyHeapObject {
public:
	explicit TrophyClientState(TrophyLwMutex *mutex);
	virtual ~TrophyClientState();

	int InitPools(SceUID pid, IPMI::Session *session, TrophyLwMutex *mutex);
	int Connect(SceUID pid, IPMI::Session *session);
	void Disconnect(int reason);
	void Suspend(int reason);
	void Resume();

	bool IsActive() const { return m_active != 0; }
	bool HasActiveRefs() const;
	SceUID GetPid() const { return m_pid; }
	IPMI::Session *GetSession() const { return m_session; }
	TrophyMount *GetMount() { return &m_mount; }
	int EnsureMounted();

	TrophyPoolBase &GetHandlePool() { return m_handlePool; }
	TrophyPoolBase &GetContextPool() { return m_contextPool; }
	TrophyPoolBase &GetTitleSessionPool() { return m_titleSessionPool; }
	TrophyPoolBase &GetTitleStoragePool() { return m_titleStoragePool; }
	TrophyPoolBase &GetConfSourcePool() { return m_confSourcePool; }
	TrophyPoolBase &GetStreamPool() { return m_streamPool; }

	int CreateHandle(int handleType, TrophyHandlePoolObj **outObj);
	int CreateContext(int titleIdMode, SceUID pid, const TrophyCommId *commId,
			  const void *commSign, uint64_t options,
			  TrophyContextPoolObj **outObj);
	int CreateTitleSession(TrophyTitleSessionPoolObj **outObj);
	int CreateStream(TrophyStreamPoolObj **outObj);
	int EnsureConfSourceOpen(TrophyTitleSessionPoolObj **sessionSlot,
				 const TrophyCommId *commId);

	int AcquireHandle(int handleId, TrophyHandlePoolObj **outObj);
	int AcquireContext(int ctxId, TrophyContextPoolObj **outObj);
	int AcquireTitleSession(int sessionId, TrophyTitleSessionPoolObj **outObj);
	int AcquireOrCreateTitleStorage(TrophyTitleSessionPoolObj *titleSessionObj,
					const TrophyCommId *commId,
					TrophyTitleStoragePoolObj **outObj);
	int AcquireOrCreateConfSource(const TrophyCommId *commId, TrophyConfSourcePoolObj **outObj);
	int AcquireStream(int streamId, TrophyStreamPoolObj **outObj);

	void SuspendTitleSessions(int reason);
	void SuspendStreams(int reason);
	void CloseAllOpenConfSources();
	void CloseAllOpenTitleStorages();
	void EvictTitleStorageForCommId(const TrophyCommId *commId);
	void EvictAllTitleStorage();

private:
	uint8_t m_active;
	SceUID m_pid;
	IPMI::Session *m_session;
	TrophyLwMutex *m_poolMutex;
	TrophyPooledObject *m_handleSlots[16];
	TrophyPooledObject *m_contextSlots[4];
	TrophyPooledObject *m_titleSessionSlots[16];
	TrophyPooledObject *m_titleStorageSlots[16];
	TrophyPooledObject *m_confSourceSlots[16];
	TrophyPooledObject *m_streamSlots[4];
	TrophyPoolBase m_handlePool;
	TrophyPoolBase m_contextPool;
	TrophyPoolBase m_titleSessionPool;
	TrophyPoolBase m_titleStoragePool;
	TrophyPoolBase m_confSourcePool;
	TrophyPoolBase m_streamPool;
	TrophyMount m_mount;
};

class SceNpTrophyService : public TrophyHeapObject {
public:
	SceNpTrophyService();
	virtual ~SceNpTrophyService();

	int Init();
	int Term();
	bool IsInitialized() const { return m_initialized != 0; }

	int RegisterClient(SceUID pid, IPMI::Session *session, TrophyClientState **outClient);
	int UnregisterClient(SceUID pid, IPMI::Session *session);
	int OnClientConnect(SceUID pid, IPMI::Session *session);
	int OnClientDisconnect(SceUID pid, IPMI::Session *session, int reason);
	int SuspendPid(SceUID pid);
	int ResumePid(SceUID pid);

	int FindClient(SceUID pid, IPMI::Session *session, TrophyClientState **outClient);
	TrophyLwMutex *GetMutex() { return &m_mutex; }

	int SetDebugParam(const char *name, const void *buf, SceSize size);
	int GetDebugParam(const char *name, void *outBuf, SceSize size);
	int ExecDebugOp(int op, const void *arg, SceSize argSize);

	bool IsSyncInGameDisabled() const;
	bool IsFakeTrialFlagEnabled() const;

private:
	TrophyClientState *FindClientLocked(SceUID pid, IPMI::Session *session);

	TrophyLwMutex m_mutex;
	TrophyClientState *m_clients[8];
	uint8_t m_initialized;
};

extern SceNpTrophyService g_npTrophyService;

#endif /* SHELL_NP_TROPHY_SERVICE_H */
