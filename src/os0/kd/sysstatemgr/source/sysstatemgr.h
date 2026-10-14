/* SceSysStateMgr private interfaces and configuration-reader state. */
#pragma once

#include <psp2kern/types.h>
#include <psp2kern/io/fcntl.h>
#include <psp2kern/io/stat.h>
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/sysmem/heap.h>
#include <psp2kern/kernel/sysmem/address_space.h>
#include <psp2kern/kernel/sysroot.h>
#include <psp2kern/kernel/dipsw.h>
#include <psp2kern/kernel/aimgr.h>
#include "sce/debug.h"
#include "sce/qafmgr.h"

#define CONF_ERROR(n) ((int)(0x80010000U | (n)))
#define CONF_NOT_FOUND ((int)0xffff0001U)
#define CONF_MEMTYPE SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_RW

/* Kernel-only imports not declared by the public SDK headers. */
int sceKernelWaitProcessEnd(SceUID pid, int *status, void *timeout);
int SceProcessmgrForKernel_5137B783(SceUID pid, SceSize len, const void *args, void *opt);
int ksceKernelCreateProcess(const char *name, int type, const char *path, void *opt);
int sceKernelDelayThread(SceUInt32 delay);
SceUInt64 sceKernelGetSystemTimeWide(void);
int ksceKernelInvokeInitCallback(int stage);
int SceSysmemForKernel_16A54BFE(void);
SceUID SceSysmemForKernel_0164D817(void);
int sceKernelGetPhyMemPartInfoCore(SceUID part, void *info);
int sceKernelSetGPO(int value);
unsigned ksceSysrootGetHardwareInfo(void);
int ksceSblQafMgrIsAllowHost0Access(void);
int ksceSblQafMgrIsAllowDecryptedBootConfigLoad(void);
int ksceSblACMgrIsDevelopmentMode2(void);
int SceSysrootForKernel_47724459(int stage);
int sceKernelSysrootSetStatus(int flags);
int SceSysrootForDriver_7C2C10E2(int kind, int value);
int SceSysrootForKernel_ksceKernelSysrootAppMgrSpawnProcess(const char *name,
	int type, const char *path, SceSize len, const void *args, void *opt, void *out);
int SceSysrootForKernel_ksceSysrootIsBsodReboot(void);
int SceSysrootForKernel_ksceSysrootIsUsbEnumWakeup(void);
int SceSysrootForKernel_ksceSysrootIsSafeMode(void);
int SceSysrootForKernel_ksceSysrootIsExternalBootMode(void);
int SceSysrootForKernel_ksceSysrootIsUpdateMode(void);
extern char SceKernelPsp2Config_A6483A7C[];
extern char SceKernelPsp2Config_C1BA20DA[];

int ksceSysrootIsManufacturingMode(void);

/* Names unknown: the linked records store a path and its loaded UID. */
typedef struct ConfLoaded {
	struct ConfLoaded *next;
	SceUID uid;
	char *path;
} ConfLoaded;

/* names unknown: the command keywords, in the order of the command table */
enum ConfCommandId {
	CONF_CMD_UNKNOWN, CONF_CMD_LOAD, CONF_CMD_UNLOAD, CONF_CMD_LOADONLY,
	CONF_CMD_START, CONF_CMD_STOP, CONF_CMD_UNLOADONLY, CONF_CMD_SPAWN,
	CONF_CMD_SPAWNWAIT, CONF_CMD_WAIT, CONF_CMD_KILL, CONF_CMD_END,
	CONF_CMD_SLEEP, CONF_CMD_ECHO, CONF_CMD_LOADCONFIG, CONF_CMD_IFMODEL,
	CONF_CMD_IF, CONF_CMD_IFNMODEL, CONF_CMD_ELSE, CONF_CMD_ENDIF,
	CONF_CMD_INCLUDE, CONF_CMD_CD, CONF_CMD_SETENV, CONF_CMD_SETMODFILE,
	CONF_CMD_REPEAT, CONF_CMD_ENDREPEAT, CONF_CMD_APPSPAWN, CONF_CMD_TLOAD,
	CONF_CMD_UMOUNT_BOOTFS, CONF_CMD_SLOAD,
	CONF_CMD_COUNT
};

/* names unknown: a command's flags, from its prefix characters */
#define CONF_FLAG_SKIP		0x01	/* empty line or '#' comment */
#define CONF_FLAG_ASYNC		0x02	/* '&': not supported */
#define CONF_FLAG_IGNORE_ERROR	0x04	/* '-': carry on when it fails */
#define CONF_FLAG_VALID		0x80

/* name unknown: a parsed command in the command buffer; its arguments follow */
typedef struct ConfCommand {
	unsigned char flags;
	unsigned char id;
	unsigned char length;		/* of the arguments */
	unsigned char reserved;
	unsigned char active[4];	/* words in network byte order */
	unsigned char repeat[4];
	unsigned char repeatOffset[4];
} ConfCommand;

typedef struct ConfReader {
	SceUID fd;
	SceSize confSize;
	char *confBuf;
	SceUID confBlock;
	SceUInt64 cursor;
	SceSize cmdSize;
	unsigned char *cmdBuf;
	SceUID cmdBlock;
	SceUID confModule;
	ConfLoaded *loaded;
	struct { int parent; int condition; } exec[32];
	int execTop;
	int depth;
	char *cwd;
	char *absPath;
	char *remotePath;
	char *rpath[4];
	char *shellFilename;
	struct { const char *name; const char *value; } env[32];
	int envCount;
	struct { unsigned offset; int count; } repeat[4];
	int repeatTop;
	int reserved;
} ConfReader;

_Static_assert(sizeof(ConfReader) == 0x280, "configuration reader ABI");

extern const char *s_bootimage;
extern int s_noRemoteRpath;
extern int s_noRemoteModule;
extern SceUID s_appProcess;

void ConfReaderInit(ConfReader *reader, int depth);
int ConfReaderExecCommand(const char *path, int depth);
void ConfReader_mountDefaultBootimage(void);
int ConfReaderUtilsAllocConfBuf(ConfReader *reader, SceSize size);
int ConfReaderUtilsAllocCmdBuf(ConfReader *reader, SceSize size);
void ConfReaderUtilsFreeConfBuf(ConfReader *reader); /* name unknown */
void ConfReaderUtilsFreeCmdBuf(ConfReader *reader); /* name unknown */
int ConfReaderUtilsRememberLoaded(ConfReader *reader, SceUID uid, const char *path); /* name unknown */
SceUID ConfReaderUtilsFindLoaded(ConfReader *reader, const char *path); /* name unknown */
int ConfReaderUtilsForgetLoaded(ConfReader *reader, SceUID uid); /* name unknown */
void ConfReaderUtilsFreeLoaded(ConfReader *reader); /* name unknown */
int ConfReaderUtilsEvalCond(ConfReader *reader, int argc, char **argv, int first, int *result);
int ConfReaderUtilsPopExecFlags(ConfReader *reader, int *parent, int *condition);
int ConfReaderUtilsPushExecFlags(ConfReader *reader, int parent, int condition);
int ConfReaderUtilsPopRepeatInfo(ConfReader *reader, int *count, unsigned *offset);
int ConfReaderUtilsPushRepeatInfo(ConfReader *reader, int count, unsigned offset);
int ConfReaderUtilsTokenize(char **argv, int limit, char *buf, unsigned size); /* name unknown */
int ConfReaderUtilsDecimal(const char *text); /* name unknown */
void afterBoot(void) __attribute__((noreturn));
