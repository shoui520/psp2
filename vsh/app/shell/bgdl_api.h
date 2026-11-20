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
	bool reserved0;			/* PLACEHOLDER: only cleared here */
	bool reserved1;
	float progress;			/* what the promote dialog's bar shows */
	int reserved3;
	int reserved4;
	bool reserved5;
	bool reserved6;
};

int shellBgdlCreate(bool clean, const int *quota);		/* (0x810FCA36) */
int shellBgdlStart(void);					/* (0x810FCA40) */
int shellBgdlDestroy(void);					/* (0x810FCA48) */
int shellBgdlRemoveFolders(void);				/* (0x810FCA50) */
int shellBgdlNotify(void);					/* (0x810FCA58) */
int shellBgdlSetPermission(void);				/* (0x810FCA64) */
int gameAppDeletePrepare(const char *titleId);			/* (0x810FCA70) */
int shellBgdlCountTitleTasks(const char *titleId);		/* (0x810FCA78) */
int shellBgdl_8110BD6E(void);					/* (0x810FCAA6) */
int shellBgdl_8110BDC0(void);					/* (0x810FCAAE) */
int shellBgdl_8110BEC2(void);					/* (0x810FCAB6) */
int shellBgdl_8110E400(void);					/* (0x810FCABE) */
int shellBgdlGetTaskInfo(const char *titleId, BgdlTaskInfo *info);	/* (0x810FCAC6) */
int shellBgdlGetTaskInfoByName(const char *name, BgdlTaskInfo *info);	/* (0x810FCACE) */
int shellBgdlAddTask(const char *url, int type, int arg2, bool arg3, int arg4, int arg5);	/* (0x810FCAD6) */
int shellBgdlCountAppTasks(const char *titleId);		/* (0x810FCB02) */
int shellBgdlCountTasks(void);					/* (0x810FCB30) */
int shellBgdl_8110ED40(int arg);				/* (0x810FCB60) */
int shellBgdl_8110ED62(void);					/* (0x810FCB68) */
int shellBgdl_8110ED8A(const char *arg0, int arg1, int arg2, int arg3);	/* (0x810FCB70) */
int shellBgdl_8110F018(void);					/* (0x810FCB78) */
int shellBgdl_8110F03C(void);					/* (0x810FCB80) */
int shellBgdl_8110F1DE(void);					/* (0x810FCB88) */
int shellBgdl_8110F1F4(bool arg);				/* (0x810FCB90) */
int shellBgdl_8110F208(int arg);				/* (0x810FCB9A) */
int shellBgdl_8110F3A2(void);					/* (0x810FCBA6) */
int shellBgdl_8110F718(void);					/* (0x810FCBAE) */
int shellBgdl_8110FB04(void);					/* (0x810FCBB6) */

#endif
