/*
 * SceHdmi: accessors of the EDID manager's state. No debug record names this
 * file; the names are ours.
 */
#include <psp2kern/types.h>
#include "hdmi_internal.h"

EdidManager edidManager;

int edidManagerIsValid(void)
{
	return edidManager.valid;
}

int edidManagerIsSonyD702(void)
{
	return edidManager.isSonyD702;
}

SceHdmiEdidInfo *edidManagerGetInfo(void)
{
	return &edidManager.info;
}
