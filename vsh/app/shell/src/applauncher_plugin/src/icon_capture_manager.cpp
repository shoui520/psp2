/*
 * icon_capture_manager.cpp: SceShellIconCaptureManager offscreen folder icon renderer.
 * Renders a 128x128 folder icon template into an offscreen ScreenSurface and encodes DXT5 DDS.
 * 3.65-CEX: 0x813C32B8 - 0x813C3C7D
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <std/string>
#include <std/vector>
#include <common/main_thread_call_list.h>
#include <common/shared_ptr.h>
#include <framework.h>
#include <plugin.h>
#include <thread/thread.h>
#include <versatileparam/idparam.h>
#include <widget/w_scene.h>
#include <widget/w_camera.h>
#include <widget/core/environment.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_screen.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <image/image.h>

#include "applauncher.h"
#include "applauncher_home.h"

extern paf::thread::RMutex g_shellMutex; /* 0x81467AAC */

#define ICON_CAPTURE_DIMENSION		128
#define ICON_CAPTURE_BUFFER_BYTES	0x10000u
#define ICON_CAPTURE_DDS_HEADER_SLACK	256u
#define ICON_CAPTURE_FOLDER_SLOTS	10
#define ICON_CAPTURE_TEMPLATE_HASH	0x8D062090u
#define ICON_CAPTURE_WIDGET_HASH	0xA2CF549Bu
#define ICON_CAPTURE_DRAW_STYLE_DEPTH	64u

enum IconCaptureState {
	ICON_CAPTURE_IDLE	= 0,
	ICON_CAPTURE_REQUESTED	= 1,
	ICON_CAPTURE_RENDERING	= 2,
	ICON_CAPTURE_COMPLETED	= 3
};

class IconCaptureManager {
public:
	explicit IconCaptureManager(paf::graph::SurfacePool *pool); /* 0x813C32B8 */
	~IconCaptureManager();

	void *CaptureFolder(const paf::vector<ApplauncherFolderSurface> *surfaces,
			    int *outSize); /* 0x813C3572 */

	static void OnMainThreadUpdate(void *userData); /* 0x813C3A8A */
	static void OnSceneDraw(const paf::ui::Scene::DrawArg &drawArg); /* 0x813C3C0A */

	paf::intrusive_ptr<paf::graph::Surface> m_captureSurface;
	paf::ui::Scene *m_scene;
	paf::ui::Widget *m_folderWidget;
	int m_captureState;
	paf::thread::Semaphore m_doneSemaphore;
};

static IconCaptureManager *s_iconCaptureManager = NULL; /* 0x815CC250 */

/* 0x813C3C0A */
void IconCaptureManager::OnSceneDraw(const paf::ui::Scene::DrawArg &drawArg)
{
	(void)drawArg;
	paf::graph::arl::ArlInstance *gl = paf::graph::arl::Gl();
	paf::intrusive_ptr<paf::graph::Surface> rootSurface =
		paf::ui::Screen::GetRootScreen()->GetSurface();
	gl->SetSceneData(rootSurface);
	ScePafGraphics_199BD5F2(paf::graph::arl::Gl(), 0);
	ScePafGraphics_29F5AA8B(paf::graph::arl::Gl(), NULL, false);
}

/* 0x813C32B8 */
IconCaptureManager::IconCaptureManager(paf::graph::SurfacePool *pool)
	: m_captureSurface(),
	  m_scene(NULL),
	  m_folderWidget(NULL),
	  m_captureState(ICON_CAPTURE_IDLE),
	  m_doneSemaphore("SceShellIconCaptureManager", 0, 1, 0)
{
	s_iconCaptureManager = this;
	g_shellMutex.Lock();

	paf::graph::ScreenSurface *screenSurface = new paf::graph::ScreenSurface(
		pool, ICON_CAPTURE_DIMENSION, ICON_CAPTURE_DIMENSION, ImageMode_RGBA8888,
		ImageOrder_Linear, 1, ImageMode_NONE, static_cast<ImageOrder>(0),
		static_cast<paf::graph::Surface::SurfaceType>(1));
	m_captureSurface = screenSurface;
	m_captureSurface->GenTexture();

	paf::ui::Environment *env = paf::Framework::Instance()->GetEnvironmentInstance();
	m_scene = new paf::ui::Scene(env, 3, NULL, NULL, ICON_CAPTURE_DIMENSION);
	m_scene->m_name = paf::IDParam("IconCapture");

	paf::ui::Screen *screen = new paf::ui::Screen(m_captureSurface);
	screen->is_auto_delete = true;
	if (m_scene->m_screen != NULL && m_scene->m_screen->is_auto_delete)
		delete m_scene->m_screen;
	m_scene->m_screen = screen;

	new paf::ui::Camera(m_scene, NULL);

	m_scene->m_draw_func = OnSceneDraw;
	m_scene->m_draw_arg.m_data = NULL;
	m_scene->m_draw_arg.m_alpha = 0.0f;
	m_scene->m_scissor_to_scene = true;

	m_scene->Hide(0xFFu, NULL, NULL, 0.0f);

	paf::common::MainThreadCallList::Register(OnMainThreadUpdate, NULL);
	g_shellMutex.Unlock();
}

IconCaptureManager::~IconCaptureManager()
{
	paf::common::MainThreadCallList::Unregister(OnMainThreadUpdate, NULL);
	g_shellMutex.Lock();
	m_scene->DestroyWidget();
	g_shellMutex.Unlock();
	s_iconCaptureManager = NULL;
}

/* 0x813C34FC */
void applauncherIconCaptureCreate(paf::graph::SurfacePool *pool)
{
	new IconCaptureManager(pool);
}

/* 0x813C3514 */
void applauncherIconCaptureDestroy(void)
{
	if (s_iconCaptureManager != NULL)
		delete s_iconCaptureManager;
}

/* 0x813C3572 */
void *IconCaptureManager::CaptureFolder(const paf::vector<ApplauncherFolderSurface> *surfaces,
					int *outSize)
{
	g_shellMutex.Lock();
	if (m_captureState != ICON_CAPTURE_IDLE) {
		g_shellMutex.Unlock();
		return NULL;
	}

	paf::Plugin *topmenuPlugin = paf::Plugin::Find("topmenu_plugin");
	if (topmenuPlugin == NULL) {
		g_shellMutex.Unlock();
		return NULL;
	}

	paf::IDParam templateId(ICON_CAPTURE_TEMPLATE_HASH);
	paf::Plugin::TemplateOpenParam openParam;
	topmenuPlugin->TemplateOpen(m_scene, templateId, openParam);

	paf::IDParam widgetId(ICON_CAPTURE_WIDGET_HASH);
	m_folderWidget = m_scene->FindChild(widgetId, 0);
	if (m_folderWidget == NULL) {
		g_shellMutex.Unlock();
		return NULL;
	}

	paf::graph::DrawObj *baseObj = m_folderWidget->GetDrawObj(0);
	if (baseObj != NULL)
		baseObj->m_draw_style.m_value = ICON_CAPTURE_DRAW_STYLE_DEPTH;

	for (SceSize i = 0; i < surfaces->size(); ++i) {
		const ApplauncherFolderSurface &entry = (*surfaces)[i];
		m_folderWidget->SetTexture(entry.m_surface, entry.m_pos + 1, 0);
	}

	for (int slot = 1; slot <= ICON_CAPTURE_FOLDER_SLOTS; ++slot) {
		if (m_folderWidget->GetDrawObj(slot) != NULL) {
			paf::intrusive_ptr<paf::graph::Surface> slotTex;
			m_folderWidget->GetTexture(slotTex, slot, 0);
			if (slotTex.get() == NULL)
				m_folderWidget->GetDrawObj(slot)->SetVisible(false);
		}
	}

	m_captureState = ICON_CAPTURE_REQUESTED;
	g_shellMutex.Unlock();

	m_doneSemaphore.Acquire();

	paf::image::ImageExtent extent(ICON_CAPTURE_DIMENSION, ICON_CAPTURE_DIMENSION);

	void *pixelBuffer = m_captureSurface->Lock(0);
	m_captureSurface->Unlock();

	paf::memory::HeapAllocator *decodeHeap = paf::Framework::Instance()->GetDecodeHeap();
	paf::common::SharedPtr<paf::image::Image> rawImage = paf::image::Image::CreateFromBuffer(
		pixelBuffer, ICON_CAPTURE_BUFFER_BYTES, NULL, 0, ICON_CAPTURE_DIMENSION, extent,
		ImageMode_RGBA8888, decodeHeap);
	paf::common::SharedPtr<paf::image::Image> dxtImage =
		rawImage->ConvertMode(ImageMode_DXT5, paf::image::ImageConvertModeType_LowQuality);
	paf::image::ImageBuffer dxtBuffer = dxtImage->ToBuffer(false);

	SceSize ddsAllocSize = dxtBuffer.size + ICON_CAPTURE_DDS_HEADER_SLACK;
	void *ddsMemory = paf::Framework::Instance()->GetDecodeHeap()->sce_paf_malloc(ddsAllocSize);
	if (ddsMemory == NULL) {
		g_shellMutex.Lock();
		m_captureState = ICON_CAPTURE_IDLE;
		g_shellMutex.Unlock();
		return NULL;
	}

	if (paf::image::Image::SaveAsDds(ddsMemory, static_cast<uint32_t>(ddsAllocSize),
					 dxtBuffer.buff, ICON_CAPTURE_DIMENSION, extent,
					 ImageMode_DXT5, 0) == 0) {
		sce_paf_free(ddsMemory);
		g_shellMutex.Lock();
		m_captureState = ICON_CAPTURE_IDLE;
		g_shellMutex.Unlock();
		return NULL;
	}

	*outSize = static_cast<int>(ddsAllocSize);

	g_shellMutex.Lock();
	m_captureState = ICON_CAPTURE_IDLE;
	g_shellMutex.Unlock();
	return ddsMemory;
}

/* 0x813C3A72 */
void *applauncherIconCaptureFolder(const paf::vector<ApplauncherFolderSurface> *surfaces,
				   int *outSize)
{
	return s_iconCaptureManager->CaptureFolder(surfaces, outSize);
}

/* 0x813C3A8A */
void IconCaptureManager::OnMainThreadUpdate(void *userData)
{
	(void)userData;
	IconCaptureManager *mgr = s_iconCaptureManager;
	if (mgr->m_captureState == ICON_CAPTURE_REQUESTED) {
		paf::ui::Environment *env = paf::Framework::Instance()->GetEnvironmentInstance();
		env->m_draw_when_suspended = true;
		mgr->m_scene->Show(0xFFu, NULL, NULL, 0.0f);
		mgr->m_captureState = ICON_CAPTURE_RENDERING;
	} else if (mgr->m_captureState == ICON_CAPTURE_RENDERING) {
		paf::ui::Environment *env = paf::Framework::Instance()->GetEnvironmentInstance();
		env->m_draw_when_suspended = false;
		mgr->m_scene->Hide(0xFFu, NULL, NULL, 0.0f);

		mgr->m_captureState = ICON_CAPTURE_COMPLETED;
		mgr->m_folderWidget->m_name = paf::IDParam("");
		mgr->m_folderWidget->DestroyWidget();
		mgr->m_folderWidget = NULL;
		mgr->m_doneSemaphore.Release();
	}
}
