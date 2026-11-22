/*
 * indicator_item.h: the information bar's status and its items, each an IndicatorItem on one of
 * the bar's widgets, and the telephony listener's events and callbacks the items take. Kept apart
 * from indicator.h, which needs the LiveSpace database's headers, for the files that use only the
 * items. The file's and all names here are ours (PLACEHOLDER).
 */
#ifndef INDICATOR_ITEM_H
#define INDICATOR_ITEM_H

#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/list>
#include <std/vector>
#include <math/ngp/v4.h>
#include <widget/core/widget.h>
#include <widget/w_text.h>

/* the page and the bar's widgets (PLACEHOLDER names) */
#define INDICATOR_PAGE			0xFFD7E3F0
#define INDICATOR_ROOT			0x3A375CEF

/*
 * What the bar shows, given to every item when it changes (144 bytes). The values are named by
 * the interface function that sets each until their meaning is read (PLACEHOLDER names).
 */
struct IndicatorStatus {
	bool visible;
	int value4;		/* the interface's function 4 */
	int bluetooth;		/* function 7: Bluetooth on, from its configuration */
	int flightMode;		/* function 5: /CONFIG/SYSTEM/flight_mode */
	int value8;		/* function 8 */
	int flag1;		/* 1 at construction */
	int flag2;		/* 1 at construction */
	int notification;	/* function 11: /CONFIG/SHELL/notification */
	int value6;		/* function 6: the bar's two counters shown or hidden */
	paf::math::v4 barColor;		/* the theme's bar color */
	paf::math::v4 iconColor;	/* the icons' and the texts' colors, by the mode */
	paf::math::v4 textColor;
	paf::string noNoticeFile;	/* the theme's notice images, or nothing */
	paf::string newNoticeFile;
	paf::math::v4 noticeFontColor;	/* the theme's notice colors */
	paf::math::v4 noticeGlowColor;
};

/* one of the bar's items, on its widget; told every change of the status */
class IndicatorItem {
public:
	IndicatorItem(paf::ui::Widget *widget = NULL) : m_widget(widget)
	{
	}

	virtual ~IndicatorItem()
	{
	}

	virtual void OnStatus(IndicatorStatus status)
	{
	}

	/* its widget given the status's text color (a text) or icon color, kept clear when it is
	   clear, and so its widgets under it, each keeping its alpha (0x8141BAE0) */
	virtual void UpdateColor(IndicatorStatus status)
	{
		paf::ui::Widget *widget = m_widget;

		if (widget == NULL)
			return;

		paf::math::v4 current = widget->GetColor();
		paf::ui::Widget *text = widget->IsInherit(paf::ui::Text::TypeName()) ? m_widget : NULL;
		paf::math::v4 color;

		if (text != NULL) {
			color = status.textColor;
			color.set_w(current.extract_w() == 0.0f ? 0.0f : status.textColor.extract_w());
		} else {
			color = status.iconColor;
			color.set_w(current.extract_w() == 0.0f ? 0.0f : status.iconColor.extract_w());
		}
		m_widget->SetColor(color, NULL, paf::ui::Widget::ANIM_COLOR, 0);
		SetChildrenColor(m_widget, color);
	}

	/* the widget's widgets given the color, each keeping its alpha, and theirs (0x8141BA3C) */
	void SetChildrenColor(paf::ui::Widget *widget, const paf::math::v4 &color)
	{
		for (paf::list<paf::ui::Widget *>::iterator it = widget->m_children.begin();
		     it != widget->m_children.end(); ++it) {
			paf::math::v4 childColor = color;

			childColor.set_w((*it)->m_color_alpha);
			(*it)->SetColor(childColor, NULL, paf::ui::Widget::ANIM_COLOR, 0);
			SetChildrenColor(*it, color);
		}
	}

	paf::ui::Widget *m_widget;	/* the widget colored, NULL for none */
};

PAF_VECTOR_INLINE_DTOR(IndicatorItem *)

/*
 * The bar's two groups of items, each laid out from the left on its widget (28 bytes; the
 * groups' code is the same, PLACEHOLDER names): the left one's (0x8115FDA0) and the right
 * one's (0x8116D398).
 */
class IndicatorLeftGroup : public IndicatorItem {
public:
	IndicatorLeftGroup(paf::Plugin *plugin, paf::ui::Widget *widget);
	virtual ~IndicatorLeftGroup();					/* (0x81160084) */
	virtual void OnStatus(IndicatorStatus status);			/* (0x8116011A) */

	paf::ui::Widget *m_root;
	paf::vector<IndicatorItem *> m_items;
};

class IndicatorRightGroup : public IndicatorItem {
public:
	IndicatorRightGroup(paf::Plugin *plugin, paf::ui::Widget *widget);
	virtual ~IndicatorRightGroup();
	virtual void OnStatus(IndicatorStatus status);

	paf::ui::Widget *m_root;
	paf::vector<IndicatorItem *> m_items;
};

/* the flight mode's icon, opened from its template in its widget (12 bytes; PLACEHOLDER) */
class IndicatorFlightModeIcon : public IndicatorItem {
public:
	IndicatorFlightModeIcon(paf::Plugin *plugin, paf::ui::Widget *widget);	/* (0x81160468) */
	virtual void OnStatus(IndicatorStatus status);			/* (0x8116058A) */

	paf::ui::Widget *m_root;
};

/* a text on the bar, the plugin's string, condensed to fit (16 bytes; PLACEHOLDER names,
   indicator_label.cpp) */
class IndicatorLabel : public IndicatorItem {
public:
	IndicatorLabel(paf::Plugin *plugin, paf::ui::Widget *widget);		/* (0x81161548) */
	virtual void OnStatus(IndicatorStatus status);			/* (0x81161A6E) */

	paf::ui::Widget *m_root;
	paf::ui::Widget *m_text;	/* set only when its template opens */
};

/*
 * The left group's other items, each opened from its template in the group's widget (16 bytes,
 * the battery's 24; PLACEHOLDER names, after what their widgets show, until each one's file is
 * written): the mobile network's four, made only with a modem, the Wi-Fi's, the battery's (not on
 * the Dolce) and the clock's.
 */
class IndicatorMobileSignal : public IndicatorItem {
public:
	IndicatorMobileSignal(paf::Plugin *plugin, paf::ui::Widget *widget);	/* (0x81169288) */
	virtual ~IndicatorMobileSignal();				/* (0x81169610) */
	virtual void OnStatus(IndicatorStatus status);			/* (0x8116C534) */
	virtual void UpdateColor(IndicatorStatus status);		/* (0x8116C5CC) */

	paf::ui::Widget *m_root;
	paf::Plugin *m_plugin;
};

/* the roaming icon (indicator_roaming.cpp) */
class IndicatorRoaming : public IndicatorItem {
public:
	IndicatorRoaming(paf::Plugin *plugin, paf::ui::Widget *widget);	/* (0x81168DF0) */
	virtual ~IndicatorRoaming();					/* (0x81168F30) */
	paf::ui::Widget *m_root;
	paf::Plugin *m_plugin;
};

class IndicatorMobileOperator : public IndicatorItem {
public:
	IndicatorMobileOperator(paf::Plugin *plugin, paf::ui::Widget *widget);	/* (0x81168030) */
	virtual ~IndicatorMobileOperator();				/* (0x81168170) */
	paf::ui::Widget *m_root;
	paf::Plugin *m_plugin;
};

/* the SIM card's condition, shown in the operator's text (indicator_sim.cpp) */
class IndicatorMobileSim : public IndicatorItem {
public:
	IndicatorMobileSim(paf::Plugin *plugin, paf::ui::Widget *widget);	/* (0x8116C804) */
	virtual ~IndicatorMobileSim();					/* (0x8116C948) */
	virtual void OnStatus(IndicatorStatus status);			/* (0x8116D2F8) */

	paf::ui::Widget *m_root;
	paf::Plugin *m_plugin;
};

/*
 * The network's icon: the phone network's type, or the Wi-Fi's signal level the item polls
 * (indicator_wireless.cpp). It is told the network's events through the telephony listener's
 * callback, g_indicatorNetworkTypeCB.
 */
class IndicatorWireless : public IndicatorItem {
public:
	IndicatorWireless(paf::Plugin *plugin, paf::ui::Widget *widget);	/* (0x81161A80) */
	virtual ~IndicatorWireless();					/* (0x81161CE4) */
	virtual void OnStatus(IndicatorStatus status);			/* (0x8116287C) */

	paf::ui::Widget *m_root;
	paf::Plugin *m_plugin;
};

/*
 * The events the icon is given (PLACEHOLDER names): the phone network's types (1 to 6, 102,
 * 103), the icon hidden (106), the Wi-Fi's levels the poll gives (206 to 212) and the phone
 * network attached and detached (306, 307).
 */
#define INDICATOR_NETWORK_NONE		0
#define INDICATOR_NETWORK_TYPE_1	1
#define INDICATOR_NETWORK_TYPE_2	2
#define INDICATOR_NETWORK_TYPE_3	3
#define INDICATOR_NETWORK_TYPE_4	4
#define INDICATOR_NETWORK_TYPE_5	5
#define INDICATOR_NETWORK_TYPE_6	6
#define INDICATOR_NETWORK_TYPE_102	102
#define INDICATOR_NETWORK_TYPE_103	103
#define INDICATOR_NETWORK_HIDE		106
#define INDICATOR_WIFI_NONE		206
#define INDICATOR_WIFI_LEVEL_1		207
#define INDICATOR_WIFI_LEVEL_2		208
#define INDICATOR_WIFI_LEVEL_3		209
#define INDICATOR_WIFI_LEVEL_4		210
#define INDICATOR_WIFI_CONNECTING	211
#define INDICATOR_WIFI_WIRED		212
#define INDICATOR_NETWORK_ATTACHED	306
#define INDICATOR_NETWORK_DETACHED	307

/*
 * The signal level events the mobile signal's item is given besides the level itself
 * (PLACEHOLDER names, after when the telephony listener gives them): no service (the SIM card
 * not usable, or the modem too hot), the flight mode left and taken, a signal while not
 * registered, and registered.
 */
#define INDICATOR_SIGNAL_NO_SERVICE	101
#define INDICATOR_SIGNAL_FLIGHT_OFF	102
#define INDICATOR_SIGNAL_FLIGHT_ON	103
#define INDICATOR_SIGNAL_UNREGISTERED	104
#define INDICATOR_SIGNAL_REGISTERED	105

/* the signal item's own events, from its OnStatus (PLACEHOLDER names): hidden for the flight mode
   or the status's value6, and the last level given back */
#define INDICATOR_SIGNAL_HIDE		106
#define INDICATOR_SIGNAL_RESTORE	107

/* the operator's names the telephony listener gives (plugins.h) */
struct TelephonyOperatorNames;

/*
 * The functions the telephony listener gives its events to on the main thread, each with its
 * argument, set by the item that shows them (indicator_telephony.cpp, 0x81544A10-0x81544A34;
 * PLACEHOLDER names, after the listener's queue names): the signal level's, the network's
 * type's, the roaming's, the operator's names' and the SIM card's condition's.
 */
extern void (*g_indicatorSignalLevelCB)(int level, void *arg);
extern void *g_indicatorSignalLevelArg;
extern void (*g_indicatorNetworkTypeCB)(int type, void *arg);
extern void *g_indicatorNetworkTypeArg;
extern void (*g_indicatorRoamingCB)(int roaming, void *arg);
extern void *g_indicatorRoamingArg;
extern void (*g_indicatorOperatorNameCB)(TelephonyOperatorNames names, void *arg);
extern void *g_indicatorOperatorNameArg;
extern void (*g_indicatorSimConditionCB)(int condition, void *arg);
extern void *g_indicatorSimConditionArg;

/* the telephony listener's general network type event: its value handed to the main thread
   (0x811685F8; PLACEHOLDER name) */
void indicatorTelGeneralNetworkTypeCB(int event, int result, const int *value, int size);

/* the plugin (indicator_interface.cpp) */
extern paf::Plugin *g_indicatorPlugin;

class IndicatorBattery : public IndicatorItem {
public:
	IndicatorBattery(paf::Plugin *plugin, paf::ui::Widget *widget);		/* (0x8118365A) */
	paf::ui::Widget *m_root;
	int m_reserved[2];
	paf::Plugin *m_plugin;
};

class IndicatorClock : public IndicatorItem {
public:
	IndicatorClock(paf::Plugin *plugin, paf::ui::Widget *widget);		/* (0x81183B60) */
	paf::ui::Widget *m_root;
	paf::Plugin *m_plugin;
};

/*
 * The home's page dots on the bar, a dot for each of topmenu's pages, the page in view's drawn
 * opaque (24 bytes; PLACEHOLDER names). Its functions are the interface's page dot slots.
 */
class IndicatorPageDots : public IndicatorItem {
public:
	IndicatorPageDots(paf::Plugin *plugin, paf::ui::Widget *widget);	/* (0x8116067C) */
	virtual ~IndicatorPageDots();					/* (0x81160774) */
	virtual void OnStatus(IndicatorStatus status);			/* (0x811609E4) */
	virtual void UpdateColor(IndicatorStatus status);		/* (0x81160AD8) */

	void Refresh();							/* (0x8116078E) */
	paf::ui::Widget *AddDot(int key);				/* (0x81160B46) */
	void RemoveDot(int key);					/* (0x81160E02) */

	paf::ui::Widget *m_root;
	paf::Plugin *m_plugin;
	int m_reserved;		/* -1 at construction and when a dot is removed; not read */
	int m_curPage;		/* the page in view's key Refresh last drew, -1 for none */
};

#endif
