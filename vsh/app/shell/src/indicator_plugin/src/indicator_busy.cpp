/*
 * indicator_busy.cpp: the bar's busy indicator, started while the status's value4 is 1 and
 * stopped otherwise (0x81183B60-0x81183D68). The file's and all names here are ours
 * (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <widget/core/widget.h>
#include <widget/w_busyindicator.h>
#include "indicator_item.h"

using namespace paf;

/* the item's template and busy indicator widget (PLACEHOLDER names) */
#define INDICATOR_BUSY_TEMPLATE		0xA128AB4D
#define INDICATOR_BUSY_WIDGET		0x814605C6

/* (0x81183B60) */
IndicatorBusy::IndicatorBusy(Plugin *plugin, ui::Widget *widget)
	: IndicatorItem(NULL), m_root(widget), m_plugin(plugin)
{
	if (widget != NULL) {
		bool opened = plugin->TemplateOpen(widget, IDParam(s_empty_name, INDICATOR_BUSY_TEMPLATE),
						   Plugin::TemplateOpenParam()) == 0;

		if (opened)
			m_widget = widget->FindChild(IDParam(s_empty_name, INDICATOR_BUSY_WIDGET), 0);
	}
}

/* (0x81183C84) */
__attribute__((noinline))
int IndicatorBusy::Stop()
{
	int ret = -1;

	if (m_widget != NULL && m_widget->IsInherit(ui::BusyIndicator::TypeName())) {
		ui::BusyIndicator *busy = (ui::BusyIndicator *)m_widget;

		if (busy != NULL) {
			switch (busy->m_anim_state) {
			case ui::BusyIndicator::ANIM_STOPPED:
			case ui::BusyIndicator::ANIM_STOPPING:
				break;
			case ui::BusyIndicator::ANIM_STARTING:
			case ui::BusyIndicator::ANIM_RUNNING:
				busy->Stop();
				break;
			default:
				break;
			}
			ret = 0;
		}
	}
	return ret;
}

/* (0x81183CC2) */
IndicatorBusy::~IndicatorBusy()
{
	Stop();
}

/* (0x81183D16) */
__attribute__((noinline))
int IndicatorBusy::Start()
{
	int ret = -1;

	if (m_widget != NULL && m_widget->IsInherit(ui::BusyIndicator::TypeName())) {
		ui::BusyIndicator *busy = (ui::BusyIndicator *)m_widget;

		if (busy != NULL) {
			switch (busy->m_anim_state) {
			case ui::BusyIndicator::ANIM_STOPPED:
			case ui::BusyIndicator::ANIM_STOPPING:
				busy->Start();
				break;
			default:
				break;
			}
			ret = 0;
		}
	}
	return ret;
}

/* (0x81183D54) */
void IndicatorBusy::OnStatus(IndicatorStatus status)
{
	if (status.value4 == 1)
		Start();
	else
		Stop();
}
