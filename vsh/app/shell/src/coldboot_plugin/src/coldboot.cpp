/*
 * coldboot.cpp: the cold boot plugin's page (0x810E4C1A-0x810E5952), opened while the shell
 * boots. Before the initial setup two texts fade in and out one after the other (later on a PS
 * TV); otherwise, and once the boot is done, the page shows the region's text, and the event that
 * text sends fades it and the page out and unloads the plugin. The file's and every
 * name here are ours (PLACEHOLDER: no record names them).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/core/animlistener.h>
#include <widget/w_scene.h>
#include <timer/timer.h>
#include <system/system.h>
#include <regmgr/regmgr.h>
#include "../../../shell.h"
#include "../../../plugins.h"
#include "coldboot.h"

using namespace paf;

/* the plugin's page and its widgets, by their IDs' hashes */
#define COLDBOOT_PAGE			0x9910C239
#define COLDBOOT_PAGE_PLANE		0xDC4832AC
#define COLDBOOT_PAGE_TEXT1		0x72492203
#define COLDBOOT_PAGE_TEXT2		0x189D8E1A
#define COLDBOOT_PAGE_REGION_TEXT	0x493F5E3A

/* the region text's strings: regions 3 to 5 and 9, and the others (PLACEHOLDER names) */
#define COLDBOOT_MSG_REGION_GROUP	0x72411882
#define COLDBOOT_MSG_REGION_DEFAULT	0x76A1B071

/* the event the region text sends (PLACEHOLDER name) */
#define COLDBOOT_EVENT_REGION_TEXT	0x100004

/* the texts' timing (ms): the wait on a PS TV, fading, shown, the second's start, the page's */
#define COLDBOOT_PSTV_DELAY		3000.0f
#define COLDBOOT_FADE_TIME		333.3f
#define COLDBOOT_TEXT_TIME		1500.0f
#define COLDBOOT_TEXT2_START		3333.3f
#define COLDBOOT_PAGE_TIME		5500.0f
#define COLDBOOT_REGION_TEXT_TIME	3000.0f

/* PLACEHOLDER: the page's state (8 bytes): the page and the texts' wait */
struct ColdbootState {
	ui::Scene *page;
	float delay;
};

/* (0x81555F34) */
static ColdbootState *s_state;

static inline void ShowWidget(ui::Widget *widget, Timer *timer, ui::AnimCB cb)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xff;
		widget->m_transition_complete = true;
		widget->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(widget, timer, 2, 0.0f, cb);
}

static inline void HideWidget(ui::Widget *widget, Timer *timer, float delay)
{
	float time = (timer != NULL ? timer->GetTotalTime() : 0.0f) + delay;

	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(time, true);
	ScePafWidget_4588699E(widget, timer, 2, delay, NULL);
}

/* the widget waits, fades in, stays shown and fades out, fin called once it has */
static inline void FadeInOut(ui::Widget *widget, float wait, float shown, ui::AnimCB fin)
{
	widget->SetAnimListener(ui::Widget::ANIM_META_ALPHA,
		new ui::AnimListener(new Timer(wait), widget, NULL, NULL, NULL), 0);
	widget->SetMetaAlpha(1.0f, new Timer(COLDBOOT_FADE_TIME));
	widget->SetAnimListener(ui::Widget::ANIM_META_ALPHA,
		new ui::AnimListener(new Timer(shown), widget, NULL, NULL, NULL), 0);
	widget->SetMetaAlpha(0.0f, new Timer(COLDBOOT_FADE_TIME), ui::Widget::ANIM_META_ALPHA, 0,
			     0.0f, fin);
}

/* (0x810E4C1A) */
void coldbootCreate(Plugin *plugin)
{
	ColdbootState *state = new ColdbootState;

	if (state != NULL) {
		state->page = NULL;
		state->delay = 0.0f;
		s_state = state;
		g_coldbootInterface->finish = coldbootFinish;
	}
}

/* (0x810E4C4E) */
void coldbootDestroy(void)
{
	if (s_state != NULL) {
		ColdbootState *state = s_state;

		s_state = NULL;
		delete state;
	}
}

static void coldbootOnRegionText(int32_t type, ui::Handler *self, ui::Event *e, void *data);

/* (0x810E4C6E) the boot done: the region's text, which fades the page out */
void coldbootFinish(void)
{
	ui::Widget *text = s_state->page->FindChild(IDParam(COLDBOOT_PAGE_REGION_TEXT), 0);
	uint32_t msg = COLDBOOT_MSG_REGION_DEFAULT;

	switch (sceRegMgrSrvGetRegion()) {
	case 3:
	case 4:
	case 5:
	case 9:
		msg = COLDBOOT_MSG_REGION_GROUP;
		break;
	}
	text->SetString(wstring(g_coldbootPlugin->GetString(IDParam(msg))));
	text->SetMetaAlpha(0.0f);
	text->AddEventListener(COLDBOOT_EVENT_REGION_TEXT,
			       new ui::EventCBListener(coldbootOnRegionText, NULL), true);
}

/* (0x810E4E42) the page opened: the texts, or the region's text */
void coldbootOpen(void)
{
	Plugin::PageOpenParam openParam;

	s_state->page = g_coldbootPlugin->PageOpen(IDParam(COLDBOOT_PAGE), openParam);
	if (system::Model::GetType() == 1)
		s_state->delay = COLDBOOT_PSTV_DELAY;

	ui::Widget *plane = s_state->page->FindChild(IDParam(COLDBOOT_PAGE_PLANE), 0);

	ScePafWidget_0253DC9C(plane, true);

	ui::Widget *leadText = s_state->page->FindChild(IDParam(COLDBOOT_PAGE_TEXT1), 0);
	ui::Widget *followText = s_state->page->FindChild(IDParam(COLDBOOT_PAGE_TEXT2), 0);

	leadText->SetMetaAlpha(0.0f);
	followText->SetMetaAlpha(0.0f);
	if (shellStartupNeedsInitialSetup() && !shellStartupIsPmMode() &&
	    shellStartupGetIduMode() != 1 && !shellStartupIsTestMode()) {
		FadeInOut(leadText, s_state->delay, COLDBOOT_TEXT_TIME, NULL);
		FadeInOut(followText, s_state->delay + COLDBOOT_TEXT2_START, COLDBOOT_TEXT_TIME, NULL);

		ui::Scene *page = s_state->page;
		float delay = s_state->delay + COLDBOOT_PAGE_TIME;

		if (delay > 0.0f)
			HideWidget(page, new Timer(0.0f), delay);
		else
			HideWidget(page, NULL, 0.0f);
		s_state->delay = 0.0f;
	} else {
		coldbootFinish();
	}
}

/* (0x810E5602) */
void coldbootClose(void)
{
	g_coldbootPlugin->PageClose(IDParam(COLDBOOT_PAGE), Plugin::PageCloseParam());
}

static void coldbootOnFaded(Timer *t, ui::AnimListener *listener, void *target, void *userData,
			    void *context);

/* (0x810E569E) the region text's event: it fades out, the page shown, and the plugin unloads */
static void coldbootOnRegionText(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	ui::Widget *text = (ui::Widget *)self;

	FadeInOut(text, s_state->delay, COLDBOOT_REGION_TEXT_TIME, coldbootOnFaded);
	ShowWidget(s_state->page, NULL, NULL);
	s_state->delay = 0.0f;
}

/* (0x810E593C) */
static void coldbootOnFaded(Timer *t, ui::AnimListener *listener, void *target, void *userData,
			    void *context)
{
	Plugin::UnloadAsync(g_coldbootPlugin, NULL, 0);
}
