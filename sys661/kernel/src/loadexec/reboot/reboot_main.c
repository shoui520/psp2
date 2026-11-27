/*
 * ePSP 6.61 warm-reboot loader (reboot.bin, embedded in loadexec_01g.prx).
 *
 * Entered at 0x88600000 from sceLoadExec after decompressing KL3E/KL4E into
 * kernel RAM. Reuses the shared Allegrex boot primitives from sys661/ipl and
 * sys661/kernel/src/loadcore/loadelf_boot.c.
 */

#include "ipl.h"
#include "ipl_data.h"
#include "boot_config.h"
#include "loadelf_boot.h"
#include "memlmd.h"

typedef struct RebootVshParam {
	SceSize size;
	SceSize args;
	void *argp;
	const char *key;
	u32 vshmainArgs;
	void *vshmainArgp;
	char *configFile;
	char *string;
	u32 flags;
	u32 extArgs;
	void *extArgp;
	u32 reserved;
} RebootVshParam;

extern int _loadprx_by_pspbtcnf_bin(SceLoadCoreBootInfo *bootInfo, char *configPath,
				    u32 bootFlags, u32 bootArg);
extern u8 __bss_start[];
extern u8 __bss_end[];

static SceLoadCoreBootServices g_sysmemServices;
static SceLoadCoreBootServices g_loadcoreServices;
static u32 g_rebootExecArg;
static RebootVshParam g_vshParam;
static char g_vshKeyBuf[256];
static char g_vshConfigBuf[256];
static char g_vshStringBuf[256];
static u32 g_dipswOverrideLo;
static u32 g_dipswOverrideHi;
static u32 g_rebootDipswState[3];

static const char g_modeGame[] = "game";
static const char g_modeVsh[] = "vsh";
static const char g_modeUpdater[] = "updater";
static const char g_modePops[] = "pops";
static const char g_modeLicenseGame[] = "licensegame";
static const char g_modeApp[] = "app";
static const char g_modeUmdEmu[] = "umdemu";
static const char g_modeMlnApp[] = "mlnapp";

static inline u32 cp0Status(void)
{
	u32 value;

	__asm__ __volatile__("mfc0 %0, $12" : "=r"(value));
	return value;
}

static inline void setCp0Status(u32 value)
{
	__asm__ __volatile__("mtc0 %0, $12" :: "r"(value) : "memory");
}

static inline u32 cp0Count(void)
{
	u32 value;

	__asm__ __volatile__("mfc0 %0, $9" : "=r"(value));
	return value;
}

int Kprintf(const char *fmt, ...)
{
	return 0;
}

int bootStartupHook(void)
{
	return 0;
}

void bootHandoffCallback(void)
{
}

static void bootIdleHook(void)
{
}

int memlmdPrepareGetLength(u8 *buffer, SceSize size)
{
	return 0;
}

static int rebootPostLoadHook(void)
{
	return 0;
}

static void setRebootDipsw(u32 low, u32 high, u32 extra)
{
	g_rebootDipswState[0] = low;
	g_rebootDipswState[1] = high;
	g_rebootDipswState[2] = extra;
}

SceCompatBootFile *findBootFile(const char *name)
{
	SceCompatBootFile *file = (SceCompatBootFile *)0x8B000000;

	while (file->buffer != NULL) {
		if (strcmp(file->name, name) == 0)
			return file;
		file++;
	}
	return NULL;
}

int openBootFile(SceCompatBootFile *file, int optional)
{
	SceCompatBootFile *entry;
	int result = SCE_ERROR_ERROR;

	if (file->name != NULL) {
		entry = findBootFile(file->name);
		result = (optional == 1) ? 0 : SCE_ERROR_ERROR;
		if (entry != NULL) {
			SceSize size = entry->size;
			void *buffer = entry->buffer;

			file->size = size;
			file->buffer = buffer;
			result = 0;
		}
	}
	return result;
}

int openBootModuleFile(SceCompatBootFile *file, int optional,
		       u32 bootFlags, u32 bootArg, SceLoadCoreBootInfo *bootInfo)
{
	return openBootFile(file, optional);
}

void Panic(const char *func, int line)
{
	SceLoadCoreBootInfo *bootInfo = g_bootInfo;
	u32 memoryConfig = HW(0xBC100040) & ~3U;
	SceSize memSize;
	void *memBase;

	if (bootInfo->memSize > 0x02000000U)
		memoryConfig |= 2U;
	else
		memoryConfig |= 1U;
	HW(0xBC100040) = memoryConfig;
	memset((void *)0xBFC00000, 0, 4096);
	memcpy((void *)0xBFC00000, haltStub, (u32)haltStubEnd - (u32)haltStub);
	memSize = g_bootInfo->memSize;
	memBase = g_bootInfo->memBase;
	g_haltEntry = (SceIplHaltEntry)0xBFC00000;
	g_haltEntry(memBase, memSize);
	for (;;)
		;
}

static u32 parseBootMode(const char *key)
{
	if (strncmp(key, g_modeGame, 4) == 0)
		return 1;
	if (strncmp(key, g_modeVsh, 3) == 0)
		return 2;
	if (strncmp(key, g_modeUpdater, 7) == 0)
		return 3;
	if (strncmp(key, g_modePops, 4) == 0)
		return 4;
	if (strncmp(key, g_modeLicenseGame, 11) == 0)
		return 5;
	if (strncmp(key, g_modeApp, 3) == 0)
		return 6;
	if (strncmp(key, g_modeUmdEmu, 6) == 0)
		return 7;
	if (strncmp(key, g_modeMlnApp, 6) == 0)
		return 8;
	return 0;
}

static int loadprx(SceLoadCoreBootInfo *bootInfo, const char *key)
{
	s32 count = bootInfo->numProtects;

	if (count > 0) {
		SceLoadCoreProtectInfo *entry = bootInfo->protects;
		s32 i;

		for (i = 0; i < count; i++, entry++) {
			if ((entry->attr & 0xFFFFU) == 8U)
				bootInfo->reserved2[0] = entry->addr;
		}
	}
	if (bootInfo->reserved2[0] != 0)
		g_rebootExecArg = bootInfo->reserved2[0];

	setGetLengthFunc(sceUtilsGetLoadModuleABLengthByPolling);
	setPrepareGetLengthFunc(memlmdPrepareGetLength);
	setMaskFunc(memlmdInitializeScrambleKey);
	setCompareSubTypeFunc(memlmdCompareSubType);
	setCompareLatestSubTypeFunc(memlmdCompareLatestSubType);

	g_bootInfo = bootInfo;
	bootInfo->bootMode = parseBootMode(key);
	if (_loadprx_by_pspbtcnf_bin(bootInfo, g_configPath,
				     bootInfo->bootArg1, bootInfo->bootArg0) != 0)
		Panic(g_strLoadprx, 207);
	rebootPostLoadHook();
	return 0;
}

static int kactivate(SceLoadCoreBootInfo *bootInfo, int index,
		     SceLoadCoreBootServices *services)
{
	SceLoadCoreExecFileInfo *execInfo = &g_execInfo[index];
	SceLoadCoreBootModuleInfo *module = &bootInfo->modules[index];
	u8 *file = module->modBuf;
	void *bootArgs[2];

	memset(execInfo, 0, sizeof(*execInfo));
	execInfo->topAddr = (u8 *)module->reserved;
	execInfo->apiType = 0;
	execInfo->modeAttribute = 0;
	execInfo->isSignChecked = 1;
	if (loadBootExecutable(file, execInfo, index, services) < 0)
		Panic(g_strKactivate, 293);
	memset(module->modBuf, 0, module->modSize);
	if (index == 1) {
		services->importsInfo = execInfo->importsInfo;
		services->importsSize = execInfo->importsSize;
		services->getLengthFunc = sceUtilsGetLoadModuleABLengthByPolling;
		services->prepareGetLengthFunc = memlmdPrepareGetLength;
		services->setMaskFunc = memlmdInitializeScrambleKey;
		services->loadcoreExecInfo = &g_execInfo[1];
		services->compareSubType = memlmdCompareSubType;
		services->compareLatestSubType = memlmdCompareLatestSubType;
		services->sysmemExecInfo = &g_execInfo[0];
	}
	sceKernelIcacheInvalidateAll();
	sceKernelDcacheWritebackAll();
	bootInfo->loadedModules = 2;
	if (index != 0) {
		if (index != 1)
			Panic(g_strKactivate, 376);
		bootArgs[0] = bootInfo;
		bootArgs[1] = services;
		callWithBootStack((int (*)(u32, void *))execInfo->entryAddr,
				  8, bootArgs, services->stackTop);
	} else {
		((int (*)(u32, SceLoadCoreBootServices *))execInfo->entryAddr)(
			4, services);
	}
	return 0;
}

static void __attribute__((noreturn))
activateKernelModules(SceLoadCoreBootInfo *bootInfo, u32 randomSeed)
{
	SceLoadCoreBootModuleInfo *modules = bootInfo->modules;
	int intrFlags;

	memset(&g_sysmemServices, 0, sizeof(g_sysmemServices));
	memset(&g_loadcoreServices, 0, sizeof(g_loadcoreServices));

	g_sysmemServices.memSize = bootInfo->memSize;
	g_sysmemServices.model = bootInfo->model;
	g_sysmemServices.configMode = bootInfo->bootMode;
	g_sysmemServices.unknown0C = 3;
	g_sysmemServices.unknown10 = 6;
	g_sysmemServices.memClearSize = 0;
	g_sysmemServices.memBase = bootInfo->memBase;
	g_sysmemServices.bootArg0 = bootInfo->bootArg0 | g_dipswOverrideLo;
	g_sysmemServices.bootArg1 = bootInfo->bootArg1 | g_dipswOverrideHi;
	g_sysmemServices.bootMode = bootInfo->reserved2[1];
	g_sysmemServices.numProtects = bootInfo->numProtects;
	g_sysmemServices.protects = bootInfo->protects;
	g_sysmemServices.handoffCallback = bootIdleHook;
	g_sysmemServices.initialMaxAllocSize = modules[1].modSize;
	g_sysmemServices.loadcoreServices = &g_loadcoreServices;
	g_sysmemServices.resumeArg = randomSeed;
	modules[0].reserved = (s32)bootInfo->memBase;

	intrFlags = sceKernelCpuSuspendIntr();
	kactivate(bootInfo, 0, &g_sysmemServices);
	g_loadcoreServices.syscallTableSeed = randomSeed;
	modules[1].reserved = (s32)g_loadcoreServices.loadCoreAddr;
	kactivate(bootInfo, 1, &g_loadcoreServices);
	sceKernelCpuResumeIntr(intrFlags);
	Panic(g_strBootMain, 599);
}

int main(SceLoadCoreBootInfo *bootInfo, const RebootVshParam *vshParam,
	 s32 apiType, u32 randomSeed)
{
	u32 dipswLo = 0;
	u32 dipswHi = 0;

	setCp0Status(cp0Status() & ~0x1FU);
	memset(__bss_start, 0, (SceSize)(__bss_end - __bss_start));

	if ((bootInfo->bootArg0 & 0x40000000U) != 0) {
		dipswLo = bootInfo->bootArg0;
		dipswHi = bootInfo->bootArg1;
		g_dipswOverrideLo = dipswLo;
		g_dipswOverrideHi = dipswHi;
	}
	setRebootDipsw(dipswLo, dipswHi, 0);
	bootStartupHook();

	g_vshParam = *vshParam;
	if (vshParam->key != NULL) {
		strncpy(g_vshKeyBuf, vshParam->key, sizeof(g_vshKeyBuf));
		g_vshParam.key = g_vshKeyBuf;
	} else {
		g_vshParam.key = NULL;
	}
	if (vshParam->configFile != NULL) {
		strncpy(g_vshConfigBuf, vshParam->configFile, sizeof(g_vshConfigBuf));
		g_vshParam.configFile = g_vshConfigBuf;
	} else {
		g_vshParam.configFile = NULL;
	}
	if (vshParam->string != NULL) {
		strncpy(g_vshStringBuf, vshParam->string, sizeof(g_vshStringBuf));
		g_vshParam.string = g_vshStringBuf;
	} else {
		g_vshParam.string = NULL;
	}

	if ((g_vshParam.flags & 0x10000U) != 0) {
		bootInfo->reserved0 = (s32)0x89000000U;
		loadprx(bootInfo, g_vshParam.key);
		bootInfo->modules[bootInfo->numModules - 1].bootData = apiType;
	} else {
		bootInfo->configFile = g_vshParam.string;
	}

	activateKernelModules(bootInfo, randomSeed + cp0Count());
}
