/*
 * applauncher_input.cpp: the listener of the home's input (0x810C9B58-0x810C9BEE). The file's
 * name is ours (PLACEHOLDER).
 */
#include <inputdevice/touchscreen/touchscreen.h>
#include "applauncher_home.h"

/* listening to the pad (0x810C9B58) */
ApplauncherInputListener::ApplauncherInputListener()
	: paf::inputdevice::InputListener(paf::inputdevice::DEVICE_TYPE_PAD)
{
}

/* PLACEHOLDER: an empty function nothing calls (0x810C9B6E) */
void applauncherInputReserved(void)
{
}

/* a touch of the front screen: an icon's drag left (0x810C9B70) */
void ApplauncherInputListener::OnUpdate(const paf::inputdevice::Data *data)
{
	if (data->m_touch_data != NULL &&
	    data->m_touch_data->state != paf::inputdevice::touchscreen::Data::STATE_NONE)
		applauncherIconCancelDrag();
}
