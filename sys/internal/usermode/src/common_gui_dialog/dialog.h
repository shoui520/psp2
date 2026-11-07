/*
 * The dialog object every export works on: the page it opens in a plugin, the paf::ui::Dialog
 * in it, its handler and its ID, by which the exports find it. No binary names this file, the
 * class or its members; the names are ours.
 */
#ifndef COMMON_GUI_DIALOG_DIALOG_H
#define COMMON_GUI_DIALOG_DIALOG_H

#include <stdint.h>
#include <std/list>
#include <plugin.h>
#include <widget/core/handler.h>
#include <widget/w_dialog.h>
#include <widget/w_text_box.h>
#include <widget/w_scene.h>
#include <widget/w_box.h>
#include <widget/w_plane.h>
#include <widget/w_button_base.h>
#include <widget/w_scroll_view_base.h>
#include <graphics/text/text_style.h>
#include <graphics/drawobj/text_obj.h>
#include <timer/timer.h>
#include <common/transition.h>
#include <inputdevice/touchscreen/touchscreen.h>
#include <framework.h>
#include <common_gui_dialog/common_gui_dialog.h>

/* the errors the library returns */
#define COMMON_GUI_DIALOG_ERROR_NO_MEMORY	((int32_t)0x80AF0022)
#define COMMON_GUI_DIALOG_ERROR_NO_DIALOG	((int32_t)0x80AF0025)

class CommonGuiDialog {
public:
	CommonGuiDialog();
	virtual ~CommonGuiDialog();

	/* the dialog with the ID, NULL without one */
	static CommonGuiDialog *Find(int32_t id);
	/* the page priority of the dialogs opened without one */
	static int32_t GetDefaultPriority();
	static void SetDefaultPriority(int32_t priority);

	/* opens the page and the dialog of the layout: 0, or an error */
	int32_t Open(paf::Plugin *plugin, int32_t priority, bool waitTransition, bool notifyClose,
		     int32_t dimmer, int32_t pageOption, const CommonGuiDialogPage *page,
		     const paf::ui::Dialog::PartsList *parts, float *transitionTime);
	/* closes the dialog once nothing holds it and no handler runs */
	void Close();
	/* closes it at once */
	void Hide();
	/* the part by its index, 3 its scroll bar and 4 the dialog itself; NULL while closed */
	paf::ui::Widget *GetPart(int32_t index);
	void SetResult(int32_t result);
	/* waits until the dialog closes, then deletes it: its result */
	int32_t Wait();
	void SetDebugMode(bool on);
	void ShowPart(int32_t index);
	void HidePart(int32_t index);
	/* has the functor called every interval milliseconds; stops it */
	void SetInterval(CommonGuiLayoutDialogOpenHandler *functor, float interval);
	void KillInterval();
	void SetHandler(CommonGuiDialogHandler *handler);
	void DeleteReadyListeners();
	/* has the functor called once the dialog has opened */
	void AddReadyListener(CommonGuiLayoutDialogOpenHandler *functor);
	/* the buttons' labels, sounds and handlers */
	void SetButtons(const CommonGuiDialogButton *buttons);
	/* calls the handler with the button and closes the dialog */
	void Select(int32_t button);
	/* plays the dialog's sound */
	void PlaySound();

	/* the dialog's page has closed: its handler told, and the dialog deleted unless a call
	   waits for it */
	void OnTerm();
	/* the dialog has opened: the transition it waited for is over */
	static void OnOpenTimer(int32_t type, paf::ui::Handler *self, paf::ui::Event *e,
				void *data);

	/* the dialogs, the ID the next one tries first, and the page priority of the dialogs
	   opened without one */
	static paf::list<CommonGuiDialog *> s_dialogs;
	static int32_t s_nextId;
	static int32_t s_defaultPriority;

	CommonGuiDialogHandler *m_handler;
	/* the exports holding it */
	int32_t m_refCount;
	paf::Plugin *m_plugin;
	char m_pageName[16];
	paf::ui::Scene *m_page;
	paf::ui::Dialog *m_dialog;
	/* the plane behind the dialog */
	paf::ui::Widget *m_dimmer;
	int32_t m_dimmerType;
	uint8_t m_open : 1;
	uint8_t m_waiting : 1;
	uint8_t m_inHandler : 1;
	uint8_t m_notifyClose : 1;
	bool *m_done;
	uint32_t m_waitVblank;
	int32_t m_result;
	int32_t m_id;
	/* the sound's resource ID */
	uint32_t m_soundId;
};

typedef char CommonGuiDialogSize[sizeof(CommonGuiDialog) == 72 ? 1 : -1];

/* a listener that calls a method of an object */
template<class T>
class MemberListener : public paf::ui::EventListener {
public:
	typedef void (T::*Method)();

	MemberListener(T *object, Method method) : m_object(object), m_method(method)
	{
	}

	virtual ~MemberListener()
	{
	}

	virtual int32_t Do(int32_t type, paf::ui::Handler *self, paf::ui::Event *e)
	{
		if (m_killed)
			return 0x80AF4101;
		(m_object->*m_method)();
		return 0;
	}

	T *m_object;
	Method m_method;
};

/* the dialog parameters copied, all but the field nothing reads */
CommonGuiDialogParam *CopyDialogParam(CommonGuiDialogParam *dst, const CommonGuiDialogParam *src);

#endif
