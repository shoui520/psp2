/*
 * indicator_bluetooth.cpp: the bar's Bluetooth icon, shown at 19x24 while Bluetooth is on and
 * hidden at 0x0 otherwise (0x8118365A-0x81183B60). The file's and all names here are ours
 * (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <timer/timer.h>
#include <math/ngp/v4.h>
#include <widget/core/widget.h>
#include <widget/w_plane.h>
#include "indicator_item.h"

using namespace paf;

/* the item's template, plane widget and icon texture (PLACEHOLDER names) */
#define INDICATOR_BT_TEMPLATE		0x7C348572
#define INDICATOR_BT_WIDGET		0x78317AF1
#define INDICATOR_BT_TEXTURE		0xCA76BE07

/* the timer killed at destruction (0x02005100; PLACEHOLDER name) */
#define INDICATOR_BT_TIMER_ID		0x02005100

static void indicatorBluetoothUpdate(int on, IndicatorBluetooth *item);

/* (0x8118365A) */
IndicatorBluetooth::IndicatorBluetooth(Plugin *plugin, ui::Widget *widget)
	: IndicatorItem(NULL), m_root(widget), m_plugin(plugin)
{
	if (widget != NULL) {
		plugin->TemplateOpen(widget, IDParam(s_empty_name, INDICATOR_BT_TEMPLATE),
				     Plugin::TemplateOpenParam());
		m_widget = widget->FindChild(IDParam(s_empty_name, INDICATOR_BT_WIDGET), 0);
	}
}

/* (0x81183774) */
IndicatorBluetooth::~IndicatorBluetooth()
{
	if (m_widget != NULL)
		m_widget->KillTimer(INDICATOR_BT_TIMER_ID);
	if (m_job.get() != NULL) {
		m_job->Cancel();
		m_job.reset();
	}
}

/* the plane shown with the Bluetooth texture at 19x24 when on, else shrunk to 0x0 and hidden
   (0x8118381A) */
__attribute__((noinline))
static void indicatorBluetoothUpdate(int on, IndicatorBluetooth *item)
{
	if (item == NULL || item->m_widget == NULL)
		return;
	if (!item->m_widget->IsInherit(ui::Plane::TypeName()))
		return;

	ui::Plane *plane = (ui::Plane *)item->m_widget;

	if (plane == NULL)
		return;

	math::v4 size(19.0f, 24.0f, 0.0f, 0.0f);
	IDParam id(s_empty_name, 0);

	if (on == 1) {
		id.m_idhash = INDICATOR_BT_TEXTURE;
		((ui::Widget *)plane)->SetTexture(item->m_plugin->GetTexture(id));
		plane->SetSize(size, new Timer(200.0f), ui::Widget::ANIM_SIZE, 0);
		plane->Show(0xff, new Timer(200.0f), NULL, 0.0f);
	} else {
		id.m_idhash = 0;
		plane->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f), new Timer(200.0f),
			       ui::Widget::ANIM_SIZE, 0);
		plane->Hide(0xff, new Timer(200.0f), NULL, 0.0f);
	}
}

/* (0x81183B50) */
void IndicatorBluetooth::OnStatus(IndicatorStatus status)
{
	indicatorBluetoothUpdate(status.bluetooth, this);
}
