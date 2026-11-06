/*
 * The progress callback sceUpdaterSetProgressCallback sets, which the package update calls
 * every interval steps with sceUpdaterGetProgress's progress. The names are ours.
 */
#ifndef UPDATER_CORE_UPDATER_PROGRESS_H
#define UPDATER_CORE_UPDATER_PROGRESS_H

#include <scetypes.h>
#include <updater_core/updater_core.h>

extern SceUpdaterProgressCallback g_updaterProgressCallback;
extern SceUInt32 g_updaterProgressInterval;

#endif
