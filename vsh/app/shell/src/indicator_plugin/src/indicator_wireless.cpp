/*
 * indicator_wireless.cpp: the network's icon on the information bar: the phone network's type
 * the telephony listener tells, or the Wi-Fi's signal level the item polls itself and hands on as
 * the same events (0x81161A80-0x81162B04). The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <libnetctl.h>
#include <plugin.h>
#include <std/string>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_plane.h>
#include <widget/w_text.h>
#include "../../../shell.h"
#include "indicator.h"

using namespace paf;

/* the icon's template, the icon in it and the root's text hidden with the icon
   (PLACEHOLDER names) */
#define INDICATOR_WIRELESS_TEMPLATE	0x07DB37A0
#define INDICATOR_WIRELESS_ICON		0x75180C2C
#define INDICATOR_WIRELESS_TEXT		0xB383ED84

/* the icon's timers: the Wi-Fi's poll, and the status's poll asked for by the status
   (PLACEHOLDER names) */
#define INDICATOR_WIRELESS_POLL_TIMER	0x2005000
#define INDICATOR_WIRELESS_POLL_TIME	5000.0f
#define INDICATOR_WIRELESS_STATUS_TIMER	0x1005000
#define INDICATOR_WIRELESS_STATUS_TIME	100.0f

/* the icon's animations' time, in milliseconds */
#define INDICATOR_WIRELESS_ANIM_TIME	200.0f

/* the phone network types' icons (PLACEHOLDER names) */
#define INDICATOR_NETWORK_TYPE_2_ICON	0xB255DB63
#define INDICATOR_NETWORK_TYPE_3_ICON	0x32870FF6
#define INDICATOR_NETWORK_TYPE_4_ICON	0x8FB12B65
#define INDICATOR_NETWORK_TYPE_5_ICON	0xBA742617
#define INDICATOR_NETWORK_TYPE_6_ICON	0x79B781FA

/* the Wi-Fi's signal levels' bounds, in percent */
#define INDICATOR_WIFI_RSSI_1		25
#define INDICATOR_WIFI_RSSI_2		50
#define INDICATOR_WIFI_RSSI_3		75

int g_indicatorLastNetworkType = INDICATOR_WIFI_NONE;	/* (0x815449E0) the phone network's
							   last type, given back when attached */
static bool s_statusShown = true;			/* (0x815449E4) the status's value6 */
static bool s_wifiLevelKnown;				/* (0x815668A8) a Wi-Fi level polled */
static bool s_networkType102;				/* (0x815668A9) cleared by types 102 and
							   103; not read */
static bool s_networkAttached;				/* (0x815668AA) */
static bool s_flightMode;				/* (0x815668AB) the status's */

/* the Wi-Fi levels' icons, 207 to 212 (0x815668B0) */
static IDParam s_wifiIcons[] = {
	IDParam(0x89DC8410),
	IDParam(0x2278FB2A),
	IDParam(0xEE958F99),
	IDParam(0xA13376BC),
	IDParam(0x85A3B888),
	IDParam(0x1FE35A03),
};

static void indicatorWirelessUpdate(int type, void *arg);
static void indicatorWirelessOnPoll(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);

/* the widget of the parent with the ID, when it is of the type */
#define INDICATOR_WIRELESS_FIND(parent, type, id)					\
	((parent)->FindChild(IDParam(s_empty_name, (id)), 0) != NULL &&			\
	 (parent)->FindChild(IDParam(s_empty_name, (id)), 0)->IsInherit(type::TypeName()) ?	\
	 (type *)(parent)->FindChild(IDParam(s_empty_name, (id)), 0) : NULL)

/* the icon's template opened in the widget and the icon found in it, shrunk and hidden, the
   item told the network's events, and the Wi-Fi polled (0x81161A80) */
IndicatorWireless::IndicatorWireless(Plugin *plugin, ui::Widget *widget)
	: m_root(widget), m_plugin(plugin)
{
	if (widget != NULL) {
		{
			plugin->TemplateOpen(widget,
					     IDParam(s_empty_name, INDICATOR_WIRELESS_TEMPLATE),
					     Plugin::TemplateOpenParam());
		}
		m_widget = widget->FindChild(IDParam(s_empty_name, INDICATOR_WIRELESS_ICON), 0);
		if (m_widget != NULL) {
			m_widget->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f),
					  new Timer(INDICATOR_WIRELESS_ANIM_TIME),
					  ui::Widget::ANIM_SIZE, 0);
			m_widget->Hide(0xff, NULL, NULL, 0.0f);
			g_indicatorNetworkTypeCB = indicatorWirelessUpdate;
			g_indicatorNetworkTypeArg = this;
			m_widget->KillTimer(INDICATOR_WIRELESS_POLL_TIMER);
			m_widget->SetTimerListener(INDICATOR_WIRELESS_POLL_TIMER,
						   INDICATOR_WIRELESS_POLL_TIME,
						   new ui::EventCBListener(indicatorWirelessOnPoll,
									   this));
		}
	}
}

/* (0x81161CE4) */
IndicatorWireless::~IndicatorWireless()
{
	if (m_widget != NULL) {
		m_widget->KillTimer(INDICATOR_WIRELESS_POLL_TIMER);
		m_widget->KillTimer(INDICATOR_WIRELESS_STATUS_TIMER);
	}
	g_indicatorNetworkTypeCB = NULL;
	g_indicatorNetworkTypeArg = NULL;
}

/* the icon shrunk and hidden */
static inline void indicatorWirelessHide(ui::Widget *icon)
{
	icon->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f), new Timer(INDICATOR_WIRELESS_ANIM_TIME),
		      ui::Widget::ANIM_SIZE, 0);
	icon->Hide(0xff, new Timer(INDICATOR_WIRELESS_ANIM_TIME), NULL, 0.0f);
}

/*
 * A network event: the icon hidden, or given the phone network type's or the Wi-Fi level's
 * image and shown; the phone network's type kept to be given back once it is attached. Without
 * the icon, type 1 hides the root's text (0x81161D90).
 */
static void indicatorWirelessUpdate(int type, void *arg)
{
	IndicatorWireless *item = (IndicatorWireless *)arg;

	if (item == NULL)
		return;

	ui::Plane *icon = item->m_widget != NULL &&
			  item->m_widget->IsInherit(ui::Plane::TypeName()) ?
			  (ui::Plane *)item->m_widget : NULL;

	if (icon != NULL) {
		math::v4 size(30.0f, 24.0f, 0.0f, 0.0f);
		IDParam image(s_empty_name, 0);

		if (type == INDICATOR_NETWORK_HIDE) {
			item->m_widget->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f),
						new Timer(INDICATOR_WIRELESS_ANIM_TIME),
						ui::Widget::ANIM_SIZE, 0);
			item->m_widget->Hide(0xff, new Timer(INDICATOR_WIRELESS_ANIM_TIME), NULL,
					     0.0f);
			g_indicatorLastNetworkType = INDICATOR_WIFI_NONE;
			return;
		}
		if (type < INDICATOR_WIFI_NONE && type > INDICATOR_NETWORK_NONE)
			g_indicatorLastNetworkType = type;
		if (type == INDICATOR_NETWORK_ATTACHED)
			s_networkAttached = true;
		else if (type == INDICATOR_NETWORK_DETACHED)
			s_networkAttached = false;
		if (type == INDICATOR_WIFI_NONE && s_networkAttached) {
			type = g_indicatorLastNetworkType;
			s_wifiLevelKnown = false;
		}
		if (type == INDICATOR_NETWORK_ATTACHED)
			type = g_indicatorLastNetworkType;
		if (s_wifiLevelKnown) {
			switch (type) {
			case INDICATOR_WIFI_LEVEL_1:
				image = s_wifiIcons[0];
				break;
			case INDICATOR_WIFI_LEVEL_2:
				image = s_wifiIcons[1];
				break;
			case INDICATOR_WIFI_LEVEL_3:
				image = s_wifiIcons[2];
				break;
			case INDICATOR_WIFI_LEVEL_4:
				image = s_wifiIcons[3];
				break;
			case INDICATOR_WIFI_CONNECTING:
				image = s_wifiIcons[4];
				break;
			case INDICATOR_WIFI_WIRED:
				image = s_wifiIcons[5];
				break;
			case INDICATOR_WIFI_NONE:
				indicatorWirelessHide(icon);
				return;
			default:
				return;
			}
		} else {
			switch (type) {
			case INDICATOR_NETWORK_NONE:
				return;
			case INDICATOR_NETWORK_TYPE_2:
				image.m_idhash = INDICATOR_NETWORK_TYPE_2_ICON;
				break;
			case INDICATOR_NETWORK_TYPE_3:
				image.m_idhash = INDICATOR_NETWORK_TYPE_3_ICON;
				break;
			case INDICATOR_NETWORK_TYPE_4:
				image.m_idhash = INDICATOR_NETWORK_TYPE_4_ICON;
				break;
			case INDICATOR_NETWORK_TYPE_5:
				image.m_idhash = INDICATOR_NETWORK_TYPE_5_ICON;
				break;
			case INDICATOR_NETWORK_TYPE_6:
				image.m_idhash = INDICATOR_NETWORK_TYPE_6_ICON;
				break;
			case INDICATOR_NETWORK_TYPE_102:
			case INDICATOR_NETWORK_TYPE_103:
				s_networkType102 = false;
				/* fall through */
			default:
				indicatorWirelessHide(icon);
				return;
			}
			if (!s_networkAttached) {
				indicatorWirelessHide(icon);
				return;
			}
		}
		icon->SetSize(size, new Timer(INDICATOR_WIRELESS_ANIM_TIME), ui::Widget::ANIM_SIZE, 0);
		icon->Show(0xff, new Timer(INDICATOR_WIRELESS_ANIM_TIME), NULL, 0.0f);
		static_cast<ui::Widget *>(icon)->SetTexture(item->m_plugin->GetTexture(image));
	}
	if (type == INDICATOR_NETWORK_TYPE_1) {
		ui::Text *text = INDICATOR_WIRELESS_FIND(item->m_root, ui::Text,
							 INDICATOR_WIRELESS_TEXT);

		if (text != NULL) {
			text->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f),
				      new Timer(INDICATOR_WIRELESS_ANIM_TIME), ui::Widget::ANIM_SIZE, 0);
			text->Hide(0xff, new Timer(INDICATOR_WIRELESS_ANIM_TIME), NULL, 0.0f);
		}
	}
}

/* the status: the icon hidden when the flight mode is turned on or value6 cleared, and the Wi-Fi
   polled soon when value4 is 1 (0x8116287C) */
void IndicatorWireless::OnStatus(IndicatorStatus status)
{
	if (s_flightMode != (status.flightMode == 1) || s_statusShown != (status.value6 == 1)) {
		s_flightMode = status.flightMode == 1;
		s_statusShown = status.value6 == 1;
		if (s_flightMode || !s_statusShown)
			indicatorWirelessUpdate(INDICATOR_NETWORK_HIDE, this);
	}
	if (status.value4 == 1)
		m_widget->SetTimerListener(INDICATOR_WIRELESS_STATUS_TIMER,
					   INDICATOR_WIRELESS_STATUS_TIME,
					   new ui::EventCBListener(indicatorWirelessOnPoll, this));
}

extern "C" {
int SceShell_5DC4A753(int *outRawState);
int SceShell_7AD86CE1(int code, SceNetCtlInfo *info);
}

/*
 * The Wi-Fi polled: its signal level, connecting, or the wired connection handed on as a
 * network event, through the telephony listener's queue (0x8116294E).
 */
static void indicatorWirelessOnPoll(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	int level = INDICATOR_WIFI_NONE;
	int state = 0;

	if (SceShell_5DC4A753(&state) != 0)
		return;
	if (state != SCE_NET_CTL_STATE_DISCONNECTED) {
		level = INDICATOR_WIFI_CONNECTING;
		indicatorTelGeneralNetworkTypeCB(0, 0, &level, 0);
		s_wifiLevelKnown = true;
		return;
	}

	SceNetCtlInfo info;

	if (SceShell_7AD86CE1(SCE_NET_CTL_INFO_DEVICE, &info) != 0) {
		indicatorTelGeneralNetworkTypeCB(0, 0, &level, 0);
		return;
	}
	if (info.device == SCE_NET_CTL_DEVICE_WIRED) {
		level = INDICATOR_WIFI_WIRED;
		indicatorTelGeneralNetworkTypeCB(0, 0, &level, 0);
		s_wifiLevelKnown = true;
		return;
	}
	if (info.device != SCE_NET_CTL_DEVICE_WIRELESS) {
		indicatorTelGeneralNetworkTypeCB(0, 0, &level, 0);
		return;
	}
	s_wifiLevelKnown = true;
	if (SceShell_7AD86CE1(SCE_NET_CTL_INFO_RSSI_PERCENTAGE, &info) != 0) {
		indicatorTelGeneralNetworkTypeCB(0, 0, &level, 0);
		return;
	}
	if (info.rssi_percentage <= INDICATOR_WIFI_RSSI_1)
		level = INDICATOR_WIFI_LEVEL_1;
	if (info.rssi_percentage > INDICATOR_WIFI_RSSI_1 &&
	    info.rssi_percentage <= INDICATOR_WIFI_RSSI_2)
		level = INDICATOR_WIFI_LEVEL_2;
	if (info.rssi_percentage > INDICATOR_WIFI_RSSI_2 &&
	    info.rssi_percentage <= INDICATOR_WIFI_RSSI_3)
		level = INDICATOR_WIFI_LEVEL_3;
	if (info.rssi_percentage > INDICATOR_WIFI_RSSI_3)
		level = INDICATOR_WIFI_LEVEL_4;
	indicatorTelGeneralNetworkTypeCB(0, 0, &level, 0);
}
