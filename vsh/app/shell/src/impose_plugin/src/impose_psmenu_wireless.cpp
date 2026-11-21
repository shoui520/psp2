/*
 * impose_psmenu_wireless.cpp: the PS menu's wireless switches (0x81158652-0x81159DF0): the
 * flight mode, Wi-Fi, the mobile network and Bluetooth, each a button whose icon, colour and
 * sound show its state. The flight mode is switched by a job (ImposeFlightModeJob, in the next
 * file) with a dialog shown until the drivers are done; the mobile network's button is taken
 * away where there is none. The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <math/math.h>
#include <versatileparam/idparam.h>
#include <common/shared_ptr.h>
#include <job/job.h>
#include <inputdevice/inputdevice.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <graphics/text/text_style.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_button_base.h>
#include <widget/w_scene.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include <kernel/processmgr.h>
#include "../../livespace_db/livespace_db_internal.h"
#include "../../livespace_db/lsdb_keys.h"
#include "../../applauncher_plugin/src/applauncher.h"
#include "../../../plugins.h"
#include "impose.h"

using namespace paf;

/* the template of the switches and its children (PLACEHOLDER names) */
#define WIRELESS_TEMPLATE		0x950BFA5A
#define WIRELESS_FLIGHT_MODE		0xDA82D2C4
#define WIRELESS_WLAN			0x6F5357FD
#define WIRELESS_BLUETOOTH		0x17D713AA
#define WIRELESS_MOBILE			0x7F5B93D1
#define WIRELESS_LABEL			0x52B82CF6	/* the switches' headings, dimmed while */
#define WIRELESS_LABEL2			0xC639FA44	/* they cannot be used */

/* a button's icon and its sound, on and off (PLACEHOLDER names) */
#define WIRELESS_TEX_ON			0x932502A0
#define WIRELESS_TEX_OFF		0xDC08D24D
#define WIRELESS_SOUND_ON		0xB85636F9
#define WIRELESS_SOUND_OFF		0x4ECAE460

/* the flight mode's waiting dialog (PLACEHOLDER name) */
#define WIRELESS_MSG_FLIGHT_MODE	0x632B6733

/* the event a button gets once it is laid out (PLACEHOLDER name) */
#define WIRELESS_EVENT_LAYOUT		0x100004

/* the width of the buttons without the mobile network's (PLACEHOLDER name) */
#define WIRELESS_WIDE_WIDTH		256.0f

/* indicator_plugin's functions: the flight mode's and Bluetooth's marks (PLACEHOLDER names) */
#define WIRELESS_INDICATOR_FLIGHT_MODE	5
#define WIRELESS_INDICATOR_BLUETOOTH	7

/* a button's icon colour, on and off (0x81566818, 0x815667E8) */
static math::v4 s_onColor(1.0f, 1.0f, 1.0f, 1.0f);
static math::v4 s_offColor(0.5f, 0.5f, 0.5f, 1.0f);

/* the widget's draw object of the index, when it is of the type */
#define WIRELESS_DRAW_OBJ(widget, index, type)						\
	((widget)->GetDrawObj(index) != NULL &&						\
	 (widget)->GetDrawObj(index)->IsInherit(type::DrawObjType()) ?			\
	 (type *)(widget)->GetDrawObj(index) : NULL)

/* the flight mode's job, which keeps the switches current once it is done (PLACEHOLDER
   name) */
class ImposePsMenuFlightModeJob : public ImposeFlightModeJob {
public:
	ImposePsMenuFlightModeJob(ui::Scene *scene, char on, SceInt32 dialog,
				  ImposePsMenuWireless *wireless)
		: ImposeFlightModeJob(scene, on, dialog), m_wireless(wireless)
	{
	}

	virtual void Finish(int result)
	{
		ImposeFlightModeJob::Finish(result);
		m_wireless->UpdateButtons();
		ShellPluginInterface *indicator = (ShellPluginInterface *)
			applauncherPluginGetInterface("indicator_plugin", 1);

		((void (*)(int))indicator->func[WIRELESS_INDICATOR_FLIGHT_MODE])(
			Impose_8115AC7C() ? 1 : 0);
	}

	ImposePsMenuWireless *m_wireless;
};

/* a listener calling the method with the button (the field read once) */
static inline void imposePsMenuWirelessListen(ImposePsMenuWireless *self, ui::Widget *button,
					      int32_t type,
					      void (ImposePsMenuWireless::*func)(ui::Widget *))
{
	button->AddEventListener(type,
		new ImposePsMenuArgListener<ImposePsMenuWireless, ui::Widget *>(self, func, button),
		false);
}

/* (0x81158652) */
__attribute__((noinline)) void ImposePsMenuWireless::SetButtonState(ui::Widget *button, bool on)
{
	if (button != NULL) {
		graph::PlaneObj *icon = WIRELESS_DRAW_OBJ(button, 2, graph::PlaneObj);
		graph::PlaneObj *base = WIRELESS_DRAW_OBJ(button, 0, graph::PlaneObj);

		if (icon != NULL && base != NULL) {
			IDParam sound(WIRELESS_SOUND_OFF);
			Plugin *plugin = m_plugin;

			if (on) {
				icon->SetTexture(plugin->GetTexture(IDParam(WIRELESS_TEX_ON)));
				base->SetColor(s_onColor);
				sound.m_idhash = WIRELESS_SOUND_ON;
			} else {
				icon->SetTexture(plugin->GetTexture(IDParam(WIRELESS_TEX_OFF)));
				base->SetColor(s_offColor);
				sound.m_idhash = WIRELESS_SOUND_OFF;
			}

			ui::WidgetSoundParam::Sound *sounds;
			uint32_t num;

			if (ScePafToplevel_DA3D4AF0(m_plugin, sound, &sounds, &num) == 0) {
				button->m_sound_num = num;
				button->m_sounds = sounds;
			}
		}
	}
}

/* each switch's state, and whether it can be used (0x811588DE) */
__attribute__((noinline)) void ImposePsMenuWireless::UpdateButtons()
{
	if (m_box == NULL)
		return;

	SetButtonState(m_flightMode, Impose_8115AC7C());
	SetButtonState(m_wlan, Impose_8115ACC2());
	SetButtonState(m_mobile, Impose_8115ACD6());
	SetButtonState(m_bluetooth, Impose_8115AD9C());

	bool flightMode = false;
	bool wlan = false;
	bool mobile = false;
	bool bluetooth = false;

	if (m_enabled && Impose_8115ADB0()) {
		m_box->FindChild(IDParam(WIRELESS_LABEL), 0)->SetMetaAlpha(1.0f);
		m_box->FindChild(IDParam(WIRELESS_LABEL2), 0)->SetMetaAlpha(1.0f);
		if (!Impose_8115AA80())
			flightMode = true;
		wlan = !Impose_8115AA80();
		mobile = false;
		if (!Impose_8115AB46())
			mobile = !Impose_8115AC7C();
		bluetooth = true;
	} else {
		m_box->FindChild(IDParam(WIRELESS_LABEL), 0)->SetMetaAlpha(0.3f);
		m_box->FindChild(IDParam(WIRELESS_LABEL2), 0)->SetMetaAlpha(0.3f);
	}

	if (m_flightMode != NULL) {
		if (flightMode)
			m_flightMode->Enable(false);
		else
			m_flightMode->Disable(false);
	}
	if (m_wlan != NULL) {
		if (wlan)
			m_wlan->Enable(false);
		else
			m_wlan->Disable(false);
	}
	if (m_mobile != NULL) {
		if (mobile)
			m_mobile->Enable(false);
		else
			m_mobile->Disable(false);
	}
	if (m_bluetooth != NULL) {
		if (bluetooth)
			m_bluetooth->Enable(false);
		else
			m_bluetooth->Disable(false);
	}
}

/* (0x81158C14) */
ImposePsMenuWireless::ImposePsMenuWireless(Plugin *plugin)
	: m_plugin(plugin), m_box(NULL), m_flightMode(NULL), m_wlan(NULL), m_mobile(NULL),
	  m_bluetooth(NULL), m_enabled(true)
{
}

/* (0x814098DC, the build's one empty function) */
ImposePsMenuWireless::~ImposePsMenuWireless()
{
}

/* the button widened to the width: its frames, and its label beside the icon (0x81158C28) */
void ImposePsMenuWireless::Layout(ui::Widget *button, float width)
{
	graph::TextObj *label = WIRELESS_DRAW_OBJ(button, 1, graph::TextObj);
	graph::PlaneObj *icon = WIRELESS_DRAW_OBJ(button, 2, graph::PlaneObj);
	graph::PlaneObj *frame = WIRELESS_DRAW_OBJ(button, 3, graph::PlaneObj);
	graph::PlaneObj *glow = WIRELESS_DRAW_OBJ(button, 4, graph::PlaneObj);

	if (label != NULL && icon != NULL && frame != NULL && glow != NULL) {
		math::v3 size(button->GetSize(0)->getImpl().getXYZ());

		size.set_x(width);
		frame->SetSize(size);
		glow->SetSize(size);
		size.set_x(width - icon->m_size.extract_x());
		label->SetSize(size);
	}
}

/* the switches opened in the menu's box (0x81158E12) */
void ImposePsMenuWireless::Open(ui::Widget *box)
{
	m_box = box;
	m_plugin->TemplateOpen(box, IDParam(WIRELESS_TEMPLATE), Plugin::TemplateOpenParam());

	m_flightMode = PSMENU_FIND(m_box, ui::ButtonBase, WIRELESS_FLIGHT_MODE);
	if (m_flightMode != NULL) {
		imposePsMenuWirelessListen(this, m_flightMode, ui::ButtonBase::CB_BTN_DECIDE,
						   &ImposePsMenuWireless::OnFlightMode);
		imposePsMenuWirelessListen(this, m_flightMode, WIRELESS_EVENT_LAYOUT,
						   &ImposePsMenuWireless::OnLayout);
		g_imposePsMenu->SetFocusButton(m_flightMode);
	}

	m_wlan = PSMENU_FIND(m_box, ui::ButtonBase, WIRELESS_WLAN);
	if (m_wlan != NULL) {
		imposePsMenuWirelessListen(this, m_wlan, ui::ButtonBase::CB_BTN_DECIDE,
						   &ImposePsMenuWireless::OnWlan);
		imposePsMenuWirelessListen(this, m_wlan, WIRELESS_EVENT_LAYOUT,
						   &ImposePsMenuWireless::OnLayout);
	}

	m_bluetooth = PSMENU_FIND(m_box, ui::ButtonBase, WIRELESS_BLUETOOTH);
	if (m_bluetooth != NULL) {
		imposePsMenuWirelessListen(this, m_bluetooth, ui::ButtonBase::CB_BTN_DECIDE,
						   &ImposePsMenuWireless::OnBluetooth);
		imposePsMenuWirelessListen(this, m_bluetooth, WIRELESS_EVENT_LAYOUT,
						   &ImposePsMenuWireless::OnLayout);
	}

	m_mobile = PSMENU_FIND(m_box, ui::ButtonBase, WIRELESS_MOBILE);
	if (m_mobile != NULL) {
		if (Impose_8115AB10()) {
			imposePsMenuWirelessListen(this, m_mobile, ui::ButtonBase::CB_BTN_DECIDE,
							   &ImposePsMenuWireless::OnMobile);
			imposePsMenuWirelessListen(this, m_mobile, WIRELESS_EVENT_LAYOUT,
							   &ImposePsMenuWireless::OnLayout);

			/* the Japanese label in two lines, the Finnish one smaller */
			graph::TextObj *text = (graph::TextObj *)m_mobile->GetDrawObj(1);
			wstring japanese(L"\x30e2\x30d0\x30a4\x30eb\x30cd\x30c3\x30c8\x30ef\x30fc\x30af");
			wstring finnish(L"Matkapuhelinverkko");
			wstring label;

			text->GetString(label);
			if (sce_paf_wcscmp(label.c_str(), japanese.c_str()) == 0)
				text->SetString(wstring(
					L"\x30e2\x30d0\x30a4\x30eb\n\x30cd\x30c3\x30c8\x30ef\x30fc\x30af"));
			if (sce_paf_wcscmp(label.c_str(), finnish.c_str()) == 0)
				text->SetStyleAttribute(graph::TextStyleAttribute_Point, 0, 0, 15.0f);
		} else {
			m_box->FindChild(IDParam(WIRELESS_MOBILE), 0)->DestroyWidget();
			m_mobile = NULL;
			Layout(m_flightMode, WIRELESS_WIDE_WIDTH);
			Layout(m_wlan, WIRELESS_WIDE_WIDTH);
			Layout(m_bluetooth, WIRELESS_WIDE_WIDTH);
		}
	}

	UpdateButtons();
}

/* (0x811598AA) */
void ImposePsMenuWireless::Close()
{
	m_box = NULL;
	m_flightMode = NULL;
	m_wlan = NULL;
	m_mobile = NULL;
	m_bluetooth = NULL;
}

/* (0x811598BA) */
void ImposePsMenuWireless::SetEnabled(bool enabled)
{
	m_enabled = enabled;
	UpdateButtons();
}

/* the flight mode switched, by its job behind the waiting dialog (0x811598C4) */
void ImposePsMenuWireless::OnFlightMode(ui::Widget *button)
{
	if (Impose_8115AA80() || button == NULL)
		return;

	bool on = !Impose_8115AC7C();

	sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT);
	inputdevice::Lock(0x7F);

	Plugin *plugin = Plugin::Find("impose_plugin");

	if (plugin != NULL) {
		CommonGuiLayoutDialogParam param;

		param.resourcePlugin = plugin;
		param.priority = 7;
		param.text = plugin->GetString(IDParam(WIRELESS_MSG_FLIGHT_MODE));
		param.layout = SceCommonGuiDialog_2A68B4A4;

		SceInt32 dialog = SceCommonGuiDialog_CCFCD9A6(&param);
		common::SharedPtr<job::JobItem> job(new ImposePsMenuFlightModeJob(
			button->m_scene, on ? '1' : '0', dialog, this));

		if (job.get() != NULL)
			job::JobQueue::default_queue->Enqueue(job);
	}

	if (Impose_8115AA1E(on))
		SetButtonState(button, on);
}

/* (0x81159B62) */
void ImposePsMenuWireless::OnWlan(ui::Widget *button)
{
	if (button != NULL) {
		bool on = !Impose_8115ACC2();

		if (Impose_8115AA94(on)) {
			SetButtonState(button, on);
			Impose_8115A080(on);
		}
	}
}

/* (0x81159B92) */
void ImposePsMenuWireless::OnMobile(ui::Widget *button)
{
	if (button != NULL) {
		bool on = !Impose_8115ACD6();

		if (Impose_8115AB5A(on))
			SetButtonState(button, on);
	}
}

/* (0x81159BBC) */
void ImposePsMenuWireless::OnBluetooth(ui::Widget *button)
{
	if (button != NULL) {
		bool on = !Impose_8115AD9C();

		if (Impose_8115AC0C(on)) {
			SetButtonState(button, on);
			ShellPluginInterface *indicator = (ShellPluginInterface *)
				applauncherPluginGetInterface("indicator_plugin", 1);

			((void (*)(int))indicator->func[WIRELESS_INDICATOR_BLUETOOTH])(on ? 1 : 0);
		}
	}
}

/* the label and the icon centred together on the button (0x81159C06) */
void ImposePsMenuWireless::OnLayout(ui::Widget *button)
{
	graph::TextObj *label = WIRELESS_DRAW_OBJ(button, 1, graph::TextObj);
	graph::PlaneObj *icon = WIRELESS_DRAW_OBJ(button, 2, graph::PlaneObj);

	if (label != NULL && icon != NULL) {
		math::v3 size = icon->m_size;
		math::v2 bounds;

		label->SetPos(math::v3((size.extract_x() + 3.0f) * 0.5f - 2.0f, -2.0f, 0.0f));
		bounds = math::v2(0.0f, 0.0f);
		label->GetBounds(bounds);
		icon->SetPos(math::v3((bounds.extract_x() + 3.0f) * -0.5f - 2.0f, 0.0f, 0.0f));
	}
}

/* (0x814098DC, the build's one empty function) */
void ImposePsMenuWireless::Update()
{
}
