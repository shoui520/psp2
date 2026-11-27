/*
 * basic_visualizer.cpp: BasicVisualizer (paf::ui::filelist::Visualizer subclass
 * with template instantiation, shadow & checkbox layouters, and event/focus
 * forwarding, 0x8134ADD4..0x8134B9D3).
 */
#include "filelist_internal.h"

namespace filelist {

namespace {

static const char kBasicVisualizerTypeName[] = "basic_visualizer";

static const SceUInt32 kEventFocusIn = 0x2000Du;
static const SceUInt32 kEventFocusOut = 0x2000Fu;
static const SceUInt32 kEventDecide = 0x10001u;
static const SceUInt32 kEventKey = 0x10002u;
static const SceUInt32 kEventCheckBoxChanged = 0x1000000Bu;
static const float kShadowPaddingPx = 32.0f;

/* Static widget/template IDs initialized at startup (0x8134B8FE) */
static const paf::IDParam kShadowTemplateId("filelist_template_common_shadow");
static const paf::IDParam kShadowWidgetId("filelist_widget_common_shadow");
static const paf::IDParam kShadowBodyId("filelist_widget_common_shadow_body");
static const paf::IDParam kCheckBoxTemplateId("filelist_template_common_check_box");
static const paf::IDParam kCheckBoxWidgetId("filelist_widget_common_check_box");
static const paf::IDParam kCheckBoxBodyId("filelist_widget_common_check_box_body");

/* Layouter that keeps shadow body 32px wider and taller than the shadow container (0x814417A8) */
class ShadowBodyLayouter : public paf::ui::Layouter {
public:
	explicit ShadowBodyLayouter(paf::ui::Widget *body)
		: paf::ui::Layouter(NULL, NULL),
		  m_body(body)
	{
	}

	virtual ~ShadowBodyLayouter()
	{
	}

	virtual int32_t OnUpdate_core()
	{
		if (m_dirty != 0 && m_widget != NULL && m_body != NULL) {
			PafVec4 baseSize = *m_widget->GetSize(0);
			PafVec4 shadowSize(baseSize.extract_x() + kShadowPaddingPx,
					   baseSize.extract_y() + kShadowPaddingPx,
					   baseSize.extract_z(),
					   baseSize.extract_w());
			m_body->SetSize(shadowSize, NULL, 65540, 0, 0.0f, NULL, NULL);
		}
		return paf::ui::Layouter::OnUpdate_core();
	}

	paf::ui::Widget *m_body;
};

/* Layouter that delegates checkbox placement to BasicVisualizer::LayoutCheckBox (0x81441780) */
class CheckBoxLayouter : public paf::ui::Layouter {
public:
	CheckBoxLayouter(BasicVisualizer *visualizer, paf::ui::Widget *checkBox)
		: paf::ui::Layouter(NULL, NULL),
		  m_visualizer(visualizer),
		  m_checkBox(checkBox)
	{
	}

	virtual ~CheckBoxLayouter()
	{
	}

	virtual int32_t OnUpdate_core()
	{
		if (m_dirty != 0 && m_visualizer != NULL)
			m_visualizer->LayoutCheckBox(m_checkBox);
		return paf::ui::Layouter::OnUpdate_core();
	}

	BasicVisualizer *m_visualizer;
	paf::ui::Widget *m_checkBox;
};

} /* anonymous namespace */

/* 0x8134ADD4 */
bool BasicVisualizer::IsInherit(const char *type) const
{
	if (type == kBasicVisualizerTypeName)
		return true;
	return paf::ui::filelist::Visualizer::IsInherit(type);
}

/* 0x8134ADEC */
const char *BasicVisualizer::GetTypeName() const
{
	return kBasicVisualizerTypeName;
}

const char *BasicVisualizer::TypeName()
{
	return kBasicVisualizerTypeName;
}

/* 0x8134ADF8 */
paf::ui::ListItem *BasicVisualizer::CreateListItemWidget(
	paf::ui::Widget *parent,
	const paf::IDParam &templateId,
	const paf::IDParam &itemId)
{
	if (templateId.GetIDHash() == 0)
		return NULL;

	paf::Plugin::TemplateOpenParam openParam;
	m_plugin->TemplateOpen(parent, templateId, openParam);

	paf::ui::Widget *child = parent->FindChild(itemId, 0);
	if (child == NULL)
		return NULL;

	if (!child->IsInherit(paf::ui::ListItem::TypeName())) {
		child->DestroyWidget();
		return NULL;
	}

	paf::ui::ListItem *listItem = static_cast<paf::ui::ListItem *>(child);
	listItem->m_name = paf::IDParam("");
	return listItem;
}

/* 0x8134AEC2 */
void BasicVisualizer::AttachListItemEvents(paf::ui::ListItem *item)
{
	if (item == NULL)
		return;

	m_listItem = item;
	item->AddEventCallback(kEventFocusIn, FocusInHandler, this);
	item->AddEventCallback(kEventFocusOut, FocusOutHandler, this);
	item->AddEventCallback(kEventDecide, DecideHandler, this);
	item->AddEventCallback(kEventKey, KeyEventHandler, this);
}

/* 0x8134AFF6 */
BasicVisualizer::BasicVisualizer(paf::ui::Widget *parent, paf::Plugin *plugin,
				 const paf::IDParam &templateId, const paf::IDParam &itemId)
	: m_listItem(NULL),
	  m_plugin(plugin),
	  m_surfacePool(plugin != NULL ? plugin->m_surface_pool : NULL),
	  m_jobSlots()
{
	paf::ui::ListItem *item = CreateListItemWidget(parent, templateId, itemId);
	AttachListItemEvents(item);
}

BasicVisualizer::~BasicVisualizer()
{
}

} /* namespace filelist */

class ShellListItemBase;

paf::ui::Widget *shellListItemCreateWidget(ShellListItemBase *self, paf::ui::Widget *parent,
					   const paf::IDParam *templateId,
					   const paf::IDParam *itemId)
{
	return reinterpret_cast<filelist::BasicVisualizer *>(self)->CreateListItemWidget(
		parent, *templateId, *itemId);
}

void shellListItemAttachWidget(ShellListItemBase *self)
{
	filelist::BasicVisualizer *vis = reinterpret_cast<filelist::BasicVisualizer *>(self);
	vis->AttachListItemEvents(vis->m_listItem);
}

void shellListItemBaseCtor(ShellListItemBase *self, int itemType, paf::Plugin *plugin,
			   const paf::IDParam *templateId, const paf::IDParam *itemId)
{
	(void)itemType;
	filelist::BasicVisualizer *vis = reinterpret_cast<filelist::BasicVisualizer *>(self);
	vis->m_listItem = NULL;
	vis->m_plugin = plugin;
	vis->m_surfacePool = (plugin != NULL) ? plugin->m_surface_pool : NULL;
	paf::ui::ListItem *item = vis->CreateListItemWidget(NULL, *templateId, *itemId);
	vis->AttachListItemEvents(item);
}

paf::ui::Widget *shellListItemGetWidget(ShellListItemBase *self)
{
	return reinterpret_cast<filelist::BasicVisualizer *>(self)->GetListItem();
}

namespace filelist {

/* 0x8134B8F2 */
void BasicVisualizer::DestroyShadowCallback(paf::Timer *timer, paf::ui::AnimListener *listener,
					    void *widgetArg, void *userdataArg, void *extraArg)
{
	(void)timer;
	(void)widgetArg;
	(void)userdataArg;
	(void)extraArg;
	if (listener != NULL && listener->m_widget != NULL)
		listener->m_widget->DestroyWidget();
}

/* 0x8134B044 */
void BasicVisualizer::SetShadowVisible(bool visible)
{
	if (m_listItem == NULL)
		return;

	paf::ui::Widget *shadow = m_listItem->FindChild(kShadowWidgetId, 2);
	if (visible) {
		if (shadow == NULL) {
			paf::Plugin::TemplateOpenParam openParam;
			m_plugin->TemplateOpen(m_listItem, kShadowTemplateId, openParam);
			shadow = m_listItem->FindChild(kShadowWidgetId, 2);
			if (shadow == NULL)
				return;

			shadow->SetParent(shadow->m_parent, NULL, 5);
			shadow->SetMetaAlpha(0.0f, NULL, paf::ui::Widget::ANIM_META_ALPHA,
					     0, 0.0f, NULL, NULL);
			shadow->m_hit_find_mode = 1;
			shadow->m_hit_detect_mode = 3;
			shadow->m_hit_detect_b_mode = 3;

			paf::ui::Widget *shadowBody = shadow->FindChild(kShadowBodyId, 2);
			if (shadowBody != NULL) {
				shadow->SetLayouter(new ShadowBodyLayouter(shadowBody));
				PafVec4 baseSize = *shadow->GetSize(0);
				PafVec4 shadowSize(baseSize.extract_x() + kShadowPaddingPx,
						   baseSize.extract_y() + kShadowPaddingPx,
						   baseSize.extract_z(),
						   baseSize.extract_w());
				shadowBody->SetSize(shadowSize, NULL, 65540, 0, 0.0f, NULL, NULL);
			}
		}

		shadow->KillAnim(paf::ui::Widget::ANIM_META_ALPHA);
		paf::Timer *fadeInTimer = new paf::Timer(0.0f, paf::Timer::FUNC_LINEAR, -1, 0);
		shadow->SetMetaAlpha(1.0f, fadeInTimer, paf::ui::Widget::ANIM_META_ALPHA,
				     0, 0.0f, NULL, NULL);
	} else if (shadow != NULL) {
		shadow->KillAnim(paf::ui::Widget::ANIM_META_ALPHA);
		paf::Timer *fadeOutTimer = new paf::Timer(0.0f, paf::Timer::FUNC_LINEAR, -1, 0);
		shadow->SetMetaAlpha(0.0f, fadeOutTimer, paf::ui::Widget::ANIM_META_ALPHA,
				     0, 0.0f, DestroyShadowCallback, NULL);
	}
}

/* 0x8134B2C8 */
void BasicVisualizer::SetCheckBoxState(int checkState, const CheckBoxStateParam &param)
{
	if (m_listItem == NULL)
		return;

	paf::ui::Widget *container = m_listItem->FindChild(kCheckBoxWidgetId, 0);
	if (checkState == -1) {
		if (container != NULL)
			container->DestroyWidget();
		return;
	}

	if (container == NULL) {
		paf::ui::Widget *parent = param.parentWidget;
		if (parent == NULL)
			parent = m_listItem;

		const paf::IDParam &tmpl =
			(param.templateId.GetIDHash() != 0) ? param.templateId : kCheckBoxTemplateId;
		paf::Plugin::TemplateOpenParam openParam;
		m_plugin->TemplateOpen(parent, tmpl, openParam);

		container = m_listItem->FindChild(kCheckBoxWidgetId, 0);
		if (container == NULL)
			return;

		container->m_hit_detect_mode = 3;
		container->m_hit_detect_b_mode = 3;

		paf::ui::Widget *checkWidget = container->FindChild(kCheckBoxBodyId, 2);
		paf::ui::CheckBox *checkBox = NULL;
		if (checkWidget != NULL && checkWidget->IsInherit(paf::ui::CheckBox::TypeName()))
			checkBox = static_cast<paf::ui::CheckBox *>(checkWidget);

		if (checkBox != NULL)
			checkBox->AddEventCallback(kEventCheckBoxChanged,
						   CheckBoxValueChangedHandler, this);

		container->SetLayouter(new CheckBoxLayouter(this, checkBox));
		LayoutCheckBox(checkBox);
	}

	paf::ui::Widget *checkWidget = container->FindChild(kCheckBoxBodyId, 2);
	if (checkWidget != NULL && checkWidget->IsInherit(paf::ui::CheckBox::TypeName())) {
		paf::ui::CheckBox *checkBox = static_cast<paf::ui::CheckBox *>(checkWidget);
		checkBox->SetCheck(checkState == 1, 0.0f, param.animate);
	}
}

/* 0x8134B4F6 */
paf::ui::Widget *BasicVisualizer::DetachListItem()
{
	paf::ui::Widget *item = m_listItem;
	if (item != NULL) {
		CheckBoxStateParam clearParam;
		SetCheckBoxState(-1, clearParam);

		item->DeleteEventCallback(kEventFocusIn, FocusInHandler, this);
		item->DeleteEventCallback(kEventFocusOut, FocusOutHandler, this);
		item->DeleteEventCallback(kEventDecide, DecideHandler, this);
		item->DeleteEventCallback(kEventKey, KeyEventHandler, this);
		m_listItem = NULL;
	}
	return item;
}

/* 0x8134B61E */
void BasicVisualizer::LayoutCheckBox(paf::ui::Widget *checkBox)
{
	if (checkBox == NULL || checkBox->m_parent == NULL || m_listItem == NULL)
		return;

	if (checkBox->m_parent->m_parent != m_listItem) {
		checkBox->SetAdjust(0, 0, 0);
		checkBox->SetSize(*m_listItem->GetSize(0), NULL, 65540, 0, 0.0f, NULL, NULL);

		if (m_listItem->m_inverse_matrix == NULL) {
			m_listItem->m_inverse_matrix = new paf::math::matrix(
				paf::math::matrix::Identity());
			m_listItem->SetUpdateParam(2u);
		}
		if ((m_listItem->m_update_flag & 7) != 0)
			m_listItem->UpdateMatrix();

		PafVec4 origin(0.0f, 0.0f, 0.0f, 0.0f);
		PafVec4 localPos(m_listItem->m_inverse_matrix->Transform(origin.getImpl()));
		checkBox->SetPos(localPos, NULL, 0x10000, 0, 0.0f, NULL, NULL);
	}
}

/* 0x8134B75A */
void BasicVisualizer::SetIconTexture(paf::ui::Widget *widget,
				     const paf::intrusive_ptr<paf::graph::Surface> &surf,
				     int status)
{
	if (status >= 0)
		widget->SetTexture(surf, 0, 0);
	else
		widget->SetTexture(surf);
}

/* 0x8134B77A */
void BasicVisualizer::Stop()
{
	m_jobSlots.CancelAll();
	paf::ui::Widget *item = DetachListItem();
	if (item != NULL)
		item->DestroyWidget();
}

/* 0x8113B090 */
paf::ui::ListItem *BasicVisualizer::GetListItem()
{
	return m_listItem;
}

void BasicVisualizer::OnCheckChanged(bool checked)
{
	(void)checked;
}

/* 0x8134B798 */
void BasicVisualizer::FocusInHandler(int eventId, paf::ui::Handler *handler,
				     paf::ui::Event *event, void *userdata)
{
	static_cast<BasicVisualizer *>(userdata)->OnFocusIn(eventId, handler, event);
}

/* 0x8134B7AC */
void BasicVisualizer::OnFocusIn(int eventId, paf::ui::Handler *handler, paf::ui::Event *event)
{
	(void)eventId;
	(void)handler;
	(void)event;
	SetShadowVisible(true);
}

/* 0x8134B7B6 */
void BasicVisualizer::FocusOutHandler(int eventId, paf::ui::Handler *handler,
				      paf::ui::Event *event, void *userdata)
{
	static_cast<BasicVisualizer *>(userdata)->OnFocusOut(eventId, handler, event);
}

/* 0x8134B7CA */
void BasicVisualizer::OnFocusOut(int eventId, paf::ui::Handler *handler, paf::ui::Event *event)
{
	(void)eventId;
	(void)handler;
	(void)event;
	SetShadowVisible(false);
}

/* 0x8134B7D4 */
void BasicVisualizer::DecideHandler(int eventId, paf::ui::Handler *handler,
				    paf::ui::Event *event, void *userdata)
{
	static_cast<BasicVisualizer *>(userdata)->OnDecide(eventId, handler, event);
}

/* 0x8134B7E8 */
void BasicVisualizer::OnDecide(int eventId, paf::ui::Handler *handler, paf::ui::Event *event)
{
	(void)eventId;
	(void)handler;
	if (event == NULL || (event->m_type & paf::ui::Event::ROUTE_TERM) == 0)
		return;

	paf::ui::Widget::FocusSearchInfo searchInfo(255);
	paf::ui::Widget *target = m_listItem->SetFocusOption(&searchInfo, 1);
	if (target != NULL) {
		paf::ui::Event forwarded(*event);
		forwarded.m_target = target;
		forwarded.m_type = (forwarded.m_type & 0xFF00FFFFu) |
				   paf::ui::Event::MODE_DISPATCH |
				   paf::ui::Event::ROUTE_TERM;
		m_listItem->m_env->m_ev_queue.PushFront(forwarded);
		event->Consume();
	}
}

/* 0x8134B8CA */
void BasicVisualizer::KeyEventHandler(int eventId, paf::ui::Handler *handler,
				      paf::ui::Event *event, void *userdata)
{
	static_cast<BasicVisualizer *>(userdata)->OnKeyEvent(eventId, handler, event);
}

void BasicVisualizer::OnKeyEvent(int eventId, paf::ui::Handler *handler, paf::ui::Event *event)
{
	OnDecide(eventId, handler, event);
}

/* 0x8134B8E0 */
void BasicVisualizer::CheckBoxValueChangedHandler(int eventId, paf::ui::Handler *handler,
						  paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)event;
	paf::ui::CheckBox *checkBox = static_cast<paf::ui::CheckBox *>(handler);
	static_cast<BasicVisualizer *>(userdata)->OnCheckChanged(checkBox->IsChecked());
}

} /* namespace filelist */
