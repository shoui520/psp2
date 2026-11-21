/*
 * impose_psmenu_controller.cpp: the PS menu's controller settings (0x81153A18-0x811558C0): the
 * vibration check box, kept in the registry; while a controller is on a port, its number and
 * its battery (blinking while it charges), the port given another number from a popup list and
 * the controller disconnected. The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <math/math.h>
#include <versatileparam/idparam.h>
#include <common/char_encoding.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_scene.h>
#include <widget/w_button.h>
#include <widget/w_check_box.h>
#include <widget/w_text.h>
#include <widget/w_composite_drawer.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/drawobj/plane_obj.h>
#include <inputdevice/pad/pad.h>
#include <regmgr/regmgr.h>
#include <ctrl/ctrl.h>
#include <vshbridge/vshbridge.h>
#include "../../livespace_db/lsdb_keys.h"
#include "../../../popup_list.h"
#include "impose.h"

using namespace paf;

/* the template of the settings and its children (PLACEHOLDER names) */
#define CONTROLLER_TEMPLATE		0x6FF8C7D6
#define CONTROLLER_VIBRATION_CHECK	0xEC86DA72
#define CONTROLLER_FOCUS_BUTTON		0x37406542	/* above the vibration in the focus */
#define CONTROLLER_DISCONNECT_BUTTON	0xEA3FAEE8
#define CONTROLLER_ASSIGN_BUTTON	0x28F52AA8
#define CONTROLLER_BATTERY		0x9FB19C21
#define CONTROLLER_PORT_LABEL_1		0x25459037
#define CONTROLLER_PORT_LABEL_2		0xED2DBFEF
#define CONTROLLER_PORT_TEXT		0x9D420574
#define CONTROLLER_PORT_MARK		0x4C054F1A
#define CONTROLLER_PORT_MARK_PLACE	0x2663B7E6
#define CONTROLLER_PORT_MARK_TEMPLATE	0x9C95D1D1
#define CONTROLLER_TEX_BATTERY		0xE0A03290
#define CONTROLLER_TEX_BATTERY_ERROR	0x93B0CAAD

/* the battery's blink: its interval timer and its time in milliseconds */
#define CONTROLLER_BLINK_TIMER		(ui::Handler::CB_INTERVAL | 0x65)
#define CONTROLLER_BLINK_TIME		500.0f

/* the ticks the settings wait for a controller before the port labels go */
#define CONTROLLER_WAIT_TICKS		5

/* sceCtrlGetBatteryInfo's levels, which no header names (PLACEHOLDER names) */
#define CONTROLLER_BATTERY_EMPTY	0
#define CONTROLLER_BATTERY_LEVEL_1	3
#define CONTROLLER_BATTERY_LEVEL_2	4
#define CONTROLLER_BATTERY_LEVEL_3	5
#define CONTROLLER_BATTERY_CHARGING	0xEE
#define CONTROLLER_BATTERY_CHARGED	0xEF
#define CONTROLLER_BATTERY_ERROR_1	0xF0
#define CONTROLLER_BATTERY_ERROR_2	0xF1

/* the controller type of a port with a coloured mark (pad::GetPortType's DualShock 4) */
#define CONTROLLER_PORT_TYPE_DS4	2

/* the popup list of the ports: its type and flags (PLACEHOLDER names) */
#define CONTROLLER_ASSIGN_LIST_TYPE	1
#define CONTROLLER_ASSIGN_LIST_FLAGS	7
#define CONTROLLER_PORTS		4
#define CONTROLLER_ASSIGN_LIST_WIDTH	300.0f
#define CONTROLLER_ASSIGN_LIST_PADDING	70.0f

/* PLACEHOLDER: the menu's vibration event flag bits set (0x8115A034, in impose_psmenu's
   neighbour) */
void Impose_8115A034(SceUInt8 vibration);

/* the ports' marks' colours (0x81564F68) */
static math::v4 s_portColors[CONTROLLER_PORTS] = {
	math::v4(54.0f, 148.0f, 255.0f, 255.0f) / 255.0f,
	math::v4(255.0f, 112.0f, 76.0f, 255.0f) / 255.0f,
	math::v4(44.0f, 168.0f, 82.0f, 255.0f) / 255.0f,
	math::v4(191.0f, 69.0f, 195.0f, 255.0f) / 255.0f,
};

/* the draw object of the index, when it is a plane object */
#define CONTROLLER_PLANE_OBJ(widget, index)						\
	((widget)->GetDrawObj(index) != NULL &&						\
	 (widget)->GetDrawObj(index)->IsInherit(graph::PlaneObj::DrawObjType()) ?	\
	 (graph::PlaneObj *)(widget)->GetDrawObj(index) : NULL)

/* the function the scene calls as it is drawn, and its data */
static inline void imposePsMenuControllerSetDraw(ui::Scene *scene, ui::Scene::DrawFunc func,
						 void *data)
{
	ui::Scene::DrawArg arg;

	arg.m_data = data;
	scene->m_draw_func = func;
	scene->m_draw_arg = arg;
}

/* the colour of a port's mark (ports from 1) */
static inline const math::v4 &imposePsMenuControllerPortColor(int port)
{
	return port > 0 ? s_portColors[(port - 1) % CONTROLLER_PORTS] : s_portColors[0];
}

/* (0x81153A18) */
ImposePsMenuController::ImposePsMenuController(Plugin *plugin)
	: m_plugin(plugin), m_box(NULL), m_port(0), m_count(0), m_battery(0)
{
}

static void imposePsMenuControllerOnDraw(const ui::Scene::DrawArg &arg);

/* the settings opened in the menu's box, the vibration from the registry (0x81153A26) */
void ImposePsMenuController::Open(ui::Widget *box)
{
	m_box = box;
	m_plugin->TemplateOpen(box, IDParam(CONTROLLER_TEMPLATE), Plugin::TemplateOpenParam());

	ui::CheckBox *vibration = PSMENU_FIND(m_box, ui::CheckBox, CONTROLLER_VIBRATION_CHECK);

	if (vibration != NULL) {
		int value;

		vibration->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
			new ImposePsMenuArgListener<ImposePsMenuController, ui::CheckBox *>(
				this, &ImposePsMenuController::OnVibration, vibration),
			false);
		value = 0;
		sceRegMgrGetKeyInt("/CONFIG/SYSTEM", "controller_vibration", &value);
		vibration->SetCheck(value == 1, -0.1f, false);

		ui::Button *above = PSMENU_FIND(m_box, ui::Button, CONTROLLER_FOCUS_BUTTON);

		if (above != NULL) {
			vibration->SetFocusDirection(0, IDParam(CONTROLLER_FOCUS_BUTTON));
			above->SetFocusDirection(1, IDParam(CONTROLLER_VIBRATION_CHECK));
		}
	}

	ui::Button *disconnect = PSMENU_FIND(m_box, ui::Button, CONTROLLER_DISCONNECT_BUTTON);

	if (disconnect != NULL)
		disconnect->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
			new ImposePsMenuListener<ImposePsMenuController>(
				this, &ImposePsMenuController::Disconnect),
			false);

	ui::Button *assign = PSMENU_FIND(m_box, ui::Button, CONTROLLER_ASSIGN_BUTTON);

	if (assign != NULL)
		assign->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
			new ImposePsMenuListener<ImposePsMenuController>(
				this, &ImposePsMenuController::OpenAssign),
			false);
	m_count = CONTROLLER_WAIT_TICKS;
	imposePsMenuControllerSetDraw(m_box->m_scene, imposePsMenuControllerOnDraw, this);
}

/* (0x8115422E) */
void ImposePsMenuController::Close()
{
	if (m_box != NULL) {
		ScePafMisc_57080F75(0);
		imposePsMenuControllerSetDraw(m_box->m_scene, NULL, NULL);
		m_count = 0;
	}
	m_box = NULL;
	m_port = 0;
}

/* the battery's bars blinking while it charges (0x81154262) */
void ImposePsMenuController::StartBlink()
{
	ui::CompositeDrawer *battery = PSMENU_FIND(m_box, ui::CompositeDrawer, CONTROLLER_BATTERY);

	if (battery != NULL)
		battery->SetTimerListener(CONTROLLER_BLINK_TIMER, CONTROLLER_BLINK_TIME,
			new ImposePsMenuArgListener<ImposePsMenuController, ui::CompositeDrawer *>(
				this, &ImposePsMenuController::Blink, battery));
}

/* (0x81154438) */
void ImposePsMenuController::StopBlink()
{
	ui::CompositeDrawer *battery = PSMENU_FIND(m_box, ui::CompositeDrawer, CONTROLLER_BATTERY);

	if (battery != NULL)
		battery->KillTimer(CONTROLLER_BLINK_TIMER);
}

/* the battery of the port's controller shown, when it changed (0x811545C0) */
void ImposePsMenuController::UpdateBattery()
{
	ui::CompositeDrawer *battery = PSMENU_FIND(m_box, ui::CompositeDrawer, CONTROLLER_BATTERY);
	SceUInt8 level;

	if (battery == NULL)
		return;
	sceCtrlGetBatteryInfo(m_port, &level);
	if (m_battery == level)
		return;
	m_battery = level;

	graph::PlaneObj *bar1 = CONTROLLER_PLANE_OBJ(battery, 0);
	graph::PlaneObj *bar2 = CONTROLLER_PLANE_OBJ(battery, 1);
	graph::PlaneObj *bar3 = CONTROLLER_PLANE_OBJ(battery, 2);

	if (bar1 == NULL || bar2 == NULL || bar3 == NULL)
		return;
	bar1->SetVisible(false);
	bar2->SetVisible(false);
	bar3->SetVisible(false);
	battery->GetDrawObj(3)->SetTexture(m_plugin->GetTexture(IDParam(CONTROLLER_TEX_BATTERY)));
	battery->Show(0xff, NULL, NULL, 0.0f);
	StopBlink();

	switch (level) {
	case 1:
	case 2:
		break;
	case CONTROLLER_BATTERY_LEVEL_1:
		bar1->SetVisible(true);
		break;
	case CONTROLLER_BATTERY_LEVEL_2:
		bar1->SetVisible(true);
		bar2->SetVisible(true);
		break;
	case CONTROLLER_BATTERY_LEVEL_3:
		bar1->SetVisible(true);
		bar2->SetVisible(true);
		bar3->SetVisible(true);
		break;
	case CONTROLLER_BATTERY_CHARGING:
		bar1->SetVisible(true);
		bar2->SetVisible(true);
		bar3->SetVisible(true);
		StartBlink();
		break;
	case CONTROLLER_BATTERY_CHARGED:
		bar1->SetVisible(true);
		bar2->SetVisible(true);
		bar3->SetVisible(true);
		break;
	case CONTROLLER_BATTERY_ERROR_1:
	case CONTROLLER_BATTERY_ERROR_2:
		battery->GetDrawObj(3)->SetTexture(
			m_plugin->GetTexture(IDParam(CONTROLLER_TEX_BATTERY_ERROR)));
		break;
	default:
		battery->Hide(0xff, NULL, NULL, 0.0f);
		break;
	}
}

/* (0x81154CD2) */
void ImposePsMenuController::Update()
{
	if (m_box != NULL)
		UpdateBattery();
}

/* the port's number on both labels, and a DualShock 4's mark in the port's colour
   (0x81154CDE) */
void ImposePsMenuController::UpdatePortLabels()
{
	if (m_box == NULL)
		return;

	IDParam labels[2] = { IDParam(CONTROLLER_PORT_LABEL_1), IDParam(CONTROLLER_PORT_LABEL_2) };

	for (unsigned int i = 0; i < 2; i++) {
		ui::Widget *label = m_box->FindChild(labels[i], 0);

		if (label == NULL)
			continue;

		ui::Text *text = PSMENU_FIND(label, ui::Text, CONTROLLER_PORT_TEXT);

		if (text != NULL) {
			char number[8];

			sce_paf_snprintf(number, sizeof(number), "%d", m_port);

			string utf8(number);
			wstring str;

			common::Utf8ToUtf16(utf8, &str, 0);
			text->SetString(str);
		}
		if (inputdevice::pad::GetPortType(m_port) == CONTROLLER_PORT_TYPE_DS4) {
			ui::CompositeDrawer *mark =
				PSMENU_FIND(label, ui::CompositeDrawer, CONTROLLER_PORT_MARK);

			if (mark == NULL) {
				m_plugin->TemplateOpen(
					label->FindChild(IDParam(CONTROLLER_PORT_MARK_PLACE), 0),
					IDParam(CONTROLLER_PORT_MARK_TEMPLATE),
					Plugin::TemplateOpenParam());
				mark = PSMENU_FIND(label, ui::CompositeDrawer, CONTROLLER_PORT_MARK);
			}
			if (mark != NULL) {
				graph::PlaneObj *obj = CONTROLLER_PLANE_OBJ(mark, 1);

				if (obj != NULL)
					obj->SetColor(imposePsMenuControllerPortColor(m_port));
			}
		}
	}
}

/* each frame: the port's labels and battery while a controller is on one, else the labels gone
   once the wait is over (0x81155248) */
void ImposePsMenuController::Tick()
{
	m_port = inputdevice::pad::GetCurrPort();
	ScePafMisc_57080F75(m_port);
	m_battery = 0;
	if (m_port != 0) {
		UpdatePortLabels();
		UpdateBattery();
		imposePsMenuControllerSetDraw(m_box->m_scene, NULL, NULL);
		m_count = 0;
	} else if (m_count <= 0) {
		ui::Widget *box = m_box;

		imposePsMenuControllerSetDraw(box->m_scene, NULL, NULL);
		m_count = 0;
		box->FindChild(IDParam(CONTROLLER_PORT_LABEL_1), 0)->DestroyWidget();
		m_box->FindChild(IDParam(CONTROLLER_PORT_LABEL_2), 0)->DestroyWidget();
	}
	m_count--;
}

/* (0x8115538E) */
static void imposePsMenuControllerOnDraw(const ui::Scene::DrawArg &arg)
{
	((ImposePsMenuController *)arg.m_data)->Tick();
}

/* the battery's next bar shown, all of them hidden once they are (0x81155398) */
void ImposePsMenuController::Blink(ui::CompositeDrawer *battery)
{
	graph::PlaneObj *bar1 = CONTROLLER_PLANE_OBJ(battery, 0);
	graph::PlaneObj *bar2 = CONTROLLER_PLANE_OBJ(battery, 1);
	graph::PlaneObj *bar3 = CONTROLLER_PLANE_OBJ(battery, 2);

	if (bar1 == NULL || bar2 == NULL || bar3 == NULL)
		return;
	if (!bar1->m_flags.m_visible) {
		bar1->SetVisible(true);
	} else if (!bar2->m_flags.m_visible) {
		bar2->SetVisible(true);
	} else if (!bar3->m_flags.m_visible) {
		bar3->SetVisible(true);
	} else {
		bar1->SetVisible(false);
		bar2->SetVisible(false);
		bar3->SetVisible(false);
	}
}

/* (0x8115553C) */
void ImposePsMenuController::Disconnect()
{
	sceCtrlDisconnect(m_port);
	g_imposePsMenu->Close();
}

/* the marks of the list's ports (PLACEHOLDER name; vtable 0x81530EB0) */
class ImposePsMenuControllerAssignItems : public ShellPopupListItemCallback {
public:
	ImposePsMenuControllerAssignItems(Plugin *plugin) : m_plugin(plugin)
	{
	}

	/* a DualShock 4's port given its mark (0x8141B310) */
	virtual void operator()(ui::Widget *item, int index)
	{
		if (inputdevice::pad::GetPortType(index + 1) != CONTROLLER_PORT_TYPE_DS4 ||
		    item == NULL)
			return;

		ui::Widget *parent = item->m_parent;

		m_plugin->TemplateOpen(parent, IDParam(CONTROLLER_PORT_MARK_TEMPLATE),
				       Plugin::TemplateOpenParam());

		ui::CompositeDrawer *mark =
			PSMENU_FIND(parent, ui::CompositeDrawer, CONTROLLER_PORT_MARK);

		if (mark != NULL) {
			graph::PlaneObj *obj = CONTROLLER_PLANE_OBJ(mark, 1);

			if (obj != NULL)
				obj->SetColor(imposePsMenuControllerPortColor(index + 1));
		}
	}

	/* a port chosen: the controller given it (0x8141B598) */
	static void OnSelect(int index, int value, void *arg)
	{
		ImposePsMenuController *controller = (ImposePsMenuController *)arg;

		if (vshCtrlChangePortAssign(controller->m_port, index + 1) == 0) {
			controller->m_port = index + 1;
			ScePafMisc_57080F75(index + 1);
			controller->UpdatePortLabels();
		}
	}

	Plugin *m_plugin;
};

/* the ports listed for the controller to be given another (0x81155554) */
void ImposePsMenuController::OpenAssign()
{
	ShellPopupListParam param(CONTROLLER_ASSIGN_LIST_TYPE, m_plugin);

	param.flags = CONTROLLER_ASSIGN_LIST_FLAGS;
	param.minSize[0] = CONTROLLER_ASSIGN_LIST_WIDTH;
	param.maxSize[0] = 0.0f;
	param.padding[0] = CONTROLLER_ASSIGN_LIST_PADDING;
	param.padding[1] = 0.0f;
	param.SetSelectCallback(ImposePsMenuControllerAssignItems::OnSelect, this);
	for (int i = 0; i < CONTROLLER_PORTS; ) {
		wchar_t number[8];

		sce_paf_swprintf(number, 8, L"%d", ++i);
		param.texts.push_back(wstring(number));
	}
	param.itemCallback = new ImposePsMenuControllerAssignItems(m_plugin);
	param.focus = m_port > 0 ? m_port - 1 : 0;
	shellPopupListOpen(&param, 0, 0);
}

/* (0x81155766) */
void ImposePsMenuController::OnVibration(ui::CheckBox *check)
{
	if (check != NULL) {
		SceUInt8 checked = check->m_is_checked;
		int value;

		Impose_8115A034(checked);
		value = checked != 0;
		sceRegMgrSetKeyInt("/CONFIG/SYSTEM", "controller_vibration", value);
		vshCtrlSetVibrationSetting(value);
	}
}
