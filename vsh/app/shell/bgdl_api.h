/*
 * The shell's background downloads (SceBgdl: "SceBgdlManagerThread", "SceBgdlMutexEse*", the
 * ux0:bgdl and ur0:bgdl task folders): the entry points the rest of the shell calls
 * (bgdl_api.cpp, 0x810FC9E8-0x810FCC30) and a task's description. No record names the files,
 * the functions or the types: the names are ours (PLACEHOLDER).
 */
#ifndef SHELL_BGDL_API_H
#define SHELL_BGDL_API_H

#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <std/map>
#include <common/shared_ptr.h>

/* a download task as the bgdl library describes it to the shell (0x5C bytes) */
struct BgdlTaskInfo {
	BgdlTaskInfo();

	int taskId;
	paf::string url;		/* the task's (0x8110E99A copies it into both) */
	paf::string path;
	paf::string title;		/* the param file's entry 106 */
	paf::string contentId;		/* entry 230, when entry 239 is set */
	paf::string titleId;		/* entry 105 */
	int kind;			/* 0-3, from the task's flags (bits 23-25) */
	int category;			/* 8 or 16, from the task's type */
	bool noOtherFlag;		/* PLACEHOLDER names: the install's GameInstallExParam flags */
	bool imported;
	float progress;			/* what the promote dialog's bar shows */
	void (*progressFunc)(void *arg, float percent);	/* the install's progress, when set */
	void *progressArg;
	bool unusedFlag;
	bool needsRestart;
};

void shellBgdlCreate(bool clean, const int *quota);		/* (0x810FCA36) */
void shellBgdlStart(void);					/* (0x810FCA40) */
void shellBgdlDestroy(void);					/* (0x810FCA48) */
bool shellBgdlRemoveFolders(void);				/* (0x810FCA50) */
int shellBgdlNotify(void);					/* (0x810FCA58) */
void shellBgdlSetPermission(void);				/* (0x810FCA64) */
int gameAppDeletePrepare(const char *titleId);			/* (0x810FCA70) */
int shellBgdlCountTitleTasks(const char *titleId);		/* (0x810FCA78) */
/* (0x810FCAA6) */
bool shellBgdlRecordHasTasks(const paf::common::SharedPtr<paf::map<unsigned int, struct LsdbValue> > *record,
			     SceUInt32 flags);
bool shellBgdlTitleHasTasks(const char *titleId, SceUInt32 flags);	/* (0x810FCAAE) */
bool shellBgdlTitleHasTasksOfKind(const char *titleId, int type, SceUInt32 flags);	/* (0x810FCAB6) */
int shellBgdlInstallTitleTasks(const char *titleId, BgdlTaskInfo *failed, bool *patched);	/* (0x810FCABE) */
int shellBgdlInstallTitleTasksPsp(const char *titleId, BgdlTaskInfo *failed);	/* (0x810FCAC6) */
int shellBgdlInstallTitleTasksPsm(const char *titleId, BgdlTaskInfo *failed);	/* (0x810FCACE) */
/* (0x810FCAD6) */
int shellBgdlInstallTitleTasksOfKinds(const char *titleId, int type, SceUInt32 flags, bool dialog,
				      BgdlTaskInfo *failed, bool *patched);
int shellBgdlCountAppTasks(const char *titleId);		/* (0x810FCB02) */
int shellBgdlCountTasks(void);					/* (0x810FCB30) */
int shellBgdlGetContentTaskInfos(paf::vector<BgdlTaskInfo> *infos);	/* (0x810FCB60) */
int shellBgdlInstallTask(BgdlTaskInfo *info);			/* (0x810FCB68) */
/* (0x810FCB70) */
int shellBgdlGetTitleTaskInfos(const char *titleId, paf::vector<BgdlTaskInfo> *infos,
			       SceUInt32 flags, int type);
int shellBgdlGetTaskInfo(SceUInt32 id, BgdlTaskInfo *info);	/* (0x810FCB78) */
int shellBgdlInstallTask2(BgdlTaskInfo *info);			/* (0x810FCB80) */
void shellBgdlHold(void);					/* (0x810FCB88) */
void shellBgdlSuspend(bool reserved);				/* (0x810FCB90) */
int shellBgdlAddPsmRuntimeTask(paf::string *iconPath);		/* (0x810FCB9A) */
bool shellBgdlGetPsmRuntimeVersion(SceUInt16 *version);		/* (0x810FCBA6) */
bool shellBgdlHasPending(void);					/* (0x810FCBAE) */
int shellBgdlGetPatchVersion(const char *titleId, SceUInt8 *version);	/* (0x810FCBB6) */

#endif
