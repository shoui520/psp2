/*
 * whats_new.cpp: LiveArea Home "What's New" promotional banner manager and
 * NSX material refresh job (0x81282C3C-0x8128430B).
 */
#include "topmenu_internal.h"

namespace {

static const SceUInt32 WHATSNEW_TEMPLATE_ID = 0x75B23771u;	/* 1974612145 */
static const SceUInt32 WHATSNEW_PANEL_ID = 0x7D7466E2u;		/* 2104774082 */
static const SceUInt32 WHATSNEW_DEFAULT_TEX_ID = 0xAFDF05D0u;	/* -1344341552 */
static const SceUInt32 WHATSNEW_SURFACE_MEM_SIZE = 393216u;

static const SceUInt32 kWhatsNewBannerWidgetIds[3] = {
	0x927FC26Cu,
	0x78AF35E7u,
	0x1BE11CF2u
};

static const paf::math::v4 kWhatsNewShowPos(0.0f, -256.0f, 0.0f, 0.0f);
static const paf::math::v4 kWhatsNewHidePos(0.0f, -341.0f, 0.0f, 0.0f);

static void showWidgetWithTimer(paf::ui::Widget *widget, paf::Timer *timer)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xFF;
		widget->m_transition_complete = true;
		widget->DoTransition_core(1.0f, true);
	}
	ScePafWidget_FF5BC957(widget, timer, 2, 0.0f, NULL);
}

static void hideWidgetWithTimer(paf::ui::Widget *widget, paf::Timer *timer,
				paf::ui::AnimCB cb)
{
	widget->m_transition_complete = false;
	widget->m_transition = 0xFF;
	widget->DoTransitionReverse_core(1.0f, true);
	ScePafWidget_4588699E(widget, timer, 2, 0.0f, cb);
}

static paf::Timer *createWhatsNewTimer(float durationMs, paf::Timer::Func funcType)
{
	paf::Timer *timer = new paf::Timer();
	if (timer != NULL)
		timer->InitTimer(durationMs, funcType, paf::Timer::REPEAT_NONE, -1, NULL);
	return timer;
}

static void onWhatsNewBannerClicked(int32_t eventType, paf::ui::Handler *self,
				    paf::ui::Event *event, void *userData);
static void onWhatsNewHideAnimFinished(paf::Timer *timer, paf::ui::AnimListener *listener,
				       void *targetWidget, void *userData, void *extraData);
static void onNsxDataUpdated(void *userData);
static int enqueueGetNsxObjJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
			       bool detached);

/* Single promotional banner slot (28 bytes) */
struct TopmenuWhatsNewBanner {
	TopmenuWhatsNewBanner()
		: targetUri(),
		  imageCache(),
		  valid(false),
		  materialId(0),
		  minAge(0),
		  maxAge(255)
	{
	}

	paf::string targetUri;						/* +0x00 */
	paf::intrusive_ptr<paf::cache::CacheImageInterface> imageCache;	/* +0x0C */
	bool valid;							/* +0x10 */
	SceInt32 materialId;						/* +0x14 */
	SceInt16 minAge;						/* +0x18 */
	SceInt16 maxAge;						/* +0x1A */
};

/* Background job loading NSX material entries (36 bytes, 0x81282C3C) */
class GetNsxObjJob : public paf::job::JobItem {
public:
	GetNsxObjJob(bool detached)
		: paf::job::JobItem("GetNsxObjJob", NULL),
		  m_materials(NULL),
		  m_detached(detached)
	{
	}

	/* 0x81282CBE / 0x81282CE8 */
	virtual ~GetNsxObjJob()
	{
		if (m_materials != NULL) {
			nsxFreeMaterialList(m_materials);
			delete m_materials;
		}
	}

	/* 0x81282D18 */
	virtual int32_t Run();

	/* 0x812832FE */
	virtual void Finish(int32_t result);

	paf::vector<NsxMaterialItem *> *m_materials;
	bool m_detached;
};

} /* namespace */

/* TopmenuWhatsNewMgr (144 bytes, 0x81283326) */
struct TopmenuWhatsNewMgr {
	TopmenuWhatsNewMgr();
	~TopmenuWhatsNewMgr();

	void ClearActiveJob();
	bool AssignBanner(TopmenuWhatsNewBanner *banner, const NsxMaterialItem *material);
	void ClearBanners();
	void SelectBanners();
	void ApplyLoadedMaterials(paf::vector<NsxMaterialItem *> *materials);
	void Show();
	void Hide();
	void OnHideFinished();
	void CheckRefresh();
	void OnAgeChanged();

	void *m_surfaceMem;						/* +0x00 */
	paf::graph::SurfacePool *m_surfacePool;				/* +0x04 */
	paf::cache::CacheManager *m_cacheManager;			/* +0x08 */
	bool m_enabled;							/* +0x0C */
	paf::ui::Widget *m_panelWidget;					/* +0x10 */
	TopmenuWhatsNewBanner m_banners[3];				/* +0x14 */
	NsxWhatsNewAccessor m_accessor;					/* +0x68 */
	paf::vector<NsxMaterialItem *> *m_currentMaterials;		/* +0x6C */
	paf::vector<NsxMaterialItem *> *m_pendingMaterials;		/* +0x70 */
	SceUInt32 m_rotationIndex;					/* +0x74 */
	paf::rtc::Tick m_nextRefreshTick;				/* +0x78 */
	paf::common::SharedPtr<paf::job::JobItem> m_activeJob;		/* +0x80 */
	SceUInt64 m_showStartTimeUs;					/* +0x88 */
};

TopmenuWhatsNewMgr *g_topmenuWhatsNewMgr;

namespace {

/* 0x81282C3C */
static int enqueueGetNsxObjJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
			       bool detached)
{
	paf::common::SharedPtr<paf::job::JobItem> job(new GetNsxObjJob(detached));
	*outJob = job;
	return paf::job::JobQueue::default_queue->Enqueue(job);
}

/* 0x81282D18 */
int32_t GetNsxObjJob::Run()
{
	m_materials = new paf::vector<NsxMaterialItem *>();
	if (g_topmenuWhatsNewMgr == NULL)
		return (SceInt32)0x80109400u;

	paf::rtc::Tick refreshTick = 0;
	int result = g_topmenuWhatsNewMgr->m_accessor.LoadMaterials(m_materials, &refreshTick);
	if (result == 0) {
		paf::rtc::Tick nowTick = 0;
		paf::rtc::Tick oneDayAhead = 0;
		if (SceLsdb_D8DF91EE(&nowTick) == 0 &&
		    paf::rtc::TickAddDays(&oneDayAhead, &nowTick, 1) == 0 &&
		    oneDayAhead > refreshTick) {
			refreshTick = oneDayAhead;
		}
		g_topmenuWhatsNewMgr->m_nextRefreshTick = refreshTick;
	}
	return result;
}

/* 0x812832FE */
void GetNsxObjJob::Finish(int32_t result)
{
	if (result == 0) {
		if (g_topmenuWhatsNewMgr != NULL)
			g_topmenuWhatsNewMgr->ApplyLoadedMaterials(m_materials);
		m_materials = NULL;
	}
	if (!m_detached && g_topmenuWhatsNewMgr != NULL)
		g_topmenuWhatsNewMgr->ClearActiveJob();
}

/* 0x81283DB6 */
static void onWhatsNewBannerClicked(int32_t eventType, paf::ui::Handler *self,
				    paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)self;
	(void)event;
	if (g_topmenuWhatsNewMgr == NULL)
		return;

	SceInt32 slotIndex = (SceInt32)(intptr_t)userData;
	TopmenuWhatsNewBanner &banner = g_topmenuWhatsNewMgr->m_banners[slotIndex];
	if (!banner.valid || banner.imageCache.get() == NULL || banner.targetUri.empty())
		return;

	const char *uri = banner.targetUri.c_str();
	if (sce_paf_strncmp(uri, "psns:", sce_paf_strlen("psns:")) != 0) {
		sceAppMgrLaunchAppByUri(0x20000u, uri);
	} else {
		char fullUri[1024];
		sce_paf_snprintf(fullUri, sizeof(fullUri),
				 "%s&_vita_internal_=whatsnew&matid=%d&chancol=%d",
				 uri, banner.materialId, slotIndex);
		sceAppMgrLaunchAppByUri(0x20000u, fullUri);
	}
}

/* 0x81283E6C */
static void onWhatsNewHideAnimFinished(paf::Timer *timer, paf::ui::AnimListener *listener,
				       void *targetWidget, void *userData, void *extraData)
{
	(void)timer;
	(void)listener;
	(void)targetWidget;
	(void)userData;
	(void)extraData;
	if (g_topmenuWhatsNewMgr != NULL)
		g_topmenuWhatsNewMgr->OnHideFinished();
}

/* 0x81283EB4 */
static void onNsxDataUpdated(void *userData)
{
	(void)userData;
	if (g_topmenuWhatsNewMgr != NULL && g_topmenuWhatsNewMgr->m_activeJob.get() == NULL) {
		paf::common::SharedPtr<paf::job::JobItem> job;
		enqueueGetNsxObjJob(&job, false);
		g_topmenuWhatsNewMgr->m_activeJob = job;
	}
}

/* 0x81283FCC */
static void triggerDetachedNsxRefresh(void)
{
	paf::common::SharedPtr<paf::job::JobItem> job;
	enqueueGetNsxObjJob(&job, true);
}

static bool isMaterialEligible(const NsxMaterialItem *material, paf::rtc::Tick nowTick,
			       int userAge)
{
	if (material->validFromTick > nowTick || material->validUntilTick < nowTick)
		return false;
	if (userAge >= 0 && (material->minAge > userAge || material->maxAge < userAge))
		return false;
	return true;
}

} /* namespace */

/* 0x81282DC4 */
void TopmenuWhatsNewMgr::ClearActiveJob()
{
	m_activeJob.reset();
}

/* 0x81282E3C */
bool TopmenuWhatsNewMgr::AssignBanner(TopmenuWhatsNewBanner *banner,
				      const NsxMaterialItem *material)
{
	if (material->iconUrl.empty())
		return false;

	m_cacheManager->CreateCache(material->iconUrl, banner->imageCache, NULL, NULL, NULL);

	if (material->targetType == NSX_TARGET_URI) {
		banner->targetUri = material->targetUri;
	} else if (material->targetType == NSX_TARGET_PRODUCT) {
		banner->targetUri = paf::string("psns:browse?product=") + material->targetUri;
	} else if (material->targetType == NSX_TARGET_CATEGORY) {
		banner->targetUri = paf::string("psns:browse?category=") + material->targetUri;
	}

	banner->materialId = material->materialId;
	banner->minAge = material->minAge;
	banner->maxAge = material->maxAge;
	banner->valid = true;
	return true;
}

/* 0x81282F7C */
void TopmenuWhatsNewMgr::ClearBanners()
{
	for (SceUInt32 idx = 0; idx < 3; ++idx) {
		m_banners[idx].imageCache.clear();
		m_banners[idx].materialId = -1;
		m_banners[idx].targetUri.clear();
		m_banners[idx].valid = false;
	}
}

/* 0x81282FCE */
void TopmenuWhatsNewMgr::SelectBanners()
{
	paf::rtc::Tick nowTick = 0;
	int tickErr = SceLsdb_D8DF91EE(&nowTick);
	ClearBanners();

	if (m_currentMaterials == NULL || tickErr != 0)
		return;

	int userAge = SceLsdb_1673AD48();
	SceUInt32 totalCount = (SceUInt32)m_currentMaterials->size();
	if (totalCount == 0)
		return;

	if (m_rotationIndex >= totalCount)
		m_rotationIndex = 0;

	SceUInt32 filled = 0;

	/* Pass 1: emergency banners */
	for (SceUInt32 i = 0; i < totalCount && filled < 3; ++i) {
		NsxMaterialItem *material = (*m_currentMaterials)[i];
		if (nsxMaterialIsEmergency(material) &&
		    isMaterialEligible(material, nowTick, userAge) &&
		    AssignBanner(&m_banners[filled], material)) {
			++filled;
		}
	}
	if (filled >= 3)
		return;

	/* Pass 2: picks banners */
	for (SceUInt32 i = 0; i < totalCount && filled < 3; ++i) {
		NsxMaterialItem *material = (*m_currentMaterials)[i];
		if (nsxMaterialIsPicks(material) &&
		    isMaterialEligible(material, nowTick, userAge) &&
		    AssignBanner(&m_banners[filled], material)) {
			++filled;
		}
	}
	if (filled >= 3)
		return;

	/* Pass 3: regular banners starting from m_rotationIndex wrapping around */
	SceUInt32 startIndex = m_rotationIndex;
	while (m_rotationIndex < totalCount) {
		NsxMaterialItem *material = (*m_currentMaterials)[m_rotationIndex];
		++m_rotationIndex;
		if (!nsxMaterialIsEmergency(material) && !nsxMaterialIsPicks(material) &&
		    isMaterialEligible(material, nowTick, userAge) &&
		    AssignBanner(&m_banners[filled], material)) {
			++filled;
			if (filled >= 3)
				return;
		}
	}

	m_rotationIndex = 0;
	while (m_rotationIndex < startIndex) {
		NsxMaterialItem *material = (*m_currentMaterials)[m_rotationIndex];
		++m_rotationIndex;
		if (!nsxMaterialIsEmergency(material) && !nsxMaterialIsPicks(material) &&
		    isMaterialEligible(material, nowTick, userAge) &&
		    AssignBanner(&m_banners[filled], material)) {
			++filled;
			if (filled >= 3)
				return;
		}
	}
}

/* 0x812832BC */
void TopmenuWhatsNewMgr::ApplyLoadedMaterials(paf::vector<NsxMaterialItem *> *materials)
{
	if (materials == NULL)
		return;

	if (m_pendingMaterials != NULL && m_pendingMaterials != materials) {
		nsxFreeMaterialList(m_pendingMaterials);
		delete m_pendingMaterials;
	}
	m_pendingMaterials = materials;

	if (m_panelWidget == NULL) {
		paf::vector<NsxMaterialItem *> *oldMaterials = m_currentMaterials;
		m_currentMaterials = materials;
		m_pendingMaterials = NULL;
		m_rotationIndex = 0;
		if (oldMaterials != NULL) {
			nsxFreeMaterialList(oldMaterials);
			delete oldMaterials;
		}
		SelectBanners();
	}
}

/* 0x81283326 */
TopmenuWhatsNewMgr::TopmenuWhatsNewMgr()
	: m_surfaceMem(NULL),
	  m_surfacePool(NULL),
	  m_cacheManager(NULL),
	  m_enabled(true),
	  m_panelWidget(NULL),
	  m_accessor(),
	  m_currentMaterials(NULL),
	  m_pendingMaterials(NULL),
	  m_rotationIndex(0),
	  m_nextRefreshTick(0),
	  m_activeJob(),
	  m_showStartTimeUs(0)
{
	int memType = (paf::system::Model::GetType() == paf::system::Model::Type_Dolce) ? 0 : 1;
	m_surfaceMem = paf::GraphMem::AllocOSMemory(
		(paf::GraphMem::DeviceType)memType, WHATSNEW_SURFACE_MEM_SIZE,
		"SceShellWhatsNewSurface", NULL);
	m_surfacePool = new paf::graph::SurfacePool(
		m_surfaceMem, WHATSNEW_SURFACE_MEM_SIZE, "SceShellWhatsNewSurface", NULL,
		paf::graph::SurfacePool::MemoryType_Normal);

	paf::cache::CacheManager::Params params;
	params.name = paf::string("WhatsNew");
	paf::memory::HeapAllocator *decodeHeap = paf::Framework::Instance()->GetDecodeHeap();
	params.longlife_heap = decodeHeap;
	params.instant_heap = decodeHeap;
	params.cache_instance_heap = NULL;
	params.surface_pool = m_surfacePool;
	params.cache_longlife_heap = false;
	params.image_cache_divide_thread = true;
	m_cacheManager = new paf::cache::CacheManager(&params);

	m_accessor.Init(onNsxDataUpdated, NULL);

	paf::common::SharedPtr<paf::job::JobItem> job;
	enqueueGetNsxObjJob(&job, false);
	m_activeJob = job;

	int showOperatorInfo = 0;
	if (sceRegMgrGetKeyIntForUser("/CONFIG/TEL", "show_operator_info", &showOperatorInfo) == 0 &&
	    showOperatorInfo == 0) {
		m_enabled = false;
	}
}

/* 0x812835D6 */
TopmenuWhatsNewMgr::~TopmenuWhatsNewMgr()
{
	m_accessor.Terminate();
	ClearBanners();

	m_cacheManager->RequestShutdown();
	delete m_cacheManager;
	delete m_surfacePool;

	int memType = (paf::system::Model::GetType() != paf::system::Model::Type_Dolce) ? 1 : 0;
	paf::GraphMem::FreeOSMemory((paf::GraphMem::DeviceType)memType, m_surfaceMem);
}

/* 0x812836B0 */
void TopmenuWhatsNewMgr::Show()
{
	if (!m_enabled || m_currentMaterials == NULL || shellStartupGetIduMode() != 0)
		return;

	consoleInfoFileAddInt(21, 1, 0);
	m_showStartTimeUs = sceKernelGetSystemTimeWideForUser();

	if (m_panelWidget != NULL) {
		if (!m_panelWidget->m_transition_complete ||
		    m_panelWidget->m_meta_alpha < 0.95f ||
		    m_panelWidget->IsAnimRunning(0x10000) ||
		    m_panelWidget->IsAnimRunning(0x10004) ||
		    m_panelWidget->IsAnimRunning(0x10002)) {
			showWidgetWithTimer(m_panelWidget,
					    createWhatsNewTimer(300.0f, paf::Timer::FUNC_QUAD_OUT));
			m_panelWidget->SetPos(
				kWhatsNewShowPos,
				createWhatsNewTimer(300.0f, paf::Timer::FUNC_CUBIC_OUT),
				0, 0.0f, 2, NULL, NULL);
		}
		return;
	}

	paf::ui::Widget *hostWidget = g_topWindow->m_scrollView;
	g_topmenuPlugin->TemplateOpen(hostWidget, paf::IDParam(WHATSNEW_TEMPLATE_ID),
				      paf::Plugin::TemplateOpenParam());
	m_panelWidget = hostWidget->FindChild(paf::IDParam(WHATSNEW_PANEL_ID), 0);

	for (SceUInt32 idx = 0; idx < 3; ++idx) {
		paf::ui::Widget *bannerWidget = m_panelWidget->FindChild(
			paf::IDParam(kWhatsNewBannerWidgetIds[idx]), 0);
		if (bannerWidget == NULL)
			continue;

		if (m_banners[idx].valid) {
			showWidgetWithTimer(bannerWidget, NULL);
			paf::intrusive_ptr<paf::graph::Surface> fallbackTex =
				g_topmenuPlugin->GetTexture(paf::IDParam(WHATSNEW_DEFAULT_TEX_ID));
			bannerWidget->SetTexture(fallbackTex);
			bannerWidget->SetDrawObjMaskBit(&m_banners[idx].imageCache, 0);
			bannerWidget->AddEventCallback(
				0x10000008, onWhatsNewBannerClicked, (void *)(intptr_t)idx);
		} else {
			hideWidgetWithTimer(bannerWidget, NULL, NULL);
		}
	}

	hideWidgetWithTimer(m_panelWidget, NULL, NULL);
	showWidgetWithTimer(m_panelWidget,
			    createWhatsNewTimer(300.0f, paf::Timer::FUNC_QUAD_OUT));
	m_panelWidget->SetPos(
		kWhatsNewShowPos,
		createWhatsNewTimer(300.0f, paf::Timer::FUNC_CUBIC_OUT),
		0, 0.0f, 2, NULL, NULL);
	m_accessor.RequestUpdate(false);
}

/* 0x81283C42 */
void TopmenuWhatsNewMgr::Hide()
{
	if (m_panelWidget == NULL)
		return;

	hideWidgetWithTimer(m_panelWidget,
			    createWhatsNewTimer(300.0f, paf::Timer::FUNC_QUAD_OUT),
			    onWhatsNewHideAnimFinished);
	m_panelWidget->SetPos(
		kWhatsNewHidePos,
		createWhatsNewTimer(300.0f, paf::Timer::FUNC_CUBIC_IN),
		0, 0.0f, 2, NULL, NULL);

	SceUInt64 nowUs = sceKernelGetSystemTimeWideForUser();
	consoleInfoFileAddTime(3, nowUs - m_showStartTimeUs);
	m_showStartTimeUs = 0;
}

/* 0x81283E6C */
void TopmenuWhatsNewMgr::OnHideFinished()
{
	if (m_panelWidget != NULL) {
		m_panelWidget->DestroyWidget();
		m_panelWidget = NULL;
	}
	if (m_pendingMaterials != NULL) {
		paf::vector<NsxMaterialItem *> *oldMaterials = m_currentMaterials;
		m_currentMaterials = m_pendingMaterials;
		m_pendingMaterials = NULL;
		m_rotationIndex = 0;
		if (oldMaterials != NULL) {
			nsxFreeMaterialList(oldMaterials);
			delete oldMaterials;
		}
	}
	SelectBanners();
}

/* 0x81284046 */
void TopmenuWhatsNewMgr::CheckRefresh()
{
	if (m_nextRefreshTick == 0 || m_activeJob.get() != NULL)
		return;

	paf::rtc::Tick nowTick = 0;
	if (SceLsdb_D8DF91EE(&nowTick) == 0 && m_nextRefreshTick < nowTick) {
		paf::common::SharedPtr<paf::job::JobItem> job;
		enqueueGetNsxObjJob(&job, false);
		m_activeJob = job;
	}
}

/* 0x81284218 */
void TopmenuWhatsNewMgr::OnAgeChanged()
{
	if (m_panelWidget != NULL)
		return;

	int userAge = SceLsdb_1673AD48();
	for (SceUInt32 idx = 0; idx < 3; ++idx) {
		if (m_banners[idx].valid && userAge >= 0 &&
		    (m_banners[idx].minAge > userAge || m_banners[idx].maxAge < userAge)) {
			SelectBanners();
			return;
		}
	}
}

/* 0x81284180 */
void topmenuWhatsNewInit(void)
{
	g_topmenuWhatsNewMgr = new TopmenuWhatsNewMgr();
}

/* 0x812841A0 */
void topmenuWhatsNewTerm(void)
{
	if (g_topmenuWhatsNewMgr != NULL) {
		delete g_topmenuWhatsNewMgr;
		g_topmenuWhatsNewMgr = NULL;
	}
}

/* 0x812841C8 */
void topmenuWhatsNewShow(void)
{
	if (g_topmenuWhatsNewMgr != NULL)
		g_topmenuWhatsNewMgr->Show();
}

/* 0x812841DC */
void topmenuWhatsNewHide(void)
{
	if (g_topmenuWhatsNewMgr != NULL)
		g_topmenuWhatsNewMgr->Hide();
}

/* 0x812841F0 */
void topmenuWhatsNewCheckRefresh(void)
{
	if (g_topmenuWhatsNewMgr != NULL)
		g_topmenuWhatsNewMgr->CheckRefresh();
}

/* 0x81284204 */
void topmenuWhatsNewForceRefresh(void)
{
	if (g_topmenuWhatsNewMgr != NULL)
		triggerDetachedNsxRefresh();
}

/* 0x81284218 */
void topmenuWhatsNewOnAgeChanged(void)
{
	if (g_topmenuWhatsNewMgr != NULL)
		g_topmenuWhatsNewMgr->OnAgeChanged();
}
