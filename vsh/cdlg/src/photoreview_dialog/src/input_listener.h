/*
 * The pad's input for the objects that ask for it: one paf input listener, while it exists, hands
 * each frame's pad data to the handlers added and enabled. Sony's photo apps and the image
 * dialogs build the same code; no binary names it, so the classes, the functions and this file's
 * name are ours.
 */
#ifndef PHOTOREVIEW_DIALOG_INPUT_LISTENER_H
#define PHOTOREVIEW_DIALOG_INPUT_LISTENER_H

#include <stdint.h>
#include <std/list>
#include <inputdevice/inputdevice.h>
#include <inputdevice/inputlistener.h>

/* what hears the pad */
class InputHandler {
public:
	virtual ~InputHandler();

	virtual void OnPadInput(const paf::inputdevice::pad::Data *data, const paf::inputdevice::pad::Data *pre) = 0;
};

class PadInputListener : public paf::inputdevice::InputGestureListener {
public:
	class Entry {
	public:
		InputHandler *handler;
		bool enabled;
	};

	PadInputListener() : m_disabled(0)
	{
	}

	/* each frame: the pad's data, now and before, to every enabled handler */
	virtual void OnUpdate(const paf::inputdevice::Data *data)
	{
		const paf::inputdevice::pad::Data *pad;
		const paf::inputdevice::pad::Data *pre;

		if (data != NULL && (pad = data->m_pad_data) != NULL &&
		    (pre = data->m_pad_data_pre) != NULL && m_disabled == 0) {
			paf::list<Entry>::iterator it;

			for (it = m_handlers.begin(); it != m_handlers.end(); it++) {
				if (it->enabled)
					it->handler->OnPadInput(pad, pre);
			}
		}
	}

	paf::list<Entry> m_handlers;
	/* nonzero: no handler hears the pad */
	int32_t m_disabled;
};

void AddInputHandler(InputHandler *handler);
void RemoveInputHandler(InputHandler *handler);
void EnableInputHandler(InputHandler *handler);
void DisableInputHandler(InputHandler *handler);
void CreateInputListener(void);
void DestroyInputListener(void);
void StartInputListener(void);
void StopInputListener(void);

#endif
