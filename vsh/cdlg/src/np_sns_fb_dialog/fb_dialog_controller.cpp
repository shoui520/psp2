/*
 * SceNpSnsFacebookDialogPlugin: FbDialogBase, FbDialogControllerBase, FbAccessTokenController,
 * FbFeedController, and FbLongAccessTokenController.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <std/new>
#include <std/string.h>
#include <std/vector>
#include <math/ngp/v4.h>
#include <plugin.h>
#include <job/job.h>
#include "fb_dialog_plugin.h"
#include "fb_dialog_job.h"
#include "fb_dialog_view_mgr.h"
#include "fb_dialog_controller.h"

using namespace paf;

/* SceNpSnsFacebookDialog feed data payload layouts */
typedef struct FbFeedDataHeader {
	int32_t locale;
	int32_t useDefaultBg;
	int32_t postType;
	int32_t countOrVis;
} FbFeedDataHeader;

typedef struct FbFeedTrophyData {
	FbFeedDataHeader hdr;
	char story[257];
	char caption[183];
	int32_t rating;
	char comment[260];
	int32_t trophyGrade;
	int32_t trophyFlags;
} FbFeedTrophyData;

typedef struct FbFeedActivityData {
	FbFeedDataHeader hdr;
	char story[257];
	char comment[259];
	int32_t activityFlags;
} FbFeedActivityData;

typedef struct FbFeedBatchTrophyPayload {
	FbFeedDataHeader hdr;
	FbFeedBatchTrophyItem items[16];
} FbFeedBatchTrophyPayload;

typedef struct FbFeedBatchActivityPayload {
	FbFeedDataHeader hdr;
	FbFeedBatchActivityItem items[16];
} FbFeedBatchActivityPayload;

/* FbDialogBase */
FbDialogBase::FbDialogBase()
	: m_aborting(0), m_finishCallback(NULL), m_children(), m_reserved(0), m_eventBus(),
	  m_viewMgr(NULL), m_jobMgr(NULL), m_jobQueue(NULL), m_result(0), m_internalResult(0)
{
}

FbDialogBase::~FbDialogBase()
{
	m_children.clear();
}

int FbDialogBase::AddChild(void *childPtr)
{
	FbComponent *child = (FbComponent *)childPtr;

	if (child == NULL)
		return FB_DIALOG_ERROR_INVALID_STATE;
	for (size_t i = 0; i < m_children.size(); ++i) {
		if (m_children[i] == child)
			return FB_DIALOG_ERROR_INVALID_STATE;
	}
	m_children.push_back(child);
	child->OnAttach(this);
	return 0;
}

int FbDialogBase::RemoveChild(void *childPtr)
{
	FbComponent *child = (FbComponent *)childPtr;

	for (vector<FbComponent *>::iterator it = m_children.begin(); it != m_children.end(); ++it) {
		if (*it == child) {
			child->OnDetach();
			m_children.erase(it);
			return 0;
		}
	}
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::Start(const FbPluginStartParam *param, Plugin *plugin, FbDialogFinishCallback cb)
{
	(void)param;
	(void)plugin;
	(void)cb;
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::Abort()
{
	return 0;
}

int FbDialogBase::WaitEnd()
{
	return 0;
}

int FbDialogBase::OnFinishTrigger()
{
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::OnJobSyncDone()
{
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::OnBgAction()
{
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::OnLoginCancel()
{
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::OnLoginSubmit(const FbAccountId *account, const char *password)
{
	(void)account;
	(void)password;
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::OnConfirmBindCancel()
{
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::OnConfirmBindAccept()
{
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::OnInputCancel()
{
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::OnInputSubmit(const FbAccountId *account, const char *password)
{
	(void)account;
	(void)password;
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::OnPermissionCancel()
{
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::OnPermissionAccept()
{
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::OnWaitCancel()
{
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::OnWaitClosed()
{
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::OnErrorClosed(int error)
{
	(void)error;
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::OnChatRestrictClosed()
{
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::OnRetryAuthClosed(int step)
{
	(void)step;
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::OnNoticeClosed()
{
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::OnJobAuthCheckDone(int result, int signedIn, int chatAllowed)
{
	(void)result;
	(void)signedIn;
	(void)chatAllowed;
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::OnJobBindDone(int result, int step)
{
	(void)result;
	(void)step;
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::OnJobCheckPermissionsDone(int result, int needPerms)
{
	(void)result;
	(void)needPerms;
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::OnJobGetAccessTokenDone(int result)
{
	(void)result;
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::OnJobPostFeedDone(int result, int subResult)
{
	(void)result;
	(void)subResult;
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogBase::InitComponents(Plugin *plugin)
{
	job::JobQueue::Option opt;

	opt.numThreads = 1;
	opt.priority = 0;
	opt.stackSize = 0x4000;
	m_jobQueue = new job::JobQueue("SceFbDialogJobQueue", &opt);
	if (m_jobQueue == NULL)
		return FB_DIALOG_ERROR_NO_MEMORY;

	m_viewMgr = new FbViewManager(&m_eventBus, plugin);
	if (m_viewMgr == NULL)
		return FB_DIALOG_ERROR_NO_MEMORY;
	int ret = AddChild(m_viewMgr);

	if (ret < 0)
		return ret;

	m_jobMgr = new FbJobManager(&m_eventBus, m_jobQueue);
	if (m_jobMgr == NULL)
		return FB_DIALOG_ERROR_NO_MEMORY;
	return AddChild(m_jobMgr);
}

int FbDialogBase::TermComponents()
{
	if (m_jobQueue != NULL) {
		if (m_jobQueue->NumItems() != 0) {
			m_jobQueue->CancelAllItems();
			m_jobQueue->WaitEmpty();
		}
		delete m_jobQueue;
		m_jobQueue = NULL;
	}
	if (m_jobMgr != NULL) {
		RemoveChild(m_jobMgr);
		delete m_jobMgr;
		m_jobMgr = NULL;
	}
	if (m_viewMgr != NULL) {
		RemoveChild(m_viewMgr);
		delete m_viewMgr;
		m_viewMgr = NULL;
	}
	return 0;
}

/* FbDialogControllerBase (vtable 0x81010FF8) */
FbDialogControllerBase::FbDialogControllerBase()
	: m_state(FB_STATE_IDLE), m_lastJobResult(0), m_lastSubResult(0), m_lastBindStep(0),
	  m_waitingForView(0)
{
}

FbDialogControllerBase::~FbDialogControllerBase()
{
}

void FbDialogControllerBase::SetState(int state)
{
	m_state = state;
}

int FbDialogControllerBase::Abort()
{
	m_aborting = 1;
	m_viewMgr->SetAborting();
	m_jobMgr->Abort();
	m_waitingForView = 0;

	switch (m_state) {
	case FB_STATE_IDLE:
	case FB_STATE_DONE:
	case FB_STATE_LOGIN:
	case FB_STATE_INPUT:
	case FB_STATE_CONFIRM_BIND:
	case FB_STATE_PERMISSION:
		m_viewMgr->CloseAll();
		m_state = FB_STATE_DONE;
		return 1;
	case FB_STATE_AUTH_CHECK:
	case FB_STATE_BIND:
	case FB_STATE_CHECK_PERMISSIONS:
	case FB_STATE_GET_ACCESS_TOKEN:
	case FB_STATE_ERROR:
	case FB_STATE_CHAT_RESTRICT:
	case FB_STATE_RETRY_AUTH:
	case FB_STATE_POST_FEED:
	case FB_STATE_POST_FEED_RETRY:
		return m_viewMgr->CloseAll();
	default:
		m_state = FB_STATE_DONE;
		return 1;
	}
}

int FbDialogControllerBase::WaitEnd()
{
	while (m_state != FB_STATE_DONE)
		sceKernelDelayThread(20000);
	return TermComponents();
}

int FbDialogControllerBase::OnFinishTrigger()
{
	return m_jobMgr->StartSyncBarrier();
}

int FbDialogControllerBase::SetDefaultBgColor()
{
	math::v4 color(74.0f / 255.0f, 73.0f / 255.0f, 86.0f / 255.0f, 1.0f);

	return m_viewMgr->SetBgColor(color);
}

int FbDialogControllerBase::ApplyBgColor(const FbPluginStartParam *param)
{
	if (param->hasBgColor != 0) {
		math::v4 color((float)param->bgColor.r / 255.0f, (float)param->bgColor.g / 255.0f,
			       (float)param->bgColor.b / 255.0f, (float)param->bgColor.a / 255.0f);

		return m_viewMgr->SetBgColor(color);
	}
	math::v4 color(74.0f / 255.0f, 73.0f / 255.0f, 86.0f / 255.0f, 1.0f);

	return m_viewMgr->SetBgColor(color);
}

int FbDialogControllerBase::OnUnexpectedState()
{
	m_state = FB_STATE_IDLE;
	OnFinishTrigger();
	return FB_DIALOG_ERROR_INVALID_STATE;
}

int FbDialogControllerBase::ShowRetryableError(int error, int isAuthFlow)
{
	switch (error) {
	case SNSFB_ERR_FB_14:
		if (isAuthFlow == 0)
			return m_viewMgr->ShowNotice(error);
		return m_viewMgr->ShowRetryServer(error);
	case SNSFB_ERR_FB_15:
	case SNSFB_ERR_FB_16:
	case SNSFB_ERR_FB_17:
	case SNSFB_ERR_FB_18:
		return m_viewMgr->ShowNotice(error);
	default:
		return m_viewMgr->ShowError(error);
	}
}

void FbDialogControllerBase::SetWaitingFlag()
{
	m_waitingForView = 0;
}

int FbDialogControllerBase::ShowNotice(Plugin *plugin)
{
	m_viewMgr->AddView(CreateFbNoticeView(&m_eventBus, plugin));
	m_state = FB_STATE_NOTICE;
	m_viewMgr->ShowNotice((SceInt32)0x80106101);
	return 0;
}

/* FbAccessTokenController (vtable 0x810106C0) */
FbAccessTokenController::FbAccessTokenController()
	: m_shortMode(0), m_fbAppId(0), m_signedIn(0), m_chatAllowed(0), m_needPerms(0),
	  m_permList(), m_permReserved(0), m_enableShareOnBind(0), m_bindReserved(0),
	  m_autoBind(0)
{
	sce_paf_memset(m_profileReserved, 0, sizeof(m_profileReserved));
	sce_paf_memset(&m_account, 0, sizeof(m_account));
	m_account.reserved = 0;
	sce_paf_memset(m_password, 0, sizeof(m_password));
	sce_paf_memset(&m_profile, 0, sizeof(m_profile));
	sce_paf_memset(m_titleId, 0, sizeof(m_titleId));
	sce_paf_memset(m_titleName, 0, sizeof(m_titleName));
	sce_paf_memset(m_permissions, 0, sizeof(m_permissions));
	sce_paf_memset(m_missingPermissions, 0, sizeof(m_missingPermissions));
	sce_paf_memset(&m_token, 0, sizeof(m_token));
}

FbAccessTokenController::~FbAccessTokenController()
{
}

int FbAccessTokenController::Start(const FbPluginStartParam *param, Plugin *plugin,
				   FbDialogFinishCallback cb)
{
	if (m_state != FB_STATE_IDLE)
		return FB_DIALOG_ERROR_INVALID_STATE;
	m_finishCallback = cb;
	int ret = InitComponents(plugin);

	if (ret < 0)
		return ret;

	m_viewMgr->AddView(CreateFbBgView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbLoginView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbConfirmBindView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbInputShortView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbInputLongView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbPermissionView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbWaitView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbErrorView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbChatRestrictView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbFeedConfirmView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbRetryAuthView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbRetryServerView(&m_eventBus, plugin));

	ret = ApplyBgColor(param);
	if (ret < 0)
		return ret;
	return ShowNotice(plugin);
}

int FbAccessTokenController::OnJobSyncDone()
{
	if (m_aborting != 0) {
		SetState(FB_STATE_DONE);
		return 0;
	}
	if (m_finishCallback == NULL) {
		SetState(FB_STATE_DONE);
		return FB_DIALOG_ERROR_INVALID_STATE;
	}
	SceNpSnsFacebookLongAccessTokenResult out;

	sce_paf_memset(&out, 0, sizeof(out));
	if (m_result == 0)
		sce_paf_memcpy(&out, &m_token, sizeof(out));
	m_finishCallback(m_result, m_internalResult, &out);
	SetState(FB_STATE_DONE);
	return 0;
}

int FbAccessTokenController::OnBgAction()
{
	return m_viewMgr->ShowLogin(m_titleName, NULL, NULL);
}

int FbAccessTokenController::OnLoginCancel()
{
	if (m_state != FB_STATE_LOGIN)
		return OnUnexpectedState();
	m_result = 1;
	SetState(FB_STATE_IDLE);
	return OnFinishTrigger();
}

int FbAccessTokenController::OnLoginSubmit(const FbAccountId *account, const char *password)
{
	if (m_state != FB_STATE_LOGIN)
		return OnUnexpectedState();
	m_enableShareOnBind = 1;
	sce_paf_strncpy(m_account.email, account->email, sizeof(m_account.email));
	m_account.email[sizeof(m_account.email) - 1] = '\0';
	sce_paf_strncpy(m_password, password, sizeof(m_password));
	m_password[sizeof(m_password) - 1] = '\0';
	SetState(FB_STATE_BIND);
	m_autoBind = 0;
	m_jobMgr->StartBind(3, account, password, 0);
	SetWaitingFlag();
	return m_viewMgr->ShowWait();
}

int FbAccessTokenController::OnConfirmBindCancel()
{
	if (m_state != FB_STATE_CONFIRM_BIND)
		return OnUnexpectedState();
	m_result = 1;
	SetState(FB_STATE_IDLE);
	return OnFinishTrigger();
}

int FbAccessTokenController::OnConfirmBindAccept()
{
	if (m_state != FB_STATE_CONFIRM_BIND)
		return OnUnexpectedState();
	int hasEmail = 0;
	int hasProfile = 0;

	m_enableShareOnBind = 1;
	int ret = FbGetAccountEmail(&hasEmail, &m_account, 0);

	if (ret >= 0) {
		if (hasEmail != 0) {
			ret = FbGetUserProfile(&m_account, &hasProfile, &m_profile, 0);
			if (ret >= 0) {
				if (hasProfile != 0) {
					SetState(FB_STATE_BIND);
					m_autoBind = 1;
					m_jobMgr->StartBind(4, NULL, NULL, 0);
					SetWaitingFlag();
					return m_viewMgr->ShowWait();
				}
				SetState(FB_STATE_INPUT);
				return m_viewMgr->ShowInput(&m_account, NULL);
			}
		} else {
			ret = FB_DIALOG_ERROR_INVALID_STATE;
		}
	}
	SetState(FB_STATE_ERROR);
	m_viewMgr->ShowError(ret);
	return ret;
}

int FbAccessTokenController::OnInputCancel()
{
	if (m_state != FB_STATE_INPUT)
		return OnUnexpectedState();
	m_result = 1;
	SetState(FB_STATE_IDLE);
	return OnFinishTrigger();
}

int FbAccessTokenController::OnInputSubmit(const FbAccountId *account, const char *password)
{
	if (m_state != FB_STATE_INPUT)
		return OnUnexpectedState();
	sce_paf_strncpy(m_account.email, account->email, sizeof(m_account.email));
	m_account.email[sizeof(m_account.email) - 1] = '\0';
	sce_paf_strncpy(m_password, password, sizeof(m_password));
	m_password[sizeof(m_password) - 1] = '\0';
	SetState(FB_STATE_BIND);
	m_autoBind = 0;
	m_jobMgr->StartBind(4, account, password, 0);
	SetWaitingFlag();
	return m_viewMgr->ShowWait();
}

int FbAccessTokenController::OnPermissionCancel()
{
	if (m_state != FB_STATE_PERMISSION)
		return OnUnexpectedState();
	m_result = 1;
	SetState(FB_STATE_IDLE);
	return OnFinishTrigger();
}

int FbAccessTokenController::OnPermissionAccept()
{
	if (m_state != FB_STATE_PERMISSION)
		return OnUnexpectedState();
	SetState(FB_STATE_GET_ACCESS_TOKEN);
	m_jobMgr->StartGetAccessToken(m_titleId, m_fbAppId, m_missingPermissions, &m_token,
				      m_shortMode);
	m_waitingForView = 0;
	return m_viewMgr->ShowWait();
}

int FbAccessTokenController::OnWaitCancel()
{
	if (m_state != FB_STATE_AUTH_CHECK && m_state != FB_STATE_BIND &&
	    m_state != FB_STATE_CHECK_PERMISSIONS && m_state != FB_STATE_GET_ACCESS_TOKEN)
		return OnUnexpectedState();
	m_waitingForView = 0;
	int ret = m_jobMgr->CancelCurrent();

	m_result = 1;
	return ret;
}

int FbAccessTokenController::OnWaitClosed()
{
	if (m_state != FB_STATE_AUTH_CHECK && m_state != FB_STATE_BIND &&
	    m_state != FB_STATE_CHECK_PERMISSIONS && m_state != FB_STATE_GET_ACCESS_TOKEN)
		return OnUnexpectedState();
	m_waitingForView = 0;
	return OnWaitStep();
}

int FbAccessTokenController::OnErrorClosed(int error)
{
	if (m_state != FB_STATE_DONE && m_state != FB_STATE_ERROR)
		return OnUnexpectedState();
	if (m_aborting != 0) {
		SetState(FB_STATE_DONE);
		return 0;
	}
	m_viewMgr->CloseAllLayers();
	m_internalResult = error;
	if (error == SNSFB_ERR_NOT_SIGNIN)
		m_result = (SceInt32)0x80106104;
	else if (error == SNSFB_ERR_CHAT_RESTRICTION)
		m_result = (SceInt32)0x80106105;
	else
		m_result = (SceInt32)0x80106101;
	SetState(FB_STATE_IDLE);
	return OnFinishTrigger();
}

int FbAccessTokenController::OnChatRestrictClosed()
{
	if (m_state != FB_STATE_CHAT_RESTRICT)
		return OnUnexpectedState();
	if (m_aborting != 0) {
		SetState(FB_STATE_DONE);
		return 0;
	}
	m_viewMgr->CloseAllLayers();
	m_result = (SceInt32)0x80106105;
	SetState(FB_STATE_IDLE);
	return OnFinishTrigger();
}

int FbAccessTokenController::OnRetryAuthClosed(int step)
{
	if (m_state != FB_STATE_RETRY_AUTH)
		return OnUnexpectedState();
	if (m_aborting != 0) {
		SetState(FB_STATE_DONE);
		return 0;
	}
	m_viewMgr->CloseAllLayers();
	if (step == 3) {
		SetState(FB_STATE_LOGIN);
		return m_viewMgr->ShowLogin(m_titleName, &m_account, m_password);
	}
	if (step == 4) {
		SetState(FB_STATE_INPUT);
		return m_viewMgr->ShowInput(&m_account, m_password);
	}
	SetState(FB_STATE_IDLE);
	return OnFinishTrigger();
}

int FbAccessTokenController::OnNoticeClosed()
{
	if (m_state != FB_STATE_NOTICE)
		return OnUnexpectedState();
	m_result = 1;
	SetState(FB_STATE_IDLE);
	return OnFinishTrigger();
}

int FbAccessTokenController::OnJobAuthCheckDone(int result, int signedIn, int chatAllowed)
{
	if (m_state != FB_STATE_AUTH_CHECK)
		return OnUnexpectedState();
	m_lastJobResult = result;
	m_signedIn = signedIn;
	m_chatAllowed = chatAllowed;
	if (m_waitingForView == 0)
		m_viewMgr->CloseTop();
	return OnWaitStep();
}

int FbAccessTokenController::OnJobBindDone(int result, int step)
{
	if (m_state != FB_STATE_BIND)
		return OnUnexpectedState();
	m_lastJobResult = result;
	m_lastBindStep = step;
	if (m_waitingForView == 0)
		m_viewMgr->CloseTop();
	return OnWaitStep();
}

int FbAccessTokenController::OnJobCheckPermissionsDone(int result, int needPerms)
{
	if (m_state != FB_STATE_CHECK_PERMISSIONS)
		return OnUnexpectedState();
	m_lastJobResult = result;
	m_needPerms = needPerms;
	if (m_waitingForView == 0)
		m_viewMgr->CloseTop();
	return OnWaitStep();
}

int FbAccessTokenController::OnJobGetAccessTokenDone(int result)
{
	if (m_state != FB_STATE_GET_ACCESS_TOKEN)
		return OnUnexpectedState();
	m_lastJobResult = result;
	if (m_waitingForView != 0)
		m_waitingForView = 0;
	m_viewMgr->CloseTop();
	return OnWaitStep();
}

void FbAccessTokenController::SetWaitingFlag()
{
	m_waitingForView = 1;
}

int FbAccessTokenController::OnWaitStep()
{
	if (m_jobMgr->m_busy != 0)
		return 0;
	if (m_waitingForView == 0 && m_viewMgr->IsWaitOpen() != 0)
		return 0;
	if (m_aborting != 0) {
		if (m_viewMgr->IsWaitOpen() != 0) {
			m_waitingForView = 0;
			m_viewMgr->CloseTop();
			return 0;
		}
		SetState(FB_STATE_DONE);
		return 0;
	}
	if (m_result == 1) {
		if (m_viewMgr->IsWaitOpen() != 0) {
			m_waitingForView = 0;
			m_viewMgr->CloseTop();
			return 0;
		}
		switch (m_state) {
		case FB_STATE_AUTH_CHECK:
		case FB_STATE_CHECK_PERMISSIONS:
			break;
		case FB_STATE_BIND:
			if (m_account.reserved == 0)
				FbInitService(0);
			break;
		case FB_STATE_GET_ACCESS_TOKEN:
			sce_paf_memset(&m_token, 0, sizeof(m_token));
			break;
		default:
			if (m_viewMgr->IsWaitOpen() != 0) {
				m_waitingForView = 0;
				m_viewMgr->CloseTop();
				return 0;
			}
			return OnUnexpectedState();
		}
		m_result = 1;
		SetState(FB_STATE_IDLE);
		return OnFinishTrigger();
	}
	switch (m_state) {
	case FB_STATE_AUTH_CHECK:
		return HandleAuthCheck(m_lastJobResult, m_signedIn, m_chatAllowed);
	case FB_STATE_BIND:
		return HandleBind(m_lastJobResult, m_lastBindStep);
	case FB_STATE_CHECK_PERMISSIONS:
		return HandleCheckPermissions(m_lastJobResult, m_needPerms);
	case FB_STATE_GET_ACCESS_TOKEN:
		return HandleGetAccessToken(m_lastJobResult);
	default:
		if (m_viewMgr->IsWaitOpen() != 0) {
			m_waitingForView = 0;
			m_viewMgr->CloseTop();
			return 0;
		}
		return OnUnexpectedState();
	}
}

int FbAccessTokenController::HandleAuthCheck(int result, int signedIn, int chatAllowed)
{
	int hasEmail = 0;
	int hasProfile = 0;
	int shareEnabled = 0;

	if (m_state != FB_STATE_AUTH_CHECK) {
		if (m_viewMgr->IsWaitOpen() == 0)
			return OnUnexpectedState();
		m_waitingForView = 0;
		m_viewMgr->CloseTop();
		return 0;
	}
	if (result >= 0) {
		if (signedIn == 0) {
			if (m_viewMgr->IsWaitOpen() != 0) {
				m_waitingForView = 0;
				m_viewMgr->CloseTop();
				return 0;
			}
			m_internalResult = result;
			m_result = (SceInt32)0x80106104;
			SetState(FB_STATE_ERROR);
			return m_viewMgr->ShowError(m_result);
		}
		if (chatAllowed == 0) {
			if (m_viewMgr->IsWaitOpen() != 0) {
				m_waitingForView = 0;
				m_viewMgr->CloseTop();
				return 0;
			}
			SetState(FB_STATE_CHAT_RESTRICT);
			return m_viewMgr->ShowChatRestrict(result);
		}
		result = FbGetAccountEmail(&hasEmail, &m_account, 0);
		if (result >= 0) {
			if (hasEmail == 0) {
				if (m_viewMgr->IsWaitOpen() != 0) {
					m_waitingForView = 0;
					m_viewMgr->CloseTop();
					return 0;
				}
				SetState(FB_STATE_LOGIN);
				return m_viewMgr->ShowLogin(m_titleName, NULL, NULL);
			}
			result = FbGetUserProfile(&m_account, &hasProfile, &m_profile, 0);
			if (result >= 0) {
				if (hasProfile == 0) {
					if (m_viewMgr->IsWaitOpen() != 0) {
						m_waitingForView = 0;
						m_viewMgr->CloseTop();
						return 0;
					}
					SetState(FB_STATE_INPUT);
					return m_viewMgr->ShowInput(&m_account, NULL);
				}
				result = FbGetShareEnabled(&shareEnabled, 0);
				if (result >= 0) {
					if (shareEnabled != 0) {
						SetState(FB_STATE_BIND);
						result = m_jobMgr->StartBind(3, NULL, NULL, 0);
						m_autoBind = 1;
						if (m_viewMgr->IsWaitOpen() == 0)
							return m_viewMgr->ShowWait();
						return result;
					}
					if (m_viewMgr->IsWaitOpen() != 0) {
						m_waitingForView = 0;
						m_viewMgr->CloseTop();
						return 0;
					}
					SetState(FB_STATE_CONFIRM_BIND);
					return m_viewMgr->ShowConfirmBind(m_titleName);
				}
			}
		}
	}
	if (m_viewMgr->IsWaitOpen() != 0) {
		m_waitingForView = 0;
		m_viewMgr->CloseTop();
		return 0;
	}
	SetState(FB_STATE_ERROR);
	m_viewMgr->ShowError(result);
	return result;
}

int FbAccessTokenController::HandleBind(int result, int step)
{
	if (m_state != FB_STATE_BIND) {
		if (m_viewMgr->IsWaitOpen() == 0)
			return OnUnexpectedState();
		m_waitingForView = 0;
		m_viewMgr->CloseTop();
		return 0;
	}
	if (result == SNSFB_ERR_AUTH_09 || result == SNSFB_ERR_AUTH_0A ||
	    result == SNSFB_ERR_ACCOUNT_NOT_BOUND) {
		if (m_autoBind != 0) {
			if (m_viewMgr->IsWaitOpen() == 0) {
				SetState(FB_STATE_INPUT);
				return m_viewMgr->ShowInput(&m_account, NULL);
			}
		} else if (m_viewMgr->IsWaitOpen() == 0) {
			SetState(FB_STATE_RETRY_AUTH);
			return m_viewMgr->ShowFeedConfirm(result, step);
		}
		m_waitingForView = 0;
		m_viewMgr->CloseTop();
		return 0;
	}
	if (FbIsRetryableError(result) != 0) {
		if (m_viewMgr->IsWaitOpen() == 0) {
			SetState(FB_STATE_ERROR);
			return ShowRetryableError(result, 1);
		}
		m_waitingForView = 0;
		m_viewMgr->CloseTop();
		return 0;
	}
	if (result >= 0 && (m_account.reserved != 0 || (result = FbTermService(0)) >= 0) &&
	    (m_enableShareOnBind == 0 || (result = FbSetShareEnabled(1, 0)) >= 0)) {
		SetState(FB_STATE_CHECK_PERMISSIONS);
		result = m_jobMgr->StartCheckPermissions(m_titleId, m_fbAppId, m_permissions,
							 &m_permList, m_missingPermissions,
							 &m_token, m_shortMode);
		m_waitingForView = 0;
		if (m_viewMgr->IsWaitOpen() == 0)
			return m_viewMgr->ShowWait();
		return result;
	}
	if (m_viewMgr->IsWaitOpen() == 0) {
		SetState(FB_STATE_ERROR);
		m_viewMgr->ShowError(result);
		return result;
	}
	m_waitingForView = 0;
	m_viewMgr->CloseTop();
	return 0;
}

int FbAccessTokenController::HandleCheckPermissions(int result, int needPerms)
{
	if (m_state != FB_STATE_CHECK_PERMISSIONS) {
		if (m_viewMgr->IsWaitOpen() == 0)
			return OnUnexpectedState();
		m_waitingForView = 0;
		m_viewMgr->CloseTop();
		return 0;
	}
	if (FbIsRetryableError(result) != 0) {
		if (m_viewMgr->IsWaitOpen() == 0) {
			SetState(FB_STATE_ERROR);
			return ShowRetryableError(result, 1);
		}
		m_waitingForView = 0;
		m_viewMgr->CloseTop();
		return 0;
	}
	if (result >= 0) {
		if (needPerms != 0) {
			if (m_viewMgr->IsWaitOpen() == 0) {
				SetState(FB_STATE_PERMISSION);
				return m_viewMgr->ShowPermission(&m_permList, m_titleName);
			}
		} else if (m_viewMgr->IsWaitOpen() == 0) {
			m_result = 0;
			SetState(FB_STATE_IDLE);
			return OnFinishTrigger();
		}
		m_waitingForView = 0;
		m_viewMgr->CloseTop();
		return 0;
	}
	if (m_viewMgr->IsWaitOpen() == 0) {
		SetState(FB_STATE_ERROR);
		m_viewMgr->ShowError(result);
		return result;
	}
	m_waitingForView = 0;
	m_viewMgr->CloseTop();
	return 0;
}

int FbAccessTokenController::HandleGetAccessToken(int result)
{
	if (m_state != FB_STATE_GET_ACCESS_TOKEN) {
		if (m_viewMgr->IsWaitOpen() == 0)
			return OnUnexpectedState();
		m_waitingForView = 0;
		m_viewMgr->CloseTop();
		return 0;
	}
	if (FbIsRetryableError(result) != 0) {
		if (m_viewMgr->IsWaitOpen() == 0) {
			SetState(FB_STATE_ERROR);
			return ShowRetryableError(result, 1);
		}
		m_waitingForView = 0;
		m_viewMgr->CloseTop();
		return 0;
	}
	if (result >= 0) {
		if (m_viewMgr->IsWaitOpen() == 0) {
			m_result = 0;
			SetState(FB_STATE_IDLE);
			return OnFinishTrigger();
		}
		m_waitingForView = 0;
		m_viewMgr->CloseTop();
		return 0;
	}
	if (m_viewMgr->IsWaitOpen() == 0) {
		SetState(FB_STATE_ERROR);
		m_viewMgr->ShowError(result);
		return result;
	}
	m_waitingForView = 0;
	m_viewMgr->CloseTop();
	return 0;
}

/* FbFeedController (vtable 0x81010764) */
FbFeedController::FbFeedController()
	: m_feedDataType(2), m_useDefaultBg(0), m_postType(0), m_visibility(3), m_rating(0),
	  m_trophyGrade(0), m_trophyFlags(0), m_activityFlags(0), m_batchCount(0)
{
	sce_paf_memset(m_profileReserved, 0, sizeof(m_profileReserved));
	sce_paf_memset(&m_account, 0, sizeof(m_account));
	m_account.reserved = 1;
	sce_paf_memset(m_password, 0, sizeof(m_password));
	sce_paf_memset(&m_profile, 0, sizeof(m_profile));
	sce_paf_memset(m_story, 0, sizeof(m_story));
	sce_paf_memset(m_caption, 0, 181);
	sce_paf_memset(m_comment, 0, 257);
	for (int i = 0; i < 16; ++i)
		sce_paf_memset(&m_batchTrophyItems[i], 0, sizeof(m_batchTrophyItems[i]));
	for (int i = 0; i < 16; ++i)
		sce_paf_memset(&m_batchActivityItems[i], 0, sizeof(m_batchActivityItems[i]));
}

FbFeedController::~FbFeedController()
{
}

int FbFeedController::StartFeedJob()
{
	if (m_postType == 1 || m_postType == 2) {
		return m_jobMgr->StartPostFeed(m_feedDataType, m_postType, m_visibility, m_story,
					       m_caption, m_comment, m_rating, m_trophyGrade,
					       m_trophyFlags, m_activityFlags);
	}
	if (m_postType == 3 || m_postType == 4) {
		return m_jobMgr->StartBatchPost(m_feedDataType, m_postType, m_batchCount,
						m_batchTrophyItems, m_batchActivityItems);
	}
	return FB_DIALOG_ERROR_INVALID_STATE;
}

extern void FbSetPluginLocale(Plugin *plugin, int locale);

int FbFeedController::Start(const FbPluginStartParam *param, Plugin *plugin,
			    FbDialogFinishCallback cb)
{
	if (m_state != FB_STATE_IDLE || param == NULL || param->feedData == NULL)
		return FB_DIALOG_ERROR_INVALID_STATE;

	const FbFeedDataHeader *hdr = (const FbFeedDataHeader *)param->feedData;

	m_feedDataType = param->feedDataType;
	if (m_feedDataType == 2)
		return FB_DIALOG_ERROR_INVALID_STATE;

	FbSetPluginLocale(plugin, hdr->locale);
	m_useDefaultBg = hdr->useDefaultBg;
	m_finishCallback = cb;

	int ret = InitComponents(plugin);

	if (ret < 0)
		return ret;

	m_viewMgr->AddView(CreateFbBgView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbInputShortView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbWaitView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbErrorView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbChatRestrictView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbFeedConfirmView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbRetryServerView(&m_eventBus, plugin));

	ret = ApplyBgColor(param);
	if (ret < 0)
		return ret;

	int hasEmail = 0;

	ret = FbGetAccountEmail(&hasEmail, &m_account, m_feedDataType);
	if (ret < 0)
		return ret;
	m_account.reserved = hasEmail;

	m_postType = hdr->postType;
	if (m_postType == 1) {
		const FbFeedTrophyData *trophyData = (const FbFeedTrophyData *)param->feedData;

		m_visibility = hdr->countOrVis;
		sce_paf_strncpy(m_story, trophyData->story, 256);
		sce_paf_strncpy(m_caption, trophyData->caption, 180);
		m_rating = trophyData->rating;
		sce_paf_strncpy(m_comment, trophyData->comment, 256);
		m_trophyGrade = trophyData->trophyGrade;
		m_trophyFlags = trophyData->trophyFlags;
	} else if (m_postType == 2) {
		const FbFeedActivityData *activityData = (const FbFeedActivityData *)param->feedData;

		m_visibility = hdr->countOrVis;
		sce_paf_strncpy(m_story, activityData->story, 256);
		sce_paf_strncpy(m_comment, activityData->comment, 256);
		m_activityFlags = activityData->activityFlags;
	} else if (m_postType == 3) {
		int count = hdr->countOrVis;

		if (count <= 0 || count > 16)
			return FB_DIALOG_ERROR_INVALID_STATE;
		m_batchCount = count;
		const FbFeedBatchTrophyPayload *payload =
			(const FbFeedBatchTrophyPayload *)param->feedData;

		for (int i = 0; i < count; ++i)
			sce_paf_memcpy(&m_batchTrophyItems[i], &payload->items[i],
				       sizeof(FbFeedBatchTrophyItem));
	} else if (m_postType == 4) {
		int count = hdr->countOrVis;

		if (count <= 0 || count > 16)
			return FB_DIALOG_ERROR_INVALID_STATE;
		m_batchCount = count;
		const FbFeedBatchActivityPayload *payload =
			(const FbFeedBatchActivityPayload *)param->feedData;

		for (int i = 0; i < count; ++i)
			sce_paf_memcpy(&m_batchActivityItems[i], &payload->items[i],
				       sizeof(FbFeedBatchActivityItem));
	} else {
		return FB_DIALOG_ERROR_INVALID_STATE;
	}

	ret = StartFeedJob();
	if (ret < 0)
		return ret;
	ret = m_viewMgr->ShowWait();
	if (ret < 0)
		return ret;
	SetState(FB_STATE_POST_FEED);
	return ret;
}

int FbFeedController::OnJobSyncDone()
{
	if (m_aborting != 0) {
		SetState(FB_STATE_DONE);
		return 0;
	}
	if (m_finishCallback == NULL) {
		SetState(FB_STATE_DONE);
		return FB_DIALOG_ERROR_INVALID_STATE;
	}
	m_finishCallback(m_result, m_internalResult, NULL);
	SetState(FB_STATE_DONE);
	return 0;
}

int FbFeedController::OnInputCancel()
{
	if (m_state != FB_STATE_INPUT)
		return OnUnexpectedState();
	m_result = 1;
	SetState(FB_STATE_IDLE);
	return OnFinishTrigger();
}

int FbFeedController::OnInputSubmit(const FbAccountId *account, const char *password)
{
	if (m_state != FB_STATE_INPUT)
		return OnUnexpectedState();
	sce_paf_strncpy(m_account.email, account->email, sizeof(m_account.email));
	m_account.email[sizeof(m_account.email) - 1] = '\0';
	sce_paf_strncpy(m_password, password, sizeof(m_password));
	m_password[sizeof(m_password) - 1] = '\0';
	SetState(FB_STATE_BIND);
	m_jobMgr->StartBind(4, account, password, m_feedDataType);
	return m_viewMgr->ShowWait();
}

int FbFeedController::OnWaitCancel()
{
	if (m_state != FB_STATE_BIND && m_state != FB_STATE_POST_FEED &&
	    m_state != FB_STATE_POST_FEED_RETRY)
		return OnUnexpectedState();
	int ret = m_jobMgr->CancelCurrent();

	m_result = 1;
	return ret;
}

int FbFeedController::OnWaitClosed()
{
	if (m_state != FB_STATE_BIND && m_state != FB_STATE_POST_FEED &&
	    m_state != FB_STATE_POST_FEED_RETRY)
		return OnUnexpectedState();
	return OnWaitStep();
}

int FbFeedController::OnErrorClosed(int error)
{
	if (m_state != FB_STATE_DONE && m_state != FB_STATE_ERROR)
		return OnUnexpectedState();
	if (m_aborting != 0) {
		SetState(FB_STATE_DONE);
		return 0;
	}
	m_viewMgr->CloseAllLayers();
	m_internalResult = error;
	if (error == SNSFB_ERR_NOT_SIGNIN)
		m_result = (SceInt32)0x80106104;
	else if (error == SNSFB_ERR_CHAT_RESTRICTION)
		m_result = (SceInt32)0x80106105;
	else
		m_result = (SceInt32)0x80106101;
	SetState(FB_STATE_IDLE);
	return OnFinishTrigger();
}

int FbFeedController::OnRetryAuthClosed(int step)
{
	if (m_state != FB_STATE_RETRY_AUTH)
		return OnUnexpectedState();
	if (m_aborting != 0) {
		SetState(FB_STATE_DONE);
		return 0;
	}
	m_viewMgr->CloseAllLayers();
	if (step == 4) {
		SetState(FB_STATE_INPUT);
		return m_viewMgr->ShowInput(&m_account, m_password);
	}
	SetState(FB_STATE_IDLE);
	return OnFinishTrigger();
}

int FbFeedController::OnJobBindDone(int result, int step)
{
	if (m_state != FB_STATE_BIND)
		return OnUnexpectedState();
	m_lastJobResult = result;
	m_lastBindStep = step;
	m_viewMgr->CloseTop();
	return OnWaitStep();
}

int FbFeedController::OnJobPostFeedDone(int result, int subResult)
{
	if (m_state != FB_STATE_POST_FEED && m_state != FB_STATE_POST_FEED_RETRY)
		return OnUnexpectedState();
	m_lastJobResult = result;
	m_lastSubResult = subResult;
	m_viewMgr->CloseTop();
	return OnWaitStep();
}

int FbFeedController::OnWaitStep()
{
	if (m_jobMgr->m_busy != 0)
		return 0;
	if (m_viewMgr->IsWaitOpen() != 0)
		return 0;
	if (m_aborting != 0) {
		SetState(FB_STATE_DONE);
		return 0;
	}
	if (m_result == 1) {
		if (m_state != FB_STATE_BIND && m_state != FB_STATE_POST_FEED &&
		    m_state != FB_STATE_POST_FEED_RETRY)
			return OnUnexpectedState();
		m_result = 1;
		SetState(FB_STATE_IDLE);
		return OnFinishTrigger();
	}
	if (m_state == FB_STATE_BIND)
		return HandleBind(m_lastJobResult, m_lastBindStep);
	if (m_state == FB_STATE_POST_FEED)
		return HandlePostFeed(m_lastJobResult, m_lastSubResult, 0);
	if (m_state == FB_STATE_POST_FEED_RETRY)
		return HandlePostFeed(m_lastJobResult, m_lastSubResult, 1);
	return OnUnexpectedState();
}

int FbFeedController::HandleBind(int result, int step)
{
	if (m_state != FB_STATE_BIND)
		return OnUnexpectedState();
	if (result == SNSFB_ERR_AUTH_09 || result == SNSFB_ERR_AUTH_0A ||
	    result == SNSFB_ERR_ACCOUNT_NOT_BOUND) {
		SetState(FB_STATE_RETRY_AUTH);
		return m_viewMgr->ShowFeedConfirm(result, step);
	}
	if (FbIsRetryableError(result) != 0) {
		SetState(FB_STATE_ERROR);
		return ShowRetryableError(result, 0);
	}
	if (result < 0) {
		SetState(FB_STATE_ERROR);
		m_viewMgr->ShowError(result);
		return result;
	}
	SetState(FB_STATE_POST_FEED_RETRY);
	result = StartFeedJob();
	if (result < 0) {
		SetState(FB_STATE_ERROR);
		m_viewMgr->ShowError(result);
		return result;
	}
	return m_viewMgr->ShowWait();
}

int FbFeedController::HandlePostFeed(int result, int subResult, int isRetry)
{
	(void)subResult;
	if (m_state != FB_STATE_POST_FEED && m_state != FB_STATE_POST_FEED_RETRY)
		return OnUnexpectedState();
	if (result == SNSFB_ERR_AUTH_09) {
		if (isRetry != 0) {
			SetState(FB_STATE_ERROR);
			m_viewMgr->ShowError(0);
			return 0;
		}
		SetState(FB_STATE_INPUT);
		if (m_useDefaultBg != 0 && m_viewMgr->OnBgAction() != 0) {
			m_viewMgr->CloseAll();
			SetDefaultBgColor();
		}
		return m_viewMgr->ShowInput(&m_account, NULL);
	}
	if (FbIsRetryableError(result) != 0) {
		SetState(FB_STATE_ERROR);
		return ShowRetryableError(result, 0);
	}
	if (result >= 0) {
		m_viewMgr->CloseTop();
		m_result = 0;
		SetState(FB_STATE_IDLE);
		return OnFinishTrigger();
	}
	SetState(FB_STATE_ERROR);
	m_viewMgr->ShowError(result);
	return result;
}

/* FbLongAccessTokenController (vtable 0x81010800) */
FbLongAccessTokenController::FbLongAccessTokenController()
{
}

FbLongAccessTokenController::~FbLongAccessTokenController()
{
}

int FbLongAccessTokenController::Start(const FbPluginStartParam *param, Plugin *plugin,
				       FbDialogFinishCallback cb)
{
	if (m_state != FB_STATE_IDLE)
		return FB_DIALOG_ERROR_INVALID_STATE;
	m_finishCallback = cb;
	int ret = InitComponents(plugin);

	if (ret < 0)
		return ret;

	m_viewMgr->AddView(CreateFbBgView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbInputLongView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbWaitView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbErrorView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbChatRestrictView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbFeedConfirmView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbRetryAuthView(&m_eventBus, plugin));
	m_viewMgr->AddView(CreateFbRetryServerView(&m_eventBus, plugin));
	m_viewMgr->m_useLongInput = 1;

	ret = ApplyBgColor(param);
	if (ret < 0)
		return ret;

	int hasEmail = 0;

	ret = FbGetAccountEmail(&hasEmail, &m_account, 0);
	if (ret < 0)
		return ret;
	m_account.reserved = hasEmail;

	ret = m_jobMgr->StartAuthCheck();
	if (ret < 0)
		return ret;
	m_waitingForView = 0;
	ret = m_viewMgr->ShowWait();
	if (ret < 0)
		return ret;
	SetState(FB_STATE_AUTH_CHECK);
	return ret;
}

int FbLongAccessTokenController::OnWaitClosed()
{
	if (m_state != FB_STATE_AUTH_CHECK && m_state != FB_STATE_BIND)
		return OnUnexpectedState();
	m_waitingForView = 0;
	return OnWaitStep();
}

int FbLongAccessTokenController::OnRetryAuthClosed(int step)
{
	if (m_state != FB_STATE_RETRY_AUTH)
		return OnUnexpectedState();
	if (m_aborting != 0) {
		SetState(FB_STATE_DONE);
		return 0;
	}
	m_viewMgr->CloseAllLayers();
	if (step == 4) {
		SetState(FB_STATE_INPUT);
		return m_viewMgr->ShowInput(&m_account, m_password);
	}
	SetState(FB_STATE_IDLE);
	return OnFinishTrigger();
}

int FbLongAccessTokenController::OnJobAuthCheckDone(int result, int signedIn, int chatAllowed)
{
	if (m_state != FB_STATE_AUTH_CHECK)
		return OnUnexpectedState();
	m_lastJobResult = result;
	m_signedIn = signedIn;
	m_chatAllowed = chatAllowed;
	m_viewMgr->CloseTop();
	return OnWaitStep();
}

int FbLongAccessTokenController::OnJobBindDone(int result, int step)
{
	if (m_state != FB_STATE_BIND)
		return OnUnexpectedState();
	m_lastJobResult = result;
	m_lastBindStep = step;
	m_viewMgr->CloseTop();
	return OnWaitStep();
}

void FbLongAccessTokenController::SetWaitingFlag()
{
	m_waitingForView = 0;
}

int FbLongAccessTokenController::OnWaitStep()
{
	if (m_jobMgr->m_busy != 0)
		return 0;
	if (m_viewMgr->IsWaitOpen() != 0) {
		if (m_waitingForView != 0) {
			m_waitingForView = 0;
			m_viewMgr->CloseTop();
		}
		return 0;
	}
	if (m_aborting != 0) {
		SetState(FB_STATE_DONE);
		return 0;
	}
	if (m_result == 1) {
		if (m_state == FB_STATE_BIND && m_account.reserved == 0)
			FbInitService(0);
		if (m_state != FB_STATE_AUTH_CHECK && m_state != FB_STATE_BIND)
			return OnUnexpectedState();
		m_result = 1;
		SetState(FB_STATE_IDLE);
		return OnFinishTrigger();
	}
	if (m_state == FB_STATE_AUTH_CHECK)
		return HandleAuthCheck(m_lastJobResult, m_signedIn, m_chatAllowed);
	if (m_state == FB_STATE_BIND)
		return HandleBind(m_lastJobResult, m_lastBindStep);
	return OnUnexpectedState();
}

int FbLongAccessTokenController::HandleAuthCheck(int result, int signedIn, int chatAllowed)
{
	int hasEmail = 0;
	int hasProfile = 0;

	if (m_state != FB_STATE_AUTH_CHECK)
		return OnUnexpectedState();
	if (result >= 0) {
		if (signedIn == 0) {
			m_internalResult = result;
			m_result = (SceInt32)0x80106104;
			SetState(FB_STATE_ERROR);
			return m_viewMgr->ShowError(m_result);
		}
		if (chatAllowed == 0) {
			SetState(FB_STATE_CHAT_RESTRICT);
			return m_viewMgr->ShowChatRestrict(result);
		}
		result = FbGetAccountEmail(&hasEmail, &m_account, 0);
		if (result >= 0) {
			if (hasEmail == 0) {
				SetState(FB_STATE_INPUT);
				return m_viewMgr->ShowInput(NULL, NULL);
			}
			result = FbGetUserProfile(&m_account, &hasProfile, &m_profile, 0);
			if (result >= 0) {
				if (hasProfile != 0) {
					SetState(FB_STATE_BIND);
					m_autoBind = 1;
					m_jobMgr->StartBind(3, NULL, NULL, 0);
					return m_viewMgr->ShowWait();
				}
				SetState(FB_STATE_INPUT);
				return m_viewMgr->ShowInput(&m_account, NULL);
			}
		}
	}
	SetState(FB_STATE_ERROR);
	m_viewMgr->ShowError(result);
	return result;
}

int FbLongAccessTokenController::HandleBind(int result, int step)
{
	if (m_state != FB_STATE_BIND)
		return OnUnexpectedState();
	if (result == SNSFB_ERR_AUTH_09 || result == SNSFB_ERR_AUTH_0A ||
	    result == SNSFB_ERR_ACCOUNT_NOT_BOUND) {
		if (m_autoBind != 0) {
			SetState(FB_STATE_INPUT);
			return m_viewMgr->ShowInput(&m_account, NULL);
		}
		SetState(FB_STATE_RETRY_AUTH);
		return m_viewMgr->ShowFeedConfirm(result, step);
	}
	if (FbIsRetryableError(result) != 0) {
		SetState(FB_STATE_ERROR);
		return ShowRetryableError(result, 1);
	}
	if (result >= 0) {
		if (m_account.reserved == 0)
			FbTermService(0);
		m_viewMgr->CloseTop();
		m_result = 0;
		SetState(FB_STATE_IDLE);
		return OnFinishTrigger();
	}
	SetState(FB_STATE_ERROR);
	m_viewMgr->ShowError(result);
	return result;
}
