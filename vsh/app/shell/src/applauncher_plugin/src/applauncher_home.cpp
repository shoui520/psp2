/*
 * applauncher_home.cpp: the home screen (0x810A888C-0x810B297C): its pages in their scroller, the
 * page indicator, the wave behind them, and the functions the plugin's table gives topmenu. The
 * file's and every name here are ours (PLACEHOLDER): no record or string of the 3.6x shell names
 * them.
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/stdio.h>
#include <timer/timer.h>
#include <math/math.h>
#include <versatileparam/idparam.h>
#include <graphics/surface/surface.h>
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
#include <common_gui_dialog/common_gui_dialog.h>
#include "../../../shell.h"
#include "../../../plugins.h"
#include "../../../theme.h"
#include "applauncher.h"
#include "applauncher_home.h"

using namespace paf;

/* the wave the home shows behind its pages */
#define APPLAUNCHER_HOME_WAVE		11

/* the home's resources, by their IDs' hashes */
#define APPLAUNCHER_HOME_TEXTURE	0x56AEBD5F
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

/* held while an application is installed onto the home (one at a time) */
static thread::Semaphore s_appInstallSemaphore("SceShellAppInstall", 0, 1, 0);

/* the home's callbacks and the functions of the plugin's table, further on */
void applauncherHomeOnScrollTouch(int32_t type, ui::Handler *self, ui::Event *e, void *data);
void applauncherHomeOnSnapped(int32_t type, ui::Handler *self, ui::Event *e, void *data);
void applauncherHomeOnSceneEvent(int32_t type, ui::Handler *self, ui::Event *e, void *data);
void applauncherHomeOnMainThread(void *data);
void applauncherHomeOnOverlayEvent(int32_t type, ui::Handler *self, ui::Event *e, void *data);
void applauncherHomeOnOverlayTouch(int32_t type, ui::Handler *self, ui::Event *e, void *data);
void applauncherHomeOnEditBarEvent(int32_t type, ui::Handler *self, ui::Event *e, void *data);
void applauncherHomeOnLayerEnter(void *data);
void applauncherHomeOnLayerLeave(void *data);
void applauncherHomeOnShellEvent(int code, const void *data, SceSize size, void *arg);
void applauncherHomeOnAppInfoEvent(int event, void *info, void *arg);
void applauncherInterfaceFunc0(void);
void applauncherInterfaceFunc1(void);
void applauncherInterfaceFunc2(void);
void applauncherInterfaceFunc3(void);
void applauncherInterfaceFunc4(void);
void applauncherInterfaceFunc5(void);
void applauncherInterfaceFunc7(void);
void applauncherInterfaceFunc8(void);
void applauncherInterfaceFunc9(void);
void applauncherInterfaceFunc10(void);
void applauncherInterfaceFunc11(void);
void applauncherInterfaceFunc12(void);
void applauncherInterfaceFunc13(void);
void applauncherInterfaceFunc14(void);
void applauncherInterfaceFunc15(void);
void applauncherInterfaceFunc17(void);
void applauncherInterfaceFunc18(void);
void applauncherInterfaceFunc19(void);
void applauncherInterfaceFunc20(void);
void applauncherInterfaceFunc21(void);
void applauncherInterfaceFunc22(void);
void applauncherInterfaceFunc23(void);
void applauncherInterfaceFunc24(void);
void applauncherInterfaceFunc26(void);
void applauncherInterfaceFunc27(void);

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
		    g_applauncherState->m_settings->m_sortEnabled != 0) {
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
	  m_backgroundState(0), m_waveType(0), m_reserved2(0), m_reserved3(0), m_alpha(1.0f),
	  m_background(NULL), m_scrollView(NULL), m_foreground(NULL), m_editBar(NULL),
	  m_editRequest(0), m_editState(0), m_indicator(NULL), m_ready(false), m_mode(0),
	  m_reserved7(0), m_overlay(NULL), m_waitDialog(-1), m_dragging(false),
	  m_dragPos(-10000.0f, -10000.0f), m_waitingAppCount(0), m_noticePage(NULL), m_appCount(0),
	  m_noticeKind(0), m_viewHeld(false), m_waveUpdated(false), m_dropIndex(-1),
	  m_dropPage(-1), m_appPage(NULL), m_reserved13(0), m_lsdbListener(NULL)
{
	m_iconScale[0] = 1.0f;
	m_iconScale[1] = 1.0f;
	m_folderScale[0] = 1.0f;
	m_folderScale[1] = 1.0f;
	m_flags0[0] = false;
	m_flags0[1] = false;
	m_flags0[2] = false;
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

	g_applauncherInterface->func[0] = applauncherInterfaceFunc0;
	g_applauncherInterface->func[1] = applauncherInterfaceFunc1;
	g_applauncherInterface->func[2] = applauncherInterfaceFunc2;
	g_applauncherInterface->func[4] = applauncherInterfaceFunc4;
	g_applauncherInterface->func[5] = applauncherInterfaceFunc5;
	g_applauncherInterface->func[6] = (void (*)(void))applauncherHomeOnLayerLeave;
	g_applauncherInterface->func[7] = applauncherInterfaceFunc7;
	g_applauncherInterface->func[8] = applauncherInterfaceFunc8;
	g_applauncherInterface->func[9] = applauncherInterfaceFunc9;
	g_applauncherInterface->func[10] = applauncherInterfaceFunc10;
	g_applauncherInterface->func[11] = applauncherInterfaceFunc11;
	g_applauncherInterface->func[3] = applauncherInterfaceFunc3;
	g_applauncherInterface->func[12] = applauncherInterfaceFunc12;
	g_applauncherInterface->func[13] = applauncherInterfaceFunc13;
	g_applauncherInterface->func[14] = applauncherInterfaceFunc14;
	g_applauncherInterface->func[15] = applauncherInterfaceFunc15;
	g_applauncherInterface->func[16] = applauncherHomeShowWave;
	g_applauncherInterface->func[17] = applauncherInterfaceFunc17;
	g_applauncherInterface->func[18] = applauncherInterfaceFunc18;
	g_applauncherInterface->func[19] = applauncherInterfaceFunc19;
	g_applauncherInterface->func[21] = applauncherInterfaceFunc21;
	g_applauncherInterface->func[22] = applauncherInterfaceFunc22;
	g_applauncherInterface->func[23] = applauncherInterfaceFunc23;
	g_applauncherInterface->func[24] = applauncherInterfaceFunc24;
	g_applauncherInterface->func[25] = (void (*)(void))applauncherThemeRefresh;
	g_applauncherInterface->func[26] = applauncherInterfaceFunc26;
	g_applauncherInterface->func[20] = applauncherInterfaceFunc20;
	g_applauncherInterface->func[27] = applauncherInterfaceFunc27;

	m_ready = true;
	((void (*)(int, void (*)(void *), void *))
		m_topmenu->func[TOPMENU_PLUGIN_FUNC_SET_LAYER_ENTER])
		(m_layer, applauncherHomeOnLayerEnter, this);
	((void (*)(int, void (*)(void *), void *))
		m_topmenu->func[TOPMENU_PLUGIN_FUNC_SET_LAYER_LEAVE])
		(m_layer, applauncherHomeOnLayerLeave, this);
	if (((int (*)(void))m_topmenu->func[TOPMENU_PLUGIN_FUNC_IS_WAVE_SHOWN])())
		applauncherHomeShowWave();
	applauncherDragCreate();
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
	applauncherDragDestroy();
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
ApplauncherPage *applauncherHomeAddPage(const ApplauncherPageParam *param, ApplauncherPage *after,
					bool focus, bool save)
{
	ApplauncherPage *page;

	if (g_applauncherHome->m_scroller.m_items.size() >= 10)
		return NULL;
	if (param != NULL) {
		page = new ApplauncherPage(g_applauncherHome->m_scroller.m_plugin, param, 0);
	} else {
		ApplauncherPageParam theme;

		theme.m_waveType = shellStartupGetPageColor();
		if (shellThemeGetInfo()->get() != NULL) {
			common::SharedPtr<LsdbThemeInfo> *info = shellThemeGetInfo();
			int index = after != NULL ? after->m_index + 1 :
				g_applauncherHome->m_scroller.m_items.size();

			if (index < 10) {
				BackgroundParam *bg = &info->get()->m_homeProperty.m_bgParam[index];

				if (bg->m_imageFilePath.length() != 0) {
					shellThemeInfoGetFilePath(info->get(), &theme.m_imagePath,
								  bg->m_imageFilePath);
					theme.m_imageWidth = 960;
					theme.m_imageHeight = 512;
					theme.m_thumbnailWidth = 960;
					theme.m_thumbnailHeight = 512;
				}
				bg = &info->get()->m_homeProperty.m_bgParam[index];
				if (bg->m_waveType >= 0)
					theme.m_waveType = bg->m_waveType;
				if (info->get()->m_status >= 100)
					theme.m_fontColor = bg->GetFontColor();
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
	if (g_applauncherHome->m_ready && g_applauncherState->m_settings->m_sortEnabled != 0) {
		if (g_applauncherHome->m_scroller.GetNext(page) != NULL)
			g_applauncherHome->m_scroller.GetNext(page)->m_widget->SetMetaAlpha(
				0.0f, NULL, ui::Widget::ANIM_META_ALPHA);
		page->GetWidget()->SetMetaAlpha(0.0f, NULL, ui::Widget::ANIM_META_ALPHA);
		page->GetWidget()->SetMetaAlpha(1.0f, new Timer(500.0f, Timer::FUNC_QUAD_OUT),
						ui::Widget::ANIM_META_ALPHA);
		page->Save();
		for (ShellSnapList::iterator it = g_applauncherHome->m_scroller.m_items.begin();
		     it != g_applauncherHome->m_scroller.m_items.end(); ++it)
			static_cast<ApplauncherPage *>(*it)->Refresh();
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
void applauncherHomeCreateOverlay(void)
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

	g_applauncherHome->m_reserved7 = 0;
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
	applauncherHomeCreateOverlay();
	for (ShellSnapList::iterator it = g_applauncherHome->m_scroller.m_items.begin();
	     it != g_applauncherHome->m_scroller.m_items.end(); ++it)
		static_cast<ApplauncherPage *>(*it)->Save();
	if (g_applauncherHome->m_appPage != NULL)
		g_applauncherHome->m_appPage->Save();
	g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_EDIT_MODE_ENTERED]();
	if (g_applauncherHome->m_flags0[1]) {
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
		if (m_noticeKind)
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

	ApplauncherPageParam param;

	param.m_waveType = shellStartupGetPageColor();

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
		if (button == NULL)
			goto decide;

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
	if (button != NULL)
		button->SetParent(button->m_parent, NULL, 6);

decide:
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
		if (button == NULL)
			goto cancel;

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
	if (button != NULL)
		button->SetParent(button->m_parent, NULL, 6);

cancel:
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
		if (button == NULL)
			goto mode;

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
	if (button != NULL)
		button->SetParent(button->m_parent, NULL, 6);

mode:
	button = buttons->FindChild(s_editModeMode, 0);
	if (button != NULL) {
		if (m_editState != 1 && m_editState != 2)
			HideEditButton(button);
	} else if (m_editState == 1 || m_editState == 2) {
		m_plugin->TemplateOpen(buttons, IDParam(APPLAUNCHER_EDIT_TEMPLATE),
				       Plugin::TemplateOpenParam());
		button = buttons->FindChild(IDParam(APPLAUNCHER_EDIT_BUTTON), 0);
		if (button == NULL)
			goto kakutei;

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
	if (button != NULL)
		button->SetParent(button->m_parent, NULL, 6);

kakutei:
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
			ScePafWidget_931AD41E(*g_applauncherDragItem->m_icon, true);
		g_applauncherDragItem->Drop();
	}
	ScePafWidget_21125870(g_applauncherHome->m_scroller.m_scrollView, 64);

	ApplauncherHome *home = g_applauncherHome;
	ui::Widget *view = home->m_scrollView;

	home->m_alpha = 1.0f;
	home->m_mode = 0;
	home->m_reserved7 = 0;
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
void applauncherHomeGetPageParam(ApplauncherPageParam *param)
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
	if (m_imagePath.length() == 0 && page->m_param.m_imagePath.length() == 0) {
		if (m_waveType != page->m_param.m_waveType)
			BeginBackgroundChange(true);
	} else if (m_imagePath.length() != 0 && page->m_param.m_imagePath.length() != 0) {
		if (m_imagePath != page->m_param.m_imagePath || m_dragging)
			BeginBackgroundChange(false);
		else if (m_waveType != page->m_param.m_waveType)
			BeginBackgroundChange(true);
	} else {
		BeginBackgroundChange(false);
	}
}

/* the parameters of the page in view set and saved, its background shown (0x810AD192) */
void applauncherHomeSetPageParam(const ApplauncherPageParam *param)
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
void applauncherHomeUpdateSnaps(void)
{
	if (shellStartupGetIduMode() == 1) {
		for (ShellSnapList::iterator it = g_applauncherHome->m_scroller.m_items.begin();
		     it != g_applauncherHome->m_scroller.m_items.end(); ++it) {
			ui::ScrollView *view = g_applauncherHome->m_scroller.m_scrollView;
			ApplauncherPage *page = static_cast<ApplauncherPage *>(*it);

			for (SceUInt32 i = 0; i < 10; i++) {
				if (page->m_slots[i].get() != NULL && !page->m_slots[i]->m_hidden) {
					view->AddSnapChild(page->GetWidget());
					goto next;
				}
			}
			view->DelSnapChild(page->GetWidget());
next:
			;
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
	ApplauncherIcon *icon = page->m_focus.get();

	if (icon != NULL && icon->m_page != NULL && icon->m_page->m_type == 0)
		icon->m_page->RestoreIcons(page->m_focus);

	ui::Widget *view = g_applauncherHome->m_scroller.m_scrollView;

	if (!view->m_transition_complete) {
		view->m_transition = 0xff;
		view->m_transition_complete = true;
		view->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(view, NULL, 2, 0.0f, NULL);
	applauncherIconPoolCloseAppPage(g_applauncherHome->m_appPage);
	if (keepFocus) {
		g_applauncherHome->m_appIcon = g_applauncherHome->m_appPage->m_focus;
		g_applauncherHome->m_reserved13 = 0;
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
		if (folder.get() != NULL && folder->m_data != NULL &&
		    folder->m_data->m_state != NULL && folder->m_data->m_state->m_state == 1) {
			whole = true;
		} else if (icon.get() != NULL) {
			ApplauncherPage *appPage = icon->m_page;

			if (appPage != NULL && appPage->m_type == 2) {
				folder = appPage->m_focus;
				if (appPage->IsAppRemoved() == 1)
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

	if (folder.get() != NULL && folder->m_page != NULL && folder->m_data != NULL &&
	    folder->m_data->m_state != NULL && !whole) {
		vector<ApplauncherRemovedIcon> removed;
		ApplauncherRemovedIcon item;

		item.m_id = folder->m_data->m_state->m_id;
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
				    g_applauncherHome->m_appPage->m_focus.get() == folder.get())
					applauncherHomeCloseAppPage(false);
				applauncherIconRemove(folder, true);
				applauncherIconDelete(folder, true, false);
			} else {
				folder->Update(true);
			}
		}
		if (g_applauncherState->m_settings->m_sortEnabled == 0) {
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
	const vector<common::SharedPtr<ApplauncherAppDataList> > &pages)
{
	int i = 0;

	for (ShellSnapList::iterator it = m_scroller.m_items.begin();
	     it != m_scroller.m_items.end(); ++it, i++)
		static_cast<ApplauncherPage *>(*it)->UpdateIcons(pages[i].get());
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
