/*
 * SceUlobjDbg (Sony's ulobj_dbg.c): the table of user-level objects the debugger reads. The
 * kernel gives the process its UlobjInfoArray when the module starts; an object's entry holds
 * its address, ULOBJ_ENTRY_USED and its kind, a free entry the index of the next free one
 * (times 8), with the first free index in the array's head word.
 */
#include <stddef.h>
#include <fw.h>
#include <scetypes.h>
#include <sceerror.h>
#include <intrinsics.h>
#include <kernel/error.h>
#include <ulobjmgr/ulobjmgr.h>
#include <fiber/fiber.h>
#include "fiber_internal.h"

#define ULOBJ_DBG_SOURCE \
	"W:\\Build\\" ULOBJ_DBG_BUILD "\\sdk-internal-step2\\internal\\usermode\\src\\fiber\\ulobj_dbg.c"
#if FW_SINCE(0x03650000)
#define ULOBJ_DBG_BUILD	"J01509655"
#else
#define ULOBJ_DBG_BUILD	"J01508048"
#endif

#define ULOBJ_DBG_MAX_ENTRIES	0x3ffc
#define ULOBJ_DBG_NO_ENTRY	(ULOBJ_ENTRY_END >> 3)

/* the array's first free entry */
#define ULOBJ_DBG_FREE_HEAD(array)	(&(array)->userData[2])

UlobjInfoArray *SceUlobjDbg_DE6CFF47 = NULL;

static int startUlobjDbg(void);
static int stopUlobjDbg(void);

static int allocUlobjEntry(UlobjInfoArray *array, SceUInt32 *index)
{
	SceUInt32 *head = ULOBJ_DBG_FREE_HEAD(array);
	SceUInt32 entry;

	do {
		entry = __builtin_ldrex(head);
		if (entry == ULOBJ_DBG_NO_ENTRY)
			return -1;
	} while (__builtin_strex(array->entry[entry] >> 3, head) != 0);
	*index = entry;
	return 0;
}

static void freeUlobjEntry(UlobjInfoArray *array, SceUInt32 index)
{
	SceUInt32 *head = ULOBJ_DBG_FREE_HEAD(array);

	do {
		array->entry[index] = __builtin_ldrex(head) << 3;
	} while (__builtin_strex(index, head) != 0);
}

int module_start(SceSize args, const void *argp)
{
	return startUlobjDbg();
}

int module_stop(SceSize args, const void *argp)
{
	return stopUlobjDbg();
}

int module_exit(SceSize args, const void *argp)
{
	return stopUlobjDbg();
}

int SceUlobjDbg_D7F0F610(void *object, SceUInt32 kind, SceUInt32 *id)
{
	SceUInt32 index;

	if (id == NULL)
		return SCE_ERROR_ERRNO_EFAULT;
	if ((kind & ~ULOBJ_ENTRY_KIND_MASK) != 0 || ((SceUIntPtr)object & ULOBJ_ENTRY_TAG_MASK) != 0)
		return SCE_ERROR_ERRNO_EINVAL;
	if (SceUlobjDbg_DE6CFF47 == NULL || allocUlobjEntry(SceUlobjDbg_DE6CFF47, &index) != 0) {
		*id = ULOBJ_DBG_NO_ENTRY;
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	SceUlobjDbg_DE6CFF47->entry[index] = (SceUIntPtr)object | kind | ULOBJ_ENTRY_USED;
	__builtin_dmb();
	*id = index;
	return SCE_OK;
}

int SceUlobjDbg_F9C0F5DA(SceUInt32 id)
{
	if (id >= ULOBJ_DBG_NO_ENTRY || SceUlobjDbg_DE6CFF47 == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	__builtin_dmb();
	freeUlobjEntry(SceUlobjDbg_DE6CFF47, id);
	return SCE_OK;
}

static int startUlobjDbg(void)
{
	if (_sceUlobjMgrStartSupportingUserlevelObject(ULOBJ_DBG_MAX_ENTRIES,
						       SCE_PSP2_SDK_VERSION & 0xffff0000,
						       (void **)&SceUlobjDbg_DE6CFF47) != 0)
		SceUlobjDbg_DE6CFF47 = NULL;
	return 0;
}

static int stopUlobjDbg(void)
{
	int status;

	if (SceUlobjDbg_DE6CFF47 != NULL) {
		status = _sceUlobjMgrStopSupportingUserlevelObject();
		fiberAssert(status == SCE_KERNEL_OK, "status == SCE_KERNEL_OK", ULOBJ_DBG_SOURCE,
			    107);
	}
	return 0;
}
