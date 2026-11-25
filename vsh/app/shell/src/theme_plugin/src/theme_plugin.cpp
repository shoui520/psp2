/*
 * theme_plugin.cpp: SceShell's theme wallpaper picker plugin (theme_plugin)
 * 3.65-CEX: 0x8126E4EC - 0x8126F6A7, ThemeChipItemFactory::Create at 0x8142FA94
 *
 * Displays the current theme's unique background wallpapers in a 2-column grid ListView,
 * highlights the currently selected page background with an animated check mark, and notifies
 * the caller plugin with the chosen page index via paf::Plugin::Notify().
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <std/memory>
#include <framework.h>
#include <plugin.h>
#include <timer/timer.h>
#include <cache/cache_manager.h>
#include <cache/cache_image.h>
#include <common/transition.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_scene.h>
#include <widget/w_list_view.h>
#include <widget/w_list_item.h>

#include "shell.h"
#include "plugins.h"
#include "theme.h"

using namespace paf;

namespace {

enum {
	THEME_PLUGIN_DRAW_PRIORITY		= 50,
	THEME_PAGE_ID_PICKER			= 0x42AC930F,	/* 1118647759 */
	THEME_WIDGET_HIT_BG			= 0x7C40341D,	/* 2084625597 */
	THEME_WIDGET_CONTAINER			= -526525848,	/* 0xE09DB6A8 */
	THEME_WIDGET_CHECKMARK			= 0x47C8B5D3,	/* 1204373587 */
	THEME_WIDGET_LISTVIEW			= 0x70522AA9,	/* 1884374761 */
	THEME_WIDGET_TITLE_TEXT			= -514682245,	/* 0xE152737B */
	THEME_WIDGET_BACK_BUTTON		= -1073112782,	/* 0xC0096B32 */
	THEME_TEMPLATE_CHIP_ITEM		= -184800941,	/* 0xF4FC1D53 */
	THEME_WIDGET_CHIP_WAVE_BOTTOM		= 0x211C24FE,	/* 555478398 */
	THEME_WIDGET_CHIP_WAVE_TOP		= -534303616,	/* 0xE0272880 */
	THEME_WIDGET_CHIP_THUMBNAIL		= -1264934432,	/* 0xB49AA9E0 */
	THEME_COMMON_TEX_LOADING		= -1344341552,	/* 0xAFDF05D0 */

	THEME_EVENT_BUTTON_DECIDE		= 0x10000008,	/* 268435464 */
	THEME_EVENT_IMAGE_LOADED		= 0x00100005,	/* 1048581 */
	THEME_ANIM_ID_COLOR			= 0x00010001,	/* 65537 */
	THEME_ANIM_ID_SIZE			= 0x00010004,	/* 65540 */
	THEME_ANIM_ID_SCALE			= 0x00010005,	/* 65541 */

	APPLAUNCHER_SLOT_GET_WAVE_COLORS	= 27,		/* +108 */
	THEME_CACHE_PRIORITY			= 10,
	THEME_GRID_COLUMNS			= 2,
	THEME_MAX_CHIPS				= LSDB_THEME_PAGES,
};

struct ThemePluginInterface {
	void *reserved;
};

static const char *s_chipItemPrefix = "chipItem";		/* 0x8154570C */
static int s_selectedChipIndex = -1;				/* 0x81545710 */
static ThemePluginInterface *s_themeInterfacePtr = NULL;	/* 0x815AC43C */
static Plugin *s_themePlugin = NULL;				/* 0x815AC440 */
static ThemePluginInterface s_themeInterface;			/* 0x815AC444 */

class ThemePickerManager;
static ThemePickerManager *s_pickerManager = NULL;		/* 0x815AC448 */

static void selectThemeChip(int chipIndex);
static void closePickerWithTransition(void);
static void onChipSelected(int32_t eventType, ui::Handler *self, ui::Event *event, void *userData);
static void onBackButtonPressed(int32_t eventType, ui::Handler *self, ui::Event *event, void *userData);
static ui::Widget *onCheckMarkKeep(ui::Widget *widget, void *userData);
static void onChipImageLoaded(int32_t eventType, ui::Handler *self, ui::Event *event, void *userData);
static void onPickerPageDeleted(ui::Widget *widget, void *userData);

static const wstring &getLocalizedThemeText(const LsdbThemeText &text, Locale locale)
{
	if ((unsigned int)locale < LSDB_THEME_LANGUAGES &&
	    text.m_language[locale].size() != 0)
		return text.m_language[locale];
	return text.m_default;
}

class ThemeChipItemFactory : public ui::listview::ItemFactory {
public:
	virtual ~ThemeChipItemFactory() {}
	virtual ui::ListItem *Create(CreateParam &param);
};

class ThemePickerManager {
public:
	ThemePickerManager(Plugin *plugin, int initialIndex);
	~ThemePickerManager();

	void Open();
	void CloseOnStop();

	int GetSourcePageIndex(int chipIndex) const
	{
		if (chipIndex >= 0 && chipIndex < THEME_MAX_CHIPS)
			return m_chipToPageIndex[chipIndex];
		return 0;
	}

	intrusive_ptr<cache::CacheImageInterface> GetChipImage(int chipIndex) const
	{
		if (chipIndex >= 0 && chipIndex < THEME_MAX_CHIPS)
			return m_chipImages[chipIndex];
		return intrusive_ptr<cache::CacheImageInterface>();
	}

	ui::Widget *GetContainer() const { return m_container; }
	ui::Widget *GetCheckMark() const { return m_checkMark; }
	ui::ListView *GetListView() const { return m_listView; }
	cache::CacheManager *GetCacheManager() const { return m_cacheManager; }

private:
	int m_initialPageIndex;
	cache::CacheManager *m_cacheManager;
	ui::Widget *m_page;
	ui::Widget *m_container;
	ui::Widget *m_checkMark;
	ui::ListView *m_listView;
	intrusive_ptr<cache::CacheImageInterface> m_chipImages[THEME_MAX_CHIPS];
	int m_chipToPageIndex[THEME_MAX_CHIPS];
	int m_uniqueChipCount;
};

/* 0x8126E61C */
ThemePickerManager::ThemePickerManager(Plugin *plugin, int initialIndex)
	: m_initialPageIndex(initialIndex),
	  m_cacheManager(NULL),
	  m_page(NULL),
	  m_container(NULL),
	  m_checkMark(NULL),
	  m_listView(NULL),
	  m_uniqueChipCount(0)
{
	s_pickerManager = this;
	for (int i = 0; i < THEME_MAX_CHIPS; ++i)
		m_chipToPageIndex[i] = -1;

	cache::CacheManager::Params cacheParams;
	cacheParams.name = "ThemeCacheManager";
	cacheParams.longlife_heap = plugin->m_decode_heap;
	cacheParams.instant_heap = Framework::Instance()->GetDecodeHeap();
	cacheParams.surface_pool = plugin->m_surface_pool;
	cacheParams.cache_longlife_heap = false;
	cacheParams.image_cache_divide_thread = true;

	m_cacheManager = new cache::CacheManager(&cacheParams);
}

/* 0x8126E77C */
ThemePickerManager::~ThemePickerManager()
{
	if (m_cacheManager != NULL) {
		delete m_cacheManager;
		m_cacheManager = NULL;
	}
	s_pickerManager = NULL;
}

/* 0x8126E7C0 */
static void selectThemeChip(int chipIndex)
{
	if (s_pickerManager == NULL)
		return;

	s_selectedChipIndex = chipIndex;
	ui::Widget *checkMark = s_pickerManager->GetCheckMark();
	if (chipIndex < 0) {
		checkMark->Hide(0xFF, NULL, NULL, 0.0f);
		return;
	}

	char chipName[16];
	sce_paf_snprintf(chipName, sizeof(chipName), "%s%d", s_chipItemPrefix, chipIndex);
	IDParam chipId(chipName);
	ui::Widget *chipWidget = s_pickerManager->GetListView()->FindChild(chipId, 0);
	if (chipWidget == NULL)
		return;

	checkMark->Show(0xFF, NULL, NULL, 0.0f);

	math::v4 transparentWhite(1.0f, 1.0f, 1.0f, 0.0f);
	checkMark->SetColor(transparentWhite, NULL, THEME_ANIM_ID_COLOR, 0, 0.0f, NULL, NULL);
	checkMark->SetParent(chipWidget, NULL, 0);

	math::v4 startScale(2.0f, 2.0f, 1.0f, 1.0f);
	checkMark->SetScale(startScale, NULL, THEME_ANIM_ID_SCALE, 0, 0.0f, NULL, NULL);

	math::v4 targetScale(1.0f, 1.0f, 1.0f, 1.0f);
	Timer *scaleTimer = new Timer(34.0f);
	checkMark->SetScale(targetScale, scaleTimer, THEME_ANIM_ID_SCALE, 0, 0.0f, NULL, NULL);

	math::v4 opaqueWhite(1.0f, 1.0f, 1.0f, 1.0f);
	Timer *colorTimer = new Timer(7.0f);
	checkMark->SetColor(opaqueWhite, colorTimer, THEME_ANIM_ID_COLOR, 0, 0.0f, NULL, NULL);
}

/* 0x8126EAB0 */
void ThemePickerManager::Open()
{
	Plugin::PageOpenParam openParam;
	IDParam pageId((SceUInt32)THEME_PAGE_ID_PICKER);
	m_page = s_themePlugin->PageOpen(pageId, openParam);
	if (m_page != NULL)
		m_page->AddDeleteCB(onPickerPageDeleted, NULL);

	IDParam hitBgId((SceUInt32)THEME_WIDGET_HIT_BG);
	ui::Widget *hitBg = m_page->FindChild(hitBgId, 0);
	if (hitBg != NULL) {
		hitBg->m_hit_find_mode = 0;
		hitBg->m_hit_detect_mode = 0;
		ScePafWidget_42C5A4D4(hitBg, 1, 0);
		ScePafWidget_164BFFA5(hitBg, 32);
	}

	IDParam containerId((SceUInt32)THEME_WIDGET_CONTAINER);
	m_container = m_page->FindChild(containerId, 0);

	IDParam checkId((SceUInt32)THEME_WIDGET_CHECKMARK);
	m_checkMark = m_container->FindChild(checkId, 0);
	if (m_checkMark != NULL) {
		m_checkMark->m_keep_cb = onCheckMarkKeep;
		m_checkMark->m_keep_arg = NULL;
		m_checkMark->m_hit_detect_mode = 2;
		m_checkMark->m_hit_find_mode = 2;
		m_checkMark->Hide(0xFF, NULL, NULL, 0.0f);
	}

	IDParam listViewId((SceUInt32)THEME_WIDGET_LISTVIEW);
	m_listView = (ui::ListView *)m_container->FindChild(listViewId, 0);
	m_listView->SetScrollType((ui::ListView::ScrollType)0);
	m_listView->SetGenerateMargin(math::v2(480.0f, 480.0f));
	m_listView->SetItemFactory(new ThemeChipItemFactory());
	m_listView->ScePafWidget_7E9DC5B1(0, math::v2(0.0f, 360.0f));
	m_listView->InsertSegment(0, 1);
	m_listView->SetSegmentLayoutType(0, (ui::ListView::LayoutType)1);
	m_listView->ScePafWidget_27EB2AEB(0, THEME_GRID_COLUMNS);
	m_listView->SetCellSizeDefault(0, math::v4(390.0f, 222.0f, 0.0f, 0.0f));

	common::SharedPtr<LsdbThemeInfo> *themeInfoPtr = shellThemeGetInfo();
	if (themeInfoPtr != NULL && themeInfoPtr->get() != NULL) {
		LsdbThemeInfo *themeInfo = themeInfoPtr->get();

		IDParam titleId((SceUInt32)THEME_WIDGET_TITLE_TEXT);
		ui::Widget *titleWidget = m_page->FindChild(titleId, 0);
		if (titleWidget != NULL) {
			Locale locale = Framework::Instance()->GetLocale();
			titleWidget->SetString(getLocalizedThemeText(
				themeInfo->m_infomationProperty.m_title, locale));
		}

		for (int i = 0; i < THEME_MAX_CHIPS; ++i) {
			const BackgroundParam &candidate = themeInfo->m_homeProperty.m_bgParam[i];
			if (candidate.m_imageFilePath.size() == 0)
				continue;

			bool duplicate = false;
			for (int j = 0; j < m_uniqueChipCount; ++j) {
				int existingIdx = m_chipToPageIndex[j];
				if (existingIdx < 0)
					continue;
				const BackgroundParam &existing =
					themeInfo->m_homeProperty.m_bgParam[existingIdx];
				if (candidate.m_imageFilePath == existing.m_imageFilePath &&
				    candidate.m_waveType == existing.m_waveType &&
				    candidate.GetFontColor() == existing.GetFontColor()) {
					duplicate = true;
					break;
				}
			}

			if (!duplicate) {
				m_chipToPageIndex[m_uniqueChipCount] = i;
				++m_uniqueChipCount;
			}
		}

		if (m_initialPageIndex >= 0 && m_initialPageIndex < THEME_MAX_CHIPS) {
			const BackgroundParam &initialBg =
				themeInfo->m_homeProperty.m_bgParam[m_initialPageIndex];
			for (int j = 0; j < m_uniqueChipCount; ++j) {
				const BackgroundParam &existing =
					themeInfo->m_homeProperty.m_bgParam[m_chipToPageIndex[j]];
				if (initialBg.m_imageFilePath == existing.m_imageFilePath &&
				    initialBg.m_waveType == existing.m_waveType &&
				    initialBg.GetFontColor() == existing.GetFontColor()) {
					m_initialPageIndex = j;
					break;
				}
			}
		}

		for (int j = 0; j < m_uniqueChipCount; ++j) {
			const BackgroundParam &bg =
				themeInfo->m_homeProperty.m_bgParam[m_chipToPageIndex[j]];
			if (bg.m_imageFilePath.size() == 0)
				continue;

			string thumbPath;
			shellThemeInfoGetFilePath(themeInfo, &thumbPath, bg.m_thumbnailFilePath);
			if (m_cacheManager != NULL) {
				cache::CacheImage::Option cacheOpt;
				sce_paf_memset(&cacheOpt, 0, sizeof(cacheOpt));
				cacheOpt.auto_delete = true;
				cacheOpt.retry = true;
				cacheOpt.retry_count = THEME_CACHE_PRIORITY;
				cacheOpt.reserved = true;
				cacheOpt.direct_load = true;
				cacheOpt.image_order_param = -1;
				if (m_cacheManager->CreateCache(thumbPath, m_chipImages[j],
								&cacheOpt, NULL, NULL) == 0) {
					intrusive_ptr<graph::Surface> preload =
						m_chipImages[j]->GetImage();
					(void)preload;
				}
			}
		}

		m_listView->InsertCell(0, 0, (uint32_t)m_uniqueChipCount);
	}

	m_listView->BlockSync();
	m_listView->UnblockSync();

	IDParam backBtnId((SceUInt32)THEME_WIDGET_BACK_BUTTON);
	ui::Widget *backButton = m_container->FindChild(backBtnId, 0);
	if (backButton != NULL) {
		backButton->AddEventListener(THEME_EVENT_BUTTON_DECIDE,
			new ui::EventCBListener(onBackButtonPressed, NULL), false);
	}

	selectThemeChip(m_initialPageIndex);
	ScePafCommon_6CF3A3C8(0.0f, m_page, (common::transition::Type)15, true, false);
}

/* 0x8126F1EC */
void ThemePickerManager::CloseOnStop()
{
	if (m_cacheManager != NULL)
		m_cacheManager->RequestShutdown();

	IDParam pageId((SceUInt32)THEME_PAGE_ID_PICKER);
	Plugin::PageCloseParam closeParam;
	s_themePlugin->PageClose(pageId, closeParam);
}

/* 0x8126F298 */
static void closePickerWithTransition(void)
{
	Plugin::PageCloseParam closeParam;
	closeParam.transition_type = (Plugin::TransitionType)2;
	IDParam pageId((SceUInt32)THEME_PAGE_ID_PICKER);
	s_themePlugin->PageClose(pageId, closeParam);
}

/* 0x8126F340 */
static void onChipSelected(int32_t eventType, ui::Handler *self, ui::Event *event, void *userData)
{
	(void)eventType;
	(void)self;
	(void)event;
	int chipIndex = (int)(intptr_t)userData;
	int sourcePageIndex = 0;
	if (s_pickerManager != NULL)
		sourcePageIndex = s_pickerManager->GetSourcePageIndex(chipIndex);
	s_themePlugin->Notify(sourcePageIndex);
	selectThemeChip(chipIndex);
	closePickerWithTransition();
}

/* 0x8126F376 */
static void onBackButtonPressed(int32_t eventType, ui::Handler *self, ui::Event *event,
	void *userData)
{
	(void)eventType;
	(void)self;
	(void)event;
	(void)userData;
	closePickerWithTransition();
}

/* 0x8126F37E */
static ui::Widget *onCheckMarkKeep(ui::Widget *widget, void *userData)
{
	(void)userData;
	if (s_pickerManager == NULL)
		return NULL;
	widget->Hide(0xFF, NULL, NULL, 0.0f);
	return s_pickerManager->GetContainer();
}

/* 0x8126F3E0 */
static void onChipImageLoaded(int32_t eventType, ui::Handler *self, ui::Event *event,
	void *userData)
{
	(void)eventType;
	(void)event;
	(void)userData;
	ui::Widget *thumbWidget = (ui::Widget *)self;
	ui::Widget *parentItem = thumbWidget->m_parent;

	IDParam bottomId((SceUInt32)THEME_WIDGET_CHIP_WAVE_BOTTOM);
	ui::Widget *waveBottom = parentItem->FindChild(bottomId, 0);
	IDParam topId((SceUInt32)THEME_WIDGET_CHIP_WAVE_TOP);
	ui::Widget *waveTop = parentItem->FindChild(topId, 0);

	thumbWidget->Show(0xFF, new Timer(0.0f), NULL, 0.0f);
	if (waveBottom != NULL)
		waveBottom->Show(0xFF, new Timer(0.0f), NULL, 0.0f);
	if (waveTop != NULL)
		waveTop->Show(0xFF, new Timer(0.0f), NULL, 0.0f);
}

/* 0x8126F694 */
static void onPickerPageDeleted(ui::Widget *widget, void *userData)
{
	(void)widget;
	(void)userData;
	Plugin::UnloadAsync("theme_plugin", NULL, 0);
}

/* 0x8142FA94 */
ui::ListItem *ThemeChipItemFactory::Create(CreateParam &param)
{
	ui::ListItem *item = new ui::ListItem(param.m_parent, NULL);
	if (item == NULL)
		return NULL;

	char chipName[16];
	sce_paf_snprintf(chipName, sizeof(chipName), "%s%d", s_chipItemPrefix, param.m_cell);
	IDParam itemId(chipName);
	item->m_name = itemId;

	math::v4 cellSize = param.m_list_view->GetCellSize(param.m_segment, param.m_cell);
	item->SetSize(cellSize, NULL, THEME_ANIM_ID_SIZE, 0, 0.0f, NULL, NULL);
	math::v4 clearColor(0.0f, 0.0f, 0.0f, 0.0f);
	item->SetColor(clearColor, NULL, THEME_ANIM_ID_COLOR, 0, 0.0f, NULL, NULL);

	common::SharedPtr<LsdbThemeInfo> *themeInfoPtr = shellThemeGetInfo();
	if (themeInfoPtr != NULL && themeInfoPtr->get() != NULL) {
		LsdbThemeInfo *themeInfo = themeInfoPtr->get();

		IDParam tmplId((SceUInt32)THEME_TEMPLATE_CHIP_ITEM);
		Plugin::TemplateOpenParam tmplParam;
		s_themePlugin->TemplateOpen(item, tmplId, tmplParam);

		IDParam bottomId((SceUInt32)THEME_WIDGET_CHIP_WAVE_BOTTOM);
		ui::Widget *waveBottom = item->FindChild(bottomId, 0);
		IDParam topId((SceUInt32)THEME_WIDGET_CHIP_WAVE_TOP);
		ui::Widget *waveTop = item->FindChild(topId, 0);
		IDParam thumbId((SceUInt32)THEME_WIDGET_CHIP_THUMBNAIL);
		ui::Widget *thumbWidget = item->FindChild(thumbId, 0);

		int sourcePageIndex = 0;
		if (s_pickerManager != NULL)
			sourcePageIndex = s_pickerManager->GetSourcePageIndex(param.m_cell);
		const BackgroundParam &bg = themeInfo->m_homeProperty.m_bgParam[sourcePageIndex];

		Plugin *launcher = Plugin::Find("applauncher_plugin");
		math::v4 topColor(0.0f, 0.0f, 0.0f, 0.0f);
		math::v4 bottomColor(0.0f, 0.0f, 0.0f, 0.0f);
		int hasWaveColors = 0;
		if (launcher != NULL) {
			ShellPluginInterface *iface =
				(ShellPluginInterface *)launcher->GetInterface(1);
			if (iface != NULL) {
				int (*getWaveColorsFn)(int, math::v4 *, math::v4 *) =
					(int (*)(int, math::v4 *, math::v4 *))iface->func[APPLAUNCHER_SLOT_GET_WAVE_COLORS];
				hasWaveColors = getWaveColorsFn(bg.m_waveType, &topColor, &bottomColor);
			}
		}

		if (waveTop != NULL && hasWaveColors != 0) {
			waveTop->SetColor(topColor, NULL, THEME_ANIM_ID_COLOR, 0, 0.0f, NULL, NULL);
			waveTop->Hide(0xFF, NULL, NULL, 0.0f);
		}
		if (waveBottom != NULL && hasWaveColors != 0) {
			waveBottom->SetColor(bottomColor, NULL, THEME_ANIM_ID_COLOR, 0, 0.0f, NULL, NULL);
			waveBottom->Hide(0xFF, NULL, NULL, 0.0f);
		}

		if (thumbWidget != NULL) {
			if (bg.m_thumbnailFilePath.size() != 0) {
				IDParam loadingIconId((SceUInt32)THEME_COMMON_TEX_LOADING);
				intrusive_ptr<graph::Surface> loadingIconTex =
					Framework::Instance()->GetCommonTexture(loadingIconId);
				thumbWidget->SetTexture(loadingIconTex);

				intrusive_ptr<cache::CacheImageInterface> cachedImg;
				if (s_pickerManager != NULL)
					cachedImg = s_pickerManager->GetChipImage(param.m_cell);

				thumbWidget->AddEventListener(THEME_EVENT_IMAGE_LOADED,
					new ui::EventCBListener(onChipImageLoaded, NULL), true);
				thumbWidget->Hide(0xFF, NULL, NULL, 0.0f);
				thumbWidget->SetDrawObjMaskBit(&cachedImg, 0, 0);
			}

			thumbWidget->AddEventListener(THEME_EVENT_BUTTON_DECIDE,
				new ui::EventCBListener(onChipSelected, (void *)(intptr_t)param.m_cell),
				false);
		}
	}

	if (s_selectedChipIndex == param.m_cell)
		selectThemeChip(s_selectedChipIndex);

	return item;
}

} /* anonymous namespace */

/* 0x8126E4EC */
void themePluginSetParam(Plugin::InitParam2 *param)
{
	param->draw_priority = THEME_PLUGIN_DRAW_PRIORITY;
}

/* 0x8126E4F2 */
void themePluginInit(Plugin *plugin)
{
	bool hasArg = false;
	Plugin::Argument selectArg = plugin->GetArg("select_idx", &hasArg);
	int selectIndex = hasArg ? (int)selectArg : -1;

	s_themePlugin = plugin;
	s_themeInterfacePtr = &s_themeInterface;
	plugin->SetInterface(1, &s_themeInterface);
	new ThemePickerManager(plugin, selectIndex);
}

/* 0x8126E568 */
void themePluginStart(Plugin *plugin)
{
	(void)plugin;
	if (s_pickerManager != NULL)
		s_pickerManager->Open();
}

/* 0x8126E570 */
void themePluginStop(Plugin *plugin)
{
	(void)plugin;
	if (s_pickerManager != NULL)
		s_pickerManager->CloseOnStop();
}

/* 0x8126E578 */
void themePluginExit(Plugin *plugin)
{
	(void)plugin;
	if (s_pickerManager != NULL)
		delete s_pickerManager;
}
