/*
 * impose_psmenu.cpp: impose_plugin's PS button menu over an application: the application's
 * own menu and its quit dialog, the brightness, the microphone and the voice chat settings,
 * the parental play time left, and the PS TV's touch emulation (0x8114AA16-0x8114F348).
 * The file's and the names are ours (PLACEHOLDER), after Sony's job names
 * ("ImposePsMenuCheckAppMenuReady", "ImposePsMenuOpenAppMenu").
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/stdio.h>
#include <std/math.h>
#include <versatileparam/idparam.h>
#include <timer/timer.h>
#include <job/job.h>
#include <system/system.h>
#include <common/char_encoding.h>
#include <common/transition.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/core/event.h>
#include <widget/w_box.h>
#include <widget/w_text.h>
#include <widget/w_plane.h>
#include <widget/w_plane_button.h>
#include <widget/w_button.h>
#include <widget/w_check_box.h>
#include <widget/w_slidebar.h>
#include <widget/w_scroll_view_base.h>
#include <graphics/drawobj/text_obj.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include <regmgr/regmgr.h>
#include <audioin.h>
#include <av_config/av_config.h>
#include <system_param.h>
#include <inputdevice/pad/pad.h>
#include "../../livespace_db/livespace_db_internal.h"
#include "../../applauncher_plugin/src/applauncher.h"
#include "../../applauncher_plugin/src/applauncher_home.h"
#include "../../../plugins.h"
#include "../../../popup_list.h"
#include "../../../shell.h"
#include "impose.h"

using namespace paf;

/* the menu's template and widgets (PLACEHOLDER names) */
#define PSMENU_TEMPLATE			0xEADFF4F2
#define PSMENU_ROOT			0xE83F6AF0
#define PSMENU_BG			0xC5B83462
#define PSMENU_CLOSE			0xC0099932
#define PSMENU_BOX			0x0EE0C8AF
/* the parental play time */
#define PSMENU_TIME_TEMPLATE		0x1FF37238
#define PSMENU_TIME_PANEL		0x963E9FFF
#define PSMENU_TIME_ICON		0xF6B73CEC
#define PSMENU_TIME_LABEL		0x8C25EE4D
#define PSMENU_TIME_TEXT		0xFBCC23BE
#define PSMENU_TIME_SETTINGS		0xF5ADF4D1
/* the PS TV's buttons */
#define PSMENU_MODEL_TEMPLATE		0xFAE265F7
#define PSMENU_MODEL_POWER		0xC6D3C5FB
#define PSMENU_MODEL_OTHER		0xCCD55012
/* the application's menu */
#define PSMENU_APP_TEMPLATE		0x19AC2616
#define PSMENU_APP_MENU			0x37406542
#define PSMENU_APP_TITLE		0x4071E0FE
#define PSMENU_APP_ICON			0x15DA0994
/* the touch emulation */
#define PSMENU_TOUCH_TEMPLATE		0x00E18A8E
#define PSMENU_TOUCH_CHECK		0xC718FB9D
#define PSMENU_TOUCH_HELP		0x6B3292FC
#define PSMENU_TOUCH_ROW		0xA32D5A3B
#define PSMENU_TOUCH_TEXT		0xD5A09E70
#define PSMENU_SCROLL			0xC5634630
/* the brightness */
#define PSMENU_BRIGHTNESS_TEMPLATE	0xABC42164
#define PSMENU_BRIGHTNESS_BAR		0xC3CCAD62
/* the microphone and the voice chat */
#define PSMENU_MIC_TEMPLATE		0x1B8F1FE1
#define PSMENU_MIC_CHECK		0x44E41DB0
#define PSMENU_MIC_ICON_ON		0xDA9D77F5
#define PSMENU_MIC_ICON_OFF		0x92C1BCF2
#define PSMENU_VOICE_TEMPLATE		0xD2F5BB83
#define PSMENU_VOICE_ICON_ON		0x8FF40A16
#define PSMENU_VOICE_TEXT		0x835779C7
#define PSMENU_VOICE_INFO		0xA34BB8D6
#define PSMENU_VOICE_CHECK		0x087B290B
#define PSMENU_VOICE_ROW		0xE6E0EC7B
/* the quit dialog's message */
#define PSMENU_MSG_QUIT			0x4FD0C3A8
/* the touch emulation's help dialog */
#define PSMENU_HELP_HEADING		0x2899F7BE
#define PSMENU_HELP_TEXT		0x269D7709
#define PSMENU_HELP_TEMPLATE		0x4AD94230
#define PSMENU_HELP_PLANE1		0x7222EB43
#define PSMENU_HELP_TEX1		0xAC87E115
#define PSMENU_HELP_PLANE2		0x331614C2
#define PSMENU_HELP_TEX2		0xD6B8A462
#define PSMENU_HELP_TEXT1		0x701F3F7B
#define PSMENU_HELP_TEXT2		0x5BDE5F73
#define PSMENU_HELP_TEXT3		0x3692E92F
#define PSMENU_HELP_CLOSE		0x27D4530E

/* the dialogs' parts (SceCommonGuiDialog_8947FC23's indexes; PLACEHOLDER names) */
#define PSMENU_DIALOG_CONTENTS		9
#define PSMENU_DIALOG_PANEL		10

/* the events no header names (PLACEHOLDER names): the text laid out, the slide bar's keys */
#define PSMENU_EVENT_TEXT_LAYOUT	0x20100004
#define PSMENU_EVENT_KEY_UP		0x10001
#define PSMENU_EVENT_KEY_DOWN		0x10003

/* the menu's interval timer */
#define PSMENU_INTERVAL			100

/* the application asked of the application manager (PLACEHOLDER name) */
#define PSMENU_APP_ID_FRONT		(-6)

/* the brightness the slide bar's 0 stands for, and its steps */
#define PSMENU_BRIGHTNESS_MIN		21
#define PSMENU_BRIGHTNESS_STEPS		65515
#define PSMENU_BRIGHTNESS_KEY_STEP	4094

/* PLACEHOLDER names: the parental play time (0x81232FA6, 0x812327F8: the seconds left, -1
   without a limit) and a function the PS TV's button calls (0x8124A6E2), in the shell's code */
int Shell_81232FA6(void);
int Shell_812327F8(void);
int Shell_8124A6E2(int arg1, int arg2);

/* PLACEHOLDER names: the menu's event flag (0x81159F9A made, 0x81159FBC deleted, 0x81159FF6
   cleared, 0x8115A01A set) and the application events it listens to (0x8115A9DE, 0x8115A9FE) */
void Impose_81159F9A(void);
void Impose_81159FBC(void);
void Impose_81159FF6(void);
void Impose_8115A01A(void);
void Impose_8115A9DE(void);
void Impose_8115A9FE(void);

/* (0x81564DC0) */
ImposePsMenu *g_imposePsMenu;

/* PLACEHOLDER names: two colours the file's initializer sets and nothing reads (0x81564DC8,
   0x81564DF8) */
static math::v4 s_fullColor(1.0f, 1.0f, 1.0f, 1.0f);
static math::v4 s_dimColor(0.5f, 0.5f, 0.5f, 1.0f);

/* the application menu's checks left before it is given up (0x815496AC) */
static int s_checkRetries = 60;

/* the child, when it is of the type */
#define PSMENU_FIND(parent, type, id)							\
	((parent)->FindChild(IDParam(id), 0) != NULL &&					\
	 (parent)->FindChild(IDParam(id), 0)->IsInherit(type::TypeName()) ?		\
	 (type *)(parent)->FindChild(IDParam(id), 0) : NULL)

/* the widget's text object */
#define PSMENU_TEXT_OBJ(widget)								\
	((widget) != NULL && (widget)->GetDrawObj(0) != NULL &&				\
	 (widget)->GetDrawObj(0)->IsInherit(graph::TextObj::DrawObjType()) ?		\
	 (graph::TextObj *)(widget)->GetDrawObj(0) : NULL)

/* PLACEHOLDER names: listeners calling a method of the menu, with an argument or without */
template<class T>
class ImposePsMenuListener : public ui::EventListener {
public:
	ImposePsMenuListener(T *obj, void (T::*func)()) : m_obj(obj), m_func(func)
	{
	}

	virtual int32_t Do(int32_t type, ui::Handler *self, ui::Event *e)
	{
		if (m_killed)
			return 0x80AF4101;
		(m_obj->*m_func)();
		return 0;
	}

	T *m_obj;
	void (T::*m_func)();
};

template<class T, class A>
class ImposePsMenuArgListener : public ui::EventListener {
public:
	ImposePsMenuArgListener(T *obj, void (T::*func)(A), A arg)
		: m_obj(obj), m_func(func), m_arg(arg)
	{
	}

	virtual int32_t Do(int32_t type, ui::Handler *self, ui::Event *e)
	{
		if (m_killed)
			return 0x80AF4101;
		(m_obj->*m_func)(m_arg);
		return 0;
	}

	T *m_obj;
	void (T::*m_func)(A);
	A m_arg;
};

/* the quit dialog's buttons: yes closes the menu (PLACEHOLDER name) */
class ImposePsMenuQuitHandler : public CommonGuiDialogHandler {
public:
	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		if (button == 4) {
			Impose_8115A01A();
			g_imposePsMenu->Close();
		}
		SceCommonGuiDialog_BC311DC0(value);
	}
};

/* the application menu's check: tried again while it is busy, the button then enabled or not
   (PLACEHOLDER name) */
class ImposePsMenuCheckJob : public ImposePsMenuCheckAppMenuReady {
public:
	virtual void Finish(int result)
	{
		if (result == 3 && s_checkRetries >= 0 && g_imposePsMenu->m_page != NULL) {
			s_checkRetries--;
			g_imposePsMenu->CheckAppMenuReady();
		} else {
			s_checkRetries = 60;
			if (result == 1)
				g_imposePsMenu->SetAppMenuReady(false);
			else if (result == 2)
				g_imposePsMenu->SetAppMenuReady(true);
		}
	}
};

/* the application's menu opened: the quit dialog, or the menu closed (PLACEHOLDER name) */
class ImposePsMenuOpenJob : public ImposePsMenuOpenAppMenu {
public:
	virtual void Finish(int result)
	{
		if (result == 2) {
			g_imposePsMenu->ShowQuitDialog();
		} else if (result == 1) {
			Impose_8115A01A();
			g_imposePsMenu->Close();
		} else if (result == 3) {
			g_imposePsMenu->Close();
		}
	}
};

/* shows a widget, over the timer's time (inline in paf's callers) */
static inline void imposePsMenuShowWidget(ui::Widget *widget, Timer *t)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xff;
		widget->m_transition_complete = true;
		widget->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(widget, t, 2, 0.0f, NULL);
}

/* hides a widget at once (inline in paf's callers) */
static inline void imposePsMenuHideWidget(ui::Widget *widget)
{
	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(widget, NULL, 2, 0.0f, NULL);
}

static void imposePsMenuOnPower(void *data);
static void imposePsMenuOnIcon(const intrusive_ptr<cache::CacheImageInterface> &icon,
			       void *data);

/* the brightness saved, the dialogs and the parts closed, and the page let go (0x8114AA16) */
void ImposePsMenu::Close()
{
	if (m_page == NULL)
		return;
	if (m_brightness != NULL && m_brightness->m_value != m_brightnessValue) {
		m_brightnessValue = m_brightness->m_value;
		sceRegMgrSetKeyInt("/CONFIG/DISPLAY", "brightness",
				   m_brightnessValue + PSMENU_BRIGHTNESS_MIN);
	}
	shellPopupListClose(0);
	if (m_prioritizeDialog >= 0) {
		SceCommonGuiDialog_BC311DC0(m_prioritizeDialog);
		m_prioritizeDialog = -1;
	}
	if (m_helpDialog >= 0) {
		ui::Widget *contents = SceCommonGuiDialog_8947FC23(m_helpDialog,
								   PSMENU_DIALOG_CONTENTS);

		if (contents != NULL) {
			ui::Widget *close = contents->FindChild(IDParam(PSMENU_HELP_CLOSE), 0);

			if (close != NULL)
				close->DestroyWidget();
			imposePsMenuHideWidget(contents);
		}
		SceCommonGuiDialog_BC311DC0(m_helpDialog);
		m_helpDialog = -1;
	}
	if (m_quitDialog >= 0) {
		SceCommonGuiDialog_BC311DC0(m_quitDialog);
		m_quitDialog = -1;
	}

	ShellPluginInterface *topmenu = (ShellPluginInterface *)
		applauncherPluginGetInterface("topmenu_plugin", 1);

	if (topmenu != NULL)
		((void (*)(ImposePsMenu *, int))topmenu->func[TOPMENU_PLUGIN_FUNC_MODE_LEAVE])(this,
											      230);
	if (m_job.get() != NULL)
		m_job->Cancel();
	m_brightness = NULL;
	m_part81158C14.Impose_811598AA();
	m_part81153A18.Impose_8115422E();
	m_part811558C0.Impose_8115756C();
	m_part81152FB8.Impose_811535D2();
	m_part811527E0.Impose_81152E5C();
	m_micMute = NULL;
	m_voicePriority = NULL;
	m_focusButton = NULL;
	inputdevice::ReleaseOwnership(0x6F);
	g_imposeTouchEmulation->Unlock();
	Impose_81159FF6();
	ScePafCommon_B48C9BDC(0.0f, m_page, common::transition::Type_SlideFromBottom4, true,
			      false);
	m_page->m_name.SetID(string(""));
	m_page = NULL;

	Plugin *power = Plugin::Find("power_manage_plugin");

	if (power != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)power->GetInterface(1);

		if (iface != NULL)
			((int (*)(void (*)(void *), void *))
				iface->func[POWER_MANAGE_PLUGIN_FUNC_REMOVE_CALLBACK])(
				imposePsMenuOnPower, this);
	}
}

/* the dialog asking to quit the application (0x8114AC6E) */
void ImposePsMenu::ShowQuitDialog()
{
	m_quitDialog = -1;

	CommonGuiLayoutDialogParam param;

	param.resourcePlugin = m_plugin;
	param.layout = SceCommonGuiDialog_6CD802FC;
	param.priority = 7;
	param.text = m_plugin->GetString(IDParam(PSMENU_MSG_QUIT));
	param.handler = new ImposePsMenuQuitHandler;
	m_quitDialog = SceCommonGuiDialog_CCFCD9A6(&param);
}

/* the application menu's button enabled once the menu is ready (0x8114AE00) */
void ImposePsMenu::SetAppMenuReady(bool ready)
{
	if (m_page == NULL)
		return;

	ui::ButtonBase *button = PSMENU_FIND(m_page, ui::Button, PSMENU_APP_MENU);

	if (button != NULL) {
		button->Enable(false);
		m_checking = ready;
	}
}

/* the application menu's check put on the default queue (0x8114AFA0) */
int ImposePsMenu::CheckAppMenuReady()
{
	m_job = common::SharedPtr<job::JobItem>(new ImposePsMenuCheckJob);
	job::JobQueue::default_queue->Enqueue(m_job);
	m_checking = true;
	return 1;
}

/* (0x8114B0DC) */
ImposePsMenu::ImposePsMenu(Plugin *plugin)
	: m_plugin(plugin), m_page(NULL), m_brightness(NULL), m_micMute(NULL),
	  m_voicePriority(NULL), m_prioritizeDialog(-1), m_helpDialog(-1), m_quitDialog(-1),
	  m_checking(false), m_focusButton(NULL), m_part81153A18(plugin), m_part811558C0(plugin),
	  m_part81152FB8(plugin), m_part811527E0(plugin)
{
	int value;

	g_imposePsMenu = this;
	if (sceRegMgrGetKeyInt("/CONFIG/SHELL", "voice_priority", &value) == 0)
		sceAppMgrEnablePrioritizingSystemChat(value);
	if (sceRegMgrGetKeyInt("/CONFIG/SHELL", "mic_mute", &value) == 0) {
		if (value != 0)
			sceAudioInSetMute(1);
		else
			sceAudioInSetMute(2);
	}
	Impose_8115A9DE();
	Impose_81159F9A();
}

/* (0x8114B1B2) */
ImposePsMenu::~ImposePsMenu()
{
	Impose_81159FBC();
	Impose_8115A9FE();
	g_imposePsMenu = NULL;
}

/* the accessibility shortcut is set (0x8114B246) */
static bool imposePsMenuHasShortcut(void)
{
	int value = 0;

	return sceRegMgrGetKeyInt("/CONFIG/ACCESSIBILITY", "shortcut", &value) == 0 && value != 0;
}

/* the microphone's and the voice chat's buttons enabled while a headset is connected
   (0x8114B290) */
void ImposePsMenu::UpdateAudioButtons()
{
	SceUInt32 devices;

	sceAVConfigGetConnectedAudioDevice(&devices);
	if ((devices & 0xFF00) != 0) {
		if (m_micMute != NULL && !m_micMute->IsButtonEnabled()) {
			m_micMute->Enable(false);
			m_micMute->m_parent->FindChild(IDParam(PSMENU_MIC_ICON_ON), 2)
				->SetMetaAlpha(1.0f);
			m_micMute->m_parent->FindChild(IDParam(PSMENU_MIC_ICON_OFF), 2)
				->SetMetaAlpha(1.0f);
		}
	} else if (m_micMute != NULL && m_micMute->IsButtonEnabled()) {
		m_micMute->Disable(false);
		m_micMute->m_parent->FindChild(IDParam(PSMENU_MIC_ICON_ON), 2)->SetMetaAlpha(0.3f);
		m_micMute->m_parent->FindChild(IDParam(PSMENU_MIC_ICON_OFF), 2)->SetMetaAlpha(0.3f);
	}

	sceAVConfigGetConnectedAudioDevice(&devices);
	if ((devices & 0xFF00) != 0) {
		if (m_micMute != NULL && m_voicePriority != NULL &&
		    !m_voicePriority->IsButtonEnabled()) {
			m_voicePriority->Enable(false);
			m_voicePriority->m_parent->FindChild(IDParam(PSMENU_VOICE_ICON_ON), 2)
				->SetMetaAlpha(1.0f);
			m_voicePriority->m_parent->FindChild(IDParam(PSMENU_VOICE_TEXT), 2)
				->SetMetaAlpha(1.0f);
			((ui::ButtonBase *)m_voicePriority->m_parent->FindChild(
				IDParam(PSMENU_VOICE_INFO), 2))->Enable(false);
		}
	} else if (m_micMute != NULL && m_voicePriority->IsButtonEnabled()) {
		m_voicePriority->Disable(false);
		m_voicePriority->m_parent->FindChild(IDParam(PSMENU_VOICE_ICON_ON), 2)
			->SetMetaAlpha(0.3f);
		m_voicePriority->m_parent->FindChild(IDParam(PSMENU_VOICE_TEXT), 2)
			->SetMetaAlpha(0.3f);
		((ui::ButtonBase *)m_voicePriority->m_parent->FindChild(IDParam(PSMENU_VOICE_INFO),
								       2))->Disable(false);
	}
}

/* a text child laid out on lines when the system asks for it, to the width unless 0
   (0x8114B894) */
void imposeLayoutText(ui::Widget *parent, const IDParam &id, float width)
{
	if ((ScePafMisc_C51EF81D() & 1) == 0)
		return;

	ui::Widget *text = parent->FindChild(id, 0) != NULL &&
		parent->FindChild(id, 0)->IsInherit(ui::Text::TypeName()) ?
		parent->FindChild(id, 0) : NULL;
	graph::TextObj *obj = PSMENU_TEXT_OBJ(text);

	if (obj == NULL)
		return;
	obj->SetLayoutAttribute(graph::TextLayoutAttribute_WordWrap, true);
	obj->SetLayoutAttribute(graph::TextLayoutAttribute_Ellipsis, false);
	obj->SetLayoutAttribute(graph::TextLayoutAttribute_CondenseX, false);
	obj->SetLayoutAttribute(graph::TextLayoutAttribute_MaxLineCount, 2);
	obj->SetLayoutAttribute(graph::TextLayoutAttribute_LinePitch, 36.0f);
	if (width != 0.0f) {
		math::v4 size = *text->GetSize(0);

		size.set_x(width);
		text->SetSize(size);
	}
}

/* the button the focus goes to first, the first one given (0x8114B9E0) */
void ImposePsMenu::SetFocusButton(ui::Widget *button)
{
	if (m_focusButton == NULL && button != NULL) {
		m_focusButton = button;
		button->SetFocus(NULL);
	}
}

/* the application in front, when it is a game or an application of the system's
   (0x8114B9FC) */
bool ImposePsMenu::GetApplication(LsdbAppInfoPtr *app)
{
	SceAppMgrAppStatus status;

	if (sceAppMgrGetStatusByAppId(PSMENU_APP_ID_FRONT, &status) != 0)
		return false;

	LsdbAppInfoPtr info;
	ShellPluginInterface *topmenu = (ShellPluginInterface *)
		applauncherPluginGetInterface("topmenu_plugin", 1);

	if (topmenu != NULL)
		((void (*)(LsdbAppInfoPtr *, const char *))
		 topmenu->func[TOPMENU_PLUGIN_FUNC_GET_APP_INFO])(&info, status.appName);
	if (info.get() == NULL)
		return false;

	int type = sceLsdbGetType(&info);

	if (type != 2 && type != 3)
		return false;
	*app = info;
	return true;
}

/* (0x8114BB84) */
bool imposeIsDolce(void)
{
	return system::Model::GetType() == system::Model::Type_Dolce;
}

/* the parental control settings' icon, once applauncher has it (0x8114BB96) */
void ImposePsMenu::UpdateParentalIcon()
{
	ShellPluginInterface *applauncher = (ShellPluginInterface *)
		applauncherPluginGetInterface("applauncher_plugin", 1);

	if (applauncher == NULL)
		return;

	intrusive_ptr<cache::CacheImageInterface> icon;
	ui::Widget *settings = m_page->FindChild(IDParam(PSMENU_TIME_SETTINGS), 0);

	if (settings != NULL) {
		bool found = ((bool (*)(intrusive_ptr<cache::CacheImageInterface> *, const string &,
					void *, void *))applauncher->func[3])(
				&icon, string("NPXS10094"), (void *)imposePsMenuOnIcon, settings) &&
			icon.get() != NULL;

		if (found) {
			settings->SetDrawObjMaskBit(&icon, 0);
			imposePsMenuShowWidget(settings, NULL);
		} else {
			imposePsMenuHideWidget(settings);
		}
	}
}

/* the play time left shown as hours and minutes, the panel grown to it (0x8114BD42) */
void ImposePsMenu::UpdatePlayTime()
{
	ui::Widget *text = PSMENU_FIND(m_page, ui::Text, PSMENU_TIME_TEXT);

	if (text == NULL || Shell_81232FA6() == 0)
		return;

	int seconds = Shell_812327F8();
	char buf[32] = { 0 };
	int minutes = (int)sce_paf_ceilf((float)seconds / 60.0f);
	int hours = minutes / 60;

	sce_paf_snprintf(buf, sizeof(buf), "%02d:%02d", hours, minutes % 60);
	{
		string utf8(buf);
		wstring str;

		common::Utf8ToUtf16(utf8, &str, 0);
		text->SetString(str);
	}

	ui::Widget *panel = m_page->FindChild(IDParam(PSMENU_TIME_PANEL), 0);
	ui::Widget *icon = m_page->FindChild(IDParam(PSMENU_TIME_ICON), 0);

	if (panel == NULL || icon == NULL)
		return;
	if (panel->GetSize(0)->extract_y() == 70.0f || hours == -1)
		return;

	math::v4 size = *panel->GetSize(0);

	size.set_y(70.0f);
	panel->SetSize(size, new Timer(200.0f));
	imposePsMenuShowWidget(panel, new Timer(200.0f));
	size = *icon->GetSize(0);
	size.set_y(20.0f);
	icon->SetSize(size, new Timer(200.0f));
	imposePsMenuShowWidget(icon, new Timer(200.0f));

	{
		string utf8("");
		wstring str;

		common::Utf8ToUtf16(utf8, &str, 0);
		text->SetString(str);
	}
	{
		string utf8(buf);
		wstring str;

		common::Utf8ToUtf16(utf8, &str, 0);
		text->SetString(str);
	}
	UpdateParentalIcon();
}

/* there is no limit, or time is left of it (0x8114C440) */
static __attribute__((noinline)) bool imposePsMenuHasPlayTime(void)
{
	return Shell_81232FA6() == 0 || Shell_812327F8() != -1;
}

static void imposePsMenuOnClose(int32_t type, ui::Handler *self, ui::Event *e, void *data);
static void imposePsMenuOnInterval(int32_t type, ui::Handler *self, ui::Event *e, void *data);
static void imposePsMenuOnBrightness(int32_t type, ui::Handler *self, ui::Event *e, void *data);
static void imposePsMenuOnBrightnessDrag(int32_t type, ui::Handler *self, ui::Event *e,
					 void *data);
static void imposePsMenuOnBrightnessSave(int32_t type, ui::Handler *self, ui::Event *e,
					 void *data);
static void imposePsMenuOnBrightnessKey(int32_t type, ui::Handler *self, ui::Event *e,
					void *data);
static void imposePsMenuOnVoicePriority(int32_t type, ui::Handler *self, ui::Event *e,
					void *data);
static void imposePsMenuOnMicMute(int32_t type, ui::Handler *self, ui::Event *e, void *data);

/* the PS TV's power button: power_manage_plugin's menu (inline, 0x8141A8B4) */
inline void imposePsMenuOnPowerButton(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	ShellPluginInterface *power = (ShellPluginInterface *)
		applauncherPluginGetInterface("power_manage_plugin", 1);

	if (power != NULL)
		((void (*)(void))power->func[POWER_MANAGE_PLUGIN_FUNC_MENU])();
}

/* the PS TV's other button (inline, 0x8141A8D0) */
inline void imposePsMenuOnOtherButton(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	Shell_8124A6E2(0, 0);
}

/* the touch emulation's check box (inline, 0x8141A8DC) */
inline void imposePsMenuOnTouchEmulation(int32_t type, ui::Handler *self, ui::Event *e,
					 void *data)
{
	g_imposePsMenu->OnTouchEmulation(type, self, e, data);
}

/* the application's menu button (inline, 0x8141A900) */
inline void imposePsMenuOnAppMenu(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	g_imposePsMenu->OnAppMenu(type, self, e, data);
}

/* the menu opened over the application in power_manage_plugin's layer (0x8114C460) */
void ImposePsMenu::Open()
{
	if (m_page != NULL)
		return;

	ShellPluginInterface *power = (ShellPluginInterface *)
		applauncherPluginGetInterface("power_manage_plugin", 1);
	ShellPluginInterface *topmenu = (ShellPluginInterface *)
		applauncherPluginGetInterface("topmenu_plugin", 1);

	if (power == NULL || topmenu == NULL)
		return;
	((int (*)(void (*)(void *), void *))power->func[POWER_MANAGE_PLUGIN_FUNC_ADD_CALLBACK])(
		imposePsMenuOnPower, this);
	if (!ScePafMisc_B7CFFF5C())
		shellExclusiveFuncNetEnd(0);

	ui::Widget *parent = ((ui::Widget *(*)(void))
			      power->func[POWER_MANAGE_PLUGIN_FUNC_GET_LAYER])();

	if (parent == NULL)
		return;
	((void (*)(void))power->func[POWER_MANAGE_PLUGIN_FUNC_MENU_OPENED])();
	inputdevice::GetOwnership(0x6F);
	g_imposeTouchEmulation->Lock();
	((void (*)(ImposePsMenu *, int, int, int, int))
	 topmenu->func[TOPMENU_PLUGIN_FUNC_MODE_ENTER])(this, m_plugin->m_draw_priority, 1, 0, 0);
	m_plugin->TemplateOpen(parent, IDParam(PSMENU_TEMPLATE), Plugin::TemplateOpenParam());
	m_page = parent->FindChild(IDParam(PSMENU_ROOT), 0);

	ScePafWidget_42C5A4D4(m_page, 0, 0);
	m_page->EnableEvent(0xFBF0);
	m_page->m_hit_detect_mode = 0;
	m_page->m_hit_detect_b_mode = 0;
	m_page->SetHitBox(3, math::v4(960.0f, 544.0f, 0.0f, 0.0f),
			  math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	ScePafWidget_0253DC9C(m_page, true);
	ScePafWidget_A1193AAB(m_page, true, false);
	m_page->SetIntervalEvent(ui::Handler::CB_INTERVAL | PSMENU_INTERVAL, 60.0f,
				 imposePsMenuOnInterval, this);

	ui::Widget *bg = m_page->FindChild(IDParam(PSMENU_BG), 2);

	if (bg != NULL) {
		bg->SetColor(math::v4(0.5f, 0.5f, 0.5f, 1.0f));
		bg->SetRot(math::v4(3.14159265f, 0.0f, 0.0f, 0.0f), 0, NULL,
			   ui::Widget::ANIM_ROT, 0, 0.0f, NULL, NULL);
	}

	ui::Widget *close = m_page->FindChild(IDParam(PSMENU_CLOSE), 0);

	if (close != NULL)
		close->AddEventCallback(ui::ButtonBase::CB_BTN_DECIDE, imposePsMenuOnClose, this);

	ui::Box *box = PSMENU_FIND(m_page, ui::Box, PSMENU_BOX);

	if (box == NULL)
		return;

	if (imposePsMenuHasPlayTime()) {
		m_plugin->TemplateOpen(box, IDParam(PSMENU_TIME_TEMPLATE),
				       Plugin::TemplateOpenParam());
		if (Shell_81232FA6() == 0) {
			ui::Widget *panel = m_page->FindChild(IDParam(PSMENU_TIME_PANEL), 0);
			math::v4 size = *panel->GetSize(0);

			size.set_y(0.0f);
			panel->SetSize(size);
			panel->Hide(0xff, NULL, NULL, 0.0f);

			ui::Widget *icon = m_page->FindChild(IDParam(PSMENU_TIME_ICON), 0);

			size = *icon->GetSize(0);
			size.set_y(0.0f);
			icon->SetSize(size);
			icon->Hide(0xff, NULL, NULL, 0.0f);
		}
		imposeLayoutText(box->FindChild(IDParam(PSMENU_TIME_PANEL), 0),
				 IDParam(PSMENU_TIME_LABEL), 570.0f);
		UpdateParentalIcon();
		UpdatePlayTime();
	}

	if (system::Model::GetType() == system::Model::Type_Dolce) {
		m_plugin->TemplateOpen(box, IDParam(PSMENU_MODEL_TEMPLATE),
				       Plugin::TemplateOpenParam());

		ui::Button *powerButton = PSMENU_FIND(box, ui::Button, PSMENU_MODEL_POWER);

		if (powerButton != NULL) {
			powerButton->AddEventCallback(ui::ButtonBase::CB_BTN_DECIDE,
						      imposePsMenuOnPowerButton, this);
			SetFocusButton(powerButton);
		}

		ui::Button *other = PSMENU_FIND(box, ui::Button, PSMENU_MODEL_OTHER);

		if (other != NULL)
			other->AddEventCallback(ui::ButtonBase::CB_BTN_DECIDE,
						imposePsMenuOnOtherButton, this);
	}

	LsdbAppInfoPtr app;

	if (GetApplication(&app)) {
		m_plugin->TemplateOpen(box, IDParam(PSMENU_APP_TEMPLATE),
				       Plugin::TemplateOpenParam());

		ui::Button *menu = PSMENU_FIND(box, ui::Button, PSMENU_APP_MENU);

		if (menu != NULL) {
			menu->AddEventCallback(ui::ButtonBase::CB_BTN_DECIDE, imposePsMenuOnAppMenu,
					       this);
			menu->Disable(false);
			SetFocusButton(menu);
			CheckAppMenuReady();

			ui::Button *other = PSMENU_FIND(box, ui::Button, PSMENU_MODEL_OTHER);

			if (other != NULL)
				other->SetFocusDirection(1, IDParam(PSMENU_APP_MENU));
		}

		ui::Widget *title = box->FindChild(IDParam(PSMENU_APP_TITLE), 0);

		if (title != NULL) {
			wstring str;

			common::Utf8ToUtf16(string(sceLsdbGetTitle(&app)), &str, 0);
			title->SetString(str);
		}

		ShellPluginInterface *applauncher = (ShellPluginInterface *)
			applauncherPluginGetInterface("applauncher_plugin", 1);

		if (applauncher != NULL) {
			intrusive_ptr<cache::CacheImageInterface> icon;
			ui::Widget *iconWidget = box->FindChild(IDParam(PSMENU_APP_ICON), 0);

			if (iconWidget != NULL) {
				bool found = ((bool (*)(intrusive_ptr<cache::CacheImageInterface> *,
							const string &, void *, void *))
					      applauncher->func[3])(
						&icon, string(sceLsdbGetName(&app)),
						(void *)imposePsMenuOnIcon, iconWidget) &&
					icon.get() != NULL;

				if (found) {
					iconWidget->SetDrawObjMaskBit(&icon, 0);
					iconWidget->Show(0xff, NULL, NULL, 0.0f);
				} else {
					iconWidget->Hide(0xff, NULL, NULL, 0.0f);
				}
			}
		}
	}

	if (system::Capability::HasWirelessController())
		m_part81153A18.Impose_81153A26(box);

	if (!system::Capability::HasFrontTouch() && system::Capability::HasWirelessController()) {
		m_plugin->TemplateOpen(box, IDParam(PSMENU_TOUCH_TEMPLATE),
				       Plugin::TemplateOpenParam());

		ui::CheckBox *check = PSMENU_FIND(box, ui::CheckBox, PSMENU_TOUCH_CHECK);

		if (check != NULL) {
			int value;

			check->AddEventCallback(ui::ButtonBase::CB_BTN_DECIDE,
						imposePsMenuOnTouchEmulation, this);
			if (sceRegMgrGetKeyInt("/CONFIG/SHELL", "touch_emulation", &value) == 0 &&
			    value != 0)
				check->SetCheck(true, -0.1f);
		}

		ui::PlaneButton *help = PSMENU_FIND(box, ui::PlaneButton, PSMENU_TOUCH_HELP);

		if (help != NULL)
			help->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
					       new ImposePsMenuListener<ImposePsMenu>(
						       this, &ImposePsMenu::ShowTouchEmulationHelp),
					       true);
		imposeLayoutText(box->FindChild(IDParam(PSMENU_TOUCH_ROW), 0),
				 IDParam(PSMENU_TOUCH_TEXT), 570.0f);
	}

	if (!ScePafMisc_7ED8F03C())
		m_part81152FB8.Impose_81152FC4(box);
	if (!ScePafMisc_B7CFFF5C())
		m_part81158C14.Impose_81158E12(box);

	if (ScePafMisc_7ED8F03C()) {
		m_plugin->TemplateOpen(box, IDParam(PSMENU_BRIGHTNESS_TEMPLATE),
				       Plugin::TemplateOpenParam());
		m_brightness = PSMENU_FIND(box, ui::SlideBar, PSMENU_BRIGHTNESS_BAR);
		if (m_brightness != NULL) {
			int value;
			int maxBrightness;

			m_brightness->AddEventListener(ui::SlideBar::CB_SLIDEBAR_CHANGED,
				new ui::EventCBListener(imposePsMenuOnBrightness, this), false);
			m_brightness->AddEventListener(ui::SlideBar::CB_SLIDEBAR_DRAG_BEGIN,
				new ui::EventCBListener(imposePsMenuOnBrightnessDrag, this), false);
			m_brightness->AddEventListener(ui::SlideBar::CB_SLIDEBAR_DRAG_END,
				new ui::EventCBListener(imposePsMenuOnBrightnessSave, this), false);
			m_brightness->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
			m_brightness->AddEventListener(PSMENU_EVENT_KEY_UP,
				new ui::EventCBListener(imposePsMenuOnBrightnessKey, this), false);
			m_brightness->AddEventListener(PSMENU_EVENT_KEY_DOWN,
				new ui::EventCBListener(imposePsMenuOnBrightnessKey, this), false);
			m_brightness->SetMaxValue(PSMENU_BRIGHTNESS_STEPS);
			m_brightness->m_key_step = PSMENU_BRIGHTNESS_KEY_STEP;
			if (sceRegMgrGetKeyInt("/CONFIG/DISPLAY", "brightness", &value) == 0) {
				sceAVConfigGetDisplayMaxBrightness((unsigned int *)&maxBrightness);
				if (maxBrightness < value)
					value = maxBrightness;
				value -= PSMENU_BRIGHTNESS_MIN;
				if (value < 0)
					value = 0;
				m_brightness->SetValue(value, false);
				m_brightnessValue = value;
			}
		}
	}

	m_part811558C0.Impose_811561AA(box);

	m_plugin->TemplateOpen(box, IDParam(PSMENU_MIC_TEMPLATE), Plugin::TemplateOpenParam());
	m_micMute = PSMENU_FIND(box, ui::CheckBox, PSMENU_MIC_CHECK);
	if (m_micMute != NULL) {
		int value;

		m_micMute->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
			new ui::EventCBListener(imposePsMenuOnMicMute, this), false);
		if (sceRegMgrGetKeyInt("/CONFIG/SHELL", "mic_mute", &value) == 0 && value != 0)
			m_micMute->SetCheck(true, -0.1f);
	}

	m_plugin->TemplateOpen(box, IDParam(PSMENU_VOICE_TEMPLATE), Plugin::TemplateOpenParam());

	ui::Text *text = PSMENU_FIND(box, ui::Text, PSMENU_VOICE_TEXT);

	if (text != NULL)
		text->AddEventListener(PSMENU_EVENT_TEXT_LAYOUT,
				       new ImposePsMenuArgListener<ImposePsMenu, ui::Widget *>(
					       this, &ImposePsMenu::LayoutPrioritizeInfo, text),
				       true);

	ui::PlaneButton *info = PSMENU_FIND(box, ui::PlaneButton, PSMENU_VOICE_INFO);

	if (info != NULL)
		info->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
				       new ImposePsMenuListener<ImposePsMenu>(
					       this, &ImposePsMenu::ShowPrioritizeInfo),
				       true);

	m_voicePriority = PSMENU_FIND(box, ui::CheckBox, PSMENU_VOICE_CHECK);
	if (m_voicePriority != NULL) {
		int value;

		m_voicePriority->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
			new ui::EventCBListener(imposePsMenuOnVoicePriority, this), false);
		if (sceRegMgrGetKeyInt("/CONFIG/SHELL", "voice_priority", &value) == 0 &&
		    value != 0)
			m_voicePriority->SetCheck(true, -0.1f);
	}
	imposeLayoutText(box->FindChild(IDParam(PSMENU_VOICE_ROW), 0),
			 IDParam(PSMENU_VOICE_TEXT), 0.0f);

	UpdateAudioButtons();
	if (imposePsMenuHasShortcut())
		m_part811527E0.Impose_811527E8(box);
	ScePafCommon_6CF3A3C8(0.0f, m_page, common::transition::Type_SlideFromBottom4, true,
			      false);
}

/* (0x8114DDBE) */
void ImposePsMenu::Impose_8114DDBE(bool arg)
{
	m_part811558C0.Impose_81158590(arg);
}

/* the close button (0x8114DDCA) */
static void imposePsMenuOnClose(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	((ImposePsMenu *)data)->Close();
}

/* every 60 ms while the menu is open: the parts and the play time kept current
   (0x8114DDD4) */
static void imposePsMenuOnInterval(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	ImposePsMenu *menu = (ImposePsMenu *)data;

	menu->m_part81158C14.Update();
	menu->m_part81153A18.Impose_81154CD2();
	menu->m_part811558C0.Impose_81157ACE();
	if (Shell_81232FA6() == 0 || Shell_812327F8() != -1)
		menu->UpdatePlayTime();
	menu->UpdateAudioButtons();
}

/* a power event closes the menu (0x8114DE10) */
static void imposePsMenuOnPower(void *data)
{
	((ImposePsMenu *)data)->Close();
}

/* the brightness set as the slide bar moves, to the display's maximum (0x8114DE18) */
static void imposePsMenuOnBrightness(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	ImposePsMenu *menu = (ImposePsMenu *)data;

	if (menu->m_brightness == NULL)
		return;

	unsigned int brightness = menu->m_brightness->m_value + PSMENU_BRIGHTNESS_MIN;
	unsigned int maxBrightness;

	sceAVConfigGetDisplayMaxBrightness(&maxBrightness);
	if (brightness > maxBrightness) {
		brightness = maxBrightness;
		menu->m_brightness->SetValue(maxBrightness - PSMENU_BRIGHTNESS_MIN, false);
	}
	sceAVConfigSetDisplayBrightness(brightness);
}

/* the slide bar taken (folded with paf's empty function) */
static void imposePsMenuOnBrightnessDrag(int32_t type, ui::Handler *self, ui::Event *e,
					 void *data)
{
}

/* the brightness written to the registry when the slide bar is let go (0x8114DE6C) */
static void imposePsMenuOnBrightnessSave(int32_t type, ui::Handler *self, ui::Event *e,
					 void *data)
{
	ImposePsMenu *menu = (ImposePsMenu *)data;

	if (menu->m_brightness != NULL &&
	    menu->m_brightness->m_value != menu->m_brightnessValue) {
		menu->m_brightnessValue = menu->m_brightness->m_value;
		sceRegMgrSetKeyInt("/CONFIG/DISPLAY", "brightness",
				   menu->m_brightnessValue + PSMENU_BRIGHTNESS_MIN);
	}
}

/* up and down on the slide bar scroll the menu, down to the touch emulation's check box
   (0x8114DE9A) */
static void imposePsMenuOnBrightnessKey(int32_t type, ui::Handler *self, ui::Event *e,
					void *data)
{
	ImposePsMenu *menu = (ImposePsMenu *)data;
	ui::ScrollView *scroll = PSMENU_FIND(menu->m_page, ui::ScrollView, PSMENU_SCROLL);

	if (scroll == NULL || e == NULL)
		return;
	if ((uint8_t)e->m_detail == 1) {
		scroll->ScePafWidget_AD706F2A(math::v2(0.0f, 1.0f), NULL);

		ui::CheckBox *check = PSMENU_FIND(menu->m_page, ui::CheckBox, PSMENU_TOUCH_CHECK);

		if (check != NULL && ScePafWidget_DB0FFD33(check))
			check->SetFocus(NULL);
	} else if ((uint8_t)e->m_detail == 2) {
		scroll->ScePafWidget_AD706F2A(math::v2(0.0f, -1.0f), NULL);
	}
}

/* the voice chat put before the application's sound (0x8114E1F4) */
static void imposePsMenuOnVoicePriority(int32_t type, ui::Handler *self, ui::Event *e,
					void *data)
{
	if (self == NULL)
		return;

	bool checked = ((ui::CheckBox *)self)->IsChecked();

	sceAppMgrEnablePrioritizingSystemChat(checked);
	sceRegMgrSetKeyInt("/CONFIG/SHELL", "voice_priority", checked);
}

/* the microphone muted (0x8114E21A) */
static void imposePsMenuOnMicMute(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	if (self == NULL)
		return;
	if (((ui::CheckBox *)self)->IsChecked()) {
		sceAudioInSetMute(1);
		sceRegMgrSetKeyInt("/CONFIG/SHELL", "mic_mute", 1);
	} else {
		sceAudioInSetMute(2);
		sceRegMgrSetKeyInt("/CONFIG/SHELL", "mic_mute", 0);
	}
}

/* the touch emulation's help dialog, its pictures and, in the languages that need it, its
   texts smaller (0x8114E262) */
void ImposePsMenu::ShowTouchEmulationHelp()
{
	g_imposeNotification->MakeSpaceIfUsed();
	m_helpDialog = -1;

	CommonGuiListDialogParam param;

	param.resourcePlugin = m_plugin;
	param.priority = 7;
	param.heading = wstring(m_plugin->GetString(IDParam(PSMENU_HELP_HEADING)));
	SceCommonGuiDialog_D44D1EFB(&param, 0, 1,
				    wstring(m_plugin->GetString(IDParam(PSMENU_HELP_TEXT))));
	m_helpDialog = SceCommonGuiDialog_BACBA7F2(&param);

	ui::Widget *panel = SceCommonGuiDialog_8947FC23(m_helpDialog, PSMENU_DIALOG_PANEL);

	if (panel != NULL) {
		m_plugin->TemplateOpen(panel, IDParam(PSMENU_HELP_TEMPLATE),
				       Plugin::TemplateOpenParam());
		if (ScePafMisc_9CB5D72E() == 0) {
			ui::Plane *plane = PSMENU_FIND(panel, ui::Plane, PSMENU_HELP_PLANE1);

			if (plane != NULL) {
				((ui::Widget *)plane)->SetTexture(intrusive_ptr<graph::Surface>());
				((ui::Widget *)plane)->SetTexture(
					m_plugin->GetTexture(IDParam(PSMENU_HELP_TEX1)));
			}
			plane = PSMENU_FIND(panel, ui::Plane, PSMENU_HELP_PLANE2);
			if (plane != NULL) {
				((ui::Widget *)plane)->SetTexture(intrusive_ptr<graph::Surface>());
				((ui::Widget *)plane)->SetTexture(
					m_plugin->GetTexture(IDParam(PSMENU_HELP_TEX2)));
			}
		}

		int locale = g_imposePlugin->m_locale;

		if (locale == SCE_SYSTEM_PARAM_LANG_NORWEGIAN ||
		    locale == SCE_SYSTEM_PARAM_LANG_POLISH ||
		    locale == SCE_SYSTEM_PARAM_LANG_RUSSIAN ||
		    locale == SCE_SYSTEM_PARAM_LANG_TURKISH ||
		    locale == SCE_SYSTEM_PARAM_LANG_GERMAN ||
		    locale == SCE_SYSTEM_PARAM_LANG_ITALIAN) {
			ui::Text *text = PSMENU_FIND(panel, ui::Text, PSMENU_HELP_TEXT1);
			graph::TextObj *obj = PSMENU_TEXT_OBJ(text);

			if (obj != NULL)
				obj->SetStyleAttribute(graph::TextStyleAttribute_EnlargeFontSize,
						       0, 0, false);
			text = PSMENU_FIND(panel, ui::Text, PSMENU_HELP_TEXT2);
			obj = PSMENU_TEXT_OBJ(text);
			if (obj != NULL)
				obj->SetStyleAttribute(graph::TextStyleAttribute_EnlargeFontSize,
						       0, 0, false);
			text = PSMENU_FIND(panel, ui::Text, PSMENU_HELP_TEXT3);
			obj = PSMENU_TEXT_OBJ(text);
			if (obj != NULL)
				obj->SetStyleAttribute(graph::TextStyleAttribute_EnlargeFontSize,
						       0, 0, false);
		}
	}
}

/* the touch emulation turned on or off, its help shown the first time (0x8114ED80) */
void ImposePsMenu::OnTouchEmulation(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	if (self == NULL || !((ui::Widget *)self)->IsInherit(ui::CheckBox::TypeName()))
		return;

	ui::CheckBox *check = (ui::CheckBox *)self;

	sceRegMgrSetKeyInt("/CONFIG/SHELL", "touch_emulation", check->m_is_checked);
	if (check->m_is_checked) {
		int shown;

		g_imposeTouchEmulation->Enable(true);
		shown = 0;
		sceRegMgrGetKeyInt("/CONFIG/SHELL", "touch_emulation_help", &shown);
		if (shown != 1) {
			sceRegMgrSetKeyInt("/CONFIG/SHELL", "touch_emulation_help", 1);
			ShowTouchEmulationHelp();
		}
	} else {
		g_imposeTouchEmulation->Disable(true);
	}
}

/* the application's menu opened, or the menu closed while its check runs (0x8114EE2E) */
void ImposePsMenu::OnAppMenu(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	if (g_imposePsMenu->m_checking) {
		g_imposePsMenu->Close();
	} else {
		common::SharedPtr<job::JobItem> job(new ImposePsMenuOpenJob);

		job::JobQueue::default_queue->Enqueue(job);
	}
}

/* the voice chat's information button put after its text (0x8114EF18) */
void ImposePsMenu::LayoutPrioritizeInfo(ui::Widget *text)
{
	math::v4 pos = text->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	math::v2 bounds(0.0f, 0.0f);

	((graph::TextObj *)text->GetDrawObj(0))->GetBounds(bounds);

	const math::v4 *size = text->GetSize(0);
	float width = bounds.extract_x();

	if (size->extract_x() < width)
		width = size->extract_x();

	ui::Widget *info = m_page->FindChild(IDParam(PSMENU_VOICE_INFO), 0);

	if (info != NULL)
		info->SetPos(math::v4(pos.extract_x() + width + 32.0f, pos.extract_y(), 0.0f,
				      0.0f));
}

/* the voice chat's information dialog (0x8114F050) */
void ImposePsMenu::ShowPrioritizeInfo()
{
	m_prioritizeDialog = -1;

	CommonGuiListDialogParam param;

	param.resourcePlugin = m_plugin;
	param.priority = 7;
	param.heading = wstring(m_plugin->GetString(IDParam("msg_chat_prioritize_party")));
	SceCommonGuiDialog_D44D1EFB(&param, 0, 1, wstring(m_plugin->GetString(
				    IDParam("msg_chat_prioritize_party_info"))));
	m_prioritizeDialog = SceCommonGuiDialog_BACBA7F2(&param);
}

/* (0x8114F232) */
void ImposePsMenu::Impose_8114F232(bool arg)
{
	m_part81158C14.Impose_811598BA(arg);
}

/* the application's or the settings' icon, once applauncher has it (0x8114F23E) */
__attribute__((noinline)) void
ImposePsMenu::SetIcon(const intrusive_ptr<cache::CacheImageInterface> &icon, ui::Widget *widget)
{
	if (m_page != NULL && widget != NULL) {
		widget->SetDrawObjMaskBit(&icon, 0);
		imposePsMenuShowWidget(widget, NULL);
	}
}

/* (0x8114F294) */
static void imposePsMenuOnIcon(const intrusive_ptr<cache::CacheImageInterface> &icon,
			       void *data)
{
	if (g_imposePsMenu != NULL)
		g_imposePsMenu->SetIcon(icon, (ui::Widget *)data);
}

/* the menu is open (folded with MtpHttpFile::IsOpened, 0x81135260) */
bool ImposePsMenu::IsOpen()
{
	return m_page != NULL;
}
