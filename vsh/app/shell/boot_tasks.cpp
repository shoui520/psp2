/*
 * Tasks the shell's boot starts: the calibration update, the clean-up of crash dumps left in
 * ux0:data/, and the daily refresh, a cron entry that has the main thread launch NPXS10079 after
 * a 300-tick countdown. The file's and every name here are ours (no record names them).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <common/main_thread_call_list.h>
#include <iofilemgr/iofilemgr.h>
#include <regmgr/regmgr.h>
#include <appmgr/appmgr.h>
#include <vshbridge/vshbridge.h>
#include "shell.h"
#include "shell_cron.h"

using namespace paf;

/* the main-thread ticks the refresh waits before launching */
#define REFRESH_COUNTDOWN	300

void shellCalibrationUpdateStart(void)
{
	shellCalibrationUpdateThreadCreate();
}

/* removes the crash dumps in ux0:data/, unless the unit keeps them */
void shellCoredumpCleanup(void)
{
	SceIoDirent dirent;
	char path[265];
	bool keep;

	if (!vshSblAimgrIsCEX()) {
		keep = true;
	} else if (!vshSblQafMgrIsAllowKeepCoreFile()) {
		keep = false;
	} else {
		int value = 0;

		keep = false;
		if (sceRegMgrGetKeyInt("/CONFIG/COREDUMP/CRASHREPORT", "keep_corefiles", &value) == 0)
			keep = value == 1;
	}

	SceUID fd = sceIoDopen("ux0:data/");

	if (fd < 0)
		return;

	while (sceIoDread(fd, &dirent) > 0) {
		size_t len = sce_paf_strlen(dirent.d_name);

		if (len > 255 || len < 13)
			continue;

		sce_paf_memset(path, 0, sizeof(path));
		sce_paf_strncpy(path, "ux0:data/", 9);
		sce_paf_strncpy(path + 9, dirent.d_name, len);
		if ((sce_paf_strncmp(path + len, ".spsp2dmp", 9) == 0 ||
		     sce_paf_strncmp(path + len + 9 - 13, ".spsp2dmp.tmp", 13) == 0) && !keep)
			sceIoRemove(path);
	}
	sceIoDclose(fd);
}

static void shellRefreshCountdown(void *userdata);
static int shellRefreshCronCallback(SceUID handle, void *arg);

void shellRefreshCronStart(void)
{
	ShellCron *cron = shellCronGetInstance();
	SceUID handle;

	if (cron != NULL)
		cron->AddDaily(&handle, shellRefreshCronCallback, NULL, cron->GetDailyTime(),
			       SHELL_CRON_EVERY_DAY, NULL);
}

/* each main-thread tick counts down; at the end NPXS10079 is launched */
static void shellRefreshCountdown(void *userdata)
{
	int *countdown = (int *)userdata;

	if (countdown == NULL) {
		common::MainThreadCallList::Unregister(shellRefreshCountdown, NULL);
		return;
	}
	if (--*countdown < 0) {
		sceAppMgrLaunchAppByName(0, "NPXS10079", NULL);
		common::MainThreadCallList::Unregister(shellRefreshCountdown, countdown);
		delete countdown;
	}
}

static int shellRefreshCronCallback(SceUID handle, void *arg)
{
	int *countdown = new int;

	if (countdown != NULL) {
		*countdown = REFRESH_COUNTDOWN;
		common::MainThreadCallList::Register(shellRefreshCountdown, countdown);
	}
	return 0;
}
