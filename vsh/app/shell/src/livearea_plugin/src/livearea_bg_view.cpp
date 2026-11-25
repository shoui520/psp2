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

#include "../../livespace_db/lsdb_keys.h"

extern paf::Plugin *g_liveareaPlugin;				/* 0x81566F94 */

extern void shellListItemBaseCtor(void *self, int itemType, paf::Plugin *plugin,
	const paf::IDParam *id1, const paf::IDParam *id2);	/* 0x8134AFF6 */
extern paf::ui::Widget *shellListItemCreateWidget(void *self, void *parent,
	const paf::IDParam *tplId, const paf::IDParam *itemId);	/* 0x8134ADF8 */
extern void shellListItemAttachWidget(void *self);		/* 0x8134AEC2 */
extern paf::ui::Widget *shellListItemGetWidget(void *self);	/* 0x8134B4F6 */

class LiveAreaBgItem : public paf::common::RefObj {
public:
	LiveAreaBgItem(void *parent, paf::ui::Widget **outWidget);
	virtual ~LiveAreaBgItem();
	virtual void HideWidget();
	virtual void OnAction();

private:
	int m_pad[3];
	paf::common::JobSlots m_slots;
};

/* 0x811B71E8 */
LiveAreaBgItem::LiveAreaBgItem(void *parent, paf::ui::Widget **outWidget)
{
	paf::IDParam id1(0u);
	paf::IDParam id2(0u);
	shellListItemBaseCtor(this, 0, g_liveareaPlugin, &id1, &id2);

	paf::ui::Widget *w = *outWidget;
	if (!w) {
		paf::IDParam tplId(2020107953u);
		paf::IDParam itemId(1158217665u);
		*outWidget = shellListItemCreateWidget(this, parent, &tplId, &itemId);
		w = *outWidget;
	}
	if (w) {
		shellListItemAttachWidget(this);
		paf::ui::Widget *target = *outWidget;
		SceUInt8 flags = *((SceUInt8 *)target + 406);
		if ((flags & 0x80) == 0) {
			*((SceUInt8 *)target + 388) = 0xFF;
			*((SceUInt8 *)target + 406) = flags | 0x80;
			typedef void (*Fn)(paf::ui::Widget *, int);
			(*(Fn *)(*(char **)target + 144))(target, 1);
		}
		ScePafWidget_FF5BC957(target, NULL, 2, 0.0f, NULL);
	}
}

/* 0x811B73F4 */
LiveAreaBgItem::~LiveAreaBgItem()
{}

/* 0x811B7424 */
void LiveAreaBgItem::HideWidget()
{
	paf::ui::Widget *w = shellListItemGetWidget(this);
	*((SceUInt8 *)w + 406) &= ~0x80u;
	*((SceUInt8 *)w + 388) = 0xFF;
	typedef void (*Fn)(paf::ui::Widget *, int);
	(*(Fn *)(*(char **)w + 148))(w, 1);
	ScePafWidget_4588699E(w, NULL, 2, 0.0f, NULL);
}

/* 0x811B746E */
void LiveAreaBgItem::OnAction()
{}

class LiveAreaBgFactory {
public:
	LiveAreaBgFactory();
	virtual ~LiveAreaBgFactory() {}
	virtual void Create(paf::common::RefObj **out, void *parent);
	virtual paf::math::v4 GetItemSize();

private:
	paf::ui::Widget *m_cachedWidget;
};

/* 0x811B7470 */
LiveAreaBgFactory::LiveAreaBgFactory()
	: m_cachedWidget(NULL)
{}

/* 0x811B7482 */
void LiveAreaBgFactory::Create(paf::common::RefObj **out, void *parent)
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
