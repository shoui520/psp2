/*
 * applauncher_theme.cpp: the theme chooser of the edit (0x810C61DC-0x810C86E0): a list of chips
 * to pick a photo, the page's image, the user's own wave or one of the 31 waves for the page,
 * the jobs that convert a picked photo and load the page's image, and the waves' colours. The
 * file's name and the names here are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <framework.h>
#include <std/stdio.h>
#include <std/string>
#include <std/memory>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <versatileparam/idparam.h>
#include <common/shared_ptr.h>
#include <common/transition.h>
#include <graphics/surface/surface.h>
#include <graphics/drawobj/effect_path.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_button_base.h>
#include <widget/w_scene.h>
#include <widget/w_list_view.h>
#include <widget/w_list_item.h>
#include <timer/timer.h>
#include <job/job.h>
#include <dir/dir.h>
#include <file/localfile.h>
#include <rtc/rtc.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../../livespace_db/lsdb_keys.h"
#include "../../../shell.h"
#include "../../../plugins.h"
#include "../../../theme.h"
#include "applauncher.h"
#include "applauncher_home.h"
#include "applauncher_mode.h"

using namespace paf;

/* PLACEHOLDER: the chooser's template, its widgets, the home's widget it hides and its close
   button */
#define APPLAUNCHER_THEME_TEMPLATE		0xB283AF24
#define APPLAUNCHER_THEME_FRAME			0xCAD4D4A8
#define APPLAUNCHER_THEME_BOX			0xE09DDA68
#define APPLAUNCHER_THEME_CURSOR		0x47C94853
#define APPLAUNCHER_THEME_LIST			0xD48B78DC
#define APPLAUNCHER_THEME_CLOSE			0xC0099932
#define APPLAUNCHER_THEME_HOME_WIDGET		0xE1C6E5D7
/* PLACEHOLDER: the chips' templates and their parts: the photo pick's button, the page's
   image, a wave's two colours, and the texture of a chip without an image */
#define APPLAUNCHER_THEME_PHOTO_CHIP		0x562DF2F8
#define APPLAUNCHER_THEME_PHOTO_BUTTON		0x7EA0B804
#define APPLAUNCHER_THEME_IMAGE_CHIP		0x473B2BF4
#define APPLAUNCHER_THEME_IMAGE			0xA7A310E2
#define APPLAUNCHER_THEME_NO_IMAGE_TEXTURE	0xAFDEF9D0
#define APPLAUNCHER_THEME_WAVE_CHIP		0xABF77024
#define APPLAUNCHER_THEME_WAVE_COLOR0		0x1F824A17
#define APPLAUNCHER_THEME_WAVE_COLOR1		0x211BED7E
/* PLACEHOLDER: the text while a photo converts, and those of the conversion's errors */
#define APPLAUNCHER_THEME_CONVERT_TEXT		0x632B6733
#define APPLAUNCHER_THEME_TOO_LARGE_TEXT	0xC8FAD0B6
#define APPLAUNCHER_THEME_UNSUPPORTED_TEXT	0x37B4D36A

/* PLACEHOLDER: the conversion's errors with a text of their own */
#define PHOTO_EXPORT_ERROR_TOO_LARGE		((int)0x80105903)
#define PHOTO_EXPORT_ERROR_TOO_LARGE2		((int)0x80105901)
#define PHOTO_EXPORT_ERROR_UNSUPPORTED		((int)0x80105902)
#define PHOTO_EXPORT_ERROR_UNSUPPORTED2		((int)0x80105904)

/* PLACEHOLDER: the waves, the user's own wave's preset, and the page's size of a wallpaper */
#define APPLAUNCHER_THEME_WAVES			31
#define APPLAUNCHER_THEME_WAVE_CUSTOM		31
#define APPLAUNCHER_THEME_WALLPAPERS		10
#define APPLAUNCHER_THEME_PAGE_WIDTH		960
#define APPLAUNCHER_THEME_PAGE_HEIGHT		512

/* the folder the picked photos are converted into */
#define APPLAUNCHER_THEME_TABPAPER_DIR		"ur0:shell/tabpaper/"

/* PLACEHOLDER: what the photo picker gives its function (0x810E2F4E) */
struct ApplauncherPhotoPickResult {
	int result;
	int reserved;
	char path[1024];
	int width;
	int height;
};

/* PLACEHOLDER: the photo picker opened with the function to call with the photo, and closed
   (0x810E2F4E, 0x810E3174); the wallpaper picker opened on a wallpaper, and closed (0x810E439E,
   0x810E45E0) */
int applauncherPhotoPickOpen(void (*func)(ApplauncherPhotoPickResult *result), void *arg);
int applauncherPhotoPickClose(void);
int applauncherWallpaperPickOpen(int index, int reserved, void (*func)(int index),
				 void *arg);
int applauncherWallpaperPickClose(void);
/* PLACEHOLDER: a chip chosen; the chips of the photo pick, the page's image, the user's own
   wave and the first wave, as the chooser counted them (0x815442A0-0x815442AC) */
static bool s_pageChosen = false;
static bool s_pickInterrupted = false;	/* a power event during a pick */
static bool s_waveColorsSet = false;
static bool s_hasImage = false;
static int s_cursorChip = 0;
static int s_firstWaveChip = 1;
static int s_imageChip = -1;
static int s_customWaveChip = -1;

/* the chooser, while one is up (0x815554E0) */
static ApplauncherThemeSelector *s_selector;
/* the page chosen, an LsdbPage's fields: its file and the rest (0x815554E8, 0x815554F4) */
static string s_pageThemeFile;
static struct {
	int bgColor;
	int texWidth;
	int texHeight;
	int imageWidth;
	int imageHeight;
	int reserved01;
} s_page;

/* the waves' colours by their place in the waves' order (0x81555540, 0x81555730) */
static math::v4 s_waveColors0[APPLAUNCHER_THEME_WAVES] = {
	math::v4(0.2314f, 0.7608f, 0.9804f, 1.0f),
	math::v4(0.8784f, 0.8941f, 0.9843f, 1.0f),
	math::v4(0.9804f, 0.7961f, 0.4118f, 1.0f),
	math::v4(0.9529f, 0.0745f, 0.2392f, 1.0f),
	math::v4(0.0f, 0.7843f, 0.8353f, 1.0f),
	math::v4(0.6157f, 0.0196f, 0.2588f, 1.0f),
	math::v4(0.8627f, 0.9098f, 0.9725f, 1.0f),
	math::v4(0.0314f, 0.2627f, 0.451f, 1.0f),
	math::v4(0.9922f, 0.8196f, 0.9059f, 1.0f),
	math::v4(0.9725f, 0.9804f, 0.1804f, 1.0f),
	math::v4(0.0f, 0.0f, 0.0f, 1.0f),
	math::v4(0.5098f, 0.4588f, 0.851f, 1.0f),
	math::v4(0.0471f, 0.1098f, 0.9686f, 1.0f),
	math::v4(1.0f, 0.2235f, 0.2235f, 1.0f),
	math::v4(1.0f, 0.7098f, 0.6745f, 1.0f),
	math::v4(0.7333f, 0.9294f, 0.9176f, 1.0f),
	math::v4(0.0f, 0.8275f, 0.8784f, 1.0f),
	math::v4(0.498f, 0.0353f, 0.7882f, 1.0f),
	math::v4(0.9647f, 0.8275f, 0.8392f, 1.0f),
	math::v4(0.4784f, 0.3804f, 0.3961f, 1.0f),
	math::v4(0.9529f, 0.1529f, 0.8314f, 1.0f),
	math::v4(0.9843f, 0.3922f, 0.7412f, 1.0f),
	math::v4(0.9137f, 0.6902f, 0.9333f, 1.0f),
	math::v4(0.7059f, 0.9569f, 0.102f, 1.0f),
	math::v4(0.1451f, 0.8667f, 0.9098f, 1.0f),
	math::v4(0.8275f, 0.8275f, 0.8275f, 1.0f),
	math::v4(0.7961f, 0.9137f, 1.0f, 1.0f),
	math::v4(1.0f, 1.0f, 0.95686275f, 1.0f),
	math::v4(0.8627451f, 0.8627451f, 0.8156863f, 1.0f),
	math::v4(0.79607844f, 0.9137255f, 1.0f, 1.0f),
	math::v4(0.8784314f, 0.9137255f, 0.89411765f, 1.0f),
};

static math::v4 s_waveColors1[APPLAUNCHER_THEME_WAVES] = {
	math::v4(0.0118f, 0.0549f, 0.8588f, 1.0f),
	math::v4(0.8353f, 0.8745f, 0.9608f, 1.0f),
	math::v4(0.9922f, 0.2667f, 0.2588f, 1.0f),
	math::v4(0.8078f, 0.0627f, 0.1333f, 1.0f),
	math::v4(0.0275f, 0.4471f, 0.6196f, 1.0f),
	math::v4(0.2196f, 0.0f, 0.0941f, 1.0f),
	math::v4(0.2078f, 0.349f, 0.9569f, 1.0f),
	math::v4(0.0078f, 0.2118f, 0.3686f, 1.0f),
	math::v4(0.9961f, 0.1647f, 0.5961f, 1.0f),
	math::v4(0.949f, 0.8039f, 0.0f, 1.0f),
	math::v4(0.0f, 0.0f, 0.0f, 1.0f),
	math::v4(0.1922f, 0.0745f, 0.6392f, 1.0f),
	math::v4(0.0235f, 0.0824f, 0.3373f, 1.0f),
	math::v4(1.0f, 0.9137f, 0.3412f, 1.0f),
	math::v4(0.9333f, 0.4784f, 0.4275f, 1.0f),
	math::v4(0.0549f, 0.8039f, 0.7059f, 1.0f),
	math::v4(0.0f, 0.6863f, 0.8863f, 1.0f),
	math::v4(0.098f, 0.0039f, 0.2039f, 1.0f),
	math::v4(0.7529f, 0.6706f, 0.6784f, 1.0f),
	math::v4(0.3373f, 0.2824f, 0.2784f, 1.0f),
	math::v4(0.6745f, 0.0353f, 0.8118f, 1.0f),
	math::v4(0.8941f, 0.051f, 0.4078f, 1.0f),
	math::v4(0.7686f, 0.5686f, 0.8627f, 1.0f),
	math::v4(0.1608f, 0.6118f, 0.0039f, 1.0f),
	math::v4(0.0f, 0.6941f, 0.8431f, 1.0f),
	math::v4(0.9059f, 0.9059f, 0.9059f, 1.0f),
	math::v4(0.5294f, 0.702f, 0.9333f, 1.0f),
	math::v4(0.6862745f, 0.7254902f, 0.101960786f, 1.0f),
	math::v4(0.34117648f, 0.41568628f, 0.30588236f, 1.0f),
	math::v4(0.43137255f, 0.61960787f, 0.63529414f, 1.0f),
	math::v4(0.3764706f, 0.40784314f, 0.48235294f, 1.0f),
};

/* the user's own wave's colours, none until waveparam.bin gives them (0x81555510) */
static ApplauncherWaveColors s_waveColors = {
	{ math::v4(0.0f, 0.0f, 0.0f, 0.0f), math::v4(0.0f, 0.0f, 0.0f, 0.0f) }
};
/* PLACEHOLDER: a chip's size; not read (0x81555530) */
static math::v4 s_chipSize(160.0f, 90.0f, 0.0f, 0.0f);

/* the waves' presets in the chooser's order (0x81507A38) */
static const int s_wavePresets[APPLAUNCHER_THEME_WAVES] = {
	0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 24, 25, 26, 23, 7, 27, 28, 29, 30,
};

static void onPhotoPicked(ApplauncherPhotoPickResult *result);
static void onPowerEvent(void *arg);
static void onWallpaperPicked(int index);
static void onClose(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
static ui::Widget *onCursorKeep(ui::Widget *cursor, void *arg);

/* the cursor moved to the chip (0x810C61DC) */
static void applauncherThemeSelectorSetCursor(int chip)
{
	char name[16];

	if (s_selector == NULL || s_selector->m_list == NULL)
		return;
	s_cursorChip = chip;
	sce_paf_snprintf(name, sizeof(name), "%s%d", "chipItem", chip);

	ui::Widget *item = s_selector->m_list->FindChild(IDParam(name), 0);

	if (item == NULL)
		return;

	ui::Widget *cursor = s_selector->m_cursor;

	if (!cursor->m_transition_complete) {
		cursor->m_transition = 0xff;
		cursor->m_transition_complete = true;
		cursor->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(cursor, NULL, 2, 0.0f, NULL);
	s_selector->m_cursor->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
	s_selector->m_cursor->SetParent(item, NULL, 0);
	s_selector->m_cursor->SetScale(math::v4(2.0f, 2.0f, 1.0f, 0.0f));
	s_selector->m_cursor->SetScale(math::v4(1.0f, 1.0f, 1.0f, 0.0f),
				       new Timer(200.0f, Timer::FUNC_BOUNCE_OUT));
	s_selector->m_cursor->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f),
				       new Timer(150.0f, Timer::FUNC_QUAD_IN));
}

/* PLACEHOLDER: the image job: the page's image loaded (0x8152F3D4) */
class LoadImageJob : public job::JobItem {
public:
	LoadImageJob(const char *path) : job::JobItem("LoadImageJob", NULL), m_path(path)
	{
	}

	virtual ~LoadImageJob();
	virtual int Run();
	virtual void Finish(int result);

	string m_path;
	intrusive_ptr<graph::Surface> m_surface;
};

/* PLACEHOLDER: the list's item factory: a chip of each kind (0x8152F380) */
class ApplauncherThemeChipFactory : public ui::listview::ItemFactory {
public:
	virtual ~ApplauncherThemeChipFactory()
	{
	}

	virtual ui::ListItem *Create(CreateParam &param);
};

/* the chooser: its widgets made on topmenu's plane, its chips and the page's (0x810C6498) */
ApplauncherThemeSelector::ApplauncherThemeSelector()
{
	m_loadJob = common::SharedPtr<job::JobItem>();
	m_waitDialog = -1;
	m_wallpaperIndex = -1;
	m_image = NULL;
	m_wallpaperPicking = false;
	m_photoPicking = false;
	reserved = false;
	if (s_selector != NULL)
		return;
	s_selector = this;

	common::SharedPtr<LsdbThemeInfo> *theme = shellThemeGetInfo();

	if (theme->get() != NULL) {
		string path;

		shellThemeInfoGetFilePath(theme->get(), &path, (*theme)->m_infomationProperty.m_packageImageFilePath);
		m_loadJob = common::SharedPtr<job::JobItem>(new LoadImageJob(path.c_str()));
		job::JobQueue::default_queue->Enqueue(
			*(common::SharedPtr<job::JobItem> *)&m_loadJob);
		s_hasImage = true;
	} else {
		s_hasImage = false;
	}

	ShellPluginInterface *topmenu =
		(ShellPluginInterface *)applauncherPluginGetInterface("topmenu_plugin", 1);
	Plugin *plugin = g_applauncherHome->m_plugin;
	SceUInt32 chips = 32;
	int chip;

	if (s_hasImage)
		chips = 33;
	if (s_waveColorsSet)
		chips++;
	chip = 1;
	s_firstWaveChip = 1;
	s_imageChip = -1;
	s_customWaveChip = -1;
	if (s_hasImage) {
		s_imageChip = 1;
		chip = 2;
		s_firstWaveChip = 2;
	}
	if (s_waveColorsSet) {
		s_customWaveChip = chip;
		s_firstWaveChip = chip + 1;
	}

	m_root = ((ui::Widget *(*)(int))topmenu->func[TOPMENU_PLUGIN_FUNC_CREATE_PLANE])(0);
	m_root->SetParent(m_root->m_parent, NULL, 6);
	ScePafWidget_42C5A4D4(m_root, 0, 0);
	ScePafWidget_164BFFA5(m_root, 32);

	ui::Widget *home = g_applauncherHome->m_scroller.m_scrollView->m_scene->FindChild(
		IDParam(s_empty_name, APPLAUNCHER_THEME_HOME_WIDGET), 0);

	if (home != NULL)
		home->SetParent(home->m_parent, NULL, 6);
	plugin->TemplateOpen(m_root, IDParam(s_empty_name, APPLAUNCHER_THEME_TEMPLATE),
			     Plugin::TemplateOpenParam());
	m_frame = m_root->FindChild(IDParam(s_empty_name, APPLAUNCHER_THEME_FRAME), 0);
	m_box = m_frame->FindChild(IDParam(s_empty_name, APPLAUNCHER_THEME_BOX), 0);
	m_cursor = m_box->FindChild(IDParam(s_empty_name, APPLAUNCHER_THEME_CURSOR), 0);
	m_cursor->m_keep_cb = onCursorKeep;
	m_cursor->m_keep_arg = NULL;
	m_cursor->m_hit_detect_b_mode = 2;
	m_cursor->m_hit_detect_mode = 2;
	m_list = (ui::ListView *)m_box->FindChild(IDParam(s_empty_name, APPLAUNCHER_THEME_LIST), 0);
	m_list->SetScrollType(ui::ListView::SCROLL_TYPE_VERTICAL);
	m_list->SetGenerateMargin(math::v2(480.0f, 480.0f));
	m_list->SetItemFactory(new ApplauncherThemeChipFactory);
	m_list->ScePafWidget_7E9DC5B1(0, math::v2(816.0f, 360.0f));
	m_list->InsertSegment(0, 1);
	m_list->SetSegmentLayoutType(0, ui::ListView::LAYOUT_TYPE_GRID);
	m_list->ScePafWidget_27EB2AEB(0, 4);
	m_list->SetCellSizeDefault(0, math::v4(188.0f, 112.0f, 0.0f, 0.0f));
	m_list->InsertCell(0, 0, chips);
	m_list->BlockSync();
	m_list->UnblockSync();

	ui::Widget *close = m_box->FindChild(IDParam(s_empty_name, APPLAUNCHER_THEME_CLOSE), 0);

	close->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE, new ui::EventCBListener(onClose, NULL));

	LsdbPage page;
	int cursor;

	applauncherHomeGetPageParam(&page);
	if (page.themeFile.size() != 0) {
		cursor = 0;
		if (theme->get() != NULL) {
			for (SceUInt32 i = 0; i < APPLAUNCHER_THEME_WALLPAPERS; i++) {
				string path;

				shellThemeInfoGetFilePath(theme->get(), &path,
					(*theme)->m_homeProperty.m_bgParam[i].m_imageFilePath);
				if (page.themeFile == path && page.bgColor ==
				    (*theme)->m_homeProperty.m_bgParam[i].m_waveType &&
				    page.reserved01 ==
				    (*theme)->m_homeProperty.m_bgParam[i].GetFontColor()) {
					m_wallpaperIndex = i;
					cursor = s_imageChip;
					break;
				}
			}
		}
	} else if (page.bgColor == APPLAUNCHER_THEME_WAVE_CUSTOM) {
		cursor = s_customWaveChip;
	} else {
		cursor = 0;
		for (SceUInt32 i = 0; i < APPLAUNCHER_THEME_WAVES; i++) {
			if (page.bgColor == s_wavePresets[i]) {
				s_page.bgColor = s_wavePresets[i];
				cursor = i + s_firstWaveChip;
				break;
			}
		}
	}
	applauncherThemeSelectorSetCursor(cursor);
	ScePafCommon_6CF3A3C8(0.0f, m_root, common::transition::Type_SlideFromBottom4, true, false);
}

/* (0x810C6CD0) */
ApplauncherThemeSelector *applauncherThemeSelectorCreate(void)
{
	return new ApplauncherThemeSelector;
}

/* (0x810C6CE2) */
ApplauncherThemeSelector::~ApplauncherThemeSelector()
{
	ScePafCommon_B48C9BDC(0.0f, m_root, common::transition::Type_SlideFromBottom4, true, false);
	if (m_waitDialog >= 0)
		SceCommonGuiDialog_BC311DC0(m_waitDialog);
	if (m_loadJob.get() != NULL)
		m_loadJob->Cancel();
	s_selector = NULL;
	if (m_photoPicking)
		applauncherPhotoPickClose();
	if (m_wallpaperPicking)
		applauncherWallpaperPickClose();
}

/* (0x810C6D84) */
void applauncherThemeSelectorDestroy(void)
{
	if (s_selector != NULL)
		delete s_selector;
}

/* (0x810C6DA0) */
bool applauncherWaveColorsSet(void)
{
	return s_waveColorsSet;
}

/* the page chosen, once (0x810C6DAC) */
bool applauncherThemeSelectorGetPage(ApplauncherThemePage *page)
{
	if (!s_pageChosen)
		return false;
	s_pageChosen = false;
	page->themeFile = s_pageThemeFile;
	page->bgColor = s_page.bgColor;
	page->texWidth = s_page.texWidth;
	page->texHeight = s_page.texHeight;
	page->imageWidth = s_page.imageWidth;
	page->imageHeight = s_page.imageHeight;
	page->reserved01 = s_page.reserved01;
	return true;
}

/* the user's own wave's colours, from waveparam.bin (0x810C6DF8) */
void applauncherWaveSetColors(SceUInt32 index, ApplauncherWaveColors colors)
{
	if (index != APPLAUNCHER_THEME_WAVE_CUSTOM)
		return;
	s_waveColors = colors;
	s_waveColorsSet = true;
}

/* a wave's or the user's wave's chip chosen: the page given the wave (0x810C6E2A) */
static void onWaveChip(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	SceUInt32 chip = (SceUInt32)data;

	if (s_customWaveChip == chip)
		s_page.bgColor = APPLAUNCHER_THEME_WAVE_CUSTOM;
	else if (s_firstWaveChip <= chip)
		s_page.bgColor = s_wavePresets[chip - s_firstWaveChip];
	s_pageChosen = false;
	s_pageThemeFile = "";
	s_page.texWidth = 0;
	s_page.texHeight = 0;
	s_page.imageWidth = 0;
	s_page.imageHeight = 0;
	s_page.reserved01 = LSDB_PAGE_DEFAULT_RESERVED01;

	LsdbPage page;

	page.themeFile = s_pageThemeFile;
	page.bgColor = s_page.bgColor;
	page.reserved01 = s_page.reserved01;
	page.texWidth = s_page.texWidth;
	page.texHeight = s_page.texHeight;
	page.imageWidth = s_page.imageWidth;
	page.imageHeight = s_page.imageHeight;
	applauncherHomeSetPageParam(&page);
	applauncherPagesCleanTabpaper();
	s_selector->m_wallpaperIndex = -1;
	applauncherThemeSelectorSetCursor(chip);
}

/* the chooser's widgets let go, a picker over it (inline in its callers) */
static inline void applauncherThemeSelectorClose(void)
{
	ScePafCommon_B48C9BDC(0.0f, s_selector->m_frame, common::transition::Type_SlideFromBottom4,
			      true, false);
	s_selector->m_frame = NULL;
	s_selector->m_list = NULL;
	s_selector->m_box = NULL;
	s_selector->m_cursor = NULL;
	if (s_selector->m_loadJob.get() != NULL)
		s_selector->m_loadJob->Cancel();
}

/* the photo pick's chip chosen: the photo picker opened (0x810C6F3C) */
static void onPhotoChip(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	ShellPluginInterface *power =
		(ShellPluginInterface *)applauncherPluginGetInterface("power_manage_plugin", 1);

	((int (*)(void (*)(void *), void *))power->func[POWER_MANAGE_PLUGIN_FUNC_ADD_CALLBACK])(
		onPowerEvent, NULL);
	applauncherPhotoPickOpen(onPhotoPicked, NULL);
	s_selector->m_photoPicking = true;
	applauncherThemeSelectorClose();
}

/* the page's image's chip chosen: the wallpaper picker opened on it (0x810C6FAA) */
static void onImageChip(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	ShellPluginInterface *power =
		(ShellPluginInterface *)applauncherPluginGetInterface("power_manage_plugin", 1);

	((int (*)(void (*)(void *), void *))power->func[POWER_MANAGE_PLUGIN_FUNC_ADD_CALLBACK])(
		onPowerEvent, NULL);
	applauncherWallpaperPickOpen(s_selector->m_wallpaperIndex, 0, onWallpaperPicked, NULL);
	s_selector->m_wallpaperPicking = true;
	applauncherThemeSelectorClose();
}

/* the page's image loaded: shown on its chip (0x810C701E) */
static __attribute__((noinline)) void applauncherThemeSelectorSetImage(
	const intrusive_ptr<graph::Surface> &image)
{
	ApplauncherThemeSelector *selector = s_selector;

	if (selector == NULL)
		return;
	selector->m_image = image;
	if (s_selector->m_list == NULL)
		return;

	ui::Widget *widget =
		s_selector->m_list->FindChild(IDParam(s_empty_name, APPLAUNCHER_THEME_IMAGE), 0);

	if (widget == NULL)
		return;
	widget->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
	widget->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f), new Timer(150.0f, Timer::FUNC_QUAD_IN));
	widget->SetTexture(image);
}

/* PLACEHOLDER: the conversion job: a picked photo converted into the tabpaper folder
   (0x8152F3B8) */
class DxtConvertJob : public job::JobItem {
public:
	DxtConvertJob(const char *path, int width, int height);

	virtual ~DxtConvertJob();
	virtual int Run();
	virtual void Finish(int result);

	string m_srcPath;
	string m_dstPath;
	/* the converted file's sizes, in the page's order */
	int m_texWidth;
	int m_texHeight;
	int m_imageWidth;
	int m_imageHeight;
	int m_width;
	int m_height;
};

/* (0x810C71A4) */
__attribute__((noinline)) DxtConvertJob::DxtConvertJob(const char *path, int width, int height)
	: job::JobItem("DxtConvertJob", NULL), m_srcPath(path), m_texWidth(0), m_texHeight(0),
	  m_imageWidth(0), m_imageHeight(0), m_width(width), m_height(height)
{
	applauncherBusyBegin();
	g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_BUSY_BEGIN]();
}

/* a photo picked: converted with a dialog up (0x810C723C) */
static void onPhotoPicked(ApplauncherPhotoPickResult *result)
{
	ShellPluginInterface *power =
		(ShellPluginInterface *)applauncherPluginGetInterface("power_manage_plugin", 1);

	((int (*)(void (*)(void *), void *))power->func[POWER_MANAGE_PLUGIN_FUNC_REMOVE_CALLBACK])(
		onPowerEvent, NULL);
	if (s_selector == NULL) {
		s_pickInterrupted = false;
		s_pageChosen = false;
		return;
	}
	s_selector->m_photoPicking = false;
	if (s_pickInterrupted) {
		s_pickInterrupted = false;
		s_pageChosen = false;
		g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_EDIT);
		return;
	}
	if (result->result != 0) {
		s_pageChosen = false;
		g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_EDIT);
		return;
	}

	wstring text(g_applauncherPlugin->GetString(
		IDParam(s_empty_name, APPLAUNCHER_THEME_CONVERT_TEXT)));

	s_selector->m_waitDialog = SceCommonGuiDialog_6CE14F66(g_applauncherPlugin, wstring(), text,
		&SceCommonGuiDialog_2A68B4A4, NULL, NULL);

	common::SharedPtr<job::JobItem> job(
		new DxtConvertJob(result->path, result->width, result->height));

	job::JobQueue::default_queue->Enqueue(job);
}

/* a power event during a pick: the pick given up (0x810C74BC) */
static void onPowerEvent(void *arg)
{
	s_pickInterrupted = true;
	s_pageChosen = false;
	g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_EDIT);

	ShellPluginInterface *power =
		(ShellPluginInterface *)applauncherPluginGetInterface("power_manage_plugin", 1);

	((int (*)(void (*)(void *), void *))power->func[POWER_MANAGE_PLUGIN_FUNC_REMOVE_CALLBACK])(
		onPowerEvent, NULL);
	applauncherPhotoPickClose();
	if (s_selector != NULL)
		s_selector->m_photoPicking = false;
}

/* a wallpaper picked: the page given it (0x810C751C) */
static void onWallpaperPicked(int index)
{
	ShellPluginInterface *power =
		(ShellPluginInterface *)applauncherPluginGetInterface("power_manage_plugin", 1);

	((int (*)(void (*)(void *), void *))power->func[POWER_MANAGE_PLUGIN_FUNC_REMOVE_CALLBACK])(
		onPowerEvent, NULL);
	if (s_selector == NULL) {
		s_pickInterrupted = false;
		s_pageChosen = false;
		return;
	}
	s_selector->m_wallpaperPicking = false;
	if (s_pickInterrupted) {
		s_pickInterrupted = false;
		s_pageChosen = false;
		g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_EDIT);
		return;
	}
	s_pageChosen = false;
	if (index >= 0 && index < APPLAUNCHER_THEME_WALLPAPERS) {
		common::SharedPtr<LsdbThemeInfo> *theme = shellThemeGetInfo();

		if (theme->get() != NULL) {
			s_pageChosen = true;
			shellThemeInfoGetFilePath(theme->get(), &s_pageThemeFile,
				(*theme)->m_homeProperty.m_bgParam[index].m_imageFilePath);
			s_page.bgColor = (*theme)->m_homeProperty.m_bgParam[index].m_waveType;
			s_page.reserved01 = (*theme)->m_homeProperty.m_bgParam[index].GetFontColor();
			s_page.texWidth = APPLAUNCHER_THEME_PAGE_WIDTH;
			s_page.texHeight = APPLAUNCHER_THEME_PAGE_HEIGHT;
			s_page.imageWidth = APPLAUNCHER_THEME_PAGE_WIDTH;
			s_page.imageHeight = APPLAUNCHER_THEME_PAGE_HEIGHT;
		}
	}
	g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_EDIT);
}

/* the chooser's close button (0x810C7618) */
static void onClose(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	s_pageChosen = false;
	g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_EDIT);
}

/* the cursor, destroyed with its chip, hidden and kept under the box (0x810C763C) */
static ui::Widget *onCursorKeep(ui::Widget *cursor, void *arg)
{
	if (s_selector == NULL)
		return NULL;
	cursor->m_transition_complete = false;
	cursor->m_transition = 0xff;
	cursor->DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
	return s_selector->m_box;
}

/* PLACEHOLDER: the conversion's error dialog's handler (0x8152F3A4) */
class ApplauncherThemeErrorHandler : public CommonGuiDialogHandler {
public:
	virtual void operator()(SceInt32 value, SceInt32 button);
};

/* the error dialog closed: the edit back (0x810C769E) */
void ApplauncherThemeErrorHandler::operator()(SceInt32 value, SceInt32 button)
{
	applauncherHomeShowOverlay();
	g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_EDIT);
	SceCommonGuiDialog_BC311DC0(value);
}

/* (0x810C76C4, 0x810C7712) */
DxtConvertJob::~DxtConvertJob()
{
}

/* the photo converted into a file of its tick (0x810C7766) */
int DxtConvertJob::Run()
{
	SceUInt64 tick;
	char path[1024];
	PhotoExportConvertParam param;
	PhotoExportConvertResult result;

	rtc::GetCurrentTick(&tick);
	if (!LocalFile::IsDirectory(APPLAUNCHER_THEME_TABPAPER_DIR))
		Dir::Create(APPLAUNCHER_THEME_TABPAPER_DIR);
	sce_paf_snprintf(path, sizeof(path), "%s%llu.dds", APPLAUNCHER_THEME_TABPAPER_DIR, tick);
	param.srcPath = m_srcPath.c_str();
	param.dstPath = path;
	param.reserved = m_width;
	param.type = m_height;

	int ret = photoExportConvertFile(&param, NULL, NULL, NULL, &result);

	if (ret == 0) {
		m_dstPath = path;
		m_imageHeight = result.imageHeight;
		m_texWidth = result.texWidth;
		m_texHeight = result.texHeight;
		m_imageWidth = result.imageWidth;
	}
	return ret;
}

/* the page given the converted photo, or the error shown (0x810C7828) */
void DxtConvertJob::Finish(int result)
{
	applauncherBusyEnd();
	g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_BUSY_END]();
	if (result == 0) {
		s_pageChosen = true;
		s_pageThemeFile = m_dstPath;
		s_page.bgColor = -1;
		s_page.texWidth = m_texWidth;
		s_page.texHeight = m_texHeight;
		s_page.imageWidth = m_imageWidth;
		s_page.imageHeight = m_imageHeight;
		s_page.reserved01 = LSDB_PAGE_DEFAULT_RESERVED01;
		g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_EDIT);
		return;
	}
	s_pageChosen = false;

	int ret;

	{
		CommonGuiMessageDialogParam param;

		param.type = result;
		param.cancelButton = 1;
		if (result == PHOTO_EXPORT_ERROR_TOO_LARGE ||
		    result == PHOTO_EXPORT_ERROR_TOO_LARGE2) {
			param.messageId = APPLAUNCHER_THEME_TOO_LARGE_TEXT;
			param.cancelButton = 0;
		} else if (result == PHOTO_EXPORT_ERROR_UNSUPPORTED ||
			   result == PHOTO_EXPORT_ERROR_UNSUPPORTED2) {
			param.messageId = APPLAUNCHER_THEME_UNSUPPORTED_TEXT;
			param.cancelButton = 0;
		}
		ret = SceCommonGuiDialog_A6422883(g_applauncherPlugin, &param,
						  new ApplauncherThemeErrorHandler, NULL);
	}
	/* the edit back at once without the dialog */
	if (ret < 0)
		g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_EDIT);
}

/* (0x810C7A88, 0x810C7AC4) */
LoadImageJob::~LoadImageJob()
{
}

/* the page's image read (0x810C7B04) */
int LoadImageJob::Run()
{
	int error;
	common::SharedPtr<File> file = LocalFile::Open(m_path.c_str(), SCE_O_RDONLY, 0xF000,
							    &error);

	if (file.get() == NULL || !file->IsOpened())
		return error;

	graph::Surface::LoadOption option;

	option.decode_heap = Framework::Instance()->GetDecodeHeap();
	option.data_load_heap = Framework::Instance()->GetDecodeHeap();
	m_surface = graph::Surface::Load(g_applauncherPlugin->m_surface_pool, file, &option);
	file->Close();
	return 0;
}

/* shown on its chip (0x810C7D12) */
void LoadImageJob::Finish(int result)
{
	if (IsCanceled())
		return;
	if (result != 0)
		return;
	applauncherThemeSelectorSetImage(intrusive_ptr<graph::Surface>(m_surface));
}

/* a preset's place in the waves' order, -1 for none (inline in its caller) */
static inline int applauncherWaveFind(int preset)
{
	for (SceUInt32 i = 0; i < APPLAUNCHER_THEME_WAVES; i++) {
		if (s_wavePresets[i] == preset)
			return i;
	}
	return -1;
}

/* (0x810C7D5A) */
bool applauncherWaveGetColors(int preset, math::v4 *color0, math::v4 *color1)
{
	int wave = applauncherWaveFind(preset);

	if (wave < 0)
		return false;
	*color0 = s_waveColors0[wave];
	*color1 = s_waveColors1[wave];
	return true;
}

/* a chip: the photo pick's, the page's image's, the user's wave's or a wave's (0x814120DC) */
ui::ListItem *ApplauncherThemeChipFactory::Create(CreateParam &param)
{
	char name[16];
	ui::ListItem *item = new ui::ListItem(param.m_parent, NULL);

	sce_paf_snprintf(name, sizeof(name), "%s%d", "chipItem", param.m_cell);
	item->m_name.SetID(string(name));
	item->SetSize(param.m_list_view->GetCellSize(param.m_segment, param.m_cell));
	item->SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	if (param.m_cell == 0) {
		g_applauncherPlugin->TemplateOpen(item,
			IDParam(s_empty_name, APPLAUNCHER_THEME_PHOTO_CHIP), Plugin::TemplateOpenParam());

		ui::Widget *button =
			item->FindChild(IDParam(s_empty_name, APPLAUNCHER_THEME_PHOTO_BUTTON), 0);

		if (button != NULL)
			button->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
				new ui::EventCBListener(onPhotoChip, (void *)param.m_cell));
	}
	if (s_imageChip == param.m_cell) {
		g_applauncherPlugin->TemplateOpen(item,
			IDParam(s_empty_name, APPLAUNCHER_THEME_IMAGE_CHIP), Plugin::TemplateOpenParam());

		ui::Widget *image = item->FindChild(IDParam(s_empty_name, APPLAUNCHER_THEME_IMAGE), 0);

		if (image != NULL) {
			image->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
				new ui::EventCBListener(onImageChip, (void *)param.m_cell));
			image->SetTexture(Framework::Instance()->GetCommonTexture(
				IDParam(s_empty_name, APPLAUNCHER_THEME_NO_IMAGE_TEXTURE)));
			if (s_selector != NULL &&
			    intrusive_ptr<graph::Surface>(s_selector->m_image).get() != NULL)
				image->SetTexture(s_selector != NULL ? s_selector->m_image :
						  intrusive_ptr<graph::Surface>());
		}
	}
	if (s_customWaveChip == param.m_cell) {
		g_applauncherPlugin->TemplateOpen(item,
			IDParam(s_empty_name, APPLAUNCHER_THEME_WAVE_CHIP), Plugin::TemplateOpenParam());

		ui::Widget *color =
			item->FindChild(IDParam(s_empty_name, APPLAUNCHER_THEME_WAVE_COLOR0), 0);

		if (color != NULL) {
			color->SetColor(s_waveColors.color[0]);
			color->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
				new ui::EventCBListener(onWaveChip, (void *)param.m_cell));
		}
		color = item->FindChild(IDParam(s_empty_name, APPLAUNCHER_THEME_WAVE_COLOR1), 0);
		if (color != NULL)
			color->SetColor(s_waveColors.color[1]);
	}
	if (s_firstWaveChip <= (SceUInt32)param.m_cell) {
		SceUInt32 wave = param.m_cell - s_firstWaveChip;

		g_applauncherPlugin->TemplateOpen(item,
			IDParam(s_empty_name, APPLAUNCHER_THEME_WAVE_CHIP), Plugin::TemplateOpenParam());

		ui::Widget *color =
			item->FindChild(IDParam(s_empty_name, APPLAUNCHER_THEME_WAVE_COLOR0), 0);

		if (color != NULL) {
			color->SetColor(s_waveColors0[wave]);
			color->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
				new ui::EventCBListener(onWaveChip, (void *)param.m_cell));
		}
		color = item->FindChild(IDParam(s_empty_name, APPLAUNCHER_THEME_WAVE_COLOR1), 0);
		if (color != NULL)
			color->SetColor(s_waveColors1[wave]);
	}
	if (s_cursorChip == param.m_cell)
		applauncherThemeSelectorSetCursor(s_cursorChip);
	return item;
}
