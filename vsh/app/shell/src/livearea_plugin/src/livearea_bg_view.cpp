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

class LiveAreaBgItem : public ShellListItemBase {
public:
	LiveAreaBgItem(paf::ui::Widget *parent, paf::ui::Widget **outWidget);
	virtual ~LiveAreaBgItem();
	virtual void HideWidget();
	virtual void OnAction();
};

/* 0x811B71E8 */
LiveAreaBgItem::LiveAreaBgItem(paf::ui::Widget *parent, paf::ui::Widget **outWidget)
{
	paf::IDParam emptyTemplateId(0u);
	paf::IDParam emptyItemId(0u);
	shellListItemBaseCtor(this, 0, g_liveareaPlugin, &emptyTemplateId, &emptyItemId);

	paf::ui::Widget *widget = *outWidget;
	if (!widget) {
		paf::IDParam templateId(2020107953u);
		paf::IDParam itemId(1158217665u);
		*outWidget = shellListItemCreateWidget(this, parent, &templateId, &itemId);
		widget = *outWidget;
	}
	if (widget) {
		shellListItemAttachWidget(this);
		(*outWidget)->Show(0xff, NULL, NULL, 0.0f);
	}
}

/* 0x811B73F4 */
LiveAreaBgItem::~LiveAreaBgItem()
{}

/* 0x811B7424 */
void LiveAreaBgItem::HideWidget()
{
	paf::ui::Widget *widget = shellListItemGetWidget(this);
	widget->Hide(0xff, NULL, NULL, 0.0f);
}

/* 0x811B746E */
void LiveAreaBgItem::OnAction()
{}

class LiveAreaBgFactory {
public:
	LiveAreaBgFactory();
	virtual ~LiveAreaBgFactory() {}
	virtual void Create(paf::common::RefObj **out, paf::ui::Widget *parent);
	virtual paf::math::v4 GetItemSize();

private:
	paf::ui::Widget *m_cachedWidget;
};

/* 0x811B7470 */
LiveAreaBgFactory::LiveAreaBgFactory()
	: m_cachedWidget(NULL)
{}

/* 0x811B7482 */
void LiveAreaBgFactory::Create(paf::common::RefObj **out, paf::ui::Widget *parent)
{
	LiveAreaBgItem *item = new LiveAreaBgItem(parent, &m_cachedWidget);
	*out = item;
	if (item)
		item->AddRef();
}

/* 0x811B74B0 */
paf::math::v4 LiveAreaBgFactory::GetItemSize()
{
	return paf::math::v4(840.0f, 512.0f, 0.0f, 0.0f);
}

paf::ui::filelist::Visualizer *liveareaCreateBgFactory(void)
{
	return reinterpret_cast<paf::ui::filelist::Visualizer *>(new LiveAreaBgFactory());
}
