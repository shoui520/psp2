/*
 * np_friend_profile_importer.cpp: SceNpFriendProfileImporter singleton that fetches
 * NP user profiles via SceNpUtility, persists them in NpFriendStorage, downloads
 * avatar/panel images via SceFriendCacheCreater, and retries failed imports on reconnect.
 * 3.65-CEX: 0x813C7938..0x813C84FB
 */
#include "np_friend_importer_internal.h"
#include <np/np_utility/np_utility_internal.h>

/* NP Manager & NP Utility profile lookup bridges (0x813268BC, 0x813887C6) */
extern int npAccountCreateRequest(void);
extern int npAccountDestroyRequest(int reqHandle);
extern int npAccountGetNpId(int reqHandle, SceNpId *outNpId);

extern int npUtilityProfileCreateTitleCtx(const SceNpCommunicationId *commId,
					  const SceNpId *selfNpId);
extern int npUtilityProfileDestroyTitleCtx(int titleCtxId);
extern int npUtilityProfileCreateRequest(int titleCtxId);
extern int npUtilityProfileDestroyRequest(int reqId);
extern int npUtilityProfileGetUserProfile(int reqId, SceUInt32 avatarSizeType,
					  SceUInt32 panelSizeType, const SceNpId *targetNpId,
					  NpLookupProfile *outProfile, void *avatarBuffer,
					  SceSize avatarBufferMax, SceSize *outAvatarSize,
					  void *panelBuffer, SceSize panelBufferMax,
					  SceSize *outPanelSize, void *option);

static NpFriendProfileImporter *s_profileImporterInstance = NULL;	/* 0x81549340 */

/* 0x813C7938 */
NpFriendProfileImporter::NpFriendProfileImporter(NpFriendStorage *storage)
	: m_selfNpIdValid(false),
	  m_storage(storage),
	  m_lock("SceNpFriendProfileImporter", 0),
	  m_selfRetryPending(false)
{
	sce_paf_memset(&m_selfNpId, 0, sizeof(m_selfNpId));
}

/* 0x813C7A3E */
NpFriendProfileImporter::~NpFriendProfileImporter()
{
	m_retryQueue.clear();
}

/* 0x813C79A0 */
int NpFriendProfileImporter::Initialize(NpFriendStorage *storage)
{
	if (storage == NULL)
		return NP_FRIEND_IMPORTER_ERR_INVALID_ARGUMENT;
	if (s_profileImporterInstance != NULL)
		return NP_FRIEND_IMPORTER_ERR_ALREADY_INITIALIZED;

	void *raw = npFriendImporterMalloc(sizeof(NpFriendProfileImporter));
	if (raw == NULL)
		return NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY;

	s_profileImporterInstance = new (raw) NpFriendProfileImporter(storage);
	return 0;
}

/* 0x813C7A00 */
int NpFriendProfileImporter::Terminate(void)
{
	if (s_profileImporterInstance == NULL)
		return NP_FRIEND_IMPORTER_ERR_NOT_INITIALIZED;

	s_profileImporterInstance->~NpFriendProfileImporter();
	npFriendImporterFree(s_profileImporterInstance);
	s_profileImporterInstance = NULL;
	return 0;
}

/* 0x813C7A32 */
NpFriendProfileImporter *NpFriendProfileImporter::GetInstance(void)
{
	return s_profileImporterInstance;
}

/* 0x813C7AE6 */
int NpFriendProfileImporter::RefreshSelfNpId(void)
{
	int req = npAccountCreateRequest();
	if (req < 0)
		return req;

	SceNpId npId;
	sce_paf_memset(&npId, 0, sizeof(npId));
	int ret = npAccountGetNpId(req, &npId);
	npAccountDestroyRequest(req);
	if (ret >= 0) {
		sce_paf_memcpy(&m_selfNpId, &npId, sizeof(m_selfNpId));
		m_selfNpIdValid = true;
		return 0;
	}
	return ret;
}

int NpFriendProfileImporter::EnsureSelfNpId(void)
{
	if (m_selfNpIdValid)
		return 0;
	return RefreshSelfNpId();
}

/* 0x813C7B3E */
void NpFriendProfileImporter::OnSignOut(void)
{
	NpFriendImporter *importer = NULL;
	if (NpFriendImporter::GetInstance(&importer) >= 0 && importer != NULL)
		importer->CancelAllProfileJobs();

	m_lock.Lock();
	m_retryQueue.clear();
	sce_paf_memset(&m_selfNpId, 0, sizeof(m_selfNpId));
	m_selfNpIdValid = false;
	m_lock.Unlock();
}

void NpFriendProfileImporter::EnqueueRetryLocked(const SceNpId *npId)
{
	for (paf::list<SceNpId>::iterator it = m_retryQueue.begin();
	     it != m_retryQueue.end(); ++it) {
		if (sceNpCmpOnlineId(&(*it), npId) == 0)
			return;
	}
	m_retryQueue.push_back(*npId);
}

/* 0x813C7BB4 */
void NpFriendProfileImporter::AddPendingRetry(const SceNpId *npId)
{
	m_lock.Lock();
	EnqueueRetryLocked(npId);
	m_lock.Unlock();
}

/* 0x813C7C10 */
int NpFriendProfileImporter::FetchProfileFromServer(const SceNpId *selfNpId,
						    const SceNpId *targetNpId,
						    NpLookupProfile *outProfile,
						    SceUInt32 avatarSizeType)
{
	int titleCtx = npUtilityProfileCreateTitleCtx(NULL, selfNpId);
	if (titleCtx < 0)
		return titleCtx;

	int req = npUtilityProfileCreateRequest(titleCtx);
	if (req < 0) {
		npUtilityProfileDestroyTitleCtx(titleCtx);
		return req;
	}

	int ret = npUtilityProfileGetUserProfile(req, avatarSizeType, 2, targetNpId, outProfile,
						 NULL, 0, NULL, NULL, 0, NULL, NULL);
	int destroyReqRet = npUtilityProfileDestroyRequest(req);
	if (ret == 0 && destroyReqRet < 0)
		ret = destroyReqRet;

	int destroyCtxRet = npUtilityProfileDestroyTitleCtx(titleCtx);
	if (ret == 0 && destroyCtxRet < 0)
		ret = destroyCtxRet;
	return ret;
}

/* 0x813C7C8A */
int NpFriendProfileImporter::ImportProfile(const SceNpId *npId, int requestDirection)
{
	int ret = EnsureSelfNpId();
	if (ret >= 0) {
		NpLookupProfile lookup;
		sce_paf_memset(&lookup, 0, sizeof(lookup));
		ret = FetchProfileFromServer(&m_selfNpId, npId, &lookup, 3);
		if (ret >= 0) {
			int msgFlag = 0;
			if (requestDirection == 1)
				msgFlag = 2;
			else if (requestDirection == 0 || requestDirection == -1)
				msgFlag = 1;

			char aboutBuf[64];
			char avatarBuf[132];
			sce_paf_memset(aboutBuf, 0, sizeof(aboutBuf));
			sce_paf_memset(avatarBuf, 0, sizeof(avatarBuf));
			sce_paf_memcpy(aboutBuf, lookup.aboutMe.data, 64);
			sce_paf_memcpy(avatarBuf, lookup.icon.data, 128);

			NpFriendProfile profile;
			sce_paf_memset(&profile, 0, sizeof(profile));
			profile.avatarUrl = avatarBuf;
			profile.aboutMe = aboutBuf;
			if (lookup.panel.color.panel.type == 2) {
				profile.panelUrl = lookup.panel.panelUrl;
				profile.color = lookup.panel.color.panel.bgColor;
			} else if (lookup.panel.color.panel.type == 1) {
				profile.color = lookup.panel.color.panel.bgColor;
			}
			profile.language1 = static_cast<SceUInt8>(lookup.languages.language1);
			profile.language2 = static_cast<SceUInt8>(lookup.languages.language2);
			profile.language3 = static_cast<SceUInt8>(lookup.languages.language3);
			profile.plus = static_cast<unsigned int>(lookup.plusIcon);

			ret = m_storage->UpdateProfile(lookup.npId.handle.data, &profile, msgFlag);
			if (ret >= 0) {
				if (npId != NULL) {
					ret = npFriendCacheDownloadAvatar(lookup.icon.data);
					if (ret >= 0 && lookup.panel.color.panel.type == 2 &&
					    sce_paf_strlen(lookup.panel.panelUrl) != 0) {
						ret = npFriendCacheDownloadAvatar(
							lookup.panel.panelUrl);
					}
				} else {
					ret = NP_FRIEND_IMPORTER_ERR_INVALID_STATE;
				}
				if (ret >= 0)
					return ret;
			}
		}
	}

	AddPendingRetry(npId);
	return ret;
}

/* 0x813C7E9E */
int NpFriendProfileImporter::ImportProfiles(unsigned int count,
					    NpFriendPlayerImpl *const *players)
{
	int selfRet = EnsureSelfNpId();
	if (selfRet < 0) {
		for (unsigned int index = 0; index < count; ++index)
			AddPendingRetry(players[index]->GetNpId());
		return 0;
	}

	for (unsigned int index = 0; index < count; ++index) {
		int ret = ImportProfile(players[index]->GetNpId(),
					players[index]->GetRequestDirection());
		if (ret < 0) {
			for (unsigned int remain = index; remain < count; ++remain)
				AddPendingRetry(players[remain]->GetNpId());
			return ret;
		}
	}
	return 0;
}

/* 0x813C804E */
void NpFriendProfileImporter::RetryPending(void)
{
	m_lock.Lock();
	while (m_retryQueue.size() != 0) {
		SceNpId npId = m_retryQueue.front();
		m_retryQueue.pop_front();
		m_lock.Unlock();

		NpFriendImporter *importer = NULL;
		paf::common::SharedPtr<paf::job::JobItem> job;
		if (NpFriendImporter::GetInstance(&importer) < 0 ||
		    npFriendImportProfileJobCreate(&job, &npId, -1) < 0 ||
		    importer->EnqueueProfileJob(job) < 0) {
			AddPendingRetry(&npId);
			return;
		}

		m_lock.Lock();
	}
	m_lock.Unlock();
}

/* 0x813C837C */
const SceNpId *NpFriendProfileImporter::GetSelfNpId(void) const
{
	return &m_selfNpId;
}

/* 0x813C8380 */
int NpFriendProfileImporter::RefreshSelfProfile(void)
{
	int ret = EnsureSelfNpId();
	if (ret < 0)
		return ret;

	SceNpId selfCopy;
	sce_paf_memcpy(&selfCopy, &m_selfNpId, sizeof(selfCopy));

	NpLookupProfile lookup;
	sce_paf_memset(&lookup, 0, sizeof(lookup));
	ret = FetchProfileFromServer(&selfCopy, &selfCopy, &lookup, 2);
	if (ret < 0)
		return ret;

	ret = npFriendCacheDownloadAvatar(lookup.icon.data);
	if (ret >= 0 && lookup.panel.color.panel.type == 2 &&
	    sce_paf_strlen(lookup.panel.panelUrl) != 0) {
		ret = npFriendCacheDownloadAvatar(lookup.panel.panelUrl);
	}
	return ret;
}

/* 0x813C84D8 */
void NpFriendProfileImporter::MarkSelfRetryPending(void)
{
	m_selfRetryPending = true;
}

/* 0x813C84E0 */
int NpFriendProfileImporter::RetrySelfProfile(void)
{
	if (!m_selfRetryPending)
		return 0;

	int ret = RefreshSelfProfile();
	if (ret >= 0) {
		m_selfRetryPending = false;
		return 0;
	}
	return ret;
}
