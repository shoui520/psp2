/*
 * bgdl_api.cpp: the shell's entry points into its background download library (SceBgdl,
 * 0x810FC9E8-0x810FCC30): the task description's constructor and a wrapper per library call.
 * The library itself is esecron (0x810FCC30 on, esetask.cpp).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>

#include "bgdl_api.h"
#include "src/livespace_db/lsdb_keys.h"
#include "src/livespace_db/livespace_db_internal.h"
#include "src/esecron/include/esetask.hpp"

/* (0x810FC9E8) */
BgdlTaskInfo::BgdlTaskInfo()
{
	sce_paf_memset(this, 0, sizeof(*this));
	kind = 0;
	taskId = -1;
	category = 0;
	noOtherFlag = false;
	imported = false;
	progress = 0;
	progressFunc = NULL;
	progressArg = NULL;
	reserved5 = false;
	reserved6 = false;
}

/* (0x810FCA36) */
void shellBgdlCreate(bool clean, const int *quota)
{
	bgdlManagerCreate(clean, quota);
}

/* (0x810FCA40) */
void shellBgdlStart(void)
{
	startMng();
}

/* (0x810FCA48) */
void shellBgdlDestroy(void)
{
	bgdlManagerDestroy();
}

/* (0x810FCA50) */
bool shellBgdlRemoveFolders(void)
{
	return bgdlRemoveFolders();
}

/* (0x810FCA58) */
int shellBgdlNotify(void)
{
	return bgdlManagerNotify(0, 2);
}

/* (0x810FCA64) */
void shellBgdlSetPermission(void)
{
	bgdlSetPermission(3, 7);
}

/* (0x810FCA70) */
int gameAppDeletePrepare(const char *titleId)
{
	return bgdlDeletePrepare(titleId);
}

/* (0x810FCA78) */
int shellBgdlCountTitleTasks(const char *titleId)
{
	SceUInt32 count = 0;

	bgdlCountTitleTasks(titleId, &count);
	return count;
}

/* (0x810FCAA6) */
bool shellBgdlRecordHasTasks(const LsdbAppInfoPtr *record, SceUInt32 flags)
{
	return bgdlRecordHasTasks(record, flags);
}

/* (0x810FCAAE) */
bool shellBgdlTitleHasTasks(const char *titleId, SceUInt32 flags)
{
	return bgdlTitleHasTasks(titleId, flags);
}

/* (0x810FCAB6) */
bool shellBgdlTitleHasTasksOfKind(const char *titleId, int type, SceUInt32 flags)
{
	return bgdlTitleHasTasksOfKind(titleId, type, flags);
}

/* (0x810FCABE) */
int shellBgdlInstallTitleTasks(const char *titleId, BgdlTaskInfo *failed, bool *patched)
{
	return bgdlInstallTitleTasks(titleId, failed, patched);
}

/* (0x810FCAC6) */
int shellBgdlInstallTitleTasksType1(const char *titleId, BgdlTaskInfo *failed)
{
	return bgdlInstallTitleTasksType1(titleId, failed);
}

/* (0x810FCACE) */
int shellBgdlInstallTitleTasksType2(const char *titleId, BgdlTaskInfo *failed)
{
	return bgdlInstallTitleTasksType2(titleId, failed);
}

/* (0x810FCAD6) type 3 is a PSP title's */
int shellBgdlInstallTitleTasksOfKinds(const char *titleId, int type, SceUInt32 flags, bool dialog,
				      BgdlTaskInfo *failed, bool *patched)
{
	if (type == 3)
		return bgdlInstallTitleTasksType3(titleId, type, flags, failed, patched);
	return bgdlInstallTitleTasksOfKinds(titleId, type, flags, dialog, failed, patched);
}

/* (0x810FCB02) */
int shellBgdlCountAppTasks(const char *titleId)
{
	SceUInt32 count = 0;

	bgdlCountAppTasks(titleId, &count);
	return count;
}

/* (0x810FCB30) */
int shellBgdlCountTasks(void)
{
	SceUInt32 count = 0;

	bgdlCountTasks(&count, NULL);
	return count;
}

/* (0x810FCB60) */
int shellBgdlGetContentTaskInfos(paf::vector<BgdlTaskInfo> *infos)
{
	return bgdlGetContentTaskInfos(infos);
}

/* (0x810FCB68) */
int shellBgdlInstallTask(BgdlTaskInfo *info)
{
	return bgdlInstallTask(info);
}

/* (0x810FCB70) */
int shellBgdlGetTitleTaskInfos(const char *titleId, paf::vector<BgdlTaskInfo> *infos,
			       SceUInt32 flags, int type)
{
	return bgdlGetTitleTaskInfos(titleId, infos, flags, type);
}

/* (0x810FCB78) */
int shellBgdlGetTaskInfo(SceUInt32 id, BgdlTaskInfo *info)
{
	return bgdlGetTaskInfo(id, info);
}

/* (0x810FCB80) */
int shellBgdlInstallTask2(BgdlTaskInfo *info)
{
	return bgdlInstallTask2(info);
}

/* (0x810FCB88) */
void shellBgdlHold(void)
{
	bgdlHold();
}

/* (0x810FCB90) */
void shellBgdlSuspend(bool reserved)
{
	bgdlSuspend(reserved);
}

/* (0x810FCB9A) */
int shellBgdlAddPsmRuntimeTask(paf::string *iconPath)
{
	return bgdlAddPsmRuntimeTask(iconPath, 0, NULL);
}

/* (0x810FCBA6) */
bool shellBgdlGetPsmRuntimeVersion(SceUInt16 *version)
{
	return bgdlGetPsmRuntimeVersion(version);
}

/* (0x810FCBAE) */
bool shellBgdlHasPending(void)
{
	return bgdlHasPending();
}

/* (0x810FCBB6) */
int shellBgdlGetPatchVersion(const char *titleId, SceUInt8 *version)
{
	return bgdlGetPatchVersion(titleId, version);
}
