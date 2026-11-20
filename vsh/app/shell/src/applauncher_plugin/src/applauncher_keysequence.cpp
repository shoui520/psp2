/*
 * applauncher_keysequence.cpp: a pad listener calling back once its keys are pressed in order
 * (0x810DB87C-0x810DB96C). The file's name is ours (PLACEHOLDER): no record of the 3.6x shell
 * names it.
 */

#include <scetypes.h>
#include <inputdevice/inputdevice.h>
#include <inputdevice/inputlistener.h>
#include <inputdevice/pad/pad.h>
#include "../../livespace_db/lsdb_keys.h"
#include "applauncher_iconinfo.h"

using namespace paf;

/* (0x810DB87C) */
ApplauncherKeySequenceListener::ApplauncherKeySequenceListener(const unsigned int *keys,
	int keyCount, Callback callback, void *arg)
	: inputdevice::InputListener(inputdevice::DEVICE_TYPE_PAD), m_matched(0), m_keys(keys),
	  m_keyCount(keyCount), m_callback(callback), m_arg(arg), m_enabled(true)
{
}

/*
 * The keys changed: the next key of the sequence pressed moves it on, its first key starts it
 * again, and its last calls back from the start (0x810DB8A8).
 */
void ApplauncherKeySequenceListener::OnUpdate(const inputdevice::Data *data)
{
	if (!m_enabled)
		return;

	unsigned int buttons = data->m_pad_data_pre->buttons;

	if (data->m_pad_data->buttons == buttons)
		return;
	buttons &= ~inputdevice::pad::Data::PAD_KEY_OPEN;

	if (m_keys[m_matched] == buttons) {
		if (m_matched + 1 == m_keyCount) {
			if (m_callback != NULL) {
				m_matched = 0;
				m_callback(m_arg);
			}
		} else {
			m_matched++;
		}
	} else if (m_keys[0] == buttons) {
		m_matched = 1;
	}
}
