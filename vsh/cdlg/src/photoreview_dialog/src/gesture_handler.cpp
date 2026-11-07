/*
 * A widget's gesture events passed to an object's virtual methods. Names ours; see
 * gesture_handler.h.
 */
#include <stddef.h>
#include <stdint.h>
#include <std/new>
#include <widget/core/handler.h>
#include <widget/core/event.h>
#include <widget/core/widget.h>
#include "gesture_handler.h"

using namespace paf;

GestureHandler::GestureHandler()
{
	SetOwner(this);
}

GestureReceiver::~GestureReceiver()
{
}

void GestureReceiver::OnGesture04(int32_t type, ui::Handler *self, ui::Event *e)
{
}

void GestureReceiver::OnGesture05(int32_t type, ui::Handler *self, ui::Event *e)
{
}

void GestureReceiver::OnGesture01(int32_t type, ui::Handler *self, ui::Event *e)
{
}

void GestureReceiver::OnGesture13(int32_t type, ui::Handler *self, ui::Event *e)
{
}

void GestureReceiver::OnGesture14(int32_t type, ui::Handler *self, ui::Event *e)
{
}

void GestureReceiver::OnGesture15(int32_t type, ui::Handler *self, ui::Event *e)
{
}

void GestureReceiver::OnGesture16(int32_t type, ui::Handler *self, ui::Event *e)
{
}

void GestureReceiver::OnGesture18(int32_t type, ui::Handler *self, ui::Event *e)
{
}

__attribute__((noinline)) GestureAttacher::GestureAttacher() : m_owner(NULL)
{
}

GestureAttacher::~GestureAttacher()
{
}

__attribute__((noinline)) void GestureAttacher::SetOwner(GestureReceiver *owner)
{
	m_owner = owner;
}

int GestureAttacher::Attach(ui::Widget *widget, uint32_t mask)
{
	if (widget == NULL)
		return -1;
	if (!(widget->m_event_enable & 0x400))
		widget->EnableFocusEvent(true);
	widget->m_event_enable |= 0xFFF0;
	widget->SetGestureMask(widget->m_touch_gesture_mask | mask, true);
	widget->AddEventListener(GESTURE_EVENT_04, new ui::EventCBListener(Gesture04CB, this), true);
	widget->AddEventListener(GESTURE_EVENT_05, new ui::EventCBListener(Gesture05CB, this), true);
	widget->AddEventListener(GESTURE_EVENT_01, new ui::EventCBListener(Gesture01CB, this), true);
	widget->AddEventListener(GESTURE_EVENT_13, new ui::EventCBListener(Gesture13CB, this), true);
	widget->AddEventListener(GESTURE_EVENT_14, new ui::EventCBListener(Gesture14CB, this), true);
	widget->AddEventListener(GESTURE_EVENT_15, new ui::EventCBListener(Gesture15CB, this), true);
	widget->AddEventListener(GESTURE_EVENT_16, new ui::EventCBListener(Gesture16CB, this), true);
	widget->AddEventListener(GESTURE_EVENT_18, new ui::EventCBListener(Gesture18CB, this), true);
	return 0;
}

void GestureAttacher::Gesture04CB(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	GestureAttacher *attacher = (GestureAttacher *)data;

	if (attacher != NULL && attacher->m_owner != NULL)
		attacher->m_owner->OnGesture04(type, self, e);
}

void GestureAttacher::Gesture05CB(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	GestureAttacher *attacher = (GestureAttacher *)data;

	if (attacher != NULL && attacher->m_owner != NULL)
		attacher->m_owner->OnGesture05(type, self, e);
}

void GestureAttacher::Gesture01CB(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	GestureAttacher *attacher = (GestureAttacher *)data;

	if (attacher != NULL && attacher->m_owner != NULL)
		attacher->m_owner->OnGesture01(type, self, e);
}

void GestureAttacher::Gesture13CB(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	GestureAttacher *attacher = (GestureAttacher *)data;

	if (attacher != NULL && attacher->m_owner != NULL)
		attacher->m_owner->OnGesture13(type, self, e);
}

void GestureAttacher::Gesture14CB(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	GestureAttacher *attacher = (GestureAttacher *)data;

	if (attacher != NULL && attacher->m_owner != NULL)
		attacher->m_owner->OnGesture14(type, self, e);
}

void GestureAttacher::Gesture15CB(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	GestureAttacher *attacher = (GestureAttacher *)data;

	if (attacher != NULL && attacher->m_owner != NULL)
		attacher->m_owner->OnGesture15(type, self, e);
}

void GestureAttacher::Gesture16CB(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	GestureAttacher *attacher = (GestureAttacher *)data;

	if (attacher != NULL && attacher->m_owner != NULL)
		attacher->m_owner->OnGesture16(type, self, e);
}

void GestureAttacher::Gesture18CB(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	GestureAttacher *attacher = (GestureAttacher *)data;

	if (attacher != NULL && attacher->m_owner != NULL)
		attacher->m_owner->OnGesture18(type, self, e);
}
