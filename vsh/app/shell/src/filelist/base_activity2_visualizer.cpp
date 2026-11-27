/*
 * base_activity2_visualizer.cpp: BaseActivityVisualizer, NpManagerGetCacheInfoJob,
 * activity icon/avatar rendering, URI scheme classification and launch, and global
 * Activity UI lifecycle/network-callback hooks (0x81348E70..0x8134A847).
 */
#include "filelist_internal.h"
#include <std/string.h>

namespace filelist {

namespace {

static const char kBaseActivityVisualizerTypeName[] = "base_activity2_visualizer";

/* Common resource hashes for BaseActivityVisualizer template & icon textures */
static const SceUInt32 kActivityTemplateHash = 0x4141BC2Cu;
static const SceUInt32 kActivityItemHash = 0x3D51B2DAu;

static const SceUInt32 kIconMaskTexHash = 0xA57491FAu;
static const SceUInt32 kWebIconTexHash = 0x5FF4EFE5u;
static const SceUInt32 kStoreIconTexHash = 0x87BF9852u;
static const SceUInt32 kGameIconTexHash = 0xE99F57A8u;
static const SceUInt32 kTrophyIconTexHash = 0x150646CCu;
static const SceUInt32 kMapIconTexHash = 0x0ECC71C1u;
static const SceUInt32 kProfileIconTexHash = 0xAA3E58D1u;
static const SceUInt32 kDefaultLinkIconTexHash = 0x3231F47Fu;
static const SceUInt32 kPanelMaskTexHash = 0x5A0B443Bu;

static const SceUInt32 kDefaultPanelBgColor = 0x808080FFu;
static const SceUInt32 kLaunchAppFlags = 0x20000u;

/* Typed layout of SceNpManager_E45E4003 output buffer (328 bytes, matches NpManagerAccountInfo) */
struct CachedNpAccountRecord {
	SceNpId npId;
	char jid[20];
	SceNpAvatarUrl avatarUrl;
	SceUInt32 panelBgColor;
	SceUInt32 panelType;
	SceUInt32 panelFlags;
	char panelUrl[128];
	SceUInt32 trailingFlags;
};

struct CachedNpProfile {
	CachedNpProfile();
	~CachedNpProfile();

	SceUInt32 panelBgColor;
	paf::string avatarUrl;
	paf::string panelUrl;
	paf::string jid;
};

class NpManagerGetCacheInfoJob : public paf::job::JobItem {
public:
	NpManagerGetCacheInfoJob()
		: paf::job::JobItem("NpManagerGetCacheInfoJob", NULL)
	{
		sce_paf_memset(&m_accountRecord, 0, sizeof(m_accountRecord));
	}

	virtual ~NpManagerGetCacheInfoJob()
	{
	}

	virtual int32_t Run()
	{
		return SceNpManager_E45E4003(
			reinterpret_cast<NpManagerAccountInfo *>(&m_accountRecord));
	}

	virtual void Finish(int32_t result);

	CachedNpAccountRecord m_accountRecord;
};

static CachedNpProfile *s_cachedNpProfile = NULL;
static paf::string s_emptyString;
static void (*s_netAcquireCb)(void) = NULL;
static void (*s_netReleaseCb)(void) = NULL;
static void (*s_netConnectCb)(void) = NULL;
static int s_dialogPriority = 0;

void NpManagerGetCacheInfoJob::Finish(int32_t result)
{
	if (result != 0 || s_cachedNpProfile == NULL)
		return;

	if (m_accountRecord.panelType != NP_MANAGER_PANEL_NONE) {
		paf::string avatarStr(m_accountRecord.avatarUrl.data,
				      sce_paf_strlen(m_accountRecord.avatarUrl.data));
		paf::string panelStr(m_accountRecord.panelUrl,
				     sce_paf_strlen(m_accountRecord.panelUrl));
		paf::string jidStr(m_accountRecord.jid,
				   sce_paf_strlen(m_accountRecord.jid));
		s_cachedNpProfile->panelBgColor = m_accountRecord.panelBgColor;
		s_cachedNpProfile->avatarUrl = avatarStr;
		s_cachedNpProfile->panelUrl = panelStr;
		s_cachedNpProfile->jid = jidStr;
	}
	activityUiNotifyCacheInfoUpdated();
}

static void EnqueueNpCacheInfoJobOnly()
{
	paf::common::SharedPtr<paf::job::JobItem> job(new NpManagerGetCacheInfoJob());
	paf::job::JobQueue *queue = activityUiGetNetworkJobQueue();
	queue->Enqueue(job);
}

/* 0x8134A0E6 */
CachedNpProfile::CachedNpProfile()
	: panelBgColor(kDefaultPanelBgColor),
	  avatarUrl(),
	  panelUrl(),
	  jid()
{
	EnqueueNpCacheInfoJobOnly();
}

/* 0x8134A1F0 */
CachedNpProfile::~CachedNpProfile()
{
}

static void InitActivityCacheImageOption(paf::cache::CacheImage::Option &opt)
{
	sce_paf_memset(&opt, 0, sizeof(opt));
	opt.auto_delete = true;
	opt.retry = true;
	opt.retry_count = 10;
	opt.reserved = true;
	opt.resize_type = paf::image::ImageResizeType_NearestNeighbor;
	opt.direct_load = false;
	opt.image_order_param = -1;
	opt.surface_flag = false;
}

static void SetWidgetCommonTexture(paf::ui::Widget *widget, SceUInt32 texHash, bool useDefaultSlot)
{
	paf::IDParam texId("", texHash);
	paf::intrusive_ptr<paf::graph::Surface> surf =
		paf::Framework::Instance()->GetCommonTexture(texId);
	if (useDefaultSlot)
		widget->SetTexture(surf);
	else
		widget->SetTexture(surf, 0, 0);
}

static void AttachCachedImage(paf::ui::Widget *widget, const paf::string &url,
			      paf::cache::CacheManager *cacheManager)
{
	paf::intrusive_ptr<paf::cache::CacheImageInterface> imageRef;
	paf::cache::CacheImage::Option opt;
	InitActivityCacheImageOption(opt);
	cacheManager->CreateCache(url, imageRef, &opt, NULL, NULL);
	widget->SetDrawObjMaskBit(&imageRef, 0, 0);
}

} /* anonymous namespace */

/* 0x81348E70 */
bool BaseActivityVisualizer::IsInherit(const char *type) const
{
	if (type == kBaseActivityVisualizerTypeName)
		return true;
	return Live2Visualizer::IsInherit(type);
}

/* 0x81348E88 */
const char *BaseActivityVisualizer::GetTypeName() const
{
	return kBaseActivityVisualizerTypeName;
}

const char *BaseActivityVisualizer::TypeName()
{
	return kBaseActivityVisualizerTypeName;
}

/* 0x81348E94 / 0x81349024 */
BaseActivityVisualizer::BaseActivityVisualizer(
	paf::ui::Widget *parent,
	paf::Plugin *plugin,
	paf::cache::CacheManager *cacheManager,
	const paf::common::SharedPtr<Live2Iterator> &iterator,
	SceInt64 cellId)
	: Live2Visualizer(parent, plugin,
			  paf::IDParam("", kActivityTemplateHash),
			  paf::IDParam("", kActivityItemHash),
			  iterator, cellId),
	  m_cacheManager(cacheManager),
	  m_displayMode(3),
	  m_category(-1)
{
}

BaseActivityVisualizer::~BaseActivityVisualizer()
{
}

/* 0x8135CCA8 */
void BaseActivityVisualizer::OnRecordCleared()
{
	RequestRecordUpdate();
}

/* 0x8134903E */
int BaseActivityVisualizer::CreateActivityImageCache(
	const paf::string &url,
	paf::intrusive_ptr<paf::cache::CacheImageInterface> &outRef,
	paf::cache::CacheManager *cacheManager)
{
	paf::cache::CacheImage::Option opt;
	InitActivityCacheImageOption(opt);
	return cacheManager->CreateCache(url, outRef, &opt, NULL, NULL);
}

/* 0x813490BE */
void BaseActivityVisualizer::ApplyActionIconForRecord(
	int category,
	const paf::intrusive_ptr<Live2Record> &record,
	paf::ui::Widget *iconWidget,
	paf::cache::CacheManager *cacheManager)
{
	paf::string actionUrl;
	paf::string iconUrl;
	if (record.get() != NULL && record->GetRecordKind() == LIVE2_RECORD_STATUS) {
		const Live2StatusRecord *statusRec =
			static_cast<const Live2StatusRecord *>(record.get());
		actionUrl = statusRec->m_status.actionUrl;
		iconUrl = statusRec->m_status.iconUrl;
	}

	if (iconWidget == NULL)
		return;

	if (category == ACTIVITY_URI_CATEGORY_EMPTY) {
		iconWidget->Hide(0xff, NULL, NULL, 0.0f);
		return;
	}

	iconWidget->Show(0xff, NULL, NULL, 0.0f);

	paf::graph::DrawObj *drawObj = iconWidget->GetDrawObj(0);
	if (drawObj != NULL) {
		drawObj->SetShader(*activityUiGetIconShader());
		paf::IDParam maskId("", kIconMaskTexHash);
		paf::intrusive_ptr<paf::graph::Surface> maskSurf =
			paf::Framework::Instance()->GetCommonTexture(maskId);
		drawObj->SetTexture(maskSurf, 1);
	}

	switch (category) {
	case ACTIVITY_URI_CATEGORY_WEB:
		SetWidgetCommonTexture(iconWidget, kWebIconTexHash, false);
		break;
	case ACTIVITY_URI_CATEGORY_STORE:
		SetWidgetCommonTexture(iconWidget, kStoreIconTexHash, false);
		AttachCachedImage(iconWidget, iconUrl, cacheManager);
		break;
	case ACTIVITY_URI_CATEGORY_GAME:
		SetWidgetCommonTexture(iconWidget, kGameIconTexHash, false);
		AttachCachedImage(iconWidget, iconUrl, cacheManager);
		break;
	case ACTIVITY_URI_CATEGORY_TROPHY:
		SetWidgetCommonTexture(iconWidget, kTrophyIconTexHash, false);
		AttachCachedImage(iconWidget, iconUrl, cacheManager);
		break;
	case ACTIVITY_URI_CATEGORY_MAP:
		SetWidgetCommonTexture(iconWidget, kMapIconTexHash, false);
		break;
	case ACTIVITY_URI_CATEGORY_PROFILE:
		SetWidgetCommonTexture(iconWidget, kProfileIconTexHash, false);
		AttachCachedImage(iconWidget, iconUrl, cacheManager);
		break;
	case ACTIVITY_URI_CATEGORY_OTHER:
		SetWidgetCommonTexture(iconWidget, kDefaultLinkIconTexHash, false);
		break;
	default: {
		paf::intrusive_ptr<paf::graph::Surface> emptySurf;
		iconWidget->SetTexture(emptySurf);
		iconWidget->Hide(0xff, NULL, NULL, 0.0f);
		break;
	}
	}
}

/* 0x81349904 */
void BaseActivityVisualizer::ApplyActionIcon(paf::ui::Widget *iconWidget)
{
	ApplyActionIconForRecord(m_category, m_record, iconWidget, m_cacheManager);
}

/* 0x8134994A */
const paf::string &BaseActivityVisualizer::GetRecordImageUrl(
	const paf::intrusive_ptr<Live2Record> &record,
	int urlKind)
{
	if (record.get() == NULL)
		return s_emptyString;

	if (record->GetRecordKind() == LIVE2_RECORD_STATUS) {
		const Live2StatusRecord *statusRec =
			static_cast<const Live2StatusRecord *>(record.get());
		if (urlKind == 3)
			return statusRec->m_status.iconUrl;
		if (urlKind == 2)
			return statusRec->m_status.announceAvatarUrl;
	} else if (record->GetRecordKind() == LIVE2_RECORD_COMMENT) {
		const Live2CommentRecord *commentRec =
			static_cast<const Live2CommentRecord *>(record.get());
		if (urlKind == 2)
			return commentRec->m_comment.announceAvatarUrl;
	}
	return s_emptyString;
}

/* 0x813499A2 */
SceUInt32 BaseActivityVisualizer::SwapColorEndian(SceUInt32 rgba, SceUInt32 *outAbgr)
{
	SceUInt32 swapped =
		((rgba >> 24) & 0xFFu) |
		((rgba >> 8) & 0xFF00u) |
		((rgba << 8) & 0xFF0000u) |
		((rgba << 24) & 0xFF000000u);
	*outAbgr = swapped;
	return swapped;
}

/* 0x813499C0 */
void BaseActivityVisualizer::ApplyAvatarIconForRecord(
	const paf::intrusive_ptr<Live2Record> &record,
	paf::ui::Widget *avatarWidget,
	paf::cache::CacheManager *cacheManager)
{
	if (record.get() == NULL)
		return;

	int ownerType = ACTIVITY_DB_OWNER_UNKNOWN;
	SceUInt32 panelBgColor = 0;

	if (record->GetRecordKind() == LIVE2_RECORD_STATUS) {
		const Live2StatusRecord *statusRec =
			static_cast<const Live2StatusRecord *>(record.get());
		ownerType = statusRec->m_status.ownerType;
		panelBgColor = statusRec->m_status.announcePanelBgColor;
	} else if (record->GetRecordKind() == LIVE2_RECORD_COMMENT) {
		const Live2CommentRecord *commentRec =
			static_cast<const Live2CommentRecord *>(record.get());
		ownerType = commentRec->m_comment.ownerType;
		panelBgColor = commentRec->m_comment.announcePanelBgColor;
	} else {
		return;
	}

	if (ownerType == ACTIVITY_DB_OWNER_USER || ownerType == ACTIVITY_DB_OWNER_SELF) {
		paf::graph::DrawObj *drawObj = avatarWidget->GetDrawObj(0);
		if (drawObj != NULL) {
			SceUInt32 abgrColor = 0;
			SwapColorEndian(panelBgColor, &abgrColor);
			drawObj->SetShader(*activityUiGetPanelShader());
			drawObj->m_color = abgrColor;

			paf::IDParam maskId("", kPanelMaskTexHash);
			paf::intrusive_ptr<paf::graph::Surface> maskSurf =
				paf::Framework::Instance()->GetCommonTexture(maskId);
			drawObj->SetTexture(maskSurf, 1);
		}
	}

	const paf::string &avatarUrl = GetRecordImageUrl(record, 2);
	paf::intrusive_ptr<paf::cache::CacheImageInterface> imageRef;
	paf::cache::CacheImage::Option opt;
	InitActivityCacheImageOption(opt);
	cacheManager->CreateCache(avatarUrl, imageRef, &opt, NULL, NULL);

	SetWidgetCommonTexture(avatarWidget, kProfileIconTexHash, true);
	avatarWidget->SetDrawObjMaskBit(&imageRef, 0, 0);
}

/* 0x81349BF0 */
void BaseActivityVisualizer::ApplyAvatarIcon(paf::ui::Widget *avatarWidget)
{
	ApplyAvatarIconForRecord(m_record, avatarWidget, m_cacheManager);
}

/* 0x81349C32 */
int BaseActivityVisualizer::GetCategory() const
{
	return m_category;
}

} /* namespace filelist */

class ActivityRecord;
class ActivityFeedItemBase;

int activityUiGetItemCategory(ActivityRecord **record)
{
	paf::intrusive_ptr<filelist::Live2Record> recRef(
		record != NULL ? reinterpret_cast<filelist::Live2Record *>(*record) : NULL);
	return filelist::BaseActivityVisualizer::ClassifyUriCategory(recRef);
}

int activityUiGetItemCategoryForItem(ActivityFeedItemBase *self)
{
	return reinterpret_cast<const filelist::BaseActivityVisualizer *>(self)->GetCategory();
}

int activityUiGetItemDisplayMode(ActivityFeedItemBase *self)
{
	return reinterpret_cast<const filelist::BaseActivityVisualizer *>(self)->GetDisplayMode();
}

namespace filelist {

/* 0x81349C36 */
void BaseActivityVisualizer::SetDisplayMode(int displayMode)
{
	m_displayMode = displayMode;
}

/* 0x81349C3A */
void BaseActivityVisualizer::LaunchProfileForRecord(
	const paf::intrusive_ptr<Live2Record> &record)
{
	if (record.get() == NULL)
		return;

	paf::string ownerId;
	if (record->GetRecordKind() == LIVE2_RECORD_STATUS) {
		ownerId = static_cast<const Live2StatusRecord *>(record.get())->m_status.ownerId;
	} else if (record->GetRecordKind() == LIVE2_RECORD_COMMENT) {
		ownerId = static_cast<const Live2CommentRecord *>(record.get())->m_comment.ownerId;
	} else {
		return;
	}

	paf::string launchParam("mode=profile&jid=");
	launchParam.append(ownerId.c_str(), ownerId.size());
	sceAppMgrLaunchAppByName(kLaunchAppFlags, "NPXS10006", launchParam.c_str());
}

/* 0x81349D96 */
void BaseActivityVisualizer::LaunchAuthorProfile()
{
	LaunchProfileForRecord(m_record);
}

bool BaseActivityVisualizer::IsUnread()
{
	return false;
}

/* 0x81349DE2 */
int BaseActivityVisualizer::OpenTemplate(paf::ui::Widget *parent,
					 const paf::IDParam &templateId)
{
	paf::Plugin::TemplateOpenParam openParam;
	return m_plugin->TemplateOpen(parent, templateId, openParam);
}

/* 0x81349E1C */
int BaseActivityVisualizer::ClassifyUriCategory(
	const paf::intrusive_ptr<Live2Record> &record)
{
	if (record.get() == NULL)
		return ACTIVITY_URI_CATEGORY_OTHER;

	paf::string uri;
	if (record->GetRecordKind() == LIVE2_RECORD_STATUS)
		uri = static_cast<const Live2StatusRecord *>(record.get())->m_status.actionUrl;

	if (paf::system::Model::GetType() == paf::system::Model::Type_Vita) {
		static const char *const kMapsPrefixes[] = {
			"http://maps.google.",
			"https://maps.google.",
			"http://g.co/maps/",
			"https://g.co/maps/",
			"http://goo.gl/maps/",
			"https://goo.gl/maps/",
			NULL
		};
		for (int i = 0; kMapsPrefixes[i] != NULL; ++i) {
			if (uri.find(kMapsPrefixes[i]) != paf::string::npos)
				return ACTIVITY_URI_CATEGORY_MAP;
		}
	}

	paf::string::size_type colonPos = uri.find(":");
	if (colonPos != paf::string::npos)
		uri = uri.substr(0, colonPos + 1);

	if (sce_paf_strcmp(uri.c_str(), "http:") == 0 ||
	    sce_paf_strcmp(uri.c_str(), "https:") == 0)
		return ACTIVITY_URI_CATEGORY_WEB;
	if (sce_paf_strcmp(uri.c_str(), "psns:") == 0 ||
	    sce_paf_strcmp(uri.c_str(), "psts:") == 0)
		return ACTIVITY_URI_CATEGORY_STORE;
	if (sce_paf_strcmp(uri.c_str(), "psgm:") == 0)
		return ACTIVITY_URI_CATEGORY_GAME;
	if (sce_paf_strcmp(uri.c_str(), "pstc:") == 0)
		return ACTIVITY_URI_CATEGORY_TROPHY;
	if (sce_paf_strcmp(uri.c_str(), "pspr:") == 0)
		return ACTIVITY_URI_CATEGORY_PROFILE;
	if (uri.size() != 0)
		return ACTIVITY_URI_CATEGORY_OTHER;
	return ACTIVITY_URI_CATEGORY_EMPTY;
}

/* 0x8134A0A0 */
void BaseActivityVisualizer::UpdateCategory()
{
	m_category = ClassifyUriCategory(m_record);
}

/* 0x8134A364 */
void BaseActivityVisualizer::LaunchActionUri(
	const paf::intrusive_ptr<Live2Record> &record,
	int category)
{
	const paf::string &uri =
		(record.get() != NULL && record->GetRecordKind() == LIVE2_RECORD_STATUS)
		? static_cast<const Live2StatusRecord *>(record.get())->m_status.actionUrl
		: s_emptyString;

	switch (category) {
	case ACTIVITY_URI_CATEGORY_WEB:
	case ACTIVITY_URI_CATEGORY_STORE:
	case ACTIVITY_URI_CATEGORY_GAME:
	case ACTIVITY_URI_CATEGORY_TROPHY:
	case ACTIVITY_URI_CATEGORY_MAP:
		sceAppMgrLaunchAppByUri(kLaunchAppFlags, uri.c_str());
		break;
	case ACTIVITY_URI_CATEGORY_PROFILE: {
		paf::string::size_type jidPos = uri.find("jid=");
		if (jidPos == paf::string::npos)
			return;
		paf::string jid = uri.substr(jidPos + sce_paf_strlen("jid="), paf::string::npos);
		paf::string launchParam("mode=profile&jid=");
		launchParam.append(jid.c_str(), jid.size());
		sceAppMgrLaunchAppByName(kLaunchAppFlags, "NPXS10006", launchParam.c_str());
		break;
	}
	default:
		break;
	}
}

/* 0x8134A57E */
int BaseActivityVisualizer::GetVoteCount(const paf::intrusive_ptr<Live2Record> &record)
{
	if (record.get() != NULL && record->GetRecordKind() == LIVE2_RECORD_STATUS)
		return static_cast<const Live2StatusRecord *>(record.get())->m_status.numOfVotes;
	return 0;
}

/* 0x8134A59E */
void BaseActivityVisualizer::TruncateAtChar(paf::string &str, char delimiter)
{
	char needle[2] = { delimiter, '\0' };
	paf::string::size_type pos = str.find(needle, 0, 1);
	if (pos != paf::string::npos)
		str.resize(pos);
}

/* 0x8134A638 */
void BaseActivityVisualizer::TruncateAtWideChar(paf::wstring &str, wchar_t delimiter)
{
	wchar_t needle[2] = { delimiter, L'\0' };
	paf::wstring::size_type pos = str.find(needle, 0, 1);
	if (pos != paf::wstring::npos)
		str.resize(pos);
}

/* 0x8134A6DC */
paf::wstring BaseActivityVisualizer::FormatActivityTime(SceUInt64 utcTick)
{
	paf::rtc::Tick tick = utcTick;
	return shellTickText(&tick, false);
}

/* 0x8134A692 */
SceUInt32 activityUiGetCachedPanelBgColor(void)
{
	if (s_cachedNpProfile != NULL)
		return s_cachedNpProfile->panelBgColor;
	return 0;
}

/* 0x8134A6A6 */
const paf::string &activityUiGetCachedAvatarUrl(void)
{
	if (s_cachedNpProfile != NULL)
		return s_cachedNpProfile->avatarUrl;
	return s_emptyString;
}

/* 0x8134A6C0 */
const paf::string &activityUiGetCachedPanelUrl(void)
{
	if (s_cachedNpProfile != NULL)
		return s_cachedNpProfile->panelUrl;
	return s_emptyString;
}

/* 0x8134A30A */
int activityUiInvokeNetAcquire(void)
{
	if (s_netAcquireCb != NULL)
		s_netAcquireCb();
	return 0;
}

/* 0x8134A320 */
int activityUiInvokeNetRelease(void)
{
	if (s_netReleaseCb != NULL)
		s_netReleaseCb();
	return 0;
}

/* 0x8134A336 */
int activityUiInvokeNetConnect(void)
{
	if (s_netConnectCb != NULL)
		s_netConnectCb();
	return 0;
}

/* 0x8134A358 */
int activityUiGetDialogPriority(void)
{
	return s_dialogPriority;
}

} /* namespace filelist */

/* 0x8134A248 */
void activityUiInit(void)
{
	activityUiDialogManagerInit();
	activityUiServiceInit();
	filelist::s_cachedNpProfile = new filelist::CachedNpProfile();
}

/* 0x8134A270 */
void activityUiTerm(void)
{
	delete filelist::s_cachedNpProfile;
	filelist::s_cachedNpProfile = NULL;
	activityUiServiceTerm();
	activityUiDialogManagerTerm();
}

/* 0x8134A2EA */
void activityUiSetNetCallbacks(void (*acquireFn)(void), void (*releaseFn)(void),
			       void (*connectFn)(void))
{
	filelist::s_netAcquireCb = acquireFn;
	filelist::s_netReleaseCb = releaseFn;
	filelist::s_netConnectCb = connectFn;
}

/* 0x8134A34C */
void activityUiSetDialogPriority(int priority)
{
	filelist::s_dialogPriority = priority;
}

/* 0x8134A716 */
void activityUiEnqueueCacheInfoJob(void)
{
	filelist::EnqueueNpCacheInfoJobOnly();
	SceActivityDb_738D4B49();
}
