/* Sony venezia/booter.c: image memory and hardware bootstrap. */
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/debug.h>
#include "../venezia_internal.h"
#include <lowio/pervasive.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/cpu.h>

VeneziaKernelMemory g_veneziaKernelMemory;
VeneziaBootMemory g_veneziaBootMemory;

extern int ksceSblFwLoaderLock(const char *path, int flags);
extern int ksceSblFwLoaderLoad(int type, void *base, SceSize size, unsigned int *result);
extern int ksceSblFwLoaderUnlock(void);

#if FW_VERSION >= 0x04000000
extern unsigned int getVeneziaRpcBootState(void) __asm__("getRpcBootState"); /* name unknown */
extern void setVeneziaRpcBootState(unsigned int state) __asm__("setRpcBootState"); /* name unknown */
extern int callVeneziaRpc(int command, ...); /* name unknown */
extern int _sceVeneziaStoreContext(void);
extern int _sceVeneziaLoadContext(void);
#else
extern unsigned int getVeneziaRpcBootState(void) __asm__("getRpcBootState");
extern void setVeneziaRpcBootState(unsigned int state) __asm__("setRpcBootState");
extern int callVeneziaRpc(int command, ...);
extern int _sceVeneziaStoreContext(void);
extern int _sceVeneziaLoadContext(void);
#endif

/* name unknown: allocate the Venezia image and its register mapping. */
int allocVeneziaImage(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	union {
		SceKernelPhyMemRangeInfo info;
		void *base;
	} result;
	int ret;

	g_veneziaBootMemory.imageBlock = -1;
	g_veneziaBootMemory.registerBlock = -1;
	g_veneziaBootMemory.imagePhysicalBase = 0;
	g_veneziaBootMemory.imageBase = NULL;
	g_veneziaBootMemory.imageSize = 0;
	g_veneziaBootMemory.registerBase = NULL;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 2;
	opt.paddr = PERVASIVE2_BASE;
	ret = SceSysmemForDriver_C94850C9("ScePervasive2Reg", 0x20100806, 0x1000, &opt);
	if (ret < 0)
		goto out;
	g_veneziaBootMemory.registerBlock = ret;
	SceSysmemForDriver_A841EDDA(ret, &result.base);
	g_veneziaBootMemory.registerBase = result.base;
	result.info.size = sizeof(result.info);
	result.info.base = 0;
	result.info.rangeSize = 0;
	sceKernelGetPhyMemInfo(8, &result.info);
	g_veneziaBootMemory.imagePhysicalBase = result.info.base;
	g_veneziaBootMemory.imageSize = result.info.rangeSize;
	result.info.size = sizeof(result.info);
	result.info.base = 0;
	result.info.rangeSize = 0;
	sceKernelGetPhyMemInfo(1, &result.info);
	g_veneziaKernelMemory.base = result.info.base;
	g_veneziaKernelMemory.size = result.info.rangeSize;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 2;
	opt.paddr = g_veneziaBootMemory.imagePhysicalBase;
	ret = SceSysmemForDriver_C94850C9("SceVeneziaImage", 0x20108006,
				    g_veneziaBootMemory.imageSize, &opt);
	if (ret >= 0) {
		g_veneziaBootMemory.imageBlock = ret;
		SceSysmemForDriver_A841EDDA(ret, &result.base);
		g_veneziaBootMemory.imageBase = result.base;
	}
out:
	return ret;
}

int _sceVeneziaBootPhase2(void)
{
	volatile ScePervasive2Registers *regs = g_veneziaBootMemory.registerBase;
	unsigned int imageBase, kernelBase, imageEnd, kernelEnd, alignedBase;
	unsigned int loaded;
	int ret;

	ScePervasiveForDriver_FB01A2DD();
	imageBase = g_veneziaBootMemory.imagePhysicalBase;
	kernelBase = g_veneziaKernelMemory.base;
	regs->mappingControl = 0x1d001000;
	if (kernelBase < imageBase) {
		imageEnd = imageBase + g_veneziaBootMemory.imageSize;
		kernelEnd = kernelBase + g_veneziaKernelMemory.size;
		if (imageEnd < kernelEnd) {
			regs->kernelStart = kernelBase;
			regs->imageStart = imageBase;
			regs->imageEnd = imageEnd;
			regs->kernelEnd = kernelEnd;
		}
	}
	alignedBase = imageBase & 0xff800000;
	regs->imagePhysicalBase = alignedBase;
	regs->windows[0].regionStart = 0x1e000000;
	regs->windows[0].regionLimit = 0x20000000;
	regs->windows[0].translatedStart = 0x22000000;
	regs->windows[0].translatedLimit = imageBase & 0xfe000000;
	regs->windows[1].regionStart = 0x18000000;
	regs->windows[1].regionLimit = 0x20000000;
	regs->windows[1].translatedStart = imageBase & 0xf8000000;
	loaded = 0;
	regs->windows[1].translatedLimit = (imageBase + 0x08000000) & 0xf8000000;
	regs->windows[2].regionStart = 0x1e000000;
	regs->windows[2].regionLimit = 0x20000000;
	regs->windows[2].translatedStart = 0x22000000;
	regs->windows[2].translatedLimit = imageBase & 0xfe000000;
	regs->bootPhysicalBase = alignedBase;
	regs->interruptControl = 0xffff0000;
	regs->bootControl = 1;
	ret = ksceSblFwLoaderLock("os0:kd/vnzimg.skprx", 0);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 234, "_sceVeneziaBootPhase2",
					  "Warning: sceSblFwLoaderOpen failed (0x%08x)\n", ret);
		goto out;
	}
	ret = ksceSblFwLoaderLoad(1, g_veneziaBootMemory.imageBase,
				g_veneziaBootMemory.imageSize, &loaded);
	if (ret < 0)
		goto loadFailed;
	ret = ksceSblFwLoaderUnlock();
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 245, "_sceVeneziaBootPhase2",
					  "Warning: sceSblFwLoaderClose failed (0x%08x)\n", ret);
		goto out;
	}
	setVeneziaRpcBootState(1);
	SceCpuForDriver_E813EBB2();
	(void)regs->interruptControl;
	__asm__ volatile("dsb sy" ::: "memory");
	ScePervasiveForDriver_A7E64C6F();
	while (getVeneziaRpcBootState() != 8)
		sceKernelDelayThread(100);
out:
	return ret;
loadFailed:
	SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 239, "_sceVeneziaBootPhase2",
				  "Warning: sceSblFwLoaderLoad failed (0x%08x)\n", ret);
	ksceSblFwLoaderUnlock();
	goto out;
}

int _sceVeneziaSuspend(void)
{
	int ret;

	ret = callVeneziaRpc(508);
	if (ret < 0)
		goto out;
	while (getVeneziaRpcBootState() != 0x10000)
		;
	ScePervasiveForDriver_28731EC5();
	ScePervasiveForDriver_2EEBE9AE();
	ret = _sceVeneziaStoreContext();
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 353, "_sceVeneziaSuspend",
					  "Warning: _sceVeneziaStoreContext failed (0x%08x)\n", ret);
out:
	return ret;
}

int _sceVeneziaResume(void)
{
	volatile ScePervasive2Registers *regs = g_veneziaBootMemory.registerBase;
	unsigned int imageBase, kernelBase, imageEnd, kernelEnd, alignedBase;
	int ret;

	ret = _sceVeneziaLoadContext();
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 370, "_sceVeneziaResume",
					  "Warning: _sceVeneziaLoadContext failed (0x%08x)\n", ret);
		return ret;
	}
	ScePervasiveForDriver_FB01A2DD();
	imageBase = g_veneziaBootMemory.imagePhysicalBase;
	kernelBase = g_veneziaKernelMemory.base;
	regs->mappingControl = 0x1d001000;
	if (imageBase > kernelBase) {
		imageEnd = imageBase + g_veneziaBootMemory.imageSize;
		kernelEnd = kernelBase + g_veneziaKernelMemory.size;
		if (imageEnd < kernelEnd) {
			regs->kernelStart = kernelBase;
			regs->imageStart = imageBase;
			regs->imageEnd = imageEnd;
			regs->kernelEnd = kernelEnd;
		}
	}
	alignedBase = imageBase & 0xff800000;
	regs->imagePhysicalBase = alignedBase;
	regs->windows[0].regionStart = 0x1e000000;
	regs->windows[0].regionLimit = 0x20000000;
	regs->windows[0].translatedStart = 0x22000000;
	regs->windows[0].translatedLimit = imageBase & 0xfe000000;
	regs->windows[1].regionStart = 0x18000000;
	regs->windows[1].regionLimit = 0x20000000;
	regs->windows[1].translatedStart = imageBase & 0xf8000000;
	regs->windows[1].translatedLimit = (imageBase + 0x08000000) & 0xf8000000;
	regs->windows[2].regionStart = 0x1e000000;
	regs->windows[2].regionLimit = 0x20000000;
	regs->windows[2].translatedStart = 0x22000000;
	regs->windows[2].translatedLimit = imageBase & 0xfe000000;
	regs->bootPhysicalBase = alignedBase;
	regs->interruptControl = 0xffff0000;
	regs->bootControl = 1;
	SceCpuForDriver_E813EBB2();
	(void)regs->interruptControl;
	__asm__ volatile("dsb sy" ::: "memory");
	ScePervasiveForDriver_A7E64C6F();
	while (getVeneziaRpcBootState() != 0x80000)
		;
	return ret;
}

/* name unknown: image address queries used by the codec drivers. */
unsigned int getVeneziaImagePhysicalBase(void)
{
	return g_veneziaBootMemory.imagePhysicalBase;
}

void *getVeneziaImageBase(void) /* name unknown */
{
	return g_veneziaBootMemory.imageBase;
}

SceSize getVeneziaImageSize(void) /* name unknown */
{
	return g_veneziaBootMemory.imageSize;
}

volatile ScePervasive2Registers *getVeneziaBootRegisters(void) /* name unknown */
{
	return g_veneziaBootMemory.registerBase;
}
