/*
 * Core archive file (CAF) contexts: the UID objects that hold a CAF
 * transform's state. The transforms themselves (header and segment
 * init/transform/final, IV creation) are cryptographic and deferred.
 */

#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include <sysmem/uid_class.h>
#include <string.h>
#include <psp2kern/io/fcntl.h>

/* a CAF transform's state behind a UID */
typedef struct CafContextObject {
	SceObjectBase base;
	SceUInt8 state[1240];
} CafContextObject;

_Static_assert(sizeof(CafContextObject) == 1248, "CAF context object size");

static int _cafContextConstructor(void *object);
static int _cafContextDestructor(void *object);

SceClass cafContextClass;

static int _cafContextConstructor(void *object)
{
	CafContextObject *context = object;

	memset(context->state, 0, sizeof(context->state));
	return 0;
}

static int _cafContextDestructor(void *object)
{
	CafContextObject *context = object;

	memset(context->state, 0, sizeof(context->state));
	return 0;
}

int sceCoredumpCafContextClassInit(void)
{
	int result;

	result = SceSysmemForDriver_61317102(&cafContextClass, "SceUIDCafContextClass",
		SceSysmemForDriver_85336A1C(), sizeof(CafContextObject),
		_cafContextConstructor, _cafContextDestructor);
	if (result < 0)
		SCE_DBG_ASSERT_LEVEL(0, 0, 244);
	return result;
}

SceUID sceCoredumpCafContextCreate(void)
{
	SceObjectBase *object;
	SceUID context;

	context = SceSysmemForDriver_56A13E90(&cafContextClass,
		"SceCoredumpCafContextObject", &object);
	if (context < 0)
		SCE_DBG_PRINTF(11, 261, "sceGUIDKernelCreate() failed: 0x%08X\n",
			context);
	return context;
}

int sceCoredumpCafContextDestroy(SceUID context)
{
	int result;

	result = sceGUIDClose(context);
	if (result < 0)
		SCE_DBG_PRINTF(11, 272, "sceGUIDClose(0x%08X) failed: 0x%08X\n",
			context, result);
	return result;
}

int ksceCoredumpDeleteCrashReportCaf(void)
{
	return ksceIoRemove("ux0:data/crash_report.caf");
}
