/*
 * power_manage_wallpaper.cpp: keylock blank curtain page and asynchronous wallpaper loader.
 * 3.65-CEX: 0x8123FF58 - 0x81240848
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <regmgr/regmgr.h>
#include <framework.h>
#include <plugin.h>
#include <common/shared_ptr.h>
#include <file/localfile.h>
#include <graphics/surface/surface.h>
#include <job/job.h>
#include <thread/thread.h>

#include "shell.h"
#include "theme.h"
#include "src/livespace_db/livespace_db_internal.h"
#include "src/livespace_db/lsdb_keys.h"
#include "power_manage_internal.h"

namespace power_manage {

KeyLockBlankPage *g_keyLockBlankPage = NULL;
KeyLockWallpaperMgr *g_keyLockWallpaperMgr = NULL;

static const SceUInt32 kKeyLockBlankPageHash = 0x2D42AF9CU;	/* 759350044 */
static const int kKeyLockBlankPagePriority = 27;
static const int kSurfaceBusyRetryError = -2136006144;		/* 0x80AF2200 */
static const unsigned int kSurfaceLoadMaxRetries = 40;
static const int kSurfaceLoadRetrySleepMs = 30;

/* --- KeyLockBlankPage --- */

KeyLockBlankPage::KeyLockBlankPage() : m_page(NULL)
{
	g_keyLockBlankPage = this;
}

void KeyLockBlankPage::Open()
{
	if (g_keyLockBlankPage->m_page != NULL)
		return;
	paf::Plugin::PageOpenParam openParam;
	openParam.overwrite_draw_priority = kKeyLockBlankPagePriority;
	g_keyLockBlankPage->m_page =
		g_powerManagePlugin->PageOpen(paf::IDParam(kKeyLockBlankPageHash), openParam);
}

void KeyLockBlankPage::Close()
{
	if (g_keyLockBlankPage->m_page == NULL)
		return;
	paf::Plugin::PageCloseParam closeParam;
	g_powerManagePlugin->PageClose(paf::IDParam(kKeyLockBlankPageHash), closeParam);
	g_keyLockBlankPage->m_page = NULL;
}

bool KeyLockBlankPage::IsOpen()
{
	return g_keyLockBlankPage != NULL && g_keyLockBlankPage->m_page != NULL;
}

/* --- KeyLockImageJob --- */

class KeyLockImageJob : public paf::job::JobItem {
public:
	KeyLockImageJob()
		: paf::job::JobItem("KeyLockImageJob", NULL), m_texWidth(-1), m_texHeight(-1)
	{
	}

	virtual ~KeyLockImageJob()
	{
	}

	bool LoadSurfaceFromFile(const char *path);

	virtual int Run()
	{
		const char *wallpaperPath = NULL;
		paf::string themePath;

		paf::common::SharedPtr<LsdbThemeInfo> theme = *shellThemeGetInfo();
		if (theme.get() != NULL && !theme->m_startScreenProperty.m_filePath.empty()) {
			shellThemeInfoGetFilePath(theme.get(), &themePath,
						  theme->m_startScreenProperty.m_filePath);
			wallpaperPath = themePath.c_str();
		}

		bool fallbackDefault = false;
		if (!shellThemeIconsEnabled()) {
			int wallpaperType = 0;
			int texW = 0;
			int texH = 0;
			if (sceRegMgrGetKeyInt("/CONFIG/THEME", "wallpaper", &wallpaperType) == 0) {
				if (wallpaperType == 0) {
					fallbackDefault = true;
				} else if (wallpaperType == 1 &&
					   sceRegMgrGetKeyInt("/CONFIG/THEME", "wallpaper_tex_w",
							      &texW) == 0 &&
					   sceRegMgrGetKeyInt("/CONFIG/THEME", "wallpaper_tex_h",
							      &texH) == 0) {
					wallpaperPath = "ur0:shell/wallpaper/lock.dds";
					m_texWidth = texW;
					m_texHeight = texH;
				}
			}
		}

		if (!fallbackDefault && wallpaperPath != NULL && LoadSurfaceFromFile(wallpaperPath))
			return 0;

		m_texWidth = -1;
		m_texHeight = -1;
		if (LoadSurfaceFromFile("vs0:data/internal/keylock/keylock.png"))
			return 0;
		return -1;
	}

	virtual void Finish(int result)
	{
		--g_keyLockWallpaperMgr->m_loadingCount;
		if (result == 0) {
			g_keyLockWallpaperMgr->m_surface = m_surface;
			g_keyLockWallpaperMgr->m_texWidth = m_texWidth;
			g_keyLockWallpaperMgr->m_texHeight = m_texHeight;
		}
		if (g_keyLockWallpaperMgr->m_loadingCount == 0 && g_keyLockScreen != NULL)
			g_keyLockScreen->ApplyWallpaper();
	}

	int m_texWidth;
	int m_texHeight;
	paf::intrusive_ptr<paf::graph::Surface> m_surface;
};

/* 0x812402A4 */
bool KeyLockImageJob::LoadSurfaceFromFile(const char *path)
{
	int32_t openError = 0;
	paf::common::SharedPtr<paf::File> file =
		paf::LocalFile::Open(path, 0x10001U, 0xF000U, &openError);
	if (file.get() == NULL || !file->IsOpened())
		return false;

	paf::graph::Surface::LoadOption loadOption;
	loadOption.decode_heap = paf::Framework::Instance()->GetDecodeHeap();
	loadOption.direct_load = true;
	loadOption.reserved5 = true;

	unsigned int retries = 0;
	for (;;) {
		m_surface = paf::graph::Surface::Load(g_powerManagePlugin->m_surface_pool,
						      file, &loadOption);
		if (m_surface.get() != NULL)
			break;
		if (loadOption.error != kSurfaceBusyRetryError)
			break;
		if (++retries > kSurfaceLoadMaxRetries)
			break;
		paf::thread::Sleep(kSurfaceLoadRetrySleepMs);
	}

	file->Close();
	return m_surface.get() != NULL;
}

/* --- KeyLockWallpaperMgr --- */

KeyLockWallpaperMgr::KeyLockWallpaperMgr()
	: m_texWidth(-1), m_texHeight(-1), m_loadingCount(0)
{
	g_keyLockWallpaperMgr = this;
	Reload();
}

KeyLockWallpaperMgr::~KeyLockWallpaperMgr()
{
	g_keyLockWallpaperMgr = NULL;
}

void KeyLockWallpaperMgr::Reload()
{
	++m_loadingCount;
	paf::thread::RMutex::main_thread_mutex.Lock();
	m_surface.clear();
	if (g_keyLockScreen != NULL)
		g_keyLockScreen->ClearWallpaper();
	paf::thread::RMutex::main_thread_mutex.Unlock();

	paf::common::SharedPtr<paf::job::JobItem> job(new KeyLockImageJob());
	paf::job::JobQueue::default_queue->Enqueue(job);
}

paf::intrusive_ptr<paf::graph::Surface> KeyLockWallpaperMgr::GetSurface(int *outWidth,
									int *outHeight)
{
	if (outWidth != NULL)
		*outWidth = m_texWidth;
	if (outHeight != NULL)
		*outHeight = m_texHeight;
	return m_surface;
}

bool KeyLockWallpaperMgr::IsLoading() const
{
	return m_loadingCount != 0;
}

} /* namespace power_manage */
