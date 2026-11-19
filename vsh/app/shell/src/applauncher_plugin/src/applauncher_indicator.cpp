/*
 * applauncher_indicator.cpp: the home's page indicator, a dot for each page in the box under
 * the pages, the page in view's dot drawn with the theme's current page image
 * (0x810C24D8-0x810C3B3C). The file's name and the names here are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string>
#include <math/ngp/v4.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/core/layouter.h>
#include <timer/timer.h>
#include <common/shared_ptr.h>
#include "applauncher.h"
#include "applauncher_home.h"
#include "../../../theme.h"

using namespace paf;

/* PLACEHOLDER: the indicator's template and its box of dots */
#define APPLAUNCHER_INDICATOR_BOX_TEMPLATE	0x72AE9F22
#define APPLAUNCHER_INDICATOR_BOX		0x6D895C97
/* PLACEHOLDER: a dot's template and its widget, named after its page's key */
#define APPLAUNCHER_INDICATOR_DOT_TEMPLATE	0x09C61366
#define APPLAUNCHER_INDICATOR_DOT		0x90BE1658
/* PLACEHOLDER: the dots' own images, without a theme's */
#define APPLAUNCHER_INDICATOR_CUR_TEXTURE	0x21AD6D05
#define APPLAUNCHER_INDICATOR_BASE_TEXTURE	0x266627F6
/* PLACEHOLDER: the box's timer of a refresh after a page is added or taken out, and a dot's
   timer of its destruction */
#define APPLAUNCHER_INDICATOR_REFRESH_TIMER	0x1002000
#define APPLAUNCHER_INDICATOR_REMOVE_TIMER	0x1002001

/* PLACEHOLDER: a ShellSnapList's events: an item put in, taken out, and the view moved */
#define SHELL_SNAP_LIST_EVENT_INSERT		0
#define SHELL_SNAP_LIST_EVENT_REMOVE		1
#define SHELL_SNAP_LIST_EVENT_MOVE		2

/* PLACEHOLDER: the handler's widget destroyed, a timer's callback (0x811614AC, not written
   yet) */
void shellWidgetDestroyCB(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);

static int onLayout(ui::Layouter *layouter, ui::Widget *widget, void *data1, void *data2,
		    void *data3);
static void onInsert(int event, void *arg, int key, void *data);
static void onRemove(int event, void *arg, int key, void *data);
static void onMove(int event, void *arg, int key, void *data);
static void onRefreshTimer(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);

/* shows and hides a widget over the timer's time (inline in paf's callers) */
static inline void ShowWidget(ui::Widget *widget, Timer *timer)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xff;
		widget->m_transition_complete = true;
		widget->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(widget, timer, 2, 0.0f, NULL);
}

static inline void HideWidget(ui::Widget *widget, Timer *timer)
{
	float time = timer != NULL ? timer->GetTotalTime() : 0.0f;

	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(time, true);
	ScePafWidget_4588699E(widget, timer, 2, 0.0f, NULL);
}

/* the box opened in the widget, laid out by onLayout; the dots follow the home's pages
   (0x810C24D8) */
ApplauncherPageIndicator::ApplauncherPageIndicator(Plugin *plugin, ui::Widget *widget)
	: m_plugin(plugin), m_box(NULL), m_hasBaseImage(false), m_hasCurImage(false)
{
	if (widget != NULL) {
		plugin->TemplateOpen(widget, IDParam(s_empty_name, APPLAUNCHER_INDICATOR_BOX_TEMPLATE),
				     Plugin::TemplateOpenParam());
		m_box = widget->FindChild(IDParam(s_empty_name, APPLAUNCHER_INDICATOR_BOX), 0);
		if (m_box != NULL)
			m_box->SetLayouter(new ui::Layouter(onLayout, NULL));
	}
	g_applauncherHome->m_scroller.AddListener(SHELL_SNAP_LIST_EVENT_INSERT, onInsert, this);
	g_applauncherHome->m_scroller.AddListener(SHELL_SNAP_LIST_EVENT_REMOVE, onRemove, this);
	g_applauncherHome->m_scroller.AddListener(SHELL_SNAP_LIST_EVENT_MOVE, onMove, this);
}

/* (0x810C29A4) */
ApplauncherPageIndicator::~ApplauncherPageIndicator()
{
	g_applauncherHome->m_scroller.DelListener(SHELL_SNAP_LIST_EVENT_INSERT, onInsert, this);
	g_applauncherHome->m_scroller.DelListener(SHELL_SNAP_LIST_EVENT_REMOVE, onRemove, this);
	g_applauncherHome->m_scroller.DelListener(SHELL_SNAP_LIST_EVENT_MOVE, onMove, this);
}

/* (0x810C2C2C) */
void ApplauncherPageIndicator::Show(float time)
{
	ui::Widget *box = m_box;

	if (box != NULL)
		ShowWidget(box, new Timer(time, Timer::FUNC_QUAD_OUT));
}

/* (0x810C2CD6) */
void ApplauncherPageIndicator::Hide(float time)
{
	ui::Widget *box = m_box;

	if (box != NULL)
		HideWidget(box, new Timer(time, Timer::FUNC_QUAD_OUT));
}

/* nothing while a refresh waits for its timer; the dots not being destroyed are put in the
   order of the home's pages, each in front of the one before (0x810C2D96) */
void ApplauncherPageIndicator::Refresh()
{
	ui::Widget *box = m_box;

	if (box->m_map_t_listener.find(APPLAUNCHER_INDICATOR_REFRESH_TIMER) !=
	    box->m_map_t_listener.end())
		return;

	ui::Widget *prev = NULL;
	int index = 0;

	for (list<ui::Widget *>::iterator it = box->m_children.begin();
	     it != box->m_children.end(); ++it, ++index) {
		if ((*it)->m_map_t_listener.find(APPLAUNCHER_INDICATOR_REMOVE_TIMER) !=
		    (*it)->m_map_t_listener.end())
			continue;

		vector<ShellSnapListItem *> *items = &g_applauncherHome->m_scroller.m_items;
		ApplauncherPage *page = NULL;

		if (items != NULL)
			page = static_cast<ApplauncherPage *>((*items)[index]);

		char name[64];

		sce_paf_snprintf(name, sizeof(name), "%d", page != NULL ? page->m_key : -1);

		ui::Widget *dot = m_box->FindChild(IDParam(string(name)), 0);

		if (dot != *it)
			dot->SetForward(prev);
		box = m_box;
		prev = dot;
	}

	ApplauncherPage *focused =
		static_cast<ApplauncherPage *>(g_applauncherHome->m_scroller.GetFocused());

	if (focused == NULL)
		return;

	int key = focused->m_key;

	if (key == -1)
		return;
	for (list<ui::Widget *>::iterator it = box->m_children.begin();
	     it != box->m_children.end(); ++it) {
		ui::Widget *dot = *it;

		if (dot->m_map_t_listener.find(APPLAUNCHER_INDICATOR_REMOVE_TIMER) !=
		    dot->m_map_t_listener.end())
			continue;
		if (key == sce_paf_strtol(dot->m_name.m_id.c_str(), NULL, 10)) {
			if (m_hasCurImage)
				dot->SetDrawObjMaskBit(&m_curImage, 0, 0);
			else
				dot->SetTexture(m_plugin->GetTexture(
					IDParam(s_empty_name, APPLAUNCHER_INDICATOR_CUR_TEXTURE)));
		} else if (m_hasBaseImage) {
			dot->SetDrawObjMaskBit(&m_baseImage, 0, 0);
		} else {
			dot->SetTexture(m_plugin->GetTexture(
				IDParam(s_empty_name, APPLAUNCHER_INDICATOR_BASE_TEXTURE)));
		}
		box = m_box;
	}
}

/* the dot opened in the box and named after the key, put after the dots of the pages before
   its page, and grown in from nothing (0x810C3160) */
void ApplauncherPageIndicator::AddDot(int key)
{
	m_plugin->TemplateOpen(m_box, IDParam(s_empty_name, APPLAUNCHER_INDICATOR_DOT_TEMPLATE),
			       Plugin::TemplateOpenParam());

	ui::Widget *dot = m_box->FindChild(IDParam(s_empty_name, APPLAUNCHER_INDICATOR_DOT), 0);

	if (dot == NULL)
		return;

	char name[64];

	sce_paf_snprintf(name, sizeof(name), "%d", key);
	dot->m_name.SetID(string(name));

	ApplauncherPage *page =
		static_cast<ApplauncherPage *>(g_applauncherHome->m_scroller.GetItemByKey(key));
	int index = page != NULL ? page->m_index : -1;
	ApplauncherPage *focused =
		static_cast<ApplauncherPage *>(g_applauncherHome->m_scroller.GetFocused());
	int focusedIndex = -1;
	int focusedKey = -1;

	if (focused != NULL) {
		focusedKey = focused->m_key;
		focusedIndex = focused->m_index;
	}

	char focusedName[64];

	sce_paf_snprintf(focusedName, sizeof(focusedName), "%d", focusedKey);

	ui::Widget *focusedDot = m_box->FindChild(IDParam(string(focusedName)), 0);
	ApplauncherPage *last =
		static_cast<ApplauncherPage *>(g_applauncherHome->m_scroller.m_items.back());

	if (key != last->m_key) {
		if (focusedIndex < index)
			dot->SetForward(focusedDot);
		else
			dot->SetBehind(focusedDot);
	}
	dot->SetMetaAlpha(0.0f, NULL, ui::Widget::ANIM_META_ALPHA, 0);

	math::v4 size = *dot->GetSize(0);

	dot->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f), NULL, ui::Widget::ANIM_SIZE, 0);
	dot->SetSize(size, new Timer(200.0f, Timer::FUNC_LINEAR), ui::Widget::ANIM_SIZE, 0);
}

/* the dot of the key faded out and shrunk, and destroyed when that is done (0x810C34C0) */
void ApplauncherPageIndicator::RemoveDot(int key)
{
	char name[64];

	sce_paf_snprintf(name, sizeof(name), "%d", key);

	ui::Widget *dot = m_box->FindChild(IDParam(string(name)), 0);

	if (dot == NULL)
		return;
	dot->SetMetaAlpha(0.0f, new Timer(100.0f, Timer::FUNC_LINEAR), ui::Widget::ANIM_META_ALPHA,
			  0);
	dot->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f), new Timer(100.0f, Timer::FUNC_LINEAR),
		     ui::Widget::ANIM_SIZE, 0);
	dot->SetTimerListener(APPLAUNCHER_INDICATOR_REMOVE_TIMER, 100.0f,
			      new ui::EventCBListener(shellWidgetDestroyCB, this));
}

/* the theme's dot images: the base page's for the dots, the current page's for the page in
   view's dot (0x810C36B8) */
void ApplauncherPageIndicator::Update()
{
	common::SharedPtr<LsdbThemeInfo> *info = shellThemeGetInfo();

	m_hasCurImage = false;
	m_hasBaseImage = false;
	Refresh();
	applauncherIconCacheClear(m_baseImage);
	applauncherIconCacheClear(m_curImage);
	if (info->get() != NULL) {
		if (info->get()->m_homeProperty.m_basePageFilePath.length() != 0) {
			m_hasBaseImage = true;

			string path;

			shellThemeInfoGetFilePath(info->get(), &path,
						  info->get()->m_homeProperty.m_basePageFilePath);
			applauncherIconCacheLoad(path, &m_baseImage, NULL, NULL, true);
		}
		if (info->get()->m_homeProperty.m_curPageFilePath.length() != 0) {
			m_hasCurImage = true;

			string path;

			shellThemeInfoGetFilePath(info->get(), &path,
						  info->get()->m_homeProperty.m_curPageFilePath);
			applauncherIconCacheLoad(path, &m_curImage, NULL, NULL, true);
		}
	}
	Refresh();
}

/* a page put in: its dot added, the dots refreshed a moment after (0x810C37AE) */
static void onInsert(int event, void *arg, int key, void *data)
{
	ApplauncherPageIndicator *indicator = (ApplauncherPageIndicator *)data;

	indicator->AddDot(key);
	indicator->m_box->SetTimerListener(APPLAUNCHER_INDICATOR_REFRESH_TIMER, 200.0f,
					   new ui::EventCBListener(onRefreshTimer, indicator));
}

/* a page taken out: its dot removed, the same (0x810C380E) */
static void onRemove(int event, void *arg, int key, void *data)
{
	ApplauncherPageIndicator *indicator = (ApplauncherPageIndicator *)data;

	indicator->RemoveDot(key);
	indicator->m_box->SetTimerListener(APPLAUNCHER_INDICATOR_REFRESH_TIMER, 200.0f,
					   new ui::EventCBListener(onRefreshTimer, indicator));
}

/* the view moved: the dots refreshed (0x810C386E) */
static void onMove(int event, void *arg, int key, void *data)
{
	((ApplauncherPageIndicator *)data)->Refresh();
}

/* the box laid out: put at its parent's left, the dots one under the other, centred on it; a
   new dot (still clear) put in place and faded in, the others moved there (0x810C3878) */
static int onLayout(ui::Layouter *layouter, ui::Widget *widget, void *data1, void *data2,
		    void *data3)
{
	if (!layouter->IsDirty(0xff) || widget == NULL)
		return 0;

	float dotSize = 16.0f;
	float gap = 6.0f;
	math::v4 step(0.0f, 0.0f, 0.0f, 0.0f);
	math::v4 box(0.0f, 0.0f, 0.0f, 0.0f);

	box.set_x(-0.5f * widget->m_parent->GetSize(0)->extract_x() + 20.0f);
	widget->SetPos(box, NULL, ui::Widget::ANIM_MOVE, 0);

	math::v4 size(dotSize, 0.0f, 0.0f, 0.0f);

	for (list<ui::Widget *>::iterator it = widget->m_children.begin();
	     it != widget->m_children.end(); ++it)
		size = size + math::v4(dotSize, dotSize, dotSize, dotSize);

	math::v4 pos(0.0f, (dotSize + size.extract_y()) * 0.5f, 0.0f, 0.0f);

	step = math::v4(0.0f, dotSize + gap, 0.0f, 0.0f);
	for (list<ui::Widget *>::iterator it = widget->m_children.begin();
	     it != widget->m_children.end(); ++it) {
		ui::Widget *dot = *it;
		float time = 200.0f;

		if (dot->m_meta_alpha == 0.0f) {
			dot->SetMetaAlpha(1.0f, new Timer(200.0f, Timer::FUNC_LINEAR),
					  ui::Widget::ANIM_META_ALPHA, 0);
			time = 0.0f;
		}
		dot->SetPos(pos, new Timer(time, Timer::FUNC_LINEAR), ui::Widget::ANIM_MOVE, 0);
		pos = pos - step;
	}
	return 0;
}

/* the refresh's timer done: the dots refreshed (0x810C3AAC) */
static void onRefreshTimer(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	self->KillTimer((type & 0xFFFF) | 0x1000000);
	if (data != NULL)
		((ApplauncherPageIndicator *)data)->Refresh();
}
