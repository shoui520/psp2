/*
 * The thread-local storage of the program's modules (SCE): the TLS image of every module that
 * registers, a block of it for each thread made when the thread first touches it
 * (__tls_get_addr), and the blocks freed when the thread exits.
 */
#include <string.h>
#include <kernel/constant.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>

#include "libc_internal.h"

#define TLS_TCB		140	/* the thread's LdTlsTcb (sceKernelGetTLSAddr slot) */
#define MODULE_GROW	32	/* the module table grows by this many entries */

/* a registered module */
typedef struct LdTlsModule {
	const void *module;		/* an address in the module; NULL: a free entry */
	unsigned int generation;	/* the generation it was registered in */
	const void *image;		/* the initial data */
	SceSize initSize;		/* its size */
	SceSize size;			/* the size of a block */
} LdTlsModule;

/* a thread's blocks, one per module index */
typedef struct LdTlsTcb {
	void **dtv;
	int count;
	unsigned int generation;	/* the generation the blocks are up to date with */
} LdTlsTcb;

/* what __tls_get_addr is passed: a module index and an offset in its block */
typedef struct LdTlsIndex {
	unsigned int index;
	unsigned int offset;
} LdTlsIndex;

/* the LdTlsIndex entries of a module, from its start-up code */
typedef struct LdTlsDescRegionInfo {
	SceSize size;
	LdTlsIndex *start;
	LdTlsIndex *end;
} LdTlsDescRegionInfo;

static SceUID destructHandler = -1;

static struct {
	LdTlsModule *module;
	int moduleNum;
	unsigned int generation;	/* counts every register and unregister */
	SceKernelLwMutexWork mutex;
} ldTls;

/* free the exiting thread's blocks */
static SceInt32 destructTCB(SceInt32 type, SceUID threadId, SceInt32 arg, void *pCommon)
{
	LdTlsTcb *tcb = sceKernelGetTLSAddr(TLS_TCB);
	int i;

	for (i = 1; i < tcb->count; i++) {
		if (tcb->dtv[i] != NULL)
			_FreeForTls(tcb->dtv[i]);
	}
	_FreeForTls(tcb->dtv);
	tcb->dtv = NULL;
	tcb->count = 0;
	tcb->generation = 0;
	return 0;
}

int _InitTlsMutex(void)
{
	int res = sceKernelCreateLwMutex(&ldTls.mutex, "SceLibcLdTlsModuleInfo",
					 SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO, 0, NULL);

	if (res < 0)
		return res;
	return 0;
}

int _TermTlsMutex(void)
{
	int ret = 0;

	if (sceKernelLockLwMutex(&ldTls.mutex, 1, NULL) < 0) {
		ret = -1;
	} else {
		if (ldTls.module != NULL)
			_FreeForTls(ldTls.module);
		if (sceKernelUnlockLwMutex(&ldTls.mutex, 1) < 0)
			ret = -1;
	}
	if (destructHandler >= 0 && sceKernelUnregisterThreadEventHandler(destructHandler) != 0)
		ret = -1;
	if (sceKernelDeleteLwMutex(&ldTls.mutex) < 0)
		return -1;
	return ret;
}

/* the index of module in the table, -1 if it is not there */
static int findModule(const void *module)
{
	int i;

	for (i = 1; i < ldTls.moduleNum; i++) {
		if (ldTls.module[i].module == module)
			return i;
	}
	return -1;
}

/* grow the table: the first new index, -1 if there is no memory */
static int growModule(void)
{
	int num = ldTls.moduleNum;
	LdTlsModule *module = _MallocForTls((num + MODULE_GROW) * sizeof(*module));

	if (module == NULL)
		return -1;
	if (ldTls.module != NULL) {
		memcpy(module, ldTls.module, num * sizeof(*module));
		_FreeForTls(ldTls.module);
	}
	ldTls.module = module;
	ldTls.moduleNum = num + MODULE_GROW;
	memset(&module[num], 0, MODULE_GROW * sizeof(*module));
	return num;
}

int _sceLdTlsRegisterModuleInfo(const void *module, const LdTlsDescRegionInfo *region, int isMain)
{
	SceKernelModuleInfo info;
	LdTlsModule *entry;
	LdTlsIndex *p;
	unsigned int generation;
	int index, i;

	if (sceKernelGetModuleInfoByAddr(module, &info) < 0)
		return -1;
	if (info.tlsInit == NULL && info.tlsInitSize == 0 && info.tlsAreaSize == 0)
		return 0;	/* the module has no TLS */
	if (sceKernelLockLwMutex(&ldTls.mutex, 1, NULL) < 0)
		return -1;
	if (ldTls.module == NULL && ldTls.moduleNum == 0) {	/* the first module */
		entry = _MallocForTls(MODULE_GROW * sizeof(*entry));
		ldTls.module = entry;
		if (entry == NULL)
			goto fail;
		ldTls.moduleNum = MODULE_GROW;
		for (i = MODULE_GROW; i != 0; i--, entry++) {
			entry->module = NULL;
			entry->generation = 0;
			entry->image = NULL;
			entry->initSize = 0;
			entry->size = 0;
		}
		destructHandler = sceKernelRegisterThreadEventHandler("SceLibcLdTlsDestructTCB",
								      SCE_UID_THREAD_ID_PROCESS_ALL,
								      SCE_KERNEL_THREAD_EVENT_TYPE_EXIT,
								      destructTCB, NULL);
		if (destructHandler < 0) {
			_FreeForTls(ldTls.module);
			ldTls.module = NULL;
			ldTls.moduleNum = 0;
			goto fail;
		}
	}
	index = 0;	/* the program itself */
	if (!isMain) {
		index = findModule(NULL);
		if (index == -1)
			index = growModule();
		if (index < 0) {
			sceKernelUnlockLwMutex(&ldTls.mutex, 1);
			return -1;
		}
	}
	generation = ++ldTls.generation;
	entry = &ldTls.module[index];
	entry->module = module;
	entry->generation = generation;
	entry->image = info.tlsInit;
	entry->initSize = info.tlsInitSize;
	entry->size = info.tlsAreaSize;
	for (p = region->start; p < region->end; p++)
		p->index = index;
	if (sceKernelUnlockLwMutex(&ldTls.mutex, 1) < 0)
		return -1;
	return 0;
fail:
	sceKernelUnlockLwMutex(&ldTls.mutex, 1);
	return -1;
}

int _sceLdTlsUnregisterModuleInfo(const void *module)
{
	LdTlsModule *entry;
	int index;

	if (sceKernelLockLwMutex(&ldTls.mutex, 1, NULL) < 0)
		return -1;
	index = findModule(module);
	if (index < 0) {
		sceKernelUnlockLwMutex(&ldTls.mutex, 1);
		return -1;
	}
	entry = &ldTls.module[index];
	entry->module = NULL;
	entry->generation = 0;
	entry->image = NULL;
	entry->initSize = 0;
	entry->size = 0;
	ldTls.generation++;
	if (sceKernelUnlockLwMutex(&ldTls.mutex, 1) < 0)
		return -1;
	return 0;
}

/* a new block of module for the thread's index i, holding the initial data */
static int allocBlock(LdTlsTcb *tcb, int i, const LdTlsModule *module)
{
	tcb->dtv[i] = NULL;
	if (module == NULL)
		return -1;
	tcb->dtv[i] = _MallocForTls(module->size);
	if (tcb->dtv[i] == NULL)
		return -1;
	memcpy(tcb->dtv[i], module->image, module->initSize);
	memset((char *)tcb->dtv[i] + module->initSize, 0, module->size - module->initSize);
	return 0;
}

/* bring the calling thread's blocks up to date with the table */
static __attribute__((noinline)) int updateTCB(LdTlsTcb *tcb)
{
	int num, i;

	if (sceKernelLockLwMutex(&ldTls.mutex, 1, NULL) < 0)
		return -1;
	num = tcb->count;
	if (num < ldTls.moduleNum) {	/* more modules: grow the dtv */
		void **dtv = _MallocForTls(ldTls.moduleNum * sizeof(*dtv));

		if (dtv == NULL)
			goto fail;
		if (tcb->dtv != NULL) {
			memcpy(dtv, tcb->dtv, tcb->count * sizeof(*dtv));
			_FreeForTls(tcb->dtv);
		}
		tcb->dtv = dtv;
		for (i = tcb->count; i < ldTls.moduleNum; i++)
			tcb->dtv[i] = NULL;
		num = ldTls.moduleNum;
		tcb->count = num;
	}
	for (i = 0; i < num; i++) {
		const LdTlsModule *module = &ldTls.module[i];
		void *block = tcb->dtv[i];

		if (module->module != NULL) {
			if (block == NULL || tcb->generation < module->generation) {
				if (i != 0) {	/* a fresh copy of the initial data */
					if (block != NULL)
						_FreeForTls(block);
					if (allocBlock(tcb, i, module) < 0)
						goto fail;
				} else if (block == NULL) {	/* the program's own is the thread's TLS */
					tcb->dtv[i] = (void *)__builtin_mrc(15, 0, 13, 0, 3);
				}
			}
		} else if (block != NULL) {	/* the module is gone */
			_FreeForTls(block);
			tcb->dtv[i] = NULL;
		}
		num = tcb->count;
	}
	tcb->generation = ldTls.generation;
	if (sceKernelUnlockLwMutex(&ldTls.mutex, 1) < 0)
		return -1;
	return 0;
fail:
	sceKernelUnlockLwMutex(&ldTls.mutex, 1);
	return -1;
}

void *__tls_get_addr(const LdTlsIndex *ti)
{
	LdTlsTcb *tcb;

	if (ti == NULL)
		return NULL;
	tcb = sceKernelGetTLSAddr(TLS_TCB);
	if (tcb->generation != ldTls.generation && updateTCB(tcb) != 0)
		return NULL;
	return (char *)tcb->dtv[ti->index] + ti->offset;
}
