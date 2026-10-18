/* SceDisplay: the DTrace probe __interrupt_vblank (SGI 8, subinterrupt 1). */
#include <scetypes.h>
#include <intrmgr/intrmgr.h>
#include <fw.h>
#include <sysmem/debug.h>
#include "display_internal.h"
#include "display_imports.h"

/* From 4.00 a failure is reported with Sony's file name and line number. */
#if FW_SINCE(0x04000000)
#define DTRACE_CHECK(ret, what)	do { if ((ret) < 0)				\
	SceDebugForDriver_391B74B7("[dtrace] ERROR:   %s - " what " (0x%x) [%s:%d]\n",	\
			 __func__, (ret), "display_dtrace.c", __LINE__); } while (0)
#else
#define DTRACE_CHECK(ret, what)	((void)(ret))
#endif

void (*vblankProbeHook)(const void *probe);
int vblankProbeEnabled;
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

static const SceDtraceProbe vblankProbe;

/* Subinterrupt: hand the probe to DTrace. */
static int vblankProbeHandler(void *subintrArg, void *userCtx, unsigned char priority) /* name unknown */
{
	if (vblankProbeHook != NULL)
		vblankProbeHook(&vblankProbe);
	return -1;
}

static const SceDtraceProbe vblankProbe = {
	.enabled = &vblankProbeEnabled,
	.provider = "__interrupt_",
	.name = "vblank",
	.address = vblankProbeHandler,	/* PLACEHOLDER: Sony's has no Thumb bit (0x81007a34) */
	.hook = &vblankProbeHook,
	.create = vblankProbeCreate,
	.enable = vblankProbeEnable,
	.disable = vblankProbeDisable,
	.destroy = vblankProbeDestroy,
};

/* DTrace finds the module's probes through this list. */
static const SceDtraceProbe *const vblankProbes[] __attribute__((used)) = { &vblankProbe };
