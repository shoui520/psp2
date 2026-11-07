/*
 * The pad listener the image dialogs share: made on the plugin's Init, added to paf's input on
 * Start and taken off on Stop, destroyed on Exit. Names ours; see input_listener.h.
 */
#include <stddef.h>
#include <stdint.h>
#include <std/new>
#include <std/list>
#include <common/shared_ptr.h>
#include <inputdevice/inputlistener.h>
#include "input_listener.h"

using namespace paf;

/* the listener while it exists, and paf's reference to it */
static PadInputListener *s_listener = NULL;
static common::SharedPtr<inputdevice::InputListener> s_listenerPtr;

InputHandler::~InputHandler()
{
}

void AddInputHandler(InputHandler *handler)
{
	if (s_listener != NULL) {
		PadInputListener::Entry entry;

		entry.handler = handler;
		entry.enabled = false;
		s_listener->m_handlers.push_back(entry);
	}
}

void RemoveInputHandler(InputHandler *handler)
{
	PadInputListener *listener = s_listener;

	if (listener != NULL) {
		list<PadInputListener::Entry>::iterator it;

		for (it = listener->m_handlers.begin(); it != listener->m_handlers.end(); ++it) {
			if (it->handler == handler) {
				listener->m_handlers.erase(it);
				return;
			}
		}
	}
}

void EnableInputHandler(InputHandler *handler)
{
	if (s_listener != NULL) {
		list<PadInputListener::Entry>::iterator it;

		for (it = s_listener->m_handlers.begin(); it != s_listener->m_handlers.end(); ++it) {
			if (it->handler == handler) {
				it->enabled = true;
				return;
			}
		}
	}
}

void DisableInputHandler(InputHandler *handler)
{
	if (s_listener != NULL) {
		list<PadInputListener::Entry>::iterator it;

		for (it = s_listener->m_handlers.begin(); it != s_listener->m_handlers.end(); ++it) {
			if (it->handler == handler) {
				it->enabled = false;
				return;
			}
		}
	}
}

void CreateInputListener(void)
{
	s_listener = new PadInputListener();
	s_listenerPtr = common::SharedPtr<inputdevice::InputListener>(s_listener);
}

void DestroyInputListener(void)
{
	s_listener = NULL;
	s_listenerPtr.reset();
}

void StartInputListener(void)
{
	if (s_listenerPtr.get() != NULL)
		inputdevice::AddInputListener(s_listenerPtr);
}

void StopInputListener(void)
{
	if (s_listenerPtr.get() != NULL)
		inputdevice::DelInputListener(s_listenerPtr);
}
