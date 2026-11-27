/*
 * np_friend_importer.cpp: SceNpFriendImporter background thread, IMPS/NetCtl event
 * handlers, friend/blocked list sync events, livearea friend notifications, and
 * NpFriendPlayerImpl.
 * 3.65-CEX: 0x813C541C..0x813C7937, 0x813C84FC..0x813C85CF
 */
#include "np_friend_importer_internal.h"
#include "shell.h"
#include "src/livespace_db/livespace_db_internal.h"
#include "src/np/imps/np_imps_svc.h"
#include <np/np_utility/np_utility_internal.h>

extern "C" {
extern int SceShell_2E0B6FA6(void (*callback)(int prevState, int state, int eventType,
					      int errorCode, int detail, void *userArg),
			     void *userArg, int *outId);
extern int SceShell_B4D888D5(int callbackId);
}

/* IMPS VSH service bridges (src/np/imps/np_imps_util_server.cpp) */
extern int npImpsSvcGetFriendEntryRelation(NpImpsCacheFriendPtr *handle, int *outRelation);
extern int npImpsSvcGetFriendEntryDate(NpImpsCacheFriendPtr *handle, paf::rtc::Tick *outTick);
extern int npImpsSvcGetFriendEntryRequestDirection(NpImpsCacheFriendPtr *handle, int *outDir);
extern int npImpsSvcRegisterListener(const NpImpsSvcListenerCallbacks *cb, SceUInt32 *outId);
extern int npImpsSvcUnregisterListener(SceUInt32 listenerId);

/* Importer-queued snapshot of one friend/blocked entry (56 bytes) */
struct NpFriendListRecord {
	SceNpId npId;
	int playerType;
	paf::rtc::Tick requestDate;
	int requestDirection;
};

typedef paf::list<NpFriendListRecord, NpFriendImporterAllocator<NpFriendListRecord> >
	NpFriendRecordList;

/* ============================================================================
 * Importer Private Heap (0x813C7378..0x813C73DF)
 * ============================================================================ */

static paf::memory::HeapAllocator *s_importerHeap = NULL;	/* 0x815CC3D0 */
static NpFriendImporter *s_importerInstance = NULL;		/* 0x8154933C */

/* 0x813C7378 */
void npFriendImporterSetHeap(paf::memory::HeapAllocator *heap)
{
	s_importerHeap = heap;
}

/* 0x813C7384 */
void npFriendImporterClearHeap(void)
{
	s_importerHeap = NULL;
}

/* 0x813C7392 */
void *npFriendImporterMalloc(SceSize size)
{
	if (s_importerHeap != NULL)
		return s_importerHeap->sce_paf_malloc(size);
	return NULL;
}

/* 0x813C73AE */
void npFriendImporterFree(void *pointer)
{
	if (s_importerHeap != NULL)
		s_importerHeap->sce_paf_free(pointer);
}

/* 0x813C73C4 */
SceSize npFriendImporterGetFreeSize(void)
{
	if (s_importerHeap != NULL)
		return s_importerHeap->GetFreeSize();
	return 0;
}

/* ============================================================================
 * NpFriendPlayerImpl (0x813C84FC..0x813C85CF)
 * ============================================================================ */

NpFriendPlayerImpl::NpFriendPlayerImpl(const SceNpId &npId, int playerType,
				       const paf::rtc::Tick &requestDate, int requestDirection)
	: m_npId(npId),
	  m_playerType(playerType),
	  m_requestDate(requestDate),
	  m_requestDirection(requestDirection)
{
}

NpFriendPlayerImpl::~NpFriendPlayerImpl()
{
}

const SceNpId *NpFriendPlayerImpl::GetNpId()
{
	return &m_npId;
}

const char *NpFriendPlayerImpl::GetOnlineId()
{
	return m_npId.handle.data;
}

int NpFriendPlayerImpl::GetPlayerType()
{
	return m_playerType;
}

const paf::rtc::Tick *NpFriendPlayerImpl::GetFriendRequestDate()
{
	return &m_requestDate;
}

int NpFriendPlayerImpl::GetRequestDirection()
{
	return m_requestDirection;
}

/* 0x813C84FC */
int NpFriendPlayerImpl::Create(paf::intrusive_ptr<NpFriendPlayerImpl> *outPlayer,
			       const SceNpId *npId, int playerType,
			       const paf::rtc::Tick *requestDate, int requestDirection)
{
	void *raw = npFriendStorageMalloc(sizeof(NpFriendPlayerImpl));
	NpFriendPlayerImpl *created = NULL;
	if (raw != NULL)
		created = new (raw) NpFriendPlayerImpl(*npId, playerType, *requestDate,
						       requestDirection);

	*outPlayer = paf::intrusive_ptr<NpFriendPlayerImpl>(created);
	return (outPlayer->get() != NULL) ? 0 : NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;
}

/* ============================================================================
 * Player Array Helpers (0x813C7548..0x813C7937)
 * ============================================================================ */

static int MapImpsRelationToPlayerType(int relation, bool allowZero, int *outType)
{
	if (relation > 1) {
		if (relation == 2) {
			*outType = 3;
			return 0;
		}
		if (relation == 3) {
			*outType = 4;
			return 0;
		}
		return NP_FRIEND_IMPORTER_ERR_INVALID_STATE;
	}
	if (allowZero ? (relation < 0) : (relation <= 0))
		return NP_FRIEND_IMPORTER_ERR_INVALID_STATE;
	*outType = 2;
	return 0;
}

/* 0x813C765C */
static void ReleasePlayerArray(paf::intrusive_ptr<NpFriendPlayerImpl> *players,
			       unsigned int count)
{
	if (players == NULL)
		return;
	for (unsigned int index = 0; index < count; ++index)
		players[index].clear();
	npFriendImporterFree(players);
}

/* 0x813C7548 */
static int BuildPlayerArrayFromImps(paf::intrusive_ptr<NpFriendPlayerImpl> **outPlayers,
				    const SceNpId *npIds, NpImpsCacheFriendPtr *const *handles,
				    unsigned int count)
{
	paf::intrusive_ptr<NpFriendPlayerImpl> *arr =
		static_cast<paf::intrusive_ptr<NpFriendPlayerImpl> *>(
			npFriendImporterMalloc(sizeof(paf::intrusive_ptr<NpFriendPlayerImpl>) *
					       count));
	*outPlayers = arr;
	if (arr == NULL)
		return NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;

	for (unsigned int index = 0; index < count; ++index)
		new (&arr[index]) paf::intrusive_ptr<NpFriendPlayerImpl>();

	for (unsigned int index = 0; index < count; ++index) {
		int relation = 0;
		paf::rtc::Tick reqDate = 0;
		int reqDir = 0;

		int ret = npImpsSvcGetFriendEntryRelation(handles[index], &relation);
		if (ret < 0)
			return ret;
		ret = npImpsSvcGetFriendEntryDate(handles[index], &reqDate);
		if (ret < 0)
			return ret;
		ret = npImpsSvcGetFriendEntryRequestDirection(handles[index], &reqDir);
		if (ret < 0)
			return ret;

		int playerType = 0;
		ret = MapImpsRelationToPlayerType(relation, true, &playerType);
		if (ret < 0)
			return ret;

		ret = NpFriendPlayerImpl::Create(&arr[index], &npIds[index], playerType, &reqDate,
						 reqDir);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/* 0x813C7686 */
static int BuildPlayerArrayFromRecordList(paf::intrusive_ptr<NpFriendPlayerImpl> **outPlayers,
					  NpFriendRecordList &records)
{
	unsigned int count = static_cast<unsigned int>(records.size());
	paf::intrusive_ptr<NpFriendPlayerImpl> *arr =
		static_cast<paf::intrusive_ptr<NpFriendPlayerImpl> *>(
			npFriendImporterMalloc(sizeof(paf::intrusive_ptr<NpFriendPlayerImpl>) *
					       count));
	*outPlayers = arr;
	if (arr == NULL)
		return NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;

	for (unsigned int index = 0; index < count; ++index)
		new (&arr[index]) paf::intrusive_ptr<NpFriendPlayerImpl>();

	unsigned int index = 0;
	for (NpFriendRecordList::iterator it = records.begin(); it != records.end();
	     ++it, ++index) {
		int ret = NpFriendPlayerImpl::Create(&arr[index], &it->npId, it->playerType,
						     &it->requestDate, it->requestDirection);
		if (ret < 0) {
			ReleasePlayerArray(arr, count);
			*outPlayers = NULL;
			return ret;
		}
	}
	return 0;
}

/* 0x813C7750 */
static int BuildBlockedPlayerArray(paf::intrusive_ptr<NpFriendPlayerImpl> **outPlayers,
				   const SceNpId *npIds, unsigned int count)
{
	paf::intrusive_ptr<NpFriendPlayerImpl> *arr =
		static_cast<paf::intrusive_ptr<NpFriendPlayerImpl> *>(
			npFriendImporterMalloc(sizeof(paf::intrusive_ptr<NpFriendPlayerImpl>) *
					       count));
	*outPlayers = arr;
	if (arr == NULL)
		return NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;

	for (unsigned int index = 0; index < count; ++index)
		new (&arr[index]) paf::intrusive_ptr<NpFriendPlayerImpl>();

	paf::rtc::Tick emptyTick = 0;
	for (unsigned int index = 0; index < count; ++index) {
		int ret = NpFriendPlayerImpl::Create(&arr[index], &npIds[index], 5, &emptyTick, 0);
		if (ret < 0) {
			ReleasePlayerArray(arr, count);
			*outPlayers = NULL;
			return ret;
		}
	}
	return 0;
}

/* 0x813C7824 */
static int AppendFriendRecord(NpFriendRecordList *records, const SceNpId *npId, int relation,
			      const paf::rtc::Tick *reqDate, int reqDir)
{
	NpFriendListRecord rec;
	sce_paf_memset(&rec, 0, sizeof(rec));
	sce_paf_memcpy(&rec.npId, npId, sizeof(rec.npId));
	rec.requestDate = *reqDate;
	int ret = MapImpsRelationToPlayerType(relation, true, &rec.playerType);
	if (ret < 0)
		return ret;
	rec.requestDirection = reqDir;

	size_t before = records->size();
	records->push_back(rec);
	return (records->size() != before) ? 0 : NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;
}

/* ============================================================================
 * LiveArea Notifications (0x813C5DAE, 0x813C62EC)
 * ============================================================================ */

/* 0x813C5DAE: Notify user of incoming friend request via SceLsdb */
static void PostFriendRequestNotification(unsigned int count,
					  const paf::intrusive_ptr<NpFriendPlayerImpl> *players)
{
	bool hasRequest = false;
	for (unsigned int index = 0; index < count; ++index) {
		if (players[index]->GetPlayerType() == 3) {
			hasRequest = true;
			break;
		}
	}
	if (!hasRequest)
		return;

	LsdbNewEvent notice;
	notice.titleId = "NPXS10006";
	notice.itemId = "0";
	notice.msgType = 1025;
	notice.actType = 2;
	notice.newFlag = true;
	notice.iconPath = "vs0:app/NPXS10006/sce_sys/icon0.png";
	notice.execMode = 0x20000;
	notice.execTitleId = "NPXS10006";
	notice.execArg = "mode=friend_req";
	SceLsdb_315B9FD6(&notice, true);
}

/* 0x813C62EC: Notify user that a friend came online via SceLsdb */
static void PostFriendOnlineNotification(const char *onlineId)
{
	LsdbNewEvent notice;
	notice.titleId = "NPXS10006";
	notice.itemId = "0";
	notice.msgType = 1024;
	notice.actType = 2;
	notice.newFlag = true;
	notice.iconPath = "vs0:app/NPXS10006/sce_sys/icon0.png";
	if (onlineId != NULL)
		notice.msgArg[0] = onlineId;
	notice.execMode = 0x20000;
	notice.execTitleId = "NPXS10006";
	notice.execArg = "mode=online";
	SceLsdb_315B9FD6(&notice, true);
}

/* ============================================================================
 * Background Profile Import Job (0x813C71C8..0x813C7377)
 * ============================================================================ */

class NpFriendImportProfileJob : public paf::job::JobItem {
public:
	NpFriendImportProfileJob(const SceNpId &npId, int requestDirection);
	virtual ~NpFriendImportProfileJob();
	virtual int Run();
	virtual void Finish(int result);

private:
	SceNpId m_npId;
	int m_requestDirection;
};

NpFriendImportProfileJob::NpFriendImportProfileJob(const SceNpId &npId, int requestDirection)
	: paf::job::JobItem("SceNpFriendJobImportProfile", NULL),
	  m_npId(npId),
	  m_requestDirection(requestDirection)
{
}

NpFriendImportProfileJob::~NpFriendImportProfileJob()
{
}

/* 0x813C71C8 */
int NpFriendImportProfileJob::Run()
{
	NpFriendProfileImporter *profImporter = NpFriendProfileImporter::GetInstance();
	return profImporter->ImportProfile(&m_npId, m_requestDirection);
}

/* 0x813C71DC */
void NpFriendImportProfileJob::Finish(int result)
{
	(void)result;
	if (IsCanceled()) {
		NpFriendProfileImporter *profImporter = NpFriendProfileImporter::GetInstance();
		if (profImporter != NULL)
			profImporter->AddPendingRetry(&m_npId);
	}
}

/* 0x813C71FA */
int npFriendImportProfileJobCreate(paf::common::SharedPtr<paf::job::JobItem> *outJob,
				   const SceNpId *npId, int requestDirection)
{
	void *raw = npFriendImporterMalloc(sizeof(NpFriendImportProfileJob));
	if (raw == NULL)
		return NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;

	NpFriendImportProfileJob *job = new (raw) NpFriendImportProfileJob(*npId, requestDirection);
	*outJob = paf::common::SharedPtr<paf::job::JobItem>(job);
	return 0;
}

/* ============================================================================
 * Concrete Event Classes (0x813C5BF6..0x813C71C7)
 * ============================================================================ */

/* AddFriends / AddBlocked event (0x813C5BF6, 0x813C5CAA, 0x813C5D52..0x813C60CF) */
class NpFriendAddEvent : public NpFriendImporterEvent {
public:
	explicit NpFriendAddEvent(NpFriendStorage *storage);
	virtual ~NpFriendAddEvent();
	virtual void Execute();

	static int CreateForFriends(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent,
				    const SceNpId *npIds, NpImpsCacheFriendPtr *const *handles,
				    unsigned int count);
	static int CreateForBlocked(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent,
				    const SceNpId *npIds, unsigned int count);

private:
	NpFriendStorage *m_storage;
	unsigned int m_count;
	paf::intrusive_ptr<NpFriendPlayerImpl> *m_players;
};

NpFriendAddEvent::NpFriendAddEvent(NpFriendStorage *storage)
	: m_storage(storage), m_count(0), m_players(NULL)
{
}

NpFriendAddEvent::~NpFriendAddEvent()
{
	if (m_players != NULL) {
		ReleasePlayerArray(m_players, m_count);
		m_players = NULL;
	}
}

/* 0x813C5F8C */
void NpFriendAddEvent::Execute()
{
	int added = 0;
	const paf::intrusive_ptr<NpFriendPlayer> *basePlayers =
		reinterpret_cast<const paf::intrusive_ptr<NpFriendPlayer> *>(m_players);
	if (m_storage->AddFriends(m_count, basePlayers, &added) < 0)
		return;

	PostFriendRequestNotification(m_count, m_players);
	NpFriendProfileImporter *profImporter = NpFriendProfileImporter::GetInstance();
	NpFriendImporter *importer = NULL;
	if (profImporter == NULL || NpFriendImporter::GetInstance(&importer) < 0)
		return;

	int enqueued = 0;
	for (unsigned int index = 0; index < m_count; ++index) {
		const SceNpId *npId = m_players[index]->GetNpId();
		int reqDir = 0;
		if (m_players[index]->GetPlayerType() == 3)
			reqDir = m_players[index]->GetRequestDirection();

		paf::common::SharedPtr<paf::job::JobItem> job;
		if (npFriendImportProfileJobCreate(&job, npId, reqDir) < 0 ||
		    importer->EnqueueProfileJob(job) < 0) {
			profImporter->AddPendingRetry(npId);
		} else if (++enqueued == added) {
			break;
		}
	}
}

/* 0x813C5BF6 */
int NpFriendAddEvent::CreateForFriends(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent,
				       const SceNpId *npIds, NpImpsCacheFriendPtr *const *handles,
				       unsigned int count)
{
	NpFriendStorage *storage = NULL;
	int ret = NpFriendStorage::GetInstance(&storage);
	if (ret < 0)
		return ret;

	void *raw = npFriendImporterMalloc(sizeof(NpFriendAddEvent));
	if (raw == NULL)
		return NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;

	NpFriendAddEvent *ev = new (raw) NpFriendAddEvent(storage);
	*outEvent = paf::intrusive_ptr<NpFriendImporterEvent>(ev);
	ret = BuildPlayerArrayFromImps(&ev->m_players, npIds, handles, count);
	if (ret >= 0)
		ev->m_count = count;
	return ret;
}

/* 0x813C5CAA */
int NpFriendAddEvent::CreateForBlocked(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent,
				       const SceNpId *npIds, unsigned int count)
{
	NpFriendStorage *storage = NULL;
	int ret = NpFriendStorage::GetInstance(&storage);
	if (ret < 0)
		return ret;

	void *raw = npFriendImporterMalloc(sizeof(NpFriendAddEvent));
	if (raw == NULL)
		return NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;

	NpFriendAddEvent *ev = new (raw) NpFriendAddEvent(storage);
	*outEvent = paf::intrusive_ptr<NpFriendImporterEvent>(ev);
	if (BuildBlockedPlayerArray(&ev->m_players, npIds, count) >= 0)
		ev->m_count = count;
	return 0;
}

/* DeletePlayers event (0x813C60D0..0x813C6219) */
class NpFriendDeleteEvent : public NpFriendImporterEvent {
public:
	explicit NpFriendDeleteEvent(NpFriendStorage *storage);
	virtual ~NpFriendDeleteEvent();
	virtual void Execute();

	static int Create(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent, const SceNpId *npIds,
			  unsigned int count, int which);

private:
	NpFriendStorage *m_storage;
	SceNpId *m_npIds;
	unsigned int m_count;
	int m_which;
};

NpFriendDeleteEvent::NpFriendDeleteEvent(NpFriendStorage *storage)
	: m_storage(storage), m_npIds(NULL), m_count(0), m_which(0)
{
}

NpFriendDeleteEvent::~NpFriendDeleteEvent()
{
	if (m_npIds != NULL) {
		npFriendImporterFree(m_npIds);
		m_npIds = NULL;
	}
}

/* 0x813C61EC */
void NpFriendDeleteEvent::Execute()
{
	int which = 0;
	if (m_which == 1)
		which = NP_FRIEND_DELETE_FRIEND;
	else if (m_which == 2)
		which = NP_FRIEND_DELETE_BLOCKED;
	else
		return;
	m_storage->DeletePlayers(m_count, m_npIds, which);
}

/* 0x813C60D0 */
int NpFriendDeleteEvent::Create(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent,
				const SceNpId *npIds, unsigned int count, int which)
{
	NpFriendStorage *storage = NULL;
	int ret = NpFriendStorage::GetInstance(&storage);
	if (ret < 0)
		return ret;

	void *raw = npFriendImporterMalloc(sizeof(NpFriendDeleteEvent));
	if (raw == NULL)
		return NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;

	NpFriendDeleteEvent *ev = new (raw) NpFriendDeleteEvent(storage);
	*outEvent = paf::intrusive_ptr<NpFriendImporterEvent>(ev);
	ev->m_npIds = static_cast<SceNpId *>(npFriendImporterMalloc(sizeof(SceNpId) * count));
	if (ev->m_npIds == NULL) {
		outEvent->clear();
		return NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;
	}
	sce_paf_memcpy(ev->m_npIds, npIds, sizeof(SceNpId) * count);
	ev->m_count = count;
	ev->m_which = which;
	return 0;
}

/* Friend presence transition event (0x813C621A..0x813C669F) */
class NpFriendPresenceEvent : public NpFriendImporterEvent {
public:
	NpFriendPresenceEvent(NpFriendStorage *storage, const SceNpId &npId, int onlineState,
			      int transitionKind);
	virtual ~NpFriendPresenceEvent();
	virtual void Execute();

	static int Create(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent, const SceNpId *npId,
			  int onlineState, int transitionKind);

private:
	NpFriendStorage *m_storage;
	SceNpId m_npId;
	int m_onlineState;
	int m_transitionKind;
};

NpFriendPresenceEvent::NpFriendPresenceEvent(NpFriendStorage *storage, const SceNpId &npId,
					     int onlineState, int transitionKind)
	: m_storage(storage),
	  m_npId(npId),
	  m_onlineState(onlineState),
	  m_transitionKind(transitionKind)
{
}

NpFriendPresenceEvent::~NpFriendPresenceEvent()
{
}

/* 0x813C64C4 */
void NpFriendPresenceEvent::Execute()
{
	if ((m_transitionKind == 1 || m_transitionKind == 2) && m_onlineState == 3)
		PostFriendOnlineNotification(m_npId.handle.data);

	if (m_onlineState == 0)
		return;

	NpFriendProfileImporter *profImporter = NpFriendProfileImporter::GetInstance();
	if (profImporter == NULL)
		return;

	NpFriendImporter *importer = NULL;
	if (NpFriendImporter::GetInstance(&importer) < 0)
		return;

	paf::common::SharedPtr<paf::job::JobItem> job;
	if (npFriendImportProfileJobCreate(&job, &m_npId, 0) < 0 ||
	    importer->EnqueueProfileJob(job) < 0) {
		profImporter->AddPendingRetry(&m_npId);
	}
}

/* 0x813C621A */
int NpFriendPresenceEvent::Create(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent,
				  const SceNpId *npId, int onlineState, int transitionKind)
{
	NpFriendStorage *storage = NULL;
	int ret = NpFriendStorage::GetInstance(&storage);
	if (ret < 0)
		return ret;

	void *raw = npFriendImporterMalloc(sizeof(NpFriendPresenceEvent));
	if (raw == NULL)
		return NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;

	NpFriendPresenceEvent *ev =
		new (raw) NpFriendPresenceEvent(storage, *npId, onlineState, transitionKind);
	*outEvent = paf::intrusive_ptr<NpFriendImporterEvent>(ev);
	return 0;
}

/* Network reconnected event (0x813C66A0..0x813C670B) */
class NpFriendNetOnlineEvent : public NpFriendImporterEvent {
public:
	NpFriendNetOnlineEvent();
	virtual ~NpFriendNetOnlineEvent();
	virtual void Execute();

	static int Create(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent);
};

NpFriendNetOnlineEvent::NpFriendNetOnlineEvent()
{
}

NpFriendNetOnlineEvent::~NpFriendNetOnlineEvent()
{
}

/* 0x813C66F4 */
void NpFriendNetOnlineEvent::Execute()
{
	NpFriendProfileImporter *profImporter = NpFriendProfileImporter::GetInstance();
	if (profImporter != NULL) {
		profImporter->RetryPending();
		profImporter->RetrySelfProfile();
	}
}

/* 0x813C66A0 */
int NpFriendNetOnlineEvent::Create(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent)
{
	void *raw = npFriendImporterMalloc(sizeof(NpFriendNetOnlineEvent));
	if (raw == NULL)
		return NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;
	NpFriendNetOnlineEvent *ev = new (raw) NpFriendNetOnlineEvent();
	*outEvent = paf::intrusive_ptr<NpFriendImporterEvent>(ev);
	return 0;
}

/* NP Sign-in / sign-out event (0x813C670C..0x813C6843) */
class NpFriendSignInEvent : public NpFriendImporterEvent {
public:
	explicit NpFriendSignInEvent(int state);
	virtual ~NpFriendSignInEvent();
	virtual void Execute();

	static int Create(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent, int state);

private:
	int m_state;
};

NpFriendSignInEvent::NpFriendSignInEvent(int state) : m_state(state)
{
}

NpFriendSignInEvent::~NpFriendSignInEvent()
{
}

/* 0x813C6768 */
void NpFriendSignInEvent::Execute()
{
	if (m_state == 1) {
		NpFriendProfileImporter *profImporter = NpFriendProfileImporter::GetInstance();
		if (profImporter != NULL)
			profImporter->OnSignOut();

		NpFriendStorage *storage = NULL;
		if (NpFriendStorage::GetInstance(&storage) >= 0)
			storage->Clear();
	} else if (m_state == 2) {
		NpFriendProfileImporter *profImporter = NpFriendProfileImporter::GetInstance();
		if (profImporter == NULL)
			return;

		profImporter->RefreshSelfNpId();
		SceNpId selfNpId;
		sce_paf_memset(&selfNpId, 0, sizeof(selfNpId));
		sce_paf_memcpy(&selfNpId, profImporter->GetSelfNpId(), sizeof(selfNpId));

		NpLookupProfile profile;
		sce_paf_memset(&profile, 0, sizeof(profile));
		if (NpFriendProfileImporter::FetchProfileFromServer(&selfNpId, &selfNpId,
								    &profile, 3) < 0 ||
		    npFriendCacheDownloadAvatar(profile.icon.data) < 0) {
			profImporter->MarkSelfRetryPending();
		}
	}
}

/* 0x813C670C */
int NpFriendSignInEvent::Create(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent, int state)
{
	void *raw = npFriendImporterMalloc(sizeof(NpFriendSignInEvent));
	if (raw == NULL)
		return NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;
	NpFriendSignInEvent *ev = new (raw) NpFriendSignInEvent(state);
	*outEvent = paf::intrusive_ptr<NpFriendImporterEvent>(ev);
	return 0;
}

/* Sync full blocked list event (0x813C6844..0x813C6B07) */
class NpFriendSyncBlockedEvent : public NpFriendImporterEvent {
public:
	explicit NpFriendSyncBlockedEvent(NpFriendStorage *storage);
	virtual ~NpFriendSyncBlockedEvent();
	virtual void Execute();

	static int Create(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent, const SceNpId *npIds,
			  unsigned int count);

private:
	NpFriendStorage *m_storage;
	NpFriendRecordList m_records;
};

NpFriendSyncBlockedEvent::NpFriendSyncBlockedEvent(NpFriendStorage *storage) : m_storage(storage)
{
}

NpFriendSyncBlockedEvent::~NpFriendSyncBlockedEvent()
{
}

/* 0x813C6AA8 */
void NpFriendSyncBlockedEvent::Execute()
{
	paf::intrusive_ptr<NpFriendPlayerImpl> *players = NULL;
	if (BuildPlayerArrayFromRecordList(&players, m_records) < 0)
		return;

	unsigned int count = static_cast<unsigned int>(m_records.size());
	const paf::intrusive_ptr<NpFriendPlayer> *basePlayers =
		reinterpret_cast<const paf::intrusive_ptr<NpFriendPlayer> *>(players);
	if (m_storage->StoreBlocked(count, basePlayers, false) >= 0) {
		NpFriendProfileImporter *profImporter = NpFriendProfileImporter::GetInstance();
		if (profImporter != NULL && count != 0)
			profImporter->ImportProfiles(count,
				reinterpret_cast<NpFriendPlayerImpl *const *>(players));
	}
	ReleasePlayerArray(players, count);
}

/* 0x813C6844 */
int NpFriendSyncBlockedEvent::Create(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent,
				     const SceNpId *npIds, unsigned int count)
{
	NpFriendStorage *storage = NULL;
	int ret = NpFriendStorage::GetInstance(&storage);
	if (ret < 0)
		return ret;

	void *raw = npFriendImporterMalloc(sizeof(NpFriendSyncBlockedEvent));
	if (raw == NULL)
		return NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;

	NpFriendSyncBlockedEvent *ev = new (raw) NpFriendSyncBlockedEvent(storage);
	*outEvent = paf::intrusive_ptr<NpFriendImporterEvent>(ev);

	for (unsigned int index = 0; index < count; ++index) {
		NpFriendListRecord rec;
		sce_paf_memset(&rec, 0, sizeof(rec));
		sce_paf_memcpy(&rec.npId, &npIds[index], sizeof(rec.npId));
		rec.playerType = 5;
		size_t before = ev->m_records.size();
		ev->m_records.push_back(rec);
		if (ev->m_records.size() == before) {
			outEvent->clear();
			return NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;
		}
	}
	return 0;
}

/* Clear blocked list event (0x813C6B08..0x813C6BA3) */
class NpFriendClearBlockedEvent : public NpFriendImporterEvent {
public:
	explicit NpFriendClearBlockedEvent(NpFriendStorage *storage);
	virtual ~NpFriendClearBlockedEvent();
	virtual void Execute();

	static int Create(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent);

private:
	NpFriendStorage *m_storage;
};

NpFriendClearBlockedEvent::NpFriendClearBlockedEvent(NpFriendStorage *storage)
	: m_storage(storage)
{
}

NpFriendClearBlockedEvent::~NpFriendClearBlockedEvent()
{
}

/* 0x813C6B92 */
void NpFriendClearBlockedEvent::Execute()
{
	m_storage->StoreBlocked(0, NULL, true);
}

/* 0x813C6B08 */
int NpFriendClearBlockedEvent::Create(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent)
{
	NpFriendStorage *storage = NULL;
	int ret = NpFriendStorage::GetInstance(&storage);
	if (ret < 0)
		return ret;

	void *raw = npFriendImporterMalloc(sizeof(NpFriendClearBlockedEvent));
	if (raw == NULL)
		return NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;

	NpFriendClearBlockedEvent *ev = new (raw) NpFriendClearBlockedEvent(storage);
	*outEvent = paf::intrusive_ptr<NpFriendImporterEvent>(ev);
	return 0;
}

/* Sync full friend list event (0x813C6BA4..0x813C6DBB) */
class NpFriendSyncFriendsEvent : public NpFriendImporterEvent {
public:
	explicit NpFriendSyncFriendsEvent(NpFriendStorage *storage);
	virtual ~NpFriendSyncFriendsEvent();
	virtual void Execute();

	static int Create(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent, const SceNpId *npIds,
			  NpImpsCacheFriendPtr *const *handles, unsigned int count);

private:
	NpFriendStorage *m_storage;
	NpFriendRecordList m_records;
};

NpFriendSyncFriendsEvent::NpFriendSyncFriendsEvent(NpFriendStorage *storage) : m_storage(storage)
{
}

NpFriendSyncFriendsEvent::~NpFriendSyncFriendsEvent()
{
}

/* 0x813C6D5C */
void NpFriendSyncFriendsEvent::Execute()
{
	paf::intrusive_ptr<NpFriendPlayerImpl> *players = NULL;
	if (BuildPlayerArrayFromRecordList(&players, m_records) < 0)
		return;

	unsigned int count = static_cast<unsigned int>(m_records.size());
	const paf::intrusive_ptr<NpFriendPlayer> *basePlayers =
		reinterpret_cast<const paf::intrusive_ptr<NpFriendPlayer> *>(players);
	if (m_storage->StoreFriends(count, basePlayers, false) >= 0) {
		NpFriendProfileImporter *profImporter = NpFriendProfileImporter::GetInstance();
		if (profImporter != NULL && count != 0)
			profImporter->ImportProfiles(count,
				reinterpret_cast<NpFriendPlayerImpl *const *>(players));
	}
	ReleasePlayerArray(players, count);
}

/* 0x813C6BA4 */
int NpFriendSyncFriendsEvent::Create(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent,
				     const SceNpId *npIds, NpImpsCacheFriendPtr *const *handles,
				     unsigned int count)
{
	NpFriendStorage *storage = NULL;
	int ret = NpFriendStorage::GetInstance(&storage);
	if (ret < 0)
		return ret;

	void *raw = npFriendImporterMalloc(sizeof(NpFriendSyncFriendsEvent));
	if (raw == NULL)
		return NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;

	NpFriendSyncFriendsEvent *ev = new (raw) NpFriendSyncFriendsEvent(storage);
	*outEvent = paf::intrusive_ptr<NpFriendImporterEvent>(ev);

	for (unsigned int index = 0; index < count; ++index) {
		int relation = 0;
		paf::rtc::Tick reqDate = 0;
		int reqDir = 0;

		ret = npImpsSvcGetFriendEntryRelation(handles[index], &relation);
		if (ret >= 0)
			ret = npImpsSvcGetFriendEntryDate(handles[index], &reqDate);
		if (ret >= 0)
			ret = npImpsSvcGetFriendEntryRequestDirection(handles[index], &reqDir);
		if (ret >= 0)
			ret = AppendFriendRecord(&ev->m_records, &npIds[index], relation,
						 &reqDate, reqDir);
		if (ret < 0) {
			outEvent->clear();
			return ret;
		}
	}
	return 0;
}

/* Clear friend list event (0x813C6DBC..0x813C6E57) */
class NpFriendClearFriendsEvent : public NpFriendImporterEvent {
public:
	explicit NpFriendClearFriendsEvent(NpFriendStorage *storage);
	virtual ~NpFriendClearFriendsEvent();
	virtual void Execute();

	static int Create(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent);

private:
	NpFriendStorage *m_storage;
};

NpFriendClearFriendsEvent::NpFriendClearFriendsEvent(NpFriendStorage *storage)
	: m_storage(storage)
{
}

NpFriendClearFriendsEvent::~NpFriendClearFriendsEvent()
{
}

/* 0x813C6E46 */
void NpFriendClearFriendsEvent::Execute()
{
	m_storage->StoreFriends(0, NULL, true);
}

/* 0x813C6DBC */
int NpFriendClearFriendsEvent::Create(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent)
{
	NpFriendStorage *storage = NULL;
	int ret = NpFriendStorage::GetInstance(&storage);
	if (ret < 0)
		return ret;

	void *raw = npFriendImporterMalloc(sizeof(NpFriendClearFriendsEvent));
	if (raw == NULL)
		return NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;

	NpFriendClearFriendsEvent *ev = new (raw) NpFriendClearFriendsEvent(storage);
	*outEvent = paf::intrusive_ptr<NpFriendImporterEvent>(ev);
	return 0;
}

/* Self profile changed event (0x813C6E58..0x813C6EC7) */
class NpFriendUpdateSelfProfileEvent : public NpFriendImporterEvent {
public:
	NpFriendUpdateSelfProfileEvent();
	virtual ~NpFriendUpdateSelfProfileEvent();
	virtual void Execute();

	static int Create(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent);
};

NpFriendUpdateSelfProfileEvent::NpFriendUpdateSelfProfileEvent()
{
}

NpFriendUpdateSelfProfileEvent::~NpFriendUpdateSelfProfileEvent()
{
}

/* 0x813C6EAC */
void NpFriendUpdateSelfProfileEvent::Execute()
{
	NpFriendProfileImporter *profImporter = NpFriendProfileImporter::GetInstance();
	if (profImporter != NULL && profImporter->RefreshSelfProfile() < 0)
		profImporter->MarkSelfRetryPending();
}

/* 0x813C6E58 */
int NpFriendUpdateSelfProfileEvent::Create(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent)
{
	void *raw = npFriendImporterMalloc(sizeof(NpFriendUpdateSelfProfileEvent));
	if (raw == NULL)
		return NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;
	NpFriendUpdateSelfProfileEvent *ev = new (raw) NpFriendUpdateSelfProfileEvent();
	*outEvent = paf::intrusive_ptr<NpFriendImporterEvent>(ev);
	return 0;
}

/* Update friend player types event (0x813C6EC8..0x813C71C7) */
class NpFriendUpdateTypesEvent : public NpFriendImporterEvent {
public:
	explicit NpFriendUpdateTypesEvent(NpFriendStorage *storage);
	virtual ~NpFriendUpdateTypesEvent();
	virtual void Execute();

	int AddRecord(const SceNpId *npId, int relation, const paf::rtc::Tick *reqDate);

	static int Create(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent, const SceNpId *npIds,
			  NpImpsCacheFriendPtr *const *handles, unsigned int count);

private:
	NpFriendStorage *m_storage;
	NpFriendRecordList m_records;
};

NpFriendUpdateTypesEvent::NpFriendUpdateTypesEvent(NpFriendStorage *storage) : m_storage(storage)
{
}

NpFriendUpdateTypesEvent::~NpFriendUpdateTypesEvent()
{
}

/* 0x813C716C */
void NpFriendUpdateTypesEvent::Execute()
{
	paf::intrusive_ptr<NpFriendPlayerImpl> *players = NULL;
	if (BuildPlayerArrayFromRecordList(&players, m_records) < 0)
		return;

	unsigned int count = static_cast<unsigned int>(m_records.size());
	const paf::intrusive_ptr<NpFriendPlayer> *basePlayers =
		reinterpret_cast<const paf::intrusive_ptr<NpFriendPlayer> *>(players);
	if (m_storage->UpdatePlayerTypes(count, basePlayers) >= 0) {
		NpFriendProfileImporter *profImporter = NpFriendProfileImporter::GetInstance();
		if (profImporter != NULL)
			profImporter->ImportProfiles(count,
				reinterpret_cast<NpFriendPlayerImpl *const *>(players));
	}
	ReleasePlayerArray(players, count);
}

/* 0x813C6EC8 */
int NpFriendUpdateTypesEvent::AddRecord(const SceNpId *npId, int relation,
					const paf::rtc::Tick *reqDate)
{
	NpFriendListRecord rec;
	sce_paf_memset(&rec, 0, sizeof(rec));
	sce_paf_memcpy(&rec.npId, npId, sizeof(rec.npId));
	rec.requestDate = *reqDate;
	int ret = MapImpsRelationToPlayerType(relation, false, &rec.playerType);
	if (ret < 0)
		return ret;

	size_t before = m_records.size();
	m_records.push_back(rec);
	return (m_records.size() != before) ? 0 : NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;
}

/* 0x813C6FBE */
int NpFriendUpdateTypesEvent::Create(paf::intrusive_ptr<NpFriendImporterEvent> *outEvent,
				     const SceNpId *npIds, NpImpsCacheFriendPtr *const *handles,
				     unsigned int count)
{
	NpFriendStorage *storage = NULL;
	int ret = NpFriendStorage::GetInstance(&storage);
	if (ret < 0)
		return ret;

	void *raw = npFriendImporterMalloc(sizeof(NpFriendUpdateTypesEvent));
	if (raw == NULL)
		return NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;

	NpFriendUpdateTypesEvent *ev = new (raw) NpFriendUpdateTypesEvent(storage);
	*outEvent = paf::intrusive_ptr<NpFriendImporterEvent>(ev);

	for (unsigned int index = 0; index < count; ++index) {
		int relation = 0;
		paf::rtc::Tick reqDate = 0;
		ret = npImpsSvcGetFriendEntryRelation(handles[index], &relation);
		if (ret >= 0)
			ret = npImpsSvcGetFriendEntryDate(handles[index], &reqDate);
		if (ret >= 0)
			ret = ev->AddRecord(&npIds[index], relation, &reqDate);
		if (ret < 0) {
			outEvent->clear();
			return ret;
		}
	}
	return 0;
}

/* ============================================================================
 * IMPS & NetCtl Callbacks (0x813C5970..0x813C5BDF)
 * ============================================================================ */

/* 0x813C5970 */
static void OnImpsFriendListCallback(int eventType, const SceNpId *npIds,
				     NpImpsCacheFriendPtr **handles, SceSize count, void *userArg)
{
	NpFriendImporter *importer = static_cast<NpFriendImporter *>(userArg);
	paf::intrusive_ptr<NpFriendImporterEvent> event;
	int ret = -1;

	switch (eventType) {
	case 1:
		ret = NpFriendSyncFriendsEvent::Create(&event, npIds, handles, count);
		break;
	case 2:
		ret = NpFriendAddEvent::CreateForFriends(&event, npIds, handles, count);
		break;
	case 3:
		ret = NpFriendDeleteEvent::Create(&event, npIds, count, 1);
		break;
	case 4:
		ret = NpFriendUpdateTypesEvent::Create(&event, npIds, handles, count);
		break;
	case 5:
		ret = NpFriendClearFriendsEvent::Create(&event);
		break;
	default:
		return;
	}

	if (ret >= 0 && event.get() != NULL)
		importer->PushEvent(event);
}

/* 0x813C5A0A */
static void OnImpsBlockListCallback(int eventType, const SceNpId *npIds, SceSize count,
				    void *userArg)
{
	NpFriendImporter *importer = static_cast<NpFriendImporter *>(userArg);
	paf::intrusive_ptr<NpFriendImporterEvent> event;
	int ret = -1;

	switch (eventType) {
	case 1:
		ret = NpFriendSyncBlockedEvent::Create(&event, npIds, count);
		break;
	case 2:
		ret = NpFriendAddEvent::CreateForBlocked(&event, npIds, count);
		break;
	case 3:
		ret = NpFriendDeleteEvent::Create(&event, npIds, count, 2);
		break;
	case 4:
		ret = NpFriendClearBlockedEvent::Create(&event);
		break;
	default:
		return;
	}

	if (ret >= 0 && event.get() != NULL)
		importer->PushEvent(event);
}

/* 0x813C5A9E */
static void OnImpsServiceStateCallback(int state, int errorCode, void *userArg)
{
	NpFriendImporter *importer = static_cast<NpFriendImporter *>(userArg);
	if (errorCode < 0)
		importer->CancelAllProfileJobs();

	paf::intrusive_ptr<NpFriendImporterEvent> event;
	if (NpFriendSignInEvent::Create(&event, state) >= 0 && event.get() != NULL)
		importer->PushEvent(event);
}

/* 0x813C5B04 */
static void OnImpsFriendContextCallback(int eventType, NpImpsCacheFriendPtr *handle,
					const SceNpId *npId, int contextState, void *extra,
					void *userArg)
{
	(void)eventType;
	(void)handle;
	NpFriendImporter *importer = static_cast<NpFriendImporter *>(userArg);
	int transitionKind = static_cast<int>(reinterpret_cast<SceIntPtr>(extra));

	if (contextState == 0)
		return;

	paf::intrusive_ptr<NpFriendImporterEvent> event;
	if (NpFriendPresenceEvent::Create(&event, npId, contextState, transitionKind) >= 0 &&
	    event.get() != NULL) {
		importer->PushEvent(event);
	}
}

/* 0x813C5B66 */
static void OnNetCtlEventCallback(int prevState, int state, int eventType, int errorCode,
				  int detail, void *userArg)
{
	(void)errorCode;
	(void)detail;
	NpFriendImporter *importer = static_cast<NpFriendImporter *>(userArg);
	if (state == 4 && (eventType == 6 || prevState == 5)) {
		paf::intrusive_ptr<NpFriendImporterEvent> event;
		if (NpFriendNetOnlineEvent::Create(&event) >= 0 && event.get() != NULL)
			importer->PushEvent(event);
	}
}

/* ============================================================================
 * NpFriendImporter Implementation (0x813C5464..0x813C5BE0, 0x813C73E0..0x813C7547)
 * ============================================================================ */

/* 0x813C5464 */
NpFriendImporter::NpFriendImporter(paf::memory::HeapAllocator *heap)
	: paf::thread::Thread(110, 12288, "SceNpFriendImporter", NULL),
	  m_netCtlCallbackId(0),
	  m_impsListenerId(0),
	  m_workerMutex("SceNpFriendImporterMutex", 0),
	  m_workerCond(m_workerMutex, "SceNpFriendImporterCond", 0),
	  m_heap(heap),
	  m_maxEvents(512),
	  m_queueMutex("SceNpFriendImporterEventQueue", 0),
	  m_jobQueue(NULL)
{
}

/* 0x813C5890 */
NpFriendImporter::~NpFriendImporter()
{
	m_eventList.clear();
}

/* 0x813C5916 */
void NpFriendImporter::WakeWorker(void)
{
	m_workerCond.Notify();
}

/* 0x813C73E0 */
int NpFriendImporter::PushEvent(const paf::intrusive_ptr<NpFriendImporterEvent> &event)
{
	m_queueMutex.Lock();
	if (npFriendImporterGetFreeSize() < 8U) {
		WakeWorker();
		paf::thread::Sleep(10);
	}

	size_t before = m_eventList.size();
	if (before >= m_maxEvents) {
		m_queueMutex.Unlock();
		return NP_FRIEND_IMPORTER_ERR_QUEUE_FULL;
	}

	m_eventList.push_back(event);
	if (m_eventList.size() == before) {
		m_queueMutex.Unlock();
		return NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;
	}

	WakeWorker();
	m_queueMutex.Unlock();
	return 0;
}

/* 0x813C7478 */
void NpFriendImporter::DispatchEvents(void)
{
	m_queueMutex.Lock();
	while (m_eventList.size() != 0) {
		paf::intrusive_ptr<NpFriendImporterEvent> event = m_eventList.front();
		m_eventList.pop_front();
		m_queueMutex.Unlock();
		if (event.get() != NULL)
			event->Execute();
		m_queueMutex.Lock();
	}
	m_queueMutex.Unlock();
}

/* 0x813C74F8 */
void NpFriendImporter::ClearEvents(void)
{
	m_queueMutex.Lock();
	m_eventList.clear();
	m_queueMutex.Unlock();
}

/* 0x813C5922 */
void NpFriendImporter::EntryFunction()
{
	m_workerMutex.Lock();
	while (!IsCanceled()) {
		m_workerCond.Wait(0);
		m_workerMutex.Unlock();
		DispatchEvents();
		m_workerMutex.Lock();
	}
	ClearEvents();
	m_workerMutex.Unlock();
}

/* 0x813C581A */
int NpFriendImporter::EnqueueUpdateSelfProfile(void)
{
	paf::intrusive_ptr<NpFriendImporterEvent> event;
	int ret = NpFriendUpdateSelfProfileEvent::Create(&event);
	if (ret >= 0 && event.get() != NULL)
		ret = PushEvent(event);
	return ret;
}

/* 0x813C5878 */
int NpFriendImporter::EnqueueProfileJob(paf::common::SharedPtr<paf::job::JobItem> &job)
{
	return m_jobQueue->Enqueue(job);
}

/* 0x813C5884 */
void NpFriendImporter::CancelAllProfileJobs(void)
{
	m_jobQueue->CancelAllItems();
}

/* 0x813C54FA */
int NpFriendImporter::Initialize(paf::memory::HeapAllocator *heap,
				 const NpFriendImporterJobQueueOption *jobOpt)
{
	if (heap == NULL)
		return NP_FRIEND_IMPORTER_ERR_INVALID_ARGUMENT;
	if (s_importerInstance != NULL)
		return NP_FRIEND_IMPORTER_ERR_ALREADY_INITIALIZED;

	npFriendImporterSetHeap(heap);
	void *rawInst = npFriendImporterMalloc(sizeof(NpFriendImporter));
	if (rawInst == NULL)
		return NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;

	s_importerInstance = new (rawInst) NpFriendImporter(heap);
	int ret = s_importerInstance->Start();
	if (ret >= 0) {
		NpFriendStorage *storage = NULL;
		ret = NpFriendStorage::GetInstance(&storage);
		if (ret >= 0) {
			ret = npFriendCacheInit();
			if (ret >= 0) {
				ret = NpFriendProfileImporter::Initialize(storage);
				if (ret >= 0) {
					paf::job::JobQueue::Option jqOpt;
					if (jobOpt != NULL) {
						jqOpt.numThreads = jobOpt->numThreads;
						jqOpt.priority = jobOpt->priority;
						jqOpt.stackSize = jobOpt->stackSize;
					} else {
						jqOpt.numThreads = 1;
						jqOpt.priority = 0x1000010A;
						jqOpt.stackSize = 0x4000;
					}

					void *rawJq = npFriendImporterMalloc(
						sizeof(paf::job::JobQueue));
					if (rawJq == NULL) {
						ret = NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;
					} else {
						s_importerInstance->m_jobQueue =
							new (rawJq) paf::job::JobQueue(
								"SceNpPresenceProfileUtilJob",
								&jqOpt);
					}

					if (ret >= 0) {
						ret = SceShell_2E0B6FA6(
							OnNetCtlEventCallback, s_importerInstance,
							&s_importerInstance->m_netCtlCallbackId);
						if (ret >= 0) {
							NpImpsSvcListenerCallbacks cbs;
							sce_paf_memset(&cbs, 0, sizeof(cbs));
							cbs.onServiceState =
								OnImpsServiceStateCallback;
							cbs.onFriendList = OnImpsFriendListCallback;
							cbs.onBlockList = OnImpsBlockListCallback;
							cbs.onFriendContext =
								OnImpsFriendContextCallback;
							cbs.userArg = s_importerInstance;
							ret = npImpsSvcRegisterListener(
								&cbs,
								&s_importerInstance
									 ->m_impsListenerId);
							if (ret >= 0)
								return 0;

							SceShell_B4D888D5(
								s_importerInstance
									->m_netCtlCallbackId);
						}

						if (s_importerInstance->m_jobQueue != NULL) {
							s_importerInstance->m_jobQueue
								->CancelAllItems();
							s_importerInstance->m_jobQueue->WaitEmpty();
							s_importerInstance->m_jobQueue->~JobQueue();
							npFriendImporterFree(
								s_importerInstance->m_jobQueue);
							s_importerInstance->m_jobQueue = NULL;
						}
					}
					NpFriendProfileImporter::Terminate();
				}
				npFriendCacheTerm();
			}
		}
		s_importerInstance->Cancel();
		s_importerInstance->Join();
	}

	s_importerInstance->~NpFriendImporter();
	npFriendImporterFree(s_importerInstance);
	s_importerInstance = NULL;
	npFriendImporterClearHeap();
	return ret;
}

/* 0x813C575C */
int NpFriendImporter::Terminate(void)
{
	if (s_importerInstance == NULL)
		return NP_FRIEND_IMPORTER_ERR_NOT_INITIALIZED;

	int ret = npImpsSvcUnregisterListener(s_importerInstance->m_impsListenerId);
	if (ret < 0)
		return ret;
	ret = SceShell_B4D888D5(s_importerInstance->m_netCtlCallbackId);
	if (ret < 0)
		return ret;

	if (s_importerInstance->m_jobQueue != NULL) {
		s_importerInstance->m_jobQueue->CancelAllItems();
		s_importerInstance->m_jobQueue->WaitEmpty();
		s_importerInstance->m_jobQueue->~JobQueue();
		npFriendImporterFree(s_importerInstance->m_jobQueue);
		s_importerInstance->m_jobQueue = NULL;
	}

	NpFriendProfileImporter::Terminate();
	npFriendCacheTerm();
	s_importerInstance->Cancel();
	s_importerInstance->Join();
	s_importerInstance->~NpFriendImporter();
	npFriendImporterFree(s_importerInstance);
	s_importerInstance = NULL;
	npFriendImporterClearHeap();
	return 0;
}

/* 0x813C57FC */
int NpFriendImporter::GetInstance(NpFriendImporter **outInstance)
{
	if (s_importerInstance == NULL)
		return NP_FRIEND_IMPORTER_ERR_NOT_INITIALIZED;
	*outInstance = s_importerInstance;
	return 0;
}

/* 0x813C5BE0 */
paf::job::JobQueue *NpFriendImporter::GetJobQueue(void)
{
	if (s_importerInstance != NULL)
		return s_importerInstance->m_jobQueue;
	return NULL;
}

/* ============================================================================
 * Public C/C++ Entry Points (0x813C541C, 0x813C5426, 0x813C542E, 0x813C5BE0)
 * ============================================================================ */

/* 0x813C541C */
int npFriendImporterInit(paf::memory::HeapAllocator *heap)
{
	return NpFriendImporter::Initialize(heap, NULL);
}

/* 0x813C5426 */
int npFriendImporterTerm(void)
{
	return NpFriendImporter::Terminate();
}

/* 0x813C542E */
void shellNotifyProfileSaved(void)
{
	NpFriendImporter *importer = NULL;
	if (NpFriendImporter::GetInstance(&importer) >= 0 && importer != NULL)
		importer->EnqueueUpdateSelfProfile();
}

/* 0x813C5BE0 */
paf::job::JobQueue *shellGetNpBackgroundJobQueue(void)
{
	return NpFriendImporter::GetJobQueue();
}
