/*
 * indicator_impose_item.cpp: the bar's volume-fixed icon, shown while impose_plugin reports the
 * volume fixed or the bar's status carries value8 (0x811831D8-0x8118365A). The file's and all
 * names here are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <timer/timer.h>
#include <math/ngp/v4.h>
#include <datetime/datetime.h>
#include <widget/core/widget.h>
#include "../../../plugins.h"
#include "../../applauncher_plugin/src/applauncher.h"
#include "indicator_item.h"

using namespace paf;

/* the item's template, widget and icon texture (PLACEHOLDER names) */
#define INDICATOR_IMPOSE_TEMPLATE	0xCC41BAC4
#define INDICATOR_IMPOSE_WIDGET		0x7E23DC95
#define INDICATOR_IMPOSE_TEXTURE	0x4C4C733A

/* the icon shown at 22x24 or hidden at 0x0 and given its texture (0x811831D8) */
__attribute__((noinline))
void IndicatorImposeItem::Update(int on)
{
	if (on == 1) {
		m_widget->Show(0xff, new Timer(200.0f), NULL, 0.0f);
		m_widget->SetSize(math::v4(22.0f, 24.0f, 0.0f, 0.0f), new Timer(200.0f),
				  ui::Widget::ANIM_SIZE, 0);
	} else {
		m_widget->Hide(0xff, new Timer(200.0f), NULL, 0.0f);
		m_widget->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f), new Timer(200.0f),
				  ui::Widget::ANIM_SIZE, 0);
	}
	m_widget->SetTexture(m_plugin->GetTexture(IDParam(s_empty_name, INDICATOR_IMPOSE_TEXTURE)));
}

/* the template opened in the widget, hidden, and updated from impose_plugin's fixed-volume
   state (0x811834BE) */
IndicatorImposeItem::IndicatorImposeItem(Plugin *plugin, ui::Widget *widget)
	: IndicatorItem(NULL), m_root(widget)
{
	if (widget != NULL) {
		m_plugin = plugin;
		plugin->TemplateOpen(widget, IDParam(s_empty_name, INDICATOR_IMPOSE_TEMPLATE),
				     Plugin::TemplateOpenParam());
		m_widget = widget->FindChild(IDParam(s_empty_name, INDICATOR_IMPOSE_WIDGET), 0);
		if (m_widget != NULL) {
			m_widget->Hide(0xff, NULL, NULL, 0.0f);

			ImposePluginInterface *iface = (ImposePluginInterface *)
				applauncherPluginGetInterface("impose_plugin", 1);
			int fixed = 0;

			if (iface != NULL)
				fixed = ((int (*)(void))iface->reserved2[6])();
			Update(fixed != 0);
		}
	}
}

/* (0x8118364E) */
void IndicatorImposeItem::OnStatus(IndicatorStatus status)
{
	Update(status.value8);
}
