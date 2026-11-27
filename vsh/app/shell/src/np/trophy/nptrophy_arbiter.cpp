/*
 * nptrophy_arbiter.cpp: SceNpTrophy storage arbiter, mount slot refcounting,
 * user session lifecycle, incremental/full PSN sync controller, and local
 * storage recovery controller (0x812CDB98..0x812D00BF).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include "nptrophy_service.h"

#define TROPHY_ARBITER_MODE_MASK	0x000F0000u
#define TROPHY_ARBITER_MODE_READ_ONLY	0x00010000u
#define TROPHY_ARBITER_MODE_READ_WRITE	0x00020000u
#define TROPHY_ARBITER_MODE_EXCLUSIVE	0x00030000u
#define TROPHY_ARBITER_PREEMPT_MASK	0x00F00000u
#define TROPHY_ARBITER_PREEMPT_FLAG	0x00200000u
#define TROPHY_ARBITER_PRIORITY_MASK	0x0000000Fu
#define TROPHY_ARBITER_NET_FLAG		0x00000100u

#define TROPHY_MOUNT_KIND_USER_ROOT	1
#define TROPHY_MOUNT_KIND_TITLE_DATA	2
#define TROPHY_MOUNT_KIND_TITLE_BACKUP	3

TrophyStorageArbiter g_trophyStorageArbiter;

/* =========================================================================
 * TrophyStorageArbiterClient (0x812CDB98..0x812CDC5F)
 * ========================================================================= */

TrophyStorageArbiterClient::TrophyStorageArbiterClient(TrophyStorageArbiter *arbiter,
						       uint32_t accessFlags)
	: m_arbiter(arbiter),
	  m_accessFlags(accessFlags),
	  m_preemptError(0),
	  m_registered(0)
{
}

TrophyStorageArbiterClient::~TrophyStorageArbiterClient()
{
	if (m_registered != 0 && m_arbiter != NULL)
		m_arbiter->ReleaseClient(this);
}

int TrophyStorageArbiterClient::CheckPreempted() const
{
	return m_preemptError;
}

void TrophyStorageArbiterClient::SetPreemptError(int errorCode)
{
	if (m_preemptError == 0)
		m_preemptError = errorCode;
}

void TrophyStorageArbiterClient::Detach()
{
	m_registered = 0;
	m_arbiter = NULL;
}

/* =========================================================================
 * TrophyMountSlot (0x812CE3C0..0x812CE47F)
 * ========================================================================= */

void TrophyMountSlot::Clear()
{
	refCount = 0;
	mountKind = 0;
	userId = 0;
	commId.Clear();
	sceClibMemset(mountPoint, 0, sizeof(mountPoint));
}

bool TrophyMountSlot::Matches(int kind, int uid, const TrophyCommId *id) const
{
	if (refCount <= 0 || mountKind != kind || userId != uid)
		return false;
	if (kind == TROPHY_MOUNT_KIND_USER_ROOT)
		return true;
	if (id == NULL)
		return false;
	return commId.Equals(*id);
}

/* =========================================================================
 * TrophyStorageArbiter (0x812CDC60..0x812CE5AF)
 * ========================================================================= */

TrophyStorageArbiter::TrophyStorageArbiter()
	: m_initialized(0)
{
	sceClibMemset(m_clients, 0, sizeof(m_clients));
	for (uint32_t i = 0; i < 3; ++i)
		m_mounts[i].Clear();
}

TrophyStorageArbiter::~TrophyStorageArbiter()
{
	Term();
}

int TrophyStorageArbiter::Init()
{
	if (m_initialized != 0)
		return NP_TROPHY_ERR_ALREADY_INITIALIZED;

	int ret = m_mutex.Init("SceNpTrophyStorageArbiter", false);
	if (ret < 0)
		return ret;

	ret = m_cond.Init("SceNpTrophyStorageArbiter", &m_mutex);
	if (ret < 0) {
		m_mutex.Term();
		return ret;
	}

	sceClibMemset(m_clients, 0, sizeof(m_clients));
	for (uint32_t i = 0; i < 3; ++i)
		m_mounts[i].Clear();

	m_initialized = 1;
	return 0;
}

void TrophyStorageArbiter::Term()
{
	if (m_initialized == 0)
		return;

	m_mutex.Lock();
	for (uint32_t i = 0; i < 16; ++i) {
		if (m_clients[i] != NULL) {
			m_clients[i]->Detach();
			m_clients[i] = NULL;
		}
	}
	for (uint32_t i = 0; i < 3; ++i) {
		if (m_mounts[i].refCount > 0 && m_mounts[i].mountPoint[0] != '\0')
			sceAppMgrUmount(m_mounts[i].mountPoint);
		m_mounts[i].Clear();
	}
	m_mutex.Unlock();

	m_cond.Term();
	m_mutex.Term();
	m_initialized = 0;
}

int TrophyStorageArbiter::PreemptErrorForPriority(uint32_t priority)
{
	switch (priority) {
	case 1:
		return NP_TROPHY_ERR_ARBITER_PREEMPT_LOW;
	case 2:
		return NP_TROPHY_ERR_ARBITER_PREEMPT_MID;
	case 3:
		return NP_TROPHY_ERR_ARBITER_PREEMPT_HIGH;
	default:
		return NP_TROPHY_ERR_BUSY;
	}
}

bool TrophyStorageArbiter::HasConflictLocked(uint32_t requestedFlags, int *outPreemptError,
					     bool *outCanPreemptOthers)
{
	uint32_t reqMode = requestedFlags & TROPHY_ARBITER_MODE_MASK;
	uint32_t reqPriority = requestedFlags & TROPHY_ARBITER_PRIORITY_MASK;
	bool reqCanPreempt = (requestedFlags & TROPHY_ARBITER_PREEMPT_MASK) ==
			     TROPHY_ARBITER_PREEMPT_FLAG;

	bool hasConflict = false;
	bool canPreemptAll = reqCanPreempt;
	int blockingError = NP_TROPHY_ERR_BUSY;

	for (uint32_t i = 0; i < 16; ++i) {
		TrophyStorageArbiterClient *existing = m_clients[i];
		if (existing == NULL)
			continue;

		uint32_t existingMode = existing->m_accessFlags & TROPHY_ARBITER_MODE_MASK;
		bool incompatible = (reqMode != TROPHY_ARBITER_MODE_READ_ONLY) ||
				    (existingMode != TROPHY_ARBITER_MODE_READ_ONLY);
		if (!incompatible)
			continue;

		hasConflict = true;
		uint32_t existingPriority = existing->m_accessFlags & TROPHY_ARBITER_PRIORITY_MASK;
		if (reqPriority <= existingPriority) {
			canPreemptAll = false;
			blockingError = PreemptErrorForPriority(existingPriority);
		}
	}

	if (outPreemptError != NULL)
		*outPreemptError = blockingError;
	if (outCanPreemptOthers != NULL)
		*outCanPreemptOthers = canPreemptAll;
	return hasConflict;
}

int TrophyStorageArbiter::AcquireClient(TrophyHandle *handle, uint32_t accessFlags,
					TrophyStorageArbiterClient **outClient)
{
	if (m_initialized == 0)
		return NP_TROPHY_ERR_NOT_INITIALIZED;
	if (outClient == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	*outClient = NULL;

	TrophyStorageArbiterClient *client = new TrophyStorageArbiterClient(this, accessFlags);
	if (client == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	uint32_t reqPriority = accessFlags & TROPHY_ARBITER_PRIORITY_MASK;
	int ret = 0;

	m_mutex.Lock();

	for (;;) {
		if (handle != NULL) {
			int cancelErr = handle->CheckCancel();
			if (cancelErr < 0) {
				ret = cancelErr;
				break;
			}
		}

		int blockingError = 0;
		bool canPreemptOthers = false;
		if (!HasConflictLocked(accessFlags, &blockingError, &canPreemptOthers)) {
			int freeSlot = -1;
			for (uint32_t i = 0; i < 16; ++i) {
				if (m_clients[i] == NULL) {
					freeSlot = static_cast<int>(i);
					break;
				}
			}
			if (freeSlot < 0) {
				ret = NP_TROPHY_ERR_ARBITER_CLIENT_LIMIT;
				break;
			}
			m_clients[freeSlot] = client;
			client->m_registered = 1;
			*outClient = client;
			ret = 0;
			break;
		}

		if (!canPreemptOthers) {
			ret = blockingError;
			break;
		}

		int preemptErr = PreemptErrorForPriority(reqPriority);
		for (uint32_t i = 0; i < 16; ++i) {
			if (m_clients[i] != NULL)
				m_clients[i]->SetPreemptError(preemptErr);
		}

		unsigned int waitUsec = 100000u;
		m_cond.Wait(&waitUsec);
	}

	m_mutex.Unlock();

	if (ret < 0)
		delete client;
	return ret;
}

void TrophyStorageArbiter::ReleaseClient(TrophyStorageArbiterClient *client)
{
	if (m_initialized == 0 || client == NULL)
		return;

	m_mutex.Lock();
	for (uint32_t i = 0; i < 16; ++i) {
		if (m_clients[i] == client) {
			m_clients[i] = NULL;
			break;
		}
	}
	client->Detach();
	m_cond.Signal();
	m_mutex.Unlock();
}

int TrophyStorageArbiter::LockMountSlot(int mountKind, int userId, const TrophyCommId *commId,
					char *outPath, SceSize outSize)
{
	if (m_initialized == 0)
		return NP_TROPHY_ERR_NOT_INITIALIZED;
	if (outPath == NULL || outSize == 0)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	m_mutex.Lock();

	for (uint32_t i = 0; i < 3; ++i) {
		if (m_mounts[i].Matches(mountKind, userId, commId)) {
			m_mounts[i].refCount++;
			npTrophyStrlcpy(outPath, m_mounts[i].mountPoint, outSize);
			m_mutex.Unlock();
			return 0;
		}
	}

	int freeSlot = -1;
	for (uint32_t i = 0; i < 3; ++i) {
		if (m_mounts[i].refCount == 0) {
			freeSlot = static_cast<int>(i);
			break;
		}
	}

	if (freeSlot < 0) {
		m_mutex.Unlock();
		return NP_TROPHY_ERR_ARBITER_PATH_CONFLICT;
	}

	char mountBuf[256];
	sceClibMemset(mountBuf, 0, sizeof(mountBuf));
	int ret = 0;

	if (mountKind == TROPHY_MOUNT_KIND_USER_ROOT) {
		ret = sceAppMgrTrophyMount(TROPHY_MOUNT_ID_USER_ROOT, -1, mountBuf);
	} else {
		if (commId == NULL) {
			m_mutex.Unlock();
			return NP_TROPHY_ERR_INVALID_ARGUMENT;
		}
		char commStr[16];
		ret = npTrophyFormatCommId(commId, commStr, sizeof(commStr));
		if (ret >= 0) {
			int appMountId = (mountKind == TROPHY_MOUNT_KIND_TITLE_DATA)
						 ? TROPHY_MOUNT_ID_TITLE_DATA
						 : TROPHY_MOUNT_ID_TITLE_BACKUP;
			ret = sceAppMgrTrophyMountById(appMountId, commStr, mountBuf);
		}
	}

	if (ret < 0) {
		m_mutex.Unlock();
		return ret;
	}

	TrophyMountSlot &slot = m_mounts[freeSlot];
	slot.refCount = 1;
	slot.mountKind = mountKind;
	slot.userId = userId;
	if (commId != NULL)
		slot.commId = *commId;
	else
		slot.commId.Clear();
	npTrophyStrlcpy(slot.mountPoint, mountBuf, sizeof(slot.mountPoint));
	npTrophyStrlcpy(outPath, slot.mountPoint, outSize);

	m_mutex.Unlock();
	return 0;
}

int TrophyStorageArbiter::LockUserRoot(int userId, char *outPath, SceSize outSize)
{
	return LockMountSlot(TROPHY_MOUNT_KIND_USER_ROOT, userId, NULL, outPath, outSize);
}

int TrophyStorageArbiter::LockTitleData(int userId, const TrophyCommId *commId, char *outPath,
					SceSize outSize)
{
	return LockMountSlot(TROPHY_MOUNT_KIND_TITLE_DATA, userId, commId, outPath, outSize);
}

int TrophyStorageArbiter::LockTitleBackup(int userId, const TrophyCommId *commId, char *outPath,
					  SceSize outSize)
{
	return LockMountSlot(TROPHY_MOUNT_KIND_TITLE_BACKUP, userId, commId, outPath, outSize);
}

int TrophyStorageArbiter::UnlockPath(const char *path)
{
	if (m_initialized == 0)
		return NP_TROPHY_ERR_NOT_INITIALIZED;
	if (path == NULL || path[0] == '\0')
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	m_mutex.Lock();

	for (uint32_t i = 0; i < 3; ++i) {
		TrophyMountSlot &slot = m_mounts[i];
		if (slot.refCount <= 0)
			continue;
		if (sceClibStrncmp(slot.mountPoint, path, sizeof(slot.mountPoint)) != 0)
			continue;

		slot.refCount--;
		if (slot.refCount == 0) {
			sceAppMgrUmount(slot.mountPoint);
			slot.Clear();
		}
		m_mutex.Unlock();
		return 0;
	}

	m_mutex.Unlock();
	return NP_TROPHY_ERR_ARBITER_NOT_FOUND;
}

/* Arbiter bridge functions declared in nptrophy_storage.h */
int npTrophyArbiterLockUserRoot(TrophyStorageArbiter *arbiter, int userId, char *outPath,
				SceSize outSize)
{
	if (arbiter == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	return arbiter->LockUserRoot(userId, outPath, outSize);
}

int npTrophyArbiterLockTitleData(TrophyStorageArbiter *arbiter, int userId,
				 const TrophyCommId *commId, char *outPath, SceSize outSize)
{
	if (arbiter == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	return arbiter->LockTitleData(userId, commId, outPath, outSize);
}

int npTrophyArbiterLockTitleBackup(TrophyStorageArbiter *arbiter, int userId,
				   const TrophyCommId *commId, char *outPath, SceSize outSize)
{
	if (arbiter == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	return arbiter->LockTitleBackup(userId, commId, outPath, outSize);
}

int npTrophyArbiterUnlockPath(TrophyStorageArbiter *arbiter, const char *path)
{
	if (arbiter == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	return arbiter->UnlockPath(path);
}

/* =========================================================================
 * TrophyUserSession (0x812CE5B0..0x812CF21F)
 * ========================================================================= */

TrophyUserSession::TrophyUserSession()
	: m_isOpen(0),
	  m_userId(0),
	  m_arbiter(NULL),
	  m_arbiterClient(NULL),
	  m_indexStorage(NULL),
	  m_netClient(NULL),
	  m_accessFlags(0)
{
	sceClibMemset(&m_openPsId, 0, sizeof(m_openPsId));
}

TrophyUserSession::~TrophyUserSession()
{
	Close();
}

int TrophyUserSession::GetAccessMode() const
{
	uint32_t mode = m_accessFlags & TROPHY_ARBITER_MODE_MASK;
	if (mode == TROPHY_ARBITER_MODE_READ_ONLY)
		return 1;
	return 2;
}

bool TrophyUserSession::IsWritable() const
{
	return (m_accessFlags & TROPHY_ARBITER_MODE_MASK) != TROPHY_ARBITER_MODE_READ_ONLY;
}

int TrophyUserSession::Open(TrophyHandle *handle, TrophyStorageArbiter *arbiter, int userId,
			    uint32_t accessFlags)
{
	if (IsOpen())
		return NP_TROPHY_ERR_BUSY;
	if (arbiter == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	int ret = npTrophyGetOpenPsId(&m_openPsId);
	if (ret < 0)
		return ret;

	ret = arbiter->AcquireClient(handle, accessFlags, &m_arbiterClient);
	if (ret < 0)
		return ret;

	m_arbiter = arbiter;
	m_userId = userId;
	m_accessFlags = accessFlags;

	m_indexStorage = new TrophyUserIndexStorage(m_arbiter, &m_openPsId, m_userId);
	if (m_indexStorage == NULL) {
		Close();
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	}

	if ((accessFlags & TROPHY_ARBITER_NET_FLAG) != 0) {
		m_netClient = new TrophyNetClient();
		if (m_netClient == NULL) {
			Close();
			return NP_TROPHY_ERR_OUT_OF_MEMORY;
		}
		ret = m_netClient->Init();
		if (ret < 0) {
			Close();
			return ret;
		}
	}

	TrophyDualTick nowTicks;
	nowTicks.Clear();
	npTrophyGetDualTick(&nowTicks);

	ret = m_indexStorage->Open(handle, GetAccessMode(), &nowTicks);
	if (ret < 0) {
		Close();
		return ret;
	}

	m_isOpen = 1;
	return 0;
}

int TrophyUserSession::Close()
{
	if (m_indexStorage != NULL) {
		m_indexStorage->Close();
		delete m_indexStorage;
		m_indexStorage = NULL;
	}
	if (m_netClient != NULL) {
		m_netClient->Term();
		delete m_netClient;
		m_netClient = NULL;
	}
	if (m_arbiterClient != NULL) {
		delete m_arbiterClient;
		m_arbiterClient = NULL;
	}
	m_arbiter = NULL;
	m_userId = 0;
	m_accessFlags = 0;
	m_isOpen = 0;
	return 0;
}

int TrophyUserSession::EnsureReady(TrophyHandle *handle, int requiredMode)
{
	if (!IsOpen())
		return NP_TROPHY_ERR_USER_SESSION_NOT_OPEN;
	if (handle != NULL) {
		int cancelErr = handle->CheckCancel();
		if (cancelErr < 0)
			return cancelErr;
	}
	if (m_arbiterClient != NULL) {
		int preemptErr = m_arbiterClient->CheckPreempted();
		if (preemptErr < 0)
			return preemptErr;
	}
	if (requiredMode > 1 && !IsWritable())
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	return 0;
}

int TrophyUserSession::PurgeTitle(TrophyHandle *handle, const TrophyCommId *commId)
{
	int ret = EnsureReady(handle, 2);
	if (ret < 0)
		return ret;
	if (commId == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	int userRet = TrophyUserStorage::RemoveDirectory(m_userId, commId);
	int confRet = TrophyConfStorage::RemoveDirectories(m_userId, commId);

	bool exists = false;
	if (m_indexStorage->HasTitle(handle, commId, &exists, NULL) >= 0 && exists) {
		ret = m_indexStorage->RemoveTitle(handle, commId);
		if (ret < 0)
			return ret;
	}

	if (userRet < 0)
		return userRet;
	return confRet;
}

int TrophyUserSession::RemoveAllTitles(TrophyHandle *handle)
{
	int ret = EnsureReady(handle, 2);
	if (ret < 0)
		return ret;

	TrophyPowerLock powerLock;
	powerLock.Lock();

	if (m_indexStorage != NULL)
		m_indexStorage->Close();

	int dataRet = npTrophyRemoveUr0DataRoot(m_userId);
	int ur0ConfRet = npTrophyRemoveUr0ConfRoot(m_userId);
	int ux0ConfRet = npTrophyRemoveUx0ConfRoot(m_userId);

	if (dataRet >= 0 && ur0ConfRet >= 0 && ux0ConfRet >= 0) {
		TrophyDualTick nowTicks;
		nowTicks.Clear();
		npTrophyGetDualTick(&nowTicks);
		ret = m_indexStorage->Open(handle, GetAccessMode(), &nowTicks);
	} else if (dataRet < 0) {
		ret = dataRet;
	} else if (ur0ConfRet < 0) {
		ret = ur0ConfRet;
	} else {
		ret = ux0ConfRet;
	}

	powerLock.Unlock();
	return ret;
}

/* UserSession bridge functions declared in nptrophy_storage.h */
int npTrophyUserSessionEnsureReady(TrophyUserSession *session, TrophyHandle *handle, int mode)
{
	if (session == NULL)
		return NP_TROPHY_ERR_USER_SESSION_NOT_OPEN;
	return session->EnsureReady(handle, mode);
}

int npTrophyUserSessionGetUserId(const TrophyUserSession *session)
{
	return (session != NULL) ? session->GetUserId() : 0;
}

TrophyStorageArbiter *npTrophyUserSessionGetArbiter(const TrophyUserSession *session)
{
	return (session != NULL) ? session->GetArbiter() : NULL;
}

TrophyUserIndexStorage *npTrophyUserSessionGetIndexStorage(const TrophyUserSession *session)
{
	return (session != NULL) ? session->GetIndexStorage() : NULL;
}

TrophyNetClient *npTrophyUserSessionGetNetClient(const TrophyUserSession *session)
{
	return (session != NULL) ? session->GetNetClient() : NULL;
}

const SceKernelOpenPsId *npTrophyUserSessionGetOpenPsId(const TrophyUserSession *session)
{
	return (session != NULL) ? session->GetOpenPsId() : NULL;
}

int npTrophyUserSessionGetAccessMode(const TrophyUserSession *session)
{
	return (session != NULL) ? session->GetAccessMode() : 1;
}

bool npTrophyUserSessionIsWritable(const TrophyUserSession *session)
{
	return (session != NULL) ? session->IsWritable() : false;
}

int npTrophyUserSessionPurgeTitle(TrophyUserSession *session, TrophyHandle *handle,
				  const TrophyCommId *commId)
{
	if (session == NULL)
		return NP_TROPHY_ERR_USER_SESSION_NOT_OPEN;
	return session->PurgeTitle(handle, commId);
}

/* =========================================================================
 * TrophySyncController (0x812CF220..0x812CFADF)
 * ========================================================================= */

TrophySyncController::TrophySyncController(TrophyUserSession *session)
	: m_session(session)
{
}

TrophySyncController::~TrophySyncController()
{
}

int TrophySyncController::CheckNetSync(TrophyHandle *handle, uint64_t options, int *outNeedSync)
{
	(void)options;
	if (m_session == NULL || outNeedSync == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	*outNeedSync = 0;

	int ret = m_session->EnsureReady(handle, 1);
	if (ret < 0)
		return ret;

	if (!npTrophyNetIsSyncAvailable())
		return 0;

	TrophyUserIndexStorage *indexStorage = m_session->GetIndexStorage();
	bool hasPending = false;
	ret = indexStorage->HasPendingUploads(handle, &hasPending);
	if (ret < 0)
		return ret;
	if (hasPending) {
		*outNeedSync = 1;
		return 0;
	}

	uint64_t intervalSec = 0;
	TrophyNetClient *netClient = m_session->GetNetClient();
	if (netClient != NULL) {
		ret = netClient->GetSyncInterval(handle, &intervalSec, NULL);
		if (ret < 0)
			return ret;
	}

	SceRtcTick syncTicks[4];
	sceClibMemset(syncTicks, 0, sizeof(syncTicks));
	ret = indexStorage->GetSyncTicks(handle, syncTicks);
	if (ret < 0)
		return ret;

	SceRtcTick nowTick;
	nowTick.tick = 0;
	ret = npTrophyGetCurrentTick(&nowTick);
	if (ret < 0)
		return ret;

	uint64_t lastSuccess = syncTicks[2].tick;
	if (lastSuccess == 0 || nowTick.tick < lastSuccess ||
	    (nowTick.tick - lastSuccess) >= intervalSec * 1000000ULL) {
		*outNeedSync = 1;
	}

	return 0;
}

int TrophySyncController::UploadDirtyTitles(TrophyHandle *handle, uint32_t *outUploadedCount)
{
	if (outUploadedCount != NULL)
		*outUploadedCount = 0;

	TrophyUserIndexStorage *indexStorage = m_session->GetIndexStorage();
	uint32_t uploaded = 0;
	uint32_t cursor = 0;

	for (;;) {
		int ret = m_session->EnsureReady(handle, 2);
		if (ret < 0)
			return ret;

		uint32_t foundIndex = 0;
		uint8_t titleInfoBuf[104];
		uint8_t syncInfoBuf[8];
		ret = indexStorage->EnumerateTitle(handle, cursor, &foundIndex, titleInfoBuf,
						   syncInfoBuf);
		if (ret == NP_TROPHY_ERR_TITLE_NOT_FOUND)
			break;
		if (ret < 0)
			return ret;

		cursor = foundIndex + 1;

		TrophyCommId commId;
		sceClibMemcpy_safe(commId.data, titleInfoBuf, sizeof(commId.data));

		TrophyTitleRecord record;
		ret = indexStorage->GetTitle(handle, &commId, &record);
		if (ret < 0)
			continue;

		if (record.IsBroken() || !record.HasPendingUpload())
			continue;

		TrophyTitleStorage titleStorage(m_session, &commId);
		ret = titleStorage.Open(handle, 0x03u);
		if (ret < 0)
			continue;

		uint32_t syncFlags = 0;
		int platinumId = -1;
		ret = titleStorage.SyncUpload(handle, &syncFlags, &platinumId);
		titleStorage.Close();
		if (ret < 0)
			return ret;

		uploaded++;
	}

	if (outUploadedCount != NULL)
		*outUploadedCount = uploaded;
	return 0;
}

int TrophySyncController::InstallTitleFromNet(TrophyHandle *handle, const TrophyCommId *commId,
					      int lang)
{
	TrophyNetClient *netClient = m_session->GetNetClient();
	if (netClient == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyNetConfSource netSource;
	int ret = netSource.Open(handle, netClient, commId);
	if (ret < 0)
		return ret;

	TrophyTitleStorage titleStorage(m_session, commId);
	ret = titleStorage.InstallFromNetwork(handle, &netSource, lang);
	titleStorage.Close();
	netSource.Close();
	return ret;
}

int TrophySyncController::DownloadModifiedTitles(TrophyHandle *handle, const SceNpId *npId,
						 const SceRtcTick *sinceTick,
						 uint32_t *outUpdatedCount)
{
	if (outUpdatedCount != NULL)
		*outUpdatedCount = 0;

	TrophyNetClient *netClient = m_session->GetNetClient();
	if (netClient == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	int sysLang = 0;
	int ret = npTrophyGetSystemLanguage(&sysLang);
	if (ret < 0)
		sysLang = -1;

	TrophyNetTitleListFetcher fetcher;
	ret = fetcher.Init(npId, sinceTick);
	if (ret < 0)
		return ret;

	uint32_t updated = 0;
	TrophyUserIndexStorage *indexStorage = m_session->GetIndexStorage();

	for (;;) {
		ret = m_session->EnsureReady(handle, 2);
		if (ret < 0)
			return ret;

		ret = fetcher.FetchNextPage(handle, netClient);
		if (ret == NP_TROPHY_ERR_SERVER_NO_RESULTS)
			break;
		if (ret < 0)
			return ret;

		uint32_t pageCount = fetcher.GetPageCount();
		if (pageCount == 0)
			break;

		for (uint32_t i = 0; i < pageCount; ++i) {
			TrophyModifiedPrivacyEntry entry;
			ret = fetcher.GetEntry(static_cast<int>(i), &entry);
			if (ret < 0)
				return ret;

			bool exists = false;
			TrophyTitleRecord existingRecord;
			ret = indexStorage->HasTitle(handle, &entry.commId, &exists,
						     &existingRecord);
			if (ret < 0)
				return ret;

			if (!exists) {
				ret = InstallTitleFromNet(handle, &entry.commId, sysLang);
				if (ret < 0 && ret != NP_TROPHY_ERR_MAX_TITLES_REACHED &&
				    ret != NP_TROPHY_ERR_INSUFFICIENT_UR0_SPACE &&
				    ret != NP_TROPHY_ERR_INSUFFICIENT_UX0_SPACE) {
					return ret;
				}
				if (ret >= 0)
					updated++;
			} else if (!existingRecord.IsBroken()) {
				TrophyTitleStorage titleStorage(m_session, &entry.commId);
				ret = titleStorage.Open(handle, 0x03u);
				if (ret < 0)
					return ret;

				uint32_t syncFlags = 0;
				int platinumId = -1;
				ret = titleStorage.SyncDownload(handle, &syncFlags, &platinumId);
				titleStorage.Close();
				if (ret < 0)
					return ret;
				updated++;
			}
		}

		if (pageCount < 64u)
			break;
	}

	if (outUpdatedCount != NULL)
		*outUpdatedCount = updated;
	return 0;
}

int TrophySyncController::SyncIncremental(TrophyHandle *handle)
{
	int ret = m_session->EnsureReady(handle, 2);
	if (ret < 0)
		return ret;

	SceNpId npId;
	ret = TrophyTicketClient::FetchNpId(handle, &npId);
	if (ret < 0)
		return ret;

	SceRtcTick attemptTick;
	attemptTick.tick = 0;
	npTrophyGetCurrentTick(&attemptTick);

	TrophyUserIndexStorage *indexStorage = m_session->GetIndexStorage();
	SceRtcTick syncTicks[4];
	sceClibMemset(syncTicks, 0, sizeof(syncTicks));
	indexStorage->GetSyncTicks(handle, syncTicks);

	uint32_t uploaded = 0;
	ret = UploadDirtyTitles(handle, &uploaded);
	if (ret >= 0) {
		SceRtcTick sinceTick;
		sinceTick.tick = syncTicks[2].tick;
		uint32_t downloaded = 0;
		ret = DownloadModifiedTitles(handle, &npId,
					     (sinceTick.tick != 0) ? &sinceTick : NULL,
					     &downloaded);
	}

	SceRtcTick finishTick;
	finishTick.tick = 0;
	npTrophyGetCurrentTick(&finishTick);

	if (ret >= 0)
		indexStorage->RecordSyncSuccess(handle, &finishTick, 0);
	else
		indexStorage->RecordSyncAttempt(handle, &attemptTick, ret);

	return ret;
}

int TrophySyncController::SyncFull(TrophyHandle *handle)
{
	int ret = m_session->EnsureReady(handle, 2);
	if (ret < 0)
		return ret;

	SceNpId npId;
	ret = TrophyTicketClient::FetchNpId(handle, &npId);
	if (ret < 0)
		return ret;

	SceRtcTick attemptTick;
	attemptTick.tick = 0;
	npTrophyGetCurrentTick(&attemptTick);

	uint32_t uploaded = 0;
	ret = UploadDirtyTitles(handle, &uploaded);
	if (ret >= 0) {
		uint32_t downloaded = 0;
		ret = DownloadModifiedTitles(handle, &npId, NULL, &downloaded);
	}

	SceRtcTick finishTick;
	finishTick.tick = 0;
	npTrophyGetCurrentTick(&finishTick);

	TrophyUserIndexStorage *indexStorage = m_session->GetIndexStorage();
	if (ret >= 0)
		indexStorage->RecordSyncSuccess(handle, &finishTick, 0);
	else
		indexStorage->RecordSyncAttempt(handle, &attemptTick, ret);

	return ret;
}

int TrophySyncController::SyncTitles(TrophyHandle *handle, uint64_t options)
{
	if (m_session == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	if (!npTrophyNetIsSyncAvailable())
		return NP_TROPHY_ERR_GENERIC;

	if ((options & 1ULL) != 0)
		return SyncFull(handle);
	return SyncIncremental(handle);
}

/* =========================================================================
 * TrophyRecoveryController (0x812CFAE0..0x812D00BF)
 * ========================================================================= */

TrophyRecoveryController::TrophyRecoveryController(TrophyUserSession *session)
	: m_session(session)
{
}

TrophyRecoveryController::~TrophyRecoveryController()
{
}

int TrophyRecoveryController::CheckRecoveryRequired(TrophyHandle *handle, bool *outRequired)
{
	if (m_session == NULL || outRequired == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	*outRequired = false;

	int ret = m_session->EnsureReady(handle, 1);
	if (ret < 0)
		return ret;

	return m_session->GetIndexStorage()->GetNeedsRecovery(handle, outRequired);
}

int TrophyRecoveryController::RecoverSingleTitle(TrophyHandle *handle, const TrophyCommId *commId,
						 const TrophyTitleRecord *indexRecord)
{
	int userId = m_session->GetUserId();
	TrophyUserIndexStorage *indexStorage = m_session->GetIndexStorage();

	TrophyUserStorage userStorage(m_session->GetArbiter(), m_session->GetOpenPsId(), userId,
				      commId);
	TrophyConfStorage confStorage(userId, commId);

	bool userBroken = false;
	bool confBroken = false;
	userStorage.CheckBrokenMarker(handle, &userBroken);
	confStorage.CheckBrokenMarker(handle, &confBroken);

	int openUserRet = userStorage.Open(handle, 0x03u, 2);
	int openConfRet = confStorage.Open(handle);

	if (openUserRet < 0 || openConfRet < 0 || userBroken || confBroken) {
		userStorage.Close();
		confStorage.Close();

		if (indexRecord != NULL) {
			TrophyTitleRecord brokenRec = *indexRecord;
			if (openConfRet < 0 || confBroken)
				brokenRec.MarkBroken(1);
			if (openUserRet < 0 || userBroken)
				brokenRec.MarkBroken(2);
			return indexStorage->UpdateTitle(handle, &brokenRec, false);
		}
		m_session->PurgeTitle(handle, commId);
		return 0;
	}

	TrophyTitleRecord freshRecord;
	int ret = userStorage.ReadTitleRecord(handle, &freshRecord);
	userStorage.Close();
	confStorage.Close();

	if (ret < 0) {
		m_session->PurgeTitle(handle, commId);
		return 0;
	}

	if (indexRecord != NULL) {
		freshRecord.slotIndex = indexRecord->slotIndex;
		return indexStorage->UpdateTitle(handle, &freshRecord, false);
	}
	return indexStorage->AddTitle(handle, &freshRecord);
}

int TrophyRecoveryController::ScanAndRebuildUserIndex(TrophyHandle *handle)
{
	int userId = m_session->GetUserId();
	TrophyUserIndexStorage *indexStorage = m_session->GetIndexStorage();

	char dataRoot[256];
	int ret = npTrophyUr0DataRootPath(dataRoot, userId);
	if (ret < 0)
		return ret;

	TrophyDir dir;
	ret = dir.Open(dataRoot, false);
	if (ret == NP_TROPHY_ERR_FILE_NOT_FOUND)
		return 0;
	if (ret < 0)
		return ret;

	for (;;) {
		ret = m_session->EnsureReady(handle, 2);
		if (ret < 0)
			break;

		TrophyDirEntry entry;
		bool hasEntry = false;
		ret = dir.Read(&entry, &hasEntry);
		if (ret < 0 || !hasEntry)
			break;

		if (entry.kind != TROPHY_DIR_ENTRY_DIR)
			continue;

		TrophyCommId commId;
		if (!npTrophyParseCommDirName(entry.name, &commId))
			continue;

		bool exists = false;
		TrophyTitleRecord existingRec;
		ret = indexStorage->HasTitle(handle, &commId, &exists, &existingRec);
		if (ret < 0)
			break;

		ret = RecoverSingleTitle(handle, &commId, exists ? &existingRec : NULL);
		if (ret < 0)
			break;
	}

	dir.Close();
	return ret;
}

int TrophyRecoveryController::RecoverAllTitles(TrophyHandle *handle)
{
	if (m_session == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	int ret = m_session->EnsureReady(handle, 2);
	if (ret < 0)
		return ret;

	TrophyPowerLock powerLock;
	powerLock.Lock();

	ret = ScanAndRebuildUserIndex(handle);
	if (ret >= 0)
		ret = m_session->GetIndexStorage()->ClearNeedsRecovery(handle);

	powerLock.Unlock();
	return ret;
}

int TrophyRecoveryController::RecoverTitleIfExists(TrophyHandle *handle,
						   const TrophyCommId *commId)
{
	if (m_session == NULL || commId == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	int ret = m_session->EnsureReady(handle, 2);
	if (ret < 0)
		return ret;

	bool exists = false;
	TrophyTitleRecord existingRec;
	ret = m_session->GetIndexStorage()->HasTitle(handle, commId, &exists, &existingRec);
	if (ret < 0)
		return ret;
	if (!exists)
		return 0;

	return RecoverSingleTitle(handle, commId, &existingRec);
}
