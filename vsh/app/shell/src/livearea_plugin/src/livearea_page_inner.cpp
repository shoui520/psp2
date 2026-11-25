/*
 * livearea_page_inner.cpp: livearea_plugin content zone, gate & frame renderer
 * (0x811A364C-0x811ACF10, sti 0x811ACC24).
 */
#include <std/stdio.h>
#include <std/string.h>
#include <timer/timer.h>
#include <appmgr.h>
#include "livearea_internal.h"

extern "C" {
int sceAppMgrLaunchAppByNameForShell(unsigned int flags, const char *name, const char *param);
int sce_paf_rand(void);
bool SceLsdb_69B32F6C(int appAttr);
}

static paf::IDParam s_destroyContentZoneId("__destroy_content_zone__"); /* 0x815672A4 */
static paf::math::v4 s_gateDimColor(0.0f, 0.0f, 0.0f, 0.75f);	/* 0x815672B8 */
static paf::string s_appContentManager("content_manager");	/* 0x815672C8 */
static paf::string s_appMusic("music");				/* 0x815672D4 */
static paf::string s_appKids("kids");				/* 0x815672E0 */

static paf::IDParam s_topIconIds[9] = {
	paf::IDParam("PATCH"),
	paf::IDParam("UPGRADE"),
	paf::IDParam("SEARCH"),
	paf::IDParam("VOUCHERCODE"),
	paf::IDParam("ONLINE_HELP"),
	paf::IDParam("HELP"),
	paf::IDParam("RELOAD"),
	paf::IDParam("IMPORT_SAVEDATA"),
	paf::IDParam("ONLINE_STORAGE")
};

static paf::IDParam s_tmplContentZoneLayout("template_content_zone_layout"); /* 0x8156737C */
static paf::IDParam s_gateId("gate");				/* 0x8156738C */

static paf::IDParam s_frameIds[10] = {
	paf::IDParam("frame1"), paf::IDParam("frame2"), paf::IDParam("frame3"),
	paf::IDParam("frame4"), paf::IDParam("frame5"), paf::IDParam("frame6"),
	paf::IDParam("frame7"), paf::IDParam("frame8"), paf::IDParam("frame9"),
	paf::IDParam("frame10")
};

/* 0x811A364C */
static void liveareaSelectNextItem(int *curIdx, LiFrame *frameDesc, bool useNetworkTime)
{
	if (frameDesc == NULL) {
		*curIdx = -1;
		return;
	}
	int count = (int)frameDesc->m_items.size();
	if (count == 0) {
		*curIdx = -1;
		return;
	}
	int valid[5];
	int numValid = 0;
	for (int i = 0; i < count; ++i) {
		if (SceLsdb_628CFBE1(frameDesc->m_items[i], false, useNetworkTime) != 0) {
			valid[numValid++] = i;
			if (numValid >= 5)
				break;
		}
	}
	if (numValid <= 0) {
		*curIdx = -1;
		return;
	}
	if (frameDesc->m_multi == 0) {
		int picked = valid[0];
		for (int i = 0; i < numValid; ++i) {
			if (*curIdx < valid[i]) {
				picked = valid[i];
				break;
			}
		}
		*curIdx = picked;
	} else if (frameDesc->m_multi == 1) {
		if (numValid <= 1) {
			*curIdx = valid[0];
		} else {
			int candidates[5];
			int numCandidates = 0;
			for (int i = 0; i < numValid; ++i) {
				if (valid[i] != *curIdx)
					candidates[numCandidates++] = valid[i];
			}
			int chosen = (int)((float)numCandidates * (float)sce_paf_rand() / 32767.0f);
			if (chosen >= numCandidates)
				chosen = numCandidates - 1;
			*curIdx = (chosen < 0) ? candidates[0] : candidates[chosen];
		}
	}
	if (*curIdx >= count)
		*curIdx = count - 1;
	if (*curIdx < -1)
		*curIdx = -1;
}

/* 0x811A37F2 */
static bool liveareaTickFrameSwitch(LiveAreaFrameState *frameState, LiFrame *frameDesc,
				    bool useNetworkTime)
{
	if (frameState->pendingLoads > 0 || frameState->isSwitching)
		return false;
	LiLiveItem *curItem = SceLsdb_92D3A2EA(frameDesc, frameState->currentIndex);
	bool needSwitch = (curItem == NULL) ||
			  (SceLsdb_628CFBE1(curItem, false, useNetworkTime) == 0);
	unsigned int interval = (unsigned int)frameDesc->m_autoflip;
	if (interval != 0) {
		frameState->flipTimer += (1.0f / 60.0f);
		if (frameState->flipTimer >= (float)interval) {
			frameState->flipTimer = 0.0f;
			needSwitch = true;
		}
	}
	if (!needSwitch)
		return false;
	liveareaSelectNextItem(&frameState->currentIndex, frameDesc, useNetworkTime);
	return curItem != SceLsdb_92D3A2EA(frameDesc, frameState->currentIndex);
}

/* 0x811A38A6 */
static void liveareaTickTextScroll(LiveAreaFrameState *frameState)
{
	if (!frameState->scrollActive)
		return;
	frameState->scrollElapsed += (1000.0f / 60.0f);
	if (frameState->frameWidget == NULL)
		return;
	paf::IDParam textWidgetId((uint32_t)-635233871);
	paf::ui::Widget *child = frameState->frameWidget->FindChild(textWidgetId, 0);
	if (child == NULL)
		return;
	switch (frameState->scrollPhase) {
	case 0:
		if (frameState->scrollElapsed > 3000.0f) {
			frameState->scrollElapsed = 0.0f;
			frameState->scrollPhase = 2;
		}
		break;
	case 1:
		if (frameState->scrollElapsed > 1000.0f) {
			frameState->scrollElapsed = 0.0f;
			frameState->scrollPhase = 2;
		}
		break;
	case 2:
		if (frameState->scrollElapsed >= frameState->scrollDuration) {
			frameState->scrollElapsed = 0.0f;
			frameState->scrollPhase = 3;
		}
		break;
	case 3:
		if (frameState->scrollElapsed > 1000.0f) {
			frameState->scrollElapsed = 0.0f;
			frameState->scrollPhase = 4;
		}
		break;
	case 4:
		if (frameState->scrollElapsed >= 1000.0f) {
			frameState->scrollElapsed = 0.0f;
			frameState->scrollPhase = 1;
		}
		break;
	}
}

/* 0x811A3C72 */
LiveAreaPageInner::LiveAreaPageInner(LiveAreaPage *page, const char *dir,
				     const LiveAreaPageInitInfo *info)
	: m_page(page),
	  m_style(info->style),
	  m_manualMode(info->manualMode),
	  m_upgradeMode(info->upgradeMode),
	  m_gateIconCache(info->gateIconCache),
	  m_title(info->title),
	  m_searchKey(info->searchKey),
	  m_url(info->url),
	  m_appAttr(info->appAttr),
	  m_kind(info->kind),
	  m_hasInfo(info->hasInfo),
	  m_hasExt(info->hasExt),
	  m_hasFeed(info->hasFeed),
	  m_hasOnlineManual(info->hasOnlineManual),
	  m_rcoRefCount(0),
	  m_reloadFlags(0),
	  m_reloadDir(),
	  m_baseDir(dir),
	  m_xmlPath(),
	  m_helpUri(),
	  m_initPhase(2),
	  m_xmlDoc(),
	  m_gateCacheRef(NULL),
	  m_gateSurface(NULL),
	  m_contentZone(NULL),
	  m_gateWidget(NULL),
	  m_gateBtn(NULL),
	  m_gateHasBg(false),
	  m_reloadCount(0),
	  m_reloadError(0),
	  m_xmlJob(),
	  m_pendingInitialLoads(0),
	  m_shown(false),
	  m_introDone(false),
	  m_scrolledDown(false),
	  m_hasLiveSurface(false),
	  m_initialLoadNotified(false),
	  m_templateOpened(false),
	  m_gateResetDone(false),
	  m_dirty(false),
	  m_frames()
{
	m_baseDir += "/";
	m_xmlPath = paf::string(dir) + paf::string("/template.xml");
}

/* 0x811A3F4E */
LiveAreaPageInner::~LiveAreaPageInner()
{
	if (m_xmlJob.get() != NULL)
		m_xmlJob->Cancel();
	while (m_rcoRefCount > 0) {
		paf::string tmplName(m_page->m_templateName);
		liveareaUnloadRco(&tmplName);
		--m_rcoRefCount;
	}
	while (m_frames.size() != 0) {
		LiveAreaFrameState *frameState = m_frames.front();
		m_frames.pop_front();
		delete frameState;
	}
	if (m_gateSurface != NULL)
		m_gateSurface->Release();
	if (m_gateCacheRef != NULL)
		m_gateCacheRef->Release();
}

/* 0x811A417E */
void LiveAreaPageInner::Init()
{
	paf::ui::FileList *fileList = m_page->m_fileList;
	paf::ui::filelist::SegmentSource *segSrc = new paf::ui::filelist::SegmentSource();
	segSrc->AddRef();
	paf::common::SharedPtr<paf::ui::filelist::Visualizer> bgVis(liveareaCreateBgFactory());
	paf::common::RefObj *cellSrc = activityUiCreateSingleCellSource();
	if (cellSrc != NULL)
		cellSrc->AddRef();

	paf::ui::filelist::SegmentListAccessor::OptionParam option;
	sce_paf_memset(&option, 0, sizeof(option));
	option.m_header_disappearance = true;
	activityUiSegmentSetCell(segSrc, 0, &cellSrc, &option);
	fileList->SetSegmentLayoutType(0, paf::ui::ListView::LAYOUT_TYPE_LIST);

	if (cellSrc != NULL)
		cellSrc->Release();
	segSrc->Release();

	paf::common::SharedPtr<paf::job::JobItem> job;
	liveareaEnqueueContentParserJob(&job, this, m_xmlPath.c_str(), NULL, 0, 0, -1, 1);
	m_xmlJob = job;
}

/* 0x811A44CA */
int LiveAreaPageInner::TryReloadXml()
{
	if (m_reloadFlags == 0)
		return 1;
	if (m_xmlJob.get() != NULL || m_initPhase != 0 || !m_shown)
		return 0;
	if (!m_baseDir.empty() && !m_reloadDir.empty()) {
		if (sce_paf_strncmp(m_baseDir.c_str(), "vs0:", 4) == 0 &&
		    sce_paf_strncmp(m_reloadDir.c_str(), "ur0:", 4) == 0)
			m_reloadFlags = -1;
	}
	paf::common::SharedPtr<paf::job::JobItem> job;
	liveareaEnqueueContentParserJob(&job, this, m_xmlPath.c_str(), m_reloadDir.c_str(),
					0, 1, m_reloadFlags, 0);
	m_xmlJob = job;
	if (m_xmlJob.get() == NULL)
		return 0;
	m_reloadFlags = 0;
	m_reloadDir = "";
	return 1;
}

/* 0x811A4684 */
void LiveAreaPageInner::AnimatePopIn(paf::ui::Widget *widget, void *callback)
{
	ScePafWidget_4588699E(widget, NULL, 2, 0.0f, NULL);
	paf::Timer *fadeTimer = new paf::Timer();
	fadeTimer->InitTimer(21.0f, 0, -1, 0, NULL);
	ScePafWidget_FF5BC957(widget, fadeTimer, 2, 0.0f, NULL);

	paf::math::v4 startScale(0.0f, 0.0f, 0.0f, 0.0f);
	widget->SetScale(startScale, NULL, 65541, 0, 0.0f, NULL, NULL);

	paf::math::v4 endScale(1.0f, 1.0f, 0.0f, 0.0f);
	paf::Timer *scaleTimer = new paf::Timer();
	scaleTimer->InitTimer(32.0f, 0, -1, 0, NULL);
	widget->SetScale(endScale, scaleTimer, 65541, 2, 0.0f, (paf::ui::AnimCB)callback, this);
}

/* 0x811A58D4 */
void LiveAreaPageInner::OnInitialImagesLoaded()
{
	m_shown = true;
	m_page->UpdateLastLaunchTime();
	m_page->EnsureFeed();
	liveareaMgrUpdatePage(this, 0);
}

/* 0x811A985E */
void LiveAreaPageInner::BuildLayout(int flags)
{
	(void)flags;
	TryReloadXml();
}

/* 0x811A9CCC */
void LiveAreaPageInner::OnRcoLoaded()
{
	BuildLayout(-1);
}

/* 0x811AA854 */
void LiveAreaPageInner::SetGateSurface(paf::graph::Surface **surface)
{
	m_hasLiveSurface = true;
	paf::graph::Surface *nextSurface = *surface;
	if (nextSurface != m_gateSurface) {
		if (m_gateSurface != NULL)
			m_gateSurface->Release();
		m_gateSurface = nextSurface;
		if (nextSurface != NULL)
			nextSurface->AddRef();
	}
	if (m_gateBtn != NULL) {
		paf::intrusive_ptr<paf::graph::Surface> surfRef(nextSurface);
		m_gateBtn->SetTexture(surfRef, 0, 0);
	}
}

/* 0x811AA9F2 */
void LiveAreaPageInner::ClearGateSurface()
{
	m_hasLiveSurface = false;
	if (m_gateSurface != NULL) {
		m_gateSurface->Release();
		m_gateSurface = NULL;
	}
}

/* 0x811AAB9E */
void LiveAreaPageInner::LoadDefaultXml()
{
	paf::common::SharedPtr<paf::job::JobItem> job;
	liveareaEnqueueContentParserJob(&job, this,
		"vs0:data/internal/livearea/default/sce_sys/livearea/contents/template.xml",
		"vs0:data/internal/livearea/default/sce_sys/livearea/contents/",
		1, 0, -1, 0);
	m_xmlJob = job;
}

/* 0x811AACC2 */
int LiveAreaPageInner::GetFrameTimestamp(const char *frameId, SceUInt64 *outTimestamp)
{
	LiTemplate *doc = m_xmlDoc.get();
	if (doc == NULL || m_reloadFlags != 0 || m_xmlJob.get() != NULL)
		return (int)0x80000002;
	if (frameId == NULL) {
		*outTimestamp = (SceUInt64)doc->m_contentRev;
		return 0;
	}
	return (int)0x80000001;
}

/* 0x811AAD60 */
int LiveAreaPageInner::QueueReload(int flags, const char *dir)
{
	if ((flags & 0x110000) != 0)
		flags = -1;
	m_reloadFlags |= flags;
	if (dir != NULL) {
		m_reloadDir = dir;
		m_reloadDir += "/";
	}
	TryReloadXml();
	return 1;
}

/* 0x811AAE02 */
int LiveAreaPageInner::QueueReloadFromReq(const LiveAreaReloadRequest *req)
{
	return QueueReload(req->reloadMask, req->contentDir);
}

/* 0x811AAE14 */
int LiveAreaPageInner::GetGateNormPos(float *outX, float *outY)
{
	if (m_gateBtn == NULL) {
		*outX = 0.0f;
		*outY = 0.0f;
		return 0;
	}
	const paf::math::matrix &worldMat = m_gateBtn->GetWorldMatrix();
	*outX = worldMat.w().extract_x() / 480.0f;
	*outY = worldMat.w().extract_y() / 272.0f;
	return 1;
}

/* 0x811AAE7C */
int LiveAreaPageInner::SwitchFrame(LiFrame *frameDesc, LiveAreaFrameState *frameState)
{
	paf::IDParam frameId(frameDesc->m_id);
	paf::ui::Widget *child = m_contentZone->FindChild(frameId, 0);
	if (child == NULL)
		return 0;
	frameState->isSwitching = true;
	return 1;
}

/* 0x811AAFB8 */
void LiveAreaPageInner::ResetGateFocus()
{
	if (m_gateBtn != NULL) {
		paf::IDParam focusChildId((uint32_t)-147058763);
		paf::ui::Widget *child = m_gateBtn->FindChild(focusChildId, 0);
		if (child != NULL)
			child->SetFocus(NULL);
	}
}

/* 0x811AB056 */
void LiveAreaPageInner::Tick()
{
	if (m_xmlDoc.get() == NULL || m_initPhase > 0 || !m_shown)
		return;
	if (m_gateBtn != NULL && !m_gateResetDone &&
	    !m_gateBtn->m_transition_in && !m_gateBtn->m_transition_out) {
		m_gateResetDone = true;
		ResetGateFocus();
	}
	bool useNetworkTime = SceLsdb_69B32F6C(m_appAttr);
	(void)useNetworkTime;
	for (paf::list<LiveAreaFrameState *>::iterator it = m_frames.begin();
	     it != m_frames.end(); ++it) {
		liveareaTickTextScroll(*it);
	}
}

/* 0x811AB2DE */
void LiveAreaPageInner::OnScroll()
{
	paf::ui::Widget *body = m_page->m_fileList->GetInternalWidget();
	paf::math::v4 pos = body->GetPos(0, NULL, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	if (pos.extract_y() <= 264.0f) {
		if (m_scrolledDown) {
			m_scrolledDown = false;
			g_liveareaMgr->m_topmenuIf->setPageScrolledDown(m_page->m_pageKey, 0);
		}
	} else if (!m_scrolledDown) {
		m_scrolledDown = true;
		g_liveareaMgr->m_topmenuIf->setPageScrolledDown(m_page->m_pageKey, 1);
	}
}

/* 0x811AB38A */
void LiveAreaPageInner::StartReloadSpin()
{
	if (m_reloadCount == 0 && m_contentZone != NULL) {
		paf::IDParam barId(1728383896U);
		paf::ui::Widget *bar = m_contentZone->FindChild(barId, 0);
		if (bar != NULL) {
			paf::ui::Widget *btn = bar->FindChild(s_topIconIds[6], 0);
			if (btn != NULL) {
				paf::IDParam iconId((uint32_t)-1039764635);
				paf::ui::Widget *icon = btn->FindChild(iconId, 0);
				if (icon != NULL) {
					icon->KillAnimType(65542);
					paf::math::v4 rot(0.0f, 0.0f, -6.2831855f, 0.0f);
					paf::Timer *spinTimer = new paf::Timer();
					spinTimer->InitTimer(22.0f, 0, -1, 0, NULL);
					icon->SetRot(rot, 0, spinTimer, 65542, 0, 0.0f, NULL, NULL);
					paf::Timer *loopTimer = new paf::Timer();
					loopTimer->InitTimer(0.0f, 0, -1, 0, NULL);
					icon->SetRot(rot, 0, loopTimer, 268500998, 0, 0.0f, NULL, NULL);
				}
			}
		}
	}
	++m_reloadCount;
}

/* 0x811AB5DA */
void LiveAreaPageInner::StopReloadSpin(int error)
{
	--m_reloadCount;
	if (m_reloadError == 0 && error < 0)
		m_reloadError = error;
	if (m_reloadCount == 0) {
		if (m_contentZone != NULL) {
			paf::IDParam barId(1728383896U);
			paf::ui::Widget *bar = m_contentZone->FindChild(barId, 0);
			if (bar != NULL) {
				paf::ui::Widget *btn = bar->FindChild(s_topIconIds[6], 0);
				if (btn != NULL) {
					paf::IDParam iconId((uint32_t)-1039764635);
					paf::ui::Widget *icon = btn->FindChild(iconId, 0);
					if (icon != NULL) {
						icon->KillAnimType(65542);
						paf::math::v4 rot(0.0f, 0.0f, -6.2831855f, 0.0f);
						paf::Timer *stopTimer = new paf::Timer();
						stopTimer->InitTimer(0.0f, 0, -1, 0, NULL);
						icon->SetRot(rot, 0, stopTimer, 65542, 0, 0.0f, NULL, NULL);
					}
				}
			}
		}
		if (m_reloadError != 0) {
			if (g_liveareaMgr->m_topmenuIf->isTopmenuForeground() != 0) {
				CommonGuiMessageDialogParam dlg;
				dlg.priority = 33;
				dlg.type = m_reloadError;
				SceCommonGuiDialog_20B141F1(&dlg);
			}
		}
		m_reloadError = 0;
	}
}

/* 0x811AB8F2 */
int LiveAreaPageInner::GetReloadCount()
{
	return m_reloadCount;
}

/* 0x811AB8F8 */
int LiveAreaPageInner::IsIntroTimerDone()
{
	return m_introDone ? 1 : 0;
}

/* 0x811AB944 */
void LiveAreaPageInner::ClearGateInitFlag()
{
	m_gateResetDone = false;
}

/* 0x811AB94C */
void LiveAreaPageInner::SetDirtyFlag()
{
	m_dirty = true;
}

/* 0x811AB954 */
void LiveAreaPageInner::UpdateInitInfo(const LiveAreaPageInitInfo *info)
{
	m_style = info->style;
	m_manualMode = info->manualMode;
	m_upgradeMode = info->upgradeMode;
	m_gateIconCache = info->gateIconCache;
	m_title = info->title;
	m_searchKey = info->searchKey;
	m_url = info->url;
	m_appAttr = info->appAttr;
	m_kind = info->kind;
	m_hasInfo = info->hasInfo;
	m_hasExt = info->hasExt;
	m_hasFeed = info->hasFeed;
	m_hasOnlineManual = info->hasOnlineManual;
}

/* 0x811AB9B8 */
int LiveAreaPageInner::GetResetMask()
{
	if (m_xmlDoc.get() == NULL)
		return 0;
	return 0;
}

/* 0x811ABA06 */
int LiveAreaPageInner::HasPatchIcon()
{
	if (m_contentZone == NULL)
		return 0;
	paf::IDParam barId(1728383896U);
	paf::ui::Widget *bar = m_contentZone->FindChild(barId, 0);
	if (bar != NULL && bar->FindChild(s_topIconIds[0], 0) != NULL)
		return 1;
	return 0;
}

/* 0x811ABE1C */
static void liveareaOnPatchClick(int eventId, paf::ui::Handler *handler,
				 paf::ui::Event *event, LiveAreaPageInner *self)
{
	(void)eventId; (void)handler; (void)event;
	g_liveareaMgr->m_topmenuIf->onPatchIconClick(self->m_page->m_pageKey);
}

/* 0x811ABE34 */
static void liveareaOnSearchClick(int eventId, paf::ui::Handler *handler,
				  paf::ui::Event *event, LiveAreaPageInner *self)
{
	(void)eventId; (void)handler; (void)event;
	if (g_liveareaMgr->m_topmenuIf->isPageInteractive(self->m_page->m_pageKey) != 0) {
		char uri[1024];
		sce_paf_snprintf(uri, sizeof(uri), "%s%s", "search:", self->m_searchKey.c_str());
		sceAppMgrLaunchAppByUri(0x20000, uri);
	}
}

/* 0x811ABEA4 */
static void liveareaOnOnlineHelpClick(int eventId, paf::ui::Handler *handler,
				      paf::ui::Event *event, LiveAreaPageInner *self)
{
	(void)eventId; (void)handler; (void)event;
	if (g_liveareaMgr->m_topmenuIf->isPageInteractive(self->m_page->m_pageKey) != 0)
		sceAppMgrLaunchAppByUri(0x20000, self->m_helpUri.c_str());
}

/* 0x811ABECE */
static void liveareaOnHelpClick(int eventId, paf::ui::Handler *handler,
				paf::ui::Event *event, LiveAreaPageInner *self)
{
	(void)eventId; (void)handler; (void)event;
	g_liveareaMgr->m_topmenuIf->onManualIconClick(self->m_page->m_pageKey);
}

/* 0x811ABEE6 */
static void liveareaOnVoucherClick(int eventId, paf::ui::Handler *handler,
				   paf::ui::Event *event, LiveAreaPageInner *self)
{
	(void)eventId; (void)handler; (void)event;
	if (g_liveareaMgr->m_topmenuIf->isPageInteractive(self->m_page->m_pageKey) != 0)
		sceAppMgrLaunchAppByUri(0x20000, "psns:redeem");
}

/* 0x811ABF14 */
static void liveareaOnReloadClick(int eventId, paf::ui::Handler *handler,
				  paf::ui::Event *event, LiveAreaPageInner *self)
{
	(void)eventId; (void)handler; (void)event;
	if (self->m_reloadCount <= 0 && self->m_xmlDoc.get() != NULL)
		liveareaMgrUpdatePage(self, 1);
}

/* 0x811ABF2E */
static void liveareaOnImportSaveClick(int eventId, paf::ui::Handler *handler,
				      paf::ui::Event *event, LiveAreaPageInner *self)
{
	(void)eventId; (void)handler; (void)event;
	g_liveareaMgr->m_topmenuIf->onImportSavedataIconClick(self->m_page->m_pageKey);
}

/* 0x811ABF46 */
static void liveareaOnOnlineStorageClick(int eventId, paf::ui::Handler *handler,
					 paf::ui::Event *event, LiveAreaPageInner *self)
{
	(void)eventId; (void)handler; (void)event;
	g_liveareaMgr->m_topmenuIf->onOnlineStorageIconClick(self->m_page->m_pageKey);
}

/* 0x811ABF5E */
static void liveareaOnItemUriClick(int eventId, paf::ui::Handler *handler,
				   paf::ui::Event *event, LiLiveItem *item)
{
	(void)eventId; (void)handler; (void)event;
	if (item != NULL) {
		LiTarget *target = item->m_target;
		if (target != NULL && !target->m_uri.empty()) {
			paf::string uri;
			liveareaReplaceOnlineId(uri, target->m_uri);
			sceAppMgrLaunchAppByUri(0x20000, uri.c_str());
		}
	}
}

/* 0x811ABFB8 */
static void liveareaOnItemTrackedUriClick(int eventId, paf::ui::Handler *handler,
					  paf::ui::Event *event, LiLiveItem *item)
{
	(void)eventId; (void)handler; (void)event;
	if (item != NULL) {
		LiTarget *target = item->m_target;
		if (target != NULL && !target->m_uri.empty()) {
			paf::string uri;
			liveareaReplaceOnlineId(uri, target->m_uri);
			const char *uriStr = uri.c_str();
			if (uriStr != NULL) {
				if (sce_paf_strncmp("http:", uriStr, 5) == 0 ||
				    sce_paf_strncmp("https:", uriStr, 5) == 0 ||
				    sce_paf_strncmp("webmodal:", uriStr, 9) == 0)
					shellRecordMetric(228, 1);
				else if (sce_paf_strncmp("psns:", uriStr, 5) == 0 ||
					 sce_paf_strncmp("psts:", uriStr, 5) == 0)
					shellRecordMetric(227, 1);
				else if (sce_paf_strncmp("psgm:", uriStr, 5) == 0)
					shellRecordMetric(229, 1);
			}
			sceAppMgrLaunchAppByUri(0x20000, uriStr);
		}
	}
}

/* 0x811AC0F4 */
static void liveareaOnItemAppClick(int eventId, paf::ui::Widget *widget,
				   paf::ui::Event *event, LiLiveItem *item)
{
	(void)eventId; (void)event;
	if (item != NULL) {
		LiTarget *target = item->m_target;
		if (target != NULL && !target->m_uri.empty()) {
			paf::string launchParam;
			const char *appName = (const char *)widget->m_user_data;
			liveareaReplaceOnlineId(launchParam, target->m_uri);
			sceAppMgrLaunchAppByNameForShell(0x20000, appName, launchParam.c_str());
		}
	}
}

/* 0x811AC156 */
static void liveareaOnItemPslaClick(int eventId, paf::ui::Widget *widget,
				    paf::ui::Event *event, LiLiveItem *item)
{
	(void)eventId; (void)event;
	if (item != NULL) {
		LiTarget *target = item->m_target;
		if (target != NULL && !target->m_uri.empty()) {
			const char *appName = (const char *)widget->m_user_data;
			paf::string launchParam(target->m_uri);
			size_t pos = launchParam.find("psla:");
			if (pos != paf::string::npos)
				launchParam.erase(pos, 5);
			sceAppMgrLaunchAppByNameForShell(0x20000, appName, launchParam.c_str());
		}
	}
}

/* 0x811AC23E */
static void liveareaOnGateClick(int eventId, paf::ui::Handler *handler,
				paf::ui::Event *event, int pageKey)
{
	(void)eventId; (void)handler; (void)event;
	g_liveareaMgr->m_topmenuIf->onGateClick(pageKey);
}

/* 0x811AC8B6 */
static void liveareaOnIntroTimer(int eventId, paf::ui::Handler *handler,
				 paf::ui::Event *event, LiveAreaPageInner *self)
{
	(void)eventId; (void)event;
	self->m_introDone = true;
	handler->KillTimer(16777507);
	g_liveareaMgr->m_topmenuIf->notifyIntroComplete(self->m_page->m_pageKey);
	self->ResetGateFocus();
}

/* 0x811AC8EC */
static void liveareaOnFocusTimer(int eventId, paf::ui::Handler *handler,
				 paf::ui::Event *event, LiveAreaPageInner *self)
{
	(void)eventId; (void)event;
	self->m_page->OnFocusComplete();
	handler->KillTimer(16777509);
}

/* 0x811AC906 */
static void liveareaOnBgClearTimer(int eventId, paf::ui::Handler *handler,
				   paf::ui::Event *event, LiveAreaPageInner *self)
{
	(void)eventId; (void)event;
	if (self->m_initPhase > 0) {
		--self->m_initPhase;
		if (self->m_initPhase == 0)
			self->BuildLayout(-1);
	}
	handler->KillTimer(16777508);
}

/* 0x811ACBAE */
static bool liveareaCompareUint(const unsigned int *lhs, const unsigned int *rhs)
{
	return *lhs < *rhs;
}
