/*
 * SceShell livearea_plugin: LiveArea background list item & factory
 * 3.65-CEX: 0x811B71E8 - 0x811B7538 (sti 0x811B74C8)
 */

#include <stddef.h>
#include <scetypes.h>
#include <common/job_slots.h>
#include <common/ref_obj.h>
#include <math/math.h>
#include <plugin.h>
#include <widget/core/widget.h>
#include "livearea_internal.h"

class LiveAreaBgItem : public filelist::BasicVisualizer {
public:
	LiveAreaBgItem(paf::ui::Widget *parent, paf::ui::ListItem **outWidget);
	virtual ~LiveAreaBgItem();
	virtual void Stop();
};

/* 0x811B71E8 */
LiveAreaBgItem::LiveAreaBgItem(paf::ui::Widget *parent, paf::ui::ListItem **outWidget)
	: filelist::BasicVisualizer(parent, g_liveareaPlugin, paf::IDParam(0u), paf::IDParam(0u))
{
	paf::ui::ListItem *widget = *outWidget;
	if (!widget) {
		paf::IDParam templateId(2020107953u);
		paf::IDParam itemId(1158217665u);
		*outWidget = CreateListItemWidget(parent, templateId, itemId);
		widget = *outWidget;
	}
	if (widget) {
		AttachListItemEvents(widget);
		widget->Show(0xff, NULL, NULL, 0.0f);
	}
}

/* 0x811B73F4 */
LiveAreaBgItem::~LiveAreaBgItem()
{
}

/* 0x811B7424 */
void LiveAreaBgItem::Stop()
{
	paf::ui::Widget *widget = DetachListItem();
	if (widget != NULL)
		widget->Hide(0xff, NULL, NULL, 0.0f);
}

class LiveAreaBgFactory : public filelist::NullVisualizerFactory {
public:
	LiveAreaBgFactory();
	virtual ~LiveAreaBgFactory() {}
	virtual paf::intrusive_ptr<paf::ui::filelist::Visualizer> CreateVisualizer(
		paf::ui::Widget *parent, int index);
	virtual paf::math::v4 GetItemSize();

private:
	paf::ui::ListItem *m_cachedWidget;
};

/* 0x811B7470 */
LiveAreaBgFactory::LiveAreaBgFactory()
	: m_cachedWidget(NULL)
{
}

/* 0x811B7482 */
paf::intrusive_ptr<paf::ui::filelist::Visualizer> LiveAreaBgFactory::CreateVisualizer(
	paf::ui::Widget *parent, int index)
{
	(void)index;
	paf::intrusive_ptr<paf::ui::filelist::Visualizer> item(
		new LiveAreaBgItem(parent, &m_cachedWidget));
	return item;
}

/* 0x811B74B0 */
paf::math::v4 LiveAreaBgFactory::GetItemSize()
{
	return paf::math::v4(840.0f, 512.0f, 0.0f, 0.0f);
}

filelist::NullVisualizerFactory *liveareaCreateBgFactory(void)
{
	return new LiveAreaBgFactory();
}
