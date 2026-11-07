/*
 * A page of the plugin's resource, opened and closed through the plugin, with what the image
 * dialogs and apps do to it: fades, a slide off the screen, its widgets shown and hidden, and the
 * events of its root and buttons passed to its own virtual methods. Sony's photo apps and image
 * dialogs build the same code; no binary names it, so the class, its methods and this file's
 * name are ours.
 */
#ifndef PHOTOREVIEW_DIALOG_DIALOG_PAGE_H
#define PHOTOREVIEW_DIALOG_DIALOG_PAGE_H

#include <stdint.h>
#include <plugin.h>
#include <timer/timer.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>

/* where SlideOut moves the page (names ours) */
enum {
	DIALOG_PAGE_SLIDE_DOWN = 1,
	DIALOG_PAGE_SLIDE_UP = 2,
	DIALOG_PAGE_SLIDE_LEFT = 3,
	DIALOG_PAGE_SLIDE_RIGHT = 4
};

/* the events the page listens for (paf's numbers; the names ours) */
#define DIALOG_PAGE_EVENT_PRESS		0x10000003	/* its root is pressed */
#define DIALOG_PAGE_EVENT_PRESS_OUT	0x10000004	/* the press leaves it */
#define DIALOG_PAGE_EVENT_DECIDE	0x10000008	/* a button of it is decided */
#define DIALOG_PAGE_EVENT_BUTTON	0x10001		/* a widget it made a button is pressed */

class DialogPage {
public:
	DialogPage(paf::Plugin *plugin, const paf::IDParam &id,
		   const paf::Plugin::PageOpenParam &openParam);

	/* the virtual methods are all defined in dialog_page.cpp, in this order */
	virtual ~DialogPage();
	virtual int OnCreate();
	virtual int OnDestroy();
	virtual int Open();
	virtual int Close();
	virtual int Activate();
	virtual int Inactivate();
	/* the page fades in or out over the time (ms), after the delay */
	virtual void FadeIn(float timeMs, float delayMs);
	virtual void FadeOut(float timeMs, float delayMs);
	/* the page fades out, and then OnHidden */
	virtual void Hide(float timeMs);
	/* the page slides off the screen as it fades out, and then OnHidden */
	virtual void SlideOut(int32_t direction, float timeMs);
	virtual bool IsOpen();

	/* the events of the page's root and widgets */
	virtual void OnInit(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnPress(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnPressOut(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnUnused64();
	virtual void OnDecide(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnButton(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnUnused76();
	virtual void OnUnused80();
	virtual void OnUnused84();
	virtual void OnUnused88();
	/* a fade of FadeIn or FadeOut has ended */
	virtual void OnFadeEnd(paf::Timer *t, paf::ui::AnimListener *listener, void *data1,
			       void *data2, void *data3);
	/* Hide or SlideOut has ended */
	virtual void OnHidden();

	paf::ui::Widget *FindChild(const paf::IDParam &id, int32_t opt);
	void ShowChild(const paf::IDParam &id, float timeMs);
	void HideChild(const paf::IDParam &id, float timeMs);
	void ListenDecide(const paf::IDParam &id);
	void MakeButton(paf::ui::Widget *widget);

	static void InitCB(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *userdata);
	static void PressCB(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *userdata);
	static void PressOutCB(int32_t type, paf::ui::Handler *self, paf::ui::Event *e,
			       void *userdata);
	static void DecideCB(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *userdata);
	static void ButtonCB(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *userdata);
	static void FadeEndCB(paf::Timer *t, paf::ui::AnimListener *listener, void *data1,
			      void *data2, void *data3);
	static void HiddenCB(paf::Timer *t, paf::ui::AnimListener *listener, void *data1,
			     void *data2, void *data3);

	paf::Plugin *m_plugin;
	paf::IDParam m_id;
	paf::Plugin::PageOpenParam m_openParam;
	paf::Plugin::PageCloseParam m_closeParam;
};

#endif
