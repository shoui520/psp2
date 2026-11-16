/*
 * prx_start_stop.c: the module's start and stop.
 */
#include <scetypes.h>
#include <kernel.h>
#include <libkernel/libkernel.h>

/* Cinterface.cpp */
int ultInitialize(void);
int ultFinalize(void);

#define PRX_ASSERT_OK(where, ret) \
	do { \
		if ((ret) != SCE_OK) { \
			sceClibPrintf("%s[ult] Assertion Failed:\t%s\n", where, "ret == SCE_OK"); \
			__breakpoint(0); \
			for (;;) \
				; \
		} \
	} while (0)

int module_start(SceSize args, const void *argp)
{
	int ret = ultInitialize();
	PRX_ASSERT_OK("prx_start_stop.c:31:", ret);
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize args, const void *argp)
{
	int ret = ultFinalize();
	PRX_ASSERT_OK("prx_start_stop.c:48:", ret);
	return SCE_KERNEL_STOP_SUCCESS;
}
