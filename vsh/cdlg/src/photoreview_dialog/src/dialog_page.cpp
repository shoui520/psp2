/*
 * DialogPage: a page of the plugin's resource and what the image dialogs do to it. Names ours;
 * see dialog_page.h.
 */
#include <stddef.h>
#include <stdint.h>
#include <std/new>
#include <plugin.h>
#include <timer/timer.h>
#include <math/ngp/v4.h>
#include <versatileparam/idparam.h>
#include <widget/core/handler.h>
#include <widget/core/widget.h>
#include <widget/w_scene.h>
#include "dialog_page.h"

using namespace paf;

/* how far SlideOut moves the page: the screen's height and width */
#define DIALOG_PAGE_SLIDE_HEIGHT	544.0f
#define DIALOG_PAGE_SLIDE_WIDTH		960.0f

/* the curve of the slide and its fade */
#define DIALOG_PAGE_SLIDE_FUNC		21

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
	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(timer != NULL ? timer->GetTotalTime() : 0.0f, true);
	ScePafWidget_4588699E(widget, timer, 2, 0.0f, NULL);
}

DialogPage::DialogPage(Plugin *plugin, const IDParam &id, const Plugin::PageOpenParam &openParam)
	: m_plugin(plugin), m_id(id), m_openParam(openParam)
{
}

DialogPage::~DialogPage()
{
}

int DialogPage::OnCreate()
{
	return 0;
}

int DialogPage::OnDestroy()
{
	return 0;
}

int DialogPage::Open()
{
	ui::Scene *scene = m_plugin->PageOpen(m_id, m_openParam);

	if (scene == NULL)
		return -1;
	scene->AddEventCallback(ui::Handler::CB_STATE_INIT, InitCB, this);
	scene->AddEventCallback(DIALOG_PAGE_EVENT_PRESS, PressCB, this);
	scene->AddEventCallback(DIALOG_PAGE_EVENT_PRESS_OUT, PressOutCB, this);
	return 0;
}

int DialogPage::Close()
{
	m_plugin->PageClose(m_id, Plugin::PageCloseParam());
	return 0;
}

int DialogPage::Activate()
{
	m_plugin->PageActivate(m_id);
	return 0;
}

int DialogPage::Inactivate()
{
	m_plugin->PageInactivate(m_id);
	return 0;
}

void DialogPage::FadeIn(float timeMs, float delayMs)
{
	ui::Scene *scene;

	if (m_plugin != NULL && (scene = m_plugin->PageRoot(m_id)) != NULL)
		scene->SetMetaAlpha(1.0f, new Timer(timeMs), ui::Widget::ANIM_META_ALPHA, 2, delayMs,
				    FadeEndCB, this);
}

void DialogPage::FadeOut(float timeMs, float delayMs)
{
	ui::Scene *scene;

	if (m_plugin != NULL && (scene = m_plugin->PageRoot(m_id)) != NULL)
		scene->SetMetaAlpha(0.0f, new Timer(timeMs), ui::Widget::ANIM_META_ALPHA, 2, delayMs,
				    FadeEndCB, this);
}

void DialogPage::Hide(float timeMs)
{
	ui::Scene *scene;

	if (m_plugin != NULL && (scene = m_plugin->PageRoot(m_id)) != NULL)
		scene->SetMetaAlpha(0.0f, new Timer(timeMs), ui::Widget::ANIM_META_ALPHA, 2, 0.0f,
				    HiddenCB, this);
}

void DialogPage::SlideOut(int32_t direction, float timeMs)
{
	ui::Scene *scene;

	if (m_plugin != NULL && (scene = m_plugin->PageRoot(m_id)) != NULL) {
		math::v4 pos = scene->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

		switch (direction) {
		case DIALOG_PAGE_SLIDE_DOWN:
			pos = pos + math::v4(0.0f, DIALOG_PAGE_SLIDE_HEIGHT, 0.0f, 0.0f);
			break;
		case DIALOG_PAGE_SLIDE_UP:
			pos = pos + math::v4(0.0f, -DIALOG_PAGE_SLIDE_HEIGHT, 0.0f, 0.0f);
			break;
		case DIALOG_PAGE_SLIDE_LEFT:
			pos = pos + math::v4(-DIALOG_PAGE_SLIDE_WIDTH, 0.0f, 0.0f, 0.0f);
			break;
		case DIALOG_PAGE_SLIDE_RIGHT:
			pos = pos + math::v4(DIALOG_PAGE_SLIDE_WIDTH, 0.0f, 0.0f, 0.0f);
			break;
		}
		scene->SetPos(pos, new Timer(timeMs, DIALOG_PAGE_SLIDE_FUNC), ui::Widget::ANIM_MOVE,
			      2, 0.0f, NULL, NULL);
		scene->SetMetaAlpha(0.0f, new Timer(timeMs, DIALOG_PAGE_SLIDE_FUNC),
				    ui::Widget::ANIM_META_ALPHA, 2, 0.0f, HiddenCB, this);
	}
}

bool DialogPage::IsOpen()
{
	return m_plugin != NULL && m_plugin->PageRoot(m_id) != NULL;
}

ui::Widget *DialogPage::FindChild(const IDParam &id, int32_t opt)
{
	ui::Scene *scene;

	if (m_plugin != NULL && (scene = m_plugin->PageRoot(m_id)) != NULL)
		return scene->FindChild(id, opt);
	return NULL;
}

void DialogPage::ShowChild(const IDParam &id, float timeMs)
{
	ui::Scene *scene;
	ui::Widget *widget;

	if (m_plugin != NULL && (scene = m_plugin->PageRoot(m_id)) != NULL &&
	    (widget = scene->FindChild(id, 0)) != NULL) {
		if (timeMs > 0.0f)
			ShowWidget(widget, new Timer(timeMs));
		else
			ShowWidget(widget, NULL);
	}
}

void DialogPage::HideChild(const IDParam &id, float timeMs)
{
	ui::Scene *scene;
	ui::Widget *widget;

	if (m_plugin != NULL && (scene = m_plugin->PageRoot(m_id)) != NULL &&
	    (widget = scene->FindChild(id, 0)) != NULL) {
		if (timeMs > 0.0f)
			HideWidget(widget, new Timer(timeMs));
		else
			HideWidget(widget, NULL);
	}
}

void DialogPage::ListenDecide(const IDParam &id)
{
	ui::Scene *scene;
	ui::Widget *widget;

	if (m_plugin != NULL && (scene = m_plugin->PageRoot(m_id)) != NULL &&
	    (widget = scene->FindChild(id, 0)) != NULL)
		widget->AddEventCallback(DIALOG_PAGE_EVENT_DECIDE, DecideCB, this);
}

void DialogPage::MakeButton(ui::Widget *widget)
{
	if (widget != NULL) {
		widget->m_event_enable |= 0x10;
		widget->AddEventCallback(DIALOG_PAGE_EVENT_BUTTON, ButtonCB, this);
	}
}

void DialogPage::OnInit(int32_t type, ui::Handler *self, ui::Event *e)
{
}

void DialogPage::OnPress(int32_t type, ui::Handler *self, ui::Event *e)
{
}

void DialogPage::OnPressOut(int32_t type, ui::Handler *self, ui::Event *e)
{
}

void DialogPage::OnUnused64()
{
}

void DialogPage::OnDecide(int32_t type, ui::Handler *self, ui::Event *e)
{
}

void DialogPage::OnButton(int32_t type, ui::Handler *self, ui::Event *e)
{
}

void DialogPage::OnUnused76()
{
}

void DialogPage::OnUnused80()
{
}

void DialogPage::OnUnused84()
{
}

void DialogPage::OnUnused88()
{
}

void DialogPage::OnFadeEnd(Timer *t, ui::AnimListener *listener, void *data1, void *data2,
			   void *data3)
{
}

void DialogPage::OnHidden()
{
}

void DialogPage::InitCB(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	if (userdata != NULL)
		((DialogPage *)userdata)->OnInit(type, self, e);
}

void DialogPage::PressCB(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	if (userdata != NULL)
		((DialogPage *)userdata)->OnPress(type, self, e);
}

void DialogPage::PressOutCB(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	if (userdata != NULL)
		((DialogPage *)userdata)->OnPressOut(type, self, e);
}

void DialogPage::DecideCB(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	if (userdata != NULL)
		((DialogPage *)userdata)->OnDecide(type, self, e);
}

void DialogPage::ButtonCB(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	if (userdata != NULL)
		((DialogPage *)userdata)->OnButton(type, self, e);
}

void DialogPage::FadeEndCB(Timer *t, ui::AnimListener *listener, void *data1, void *data2,
			   void *data3)
{
	if (data2 != NULL)
		((DialogPage *)data2)->OnFadeEnd(t, listener, data1, data2, data3);
}

void DialogPage::HiddenCB(Timer *t, ui::AnimListener *listener, void *data1, void *data2,
			  void *data3)
{
	if (data2 != NULL)
		((DialogPage *)data2)->OnHidden();
}
