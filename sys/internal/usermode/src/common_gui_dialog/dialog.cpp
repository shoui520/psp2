/*
 * The dialog object behind every dialog the library opens: its page in the plugin, the
 * paf::ui::Dialog on it and the plane that dims the screen behind it, the handler its buttons
 * call, and the list the exports find it in by its ID. Also the PlayStation Plus and list dialogs,
 * which build on the layout dialog. No binary names this file, its classes or their members; the
 * names are ours.
 */
#include <stdint.h>
#include <std/new>
#include <std/math.h>
#include <std/stdio.h>
#include <appmgr.h>
#include "layouts.h"
#include "dialog.h"

using namespace paf;
using namespace paf::ui;

/* the widget of the widget's descendants with the ID, when it is of the type */
#define COMMON_GUI_DIALOG_FIND(widget, type, id)						\
	((widget)->FindChild(IDParam((uint32_t)(id)), 0) != NULL &&				\
	 (widget)->FindChild(IDParam((uint32_t)(id)), 0)->IsInherit(type::TypeName()) ?	\
	 (type *)(widget)->FindChild(IDParam((uint32_t)(id)), 0) : NULL)

/* the PlayStation Plus dialog's text, and the hint locale's language */
#define PLUS_DIALOG_TEXT		0xC85BE980
#define PLUS_DIALOG_LOCALE_ENLARGED	13

/* the PlayStation Store's PlayStation Plus page, and the page that also tries it out */
static const char s_plusUri[] = "psts:browse?target=plus";
static const char s_plusTrialUri[] = "psts:browse?target=plus&exp=true";

/* the PlayStation Plus dialog's buttons */
#define PLUS_DIALOG_BUTTON_CLOSE	0
#define PLUS_DIALOG_BUTTON_CANCEL	3
#define PLUS_DIALOG_BUTTON_JOIN		7
#define PLUS_DIALOG_BUTTON_TRY		8

/* the parts of a list dialog */
#define LIST_DIALOG_PART_BOX		4
#define LIST_DIALOG_PART_VIEW		9
#define LIST_DIALOG_PART_LINES		10
#define LIST_DIALOG_PART_ENLARGED	12

/* a list dialog's line template and its parts, and its button's */
#define LIST_DIALOG_LINE		0xDD8302BA
#define LIST_DIALOG_LINE_MARK		0x0A1F2AE1
#define LIST_DIALOG_LINE_TEXT		0x57C8E2C6
#define LIST_DIALOG_BUTTON		0x3B8CD3EA
#define LIST_DIALOG_BUTTON_WIDGET	0xE5D9C984
#define LIST_DIALOG_BUTTON_STYLE	0x20413274

/* the dialog's texts whose adjustment the fitted list dialog changes */
#define DIALOG_HEADING			0x768E69AA
#define DIALOG_CONTENTS			0x918215F3
#define DIALOG_VIEW			0xE9C61C2C

/* the event a button sends when it is pressed */
#define DIALOG_BUTTON_EVENT		0x10000008

/* the height a fitted list dialog opens with, and the most it grows to */
#define LIST_DIALOG_FIT_HEIGHT		240.0f
#define LIST_DIALOG_FIT_HEIGHT_MAX	440.0f

/* shows a widget over the timer's time (inline in paf's callers) */
static inline void ShowWidget(Widget *widget, Timer *timer)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xff;
		widget->m_transition_complete = true;
		widget->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(widget, timer, 2, 0.0f, NULL);
}

/*
 * The PlayStation Plus dialog's handler: its join and try buttons open the store's page once the
 * dialog has closed, and every button is passed on to the caller's handler.
 */
class PlusDialogHandler : public CommonGuiDialogHandler {
public:
	PlusDialogHandler(CommonGuiDialogHandler *handler) : m_uri(NULL), m_handler(handler)
	{
	}

	virtual ~PlusDialogHandler()
	{
		if (m_handler != NULL)
			delete m_handler;
	}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		if (button == PLUS_DIALOG_BUTTON_JOIN) {
			m_uri = s_plusUri;
			SceCommonGuiDialog_BC311DC0(value);
		} else if (button == PLUS_DIALOG_BUTTON_TRY) {
			m_uri = s_plusTrialUri;
			SceCommonGuiDialog_BC311DC0(value);
		} else if (button == PLUS_DIALOG_BUTTON_CANCEL) {
			SceCommonGuiDialog_BC311DC0(value);
		} else if (button == PLUS_DIALOG_BUTTON_CLOSE) {
			if (m_uri != NULL)
				sceAppMgrLaunchAppByUri(0x60000, m_uri);
		}
		if (m_handler != NULL)
			(*m_handler)(value, button);
	}

	const char *m_uri;
	/* never set */
	uint32_t m_reserved;
	CommonGuiDialogHandler *m_handler;
};

int SceCommonGuiDialog_D6B6809B(const CommonGuiPlusDialogParam *param)
{
	CommonGuiLayoutDialogParam layoutParam;
	int id;

	CopyDialogParam(&layoutParam, param);
	layoutParam.text = layoutParam.resourcePlugin->GetString(IDParam((uint32_t)PLUS_DIALOG_TEXT));
	layoutParam.layout = s_layout04D71470;
	layoutParam.notifyClose = true;
	layoutParam.handler = new PlusDialogHandler(param->handler);
	id = SceCommonGuiDialog_CCFCD9A6(&layoutParam);
	if (id >= 0 && layoutParam.resourcePlugin->m_locale == PLUS_DIALOG_LOCALE_ENLARGED) {
		Widget *part = SceCommonGuiDialog_8947FC23(id, LIST_DIALOG_PART_ENLARGED);

		if (part != NULL) {
			graph::DrawObj *obj = part->GetDrawObj(1);

			if (obj != NULL && obj->IsInherit(graph::TextObj::DrawObjType()))
				((graph::TextObj *)obj)->SetStyleAttribute(
					graph::TextStyleAttribute_EnlargeFontSize, 0, 0, false);
		}
	}
	return id;
}

/* the dialog's heading, contents and view adjusted on the y axis as ay */
static void SetDialogAdjustY(Widget *widget, uint32_t ay)
{
	Widget *child;

	child = widget->FindChild(IDParam((uint32_t)DIALOG_HEADING), 0);
	if (child != NULL)
		child->SetAdjust(child->m_adjust_x, ay, child->m_adjust_z);
	child = widget->FindChild(IDParam((uint32_t)DIALOG_CONTENTS), 0);
	if (child != NULL)
		child->SetAdjust(child->m_adjust_x, ay, child->m_adjust_z);
	child = widget->FindChild(IDParam((uint32_t)DIALOG_VIEW), 0);
	if (child != NULL)
		child->SetAdjust(child->m_adjust_x, ay, child->m_adjust_z);
}

/* a list line's button: the dialog closes with the line's value */
class ListButtonListener : public EventListener {
public:
	ListButtonListener(int32_t id, int32_t value) : m_id(id), m_value(value)
	{
	}

	virtual int32_t Do(int32_t type, Handler *self, Event *e);

	int32_t m_id;
	int32_t m_value;
};

/* a button made ready: its label kept to one line, smaller where it has more, and shown */
static inline int32_t FitButtonLabel(Widget *button)
{
	graph::DrawObj *obj = button->GetDrawObj(1);
	graph::TextObj *text;
	math::v4 color;
	float point;
	uint32_t lines;

	if (obj == NULL || !obj->IsInherit(graph::TextObj::DrawObjType()))
		return 0;
	text = (graph::TextObj *)obj;
	text->GetStyleAttribute(graph::TextStyleAttribute_Point, 0, point);
	if (point == 22.0f) {
		color = text->GetColor();
		color.set_w(1.0f);
		text->SetColor(color);
		return 0;
	}
	if (text->GetLineCount(lines) != 0)
		return 0;
	if (lines > 1) {
		text->SetStyleAttribute(graph::TextStyleAttribute_Point, 0, 0, 22.0f);
	} else {
		color = text->GetColor();
		color.set_w(1.0f);
		text->SetColor(color);
	}
	return 0;
}

/* a list line's button made ready */
class ListButtonReadyListener : public EventListener {
public:
	virtual int32_t Do(int32_t type, Handler *self, Event *e)
	{
		return FitButtonLabel((Widget *)self);
	}
};

/* the button of a list line, opened from its template under the line */
static void AddListButton(int32_t id, Plugin *plugin, Widget *line,
			  const CommonGuiListDialogItem *item)
{
	Widget *button;
	WidgetSoundParam::Sound *sounds;
	uint32_t num;
	int32_t ret;

	if (plugin->TemplateOpen(line, IDParam((uint32_t)LIST_DIALOG_BUTTON),
				 Plugin::TemplateOpenParam()) != 0)
		return;
	button = line->FindChild(IDParam((uint32_t)LIST_DIALOG_BUTTON_WIDGET), 0);
	if (button == NULL)
		return;
	if (!(item->buttonColor == SceCommonGuiDialog_EF2C5382)) {
		graph::DrawObj *obj = button->GetDrawObj(1);

		if (obj != NULL)
			obj->SetColor(item->buttonColor);
	}
	button->SetString(item->buttonLabel);
	button->AddEventListener(DIALOG_BUTTON_EVENT,
				 new ListButtonListener(id, item->buttonValue));
	button->AddEventListener(Handler::CB_STATE_READY, new ListButtonReadyListener());
	ret = ScePafToplevel_DA3D4AF0(plugin, IDParam((uint32_t)LIST_DIALOG_BUTTON_STYLE), &sounds,
				      &num);
	if (ret == 0) {
		button->m_sounds = sounds;
		button->m_sound_num = num;
	}
}

/* the list's view laid out: its height made even, and its scroll bar shown again */
class ListViewListener : public EventListener {
public:
	virtual int32_t Do(int32_t type, Handler *self, Event *e);
};

/* the lines laid out: the view's focus taken */
class ListLinesListener : public EventListener {
public:
	ListLinesListener(Widget *view) : m_view(view)
	{
	}

	virtual int32_t Do(int32_t type, Handler *self, Event *e);

	Widget *m_view;
};

/* the lines of a list dialog, each opened from its template */
static void AddListLines(int32_t id, const CommonGuiListDialogParam *param)
{
	Widget *view = SceCommonGuiDialog_8947FC23(id, LIST_DIALOG_PART_VIEW);
	Widget *lines;

	if (view != NULL)
		view->AddEventListener(Handler::CB_STATE_READY, new ListViewListener());
	lines = SceCommonGuiDialog_8947FC23(id, LIST_DIALOG_PART_LINES);
	if (lines == NULL)
		return;
	for (uint32_t i = 0; i < (uint32_t)param->items.size(); i++) {
		const CommonGuiListDialogItem *item = &param->items[i];
		int32_t index = lines->m_children.size();
		Widget *line;
		Text *text;

		if (param->resourcePlugin->TemplateOpen(lines, IDParam((uint32_t)LIST_DIALOG_LINE),
							Plugin::TemplateOpenParam()) != 0)
			continue;
		line = lines->GetChild(index);
		text = COMMON_GUI_DIALOG_FIND(line, Text, LIST_DIALOG_LINE_MARK);
		if (text != NULL) {
			if (item->mark) {
				if (!(item->markColor == SceCommonGuiDialog_EF2C5382))
					text->SetColor(item->markColor);
				if (item->shadow)
					text->SetStyleAttribute(graph::TextStyleAttribute_Shadow, 0, 0,
								true);
			} else {
				text->DestroyWidget();
			}
		}
		text = COMMON_GUI_DIALOG_FIND(line, Text, LIST_DIALOG_LINE_TEXT);
		if (text != NULL) {
			if (!(item->textColor == SceCommonGuiDialog_EF2C5382))
				text->SetColor(item->textColor);
			text->SetString(item->text);
			if (item->shadow)
				text->SetStyleAttribute(graph::TextStyleAttribute_Shadow, 0, 0, true);
			if (!item->mark && param->items.size() == 1)
				text->SetLayoutAttribute(graph::TextLayoutAttribute_AlignX, 1);
		}
		if (item->buttonLabel.length() != 0)
			AddListButton(id, param->resourcePlugin, line, item);
	}
	lines->AddEventListener(Handler::CB_STATE_READY, new ListLinesListener(view));
}

/* the fitted list dialog's box laid out: made taller while its view's contents do not fit,
   then shown */
class ListBoxListener : public EventListener {
public:
	ListBoxListener(int32_t id) : m_id(id)
	{
	}

	virtual int32_t Do(int32_t type, Handler *self, Event *e)
	{
		Widget *box = SceCommonGuiDialog_8947FC23(m_id, LIST_DIALOG_PART_BOX);
		Widget *part;
		ScrollViewBase *view;
		math::v4 size;
		math::v4 viewSize;
		math::v2 contentSize;

		if (box == NULL)
			return 0;
		size = *box->GetSize(0);
		part = SceCommonGuiDialog_8947FC23(m_id, LIST_DIALOG_PART_VIEW);
		view = part != NULL && part->IsInherit(ScrollViewBase::TypeName()) ?
			(ScrollViewBase *)part : NULL;
		if (view == NULL) {
			ShowWidget(box, NULL);
			return 0;
		}
		viewSize = *view->GetSize(0);
		contentSize = view->m_view_box_size;
		if (size.extract_y() == LIST_DIALOG_FIT_HEIGHT) {
			if (viewSize.extract_y() >= contentSize.extract_y()) {
				ShowWidget(box, NULL);
				return 0;
			}
			size.set_y(LIST_DIALOG_FIT_HEIGHT_MAX);
			box->SetSize(size);
		} else if (size.extract_y() == LIST_DIALOG_FIT_HEIGHT_MAX) {
			float height = view->m_view_box_size.extract_y();

			if (viewSize.extract_y() <= contentSize.extract_y()) {
				ShowWidget(box, NULL);
				return 0;
			}
			box->SetAdjust(box->m_adjust_x, 2, box->m_adjust_z);
			SetDialogAdjustY(box, 2);
			viewSize.set_y((float)(((int32_t)sce_paf_ceilf(height) + 1) / 2 * 2));
			view->SetSize(viewSize);
		} else {
			ShowWidget(box, NULL);
		}
		return 0;
	}

	int32_t m_id;
};

/* a list dialog's handler: the caller's, or the dialog's result where the call waits */
class ListDialogHandler : public CommonGuiDialogHandler {
public:
	ListDialogHandler(bool wait, CommonGuiDialogHandler *handler)
		: m_wait(wait), m_handler(handler)
	{
	}

	virtual ~ListDialogHandler();
	virtual void operator()(SceInt32 value, SceInt32 button);

	bool m_wait;
	CommonGuiDialogHandler *m_handler;
};

int SceCommonGuiDialog_F3D37CAE(const CommonGuiListDialogParam *param)
{
	CommonGuiLayoutDialogParam layoutParam;
	int id;

	CopyDialogParam(&layoutParam, param);
	layoutParam.heading = param->heading;
	switch (param->type) {
	case 1:
		layoutParam.layout = s_layout1424609E;
		break;
	case 2:
		layoutParam.layout = s_contentsLayoutNoButtons;
		break;
	case 3:
		layoutParam.layout = s_layout3AC06BC6;
		break;
	default:
		layoutParam.layout = s_layoutE872FB5D;
		break;
	}
	layoutParam.textColor = param->headingColor;
	layoutParam.handler = new ListDialogHandler(param->wait, param->handler);
	id = SceCommonGuiDialog_CCFCD9A6(&layoutParam);
	if (id < 0)
		return id;
	if (param->outId != NULL)
		*param->outId = id;
	AddListLines(id, param);
	if (param->fit) {
		Widget *part = SceCommonGuiDialog_8947FC23(id, LIST_DIALOG_PART_BOX);
		Box *box = part != NULL && part->IsInherit(Box::TypeName()) ? (Box *)part : NULL;

		if (box != NULL) {
			math::v4 size;

			box->m_auto_show = false;
			box->SetChildrenAdjustY(Box::CHILDREN_ADJUST_NONE, 0.0f);
			SetDialogAdjustY(box, 1);
			size = *box->GetSize(0);
			size.set_y(LIST_DIALOG_FIT_HEIGHT);
			box->SetSize(size);
			box->AddEventListener(Handler::CB_STATE_READY, new ListBoxListener(id));
		}
	}
	if (param->wait)
		id = SceCommonGuiDialog_D2381B88(id);
	return id;
}

int32_t ListViewListener::Do(int32_t type, Handler *self, Event *e)
{
	ScrollViewBase *view = (ScrollViewBase *)self;

	if (view->m_adjust_y == 2) {
		math::v4 size = *view->GetSize(0);

		size.set_y((float)(((int32_t)sce_paf_ceilf(view->m_view_box_size.extract_y()) + 1) / 2 * 2));
		view->SetSize(size);
	}
	view->ResetBarShown();
	return 0;
}

int32_t ListButtonListener::Do(int32_t type, Handler *self, Event *e)
{
	SceCommonGuiDialog_01702008(m_id, m_value);
	return 0;
}

int32_t ListLinesListener::Do(int32_t type, Handler *self, Event *e)
{
	m_view->ChangeState(3, -1);
	return 0;
}

ListDialogHandler::~ListDialogHandler()
{
	if (m_handler != NULL)
		delete m_handler;
}

void ListDialogHandler::operator()(SceInt32 value, SceInt32 button)
{
	if (m_wait)
		SceCommonGuiDialog_2CA16BBF(value, button);
	else if (m_handler != NULL)
		(*m_handler)(value, button);
	SceCommonGuiDialog_BC311DC0(value);
}

CommonGuiDialog *CommonGuiDialog::Find(int32_t id)
{
	list<CommonGuiDialog *>::iterator it;

	if (id < 0)
		return NULL;
	for (it = s_dialogs.begin(); it != s_dialogs.end(); ++it) {
		if ((*it)->m_id == id)
			return *it;
	}
	return NULL;
}

void CommonGuiDialog::OnTerm()
{
	inputdevice::touchscreen::Unlock(0);
	if (m_waiting) {
		if (m_done != NULL)
			*m_done = true;
		return;
	}
	if (m_notifyClose && m_handler != NULL) {
		m_inHandler = true;
		(*m_handler)(m_id, 0);
		m_inHandler = false;
	}
	delete this;
}

void CommonGuiDialog::Close()
{
	int32_t ret;

	if (!m_open || m_refCount != 0 || m_inHandler)
		return;
	inputdevice::touchscreen::Lock(0);
	ret = m_dialog->AddEventListener(Handler::CB_STATE_TERM,
					 new MemberListener<CommonGuiDialog>(this,
									     &CommonGuiDialog::OnTerm));
	if (ret == 0) {
		Widget *contents;
		float time;

		ScePafWidget_DB993F51(&m_waitVblank);
		time = ScePafCommon_B48C9BDC(0.0f, m_dialog, common::transition::Type_Popup1, false,
					     false);
		contents = m_dialog->FindChild(IDParam((uint32_t)DIALOG_CONTENTS), 0);
		if (contents != NULL)
			contents->SetMetaAlpha(0.0f, new Timer(time * 0.7f, Timer::FUNC_SLOWDOWN),
					       Widget::ANIM_META_ALPHA, 2, time * 0.3f);
		if (m_dimmer != NULL && m_dimmerType == 0)
			ScePafCommon_B48C9BDC(time - 100.0f, m_dimmer,
					      common::transition::Type_FadeinSlow, false, false);
		ScePafWidget_5A9793B0(m_page, time + 50.0f);
	}
	m_open = false;
	m_plugin->PageClose(IDParam(m_pageName), Plugin::PageCloseParam());
	if (ret != 0)
		OnTerm();
}

/* the dialog's sound played through its environment */
void CommonGuiDialog::PlaySound()
{
	const char *buffer1;
	const char *buffer2;
	uint32_t size1;
	uint32_t size2;
	int32_t samplerate;
	uint32_t type;
	Environment *env;

	if (m_soundId == 0)
		return;
	if (ScePafToplevel_CA3E606F(m_plugin, IDParam(m_soundId), &buffer1, &size1, &buffer2,
				    &size2, &samplerate, &type) != 0)
		return;
	env = m_dialog->m_env;
	if (env->m_sound_callback != NULL)
		env->m_sound_callback(buffer1, size1, buffer2, size2, samplerate, -1.0f, false,
				      env->m_sound_callback_data);
}

void CommonGuiDialog::Select(int32_t button)
{
	if (m_handler == NULL || !m_open)
		return;
	m_inHandler = true;
	(*m_handler)(m_id, button);
	m_inHandler = false;
	Close();
}

CommonGuiDialog::CommonGuiDialog()
	: m_handler(NULL), m_refCount(0), m_plugin(NULL), m_page(NULL), m_dialog(NULL),
	  m_dimmer(NULL), m_open(false), m_waiting(false), m_inHandler(false),
	  m_notifyClose(false), m_done(NULL), m_waitVblank(0), m_result(0), m_id(-1), m_soundId(0)
{
	for (uint32_t i = 0; i < s_dialogs.size() + 1; i++) {
		int32_t id = s_nextId;

		if (id < 0)
			id = 0;
		s_nextId = id + 1;
		if (Find(id) == NULL) {
			m_id = id;
			break;
		}
	}
	s_dialogs.push_back(this);
	m_pageName[0] = '\0';
}

CommonGuiDialog::~CommonGuiDialog()
{
	if (m_handler != NULL)
		delete m_handler;
	m_handler = NULL;
	s_dialogs.remove(this);
	while (m_waitVblank != 0)
		ScePafWidget_DB993F51(&m_waitVblank);
	if (m_done != NULL)
		*m_done = true;
}

int32_t CommonGuiDialog::GetDefaultPriority()
{
	return s_defaultPriority;
}

void CommonGuiDialog::SetDefaultPriority(int32_t priority)
{
	s_defaultPriority = priority;
}

/* a functor called once the dialog has opened */
class ReadyListener : public EventListener {
public:
	ReadyListener(int32_t id, CommonGuiLayoutDialogOpenHandler *functor)
		: m_id(id), m_functor(functor)
	{
	}

	virtual ~ReadyListener();
	virtual int32_t Do(int32_t type, Handler *self, Event *e);

	int32_t m_id;
	CommonGuiLayoutDialogOpenHandler *m_functor;
};

void CommonGuiDialog::AddReadyListener(CommonGuiLayoutDialogOpenHandler *functor)
{
	if (m_open)
		m_dialog->AddEventListener(Handler::CB_STATE_READY, new ReadyListener(m_id, functor));
}

/* the dialog's page and widgets: the default ones where its page names none */
#define DIALOG_TEMPLATE			0x3A918C2C
#define DIALOG_WIDGET			0x438C7285

/* the parts a layout dialog's frame is shown without (dimmer type above 0) */
#define DIALOG_FRAME			0x5066AB1C
#define DIALOG_BACKGROUND		0x2C5BA918
#define DIALOG_BACKGROUND_TEXTURE	0xE880B27A
#define DIALOG_DIVIDER_TOP		0x1BCD9EA1
#define DIALOG_DIVIDER_BOTTOM		0x85A6C82E

/* the width from which the dialog covers the screen */
#define DIALOG_WIDTH_FULL		960.0f

/* the dimmer's colours: covered, plain, and a full-screen dialog's */
static const math::v4 s_dimmerColorOpaque(math::v4(0.0f, 0.0f, 0.0f, 255.0f) / 255.0f);
static const math::v4 s_dividerColor(math::v4(12.0f, 12.0f, 17.0f, 255.0f) / 255.0f);
static const math::v4 s_dimmerColorFull(math::v4(39.0f, 40.0f, 55.0f, 255.0f) / 255.0f);

int32_t CommonGuiDialog::Open(Plugin *plugin, int32_t priority, bool waitTransition,
			      bool notifyClose, int32_t dimmer, int32_t pageOption,
			      const CommonGuiDialogPage *page, const Dialog::PartsList *parts,
			      float *transitionTime)
{
	bool newDimmer;
	Widget *dialog;
	Widget *contents;
	int32_t ret;
	float time;

	if (m_open)
		return -1;
	m_plugin = plugin;
	m_notifyClose = notifyClose;
	m_dimmerType = dimmer;
	if (m_page == NULL) {
		sce_paf_snprintf(m_pageName, sizeof(m_pageName), "dp%d", m_id);

		Plugin::PageOpenParam openParam;

		if (priority < 0)
			priority = s_defaultPriority;
		openParam.overwrite_draw_priority = priority;
		openParam.option = 1;
		openParam.graphics_flag = pageOption;
		m_page = m_plugin->PageOpen(IDParam(m_pageName), openParam);
		if (m_page == NULL)
			return -1;
		ScePafWidget_A1193AAB(m_page, 1, 0);
	}
	newDimmer = false;
	if (m_dimmer == NULL) {
		newDimmer = true;
		m_dimmer = new Plane(m_page, NULL);
		if (m_dimmer == NULL)
			return COMMON_GUI_DIALOG_ERROR_NO_MEMORY;
		math::rect screen(m_page->m_screen->sp);
		m_dimmer->SetSize(math::v4(screen.width(), screen.height(), 0.0f, 0.0f) * 3.0f);
		ScePafWidget_42C5A4D4(m_dimmer, 0, 0);
	}
	ret = m_plugin->TemplateOpen(m_dimmer,
				     IDParam(page->id[0] != 0 ? page->id[0] : DIALOG_TEMPLATE),
				     Plugin::TemplateOpenParam());
	if (ret != 0)
		return ret;
	dialog = m_page->FindChild(IDParam(page->id[1] != 0 ? page->id[1] : DIALOG_WIDGET), 0);
	if (dialog == NULL)
		return -1;
	if (!dialog->IsInherit(Dialog::TypeName()))
		return COMMON_GUI_DIALOG_ERROR_NO_DIALOG;
	m_dialog = (Dialog *)dialog;
	if (page->id[2] != 0) {
		ret = m_plugin->TemplateOpen(m_dialog, IDParam(page->id[2]),
					     Plugin::TemplateOpenParam());
		if (ret != 0)
			return ret;
	}
	if (newDimmer) {
		if (m_dialog->GetSize(0)->extract_x() >= DIALOG_WIDTH_FULL) {
			m_dimmer->SetColor(s_dimmerColorFull);
		} else if (m_dimmerType != 0) {
			m_dimmer->SetColor(s_dimmerColorOpaque);
			m_dimmer->GetDrawObj(0)->m_draw_style.blend = 0;
		} else {
			Framework *framework = Framework::Instance();
			int32_t mode = framework->GetMode();

			if (mode == 4)
				mode = framework->GetMode_ApplicationC();
			m_dimmer->SetColor(math::v4(0.0f, 0.0f, 0.0f, mode == 3 ? 0.0f : 0.3f));
		}
		if (m_dimmerType == 0)
			ScePafCommon_6CF3A3C8(0.0f, m_dimmer, common::transition::Type_FadeinSlow, false,
					      false);
	}
	if (m_dimmerType > 0) {
		m_dialog->GetDrawObj(0)->m_draw_style.blend = 0;
		if (m_dimmerType > 0) {
			Widget *part;

			part = m_dialog->FindChild(IDParam((uint32_t)DIALOG_FRAME), 0);
			if (part != NULL)
				part->DestroyWidget();
			part = m_dialog->FindChild(IDParam((uint32_t)DIALOG_BACKGROUND), 0);
			if (part != NULL) {
				part->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
				part->GetDrawObj(0)->m_draw_style.blend = 0;
				part->SetTexture(m_plugin->GetTexture(
					IDParam((uint32_t)DIALOG_BACKGROUND_TEXTURE)));
			}
			math::v4 color = s_dividerColor;

			part = m_dialog->FindChild(IDParam((uint32_t)DIALOG_DIVIDER_TOP), 0);
			if (part != NULL)
				part->SetColor(color);
			part = m_dialog->FindChild(IDParam((uint32_t)DIALOG_DIVIDER_BOTTOM), 0);
			if (part != NULL)
				part->SetColor(color);
		}
	}
	time = ScePafCommon_6CF3A3C8(0.0f, m_dialog, common::transition::Type_Popup1, true, false);
	if (waitTransition)
		m_dialog->SetTimeoutEvent(0, time, OnOpenTimer, (void *)m_id);
	contents = m_dialog->FindChild(IDParam((uint32_t)DIALOG_CONTENTS), 0);
	if (contents != NULL) {
		float alpha = contents->m_meta_alpha;

		contents->SetMetaAlpha(0.0f);
		contents->SetMetaAlpha(alpha, new Timer(time * 0.8f, Timer::FUNC_QUART_IN));
	}
	if (transitionTime != NULL)
		*transitionTime = time + 150.0f;

	static const Dialog::PartsList s_parts[] = {
		{ 6, IDParam((uint32_t)0x34B39E56) },
		{ 7, IDParam((uint32_t)0x5B21B90A) },
		{ 10, IDParam((uint32_t)0x019BC65D) },
		{ 0, IDParam() },
	};

	m_dialog->SetPartsList(s_parts);
	m_dialog->SetPartsList(parts);
	m_open = true;
	if (m_open)
		m_dialog->AddEventListener(Handler::CB_STATE_READY, new ReadyListener(m_id, NULL));
	return 0;
}

void CommonGuiDialog::Hide()
{
	float time;

	if (!m_open)
		return;
	time = ScePafCommon_B48C9BDC(0.0f, m_dialog, common::transition::Type_Popup1, false, false);
	m_dialog->m_name.m_idhash = 0;
	ScePafWidget_5A9793B0(m_dialog, time + 50.0f);
	m_open = false;
}

Widget *CommonGuiDialog::GetPart(int32_t index)
{
	if (!m_open)
		return NULL;
	if (index == 3)
		return m_dialog->m_parent;
	if (index == 4)
		return m_dialog;
	return m_dialog->GetPartsWidget(index);
}

void CommonGuiDialog::SetResult(int32_t result)
{
	m_result = result;
}

int32_t CommonGuiDialog::Wait()
{
	int32_t result;

	m_waiting = true;
	ScePafToplevel_1EEFAECD(Framework::Instance(), &m_done);
	result = m_result;
	m_waiting = false;
	delete this;
	return result;
}

void CommonGuiDialog::SetDebugMode(bool on)
{
	m_plugin->SetPageDebugMode(IDParam(m_pageName), (Plugin::PageDebugMode)0, on);
}

void CommonGuiDialog::ShowPart(int32_t index)
{
	Widget *part = GetPart(index);

	if (part != NULL)
		ShowWidget(part, NULL);
}

void CommonGuiDialog::HidePart(int32_t index)
{
	Widget *part = GetPart(index);

	if (part == NULL)
		return;
	part->m_transition_complete = false;
	part->m_transition = 0xff;
	part->DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(part, NULL, 2, 0.0f, NULL);
}

/* a functor called every interval */
class IntervalListener : public EventListener {
public:
	IntervalListener(int32_t id, CommonGuiLayoutDialogOpenHandler *functor)
		: m_id(id), m_functor(functor)
	{
	}

	virtual ~IntervalListener();
	virtual int32_t Do(int32_t type, Handler *self, Event *e);

	int32_t m_id;
	CommonGuiLayoutDialogOpenHandler *m_functor;
};

void CommonGuiDialog::SetInterval(CommonGuiLayoutDialogOpenHandler *functor, float interval)
{
	if (m_open)
		m_dialog->SetTimerListener(Handler::CB_INTERVAL | 1, interval,
					   new IntervalListener(m_id, functor));
}

void CommonGuiDialog::KillInterval()
{
	if (m_open)
		m_dialog->KillTimer(Handler::CB_INTERVAL | 1);
}

void CommonGuiDialog::SetHandler(CommonGuiDialogHandler *handler)
{
	if (m_handler != NULL)
		delete m_handler;
	m_handler = handler;
}

void CommonGuiDialog::DeleteReadyListeners()
{
	if (m_open)
		m_dialog->DeleteEventListener(Handler::CB_STATE_READY, NULL);
}

/* a button pressed: the dialog's handler called with it */
class ButtonListener : public EventListener {
public:
	ButtonListener(CommonGuiDialog *dialog, int32_t button) : m_dialog(dialog), m_button(button)
	{
	}

	virtual int32_t Do(int32_t type, Handler *self, Event *e);

	CommonGuiDialog *m_dialog;
	int32_t m_button;
};

/* a dialog's button made ready */
class ButtonTextReadyListener : public EventListener {
public:
	virtual int32_t Do(int32_t type, Handler *self, Event *e)
	{
		return FitButtonLabel((Widget *)self);
	}
};

/* the event a text part sends when it is pressed */
#define DIALOG_TEXT_EVENT		0x10000003

/* the part that takes the cancel key rather than the focus */
#define DIALOG_PART_CANCEL		8

void CommonGuiDialog::SetButtons(const CommonGuiDialogButton *buttons)
{
	int32_t focused;

	if (buttons == NULL || !m_open)
		return;
	focused = 0;
	for (; buttons->index != 0; buttons++) {
		Widget *part = m_dialog->GetPartsWidget(buttons->index);

		if (part == NULL)
			continue;
		if (buttons->labelId != 0) {
			const wchar_t *label = m_plugin->GetString(IDParam(buttons->labelId));

			part->SetString(wstring(label));
		}
		if (buttons->styleId != 0) {
			WidgetSoundParam::Sound *sounds;
			uint32_t num;
			int32_t ret;

			ret = ScePafToplevel_DA3D4AF0(m_plugin, IDParam(buttons->styleId), &sounds,
						      &num);
			if (ret == 0) {
				part->m_sounds = sounds;
				part->m_sound_num = num;
			}
		}
		if (part->IsInherit(ButtonBase::TypeName())) {
			part->AddEventListener(DIALOG_BUTTON_EVENT,
					       new ButtonListener(this, buttons->button));
			if (part->m_adjust_x == 0) {
				graph::DrawObj *obj = part->GetDrawObj(1);

				if (obj != NULL) {
					math::v4 color = obj->GetColor();

					color.set_w(0.0f);
					obj->SetColor(color);
					part->AddEventListener(Handler::CB_STATE_READY,
							       new ButtonTextReadyListener());
				}
			}
		} else if (part->IsInherit(Text::TypeName())) {
			Text *text = (Text *)part;
			uint32_t length = 0;
			graph::TextStyle style;

			text->GetLength(length);
			ScePafWidget_4D5F35D5(text, 0, length);
			text->GetStyle(0, style);
			style.SetStyleAttribute(graph::TextStyleAttribute_Color,
						math::v4(1.0f, 1.0f, 1.0f, 0.7f));
			ScePafWidget_B2A7DF66(text, 1, style);
			style.SetStyleAttribute(graph::TextStyleAttribute_Color,
						math::v4(1.0f, 1.0f, 1.0f, 1.0f));
			ScePafWidget_B2A7DF66(text, 2, style);
			text->AddEventListener(DIALOG_TEXT_EVENT,
					       new ButtonListener(this, buttons->button));
		}
		if (buttons->index == DIALOG_PART_CANCEL) {
			ScePafWidget_21125870(part, 32);
		} else {
			if (focused == 0)
				part->SetFocus(NULL);
			focused++;
		}
	}
}

void CommonGuiDialog::OnOpenTimer(int32_t type, Handler *self, Event *e, void *data)
{
	CommonGuiDialog *dialog = Find((int32_t)data);

	if (dialog != NULL)
		ScePafWidget_10BEC7CF(&dialog->m_waitVblank);
}

IntervalListener::~IntervalListener()
{
	if (m_functor != NULL)
		delete m_functor;
	m_functor = NULL;
}

int32_t IntervalListener::Do(int32_t type, Handler *self, Event *e)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(m_id);

	if (dialog != NULL && dialog->m_open && m_functor != NULL)
		(*m_functor)();
	return 0;
}

ReadyListener::~ReadyListener()
{
	if (m_functor != NULL)
		delete m_functor;
	m_functor = NULL;
}

int32_t ReadyListener::Do(int32_t type, Handler *self, Event *e)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(m_id);

	if (dialog == NULL || !dialog->m_open)
		return 0;
	dialog->m_inHandler = true;
	if (m_functor != NULL)
		(*m_functor)();
	dialog->m_dialog->DeleteEventListener(Handler::CB_STATE_READY, this);
	dialog->PlaySound();
	dialog->m_inHandler = false;
	dialog->Close();
	return 0;
}

int32_t ButtonListener::Do(int32_t type, Handler *self, Event *e)
{
	m_dialog->Select(m_button);
	return 0;
}

list<CommonGuiDialog *> CommonGuiDialog::s_dialogs;
int32_t CommonGuiDialog::s_nextId;
int32_t CommonGuiDialog::s_defaultPriority = 8;
