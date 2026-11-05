/* SceDisplay: the DTrace probe __interrupt_vblank (SGI 8, subinterrupt 1). */
#include <scetypes.h>
#include <intrmgr/intrmgr.h>
#include <fw.h>
#include <sysmem/debug.h>
#include "display_internal.h"

/* From 4.00 a failure is reported with Sony's file name and line number (__LINE__: the
 * blank lines keep each check on Sony's line). */
#if FW_IS_400
#define DTRACE_CHECK(ret, what)	do { if ((ret) < 0)				\
	SceDebugForDriver_391B74B7("[dtrace] ERROR:   %s - " what " (0x%x) [%s:%d]\n",	\
			 __func__, (ret), "display_dtrace.c", __LINE__); } while (0)
#else
#define DTRACE_CHECK(ret, what)	((void)(ret))
#endif

static void (*vblankProbeHook)(const void *probe);
static int vblankProbeEnabled;
static int vblankProbeHandler(void *subintrArg, void *userCtx, unsigned char priority);

/* Sony's name (__func__). */
static void vblankProbeCreate(void)
{
	int ret;

	ret = sceKernelRegisterSubIntrHandler(8, 1, "SceDispalyDtrace", vblankProbeHandler, NULL);
	DTRACE_CHECK(ret, "failed to register interrupt");
}





/* Sony's name (__func__). */
static void vblankProbeEnable(void)
{
	int ret;

	ret = sceKernelEnableSubIntr(8, 1);
	DTRACE_CHECK(ret, "failed to enable vblank interrupt");
}





/* Sony's name (__func__). */
static void vblankProbeDisable(void)
{
	int ret;

	ret = sceKernelDisableSubIntr(8, 1);
	DTRACE_CHECK(ret, "failed to disable vblank interrupt");
}





/* Sony's name (__func__). */
static void vblankProbeDestroy(void)
{
	int ret;

	ret = sceKernelReleaseSubIntrHandler(8, 1);
	DTRACE_CHECK(ret, "failed to release interrupt");
}

/* Subinterrupt: hand the probe to DTrace. */
static __attribute__((used)) int vblankProbeHandler(void *subintrArg, void *userCtx, unsigned char priority) /* name unknown */
{
	__label__ pc;
pc:;
	static const SceDtraceProbe vblankProbe asm("__dtrace_probe$0___interrupt____vblank") __attribute__((used)) = {
		.enabled = &vblankProbeEnabled,
		.provider = "__interrupt_",
		.name = "vblank",
		.address = (void *)&&pc,
		.hook = &vblankProbeHook,
		.create = vblankProbeCreate,
		.enable = vblankProbeEnable,
		.disable = vblankProbeDisable,
		.destroy = vblankProbeDestroy,
	};
	asm volatile(
		".type  __dtrace_probe$0___interrupt____vblank, %%object\n\t"
		".globl __dtrace_probe$0___interrupt____vblank\n\t"
		"# " : : );

	if (vblankProbeHook != NULL)
		vblankProbeHook(&vblankProbe);
	return -1;
}

extern const SceDtraceProbe vblankProbe asm("__dtrace_probe$0___interrupt____vblank");
static const SceDtraceProbe *const vblankProbes[] __attribute__((used)) = { &vblankProbe };
