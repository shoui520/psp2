/*
 * livearea_page_inner.cpp: livearea_plugin content zone, gate & frame renderer
 * (0x811A364C-0x811ACF10, sti 0x811ACC24).
 */
#include <std/stdio.h>
#include <std/string.h>
#include <std/algorithm>
#include <common/char_encoding.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <timer/timer.h>
#include <appmgr.h>
#include <widget/w_composite_drawer.h>
#include "livearea_internal.h"

namespace filelist {
paf::ui::filelist::SegmentSource *createBasicSegmentSource();
}

extern "C" {
int sceAppMgrLaunchAppByNameForShell(unsigned int flags, const char *name, const char *param);
int sce_paf_rand(void);
bool SceLsdb_69B32F6C(int appAttr);
const char *SceActivityDb_E6C23731(void);
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

static const int s_topFuncsGame[3] = { 2, 6, 8 };		/* 0x81511558 */
static const int s_topFuncsMedia[2] = { 2, 6 };			/* 0x81511564 */
static const int s_topFuncsWeb[1] = { 2 };			/* 0x81502010 */

struct LiveAreaRectBounds {
	float minX;
	float minY;
	float maxX;
	float maxY;
};

static void liveareaOnPatchClick(int eventId, paf::ui::Handler *handler,
				 paf::ui::Event *event, void *userdata);
static void liveareaOnSearchClick(int eventId, paf::ui::Handler *handler,
				  paf::ui::Event *event, void *userdata);
static void liveareaOnOnlineHelpClick(int eventId, paf::ui::Handler *handler,
				      paf::ui::Event *event, void *userdata);
static void liveareaOnHelpClick(int eventId, paf::ui::Handler *handler,
				paf::ui::Event *event, void *userdata);
static void liveareaOnVoucherClick(int eventId, paf::ui::Handler *handler,
				   paf::ui::Event *event, void *userdata);
static void liveareaOnReloadClick(int eventId, paf::ui::Handler *handler,
				  paf::ui::Event *event, void *userdata);
static void liveareaOnImportSaveClick(int eventId, paf::ui::Handler *handler,
				      paf::ui::Event *event, void *userdata);
static void liveareaOnOnlineStorageClick(int eventId, paf::ui::Handler *handler,
					 paf::ui::Event *event, void *userdata);
static void liveareaOnItemUriClick(int eventId, paf::ui::Handler *handler,
				   paf::ui::Event *event, void *userdata);
static void liveareaOnItemTrackedUriClick(int eventId, paf::ui::Handler *handler,
					  paf::ui::Event *event, void *userdata);
static void liveareaOnItemAppClick(int eventId, paf::ui::Handler *handler,
				   paf::ui::Event *event, void *userdata);
static void liveareaOnItemPslaClick(int eventId, paf::ui::Handler *handler,
				    paf::ui::Event *event, void *userdata);
static void liveareaOnGateClick(int eventId, paf::ui::Handler *handler,
				paf::ui::Event *event, void *userdata);
static void liveareaOnItemPress(int eventId, paf::ui::Handler *handler,
				paf::ui::Event *event, void *userdata);
static void liveareaOnItemRelease(int eventId, paf::ui::Handler *handler,
				  paf::ui::Event *event, void *userdata);
static void liveareaOnGateImageLoad(int eventId, paf::ui::Handler *handler,
				    paf::ui::Event *event, void *userdata);
static void liveareaOnFrameImageLoad(int eventId, paf::ui::Handler *handler,
				     paf::ui::Event *event, void *userdata);
static void liveareaOnItemTextInit(int eventId, paf::ui::Handler *handler,
				   paf::ui::Event *event, void *userdata);
static void liveareaOnBgImageLoad(int eventId, paf::ui::Handler *handler,
				  paf::ui::Event *event, void *userdata);
static void liveareaOnDefaultBgImageLoad(int eventId, paf::ui::Handler *handler,
					 paf::ui::Event *event, void *userdata);
static void liveareaOnGateTextLayout(int eventId, paf::ui::Handler *handler,
				     paf::ui::Event *event, void *userdata);
static void liveareaOnItemTextLayout(int eventId, paf::ui::Handler *handler,
				     paf::ui::Event *event, void *userdata);
static void liveareaOnIntroTimer(int eventId, paf::ui::Handler *handler,
				 paf::ui::Event *event, void *userdata);
static void liveareaOnFocusTimer(int eventId, paf::ui::Handler *handler,
				 paf::ui::Event *event, void *userdata);
static void liveareaOnBgClearTimer(int eventId, paf::ui::Handler *handler,
				   paf::ui::Event *event, void *userdata);
static void liveareaOnSwitchFrameOut(paf::Timer *timer, paf::ui::AnimListener *listener,
				     void *widgetPtr, void *userdata, void *unused);
static void liveareaOnReloadFrameOut(paf::Timer *timer, paf::ui::AnimListener *listener,
				     void *widgetPtr, void *userdata, void *unused);
static void liveareaOnDestroyWidgetTimer(int eventId, paf::ui::Handler *handler,
					 paf::ui::Event *event, void *userdata);
static bool liveareaCompareFunctions(const LiFunction &lhs, const LiFunction &rhs);

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
		frameState->flipTimer += (paf::Timer::s_tick_unit * paf::Timer::s_tick_interval);
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

	frameState->scrollElapsed +=
		(paf::Timer::s_tick_unit * paf::Timer::s_tick_interval) * 1000.0f;
	if (frameState->frameWidget == NULL)
		return;

	paf::IDParam drawerId((uint32_t)-635233871);
	paf::ui::CompositeDrawer *drawer =
		(paf::ui::CompositeDrawer *)frameState->frameWidget->FindChild(drawerId, 0);
	if (drawer == NULL)
		return;

	paf::graph::TextObj *textObj =
		(paf::graph::TextObj *)drawer->GetDrawObjByIndex(frameState->textObjIndex);

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
	case 2: {
		float progress = frameState->scrollElapsed / frameState->scrollDuration;
		if (progress < 1.0f) {
			paf::math::v2 visiblePos(frameState->scrollDeltaX * progress,
						 frameState->scrollDeltaY * progress);
			textObj->SetLayoutAttribute(paf::graph::TextLayoutAttribute_VisiblePos,
						    visiblePos);
		} else {
			paf::math::v4 color = textObj->GetColor();
			textObj->SetColor(paf::math::v4(color.extract_x(), color.extract_y(),
							color.extract_z(), 0.0f));
			paf::math::v2 zeroPos(0.0f, 0.0f);
			textObj->SetLayoutAttribute(paf::graph::TextLayoutAttribute_VisiblePos,
						    zeroPos);
			frameState->scrollElapsed = 0.0f;
			frameState->scrollPhase = 3;
		}
		break;
	}
	case 3:
		if (frameState->scrollElapsed > 1000.0f) {
			frameState->scrollElapsed = 0.0f;
			frameState->scrollPhase = 4;
		}
		break;
	case 4: {
		float alpha = frameState->scrollElapsed / 1000.0f;
		paf::math::v4 color = textObj->GetColor();
		if (alpha < 1.0f) {
			textObj->SetColor(paf::math::v4(color.extract_x(), color.extract_y(),
							color.extract_z(), alpha));
		} else {
			textObj->SetColor(paf::math::v4(color.extract_x(), color.extract_y(),
							color.extract_z(), 1.0f));
			frameState->scrollElapsed = 0.0f;
			frameState->scrollPhase = 1;
		}
		break;
	}
	}
}

/* 0x811A3B26 */
static void liveareaInitTextScroll(LiveAreaFrameState *frameState, int scrollMode,
				   paf::graph::TextObj *textObj)
{
	frameState->scrollMode = scrollMode;
	frameState->scrollPhase = 0;
	frameState->scrollDeltaX = 0.0f;
	frameState->scrollDeltaY = 0.0f;
	frameState->scrollElapsed = 0.0f;
	frameState->scrollDuration = 0.0f;

	if (scrollMode == 0)
		return;

	textObj->SetLayoutAttribute(paf::graph::TextLayoutAttribute_Ellipsis, false);

	paf::math::v2 bounds(0.0f, 0.0f);
	textObj->GetBounds(bounds);

	float extent = 0.0f;
	if (frameState->scrollMode == 1) {
		if (bounds.extract_x() > textObj->m_size.extract_x()) {
			frameState->scrollDeltaX = bounds.extract_x();
			frameState->scrollDeltaY = 0.0f;
			extent = bounds.extract_x();
		}
	} else if (frameState->scrollMode == 2) {
		if (bounds.extract_y() > textObj->m_size.extract_y()) {
			frameState->scrollDeltaX = 0.0f;
			frameState->scrollDeltaY = bounds.extract_y();
			extent = bounds.extract_y();
		}
	}

	if (extent > 0.0f) {
		frameState->scrollActive = true;
		paf::math::v4 color = textObj->GetColor();
		textObj->SetColor(paf::math::v4(color.extract_x(), color.extract_y(),
						color.extract_z(), 1.0f));
		paf::math::v2 zeroPos(0.0f, 0.0f);
		textObj->SetLayoutAttribute(paf::graph::TextLayoutAttribute_VisiblePos, zeroPos);
		frameState->scrollDuration = (extent / 60.0f) * 1000.0f;
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
	paf::intrusive_ptr<filelist::BasicSegmentSource> segSrc(new filelist::BasicSegmentSource());
	paf::common::SharedPtr<filelist::NullVisualizerFactory> bgFactory(liveareaCreateBgFactory());
	paf::intrusive_ptr<paf::ui::filelist::CellSource> cellSrc(
		new filelist::NullCellSource(bgFactory, 1));

	paf::ui::filelist::SegmentListAccessor::OptionParam option;
	sce_paf_memset(&option, 0, sizeof(option));
	option.m_header_disappearance = true;
	segSrc->InsertSegment(0, cellSrc, option);
	paf::intrusive_ptr<paf::ui::filelist::SegmentSource> baseSegSrc(segSrc.get());
	fileList->SetSegmentSource(baseSegSrc);
	fileList->SetSegmentLayoutType(0, paf::ui::ListView::LAYOUT_TYPE_LIST);

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
void LiveAreaPageInner::AnimatePopIn(paf::ui::Widget *widget, float delay,
				     paf::ui::AnimCB callback)
{
	widget->Hide(0xff, NULL, NULL, 0.0f);
	paf::Timer *fadeTimer = new paf::Timer(600.0f, paf::Timer::FUNC_QUAD_OUT);
	widget->Show(0xff, fadeTimer, NULL, delay);

	paf::math::v4 startScale(0.0f, 0.0f, 0.0f, 0.0f);
	widget->SetScale(startScale, NULL, paf::ui::Widget::ANIM_SCALE, 0, 0.0f, NULL, NULL);

	paf::math::v4 endScale(1.0f, 1.0f, 0.0f, 0.0f);
	paf::Timer *scaleTimer = new paf::Timer(600.0f, paf::Timer::FUNC_BACK_OUT);
	widget->SetScale(endScale, scaleTimer, paf::ui::Widget::ANIM_SCALE, 2, delay,
			 callback, this);
}

/* 0x811A483A */
static LiveAreaRectBounds *liveareaExpandBounds(LiveAreaRectBounds *bounds,
						const paf::math::v3 &pos,
						const paf::math::v3 &size)
{
	float halfW = size.extract_x() * 0.5f;
	float halfH = size.extract_y() * 0.5f;
	float left = pos.extract_x() - halfW;
	float right = pos.extract_x() + halfW;
	float bottom = pos.extract_y() - halfH;
	float top = pos.extract_y() + halfH;

	if (left < bounds->minX)
		bounds->minX = left;
	if (bottom < bounds->minY)
		bounds->minY = bottom;
	if (right > bounds->maxX)
		bounds->maxX = right;
	if (top > bounds->maxY)
		bounds->maxY = top;
	return bounds;
}

/* 0x811A48B0 */
static int liveareaLayoutElement(const LiPlacedElement *elem, paf::graph::DrawObj *drawObj,
				 paf::graph::DrawObj *originObj, paf::graph::DrawObj *parentObj,
				 paf::ui::Widget *drawerWidget, const float *margins,
				 int formatVer)
{
	if (elem == NULL || drawObj == NULL || drawerWidget == NULL)
		return 0;

	bool isText = (paf::graph::TextObj::DrawObjType() == drawObj->GetDrawObjType());
	paf::math::v3 objSize = drawObj->m_size;

	if (isText) {
		if (formatVer >= 102) {
			paf::graph::TextObj *textObj = (paf::graph::TextObj *)drawObj;
			paf::math::v2 textBounds(0.0f, 0.0f);
			if (textObj->GetBounds(textBounds) == 0) {
				bool sizeChanged = false;
				if (elem->m_width == 0 && !elem->m_hasX &&
				    (elem->m_align == 3 || elem->m_align == 4) &&
				    objSize.extract_x() > textBounds.extract_x()) {
					objSize = paf::math::v3(textBounds.extract_x(),
								objSize.extract_y(),
								objSize.extract_z());
					sizeChanged = true;
				}
				if (elem->m_height == 0 && !elem->m_hasY &&
				    (elem->m_valign == 3 || elem->m_valign == 4) &&
				    objSize.extract_y() > textBounds.extract_y()) {
					objSize = paf::math::v3(objSize.extract_x(),
								textBounds.extract_y(),
								objSize.extract_z());
					sizeChanged = true;
				}
				if (sizeChanged)
					drawObj->SetSize(objSize);
			}
		}
	} else {
		paf::intrusive_ptr<paf::graph::Surface> surf;
		drawObj->GetTexture(surf, 0);
		if (surf.get() != NULL) {
			float width = objSize.extract_x();
			float height = objSize.extract_y();
			if (width == 0.0f)
				width = (float)surf->GetWidth();
			if (height == 0.0f)
				height = (float)surf->GetHeight();
			objSize = paf::math::v3(width, height, objSize.extract_z());
			drawObj->SetSize(objSize);
		}
	}

	bool hasOrigin = (originObj != NULL);
	const paf::math::v4 *widgetSize = drawerWidget->GetSize(0);
	float frameW = widgetSize->extract_x();
	float frameH = widgetSize->extract_y();

	float refX = 0.0f;
	float refY = 0.0f;
	float refW = frameW;
	float refH = frameH;

	paf::math::v4 widgetPos =
		drawerWidget->GetPos(0, NULL, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	float outerX = widgetPos.extract_x();
	float outerY = widgetPos.extract_y();
	float outerW = frameW;
	float outerH = frameH;

	if (originObj != NULL) {
		refX = originObj->m_pos.extract_x();
		refY = originObj->m_pos.extract_y();
		refW = originObj->m_size.extract_x();
		refH = originObj->m_size.extract_y();
	}
	if (parentObj != NULL) {
		outerX = parentObj->m_pos.extract_x();
		outerY = parentObj->m_pos.extract_y();
		outerW = parentObj->m_size.extract_x();
		outerH = parentObj->m_size.extract_y();
	}

	float totalW = objSize.extract_x();
	float totalH = objSize.extract_y();
	if (margins != NULL) {
		totalW += margins[2] + margins[3];
		totalH += margins[0] + margins[1];
	}

	float clipX = 0.0f;
	float clipY = 0.0f;
	float clipW = frameW;
	float clipH = frameH;
	bool outsideAnchor = false;

	if (hasOrigin &&
	    ((!elem->m_hasX && (elem->m_align == 3 || elem->m_align == 4)) ||
	     (!elem->m_hasY && (elem->m_valign == 3 || elem->m_valign == 4)))) {
		outsideAnchor = true;
		clipX = outerX;
		clipY = outerY;
		clipW = outerW;
		clipH = outerH;
	}

	if (!elem->m_hasX) {
		switch (elem->m_align) {
		case 0:
		case 1:
		case 2:
			if (!outsideAnchor) {
				clipX = refX;
				clipW = refW;
			}
			break;
		case 3:
			if (hasOrigin) {
				float originLeft = refX - refW * 0.5f;
				float outerLeft = outerX - outerW * 0.5f;
				clipX = (outerLeft + originLeft) * 0.5f;
				clipW = __builtin_fabsf(originLeft - outerLeft);
			}
			break;
		case 4:
			if (hasOrigin) {
				float outerRight = outerX + outerW * 0.5f;
				float originRight = refX + refW * 0.5f;
				clipX = (originRight + outerRight) * 0.5f;
				clipW = __builtin_fabsf(outerRight - originRight);
			}
			break;
		default:
			break;
		}
	}

	if (!elem->m_hasY) {
		switch (elem->m_valign) {
		case 0:
		case 1:
		case 2:
			if (!outsideAnchor) {
				clipY = refY;
				clipH = refH;
			}
			break;
		case 3:
			if (hasOrigin) {
				float outerTop = outerY + outerH * 0.5f;
				float originTop = refY + refH * 0.5f;
				clipY = (originTop + outerTop) * 0.5f;
				clipH = __builtin_fabsf(outerTop - originTop);
			}
			break;
		case 4:
			if (hasOrigin) {
				float originBottom = refY - refH * 0.5f;
				float outerBottom = outerY - outerH * 0.5f;
				clipY = (outerBottom + originBottom) * 0.5f;
				clipH = __builtin_fabsf(originBottom - outerBottom);
			}
			break;
		default:
			break;
		}
	}

	bool clamped = false;
	if (totalW > clipW) {
		totalW = clipW;
		clamped = true;
		if (isText) {
			((paf::graph::TextObj *)drawObj)->SetLayoutAttribute(
				paf::graph::TextLayoutAttribute_AdjustX, false);
		} else {
			paf::graph::PlaneObj *plane = (paf::graph::PlaneObj *)drawObj;
			plane->SetScaleMode(paf::graph::PlaneObj::SCALE_SIZE,
					    plane->m_scale_mode[1]);
		}
	}
	if (totalH > clipH) {
		totalH = clipH;
		clamped = true;
		if (isText) {
			((paf::graph::TextObj *)drawObj)->SetLayoutAttribute(
				paf::graph::TextLayoutAttribute_AdjustY, false);
		} else {
			paf::graph::PlaneObj *plane = (paf::graph::PlaneObj *)drawObj;
			plane->SetScaleMode(plane->m_scale_mode[0],
					    paf::graph::PlaneObj::SCALE_SIZE);
		}
	}

	if (clamped) {
		float innerW = totalW;
		float innerH = totalH;
		if (margins != NULL) {
			innerW -= margins[2] + margins[3];
			innerH -= margins[0] + margins[1];
		}
		drawObj->SetSize(paf::math::v3(innerW, innerH, objSize.extract_z()));
	}

	float posX = 0.0f;
	float posY = 0.0f;

	if (elem->m_hasX) {
		posX = (float)elem->m_x;
	} else {
		switch (elem->m_align) {
		case 1:
			posX = totalW * 0.5f - refW * 0.5f;
			break;
		case 2:
			posX = refW * 0.5f - totalW * 0.5f;
			break;
		case 3:
			if (hasOrigin)
				posX = -refW * 0.5f - totalW * 0.5f;
			break;
		case 4:
			if (hasOrigin)
				posX = totalW * 0.5f + refW * 0.5f;
			break;
		default:
			break;
		}
	}

	if (elem->m_hasY) {
		posY = (float)elem->m_y;
	} else {
		switch (elem->m_valign) {
		case 0:
			if (isText) {
				paf::graph::TextObj *textObj = (paf::graph::TextObj *)drawObj;
				paf::math::v2 textBounds(0.0f, 0.0f);
				textObj->GetBounds(textBounds);
				uint32_t lineCount = 0;
				if (textBounds.extract_y() <= totalH &&
				    textObj->GetLineCount(lineCount) == 0 && lineCount != 0) {
					paf::graph::TextLayoutLineInfo firstLine;
					paf::graph::TextLayoutLineInfo lastLine;
					textObj->GetLineInfo(0, firstLine);
					textObj->GetLineInfo(lineCount - 1, lastLine);
					posY = (((firstLine.frame.extract_y() + firstLine.descender) +
						 lastLine.frame.extract_y() + lastLine.ascender) -
						totalH) * 0.5f;
				}
			}
			break;
		case 1:
			posY = refH * 0.5f - totalH * 0.5f;
			break;
		case 2:
			posY = totalH * 0.5f - refH * 0.5f;
			break;
		case 3:
			if (hasOrigin)
				posY = totalH * 0.5f + refH * 0.5f;
			break;
		case 4:
			if (hasOrigin)
				posY = -refH * 0.5f - totalH * 0.5f;
			break;
		default:
			break;
		}
	}

	if (hasOrigin) {
		posX += refX;
		posY += refY;
		float leftEdge = posX - totalW * 0.5f;
		float rightEdge = posX + totalW * 0.5f;
		float topEdge = posY + totalH * 0.5f;
		float bottomEdge = posY - totalH * 0.5f;

		float minClipX = clipX - clipW * 0.5f;
		if (minClipX > leftEdge)
			posX -= leftEdge - minClipX;
		float maxClipX = clipX + clipW * 0.5f;
		if (maxClipX < rightEdge)
			posX -= rightEdge - maxClipX;
		float maxClipY = clipY + clipH * 0.5f;
		if (maxClipY < topEdge)
			posY -= topEdge - maxClipY;
		float minClipY = clipY - clipH * 0.5f;
		if (minClipY > bottomEdge)
			posY -= bottomEdge - minClipY;
	}

	float leftBound = posX - totalW * 0.5f;
	float rightBound = posX + totalW * 0.5f;
	float topBound = posY + totalH * 0.5f;
	float bottomBound = posY - totalH * 0.5f;

	if (-frameW * 0.5f > leftBound)
		posX -= leftBound - (-frameW * 0.5f);
	if (frameW * 0.5f < rightBound)
		posX -= rightBound - (frameW * 0.5f);
	if (frameH * 0.5f < topBound)
		posY -= topBound - (frameH * 0.5f);
	if (-frameH * 0.5f > bottomBound)
		posY -= bottomBound - (-frameH * 0.5f);

	if (margins != NULL) {
		posX += (margins[2] - margins[3]) * 0.5f;
		posY += (margins[1] - margins[0]) * 0.5f;
	}

	drawObj->SetPos(paf::math::v3(posX, posY, drawObj->m_pos.extract_z()));

	if (clamped && isText)
		return 0;
	return 1;
}

/* 0x811A5252 */
static int liveareaLayoutLiveItem(paf::ui::Widget *itemWidget, LiLiveItem *liveItem,
				  LiveAreaFrameState *frameState, bool isShown,
				  LiveAreaPageInner *pageInner, bool fromTextEvent)
{
	if (liveItem == NULL)
		return 0;

	if (frameState->isSwitching) {
		if (itemWidget->FindAnim(65538) == NULL)
			pageInner->AnimatePopIn(itemWidget, 0.0f, NULL);
	} else if (isShown) {
		if (!fromTextEvent || itemWidget->FindAnim(65538) == NULL)
			pageInner->AnimatePopIn(itemWidget, 0.0f, NULL);
	}

	paf::IDParam drawerId((uint32_t)-635233871);
	paf::ui::CompositeDrawer *drawer =
		(paf::ui::CompositeDrawer *)itemWidget->FindChild(drawerId, 0);

	LiveAreaRectBounds bounds;
	bounds.minX = 0.0f;
	bounds.minY = 0.0f;
	bounds.maxX = 0.0f;
	bounds.maxY = 0.0f;

	int formatVer = 100;
	LiTemplate *xmlDoc = pageInner->m_xmlDoc.get();
	if (xmlDoc != NULL)
		formatVer = SceLsdb_19BA5A0A(&xmlDoc->m_formatVer);

	LiItemBackground *bgElem = NULL;
	LiImage *imgElem = NULL;
	LiText *textElem = NULL;
	paf::graph::DrawObj *bgObj = NULL;
	paf::graph::DrawObj *imgObj = NULL;
	paf::graph::TextObj *textObj = NULL;

	for (paf::vector<LiItemElement *>::iterator it = liveItem->m_elements.begin();
	     it != liveItem->m_elements.end(); ++it) {
		LiItemElement *elem = *it;
		if (elem->m_type == LI_ITEM_BACKGROUND) {
			bgElem = (LiItemBackground *)elem;
			bgObj = drawer->GetDrawObjByIndex(frameState->bgImageObjIndex);
		} else if (elem->m_type == LI_ITEM_IMAGE) {
			imgElem = (LiImage *)elem;
			imgObj = drawer->GetDrawObjByIndex(frameState->imageObjIndex);
		} else if (elem->m_type == LI_ITEM_TEXT) {
			textElem = (LiText *)elem;
			textObj = (paf::graph::TextObj *)drawer->GetDrawObjByIndex(
				frameState->textObjIndex);
		}
	}

	if (bgObj != NULL && bgElem != NULL) {
		liveareaLayoutElement(bgElem, bgObj, NULL, NULL, drawer, NULL, formatVer);
		bgObj->SetVisible(true);
		liveareaExpandBounds(&bounds, bgObj->m_pos, bgObj->m_size);
	}

	paf::graph::DrawObj *imgOrigin = NULL;
	if (imgObj != NULL && imgElem != NULL) {
		if (imgElem->m_origin == LI_ITEM_BACKGROUND && bgObj != NULL)
			imgOrigin = bgObj;
		liveareaLayoutElement(imgElem, imgObj, imgOrigin, NULL, drawer, NULL, formatVer);
		imgObj->SetVisible(true);
		liveareaExpandBounds(&bounds, imgObj->m_pos, imgObj->m_size);
	}

	if (textObj != NULL && textElem != NULL) {
		if (textObj->m_flags.m_prepare)
			return 0;

		paf::graph::DrawObj *textOrigin = NULL;
		paf::graph::DrawObj *textParent = NULL;
		if (textElem->m_origin == LI_ITEM_BACKGROUND) {
			if (bgObj != NULL)
				textOrigin = bgObj;
		} else if (textElem->m_origin == LI_ITEM_IMAGE) {
			if (imgObj != NULL) {
				textOrigin = imgObj;
				textParent = imgOrigin;
			}
		}

		if (textElem->m_wordScroll) {
			uint32_t lineCount = 0;
			if (textObj->GetLineCount(lineCount) == 0) {
				if (lineCount > 1)
					liveareaInitTextScroll(frameState, 2, textObj);
				else
					liveareaInitTextScroll(frameState, 1, textObj);
			}
		}

		if (liveareaLayoutElement(textElem, textObj, textOrigin, textParent, drawer,
					  &frameState->textMarginTop, formatVer) == 0)
			return 0;

		textObj->SetVisible(true);
		liveareaExpandBounds(&bounds, textObj->m_pos, textObj->m_size);
	}

	frameState->isSwitching = false;

	const paf::math::v4 *drawerSize = drawer->GetSize(0);
	float minLimitX = -drawerSize->extract_x() * 0.5f;
	float minLimitY = -drawerSize->extract_y() * 0.5f;
	float maxLimitX = drawerSize->extract_x() * 0.5f;
	float maxLimitY = drawerSize->extract_y() * 0.5f;

	float focusMinX = bounds.minX - 8.0f;
	float focusMinY = bounds.minY - 8.0f;
	float focusMaxX = bounds.maxX + 8.0f;
	float focusMaxY = bounds.maxY + 8.0f;

	float width = focusMaxX - focusMinX;
	if (width < 64.0f) {
		float expand = (64.0f - width) * 0.5f;
		focusMinX -= expand;
		focusMaxX += expand;
	}
	float height = focusMaxY - focusMinY;
	if (height < 64.0f) {
		float expand = (64.0f - height) * 0.5f;
		focusMinY -= expand;
		focusMaxY += expand;
	}

	if (focusMinX < minLimitX)
		focusMinX = minLimitX;
	if (focusMaxX > maxLimitX)
		focusMaxX = maxLimitX;
	if (focusMinY < minLimitY)
		focusMinY = minLimitY;
	if (focusMaxY > maxLimitY)
		focusMaxY = maxLimitY;

	paf::ui::FocusShape shape;
	shape.m_shape = 2;
	shape.m_offset = paf::math::v3((focusMaxX + focusMinX) * 0.5f,
				       (focusMaxY + focusMinY) * 0.5f, 0.0f);
	shape.m_size = paf::math::v2(focusMaxX - focusMinX, focusMaxY - focusMinY);
	shape.m_rot = paf::math::v3(0.0f, 0.0f, 0.0f);
	shape.m_use_parts = false;
	shape.m_changed = true;

	ScePafWidget_13B246E0(itemWidget, &shape);
	itemWidget->SetFocusOption(NULL, 0);
	return 1;
}

/* 0x811A58D4 */
void LiveAreaPageInner::OnInitialImagesLoaded()
{
	LiTemplate *xmlDoc = m_xmlDoc.get();
	for (paf::list<LiPart *>::iterator it = xmlDoc->m_parts.begin();
	     it != xmlDoc->m_parts.end(); ++it) {
		LiPart *part = *it;
		if (part->m_type == LI_PART_FRAME) {
			LiFrame *frameDesc = (LiFrame *)part;
			paf::IDParam frameId(frameDesc->m_id);
			paf::ui::Widget *frameWidget = m_contentZone->FindChild(frameId, 0);
			if (frameWidget != NULL) {
				paf::IDParam itemId(1375437051U);
				paf::ui::Widget *itemWidget = frameWidget->FindChild(itemId, 0);
				LiveAreaFrameState *frameState = NULL;
				for (paf::list<LiveAreaFrameState *>::iterator fIt =
					     m_frames.begin();
				     fIt != m_frames.end(); ++fIt) {
					if (frameId.GetIDHash() ==
					    (*fIt)->frameWidget->m_name.GetIDHash()) {
						frameState = *fIt;
						break;
					}
				}
				if (frameState != NULL) {
					LiLiveItem *curItem = SceLsdb_92D3A2EA(
						frameDesc, frameState->currentIndex);
					liveareaLayoutLiveItem(itemWidget, curItem, frameState,
							       m_shown, this, false);
				}
			}
		}
	}

	if (!m_shown) {
		g_liveareaMgr->m_topmenuIf->showPageContent(m_page->m_pageKey, 600.0f);

		paf::ui::EventCBListener *focusListener =
			new paf::ui::EventCBListener(liveareaOnFocusTimer, this);
		m_page->m_fileList->SetTimerListener(16777509, 600.0f, focusListener);

		m_contentZone->Hide(0xff, NULL, NULL, 0.0f);
		paf::Timer *showTimer = new paf::Timer(400.0f);
		m_contentZone->Show(0xff, showTimer, NULL, 0.0f);

		paf::ui::EventCBListener *introListener =
			new paf::ui::EventCBListener(liveareaOnIntroTimer, this);
		m_contentZone->SetTimerListener(16777507, 400.0f, introListener);

		paf::IDParam barId(1728383896U);
		m_contentZone->FindChild(barId, 0);

		for (paf::list<paf::ui::Widget *>::iterator it = m_contentZone->m_children.begin();
		     it != m_contentZone->m_children.end(); ++it) {
			paf::ui::Widget *child = *it;
			SceUInt32 childHash = child->m_name.GetIDHash();
			if (childHash != 1728383896U &&
			    childHash != s_destroyContentZoneId.GetIDHash()) {
				child->Show(0xff, NULL, NULL, 0.0f);
				if (childHash == s_gateId.GetIDHash()) {
					paf::graph::PlaneObj *gatePlane =
						(paf::graph::PlaneObj *)
						((paf::ui::CompositeDrawer *)m_gateBtn)
							->GetDrawObjByIndex(1);
					if (gatePlane->m_flags.m_visible) {
						float curW = gatePlane->m_size.extract_x();
						float curH = gatePlane->m_size.extract_y();
						float clampedW = (curW < 280.0f) ? curW : 280.0f;
						float clampedH = (curH < 158.0f) ? curH : 158.0f;
						if (curW != clampedW || curH != clampedH) {
							gatePlane->SetScaleMode(
								paf::graph::PlaneObj::SCALE_SIZE,
								paf::graph::PlaneObj::SCALE_SIZE);
							gatePlane->SetSize(paf::math::v3(
								clampedW, clampedH,
								gatePlane->m_size.extract_z()));
						}
					}
				} else {
					paf::IDParam itemId(1375437051U);
					paf::ui::Widget *itemWidget = child->FindChild(itemId, 0);
					if (itemWidget != NULL)
						AnimatePopIn(itemWidget, 0.0f, NULL);
				}
			}
		}

		m_shown = true;
		m_page->EnsureFeed();
		if (!m_initialLoadNotified) {
			m_initialLoadNotified = true;
			if (xmlDoc != NULL)
				liveareaMgrUpdatePage(this, 0);
			m_page->UpdateLastLaunchTime();
		}
	}

	TryReloadXml();
}

/* 0x811A5E40 */
int LiveAreaPageInner::SetupDefaultBackground(paf::Plugin *plugin)
{
	(void)plugin;
	paf::ui::Widget *bgWidget =
		g_liveareaMgr->m_topmenuIf->getPageTurnWidget(m_page->m_pageKey);
	if (bgWidget == NULL)
		return 1;

	paf::intrusive_ptr<paf::graph::Surface> curSurf;
	bgWidget->GetTexture(curSurf);
	if (curSurf.get() == NULL) {
		paf::intrusive_ptr<paf::cache::CacheImageInterface> cacheRef;
		paf::string safeBgPath("vs0:data/internal/livearea/safebg.png");
		m_page->CreateImageCache(safeBgPath, cacheRef);
		++m_pendingInitialLoads;

		paf::ui::EventCBListener *okListener =
			new paf::ui::EventCBListener(liveareaOnDefaultBgImageLoad, this);
		bgWidget->AddEventListener(1048581, okListener, 1);

		paf::ui::EventCBListener *errListener =
			new paf::ui::EventCBListener(liveareaOnDefaultBgImageLoad, this);
		bgWidget->AddEventListener(1048583, errListener, 1);

		paf::intrusive_ptr<paf::graph::Surface> emptySurf;
		bgWidget->SetTexture(emptySurf);
		bgWidget->SetDrawObjMaskBit(&cacheRef, 0);
	}
	return 1;
}

/* 0x811A5FBE */
int LiveAreaPageInner::SetupTopBarWidgets(paf::vector<LiFunction> *functions)
{
	paf::IDParam barId(1728383896U);
	paf::ui::Widget *barWidget = m_contentZone->FindChild(barId, 0);
	if (barWidget == NULL)
		return 0;

	for (paf::list<paf::ui::Widget *>::iterator it = barWidget->m_children.begin();
	     it != barWidget->m_children.end(); ++it) {
		paf::ui::Widget *btn = *it;
		bool stillPresent = false;
		for (paf::vector<LiFunction>::iterator fnIt = functions->begin();
		     fnIt != functions->end(); ++fnIt) {
			if (s_topIconIds[fnIt->func].GetIDHash() == btn->m_name.GetIDHash()) {
				stillPresent = true;
				break;
			}
		}
		if (!stillPresent) {
			btn->m_name = s_destroyContentZoneId;
			paf::ui::EventCBListener *destroyListener =
				new paf::ui::EventCBListener(liveareaOnDestroyWidgetTimer, NULL);
			btn->SetTimerListener(16777510, 400.0f, destroyListener);

			paf::Timer *hideTimer = new paf::Timer(400.0f);
			btn->Hide(0xff, hideTimer, NULL, 0.0f);
		}
	}

	if (functions->size() == 0)
		return 1;

	float barWidth =
		(float)(functions->size() - 1) * 32.0f + (float)functions->size() * 80.0f;
	barWidget->SetSize(paf::math::v4(barWidth, 80.0f, 0.0f, 0.0f), NULL,
			   paf::ui::Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);

	float posX = barWidth * -0.5f + 40.0f;
	for (paf::vector<LiFunction>::iterator fnIt = functions->begin();
	     fnIt != functions->end(); ++fnIt, posX += 112.0f) {
		paf::ui::Widget *btn = NULL;
		bool newlyCreated = false;

		switch (fnIt->func) {
		case 0:
			btn = barWidget->FindChild(s_topIconIds[0], 0);
			if (btn == NULL) {
				paf::Plugin::TemplateOpenParam openParam;
				g_liveareaPlugin->TemplateOpen(
					barWidget, paf::IDParam((uint32_t)-124916220), openParam);
				btn = barWidget->FindChild(paf::IDParam(1461835643U), 0);
				if (btn != NULL) {
					btn->m_name = s_topIconIds[0];
					paf::intrusive_ptr<paf::graph::Surface> tex =
						g_liveareaPlugin->GetTexture(
							paf::IDParam(1017993839U));
					btn->SetTexture(tex);
					paf::ui::EventCBListener *clickListener =
						new paf::ui::EventCBListener(
							liveareaOnPatchClick, this);
					btn->AddEventListener(268435464, clickListener, 1);
					newlyCreated = true;
					if (m_shown)
						btn->Hide(0xff, NULL, NULL, 0.0f);
				}
			}
			break;
		case 2:
			btn = barWidget->FindChild(s_topIconIds[2], 0);
			if (btn == NULL) {
				paf::Plugin::TemplateOpenParam openParam;
				g_liveareaPlugin->TemplateOpen(
					barWidget, paf::IDParam((uint32_t)-124916220), openParam);
				btn = barWidget->FindChild(paf::IDParam(1461835643U), 0);
				if (btn != NULL) {
					btn->m_name = s_topIconIds[2];
					paf::intrusive_ptr<paf::graph::Surface> tex =
						g_liveareaPlugin->GetTexture(
							paf::IDParam(1844824002U));
					btn->SetTexture(tex);
					paf::ui::EventCBListener *clickListener =
						new paf::ui::EventCBListener(
							liveareaOnSearchClick, this);
					btn->AddEventListener(268435464, clickListener, 1);
					newlyCreated = true;
					if (m_shown)
						btn->Hide(0xff, NULL, NULL, 0.0f);
				}
			}
			break;
		case 3:
			btn = barWidget->FindChild(s_topIconIds[3], 0);
			if (btn == NULL) {
				paf::Plugin::TemplateOpenParam openParam;
				g_liveareaPlugin->TemplateOpen(
					barWidget, paf::IDParam((uint32_t)-124916220), openParam);
				btn = barWidget->FindChild(paf::IDParam(1461835643U), 0);
				if (btn != NULL) {
					btn->m_name = s_topIconIds[3];
					paf::intrusive_ptr<paf::graph::Surface> tex =
						g_liveareaPlugin->GetTexture(
							paf::IDParam(340307099U));
					btn->SetTexture(tex);
					paf::ui::EventCBListener *clickListener =
						new paf::ui::EventCBListener(
							liveareaOnVoucherClick, this);
					btn->AddEventListener(268435464, clickListener, 1);
					newlyCreated = true;
					if (m_shown)
						btn->Hide(0xff, NULL, NULL, 0.0f);
				}
			}
			break;
		case 4:
			btn = barWidget->FindChild(s_topIconIds[4], 0);
			if (btn == NULL) {
				paf::Plugin::TemplateOpenParam openParam;
				g_liveareaPlugin->TemplateOpen(
					barWidget, paf::IDParam((uint32_t)-124916220), openParam);
				btn = barWidget->FindChild(paf::IDParam(1461835643U), 0);
				if (btn != NULL) {
					btn->m_name = s_topIconIds[4];
					paf::intrusive_ptr<paf::graph::Surface> tex =
						g_liveareaPlugin->GetTexture(
							paf::IDParam((uint32_t)-412683649));
					btn->SetTexture(tex);
					paf::ui::EventCBListener *clickListener =
						new paf::ui::EventCBListener(
							liveareaOnOnlineHelpClick, this);
					btn->AddEventListener(268435464, clickListener, 1);
					newlyCreated = true;
					if (m_shown)
						btn->Hide(0xff, NULL, NULL, 0.0f);
				}
			}
			m_helpUri = fnIt->target;
			break;
		case 5:
			btn = barWidget->FindChild(s_topIconIds[5], 0);
			if (btn == NULL) {
				paf::Plugin::TemplateOpenParam openParam;
				g_liveareaPlugin->TemplateOpen(
					barWidget, paf::IDParam((uint32_t)-124916220), openParam);
				btn = barWidget->FindChild(paf::IDParam(1461835643U), 0);
				if (btn != NULL) {
					btn->m_name = s_topIconIds[5];
					paf::intrusive_ptr<paf::graph::Surface> tex =
						g_liveareaPlugin->GetTexture(
							paf::IDParam(797504905U));
					btn->SetTexture(tex);
					paf::ui::EventCBListener *clickListener =
						new paf::ui::EventCBListener(
							liveareaOnHelpClick, this);
					btn->AddEventListener(268435464, clickListener, 1);
					newlyCreated = true;
					if (m_shown)
						btn->Hide(0xff, NULL, NULL, 0.0f);
				}
			}
			break;
		case 6:
			btn = barWidget->FindChild(s_topIconIds[6], 0);
			if (btn == NULL) {
				paf::Plugin::TemplateOpenParam openParam;
				g_liveareaPlugin->TemplateOpen(
					barWidget, paf::IDParam((uint32_t)-124916220), openParam);
				btn = barWidget->FindChild(paf::IDParam(1461835643U), 0);
				if (btn != NULL) {
					btn->m_name = s_topIconIds[6];
					paf::intrusive_ptr<paf::graph::Surface> tex =
						g_liveareaPlugin->GetTexture(
							paf::IDParam(11839196U));
					btn->SetTexture(tex);
					paf::ui::EventCBListener *clickListener =
						new paf::ui::EventCBListener(
							liveareaOnReloadClick, this);
					btn->AddEventListener(268435464, clickListener, 1);

					paf::Plugin::TemplateOpenParam spinOpenParam;
					g_liveareaPlugin->TemplateOpen(
						btn, paf::IDParam((uint32_t)-934747523),
						spinOpenParam);
					paf::ui::Widget *spinIcon =
						btn->FindChild(paf::IDParam((uint32_t)-1039764635),
							       0);
					if (spinIcon != NULL) {
						paf::intrusive_ptr<paf::graph::Surface> spinTex =
							g_liveareaPlugin->GetTexture(
								paf::IDParam(1296932000U));
						spinIcon->SetTexture(spinTex);
					}
					newlyCreated = true;
					if (m_shown)
						btn->Hide(0xff, NULL, NULL, 0.0f);
				}
			}
			break;
		case 7:
			btn = barWidget->FindChild(s_topIconIds[7], 0);
			if (btn == NULL) {
				paf::Plugin::TemplateOpenParam openParam;
				g_liveareaPlugin->TemplateOpen(
					barWidget, paf::IDParam((uint32_t)-124916220), openParam);
				btn = barWidget->FindChild(paf::IDParam(1461835643U), 0);
				if (btn != NULL) {
					btn->m_name = s_topIconIds[7];
					paf::intrusive_ptr<paf::graph::Surface> tex =
						g_liveareaPlugin->GetTexture(
							paf::IDParam(127408714U));
					btn->SetTexture(tex);
					paf::ui::EventCBListener *clickListener =
						new paf::ui::EventCBListener(
							liveareaOnImportSaveClick, this);
					btn->AddEventListener(268435464, clickListener, 1);
					newlyCreated = true;
					if (m_shown)
						btn->Hide(0xff, NULL, NULL, 0.0f);
				}
			}
			break;
		case 8:
			btn = barWidget->FindChild(s_topIconIds[8], 0);
			if (btn == NULL) {
				paf::Plugin::TemplateOpenParam openParam;
				g_liveareaPlugin->TemplateOpen(
					barWidget, paf::IDParam((uint32_t)-124916220), openParam);
				btn = barWidget->FindChild(paf::IDParam(1461835643U), 0);
				if (btn != NULL) {
					btn->m_name = s_topIconIds[8];
					paf::intrusive_ptr<paf::graph::Surface> tex =
						g_liveareaPlugin->GetTexture(
							paf::IDParam(1633895826U));
					btn->SetTexture(tex);
					paf::ui::EventCBListener *clickListener =
						new paf::ui::EventCBListener(
							liveareaOnOnlineStorageClick, this);
					btn->AddEventListener(268435464, clickListener, 1);
					newlyCreated = true;
					if (m_shown)
						btn->Hide(0xff, NULL, NULL, 0.0f);
				}
			}
			break;
		default:
			break;
		}

		if (btn != NULL) {
			paf::math::v4 pos(posX, 0.0f, 0.0f, 0.0f);
			if (m_shown) {
				if (newlyCreated) {
					btn->SetPos(pos, NULL, paf::ui::Widget::ANIM_MOVE, 0,
						    0.0f, NULL, NULL);
				} else {
					paf::Timer *moveTimer =
						new paf::Timer(300.0f, paf::Timer::FUNC_QUAD_INOUT);
					btn->SetPos(pos, moveTimer, paf::ui::Widget::ANIM_MOVE, 2,
						    0.0f, NULL, NULL);
				}
				paf::Timer *fadeTimer = new paf::Timer(300.0f);
				btn->Show(0xff, fadeTimer, NULL, 0.0f);
			} else {
				btn->SetPos(pos, NULL, paf::ui::Widget::ANIM_MOVE, 0,
					    0.0f, NULL, NULL);
			}
		}
	}

	return 1;
}

/* 0x811A7352 */
int LiveAreaPageInner::SetupTopBar(paf::Plugin *plugin, LiFunctionZone *funcZone)
{
	(void)plugin;
	paf::vector<LiFunction> functions;
	if (funcZone != NULL)
		functions = funcZone->m_functions;

	if (m_style != 0) {
		if (m_manualMode == 1) {
			LiFunction manualFn;
			manualFn.func = 5;
			functions.push_back(manualFn);
		}
		switch (m_style) {
		case 1:
			if (m_hasOnlineManual) {
				LiFunction importFn;
				importFn.func = 7;
				functions.push_back(importFn);
			}
			for (unsigned int i = 0; i < 3; ++i) {
				LiFunction defFn;
				defFn.func = s_topFuncsGame[i];
				functions.push_back(defFn);
			}
			break;
		case 2:
			for (unsigned int i = 0; i < 2; ++i) {
				LiFunction defFn;
				defFn.func = s_topFuncsMedia[i];
				functions.push_back(defFn);
			}
			break;
		case 3:
		case 4:
			for (unsigned int i = 0; i < 1; ++i) {
				LiFunction defFn;
				defFn.func = s_topFuncsWeb[i];
				functions.push_back(defFn);
			}
			break;
		default:
			break;
		}
	}

	paf::sort(functions.begin(), functions.end(), liveareaCompareFunctions);

	paf::vector<LiFunction>::iterator it = functions.begin();
	while (it != functions.end()) {
		paf::vector<LiFunction>::iterator nextIt = it;
		++nextIt;
		if (nextIt == functions.end())
			break;
		if (it->func == nextIt->func)
			it = functions.erase(it, nextIt);
		else
			++it;
	}

	return SetupTopBarWidgets(&functions);
}

/* 0x811A7596 */
int LiveAreaPageInner::SetupTitle(paf::Plugin *plugin, LiTitle *titleDesc)
{
	(void)plugin;
	if (m_style != 0)
		return 1;

	const char *titleId = m_page->m_titleId.c_str();
	if (titleId != NULL && sce_paf_strcmp(SceActivityDb_E6C23731(), titleId) == 0)
		return 1;

	paf::ui::Widget *pageRoot =
		g_liveareaMgr->m_topmenuIf->getPageRootWidget(m_page->m_pageKey);
	if (pageRoot == NULL)
		return 1;

	paf::Plugin::TemplateOpenParam openParam;
	g_liveareaPlugin->TemplateOpen(pageRoot, paf::IDParam(966593341U), openParam);
	paf::ui::Widget *titlePlane = pageRoot->FindChild(paf::IDParam((uint32_t)-661515378), 0);
	if (titlePlane != NULL) {
		paf::ui::Widget *titleText = pageRoot->FindChild(paf::IDParam(351679779U), 0);
		titleText->SetString(m_title);
		paf::math::v4 titleColor(titleDesc->m_color[0], titleDesc->m_color[1],
					 titleDesc->m_color[2], titleDesc->m_color[3]);
		titleText->SetColor(titleColor, NULL, paf::ui::Widget::ANIM_COLOR, 0, 0.0f,
				    NULL, NULL);
		titlePlane->SetParent(titlePlane->m_parent, NULL, 5);
	}
	return 1;
}

/* 0x811A777C */
int LiveAreaPageInner::SetupItemText(LiText *textDesc, paf::ui::Widget *itemWidget,
				     LiveAreaFrameState *frameState)
{
	LiTemplate *xmlDoc = m_xmlDoc.get();
	paf::IDParam drawerId((uint32_t)-635233871);
	paf::ui::CompositeDrawer *drawer =
		(paf::ui::CompositeDrawer *)itemWidget->FindChild(drawerId, 0);

	frameState->textObjIndex = (int)drawer->m_drawobjs.size();
	paf::common::SharedPtr<paf::graph::DrawObj> textRef(
		new paf::graph::TextObj(paf::graph::TextObjStyle()));
	if (textRef.get() == NULL) {
		frameState->textObjIndex = -1;
		return 0;
	}

	drawer->InsertDrawObj(textRef, frameState->textObjIndex, false);
	paf::graph::TextObj *textObj = (paf::graph::TextObj *)textRef.get();
	textObj->m_draw_style.deepen_color = 0;

	float boxW = 0.0f;
	float boxH = 0.0f;
	if (textDesc->m_width <= 0) {
		textObj->SetLayoutAttribute(paf::graph::TextLayoutAttribute_AdjustX, true);
	} else {
		textObj->SetLayoutAttribute(paf::graph::TextLayoutAttribute_AdjustX, false);
		boxW = (float)textDesc->m_width;
	}
	if (textDesc->m_height <= 0) {
		textObj->SetLayoutAttribute(paf::graph::TextLayoutAttribute_AdjustY, true);
	} else {
		textObj->SetLayoutAttribute(paf::graph::TextLayoutAttribute_AdjustY, false);
		boxH = (float)textDesc->m_height;
	}
	textObj->SetSize(paf::math::v3(boxW, boxH, 0.0f));

	if (textDesc->m_textAlign == 0)
		textObj->SetLayoutAttribute(paf::graph::TextLayoutAttribute_AlignX,
					    (int32_t)paf::graph::TextLayoutAlignX_Center);
	else if (textDesc->m_textAlign == 1)
		textObj->SetLayoutAttribute(paf::graph::TextLayoutAttribute_AlignX,
					    (int32_t)paf::graph::TextLayoutAlignX_Left);
	else if (textDesc->m_textAlign == 2)
		textObj->SetLayoutAttribute(paf::graph::TextLayoutAttribute_AlignX,
					    (int32_t)paf::graph::TextLayoutAlignX_Right);

	if (textDesc->m_textValign == 0)
		textObj->SetLayoutAttribute(paf::graph::TextLayoutAttribute_AlignY,
					    (int32_t)paf::graph::TextLayoutAlignY_Center);
	else if (textDesc->m_textValign == 1)
		textObj->SetLayoutAttribute(paf::graph::TextLayoutAttribute_AlignY,
					    (int32_t)paf::graph::TextLayoutAlignY_Top);
	else if (textDesc->m_textValign == 2)
		textObj->SetLayoutAttribute(paf::graph::TextLayoutAttribute_AlignY,
					    (int32_t)paf::graph::TextLayoutAlignY_Bottom);

	if (textDesc->m_lineAlign == 0)
		textObj->SetLayoutAttribute(paf::graph::TextLayoutAttribute_TextAlign,
					    (int32_t)paf::graph::TextLayoutTextAlign_Center);
	else if (textDesc->m_lineAlign == 1)
		textObj->SetLayoutAttribute(paf::graph::TextLayoutAttribute_TextAlign,
					    (int32_t)paf::graph::TextLayoutTextAlign_Left);
	else if (textDesc->m_lineAlign == 2)
		textObj->SetLayoutAttribute(paf::graph::TextLayoutAttribute_TextAlign,
					    (int32_t)paf::graph::TextLayoutTextAlign_Right);

	if (xmlDoc != NULL && SceLsdb_19BA5A0A(&xmlDoc->m_formatVer) >= 101)
		textObj->SetLayoutAttribute(paf::graph::TextLayoutAttribute_LineSpace,
					    (float)textDesc->m_lineSpace);

	textObj->SetLayoutAttribute(paf::graph::TextLayoutAttribute_Kinsoku, textDesc->m_lineBreak);
	textObj->SetLayoutAttribute(paf::graph::TextLayoutAttribute_WordWrap, textDesc->m_wordWrap);
	textObj->SetLayoutAttribute(paf::graph::TextLayoutAttribute_Ellipsis, textDesc->m_ellipsis);

	frameState->textMarginTop = (float)textDesc->m_marginTop;
	frameState->textMarginBottom = (float)textDesc->m_marginBottom;
	frameState->textMarginLeft = (float)textDesc->m_marginLeft;
	frameState->textMarginRight = (float)textDesc->m_marginRight;

	if (textDesc->m_titleIndex < 0) {
		textObj->SetAttributedString(textDesc->m_string);
	} else {
		paf::wstring origStr;
		paf::graph::AttributedString builtStr;
		paf::wstring mergedStr;
		if (textDesc->m_string.GetString(origStr) != 0)
			return 0;

		paf::_list_iterator<paf::graph::TextStyleRun *> runBegin;
		paf::_list_iterator<paf::graph::TextStyleRun *> runEnd;
		if (textDesc->m_string.GetStyleRuns(runBegin, runEnd) != 0)
			return 0;

		uint32_t titleLen = 0xFFFFFFFFU;
		int runIdx = 0;
		size_t charOffset = 0;
		for (paf::_list_iterator<paf::graph::TextStyleRun *> it = runBegin;
		     it != runEnd; ++it, ++runIdx) {
			paf::graph::TextStyleRun *run = *it;
			if (runIdx == textDesc->m_titleIndex) {
				paf::wstring titleWide;
				paf::common::Utf8ToUtf16(m_searchKey, &titleWide, 0);
				titleLen = titleWide.size();
				mergedStr.append(titleWide.c_str(), titleWide.size());
			} else {
				paf::wstring segment = origStr.substr(charOffset, run->len);
				charOffset += run->len;
				mergedStr.append(segment.c_str(), segment.size());
			}
		}

		builtStr.SetString(mergedStr);

		uint32_t outPos = 0;
		runIdx = 0;
		for (paf::_list_iterator<paf::graph::TextStyleRun *> it = runBegin;
		     it != runEnd; ++it, ++runIdx) {
			paf::graph::TextStyleRun *run = *it;
			uint32_t segLen = (runIdx == textDesc->m_titleIndex) ? titleLen : run->len;
			builtStr.SetStyle(outPos, segLen, run->style);
			outPos += segLen;
		}
		textObj->SetAttributedString(builtStr);
	}

	if (textObj->m_flags.m_visible)
		textObj->m_flags.m_visible = false;

	paf::ui::EventCBListener *layoutListener =
		new paf::ui::EventCBListener(liveareaOnItemTextInit, this);
	drawer->AddEventListener(1048580, layoutListener, 1);
	return 1;
}

/* 0x811A7DC6 */
int LiveAreaPageInner::CreateContentImageCache(
	const paf::string *relPath,
	paf::intrusive_ptr<paf::cache::CacheImageInterface> *outRef)
{
	paf::string fullPath = m_baseDir + *relPath;
	return m_page->CreateImageCache(fullPath, *outRef);
}

/* 0x811A7EA8 */
int LiveAreaPageInner::SetupPlaneSize(LiPlacedElement *elem, paf::graph::PlaneObj *plane)
{
	float width = (elem->m_width > 0) ? (float)elem->m_width : 0.0f;
	float height = (elem->m_height > 0) ? (float)elem->m_height : 0.0f;
	plane->SetSize(paf::math::v3(width, height, 0.0f));
	return 1;
}

/* 0x811A7F1A */
int LiveAreaPageInner::SetupItemImage(LiImage *imageDesc, paf::ui::Widget *itemWidget,
				      LiveAreaFrameState *frameState)
{
	paf::IDParam drawerId((uint32_t)-635233871);
	paf::ui::CompositeDrawer *drawer =
		(paf::ui::CompositeDrawer *)itemWidget->FindChild(drawerId, 0);

	frameState->imageObjIndex = (int)drawer->m_drawobjs.size();
	paf::common::SharedPtr<paf::graph::DrawObj> planeRef(
		new paf::graph::PlaneObj(paf::graph::PlaneObjStyle()));
	if (planeRef.get() == NULL) {
		frameState->imageObjIndex = -1;
		return 0;
	}

	drawer->InsertDrawObj(planeRef, frameState->imageObjIndex, false);
	SetupPlaneSize(imageDesc, (paf::graph::PlaneObj *)planeRef.get());

	paf::intrusive_ptr<paf::cache::CacheImageInterface> cacheRef;
	CreateContentImageCache(&imageDesc->m_image, &cacheRef);
	if (cacheRef.get() != NULL) {
		if (m_shown)
			++frameState->pendingLoads;
		else
			++m_pendingInitialLoads;

		paf::ui::EventCBListener *okListener =
			new paf::ui::EventCBListener(liveareaOnFrameImageLoad, this);
		drawer->AddEventListener(1048581, okListener, 1);

		paf::ui::EventCBListener *errListener =
			new paf::ui::EventCBListener(liveareaOnFrameImageLoad, this);
		drawer->AddEventListener(1048583, errListener, 1);

		drawer->SetDrawObjMaskBit(&cacheRef, frameState->imageObjIndex, 0);
	}
	return 1;
}

/* 0x811A81BC */
int LiveAreaPageInner::SetupItemBgImage(LiItemBackground *bgDesc, paf::ui::Widget *itemWidget,
					LiveAreaFrameState *frameState)
{
	paf::IDParam drawerId((uint32_t)-635233871);
	paf::ui::CompositeDrawer *drawer =
		(paf::ui::CompositeDrawer *)itemWidget->FindChild(drawerId, 0);

	frameState->bgImageObjIndex = (int)drawer->m_drawobjs.size();
	paf::common::SharedPtr<paf::graph::DrawObj> planeRef(
		new paf::graph::PlaneObj(paf::graph::PlaneObjStyle()));
	if (planeRef.get() == NULL) {
		frameState->bgImageObjIndex = -1;
		return 0;
	}

	drawer->InsertDrawObj(planeRef, frameState->bgImageObjIndex, false);
	SetupPlaneSize(bgDesc, (paf::graph::PlaneObj *)planeRef.get());

	paf::intrusive_ptr<paf::cache::CacheImageInterface> cacheRef;
	CreateContentImageCache(&bgDesc->m_image, &cacheRef);
	if (cacheRef.get() != NULL) {
		if (m_shown)
			++frameState->pendingLoads;
		else
			++m_pendingInitialLoads;

		paf::ui::EventCBListener *okListener =
			new paf::ui::EventCBListener(liveareaOnFrameImageLoad, this);
		drawer->AddEventListener(1048581, okListener, 1);

		paf::ui::EventCBListener *errListener =
			new paf::ui::EventCBListener(liveareaOnFrameImageLoad, this);
		drawer->AddEventListener(1048583, errListener, 1);

		drawer->SetDrawObjMaskBit(&cacheRef, frameState->bgImageObjIndex, 0);
	}
	return 1;
}

/* 0x811A845E */
int LiveAreaPageInner::PopulateFrame(LiFrame *frameDesc, LiveAreaFrameState *frameState)
{
	paf::IDParam frameId(frameDesc->m_id);
	paf::ui::Widget *frameWidget = m_contentZone->FindChild(frameId, 0);
	if (frameWidget == NULL)
		return 0;

	paf::IDParam itemId(1375437051U);
	paf::ui::Widget *oldItem = frameWidget->FindChild(itemId, 0);
	if (oldItem != NULL) {
		oldItem->m_name = s_destroyContentZoneId;
		oldItem->DestroyWidget();
		frameState->bgImageObjIndex = -1;
		frameState->imageObjIndex = -1;
		frameState->textObjIndex = -1;
		frameState->scrollDeltaX = 0.0f;
		frameState->scrollDeltaY = 0.0f;
		frameState->textMarginTop = 0.0f;
		frameState->textMarginBottom = 0.0f;
		frameState->textMarginLeft = 0.0f;
		frameState->textMarginRight = 0.0f;
		frameState->scrollMode = 0;
		frameState->scrollActive = false;
	}

	LiLiveItem *liveItem = SceLsdb_92D3A2EA(frameDesc, frameState->currentIndex);
	if (liveItem == NULL)
		return 0;

	paf::Plugin::TemplateOpenParam openParam;
	g_liveareaPlugin->TemplateOpen(frameWidget, paf::IDParam(1051252360U), openParam);
	paf::ui::Widget *itemWidget = frameWidget->FindChild(itemId, 0);
	if (itemWidget == NULL)
		return 0;

	itemWidget->Hide(0xff, NULL, NULL, 0.0f);

	for (paf::vector<LiItemElement *>::iterator it = liveItem->m_elements.begin();
	     it != liveItem->m_elements.end(); ++it) {
		LiItemElement *elem = *it;
		if (elem->m_type == LI_ITEM_BACKGROUND)
			SetupItemBgImage((LiItemBackground *)elem, itemWidget, frameState);
		else if (elem->m_type == LI_ITEM_IMAGE)
			SetupItemImage((LiImage *)elem, itemWidget, frameState);
		else if (elem->m_type == LI_ITEM_TEXT)
			SetupItemText((LiText *)elem, itemWidget, frameState);
	}

	LiTarget *target = liveItem->m_target;
	if (target == NULL || target->m_uri.empty())
		return 1;

	bool isActivityApp = false;
	const char *titleId = m_page->m_titleId.c_str();
	if (titleId != NULL)
		isActivityApp = (sce_paf_strcmp(SceActivityDb_E6C23731(), titleId) == 0);

	const char *uri = target->m_uri.c_str();
	bool allowTarget = false;
	bool launchPsla = false;
	bool launchApp = false;

	if (sce_paf_strncmp(uri, "psns:", sce_paf_strlen("psns:")) == 0 ||
	    sce_paf_strncmp(uri, "http:", sce_paf_strlen("http:")) == 0 ||
	    sce_paf_strncmp(uri, "https:", sce_paf_strlen("https:")) == 0 ||
	    sce_paf_strncmp(uri, "psts:", sce_paf_strlen("psts:")) == 0 ||
	    sce_paf_strncmp(uri, "webmodal:", sce_paf_strlen("webmodal:")) == 0) {
		allowTarget = true;
	} else if (sce_paf_strncmp(uri, "psla:", sce_paf_strlen("psla:")) == 0) {
		allowTarget = true;
		if (m_style == 3)
			launchPsla = true;
		else
			launchApp = true;
	} else if (m_style == 0 || m_style == 4) {
		allowTarget = true;
	} else if (isActivityApp) {
		if (sce_paf_strncmp(uri, "psgm:", sce_paf_strlen("psgm:")) == 0)
			allowTarget = true;
	} else if (titleId != NULL && sce_paf_strcmp("PCSI00013", titleId) == 0) {
		allowTarget = true;
	}

	if (!allowTarget)
		return 1;

	if (isActivityApp) {
		paf::ui::EventCBListener *clickListener =
			new paf::ui::EventCBListener(liveareaOnItemTrackedUriClick, liveItem);
		itemWidget->AddEventListener(268435464, clickListener, 1);
	} else if (launchPsla) {
		paf::ui::EventCBListener *clickListener =
			new paf::ui::EventCBListener(liveareaOnItemPslaClick, liveItem);
		itemWidget->AddEventListener(268435464, clickListener, 1);
		itemWidget->m_user_data[0] = (void *)titleId;
	} else if (!launchApp) {
		paf::ui::EventCBListener *clickListener =
			new paf::ui::EventCBListener(liveareaOnItemUriClick, liveItem);
		itemWidget->AddEventListener(268435464, clickListener, 1);
	} else {
		bool needsPatch = false;
		LiTemplate *xmlDoc = m_xmlDoc.get();
		if (xmlDoc != NULL && SceLsdb_19BA5A0A(&xmlDoc->m_formatVer) >= 103 &&
		    !target->m_reqAppVer.empty()) {
			int reqVer = SceLsdb_87172F61(&target->m_reqAppVer);
			if (reqVer > 0) {
				int curVer = SceLsdb_87172F61(&m_url);
				if (curVer >= 0 && curVer < reqVer)
					needsPatch = true;
			}
		}

		if (needsPatch) {
			paf::ui::EventCBListener *patchListener =
				new paf::ui::EventCBListener(liveareaOnPatchClick, this);
			itemWidget->AddEventListener(268435464, patchListener, 1);
		} else {
			paf::ui::EventCBListener *appListener =
				new paf::ui::EventCBListener(liveareaOnItemAppClick, liveItem);
			itemWidget->AddEventListener(268435464, appListener, 1);
			itemWidget->m_user_data[0] = (void *)titleId;
		}
	}

	paf::ui::EventCBListener *pressListener =
		new paf::ui::EventCBListener(liveareaOnItemPress, liveItem);
	itemWidget->AddEventListener(268435459, pressListener, 1);

	paf::ui::EventCBListener *releaseListener =
		new paf::ui::EventCBListener(liveareaOnItemRelease, liveItem);
	itemWidget->AddEventListener(268435457, releaseListener, 1);

	paf::ui::EventCBListener *cancelListener =
		new paf::ui::EventCBListener(liveareaOnItemRelease, liveItem);
	itemWidget->AddEventListener(268435460, cancelListener, 1);

	return 1;
}

/* 0x811A8B70 */
void LiveAreaPageInner::AnimatePopOut(paf::ui::Widget *widget, paf::ui::AnimCB callback,
				      void *userdata)
{
	paf::Timer *fadeTimer = new paf::Timer(300.0f, paf::Timer::FUNC_QUAD_OUT);
	widget->Hide(0xff, fadeTimer, NULL, 0.0f);
	widget->DeleteEventCallback(268435464, NULL, NULL);

	paf::math::v4 zeroScale(0.0f, 0.0f, 0.0f, 0.0f);
	paf::Timer *scaleTimer = new paf::Timer(300.0f, paf::Timer::FUNC_QUAD_INOUT);
	widget->SetScale(zeroScale, scaleTimer, paf::ui::Widget::ANIM_SCALE, 2, 0.0f,
			 callback, userdata);
}

/* 0x811A8CE4 */
int LiveAreaPageInner::SetupFrame(paf::Plugin *plugin, LiFrame *frameDesc,
				  LiveAreaFrameState *frameState)
{
	(void)plugin;
	paf::IDParam frameId(frameDesc->m_id);
	paf::ui::Widget *frameWidget = m_contentZone->FindChild(frameId, 0);
	if (frameWidget == NULL)
		return 0;

	if (!m_shown)
		frameWidget->Hide(0xff, NULL, NULL, 0.0f);

	paf::IDParam itemId(1375437051U);
	paf::ui::Widget *existingItem = frameWidget->FindChild(itemId, 0);
	if (existingItem != NULL) {
		existingItem->m_name = s_destroyContentZoneId;
		if (m_shown) {
			AnimatePopOut(existingItem, liveareaOnReloadFrameOut, this);
			++frameState->pendingLoads;
		} else {
			existingItem->DestroyWidget();
		}
	}

	frameState->frameWidget = frameWidget;
	bool useNetworkTime = SceLsdb_69B32F6C(m_appAttr);
	liveareaSelectNextItem(&frameState->currentIndex, frameDesc, useNetworkTime);
	if (frameState->currentIndex >= 0)
		return PopulateFrame(frameDesc, frameState);
	return 1;
}

/* 0x811A8E84 */
int LiveAreaPageInner::SetupGate(paf::Plugin *plugin, LiGate *gateDesc)
{
	(void)plugin;
	paf::ui::Widget *gateZone = m_contentZone->FindChild(s_gateId, 0);
	if (gateZone == NULL)
		return 0;

	m_gateHasBg = false;
	bool created = false;
	paf::IDParam gateBtnId(1479557318U);
	if (gateZone->FindChild(gateBtnId, 0) == NULL) {
		gateZone->SetSize(paf::math::v4(292.0f, 158.0f, 0.0f, 0.0f), NULL,
				  paf::ui::Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);
		paf::Plugin::TemplateOpenParam openParam;
		g_liveareaPlugin->TemplateOpen(gateZone, paf::IDParam(342357833U), openParam);
		gateZone->Hide(0xff, NULL, NULL, 0.0f);
		created = true;
	}

	m_gateBtn = gateZone->FindChild(gateBtnId, 0);
	if (m_gateBtn == NULL)
		return 0;

	if (created) {
		paf::ui::EventCBListener *layoutListener =
			new paf::ui::EventCBListener(liveareaOnGateTextLayout, this);
		m_gateBtn->AddEventListener(1048580, layoutListener, 1);
	}

	paf::ui::CompositeDrawer *gateDrawer = (paf::ui::CompositeDrawer *)m_gateBtn;
	paf::graph::TextObj *labelText = (paf::graph::TextObj *)gateDrawer->GetDrawObjByIndex(3);
	if (labelText != NULL) {
		const wchar_t *label = m_hasLiveSurface
			? g_liveareaPlugin->GetString(paf::IDParam((uint32_t)-445251843))
			: g_liveareaPlugin->GetString(paf::IDParam((uint32_t)-613798874));
		paf::wstring labelStr(label ? label : L"");
		labelText->SetString(labelStr);
	}

	if (m_hasLiveSurface) {
		paf::graph::DrawObj *bgPlane = gateDrawer->GetDrawObjByIndex(0);
		if (m_gateSurface != NULL)
			bgPlane->SetColor(paf::math::v4(1.0f, 1.0f, 1.0f, 1.0f));
		else
			bgPlane->SetColor(paf::math::v4(0.0f, 0.0f, 0.0f, 1.0f));
	}

	if (!gateDesc->m_startupImage.empty()) {
		paf::intrusive_ptr<paf::cache::CacheImageInterface> cacheRef;
		CreateContentImageCache(&gateDesc->m_startupImage, &cacheRef);
		if (m_gateCacheRef != cacheRef.get()) {
			if (m_gateCacheRef != NULL)
				m_gateCacheRef->Release();
			m_gateCacheRef = cacheRef.get();
			if (m_gateCacheRef != NULL)
				m_gateCacheRef->AddRef();
		}

		if (m_gateCacheRef != NULL) {
			if (m_hasLiveSurface) {
				paf::graph::DrawObj *fgPlane = gateDrawer->GetDrawObjByIndex(1);
				if (fgPlane->m_flags.m_visible)
					fgPlane->m_flags.m_visible = false;
				paf::intrusive_ptr<paf::graph::Surface> surfRef(m_gateSurface);
				m_gateBtn->SetTexture(surfRef, 0, 0);
			} else {
				++m_pendingInitialLoads;
				if ((unsigned int)m_style > 1U) {
					paf::graph::PlaneObj *fgPlane =
						(paf::graph::PlaneObj *)gateDrawer->GetDrawObjByIndex(1);
					paf::intrusive_ptr<paf::cache::CacheImageInterface> emptyCache;
					m_gateBtn->SetDrawObjMaskBit(&emptyCache, 0, 0);

					paf::graph::DrawObj *bgPlane = gateDrawer->GetDrawObjByIndex(0);
					bgPlane->SetColor(s_gateDimColor);
					m_gateHasBg = true;
					fgPlane->SetVisible(true);

					if (m_style == 4) {
						fgPlane->SetSize(paf::math::v3(96.0f, 96.0f, 0.0f));
						fgPlane->SetScaleMode(
							paf::graph::PlaneObj::SCALE_SIZE,
							paf::graph::PlaneObj::SCALE_SIZE);
					} else {
						fgPlane->SetSize(paf::math::v3(0.0f, 0.0f, 0.0f));
						fgPlane->SetScaleMode(
							paf::graph::PlaneObj::SCALE_SURFACE,
							paf::graph::PlaneObj::SCALE_SURFACE);
					}

					paf::ui::EventCBListener *okListener =
						new paf::ui::EventCBListener(
							liveareaOnGateImageLoad, this);
					m_gateBtn->AddEventListener(1048581, okListener, 1);

					paf::ui::EventCBListener *errListener =
						new paf::ui::EventCBListener(
							liveareaOnGateImageLoad, this);
					m_gateBtn->AddEventListener(1048583, errListener, 1);

					paf::intrusive_ptr<paf::cache::CacheImageInterface> gateCache(
						m_gateCacheRef);
					m_gateBtn->SetDrawObjMaskBit(&gateCache, 1, 0);
				} else {
					paf::ui::EventCBListener *okListener =
						new paf::ui::EventCBListener(
							liveareaOnGateImageLoad, this);
					m_gateBtn->AddEventListener(1048581, okListener, 1);

					paf::ui::EventCBListener *errListener =
						new paf::ui::EventCBListener(
							liveareaOnGateImageLoad, this);
					m_gateBtn->AddEventListener(1048583, errListener, 1);

					paf::intrusive_ptr<paf::cache::CacheImageInterface> gateCache(
						m_gateCacheRef);
					m_gateBtn->SetDrawObjMaskBit(&gateCache, 0, 0);

					paf::graph::DrawObj *fgPlane = gateDrawer->GetDrawObjByIndex(1);
					if (fgPlane->m_flags.m_visible)
						fgPlane->m_flags.m_visible = false;
				}
			}
		}
	} else {
		paf::graph::DrawObj *fgPlane = gateDrawer->GetDrawObjByIndex(1);
		paf::intrusive_ptr<paf::cache::CacheImageInterface> emptyCache;
		m_gateBtn->SetDrawObjMaskBit(&emptyCache, 0, 0);

		paf::graph::DrawObj *bgPlane = gateDrawer->GetDrawObjByIndex(0);
		bgPlane->SetColor(s_gateDimColor);

		if (m_hasLiveSurface) {
			if (fgPlane->m_flags.m_visible)
				fgPlane->m_flags.m_visible = false;
			paf::intrusive_ptr<paf::graph::Surface> surfRef(m_gateSurface);
			m_gateBtn->SetTexture(surfRef, 0, 0);
		} else {
			m_gateHasBg = true;
			fgPlane->SetVisible(true);
			m_gateBtn->SetDrawObjMaskBit(&m_gateIconCache, 1, 0);
			paf::intrusive_ptr<paf::graph::Surface> *defaultTex =
				g_liveareaMgr->m_topmenuIf->getDefaultBgTexture();
			m_gateBtn->SetTexture(*defaultTex, 1, 1);
		}
	}

	paf::ui::Widget *gateClickWidget =
		m_gateBtn->FindChild(paf::IDParam((uint32_t)-147058763), 0);
	if (gateClickWidget != NULL) {
		paf::ui::EventCBListener *clickListener =
			new paf::ui::EventCBListener(liveareaOnGateClick,
						     (void *)m_page->m_pageKey);
		gateClickWidget->AddEventListener(268435464, clickListener, 1);
	}
	return 1;
}

/* 0x811A9742 */
int LiveAreaPageInner::SetupBackground(paf::Plugin *plugin, LiBackground *bgDesc)
{
	(void)plugin;
	paf::ui::Widget *bgWidget =
		g_liveareaMgr->m_topmenuIf->getPageTurnWidget(m_page->m_pageKey);
	if (bgWidget == NULL)
		return 1;

	paf::intrusive_ptr<paf::cache::CacheImageInterface> cacheRef;
	paf::intrusive_ptr<paf::graph::Surface> emptySurf;
	CreateContentImageCache(&bgDesc->m_image, &cacheRef);
	++m_pendingInitialLoads;

	paf::ui::EventCBListener *okListener =
		new paf::ui::EventCBListener(liveareaOnBgImageLoad, this);
	bgWidget->AddEventListener(1048581, okListener, 1);

	paf::ui::EventCBListener *errListener =
		new paf::ui::EventCBListener(liveareaOnBgImageLoad, this);
	bgWidget->AddEventListener(1048583, errListener, 1);

	bgWidget->SetTexture(emptySurf);
	bgWidget->SetDrawObjMaskBit(&cacheRef, 0);
	return 1;
}

/* 0x811A985E */
int LiveAreaPageInner::BuildLayout(int flags)
{
	if (m_shown)
		TryReloadXml();

	paf::Plugin *tmplPlugin = paf::Plugin::Find(m_page->m_templateName.c_str());
	if (tmplPlugin == NULL)
		return 0;

	if (m_contentZone != NULL) {
		m_page->m_cacheManager->MakeSpaceSurfacePool();
	} else {
		paf::ui::Widget *listItem = m_page->m_fileList->GetListItem(0, 0);
		m_contentZone = listItem->FindChild(paf::IDParam((uint32_t)-53444250), 0);
		m_gateWidget = listItem->FindChild(paf::IDParam((uint32_t)-2123959110), 0);
	}

	if (!m_templateOpened) {
		paf::Plugin::TemplateOpenParam openParam;
		tmplPlugin->TemplateOpen(m_contentZone, s_tmplContentZoneLayout, openParam);
		m_templateOpened = true;
	}

	LiTemplate *xmlDoc = m_xmlDoc.get();
	if (xmlDoc == NULL)
		return 0;

	bool hasBackgroundPart = false;
	bool builtBackground = false;
	bool builtFunctionZone = false;

	for (paf::list<LiPart *>::iterator it = xmlDoc->m_parts.begin();
	     it != xmlDoc->m_parts.end(); ++it) {
		LiPart *part = *it;
		if (part->m_type == LI_PART_BACKGROUND)
			hasBackgroundPart = true;

		if ((part->GetMask() & (unsigned int)flags) != 0) {
			switch (part->m_type) {
			case LI_PART_BACKGROUND:
				SetupBackground(tmplPlugin, (LiBackground *)part);
				builtBackground = true;
				break;
			case LI_PART_FUNCTION_ZONE:
				SetupTopBar(tmplPlugin, (LiFunctionZone *)part);
				builtFunctionZone = true;
				break;
			case LI_PART_GATE:
				SetupGate(tmplPlugin, (LiGate *)part);
				break;
			case LI_PART_FRAME: {
				LiveAreaFrameState *frameState = new LiveAreaFrameState();
				if (frameState != NULL) {
					frameState->currentIndex = -1;
					frameState->flipTimer = 0.0f;
					frameState->pendingLoads = 0;
					frameState->isSwitching = false;
					frameState->frameWidget = NULL;
					frameState->bgImageObjIndex = -1;
					frameState->imageObjIndex = -1;
					frameState->textObjIndex = -1;
					frameState->scrollMode = 0;
					frameState->scrollDeltaX = 0.0f;
					frameState->scrollDeltaY = 0.0f;
					frameState->scrollActive = false;
					frameState->scrollPhase = 0;
					frameState->scrollElapsed = 0.0f;
					frameState->scrollDuration = 0.0f;
				}
				if (SetupFrame(tmplPlugin, (LiFrame *)part, frameState) != 0)
					m_frames.push_back(frameState);
				else
					delete frameState;
				break;
			}
			case LI_PART_TITLE:
				SetupTitle(tmplPlugin, (LiTitle *)part);
				break;
			default:
				break;
			}
		}
	}

	if (!builtFunctionZone && ((unsigned int)flags & LI_MASK_FUNCTION_ZONE) != 0)
		SetupTopBar(tmplPlugin, NULL);

	if (!builtBackground && !m_shown && !hasBackgroundPart)
		SetupDefaultBackground(tmplPlugin);

	if (m_shown) {
		if (((unsigned int)flags & LI_MASK_FUNCTION_ZONE) != 0)
			m_contentZone->FindChild(paf::IDParam(1728383896U), 0);
	} else {
		for (paf::list<paf::ui::Widget *>::iterator it = m_contentZone->m_children.begin();
		     it != m_contentZone->m_children.end(); ++it) {
			paf::ui::Widget *child = *it;
			SceUInt32 childHash = child->m_name.GetIDHash();
			if (childHash != 1728383896U &&
			    childHash != s_destroyContentZoneId.GetIDHash()) {
				child->Hide(0xff, NULL, NULL, 0.0f);
			}
		}
		if (m_pendingInitialLoads == 0)
			OnInitialImagesLoaded();
	}

	if (xmlDoc->m_style == s_appMusic)
		liveareaMusicSetup(m_contentZone, (void *)m_page->m_pageKey);
	if (xmlDoc->m_style == s_appKids)
		liveareaParentalSetup(m_contentZone, (void *)m_page->m_pageKey);
	if (xmlDoc->m_style == s_appContentManager)
		liveareaCmaSetup(tmplPlugin, m_contentZone);

	return 1;
}

/* 0x811A9CCC */
void LiveAreaPageInner::OnRcoLoaded()
{
	BuildLayout(-1);
}

/* 0x811A9CE4 */
void LiveAreaPageInner::OnParseFail()
{
	if (!m_introDone) {
		m_introDone = true;
		g_liveareaMgr->m_topmenuIf->notifyIntroComplete(m_page->m_pageKey);
	}
}

/* 0x811A9D0C */
void LiveAreaPageInner::OnParseComplete(paf::common::SharedPtr<LiTemplate> *parsed, int reloadMask,
					paf::string *contentDir)
{
	if (contentDir != NULL && !contentDir->empty())
		m_baseDir = *contentDir;

	m_xmlJob.reset();

	bool delayedReset = false;
	if (m_contentZone != NULL) {
		if (reloadMask == -1) {
			g_liveareaMgr->m_topmenuIf->onContentReset(m_page->m_pageKey);
			paf::ui::EventCBListener *bgListener = new paf::ui::EventCBListener(
				liveareaOnBgClearTimer, this);
			m_contentZone->SetTimerListener(16777508, 0.0f, bgListener);
			delayedReset = true;
		}

		for (paf::list<paf::ui::Widget *>::iterator it = m_contentZone->m_children.begin();
		     it != m_contentZone->m_children.end(); ++it) {
			paf::ui::Widget *child = *it;
			SceUInt32 childHash = child->m_name.GetIDHash();

			if (childHash == 1728383896U) {
				if (reloadMask == -1) {
					for (paf::list<paf::ui::Widget *>::iterator btnIt =
						     child->m_children.begin();
					     btnIt != child->m_children.end(); ++btnIt) {
						paf::ui::Widget *btn = *btnIt;
						btn->m_name = s_destroyContentZoneId;
						paf::ui::EventCBListener *destroyListener =
							new paf::ui::EventCBListener(
								liveareaOnDestroyWidgetTimer,
								NULL);
						btn->SetTimerListener(16777510, 0.0f, destroyListener);
					}
					m_helpUri.clear();
				}
			} else if (childHash == s_gateId.GetIDHash()) {
				if (reloadMask == -1) {
					child->m_name = s_destroyContentZoneId;
					paf::ui::EventCBListener *destroyListener =
						new paf::ui::EventCBListener(
							liveareaOnDestroyWidgetTimer,
							NULL);
					child->SetTimerListener(16777510, 0.0f, destroyListener);
					ScePafWidget_A96A0B4A(child, 1);
					m_gateBtn = NULL;
				}
			} else if (childHash != s_destroyContentZoneId.GetIDHash()) {
				int frameIndex = -1;
				for (int idx = 0; idx < 10; ++idx) {
					if (s_frameIds[idx].GetIDHash() == childHash) {
						frameIndex = idx;
						break;
					}
				}
				if (frameIndex >= 0 && ((1 << frameIndex) & reloadMask) != 0) {
					for (paf::list<LiveAreaFrameState *>::iterator fIt =
						     m_frames.begin();
					     fIt != m_frames.end(); ++fIt) {
						if ((*fIt)->frameWidget == child) {
							LiveAreaFrameState *st = *fIt;
							m_frames.erase(fIt);
							delete st;
							break;
						}
					}
					if (reloadMask == -1) {
						ScePafWidget_A96A0B4A(child, 1);
						child->m_name = s_destroyContentZoneId;
						paf::ui::EventCBListener *destroyListener =
							new paf::ui::EventCBListener(
								liveareaOnDestroyWidgetTimer,
								NULL);
						child->SetTimerListener(16777510, 0.0f, destroyListener);
					}
				}
			}
		}
	}

	if ((reloadMask & 0x80000) != 0) {
		paf::ui::Widget *pageRoot =
			g_liveareaMgr->m_topmenuIf->getPageRootWidget(m_page->m_pageKey);
		if (pageRoot != NULL) {
			paf::IDParam bgWidgetId((SceUInt32)-661515378);
			paf::ui::Widget *bgWidget = pageRoot->FindChild(bgWidgetId, 0);
			if (bgWidget != NULL) {
				bgWidget->m_name = s_destroyContentZoneId;
				paf::ui::EventCBListener *destroyListener =
					new paf::ui::EventCBListener(
						liveareaOnDestroyWidgetTimer,
						NULL);
				bgWidget->SetTimerListener(16777510, 0.0f, destroyListener);
				if (reloadMask != -1) {
					paf::Timer *fadeTimer = new paf::Timer();
					fadeTimer->InitTimer(21.0f, 0, -1, 0, NULL);
					bgWidget->SetMetaAlpha(0.0f, fadeTimer, 65539, 2, 0.0f, NULL, NULL);
				}
			}
		}
		const wchar_t *pageTitle =
			g_liveareaMgr->m_topmenuIf->getPageTitle(m_page->m_pageKey);
		m_title = (pageTitle != NULL) ? pageTitle : L"";
	}

	if (m_dirty) {
		LiTemplate *oldDoc = m_xmlDoc.get();
		LiTemplate *newDoc = (parsed != NULL) ? parsed->get() : NULL;
		if (oldDoc != NULL && newDoc != NULL) {
			for (paf::list<LiPart *>::iterator it = oldDoc->m_parts.begin();
			     it != oldDoc->m_parts.end(); ++it) {
				LiPart *oldPart = *it;
				unsigned int mask = oldPart->GetMask();
				if ((mask & (unsigned int)reloadMask) != 0 && oldPart->m_type == 4) {
					LiFrame *oldFrame = (LiFrame *)oldPart;
					if (oldFrame->m_isDefault) {
						bool foundInNew = false;
						for (paf::list<LiPart *>::iterator nIt =
							     newDoc->m_parts.begin();
						     nIt != newDoc->m_parts.end(); ++nIt) {
							if ((*nIt)->GetMask() == mask) {
								foundInNew = true;
								break;
							}
						}
						if (!foundInNew) {
							LiFrame *cloned = new LiFrame();
							cloned->m_id = oldFrame->m_id;
							newDoc->m_parts.push_back(cloned);
						}
					}
				}
			}
		}
		m_dirty = false;
	}

	LiTemplate *curDoc = m_xmlDoc.get();
	if (curDoc != NULL) {
		paf::list<LiPart *>::iterator it = curDoc->m_parts.begin();
		while (it != curDoc->m_parts.end()) {
			LiPart *part = *it;
			if ((part->GetMask() & (unsigned int)reloadMask) != 0) {
				paf::list<LiPart *>::iterator nextIt = it;
				++nextIt;
				delete part;
				curDoc->m_parts.erase(it);
				it = nextIt;
			} else {
				++it;
			}
		}
	}

	if ((reloadMask & 0x100000) != 0) {
		if (m_rcoRefCount > 0) {
			LiTemplate *doc = m_xmlDoc.get();
			if (doc != NULL) {
				if (doc->m_style == s_appMusic)
					liveareaMusicCleanup(m_contentZone);
				else if (doc->m_style == s_appKids)
					liveareaParentalCleanup(m_contentZone);
				else if (doc->m_style == s_appContentManager)
					liveareaCmaCleanup(m_contentZone);
			}
			if (m_rcoRefCount > 0) {
				paf::string tmplName(m_page->m_templateName);
				liveareaUnloadRco(&tmplName);
				--m_rcoRefCount;
			}
		}

		m_templateOpened = false;
		m_xmlDoc.reset();
		m_pendingInitialLoads = 0;
		m_shown = false;
		m_initPhase = delayedReset ? 3 : 2;

		paf::string tmplName(m_page->m_templateName);
		LiTemplate *nextDoc = (parsed != NULL) ? parsed->get() : NULL;
		if (liveareaLoadRco(&nextDoc->m_style, &tmplName, m_page->m_pageKey) == 0) {
			if (!m_introDone) {
				m_introDone = true;
				g_liveareaMgr->m_topmenuIf->notifyIntroComplete(m_page->m_pageKey);
			}
			return;
		}
		++m_rcoRefCount;
	}

	if (m_initPhase <= 0) {
		LiTemplate *newDoc = (parsed != NULL) ? parsed->get() : NULL;
		if (newDoc == NULL)
			return;
		LiTemplate *oldDoc = m_xmlDoc.get();
		if (oldDoc == NULL) {
			m_xmlDoc = *parsed;
			BuildLayout(-1);
			return;
		}

		paf::list<LiPart *>::iterator oldIt = oldDoc->m_parts.begin();
		while (oldIt != oldDoc->m_parts.end()) {
			LiPart *oldPart = *oldIt;
			unsigned int oldMask = oldPart->GetMask();
			bool replaced = false;
			for (paf::list<LiPart *>::iterator newIt = newDoc->m_parts.begin();
			     newIt != newDoc->m_parts.end(); ++newIt) {
				if ((*newIt)->GetMask() == oldMask) {
					delete *newIt;
					*newIt = oldPart;
					replaced = true;
					break;
				}
			}
			if (replaced) {
				paf::list<LiPart *>::iterator nextIt = oldIt;
				++nextIt;
				oldDoc->m_parts.erase(oldIt);
				oldIt = nextIt;
			} else {
				++oldIt;
			}
		}

		m_xmlDoc = *parsed;
		BuildLayout(reloadMask);
	} else {
		m_xmlDoc = *parsed;
		if (m_initPhase > 0) {
			--m_initPhase;
			if (m_initPhase == 0)
				BuildLayout(-1);
		}
	}
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
	for (paf::list<LiPart *>::iterator it = doc->m_parts.begin();
	     it != doc->m_parts.end(); ++it) {
		LiPart *part = *it;
		if (part->m_type == LI_PART_FRAME) {
			LiFrame *frame = (LiFrame *)part;
			if (sce_paf_strncmp(frameId, frame->m_id.c_str(),
					    frame->m_id.size()) == 0) {
				*outTimestamp = (SceUInt64)frame->m_rev;
				return 0;
			}
		}
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
	paf::ui::Widget *frameWidget = m_contentZone->FindChild(frameId, 0);
	if (frameWidget == NULL)
		return 0;

	frameState->isSwitching = true;
	paf::IDParam itemId(1375437051U);
	paf::ui::Widget *itemWidget = frameWidget->FindChild(itemId, 0);
	if (itemWidget != NULL) {
		AnimatePopOut(itemWidget, liveareaOnSwitchFrameOut, this);
	} else {
		frameState->bgImageObjIndex = -1;
		frameState->imageObjIndex = -1;
		frameState->textObjIndex = -1;
		frameState->textMarginTop = 0.0f;
		frameState->textMarginBottom = 0.0f;
		frameState->textMarginLeft = 0.0f;
		frameState->textMarginRight = 0.0f;
		frameState->scrollMode = 0;
		frameState->scrollDeltaX = 0.0f;
		frameState->scrollDeltaY = 0.0f;
		frameState->scrollActive = false;
		PopulateFrame(frameDesc, frameState);
	}
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
	LiTemplate *xmlDoc = m_xmlDoc.get();
	if (xmlDoc == NULL || m_initPhase > 0 || !m_shown)
		return;

	if (m_gateBtn != NULL && !m_gateResetDone &&
	    !m_gateBtn->m_transition_in && !m_gateBtn->m_transition_out) {
		m_gateResetDone = true;
		ResetGateFocus();
	}

	if (m_style == 0) {
		paf::ui::Widget *pageRoot =
			g_liveareaMgr->m_topmenuIf->getPageRootWidget(m_page->m_pageKey);
		if (pageRoot != NULL) {
			paf::ui::Widget *titlePlane =
				pageRoot->FindChild(paf::IDParam((uint32_t)-661515378), 0);
			if (titlePlane != NULL) {
				paf::ui::Widget *body = m_page->m_fileList->GetInternalWidget();
				paf::math::v4 pos = body->GetPos(
					0, NULL, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
				float fade = (pos.extract_y() - 256.0f) * (1.0f / 256.0f);
				if (fade < 0.0f)
					fade = 0.0f;
				else if (fade > 1.0f)
					fade = 1.0f;
				titlePlane->SetMetaAlpha(1.0f - fade, NULL,
							 paf::ui::Widget::ANIM_META_ALPHA, 0,
							 0.0f, NULL, NULL);
			}
		}
	}

	bool useNetworkTime = SceLsdb_69B32F6C(m_appAttr);
	for (paf::list<LiPart *>::iterator it = xmlDoc->m_parts.begin();
	     it != xmlDoc->m_parts.end(); ++it) {
		LiPart *part = *it;
		if (part->m_type == LI_PART_FRAME) {
			LiFrame *frameDesc = (LiFrame *)part;
			paf::IDParam frameId(frameDesc->m_id);
			LiveAreaFrameState *frameState = NULL;
			for (paf::list<LiveAreaFrameState *>::iterator fIt = m_frames.begin();
			     fIt != m_frames.end(); ++fIt) {
				if (frameId.GetIDHash() ==
				    (*fIt)->frameWidget->m_name.GetIDHash()) {
					frameState = *fIt;
					break;
				}
			}
			if (frameState != NULL) {
				if (liveareaTickFrameSwitch(frameState, frameDesc, useNetworkTime))
					SwitchFrame(frameDesc, frameState);
				liveareaTickTextScroll(frameState);
			}
		}
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
	LiTemplate *xmlDoc = m_xmlDoc.get();
	if (xmlDoc == NULL)
		return 0;

	int mask = 0;
	for (paf::list<LiPart *>::iterator it = xmlDoc->m_parts.begin();
	     it != xmlDoc->m_parts.end(); ++it) {
		LiPart *part = *it;
		if (part->m_type == LI_PART_FRAME) {
			LiFrame *frame = (LiFrame *)part;
			if (frame->m_isDefault)
				mask |= (int)frame->GetMask();
		}
	}
	return mask;
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

/* 0x811ABAB4 */
int LiveAreaPageInner::LayoutFrameChild(paf::ui::Widget *widget, bool fromTextEvent)
{
	LiTemplate *xmlDoc = m_xmlDoc.get();
	for (paf::list<LiPart *>::iterator it = xmlDoc->m_parts.begin();
	     it != xmlDoc->m_parts.end(); ++it) {
		LiPart *part = *it;
		if (part->m_type == LI_PART_FRAME) {
			LiFrame *frameDesc = (LiFrame *)part;
			paf::IDParam frameId(frameDesc->m_id);
			paf::ui::Widget *frameWidget = m_contentZone->FindChild(frameId, 0);
			if (frameWidget != NULL) {
				paf::IDParam itemId(1375437051U);
				paf::ui::Widget *itemWidget = frameWidget->FindChild(itemId, 0);
				if (itemWidget != NULL && itemWidget->FindChild(widget, 0) != NULL) {
					LiveAreaFrameState *frameState = NULL;
					for (paf::list<LiveAreaFrameState *>::iterator fIt =
						     m_frames.begin();
					     fIt != m_frames.end(); ++fIt) {
						if (frameId.GetIDHash() ==
						    (*fIt)->frameWidget->m_name.GetIDHash()) {
							frameState = *fIt;
							break;
						}
					}
					if (frameState != NULL) {
						LiLiveItem *curItem = SceLsdb_92D3A2EA(
							frameDesc, frameState->currentIndex);
						return liveareaLayoutLiveItem(
							itemWidget, curItem, frameState,
							m_shown, this, fromTextEvent);
					}
				}
			}
		}
	}
	return 0;
}

/* 0x811ABC98 */
LiveAreaFrameState *LiveAreaPageInner::FindFrameState(paf::ui::Widget *widget, LiFrame **outDesc)
{
	LiTemplate *xmlDoc = m_xmlDoc.get();
	for (paf::list<LiPart *>::iterator it = xmlDoc->m_parts.begin();
	     it != xmlDoc->m_parts.end(); ++it) {
		LiPart *part = *it;
		if (part->m_type == LI_PART_FRAME) {
			LiFrame *frameDesc = (LiFrame *)part;
			paf::IDParam frameId(frameDesc->m_id);
			paf::ui::Widget *frameWidget = m_contentZone->FindChild(frameId, 0);
			if (frameWidget != NULL) {
				paf::IDParam itemId(1375437051U);
				paf::ui::Widget *itemWidget = frameWidget->FindChild(itemId, 0);
				if (itemWidget != NULL && itemWidget->FindChild(widget, 0) != NULL) {
					LiveAreaFrameState *found = NULL;
					for (paf::list<LiveAreaFrameState *>::iterator fIt =
						     m_frames.begin();
					     fIt != m_frames.end(); ++fIt) {
						if (frameId.GetIDHash() ==
						    (*fIt)->frameWidget->m_name.GetIDHash()) {
							found = *fIt;
							break;
						}
					}
					if (outDesc != NULL)
						*outDesc = frameDesc;
					return found;
				}
			}
		}
	}
	return NULL;
}

/* 0x811ABE1C */
static void liveareaOnPatchClick(int eventId, paf::ui::Handler *handler,
				 paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)handler; (void)event;
	LiveAreaPageInner *self = (LiveAreaPageInner *)userdata;
	g_liveareaMgr->m_topmenuIf->onPatchIconClick(self->m_page->m_pageKey);
}

/* 0x811ABE34 */
static void liveareaOnSearchClick(int eventId, paf::ui::Handler *handler,
				  paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)handler; (void)event;
	LiveAreaPageInner *self = (LiveAreaPageInner *)userdata;
	if (g_liveareaMgr->m_topmenuIf->isPageInteractive(self->m_page->m_pageKey) != 0) {
		char uri[1024];
		sce_paf_snprintf(uri, sizeof(uri), "%s%s", "search:", self->m_searchKey.c_str());
		sceAppMgrLaunchAppByUri(0x20000, uri);
	}
}

/* 0x811ABEA4 */
static void liveareaOnOnlineHelpClick(int eventId, paf::ui::Handler *handler,
				      paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)handler; (void)event;
	LiveAreaPageInner *self = (LiveAreaPageInner *)userdata;
	if (g_liveareaMgr->m_topmenuIf->isPageInteractive(self->m_page->m_pageKey) != 0)
		sceAppMgrLaunchAppByUri(0x20000, self->m_helpUri.c_str());
}

/* 0x811ABECE */
static void liveareaOnHelpClick(int eventId, paf::ui::Handler *handler,
				paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)handler; (void)event;
	LiveAreaPageInner *self = (LiveAreaPageInner *)userdata;
	g_liveareaMgr->m_topmenuIf->onManualIconClick(self->m_page->m_pageKey);
}

/* 0x811ABEE6 */
static void liveareaOnVoucherClick(int eventId, paf::ui::Handler *handler,
				   paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)handler; (void)event;
	LiveAreaPageInner *self = (LiveAreaPageInner *)userdata;
	if (g_liveareaMgr->m_topmenuIf->isPageInteractive(self->m_page->m_pageKey) != 0)
		sceAppMgrLaunchAppByUri(0x20000, "psns:redeem");
}

/* 0x811ABF14 */
static void liveareaOnReloadClick(int eventId, paf::ui::Handler *handler,
				  paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)handler; (void)event;
	LiveAreaPageInner *self = (LiveAreaPageInner *)userdata;
	if (self->m_reloadCount <= 0 && self->m_xmlDoc.get() != NULL)
		liveareaMgrUpdatePage(self, 1);
}

/* 0x811ABF2E */
static void liveareaOnImportSaveClick(int eventId, paf::ui::Handler *handler,
				      paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)handler; (void)event;
	LiveAreaPageInner *self = (LiveAreaPageInner *)userdata;
	g_liveareaMgr->m_topmenuIf->onImportSavedataIconClick(self->m_page->m_pageKey);
}

/* 0x811ABF46 */
static void liveareaOnOnlineStorageClick(int eventId, paf::ui::Handler *handler,
					 paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)handler; (void)event;
	LiveAreaPageInner *self = (LiveAreaPageInner *)userdata;
	g_liveareaMgr->m_topmenuIf->onOnlineStorageIconClick(self->m_page->m_pageKey);
}

/* 0x811ABF5E */
static void liveareaOnItemUriClick(int eventId, paf::ui::Handler *handler,
				   paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)handler; (void)event;
	LiLiveItem *item = (LiLiveItem *)userdata;
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
					  paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)handler; (void)event;
	LiLiveItem *item = (LiLiveItem *)userdata;
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
					consoleInfoFileAddInt(228, 1, 0);
				else if (sce_paf_strncmp("psns:", uriStr, 5) == 0 ||
					 sce_paf_strncmp("psts:", uriStr, 5) == 0)
					consoleInfoFileAddInt(227, 1, 0);
				else if (sce_paf_strncmp("psgm:", uriStr, 5) == 0)
					consoleInfoFileAddInt(229, 1, 0);
			}
			sceAppMgrLaunchAppByUri(0x20000, uriStr);
		}
	}
}

/* 0x811AC0F4 */
static void liveareaOnItemAppClick(int eventId, paf::ui::Handler *handler,
				   paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)event;
	paf::ui::Widget *widget = (paf::ui::Widget *)handler;
	LiLiveItem *item = (LiLiveItem *)userdata;
	if (item != NULL) {
		LiTarget *target = item->m_target;
		if (target != NULL && !target->m_uri.empty()) {
			paf::string launchParam;
			const char *appName = (const char *)widget->m_user_data[0];
			liveareaReplaceOnlineId(launchParam, target->m_uri);
			sceAppMgrLaunchAppByNameForShell(0x20000, appName, launchParam.c_str());
		}
	}
}

/* 0x811AC156 */
static void liveareaOnItemPslaClick(int eventId, paf::ui::Handler *handler,
				    paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)event;
	paf::ui::Widget *widget = (paf::ui::Widget *)handler;
	LiLiveItem *item = (LiLiveItem *)userdata;
	if (item != NULL) {
		LiTarget *target = item->m_target;
		if (target != NULL && !target->m_uri.empty()) {
			const char *appName = (const char *)widget->m_user_data[0];
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
				paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)handler; (void)event;
	g_liveareaMgr->m_topmenuIf->onGateClick((int)userdata);
}

/* 0x811AC256 */
static void liveareaOnItemPress(int eventId, paf::ui::Handler *handler,
				paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)event; (void)userdata;
	paf::ui::Widget *widget = (paf::ui::Widget *)handler;
	paf::math::v4 dimColor(0.5f, 0.5f, 0.5f, 1.0f);
	for (paf::list<paf::ui::Widget *>::iterator it = widget->m_children.begin();
	     it != widget->m_children.end(); ++it) {
		(*it)->SetColor(dimColor, NULL, paf::ui::Widget::ANIM_COLOR, 0, 0.0f, NULL, NULL);
	}
}

/* 0x811AC2D8 */
static void liveareaOnItemRelease(int eventId, paf::ui::Handler *handler,
				  paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)event; (void)userdata;
	paf::ui::Widget *widget = (paf::ui::Widget *)handler;
	paf::math::v4 normalColor(1.0f, 1.0f, 1.0f, 1.0f);
	for (paf::list<paf::ui::Widget *>::iterator it = widget->m_children.begin();
	     it != widget->m_children.end(); ++it) {
		(*it)->SetColor(normalColor, NULL, paf::ui::Widget::ANIM_COLOR, 0, 0.0f,
				NULL, NULL);
	}
}

/* 0x811AC34C */
static void liveareaOnGateImageLoad(int eventId, paf::ui::Handler *handler,
				    paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)event;
	LiveAreaPageInner *self = (LiveAreaPageInner *)userdata;
	paf::ui::Widget *widget = (paf::ui::Widget *)handler;
	if (self->m_shown) {
		LiveAreaFrameState *frameState = self->FindFrameState(widget, NULL);
		if (frameState != NULL && frameState->pendingLoads != 0) {
			--frameState->pendingLoads;
			if (frameState->pendingLoads == 0)
				self->LayoutFrameChild(widget, false);
		}
	} else if (self->m_pendingInitialLoads != 0) {
		--self->m_pendingInitialLoads;
		if (self->m_pendingInitialLoads == 0)
			self->OnInitialImagesLoaded();
	}
	handler->DeleteEventCallback(1048581, liveareaOnGateImageLoad, userdata);
	handler->DeleteEventCallback(1048583, liveareaOnGateImageLoad, userdata);
}

/* 0x811AC3BA */
static void liveareaOnFrameImageLoad(int eventId, paf::ui::Handler *handler,
				     paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)event;
	LiveAreaPageInner *self = (LiveAreaPageInner *)userdata;
	paf::ui::Widget *widget = (paf::ui::Widget *)handler;
	if (self->m_shown) {
		LiveAreaFrameState *frameState = self->FindFrameState(widget, NULL);
		if (frameState != NULL && frameState->pendingLoads != 0) {
			--frameState->pendingLoads;
			if (frameState->pendingLoads == 0)
				self->LayoutFrameChild(widget, false);
		}
	} else if (self->m_pendingInitialLoads != 0) {
		--self->m_pendingInitialLoads;
		if (self->m_pendingInitialLoads == 0)
			self->OnInitialImagesLoaded();
	}
}

/* 0x811AC400 */
static void liveareaOnItemTextInit(int eventId, paf::ui::Handler *handler,
				   paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)event;
	LiveAreaPageInner *self = (LiveAreaPageInner *)userdata;
	paf::ui::Widget *widget = (paf::ui::Widget *)handler;
	handler->DeleteEventCallback(1048580, liveareaOnItemTextInit, userdata);

	paf::ui::EventCBListener *layoutListener =
		new paf::ui::EventCBListener(liveareaOnItemTextLayout, userdata);
	handler->AddEventListener(1048580, layoutListener, 1);

	if (self->m_shown) {
		LiveAreaFrameState *frameState = self->FindFrameState(widget, NULL);
		if (frameState != NULL && frameState->pendingLoads == 0)
			self->LayoutFrameChild(widget, true);
	} else if (self->m_pendingInitialLoads == 0) {
		if (self->LayoutFrameChild(widget, true) != 0)
			widget->Show(0xff, NULL, NULL, 0.0f);
	}
}

/* 0x811AC4DE */
static void liveareaOnBgImageLoad(int eventId, paf::ui::Handler *handler,
				  paf::ui::Event *event, void *userdata)
{
	(void)event;
	LiveAreaPageInner *self = (LiveAreaPageInner *)userdata;
	paf::ui::Widget *widget = (paf::ui::Widget *)handler;
	if (eventId == 1048581) {
		if (!self->m_shown && self->m_pendingInitialLoads != 0) {
			--self->m_pendingInitialLoads;
			if (self->m_pendingInitialLoads == 0)
				self->OnInitialImagesLoaded();
		}
	} else {
		paf::ui::EventCBListener *okListener =
			new paf::ui::EventCBListener(liveareaOnDefaultBgImageLoad, userdata);
		handler->AddEventListener(1048581, okListener, 1);

		paf::ui::EventCBListener *errListener =
			new paf::ui::EventCBListener(liveareaOnDefaultBgImageLoad, userdata);
		handler->AddEventListener(1048583, errListener, 1);

		paf::intrusive_ptr<paf::cache::CacheImageInterface> safeCache;
		paf::string safeBgPath("vs0:data/internal/livearea/safebg.png");
		self->m_page->CreateImageCache(safeBgPath, safeCache);
		widget->SetDrawObjMaskBit(&safeCache, 0);
	}
	handler->DeleteEventCallback(1048581, liveareaOnBgImageLoad, userdata);
	handler->DeleteEventCallback(1048583, liveareaOnBgImageLoad, userdata);
}

/* 0x811AC652 */
static void liveareaOnDefaultBgImageLoad(int eventId, paf::ui::Handler *handler,
					 paf::ui::Event *event, void *userdata)
{
	(void)event;
	LiveAreaPageInner *self = (LiveAreaPageInner *)userdata;
	paf::ui::Widget *widget = (paf::ui::Widget *)handler;
	if (!self->m_shown && self->m_pendingInitialLoads != 0) {
		--self->m_pendingInitialLoads;
		if (self->m_pendingInitialLoads == 0)
			self->OnInitialImagesLoaded();
	}
	if (eventId == 1048583) {
		paf::intrusive_ptr<paf::graph::Surface> fallbackTex =
			paf::Framework::Instance()->GetCommonTexture(paf::IDParam(1711163699U));
		widget->SetTexture(fallbackTex);
	}
	handler->DeleteEventCallback(1048581, liveareaOnDefaultBgImageLoad, userdata);
	handler->DeleteEventCallback(1048583, liveareaOnDefaultBgImageLoad, userdata);
}

/* 0x811AC750 */
static void liveareaOnGateTextLayout(int eventId, paf::ui::Handler *handler,
				     paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)event; (void)userdata;
	paf::ui::CompositeDrawer *drawer = (paf::ui::CompositeDrawer *)handler;
	paf::graph::TextObj *textObj = (paf::graph::TextObj *)drawer->GetDrawObjByIndex(3);
	paf::graph::DrawObj *bgObj = drawer->GetDrawObjByIndex(2);
	if (textObj == NULL || bgObj == NULL)
		return;

	paf::math::v2 bounds(0.0f, 0.0f);
	if (textObj->GetBounds(bounds) != 0)
		return;

	float width = bounds.extract_x() + 32.0f;
	if (width < 136.0f)
		width = 136.0f;
	bgObj->SetSize(paf::math::v3(width, bgObj->m_size.extract_y(), 0.0f));

	paf::graph::TextLayoutLineInfo lineInfo;
	if (textObj->GetLineInfo(0, lineInfo) == 0) {
		float centeredY =
			bgObj->m_pos.extract_y() +
			((lineInfo.descender + lineInfo.ascender) - lineInfo.frame.extract_w()) *
				0.5f;
		float snappedY = (float)(int)centeredY + 1.0f;
		textObj->SetPos(paf::math::v3(bgObj->m_pos.extract_x(), snappedY,
					      bgObj->m_pos.extract_z()));
	}
}

/* 0x811AC8AA */
static void liveareaOnItemTextLayout(int eventId, paf::ui::Handler *handler,
				     paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)event;
	LiveAreaPageInner *self = (LiveAreaPageInner *)userdata;
	self->LayoutFrameChild((paf::ui::Widget *)handler, true);
}

/* 0x811AC8B6 */
static void liveareaOnIntroTimer(int eventId, paf::ui::Handler *handler,
				 paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)event;
	LiveAreaPageInner *self = (LiveAreaPageInner *)userdata;
	self->m_introDone = true;
	handler->KillTimer(16777507);
	g_liveareaMgr->m_topmenuIf->notifyIntroComplete(self->m_page->m_pageKey);
	self->ResetGateFocus();
}

/* 0x811AC8EC */
static void liveareaOnFocusTimer(int eventId, paf::ui::Handler *handler,
				 paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)event;
	LiveAreaPageInner *self = (LiveAreaPageInner *)userdata;
	self->m_page->OnFocusComplete();
	handler->KillTimer(16777509);
}

/* 0x811AC906 */
static void liveareaOnBgClearTimer(int eventId, paf::ui::Handler *handler,
				   paf::ui::Event *event, void *userdata)
{
	(void)eventId; (void)event;
	LiveAreaPageInner *self = (LiveAreaPageInner *)userdata;
	if (self->m_initPhase > 0) {
		--self->m_initPhase;
		if (self->m_initPhase == 0)
			self->BuildLayout(-1);
	}
	handler->KillTimer(16777508);
}

/* 0x811AC9A8 */
static void liveareaOnSwitchFrameOut(paf::Timer *timer, paf::ui::AnimListener *listener,
				     void *widgetPtr, void *userdata, void *unused)
{
	(void)timer; (void)listener; (void)unused;
	paf::ui::Widget *widget = (paf::ui::Widget *)widgetPtr;
	LiveAreaPageInner *self = (LiveAreaPageInner *)userdata;

	LiFrame *frameDesc = NULL;
	LiveAreaFrameState *frameState = self->FindFrameState(widget, &frameDesc);
	widget->m_name = s_destroyContentZoneId;
	widget->DestroyWidget();

	if (frameState != NULL) {
		frameState->bgImageObjIndex = -1;
		frameState->imageObjIndex = -1;
		frameState->scrollActive = false;
		frameState->textObjIndex = -1;
		frameState->scrollDeltaX = 0.0f;
		frameState->scrollDeltaY = 0.0f;
		frameState->textMarginTop = 0.0f;
		frameState->textMarginBottom = 0.0f;
		frameState->textMarginLeft = 0.0f;
		frameState->textMarginRight = 0.0f;
		frameState->scrollMode = 0;
		self->PopulateFrame(frameDesc, frameState);
	}
}

/* 0x811ACA32 */
static void liveareaOnReloadFrameOut(paf::Timer *timer, paf::ui::AnimListener *listener,
				     void *widgetPtr, void *userdata, void *unused)
{
	(void)timer; (void)listener; (void)unused;
	paf::ui::Widget *widget = (paf::ui::Widget *)widgetPtr;
	LiveAreaPageInner *self = (LiveAreaPageInner *)userdata;
	paf::ui::Widget *parentFrame = widget->m_parent;

	LiveAreaFrameState *frameState = NULL;
	for (paf::list<LiveAreaFrameState *>::iterator fIt = self->m_frames.begin();
	     fIt != self->m_frames.end(); ++fIt) {
		if (parentFrame->m_name.GetIDHash() ==
		    (*fIt)->frameWidget->m_name.GetIDHash()) {
			frameState = *fIt;
			break;
		}
	}

	widget->m_name = s_destroyContentZoneId;
	widget->DestroyWidget();

	if (frameState != NULL && frameState->pendingLoads != 0) {
		--frameState->pendingLoads;
		if (frameState->pendingLoads == 0) {
			paf::ui::Widget *nextItem =
				parentFrame->FindChild(paf::IDParam(1375437051U), 0);
			if (nextItem != NULL)
				self->LayoutFrameChild(nextItem, false);
		}
	}
}

/* 0x811ACB34 */
static void liveareaOnDestroyWidgetTimer(int eventId, paf::ui::Handler *handler,
					 paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)event;
	(void)userdata;
	paf::ui::Widget *widget = (paf::ui::Widget *)handler;
	if (widget == NULL)
		return;
	paf::intrusive_ptr<paf::ui::TextureSource> emptySource;
	paf::intrusive_ptr<paf::graph::Surface> emptySurf;
	widget->SetDrawObjMaskBit(&emptySource, 0, 0);
	widget->SetTexture(emptySurf);
	widget->m_name = s_destroyContentZoneId;
	widget->DestroyWidget();
}

/* 0x811ACBAE */
static bool liveareaCompareFunctions(const LiFunction &lhs, const LiFunction &rhs)
{
	return lhs.func < rhs.func;
}
