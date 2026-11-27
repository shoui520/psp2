/*
 * boot_screen.cpp: TopmenuBootScreen and LoadImageJob (0x8128E37C-0x8128F55B).
 * Manages the dedicated "BootScreen" PAF scene, splash image decoding on the
 * "SceShellBootScreen" job queue, display framebuffer callback registration,
 * and application-close screenshot fade transitions.
 */
#include "topmenu_internal.h"

struct TopmenuBootScreen {
	TopmenuBootScreen();
	~TopmenuBootScreen();

	void LoadSplashImage(const char *splashPath);
	void ClearSplashImage();
	void ShowSplash(const char *splashPath, TopmenuAppProcess *proc);
	void HideSplash();
	void CaptureAndFadeClose(TopmenuAppProcess *proc);

	void *m_osMemory;						/* +0x00 */
	paf::graph::SurfacePool *m_surfacePool;				/* +0x04 */
	paf::job::JobQueue *m_jobQueue;					/* +0x08 */
	paf::ui::Scene *m_scene;					/* +0x0C */
	paf::ui::Widget *m_splashGroup;					/* +0x10 */
	paf::ui::Plane *m_splashPlane;					/* +0x14 */
	paf::ui::Plane *m_bgPlane;					/* +0x18 */
	paf::intrusive_ptr<paf::graph::Surface> m_splashSurface;	/* +0x1C */
	TopmenuAppProcess *m_activeProcess;				/* +0x20 */
	paf::ui::Widget *m_closeGroup;					/* +0x24 */
	paf::ui::Plane *m_closeCapturePlane;				/* +0x28 */
	SceUID m_frameBufCbId;						/* +0x2C */
};

typedef char TopmenuBootScreenSizeCheck[sizeof(TopmenuBootScreen) == 48 ? 1 : -1];

TopmenuBootScreen *g_topmenuBootScreen = NULL;

namespace {

const SceSize BOOT_SCREEN_POOL_SIZE		= 576716;
const SceUInt32 BOOT_SCREEN_MAX_FILE_BYTES	= 0x96000u;
const SceUInt32 BOOT_SCREEN_DIRECT_THRESHOLD	= 0x64000u;

static int onFrameBufRegistered(int notifyId, int notifyCount, int notifyArg, void *userData)
{
	(void)notifyId;
	(void)notifyCount;
	(void)userData;
	TopmenuAppProcess *proc = topmenuFindProcessByAppId(g_topWindow, notifyArg);
	if (proc != NULL)
		proc->m_hasAppId = true;
	return 0;
}

static void onCloseFadeFinished(paf::Timer *timer, paf::ui::AnimListener *listener,
				void *widget, void *paramA, void *paramB)
{
	(void)timer;
	(void)listener;
	(void)widget;
	(void)paramA;
	(void)paramB;
	topmenuLaunchPendingCrashReport();
}

static void hideWidgetImmediate(paf::ui::Widget *widget, paf::Timer *timer,
				paf::ui::AnimCB finishCb)
{
	widget->Hide(0xff, timer, finishCb, 0.0f);
}

static void showWidgetImmediate(paf::ui::Widget *widget)
{
	widget->Show(0xff, NULL, NULL, 0.0f);
}

class LoadImageJob : public paf::job::JobItem {
public:
	explicit LoadImageJob(const char *splashPath)
		: paf::job::JobItem("LoadImageJob", NULL),
		  m_splashPath(splashPath != NULL ? splashPath : "")
	{
	}

	virtual ~LoadImageJob() {}

	virtual int32_t Run()
	{
		g_topmenuBootScreen->LoadSplashImage(m_splashPath.c_str());
		return 0;
	}

	virtual void Finish(int32_t result)
	{
		(void)result;
	}

	paf::string m_splashPath;	/* +0x1C */
};

typedef char LoadImageJobSizeCheck[sizeof(LoadImageJob) == 40 ? 1 : -1];

} /* namespace */

/* (0x8128E3E8) */
void TopmenuBootScreen::LoadSplashImage(const char *splashPath)
{
	if (splashPath == NULL)
		return;

	int32_t openErr = 0;
	paf::common::SharedPtr<paf::File> fp =
		paf::LocalFile::Open(splashPath, 1, 0xF000, &openErr);
	if (fp.get() == NULL || !fp->IsOpened())
		return;

	SceOff fileSize = fp->GetFileSize();
	if (fileSize > static_cast<SceOff>(BOOT_SCREEN_MAX_FILE_BYTES))
		return;

	paf::graph::Surface::LoadOption loadOpt;
	loadOpt.decode_heap = paf::Framework::Instance()->GetDecodeHeap();
	loadOpt.data_load_heap = paf::Framework::Instance()->GetDecodeHeap();
	if (fileSize > static_cast<SceOff>(BOOT_SCREEN_DIRECT_THRESHOLD))
		loadOpt.direct_load = true;

	m_splashSurface = paf::graph::Surface::Load(m_surfacePool, fp, &loadOpt);

	if (m_splashSurface.get() == NULL && !loadOpt.direct_load) {
		paf::graph::Surface::LoadOption retryOpt;
		retryOpt.decode_heap = paf::Framework::Instance()->GetDecodeHeap();
		retryOpt.data_load_heap = paf::Framework::Instance()->GetDecodeHeap();
		retryOpt.direct_load = true;
		m_splashSurface = paf::graph::Surface::Load(m_surfacePool, fp, &retryOpt);
	}

	if (m_splashSurface.get() != NULL) {
		paf::thread::RMutex::main_thread_mutex.Lock();
		m_splashPlane->SetTexture(m_splashSurface, 0, 0);
		m_splashPlane->SetColor(paf::math::v4(1.0f, 1.0f, 1.0f, 1.0f),
					NULL, 0x10001, 0, 0.0f, NULL, NULL);
		paf::thread::RMutex::main_thread_mutex.Unlock();
	}

	fp->Close();
}

/* (0x8128E7F0) */
TopmenuBootScreen::TopmenuBootScreen()
	: m_osMemory(NULL),
	  m_surfacePool(NULL),
	  m_jobQueue(NULL),
	  m_scene(NULL),
	  m_splashGroup(NULL),
	  m_splashPlane(NULL),
	  m_bgPlane(NULL),
	  m_splashSurface(),
	  m_activeProcess(NULL),
	  m_closeGroup(NULL),
	  m_closeCapturePlane(NULL),
	  m_frameBufCbId(-1)
{
	m_osMemory = paf::GraphMem::AllocOSMemory(paf::GraphMem::DeviceType_SystemMemory,
						  BOOT_SCREEN_POOL_SIZE,
						  "SceShellBootScreen", 0);
	m_surfacePool = new paf::graph::SurfacePool(m_osMemory, BOOT_SCREEN_POOL_SIZE,
						    "SceShellBootScreen", NULL,
						    paf::graph::SurfacePool::MemoryType_Normal);

	paf::job::JobQueue::Option queueOpt;
	queueOpt.priority = 0x100000F4;
	queueOpt.stackSize = 0x4000;
	queueOpt.numThreads = 1;
	m_jobQueue = new paf::job::JobQueue("SceShellBootScreen", &queueOpt);

	paf::ui::Environment *env = paf::Framework::Instance()->GetEnvironmentInstance();
	m_scene = new paf::ui::Scene(env, 35, NULL, NULL, 128);
	m_scene->m_name = paf::IDParam("BootScreen");
	new paf::ui::Camera(m_scene, NULL);
	m_scene->m_draw_always = true;

	paf::math::v4 screenSize(960.0f, 544.0f, 0.0f, 0.0f);
	paf::math::v4 colorWhite(1.0f, 1.0f, 1.0f, 1.0f);
	paf::math::v4 colorClear(0.0f, 0.0f, 0.0f, 0.0f);

	m_closeGroup = new paf::ui::Widget(m_scene, NULL);
	hideWidgetImmediate(m_closeGroup, NULL, NULL);
	m_closeGroup->SetSize(screenSize, NULL, 0x10004, 0, 0.0f, NULL, NULL);

	m_closeCapturePlane = new paf::ui::Plane(m_closeGroup, NULL);
	m_closeCapturePlane->SetSize(screenSize, NULL, 0x10004, 0, 0.0f, NULL, NULL);
	m_closeCapturePlane->SetColor(colorWhite, NULL, 0x10001, 0, 0.0f, NULL, NULL);

	m_splashGroup = new paf::ui::Widget(m_scene, NULL);
	m_splashGroup->SetSize(screenSize, NULL, 0x10004, 0, 0.0f, NULL, NULL);
	hideWidgetImmediate(m_splashGroup, NULL, NULL);

	m_bgPlane = new paf::ui::Plane(m_splashGroup, NULL);
	m_bgPlane->SetSize(screenSize, NULL, 0x10004, 0, 0.0f, NULL, NULL);
	m_bgPlane->SetColor(colorWhite, NULL, 0x10001, 0, 0.0f, NULL, NULL);
	hideWidgetImmediate(m_bgPlane, NULL, NULL);

	m_splashPlane = new paf::ui::Plane(m_splashGroup, NULL);
	m_splashPlane->SetSize(screenSize, NULL, 0x10004, 0, 0.0f, NULL, NULL);
	m_splashPlane->SetColor(colorClear, NULL, 0x10001, 0, 0.0f, NULL, NULL);

	m_frameBufCbId = sceKernelCreateCallback("SceShellRegFrameBufCB", 0,
							onFrameBufRegistered, NULL);
	vshDisplayRegisterFrameBufCallback(m_frameBufCbId);
}

/* (0x8128F432 cleanup body) */
TopmenuBootScreen::~TopmenuBootScreen()
{
	if (m_scene != NULL)
		m_scene->DestroyWidget();
	delete m_jobQueue;
	delete m_surfacePool;
	paf::GraphMem::FreeOSMemory(paf::GraphMem::DeviceType_SystemMemory, m_osMemory);
}

/* (0x8128EC26) */
void TopmenuBootScreen::ClearSplashImage()
{
	paf::intrusive_ptr<paf::graph::Surface> emptySurface;
	m_splashPlane->SetTexture(emptySurface, 0, 0);
	m_splashPlane->SetColor(paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f),
				NULL, 0x10001, 0, 0.0f, NULL, NULL);
	m_splashSurface.clear();
}

/* (0x8128EC98) */
void TopmenuBootScreen::ShowSplash(const char *splashPath, TopmenuAppProcess *proc)
{
	if (m_activeProcess == proc)
		return;

	m_activeProcess = proc;
	showWidgetImmediate(m_splashGroup);

	paf::intrusive_ptr<paf::graph::Surface> emptySurface;
	m_splashPlane->SetTexture(emptySurface, 0, 0);
	m_splashPlane->SetColor(paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f),
				NULL, 0x10001, 0, 0.0f, NULL, NULL);

	if (m_activeProcess->m_tabAppData != NULL) {
		hideWidgetImmediate(m_bgPlane, NULL, NULL);
	} else {
		showWidgetImmediate(m_bgPlane);
		m_bgPlane->SetTexture(g_topWindow->m_appCloseSnapshot, 0, 0);
		ScePafCommon_6CF3A3C8(0.0f, m_splashPlane,
				      static_cast<paf::common::transition::Type>(19), false, false);
		hideWidgetImmediate(m_bgPlane, NULL, NULL);
	}

	if (splashPath != NULL) {
		m_splashSurface.clear();
		paf::common::SharedPtr<paf::job::JobItem> job(new LoadImageJob(splashPath));
		m_jobQueue->Enqueue(job);
	}

	topmenuSetDefaultBgStyle(0, NULL, false);
}

/* (0x8128EFEC) */
void TopmenuBootScreen::HideSplash()
{
	if (m_activeProcess == NULL)
		return;

	m_activeProcess = NULL;
	paf::Timer *timer = new paf::Timer();
	if (timer != NULL)
		timer->InitTimer(0.0f, paf::Timer::FUNC_LINEAR, 0, -1, NULL);
	hideWidgetImmediate(m_splashGroup, timer, NULL);
}

/* (0x8128F0A0) */
void TopmenuBootScreen::CaptureAndFadeClose(TopmenuAppProcess *proc)
{
	if (proc == NULL)
		return;

	paf::intrusive_ptr<paf::graph::Surface> capturedSurface;
	SceInt32 captureTarget = -5;
	if (SceLsdb_849E8DC6(&proc->m_appInfo) == 0)
		captureTarget = proc->m_appId;

	if (shellScreenshotCaptureSurface(&capturedSurface, captureTarget) == 0) {
		m_closeCapturePlane->SetTexture(capturedSurface, 0, 0);
		showWidgetImmediate(m_closeGroup);
		m_closeGroup->SetPos(paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f),
				     NULL, 0x10000, 0, 0.0f, NULL, NULL);
		m_closeGroup->SetMetaAlpha(0.0f, NULL, 0x10003, 0, 0.0f, NULL, NULL);
		ScePafCommon_B48C9BDC(0.0f, m_closeGroup,
				      static_cast<paf::common::transition::Type>(15), false, false);
		hideWidgetImmediate(m_closeGroup, NULL, NULL);

		if (topmenuHasPendingCrashReport() &&
		    (g_topWindow->m_gateEffectSurface.get() == NULL ||
		     g_topWindow->m_gateEffectActive)) {
			hideWidgetImmediate(m_closeGroup, NULL, onCloseFadeFinished);
		} else {
			hideWidgetImmediate(m_closeGroup, NULL, NULL);
		}
	}
}

/* (0x8128F412) */
void topmenuBootScreenStart(void)
{
	g_topmenuBootScreen = new TopmenuBootScreen();
}

/* (0x8128F432) */
void topmenuBootScreenStop(void)
{
	delete g_topmenuBootScreen;
	g_topmenuBootScreen = NULL;
}

/* (0x8128F484) */
void topmenuBootScreenClearImage(void)
{
	g_topmenuBootScreen->ClearSplashImage();
}

/* (0x8128F496) */
void topmenuBootScreenShow(const char *splashPath, TopmenuAppProcess *proc)
{
	g_topmenuBootScreen->ShowSplash(splashPath, proc);
}

/* (0x8128F4AE) */
void topmenuBootScreenHide(void)
{
	g_topmenuBootScreen->HideSplash();
}

/* (0x8128F4C0) */
void topmenuBootScreenCaptureClose(TopmenuAppProcess *proc)
{
	g_topmenuBootScreen->CaptureAndFadeClose(proc);
}

/* (0x8128F4D6) */
paf::intrusive_ptr<paf::graph::Surface> *topmenuBootScreenGetSurfacePtr(void)
{
	return &g_topmenuBootScreen->m_splashSurface;
}
