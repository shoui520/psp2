/*
 * bgdl_api.cpp: the shell's entry points into its background download library (SceBgdl,
 * 0x810FC9E8-0x810FCC30): the task description's constructor and a wrapper per library call.
 * The library itself (0x810FCC30 on) is not written yet: its functions are named by their
 * addresses until it is (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>

#include "bgdl_api.h"
#include "src/livespace_db/lsdb_keys.h"

/* the library (PLACEHOLDER names: the functions are not written yet) */
int bgdlManagerCreate(bool clean, const int *quota);		/* 0x811041EC */
int bgdlManagerStart(void);					/* 0x811042C0 */
int bgdlManagerRemoveFolders(void);				/* 0x81104346 */
int bgdlManagerDestroy(void);					/* 0x81104474 */
int bgdlManagerSetPermission(int arg0, int arg1);		/* 0x81104574 */
int bgdlManagerNotify(int arg0, int arg1);			/* 0x810FD862 */
int bgdlCountTitleTasks(const char *titleId, SceUInt32 *count);	/* 0x8110B7E2 */
int Bgdl_8110BD6E(void);
int Bgdl_8110BDC0(void);
int Bgdl_8110BEC2(void);
int bgdlCountAppTasks(const char *titleId, SceUInt32 *count);		/* 0x8110BFE4 */
int bgdlDeletePrepare(const char *titleId);			/* 0x8110CD86 */
int Bgdl_8110E400(void);
int bgdlGetTaskInfo(const char *titleId, BgdlTaskInfo *info);	/* 0x8110E430 */
int bgdlGetTaskInfoByName(const char *name, BgdlTaskInfo *info);	/* 0x8110E45E */
int bgdlAddPspTask(const char *url, int type, int arg2, int arg4, int arg5);	/* 0x8110E48C */
int bgdlAddTask(const char *url, int type, int arg2, bool arg3, int arg4, int arg5);	/* 0x8110E6DE */
int bgdlCountTasks(int *count, void *list);			/* 0x8110E884 */
int Bgdl_8110ED40(int arg);
int Bgdl_8110ED62(void);
int Bgdl_8110ED8A(const char *arg0, int arg1, int arg2, int arg3);
int Bgdl_8110F018(void);
int Bgdl_8110F03C(void);
int Bgdl_8110F1DE(void);
int Bgdl_8110F1F4(bool arg);
int Bgdl_8110F208(int arg0, int arg1, int arg2);
int Bgdl_8110F3A2(void);
int Bgdl_8110F718(void);
int Bgdl_8110FB04(void);

/* (0x810FC9E8) */
BgdlTaskInfo::BgdlTaskInfo()
{
	sce_paf_memset(this, 0, sizeof(*this));
	kind = 0;
	taskId = -1;
	category = 0;
	reserved0 = false;
	reserved1 = false;
	progress = 0;
	reserved3 = 0;
	reserved4 = 0;
	reserved5 = false;
	reserved6 = false;
}

/* (0x810FCA36) */
int shellBgdlCreate(bool clean, const int *quota)
{
	return bgdlManagerCreate(clean, quota);
}

/* (0x810FCA40) */
int shellBgdlStart(void)
{
	return bgdlManagerStart();
}

/* (0x810FCA48) */
int shellBgdlDestroy(void)
{
	return bgdlManagerDestroy();
}

/* (0x810FCA50) */
int shellBgdlRemoveFolders(void)
{
	return bgdlManagerRemoveFolders();
}

/* (0x810FCA58) */
int shellBgdlNotify(void)
{
	return bgdlManagerNotify(0, 2);
}

/* (0x810FCA64) */
int shellBgdlSetPermission(void)
{
	return bgdlManagerSetPermission(3, 7);
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
int shellBgdl_8110BD6E(void)
{
	return Bgdl_8110BD6E();
}

/* (0x810FCAAE) */
int shellBgdl_8110BDC0(void)
{
	return Bgdl_8110BDC0();
}

/* (0x810FCAB6) */
int shellBgdl_8110BEC2(void)
{
	return Bgdl_8110BEC2();
}

/* (0x810FCABE) */
int shellBgdl_8110E400(void)
{
	return Bgdl_8110E400();
}

/* (0x810FCAC6) */
int shellBgdlGetTaskInfo(const char *titleId, BgdlTaskInfo *info)
{
	return bgdlGetTaskInfo(titleId, info);
}

/* (0x810FCACE) */
int shellBgdlGetTaskInfoByName(const char *name, BgdlTaskInfo *info)
{
	return bgdlGetTaskInfoByName(name, info);
}

/* (0x810FCAD6) type 3 is a PSP title's task */
int shellBgdlAddTask(const char *url, int type, int arg2, bool arg3, int arg4, int arg5)
{
	if (type == 3)
		return bgdlAddPspTask(url, type, arg2, arg4, arg5);
	return bgdlAddTask(url, type, arg2, arg3, arg4, arg5);
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
	int count = 0;

	bgdlCountTasks(&count, NULL);
	return count;
}

/* (0x810FCB60) */
int shellBgdl_8110ED40(int arg)
{
	return Bgdl_8110ED40(arg);
}

/* (0x810FCB68) */
int shellBgdl_8110ED62(void)
{
	return Bgdl_8110ED62();
}

/* (0x810FCB70) */
int shellBgdl_8110ED8A(const char *arg0, int arg1, int arg2, int arg3)
{
	return Bgdl_8110ED8A(arg0, arg1, arg2, arg3);
}

/* (0x810FCB78) */
int shellBgdl_8110F018(void)
{
	return Bgdl_8110F018();
}

/* (0x810FCB80) */
int shellBgdl_8110F03C(void)
{
	return Bgdl_8110F03C();
}

/* (0x810FCB88) */
int shellBgdl_8110F1DE(void)
{
	return Bgdl_8110F1DE();
}

/* (0x810FCB90) */
int shellBgdl_8110F1F4(bool arg)
{
	return Bgdl_8110F1F4(arg);
}

/* (0x810FCB9A) */
int shellBgdl_8110F208(int arg)
{
	return Bgdl_8110F208(arg, 0, 0);
}

/* (0x810FCBA6) */
int shellBgdl_8110F3A2(void)
{
	return Bgdl_8110F3A2();
}

/* (0x810FCBAE) */
int shellBgdl_8110F718(void)
{
	return Bgdl_8110F718();
}

/* (0x810FCBB6) */
int shellBgdl_8110FB04(void)
{
	return Bgdl_8110FB04();
}
