/*
 * The AS handle API: the log, the class table, creating a handle from a class name and a
 * configuration string, destroying it with its children, and the calls that reach a
 * handle's operations under a reference.
 */

#include <stdarg.h>
#include <stddef.h>
#include <kernel/threadmgr.h>

#include "as_ssp.h"
#include "as.h"
#include "as_buss.h"
#include "as_core.h"
#include "as_rack.h"

/* the operations a class leaves out */
static int sceSasAsDefaultLock(SceSasAsHandle *handle, SceUInt32 block, void **pointer)
{
	return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
}

static int sceSasAsDefaultUnlock(SceSasAsHandle *handle, SceUInt32 block)
{
	return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
}

static int sceSasAsDefaultReserved(SceSasAsHandle *handle)
{
	return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
}

static int sceSasAsDefaultSetEvent(SceSasAsHandle *handle, SceUInt32 event, SceUInt32 value)
{
	return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
}

static int sceSasAsDefaultGetState(SceSasAsHandle *handle, SceUInt32 state, void *value)
{
	return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
}

static int sceSasAsDefaultUpdate(SceSasAsHandle *handle, const void *render)
{
	return 0;
}

static int sceSasAsDefaultRender(SceSasAsHandle *handle, const void *render, void *jobs)
{
	return 0;
}

static int sceSasAsDefaultRenderEnd(SceSasAsHandle *handle)
{
	return 0;
}

static const SceSasAsClass *const sceSasAsClassTable[] = {
	&sceSasAsCoreClass,
	&sceSasAsBussClass,
	&sceSasAsSynthRackClass,
	&sceSasAsMasterRackClass,
	&sceSasAsReverbRackClass,
	&sceSasAsMixerRackClass,
	NULL,
	NULL,
};

static SceSasAsLogHandler volatile sceSasAsLogHandlers[SCE_SAS_AS_LOG_HANDLER_COUNT];
/* the handles without a parent */
static volatile SceUInt32 sceSasAsRootLock;
static SceSasAsHandle *sceSasAsRootHandles;

int sceSasAsLog(int level, int code, const char *format, va_list arguments)
{
	char message[SCE_SAS_AS_LOG_LENGTH];
	SceSasAsLogHandler handler;
	int ret;
	int i;

	for (i = 0; i < SCE_SAS_AS_LOG_HANDLER_COUNT; i++) {
		handler = sceSasAsLogHandlers[i];
		if (handler != NULL) {
			sceSasAsVsnprintf(message, sizeof(message), format, arguments);
			ret = handler(level, code, message);
		}
	}
	return ret;
}

static int sceSasAsCreateHandle(SceSasAsAllocator *allocator, SceSasAsHandle *parent,
				const char *className, const char *config, SceSasAsHandle **handle)
{
	const SceSasAsClass *const *entry;
	const SceSasAsClass *class;
	SceSasAsConfigEntry **list;
	SceSasAsHandle *object;
	SceUInt32 *subsystem;
	SceSize configSize;
	SceUInt32 numChannels;
	SceUInt32 numProcessors;
	SceUInt32 numGranularities;
	int ret;

	entry = sceSasAsClassTable;
	class = *entry;
	while (sceSasAsStrcmp(class->name, className) != 0) {
		class = *++entry;
		if (class == NULL)
			return SCE_SAS_AS_ERROR_NOT_FOUND;
	}
	ret = sceSasAsConfigGetSize(config, &configSize);
	if (ret < 0)
		return ret;
	ret = sceSasAsConfigCreate(config, __builtin_alloca(configSize), configSize, &list);
	if (ret < 0)
		return ret;
	sceSasAsConfigGetUInt(list, "numChannels", 2, &numChannels);
	sceSasAsConfigGetUInt(list, "numProcessors", 8, &numProcessors);
	sceSasAsConfigGetUInt(list, "numGranularities", 256, &numGranularities);
	ret = class->create(allocator, list, &object);
	if (ret < 0)
		return ret;
	subsystem = sceSasAsAllocatorAlloc(allocator, sizeof(*subsystem), 1);
	if (object != NULL) {
		object->magic = SCE_SAS_AS_HANDLE_MAGIC;
		object->classId = class->id;
		object->class = class;
		object->parent = parent;
		object->buffer = sceSasAsAllocatorGetBuffer(allocator);
		object->size = sceSasAsAllocatorGetSize(allocator);
		object->ops.lock = class->ops.lock != NULL ? class->ops.lock : sceSasAsDefaultLock;
		object->ops.unlock = class->ops.unlock != NULL ? class->ops.unlock : sceSasAsDefaultUnlock;
		object->ops.reserved = class->ops.reserved != NULL ?
				       class->ops.reserved : sceSasAsDefaultReserved;
		object->ops.setEvent = class->ops.setEvent != NULL ?
				       class->ops.setEvent : sceSasAsDefaultSetEvent;
		object->ops.getState = class->ops.getState != NULL ?
				       class->ops.getState : sceSasAsDefaultGetState;
		object->ops.update = class->ops.update != NULL ? class->ops.update : sceSasAsDefaultUpdate;
		object->ops.render = class->ops.render != NULL ? class->ops.render : sceSasAsDefaultRender;
		object->ops.renderEnd = class->ops.renderEnd != NULL ?
					class->ops.renderEnd : sceSasAsDefaultRenderEnd;
		object->children = NULL;
		object->next = NULL;
		object->numChannels = numChannels;
		object->numProcessors = numProcessors;
		object->numGranularities = numGranularities;
		*subsystem = SCE_SAS_AS_SUBSYSTEM_MAGIC;
		object->subsystem = subsystem;
		if (parent != NULL) {
			if (parent->magic == SCE_SAS_AS_HANDLE_MAGIC &&
			    sceSasAsHandleTryAddRef(parent)) {
				sceSasAsSpinLock(&parent->lock);
				object->next = parent->children;
				parent->children = object;
				sceSasAsSpinUnlock(&parent->lock);
				sceSasAsHandleDropRef(parent);
			}
		} else {
			sceSasAsSpinLock(&sceSasAsRootLock);
			object->next = sceSasAsRootHandles;
			sceSasAsRootHandles = object;
			sceSasAsSpinUnlock(&sceSasAsRootLock);
		}
	}
	if (handle != NULL)
		*handle = object;
	return 0;
}

int sceSasAsInitialize(void)
{
	return 0;
}

int sceSasAsTerminate(void)
{
	return 0;
}

int sceSasAsRegisterLogHandler(SceSasAsLogHandler handler)
{
	volatile SceUInt32 *slot;

	for (slot = (volatile SceUInt32 *)sceSasAsLogHandlers;
	     slot != (volatile SceUInt32 *)&sceSasAsLogHandlers[SCE_SAS_AS_LOG_HANDLER_COUNT]; slot++) {
		if (sceSasAsCompareAndSwap(slot, 0, (SceUInt32)handler) == 0)
			return 0;
	}
	return SCE_SAS_AS_ERROR_NO_LOG_HANDLER;
}

int sceSasAsUnregisterLogHandler(SceSasAsLogHandler handler)
{
	volatile SceUInt32 *slot;

	for (slot = (volatile SceUInt32 *)sceSasAsLogHandlers;
	     slot != (volatile SceUInt32 *)&sceSasAsLogHandlers[SCE_SAS_AS_LOG_HANDLER_COUNT]; slot++) {
		if (sceSasAsCompareAndSwap(slot, (SceUInt32)handler, 0) == (SceUInt32)handler)
			return 0;
	}
	return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
}

int sceSasAsError(int code, const char *format, ...)
{
	va_list arguments;

	va_start(arguments, format);
	sceSasAsLog(SCE_SAS_AS_LOG_ERROR, code, format, arguments);
	va_end(arguments);
	return code;
}

int sceSasAsWarning(const char *format, ...)
{
	va_list arguments;
	int ret;

	va_start(arguments, format);
	ret = sceSasAsLog(SCE_SAS_AS_LOG_WARNING, 0, format, arguments);
	va_end(arguments);
	return ret;
}

int sceSasAsInfo(const char *format, ...)
{
	va_list arguments;
	int ret;

	va_start(arguments, format);
	ret = sceSasAsLog(SCE_SAS_AS_LOG_INFO, 0, format, arguments);
	va_end(arguments);
	return ret;
}

int sceSasAsHandleIsClass(const SceSasAsHandle *handle, SceUInt32 classId)
{
	if (handle == NULL)
		return 0;
	return handle->magic == SCE_SAS_AS_HANDLE_MAGIC && handle->classId == classId &&
	       handle->class != NULL;
}

int sceSasAsHandleCheckSubsystem(const SceSasAsHandle *handle)
{
	if (handle->magic != SCE_SAS_AS_HANDLE_MAGIC ||
	    *handle->subsystem != SCE_SAS_AS_SUBSYSTEM_MAGIC)
		return sceSasAsError(SCE_SAS_AS_ERROR_BROKEN, "Sub system has broken (%X,%X)\n",
				     handle->magic, *handle->subsystem);
	return 0;
}

int sceSasAsHandleAddRef(SceSasAsHandle *handle)
{
	return sceSasAsHandleTryAddRef(handle);
}

void sceSasAsHandleRelease(SceSasAsHandle *handle)
{
	sceSasAsHandleDropRef(handle);
}

int sceSasAsHandleGetSize(const char *className, const char *config, SceSize *size)
{
	SceSasAsAllocator allocator;
	int ret;

	sceSasAsAllocatorInit(&allocator, NULL);
	ret = sceSasAsCreateHandle(&allocator, NULL, className, config, NULL);
	if (ret < 0)
		return ret;
	if (size != NULL)
		*size = sceSasAsAllocatorGetSize(&allocator);
	return 0;
}

int sceSasAsHandleCreate(SceSasAsHandle *parent, const char *className, const char *config,
			 void *buffer, SceSize size, SceSasAsHandle **handle)
{
	SceSasAsAllocator allocator;
	int referenced;
	int ret;

	sceSasAsAllocatorInit(&allocator, NULL);
	ret = sceSasAsCreateHandle(&allocator, NULL, className, config, NULL);
	if (ret < 0)
		return ret;
	if (size != sceSasAsAllocatorGetSize(&allocator))
		return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	referenced = 0;
	if (parent != NULL) {
		if (!sceSasAsHandleTryAddRef(parent))
			return sceSasAsError(SCE_SAS_AS_ERROR_INVALID_HANDLE,
					     "Parent handle is invalid handle (0x%08x)\n", parent);
		referenced = 1;
	}
	sceSasAsAllocatorInit(&allocator, buffer);
	ret = sceSasAsCreateHandle(&allocator, parent, className, config, handle);
	if (ret < 0) {
		if (referenced)
			sceSasAsHandleDropRef(parent);
		return ret;
	}
	return 0;
}

int sceSasAsHandleDestroy(SceSasAsHandle *handle, void **buffer, SceSize *size)
{
	SceSasAsHandle *parent;
	SceSasAsHandle *child;
	SceSasAsHandle **link;
	int ret;

	if (handle == NULL || handle->magic != SCE_SAS_AS_HANDLE_MAGIC)
		return SCE_SAS_AS_ERROR_INVALID_HANDLE;
	if (*handle->subsystem != SCE_SAS_AS_SUBSYSTEM_MAGIC)
		sceSasAsError(SCE_SAS_AS_ERROR_BROKEN, "Sub system has broken (%X,%X)\n",
			      handle->magic, *handle->subsystem);
	for (;;) {
		sceSasAsSpinLock(&handle->lock);
		child = handle->children;
		sceSasAsSpinUnlock(&handle->lock);
		if (child == NULL)
			break;
		ret = sceSasAsHandleDestroy(child, NULL, NULL);
		if (ret < 0)
			return ret;
	}
	if (sceSasAsCompareAndSwap((volatile SceUInt32 *)&handle->referenceCount, 0, -1) != 0)
		return sceSasAsError(SCE_SAS_AS_ERROR_BUSY,
				     "Couldn't destroy AS handle. The handler is still using. (%p,%s)\n",
				     handle, handle->class->name);
	if (handle->class->destroy(handle) < 0)
		sceSasAsError(SCE_SAS_AS_ERROR_BUSY, "Failed to destroy AS handle. (%p,%s)\n",
			      handle, handle->class->name);
	parent = handle->parent;
	if (parent != NULL) {
		if (parent->magic == SCE_SAS_AS_HANDLE_MAGIC && sceSasAsHandleTryAddRef(parent)) {
			sceSasAsSpinLock(&parent->lock);
			for (link = &parent->children; *link != NULL; link = &(*link)->next) {
				if (*link == handle) {
					*link = handle->next;
					parent->class->removeChild(parent, handle);
					break;
				}
			}
			sceSasAsSpinUnlock(&parent->lock);
			sceSasAsHandleDropRef(parent);
		}
	} else {
		sceSasAsSpinLock(&sceSasAsRootLock);
		for (link = &sceSasAsRootHandles; *link != NULL; link = &(*link)->next) {
			if (*link == handle) {
				*link = handle->next;
				break;
			}
		}
		sceSasAsSpinUnlock(&sceSasAsRootLock);
	}
	if (buffer != NULL)
		*buffer = handle->buffer;
	if (size != NULL)
		*size = handle->size;
	if (handle->parent != NULL)
		sceSasAsHandleDropRef(handle->parent);
	sceSasAsMemZero(handle, sizeof(*handle));
	return 0;
}

int sceSasAsHandleLock(SceSasAsHandle *handle, SceUInt32 block, void **pointer)
{
	int ret;

	if (pointer != NULL && handle != NULL && handle->magic == SCE_SAS_AS_HANDLE_MAGIC &&
	    sceSasAsHandleTryAddRef(handle)) {
		ret = handle->ops.lock(handle, block, pointer);
		sceSasAsHandleDropRef(handle);
		return ret;
	}
	return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
}

int sceSasAsHandleUnlock(void *pointer)
{
	SceSasAsLockedBlock *locked;
	SceSasAsHandle *handle;
	int ret;

	if (pointer != NULL) {
		locked = (SceSasAsLockedBlock *)pointer - 1;
		handle = locked->handle;
		if (handle != NULL && handle->magic == SCE_SAS_AS_HANDLE_MAGIC &&
		    sceSasAsHandleTryAddRef(handle)) {
			ret = handle->ops.unlock(locked->handle, locked->block);
			sceSasAsHandleDropRef(handle);
			return ret;
		}
	}
	return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
}

int sceSasAsHandleSetEvent(SceSasAsHandle *handle, SceUInt32 event, SceUInt32 value)
{
	int ret;

	if (handle != NULL && handle->magic == SCE_SAS_AS_HANDLE_MAGIC &&
	    sceSasAsHandleTryAddRef(handle)) {
		ret = handle->ops.setEvent(handle, event, value);
		sceSasAsHandleDropRef(handle);
		return ret;
	}
	return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
}

int sceSasAsHandleGetState(SceSasAsHandle *handle, SceUInt32 state, void *value)
{
	int ret;

	if (value != NULL && handle != NULL && handle->magic == SCE_SAS_AS_HANDLE_MAGIC &&
	    sceSasAsHandleTryAddRef(handle)) {
		ret = handle->ops.getState(handle, state, value);
		sceSasAsHandleDropRef(handle);
		return ret;
	}
	return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
}

static inline int sceSasAsIsCore(const SceSasAsHandle *handle)
{
	return handle != NULL && handle->magic == SCE_SAS_AS_HANDLE_MAGIC &&
	       handle->classId == SCE_SAS_AS_CLASS_CORE && handle->class != NULL;
}

int sceSasAsCoreSetOutput(SceSasAsHandle *core, SceSasAsHandle *buss)
{
	SceSasAsHandle **output;
	int ret;

	if (sceSasAsIsCore(core) && sceSasAsHandleTryAddRef(core)) {
		ret = core->ops.lock(core, SCE_SAS_AS_CORE_BLOCK_OUTPUT, (void **)&output);
		sceSasAsHandleDropRef(core);
		if (ret < 0)
			return ret;
		*output = buss;
		ret = sceSasAsHandleUnlock(output);
		return ret < 0 ? ret : 0;
	}
	return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
}

int sceSasAsCoreSetRacks(SceSasAsHandle *core, const SceUIntPtr *racks, int count)
{
	SceSasAsCoreRacks *list;
	void *block;
	int ret;
	int i;

	if (sceSasAsIsCore(core) && sceSasAsHandleTryAddRef(core)) {
		ret = core->ops.lock(core, SCE_SAS_AS_CORE_BLOCK_RACKS, &block);
		sceSasAsHandleDropRef(core);
		if (ret < 0)
			return ret;
		list = block;
		if (list->capacity < count) {
			sceSasAsHandleUnlock(list);
			return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
		}
		for (i = 0; i < count; i++)
			list->racks[i] = racks[i];
		list->count = count;
		ret = sceSasAsHandleUnlock(list);
		return ret < 0 ? ret : 0;
	}
	return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
}

int sceSasAsCoreSetGranularity(SceSasAsHandle *core, SceUInt32 granularity)
{
	if (!sceSasAsIsCore(core))
		return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	return sceSasAsCoreClassSetGranularity((SceSasAsCore *)core, granularity);
}

int sceSasAsCoreGetGranularity(SceSasAsHandle *core, SceUInt32 *granularity)
{
	if (!sceSasAsIsCore(core))
		return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	if (granularity != NULL)
		*granularity = sceSasAsCoreClassGetGranularity((SceSasAsCore *)core);
	return 0;
}

int sceSasAsCoreRender(SceSasAsHandle *core, void *output, void *jobs)
{
	if (!sceSasAsIsCore(core))
		return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	sceSasAsCoreClassRender((SceSasAsCore *)core, output, jobs);
	return 0;
}

int sceSasAsCoreLockUpdate(SceSasAsHandle *core)
{
	if (!sceSasAsIsCore(core))
		return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	sceSasAsCoreClassLockUpdate((SceSasAsCore *)core);
	return 0;
}

int sceSasAsCoreUnlockUpdate(SceSasAsHandle *core)
{
	if (!sceSasAsIsCore(core))
		return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	sceSasAsCoreClassUnlockUpdate((SceSasAsCore *)core);
	return 0;
}
