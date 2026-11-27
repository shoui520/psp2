/*
 * applauncher_home.cpp: the home screen (0x810A888C-0x810B297C): its pages in their scroller, the
 * page indicator, the wave behind them, and the functions the plugin's table gives topmenu. The
 * file's and every name here are ours (PLACEHOLDER): no record or string of the 3.6x shell names
 * them.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/dipsw.h>
#include <plugin.h>
#include <std/stdio.h>
#include <std/math.h>
#include <timer/timer.h>
#include <system/system.h>
#include <math/math.h>
#include <versatileparam/idparam.h>
#include <graphics/surface/surface.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/effects/wave_switch_api.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/core/environment.h>
#include <widget/w_scroll_view_base.h>
#include <widget/w_scene.h>
#include <widget/w_box.h>
#include <inputdevice/inputlistener.h>
#include <inputdevice/pad/pad.h>
#include <common/main_thread_call_list.h>
#include <common/shared_ptr.h>
#include <thread/thread.h>
#include <framework.h>
#include <dir/dir.h>
#include <file/localfile.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../../../boot.h"
#include "../../../shell.h"
#include "../../../plugins.h"
#include "../../../theme.h"
#include "applauncher.h"
#include "applauncher_home.h"

using namespace paf;

#if !defined(FW_TYPE_CEX)
extern "C" void ScePafAutoTestTty_9E10E873(int32_t type, const char *format, ...);
#endif

/* the wave the home shows behind its pages */
#define APPLAUNCHER_HOME_WAVE		11

/* the home's resources, by their IDs' hashes */
#define APPLAUNCHER_HOME_TEXTURE	0x56AEBD5F
/* PLACEHOLDER: the background shown while a page's image cannot be read */
#define APPLAUNCHER_HOME_NO_IMAGE_TEXTURE	0xAFDEF9D0
#define APPLAUNCHER_HOME_TEMPLATE	0xC65C4136
#define APPLAUNCHER_HOME_FOREGROUND	0x37677DEF
#define APPLAUNCHER_HOME_BACKGROUND	0xCBD8EDA5
#define APPLAUNCHER_HOME_SCROLL_VIEW	0x0E61B084
#define APPLAUNCHER_INDICATOR_TEMPLATE	0xA147AB4E
#define APPLAUNCHER_INDICATOR		0x39BDA15F

/* the notice page's texts, by their IDs' hashes */
#define APPLAUNCHER_NOTICE_TOO_MANY	0x02BC18CC
#define APPLAUNCHER_NOTICE_FULL0	0xD90C6DBC
#define APPLAUNCHER_NOTICE_FULL1	0xA0AE6FE3
#define APPLAUNCHER_NOTICE_ROOM		0x5F460356

/* the most applications the home holds */
#define APPLAUNCHER_HOME_MAX_APPS	500

/* the edit buttons' template, button, icon and label, the cross and circle icons (the enter
   button's or the other's, ScePafMisc_9CB5D72E nonzero: cross), and the open and decide texts, by their IDs' hashes */
#define APPLAUNCHER_EDIT_TEMPLATE	0x847FB677
#define APPLAUNCHER_EDIT_BUTTON		0x83E5552A
#define APPLAUNCHER_EDIT_ICON		0x57909DFC
#define APPLAUNCHER_EDIT_ICON_CROSS	0x227E56ED
#define APPLAUNCHER_EDIT_ICON_CIRCLE	0x9FC19CE1
#define APPLAUNCHER_EDIT_LABEL		0x071E2A0C
#define APPLAUNCHER_EDIT_OPEN_TEXT	0x113068C8
#define APPLAUNCHER_EDIT_DECIDE_TEXT	0xB7F752B8
#define APPLAUNCHER_EDIT_BACK_TEXT	0xAA2C6D2A
#define APPLAUNCHER_EDIT_CANCEL_TEXT	0xADCD9ADF

/* the edit's bar: its template, the bar, the box of its buttons and the event its buttons send,
   the menu and mode buttons' icons and texts, by their IDs' hashes */
#define APPLAUNCHER_EDIT_BAR_TEMPLATE	0x52510F9B
#define APPLAUNCHER_EDIT_BAR		0x7698040F
#define APPLAUNCHER_EDIT_BAR_BUTTONS	0xA299FE30
#define APPLAUNCHER_EDIT_BAR_EVENT	0x100008
#define APPLAUNCHER_EDIT_MENU_ICON	0x5536B69D
#define APPLAUNCHER_EDIT_MENU_TEXT	0xE205DEB0
#define APPLAUNCHER_EDIT_MODE_ICON	0xE1F39E60
#define APPLAUNCHER_EDIT_MODE_TEXT	0x74347E49

/* the waiting dialog's text, by its ID's hash */
#define APPLAUNCHER_HOME_WAIT_TEXT	0x632B6733

/* the overlay over the pages, by its IDs' hashes, and the events it sends */
#define APPLAUNCHER_OVERLAY_TEMPLATE	0xD465AF1B
#define APPLAUNCHER_OVERLAY		0xE1C6E5D7
#define APPLAUNCHER_OVERLAY_EVENT	0x10000008
#define APPLAUNCHER_OVERLAY_TOUCH	0x40001

/* the shell event the home follows (SceShell_7FE4E732) */
#define APPLAUNCHER_HOME_SHELL_EVENT	8

/* what a scene sends its widgets' listeners (paf's headers do not name it) */
#define APPLAUNCHER_SCENE_EVENT		0x800008

ApplauncherHome *g_applauncherHome;

const float g_applauncherFolderMoveTime = 250.0f;	/* 0x8150746C */
const float g_applauncherEditDepth = -200.0f;		/* 0x81507474 */
const float g_applauncherEditMoveTime = 750.0f;		/* 0x81507478 */
const float g_applauncherIconAsideTime = 375.0f;	/* 0x8150781C */
const float g_applauncherIconMoveTime = 275.0f;		/* 0x81507820 */
const float g_applauncherIconRestoreTime = 325.0f;	/* 0x81507824 */

void ApplauncherPage::OnIconAdded()
{
}

/* held while an application is installed onto the home (one at a time) */
/* PLACEHOLDER: set when entry #0 rewrites an application's whole row, given to livearea_plugin
   once */
static bool s_newAppAdded = false;
/* the theme read last, kept until applauncherHomeApplyTheme takes it */
static ApplauncherThemeData *s_themeData = NULL;
static thread::Semaphore s_appInstallSemaphore("SceShellAppInstall", 0, 1, 0);

/* the home's callbacks and the functions of the plugin's table, further on */
void applauncherHomeOnScrollTouch(int32_t type, ui::Handler *self, ui::Event *e, void *data);
void applauncherHomeOnSnapped(int32_t type, ui::Handler *self, ui::Event *e, void *data);
void applauncherHomeOnSceneEvent(int32_t type, ui::Handler *self, ui::Event *e, void *data);
void applauncherHomeOnMainThread(void *data);
void applauncherHomeOnOverlayEvent(int32_t type, ui::Handler *self, ui::Event *e, void *data);
void applauncherHomeOnOverlayTouch(int32_t type, ui::Handler *self, ui::Event *e, void *data);
void applauncherHomeOnEditBarEvent(int32_t type, ui::Handler *self, ui::Event *e, void *data);
void applauncherHomeOnLayerEnter(void *data)
{
	(void)data;
}

/* (0x810C1BF4) records the wallpaper source/destination mapping in ux0:shell/bgimage.bgd */
int applauncherWriteBgImage(const char *srcPath, const char *dstPath)
{
	if (!Dir::IsDirectory("ux0:shell"))
		Dir::CreateRecursive("ux0:shell", 6);

	int err = 0;
	common::SharedPtr<File> file = LocalFile::Open("ux0:shell/bgimage.bgd", 0x302u, 6u, &err);
	if (err != 0)
		return err;

	string name(srcPath != NULL ? srcPath : "");
	size_t slashPos = name.rfind('/');
	if (slashPos != string::npos)
		name.erase(0, slashPos);
	size_t colonPos = name.rfind(':');
	if (colonPos != string::npos)
		name.erase(0, colonPos);

	err = file->Write(name.c_str(), name.size());
	if (err < 0)
		return err;
	err = file->Write(",", 1);
	if (err < 0)
		return err;
	err = file->Write(dstPath, sce_paf_strlen(dstPath));
	if (err < 0)
		return err;
	err = file->Write("\n", 1);
	if (err < 0)
		return err;
	file->Close();
	return 0;
}

void applauncherHomeOnLayerLeave(void *data);
void applauncherHomeOnShellEvent(int code, const void *data, SceSize size, void *arg);
void applauncherHomeOnAppInfoEvent(int event, void *info, void *arg);
int applauncherInterfaceInstallApp(const ApplauncherInstallRequest *req, bool *async);

/*
 * The pages put one under the other from the top, a spacing apart. Mode 1 moves only the pages
 * shown in place over half a second; any other mode moves every page so (0x810A888C).
 */
void ApplauncherPageScroller::Layout(int mode)
{
	math::v4 pos(0.0f, 0.0f, 0.0f, 0.0f);

	for (iterator it = m_items.begin(); it != m_items.end(); ++it) {
		ApplauncherPage *page = static_cast<ApplauncherPage *>(*it);
		ui::Widget *widget = page->GetWidget();

		if (mode == 1) {
			widget->KillAnim(ui::Widget::ANIM_MOVE);
			if (page->m_state == 1)
				widget->SetPos(pos, new Timer(500.0f, Timer::FUNC_QUART_OUT));
			else
				widget->SetPos(pos);
		} else {
			widget->KillAnim(ui::Widget::ANIM_MOVE);
			widget->SetPos(pos, new Timer(500.0f, Timer::FUNC_QUART_OUT));
		}
		pos.set_y(pos.extract_y() - (float)m_spacing);
	}
}

ApplauncherPageScroller::~ApplauncherPageScroller()
{
}

/*
 * The page taken off the home with a DeletePageJob, unless it is the last; the view moved to the
 * page before it, and the pages refreshed when the icons are sorted. The page after it
 * (0x810A8ABE).
 */
ShellSnapList::iterator applauncherHomeDeletePage(ApplauncherPage *page)
{
	ShellSnapList::iterator next;

	if (g_applauncherHome->m_scroller.m_items.size() > 1) {
		int index = page->m_index;

		applauncherDeletePageJobStart(((job::JobQueue *(*)(void))
			g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_GET_JOB_QUEUE])(),
			index, NULL, NULL);
		next = g_applauncherHome->m_scroller.Remove(page);
		index--;
		if (index >= 0) {
			g_applauncherHome->m_scroller.JumpTo(
				g_applauncherHome->m_scroller.m_items.ptr[index]);
		} else if (g_applauncherHome->m_scroller.m_items.size() != 0) {
			g_applauncherHome->m_scroller.JumpTo(
				g_applauncherHome->m_scroller.m_items.ptr[0]);
		}
		delete page;
		if (g_applauncherHome->m_mode == 1 &&
		    g_applauncherModeManager->m_modes[APPLAUNCHER_MODE_EDIT] != 0) {
			for (ShellSnapList::iterator it = g_applauncherHome->m_scroller.m_items.begin();
			     it != g_applauncherHome->m_scroller.m_items.end(); ++it)
				static_cast<ApplauncherPage &>(**it).Refresh();
		}
	}
	return next;
}

/* the home's wave shown, and its view faded in at once (the plugin's function 16, 0x810A8C02) */
void applauncherHomeShowWave(void)
{
	if (g_applauncherHome != NULL) {
		g_applauncherHome->m_waveUpdated = true;
		scePafGraphicsUpdateCurrentWave(APPLAUNCHER_HOME_WAVE, 0.0f);

		ui::Widget *view = g_applauncherHome->m_scroller.m_scrollView->GetViewParent();

		view->m_transition_complete = false;
		view->m_transition = 0xff;
		view->DoTransitionReverse_core(0.0f, true);
		ScePafWidget_4588699E(view, NULL, 2, 0.0f, NULL);
	}
}

/*
 * The home made in topmenu's layer from its template: the scroll view of its pages, the page
 * indicator, its touches, its main-thread call, the plugin's table filled, and its listeners of
 * the shell's and the application database's events (0x810A8C76).
 */
ApplauncherHome::ApplauncherHome(Plugin *plugin)
	: m_plugin(plugin),
	  m_topmenu((ShellPluginInterface *)applauncherPluginGetInterface("topmenu_plugin", 1)),
	  m_powerManage((ShellPluginInterface *)
		applauncherPluginGetInterface("power_manage_plugin", 1)),
	  m_livearea(NULL), m_scroller(plugin), m_scale(1.0f), m_layer(-1), m_snapId(-1),
	  m_backgroundState(0), m_waveType(0), m_texU(0), m_texV(0), m_alpha(1.0f),
	  m_background(NULL), m_scrollView(NULL), m_foreground(NULL), m_editBar(NULL),
	  m_editRequest(0), m_editState(0), m_indicator(NULL), m_ready(false), m_mode(0),
	  m_fadeTime(0.0f), m_overlay(NULL), m_waitDialog(-1), m_imageCached(false),
	  m_dragPos(-10000.0f, -10000.0f), m_waitingAppCount(0), m_noticePage(NULL), m_appCount(0),
	  m_folderCount(0), m_viewHeld(false), m_waveUpdated(false), m_dropIndex(-1),
	  m_dropPage(-1), m_appPage(NULL), m_appIconTime(0.0f), m_lsdbListener(NULL)
{
	m_texScale[0] = 1.0f;
	m_texScale[1] = 1.0f;
	m_fade[0] = 1.0f;
	m_fade[1] = 1.0f;
	m_homeFlags[0] = false;
	m_homeFlags[1] = false;
	m_homeFlags[2] = false;
	g_applauncherHome = this;

	m_layer = ((int (*)(int, const intrusive_ptr<graph::Surface> &))
		m_topmenu->func[TOPMENU_PLUGIN_FUNC_ADD_LAYER])
		(-1, plugin->GetTexture(IDParam(APPLAUNCHER_HOME_TEXTURE)));

	ui::Widget *root = ((ui::Widget *(*)(int))
		m_topmenu->func[TOPMENU_PLUGIN_FUNC_GET_LAYER_ROOT])(m_layer);
	math::v4 size = *root->GetSize(0);

	size.set_x(960.0f);
	root->SetSize(size);
	root->SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	plugin->TemplateOpen(root, IDParam(APPLAUNCHER_HOME_TEMPLATE), Plugin::TemplateOpenParam());
	m_foreground = root->FindChild(IDParam(APPLAUNCHER_HOME_FOREGROUND), 0);
	m_background = root->FindChild(IDParam(APPLAUNCHER_HOME_BACKGROUND), 0);

	ui::ScrollView *view = (ui::ScrollView *)
		root->FindChild(IDParam(APPLAUNCHER_HOME_SCROLL_VIEW), 0);

	((void (*)(ui::ScrollView *))m_topmenu->func[TOPMENU_PLUGIN_FUNC_SET_SCROLL_VIEW])(view);
	m_scrollView = view;
	ScePafWidget_CB95F8F2(view, 1);
	view->SetVerticalScrollBar(NULL);
	view->SetHorizontalScrollBar(NULL);
	view->m_clip_children = false;
	ScePafWidget_21125870(view, 64);
	view->AddEventListener(0x10001, new ui::EventCBListener(applauncherHomeOnScrollTouch, NULL));
	view->AddEventListener(0x10003, new ui::EventCBListener(applauncherHomeOnScrollTouch, NULL));

	ui::ScrollViewScrollParam scroll;

	scroll.m_value[12] = 16.0f;
	scroll.m_value[0] = 16.0f;
	scroll.m_value[1] = 16.0f;
	scroll.m_bounce = false;
	view->SetScrollParam(&scroll);
	m_scroller.SetScrollView(view);
	view->AddEventListener(ui::ScrollViewBase::CB_SCROLL_VIEW_BASE_SNAPPED,
			       new ui::EventCBListener(applauncherHomeOnSnapped, this));
	view->m_scene->AddEventListener(APPLAUNCHER_SCENE_EVENT,
					new ui::EventCBListener(applauncherHomeOnSceneEvent, this));

	plugin->TemplateOpen(root, IDParam(APPLAUNCHER_INDICATOR_TEMPLATE),
			     Plugin::TemplateOpenParam());

	ui::Widget *indicator = root->FindChild(IDParam(APPLAUNCHER_INDICATOR), 0);

	indicator->m_hit_detect_b_mode = 2;
	indicator->m_hit_detect_mode = 2;
	m_indicator = new ApplauncherPageIndicator(plugin, indicator);
	m_indicator->Update();
	applauncherPagesReload();

	{
		common::SharedPtr<inputdevice::InputListener> listener(
			new ApplauncherInputListener());

		inputdevice::AddInputListener(listener);
	}
	common::MainThreadCallList::Register(applauncherHomeOnMainThread, NULL);

	g_applauncherInterface->func[0] = (void (*)(void))applauncherInterfaceInstallApp;
	g_applauncherInterface->func[1] = (void (*)(void))applauncherInterfaceRemoveApp;
	g_applauncherInterface->func[2] = (void (*)(void))applauncherInterfaceOpenLiveArea;
	g_applauncherInterface->func[4] = (void (*)(void))applauncherInterfaceFocusApp;
	g_applauncherInterface->func[5] = (void (*)(void))applauncherInterfaceUnselectApp;
	g_applauncherInterface->func[6] = (void (*)(void))applauncherHomeOnLayerLeave;
	g_applauncherInterface->func[7] = (void (*)(void))applauncherInterfaceEndIconEffect;
	g_applauncherInterface->func[8] = applauncherInterfaceEndIconEffects;
	g_applauncherInterface->func[9] = applauncherInterfaceScrollToNotice;
	g_applauncherInterface->func[10] = applauncherInterfaceScrollToFirst;
	g_applauncherInterface->func[11] = (void (*)(void))applauncherInterfaceLoadIconImage;
	g_applauncherInterface->func[3] = (void (*)(void))applauncherInterfaceGetIconImage;
	g_applauncherInterface->func[12] = (void (*)(void))applauncherInterfaceMarkAppUpdate;
	g_applauncherInterface->func[13] = (void (*)(void))applauncherInterfaceRequestAppData;
	g_applauncherInterface->func[14] = applauncherInterfaceReloadIcons;
	g_applauncherInterface->func[15] = applauncherInterfaceClearRequest;
	g_applauncherInterface->func[16] = applauncherHomeShowWave;
	g_applauncherInterface->func[17] = applauncherHomeHideWave;
	g_applauncherInterface->func[18] = (void (*)(void))applauncherInterfaceGetFocusedPage;
	g_applauncherInterface->func[19] = applauncherInterfaceCloseAppPage;
	g_applauncherInterface->func[21] = (void (*)(void))applauncherInterfaceHasApp;
	g_applauncherInterface->func[22] = (void (*)(void))applauncherInterfaceOpenChangeLog;
	g_applauncherInterface->func[23] = (void (*)(void))applauncherInterfaceRefreshTheme;
	g_applauncherInterface->func[24] = (void (*)(void))applauncherInterfaceReloadTheme;
	g_applauncherInterface->func[25] = (void (*)(void))applauncherThemeRefresh;
	g_applauncherInterface->func[26] = (void (*)(void))applauncherInterfaceSetRequest;
	g_applauncherInterface->func[20] = (void (*)(void))applauncherWriteBgImage;
	g_applauncherInterface->func[27] = (void (*)(void))applauncherWaveGetColors;

	m_ready = true;
	((void (*)(int, void (*)(void *), void *))
		m_topmenu->func[TOPMENU_PLUGIN_FUNC_SET_LAYER_ENTER])
		(m_layer, applauncherHomeOnLayerEnter, this);
	((void (*)(int, void (*)(void *), void *))
		m_topmenu->func[TOPMENU_PLUGIN_FUNC_SET_LAYER_LEAVE])
		(m_layer, applauncherHomeOnLayerLeave, this);
	if (((int (*)(void))m_topmenu->func[TOPMENU_PLUGIN_FUNC_IS_WAVE_SHOWN])())
		applauncherHomeShowWave();
	applauncherLaunchEffectCreate();
	SceShell_7FE4E732(APPLAUNCHER_HOME_SHELL_EVENT, applauncherHomeOnShellEvent, NULL);
	if (m_lsdbListener == NULL) {
		m_lsdbListener = new LsdbEventListener();
		SceLsdb_BF481B29(m_lsdbListener, applauncherHomeOnAppInfoEvent, NULL);
	}
}

/* the home's listeners, its call and its parts taken down, in the other order (0x810A95D0) */
ApplauncherHome::~ApplauncherHome()
{
	LsdbEventListener *listener = m_lsdbListener;

	if (listener != NULL) {
		SceLsdb_5FCAF61C(listener);
		delete listener;
		m_lsdbListener = NULL;
	}
	SceShell_D4C7D2B7(APPLAUNCHER_HOME_SHELL_EVENT, applauncherHomeOnShellEvent, NULL);
	applauncherLaunchEffectDestroy();
	common::MainThreadCallList::Unregister(applauncherHomeOnMainThread, NULL);
	if (m_indicator != NULL)
		delete m_indicator;
	((void (*)(ui::ScrollView *))m_topmenu->func[TOPMENU_PLUGIN_FUNC_SET_SCROLL_VIEW])(NULL);
	g_applauncherHome = NULL;
}

/* the home made on the plugin, and taken down (0x810A96EE, 0x810A9706) */
void applauncherPageCreate(Plugin *plugin)
{
	new ApplauncherHome(plugin);
}

void applauncherPageDestroy(void)
{
	if (g_applauncherHome != NULL)
		delete g_applauncherHome;
}

/* livearea_plugin's GetInterface(1), fetched once (0x810A971E) */
ShellPluginInterface *applauncherHomeGetLivearea(void)
{
	if (g_applauncherHome->m_livearea == NULL)
		g_applauncherHome->m_livearea = (ShellPluginInterface *)
			applauncherPluginGetInterface("livearea_plugin", 1);
	return g_applauncherHome->m_livearea;
}

/*
 * The pages loaded around the one nearest the view: that page and one on each side (the first
 * and the last page keep the two next to them), the others let go of (0x810A974E).
 */
void ApplauncherHome::UpdateLoadedPages(bool fast)
{
	ApplauncherHome *home = g_applauncherHome;
	int count = home->m_scroller.m_items.size();

	if (static_cast<ApplauncherPage *>(home->m_scroller.GetNearest()) == NULL)
		return;

	ApplauncherPage *page = static_cast<ApplauncherPage *>(g_applauncherHome->m_scroller.GetNearest());
	int index = page->m_index;

	if (index == 0)
		index = count > 1;
	if (count - 1 == index && count > 1)
		index = count - 2;

	int first = index - 1;
	int last = index + 1;

	if (first < 0)
		first = 0;
	if (count - 1 < last)
		last = count - 1;
	for (int i = 0; i < count; i++) {
		if (i < first || i > last)
			static_cast<ApplauncherPage &>(*home->m_scroller.m_items.ptr[i]).Unload();
	}
	for (int i = 0; i < count; i++) {
		if (i >= first && i <= last)
			static_cast<ApplauncherPage &>(*home->m_scroller.m_items.ptr[i]).Load(fast);
	}
}

/*
 * A page made, from the parameters or, without them, from the theme's page after the other's
 * (its background and wave), put in after the other, shown at once or faded in when the home is
 * ready and sorted, and saved with a CreateNewPageJob when asked; none past ten pages
 * (0x810A9812).
 */
ApplauncherPage *applauncherHomeAddPage(const LsdbPage *param, ApplauncherPage *after,
					bool focus, bool save)
{
	ApplauncherPage *page;

	if (g_applauncherHome->m_scroller.m_items.size() >= 10)
		return NULL;
	if (param != NULL) {
		page = new ApplauncherPage(g_applauncherHome->m_scroller.m_plugin, param, 0);
	} else {
		LsdbPage theme;

		theme.bgColor = shellStartupGetPageColor();
		if (shellThemeGetInfo()->get() != NULL) {
			common::SharedPtr<LsdbThemeInfo> *info = shellThemeGetInfo();
			int index = after != NULL ? after->m_index + 1 :
				g_applauncherHome->m_scroller.m_items.size();

			if (index < 10) {
				BackgroundParam *bg = &info->get()->m_homeProperty.m_bgParam[index];

				if (bg->m_imageFilePath.length() != 0) {
					shellThemeInfoGetFilePath(info->get(), &theme.themeFile,
								  bg->m_imageFilePath);
					theme.texWidth = 960;
					theme.texHeight = 512;
					theme.imageWidth = 960;
					theme.imageHeight = 512;
				}
				bg = &info->get()->m_homeProperty.m_bgParam[index];
				if (bg->m_waveType >= 0)
					theme.bgColor = bg->m_waveType;
				if (info->get()->m_status >= 100)
					theme.reserved01 = bg->GetFontColor();
			}
		}
		page = new ApplauncherPage(g_applauncherHome->m_scroller.m_plugin, &theme, 0);
	}
	if (page == NULL)
		return NULL;
	if (g_applauncherHome->m_scroller.m_items.size() == 0)
		focus = true;
	g_applauncherHome->m_scroller.Insert(page, 1, after);
	if (focus)
		g_applauncherHome->m_scroller.JumpTo(page);
	if (g_applauncherHome->m_ready && g_applauncherModeManager->m_modes[APPLAUNCHER_MODE_EDIT] != 0) {
		if (g_applauncherHome->m_scroller.GetNext(page) != NULL)
			g_applauncherHome->m_scroller.GetNext(page)->m_widget->SetMetaAlpha(
				0.0f, NULL, ui::Widget::ANIM_META_ALPHA);
		page->GetWidget()->SetMetaAlpha(0.0f, NULL, ui::Widget::ANIM_META_ALPHA);
		page->GetWidget()->SetMetaAlpha(1.0f, new Timer(500.0f, Timer::FUNC_QUAD_OUT),
						ui::Widget::ANIM_META_ALPHA);
		page->BeginEdit();
		for (ShellSnapList::iterator it = g_applauncherHome->m_scroller.m_items.begin();
		     it != g_applauncherHome->m_scroller.m_items.end(); ++it) {
			ApplauncherPage *item = static_cast<ApplauncherPage *>(*it);

			item->Refresh();
		}
	}
	if (save) {
		paf::vector<LsdbAppInfoPtr> icons;

		applauncherCreateNewPageJobStart(((job::JobQueue *(*)(void))
			g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_GET_JOB_QUEUE])(),
			page->m_index, icons, page->m_param, NULL, NULL);
	}
	g_applauncherHome->UpdateLoadedPages(false);
	return page;
}

/* the page before the other; the page itself when it is the first (0x810A9B6A) */
ApplauncherPage *applauncherHomeGetPreviousPage(ApplauncherPage *page)
{
	return static_cast<ApplauncherPage *>(g_applauncherHome->m_scroller.GetPrevious(page));
}

/* the last page with icons: the last page, or the one before the empty last page (0x810A9B98) */
ApplauncherPage *applauncherHomeGetLastPage(void)
{
	int count = g_applauncherHome->m_scroller.m_items.size();

	if (count == 0)
		return NULL;

	ApplauncherPage *page = static_cast<ApplauncherPage *>(
		g_applauncherHome->m_scroller.m_items.ptr[count - 1]);

	if (page == NULL)
		return NULL;
	if (page->m_type == 1)
		return static_cast<ApplauncherPage *>(
			g_applauncherHome->m_scroller.GetPrevious(page));
	return page;
}

/*
 * The overlay over the pages made once, from its template in the scene of the view's parent:
 * named by its own address, its events followed, and shown at once (0x810A9BE0).
 */
void ApplauncherHome::CreateOverlay()
{
	if (g_applauncherHome->m_overlay != NULL)
		return;

	Plugin *plugin = g_applauncherHome->m_plugin;

	plugin->TemplateOpen(g_applauncherHome->m_scroller.m_scrollView->GetViewParent()->m_scene,
			     IDParam(APPLAUNCHER_OVERLAY_TEMPLATE), Plugin::TemplateOpenParam());
	g_applauncherHome->m_overlay = g_applauncherHome->m_scroller.m_scrollView->GetViewParent()->
		m_scene->FindChild(IDParam(APPLAUNCHER_OVERLAY), 0);
	if (g_applauncherHome->m_overlay == NULL)
		return;

	char name[64];

	sce_paf_snprintf(name, sizeof(name), "%p", g_applauncherHome->m_overlay);
	g_applauncherHome->m_overlay->m_name.SetID(string(name));
	ScePafWidget_42C5A4D4(g_applauncherHome->m_overlay, 1, 0);
	g_applauncherHome->m_overlay->AddEventListener(APPLAUNCHER_OVERLAY_EVENT,
		new ui::EventCBListener(applauncherHomeOnOverlayEvent, NULL));
	g_applauncherHome->m_overlay->AddEventListener(APPLAUNCHER_OVERLAY_TOUCH,
		new ui::EventCBListener(applauncherHomeOnOverlayTouch, NULL));

	ui::Widget *overlay = g_applauncherHome->m_overlay;

	overlay->m_transition_complete = false;
	overlay->m_transition = 0xff;
	overlay->DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(overlay, NULL, 2, 0.0f, NULL);
	ScePafWidget_44B22ACA(g_applauncherHome->m_overlay, (common::transition::Type)255, 0.0f,
			      NULL, NULL);
}

/*
 * The home's edit mode entered: a page taken off for the edit put away, the view's keys held and
 * its sound played, the view faded to the home's alpha (or out without a focus), the background
 * dropped below the pages, the overlay made, every page saved, topmenu told, and the waiting
 * dialog opened when asked (0x810A9EC4).
 */
void ApplauncherHome::EnterEditMode()
{
	ApplauncherHome *home = g_applauncherHome;

	thread::RMutex::main_thread_mutex.Lock();
	if (home->m_noticePage == NULL) {
		thread::RMutex::main_thread_mutex.Unlock();
	} else {
		g_applauncherHome->m_scroller.Remove(home->m_noticePage);
		delete home->m_noticePage;
		home->m_noticePage = NULL;
		thread::RMutex::main_thread_mutex.Unlock();
	}
	ScePafWidget_164BFFA5(g_applauncherHome->m_scroller.m_scrollView, 64);
	Framework::Instance()->PlayCommonSE(41, 1.0f, false);
	home = g_applauncherHome;
	home->m_mode = 1;

	ui::Widget *view = home->m_scrollView;

	g_applauncherHome->m_fadeTime = 0.0f;
	if (g_applauncherHome->m_backgroundState != 2) {
		view->KillAnim(ui::Widget::ANIM_COLOR);

		math::v4 color = (g_applauncherHome->m_imagePath.length() != 0 ?
			math::v4(1.0f, 1.0f, 1.0f, 1.0f) : math::v4(0.0f, 0.0f, 0.0f, 0.0f)) * g_applauncherHome->m_alpha;

		view->SetColor(color, new Timer(300.0f, Timer::FUNC_QUAD_OUT), ui::Widget::ANIM_COLOR, 2);
	}

	math::v4 pos = g_applauncherHome->m_background->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	pos.set_z(-200.0f);
	g_applauncherHome->m_background->SetPos(pos, new Timer(750.0f, Timer::FUNC_BOUNCE_OUT),
						ui::Widget::ANIM_MOVE, 2);
	g_applauncherHome->CreateOverlay();
	home = g_applauncherHome;
	for (ShellSnapList::iterator it = home->m_scroller.m_items.begin();
	     it != home->m_scroller.m_items.end(); ++it)
		static_cast<ApplauncherPage *>(*it)->BeginEdit();
	if (g_applauncherHome->m_appPage != NULL)
		g_applauncherHome->m_appPage->BeginEdit();
	g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_EDIT_MODE_ENTERED]();
	if (g_applauncherHome->m_homeFlags[1]) {
		wstring text(g_applauncherPlugin->GetString(IDParam(APPLAUNCHER_HOME_WAIT_TEXT)));

		g_applauncherHome->m_waitDialog = SceCommonGuiDialog_6CE14F66(g_applauncherPlugin,
			wstring(), text, &SceCommonGuiDialog_2A68B4A4, NULL, NULL);
	}
}

/*
 * The notice page's text: past 500 applications how many are too many, else whether a page with
 * icons still has room (0x810AA258).
 */
void ApplauncherHome::UpdateNoticeText()
{
	ApplauncherPage *page = m_noticePage;

	if (page == NULL)
		return;

	ui::Widget *text = page->m_noticeText;

	if (text == NULL)
		return;

	int count = m_appCount + m_waitingAppCount;
	wstring str;
	bool full = true;

	for (ShellSnapList::iterator it = g_applauncherHome->m_scroller.m_items.begin();
	     it != g_applauncherHome->m_scroller.m_items.end(); ++it) {
		ApplauncherPage *p = static_cast<ApplauncherPage *>(*it);

		if (p->m_type == 0 && !p->IsFull()) {
			full = false;
			break;
		}
	}
	if (count > APPLAUNCHER_HOME_MAX_APPS) {
		wchar_t total[64], over[64], limit[64];

		sce_paf_swprintf(total, sizeof(total), L"%d", count);
		sce_paf_swprintf(over, sizeof(over), L"%d", count - APPLAUNCHER_HOME_MAX_APPS);
		sce_paf_swprintf(limit, sizeof(limit), L"%d", APPLAUNCHER_HOME_MAX_APPS);
		str = shellFormatText(applauncherGetString(IDParam(APPLAUNCHER_NOTICE_TOO_MANY)),
				      total, limit, over);
	} else if (full) {
		if (m_folderCount)
			str = applauncherGetString(IDParam(APPLAUNCHER_NOTICE_FULL0));
		else
			str = applauncherGetString(IDParam(APPLAUNCHER_NOTICE_FULL1));
	} else {
		str = applauncherGetString(IDParam(APPLAUNCHER_NOTICE_ROOM));
	}
	text->SetString(str);
}

/* the notice page's text updated, or the page made at the end while applications wait to be
   added (0x810AA5AA) */
void ApplauncherHome::ShowNotice()
{
	thread::RMutexLock lock(&thread::RMutex::main_thread_mutex);

	if (m_noticePage != NULL) {
		g_applauncherHome->UpdateNoticeText();
		return;
	}
	if (m_waitingAppCount <= 0)
		return;

	LsdbPage param;

	param.bgColor = shellStartupGetPageColor();

	ApplauncherPage *page = new ApplauncherPage(g_applauncherHome->m_scroller.m_plugin, &param, 1);

	if (page == NULL)
		return;
	m_noticePage = page;
	g_applauncherHome->m_scroller.Insert(page, 1, NULL);
	g_applauncherHome->UpdateNoticeText();
}

/* an edit button faded out over 200 ms, and deleted after its fade, its name taken back
   (0x810AA6B6) */
void ApplauncherHome::HideEditButton(ui::Widget *button)
{
	math::v4 pos = button->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	pos.set_y(-36.0f);

	Timer *timer = new Timer(200.0f, Timer::FUNC_QUAD_OUT);

	button->m_transition_complete = false;
	button->m_transition = 0xff;
	button->DoTransitionReverse_core(timer != NULL ? timer->GetTotalTime() : 0.0f, true);
	ScePafWidget_4588699E(button, timer, 2, 0.0f, NULL);
	ScePafWidget_5A9793B0(button, 200.0f);
	button->m_name.SetID(string(""));
}

/*
 * The edit's buttons in the widget: the open button while the edit state is 2 and the decide
 * button while it is 4 or 5, made from their template (the enter button's icon, the text) and
 * shown at once, each faded out in the other states, and each brought to the front of its
 * parent (0x810AA81C).
 */
void ApplauncherHome::UpdateEditButtons(ui::Widget *parent)
{
	static IDParam s_editDecide("edit_decide");
	static IDParam s_editOpen("edit_open");

	ui::Widget *button = parent->FindChild(s_editOpen, 0);

	if (button != NULL) {
		if (m_editState != 2)
			HideEditButton(button);
	} else if (m_editState == 2) {
		m_plugin->TemplateOpen(parent, IDParam(APPLAUNCHER_EDIT_TEMPLATE),
				       Plugin::TemplateOpenParam());
		button = parent->FindChild(IDParam(APPLAUNCHER_EDIT_BUTTON), 0);
		if (button != NULL) {
			ui::Widget *icon = button->FindChild(IDParam(APPLAUNCHER_EDIT_ICON), 0);

			button->m_name = s_editOpen;
			if (icon != NULL) {
				if (ScePafMisc_9CB5D72E())
					icon->SetTexture(m_plugin->GetTexture(
						IDParam(APPLAUNCHER_EDIT_ICON_CROSS)));
				else
					icon->SetTexture(m_plugin->GetTexture(
						IDParam(APPLAUNCHER_EDIT_ICON_CIRCLE)));
			}

			ui::Widget *label = button->FindChild(IDParam(APPLAUNCHER_EDIT_LABEL), 0);

			if (label != NULL)
				label->SetString(wstring(m_plugin->GetString(
					IDParam(APPLAUNCHER_EDIT_OPEN_TEXT))));
			static_cast<ui::Box *>(button)->m_auto_show = false;
			button->m_transition_complete = false;
			button->m_transition = 0xff;
			button->DoTransitionReverse_core(0.0f, true);
			ScePafWidget_4588699E(button, NULL, 2, 0.0f, NULL);
		}
	}
	if (button != NULL)
		button->SetParent(button->m_parent, NULL, 6);

	button = parent->FindChild(s_editDecide, 0);
	if (button != NULL) {
		if (m_editState != 4 && m_editState != 5)
			HideEditButton(button);
	} else if (m_editState == 4 || m_editState == 5) {
		m_plugin->TemplateOpen(parent, IDParam(APPLAUNCHER_EDIT_TEMPLATE),
				       Plugin::TemplateOpenParam());
		button = parent->FindChild(IDParam(APPLAUNCHER_EDIT_BUTTON), 0);
		if (button == NULL)
			return;

		ui::Widget *icon = button->FindChild(IDParam(APPLAUNCHER_EDIT_ICON), 0);

		button->m_name = s_editDecide;
		if (icon != NULL) {
			if (ScePafMisc_9CB5D72E())
				icon->SetTexture(m_plugin->GetTexture(
					IDParam(APPLAUNCHER_EDIT_ICON_CROSS)));
			else
				icon->SetTexture(m_plugin->GetTexture(
					IDParam(APPLAUNCHER_EDIT_ICON_CIRCLE)));
		}

		ui::Widget *label = button->FindChild(IDParam(APPLAUNCHER_EDIT_LABEL), 0);

		if (label != NULL)
			label->SetString(wstring(m_plugin->GetString(
				IDParam(APPLAUNCHER_EDIT_DECIDE_TEXT))));
		static_cast<ui::Box *>(button)->m_auto_show = false;
		button->m_transition_complete = false;
		button->m_transition = 0xff;
		button->DoTransitionReverse_core(0.0f, true);
		ScePafWidget_4588699E(button, NULL, 2, 0.0f, NULL);
	}
	if (button != NULL)
		button->SetParent(button->m_parent, NULL, 6);
}

/*
 * The edit's other buttons in the widget: the back button while the edit state is 1, 2, 3 or 5
 * and the cancel button while it is 4, made like the others with the icon of the button that is
 * not the enter button (0x810AB1B6).
 */
void ApplauncherHome::UpdateBackButtons(ui::Widget *parent)
{
	static IDParam s_editCancel("edit_cancel");
	static IDParam s_editBack("edit_back");

	ui::Widget *button = parent->FindChild(s_editBack, 0);

	if (button != NULL) {
		if (m_editState != 1 && m_editState != 2 && m_editState != 5 &&
		    m_editState != 3)
			HideEditButton(button);
	} else if (m_editState == 1 || m_editState == 2 || m_editState == 5 ||
		   m_editState == 3) {
		m_plugin->TemplateOpen(parent, IDParam(APPLAUNCHER_EDIT_TEMPLATE),
				       Plugin::TemplateOpenParam());
		button = parent->FindChild(IDParam(APPLAUNCHER_EDIT_BUTTON), 0);
		if (button != NULL) {
			ui::Widget *icon = button->FindChild(IDParam(APPLAUNCHER_EDIT_ICON), 0);

			button->m_name = s_editBack;
			if (icon != NULL) {
				if (ScePafMisc_9CB5D72E())
					icon->SetTexture(m_plugin->GetTexture(
						IDParam(APPLAUNCHER_EDIT_ICON_CIRCLE)));
				else
					icon->SetTexture(m_plugin->GetTexture(
						IDParam(APPLAUNCHER_EDIT_ICON_CROSS)));
			}

			ui::Widget *label = button->FindChild(IDParam(APPLAUNCHER_EDIT_LABEL), 0);

			if (label != NULL)
				label->SetString(wstring(m_plugin->GetString(
					IDParam(APPLAUNCHER_EDIT_BACK_TEXT))));
			static_cast<ui::Box *>(button)->m_auto_show = false;
			button->m_transition_complete = false;
			button->m_transition = 0xff;
			button->DoTransitionReverse_core(0.0f, true);
			ScePafWidget_4588699E(button, NULL, 2, 0.0f, NULL);
		}
	}
	if (button != NULL)
		button->SetParent(button->m_parent, NULL, 6);

	button = parent->FindChild(s_editCancel, 0);
	if (button != NULL) {
		if (m_editState != 4)
			HideEditButton(button);
	} else if (m_editState == 4) {
		m_plugin->TemplateOpen(parent, IDParam(APPLAUNCHER_EDIT_TEMPLATE),
				       Plugin::TemplateOpenParam());
		button = parent->FindChild(IDParam(APPLAUNCHER_EDIT_BUTTON), 0);
		if (button == NULL)
			return;

		ui::Widget *icon = button->FindChild(IDParam(APPLAUNCHER_EDIT_ICON), 0);

		button->m_name = s_editCancel;
		if (icon != NULL) {
			if (ScePafMisc_9CB5D72E())
				icon->SetTexture(m_plugin->GetTexture(
					IDParam(APPLAUNCHER_EDIT_ICON_CIRCLE)));
			else
				icon->SetTexture(m_plugin->GetTexture(
					IDParam(APPLAUNCHER_EDIT_ICON_CROSS)));
		}

		ui::Widget *label = button->FindChild(IDParam(APPLAUNCHER_EDIT_LABEL), 0);

		if (label != NULL)
			label->SetString(wstring(m_plugin->GetString(
				IDParam(APPLAUNCHER_EDIT_CANCEL_TEXT))));
		static_cast<ui::Box *>(button)->m_auto_show = false;
		button->m_transition_complete = false;
		button->m_transition = 0xff;
		button->DoTransitionReverse_core(0.0f, true);
		ScePafWidget_4588699E(button, NULL, 2, 0.0f, NULL);
	}
	if (button != NULL)
		button->SetParent(button->m_parent, NULL, 6);
}

/*
 * The edit state set. The edit's bar is made as the edit starts (slid in from below the screen,
 * named by its own address, its buttons' events followed) and slid out and deleted as it ends;
 * in between its buttons are made for the state: the open, decide, back and cancel buttons (the
 * enter button's first), the menu button in state 1, the mode button in states 1 and 2 and the
 * decide button in state 3 (0x810ABB64).
 */
void ApplauncherHome::SetEditState(int state)
{
	int old = m_editState;

	if (old == state)
		return;
	m_editState = state;
	if (old == 0 && m_editBar == NULL) {
		m_plugin->TemplateOpen(m_background->m_parent, IDParam(APPLAUNCHER_EDIT_BAR_TEMPLATE),
				       Plugin::TemplateOpenParam());
		m_editBar = m_background->m_parent->FindChild(IDParam(APPLAUNCHER_EDIT_BAR), 0);
		if (m_editBar != NULL) {
			char name[64];

			sce_paf_snprintf(name, sizeof(name), "%p", m_editBar);
			m_editBar->m_name.SetID(string(name));

			math::v4 pos = m_editBar->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

			pos.set_y(-272.0f);
			m_editBar->SetPos(pos, new Timer(300.0f, Timer::FUNC_QUAD_OUT),
					  ui::Widget::ANIM_MOVE, 2);

			ui::Widget *buttons = m_editBar->FindChild(IDParam(APPLAUNCHER_EDIT_BAR_BUTTONS), 0);

			if (buttons != NULL)
				buttons->AddEventListener(APPLAUNCHER_EDIT_BAR_EVENT,
					new ui::EventCBListener(applauncherHomeOnEditBarEvent, NULL), true);
		}
	}
	if (m_editBar == NULL)
		return;

	ui::Widget *buttons = m_editBar->FindChild(IDParam(APPLAUNCHER_EDIT_BAR_BUTTONS), 0);

	if (m_editState == 0) {
		math::v4 pos = m_editBar->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

		pos.set_y(-330.0f);
		m_editBar->SetPos(pos, new Timer(300.0f, Timer::FUNC_QUAD_OUT), ui::Widget::ANIM_MOVE,
				  2);
		ScePafWidget_5A9793B0(m_editBar, 300.0f);
		m_editBar = NULL;
		return;
	}

	static IDParam s_editModeMode("edit_modeMode");
	static IDParam s_editMenu("edit_menu");
	static IDParam s_editKakutei("edit_kakutei");

	if (ScePafMisc_9CB5D72E()) {
		UpdateEditButtons(buttons);
		UpdateBackButtons(buttons);
	} else {
		UpdateBackButtons(buttons);
		UpdateEditButtons(buttons);
	}

	ui::Widget *button = buttons->FindChild(s_editMenu, 0);

	if (button != NULL) {
		if (m_editState != 1)
			HideEditButton(button);
	} else if (m_editState == 1) {
		m_plugin->TemplateOpen(buttons, IDParam(APPLAUNCHER_EDIT_TEMPLATE),
				       Plugin::TemplateOpenParam());
		button = buttons->FindChild(IDParam(APPLAUNCHER_EDIT_BUTTON), 0);
		if (button != NULL) {
			ui::Widget *icon = button->FindChild(IDParam(APPLAUNCHER_EDIT_ICON), 0);

			button->m_name = s_editMenu;
			if (icon != NULL)
				icon->SetTexture(m_plugin->GetTexture(IDParam(APPLAUNCHER_EDIT_MENU_ICON)));

			ui::Widget *label = button->FindChild(IDParam(APPLAUNCHER_EDIT_LABEL), 0);

			if (label != NULL)
				label->SetString(wstring(m_plugin->GetString(
					IDParam(APPLAUNCHER_EDIT_MENU_TEXT))));
			static_cast<ui::Box *>(button)->m_auto_show = false;
			button->m_transition_complete = false;
			button->m_transition = 0xff;
			button->DoTransitionReverse_core(0.0f, true);
			ScePafWidget_4588699E(button, NULL, 2, 0.0f, NULL);
		}
	}
	if (button != NULL)
		button->SetParent(button->m_parent, NULL, 6);

	button = buttons->FindChild(s_editModeMode, 0);
	if (button != NULL) {
		if (m_editState != 1 && m_editState != 2)
			HideEditButton(button);
	} else if (m_editState == 1 || m_editState == 2) {
		m_plugin->TemplateOpen(buttons, IDParam(APPLAUNCHER_EDIT_TEMPLATE),
				       Plugin::TemplateOpenParam());
		button = buttons->FindChild(IDParam(APPLAUNCHER_EDIT_BUTTON), 0);
		if (button != NULL) {
			ui::Widget *icon = button->FindChild(IDParam(APPLAUNCHER_EDIT_ICON), 0);

			button->m_name = s_editModeMode;
			if (icon != NULL)
				icon->SetTexture(m_plugin->GetTexture(IDParam(APPLAUNCHER_EDIT_MODE_ICON)));

			ui::Widget *label = button->FindChild(IDParam(APPLAUNCHER_EDIT_LABEL), 0);

			if (label != NULL)
				label->SetString(wstring(m_plugin->GetString(
					IDParam(APPLAUNCHER_EDIT_MODE_TEXT))));
			static_cast<ui::Box *>(button)->m_auto_show = false;
			button->m_transition_complete = false;
			button->m_transition = 0xff;
			button->DoTransitionReverse_core(0.0f, true);
			ScePafWidget_4588699E(button, NULL, 2, 0.0f, NULL);
		}
	}
	if (button != NULL)
		button->SetParent(button->m_parent, NULL, 6);

	button = buttons->FindChild(s_editKakutei, 0);
	if (button != NULL) {
		if (m_editState != 3)
			HideEditButton(button);
	} else if (m_editState == 3) {
		m_plugin->TemplateOpen(buttons, IDParam(APPLAUNCHER_EDIT_TEMPLATE),
				       Plugin::TemplateOpenParam());
		button = buttons->FindChild(IDParam(APPLAUNCHER_EDIT_BUTTON), 0);
		if (button == NULL)
			return;

		ui::Widget *icon = button->FindChild(IDParam(APPLAUNCHER_EDIT_ICON), 0);

		button->m_name = s_editKakutei;
		if (icon != NULL)
			icon->SetTexture(m_plugin->GetTexture(IDParam(APPLAUNCHER_EDIT_MODE_ICON)));

		ui::Widget *label = button->FindChild(IDParam(APPLAUNCHER_EDIT_LABEL), 0);

		if (label != NULL)
			label->SetString(wstring(m_plugin->GetString(
				IDParam(APPLAUNCHER_EDIT_DECIDE_TEXT))));
		static_cast<ui::Box *>(button)->m_auto_show = false;
		button->m_transition_complete = false;
		button->m_transition = 0xff;
		button->DoTransitionReverse_core(0.0f, true);
		ScePafWidget_4588699E(button, NULL, 2, 0.0f, NULL);
	}
	if (button != NULL)
		button->SetParent(button->m_parent, NULL, 6);
}

/* the edit state asked for: set while the environment shows a focus, else none (0x810ACC5E) */
void ApplauncherHome::RequestEditState(int state)
{
	if (m_editRequest == state)
		return;
	m_editRequest = state;
	if (ScePafWidget_529FB877(Framework::Instance()->GetEnvironmentInstance(), -1))
		SetEditState(m_editRequest);
	else
		SetEditState(0);
}

/*
 * The home's edit mode left: the icon being dragged dropped, the view's keys let go, the view
 * faded back and the background raised, the edit state cleared, the pages out of their edit
 * (the empty ones deleted) or the application's page alone, topmenu told, the notice shown and
 * the input reset (0x810ACC98).
 */
void ApplauncherHome::ExitEditMode()
{
	Framework::Instance()->PlayCommonSE(42, 1.0f, false);
	if (g_applauncherDragItem.get() != NULL) {
		if (g_applauncherDragItem->m_icon != NULL)
			ScePafWidget_931AD41E(g_applauncherDragItem->m_icon->m_widget, true);
		g_applauncherDragItem->Drop();
	}
	ScePafWidget_21125870(g_applauncherHome->m_scroller.m_scrollView, 64);

	ApplauncherHome *home = g_applauncherHome;
	ui::Widget *view = home->m_scrollView;

	home->m_alpha = 1.0f;
	home->m_mode = 0;
	home->m_fadeTime = 0.0f;
	view->KillAnim(ui::Widget::ANIM_COLOR);

	math::v4 color = g_applauncherHome->m_imagePath.length() != 0 ?
		math::v4(1.0f, 1.0f, 1.0f, 1.0f) : math::v4(0.0f, 0.0f, 0.0f, 0.0f);

	view->SetColor(color, new Timer(300.0f, Timer::FUNC_QUAD_OUT), ui::Widget::ANIM_COLOR, 2);

	math::v4 pos = g_applauncherHome->m_background->GetPos(0, NULL,
							       math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	pos.set_z(0.0f);
	g_applauncherHome->m_background->SetPos(pos, new Timer(750.0f, Timer::FUNC_QUAD_OUT),
						ui::Widget::ANIM_MOVE, 2);
	g_applauncherHome->RequestEditState(0);
	if (g_applauncherHome->m_appPage != NULL) {
		g_applauncherHome->m_appPage->EndEdit();
	} else {
		ui::Widget *overlay = g_applauncherHome->m_overlay;

		if (overlay != NULL) {
			ScePafWidget_5A9793B0(overlay, ScePafWidget_F1176ACC(overlay,
				(common::transition::Type)255, 0.0f, NULL, NULL));
			g_applauncherHome->m_overlay = NULL;
		}
	}

	home = g_applauncherHome;
	for (ShellSnapList::iterator it = home->m_scroller.m_items.begin();
	     it != home->m_scroller.m_items.end();) {
		ApplauncherPage *page = static_cast<ApplauncherPage *>(*it);

		if (page->IsEmpty()) {
			it = applauncherHomeDeletePage(page);
		} else {
			page->EndEdit();
			++it;
		}
	}
	g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_EDIT_MODE_LEFT]();
	g_applauncherHome->ShowNotice();
	inputdevice::Lock(0x7F);
	inputdevice::Unlock(0x7F);
}

/* the page after it, the page itself at the end (0x810ACF7C) */
ApplauncherPage *applauncherHomeGetNextPage(ApplauncherPage *page)
{
	return static_cast<ApplauncherPage *>(g_applauncherHome->m_scroller.GetNext(page));
}

/* the parameters of the page in view copied out (0x810ACFAA) */
void applauncherHomeGetPageParam(LsdbPage *param)
{
	ApplauncherPage *page = static_cast<ApplauncherPage *>(
		g_applauncherHome->m_scroller.GetItemBySnapID(g_applauncherHome->m_snapId));

	page->GetParam(param);
}

/*
 * The background's change begun, unless it is changing: the view faded out (left as it is when
 * the image stays), the icons undimmed, and the change finished once the fade is over
 * (0x810ACFD8).
 */
void ApplauncherHome::BeginBackgroundChange(bool keepImage)
{
	if (m_backgroundState == 2 || m_backgroundState == 3)
		return;

	ui::Widget *view = m_scrollView;

	m_backgroundState = 2;
	if (!keepImage) {
		view->KillAnim(ui::Widget::ANIM_COLOR);
		view->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f),
			       new Timer(300.0f, Timer::FUNC_QUAD_OUT), ui::Widget::ANIM_COLOR, 0);
	}
	applauncherIconPoolSetDim(false);
	view->KillTimeoutEvent(0xFF01);
	view->SetTimeoutEvent(0xFF01, 300.0f, applauncherHomeOnBackgroundTimeout, this);
}

/* the background changed to the page's when its image or wave differs (0x810AD11E) */
void ApplauncherHome::UpdateBackground(ApplauncherPage *page)
{
	if (m_imagePath.length() == 0 && page->m_param.themeFile.length() == 0) {
		if (m_waveType != page->m_param.bgColor)
			BeginBackgroundChange(true);
	} else if (m_imagePath.length() != 0 && page->m_param.themeFile.length() != 0) {
		if (m_imagePath != page->m_param.themeFile || m_imageCached)
			BeginBackgroundChange(false);
		else if (m_waveType != page->m_param.bgColor)
			BeginBackgroundChange(true);
	} else {
		BeginBackgroundChange(false);
	}
}

/* the parameters of the page in view set and saved, its background shown (0x810AD192) */
void applauncherHomeSetPageParam(const LsdbPage *param)
{
	ApplauncherPage *page = static_cast<ApplauncherPage *>(
		g_applauncherHome->m_scroller.GetItemBySnapID(g_applauncherHome->m_snapId));

	page->SetParam(param, true);
	g_applauncherHome->UpdateBackground(page);
}

/*
 * The snaps of the pages set: in idu mode a page is snapped to only while one of its icons is
 * shown; otherwise every page is (0x810AD1D2).
 */
void ApplauncherHome::UpdateSnaps()
{
	if (shellStartupGetIduMode() == 1) {
		for (ShellSnapList::iterator it = g_applauncherHome->m_scroller.m_items.begin();
		     it != g_applauncherHome->m_scroller.m_items.end(); ++it) {
			ui::ScrollView *view = g_applauncherHome->m_scroller.m_scrollView;
			ApplauncherPage *page = static_cast<ApplauncherPage *>(*it);

			bool hasVisible = false;

			for (SceUInt32 i = 0; i < 10; i++) {
				if (page->m_slots[i].get() != NULL && !page->m_slots[i]->m_hidden) {
					view->AddSnapChild(page->GetWidget());
					hasVisible = true;
					break;
				}
			}
			if (!hasVisible)
				view->DelSnapChild(page->GetWidget());
		}
	} else if (shellStartupGetIduMode() == 2) {
		for (ShellSnapList::iterator it = g_applauncherHome->m_scroller.m_items.begin();
		     it != g_applauncherHome->m_scroller.m_items.end(); ++it) {
			ApplauncherPage *page = static_cast<ApplauncherPage *>(*it);

			g_applauncherHome->m_scroller.m_scrollView->AddSnapChild(page->GetWidget());
		}
	}
}

/*
 * The application's page closed: the dots shown, the icons put back on the page the focused one
 * came from, the view shown, the page's icons put back, the focus kept when asked, and outside
 * the edit the overlay faded out (0x810AD2CC).
 */
void applauncherHomeCloseAppPage(bool keepFocus)
{
	if (g_applauncherHome->m_appPage == NULL)
		return;

	g_applauncherHome->m_indicator->Show(250.0f);

	ApplauncherPage *page = g_applauncherHome->m_appPage;
	ApplauncherIcon *icon = applauncherPageAsFolder(page)->m_icon.get();

	if (icon != NULL && icon->m_page != NULL && icon->m_page->m_type == 0)
		icon->m_page->RestoreIcons(applauncherPageAsFolder(page)->m_icon);

	ui::Widget *view = g_applauncherHome->m_scroller.m_scrollView;

	if (!view->m_transition_complete) {
		view->m_transition = 0xff;
		view->m_transition_complete = true;
		view->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(view, NULL, 2, 0.0f, NULL);
	applauncherIconPoolCloseAppPage(g_applauncherHome->m_appPage);
	if (keepFocus) {
		g_applauncherHome->m_appIcon = applauncherPageAsFolder(g_applauncherHome->m_appPage)->m_icon;
		g_applauncherHome->m_appIconTime = 0.0f;
	}
	g_applauncherHome->m_appPage = NULL;
	if (g_applauncherHome->m_mode == 0) {
		ui::Widget *overlay = g_applauncherHome->m_overlay;

		if (overlay != NULL) {
			ScePafWidget_5A9793B0(overlay, ScePafWidget_F1176ACC(overlay,
				(common::transition::Type)255, 0.0f, NULL, NULL));
			g_applauncherHome->m_overlay = NULL;
		}
	}
}

/* the view moved to the page at once, unless it is the application's page (0x810AD466) */
void applauncherHomeJumpTo(ApplauncherPage *page)
{
	if (page->m_type != 2)
		g_applauncherHome->m_scroller.JumpTo(page);
}

/* the job queue topmenu_plugin keeps for the home */
#define APPLAUNCHER_HOME_JOB_QUEUE() ((job::JobQueue *(*)(void)) \
	g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_GET_JOB_QUEUE])()

/*
 * PLACEHOLDER name: the interface's entry #0. The application's row written to the database by a
 * job, waited for. An application whose _eboot_attr has bit 0 or whose LAREA_TYPE is 3 is
 * written only. Otherwise a rewrite (mode 1 or 2) writes the icon's copy; for mode 1 of an
 * application with a LiveArea of type 0, livearea_plugin is waited for (ten seconds at most)
 * and told first. A new application (mode 0) goes after the last icon of the last page, else
 * in the first free slot from the last page back once there are ten pages, else on a new page
 * made from the theme; its #_lltime is set unless #_promote_attr has bit 2. Once written, its
 * icon is made, selected and faded in, its page shown when asked, and the waiting dialog let go
 * of. With no room (500 applications, or ten full pages) it is written without a page and
 * counted as waiting (0x810AD48E).
 */
static int applauncherHomeInstallApp(const ApplauncherInstallRequest *req, bool *async)
{
	int ret;
	int i;

	s_newAppAdded = false;
	*async = true;
	if ((SceLsdb_45B38A03(&req->app) & 1) != 0 ||
	    sceLsdbGetMetaContentsPath(&req->app) == 3) {
		LsdbIconPtr icon;
		common::SharedPtr<job::JobItem> job;

		if (req->mode == 0) {
			job = applauncherInsertAppInfoJobStart(APPLAUNCHER_HOME_JOB_QUEUE(), req->app,
				req->icons, -1, icon, applauncherHomeOnJobDone,
				&s_appInstallSemaphore);
		} else if (req->mode == 1) {
			job = applauncherUpdateAppInfoDiffJobStart(APPLAUNCHER_HOME_JOB_QUEUE(),
				req->app, req->icons, icon, req->diff, applauncherHomeOnJobDone,
				&s_appInstallSemaphore);
		} else if (req->mode == 2) {
			job = applauncherUpdateAppInfoJobStart(APPLAUNCHER_HOME_JOB_QUEUE(), req->app,
				req->icons, icon, applauncherHomeOnJobDone,
				&s_appInstallSemaphore);
		}
		if (job.get() == NULL)
			return APPLAUNCHER_ERROR_NO_MEMORY;
		s_appInstallSemaphore.Acquire();
		return static_cast<ApplauncherLsdbJob *>(job.get())->m_ret;
	}

	LsdbIconPtr icon(new LsdbIcon(*req->icon.get()));

	if (req->mode != 0) {
		common::SharedPtr<job::JobItem> job;

		if (req->mode == 1) {
			if (SceLsdb_C070E42E(&req->app) != NULL &&
			    sceLsdbGetMetaContentsPath(&req->app) == 0) {
				*async = false;
				for (i = 0; applauncherHomeGetLivearea() == NULL; i++) {
					if (i > 600)
						return APPLAUNCHER_ERROR_NO_LIVEAREA;
					thread::Sleep(16);
				}
				for (i = 0; applauncherHomeGetLivearea()->func[
					     LIVEAREA_PLUGIN_FUNC_APP_ADDED] == NULL; i++) {
					if (i > 600)
						return APPLAUNCHER_ERROR_NO_LIVEAREA;
					thread::Sleep(16);
				}

				string name(sceLsdbGetName(&req->app));

				((void (*)(const string *))applauncherHomeGetLivearea()->func[
					LIVEAREA_PLUGIN_FUNC_APP_ADDED])(&name);
			}
			job = applauncherUpdateAppInfoDiffJobStart(APPLAUNCHER_HOME_JOB_QUEUE(),
				req->app, req->icons, icon, req->diff, applauncherHomeOnJobDone,
				&s_appInstallSemaphore);
		} else if (req->mode == 2) {
			s_newAppAdded = true;
			job = applauncherUpdateAppInfoJobStart(APPLAUNCHER_HOME_JOB_QUEUE(), req->app,
				req->icons, icon, applauncherHomeOnJobDone,
				&s_appInstallSemaphore);
		}
		if (job.get() == NULL)
			return APPLAUNCHER_ERROR_NO_MEMORY;
		s_appInstallSemaphore.Acquire();
		return static_cast<ApplauncherLsdbJob *>(job.get())->m_ret;
	}

	ApplauncherPage *page;
	bool newPage;
	int pos = -1;

	for (;;) {
		thread::RMutex::main_thread_mutex.Lock();
		if (g_applauncherModeManager->m_modes[APPLAUNCHER_MODE_EDIT] == 0)
			break;
		thread::RMutex::main_thread_mutex.Unlock();
	}
	newPage = false;
	page = NULL;
	if (g_applauncherHome->m_appCount < 500) {
		ApplauncherPage *last = applauncherHomeGetLastPage();

		if (last->CanAddIcon()) {
			page = last;
			if (last->GetLastIcon().get() == NULL)
				pos = 0;
			else
				pos = last->GetLastIcon()->m_slot + 1;
		} else if (g_applauncherHome->m_scroller.m_items.size() >= 10) {
			for (i = g_applauncherHome->m_scroller.m_items.size() - 1; i >= 0; i--) {
				ApplauncherPage *item = static_cast<ApplauncherPage *>(
					g_applauncherHome->m_scroller.m_items.ptr[i]);

				if (item->m_type != 1) {
					int slot = item->GetFreeSlot();

					if (slot >= 0) {
						pos = slot;
						page = item;
						break;
					}
				}
			}
		} else {
			pos = 0;
			newPage = true;
		}
	}

	if (page == NULL && !newPage) {
		thread::RMutex::main_thread_mutex.Unlock();

		common::SharedPtr<job::JobItem> job = applauncherInsertAppInfoJobStart(
			APPLAUNCHER_HOME_JOB_QUEUE(), req->app, req->icons, -100000000, icon,
			applauncherHomeOnJobDone, &s_appInstallSemaphore);

		s_appInstallSemaphore.Acquire();
		ret = static_cast<ApplauncherLsdbJob *>(job.get())->m_ret;
		if (ret == 0) {
			thread::RMutex::main_thread_mutex.Lock();
			g_applauncherHome->m_waitingAppCount++;
			if (g_applauncherModeManager->m_modes[APPLAUNCHER_MODE_EDIT] == 0)
				g_applauncherHome->ShowNotice();
			thread::RMutex::main_thread_mutex.Unlock();
		}
		return ret;
	}

	LsdbAppInfoPtr app(new LsdbAppInfo());

	*app = *req->app;
	icon->pos = pos;
	if ((SceLsdb_0B16523B(&app) & 4) == 0) {
		rtc::Tick tick = 0;
		char now[64];

		if (rtc::GetCurrentTick(&tick) == 0 &&
		    rtc::FormatSQLiteDateTime(now, sizeof(now), &tick, ' ') == 0)
			(*app)[LSDB_KEY_LLTIME].SetText(now);
	}

	common::SharedPtr<job::JobItem> job;
	ApplauncherPage *last = NULL;

	if (newPage) {
		LsdbPage param;
		common::SharedPtr<LsdbThemeInfo> info = *shellThemeGetInfo();

		param.bgColor = shellStartupGetPageColor();
		if (info.get() != NULL) {
			int index = g_applauncherHome->m_scroller.m_items.size();

			if (index < 10) {
				BackgroundParam *bg = &info->m_homeProperty.m_bgParam[index];

				if (bg->m_imageFilePath.length() != 0) {
					shellThemeInfoGetFilePath(info.get(), &param.themeFile,
								  bg->m_imageFilePath);
					param.texWidth = 960;
					param.texHeight = 512;
					param.imageWidth = 960;
					param.imageHeight = 512;
				}
				if (bg->m_waveType >= 0)
					param.bgColor = bg->m_waveType;
				if (info->m_status >= 100)
					param.reserved01 = bg->GetFontColor();
			}
		}
		last = applauncherHomeGetLastPage();
		job = applauncherCreateNewPageAndInsertAppJobStart(APPLAUNCHER_HOME_JOB_QUEUE(),
			last->m_index + 1, param, app, req->icons, icon, applauncherHomeOnJobDone,
			&s_appInstallSemaphore);
	} else {
		job = applauncherInsertAppInfoJobStart(APPLAUNCHER_HOME_JOB_QUEUE(), app,
			req->icons, page->m_index, icon, applauncherHomeOnJobDone,
			&s_appInstallSemaphore);
	}
	if (job.get() == NULL) {
		thread::RMutex::main_thread_mutex.Unlock();
		return APPLAUNCHER_ERROR_NO_MEMORY;
	}

	*async = false;
	g_applauncherHome->m_homeFlags[1] = true;
	thread::RMutex::main_thread_mutex.Unlock();
	s_appInstallSemaphore.Acquire();
	ret = static_cast<ApplauncherLsdbJob *>(job.get())->m_ret;
	if (ret == 0) {
		thread::RMutex::main_thread_mutex.Lock();
		if (newPage)
			page = applauncherHomeAddPage(NULL, last, false, false);

		common::SharedPtr<ApplauncherIcon> added = page->AddIcon(icon);

		page->UpdateFocusLinks();
		g_applauncherHome->m_appCount++;
		added->Select();
		page->OnIconAdded();
		if (req->focus) {
			g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_SHOW_HOME]();
			applauncherHomeJumpTo(page);
			if (page->m_type != 2)
				applauncherHomeCloseAppPage(false);
		}
		added->m_added = true;
		if (added->GetWidget() != NULL) {
			ui::Widget *widget = added->GetWidget();

			widget->m_transition_complete = false;
			widget->m_transition = 0xff;
			widget->DoTransition_core(0.0f, true);
			ScePafWidget_4588699E(widget, NULL, 2, 0.0f, NULL);
		}
		g_applauncherHome->m_homeFlags[1] = false;
		if (g_applauncherHome->m_waitDialog >= 0) {
			SceCommonGuiDialog_BC311DC0(g_applauncherHome->m_waitDialog);
			g_applauncherHome->m_waitDialog = -1;
		}
		if (g_applauncherModeManager->m_modes[APPLAUNCHER_MODE_EDIT] == 0)
			g_applauncherHome->UpdateNoticeText();
		g_applauncherHome->UpdateSnaps();
		thread::RMutex::main_thread_mutex.Unlock();
	}
	return ret;
}

/* the title's icon on a page, else on the application's page, and the page it is on
   (0x810AE89C) */
common::SharedPtr<ApplauncherIcon> ApplauncherHome::FindIcon(const string &titleId,
							   ApplauncherPage **page)
{
	for (ShellSnapList::iterator it = m_scroller.m_items.begin();
	     it != m_scroller.m_items.end(); ++it) {
		ApplauncherPage *item = static_cast<ApplauncherPage *>(*it);
		common::SharedPtr<ApplauncherIcon> icon = item->FindIcon(titleId);

		if (icon.get() != NULL) {
			if (page != NULL)
				*page = item;
			return icon;
		}
	}
	if (m_appPage != NULL) {
		common::SharedPtr<ApplauncherIcon> icon = m_appPage->FindIcon(titleId);

		if (icon.get() != NULL) {
			if (page != NULL)
				*page = m_appPage;
			return icon;
		}
	}
	if (page != NULL)
		*page = NULL;
	return common::SharedPtr<ApplauncherIcon>();
}

/*
 * The title's icon on the application's page, else on a page or in a folder of one; the folder
 * it is in and its page are given back (0x810AEA46).
 */
common::SharedPtr<ApplauncherIcon> ApplauncherHome::FindIconInFolder(const string &titleId,
	common::SharedPtr<ApplauncherIcon> *folder, ApplauncherPage **page)
{
	if (m_appPage != NULL) {
		common::SharedPtr<ApplauncherIcon> icon = m_appPage->FindIcon(titleId);

		if (icon.get() != NULL) {
			if (page != NULL)
				*page = m_appPage;
			return icon;
		}
	}
	for (ShellSnapList::iterator it = m_scroller.m_items.begin();
	     it != m_scroller.m_items.end(); ++it) {
		ApplauncherPage *item = static_cast<ApplauncherPage *>(*it);
		common::SharedPtr<ApplauncherIcon> icon = item->FindIcon(titleId, folder);

		if (icon.get() != NULL || folder->get() != NULL) {
			if (page != NULL)
				*page = item;
			return icon;
		}
	}
	return common::SharedPtr<ApplauncherIcon>();
}

/*
 * The title's application taken off: livespace_db told (with the whole folder when the folder
 * or the application's page goes), the folder's saved order told, its icon deleted or the
 * folder's child taken out, and outside sorting the notice updated and the empty pages deleted
 * (0x810AEBFA).
 */
int ApplauncherHome::RemoveApp(const string &titleId)
{
	common::SharedPtr<ApplauncherIcon> folder;
	ApplauncherPage *page = NULL;
	int ret = 0;
	bool whole;

	thread::RMutex::main_thread_mutex.Lock();
	{
		common::SharedPtr<ApplauncherIcon> icon =
			g_applauncherHome->FindIconInFolder(titleId, &folder, NULL);

		whole = false;
		if (folder.get() != NULL && folder->m_data.get() != NULL &&
		    folder->m_data->folder.get() != NULL && folder->m_data->folder->iconCount == 1) {
			whole = true;
		} else if (icon.get() != NULL) {
			ApplauncherPage *appPage = icon->m_page;

			if (appPage != NULL && appPage->m_type == 2) {
				folder = applauncherPageAsFolder(appPage)->m_icon;
				if (appPage->GetIconCount() == 1)
					whole = true;
			}
		}
		if ((icon.get() != NULL || folder.get() != NULL) && g_applauncherHome != NULL)
			g_applauncherHome->m_appCount--;
	}
	thread::RMutex::main_thread_mutex.Unlock();

	int err = SceLsdb_B33FE18E(g_shellLsdbClient->client, titleId, whole, 0);

	if (err != 0)
		return err;

	if (folder.get() != NULL && folder->m_page != NULL && folder->m_data.get() != NULL &&
	    folder->m_data->folder.get() != NULL && !whole) {
		vector<ApplauncherRemovedIcon> removed;
		ApplauncherRemovedIcon item;

		item.m_id = folder->m_data->folder->id;
		item.m_pageIndex = folder->m_page->GetIndex();
		item.m_slot = folder->m_slot;
		removed.push_back(item);
		applauncherPagesRemoveIcons(&removed);
	}

	thread::RMutex::main_thread_mutex.Lock();
	{
		bool found = false;
		common::SharedPtr<ApplauncherIcon> icon = FindIcon(titleId, &page);

		if (icon.get() != NULL) {
			found = true;
			applauncherIconRemove(icon, true);
			applauncherIconDelete(icon, true, true);
		}
		if (folder.get() != NULL) {
			if (!found)
				folder->RemoveChild(titleId.c_str());
			if (whole) {
				if (g_applauncherHome->m_appPage != NULL &&
				    applauncherPageAsFolder(g_applauncherHome->m_appPage)->m_icon.get() == folder.get())
					applauncherHomeCloseAppPage(false);
				applauncherIconRemove(folder, true);
				applauncherIconDelete(folder, true, false);
			} else {
				folder->Update(true);
			}
		}
		if (g_applauncherModeManager->m_modes[APPLAUNCHER_MODE_EDIT] == 0) {
			g_applauncherHome->UpdateNoticeText();
			for (ShellSnapList::iterator it = g_applauncherHome->m_scroller.m_items.begin();
			     it != g_applauncherHome->m_scroller.m_items.end();) {
				ApplauncherPage *item = static_cast<ApplauncherPage *>(*it);

				if (item->m_type != 0 || !item->IsEmpty())
					++it;
				else
					it = applauncherHomeDeletePage(item);
			}
		}
	}
	thread::RMutex::main_thread_mutex.Unlock();
	return ret;
}

/* the overlay faded in (0x810AF160) */
void applauncherHomeShowOverlay(void)
{
	ui::Widget *overlay = g_applauncherHome->m_overlay;

	if (overlay != NULL)
		ScePafWidget_44B22ACA(overlay, (common::transition::Type)255, 0.0f, NULL, NULL);
}

/* the overlay faded out (0x810AF184) */
void applauncherHomeHideOverlay(void)
{
	ui::Widget *overlay = g_applauncherHome->m_overlay;

	if (overlay != NULL)
		ScePafWidget_F1176ACC(overlay, (common::transition::Type)255, 0.0f, NULL, NULL);
}

/* the scene changed: the pages kept loaded set to the pages, the notice page left out, ten at
   most (0x810AF1A8) */
void applauncherHomeOnSceneEvent(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	int count = g_applauncherHome->m_scroller.m_items.size();

	if (g_applauncherHome->m_noticePage != NULL)
		count--;
	if (count > 10)
		count = 10;
	applauncherPagesSetCount(count);
}

/* each page's icons given their data, the notice updated (0x810AF1CA) */
void ApplauncherHome::UpdatePageIcons(
	const vector<common::SharedPtr<LsdbIconList> > &pages)
{
	int i = 0;

	for (ShellSnapList::iterator it = m_scroller.m_items.begin();
	     it != m_scroller.m_items.end(); ++it, i++) {
		ApplauncherPage *page = static_cast<ApplauncherPage *>(*it);

		page->UpdateIcons(pages[i].get());
	}
	UpdateNoticeText();
}

/* the view's shows held, once (0x810AF220) */
void ApplauncherHome::HoldView()
{
	if (!m_viewHeld) {
		ScePafWidget_A96A0B4A(m_scroller.m_scrollView, 1);
		m_viewHeld = true;
	}
}

/* the view's shows let go of (0x810AF23A) */
void ApplauncherHome::ReleaseView()
{
	if (m_viewHeld) {
		ScePafWidget_A96A0B4A(m_scroller.m_scrollView, -1);
		m_viewHeld = false;
	}
}

/*
 * The theme read again for applauncherHomeApplyTheme: each page's parameters from the theme's
 * backgrounds where its icons are on (its image, wave and font colour), else from livespace_db;
 * then the icon of each title the theme names, its image the theme's where it has one, and on a
 * PlayStation TV the power button's (0x810AF256).
 */
int applauncherThemeReload(void)
{
	ApplauncherThemeData *theme = new ApplauncherThemeData();
	int pageCount = SceLsdb_94128795(g_shellLsdbClient->client);
	bool iconsEnabled = shellThemeIconsEnabled();
	common::SharedPtr<LsdbThemeInfo> *info = shellThemeGetInfo();

	if (s_themeData != NULL) {
		delete s_themeData;
		s_themeData = NULL;
	}
	if (pageCount < 0) {
		delete theme;
		return APPLAUNCHER_ERROR_NO_TITLE;
	}

	theme->m_pages.resize(pageCount);
	for (int i = 0; i < pageCount; i++)
		theme->m_pages[i] = common::SharedPtr<LsdbPage>(new LsdbPage());
	if (iconsEnabled) {
		if (info->get() != NULL) {
			for (int i = 0; i < pageCount; i++) {
				shellThemeInfoGetFilePath(info->get(), &theme->m_pages[i]->themeFile,
					info->get()->m_homeProperty.m_bgParam[i].m_imageFilePath);

				BackgroundParam *bg = &info->get()->m_homeProperty.m_bgParam[i];
				LsdbPage *page = theme->m_pages[i].get();

				page->bgColor = bg->m_waveType;
				page->texWidth = 960;
				page->imageWidth = 960;
				page->texHeight = 512;
				page->imageHeight = 512;
				if (info->get()->m_status >= 100)
					theme->m_pages[i]->reserved01 = bg->GetFontColor();
			}
		}
	} else {
		int result = SceLsdb_8FAD093B(g_shellLsdbClient->client, 0, pageCount,
					      &theme->m_pages);

		if (result < 0) {
			delete theme;
			return result;
		}
	}

	SceUInt32 titleCount = 0;
	const char **titles = shellThemeGetIconTitles(&titleCount);

	if (titles == NULL) {
		delete theme;
		return APPLAUNCHER_ERROR_NO_TITLE;
	}
	theme->m_apps.resize(titleCount);
	for (SceUInt32 i = 0; i < titleCount; i++, titles++) {
		LsdbIconPtr icon(new LsdbIcon());
		int result;

		{
			string titleId(*titles);

			result = SceLsdb_DE0DA538(g_shellLsdbClient->client, &titleId, &icon, NULL,
						  true);
		}
		if (result != 0)
			continue;
		if (iconsEnabled && info->get() != NULL) {
			const IconParam *param =
				info->get()->m_homeProperty.GetIconParam(*titles, 0);

			if (param != NULL && param->m_iconFilePath.length() != 0)
				shellThemeInfoGetFilePath(info->get(), &icon->iconPath,
							  param->m_iconFilePath);
		}
		theme->m_apps[i] = icon;
	}

	if (system::Model::GetType() == 1) {
		LsdbIconPtr power(new LsdbIcon());

		power->type = APPLAUNCHER_ICON_TYPE_POWER;
		power->icon0Type = 8;		/* PLACEHOLDER: the power button's image kind */
		if (info->get() != NULL) {
			const IconParam *param =
				info->get()->m_homeProperty.GetIconParam("", APPLAUNCHER_ICON_TYPE_POWER);

			if (param != NULL && param->m_iconFilePath.length() != 0)
				shellThemeInfoGetFilePath(info->get(), &power->iconPath,
							  param->m_iconFilePath);
		}
		if (power->iconPath.length() == 0)
			power->iconPath = "vs0:data/internal/icon/power.png";
		theme->m_apps.push_back(power);
	}
	s_themeData = theme;
	return 0;
}

/* the applications with no room yet set, the notice shown for them (0x810AFA08) */
void applauncherHomeSetWaitingAppCount(int count)
{
	g_applauncherHome->m_waitingAppCount = count;
	if (count > 0)
		g_applauncherHome->ShowNotice();
}

/* the title's icon given its new data (its folder's taken in after); without an icon, the
   cached image of its icon cleared, loaded again and given to topmenu (0x810AFA24) */
void applauncherHomeUpdateApp(const string &titleId, const LsdbIconPtr &data)
{
	common::SharedPtr<ApplauncherIcon> folder;
	common::SharedPtr<ApplauncherIcon> icon =
		g_applauncherHome->FindIconInFolder(titleId, &folder, NULL);

	if (icon.get() != NULL) {
		icon->SetData(data);
		if (folder.get() != NULL)
			folder->UpdateFolder();
	} else if (data.get() != NULL) {
		{
			intrusive_ptr<cache::CacheImageInterface> image;

			applauncherIconCacheLoad(string(data->iconPath.c_str()), &image, NULL, 0,
						 false);
			if (image.get() != NULL)
				applauncherIconCacheClear(image);
		}
		{
			intrusive_ptr<cache::CacheImageInterface> image;

			applauncherIconCacheLoad(string(data->iconPath.c_str()), &image, NULL, 0,
						 false);
			((void (*)(const string &, intrusive_ptr<cache::CacheImageInterface> *))
				g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_SET_APP_IMAGE])
				(titleId, &image);
		}
		if (folder.get() != NULL)
			folder->UpdateFolder();
	}
}

/* the pages' background images listed (0x810AFC08) */
void applauncherHomeGetPageImages(vector<string> *paths)
{
	for (ShellSnapList::iterator it = g_applauncherHome->m_scroller.m_items.begin();
	     it != g_applauncherHome->m_scroller.m_items.end(); ++it) {
		ApplauncherPage *page = static_cast<ApplauncherPage *>(*it);

		if (!page->m_param.themeFile.empty())
			paths->push_back(page->m_param.themeFile);
	}
}

/* the empty pages deleted, the last kept (0x810AFCA0) */
void applauncherHomeDeleteEmptyPages(void)
{
	for (ShellSnapList::iterator it = g_applauncherHome->m_scroller.m_items.begin();
	     it != g_applauncherHome->m_scroller.m_items.end();) {
		ApplauncherPage *page = static_cast<ApplauncherPage *>(*it);

		if (page->m_type != 0 || !page->IsEmpty())
			++it;
		else
			it = applauncherHomeDeletePage(page);
	}
}

/* the application's page opened from its icon: the one open closed, the dots hidden, the
   icons around it moved aside on a page and the overlay made; saved while sorting
   (0x810AFD30) */
ApplauncherPage *applauncherHomeOpenAppPage(const common::SharedPtr<ApplauncherIcon> &icon,
					    const char *name, bool animate)
{
	if (g_applauncherHome->m_appPage != NULL)
		applauncherHomeCloseAppPage(false);
	if (icon.get() == NULL)
		return NULL;
	if (icon->m_data.get() != NULL && icon->m_data->folder.get() != NULL) {
		g_applauncherHome->m_appPage = applauncherIconPoolOpenAppPage(icon, name, animate);
		if (g_applauncherHome->m_appPage != NULL) {
			g_applauncherHome->m_indicator->Hide(250.0f);
			if (icon->m_page->m_type == 0)
				icon->m_page->MoveIconsAside(icon);
			g_applauncherHome->CreateOverlay();
			if (g_applauncherModeManager->m_modes[APPLAUNCHER_MODE_EDIT] != 0)
				g_applauncherHome->m_appPage->BeginEdit();
		}
	}
	return g_applauncherHome->m_appPage;
}

/* the application's page open, NULL without one (0x810AFDEA) */
ApplauncherPage *applauncherHomeGetAppPage(void)
{
	return g_applauncherHome->m_appPage;
}

/* the closed application page's focus let go of when it is the icon (0x810AFDFA) */
void applauncherHomeClearAppIcon(const common::SharedPtr<ApplauncherIcon> &icon)
{
	ApplauncherHome *home = g_applauncherHome;

	if (home->m_appIcon.get() == icon.get()) {
		home->m_appIcon.reset();
		g_applauncherHome->m_appIconTime = 0.0f;
	}
}

/* the application's icon on any page (0x810AFE82) */
common::SharedPtr<ApplauncherIcon> ApplauncherHome::FindIcon(SceUInt64 appId)
{
	for (ShellSnapList::iterator it = m_scroller.m_items.begin();
	     it != m_scroller.m_items.end(); ++it) {
		ApplauncherPage *page = static_cast<ApplauncherPage *>(*it);
		common::SharedPtr<ApplauncherIcon> icon = page->FindIcon(appId);

		if (icon.get() != NULL)
			return icon;
	}
	return common::SharedPtr<ApplauncherIcon>();
}

/* PLACEHOLDER name: the interface's entry #0, the application put on the home (0x810AFF78) */
int applauncherInterfaceInstallApp(const ApplauncherInstallRequest *req, bool *async)
{
	return applauncherHomeInstallApp(req, async);
}

/* the interface's RemoveApp: the title's application taken off (0x810AFF80) */
int applauncherInterfaceRemoveApp(const string &titleId)
{
	return g_applauncherHome->RemoveApp(titleId);
}

/*
 * PLACEHOLDER name: the interface's entry #2. Without an icon (or a folder) of the title in
 * focus, nothing; else livearea_plugin waited for (ten seconds at most) and its LiveArea
 * opened, told whether entry #0 just added the application. The title's icon is then faded in,
 * once its image is ready when it is still all but transparent, and topmenu told of the
 * title (0x810AFF96).
 */
int applauncherInterfaceOpenLiveArea(const LsdbAppInfoPtr *app, bool animate)
{
	const char *name = sceLsdbGetName(app);
	bool newApp;
	bool wait;
	int ret;
	int i;

	if (name != NULL) {
		thread::RMutexLock lock(&thread::RMutex::main_thread_mutex);
		common::SharedPtr<ApplauncherIcon> folder;
		common::SharedPtr<ApplauncherIcon> icon =
			g_applauncherHome->FindIconInFolder(string(name), &folder, NULL);

		if (icon.get() == NULL && folder.get() == NULL) {
			s_newAppAdded = false;
			return 0;
		}
	}

	for (i = 0; applauncherHomeGetLivearea() == NULL; i++) {
		if (i > 600)
			return APPLAUNCHER_ERROR_NO_LIVEAREA;
		thread::Sleep(16);
	}
	for (i = 0; applauncherHomeGetLivearea()->func[LIVEAREA_PLUGIN_FUNC_OPEN] == NULL; i++) {
		if (i > 600)
			return APPLAUNCHER_ERROR_NO_LIVEAREA;
		thread::Sleep(16);
	}
	newApp = s_newAppAdded;
	ret = ((int (*)(const LsdbAppInfoPtr *, bool, bool))
		applauncherHomeGetLivearea()->func[LIVEAREA_PLUGIN_FUNC_OPEN])(app, animate,
									     newApp);
	s_newAppAdded = false;
	if (name == NULL)
		return APPLAUNCHER_ERROR_NO_TITLE;

	wait = false;
	thread::RMutex::main_thread_mutex.Lock();
	{
		common::SharedPtr<ApplauncherIcon> icon =
			g_applauncherHome->FindIcon(string(name), NULL);

		if (icon.get() != NULL) {
			ui::Widget *widget = icon->GetWidget();

			icon->Update(true);
			if (widget != NULL) {
				if (widget->m_show_alpha <= 0.02f) {
					if (icon->m_image.get() == NULL) {
						wait = true;
						widget->AddEventListener(
							ui::Handler::CB_STATE_READY_CACHEIMAGE,
							new ui::EventCBListener(
								applauncherHomeOnIconImage, NULL),
							true);
						widget->AddEventListener(0x100007,
							new ui::EventCBListener(
								applauncherHomeOnIconImage, NULL),
							true);
					} else {
						icon->Show();
					}
				}
			} else {
				icon->Show();
			}
		}
	}
	thread::RMutex::main_thread_mutex.Unlock();

	if (wait) {
		s_appInstallSemaphore.Acquire();
		thread::RMutex::main_thread_mutex.Lock();
		{
			common::SharedPtr<ApplauncherIcon> icon =
				g_applauncherHome->FindIcon(string(name), NULL);

			if (icon.get() != NULL) {
				ui::Handler *widget = icon->GetWidget();

				widget->DeleteEventCallback(ui::Handler::CB_STATE_READY_CACHEIMAGE,
							    applauncherHomeOnIconImage, NULL);
				widget->DeleteEventCallback(0x100007, applauncherHomeOnIconImage,
							    NULL);
				icon->Show();
			}
		}
		thread::RMutex::main_thread_mutex.Unlock();
	}
	((void (*)(const char *))
		g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_LIVEAREA_OPENED])(name);
	return ret;
}

/* the application's page opened from its icon when its page is in view; else the view sent to
   its page and the opening kept for when it gets there (0x810B04F8) */
void ApplauncherHome::OpenApp(const common::SharedPtr<ApplauncherIcon> &icon, const string &name,
			      bool animate)
{
	if (icon.get() == NULL)
		return;
	if (m_appPage != NULL)
		applauncherHomeCloseAppPage(false);
	if (icon->m_page == NULL)
		return;
	if (static_cast<ApplauncherPage *>(m_scroller.GetItemBySnapID(m_snapId)) ==
	    icon->m_page) {
		applauncherHomeOpenAppPage(icon, name.c_str(), animate);
		return;
	}
	m_dropPage = g_applauncherHome->m_scroller.GetSnapID(icon->m_page);
	m_appName = name;
	m_openAnimate = animate;
}

/* the title's icon given the focus (marked selected when asked), its page scrolled to and the
   other application's page closed; for a title in a folder, the folder's page opened instead
   (0x810B0578) */
void ApplauncherHome::FocusApp(const string &titleId, bool select)
{
	thread::RMutex::main_thread_mutex.Lock();
	{
		common::SharedPtr<ApplauncherIcon> folder;
		ApplauncherPage *page;
		common::SharedPtr<ApplauncherIcon> icon = FindIconInFolder(titleId, &folder, &page);

		if (icon.get() != NULL) {
			if (m_appPage != NULL && m_appPage != icon->m_page)
				applauncherHomeCloseAppPage(false);
			if (select)
				icon->Select();
			g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_SHOW_HOME]();
			if (page->m_type != 2)
				g_applauncherHome->m_scroller.ScrollTo(page);
			icon->Focus();
		} else if (folder.get() != NULL) {
			g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_SHOW_HOME]();
			page = folder->m_page;
			if (page->m_type != 2)
				g_applauncherHome->m_scroller.ScrollTo(page);
			OpenApp(folder, titleId, select);
		}
	}
	thread::RMutex::main_thread_mutex.Unlock();
}

/* the interface's entry #4: the title's icon given the focus (0x810B071E) */
void applauncherInterfaceFocusApp(const string &titleId, bool select)
{
	g_applauncherHome->FocusApp(titleId, select);
}

/* the interface's entry #5: the selected mark of the title's icon cleared (0x810B0736) */
void applauncherInterfaceUnselectApp(const string &titleId)
{
	thread::RMutex::main_thread_mutex.Lock();
	{
		common::SharedPtr<ApplauncherIcon> icon =
			g_applauncherHome->FindIcon(titleId, NULL);

		if (icon.get() != NULL)
			icon->Unselect();
	}
	thread::RMutex::main_thread_mutex.Unlock();
}

/* the home's layer left (also the interface's entry #6): the selected marks of the focused page's
   icons cleared (0x810B07DC) */
void applauncherHomeOnLayerLeave(void *data)
{
	ApplauncherPage *page =
		static_cast<ApplauncherPage *>(g_applauncherHome->m_scroller.GetFocused());

	if (page != NULL)
		page->UnselectAll();
}

/* the interface's entry #7: the icon effect of the title ended (0x810B07FC) */
void applauncherInterfaceEndIconEffect(const string &titleId)
{
	applauncherIconEffectEnd(titleId);
}

/* the interface's entry #8: any icon effect ended (0x810B0804) */
void applauncherInterfaceEndIconEffects(void)
{
	applauncherIconEffectEndAll();
}

/* the interface's entry #9: the view scrolled to the notice page (0x810B080C) */
void applauncherInterfaceScrollToNotice(void)
{
	thread::RMutex::main_thread_mutex.Lock();
	ApplauncherPage *page = g_applauncherHome->m_noticePage;

	if (page != NULL && page->m_type != 2)
		g_applauncherHome->m_scroller.ScrollTo(page);
	thread::RMutex::main_thread_mutex.Unlock();
}

/* the interface's entry #10: the view scrolled to the first page (0x810B0844) */
void applauncherInterfaceScrollToFirst(void)
{
	thread::RMutex::main_thread_mutex.Lock();
	if (g_applauncherHome->m_scroller.m_items.size() != 0) {
		ApplauncherPage *page =
			static_cast<ApplauncherPage *>(g_applauncherHome->m_scroller.m_items[0]);

		if (page->m_type != 2)
			g_applauncherHome->m_scroller.ScrollTo(page);
	}
	thread::RMutex::main_thread_mutex.Unlock();
}

/* the interface's entry #11: the icon image of the application's folder loaded (0x810B088E) */
void applauncherInterfaceLoadIconImage(const char *dir,
				       intrusive_ptr<cache::CacheImageInterface> *image)
{
	applauncherIconLoadImage(dir, image);
}

/* the interface's entry #3: the image of the title's icon given; without an icon, the image
   asked of the pages when a request is given; false without either (0x810B0896) */
bool applauncherInterfaceGetIconImage(intrusive_ptr<cache::CacheImageInterface> *image,
				      const string &titleId, void *param, void *data)
{
	common::SharedPtr<ApplauncherIcon> icon = g_applauncherHome->FindIcon(titleId, NULL);

	if (icon.get() != NULL) {
		*image = icon->m_image;
		return true;
	}
	if (param != NULL) {
		applauncherPagesRequestImage(titleId, param, data);
		return true;
	}
	return false;
}

/* the background taken down: the view's texture and its request dropped, the image's task
   ended and its path cleared, the surface pool made room in when the image was cached
   (0x810B09B8) */
void ApplauncherHome::ClearBackground()
{
	ui::Widget *view = m_scrollView;

	{
		intrusive_ptr<ui::TextureSource> source;

		view->SetDrawObjMaskBit(&source, 0, 0);
	}
	view->SetTexture(intrusive_ptr<graph::Surface>());
	m_task.clear();
	m_imagePath.clear();
	if (m_imageCached) {
		applauncherDataCacheMakeSpace();
		m_imageCached = false;
	}
}

/* the background shown: the view faded in (to nothing without an image), the wave updated
   unless it was, and the icons dimmed over an image (0x810B0A46) */
void ApplauncherHome::ShowBackground()
{
	ui::Widget *view = m_scrollView;
	math::v4 color(0.0f, 0.0f, 0.0f, 0.0f);

	m_backgroundState = 1;
	view->KillAnim(ui::Widget::ANIM_COLOR);
	if (m_imagePath.length() != 0)
		color = math::v4(1.0f, 1.0f, 1.0f, 1.0f);
	if (!m_waveUpdated)
		scePafGraphicsUpdateCurrentWave(m_waveType, 1.0f);
	view->SetColor(color * m_alpha, new Timer(300.0f, Timer::FUNC_QUAD_IN),
		       ui::Widget::ANIM_COLOR, 0);
	if (m_imagePath.length() != 0)
		applauncherIconPoolSetDim(true);
}

/* the page's background loaded: its image and wave taken; shown at once without an image or
   with the same one, else the image's area fitted to the screen's 30:17 and its loading
   begun (0x810B0B28) */
bool ApplauncherHome::LoadBackground(ApplauncherPage *page)
{
	if (page == NULL)
		return true;

	bool same = m_imagePath == page->m_param.themeFile;

	m_imagePath = page->m_param.themeFile;
	m_waveType = page->m_param.bgColor;
	m_backgroundState = 3;
	if (m_imagePath.length() == 0 || same) {
		ShowBackground();
		return true;
	}

	SceUInt32 width = page->m_param.imageWidth;
	SceUInt32 height = page->m_param.imageHeight;
	SceUInt32 texHeight = page->m_param.texHeight;
	SceUInt32 texWidth = page->m_param.texWidth;
	float unitX = (float)width / 30.0f;
	float fitWidth = (float)width;
	float unitY = (float)height / 17.0f;
	float fitHeight = (float)height;

	if (width == 960 && height == 512 && texWidth == 960 && texHeight == 512) {
		m_texU = 0.0f;
		m_texV = 0.0f;
		m_texScale[0] = 1.0f;
		m_texScale[1] = 1.0f;
	} else {
		float u = 0.0f;
		float du = 0.0f;

		if (unitX == unitY) {
			fitWidth = (float)width;
			fitHeight = (float)height;
		} else if (unitX < unitY) {
			fitHeight = unitX * 17.0f;
		} else {
			fitWidth = unitY * 30.0f;
		}
		if (texWidth != 0) {
			float margin = ((float)width - fitWidth) * 0.5f;

			u = margin / (float)texWidth;
			du = (fitWidth + margin) / (float)texWidth - u;
		}
		m_texU = u;
		m_texScale[0] = du;
		if (texHeight != 0) {
			float margin = ((float)height - fitHeight) * 0.5f;
			float texBottom = (fitHeight + margin) / (float)texHeight;
			float texTop = margin / (float)texHeight;

			texTop = texTop + (texBottom - texTop) * (1.0f / 17.0f);
			m_texV = texTop;
			m_texScale[1] = texBottom - texTop;
		} else {
			m_texV = 0.0f;
			m_texScale[1] = 0.0f;
		}
	}

	string path(m_imagePath);

	m_homeFlags[2] = true;
	applauncherDataCacheLoad(path, &m_task, NULL, NULL, false);
	return true;
}

/* the background of the page in view loaded again, the view first set to its image's colour
   at once (0x810B0CF6) */
void applauncherHomeReloadBackground(void)
{
	if (static_cast<ApplauncherPage *>(g_applauncherHome->m_scroller.GetFocused()) != NULL) {
		ApplauncherPage *page =
			static_cast<ApplauncherPage *>(g_applauncherHome->m_scroller.GetFocused());

		g_applauncherHome->m_scrollView->SetColor(page->m_param.themeFile.length() != 0 ?
			math::v4(1.0f, 1.0f, 1.0f, 0.0f) : math::v4(0.0f, 0.0f, 0.0f, 0.0f),
			NULL, ui::Widget::ANIM_COLOR, 0);
	}
	g_applauncherHome->LoadBackground(
		static_cast<ApplauncherPage *>(g_applauncherHome->m_scroller.GetFocused()));
}

/* the interface's entry #12: the title's icon marked for an update (0x810B0DB2) */
int applauncherInterfaceMarkAppUpdate(const char *titleId)
{
	thread::RMutex::main_thread_mutex.Lock();
	{
		common::SharedPtr<ApplauncherIcon> icon =
			g_applauncherHome->FindIcon(string(titleId), NULL);

		if (icon.get() != NULL)
			icon->MarkUpdate();
	}
	thread::RMutex::main_thread_mutex.Unlock();
	return 1;
}

/* the interface's entry #13: the title's data asked for again, by its icon or, without one, of
   the pages (0x810B0EA2) */
int applauncherInterfaceRequestAppData(const char *titleId)
{
	thread::RMutex::main_thread_mutex.Lock();
	{
		common::SharedPtr<ApplauncherIcon> icon =
			g_applauncherHome->FindIcon(string(titleId), NULL);

		if (icon.get() != NULL)
			icon->RequestData();
		else
			applauncherPagesRequestData(string(titleId));
	}
	thread::RMutex::main_thread_mutex.Unlock();
	return 1;
}

/* the interface's entry #14: every page's icon images reloaded (0x810B0FD4) */
void applauncherInterfaceReloadIcons(void)
{
	thread::RMutex::main_thread_mutex.Lock();
	ApplauncherHome *home = g_applauncherHome;

	for (ShellSnapList::iterator it = home->m_scroller.m_items.begin();
	     it != home->m_scroller.m_items.end(); ++it) {
		ApplauncherPage *page = static_cast<ApplauncherPage *>(*it);

		page->ReloadIcons();
	}
	thread::RMutex::main_thread_mutex.Unlock();
}

/* the interface's entry #15: the kept request's state cleared (0x810B1026) */
void applauncherInterfaceClearRequest(void)
{
	if (g_applauncherModeManager->m_currentMode != -1)
		g_applauncherModeManager->m_nextMode = 0;
}

/* the interface's entry #17: the wave hidden, the view shown again and, but on a PS TV, the icons'
   effects of the page in view resumed (0x810B103E) */
void applauncherHomeHideWave(void)
{
	if (g_applauncherHome != NULL) {
		scePafGraphicsUpdateCurrentWave(g_applauncherHome->m_waveType, 0.5f);

		ui::Widget *view = g_applauncherHome->m_scroller.m_scrollView->GetViewParent();

		if (!view->m_transition_complete) {
			view->m_transition = 0xff;
			view->m_transition_complete = true;
			view->DoTransition_core(0.0f, true);
		}
		ScePafWidget_FF5BC957(view, NULL, 2, 0.0f, NULL);

		ApplauncherPage *page =
			static_cast<ApplauncherPage *>(g_applauncherHome->m_scroller.GetFocused());

		if (system::Model::GetType() != system::Model::Type_Dolce && page != NULL)
			page->ResumeIcons();
		g_applauncherHome->m_waveUpdated = false;
	}
}

/* the interface's entry #18: the index of the page in focus, -1 without one (0x810B10E2) */
int applauncherInterfaceGetFocusedPage(void)
{
	if (g_applauncherHome == NULL)
		return -1;

	ApplauncherPage *page =
		static_cast<ApplauncherPage *>(g_applauncherHome->m_scroller.GetFocused());

	if (page == NULL)
		return -1;
	return page->m_index;
}

/* the interface's entry #19: the application's page closed (0x810B1110) */
void applauncherInterfaceCloseAppPage(void)
{
	if (g_applauncherHome != NULL)
		applauncherHomeCloseAppPage(false);
}

/* the interface's entry #21: whether the title is on the home, as an icon or in a folder
   (0x810B1126) */
bool applauncherInterfaceHasApp(const string &titleId)
{
	if (g_applauncherHome == NULL)
		return false;

	common::SharedPtr<ApplauncherIcon> folder;
	common::SharedPtr<ApplauncherIcon> icon =
		g_applauncherHome->FindIconInFolder(titleId, &folder, NULL);

	if (icon.get() != NULL || folder.get() != NULL)
		return true;
	return false;
}

/* the interface's entry #22: the title's change log opened (0x810B12B4) */
int applauncherInterfaceOpenChangeLog(const paf::string *titleId, void *loadArg,
				      ApplauncherChangeLogCallback callback, int value, void *arg)
{
	return applauncherChangeLogOpen(titleId, loadArg, callback, value, arg);
}

/* the interface's entry #23: the theme reloaded and, once it has data, its UI refreshed on the
   main thread (0x810B12C6) */
int applauncherInterfaceRefreshTheme(void)
{
	applauncherThemeReload();
	if (s_themeData != NULL)
		thread::CallQueue::main_thread_callqueue.Enqueue("refreshThemeUICB",
								 applauncherHomeOnRefreshThemeUI,
								 NULL);
	return 0;
}

/* the interface's entry #24: the theme reloaded (0x810B12FA) */
int applauncherInterfaceReloadTheme(void)
{
	return applauncherThemeReload();
}

/* the theme read last applied: each page given its background, each icon of the theme's
   titles (and a theme's power icon) its data, or without an icon on the home the top menu its
   image again; the folders of those icons updated and the dots too (0x810B1302) */
void applauncherHomeApplyTheme(void)
{
	ApplauncherThemeData *theme = s_themeData;
	ApplauncherHome *home = g_applauncherHome;
	SceUInt32 i = 0;

	s_themeData = NULL;
	home->m_imageCached = true;
	for (ShellSnapList::iterator it = home->m_scroller.m_items.begin();
	     it != home->m_scroller.m_items.end(); ++it, ++i) {
		ApplauncherPage *page = static_cast<ApplauncherPage *>(*it);

		if (i >= theme->m_pages.size())
			break;
		if (theme->m_pages[i].get() != NULL)
			page->SetParam(theme->m_pages[i].get(), false);
	}

	SceUInt32 count = 0;
	const char **titles = shellThemeGetIconTitles(&count);

	if (titles == NULL) {
		delete theme;
		return;
	}
	applauncherIconCacheMakeSpace();

	vector<common::SharedPtr<ApplauncherIcon> > folders;
	SceUInt32 index = 0;

	for (vector<LsdbIconPtr>::iterator it = theme->m_apps.begin();
	     it != theme->m_apps.end(); ++it, ++titles, ++index) {
		LsdbIconPtr app = *it;
		const char *titleId;

		if (app.get() == NULL)
			continue;
		if (index < count)
			titleId = *titles;
		else if (app->type == APPLAUNCHER_ICON_TYPE_POWER)
			titleId = "power";
		else
			break;

		common::SharedPtr<ApplauncherIcon> folder;
		common::SharedPtr<ApplauncherIcon> icon =
			g_applauncherHome->FindIconInFolder(string(titleId), &folder, NULL);

		if (icon.get() != NULL) {
			if (app->type == APPLAUNCHER_ICON_TYPE_POWER) {
				icon->m_data->iconPath = app->iconPath;
				icon->Update(true);
			} else {
				icon->SetData(app);
			}
		} else {
			intrusive_ptr<cache::CacheImageInterface> image;

			((void (*)(const string &, intrusive_ptr<cache::CacheImageInterface> *))
				g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_SET_APP_IMAGE])
				(app->titleId, &image);
			applauncherIconCacheClearPath(app->iconPath);
			applauncherIconCacheLoad(string(app->iconPath.c_str()), &image, NULL, 0, false);
			((void (*)(const string &, intrusive_ptr<cache::CacheImageInterface> *))
				g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_SET_APP_IMAGE])
				(app->titleId, &image);
		}
		if (folder.get() != NULL)
			folders.push_back(folder);
	}
	for (vector<common::SharedPtr<ApplauncherIcon> >::iterator it = folders.begin();
	     it != folders.end(); ++it) {
		common::SharedPtr<ApplauncherIcon> folder = *it;

		folder->UpdateFolder();
	}
	if (g_applauncherHome->m_indicator != NULL)
		g_applauncherHome->m_indicator->Update();
	delete theme;
}

/* the theme read last applied, at the plugin's start (0x810B17CE) */
int applauncherThemeRefresh(void)
{
	applauncherHomeApplyTheme();
	return 0;
}

/* the interface's entry #26: the kept request marked (0x810B17D8) */
int applauncherInterfaceSetRequest(void)
{
	if (g_applauncherModeManager->m_currentMode != -1)
		g_applauncherModeManager->m_nextMode = 1;
	return 0;
}

/* the icon of a page whose data's state has the id (0x810B17F2) */
common::SharedPtr<ApplauncherIcon> ApplauncherHome::FindIconById(int id)
{
	for (ShellSnapList::iterator it = m_scroller.m_items.begin();
	     it != m_scroller.m_items.end(); ++it) {
		ApplauncherPage *page = static_cast<ApplauncherPage *>(*it);
		common::SharedPtr<ApplauncherIcon> icon = page->FindIconById(id);

		if (icon.get() != NULL)
			return icon;
	}
	return common::SharedPtr<ApplauncherIcon>();
}

/* the scroller snapped to a page: the drop's page forgotten when it is another, and a page
   newly in view made the one whose background is shown, with a sound when one was before
   (0x810B18DE) */
void applauncherHomeOnSnapped(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	int dropPage = g_applauncherHome->m_dropPage;

	g_applauncherHome->m_dragPos = math::v2(10000.0f, 0.0f);
	if (dropPage >= 0 && dropPage != e->m_data[0])
		g_applauncherHome->m_dropPage = -1;
	if (g_applauncherHome->m_snapId == e->m_data[0])
		return;

	ApplauncherPage *page = static_cast<ApplauncherPage *>(
		g_applauncherHome->m_scroller.GetItemBySnapID(e->m_data[0]));

	if (page == NULL)
		return;

	int snapId = g_applauncherHome->m_snapId;

	g_applauncherHome->m_dropIndex = -1;
	g_applauncherHome->m_snapId = e->m_data[0];
	g_applauncherHome->UpdateBackground(page);
	if (snapId != -1)
		Framework::Instance()->PlayCommonSE(33, 1.0f, false);
#if !defined(FW_TYPE_CEX)
	ScePafAutoTestTty_9E10E873(1, "/livespace/home_page%d", page->m_index);
#endif
}

/* the background's change waited out: the background cleared unless the page in view has the
   same image, not taken from the surface pool, then the page's loaded (0x810B1964) */
void applauncherHomeOnBackgroundTimeout(SceInt32 type, ui::Handler *self, ui::Event *e,
					void *data)
{
	ApplauncherPage *page = static_cast<ApplauncherPage *>(
		g_applauncherHome->m_scroller.GetItemBySnapID(g_applauncherHome->m_snapId));

	if (page == NULL || page->m_param.themeFile != g_applauncherHome->m_imagePath ||
	    g_applauncherHome->m_imageCached)
		g_applauncherHome->ClearBackground();
	g_applauncherHome->LoadBackground(page);
}

/* the overlay touched: the application's page closed, a dragged icon's widget first put back;
   without the page open the kept request marked again, or cleared when it is the first
   (0x810B19C8) */
void applauncherHomeOnOverlayEvent(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	if (g_applauncherHome->m_appPage != NULL) {
		if (g_applauncherDragItem.get() != NULL && g_applauncherDragItem->m_icon != NULL &&
		    g_applauncherDragItem->m_icon->m_widget != NULL &&
		    g_applauncherDragItem->m_icon->m_widget->m_icon_mode == 2)
			applauncherIconWidgetLeaveDrag(g_applauncherDragItem->m_icon);
		applauncherHomeCloseAppPage(true);
	} else if (g_applauncherModeManager->m_currentMode == 1) {
		g_applauncherModeManager->m_nextMode = 0;
	} else if (g_applauncherModeManager->m_currentMode != -1) {
		g_applauncherModeManager->m_nextMode = 1;
	}
}

/* the scroller touched: a drop's place forgotten on a touch's start, move or end, and a long
   press on the home in view, outside sorting and the IDU, marks the kept request (0x810B1A34) */
void applauncherHomeOnScrollTouch(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	int detail = (SceUInt8)e->GetDetail();

	if (detail == 1 || detail == 2 || detail == 3 || detail == 4)
		g_applauncherHome->m_dropIndex = -1;
	if (e->GetType() == 17 && shellStartupGetIduMode() != 1 && detail == 29 &&
	    g_applauncherModeManager->m_modes[APPLAUNCHER_MODE_EDIT] == 0 &&
	    ((bool (*)(int))g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_IS_LAYER_ACTIVE])
		    (g_applauncherHome->m_layer) &&
	    g_applauncherModeManager->m_currentMode != -1)
		g_applauncherModeManager->m_nextMode = 1;
}

/* the overlay touched while the first request is kept: the edit's state 5 asked for
   (0x810B1AB0) */
void applauncherHomeOnOverlayTouch(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	if (g_applauncherModeManager->m_currentMode == 1)
		g_applauncherHome->RequestEditState(5);
}

/* the edit's bar laid out: its buttons not held or kept placed from the left 16 apart, moved
   there when shown and shown there when not (0x810B1B04) */
void applauncherHomeOnEditBarEvent(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	ui::Widget *bar = static_cast<ui::Widget *>(self);
	float x = 0.0f;

	for (list<ui::Widget *>::iterator it = bar->m_children.begin(); it != bar->m_children.end();
	     ++it) {
		ui::Widget *button = *it;

		if (button->m_destroy_protected || button->m_hold_count > 0)
			continue;

		math::v4 pos = button->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

		pos.set_x(x);
		if (button->m_show_alpha > 0.0f) {
			button->SetPos(pos, new Timer(150.0f, Timer::FUNC_QUAD_OUT),
				       ui::Widget::ANIM_MOVE, 2);
		} else {
			button->SetPos(pos, NULL, ui::Widget::ANIM_MOVE, 0);

			Timer *timer = new Timer(200.0f, Timer::FUNC_QUAD_IN);

			if (!button->m_transition_complete) {
				button->m_transition = 0xff;
				button->m_transition_complete = true;
				button->DoTransition_core(0.0f, true);
			}
			ScePafWidget_FF5BC957(button, timer, 2, 0.0f, NULL);
		}
		x += button->GetSize(0)->extract_x() + 16.0f;
	}
}

/* a shell event: the loaded pages updated and the snaps set again (0x810B1D18) */
void applauncherHomeOnShellEvent(int code, const void *data, SceSize size, void *arg)
{
	g_applauncherHome->UpdateLoadedPages(true);
	g_applauncherHome->UpdateSnaps();
}

/*
 * The home updated each frame: the icons' tilt moved on and a drag left without the focus, the
 * pages loaded around a new view focus, the closed application page's focus given back to its
 * icon (given up after 275 ms), the edit state asked for set, the slot under the cursor kept,
 * the application page opened once the view rests on the page dropped on, the background shown
 * once its image is ready, the wave faded, the start finished once everything is loaded, and
 * the dragged icon moved on (0x810B1D32).
 */
void ApplauncherHome::Update()
{
	applauncherIconEffectUpdate();
	if (!ScePafWidget_529FB877(Framework::Instance()->GetEnvironmentInstance(), -1))
		applauncherIconCancelDrag();

	if (!(m_dragPos == m_scroller.m_scrollView->GetViewFocus())) {
		m_dragPos = m_scroller.m_scrollView->GetViewFocus();
		UpdateLoadedPages(false);
	}

	if (m_appIcon.get() != NULL) {
		m_appIconTime += Timer::s_tick_unit * Timer::s_tick_interval;
		if (g_applauncherIconMoveTime < m_appIconTime * 1000.0f) {
			m_appIcon.reset();
			m_appIconTime = 0.0f;
		} else if (m_appIcon->GetWidget() != NULL) {
			if (m_appIcon->GetWidget()->m_cursor_on) {
				m_appIcon.reset();
				m_appIconTime = 0.0f;
			} else {
				m_appIcon->GetWidget()->SetFocus(NULL);
			}
		}
	}

	bool focusShown = ScePafWidget_529FB877(Framework::Instance()->GetEnvironmentInstance(), -1);

	if (system::Model::GetType() == 1 || focusShown)
		SetEditState(m_editRequest);
	else
		SetEditState(0);

	math::v4 viewPos = m_scroller.m_scrollView->GetWorldMatrix().w();
	ApplauncherPage *page = static_cast<ApplauncherPage *>(m_scroller.GetItemBySnapID(m_snapId));

	if (page != NULL && page->GetCursorSlot() >= 0) {
		if (__builtin_fabsf(viewPos.extract_x()) < 20.0f) {
			ApplauncherPage *current =
				static_cast<ApplauncherPage *>(m_scroller.GetItemBySnapID(m_snapId));

			if (current != NULL)
				m_dropIndex = current->GetCursorSlot();
		} else if (((bool (*)(int))m_topmenu->func[TOPMENU_PLUGIN_FUNC_IS_LAYER_ACTIVE])
				   (m_layer)) {
			page->FocusSlot(m_dropIndex);
		}
	}

	if (m_dropPage >= 0) {
		ApplauncherPage *focused =
			static_cast<ApplauncherPage *>(g_applauncherHome->m_scroller.GetFocused());
		ui::ScrollView *view = m_scroller.m_scrollView;

		if (focused == static_cast<ApplauncherPage *>(m_scroller.GetItemBySnapID(m_dropPage))) {
			float rest = (view->GetWorldMatrix().w().extract_y() + 16.0f) * (1.0f / 512.0f);

			if (rest == sce_paf_floorf(rest)) {
				common::SharedPtr<ApplauncherIcon> folder;
				common::SharedPtr<ApplauncherIcon> icon =
					FindIconInFolder(m_appName, &folder, NULL);

				applauncherHomeOpenAppPage(folder, m_appName.c_str(), m_openAnimate);
				m_dropPage = -1;
			}
		}
	}

	if (m_backgroundState == 3 && m_task->GetJobStatus() == 2) {
		ApplauncherPage *current = static_cast<ApplauncherPage *>(
			g_applauncherHome->m_scroller.GetItemBySnapID(g_applauncherHome->m_snapId));

		if (m_imagePath == current->m_param.themeFile) {
			ui::Widget *view = g_applauncherHome->m_scrollView;

			bool retryLoad = false;
			if (m_task->GetImage().get() != NULL) {
				view->SetTexture(m_task->GetImage());
			} else if (m_homeFlags[2]) {
				m_homeFlags[2] = false;
				applauncherDataCacheLoad(m_imagePath, &m_task, NULL, NULL, true);
				retryLoad = true;
			} else {
				view->SetTexture(g_applauncherPlugin->GetTexture(
					IDParam(APPLAUNCHER_HOME_NO_IMAGE_TEXTURE)));
			}
			if (!retryLoad) {
				static_cast<graph::PlaneObj *>(view->GetDrawObj(0))->SetTexRegion(
					math::v4(m_texU, m_texV, m_texScale[0], m_texScale[1]));
				ShowBackground();
			}
		} else {
			ClearBackground();
			LoadBackground(current);
		}
	}

	m_fadeTime += Timer::s_tick_unit * Timer::s_tick_interval;
	switch (m_mode) {
	case 0: {
		float rate = m_fadeTime * 1000.0f / 750.0f;

		if (rate > 1.0f)
			rate = 1.0f;
		m_fade[0] = rate;
		break;
	}
	case 1: {
		float rate = m_fadeTime * 1000.0f / 750.0f;

		if (rate > 1.0f)
			rate = 1.0f;
		m_fade[0] = 1.0f - rate;
		break;
	}
	}
	m_fade[1] = Timer::DoFuncQuadOut(m_fade[0]);
	for (ShellSnapList::iterator it = m_scroller.m_items.begin(); it != m_scroller.m_items.end();
	     ++it) {
		ApplauncherPage *faded = static_cast<ApplauncherPage *>(*it);

		faded->UpdateFade();
	}

	if (!m_homeFlags[0] && shellBootArePluginsLoaded() &&
	    ((job::JobQueue *(*)(void))g_applauncherHome->m_topmenu->func[
		    TOPMENU_PLUGIN_FUNC_GET_JOB_QUEUE])()->NumItems() == 0 &&
	    g_applauncherIconCache->m_manager->GetQueuedJobCount() == 0 &&
	    applauncherPagesLoaded() && applauncherHomeGetLivearea() != NULL) {
		int index = 0;

		for (ShellSnapList::iterator it = m_scroller.m_items.begin();
		     it != m_scroller.m_items.end(); ++it, ++index) {
			ApplauncherPage *loaded = static_cast<ApplauncherPage *>(*it);

			if (index > 2)
				break;
			for (SceUInt32 slot = 0; slot < 10; slot++) {
				ApplauncherIcon *icon = loaded->m_slots[slot].get();

				if (icon != NULL && icon->m_icon != NULL && icon->m_image.get() == NULL)
					icon->Update(false);
			}
		}
		if (m_backgroundState == 1 && m_imagePath.length() != 0 && m_task.get() != NULL) {
			intrusive_ptr<graph::Surface> image = m_task->GetImage();

			if (image.get() != NULL) {
				g_applauncherHome->m_scrollView->SetTexture(image);
			} else {
				ClearBackground();
				applauncherHomeReloadBackground();
			}
		}
		shellBootScreenClose();
		((void (*)(void))m_topmenu->func[TOPMENU_PLUGIN_FUNC_HOME_STARTED])();
		shellEventServerSend(0, NULL, 0);
		applauncherPagesSetStarted();
		if (sceKernelCheckDipsw(185)) {
			ImposePluginInterface *impose = (ImposePluginInterface *)
				applauncherPluginGetInterface("impose_plugin", 1);

			if (impose != NULL) {
				ImposeNotice notice;
				wchar_t message[1024];
				SceUInt32 start = (SceUInt32)shellStartGetTime();
				SceUInt32 now = SceThreadmgr_47F6DE49();

				sce_paf_swprintf(message, sizeof(message),
						 L"ShellBootTime\n%.2f[sec] %.2f[sec]",
						 (double)((float)start / 1000000.0f),
						 (double)((float)(now - start) / 1000000.0f));
				notice.message = message;
				notice.callback = NULL;
				notice.name = "ShellBootTime";
				impose->showNotice(&notice);
			}
		}
		((void (*)(void))m_topmenu->func[TOPMENU_PLUGIN_FUNC_HOME_READY])();
		UpdateSnaps();
		m_homeFlags[0] = true;
	}

	ApplauncherIcon *item = g_applauncherDragItem.get();

	if (item != NULL && item->m_icon != NULL && item->m_icon->m_widget != NULL &&
	    item->m_icon->m_widget->m_icon_mode == 2) {
		if (g_applauncherDropTarget.get() != NULL) {
			item->MoveDrag(0);
		} else if (item->m_page == NULL) {
			ApplauncherPage *appPage = applauncherHomeGetAppPage();

			if (appPage != NULL && appPage->IsOpened())
				g_applauncherDragItem.get()->MoveDrag(0);
		}
	}
}

/* the home updated each frame, on the main thread (0x810B262A) */
void applauncherHomeOnMainThread(void *data)
{
	g_applauncherHome->Update();
}

/* an application's information changed: its icon's data read again from livespace_db, and the
   icon on the home given it when its parental lock level is another (0x810B263C) */
void applauncherHomeOnAppInfoEvent(int event, void *info, void *arg)
{
	if (g_applauncherHome == NULL || event != 3)
		return;

	LsdbValue &value = (*static_cast<const LsdbAppInfoPtr *>(info)->get())[LSDB_KEY_TITLE_ID];
	string titleId = value.type == LSDB_VALUE_TEXT && value.text != NULL ? *value.text :
		string("");
	LsdbIconPtr data(new LsdbIcon());

	SceLsdb_DE0DA538(g_shellLsdbClient->client, &titleId, &data, NULL, false);

	common::SharedPtr<ApplauncherIcon> icon = g_applauncherHome->FindIcon(titleId, NULL);

	if (icon.get() != NULL) {
		LsdbIconPtr current = icon->m_data;

		if (current.get() != NULL && current->parentalLockLv != data->parentalLockLv)
			applauncherHomeUpdateApp(titleId, data);
	}
}

/* the theme's UI refreshed, on the main thread (0x810B28B0) */
void applauncherHomeOnRefreshThemeUI(void *data)
{
	applauncherHomeApplyTheme();
}

/* PLACEHOLDER name: a job's end, the semaphore given released (0x810B28B8) */
void applauncherHomeOnJobDone(ApplauncherLsdbJob *job, void *data)
{
	static_cast<thread::Semaphore *>(data)->Release();
}

/* the icon's image ready or given up: the opening let go on (0x810B28C2) */
void applauncherHomeOnIconImage(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	s_appInstallSemaphore.Release();
}
