/*
 * nptrophy_service.cpp: SceNpTrophyService, TrophyClientState, refcounted
 * object pools, TrophyContext, TrophyStreamWrapper, debug parameters, and
 * title-ID resolution (0x812C7F70..0x812CB693).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include "nptrophy_service.h"

namespace {

struct TrophyDebugParamEntry {
	const char *name;
	int value;
	int defaultValue;
};

/* 0x81545BE0: debug configuration parameters */
static TrophyDebugParamEntry s_debugParams[2] = {
	{ "disable_sync_in_game", 0, 0 },
	{ "fake_trial_flag", 0, 0 }
};

/* 0x81545BF8: MotorStorm RC trial-flag exemption list */
static const char s_trialExemptTitleIds[3][16] = {
	"PCSF00043",
	"PCSB00060",
	"PCSE00027"
};

struct TrophyAppStatusBuf {
	uint8_t header[12];
	char titleId[16];
	uint8_t extra[228];
};

struct TrophyDebugMarkBrokenArg {
	TrophyCommId commId;
	int brokenKind;
};

static int lookupDebugParamIndex(const char *name)
{
	if (name == NULL)
		return -1;
	for (int i = 0; i < 2; ++i) {
		if (sceClibStrncmp(s_debugParams[i].name, name, 64) == 0)
			return i;
	}
	return -1;
}

} /* namespace */

SceNpTrophyService g_npTrophyService;

/* =========================================================================
 * Title ID resolution (0x812CB034)
 * ========================================================================= */

int npTrophyResolvePidTitleId(SceUID pid, char *outTitleId)
{
	if (outTitleId == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyAppStatusBuf statusBuf;
	sceClibMemset(&statusBuf, 0, sizeof(statusBuf));

	int ret = sceAppMgrGetStatusById(pid, &statusBuf);
	if (ret < 0)
		return ret;

	sceClibMemset(outTitleId, 0, 16);
	if (sceClibStrncmp(statusBuf.titleId, "main", 16) == 0)
		npTrophyStrlcpy(outTitleId, "NPXS10999", 16);
	else
		npTrophyStrlcpy(outTitleId, statusBuf.titleId, 16);

	return 0;
}

/* =========================================================================
 * TrophyContext (0x812CB0A0..0x812CB4BF)
 * ========================================================================= */

TrophyContext::TrophyContext()
	: m_pid(-1),
	  m_commSign(NULL),
	  m_titleId(NULL),
	  m_trpConfHandle(NULL),
	  m_storageConfHandle(NULL),
	  m_installLang(-1),
	  m_displayLang(-1),
	  m_options(0)
{
	m_commId.Clear();
	sceClibMemset(&m_gradeBitsets, 0, sizeof(m_gradeBitsets));
}

TrophyContext::~TrophyContext()
{
	if (m_trpConfHandle != NULL) {
		delete m_trpConfHandle;
		m_trpConfHandle = NULL;
	}
	if (m_storageConfHandle != NULL) {
		delete m_storageConfHandle;
		m_storageConfHandle = NULL;
	}
	if (m_commSign != NULL) {
		npTrophyHeapFree(m_commSign);
		m_commSign = NULL;
	}
	if (m_titleId != NULL) {
		npTrophyHeapFree(m_titleId);
		m_titleId = NULL;
	}
}

bool TrophyContext::IsTrialExemptTitleId(const char *titleId)
{
	if (titleId == NULL)
		return false;
	for (int i = 0; i < 3; ++i) {
		if (sceClibStrncmp(titleId, s_trialExemptTitleIds[i], 16) == 0)
			return true;
	}
	return false;
}

int TrophyContext::Init(SceUID pid, const TrophyCommId *commId, const void *commSign,
			uint64_t options)
{
	if (commId == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	char resolvedTitleId[16];
	int ret = npTrophyResolvePidTitleId(pid, resolvedTitleId);
	if (ret < 0)
		return ret;

	int sysLang = 0;
	ret = npTrophyGetSystemLanguage(&sysLang);
	if (ret < 0)
		return ret;

	m_installLang = sysLang;
	m_displayLang = sysLang;
	m_commId = *commId;
	m_pid = pid;

	if (commSign != NULL) {
		ret = npTrophyVerifyCommSign(commId, commSign);
		if (ret < 0) {
			m_commId.Clear();
			return ret;
		}
		m_commSign = static_cast<uint8_t *>(npTrophyHeapAlloc(160));
		if (m_commSign == NULL) {
			m_commId.Clear();
			return NP_TROPHY_ERR_OUT_OF_MEMORY;
		}
		sceClibMemcpy_safe(m_commSign, commSign, 160);
	} else {
		m_commSign = NULL;
	}

	m_titleId = static_cast<char *>(npTrophyHeapAlloc(16));
	if (m_titleId == NULL) {
		if (m_commSign != NULL) {
			npTrophyHeapFree(m_commSign);
			m_commSign = NULL;
		}
		m_commId.Clear();
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	}
	sceClibMemcpy_safe(m_titleId, resolvedTitleId, 16);
	m_options = options;
	return 0;
}

int TrophyContext::GetServerTag(const void **outTag) const
{
	if (outTag == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	*outTag = m_commSign;
	return 0;
}

int TrophyContext::GetTrialFlag(uint8_t *outIsTrial) const
{
	if (outIsTrial == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	*outIsTrial = 0;
	if (IsTrialExemptTitleId(m_titleId))
		return 0;

	if (g_npTrophyService.IsFakeTrialFlagEnabled()) {
		*outIsTrial = 1;
		return 0;
	}

	int trialAttr = 0;
	int ret = sceAppMgrAppParamGetInt(m_pid, 13, &trialAttr);
	if (ret >= 0 && trialAttr != 0)
		*outIsTrial = 1;
	return 0;
}

int TrophyContext::GetUnlockAllowed(uint8_t *outAllowed) const
{
	if (outAllowed == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	*outAllowed = 1;
	int attrVal = 0;
	int ret = sceAppMgrAppParamGetInt(m_pid, 14, &attrVal);
	if (ret >= 0 && attrVal != 0)
		*outAllowed = 0;
	return 0;
}

int TrophyContext::GetOfflineFlag(uint8_t *outOffline) const
{
	if (outOffline == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	*outOffline = ((m_options & 1ULL) != 0) ? 1 : 0;
	return 0;
}

int TrophyContext::SetTrpConfHandle(TrophyConfHandle **ioHandle)
{
	if (ioHandle == NULL || *ioHandle == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	if (m_trpConfHandle != NULL)
		delete m_trpConfHandle;
	m_trpConfHandle = *ioHandle;
	*ioHandle = NULL;

	const TrophyConfData *conf = m_trpConfHandle->GetConfData();
	if (conf != NULL)
		conf->BuildGradeBitsets(&m_gradeBitsets);
	return 0;
}

int TrophyContext::SetStorageConfHandle(TrophyConfHandle **ioHandle)
{
	if (ioHandle == NULL || *ioHandle == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	if (m_storageConfHandle != NULL)
		delete m_storageConfHandle;
	m_storageConfHandle = *ioHandle;
	*ioHandle = NULL;
	return 0;
}

/* Context bridge functions declared in nptrophy_storage.h */
int npTrophyContextGetServerTag(const TrophyContext *context, const void **outTag)
{
	if (context == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	return context->GetServerTag(outTag);
}

int npTrophyContextGetTrialFlag(const TrophyContext *context, uint8_t *outIsTrial)
{
	if (context == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	return context->GetTrialFlag(outIsTrial);
}

int npTrophyContextGetUnlockAllowed(const TrophyContext *context, uint8_t *outAllowed)
{
	if (context == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	return context->GetUnlockAllowed(outAllowed);
}

int npTrophyContextGetOfflineFlag(const TrophyContext *context, uint8_t *outOffline)
{
	if (context == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	return context->GetOfflineFlag(outOffline);
}

const char *npTrophyContextGetTitleId(const TrophyContext *context)
{
	return (context != NULL) ? context->GetTitleId() : NULL;
}

/* =========================================================================
 * TrophyStreamWrapper (0x812CB4C0..0x812CB693)
 * ========================================================================= */

TrophyStreamWrapper::TrophyStreamWrapper()
	: m_file(NULL),
	  m_baseOffset(0),
	  m_positioned(0)
{
}

TrophyStreamWrapper::~TrophyStreamWrapper()
{
	Close();
}

int TrophyStreamWrapper::BindFile(TrophyFileBase *ownedFile, bool stripSfmHeader,
				  SceSize headerSize)
{
	if (ownedFile == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	if (m_file != NULL)
		return NP_TROPHY_ERR_FILE_ALREADY_OPEN;

	m_file = ownedFile;
	m_baseOffset = stripSfmHeader ? static_cast<SceOff>(headerSize) : 0;
	m_positioned = 0;
	return 0;
}

int TrophyStreamWrapper::Close()
{
	if (m_file == NULL)
		return 0;
	int ret = m_file->Close();
	delete m_file;
	m_file = NULL;
	m_baseOffset = 0;
	m_positioned = 0;
	return ret;
}

int TrophyStreamWrapper::GetReadableSize(SceUInt64 *outSize)
{
	if (m_file == NULL)
		return NP_TROPHY_ERR_FILE_NOT_OPEN;
	if (outSize == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	SceOff totalSize = 0;
	int ret = m_file->GetSize(&totalSize);
	if (ret < 0)
		return ret;

	if (totalSize < m_baseOffset)
		*outSize = 0;
	else
		*outSize = static_cast<SceUInt64>(totalSize - m_baseOffset);
	return 0;
}

int TrophyStreamWrapper::Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead)
{
	if (m_file == NULL)
		return NP_TROPHY_ERR_FILE_NOT_OPEN;

	if (m_positioned == 0) {
		if (m_baseOffset > 0) {
			int ret = m_file->Seek(handle, m_baseOffset, SCE_SEEK_SET, NULL);
			if (ret < 0)
				return ret;
		}
		m_positioned = 1;
	}

	return m_file->Read(handle, outBuf, size, outRead);
}

/* =========================================================================
 * Refcounted pool base & pooled object types (0x812C8030..0x812C86A7)
 * ========================================================================= */

TrophyPooledObject::TrophyPooledObject()
	: m_id(0),
	  m_autoEvict(0),
	  m_valid(1),
	  m_suspendedNotified(0),
	  m_suspendError(0),
	  m_registered(0),
	  m_refCount(0),
	  m_pool(NULL),
	  m_slotPtr(NULL)
{
}

TrophyPooledObject::~TrophyPooledObject()
{
	if (m_slotPtr != NULL) {
		*m_slotPtr = NULL;
		m_slotPtr = NULL;
	}
}

void TrophyPooledObject::AddRef()
{
	m_refCount++;
}

void TrophyPooledObject::ReleaseRef()
{
	if (m_pool != NULL)
		m_pool->Release(this);
}

TrophyPoolBase::TrophyPoolBase(TrophyLwMutex *mutex, int idBase, int idSpan, uint32_t capacity,
			       TrophyPooledObject **slots)
	: m_mutex(mutex),
	  m_idBase(idBase),
	  m_idSpan(idSpan),
	  m_nextIdOffset(1),
	  m_capacity(capacity),
	  m_slots(slots)
{
	for (uint32_t i = 0; i < m_capacity; ++i)
		m_slots[i] = NULL;
}

TrophyPoolBase::~TrophyPoolBase()
{
	RemoveAll();
}

int TrophyPoolBase::AllocateIdLocked()
{
	for (int attempt = 0; attempt < m_idSpan; ++attempt) {
		int candidate = m_idBase + m_nextIdOffset;
		m_nextIdOffset++;
		if (m_nextIdOffset >= m_idSpan)
			m_nextIdOffset = 1;

		bool inUse = false;
		for (uint32_t i = 0; i < m_capacity; ++i) {
			if (m_slots[i] != NULL && m_slots[i]->m_id == candidate) {
				inUse = true;
				break;
			}
		}
		if (!inUse)
			return candidate;
	}
	return 0;
}

void TrophyPoolBase::DestroyObjectLocked(TrophyPooledObject *obj)
{
	if (obj == NULL)
		return;
	obj->m_registered = 0;
	obj->OnRemoved();
	if (obj->m_slotPtr != NULL) {
		*obj->m_slotPtr = NULL;
		obj->m_slotPtr = NULL;
	}
	obj->m_pool = NULL;
	delete obj;
}

int TrophyPoolBase::Add(TrophyPooledObject *obj, int *outId)
{
	if (obj == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	m_mutex->Lock();

	int freeSlot = -1;
	for (uint32_t i = 0; i < m_capacity; ++i) {
		if (m_slots[i] == NULL) {
			freeSlot = static_cast<int>(i);
			break;
		}
	}

	if (freeSlot < 0) {
		for (uint32_t i = 0; i < m_capacity; ++i) {
			if (m_slots[i] != NULL && m_slots[i]->m_autoEvict != 0 &&
			    m_slots[i]->m_refCount == 0) {
				DestroyObjectLocked(m_slots[i]);
				freeSlot = static_cast<int>(i);
				break;
			}
		}
	}

	if (freeSlot < 0) {
		m_mutex->Unlock();
		return NP_TROPHY_ERR_HANDLE_LIMIT;
	}

	int newId = AllocateIdLocked();
	if (newId == 0) {
		m_mutex->Unlock();
		return NP_TROPHY_ERR_HANDLE_LIMIT;
	}

	obj->m_id = newId;
	obj->m_pool = this;
	obj->m_slotPtr = &m_slots[freeSlot];
	obj->m_registered = 1;
	obj->m_valid = 1;
	obj->m_refCount = 0;
	m_slots[freeSlot] = obj;
	obj->OnAdded();

	if (outId != NULL)
		*outId = newId;

	m_mutex->Unlock();
	return 0;
}

int TrophyPoolBase::AcquireById(int id, TrophyPooledObject **outObj)
{
	if (outObj == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	*outObj = NULL;

	m_mutex->Lock();
	for (uint32_t i = 0; i < m_capacity; ++i) {
		TrophyPooledObject *obj = m_slots[i];
		if (obj != NULL && obj->m_id == id && obj->m_registered != 0 &&
		    obj->m_valid != 0) {
			obj->m_refCount++;
			*outObj = obj;
			m_mutex->Unlock();
			return 0;
		}
	}
	m_mutex->Unlock();
	return NP_TROPHY_ERR_INVALID_HANDLE;
}

void TrophyPoolBase::Release(TrophyPooledObject *obj)
{
	if (obj == NULL)
		return;

	m_mutex->Lock();
	if (obj->m_refCount > 0)
		obj->m_refCount--;

	if (obj->m_refCount == 0 && obj->m_registered == 0)
		DestroyObjectLocked(obj);
	m_mutex->Unlock();
}

int TrophyPoolBase::RemoveById(int id)
{
	m_mutex->Lock();
	for (uint32_t i = 0; i < m_capacity; ++i) {
		TrophyPooledObject *obj = m_slots[i];
		if (obj != NULL && obj->m_id == id && obj->m_registered != 0) {
			obj->m_registered = 0;
			obj->m_valid = 0;
			if (obj->m_refCount == 0)
				DestroyObjectLocked(obj);
			m_mutex->Unlock();
			return 0;
		}
	}
	m_mutex->Unlock();
	return NP_TROPHY_ERR_INVALID_HANDLE;
}

void TrophyPoolBase::RemoveAll()
{
	m_mutex->Lock();
	for (uint32_t i = 0; i < m_capacity; ++i) {
		TrophyPooledObject *obj = m_slots[i];
		if (obj == NULL)
			continue;
		obj->m_registered = 0;
		obj->m_valid = 0;
		if (obj->m_refCount == 0)
			DestroyObjectLocked(obj);
	}
	m_mutex->Unlock();
}

void TrophyPoolBase::SuspendAll(int suspendError)
{
	m_mutex->Lock();
	for (uint32_t i = 0; i < m_capacity; ++i) {
		TrophyPooledObject *obj = m_slots[i];
		if (obj == NULL)
			continue;
		obj->m_suspendError = suspendError;
		obj->m_suspendedNotified = 1;
		obj->OnSuspended(suspendError);
	}
	m_mutex->Unlock();
}

void TrophyPoolBase::ResumeAll()
{
	m_mutex->Lock();
	for (uint32_t i = 0; i < m_capacity; ++i) {
		TrophyPooledObject *obj = m_slots[i];
		if (obj == NULL)
			continue;
		obj->m_suspendError = 0;
		obj->m_suspendedNotified = 0;
	}
	m_mutex->Unlock();
}

void TrophyPoolBase::EvictAutoEvictable()
{
	m_mutex->Lock();
	for (uint32_t i = 0; i < m_capacity; ++i) {
		TrophyPooledObject *obj = m_slots[i];
		if (obj == NULL || obj->m_autoEvict == 0)
			continue;
		obj->m_registered = 0;
		obj->m_valid = 0;
		if (obj->m_refCount == 0)
			DestroyObjectLocked(obj);
	}
	m_mutex->Unlock();
}

TrophyPooledObject *TrophyPoolBase::GetSlot(uint32_t index) const
{
	if (index >= m_capacity)
		return NULL;
	return m_slots[index];
}

/* =========================================================================
 * Concrete pooled object wrappers (0x812C8030..0x812C86A7)
 * ========================================================================= */

TrophyHandlePoolObj::TrophyHandlePoolObj()
	: m_handleType(0),
	  m_handle(NULL)
{
}

TrophyHandlePoolObj::~TrophyHandlePoolObj()
{
	if (m_handle != NULL) {
		m_handle->Term();
		delete m_handle;
		m_handle = NULL;
	}
}

int TrophyHandlePoolObj::Init(int handleType)
{
	m_handle = new TrophyHandle();
	if (m_handle == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	int ret = m_handle->Init();
	if (ret < 0) {
		delete m_handle;
		m_handle = NULL;
		return ret;
	}
	m_handleType = handleType;
	return 0;
}

void TrophyHandlePoolObj::OnSuspended(int suspendError)
{
	if (m_handle != NULL)
		m_handle->Cancel(suspendError);
}

TrophyContextPoolObj::TrophyContextPoolObj()
	: m_titleIdMode(0),
	  m_context(NULL),
	  m_registeredFlag(0),
	  m_trpLoadedFlag(0)
{
}

TrophyContextPoolObj::~TrophyContextPoolObj()
{
	if (m_context != NULL) {
		delete m_context;
		m_context = NULL;
	}
}

int TrophyContextPoolObj::Init(int titleIdMode, SceUID pid, const TrophyCommId *commId,
			       const void *commSign, uint64_t options)
{
	m_context = new TrophyContext();
	if (m_context == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	int ret = m_context->Init(pid, commId, commSign, options);
	if (ret < 0) {
		delete m_context;
		m_context = NULL;
		return ret;
	}

	m_titleIdMode = titleIdMode;
	return 0;
}

bool TrophyContextPoolObj::CanWriteStorage() const
{
	if (m_titleIdMode == 1)
		return false;
	return (m_context != NULL) && m_context->HasCommSign();
}

int TrophyContextPoolObj::MarkRegistered()
{
	if (m_context == NULL)
		return NP_TROPHY_ERR_INVALID_CONTEXT;
	m_registeredFlag = 1;
	return 0;
}

int TrophyContextPoolObj::MarkTrpLoaded()
{
	m_trpLoadedFlag = 1;
	return 0;
}

bool TrophyContextPoolObj::IsRegisteredAndLoaded() const
{
	return m_registeredFlag != 0 && m_trpLoadedFlag != 0;
}

int TrophyContextPoolObj::EnsureTrpConfLoaded(TrophyHandle *handle, TrophyTrpConfSource *trpSource)
{
	if (m_context == NULL || trpSource == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	if (m_context->GetTrpConfHandle() != NULL)
		return 0;

	TrophyConfHandle *confHandle = NULL;
	int ret = trpSource->ReadConfHandle(handle, m_context->GetDisplayLang(), &confHandle);
	if (ret < 0) {
		if (confHandle != NULL)
			delete confHandle;
		return ret;
	}
	return m_context->SetTrpConfHandle(&confHandle);
}

int TrophyContextPoolObj::EnsureStorageConfLoaded(TrophyHandle *handle,
						  TrophyTitleStorage *titleStorage)
{
	if (m_context == NULL || titleStorage == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	if (m_context->GetStorageConfHandle() != NULL)
		return 0;

	TrophyConfHandle *confHandle = NULL;
	int ret = titleStorage->ReadConfHandle(handle, &confHandle);
	if (ret < 0) {
		if (confHandle != NULL)
			delete confHandle;
		return ret;
	}
	return m_context->SetStorageConfHandle(&confHandle);
}

TrophyTitleSessionPoolObj::TrophyTitleSessionPoolObj()
	: m_userSession(NULL)
{
}

TrophyTitleSessionPoolObj::~TrophyTitleSessionPoolObj()
{
	OnRemoved();
}

int TrophyTitleSessionPoolObj::Init(TrophyHandle *handle, TrophyStorageArbiter *arbiter,
				    int userId, uint32_t accessFlags)
{
	m_userSession = new TrophyUserSession();
	if (m_userSession == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	int ret = m_userSession->Open(handle, arbiter, userId, accessFlags);
	if (ret < 0) {
		delete m_userSession;
		m_userSession = NULL;
		return ret;
	}
	return 0;
}

void TrophyTitleSessionPoolObj::OnRemoved()
{
	if (m_userSession != NULL) {
		m_userSession->Close();
		delete m_userSession;
		m_userSession = NULL;
	}
}

TrophyTitleStoragePoolObj::TrophyTitleStoragePoolObj()
	: m_titleStorage(NULL),
	  m_ownerSession(NULL)
{
	m_commId.Clear();
	SetAutoEvict(true);
}

TrophyTitleStoragePoolObj::~TrophyTitleStoragePoolObj()
{
	OnRemoved();
}

int TrophyTitleStoragePoolObj::Init(TrophyUserSession *session, const TrophyCommId *commId)
{
	if (session == NULL || commId == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	m_titleStorage = new TrophyTitleStorage(session, commId);
	if (m_titleStorage == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	m_commId = *commId;
	m_ownerSession = session;
	return 0;
}

void TrophyTitleStoragePoolObj::OnRemoved()
{
	if (m_titleStorage != NULL) {
		m_titleStorage->Close();
		delete m_titleStorage;
		m_titleStorage = NULL;
	}
	m_ownerSession = NULL;
}

TrophyConfSourcePoolObj::TrophyConfSourcePoolObj()
	: m_confSource(NULL)
{
	m_commId.Clear();
	SetAutoEvict(true);
}

TrophyConfSourcePoolObj::~TrophyConfSourcePoolObj()
{
	OnRemoved();
}

int TrophyConfSourcePoolObj::Init(const TrophyCommId *commId)
{
	if (commId == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	m_confSource = new TrophyTrpConfSource();
	if (m_confSource == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	m_commId = *commId;
	return 0;
}

void TrophyConfSourcePoolObj::OnRemoved()
{
	if (m_confSource != NULL) {
		m_confSource->Close();
		delete m_confSource;
		m_confSource = NULL;
	}
}

TrophyStreamPoolObj::TrophyStreamPoolObj()
	: m_stream(NULL),
	  m_heldStorage(NULL),
	  m_heldConfSource(NULL)
{
}

TrophyStreamPoolObj::~TrophyStreamPoolObj()
{
	OnRemoved();
}

int TrophyStreamPoolObj::Init(TrophyStreamWrapper *stream, TrophyTitleStoragePoolObj *heldStorage,
			      TrophyConfSourcePoolObj *heldConfSource)
{
	if (stream == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	m_stream = stream;
	m_heldStorage = heldStorage;
	m_heldConfSource = heldConfSource;
	return 0;
}

void TrophyStreamPoolObj::OnRemoved()
{
	if (m_stream != NULL) {
		m_stream->Close();
		delete m_stream;
		m_stream = NULL;
	}
	if (m_heldStorage != NULL) {
		m_heldStorage->ReleaseRef();
		m_heldStorage = NULL;
	}
	if (m_heldConfSource != NULL) {
		m_heldConfSource->ReleaseRef();
		m_heldConfSource = NULL;
	}
}

/* =========================================================================
 * TrophyClientState (0x812C86A8..0x812C9FCC)
 * ========================================================================= */

TrophyClientState::TrophyClientState(TrophyLwMutex *mutex)
	: m_active(0),
	  m_pid(-1),
	  m_session(NULL),
	  m_handlePool(mutex, TROPHY_ID_BASE_HANDLE, TROPHY_ID_SPAN, 16, m_handleSlots),
	  m_contextPool(mutex, TROPHY_ID_BASE_CONTEXT, TROPHY_ID_SPAN, 4, m_contextSlots),
	  m_titleSessionPool(mutex, TROPHY_ID_BASE_TITLE_SESSION, TROPHY_ID_SPAN, 16,
			     m_titleSessionSlots),
	  m_titleStoragePool(mutex, TROPHY_ID_BASE_TITLE_STORAGE, TROPHY_ID_SPAN, 16,
			     m_titleStorageSlots),
	  m_confSourcePool(mutex, TROPHY_ID_BASE_CONF_SOURCE, TROPHY_ID_SPAN, 16,
			   m_confSourceSlots),
	  m_streamPool(mutex, TROPHY_ID_BASE_STREAM, TROPHY_ID_SPAN, 4, m_streamSlots)
{
}

TrophyClientState::~TrophyClientState()
{
	Disconnect(NP_TROPHY_ERR_CLIENT_CLOSED);
}

int TrophyClientState::Connect(SceUID pid, IPMI::Session *session)
{
	m_pid = pid;
	m_session = session;
	m_active = 1;
	return 0;
}

void TrophyClientState::Disconnect(int reason)
{
	if (m_active == 0)
		return;

	m_handlePool.SuspendAll(reason);
	m_streamPool.RemoveAll();
	m_confSourcePool.RemoveAll();
	m_titleStoragePool.RemoveAll();
	m_titleSessionPool.RemoveAll();
	m_contextPool.RemoveAll();
	m_handlePool.RemoveAll();
	m_mount.Unmount();

	m_pid = -1;
	m_session = NULL;
	m_active = 0;
}

void TrophyClientState::Suspend(int reason)
{
	if (m_active == 0)
		return;

	m_handlePool.SuspendAll(reason);
	m_streamPool.RemoveAll();
	m_confSourcePool.EvictAutoEvictable();
	m_titleStoragePool.EvictAutoEvictable();
}

void TrophyClientState::Resume()
{
	if (m_active == 0)
		return;

	m_handlePool.ResumeAll();
	for (uint32_t i = 0; i < m_handlePool.GetCapacity(); ++i) {
		TrophyHandlePoolObj *obj =
			static_cast<TrophyHandlePoolObj *>(m_handlePool.GetSlot(i));
		if (obj != NULL && obj->GetHandle() != NULL)
			obj->GetHandle()->ClearSuspended();
	}
}

int TrophyClientState::EnsureMounted()
{
	return m_mount.MountForPid(m_pid);
}

int TrophyClientState::AcquireHandle(int handleId, TrophyHandlePoolObj **outObj)
{
	TrophyPooledObject *baseObj = NULL;
	int ret = m_handlePool.AcquireById(handleId, &baseObj);
	if (ret < 0)
		return NP_TROPHY_ERR_INVALID_HANDLE;
	*outObj = static_cast<TrophyHandlePoolObj *>(baseObj);
	return 0;
}

int TrophyClientState::AcquireContext(int ctxId, TrophyContextPoolObj **outObj)
{
	TrophyPooledObject *baseObj = NULL;
	int ret = m_contextPool.AcquireById(ctxId, &baseObj);
	if (ret < 0)
		return NP_TROPHY_ERR_INVALID_CONTEXT;
	*outObj = static_cast<TrophyContextPoolObj *>(baseObj);
	return 0;
}

int TrophyClientState::AcquireTitleSession(int sessionId, TrophyTitleSessionPoolObj **outObj)
{
	TrophyPooledObject *baseObj = NULL;
	int ret = m_titleSessionPool.AcquireById(sessionId, &baseObj);
	if (ret < 0)
		return NP_TROPHY_ERR_INVALID_TITLE_SESSION;
	*outObj = static_cast<TrophyTitleSessionPoolObj *>(baseObj);
	return 0;
}

int TrophyClientState::AcquireOrCreateTitleStorage(TrophyTitleSessionPoolObj *titleSessionObj,
						   const TrophyCommId *commId,
						   TrophyTitleStoragePoolObj **outObj)
{
	if (titleSessionObj == NULL || commId == NULL || outObj == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	*outObj = NULL;
	TrophyUserSession *userSession = titleSessionObj->GetUserSession();

	TrophyLwMutex *mutex = g_npTrophyService.GetMutex();
	mutex->Lock();

	for (uint32_t i = 0; i < m_titleStoragePool.GetCapacity(); ++i) {
		TrophyTitleStoragePoolObj *entry =
			static_cast<TrophyTitleStoragePoolObj *>(m_titleStoragePool.GetSlot(i));
		if (entry != NULL && entry->IsValid() && entry->GetOwnerSession() == userSession &&
		    entry->GetCommId().Equals(*commId)) {
			entry->AddRef();
			*outObj = entry;
			mutex->Unlock();
			return 0;
		}
	}

	mutex->Unlock();

	TrophyTitleStoragePoolObj *created = new TrophyTitleStoragePoolObj();
	if (created == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	int ret = created->Init(userSession, commId);
	if (ret < 0) {
		delete created;
		return ret;
	}

	int storageId = 0;
	ret = m_titleStoragePool.Add(created, &storageId);
	if (ret < 0) {
		delete created;
		return ret;
	}

	TrophyPooledObject *acquired = NULL;
	ret = m_titleStoragePool.AcquireById(storageId, &acquired);
	if (ret < 0)
		return ret;

	*outObj = static_cast<TrophyTitleStoragePoolObj *>(acquired);
	return 0;
}

int TrophyClientState::AcquireOrCreateConfSource(const TrophyCommId *commId,
						 TrophyConfSourcePoolObj **outObj)
{
	if (commId == NULL || outObj == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	*outObj = NULL;

	TrophyLwMutex *mutex = g_npTrophyService.GetMutex();
	mutex->Lock();

	for (uint32_t i = 0; i < m_confSourcePool.GetCapacity(); ++i) {
		TrophyConfSourcePoolObj *entry =
			static_cast<TrophyConfSourcePoolObj *>(m_confSourcePool.GetSlot(i));
		if (entry != NULL && entry->IsValid() && entry->GetCommId().Equals(*commId)) {
			entry->AddRef();
			*outObj = entry;
			mutex->Unlock();
			return 0;
		}
	}

	mutex->Unlock();

	TrophyConfSourcePoolObj *created = new TrophyConfSourcePoolObj();
	if (created == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	int ret = created->Init(commId);
	if (ret < 0) {
		delete created;
		return ret;
	}

	int sourceId = 0;
	ret = m_confSourcePool.Add(created, &sourceId);
	if (ret < 0) {
		delete created;
		return ret;
	}

	TrophyPooledObject *acquired = NULL;
	ret = m_confSourcePool.AcquireById(sourceId, &acquired);
	if (ret < 0)
		return ret;

	*outObj = static_cast<TrophyConfSourcePoolObj *>(acquired);
	return 0;
}

int TrophyClientState::AcquireStream(int streamId, TrophyStreamPoolObj **outObj)
{
	TrophyPooledObject *baseObj = NULL;
	int ret = m_streamPool.AcquireById(streamId, &baseObj);
	if (ret < 0)
		return NP_TROPHY_ERR_INVALID_STREAM;
	*outObj = static_cast<TrophyStreamPoolObj *>(baseObj);
	return 0;
}

void TrophyClientState::EvictTitleStorageForCommId(const TrophyCommId *commId)
{
	if (commId == NULL)
		return;

	TrophyLwMutex *mutex = g_npTrophyService.GetMutex();
	mutex->Lock();

	for (uint32_t i = 0; i < m_titleStoragePool.GetCapacity(); ++i) {
		TrophyTitleStoragePoolObj *entry =
			static_cast<TrophyTitleStoragePoolObj *>(m_titleStoragePool.GetSlot(i));
		if (entry != NULL && entry->GetCommId().Equals(*commId))
			m_titleStoragePool.RemoveById(entry->GetId());
	}

	mutex->Unlock();
}

void TrophyClientState::EvictAllTitleStorage()
{
	m_titleStoragePool.RemoveAll();
}

/* =========================================================================
 * SceNpTrophyService (0x812C7F70..0x812C802F, 0x812CAE1C..0x812CB033)
 * ========================================================================= */

SceNpTrophyService::SceNpTrophyService()
	: m_initialized(0)
{
	sceClibMemset(m_clients, 0, sizeof(m_clients));
}

SceNpTrophyService::~SceNpTrophyService()
{
	Term();
}

int SceNpTrophyService::Init()
{
	if (m_initialized != 0)
		return NP_TROPHY_ERR_ALREADY_INITIALIZED;

	int ret = m_mutex.Init("SceNpTrophyService", true);
	if (ret < 0)
		return ret;

	ret = g_trophyStorageArbiter.Init();
	if (ret < 0) {
		m_mutex.Term();
		return ret;
	}

	sceClibMemset(m_clients, 0, sizeof(m_clients));
	m_initialized = 1;
	return 0;
}

int SceNpTrophyService::Term()
{
	if (m_initialized == 0)
		return NP_TROPHY_ERR_NOT_INITIALIZED;

	m_mutex.Lock();
	for (int i = 0; i < 8; ++i) {
		if (m_clients[i] != NULL) {
			m_clients[i]->Disconnect(NP_TROPHY_ERR_SERVER_STOPPED);
			delete m_clients[i];
			m_clients[i] = NULL;
		}
	}
	m_mutex.Unlock();

	g_trophyStorageArbiter.Term();
	m_mutex.Term();
	m_initialized = 0;
	return 0;
}

TrophyClientState *SceNpTrophyService::FindClientLocked(SceUID pid, IPMI::Session *session)
{
	for (int i = 0; i < 8; ++i) {
		TrophyClientState *client = m_clients[i];
		if (client != NULL && client->IsActive() && client->GetPid() == pid &&
		    client->GetSession() == session) {
			return client;
		}
	}
	return NULL;
}

int SceNpTrophyService::OnClientConnect(SceUID pid, IPMI::Session *session)
{
	if (m_initialized == 0)
		return NP_TROPHY_ERR_NOT_INITIALIZED;

	m_mutex.Lock();

	if (FindClientLocked(pid, session) != NULL) {
		m_mutex.Unlock();
		return 0;
	}

	int freeSlot = -1;
	for (int i = 0; i < 8; ++i) {
		if (m_clients[i] == NULL) {
			freeSlot = i;
			break;
		}
	}

	if (freeSlot < 0) {
		m_mutex.Unlock();
		return NP_TROPHY_ERR_CLIENT_LIMIT;
	}

	TrophyClientState *client = new TrophyClientState(&m_mutex);
	if (client == NULL) {
		m_mutex.Unlock();
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	}

	client->Connect(pid, session);
	m_clients[freeSlot] = client;

	m_mutex.Unlock();
	return 0;
}

int SceNpTrophyService::OnClientDisconnect(SceUID pid, IPMI::Session *session, int reason)
{
	if (m_initialized == 0)
		return NP_TROPHY_ERR_NOT_INITIALIZED;

	m_mutex.Lock();

	for (int i = 0; i < 8; ++i) {
		TrophyClientState *client = m_clients[i];
		if (client != NULL && client->IsActive() && client->GetPid() == pid &&
		    client->GetSession() == session) {
			m_clients[i] = NULL;
			m_mutex.Unlock();
			client->Disconnect(reason);
			delete client;
			return 0;
		}
	}

	m_mutex.Unlock();
	return NP_TROPHY_ERR_CLIENT_NOT_FOUND;
}

int SceNpTrophyService::SuspendPid(SceUID pid)
{
	if (m_initialized == 0)
		return NP_TROPHY_ERR_NOT_INITIALIZED;

	m_mutex.Lock();
	for (int i = 0; i < 8; ++i) {
		TrophyClientState *client = m_clients[i];
		if (client != NULL && client->IsActive() && client->GetPid() == pid)
			client->Suspend(NP_TROPHY_ERR_SUSPENDED);
	}
	m_mutex.Unlock();
	return 0;
}

int SceNpTrophyService::ResumePid(SceUID pid)
{
	if (m_initialized == 0)
		return NP_TROPHY_ERR_NOT_INITIALIZED;

	m_mutex.Lock();
	for (int i = 0; i < 8; ++i) {
		TrophyClientState *client = m_clients[i];
		if (client != NULL && client->IsActive() && client->GetPid() == pid)
			client->Resume();
	}
	m_mutex.Unlock();
	return 0;
}

int SceNpTrophyService::FindClient(SceUID pid, IPMI::Session *session,
				   TrophyClientState **outClient)
{
	if (m_initialized == 0)
		return NP_TROPHY_ERR_NOT_INITIALIZED;
	if (outClient == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	m_mutex.Lock();
	TrophyClientState *client = FindClientLocked(pid, session);
	m_mutex.Unlock();

	if (client == NULL)
		return NP_TROPHY_ERR_CLIENT_NOT_FOUND;

	*outClient = client;
	return 0;
}

/* 0x812CAE1C */
int SceNpTrophyService::SetDebugParam(const char *name, const void *buf, SceSize size)
{
	if (name == NULL || buf == NULL || size != sizeof(int))
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	int idx = lookupDebugParamIndex(name);
	if (idx < 0)
		return NP_TROPHY_ERR_DEBUG_PARAM_NOT_FOUND;

	m_mutex.Lock();
	sceClibMemcpy_safe(&s_debugParams[idx].value, buf, sizeof(int));
	m_mutex.Unlock();
	return 0;
}

/* 0x812CAE8C */
int SceNpTrophyService::GetDebugParam(const char *name, void *outBuf, SceSize size)
{
	if (name == NULL || outBuf == NULL || size != sizeof(int))
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	int idx = lookupDebugParamIndex(name);
	if (idx < 0)
		return NP_TROPHY_ERR_DEBUG_PARAM_NOT_FOUND;

	m_mutex.Lock();
	sceClibMemcpy_safe(outBuf, &s_debugParams[idx].value, sizeof(int));
	m_mutex.Unlock();
	return 0;
}

/* 0x812CAEFC, 0x812CF130 */
int SceNpTrophyService::ExecDebugOp(int op, const void *arg, SceSize argSize)
{
	if (op == 0) {
		m_mutex.Lock();
		for (int i = 0; i < 2; ++i)
			s_debugParams[i].value = s_debugParams[i].defaultValue;
		m_mutex.Unlock();
		return 0;
	}

	if (op == 1) {
		if (arg == NULL || argSize != sizeof(TrophyDebugMarkBrokenArg))
			return NP_TROPHY_ERR_INVALID_ARGUMENT;

		const TrophyDebugMarkBrokenArg *brokenArg =
			static_cast<const TrophyDebugMarkBrokenArg *>(arg);

		TrophyHandle tempHandle;
		int ret = tempHandle.Init();
		if (ret < 0)
			return ret;

		TrophyUserSession tempSession;
		ret = tempSession.Open(&tempHandle, &g_trophyStorageArbiter, 0, 0x00230002u);
		if (ret >= 0) {
			TrophyTitleStorage titleStorage(&tempSession, &brokenArg->commId);
			ret = titleStorage.MarkBroken(&tempHandle, brokenArg->brokenKind);
			titleStorage.Close();
			tempSession.Close();
		}
		tempHandle.Term();
		return ret;
	}

	return NP_TROPHY_ERR_INVALID_ARGUMENT;
}

bool SceNpTrophyService::IsSyncInGameDisabled() const
{
	return s_debugParams[0].value != 0;
}

bool SceNpTrophyService::IsFakeTrialFlagEnabled() const
{
	return s_debugParams[1].value != 0;
}

/* Service bridge function declared in nptrophy_storage.h */
bool npTrophyServiceIsSyncInGameDisabled()
{
	return g_npTrophyService.IsSyncInGameDisabled();
}
