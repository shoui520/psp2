/*
 * impose_parental_control.cpp: impose_plugin's parental control lock page (0x81141400-0x81141E44).
 * The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <framework.h>
#include <std/string>
#include <std/stdio.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <common/transition.h>
#include <inputdevice/inputdevice.h>
#include <appmgr/appmgr.h>
#include "../../livespace_db/lsdb_keys.h"
#include "../../applauncher_plugin/src/applauncher.h"
#include "../../applauncher_plugin/src/applauncher_home.h"
#include "../../../plugins.h"
#include "../../../../game/game_plugin/game_plugin.h"
#include "impose.h"

using namespace paf;

/* the page and its widgets (PLACEHOLDER names) */
#define PARENTAL_PAGE		0x561E4D15
#define PARENTAL_SETTINGS	0xB87C401F	/* the button that opens the settings */
#define PARENTAL_POWER		0x7BD3B2B7	/* the button that turns the console off */
#define PARENTAL_FRAME		0xB7B458E5
#define PARENTAL_PANEL		0x20F70188
#define PARENTAL_MESSAGE	0xD1B3DE28
#define PARENTAL_MSG_LOCKED	0x3C9C49A1
#define PARENTAL_MSG_WRONG	0x1CFA3ECA	/* the passcode was wrong, with the count */
#define PARENTAL_NOTICE_TITLE	0x2617A32D

ImposeParentalControl *g_imposeParentalControl;

/* (0x81141400) */
ImposeParentalControl::ImposeParentalControl(Plugin *plugin) : m_plugin(plugin), m_page(NULL)
{
	g_imposeParentalControl = this;
}

/* (0x81141414) */
ImposeParentalControl::~ImposeParentalControl()
{
	g_imposeParentalControl = NULL;
}

static void imposeParentalControlOnSettings(int32_t type, ui::Handler *self, ui::Event *e,
					    void *userdata);
static void imposeParentalControlOnPower(int32_t type, ui::Handler *self, ui::Event *e,
					 void *userdata);

/* the page's buttons wired, the power button given the icon (0x81141424) */
void ImposeParentalControl::SetIcon(const intrusive_ptr<cache::CacheImageInterface> &icon)
{
	if (m_page != NULL) {
		ui::Widget *settings = m_page->FindChild(IDParam(PARENTAL_SETTINGS), 0);

		if (settings != NULL) {
			settings->AddEventListener(0x10000008,
				new ui::EventCBListener(imposeParentalControlOnSettings, NULL), true);
			settings->SetFocus(NULL);
			if (icon.get() != NULL) {
				ui::Widget *power = m_page->FindChild(IDParam(PARENTAL_POWER), 0);

				if (power != NULL) {
					power->AddEventListener(0x10000008,
						new ui::EventCBListener(imposeParentalControlOnPower,
									NULL), true);
					power->SetMetaAlpha(1.0f, NULL, ui::Widget::ANIM_META_ALPHA, 0,
							    0.0f, NULL, NULL);
					power->SetDrawObjMaskBit(&icon, 0);

					graph::DrawObj *obj = power->GetDrawObj(0);

					if (obj != NULL)
						obj->SetShader(Shell_8139303C());
				}
			}
		}
	}
}

static void imposeParentalControlIconLoaded(const intrusive_ptr<cache::CacheImageInterface> &icon);

/* the page opened over everything and the input taken (0x8114161E) */
void ImposeParentalControl::Open(int keepMessage)
{
	Plugin::PageOpenParam param;

	param.overwrite_draw_priority = 33;
	m_page = m_plugin->PageOpen(IDParam(PARENTAL_PAGE), param);
	if (m_page != NULL) {
		Framework::Instance()->PlayCommonSE(13, 1.0f, false);

		ui::Widget *frame = m_page->FindChild(IDParam(PARENTAL_FRAME), 0);

		if (frame != NULL) {
			ScePafWidget_0253DC9C(frame, true);
			ScePafCommon_6CF3A3C8(0.0f, frame, common::transition::Type_FadeinSlow, false,
					      false);
		}

		ui::Widget *panel = m_page->FindChild(IDParam(PARENTAL_PANEL), 0);

		if (panel != NULL) {
			ScePafWidget_00AA6519(panel);
			ScePafCommon_6CF3A3C8(0.0f, panel, common::transition::Type_Popup1, true, false);
		}
		if (keepMessage == 0) {
			ui::Widget *message = m_page->FindChild(IDParam(PARENTAL_MESSAGE), 0);

			if (message != NULL)
				message->SetString(wstring(m_plugin->GetString(IDParam(PARENTAL_MSG_LOCKED))));
		}

		ShellPluginInterface *applauncher = (ShellPluginInterface *)
			applauncherPluginGetInterface("applauncher_plugin", 1);

		if (applauncher != NULL) {
			intrusive_ptr<cache::CacheImageInterface> icon;

			((bool (*)(intrusive_ptr<cache::CacheImageInterface> *, const string &, void *,
				   void *))applauncher->func[3])(&icon, string("power"),
				(void *)imposeParentalControlIconLoaded, NULL);
			SetIcon(icon);
		}
		inputdevice::GetOwnership(0x6F);

		ShellPluginInterface *topmenu = (ShellPluginInterface *)
			applauncherPluginGetInterface("topmenu_plugin", 1);

		if (topmenu != NULL)
			((void (*)(ImposeParentalControl *, int, int, int, int))
				topmenu->func[TOPMENU_PLUGIN_FUNC_MODE_ENTER])(
				this, param.overwrite_draw_priority, 1, 0, 0);

		ShellPluginInterface *power = (ShellPluginInterface *)
			applauncherPluginGetInterface("power_manage_plugin", 1);

		if (power != NULL)
			((void (*)(void (*)(void), void *))
				power->func[POWER_MANAGE_PLUGIN_FUNC_ADD_CALLBACK])(imposeExitReserved, NULL);

		ShellPluginInterface *game = (ShellPluginInterface *)
			applauncherPluginGetInterface("game_plugin", 1);

		if (game != NULL)
			((void (*)(bool))game->func[GAME_PLUGIN_FUNC_SET_DIALOG_HOLD])(true);
	}
}

/* the page closed and the input given back (0x811419F2) */
void ImposeParentalControl::Close()
{
	if (m_page == NULL)
		return;

	float time = 0.0f;
	ui::Widget *panel = m_page->FindChild(IDParam(PARENTAL_PANEL), 0);

	if (panel != NULL)
		time = ScePafCommon_B48C9BDC(time, panel, common::transition::Type_Popup1, false,
					     false);

	ui::Widget *frame = m_page->FindChild(IDParam(PARENTAL_FRAME), 0);

	if (frame != NULL)
		ScePafCommon_B48C9BDC(time - 100.0f, frame, common::transition::Type_FadeinSlow,
				      false, false);
	ScePafWidget_5A9793B0(m_page, time);
	m_plugin->PageClose(IDParam(PARENTAL_PAGE), Plugin::PageCloseParam());
	m_page = NULL;

	ShellPluginInterface *game = (ShellPluginInterface *)
		applauncherPluginGetInterface("game_plugin", 1);

	if (game != NULL)
		((void (*)(bool))game->func[GAME_PLUGIN_FUNC_SET_DIALOG_HOLD])(false);

	ShellPluginInterface *power = (ShellPluginInterface *)
		applauncherPluginGetInterface("power_manage_plugin", 1);

	if (power != NULL)
		((void (*)(void (*)(void), void *))
			power->func[POWER_MANAGE_PLUGIN_FUNC_REMOVE_CALLBACK])(imposeExitReserved, NULL);

	ShellPluginInterface *topmenu = (ShellPluginInterface *)
		applauncherPluginGetInterface("topmenu_plugin", 1);

	if (topmenu != NULL)
		((void (*)(ImposeParentalControl *, int))
			topmenu->func[TOPMENU_PLUGIN_FUNC_MODE_LEAVE])(this, 230);
	inputdevice::ReleaseOwnership(0x6F);
}

/* the wrong passcode told, with the count (0x81141C16) */
void ImposeParentalControl::ShowNotice(int count)
{
	ImposePart81148260 *notices = g_imposePart81148260;

	if (notices == NULL)
		return;

	ImposeNotice notice;
	wchar_t number[3];

	notice.name = "parental_control_dialog";
	notice.titleHash = PARENTAL_NOTICE_TITLE;
	sce_paf_swprintf(number, 3, L"%d", count);

	wstring text = shellFormatText(m_plugin->GetString(IDParam(PARENTAL_MSG_WRONG)), number);

	notice.message = text.c_str();
	notices->Impose_811497A0(&notice);
}

/* the settings button: the parental control settings launched (0x81141D84) */
static void imposeParentalControlOnSettings(int32_t type, ui::Handler *self, ui::Event *e,
					    void *userdata)
{
	sceAppMgrLaunchAppByName(0x20000, "NPXS10094", "temporary");
}

/* the power button: the console turned off (0x81141DA0) */
static void imposeParentalControlOnPower(int32_t type, ui::Handler *self, ui::Event *e,
					 void *userdata)
{
	ShellPluginInterface *power = (ShellPluginInterface *)
		applauncherPluginGetInterface("power_manage_plugin", 1);

	if (power != NULL)
		power->func[19]();
}

/* the power icon, once applauncher has it (0x81141DBC) */
static void imposeParentalControlIconLoaded(const intrusive_ptr<cache::CacheImageInterface> &icon)
{
	g_imposeParentalControl->SetIcon(icon);
}
