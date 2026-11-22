/*
 * indicator_signal.cpp: the mobile network's signal on the information bar: an icon with five
 * bars grown to the level the telephony listener gives, a mark over it while not registered, the
 * flight mode's image in its place, and the "no service" text while there is no signal
 * (0x81169288-0x8116C804). The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_scene.h>
#include <widget/w_plane.h>
#include <widget/w_text.h>
#include "indicator_item.h"

using namespace paf;

/* the signal's template, its icon, the icon's bars and mark, and its text (PLACEHOLDER names) */
#define INDICATOR_SIGNAL_TEMPLATE	0x3DDFCB34
#define INDICATOR_SIGNAL_ICON		0x4AE47F6E
#define INDICATOR_SIGNAL_BARS		0x916CEE1B
#define INDICATOR_SIGNAL_BAR_1		0xE41AF1FE
#define INDICATOR_SIGNAL_BAR_2		0xE4981AB7
#define INDICATOR_SIGNAL_BAR_3		0x2AD0DF02
#define INDICATOR_SIGNAL_BAR_4		0x558D85B2
#define INDICATOR_SIGNAL_BAR_5		0x11A0A1F7
#define INDICATOR_SIGNAL_MARK		0x0A4CDBF4
#define INDICATOR_SIGNAL_TEXT		0x6EB51594

/* the operator's name, hidden with the signal (indicator_operator.cpp's text) */
#define INDICATOR_SIGNAL_OPERATOR_TEXT	0xB383ED84

/* the icon's images in the flight mode and while not registered, and the text's string
   (PLACEHOLDER names) */
#define INDICATOR_SIGNAL_FLIGHT_TEXTURE	0xC4790363
#define INDICATOR_SIGNAL_UNREGISTERED_TEXTURE 0xF5C2D1B2
#define INDICATOR_SIGNAL_NO_SERVICE_STRING 0xC0D915A8

/* the scene's event the signal starts on, and the text's layout done (paf's events, PLACEHOLDER
   names) */
#define INDICATOR_SIGNAL_SCENE_EVENT	0x800008
#define INDICATOR_SIGNAL_LAYOUT_EVENT	0x100004

/* the icon's and the bars' animations' time, in milliseconds */
#define INDICATOR_SIGNAL_ANIM_TIME	200.0f
#define INDICATOR_SIGNAL_GROW_TIME	400.0f

/* the bars */
#define INDICATOR_SIGNAL_BAR_NUM	5

/* the level last given, -1 before the first (0x81544A40) */
static int s_signalLevel = -1;
/* the status's value6, as it was told last (0x81544A44) */
static bool s_signalStatusShown = true;
/* the flight mode's image shown (0x81566974) */
static bool s_signalFlight;
/* the mark shown, not registered (0x81566975) */
static bool s_signalUnregistered;
/* the status's flight mode, as it was told last (0x81566976) */
static bool s_signalFlightMode;
/* the text shown at its size, its layout done (0x81566977) */
static bool s_signalTextShown;

static void indicatorMobileSignalUpdate(int level, void *arg);
static void indicatorMobileSignalOnLayout(SceInt32 type, ui::Handler *self, ui::Event *e,
					  void *data);
static void indicatorMobileSignalOnScene(SceInt32 type, ui::Handler *self, ui::Event *e,
					 void *data);

/* the widget of the parent with the ID, when it is of the type */
#define INDICATOR_SIGNAL_FIND(parent, type, param)					\
	((parent)->FindChild(param, 0) != NULL &&					\
	 (parent)->FindChild(param, 0)->IsInherit(type::TypeName()) ?			\
	 (type *)(parent)->FindChild(param, 0) : NULL)

/* the signal's template opened in the widget and its icon found in it and hidden, and the bars;
   the item told the signal's levels, and the scene's start (0x81169288) */
IndicatorMobileSignal::IndicatorMobileSignal(Plugin *plugin, ui::Widget *widget)
	: m_root(widget), m_plugin(plugin)
{
	if (widget != NULL) {
		{
			plugin->TemplateOpen(widget,
					     IDParam(s_empty_name, INDICATOR_SIGNAL_TEMPLATE),
					     Plugin::TemplateOpenParam());
		}
		m_widget = widget->FindChild(IDParam(s_empty_name, INDICATOR_SIGNAL_ICON), 0);
		m_widget->Hide(0xff, NULL, NULL, 0.0f);
		if (m_widget != NULL) {
			ui::Plane *bars = INDICATOR_SIGNAL_FIND(widget, ui::Plane,
								IDParam(s_empty_name,
									INDICATOR_SIGNAL_BARS));

			if (bars != NULL)
				bars->Hide(0xff, NULL, NULL, 0.0f);
			g_indicatorSignalLevelCB = indicatorMobileSignalUpdate;
			g_indicatorSignalLevelArg = this;
			m_widget->m_scene->AddEventListener(INDICATOR_SIGNAL_SCENE_EVENT,
							    new ui::EventCBListener(
								    indicatorMobileSignalOnScene, this));
		}
	}
}

/* (0x81169610) */
IndicatorMobileSignal::~IndicatorMobileSignal()
{
	g_indicatorSignalLevelCB = NULL;
	g_indicatorSignalLevelArg = NULL;
}

/*
 * The bars of the icon given the level in the color: those above the level shrunk at once, those
 * up to it grown over 200 ms, or, when the level rose from the last, first past their size and
 * back over 400 ms (0x81169634).
 */
static void indicatorMobileSignalSetBars(IndicatorMobileSignal *item, ui::Widget *icon, int last,
					 int level, math::v4 color)
{
	ui::Plane *bars = INDICATOR_SIGNAL_FIND(icon, ui::Plane,
						IDParam(s_empty_name, INDICATOR_SIGNAL_BARS));

	if (bars == NULL)
		return;

	ui::Plane *bar[INDICATOR_SIGNAL_BAR_NUM] = {
		INDICATOR_SIGNAL_FIND(bars, ui::Plane,
				      IDParam(s_empty_name, INDICATOR_SIGNAL_BAR_1)),
		INDICATOR_SIGNAL_FIND(bars, ui::Plane,
				      IDParam(s_empty_name, INDICATOR_SIGNAL_BAR_2)),
		INDICATOR_SIGNAL_FIND(bars, ui::Plane,
				      IDParam(s_empty_name, INDICATOR_SIGNAL_BAR_3)),
		INDICATOR_SIGNAL_FIND(bars, ui::Plane,
				      IDParam(s_empty_name, INDICATOR_SIGNAL_BAR_4)),
		INDICATOR_SIGNAL_FIND(bars, ui::Plane,
				      IDParam(s_empty_name, INDICATOR_SIGNAL_BAR_5)),
	};
	math::v4 size[INDICATOR_SIGNAL_BAR_NUM] = {
		math::v4(3.0f, 4.0f, 0.0f, 0.0f),
		math::v4(3.0f, 7.0f, 0.0f, 0.0f),
		math::v4(3.0f, 10.0f, 0.0f, 0.0f),
		math::v4(3.0f, 13.0f, 0.0f, 0.0f),
		math::v4(3.0f, 17.0f, 0.0f, 0.0f),
	};
	math::v4 grown[INDICATOR_SIGNAL_BAR_NUM] = {
		math::v4(3.0f, 6.0f, 0.0f, 0.0f),
		math::v4(3.0f, 9.0f, 0.0f, 0.0f),
		math::v4(3.0f, 12.0f, 0.0f, 0.0f),
		math::v4(3.0f, 15.0f, 0.0f, 0.0f),
		math::v4(3.0f, 19.0f, 0.0f, 0.0f),
	};
	math::v4 off(3.0f, 1.0f, 0.0f, 0.0f);

	for (int i = 0; i < INDICATOR_SIGNAL_BAR_NUM; i++) {
		if (bar[i] == NULL)
			continue;
		if (i <= level - 2) {
			if (last - 2 >= level - 2) {
				bar[i]->SetSize(size[i], new Timer(INDICATOR_SIGNAL_ANIM_TIME),
						ui::Widget::ANIM_SIZE, 0);
			} else {
				bar[i]->SetSize(grown[i], new Timer(INDICATOR_SIGNAL_ANIM_TIME),
						ui::Widget::ANIM_SIZE, 0);
				bar[i]->SetSize(size[i], new Timer(INDICATOR_SIGNAL_GROW_TIME),
						ui::Widget::ANIM_SIZE, 0);
			}
		} else {
			bar[i]->SetSize(off, new Timer(0.0f), ui::Widget::ANIM_SIZE, 0);
		}
		bar[i]->SetColor(color, NULL, ui::Widget::ANIM_COLOR, 0);
	}
}

/*
 * A signal event: a level (0 to 6) shows the bars, or for 0 the "no service" text once laid out;
 * no service shows the icon empty with the text, the flight mode its image, not registered the
 * mark, registered and the flight mode left take the mark and the image away and give the last
 * level back. While the status is in the flight mode or not shown, the icon is shrunk and hidden;
 * hidden, so is the operator's name (0x8116A20A).
 */
static void indicatorMobileSignalUpdate(int level, void *arg)
{
	IndicatorMobileSignal *item = (IndicatorMobileSignal *)arg;

	if (item == NULL)
		return;

	ui::Widget *widget = item->m_widget;

	if (widget == NULL || !widget->IsInherit(ui::Widget::TypeName()))
		return;

	ui::Widget *icon = item->m_widget;

	if (icon == NULL)
		return;

	ui::Plane *bars = INDICATOR_SIGNAL_FIND(icon, ui::Plane, IDParam(INDICATOR_SIGNAL_BARS));
	ui::Plane *bar1 = INDICATOR_SIGNAL_FIND(bars, ui::Plane, IDParam(INDICATOR_SIGNAL_BAR_1));
	ui::Plane *bar2 = INDICATOR_SIGNAL_FIND(bars, ui::Plane, IDParam(INDICATOR_SIGNAL_BAR_2));
	ui::Plane *bar3 = INDICATOR_SIGNAL_FIND(bars, ui::Plane, IDParam(INDICATOR_SIGNAL_BAR_3));
	ui::Plane *bar4 = INDICATOR_SIGNAL_FIND(bars, ui::Plane, IDParam(INDICATOR_SIGNAL_BAR_4));
	ui::Plane *bar5 = INDICATOR_SIGNAL_FIND(bars, ui::Plane, IDParam(INDICATOR_SIGNAL_BAR_5));
	ui::Widget *mark = INDICATOR_SIGNAL_FIND(icon, ui::Plane, IDParam(INDICATOR_SIGNAL_MARK));
	ui::Text *text = INDICATOR_SIGNAL_FIND(icon, ui::Text, IDParam(INDICATOR_SIGNAL_TEXT));

	if (bars != NULL) {
		math::v4 color = icon->GetColor();
		math::v4 size(23.0f, 20.0f, 0.0f, 0.0f);

		color.set_w(0.0f);

		IDParam texture;

		if (level < INDICATOR_SIGNAL_FLIGHT_OFF) {
			if (s_signalLevel == level && s_signalLevel != 0)
				return;
			s_signalLevel = level;
		}
		if (level == INDICATOR_SIGNAL_RESTORE)
			level = s_signalLevel;
		if (level == INDICATOR_SIGNAL_REGISTERED && s_signalUnregistered) {
			s_signalUnregistered = false;
			color.set_w(0.0f);
			icon->SetColor(color, NULL, ui::Widget::ANIM_COLOR, 0);
			icon->SetSize(size, new Timer(INDICATOR_SIGNAL_ANIM_TIME), ui::Widget::ANIM_SIZE,
				      0);
			icon->Show(0xff, NULL, NULL, 0.0f);
			mark->SetColor(color, NULL, ui::Widget::ANIM_COLOR, 0);
			bars->Show(0xff, NULL, NULL, 0.0f);
			level = s_signalLevel;
		}
		if (level == INDICATOR_SIGNAL_FLIGHT_OFF && s_signalFlight) {
			s_signalFlight = false;
			color.set_w(0.0f);
			icon->SetColor(color, NULL, ui::Widget::ANIM_COLOR, 0);
			icon->SetSize(size, new Timer(INDICATOR_SIGNAL_ANIM_TIME), ui::Widget::ANIM_SIZE,
				      0);
			icon->Show(0xff, NULL, NULL, 0.0f);
			bars->Show(0xff, NULL, NULL, 0.0f);
			level = s_signalLevel;
		}
		if (s_signalFlight)
			return;
		if (s_signalFlightMode || !s_signalStatusShown) {
			s_signalTextShown = false;
			item->m_widget->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f),
						new Timer(INDICATOR_SIGNAL_ANIM_TIME),
						ui::Widget::ANIM_SIZE, 0);
			item->m_widget->Hide(INDICATOR_SIGNAL_ANIM_TIME, 0.0f);

			ui::Plane *hidden = INDICATOR_SIGNAL_FIND(item->m_widget, ui::Plane,
								  IDParam(s_empty_name,
									  INDICATOR_SIGNAL_BARS));

			if (hidden != NULL)
				hidden->Hide(INDICATOR_SIGNAL_ANIM_TIME, 0.0f);
			return;
		}

		switch (level) {
		case 0:
			if (s_signalTextShown)
				return;
			icon->Hide(0xff, NULL, NULL, 0.0f);
			icon->SetColor(color, NULL, ui::Widget::ANIM_COLOR, 0);
			bars->Hide(0xff, NULL, NULL, 0.0f);
			bars->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f),
				      new Timer(INDICATOR_SIGNAL_ANIM_TIME), ui::Widget::ANIM_SIZE, 0);
			text->Hide(0xff, NULL, NULL, 0.0f);
			mark->Hide(0xff, NULL, NULL, 0.0f);
			text->SetAdjust(2, text->m_adjust_y, text->m_adjust_z);
			text->SetAdjust(text->m_adjust_x, 2, text->m_adjust_z);
			text->SetLayoutAttribute(graph::TextLayoutAttribute_CondenseX, false);
			text->AddEventListener(INDICATOR_SIGNAL_LAYOUT_EVENT,
					       new ui::EventCBListener(indicatorMobileSignalOnLayout,
								       item));
			text->SetString(wstring(L""));
			text->SetString(wstring(g_indicatorPlugin->GetString(
				IDParam(s_empty_name, INDICATOR_SIGNAL_NO_SERVICE_STRING))));
			return;
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
			indicatorMobileSignalSetBars(item, icon, s_signalLevel, level, color);
			icon->SetColor(color, NULL, ui::Widget::ANIM_COLOR, 0);
			icon->SetSize(size, new Timer(INDICATOR_SIGNAL_ANIM_TIME), ui::Widget::ANIM_SIZE,
				      0);
			icon->Show(0xff, NULL, NULL, 0.0f);
			bars->SetColor(color, NULL, ui::Widget::ANIM_COLOR, 0);
			bars->Show(0xff, NULL, NULL, 0.0f);
			if (s_signalUnregistered)
				color.set_w(0.5f);
			else
				color.set_w(1.0f);
			indicatorMobileSignalSetBars(item, icon, s_signalLevel, level, color);
			text->Hide(0xff, NULL, NULL, 0.0f);
			s_signalTextShown = false;
			break;
		case INDICATOR_SIGNAL_NO_SERVICE:
			icon->SetColor(color, NULL, ui::Widget::ANIM_COLOR, 0);
			icon->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f),
				      new Timer(INDICATOR_SIGNAL_ANIM_TIME), ui::Widget::ANIM_SIZE, 0);
			icon->Show(0xff, NULL, NULL, 0.0f);
			mark->Hide(0xff, NULL, NULL, 0.0f);
			bars->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f),
				      new Timer(INDICATOR_SIGNAL_ANIM_TIME), ui::Widget::ANIM_SIZE, 0);
			bars->Show(0xff, NULL, NULL, 0.0f);
			bar1->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f), NULL, ui::Widget::ANIM_SIZE, 0);
			bar2->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f), NULL, ui::Widget::ANIM_SIZE, 0);
			bar3->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f), NULL, ui::Widget::ANIM_SIZE, 0);
			bar4->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f), NULL, ui::Widget::ANIM_SIZE, 0);
			bar5->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f), NULL, ui::Widget::ANIM_SIZE, 0);
			text->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f), NULL, ui::Widget::ANIM_SIZE, 0);
			text->Hide(0xff, NULL, NULL, 0.0f);
			s_signalTextShown = false;
			text->SetAdjust(2, text->m_adjust_y, text->m_adjust_z);
			text->SetAdjust(text->m_adjust_x, 2, text->m_adjust_z);
			text->SetLayoutAttribute(graph::TextLayoutAttribute_CondenseX, false);
			text->SetString(wstring(L""));
			return;
		case INDICATOR_SIGNAL_FLIGHT_OFF:
		case INDICATOR_SIGNAL_REGISTERED:
			return;
		case INDICATOR_SIGNAL_FLIGHT_ON:
			color.set_w(1.0f);
			icon->SetColor(color, NULL, ui::Widget::ANIM_COLOR, 0);
			texture.m_idhash = INDICATOR_SIGNAL_FLIGHT_TEXTURE;
			s_signalFlight = true;
			icon->SetTexture(item->m_plugin->GetTexture(texture));
			icon->SetSize(size, new Timer(INDICATOR_SIGNAL_ANIM_TIME), ui::Widget::ANIM_SIZE,
				      0);
			icon->Show(0xff, NULL, NULL, 0.0f);
			color = math::v4(0.917f, 0.0f, 0.0f, color.extract_w());
			indicatorMobileSignalSetBars(item, icon, 0, 0, color);
			color.set_w(0.0f);
			mark->SetColor(color, NULL, ui::Widget::ANIM_COLOR, 0);
			bars->Show(0xff, NULL, NULL, 0.0f);
			text->Hide(0xff, NULL, NULL, 0.0f);
			s_signalTextShown = false;
			return;
		case INDICATOR_SIGNAL_UNREGISTERED:
			color.set_w(0.0f);
			icon->SetColor(color, NULL, ui::Widget::ANIM_COLOR, 0);
			texture.m_idhash = INDICATOR_SIGNAL_UNREGISTERED_TEXTURE;
			s_signalUnregistered = true;
			color.set_w(0.5f);
			mark->SetColor(color, NULL, ui::Widget::ANIM_COLOR, 0);
			mark->SetTexture(item->m_plugin->GetTexture(texture));
			mark->Show(0xff, NULL, NULL, 0.0f);
			icon->Show(0xff, NULL, NULL, 0.0f);
			bars->Show(0xff, NULL, NULL, 0.0f);
			indicatorMobileSignalSetBars(item, icon, s_signalLevel, s_signalLevel, color);
			text->Hide(0xff, NULL, NULL, 0.0f);
			s_signalTextShown = false;
			return;
		default:
			icon->Hide(0xff, NULL, NULL, 0.0f);
			return;
		}
	}
	if (level == INDICATOR_SIGNAL_HIDE) {
		ui::Text *name = INDICATOR_SIGNAL_FIND(item->m_root, ui::Text,
						       IDParam(s_empty_name,
							       INDICATOR_SIGNAL_OPERATOR_TEXT));

		if (name != NULL) {
			name->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f),
				      new Timer(INDICATOR_SIGNAL_ANIM_TIME), ui::Widget::ANIM_SIZE, 0);
			name->Hide(INDICATOR_SIGNAL_ANIM_TIME, 0.0f);
		}
	}
}

/*
 * The "no service" text laid out: with a level the icon is shown again and the text hidden; a
 * text wider than its room is put at the left, condensed to the room and given the string again;
 * one that fits is put in the icon's place at its size and shown, unless the status is in the
 * flight mode or not shown, which shrinks and hides the icon and its bars (0x8116BA34).
 */
static void indicatorMobileSignalOnLayout(SceInt32 type, ui::Handler *self, ui::Event *e,
					  void *data)
{
	IndicatorMobileSignal *item = (IndicatorMobileSignal *)data;
	math::v4 room(151.0f, 28.0f, 0.0f, 0.0f);
	math::v4 iconSize(23.0f, 20.0f, 0.0f, 0.0f);
	ui::Text *text = NULL;

	if (self != NULL && ((ui::Widget *)self)->IsInherit(ui::Text::TypeName()))
		text = (ui::Text *)self;

	ui::Widget *icon = NULL;

	if (item->m_widget != NULL && item->m_widget->IsInherit(ui::Widget::TypeName()))
		icon = item->m_widget;

	switch (s_signalLevel) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
		icon->SetSize(iconSize, new Timer(INDICATOR_SIGNAL_ANIM_TIME), ui::Widget::ANIM_SIZE,
			      0);
		icon->Show(0xff, NULL, NULL, 0.0f);
		text->Hide(0xff, NULL, NULL, 0.0f);
		text->DeleteEventCallback(INDICATOR_SIGNAL_LAYOUT_EVENT, NULL, NULL);
		break;
	default: {
		math::v4 size = *text->GetSize(0);

		if (size.extract_x() > room.extract_x()) {
			text->SetAdjust(0, text->m_adjust_y, text->m_adjust_z);
			text->SetLayoutAttribute(graph::TextLayoutAttribute_CondenseX, true);
			text->SetSize(room, NULL, ui::Widget::ANIM_SIZE, 0);
			text->SetString(wstring(g_indicatorPlugin->GetString(
				IDParam(s_empty_name, INDICATOR_SIGNAL_NO_SERVICE_STRING))));
		} else {
			text->DeleteEventCallback(INDICATOR_SIGNAL_LAYOUT_EVENT, NULL, NULL);
			if (!s_signalFlightMode && s_signalStatusShown) {
				icon->SetSize(size, new Timer(INDICATOR_SIGNAL_ANIM_TIME),
					      ui::Widget::ANIM_SIZE, 0);
				icon->Show(0xff, NULL, NULL, 0.0f);
				text->SetAdjust(0, text->m_adjust_y, text->m_adjust_z);
				text->SetLayoutAttribute(graph::TextLayoutAttribute_CondenseX, true);
				text->Show(0xff, NULL, NULL, 0.0f);
				s_signalTextShown = true;
			} else {
				item->m_widget->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f),
							new Timer(INDICATOR_SIGNAL_ANIM_TIME),
							ui::Widget::ANIM_SIZE, 0);
				item->m_widget->Hide(0xff, new Timer(INDICATOR_SIGNAL_ANIM_TIME), NULL,
						     0.0f);

				ui::Plane *bars = INDICATOR_SIGNAL_FIND(item->m_widget, ui::Plane,
									IDParam(s_empty_name,
										INDICATOR_SIGNAL_BARS));

				if (bars != NULL)
					bars->Hide(0xff, new Timer(INDICATOR_SIGNAL_ANIM_TIME), NULL,
						   0.0f);
			}
		}
		break;
	}
	}
}

/* the scene started: without a level yet, the icon hidden and the "no service" text set, laid
   out by indicatorMobileSignalOnLayout (0x8116C1CE) */
static void indicatorMobileSignalOnScene(SceInt32 type, ui::Handler *self, ui::Event *e,
					 void *data)
{
	IndicatorMobileSignal *item = (IndicatorMobileSignal *)data;

	if (item->m_widget == NULL || !item->m_widget->IsInherit(ui::Widget::TypeName()))
		return;

	ui::Widget *icon = item->m_widget;

	if (icon == NULL)
		return;

	ui::Text *text = INDICATOR_SIGNAL_FIND(icon, ui::Text,
					       IDParam(s_empty_name, INDICATOR_SIGNAL_TEXT));

	if (s_signalLevel == 0) {
		icon->Hide(0xff, NULL, NULL, 0.0f);
		text->SetAdjust(2, text->m_adjust_y, text->m_adjust_z);
		text->SetAdjust(text->m_adjust_x, 2, text->m_adjust_z);
		text->SetLayoutAttribute(graph::TextLayoutAttribute_CondenseX, false);
		text->AddEventListener(INDICATOR_SIGNAL_LAYOUT_EVENT,
				       new ui::EventCBListener(indicatorMobileSignalOnLayout, item));
		text->SetString(wstring(L""));
		text->SetString(wstring(g_indicatorPlugin->GetString(
			IDParam(s_empty_name, INDICATOR_SIGNAL_NO_SERVICE_STRING))));
	}
}

/* the status's flight mode or value6 changed: the signal hidden while either keeps it away, and
   the last level given back once neither does (0x8116C534) */
void IndicatorMobileSignal::OnStatus(IndicatorStatus status)
{
	if (s_signalFlightMode != (status.flightMode == 1) ||
	    s_signalStatusShown != (status.value6 == 1)) {
		s_signalFlightMode = status.flightMode == 1;
		s_signalStatusShown = status.value6 == 1;
		if (s_signalFlightMode || !s_signalStatusShown)
			indicatorMobileSignalUpdate(INDICATOR_SIGNAL_HIDE, this);
		if (!s_signalFlightMode || s_signalStatusShown)
			indicatorMobileSignalUpdate(INDICATOR_SIGNAL_RESTORE, this);
	}
}

/* the status's colors, kept off the flight mode's image (0x8116C5CC) */
void IndicatorMobileSignal::UpdateColor(IndicatorStatus status)
{
	if (!s_signalFlight)
		IndicatorItem::UpdateColor(status);
}
