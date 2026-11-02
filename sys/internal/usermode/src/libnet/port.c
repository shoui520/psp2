/*
 * port.c: the services libnet takes from the system (random numbers, time, threads, event
 * flags, callbacks, the thread's errno), and the module's entry points. A failing call is
 * reported both to the console and to libnet's debug output.
 */
#include <libkernel/libkernel.h>
#include <kernel/error.h>
#include <kernel/processmgr.h>
#include <kernel/threadmgr.h>
#include <libsfmt19937.h>
#include "libnet_internal.h"

/* the TLS slot of the thread's libnet errno */
#define LIBNET_TLS_ERRNO	64

static SceSfmt19937Context libnet_random_context;
static int libnet_errno_fallback;

int libnet_random_init(int seed)
{
	sceSfmt19937InitGenRand(&libnet_random_context, seed);
	return 0;
}

int libnet_random_term(void)
{
	return 0;
}

unsigned int libnet_random(void)
{
	return sceSfmt19937GenRand32(&libnet_random_context);
}

/* every libnet callback object starts with the function its notifications call */
static int libnet_callback_entry(SceUID notifyId, SceInt32 notifyCount, SceInt32 notifyArg,
				 void *common)
{
	LibnetCallback *callback = common;

	callback->function(notifyId, notifyCount, notifyArg, common);
	return 0;
}

static SceInt32 libnet_thread_entry(SceSize argSize, void *pArgBlock)
{
	LibnetThreadParam *param = pArgBlock;

	param->entry(param->argument);
	if (param->flags & 1)
		SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return 0;
}

SceUInt64 libnet_get_time(SceUInt64 *time)
{
	return *time = sceKernelGetProcessTimeWide();
}

/* write VALUE in hexadecimal over the '*' of NAME, the last one first */
int libnet_name_hex(int value, char *name, int length)
{
	signed char *p;
	int digit;

	for (p = (signed char *)name + length; length > 0; length--) {
		if (*p-- != '*')
			continue;
		digit = value % 16;
		value /= 16;
		if (digit > 9)
			p[1] = digit + 'A' - 10;
		else
			p[1] = digit + '0';
	}
	return value;
}

int libnet_thread_create(SceUID *thread, LibnetThreadParam *param, const char *name)
{
	SceUID id;
	int ret;

	id = sceKernelCreateThread(name, libnet_thread_entry, param->priority, param->stackSize,
				   0, 0, NULL);
	if (id < 0) {
		LIBNET_PRINTF("[LIBNET] sceKernelCreateThread -> %x", id);
		return id;
	}
	ret = sceKernelStartThread(id, sizeof(*param), param);
	if (ret < 0)
		LIBNET_PRINTF("[LIBNET] sceKernelStartThread -> %x", ret);
	else
		*thread = id;
	return ret;
}

int libnet_get_thread_id(void)
{
	SceUID id;

	id = sceKernelGetThreadId();
	if (id < 0) {
		LIBNET_PRINTF("[LIBNET] sceKernelGetThreadId -> %x", id);
		return -1;
	}
	return id;
}

int libnet_thread_join(SceUID thread)
{
	SceInt32 status;
	int ret;

	ret = sceKernelWaitThreadEnd(thread, &status, NULL);
	if (ret < 0)
		status = ret;
	sceKernelDeleteThread(thread);
	return status;
}

int libnet_thread_exit(void)
{
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	sceClibPrintf("[LIBNET] sceKernelExitThread -> return");
	return SceNetInternal_694F8996("[LIBNET] sceKernelExitThread -> return");
}

int libnet_usleep(int usec)
{
	if (usec >= 0)
		return sceKernelDelayThread(usec);
	LIBNET_PRINTF("[LIBNET] usleep %d\n", usec);
	return SCE_NET_ERROR_EINVAL;
}

int libnet_sync_create(LibnetSync *sync, int bits, const char *name)
{
	SceUID id;
	int ret;

	id = sceKernelCreateEventFlag(name, SCE_KERNEL_EVF_ATTR_MULTI, bits, NULL);
	ret = id;
	if (id < 0) {
		LIBNET_PRINTF("[LIBNET] %s -> %x\n", __func__, ret);
	} else {
		sync->eventFlag = id;
		ret = 0;
		sync->bits = bits;
	}
	return ret;
}

int libnet_sync_destroy(LibnetSync *sync)
{
	sceKernelDeleteEventFlag(sync->eventFlag);
	return 0;
}

/* wait until the sync is signalled, and take the bits signalled to it */
int libnet_sync_wait(LibnetSync *sync, SceUInt32 *timeout)
{
	SceUInt32 outBits;
	int ret;

	ret = sceKernelWaitEventFlag(sync->eventFlag, 1,
				     SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				     &outBits, timeout);
	if (ret < 0) {
		if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
			*timeout = 0xffffffff;
		else
			LIBNET_PRINTF("[LIBNET] %s -> %x\n", __func__, ret);
		return 0;
	}
	ret = sync->bits;
	sync->bits = 0;
	return ret;
}

int libnet_sync_signal(LibnetSync *sync, int bits)
{
	int ret;

	sync->bits |= bits;
	ret = sceKernelSetEventFlag(sync->eventFlag, 1);
	if (ret < 0) {
		LIBNET_PRINTF("[LIBNET] %s -> %x\n", __func__, ret);
		return ret;
	}
	return ret;
}

int libnet_callback_create(SceUID *callback, void *argument, const char *name)
{
	SceUID id;
	int ret;

	id = sceKernelCreateCallback(name, 0, libnet_callback_entry, argument);
	if (id < 0) {
		LIBNET_PRINTF("[LIBNET] sceKernelCreateCallback -> %x\n", id);
		ret = id;
	} else {
		ret = 0;
	}
	*callback = id;
	return ret;
}

int libnet_callback_destroy(SceUID callback)
{
	sceKernelDeleteCallback(callback);
	return 0;
}

/* sleep USEC, running the thread's callbacks */
int libnet_wait_cb(int unused0, int unused1, int unused2, int usec)
{
	if (usec < 0) {
		LIBNET_PRINTF("[LIBNET] wait cb %d\n", usec);
		return SCE_NET_ERROR_EINVAL;
	}
	sceKernelDelayThreadCB(usec);
	return 0;
}

int *libnet_tls_errno(void)
{
	int *errnoLocation;

	errnoLocation = sceKernelGetTLSAddr(LIBNET_TLS_ERRNO);
	if (errnoLocation == NULL) {
		libnet_errno_fallback = SCE_NET_ETLS;
		return &libnet_errno_fallback;
	}
	return errnoLocation;
}

int module_start(SceSize arglen, const void *argp)
{
	return 0;
}

int module_stop(SceSize arglen, const void *argp)
{
	return 0;
}
