/*
 * SceNpSnsFacebookDialogPlugin: FbDialogBase, FbDialogControllerBase, controllers, and view
 * factories.
 */
#ifndef NP_SNS_FB_DIALOG_CONTROLLER_H
#define NP_SNS_FB_DIALOG_CONTROLLER_H

#include <stdint.h>
#include <scetypes.h>
#include <std/vector>
#include <plugin.h>
#include <job/job.h>
#include "fb_dialog_plugin.h"
#include "fb_dialog_job.h"
#include "fb_dialog_view_mgr.h"

/* Dialog controller state machine states */
enum FbDialogState {
	FB_STATE_IDLE = 0,
	FB_STATE_DONE = 1,
	FB_STATE_AUTH_CHECK = 2,
	FB_STATE_LOGIN = 3,
	FB_STATE_INPUT = 4,
	FB_STATE_CONFIRM_BIND = 5,
	FB_STATE_BIND = 6,
	FB_STATE_CHECK_PERMISSIONS = 7,
	FB_STATE_PERMISSION = 8,
	FB_STATE_GET_ACCESS_TOKEN = 9,
	FB_STATE_ERROR = 10,
	FB_STATE_CHAT_RESTRICT = 11,
	FB_STATE_RETRY_AUTH = 12,
	FB_STATE_POST_FEED = 13,
	FB_STATE_POST_FEED_RETRY = 14,
	FB_STATE_NOTICE = 15
};

/* Base class for Facebook dialog controllers (vtable 0x81010F74) */
class FbDialogBase {
public:
	FbDialogBase();
	virtual ~FbDialogBase();

	virtual int AddChild(void *child);
	virtual int RemoveChild(void *child);
	virtual int Start(const FbPluginStartParam *param, paf::Plugin *plugin,
			  FbDialogFinishCallback cb);
	virtual int Abort();
	virtual int WaitEnd();
	virtual int OnFinishTrigger();
	virtual int OnJobDummyDone();
	virtual int OnBgAction();
	virtual int OnLoginCancel();
	virtual int OnLoginSubmit(const FbAccountId *account, const char *password);
	virtual int OnConfirmBindCancel();
	virtual int OnConfirmBindAccept();
	virtual int OnInputCancel();
	virtual int OnInputSubmit(const FbAccountId *account, const char *password);
	virtual int OnPermissionCancel();
	virtual int OnPermissionAccept();
	virtual int OnWaitCancel();
	virtual int OnWaitClosed();
	virtual int OnErrorClosed(int error);
	virtual int OnChatRestrictClosed();
	virtual int OnRetryAuthClosed(int step);
	virtual int OnNoticeClosed();
	virtual int OnJobAuthCheckDone(int result, int signedIn, int chatAllowed);
	virtual int OnJobBindDone(int result, int step);
	virtual int OnJobCheckPermissionsDone(int result, int needPerms);
	virtual int OnJobGetAccessTokenDone(int result);
	virtual int OnJobPostFeedDone(int result, int subResult);
	virtual int InitComponents(paf::Plugin *plugin);
	virtual int TermComponents();

	int m_aborting;
	FbDialogFinishCallback m_finishCallback;
	paf::vector<FbComponent *> m_children;
	int m_reserved;
	FbEventBus m_eventBus;
	FbViewManager *m_viewMgr;
	FbJobManager *m_jobMgr;
	paf::job::JobQueue *m_jobQueue;
	int m_result;
	int m_internalResult;
};

/* State machine controller base (vtable 0x81010FF8) */
class FbDialogControllerBase : public FbDialogBase {
public:
	FbDialogControllerBase();
	virtual ~FbDialogControllerBase();

	virtual int Abort();
	virtual int WaitEnd();
	virtual int OnFinishTrigger();
	virtual int OnUnexpectedState();
	virtual int ShowRetryableError(int error, int isAuthFlow);
	virtual void SetWaitingFlag();

	void SetState(int state);
	int SetDefaultBgColor();
	int ApplyBgColor(const FbPluginStartParam *param);
	int ShowNotice(paf::Plugin *plugin);

	int m_state;
	int m_lastJobResult;
	int m_lastSubResult;
	int m_lastBindStep;
	int m_waitingForView;
};

/* Access token controller (vtable 0x810106C0) */
class FbAccessTokenController : public FbDialogControllerBase {
public:
	FbAccessTokenController();
	virtual ~FbAccessTokenController();

	virtual int Start(const FbPluginStartParam *param, paf::Plugin *plugin,
			  FbDialogFinishCallback cb);
	virtual int OnJobDummyDone();
	virtual int OnBgAction();
	virtual int OnLoginCancel();
	virtual int OnLoginSubmit(const FbAccountId *account, const char *password);
	virtual int OnConfirmBindCancel();
	virtual int OnConfirmBindAccept();
	virtual int OnInputCancel();
	virtual int OnInputSubmit(const FbAccountId *account, const char *password);
	virtual int OnPermissionCancel();
	virtual int OnPermissionAccept();
	virtual int OnWaitCancel();
	virtual int OnWaitClosed();
	virtual int OnErrorClosed(int error);
	virtual int OnChatRestrictClosed();
	virtual int OnRetryAuthClosed(int step);
	virtual int OnNoticeClosed();
	virtual int OnJobAuthCheckDone(int result, int signedIn, int chatAllowed);
	virtual int OnJobBindDone(int result, int step);
	virtual int OnJobCheckPermissionsDone(int result, int needPerms);
	virtual int OnJobGetAccessTokenDone(int result);
	virtual void SetWaitingFlag();
	virtual int OnWaitStep();
	virtual int HandleAuthCheck(int result, int signedIn, int chatAllowed);
	virtual int HandleBind(int result, int step);
	virtual int HandleCheckPermissions(int result, int needPerms);
	virtual int HandleGetAccessToken(int result);

	int m_shortMode;
	char m_titleId[10];
	char m_titleName[128];
	uint8_t m_titlePadding[2];
	SceUInt64 m_fbAppId;
	char m_permissions[256];
	char m_missingPermissions[256];
	int m_signedIn;
	int m_chatAllowed;
	int m_needPerms;
	FbAccountId m_account;
	char m_password[256];
	FbUserProfile m_profile;
	uint8_t m_profilePadding[3];
	int m_profileReserved[4];
	paf::vector<FbPermissionDesc> m_permList;
	int m_permReserved;
	int m_enableShareOnBind;
	int m_bindReserved;
	SceNpSnsFacebookLongAccessTokenResult m_token;
	int m_autoBind;
};

/* Feed posting controller (vtable 0x81010764) */
class FbFeedController : public FbDialogControllerBase {
public:
	FbFeedController();
	virtual ~FbFeedController();

	virtual int Start(const FbPluginStartParam *param, paf::Plugin *plugin,
			  FbDialogFinishCallback cb);
	virtual int OnJobDummyDone();
	virtual int OnInputCancel();
	virtual int OnInputSubmit(const FbAccountId *account, const char *password);
	virtual int OnWaitCancel();
	virtual int OnWaitClosed();
	virtual int OnErrorClosed(int error);
	virtual int OnRetryAuthClosed(int step);
	virtual int OnJobBindDone(int result, int step);
	virtual int OnJobPostFeedDone(int result, int subResult);
	virtual int OnWaitStep();
	virtual int HandleBind(int result, int step);
	virtual int HandlePostFeed(int result, int subResult, int isRetry);

	int StartFeedJob();

	FbAccountId m_account;
	char m_password[256];
	FbUserProfile m_profile;
	uint8_t m_profilePadding[3];
	int m_profileReserved[4];
	int m_feedDataType;
	int m_useDefaultBg;
	int m_postType;
	int m_visibility;
	char m_story[257];
	char m_caption[183];
	int m_rating;
	char m_comment[260];
	int m_trophyGrade;
	int m_trophyFlags;
	int m_activityFlags;
	int m_batchCount;
	FbFeedBatchItem3 m_batchItems3[16];
	FbFeedBatchItem4 m_batchItems4[16];
};

/* Long access token controller (vtable 0x81010800) */
class FbLongAccessTokenController : public FbAccessTokenController {
public:
	FbLongAccessTokenController();
	virtual ~FbLongAccessTokenController();

	virtual int Start(const FbPluginStartParam *param, paf::Plugin *plugin,
			  FbDialogFinishCallback cb);
	virtual int OnWaitClosed();
	virtual int OnRetryAuthClosed(int step);
	virtual int OnJobAuthCheckDone(int result, int signedIn, int chatAllowed);
	virtual int OnJobBindDone(int result, int step);
	virtual void SetWaitingFlag();
	virtual int OnWaitStep();
	virtual int HandleAuthCheck(int result, int signedIn, int chatAllowed);
	virtual int HandleBind(int result, int step);
};

/* View factories (implemented in fb_dialog_views.cpp) */
FbViewBase *CreateFbBgView(FbEventBus *bus, paf::Plugin *plugin);
FbViewBase *CreateFbLoginView(FbEventBus *bus, paf::Plugin *plugin);
FbViewBase *CreateFbConfirmBindView(FbEventBus *bus, paf::Plugin *plugin);
FbViewBase *CreateFbInputShortView(FbEventBus *bus, paf::Plugin *plugin);
FbViewBase *CreateFbInputLongView(FbEventBus *bus, paf::Plugin *plugin);
FbViewBase *CreateFbPermissionView(FbEventBus *bus, paf::Plugin *plugin);
FbViewBase *CreateFbWaitView(FbEventBus *bus, paf::Plugin *plugin);
FbViewBase *CreateFbErrorView(FbEventBus *bus, paf::Plugin *plugin);
FbViewBase *CreateFbChatRestrictView(FbEventBus *bus, paf::Plugin *plugin);
FbViewBase *CreateFbFeedConfirmView(FbEventBus *bus, paf::Plugin *plugin);
FbViewBase *CreateFbRetryAuthView(FbEventBus *bus, paf::Plugin *plugin);
FbViewBase *CreateFbRetryServerView(FbEventBus *bus, paf::Plugin *plugin);
FbViewBase *CreateFbNoticeView(FbEventBus *bus, paf::Plugin *plugin);

#endif /* NP_SNS_FB_DIALOG_CONTROLLER_H */
