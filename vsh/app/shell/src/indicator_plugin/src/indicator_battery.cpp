/*
 * indicator_battery.cpp: the bar's battery icon, polling the remaining level every 2.5 seconds
 * and on shell events 13 and 14, animating the charging overlay while charging below 100%, and
 * posting the low-battery notice through impose_plugin (0x81184484-0x81185130). The file's and
 * all names here are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <power/power.h>
#include <plugin.h>
#include <std/stdlib.h>
#include <timer/timer.h>
#include <datetime/datetime.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include "../../../shell.h"
#include "../../../plugins.h"
#include "../../applauncher_plugin/src/applauncher.h"
#include "indicator_item.h"

using namespace paf;

/* the battery's template, widgets, textures and low-battery notice (PLACEHOLDER names) */
#define INDICATOR_BAT_TEMPLATE		0xFAE265F7
#define INDICATOR_BAT_WIDGET		0x91728B71
#define INDICATOR_BAT_BAR		0xFF5AB9B2
#define INDICATOR_BAT_CHARGE		0xFDDE1A6F
#define INDICATOR_BAT_LOW_MSG		0x95C00005
#define INDICATOR_BAT_LOW_ICON		0x2617A32D
#define INDICATOR_BAT_NO_CHARGE		0xA5AAFD9B
#define INDICATOR_BAT_LEVEL_0		0x78600544
#define INDICATOR_BAT_LEVEL_1		0x4A8AE255
#define INDICATOR_BAT_LEVEL_2		0x9E9CFCAB
#define INDICATOR_BAT_LEVEL_3		0xF85F0B80
#define INDICATOR_BAT_LEVEL_4		0x56E72F16
#define INDICATOR_BAT_CHARGE_0		0x32465C57
#define INDICATOR_BAT_CHARGE_1		0x00EA236E

/* the poll (2.5 s) and charge-animation (10 ms) interval timers */
#define INDICATOR_BAT_POLL_TIMER	0x2000
#define INDICATOR_BAT_CHARGE_TIMER	0x2001
#define INDICATOR_BAT_POLL_INTERVAL	2500.0f
#define INDICATOR_BAT_CHARGE_INTERVAL	10.0f
#define INDICATOR_SHELL_EVENT_13	13
#define INDICATOR_SHELL_EVENT_14	14

/* whether the low-battery notice has been posted (0x81544CCC) */
static bool s_lowBatteryNotified;

static void indicatorBatteryOnTimer(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
static void indicatorBatteryOnChargeTimer(SceInt32 type, ui::Handler *self, ui::Event *e,
					  void *data);
static void indicatorBatteryOnShellEvent(int event, const void *data, SceSize size, void *arg);

/* the low-battery notice posted once on entering the empty level, reset on leaving (0x81184484) */
__attribute__((noinline))
int IndicatorBattery::ShowLowNotice(int low)
{
	ImposePluginInterface *iface = (ImposePluginInterface *)
		applauncherPluginGetInterface("impose_plugin", 1);

	if (iface != NULL) {
		if (low == 1 && !s_lowBatteryNotified) {
			ImposeNotice notice;

			s_lowBatteryNotified = true;
			notice.name = "indicator_plugin";
			notice.message = indicatorGetString(IDParam(s_empty_name,
								    INDICATOR_BAT_LOW_MSG));
			notice.iconHash = INDICATOR_BAT_LOW_ICON;
			iface->showNotice(&notice);
		} else if (low == 0) {
			s_lowBatteryNotified = false;
		}
	}
	return low;
}

/* the battery's level and charging state read from ScePower and drawn (0x811845B8) */
__attribute__((noinline))
void IndicatorBattery::Update(bool animate)
{
	int maxLevel = scePowerGetBatteryRemainMaxLevel();
	int level = scePowerGetBatteryRemainLevel();

	if (level < 0) {
		int32_t timerId = ui::Handler::CB_INTERVAL | INDICATOR_BAT_CHARGE_TIMER;

		if (m_widget->m_map_t_listener.find(timerId) != m_widget->m_map_t_listener.end()) {
			m_charging = level;
			m_widget->KillIntervalEvent(INDICATOR_BAT_CHARGE_TIMER);
		}
		return;
	}

	if (animate)
		m_widget->Show(0xff, new Timer(200.0f), NULL, 0.0f);
	else
		m_widget->Show(0xff, NULL, NULL, 0.0f);

	int charging = scePowerIsBatteryCharging();

	if (charging < 0)
		return;
	if (scePowerGetBatteryLifePercent() == 100)
		charging = 0;

	if (charging != m_charging) {
		int32_t timerId = ui::Handler::CB_INTERVAL | INDICATOR_BAT_CHARGE_TIMER;

		m_charging = charging;
		if (charging != 0) {
			if (m_widget->m_map_t_listener.find(timerId) ==
			    m_widget->m_map_t_listener.end())
				m_widget->SetIntervalEvent(INDICATOR_BAT_CHARGE_TIMER,
							   INDICATOR_BAT_CHARGE_INTERVAL,
							   indicatorBatteryOnChargeTimer, this);
		} else {
			if (m_widget->m_map_t_listener.find(timerId) !=
			    m_widget->m_map_t_listener.end())
				m_widget->KillIntervalEvent(INDICATOR_BAT_CHARGE_TIMER);
			m_chargeWidget->SetTexture(m_plugin->GetTexture(
				IDParam(s_empty_name, INDICATOR_BAT_NO_CHARGE)));
			if (animate)
				m_chargeWidget->Show(0xff, new Timer(200.0f), NULL, 0.0f);
			else
				m_chargeWidget->Show(0xff, NULL, NULL, 0.0f);
		}
	}

	if (level == 0)
		ShowLowNotice(1);
	else
		ShowLowNotice(0);

	IDParam id(s_empty_name, 0);
	int step = (int)((float)level * (4.0f / (float)maxLevel));

	switch (step) {
	case 0:
		id.m_idhash = INDICATOR_BAT_LEVEL_0;
		break;
	case 1:
		id.m_idhash = INDICATOR_BAT_LEVEL_1;
		break;
	case 2:
		id.m_idhash = INDICATOR_BAT_LEVEL_2;
		break;
	case 3:
		id.m_idhash = INDICATOR_BAT_LEVEL_3;
		break;
	default:
		id.m_idhash = INDICATOR_BAT_LEVEL_4;
		break;
	}
	m_barWidget->SetTexture(m_plugin->GetTexture(id));
	if (animate)
		m_barWidget->Show(0xff, new Timer(200.0f), NULL, 0.0f);
	else
		m_barWidget->Show(0xff, NULL, NULL, 0.0f);
}

/* one frame of the charging overlay, picking one of the two textures at random (0x81184BA8) */
__attribute__((noinline))
void IndicatorBattery::AnimateCharge()
{
	static IDParam s_chargeIds[2] = {
		IDParam(s_empty_name, INDICATOR_BAT_CHARGE_0),
		IDParam(s_empty_name, INDICATOR_BAT_CHARGE_1)
	};

	m_chargeWidget->SetTexture(m_plugin->GetTexture(s_chargeIds[sce_paf_rand() & 1]));
	m_chargeWidget->Show(0xff, new Timer(200.0f), NULL, 0.0f);
}

/* (0x81184D68) */
IndicatorBattery::IndicatorBattery(Plugin *plugin, ui::Widget *widget)
	: IndicatorItem(NULL), m_root(widget), m_plugin(plugin), m_reserved(0), m_charging(0)
{
	if (widget != NULL) {
		bool opened = plugin->TemplateOpen(widget, IDParam(s_empty_name, INDICATOR_BAT_TEMPLATE),
						   Plugin::TemplateOpenParam()) == 0;

		if (opened) {
			m_widget = widget->FindChild(IDParam(s_empty_name, INDICATOR_BAT_WIDGET), 0);
			if (m_widget != NULL) {
				m_widget->Hide(0xff, NULL, NULL, 0.0f);
				m_barWidget = m_widget->FindChild(
					IDParam(s_empty_name, INDICATOR_BAT_BAR), 0);
				m_chargeWidget = m_widget->FindChild(
					IDParam(s_empty_name, INDICATOR_BAT_CHARGE), 0);
				m_widget->SetIntervalEvent(INDICATOR_BAT_POLL_TIMER,
							   INDICATOR_BAT_POLL_INTERVAL,
							   indicatorBatteryOnTimer, this);
				Update(false);
				SceShell_7FE4E732(INDICATOR_SHELL_EVENT_13,
						  indicatorBatteryOnShellEvent, this);
				SceShell_7FE4E732(INDICATOR_SHELL_EVENT_14,
						  indicatorBatteryOnShellEvent, this);
			}
		}
	}
}

/* (0x8118502A) */
IndicatorBattery::~IndicatorBattery()
{
	if (m_widget != NULL) {
		SceShell_D4C7D2B7(INDICATOR_SHELL_EVENT_13, indicatorBatteryOnShellEvent, this);
		SceShell_D4C7D2B7(INDICATOR_SHELL_EVENT_14, indicatorBatteryOnShellEvent, this);
		m_widget->KillIntervalEvent(INDICATOR_BAT_POLL_TIMER);
	}
}

/* (0x811850CC) */
static void indicatorBatteryOnTimer(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	((IndicatorBattery *)data)->Update(true);
}

/* (0x811850D8) */
static void indicatorBatteryOnChargeTimer(SceInt32 type, ui::Handler *self, ui::Event *e,
					  void *data)
{
	((IndicatorBattery *)data)->AnimateCharge();
}

/* (0x811850E2) */
static void indicatorBatteryOnShellEvent(int event, const void *data, SceSize size, void *arg)
{
	if (event == INDICATOR_SHELL_EVENT_13)
		((IndicatorBattery *)arg)->Update(false);
	else
		((IndicatorBattery *)arg)->Update(true);
}
