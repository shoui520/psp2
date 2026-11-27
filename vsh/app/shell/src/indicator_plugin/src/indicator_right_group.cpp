/*
 * indicator_right_group.cpp: the bar's right group of items, laid out from the right on its
 * widget (0x8116D398-0x8116D76A). The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/list>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <widget/core/widget.h>
#include <widget/core/layouter.h>
#include "../../impose_plugin/src/impose.h"
#include "indicator_item.h"

using namespace paf;

/* the items' animations' time, in milliseconds */
#define INDICATOR_RIGHT_GROUP_ANIM_TIME	200.0f

/* the room between two items */
#define INDICATOR_RIGHT_GROUP_SPACING	2.0f

static int indicatorRightGroupOnLayout(ui::Layouter *layouter, ui::Widget *widget, void *layoutArg,
				       void *userData, void *context);

/* the group's items made in its widget with the indicator's plugin, laid out by
   indicatorRightGroupOnLayout: the timed icon but on the Dolce, the started item, the impose
   item and the notices' icon (0x8116D398) */
IndicatorRightGroup::IndicatorRightGroup(Plugin *plugin, ui::Widget *widget)
	: IndicatorItem(widget), m_root(widget)
{
	if (widget == NULL)
		return;
	widget->SetLayouter(new ui::Layouter(indicatorRightGroupOnLayout, NULL));

	if (!imposeIsDolce()) {
		ui::Widget *parent = m_widget;
		Plugin *owner = g_indicatorPlugin;
		IndicatorItem *item = new IndicatorBattery(owner, parent);

		if (item != NULL)
			m_items.push_back(item);
	}

	ui::Widget *parent = m_widget;
	Plugin *owner = g_indicatorPlugin;
	IndicatorClock *clock = new IndicatorClock(owner, parent);

	if (clock != NULL) {
		clock->Start();
		m_items.push_back(clock);
	}

	parent = m_widget;
	owner = g_indicatorPlugin;

	IndicatorItem *item = new IndicatorImposeItem(owner, parent);

	if (item != NULL)
		m_items.push_back(item);
	parent = m_widget;
	owner = g_indicatorPlugin;
	item = new IndicatorNoticeIcon(owner, parent);
	if (item != NULL)
		m_items.push_back(item);
}

/* its items deleted, the first one first (0x8116D580) */
IndicatorRightGroup::~IndicatorRightGroup()
{
	while (m_items.size() != 0) {
		IndicatorItem *item = m_items[0];

		m_items.erase(m_items.begin(), m_items.begin() + 1);
		if (item != NULL)
			delete item;
	}
}

/* the group's widgets put one before the other from the right, 2 apart (0x8116D618) */
static int indicatorRightGroupOnLayout(ui::Layouter *layouter, ui::Widget *widget, void *layoutArg,
				       void *userData, void *context)
{
	if (widget != NULL && layouter->IsDirty(0xff)) {
		float x = 0.0f;

		for (list<ui::Widget *>::iterator it = widget->m_children.begin();
		     it != widget->m_children.end(); ++it) {
			ui::Widget *child = *it;
			math::v4 size = *child->GetSize(1);
			math::v4 pos = child->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

			pos.set_x(x - size.extract_x() * 0.5f);
			child->SetPos(pos, new Timer(INDICATOR_RIGHT_GROUP_ANIM_TIME),
				      ui::Widget::ANIM_MOVE, 0);
			x = x - (size.extract_x() + INDICATOR_RIGHT_GROUP_SPACING);
		}
	}
	return 0;
}

/* each item colored and told the status (folded by Sony's linker with the left group's,
   0x8116011A) */
void IndicatorRightGroup::OnStatus(IndicatorStatus status)
{
	for (vector<IndicatorItem *>::iterator it = m_items.begin(); it != m_items.end(); ++it) {
		(*it)->UpdateColor(status);
		(*it)->OnStatus(status);
	}
}
