/*
 * SceKernelBusError (secure world): the bus error interrupt (0x21) and the
 * monitor calls with which the kernel has the error registers collected into
 * the sysroot shared memory (see ../buserror_es3.h).
 */

#include "buserror.h"

/* the monitor call handlers (names unknown): veneers into buserror_es3.c */
static int busErrorMoncallNop(int arg0, int arg1, int arg2, int arg3)
{
	return MoncallNop_ES3(arg0, arg1, arg2, arg3);
}

static int busErrorMoncallCollectClean(int arg0, int arg1, int arg2, int arg3)
{
	return MoncallCollectCleanBusErrorInfo_ES3(arg0, arg1, arg2, arg3);
}

static int busErrorMoncallCollect(int arg0, int arg1, int arg2, int arg3)
{
	return MoncallCollectBusErrorInfo_ES3(arg0, arg1, arg2, arg3);
}

static int RegisterMoncallHandler(void)
{
	int ret;

	ret = sceKernelRegisterMonitorCallHandler(BUSERROR_MONCALL_COLLECT, busErrorMoncallCollect);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 115, "Error:sceKernelRegisterMonitorCallHandler failed 0x%08x\n", ret);
		return ret;
	}
	ret = sceKernelRegisterMonitorCallHandler(BUSERROR_MONCALL_COLLECT_CLEAN,
						  busErrorMoncallCollectClean);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 121, "Error:sceKernelRegisterMonitorCallHandler failed 0x%08x\n", ret);
		return ret;
	}
	ret = sceKernelRegisterMonitorCallHandler(BUSERROR_MONCALL_NOP, busErrorMoncallNop);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 127, "Error:sceKernelRegisterMonitorCallHandler failed 0x%08x\n", ret);
		return ret;
	}
	return 0;
}

static int RegisterErrorHandler(void)
{
	int ret = RegisterBusErrorHandler_ES3();

	if (ret < 0)
		SCE_DBG_PRINTF(6, 91, "register default bus error handler failed. 0x%08x\n", ret);
	return ret;
}

int module_start(SceSize args, void *argp)
{
	if (RegisterMoncallHandler() < 0)
		return SCE_KERNEL_START_NO_RESIDENT;
	if (RegisterErrorHandler() < 0)
		return SCE_KERNEL_START_NO_RESIDENT;
	return SCE_KERNEL_START_SUCCESS;
}
