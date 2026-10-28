/* IPL utility family; the stripped payload has no source-file records. */
#include "ipl.h"
#include "ipl_data.h"
#include "psp_header.h"
#include "boot_config.h"
#include "loadelf_boot.h"
#include "memlmd.h"

/* The original boot formatter consumes no arguments and returns zero. */
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

/* The boot copy scans unsigned bytes and advances the source pointer. */
SceSize bootStringLength(const u8 *string)
{
	SceSize length = 0;

	if (string == NULL)
		return 0;
	while (*string++ != '\0')
		length++;
	return length;
}

/* The original registered memlmd preparation hook is a successful no-op. */
int memlmdPrepareGetLength(u8 *buffer, SceSize size)
{
	return 0;
}

void Panic(const char *func, int line)
{
	SceLoadCoreBootInfo *bootInfo = g_bootInfo;
	u32 memoryConfig = HW(0xBC100040) & ~3;
	SceSize memSize;
	void *memBase;

	if (bootInfo->memSize > 0x02000000)
		memoryConfig |= 2;
	else
		memoryConfig |= 1;
	HW(0xBC100040) = memoryConfig;
	memset((void *)0xBFC00000, 0, 4096);
	memcpy((void *)0xBFC00000, haltStub,
	       (u32)haltStubEnd - (u32)haltStub);
	memSize = g_bootInfo->memSize;
	memBase = g_bootInfo->memBase;
	g_haltEntry = (SceIplHaltEntry)0xBFC00000;
	g_haltEntry(memBase, memSize);
	for (;;)
		;
}

int copyBootModuleHeader(u8 **keyData,
			 const SceLoadCoreBootModuleInfo *module, void *headerMemory)
{
	PspHeaderSnapshot *snapshot = headerMemory;
	const u8 *source = module->modBuf;
	u8 *destination = headerMemory;
	const u8 *end;
	int result;

	if (module->modSize <= sizeof(*snapshot))
		return -1;
	end = source + sizeof(*snapshot);
	do {
		u32 block[4];

		/* Read the complete block before any overlapping output stores. */
		__builtin_memcpy(block, source, sizeof(block));
		__builtin_memcpy(destination, block, sizeof(block));
		source += sizeof(block);
		destination += sizeof(block);
	} while (source != end);
	result = prepareModuleHeader(headerMemory, sizeof(*snapshot));
	if (result < 0)
		return result;
	*keyData = snapshot->header.keyData4;
	return 0;
}

int compareBootModuleHash(const SceLoadCoreBootModuleInfo *module,
			  const PspBootConfigModule *configModule)
{
	u8 scratch[sizeof(PspHeaderSnapshot) + 63];
	void *headerMemory = (void *)(((u32)scratch + 63) & ~63U);
	u8 *keyData;

	if (copyBootModuleHeader(&keyData, module, headerMemory) < 0)
		return -1;
	/* The original calls memcmp but discards its result. */
	memcmp(configModule->hash, keyData, sizeof(configModule->hash));
	return 0;
}

int loadprx(SceLoadCoreBootInfo *bootInfo, u32 bootFlags, u32 bootArg)
{
	setGetLengthFunc(sceUtilsGetLoadModuleABLengthByPolling);
	setPrepareGetLengthFunc(memlmdPrepareGetLength);
	setMaskFunc(memlmdInitializeScrambleKey);
	setCompareSubTypeFunc(memlmdCompareSubType);
	setCompareLatestSubTypeFunc(memlmdCompareLatestSubType);
	if (_loadprx_by_pspbtcnf_bin(g_bootInfo, g_configPath,
				   bootFlags, bootArg) != 0)
		Panic(g_strLoadprx, 207);
	setupBootProtection(g_bootInfo);
	return 0;
}

int addBootProtection(SceLoadCoreBootInfo *bootInfo, u32 attr,
		      u32 addr, SceSize size)
{
	int index = bootInfo->numProtects;
	SceLoadCoreProtectInfo *protect;

	if (index == 0)
		bootInfo->protects = (SceLoadCoreProtectInfo *)
			(bootInfo->modules + bootInfo->numModules);
	protect = &bootInfo->protects[index];
	protect->attr = attr;
	protect->addr = addr;
	protect->size = size;
	bootInfo->numProtects++;
	return 0;
}
