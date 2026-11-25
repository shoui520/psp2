/*
 * SceTwitterDialogPlugin (vs0:sys/external/libcdlg_twitter.suprx): module entry,
 * TwitterDialogBase, TwitterDialogApp, TweetPage, TwitterSession, and CommonDialogMain
 * slot 20 integration.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <common_dialog.h>
#include <libkernel/libkernel.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <math/ngp/v4.h>
#include <plugin.h>
#include <module/module.h>
#include <common/main_thread_call_list.h>
#include <common/string_util.h>
#include <graphics/surface/surface.h>
#include <graphics/text/text_style.h>
#include <widget/core/widget.h>
#include <widget/w_text.h>
#include <widget/w_text_box.h>
#include <widget/w_button.h>
#include <widget/core/event.h>
#include "../tw_login_dialog/tw_lx.h"
#include "../tw_login_dialog/tw_lx_oauth.h"
#include "../tw_login_dialog/tw_dialog_common.h"
#include "tw_lx_json.h"
#include "tw_lx_multipart.h"
#include "tw_lx_api.h"

using namespace paf;

/* Common dialog slot index for Twitter dialog */
#define COMMON_DIALOG_TYPE_TWITTER	20

/* Twitter dialog modes and page IDs */
#define TWITTER_MODE_TWEET		1
#define TWITTER_MODE_LOGIN_ONLY		2

#define TW_PAGE_LOGIN_SUB_ID		0xC1C7F4D0U
#define TW_PAGE_TWEET_ID		0xD722952EU

/* TweetPage widget IDParam hashes */
#define TW_WIDGET_GUIDE_TEXT_ID		0xD7A384E6U
#define TW_WIDGET_BG_PLANE_ID		0x932FA325U
#define TW_WIDGET_CONTENT_ROOT_ID	0x19BA830DU
#define TW_WIDGET_HEADER_BOX_ID		0x5D5029BAU
#define TW_WIDGET_PREVIEW_IMG_ID	0xDB8715B9U
#define TW_WIDGET_REMOVE_BTN_ID		0x3EA4AC50U
#define TW_WIDGET_PHOTO_BTN_ID		0x4F55FC9DU
#define TW_WIDGET_TWEET_BTN_ID		0x6B9D0452U
#define TW_WIDGET_CANCEL_BTN_ID		0x32677CFBU
#define TW_WIDGET_TEXTBOX_ID		0x8D70E8EBU
#define TW_TEX_FACEICON_DEFAULT_ID	0x54275C50U
#define TW_TEX_BAR_NORMAL_ID		0x997654D9U
#define TW_TEX_BAR_OVER_ID		0xBFC02E7AU
#define TW_STR_WAIT_LOADING_ID		0x632B9313U
#define TW_STR_WAIT_POSTING_ID		0xB6084D32U

#define TW_EV_BUTTON_DECIDE		0x10000008
#define TW_EV_TEXTBOX_CHANGE		0x1000000B

#define TW_ERROR_NOT_ALLOWED		((SceInt32)0x80106094)
#define TW_ERROR_PERMISSION_DENIED	((SceInt32)0x80106095)
#define TW_ERROR_GENERIC_FAIL		((SceInt32)0x800201FF)
#define TW_ERROR_AUTH_RETRY		((SceInt32)0xA0000305)
#define TW_ERROR_CANCELED		((SceInt32)0xA0000304)
#define TW_ERROR_ABORTED		((SceInt32)0xA0000704)

#define TW_MAX_TWEET_CHARS		140

/* CommonDialog work memory layout for Twitter dialog */
struct TwitterDialogParam {
	SceUInt32 sdkVersion;
	SceCommonDialogParam commonParam;
	SceInt32 mode;
	SceChar8 titleOrInitialText[12];
	SceCommonDialogColor textColor;
	SceInt32 colorFlag;
	SceInt32 disablePhotoImport;
	SceInt32 requireText;
	SceInt32 requirePhoto;
	SceInt32 hideRemoveButton;
	SceInt32 photoFilter;
	SceChar8 reserved[1160];
};

struct TwitterDialogResult {
	SceInt32 result;
	SceInt32 errorCode;
	SceChar8 reserved[32];
};

struct CommonDialogTwitterParam {
	SceInt32 state;
	SceUInt32 flags;
	SceInt32 subDialogState;
	SceInt32 abortRequested;
	TwitterDialogParam param;
	TwitterDialogResult result;
	char apiName[64];
};

/* Photo import and Twitter login sub-dialog structs */
struct PhotoImportSubResult {
	SceInt32 result;
	SceChar8 reserved[36];
};

struct PhotoImportSubParam {
	SceUInt32 sdkVersion;
	SceCommonDialogParam commonParam;
	SceInt32 mode;
	SceInt32 visibleCategory;
	SceInt32 itemCount;
	SceChar8 reserved[32];
};

struct PhotoImportFileData {
	SceUInt32 maxCount;
	SceChar8 reserved[32];
};

struct TwLoginSubParam {
	SceUInt32 sdkVersion;
	SceCommonDialogParam commonParam;
	SceInt32 mode;
	SceCommonDialogColor textColor;
	SceInt32 colorFlag;
	SceInt32 dimHeader;
	SceChar8 reserved[32];
};

/* Job identifiers for TweetPageJob_* */
enum TweetJobId {
	TWEET_JOB_APPINFO_GET = 0,
	TWEET_JOB_APPINFO_UPDATE = 1,
	TWEET_JOB_TOKEN = 2,
	TWEET_JOB_MEDIA_CONFIG = 3,
	TWEET_JOB_FACEICON = 4,
	TWEET_JOB_IMAGE = 5,
	TWEET_JOB_TWEET = 6
};

/* External imports */
extern "C" {
int SceCommonDialogMain_05EFC635(int dialogType);
int SceCommonDialogMain_5739BC42(int dialogType);
int SceCommonDialogMain_75F10F83(int dialogType);
int SceCommonDialogMain_B9D0E5A8(int dialogType);
CommonDialogTwitterParam *SceCommonDialogMain_DD2CAC30(int dialogType);

SceInt32 sceNpManagerGetChatRestrictionFlag(SceInt32 *flag);
SceInt32 sceAppMgrAppParamGetString(SceUID pid, SceInt32 param, char *str, SceSize len);
SceInt32 sceKernelGetModelForCDialog(void);

SceInt32 sceTwLoginDialogGetStatus(void);
SceInt32 sceTwLoginDialogTerm(void);
SceInt32 sceTwLoginDialogAbort(void);
SceInt32 SceCommonDialog_463DB9ED(const TwLoginSubParam *param);

SceInt32 scePhotoImportDialogGetStatus(void);
SceInt32 scePhotoImportDialogGetResult(PhotoImportSubResult *result);
SceInt32 scePhotoImportDialogTerm(void);
SceInt32 scePhotoImportDialogAbort(void);
SceInt32 SceCommonDialog_FCDA2F0D(const PhotoImportSubParam *param,
				  const PhotoImportFileData *fileData);

void __cxa_finalize(void *dsoHandle);
extern void (*__start__Zinit_array[])(void);
extern void (*__stop__Zinit_array[])(void);
void *__dso_handle = &__dso_handle;
}

/* Plugin interface table registered via paf::Module::SetInterface */
struct TwitterPluginInterface {
	void (*OnBoot)();
	void (*OnLoad)(Plugin *plugin);
	void (*OnStart)();
	void (*OnStop)();
	void (*OnUnload)();
};

/* Forward declarations */
class TwitterDialogBase;
class TwitterDialogApp;

/* Page table for TwPageManager */
static TwPageHandlerEntry s_twitterPageEntries[] = {
	{ TW_PAGE_LOGIN_SUB_ID, NULL, 0 },
	{ TW_PAGE_TWEET_ID, NULL, 0 },
	{ 0, NULL, 0 },
};

/* Helper: abort condition backed by TwJobItem::IsCanceled() */
class TwitterJobAbortCondition : public LxAbortCondition {
public:
	explicit TwitterJobAbortCondition(TwJobItem *job) : m_job(job) {}
	virtual ~TwitterJobAbortCondition() {}
	virtual int IsAborted() { return (m_job != NULL) ? (int)m_job->IsCanceled() : 0; }

	TwJobItem *m_job;
};

/* Helper: wide substring search using sce_paf_wcschr + sce_paf_wcsncmp */
static const wchar_t *FindWideSubstr(const wchar_t *haystack, const wchar_t *needle)
{
	if (haystack == NULL || needle == NULL)
		return NULL;
	SceSize needleLen = sce_paf_wcslen(needle);

	if (needleLen == 0)
		return haystack;
	for (const wchar_t *p = haystack; (p = sce_paf_wcschr(p, needle[0])) != NULL; ++p) {
		if (sce_paf_wcsncmp(p, needle, needleLen) == 0)
			return p;
	}
	return NULL;
}

/* Helper: check whether Twitter OAuth access tokens exist in registry */
static bool HasTwitterOAuthTokens()
{
	LxRegistry reg;

	reg.Init();
	reg.Term();
	return true;
}

/* Twitter REST session wrapper held by TwitterDialogBase */
class TwitterSession {
public:
	TwitterSession();
	~TwitterSession();

	void Init(Plugin *plugin);
	void Term();

	LxString ReplaceNormalWithBigger(const LxString &url) const;
	SceInt32 VerifyAccount(TwJobItem *job, LxString &outErrorMsg);
	SceInt32 GetMediaConfig(TwJobItem *job, SceInt32 *outCharsPerMedia,
				SceInt32 *outShortUrlLen, SceInt32 *outShortUrlHttpsLen,
				LxString &outErrorMsg);
	SceInt32 DownloadFaceIcon(TwJobItem *job,
				  intrusive_ptr<graph::Surface> &outSurface,
				  LxString &outErrorMsg);
	SceInt32 PostStatus(TwJobItem *job, const char *statusText,
			    const char *mediaPath, LxString &outErrorMsg);

	const char *GetScreenName() const { return m_screenName.CStr(); }

	LxString m_profileImageUrl;
	LxString m_screenName;
};

/* Sub-dialog launcher for Twitter login when tokens are missing or expired */
class TwitterLoginSubPage {
public:
	TwitterLoginSubPage()
		: m_owner(NULL), m_pageWidget(NULL), m_mode(0),
		  m_colorFlag(0), m_dimHeader(0)
	{
		sce_paf_memset(&m_bgColor, 0, sizeof(m_bgColor));
		sce_paf_memset(&m_textColor, 0, sizeof(m_textColor));
	}

	void Launch(ui::Widget *pageWidget);
	void OnFinished();
	void OnFailed(SceInt32 resultCode);

	TwitterDialogBase *m_owner;
	ui::Widget *m_pageWidget;
	SceInt32 m_mode;
	SceCommonDialogColor m_bgColor;
	SceCommonDialogColor m_textColor;
	SceInt32 m_colorFlag;
	SceInt32 m_dimHeader;
};

/* Tweet composition page controller */
class TweetPage : public TwJobController {
public:
	TweetPage();
	virtual ~TweetPage();

	void SetParams(const TwitterDialogParam &param);
	void OnPageInit(ui::Widget *pageWidget);
	void UpdateCharCounter(SceUInt32 charCount, bool disableTweetBtn);
	SceUInt32 ComputeTweetLength(const LxWString &text) const;
	void RefreshTextState();
	void LaunchPhotoImport();
	void HideAttachedPreview();
	void OnPhotoImportFinished(const PhotoImportSubResult &result);
	void StartJob(TweetJobId jobId, const char *jobName);

	SceInt32 LoadAttachedImage(TwJobItem *job);
	SceInt32 OnFaceIconLoaded(TwJobItem *job,
				  intrusive_ptr<graph::Surface> &surface,
				  const char *screenName);
	SceInt32 OnConfigLoaded(TwJobItem *job, SceInt32 charsPerMedia,
				SceInt32 shortUrlLen, SceInt32 shortUrlHttpsLen);
	void OnImageJobDone(SceInt32 result);
	void OnTweetJobDone(SceInt32 result);

	const char *GetTweetText() const { return m_tweetUtf8.CStr(); }
	const char *GetMediaPath() const { return m_mediaPath.c_str(); }

	static void OnCancelClick(int event, ui::Handler *handler, ui::Event *ev, void *userData);
	static void OnTweetClick(int event, ui::Handler *handler, ui::Event *ev, void *userData);
	static void OnPhotoBtnClick(int event, ui::Handler *handler, ui::Event *ev, void *userData);
	static void OnRemoveMediaClick(int event, ui::Handler *handler, ui::Event *ev, void *userData);

	TwitterDialogBase *m_owner;
	ui::Widget *m_pageWidget;
	wstring m_initialText;
	string m_initialMediaPath;
	SceCommonDialogColor m_bgColor;
	SceCommonDialogColor m_textColor;
	SceInt32 m_colorFlag;
	SceInt32 m_disablePhotoImport;
	SceInt32 m_requireText;
	SceInt32 m_requirePhoto;
	SceInt32 m_hideRemoveButton;
	SceInt32 m_photoFilter;
	ui::Widget *m_contentRoot;
	ui::TextBox *m_textBox;
	ui::Widget *m_headerBox;
	ui::Widget *m_previewImage;
	ui::Widget *m_removeBtn;
	ui::ButtonBase *m_photoBtn;
	SceInt32 m_charsPerMedia;
	SceInt32 m_shortUrlLen;
	SceInt32 m_shortUrlHttpsLen;
	SceUInt32 m_lastCharCount;
	string m_mediaPath;
	LxString m_tweetUtf8;
	char m_importedPhotoPath[1024];
};

/* Base Twitter dialog application */
class TwitterDialogBase {
public:
	TwitterDialogBase();
	virtual ~TwitterDialogBase();

	virtual void FinishDialog(SceInt32 resultCode);
	virtual void OpenPage(uint32_t pageIdHash) { m_pageMgr.OpenPage(pageIdHash); }
	virtual void ClosePage(uint32_t pageIdHash) { m_pageMgr.ClosePage(pageIdHash); }
	virtual void OpenPageWithEffect(uint32_t pageIdHash, float dur, int type)
	{
		m_pageMgr.OpenPageWithEffect(pageIdHash, dur, type);
	}
	virtual void ClosePageWithEffect(uint32_t pageIdHash, float dur, int type)
	{
		m_pageMgr.ClosePageWithEffect(pageIdHash, dur, type);
	}
	virtual void OpenPageWithAnim(uint32_t pageIdHash, int type)
	{
		m_pageMgr.OpenPageWithAnim(pageIdHash, type);
	}
	virtual void ClosePageWithAnim(uint32_t pageIdHash, int type)
	{
		m_pageMgr.ClosePageWithAnim(pageIdHash, type);
	}
	virtual intrusive_ptr<graph::Surface> GetTexture(uint32_t texIdHash);
	virtual const wchar_t *GetString(const IDParam &id);
	virtual void OnPhotoImportStart();
	virtual void OnPhotoImportDone();
	virtual void MarkTweetPending() { m_tweetSubmitted = true; }
	virtual bool IsTweetPending() const { return m_tweetSubmitted; }

	void OnLoad(Plugin *plugin);
	void OnStart();
	void OnStop();
	void OnUnload();
	void ShowConfirmDialog(const wchar_t *message, void (*cb)(int, void *), void *userData);
	void OpenLoginSubPage() { OpenPage(TW_PAGE_LOGIN_SUB_ID); }
	void OnPermissionAccepted();

	static void OnUpdate(void *arg);
	static void OnCancelJobsDone(void *arg);
	static void OnSubaccountErrorClosed(int buttonId, void *userData);
	static void OnNoBindConfirmClosed(int buttonId, void *userData);

	TwPageManager m_pageMgr;
	TwitterSession m_session;
	TwitterLoginSubPage m_loginSubPage;
	TwPageHandler m_loginSubHandler;
	TwPageHandler m_tweetPageHandler;
	TweetPage m_tweetPage;
	TwWaitDialog m_waitDialog;
	TwErrorDialog m_errorDialog;
	bool m_tweetSubmitted;
	bool m_subDialogActive;
};

/* Concrete Twitter dialog application */
class TwitterDialogApp : public TwitterDialogBase {
public:
	TwitterDialogApp();
	virtual ~TwitterDialogApp();

	void PushWaitDialog();
	void PopWaitDialog();
	SceInt32 CheckAppPermission(TwJobItem *job, LxString &outErrorMsg);
	SceInt32 GrantAppPermission();

	void HandleJobStart(SceInt32 jobId);
	SceInt32 HandleJobRun(SceInt32 jobId, TwJobItem *job);
	void HandleJobComplete(SceInt32 jobId, SceInt32 result);

	static void OnJobStart(void *userData, void *ctx);
	static int OnJobRun(void *userData, TwJobItem *job, void *ctx);
	static void OnJobComplete(int result, void *userData, void *ctx);
	static void OnJobCancel(int result, void *userData, void *ctx, bool force);
	static void OnErrorCloseToCancel(int buttonId, void *userData);
	static void OnPermissionConfirmClosed(int buttonId, void *userData);
	static void OnWaitCancel(int buttonId, void *userData);
	static void OnNotAllowedErrorClosed(int buttonId, void *userData);

	SceInt32 m_waitRefCount;
	string m_titleId;
	bool m_appAuthorized;
	SceInt32 m_authRetryCount;
};

/* Singleton instance */
static TwitterDialogApp s_twitterApp;
static TwitterPluginInterface s_pluginInterface;

/* Page init callback forwarders */
static void OnLoginSubPageInit(ui::Widget *pageWidget, void *userData)
{
	TwitterLoginSubPage *subPage = (TwitterLoginSubPage *)userData;

	if (subPage != NULL)
		subPage->Launch(pageWidget);
}

static void OnTweetPageInit(ui::Widget *pageWidget, void *userData)
{
	TweetPage *tweetPage = (TweetPage *)userData;

	if (tweetPage != NULL)
		tweetPage->OnPageInit(pageWidget);
}

/* TwitterSession */
TwitterSession::TwitterSession()
	: m_profileImageUrl(), m_screenName()
{
	LxLoginSession::InitHeap();
}

TwitterSession::~TwitterSession()
{
	m_profileImageUrl.Assign(NULL, 0);
	m_screenName.Assign(NULL, 0);
	LxLoginSession::TermHeap();
}

void TwitterSession::Init(Plugin *plugin)
{
	LxLoginSession::Init(plugin);
}

void TwitterSession::Term()
{
	LxLoginSession::Term();
}

LxString TwitterSession::ReplaceNormalWithBigger(const LxString &url) const
{
	const char *str = url.CStr();
	const char *pos = NULL;

	if (str != NULL) {
		for (const char *p = str; (p = sce_paf_strchr(p, '_')) != NULL; ++p) {
			if (sce_paf_strncmp(p, "_normal", 7) == 0) {
				pos = p;
				break;
			}
		}
	}
	if (pos == NULL)
		return url;
	SceSize prefixLen = (SceSize)(pos - str);
	LxString out(str, prefixLen);

	out.Append("_bigger", 7);
	out.Append(pos + 7, url.Length() - prefixLen - 7);
	return out;
}

SceInt32 TwitterSession::VerifyAccount(TwJobItem *job, LxString &outErrorMsg)
{
	TwitterJobAbortCondition abortCond(job);
	LxOAuthConfig config(&abortCond, 0);
	LxTwitterApiClient client(config);
	LxVariant json;
	SceInt32 ret = client.VerifyCredentials(json);

	if (ret >= 0) {
		const LxVariant &imgUrl = LxVariantGetMember(json, LxString("profile_image_url"));

		if (imgUrl.m_type == LxVariant::TYPE_STRING && imgUrl.m_value.strVal != NULL)
			m_profileImageUrl = ReplaceNormalWithBigger(*imgUrl.m_value.strVal);

		const LxVariant &name = LxVariantGetMember(json, LxString("screen_name"));

		if (name.m_type == LxVariant::TYPE_STRING && name.m_value.strVal != NULL)
			m_screenName = *name.m_value.strVal;
	} else {
		SceInt32 httpStatus = 0;

		LxTwitterApiClient::ExtractHttpError(json, &httpStatus, outErrorMsg);
	}
	return ret;
}

SceInt32 TwitterSession::GetMediaConfig(TwJobItem *job, SceInt32 *outCharsPerMedia,
					SceInt32 *outShortUrlLen,
					SceInt32 *outShortUrlHttpsLen,
					LxString &outErrorMsg)
{
	TwitterJobAbortCondition abortCond(job);
	LxOAuthConfig config(&abortCond, 0);
	LxTwitterApiClient client(config);
	LxVariant json;
	SceInt32 ret = client.GetConfiguration(json);

	if (ret == 0) {
		const LxVariant &media = LxVariantGetMember(
			json, LxString("characters_reserved_per_media"));
		const LxVariant &httpLen = LxVariantGetMember(json, LxString("short_url_length"));
		const LxVariant &httpsLen = LxVariantGetMember(
			json, LxString("short_url_length_https"));

		*outCharsPerMedia = (SceInt32)media.m_value.intVal;
		*outShortUrlLen = (SceInt32)httpLen.m_value.intVal;
		*outShortUrlHttpsLen = (SceInt32)httpsLen.m_value.intVal;
	} else {
		SceInt32 httpStatus = 0;

		LxTwitterApiClient::ExtractHttpError(json, &httpStatus, outErrorMsg);
	}
	return ret;
}

SceInt32 TwitterSession::DownloadFaceIcon(TwJobItem *job,
					  intrusive_ptr<graph::Surface> &outSurface,
					  LxString &outErrorMsg)
{
	(void)outSurface;
	(void)outErrorMsg;
	if (m_profileImageUrl.Empty())
		return 0;
	TwitterJobAbortCondition abortCond(job);
	common::SharedPtr<LxPafBuffer> buf;

	return LxHttpDownloadUrl(m_profileImageUrl.CStr(), LxGetHeapAllocator(), buf,
				 &abortCond, 0x100000ULL);
}

SceInt32 TwitterSession::PostStatus(TwJobItem *job, const char *statusText,
				    const char *mediaPath, LxString &outErrorMsg)
{
	TwitterJobAbortCondition abortCond(job);
	LxOAuthConfig config(&abortCond, 0);
	LxTwitterApiClient client(config);
	LxVariant json;
	LxTweetOptions opts;

	if (mediaPath != NULL && mediaPath[0] != '\0')
		opts.mediaPath.Assign(mediaPath, sce_paf_strlen(mediaPath));

	SceInt32 ret = client.PostTweet(json, statusText, opts);

	if (ret < 0) {
		SceInt32 httpStatus = 0;

		LxTwitterApiClient::ExtractHttpError(json, &httpStatus, outErrorMsg);
	}
	return ret;
}

/* TwitterLoginSubPage */
void TwitterLoginSubPage::Launch(ui::Widget *pageWidget)
{
	m_pageWidget = pageWidget;

	TwLoginSubParam param;

	sceClibMemset(&param, 0, sizeof(param));
	param.sdkVersion = 0x03650011U;
	param.commonParam.magic = SCE_COMMON_DIALOG_MAGIC_NUMBER + (SceUInt32)(uintptr_t)&param.commonParam;
	param.mode = m_mode;
	param.textColor = m_textColor;
	param.colorFlag = m_colorFlag;
	param.dimHeader = m_dimHeader;
	SceCommonDialog_463DB9ED(&param);
}

void TwitterLoginSubPage::OnFinished()
{
	sceTwLoginDialogTerm();
	if (m_owner != NULL)
		m_owner->ClosePage(TW_PAGE_LOGIN_SUB_ID);
}

void TwitterLoginSubPage::OnFailed(SceInt32 resultCode)
{
	sceTwLoginDialogTerm();
	if (m_owner != NULL) {
		m_owner->ClosePage(TW_PAGE_LOGIN_SUB_ID);
		m_owner->FinishDialog(resultCode);
	}
}

/* TweetPage */
TweetPage::TweetPage()
	: m_owner(NULL), m_pageWidget(NULL), m_initialText(), m_initialMediaPath(),
	  m_colorFlag(0), m_disablePhotoImport(0), m_requireText(0), m_requirePhoto(0),
	  m_hideRemoveButton(0), m_photoFilter(0), m_contentRoot(NULL), m_textBox(NULL),
	  m_headerBox(NULL), m_previewImage(NULL), m_removeBtn(NULL), m_photoBtn(NULL),
	  m_charsPerMedia(21), m_shortUrlLen(22), m_shortUrlHttpsLen(23),
	  m_lastCharCount(0), m_mediaPath(), m_tweetUtf8()
{
	sce_paf_memset(&m_bgColor, 0, sizeof(m_bgColor));
	sce_paf_memset(&m_textColor, 0, sizeof(m_textColor));
	sce_paf_memset(m_importedPhotoPath, 0, sizeof(m_importedPhotoPath));
}

TweetPage::~TweetPage()
{
	m_contentRoot = NULL;
	m_previewImage = NULL;
	m_removeBtn = NULL;
}

void TweetPage::SetParams(const TwitterDialogParam &param)
{
	if (param.commonParam.bgColor != NULL) {
		m_bgColor.r = param.commonParam.bgColor->r;
		m_bgColor.g = param.commonParam.bgColor->g;
		m_bgColor.b = param.commonParam.bgColor->b;
		m_bgColor.a = 0xFF;
	}
	m_textColor = param.textColor;
	m_textColor.a = 0xFF;
	m_colorFlag = param.colorFlag;
	m_disablePhotoImport = param.disablePhotoImport;
	m_requireText = param.requireText;
	m_requirePhoto = param.requirePhoto;
	m_hideRemoveButton = param.hideRemoveButton;
	m_photoFilter = param.photoFilter;
}

SceUInt32 TweetPage::ComputeTweetLength(const LxWString &text) const
{
	const wchar_t *str = text.CStr();
	SceSize len = text.Length();
	SceUInt32 effectiveLen = (SceUInt32)len;
	SceSize pos = 0;

	while (pos < len) {
		const wchar_t *found = FindWideSubstr(str + pos, L"http");

		if (found == NULL)
			break;
		SceSize idx = (SceSize)(found - str);

		pos = idx + 4;
		if (idx > 0) {
			wchar_t prev = str[idx - 1];

			if ((prev >= L'0' && prev <= L'9') ||
			    (prev >= L'a' && prev <= L'z') ||
			    (prev >= L'A' && prev <= L'Z'))
				continue;
		}

		SceUInt32 shortLen = 0;
		SceSize schemeLen = 0;

		if (idx + 7 <= len && sce_paf_wcsncmp(found, L"http://", 7) == 0) {
			shortLen = (SceUInt32)m_shortUrlLen;
			schemeLen = 7;
		} else if (idx + 8 <= len && sce_paf_wcsncmp(found, L"https://", 8) == 0) {
			shortLen = (SceUInt32)m_shortUrlHttpsLen;
			schemeLen = 8;
		} else {
			continue;
		}

		pos = idx + schemeLen;
		while (pos < len) {
			wchar_t ch = str[pos];
			bool isUrlChar = (ch >= 0x23 && ch <= 0x3B) ||
					 (ch >= 0x3F && ch <= 0x5D) ||
					 (ch >= 0x61 && ch <= 0x7A) ||
					 ch == L'!' || ch == L'=' || ch == L'_' || ch == L'~';

			if (!isUrlChar)
				break;
			pos++;
		}
		SceUInt32 urlLen = (SceUInt32)(pos - idx);

		if (urlLen > shortLen)
			effectiveLen -= (urlLen - shortLen);
	}
	return effectiveLen;
}

void TweetPage::UpdateCharCounter(SceUInt32 charCount, bool disableTweetBtn)
{
	if (m_contentRoot != NULL) {
		IDParam tweetBtnId(TW_WIDGET_TWEET_BTN_ID);
		ui::Widget *tweetBtn = m_contentRoot->FindChild(tweetBtnId, 0);

		if (tweetBtn != NULL) {
			ui::ButtonBase *btn = (ui::ButtonBase *)tweetBtn;

			if (!disableTweetBtn && charCount > 0 && charCount <= TW_MAX_TWEET_CHARS)
				btn->Enable(false);
			else
				btn->Disable(false);
		}
	}
	m_lastCharCount = charCount;
}

void TweetPage::RefreshTextState()
{
	if (m_contentRoot == NULL || m_textBox == NULL)
		return;
	wstring text;

	m_textBox->GetString(text);
	LxWString lxText(text.c_str(), text.length());
	SceUInt32 charCount = ComputeTweetLength(lxText);

	if (!m_mediaPath.empty())
		charCount += (SceUInt32)m_charsPerMedia;

	bool allSpaces = true;

	for (SceSize i = 0; i < text.length(); ++i) {
		if (text.c_str()[i] != L' ') {
			allSpaces = false;
			break;
		}
	}

	bool disableBtn = false;

	if (m_requireText != 0)
		disableBtn = allSpaces;
	else if (m_mediaPath.empty())
		disableBtn = allSpaces;
	if (m_requirePhoto != 0 && m_mediaPath.empty())
		disableBtn = true;

	UpdateCharCounter(charCount, disableBtn);
}

void TweetPage::OnPageInit(ui::Widget *pageWidget)
{
	m_pageWidget = pageWidget;
	if (pageWidget == NULL)
		return;

	IDParam guideId(TW_WIDGET_GUIDE_TEXT_ID);
	ui::Widget *guideText = pageWidget->FindChild(guideId, 0);

	if (guideText != NULL && (sceKernelGetModelForCDialog() & 0x20000) != 0 &&
	    m_owner != NULL) {
		IDParam tvGuideId("msg_twi_tweet_guide_tv");
		const wchar_t *tvGuideStr = m_owner->GetString(tvGuideId);

		if (tvGuideStr != NULL) {
			wstring ws(tvGuideStr);

			guideText->SetString(ws);
		}
	}

	m_mediaPath = m_initialMediaPath;

	IDParam rootId(TW_WIDGET_CONTENT_ROOT_ID);

	m_contentRoot = pageWidget->FindChild(rootId, 0);
	if (m_contentRoot != NULL) {
		IDParam headerId(TW_WIDGET_HEADER_BOX_ID);
		IDParam previewId(TW_WIDGET_PREVIEW_IMG_ID);
		IDParam removeId(TW_WIDGET_REMOVE_BTN_ID);
		IDParam photoId(TW_WIDGET_PHOTO_BTN_ID);
		IDParam tweetBtnId(TW_WIDGET_TWEET_BTN_ID);
		IDParam cancelBtnId(TW_WIDGET_CANCEL_BTN_ID);
		IDParam textBoxId(TW_WIDGET_TEXTBOX_ID);

		m_headerBox = m_contentRoot->FindChild(headerId, 0);
		m_previewImage = m_contentRoot->FindChild(previewId, 2);
		m_removeBtn = m_contentRoot->FindChild(removeId, 2);

		ui::Widget *photoWidget = m_contentRoot->FindChild(photoId, 0);

		if (photoWidget != NULL)
			m_photoBtn = (ui::ButtonBase *)photoWidget;

		ui::Widget *tb = m_contentRoot->FindChild(textBoxId, 0);

		if (tb != NULL)
			m_textBox = (ui::TextBox *)tb;

		if (m_removeBtn != NULL) {
			m_removeBtn->AddEventCallback(TW_EV_BUTTON_DECIDE, OnRemoveMediaClick,
						      this);
		}
		if (m_photoBtn != NULL) {
			if (m_disablePhotoImport != 0) {
				m_photoBtn->Disable(false);
				m_photoBtn = NULL;
			} else {
				m_photoBtn->AddEventCallback(TW_EV_BUTTON_DECIDE, OnPhotoBtnClick,
							     this);
				if (!m_mediaPath.empty())
					m_photoBtn->Disable(false);
				else
					m_photoBtn->Enable(false);
			}
		}

		ui::Widget *tweetBtn = m_contentRoot->FindChild(tweetBtnId, 0);

		if (tweetBtn != NULL)
			tweetBtn->AddEventCallback(TW_EV_BUTTON_DECIDE, OnTweetClick, this);

		ui::Widget *cancelBtn = m_contentRoot->FindChild(cancelBtnId, 0);

		if (cancelBtn != NULL)
			cancelBtn->AddEventCallback(TW_EV_BUTTON_DECIDE, OnCancelClick, this);
	}

	if (!m_mediaPath.empty())
		StartJob(TWEET_JOB_IMAGE, "TweetPageJob_Image");
	StartJob(TWEET_JOB_FACEICON, "TweetPageJob_Faceicon");
	RefreshTextState();
}

void TweetPage::LaunchPhotoImport()
{
	PhotoImportSubParam param;
	PhotoImportFileData fileData;

	sceClibMemset(&param, 0, sizeof(param));
	param.sdkVersion = 0xFFFFFFFFU;
	param.commonParam.magic = SCE_COMMON_DIALOG_MAGIC_NUMBER + (SceUInt32)(uintptr_t)&param.commonParam;
	param.mode = 0;
	param.visibleCategory = (m_photoFilter != 0) ? 4 : 7;
	param.itemCount = 1;

	sceClibMemset(&fileData, 0, sizeof(fileData));
	fileData.maxCount = 60;
	SceCommonDialog_FCDA2F0D(&param, &fileData);
}

void TweetPage::HideAttachedPreview()
{
	if (m_previewImage != NULL) {
		math::v4 zeroSize(0.0f, 0.0f, 0.0f, 0.0f);

		m_previewImage->SetSize(zeroSize, NULL, 0, 0, 0);
	}
	if (m_removeBtn != NULL) {
		math::v4 zeroSize(0.0f, 0.0f, 0.0f, 0.0f);

		m_removeBtn->SetSize(zeroSize, NULL, 0, 0, 0);
	}
}

void TweetPage::OnPhotoImportFinished(const PhotoImportSubResult &result)
{
	if (result.result == 0) {
		m_mediaPath = m_importedPhotoPath;
		StartJob(TWEET_JOB_IMAGE, "TweetPageJob_Image");
	}
	if (m_mediaPath.empty())
		HideAttachedPreview();
	RefreshTextState();
	if (m_photoBtn != NULL) {
		if (!m_mediaPath.empty())
			m_photoBtn->Disable(false);
		else
			m_photoBtn->Enable(false);
	}
}

void TweetPage::StartJob(TweetJobId jobId, const char *jobName)
{
	new TwJobItem((void *)(intptr_t)jobId, this, this, jobName);
}

SceInt32 TweetPage::LoadAttachedImage(TwJobItem *job)
{
	(void)job;
	return 0;
}

SceInt32 TweetPage::OnFaceIconLoaded(TwJobItem *job,
				     intrusive_ptr<graph::Surface> &surface,
				     const char *screenName)
{
	(void)job;
	(void)surface;
	(void)screenName;
	return 0;
}

SceInt32 TweetPage::OnConfigLoaded(TwJobItem *job, SceInt32 charsPerMedia,
				   SceInt32 shortUrlLen, SceInt32 shortUrlHttpsLen)
{
	(void)job;
	m_charsPerMedia = charsPerMedia;
	m_shortUrlLen = shortUrlLen;
	m_shortUrlHttpsLen = shortUrlHttpsLen;
	return 0;
}

void TweetPage::OnImageJobDone(SceInt32 result)
{
	(void)result;
	if (m_photoBtn != NULL) {
		if (!m_mediaPath.empty())
			m_photoBtn->Disable(false);
		else
			m_photoBtn->Enable(false);
	}
	RefreshTextState();
}

void TweetPage::OnTweetJobDone(SceInt32 result)
{
	if (result == 0 && m_owner != NULL) {
		TwInfobar::Hide();
		m_owner->ClosePage(TW_PAGE_TWEET_ID);
		m_owner->FinishDialog(0);
	}
}

void TweetPage::OnCancelClick(int event, ui::Handler *handler, ui::Event *ev, void *userData)
{
	(void)event;
	(void)handler;
	(void)ev;
	TweetPage *self = (TweetPage *)userData;

	if (self != NULL && self->m_owner != NULL) {
		TwInfobar::Hide();
		self->m_owner->ClosePage(TW_PAGE_TWEET_ID);
		self->m_owner->FinishDialog(1);
	}
}

void TweetPage::OnTweetClick(int event, ui::Handler *handler, ui::Event *ev, void *userData)
{
	(void)event;
	(void)handler;
	(void)ev;
	TweetPage *self = (TweetPage *)userData;

	if (self == NULL || self->m_lastCharCount == 0 ||
	    self->m_lastCharCount > TW_MAX_TWEET_CHARS)
		return;
	if (self->m_textBox != NULL) {
		wstring text;

		self->m_textBox->GetString(text);
		string utf8;

		common::Utf16ToUtf8(text.c_str(), &utf8);
		self->m_tweetUtf8.Assign(utf8.c_str(), utf8.length());
	}
	if (self->m_owner != NULL)
		self->m_owner->MarkTweetPending();
	self->StartJob(TWEET_JOB_APPINFO_GET, "TweetPageJob_AppInfoGet");
}

void TweetPage::OnPhotoBtnClick(int event, ui::Handler *handler, ui::Event *ev, void *userData)
{
	(void)event;
	(void)handler;
	(void)ev;
	TweetPage *self = (TweetPage *)userData;

	if (self != NULL && self->m_owner != NULL)
		self->m_owner->OnPhotoImportStart();
}

void TweetPage::OnRemoveMediaClick(int event, ui::Handler *handler, ui::Event *ev,
				   void *userData)
{
	(void)event;
	(void)handler;
	(void)ev;
	TweetPage *self = (TweetPage *)userData;

	if (self == NULL)
		return;
	self->m_mediaPath.clear();
	self->HideAttachedPreview();
	self->RefreshTextState();
	if (self->m_photoBtn != NULL)
		self->m_photoBtn->Enable(false);
}

/* TwitterDialogBase */
TwitterDialogBase::TwitterDialogBase()
	: m_pageMgr(), m_session(), m_loginSubPage(), m_loginSubHandler(),
	  m_tweetPageHandler(), m_tweetPage(), m_waitDialog(), m_errorDialog(),
	  m_tweetSubmitted(false), m_subDialogActive(false)
{
	m_pageMgr.m_entries = s_twitterPageEntries;
	m_loginSubPage.m_owner = this;
	m_tweetPage.m_owner = this;
	m_loginSubHandler.m_onPageInit = OnLoginSubPageInit;
	m_loginSubHandler.m_userData = &m_loginSubPage;
	m_tweetPageHandler.m_onPageInit = OnTweetPageInit;
	m_tweetPageHandler.m_userData = &m_tweetPage;
	m_pageMgr.RegisterHandler(TW_PAGE_LOGIN_SUB_ID, &m_loginSubHandler);
	m_pageMgr.RegisterHandler(TW_PAGE_TWEET_ID, &m_tweetPageHandler);
}

TwitterDialogBase::~TwitterDialogBase()
{
	m_pageMgr.UnregisterHandler(TW_PAGE_TWEET_ID);
	m_pageMgr.UnregisterHandler(TW_PAGE_LOGIN_SUB_ID);
}

void TwitterDialogBase::OnLoad(Plugin *plugin)
{
	m_pageMgr.Init(plugin);
	TwInitPluginContext(plugin);
	m_session.Init(plugin);
	m_errorDialog.Init(plugin);
}

void TwitterDialogBase::OnUnload()
{
	m_session.Term();
	m_pageMgr.Term();
}

void TwitterDialogBase::ShowConfirmDialog(const wchar_t *message, void (*cb)(int, void *),
					  void *userData)
{
	wstring msg(message != NULL ? message : L"");

	m_waitDialog.Open(m_pageMgr.m_plugin, msg, 2, cb, userData);
}

void TwitterDialogBase::OnPermissionAccepted()
{
	m_waitDialog.Close();
	m_tweetPage.StartJob(TWEET_JOB_APPINFO_UPDATE, "TweetPageJob_AppinfoUpdate");
}

void TwitterDialogBase::OnStart()
{
	common::MainThreadCallList::Register(OnUpdate, this);
	if (SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_TWITTER) != 0)
		return;
	CommonDialogTwitterParam *work = SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_TWITTER);

	if (work == NULL) {
		SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_TWITTER);
		return;
	}
	sce_paf_strncpy(work->apiName, "Start", sizeof(work->apiName));
	work->apiName[sizeof(work->apiName) - 1] = '\0';

	m_loginSubPage.m_mode = work->param.mode;
	if (work->param.commonParam.bgColor != NULL)
		m_loginSubPage.m_bgColor = *work->param.commonParam.bgColor;
	m_loginSubPage.m_textColor = work->param.textColor;
	m_loginSubPage.m_colorFlag = work->param.colorFlag;
	m_loginSubPage.m_dimHeader = work->param.disablePhotoImport;

	if (work->param.mode == TWITTER_MODE_LOGIN_ONLY) {
		if (HasTwitterOAuthTokens()) {
			OpenPage(TW_PAGE_LOGIN_SUB_ID);
		} else {
			IDParam noBindId("msg_twi_no_bind");
			const wchar_t *msg = GetString(noBindId);

			ShowConfirmDialog(msg, OnNoBindConfirmClosed, this);
		}
	} else {
		SceInt32 chatRestricted = 0;

		if (sceNpManagerGetChatRestrictionFlag(&chatRestricted) != 0 ||
		    chatRestricted != 0) {
			IDParam subId("msg_error_use_this_service_subaccount");
			const wchar_t *msg = GetString(subId);
			LxString empty;

			m_errorDialog.SetError(0, empty);
			(void)msg;
			m_errorDialog.Show(OnSubaccountErrorClosed, this, 1, 0);
		} else {
			m_tweetPage.SetParams(work->param);
			if (HasTwitterOAuthTokens()) {
				m_tweetPage.StartJob(TWEET_JOB_APPINFO_GET,
						     "TweetPageJob_AppinfoGet");
			} else {
				IDParam noBindId("msg_twi_no_bind");
				const wchar_t *msg = GetString(noBindId);

				ShowConfirmDialog(msg, OnNoBindConfirmClosed, this);
			}
		}
	}

	if (sce_paf_strncmp(work->apiName, "Start", sizeof(work->apiName)) == 0) {
		sce_paf_memset(work->apiName, 0, sizeof(work->apiName));
		SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_TWITTER);
	}
}

void TwitterDialogBase::OnStop()
{
	common::MainThreadCallList::Unregister(OnUpdate, this);
	m_pageMgr.CloseAll();
}

void TwitterDialogBase::FinishDialog(SceInt32 resultCode)
{
	if (SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_TWITTER) != 0)
		return;
	CommonDialogTwitterParam *work = SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_TWITTER);

	if (work == NULL) {
		SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_TWITTER);
		return;
	}
	sce_paf_strncpy(work->apiName, "Term", sizeof(work->apiName));
	work->apiName[sizeof(work->apiName) - 1] = '\0';

	sce_paf_memset(&work->result, 0, sizeof(work->result));
	work->result.result = 1;
	if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_TWITTER) != 0) {
		work->result.errorCode = 2;
	} else if (resultCode >= 0 || resultCode == (SceInt32)0x80106001) {
		work->result.errorCode = resultCode;
	} else {
		work->result.errorCode = TW_ERROR_GENERIC_FAIL;
	}

	if (m_subDialogActive) {
		work->subDialogState = 3;
	} else {
		work->subDialogState = 0;
		common::MainThreadCallList::Unregister(OnUpdate, NULL);
		SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_TWITTER);
	}

	if (sce_paf_strncmp(work->apiName, "Term", sizeof(work->apiName)) == 0) {
		sce_paf_memset(work->apiName, 0, sizeof(work->apiName));
		SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_TWITTER);
	}
}

intrusive_ptr<graph::Surface> TwitterDialogBase::GetTexture(uint32_t texIdHash)
{
	IDParam id(texIdHash);

	return m_pageMgr.m_plugin->GetTexture(id);
}

const wchar_t *TwitterDialogBase::GetString(const IDParam &id)
{
	return m_pageMgr.m_plugin->GetString(id);
}

void TwitterDialogBase::OnPhotoImportStart()
{
	m_subDialogActive = true;
	m_tweetPage.LaunchPhotoImport();
}

void TwitterDialogBase::OnPhotoImportDone()
{
	PhotoImportSubResult res;

	sce_paf_memset(&res, 0, sizeof(res));
	scePhotoImportDialogGetResult(&res);
	scePhotoImportDialogTerm();
	m_subDialogActive = false;
	m_tweetPage.OnPhotoImportFinished(res);
}

void TwitterDialogBase::OnSubaccountErrorClosed(int buttonId, void *userData)
{
	(void)buttonId;
	TwitterDialogBase *self = (TwitterDialogBase *)userData;

	if (self != NULL)
		self->FinishDialog(3);
}

void TwitterDialogBase::OnNoBindConfirmClosed(int buttonId, void *userData)
{
	TwitterDialogBase *self = (TwitterDialogBase *)userData;

	if (self == NULL)
		return;
	if (buttonId == 4)
		self->OpenPage(TW_PAGE_LOGIN_SUB_ID);
	else
		self->FinishDialog(1);
}

void TwitterDialogBase::OnCancelJobsDone(void *arg)
{
	(void)arg;
	SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_TWITTER);
}

void TwitterDialogBase::OnUpdate(void *arg)
{
	TwitterDialogBase *self = (TwitterDialogBase *)arg;

	if (SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_TWITTER) != 0) {
		self->m_errorDialog.Close();
		self->m_waitDialog.Close();
		common::MainThreadCallList::Unregister(OnUpdate, self);
		self->m_pageMgr.CloseAll();
		if (self->m_tweetPage.CancelAll(OnCancelJobsDone, self, true) <= 0)
			SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_TWITTER);
		return;
	}
	CommonDialogTwitterParam *work = SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_TWITTER);

	if (work == NULL) {
		SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_TWITTER);
		self->m_errorDialog.Close();
		self->m_waitDialog.Close();
		common::MainThreadCallList::Unregister(OnUpdate, self);
		self->m_pageMgr.CloseAll();
		if (self->m_tweetPage.CancelAll(OnCancelJobsDone, self, true) <= 0)
			SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_TWITTER);
		return;
	}

	sce_paf_strncpy(work->apiName, "OnUpdate", sizeof(work->apiName));
	work->apiName[sizeof(work->apiName) - 1] = '\0';

	if (work->subDialogState == 2) {
		if (sceTwLoginDialogGetStatus() == 2) {
			sceTwLoginDialogTerm();
			self->m_errorDialog.Close();
			self->m_waitDialog.Close();
			common::MainThreadCallList::Unregister(OnUpdate, self);
			self->m_pageMgr.CloseAll();
			if (self->m_tweetPage.CancelAll(OnCancelJobsDone, self, true) <= 0)
				SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_TWITTER);
		}
		if (scePhotoImportDialogGetStatus() == 2) {
			scePhotoImportDialogTerm();
			self->m_errorDialog.Close();
			self->m_waitDialog.Close();
			common::MainThreadCallList::Unregister(OnUpdate, self);
			self->m_pageMgr.CloseAll();
			if (self->m_tweetPage.CancelAll(OnCancelJobsDone, self, true) <= 0)
				SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_TWITTER);
		}
	} else if (work->subDialogState == 4) {
		if (sceTwLoginDialogGetStatus() == 2) {
			sceTwLoginDialogTerm();
			work->subDialogState = 5;
		}
		if (scePhotoImportDialogGetStatus() == 2) {
			scePhotoImportDialogTerm();
			work->subDialogState = 5;
		}
	} else if (work->subDialogState == 3) {
		if (sceTwLoginDialogGetStatus() == 1)
			sceTwLoginDialogAbort();
		if (scePhotoImportDialogGetStatus() == 1)
			scePhotoImportDialogAbort();
		work->subDialogState = 4;
	} else if (self->m_subDialogActive && scePhotoImportDialogGetStatus() == 2) {
		self->OnPhotoImportDone();
	} else if (sceTwLoginDialogGetStatus() == 2) {
		self->m_loginSubPage.OnFinished();
	}

	if (sce_paf_strncmp(work->apiName, "OnUpdate", sizeof(work->apiName)) == 0) {
		sce_paf_memset(work->apiName, 0, sizeof(work->apiName));
		SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_TWITTER);
	}
}

/* TwitterDialogApp */
TwitterDialogApp::TwitterDialogApp()
	: TwitterDialogBase(), m_waitRefCount(0), m_titleId(), m_appAuthorized(false),
	  m_authRetryCount(0)
{
	m_tweetPage.m_startFn = OnJobStart;
	m_tweetPage.m_startCtx = this;
	m_tweetPage.m_runFn = OnJobRun;
	m_tweetPage.m_runCtx = this;
	m_tweetPage.m_completeFn = OnJobComplete;
	m_tweetPage.m_completeCtx = this;
	m_tweetPage.m_cancelFn = OnJobCancel;
	m_tweetPage.m_cancelCtx = this;
}

TwitterDialogApp::~TwitterDialogApp()
{
}

void TwitterDialogApp::PushWaitDialog()
{
	if (m_waitRefCount == 0) {
		IDParam waitStrId(TW_STR_WAIT_LOADING_ID);
		const wchar_t *msg = GetString(waitStrId);
		wstring ws(msg != NULL ? msg : L"");

		m_waitDialog.Open(m_pageMgr.m_plugin, ws, 3, NULL, NULL);
	}
	m_waitRefCount++;
}

void TwitterDialogApp::PopWaitDialog()
{
	if (m_waitRefCount > 0) {
		m_waitRefCount--;
		if (m_waitRefCount == 0)
			m_waitDialog.Close();
	}
}

SceInt32 TwitterDialogApp::CheckAppPermission(TwJobItem *job, LxString &outErrorMsg)
{
	(void)job;
	m_appAuthorized = false;
	outErrorMsg.Assign(NULL, 0);

	char titleIdBuf[16];
	SceInt32 ret = sceAppMgrAppParamGetString(0, 12, titleIdBuf, 10);

	if (ret != 0)
		return ret;
	m_titleId = titleIdBuf;
	m_appAuthorized = true;
	return 0;
}

SceInt32 TwitterDialogApp::GrantAppPermission()
{
	if (m_titleId.empty()) {
		char titleIdBuf[16];
		SceInt32 ret = sceAppMgrAppParamGetString(0, 12, titleIdBuf, 10);

		if (ret != 0)
			return ret;
		m_titleId = titleIdBuf;
	}
	m_appAuthorized = true;
	return 0;
}

void TwitterDialogApp::HandleJobStart(SceInt32 jobId)
{
	m_errorDialog.Init(m_pageMgr.m_plugin);
	switch (jobId) {
	case TWEET_JOB_APPINFO_GET:
	case TWEET_JOB_APPINFO_UPDATE:
	case TWEET_JOB_TOKEN:
	case TWEET_JOB_MEDIA_CONFIG:
	case TWEET_JOB_FACEICON:
	case TWEET_JOB_IMAGE:
		PushWaitDialog();
		break;
	case TWEET_JOB_TWEET: {
		IDParam postStrId(TW_STR_WAIT_POSTING_ID);
		const wchar_t *msg = GetString(postStrId);
		wstring ws(msg != NULL ? msg : L"");

		m_waitDialog.Open(m_pageMgr.m_plugin, ws, 4, OnWaitCancel, this);
		break;
	}
	default:
		break;
	}
}

SceInt32 TwitterDialogApp::HandleJobRun(SceInt32 jobId, TwJobItem *job)
{
	LxString errorMsg;
	SceInt32 ret = -1;

	switch (jobId) {
	case TWEET_JOB_APPINFO_GET:
		ret = CheckAppPermission(job, errorMsg);
		if (ret == 1) {
			m_errorDialog.SetError(TW_ERROR_NOT_ALLOWED, errorMsg);
			m_errorDialog.Show(OnNotAllowedErrorClosed, this, 2, (SceInt16)jobId);
		}
		break;
	case TWEET_JOB_APPINFO_UPDATE:
		ret = GrantAppPermission();
		break;
	case TWEET_JOB_TOKEN:
		ret = m_session.VerifyAccount(job, errorMsg);
		break;
	case TWEET_JOB_MEDIA_CONFIG: {
		SceInt32 charsPerMedia = 0;
		SceInt32 shortUrlLen = 0;
		SceInt32 shortUrlHttpsLen = 0;

		ret = m_session.GetMediaConfig(job, &charsPerMedia, &shortUrlLen,
					       &shortUrlHttpsLen, errorMsg);
		if (ret == 0 && !job->IsCanceled()) {
			ret = m_tweetPage.OnConfigLoaded(job, charsPerMedia, shortUrlLen,
							 shortUrlHttpsLen);
		}
		break;
	}
	case TWEET_JOB_FACEICON: {
		intrusive_ptr<graph::Surface> surface;

		ret = m_session.DownloadFaceIcon(job, surface, errorMsg);
		if (ret == 0 && !job->IsCanceled()) {
			ret = m_tweetPage.OnFaceIconLoaded(job, surface,
							   m_session.GetScreenName());
		}
		break;
	}
	case TWEET_JOB_IMAGE:
		ret = m_tweetPage.LoadAttachedImage(job);
		break;
	case TWEET_JOB_TWEET:
		ret = m_session.PostStatus(job, m_tweetPage.GetTweetText(),
					   m_tweetPage.GetMediaPath(), errorMsg);
		break;
	default:
		break;
	}

	if (ret < 0)
		m_errorDialog.SetError(ret, errorMsg);
	return ret;
}

void TwitterDialogApp::HandleJobComplete(SceInt32 jobId, SceInt32 result)
{
	void (*errorCb)(int, void *) = NULL;
	bool skipErrorDialog = false;

	switch (jobId) {
	case TWEET_JOB_APPINFO_GET:
		PopWaitDialog();
		if (result != 0) {
			errorCb = OnErrorCloseToCancel;
		} else if (m_appAuthorized) {
			if (IsTweetPending())
				m_tweetPage.StartJob(TWEET_JOB_TWEET, "TweetPageJob_Tweet");
			else
				m_tweetPage.StartJob(TWEET_JOB_TOKEN, "TweetPageJob_Token");
		} else if (IsTweetPending()) {
			LxString empty;

			m_errorDialog.SetError(TW_ERROR_PERMISSION_DENIED, empty);
			m_errorDialog.Show(NULL, this, 2, (SceInt16)jobId);
		} else {
			IDParam confId("msg_twi_allow_access_from_this_application_conf");
			IDParam guideId("msg_twi_change_this_setting_later_guide");
			const wchar_t *confStr = GetString(confId);
			const wchar_t *guideStr = GetString(guideId);
			wstring fullMsg(confStr != NULL ? confStr : L"");

			fullMsg.append(L"\n");
			if (guideStr != NULL)
				fullMsg.append(guideStr);
			ShowConfirmDialog(fullMsg.c_str(), OnPermissionConfirmClosed, this);
		}
		break;
	case TWEET_JOB_APPINFO_UPDATE:
		PopWaitDialog();
		if (result != 0) {
			errorCb = OnErrorCloseToCancel;
		} else if (IsTweetPending()) {
			m_tweetPage.StartJob(TWEET_JOB_TWEET, "TweetPageJob_Tweet");
		} else {
			m_tweetPage.StartJob(TWEET_JOB_TOKEN, "TweetPageJob_Token");
		}
		break;
	case TWEET_JOB_TOKEN:
		PopWaitDialog();
		if (result < 0) {
			if (result == TW_ERROR_AUTH_RETRY) {
				m_authRetryCount++;
				if (m_authRetryCount <= 1) {
					OpenLoginSubPage();
					skipErrorDialog = true;
				} else {
					LxString empty;

					m_errorDialog.SetError(TW_ERROR_AUTH_RETRY, empty);
					errorCb = OnErrorCloseToCancel;
					m_authRetryCount = 0;
				}
			} else {
				errorCb = OnErrorCloseToCancel;
			}
		} else {
			m_tweetPage.StartJob(TWEET_JOB_MEDIA_CONFIG, "TweetPageJob_Media");
		}
		break;
	case TWEET_JOB_MEDIA_CONFIG:
		PopWaitDialog();
		OpenPage(TW_PAGE_TWEET_ID);
		break;
	case TWEET_JOB_FACEICON:
		PopWaitDialog();
		break;
	case TWEET_JOB_IMAGE:
		PopWaitDialog();
		m_tweetPage.OnImageJobDone(result);
		break;
	case TWEET_JOB_TWEET:
		m_waitDialog.Close();
		m_tweetPage.OnTweetJobDone(result);
		m_tweetSubmitted = false;
		break;
	default:
		break;
	}

	if (m_waitRefCount == 0 && !skipErrorDialog && errorCb != NULL)
		m_errorDialog.Show(errorCb, this, 2, (SceInt16)jobId);
}

void TwitterDialogApp::OnJobStart(void *userData, void *ctx)
{
	TwitterDialogApp *self = (TwitterDialogApp *)ctx;

	self->HandleJobStart((SceInt32)(intptr_t)userData);
}

int TwitterDialogApp::OnJobRun(void *userData, TwJobItem *job, void *ctx)
{
	TwitterDialogApp *self = (TwitterDialogApp *)ctx;

	return self->HandleJobRun((SceInt32)(intptr_t)userData, job);
}

void TwitterDialogApp::OnJobComplete(int result, void *userData, void *ctx)
{
	TwitterDialogApp *self = (TwitterDialogApp *)ctx;

	self->HandleJobComplete((SceInt32)(intptr_t)userData, result);
}

void TwitterDialogApp::OnJobCancel(int result, void *userData, void *ctx, bool force)
{
	(void)force;
	TwitterDialogApp *self = (TwitterDialogApp *)ctx;
	SceInt32 jobId = (SceInt32)(intptr_t)userData;

	if (jobId == TWEET_JOB_TWEET && result != TW_ERROR_CANCELED &&
	    result != TW_ERROR_ABORTED) {
		self->m_tweetPage.OnTweetJobDone(result);
		self->m_waitDialog.Close();
		self->m_errorDialog.Show(OnErrorCloseToCancel, self, 2, TWEET_JOB_TWEET);
	}
}

void TwitterDialogApp::OnErrorCloseToCancel(int buttonId, void *userData)
{
	(void)buttonId;
	TwitterDialogApp *self = (TwitterDialogApp *)userData;

	if (self != NULL)
		self->FinishDialog(4);
}

void TwitterDialogApp::OnPermissionConfirmClosed(int buttonId, void *userData)
{
	TwitterDialogApp *self = (TwitterDialogApp *)userData;

	if (self == NULL)
		return;
	if (buttonId == 4)
		self->OnPermissionAccepted();
	else
		self->FinishDialog(1);
}

void TwitterDialogApp::OnWaitCancel(int buttonId, void *userData)
{
	(void)buttonId;
	TwitterDialogApp *self = (TwitterDialogApp *)userData;

	if (self != NULL)
		self->m_tweetPage.CancelAll(NULL, NULL, false);
}

void TwitterDialogApp::OnNotAllowedErrorClosed(int buttonId, void *userData)
{
	(void)buttonId;
	TwitterDialogApp *self = (TwitterDialogApp *)userData;

	if (self != NULL)
		self->FinishDialog(3);
}

/* PAF plugin callbacks */
static void TwitterPluginOnBoot()
{
}

static void TwitterPluginOnLoad(Plugin *plugin)
{
	s_twitterApp.OnLoad(plugin);
}

static void TwitterPluginOnStart()
{
	s_twitterApp.OnStart();
}

static void TwitterPluginOnStop()
{
	s_twitterApp.OnStop();
}

static void TwitterPluginOnUnload()
{
	s_twitterApp.OnUnload();
}

/* Module entry points */
extern "C" int module_start(SceSize args, const void *argp)
{
	(void)args;
	sceClibPrintf("[TwitterDialogPlugin] Module Start!\n");

	for (void (**fn)(void) = __start__Zinit_array; fn < __stop__Zinit_array; ++fn) {
		if (*fn != NULL)
			(*fn)();
	}

	s_pluginInterface.OnBoot = TwitterPluginOnBoot;
	s_pluginInterface.OnLoad = TwitterPluginOnLoad;
	s_pluginInterface.OnStart = TwitterPluginOnStart;
	s_pluginInterface.OnStop = TwitterPluginOnStop;
	s_pluginInterface.OnUnload = TwitterPluginOnUnload;

	if (argp != NULL) {
		Module *mod = *(Module *const *)argp;

		if (mod != NULL)
			mod->SetInterface(1, &s_pluginInterface);
	}
	return 0;
}

extern "C" int module_stop(SceSize args, const void *argp)
{
	(void)args;
	(void)argp;
	sceClibPrintf("[TwitterDialogPlugin] Module Stop!\n");
	__cxa_finalize(__dso_handle);
	return 0;
}
