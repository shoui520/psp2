/*
 * A widget's gesture events passed to an object's virtual methods: a GestureAttacher registers
 * the eight gesture events on a widget and hands each to its owner, a GestureReceiver, whose
 * methods do nothing until a subclass overrides them; a GestureHandler is both. The photo apps
 * and the image dialogs build the same code; no binary names it, so the classes, their methods
 * and this file's name are ours. The methods are named after the low byte of the event they get
 * (0x200NN, the widget's gesture events), whose Sony names are not known.
 */
#ifndef PHOTOREVIEW_DIALOG_GESTURE_HANDLER_H
#define PHOTOREVIEW_DIALOG_GESTURE_HANDLER_H

#include <stdint.h>
#include <widget/core/handler.h>
#include <widget/core/event.h>
#include <widget/core/widget.h>

/* the widget's gesture events a GestureAttacher registers */
enum {
	GESTURE_EVENT_01 = 0x20001,
	GESTURE_EVENT_04 = 0x20004,
	GESTURE_EVENT_05 = 0x20005,
	GESTURE_EVENT_13 = 0x20013,
	GESTURE_EVENT_14 = 0x20014,
	GESTURE_EVENT_15 = 0x20015,
	GESTURE_EVENT_16 = 0x20016,
	GESTURE_EVENT_18 = 0x20018
};

/* what is given a widget's gesture events */
class GestureReceiver {
public:
	virtual ~GestureReceiver();

	virtual void OnGesture04(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnGesture05(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnGesture01(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnGesture13(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnGesture14(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnGesture15(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnGesture16(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnGesture18(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
};

/* registers a widget's gesture events for its owner */
class GestureAttacher {
public:
	GestureAttacher();
	virtual ~GestureAttacher();

	void SetOwner(GestureReceiver *owner);
	/* the widget's gesture events, with the gestures of mask added to its touches': 0, or
	   -1 without a widget */
	int Attach(paf::ui::Widget *widget, uint32_t mask);

	static void Gesture04CB(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *data);
	static void Gesture05CB(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *data);
	static void Gesture01CB(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *data);
	static void Gesture13CB(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *data);
	static void Gesture14CB(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *data);
	static void Gesture15CB(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *data);
	static void Gesture16CB(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *data);
	static void Gesture18CB(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *data);

	GestureReceiver *m_owner;
};

/* a receiver that attaches itself */
class GestureHandler : public GestureReceiver, public GestureAttacher {
public:
	GestureHandler();
};

#endif
