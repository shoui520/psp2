/*
 * SceNpSnsFacebookDialogPlugin: FbEventBus, FbMutex, FbScopedLock, FbDialogJob subclasses,
 * and FbJobManager.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <std/new>
#include <std/string.h>
#include <std/stdio.h>
#include <common/shared_ptr.h>
#include <framework.h>
#include <job/job.h>
#include "fb_dialog_plugin.h"
#include "fb_dialog_job.h"
#include "fb_dialog_controller.h"

using namespace paf;

/* SceNpSnsFacebook_4A453D97 output buffer (0x1810 bytes) */
struct FbPermissionDescResponse {
	int32_t count;
	FbPermissionDesc items[12];
};

/* SceNpSnsFacebook_CB2ED352 output buffer (0x204 bytes) */
struct FbCheckPermResponse {
	int32_t reserved;
	char granted[256];
	char missing[256];
};

/* Short token output from SceNpSnsFacebook_B22D89FD (0x108 bytes) */
struct FbShortTokenResponse {
	SceUInt64 expiration;
	char accessToken[256];
};

/* Event bus */
FbEventBus::FbEventBus() : m_listeners()
{
}

FbEventBus::~FbEventBus()
{
}

void FbEventBus::PostEvent(int event, void *arg)
{
	for (size_t i = 0; i < m_listeners.size(); ++i) {
		if (m_listeners[i] != NULL)
			m_listeners[i]->OnEvent(event, arg);
	}
}

int FbEventBus::AddListener(FbEventListener *listener)
{
	for (size_t i = 0; i < m_listeners.size(); ++i) {
		if (m_listeners[i] == listener)
			return FB_DIALOG_ERROR_INVALID_STATE;
	}
	m_listeners.push_back(listener);
	return 0;
}

int FbEventBus::RemoveListener(FbEventListener *listener)
{
	for (vector<FbEventListener *>::iterator it = m_listeners.begin(); it != m_listeners.end();
	     ++it) {
		if (*it == listener) {
			m_listeners.erase(it);
			return 0;
		}
	}
	return FB_DIALOG_ERROR_INVALID_STATE;
}

/* Lightweight mutex wrapper */
FbMutex::FbMutex(const char *name)
{
	sceKernelCreateLwMutex(&m_mutex, name, 2, 0, NULL);
}

FbMutex::~FbMutex()
{
	sceKernelDeleteLwMutex(&m_mutex);
}

int FbMutex::Lock()
{
	return sceKernelLockLwMutex(&m_mutex, 1, NULL);
}

int FbMutex::Unlock()
{
	return sceKernelUnlockLwMutex(&m_mutex, 1);
}

/* Scoped lock */
FbScopedLock::FbScopedLock(FbMutex *mutex) : m_mutex(mutex)
{
	if (m_mutex != NULL)
		m_mutex->Lock();
}

FbScopedLock::~FbScopedLock()
{
	if (m_mutex != NULL)
		m_mutex->Unlock();
}

/* Base FbDialogJob (also serves as sync barrier job) */
FbDialogJob::FbDialogJob(FbEventBus *bus)
	: job::JobItem("FbDialogJob", NULL), m_bus(bus), m_requestId(0)
{
}

FbDialogJob::~FbDialogJob()
{
}

void FbDialogJob::Cancel()
{
	job::JobItem::Cancel();
}

int FbDialogJob::Run()
{
	return 0;
}

void FbDialogJob::Finish(int result)
{
	int res = result;

	m_bus->PostEvent(5, &res);
}

void FbDialogJob::SetRequestId(int reqId)
{
	m_requestId = reqId;
}

int FbDialogJob::CreateRequest(int index)
{
	int reqId = SceNpSnsFacebook_A9C661E9(index);

	if (reqId > 0)
		SetRequestId(reqId);
	return reqId;
}

int FbDialogJob::AbortRequest(int index)
{
	if (m_requestId > 0)
		return SceNpSnsFacebook_46C23078(index, m_requestId);
	return 0;
}

/* Job 0: Check PSN sign-in and chat restriction */
class FbAuthCheckJob : public FbDialogJob {
public:
	__attribute__((noinline)) FbAuthCheckJob(FbEventBus *bus) : FbDialogJob(bus), m_signedIn(0), m_chatAllowed(0)
	{
		int reqId = SceNpSnsFacebook_A9C661E9(0);

		if (reqId > 0)
			SetRequestId(reqId);
	}

	virtual ~FbAuthCheckJob() {}

	virtual void Cancel()
	{
		job::JobItem::Cancel();
		AbortRequest(0);
	}

	virtual int Run()
	{
		int ret;

		if (m_requestId <= 0) {
			ret = FB_DIALOG_ERROR_INVALID_STATE;
			SceNpSnsFacebook_C9ACC1C1(m_requestId);
			return ret;
		}
		ret = SceNpSnsFacebook_6A170F5D(m_requestId, &m_signedIn, &m_chatAllowed);
		if (ret < 0) {
			SceNpSnsFacebook_C9ACC1C1(m_requestId);
			return ret;
		}
		return SceNpSnsFacebook_C9ACC1C1(m_requestId);
	}

	virtual void Finish(int result)
	{
		int args[3];

		args[0] = result;
		args[1] = m_signedIn;
		args[2] = m_chatAllowed;
		m_bus->PostEvent(0, args);
	}

	int m_signedIn;
	int m_chatAllowed;
};

/* Job 1: Bind/login Facebook account */
class FbBindJob : public FbDialogJob {
public:
	__attribute__((noinline)) FbBindJob(FbEventBus *bus, int step, const FbAccountId *account, const char *password,
		  int index)
		: FbDialogJob(bus), m_step(step), m_index(index), m_hasCreds(0)
	{
		if (account != NULL && password != NULL) {
			m_hasCreds = 1;
			sce_paf_memcpy(&m_account, account, sizeof(m_account));
			sce_paf_memcpy(m_password, password, sizeof(m_password));
		} else {
			m_hasCreds = 0;
			sce_paf_memset(&m_account, 0, sizeof(m_account));
			sce_paf_memset(m_password, 0, sizeof(m_password));
		}
		int reqId = SceNpSnsFacebook_A9C661E9(m_index);

		if (reqId > 0)
			SetRequestId(reqId);
	}

	virtual ~FbBindJob() {}

	virtual void Cancel()
	{
		job::JobItem::Cancel();
		AbortRequest(m_index);
	}

	virtual int Run()
	{
		int ret;

		if (m_requestId <= 0) {
			ret = FB_DIALOG_ERROR_INVALID_STATE;
			SceNpSnsFacebook_8770E114(m_index, m_requestId);
			return ret;
		}
		ret = SceNpSnsFacebook_ABF56333(m_index, m_requestId, m_account.email, m_password);
		if (ret < 0) {
			SceNpSnsFacebook_8770E114(m_index, m_requestId);
			return ret;
		}
		return SceNpSnsFacebook_8770E114(m_index, m_requestId);
	}

	virtual void Finish(int result)
	{
		int args[2];

		args[0] = result;
		args[1] = m_step;
		m_bus->PostEvent(1, args);
	}

	int m_step;
	int m_index;
	int m_hasCreds;
	FbAccountId m_account;
	char m_password[256];
};

/* Job 2: Get token + check missing permissions + fetch permission descriptions */
class FbCheckPermissionsJob : public FbDialogJob {
public:
	__attribute__((noinline)) FbCheckPermissionsJob(FbEventBus *bus, FbMutex *mutex, const char *titleId, SceUInt64 appId,
			      const char *permissions, vector<FbPermissionDesc> *permListOut,
			      char *missingPermsOut,
			      SceNpSnsFacebookLongAccessTokenResult *tokenOut, int shortMode)
		: FbDialogJob(bus), m_mutex(mutex), m_appId(appId), m_shortMode(shortMode),
		  m_needPerms(0), m_permListOut(permListOut), m_missingPermsOut(missingPermsOut),
		  m_tokenOut(tokenOut)
	{
		sce_paf_memset(m_titleId, 0, sizeof(m_titleId));
		sce_paf_strncpy(m_titleId, titleId, sizeof(m_titleId) - 1);
		sce_paf_memset(m_permissions, 0, sizeof(m_permissions));
		sce_paf_strncpy(m_permissions, permissions, sizeof(m_permissions) - 1);
		if (m_tokenOut != NULL && m_mutex != NULL) {
			int reqId = SceNpSnsFacebook_A9C661E9(0);

			if (reqId > 0)
				SetRequestId(reqId);
		}
	}

	virtual ~FbCheckPermissionsJob() {}

	virtual void Cancel()
	{
		FbScopedLock lock(m_mutex);

		job::JobItem::Cancel();
		AbortRequest(0);
	}

	virtual int Run()
	{
		FbCheckPermResponse checkResp;
		FbPermissionDescResponse descResp;
		FbShortTokenResponse shortTok;
		char appIdStr[32];
		int ret;

		sce_paf_memset(&checkResp, 0, sizeof(checkResp));
		sce_paf_memset(&descResp, 0, sizeof(descResp));
		sce_paf_memset(&shortTok, 0, sizeof(shortTok));

		do {
			{
				FbScopedLock lock(m_mutex);

				if (IsCanceled()) {
					ret = SNSFB_ERR_ABORTED;
					break;
				}
				if (m_requestId <= 0) {
					ret = FB_DIALOG_ERROR_INVALID_STATE;
					break;
				}
			}

			sce_paf_memset(appIdStr, 0, sizeof(appIdStr));
			sce_paf_snprintf(appIdStr, sizeof(appIdStr), "%llu", m_appId);

			if (m_shortMode != 0) {
				ret = SceNpSnsFacebook_B22D89FD(m_requestId, appIdStr, m_titleId,
								NULL, &shortTok);
				if (ret < 0)
					break;
				sce_paf_memset(m_tokenOut, 0, sizeof(*m_tokenOut));
				m_tokenOut->expiration = shortTok.expiration;
				sce_paf_strncpy(m_tokenOut->accessToken, shortTok.accessToken, 255);
			} else {
				ret = SceNpSnsFacebook_FD842B21(m_requestId, appIdStr, m_titleId,
								NULL, m_tokenOut);
			}
			if (ret < 0)
				break;

			ret = SceNpSnsFacebook_C9ACC1C1(m_requestId);
			if (ret < 0)
				break;
			m_requestId = 0;

			if (m_permissions[0] == '\0') {
				m_needPerms = 0;
				if (IsCanceled())
					ret = SNSFB_ERR_ABORTED;
				break;
			}

			{
				FbScopedLock lock(m_mutex);

				if (IsCanceled()) {
					ret = SNSFB_ERR_ABORTED;
					break;
				}
				ret = CreateRequest(0);
				if (ret < 0)
					break;
			}

			ret = SceNpSnsFacebook_CB2ED352(m_requestId, m_tokenOut, m_permissions,
							&checkResp);
			if (ret < 0)
				break;

			ret = SceNpSnsFacebook_C9ACC1C1(m_requestId);
			if (ret < 0)
				break;
			m_requestId = 0;

			if (sceClibStrnlen(checkResp.missing, sizeof(checkResp.missing)) != 0) {
				m_needPerms = 1;
				sce_paf_strncpy(m_missingPermsOut, checkResp.missing, 255);
			}

			if (m_needPerms != 0) {
				const char *cursor = checkResp.missing;
				int done = 0;

				while (!done) {
					{
						FbScopedLock lock(m_mutex);

						if (IsCanceled()) {
							ret = SNSFB_ERR_ABORTED;
							break;
						}
						ret = CreateRequest(0);
						if (ret < 0)
							break;
					}

					int locale = 1;
					Framework *fw = Framework::Instance();

					if (fw != NULL)
						locale = fw->GetLocale();
					ret = SceNpSnsFacebook_4A453D97(m_requestId, locale,
									m_tokenOut, cursor,
									&descResp);
					if (ret < 0)
						break;
					ret = SceNpSnsFacebook_C9ACC1C1(m_requestId);
					if (ret < 0)
						break;
					m_requestId = 0;

					for (int i = 0; i < descResp.count; ++i) {
						FbPermissionDesc item;

						sce_paf_strncpy(item.name, descResp.items[i].name,
								255);
						sce_paf_strncpy(item.desc, descResp.items[i].desc,
								255);
						item.flag = descResp.items[i].flag;
						m_permListOut->push_back(item);
					}

					int skipped = 0;

					while (skipped < 12) {
						int len = 0;

						while (cursor[len] != '\0' &&
						       (sceClibLookCtypeTable(cursor[len]) & 8) ==
							       0 &&
						       cursor[len] != ',')
							++len;
						if (cursor[len] == '\0') {
							done = 1;
							break;
						}
						while ((sceClibLookCtypeTable(cursor[len]) & 8) !=
							       0 ||
						       cursor[len] == ',')
							++len;
						if (cursor[len] == '\0') {
							done = 1;
							break;
						}
						cursor += len;
						++skipped;
					}
				}
				if (ret < 0)
					break;
			}

			if (IsCanceled())
				ret = SNSFB_ERR_ABORTED;
		} while (0);

		if (m_requestId > 0)
			SceNpSnsFacebook_C9ACC1C1(m_requestId);
		return ret;
	}

	virtual void Finish(int result)
	{
		int args[2];

		args[0] = result;
		args[1] = m_needPerms;
		m_bus->PostEvent(2, args);
	}

	FbMutex *m_mutex;
	char m_titleId[10];
	SceUInt64 m_appId;
	char m_permissions[256];
	int m_shortMode;
	int m_needPerms;
	vector<FbPermissionDesc> *m_permListOut;
	char *m_missingPermsOut;
	SceNpSnsFacebookLongAccessTokenResult *m_tokenOut;
};

/* Job 3: Get access token with permissions */
class FbAccessTokenJob : public FbDialogJob {
public:
	__attribute__((noinline)) FbAccessTokenJob(FbEventBus *bus, const char *titleId, SceUInt64 appId,
			 const char *permissions,
			 SceNpSnsFacebookLongAccessTokenResult *tokenOut, int shortMode)
		: FbDialogJob(bus), m_appId(appId), m_shortMode(shortMode), m_tokenOut(tokenOut)
	{
		sce_paf_memset(m_titleId, 0, sizeof(m_titleId));
		sce_paf_strncpy(m_titleId, titleId, sizeof(m_titleId) - 1);
		sce_paf_memset(m_permissions, 0, sizeof(m_permissions));
		sce_paf_strncpy(m_permissions, permissions, sizeof(m_permissions) - 1);
		if (m_tokenOut != NULL) {
			int reqId = SceNpSnsFacebook_A9C661E9(0);

			if (reqId > 0)
				SetRequestId(reqId);
		}
	}

	virtual ~FbAccessTokenJob() {}

	virtual void Cancel()
	{
		job::JobItem::Cancel();
		AbortRequest(0);
	}

	virtual int Run()
	{
		char appIdStr[32];
		FbShortTokenResponse shortTok;
		int ret;

		sce_paf_memset(&shortTok, 0, sizeof(shortTok));
		do {
			if (m_requestId <= 0) {
				ret = FB_DIALOG_ERROR_INVALID_STATE;
				break;
			}
			sce_paf_memset(appIdStr, 0, sizeof(appIdStr));
			sce_paf_snprintf(appIdStr, sizeof(appIdStr), "%llu", m_appId);
			if (m_shortMode != 0) {
				ret = SceNpSnsFacebook_B22D89FD(m_requestId, appIdStr, m_titleId,
								m_permissions, &shortTok);
				if (ret < 0)
					break;
				sce_paf_memset(m_tokenOut, 0, sizeof(*m_tokenOut));
				m_tokenOut->expiration = shortTok.expiration;
				sce_paf_strncpy(m_tokenOut->accessToken, shortTok.accessToken, 255);
			} else {
				ret = SceNpSnsFacebook_FD842B21(m_requestId, appIdStr, m_titleId,
								m_permissions, m_tokenOut);
			}
			if (ret >= 0) {
				ret = SceNpSnsFacebook_C9ACC1C1(m_requestId);
				if (ret >= 0) {
					m_requestId = 0;
					return ret;
				}
			}
		} while (0);
		if (m_requestId > 0)
			SceNpSnsFacebook_C9ACC1C1(m_requestId);
		return ret;
	}

	virtual void Finish(int result)
	{
		int res = result;

		m_bus->PostEvent(3, &res);
	}

	char m_titleId[10];
	SceUInt64 m_appId;
	char m_permissions[256];
	int m_shortMode;
	SceNpSnsFacebookLongAccessTokenResult *m_tokenOut;
};

/* Job 4a: Single feed post */
class FbPostFeedJob : public FbDialogJob {
public:
	__attribute__((noinline)) FbPostFeedJob(FbEventBus *bus, int index, int postType, int visibility, const char *story,
		      const char *caption, const char *comment, int rating, int trophyGrade,
		      int trophyFlags, int activityFlags)
		: FbDialogJob(bus), m_index(index), m_postType(postType), m_visibility(visibility),
		  m_rating(rating), m_trophyGrade(trophyGrade), m_trophyFlags(trophyFlags),
		  m_activityFlags(activityFlags), m_subResult(0)
	{
		sce_paf_memset(m_story, 0, sizeof(m_story));
		if (story != NULL)
			sce_paf_strncpy(m_story, story, sizeof(m_story) - 1);
		sce_paf_memset(m_caption, 0, sizeof(m_caption));
		if (caption != NULL)
			sce_paf_strncpy(m_caption, caption, sizeof(m_caption) - 1);
		sce_paf_memset(m_comment, 0, sizeof(m_comment));
		if (comment != NULL)
			sce_paf_strncpy(m_comment, comment, sizeof(m_comment) - 1);
		int reqId = SceNpSnsFacebook_A9C661E9(m_index);

		if (reqId > 0)
			SetRequestId(reqId);
	}

	virtual ~FbPostFeedJob() {}

	virtual void Cancel()
	{
		job::JobItem::Cancel();
		AbortRequest(m_index);
	}

	virtual int Run()
	{
		int ret = 0;

		do {
			if (m_requestId <= 0) {
				ret = FB_DIALOG_ERROR_INVALID_STATE;
				break;
			}
			if (m_postType == 1) {
				typedef int (*PostTrophyFn)(int, int, int, const char *,
							    const char *, int, const char *, int,
							    int, int *);
				ret = ((PostTrophyFn)SceNpSnsFacebook_9DE0B1BC)(
					m_index, m_requestId, m_visibility, m_story, m_caption,
					m_rating, m_comment, m_trophyGrade, m_trophyFlags,
					&m_subResult);
				if (ret < 0)
					break;
			} else if (m_postType == 2) {
				typedef int (*PostActivityFn)(int, int, int, const char *,
							      const char *, int, int *);
				ret = ((PostActivityFn)SceNpSnsFacebook_C10D390C)(
					m_index, m_requestId, m_visibility, m_story, m_comment,
					m_activityFlags, &m_subResult);
				if (ret < 0)
					break;
			} else {
				break;
			}
			ret = SceNpSnsFacebook_8770E114(m_index, m_requestId);
			if (ret >= 0) {
				m_requestId = 0;
				return ret;
			}
		} while (0);
		if (m_requestId > 0)
			SceNpSnsFacebook_8770E114(m_index, m_requestId);
		return ret;
	}

	virtual void Finish(int result)
	{
		int args[2];

		args[0] = result;
		args[1] = m_subResult;
		m_bus->PostEvent(4, args);
	}

	int m_index;
	int m_postType;
	int m_visibility;
	char m_story[257];
	char m_caption[181];
	int m_rating;
	char m_comment[257];
	int m_trophyGrade;
	int m_trophyFlags;
	int m_activityFlags;
	int m_subResult;
};

/* Job 4b: Batch feed post (up to 16 items) */
class FbBatchPostJob : public FbDialogJob {
public:
	__attribute__((noinline)) FbBatchPostJob(FbEventBus *bus, FbMutex *mutex, int index,
						 int postType, int count,
						 const FbFeedBatchTrophyItem *trophyItems,
						 const FbFeedBatchActivityItem *activityItems)
		: FbDialogJob(bus), m_mutex(mutex), m_index(index), m_postType(postType),
		  m_count(count), m_trophyItems(trophyItems), m_activityItems(activityItems),
		  m_subResult(0)
	{
	}

	virtual ~FbBatchPostJob() {}

	virtual void Cancel()
	{
		FbScopedLock lock(m_mutex);

		job::JobItem::Cancel();
		AbortRequest(m_index);
	}

	virtual int Run()
	{
		int ret = 0;

		if (m_postType == 3) {
			if (m_count <= 0 || m_count > 16 || m_trophyItems == NULL)
				return FB_DIALOG_ERROR_INVALID_STATE;
		} else if (m_postType == 4) {
			if (m_count <= 0 || m_count > 16 || m_activityItems == NULL)
				return FB_DIALOG_ERROR_INVALID_STATE;
		} else {
			return FB_DIALOG_ERROR_INVALID_STATE;
		}

		for (int i = 0; i < m_count; ++i) {
			{
				FbScopedLock lock(m_mutex);

				if (IsCanceled()) {
					ret = SNSFB_ERR_ABORTED;
					break;
				}
				ret = CreateRequest(m_index);
				if (ret < 0)
					break;
			}
			if (m_postType == 3) {
				typedef int (*PostTrophyFn)(int, int, int, const char *,
							    const char *, int, const char *, int,
							    int, int *);
				const FbFeedBatchTrophyItem *it = &m_trophyItems[i];

				ret = ((PostTrophyFn)SceNpSnsFacebook_9DE0B1BC)(
					m_index, m_requestId, it->visibility, it->story,
					it->caption, it->rating, it->comment, it->trophyGrade,
					it->trophyFlags, &m_subResult);
				if (ret < 0)
					break;
			} else if (m_postType == 4) {
				typedef int (*PostActivityFn)(int, int, int, const char *,
							      const char *, int, int *);
				const FbFeedBatchActivityItem *it = &m_activityItems[i];

				ret = ((PostActivityFn)SceNpSnsFacebook_C10D390C)(
					m_index, m_requestId, it->visibility, it->story,
					it->comment, it->activityFlags, &m_subResult);
				if (ret < 0)
					break;
			} else {
				ret = FB_DIALOG_ERROR_INVALID_STATE;
				break;
			}
			ret = SceNpSnsFacebook_8770E114(m_index, m_requestId);
			if (ret < 0)
				break;
			m_requestId = 0;
		}

		if (ret >= 0 && IsCanceled())
			ret = SNSFB_ERR_ABORTED;
		if (m_requestId > 0)
			SceNpSnsFacebook_8770E114(m_index, m_requestId);
		return ret;
	}

	virtual void Finish(int result)
	{
		int args[2];

		args[0] = result;
		args[1] = m_subResult;
		m_bus->PostEvent(4, args);
	}

	FbMutex *m_mutex;
	int m_index;
	int m_postType;
	int m_count;
	const FbFeedBatchTrophyItem *m_trophyItems;
	const FbFeedBatchActivityItem *m_activityItems;
	int m_subResult;
};

/* FbJobManager */
FbJobManager::FbJobManager(FbEventBus *bus, job::JobQueue *queue)
	: m_aborting(0), m_bus(bus), m_queue(queue), m_busy(0), m_reserved(0), m_mutex(NULL)
{
	m_bus->AddListener(this);
	m_mutex = new FbMutex("SceFbDialogJobLock");
}

FbJobManager::~FbJobManager()
{
	m_bus->RemoveListener(this);
	if (m_mutex != NULL) {
		delete m_mutex;
		m_mutex = NULL;
	}
}

int FbJobManager::CancelCurrent()
{
	if (m_busy == 1) {
		m_queue->CancelAllItems();
		return 0;
	}
	if (m_busy == 0)
		return 0;
	return FB_DIALOG_ERROR_INVALID_STATE;
}

void FbJobManager::Abort()
{
	m_aborting = 1;
	if (m_busy == 1)
		m_queue->CancelAllItems();
}

int FbJobManager::StartAuthCheck()
{
	if (m_busy != 0)
		return FB_DIALOG_ERROR_INVALID_STATE;
	common::SharedPtr<job::JobItem> item(new FbAuthCheckJob(m_bus));

	m_queue->Enqueue(item);
	m_busy = 1;
	return 0;
}

int FbJobManager::StartBind(int step, const FbAccountId *account, const char *password, int index)
{
	if (m_busy != 0)
		return FB_DIALOG_ERROR_INVALID_STATE;
	common::SharedPtr<job::JobItem> item(new FbBindJob(m_bus, step, account, password, index));

	m_queue->Enqueue(item);
	m_busy = 1;
	return 0;
}

int FbJobManager::StartCheckPermissions(const char *titleId, SceUInt64 appId,
					const char *permissions,
					vector<FbPermissionDesc> *permListOut,
					char *missingPermsOut,
					SceNpSnsFacebookLongAccessTokenResult *tokenOut,
					int shortMode)
{
	if (m_busy != 0)
		return FB_DIALOG_ERROR_INVALID_STATE;
	common::SharedPtr<job::JobItem> item(new FbCheckPermissionsJob(
		m_bus, m_mutex, titleId, appId, permissions, permListOut, missingPermsOut, tokenOut,
		shortMode));

	m_queue->Enqueue(item);
	m_busy = 1;
	return 0;
}

int FbJobManager::StartGetAccessToken(const char *titleId, SceUInt64 appId, const char *permissions,
				      SceNpSnsFacebookLongAccessTokenResult *tokenOut,
				      int shortMode)
{
	if (m_busy != 0)
		return FB_DIALOG_ERROR_INVALID_STATE;
	common::SharedPtr<job::JobItem> item(
		new FbAccessTokenJob(m_bus, titleId, appId, permissions, tokenOut, shortMode));

	m_queue->Enqueue(item);
	m_busy = 1;
	return 0;
}

int FbJobManager::StartPostFeed(int index, int postType, int visibility, const char *story,
				const char *caption, const char *comment, int rating,
				int trophyGrade, int trophyFlags, int activityFlags)
{
	if (m_busy != 0)
		return FB_DIALOG_ERROR_INVALID_STATE;
	common::SharedPtr<job::JobItem> item(new FbPostFeedJob(
		m_bus, index, postType, visibility, story, caption, comment, rating, trophyGrade,
		trophyFlags, activityFlags));

	m_queue->Enqueue(item);
	m_busy = 1;
	return 0;
}

int FbJobManager::StartBatchPost(int index, int postType, int count,
				 const FbFeedBatchTrophyItem *trophyItems,
				 const FbFeedBatchActivityItem *activityItems)
{
	if (m_busy != 0)
		return FB_DIALOG_ERROR_INVALID_STATE;
	common::SharedPtr<job::JobItem> item(new FbBatchPostJob(
		m_bus, m_mutex, index, postType, count, trophyItems, activityItems));

	m_queue->Enqueue(item);
	m_busy = 1;
	return 0;
}

int FbJobManager::StartSyncBarrier()
{
	if (m_busy != 0)
		return FB_DIALOG_ERROR_INVALID_STATE;
	common::SharedPtr<job::JobItem> item(new FbDialogJob(m_bus));

	m_queue->Enqueue(item);
	m_busy = 1;
	return 0;
}

int FbJobManager::OnEvent(int event, void *arg)
{
	if (arg == NULL)
		return 0;
	int *a = (int *)arg;
	/* Dispatch to owning controller (FbDialogBase virtual callbacks) */
	switch (event) {
	case 0:
		m_busy = 0;
		return m_owner->OnJobAuthCheckDone(a[0], a[1], a[2]);
	case 1:
		m_busy = 0;
		return m_owner->OnJobBindDone(a[0], a[1]);
	case 2:
		m_busy = 0;
		return m_owner->OnJobCheckPermissionsDone(a[0], a[1]);
	case 3:
		m_busy = 0;
		return m_owner->OnJobGetAccessTokenDone(a[0]);
	case 4:
		m_busy = 0;
		return m_owner->OnJobPostFeedDone(a[0], a[1]);
	case 5:
		m_busy = 0;
		return m_owner->OnJobSyncDone();
	default:
		return 0;
	}
}
