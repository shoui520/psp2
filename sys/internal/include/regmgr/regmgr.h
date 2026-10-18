#pragma once

#include <scetypes.h>

#define REGMGR_WORD_SIZE	0x100

/* a change, as the notify function gets it (names unknown) */
typedef struct RegMgrKeyInfo {
	SceSize size;
	const char *name;
	SceUInt16 type;
	SceUInt16 valueSize;
	union {
		int value;
		const void *data;
	};
} RegMgrKeyInfo;

/* a key and its value, as SceRegMgrForDebugger_53970232's function gets it (names unknown) */
typedef struct RegMgrNotifyInfo {
	SceSize size;			/* REGMGR_NOTIFY_INFO_SIZE */
	char path[REGMGR_WORD_SIZE];
	SceUInt16 type;
	SceUInt16 valueSize;
	int value;
	char data[0x7c0];
} RegMgrNotifyInfo;

_Static_assert(sizeof(RegMgrNotifyInfo) == 0x8cc, "RegMgrNotifyInfo");

int sceRegMgrGetKeyBin(const char *category, const char *name,
		     void *buffer, SceSize size);
int sceRegMgrSetKeyBin(const char *category, const char *name,
		     const void *buffer, SceSize size);

int sceRegMgrSetKeyInt(const char *category, const char *name, int value);
int sceRegMgrGetKeyInt(const char *category, const char *name, int *value);

int sceRegMgrSystemParamGetInt(int id, int *value);

int SceRegMgrForDebugger_46AC0CCB(int cmd, void *arg);
int SceRegMgrForDebugger_8AE5757E(SceSize *uisize);
int SceRegMgrForDebugger_8E798E47(void);
int SceRegMgrForDebugger_B9A0BB77(void);
int SceRegMgrForDebugger_C588D3E7(void);
int SceRegMgrForDebugger_D668ABD2(SceSize *uisize);
int SceRegMgrForDebugger_53970232(int (*func)(RegMgrNotifyInfo *info), RegMgrNotifyInfo *info);
int SceRegMgrForDebugger_810D2F09(const RegMgrKeyInfo *info);
int SceRegMgrForDebugger_C64C67A8(RegMgrKeyInfo *info);
int SceRegMgrForTool_0B623D0B(void);
int SceRegMgrForTool_0CFA929B(int region, SceUInt32 config);
int SceRegMgrForTool_2A76051A(const char *category, int *handle);
int SceRegMgrForTool_2D415472(int handle, int *num);
int SceRegMgrForTool_6276E7D8(int handle, const char *name, int *type, int *size);
int SceRegMgrForTool_6808B7AD(void);
int SceRegMgrForTool_B441660B(int handle);
int SceRegMgrForTool_C689E36C(int handle, const char *name, void *buf, int size);
int SceRegMgrForTool_E6E81558(int level);
int SceRegMgrForTool_F55E6E4F(int *num);
int sceRegMgrAddRegistryCallback(SceUID cbid, const char *path);
int sceRegMgrDbBackup(const char *path, int level);
int sceRegMgrDbRestore(const char *path);
int sceRegMgrGetKeyStr(const char *category, const char *name, char *buf, SceSize size);
int sceRegMgrGetRegVersion(void);
int sceRegMgrIsBlueScreen(int error);
int sceRegMgrRegisterCallback(SceUID cbid);
int sceRegMgrRegisterDrvErrCallback(SceUID cbid);
int sceRegMgrResetRegistryLv(const char *category, int level);
int sceRegMgrSetKeyStr(const char *category, const char *name, const char *buf, SceSize size);
int sceRegMgrSrvCnvRegionInt(const char *name);
int sceRegMgrSrvCnvRegionPsCode(int region);
int sceRegMgrSrvCnvRegionStr(int region, char *buf, SceSize size);
int sceRegMgrSrvGetRegion(void);
int sceRegMgrSrvGetRegionStr(char *buf, SceSize size);
int sceRegMgrStartCallback(SceUID cbid);
int sceRegMgrStopCallback(SceUID cbid);
int sceRegMgrSystemIsBlueScreen(int error);
int sceRegMgrSystemParamGetBin(int id, void *buf, SceSize size);
int sceRegMgrSystemParamGetStr(int id, char *buf, SceSize size);
int sceRegMgrSystemParamSetBin(int id, const void *buf, SceSize size);
int sceRegMgrSystemParamSetInt(int id, int value);
int sceRegMgrSystemParamSetStr(int id, const char *buf, SceSize size);
int sceRegMgrUnregisterCallback(SceUID cbid);
int sceRegMgrUnregisterDrvErrCallback(SceUID cbid);
int sceRegMgrUtilityGetBin(int id, void *buf, SceSize size);
int sceRegMgrUtilityGetInt(int id, int *value);
int sceRegMgrUtilityGetStr(int id, char *buf, SceSize size);
int sceRegMgrUtilitySetBin(int id, const void *buf, SceSize size);
int sceRegMgrUtilitySetInt(int id, int value);
int sceRegMgrUtilitySetStr(int id, const char *buf, SceSize size);

/* the kernel APIs */
int sceRegMgrGetKeysInfo(const char *category, RegMgrKeyInfo *info, int count);
int sceRegMgrSetKeys(const char *category, RegMgrKeyInfo *info, int count);
int sceRegMgrGetKeys(const char *category, RegMgrKeyInfo *info, int count);
int sceRegMgrGetInitVals(const char *category, RegMgrKeyInfo *info, int count);
