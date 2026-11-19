/*
 * applauncher_pages.cpp: the home's pages read from the database (0x810B297C-0x810B3B36): the
 * jobs that read them, their icons and parameters, the icon images asked of the cache, the
 * folder icons' captures and the wallpapers no page uses deleted. The file's name and every name
 * here but the jobs' are ours (PLACEHOLDER): no record or string of the 3.6x shell names them.
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/vector>
#include <dir/dir.h>
#include <file/localfile.h>
#include <job/job.h>
#include <common/shared_ptr.h>
#include "../../../shell.h"
#include "../../../plugins.h"
#include "../../../theme.h"
#include "../../livespace_db/lsdb_keys.h"
#include "applauncher.h"
#include "applauncher_home.h"

using namespace paf;

/* the job queue topmenu gives the home */
#define APPLAUNCHER_PAGES_JOB_QUEUE() \
	(((job::JobQueue *(*)(void))g_applauncherHome->m_topmenu->func[13])())

/* the folder the wallpapers of the pages are copied to */
#define APPLAUNCHER_PAGES_TABPAPER_DIR	"ur0:shell/tabpaper/"
/* the title whose data is never asked of the database: Welcome Park */
#define APPLAUNCHER_PAGES_WELCOME_PARK	"PCSI00011"
/* the title opened with the focus on a first start */
#define APPLAUNCHER_PAGES_FIRST_TITLE	"NPXS10007"
/* a theme's wave that the console's own page colour replaces */
#define APPLAUNCHER_PAGES_CONSOLE_WAVE	31
/* the theme content version from which the theme gives its pages' font colours */
#define APPLAUNCHER_PAGES_FONT_COLOR_VER	100

/* the pages read once (0x81555208) */
static bool s_pagesLoaded;

/* PLACEHOLDER: the pages laid out for the IDU (0x810C0626), and whether the console's page
   colour is already applied (0x810C6DA0), both in the IDU layout's file */
void applauncherIduLayout(bool reset);
bool applauncherIduLayoutColorApplied(void);

/* the files of a folder deleted, but for those the pages still use */
class FileDeleteJob : public job::JobItem {
public:
	FileDeleteJob(const char *dir) : job::JobItem("FileDeleteJob", NULL), m_dir(dir)
	{
		applauncherHomeGetPageImages(&m_keep);
	}

	__attribute__((noinline)) virtual ~FileDeleteJob()
	{
	}

	virtual int Run();

	virtual void Finish(int result)
	{
	}

	string m_dir;
	vector<string> m_keep;		/* the pages' images */
};

/* the job deleting the folder's files put on the default queue */
__attribute__((noinline))
static common::SharedPtr<job::JobItem> fileDeleteJobStart(const char *dir)
{
	common::SharedPtr<job::JobItem> job(new FileDeleteJob(dir));

	job::JobQueue::default_queue->Enqueue(job);
	return job;
}

/* each file of the folder that no page uses deleted; -1 when it cannot be opened */
int FileDeleteJob::Run()
{
	Dir dir;
	DirEnt ent;

	if (dir.Open(m_dir.c_str()) != 0)
		return -1;
	while (dir.Read(&ent) == 0) {
		if (ent.type == DirEnt::Type_File && ent.name.c_str()[0] != '.' &&
		    ent.name.length() != 0) {
			string path = m_dir + ent.name;
			SceUInt32 i;

			for (i = 0; i < m_keep.size(); i++) {
				if (m_keep[i] == path)
					break;
			}
			if (i >= m_keep.size() && LocalFile::IsFile(path.c_str()))
				LocalFile::RemoveFile(path.c_str());
		}
	}
	return 0;
}

/* the pages laid out for the IDU, then counted again */
class IduLayoutJob : public job::JobItem {
public:
	IduLayoutJob(bool reset) : job::JobItem("IduLayoutJob", NULL)
	{
		m_reset = reset;
	}

	virtual ~IduLayoutJob()
	{
	}

	virtual int Run();
	virtual void Finish(int result);

	bool m_reset;
};

/* the IDU layout's job put on the default queue */
__attribute__((noinline))
static common::SharedPtr<job::JobItem> iduLayoutJobStart(bool reset)
{
	common::SharedPtr<job::JobItem> job(new IduLayoutJob(reset));

	job::JobQueue::default_queue->Enqueue(job);
	return job;
}

int IduLayoutJob::Run()
{
	applauncherIduLayout(m_reset);
	return 0;
}

void IduLayoutJob::Finish(int result)
{
	applauncherGetTotalPageJobStart(APPLAUNCHER_PAGES_JOB_QUEUE(), applauncherPagesOnTotalPage,
					NULL);
}

/* what the image of a title's icon is handed to */
typedef void (*ApplauncherIconImageCallback)(intrusive_ptr<cache::CacheImageInterface> *image,
					     void *data);

/* the image of a title's icon read from the icon cache, its row read first */
class GetIconCacheImageJob : public job::JobItem {
public:
	GetIconCacheImageJob(const string &titleId, ApplauncherIconImageCallback callback,
			     void *data);

	__attribute__((noinline)) virtual ~GetIconCacheImageJob()
	{
	}

	virtual int Run();
	virtual void Finish(int result);

	string m_titleId;
	LsdbIconPtr m_icon;
	ApplauncherIconImageCallback m_callback;
	void *m_callbackData;
};

GetIconCacheImageJob::GetIconCacheImageJob(const string &titleId,
					   ApplauncherIconImageCallback callback, void *data)
	: job::JobItem("GetIconCacheImageJob", NULL), m_titleId(titleId),
	  m_icon(new LsdbIcon())
{
	m_callback = callback;
	m_callbackData = data;
}

/* the job asking the image of the title's icon put on the home's queue */
__attribute__((noinline))
static common::SharedPtr<job::JobItem> getIconCacheImageJobStart(const string &titleId,
	ApplauncherIconImageCallback callback, void *data)
{
	common::SharedPtr<job::JobItem> job(new GetIconCacheImageJob(titleId, callback, data));

	APPLAUNCHER_PAGES_JOB_QUEUE()->Enqueue(job);
	return job;
}

int GetIconCacheImageJob::Run()
{
	return SceLsdb_DE0DA538(g_shellLsdbClient->client, &m_titleId, &m_icon, NULL, false);
}

void GetIconCacheImageJob::Finish(int result)
{
	if (result == 0 && m_icon.get() != NULL) {
		intrusive_ptr<cache::CacheImageInterface> image;

		applauncherIconCacheLoad(m_icon->iconPath, &image, NULL, 0, false);
		if (m_callback != NULL)
			m_callback(&image, m_callbackData);
	}
}

/* the icons taken off a folder told to the pages' saved order, the folder's icon updated then */
class ReloadFolderIconCaptureJob : public job::JobItem {
public:
	ReloadFolderIconCaptureJob(const vector<ApplauncherRemovedIcon> &icons, SceUInt64 appId)
		: job::JobItem("ReloadFolderIconCaptureJob", NULL), m_icons(icons)
	{
		m_appId = appId;
	}

	virtual ~ReloadFolderIconCaptureJob()
	{
	}

	virtual int Run();
	virtual void Finish(int result);

	vector<ApplauncherRemovedIcon> m_icons;
	SceUInt64 m_appId;		/* the folder's */
};

/* the folder's capture job put on the home's queue */
__attribute__((noinline))
static common::SharedPtr<job::JobItem> reloadFolderIconCaptureJobStart(
	const vector<ApplauncherRemovedIcon> &icons, SceUInt64 appId)
{
	common::SharedPtr<job::JobItem> job(new ReloadFolderIconCaptureJob(icons, appId));

	APPLAUNCHER_PAGES_JOB_QUEUE()->Enqueue(job);
	return job;
}

int ReloadFolderIconCaptureJob::Run()
{
	applauncherPagesRemoveIcons(&m_icons);
	return 0;
}

void ReloadFolderIconCaptureJob::Finish(int result)
{
	common::SharedPtr<ApplauncherIcon> icon = g_applauncherHome->FindIcon(m_appId);

	if (icon.get() != NULL)
		icon->Update(true);
}

/* the pages read again: laid out for the IDU first in IDU mode or with a theme's icons, counted
   otherwise (0x810B31DE) */
void applauncherPagesReload(void)
{
	if (shellStartupGetIduMode() != 0)
		iduLayoutJobStart(true);
	else if (shellThemeIconsEnabled())
		iduLayoutJobStart(false);
	else
		applauncherGetTotalPageJobStart(APPLAUNCHER_PAGES_JOB_QUEUE(),
						applauncherPagesOnTotalPage, NULL);
}

/* the icons of the first pages read again, the count given (0x810B3338) */
void applauncherPagesSetCount(int count)
{
	applauncherGetPageLayoutJobStart(APPLAUNCHER_PAGES_JOB_QUEUE(), 0, count,
					 applauncherPagesOnPageIcons, NULL);
}

/* the title's icon row read again, but Welcome Park's (0x810B33D6) */
void applauncherPagesRequestData(const string &titleId)
{
	if (sce_paf_strcmp(APPLAUNCHER_PAGES_WELCOME_PARK, titleId.c_str()) != 0)
		applauncherGetIconInfoJobStart(APPLAUNCHER_PAGES_JOB_QUEUE(), titleId,
					       applauncherPagesOnIconInfo, NULL);
}

/* the wallpapers the pages no longer use deleted (0x810B3482) */
void applauncherPagesCleanTabpaper(void)
{
	fileDeleteJobStart(APPLAUNCHER_PAGES_TABPAPER_DIR);
}

/* the pages read once (0x810B3502) */
bool applauncherPagesLoaded(void)
{
	return s_pagesLoaded;
}

/* the image of the title's icon asked, handed to the function given (0x810B350E) */
void applauncherPagesRequestImage(const string &titleId, void *param, void *data)
{
	getIconCacheImageJobStart(titleId, (ApplauncherIconImageCallback)param, data);
}

/* the icons taken off the folder told to the database, its icon updated then (0x810B358C) */
void applauncherPagesReloadFolderIconCapture(const vector<ApplauncherRemovedIcon> &icons,
					     SceUInt64 appId)
{
	reloadFolderIconCaptureJobStart(icons, appId);
}

/*
 * The pages' icons read: a page made for each list after the last one, an icon for each row
 * (the power button's given its title), the first title's icon selected on a first start, the
 * applications counted with those in folders, and the focus links set (0x810B3606).
 */
void applauncherPagesOnPageLayout(ApplauncherLsdbJob *job, void *arg)
{
	GetPageLayoutJob *layout = static_cast<GetPageLayoutJob *>(job);
	ApplauncherPage *page = NULL;

	for (SceUInt32 i = 0; i < layout->m_icons.size(); i++) {
		page = applauncherHomeAddPage(NULL, page, false, false);

		LsdbIconList *list = layout->m_icons[i].get();

		for (SceUInt32 j = 0; j < list->size(); j++) {
			const LsdbIconPtr &data = (*list)[j];

			if (data->type == APPLAUNCHER_ICON_TYPE_POWER)
				data->titleId = "power";

			common::SharedPtr<ApplauncherIcon> icon = page->AddIcon(data);

			if (shellStartupNeedsInitialSetup() &&
			    string(APPLAUNCHER_PAGES_FIRST_TITLE) == data->titleId)
				icon->Select();
			if (data->type == LSDB_ICON_TYPE_FOLDER) {
				if (data->folder.get() != NULL)
					g_applauncherHome->m_appCount += data->folder->iconCount;
			} else {
				g_applauncherHome->m_appCount++;
			}
		}
		page->UpdateFocusLinks();
	}
}

/*
 * The pages' parameters read: with a theme's icons each page given the theme's wallpaper (its
 * whole 960x512), wave (the console's colour first applied for its own) and font colour, then
 * its parameters set; the empty pages deleted (0x810B37F8).
 */
void applauncherPagesOnPageTheme(ApplauncherLsdbJob *job, void *arg)
{
	GetPageThemeJob *theme = static_cast<GetPageThemeJob *>(job);
	ApplauncherHome *home = g_applauncherHome;
	bool enabled = shellThemeIconsEnabled();
	common::SharedPtr<LsdbThemeInfo> *info = shellThemeGetInfo();

	for (SceUInt32 i = 0; i < theme->m_pages.size(); i++) {
		ApplauncherPage *page = static_cast<ApplauncherPage *>(
			home->m_scroller.m_items.ptr[i + theme->m_page]);

		if (enabled) {
			if ((*info)->m_homeProperty.m_bgParam[i].m_imageFilePath.length() != 0) {
				shellThemeInfoGetFilePath(info->get(),
					&theme->m_pages[i]->themeFile,
					(*info)->m_homeProperty.m_bgParam[i].m_imageFilePath);
				theme->m_pages[i]->texWidth = 960;
				theme->m_pages[i]->texHeight = 512;
				theme->m_pages[i]->imageWidth = 960;
				theme->m_pages[i]->imageHeight = 512;
			}
			if ((*info)->m_homeProperty.m_bgParam[i].m_waveType >= 0) {
				if ((*info)->m_homeProperty.m_bgParam[i].m_waveType ==
					    APPLAUNCHER_PAGES_CONSOLE_WAVE &&
				    !applauncherIduLayoutColorApplied())
					shellStartupGetPageColor();
				theme->m_pages[i]->bgColor =
					(*info)->m_homeProperty.m_bgParam[i].m_waveType;
			}
			if ((*info)->m_status >= APPLAUNCHER_PAGES_FONT_COLOR_VER)
				theme->m_pages[i]->reserved01 =
					(*info)->m_homeProperty.m_bgParam[i].GetFontColor();
		}
		page->SetParam(theme->m_pages[i].get(), false);
	}
	applauncherHomeDeleteEmptyPages();
}

/* the icons counted: the home told how many wait (0x810B38E8) */
void applauncherPagesOnIconNum(ApplauncherLsdbJob *job, void *arg)
{
	applauncherHomeSetWaitingAppCount(static_cast<GetIconNumJob *>(job)->m_count);
}

/* the pages counted: their icons and parameters read, the icons waiting counted, and the pages
   marked read (0x810B38F2) */
void applauncherPagesOnTotalPage(ApplauncherLsdbJob *job, void *arg)
{
	int total = static_cast<GetTotalPageJob *>(job)->m_total;

	if (total > 0) {
		applauncherGetPageLayoutJobStart(APPLAUNCHER_PAGES_JOB_QUEUE(), 0, total,
						 applauncherPagesOnPageLayout, NULL);
		applauncherGetPageThemeJobStart(APPLAUNCHER_PAGES_JOB_QUEUE(), 0, total,
						applauncherPagesOnPageTheme, NULL);
	}
	applauncherGetIconNumJobStart(APPLAUNCHER_PAGES_JOB_QUEUE(), -100000000,
				      applauncherPagesOnIconNum, NULL);
	s_pagesLoaded = true;
}

/* the first pages' icons read again: given to the home (0x810B3A96) */
void applauncherPagesOnPageIcons(ApplauncherLsdbJob *job, void *arg)
{
	g_applauncherHome->UpdatePageIcons(static_cast<GetPageLayoutJob *>(job)->m_icons);
}

/* a title's icon row read again: given to the home when read (0x810B3AAE) */
void applauncherPagesOnIconInfo(ApplauncherLsdbJob *job, void *arg)
{
	GetIconInfoJob *info = static_cast<GetIconInfoJob *>(job);

	if (info->m_ret == 0)
		applauncherHomeUpdateApp(info->m_titleId, info->m_icon);
}
