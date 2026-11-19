/*
 * applauncher_started.cpp: the home's start marked done (0x810B3B36-0x810B3BBA). The file's name
 * and its names are ours (PLACEHOLDER): no record or string of the 3.6x shell names them.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <job/job.h>
#include <common/shared_ptr.h>
#include "../../livespace_db/lsdb_keys.h"
#include "applauncher.h"
#include "applauncher_home.h"

/* the home started once (0x81544270): set, never read */
static bool s_started = false;

/* the home's start marked done, once (0x810B3B36) */
void applauncherPagesSetStarted(void)
{
	if (!s_started)
		s_started = true;
}
