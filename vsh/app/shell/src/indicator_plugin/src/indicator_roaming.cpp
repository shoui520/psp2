/*
 * indicator_roaming.cpp: the roaming icon on the information bar, shown while the telephony
 * listener tells the phone is roaming (0x81168DF0-0x81169288). The file's and the names are ours
 * (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <widget/core/widget.h>
#include <widget/w_plane.h>
#include "indicator_item.h"

using namespace paf;

/* the icon's template and the icon in it (PLACEHOLDER names) */
#define INDICATOR_ROAMING_TEMPLATE	0xDA261734
#define INDICATOR_ROAMING_ICON		0x256AB7E3

/* the roaming's value while roaming */
#define INDICATOR_ROAMING_ON		2

/* the icon's animations' time, in milliseconds */
#define INDICATOR_ROAMING_ANIM_TIME	200.0f

static void indicatorRoamingUpdate(int roaming, void *arg);

/* the icon's template opened in the widget and the icon found in it, the item told the
   roaming (0x81168DF0) */
IndicatorRoaming::IndicatorRoaming(Plugin *plugin, ui::Widget *widget)
	: m_root(widget), m_plugin(plugin)
{
	if (widget != NULL) {
		{
			plugin->TemplateOpen(widget,
					     IDParam(s_empty_name, INDICATOR_ROAMING_TEMPLATE),
					     Plugin::TemplateOpenParam());
		}
		m_widget = widget->FindChild(IDParam(s_empty_name, INDICATOR_ROAMING_ICON), 0);
		if (m_widget != NULL) {
			g_indicatorRoamingCB = indicatorRoamingUpdate;
			g_indicatorRoamingArg = this;
		}
	}
}

/* (0x81168F30) */
IndicatorRoaming::~IndicatorRoaming()
{
	g_indicatorRoamingCB = NULL;
	g_indicatorRoamingArg = NULL;
}

/* the roaming told: the icon grown to its size and shown while roaming, else shrunk and hidden
   (0x81168F7E) */
static void indicatorRoamingUpdate(int roaming, void *arg)
{
	IndicatorRoaming *item = (IndicatorRoaming *)arg;

	if (item == NULL)
		return;

	ui::Plane *icon = item->m_widget != NULL &&
			  item->m_widget->IsInherit(ui::Plane::TypeName()) ?
			  (ui::Plane *)item->m_widget : NULL;

	if (icon == NULL)
		return;

	math::v4 size(18.0f, 22.0f, 0.0f, 0.0f);
	IDParam image(s_empty_name, 0);

	if (roaming == INDICATOR_ROAMING_ON) {
		icon->SetSize(size, new Timer(INDICATOR_ROAMING_ANIM_TIME), ui::Widget::ANIM_SIZE, 0);
		icon->Show(0xff, new Timer(INDICATOR_ROAMING_ANIM_TIME), NULL, 0.0f);
	} else {
		icon->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f),
			      new Timer(INDICATOR_ROAMING_ANIM_TIME), ui::Widget::ANIM_SIZE, 0);
		icon->Hide(0xff, new Timer(INDICATOR_ROAMING_ANIM_TIME), NULL, 0.0f);
	}
}
