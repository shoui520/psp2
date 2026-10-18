/* SceCoredump module initialization. */

#include <psp2kern/types.h>
#include <sysmem/debug.h>
#include <sysmem/sysroot.h>
#include <coredump/coredump.h>

/* Private implementations follow in their original source files. */
extern int coredumpMemoryInit(void);
extern int triggerInit(void);
extern int triggerFinal(void);
extern int generatorInit(void);
extern int generatorFinal(void);
extern int dumpBufferInit(void);
extern int sceCoredumpCafContextClassInit(void);
extern void _getBuildVerInfo(void);
extern int sceKernelCoredumpCancelForDriver(void);
extern int SceCoredumpForDriver_EF20949F(void);

SCE_DBGINFO_DEFINE(static, startMemoryInfo, 51, "sceCoredumpStart");
SCE_DBGINFO_DEFINE(static, startTriggerInfo, 58, "sceCoredumpStart");
SCE_DBGINFO_DEFINE(static, startGeneratorInfo, 65, "sceCoredumpStart");
SCE_DBGINFO_DEFINE(static, startCafInfo, 73, "sceCoredumpStart");
SCE_DBGINFO_DEFINE(static, startDumpBufferInfo, 82, "sceCoredumpStart");
static const char startErrorFormat[] = "0x%08x:%d,%d:0x%08x:0x%08x\n";

#define START_ERROR(info, line, error) \
	SceDebugForDriver_02B04343(0, &(info), startErrorFormat, \
		0xcf168d67U, 1, (line), 0x201, (error))

int sceCoredumpStart(SceSize arglen, const void *argp)
{
	int result;

	(void)arglen;
	(void)argp;
	SceSysrootForDriver_DD473B05(sceCoredumpTrigger);
	SceSysrootForDriver_400B9793(sceKernelCoredumpCancelForDriver);
	SceSysrootForDriver_9DCD0894(SceCoredumpForDriver_EF20949F);
	result = coredumpMemoryInit();
	if (result != 0) {
		START_ERROR(startMemoryInfo, 51, result);
		return result;
	}
	result = triggerInit();
	if (result != 0) {
		START_ERROR(startTriggerInfo, 58, result);
		return result;
	}
	result = generatorInit();
	if (result != 0) {
		START_ERROR(startGeneratorInfo, 65, result);
		triggerFinal();
		return result;
	}
	result = sceCoredumpCafContextClassInit();
	if (result != 0) {
		START_ERROR(startCafInfo, 73, result);
		generatorFinal();
		triggerFinal();
		return result;
	}
	result = dumpBufferInit();
	if (result != 0) {
		START_ERROR(startDumpBufferInfo, 82, result);
		return result;
	}
	_getBuildVerInfo();
	return 0;
}

extern int module_start(SceSize arglen, const void *argp)
	__attribute__((alias("sceCoredumpStart")));
