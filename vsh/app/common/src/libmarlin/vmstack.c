/*
 * The VM's system calls that reach its host or the VMs it spawns, and the stack helpers they
 * use: two words pushed, one or two popped, and the top popped with the word under it read.
 */
#include "mln_internal.h"

/* pushes first, then second */
__attribute__((noinline)) unsigned int MlnImpl_81083010(MlnVmContext *context, unsigned int first, unsigned int second)
{
	unsigned char *sp = context->sp;

	if (context->stackLow > sp - 8)
		return MLN_VM_ERROR_STACK_OVERFLOW;
	if (sp > context->stackHigh)
		return MLN_VM_ERROR_STACK_UNDERFLOW;
	context->sp = sp - 4;
	MlnVmSetWord(sp - 4, first);
	context->sp -= 4;
	MlnVmSetWord(context->sp, second);
	return 0;
}

__attribute__((noinline)) unsigned int MlnImpl_810830A4(MlnVmContext *context, int *value)
{
	unsigned char *sp = context->sp;

	if (sp < context->stackLow)
		return MLN_VM_ERROR_STACK_OVERFLOW;
	if (sp >= context->stackHigh)
		return MLN_VM_ERROR_STACK_UNDERFLOW;
	*value = MLN_VM_WORD(sp);
	context->sp += 4;
	return 0;
}

__attribute__((noinline)) unsigned int MlnImpl_81083118(MlnVmContext *context, int *top, int *next)
{
	unsigned char *sp = context->sp;

	if (sp < context->stackLow)
		return MLN_VM_ERROR_STACK_OVERFLOW;
	if (context->stackHigh < sp + 8)
		return MLN_VM_ERROR_STACK_UNDERFLOW;
	*top = MLN_VM_WORD(sp);
	context->sp += 4;
	*next = MLN_VM_WORD(context->sp);
	context->sp += 4;
	return 0;
}

/* pops the top and reads the word under it, which stays */
__attribute__((noinline)) unsigned int MlnImpl_810831C0(MlnVmContext *context, int *top, int *next)
{
	unsigned char *sp = context->sp;

	if (sp < context->stackLow)
		return MLN_VM_ERROR_STACK_OVERFLOW;
	if (context->stackHigh < sp + 8)
		return MLN_VM_ERROR_STACK_UNDERFLOW;
	*top = MLN_VM_WORD(sp);
	context->sp += 4;
	*next = MLN_VM_WORD(context->sp);
	return 0;
}

/* System.Host.GetObject: index, path, buffer and its size; gives the type over the size */
unsigned int MlnImpl_8108325C(MlnVmContext *context)
{
	MlnVm *vm = context->vm;
	unsigned char *sp = context->sp;
	const unsigned char *path, *buffer;
	unsigned int index, pathAddress, bufferAddress, size, outSize = 0;
	MlnVmModule *module;
	MlnCoreHost *host;
	int type = 0;
	unsigned int ret;

	if (sp < context->stackLow) {
		ret = MLN_VM_ERROR_STACK_OVERFLOW;
		goto out;
	}
	if (context->stackHigh < sp + 16) {
		ret = MLN_VM_ERROR_STACK_UNDERFLOW;
		goto out;
	}
	index = MLN_VM_WORD(sp);
	context->sp = sp + 4;
	pathAddress = MLN_VM_WORD(sp + 4);
	context->sp = sp + 8;
	bufferAddress = MLN_VM_WORD(sp + 8);
	context->sp = sp + 12;
	size = MLN_VM_WORD(sp + 12);
	context->sp = sp + 16;
	path = context->data + pathAddress;
	buffer = context->data + bufferAddress;
	if (path < context->data || path >= context->dataEnd || buffer < context->data ||
	    buffer + size >= context->dataEnd) {
		ret = MLN_VM_ERROR_ACCESS;
		goto out;
	}
	host = vm->host;
	module = context->module;
	ret = ((MlnVmHostGetObjectFunction)host->functions[MLN_VM_HOST_GET_OBJECT])(
		host, index, (const char *)path, (unsigned char *)buffer, size, module->signer,
		module->signerSize, module->attributes, &type, &outSize);
out:
	MlnImpl_8106A15C(ret, &type);
	ret = MlnImpl_81083010(context, outSize, type);
	if (ret != 0)
		goto out;
	return ret;
}

/* System.Host.SetObject: index, path, data, its type and size; gives the result */
unsigned int MlnImpl_81083408(MlnVmContext *context)
{
	MlnVm *vm = context->vm;
	unsigned char *sp = context->sp;
	const unsigned char *path, *data;
	unsigned int index, pathAddress, size;
	int dataAddress, type, result = 0;
	MlnVmModule *module;
	MlnCoreHost *host;
	unsigned int ret;

	if (sp < context->stackLow) {
		ret = MLN_VM_ERROR_STACK_OVERFLOW;
		goto out;
	}
	if (context->stackHigh < sp + 20) {
		ret = MLN_VM_ERROR_STACK_UNDERFLOW;
		goto out;
	}
	index = MLN_VM_WORD(sp);
	context->sp = sp + 4;
	pathAddress = MLN_VM_WORD(sp + 4);
	context->sp = sp + 8;
	dataAddress = MLN_VM_WORD(sp + 8);
	context->sp = sp + 12;
	type = MLN_VM_WORD(sp + 12);
	context->sp = sp + 16;
	size = MLN_VM_WORD(sp + 16);
	context->sp = sp + 20;
	path = context->data + pathAddress;
	if (path < context->data || path >= context->dataEnd) {
		ret = MLN_VM_ERROR_ACCESS;
		goto out;
	}
	data = NULL;
	if (dataAddress > 0) {
		data = context->data + dataAddress;
		if (data < context->data || data + size >= context->dataEnd) {
			ret = MLN_VM_ERROR_ACCESS;
			goto out;
		}
	}
	host = vm->host;
	module = context->module;
	ret = ((MlnVmHostSetObjectFunction)host->functions[MLN_VM_HOST_SET_OBJECT])(
		host, index, (const char *)path, data, size, module->signer, module->signerSize,
		type, &result);
out:
	MlnImpl_8106A15C(ret, &result);
	ret = MlnVmPush(context, result);
	if (ret != 0)
		goto out;
	return ret;
}

/* Octopus.Links.IsNodeReachable: the node's uid; gives the result over 0 */
unsigned int MlnImpl_81083638(MlnVmContext *context)
{
	MlnVm *vm = context->vm;
	MlnCoreHost *host;
	const char *uid;
	int address, result = 0;
	unsigned int ret;

	ret = MlnImpl_810830A4(context, &address);
	if (ret != 0)
		goto out;
	uid = (const char *)context->data + address;
	if ((const unsigned char *)uid < context->data ||
	    (const unsigned char *)uid >= context->dataEnd) {
		ret = MLN_VM_ERROR_ACCESS;
		goto out;
	}
	host = vm->host;
	ret = ((MlnVmHostReachableFunction)host->functions[MLN_VM_HOST_IS_NODE_REACHABLE])(
		host, MlnStrlen(uid), uid, &result);
out:
	MlnImpl_8106A15C(ret, &result);
	ret = MlnImpl_81083010(context, 0, result);
	if (ret != 0)
		goto out;
	return ret;
}

/* System.Host.ReleaseVm: the index of a VM this one spawned; gives the result */
unsigned int MlnImpl_810836DC(MlnVmContext *context)
{
	MlnVm *vm = context->vm;
	MlnVmGroup *group;
	MlnVm *child;
	unsigned int index = 0;
	int result = 0;
	unsigned int ret;

	ret = MlnImpl_810830A4(context, (int *)&index);
	if (ret != 0)
		goto out;
	group = vm->group;
	if (index >= group->vmCount || (child = group->vms[index]) == NULL) {
		ret = MLN_VM_ERROR_SYSTEM_CALL;
		goto out;
	}
	ret = MlnImpl_8106A590(child);
	if (ret == 0)
		vm->children ^= 1 << index;
out:
	MlnImpl_8106A15C(ret, &result);
	ret = MlnVmPush(context, result);
	if (ret != 0)
		goto out;
	return ret;
}

/*
 * System.Host.SpawnVm: the uid of a code module of the group and options (0); gives the
 * result over the new VM's index
 */
unsigned int MlnImpl_810837CC(MlnVmContext *context)
{
	MlnVm *vm = context->vm;
	MlnVmGroup *group = vm->group;
	MlnVm *child = NULL;
	const char *uid;
	int address = 0, options = 0, result = 0, index = 0, i;
	unsigned int ret;

	ret = MlnImpl_81083118(context, &address, &options);
	if (ret != 0)
		goto out;
	if (options != 0) {
		result = MLN_VM_RESULT_UNSUPPORTED;
		ret = 1;
		goto out;
	}
	uid = (const char *)context->data + address;
	if ((const unsigned char *)uid < context->data ||
	    (const unsigned char *)uid >= context->dataEnd) {
		index = options;
		ret = MLN_VM_ERROR_ACCESS;
		goto out;
	}
	for (i = 0; i < group->moduleCount; i++)
		if (MlnBytesCompare(MLN_HEAP_GENERAL, group->uids[i]->size, group->uids[i]->data,
				    MlnStrlen(uid), uid) == 0)
			break;
	if (i >= group->moduleCount) {
		result = MLN_VM_RESULT_NO_SUCH_ITEM;
		ret = 1;
		goto out;
	}
	ret = MlnVmCreate(vm->memory->size, vm->host, &child);
	if (ret != 0)
		goto out;
	child->group = group;
	child->index = group->vmCount;
	if ((unsigned int)child->index >= MLN_VM_INSTANCES) {
		result = MLN_VM_RESULT_OUT_OF_RESOURCES;
		ret = 1;
		goto out;
	}
	ret = MlnImpl_8106BDA0(child, group->uids[i]);
	if (ret != 0)
		goto out;
	vm->children |= 1 << child->index;
	index = child->index;
	group->vms[index] = child;
	child = NULL;
	group->vmCount++;
out:
	if (child != NULL)
		MlnImpl_8106A590(child);
	MlnImpl_8106A15C(ret, &result);
	ret = MlnImpl_81083010(context, index, result);
	if (ret != 0)
		goto out;
	return ret;
}

/*
 * System.Host.CallVm: a spawned VM's index, the routine's name, the parameters and their
 * size, and where the results go and their room. The parameters are pushed on the VM's stack
 * with their size; the routine leaves its status, where its results are and their size.
 * Gives the result over the status and the results' size.
 */
unsigned int MlnImpl_81083A58(MlnVmContext *context)
{
	MlnVm *vm = context->vm;
	unsigned char *sp = context->sp;
	const unsigned char *routine, *parameters;
	unsigned int index, routineAddress, aligned;
	int parametersAddress, parametersSize, resultsAddress, resultsSize;
	int status, resultsOffset;
	unsigned int length = 0, outStatus = 0;
	int result = 0;
	MlnVmGroup *group;
	MlnVm *target;
	unsigned char *top;
	unsigned int ret, code;

	if (sp < context->stackLow) {
		ret = MLN_VM_ERROR_STACK_OVERFLOW;
		goto out;
	}
	if (context->stackHigh < sp + 24) {
		ret = MLN_VM_ERROR_STACK_UNDERFLOW;
		goto out;
	}
	index = MLN_VM_WORD(sp);
	context->sp = sp + 4;
	routineAddress = MLN_VM_WORD(sp + 4);
	context->sp = sp + 8;
	parametersAddress = MLN_VM_WORD(sp + 8);
	context->sp = sp + 12;
	parametersSize = MLN_VM_WORD(sp + 12);
	context->sp = sp + 16;
	resultsAddress = MLN_VM_WORD(sp + 16);
	context->sp = sp + 20;
	resultsSize = MLN_VM_WORD(sp + 20);
	context->sp = sp + 24;
	group = vm->group;
	if (index >= group->vmCount || (target = group->vms[index]) == NULL) {
		ret = MLN_VM_ERROR_SYSTEM_CALL;
		goto out;
	}
	routine = context->data + routineAddress;
	if (routine < context->data || routine >= context->dataEnd) {
		ret = MLN_VM_ERROR_ACCESS;
		goto out;
	}
	parameters = NULL;
	if (parametersAddress > 0) {
		parameters = context->data + parametersAddress;
		if (parameters < context->data ||
		    parameters + parametersSize >= context->dataEnd) {
			ret = MLN_VM_ERROR_ACCESS;
			goto out;
		}
	}
	if (resultsAddress > 0 &&
	    (context->data + resultsAddress < context->data ||
	     context->data + resultsAddress + resultsSize >= context->dataEnd)) {
		ret = MLN_VM_ERROR_ACCESS;
		goto out;
	}
	if (parametersAddress > 0) {
		aligned = (parametersSize + 3) / 4 * 4;
		target->sp -= aligned;
		if (target->sp < target->memory->data + target->memory->dataSize) {
			ret = MLN_VM_ERROR_STACK_OVERFLOW;
			goto out;
		}
		if (target->sp > target->memory->data + target->memory->size) {
			ret = MLN_VM_ERROR_STACK_UNDERFLOW;
			goto out;
		}
		MlnMemcpy(target->sp, parameters, parametersSize);
		MlnMemset(target->sp + parametersSize, 0, aligned - parametersSize);
	}
	target->sp -= 4;
	if (target->sp < target->memory->data + target->memory->dataSize) {
		ret = MLN_VM_ERROR_STACK_OVERFLOW;
		goto out;
	}
	if (target->sp >= target->memory->data + target->memory->size) {
		ret = MLN_VM_ERROR_STACK_UNDERFLOW;
		goto out;
	}
	MlnVmSetWord(target->sp, parametersSize);
	ret = MlnImpl_8106A780(target, (const char *)routine, &status);
	if (ret != 0)
		goto out;
	if (status == MLN_VM_RESULT_FAULT) {
		result = MLN_VM_RESULT_FAULT;
		goto out;
	}
	if (status != 0) {
		ret = MLN_VM_ERROR_CALL_STATUS;
		goto out;
	}
	top = target->sp;
	if (top < target->memory->data + target->memory->dataSize) {
		ret = MLN_VM_ERROR_STACK_OVERFLOW;
		goto out;
	}
	if (target->memory->data + target->memory->size < top + 12) {
		ret = MLN_VM_ERROR_STACK_UNDERFLOW;
		goto out;
	}
	outStatus = MLN_VM_WORD(top);
	target->sp = top + 4;
	resultsOffset = MLN_VM_WORD(top + 4);
	target->sp = top + 8;
	length = MLN_VM_WORD(top + 8);
	target->sp = top + 12;
	if (resultsOffset > 0 && resultsAddress > 0) {
		if (resultsSize >= (int)length) {
			MlnMemcpy(context->data + resultsAddress,
				  target->memory->data + resultsOffset, length);
		} else {
			ret = 1;
			result = MLN_VM_RESULT_INSUFFICIENT_SPACE;
		}
	}
out:
	code = MlnImpl_8106A15C(ret, &result);
	sp = context->sp;
	if (context->stackLow > sp - 12) {
		ret = MLN_VM_ERROR_STACK_OVERFLOW;
		goto out;
	}
	if (sp > context->stackHigh) {
		ret = MLN_VM_ERROR_STACK_UNDERFLOW;
		goto out;
	}
	context->sp = sp - 4;
	MlnVmSetWord(sp - 4, length);
	context->sp -= 4;
	MlnVmSetWord(context->sp, outStatus);
	context->sp -= 4;
	MlnVmSetWord(context->sp, result);
	return code;
}
