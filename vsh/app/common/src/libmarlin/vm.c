/*
 * The Plankton virtual machine that runs a control's code: its interpreter, the pkCM
 * code module loader and the VM's interface to its host (0x8106A15C-0x8106CB50). The
 * file's name is ours.
 */
#include "mln_internal.h"

/* what a routine's run ends with */
#define MLN_VM_HALT			0x23090001	/* STOP, or RET from the routine */
#define MLN_VM_ERROR_MEMORY		0x23090002	/* the VM's memory is too small */
#define MLN_VM_ERROR_MODULE		0x23090003	/* the code module cannot be used */
#define MLN_VM_ERROR_NOT_FOUND		0x23090004	/* no module exports the routine */
#define MLN_VM_ERROR_DATA_SIZE		0x23090005	/* the data segment does not fit */
#define MLN_VM_ERROR_STACK_UNDERFLOW	0x23090006
#define MLN_VM_ERROR_STACK_OVERFLOW	0x23090007
#define MLN_VM_ERROR_CALL_DEPTH		0x23090008
#define MLN_VM_ERROR_DIVIDE		0x23090009
#define MLN_VM_ERROR_OPCODE		0x2309000A
#define MLN_VM_ERROR_RANGE_LAST		0x2309000B
#define MLN_VM_ERROR_SYSTEM_CALL	0x2309000C
#define MLN_VM_ERROR_ACCESS		0x2309000D	/* code or data out of range */
#define MLN_VM_ERROR_MODULES		0x2309000E	/* the group has no room for a module */
#define MLN_VM_ERROR_UNLOAD		0x2309000F
#define MLN_VM_ERROR_LOADED		0x23090011	/* the VM already has its module */

/* the result codes a routine sees */
#define MLN_VM_RESULT_FAILURE		(-1)
#define MLN_VM_RESULT_INVALID_PARAMETERS (-2)
#define MLN_VM_RESULT_OUT_OF_MEMORY	(-4)
#define MLN_VM_RESULT_NO_SUCH_ITEM	(-6)
#define MLN_VM_RESULT_FAULT		(-9)
#define MLN_VM_RESULT_INVALID_MODULE	(-10)

/* the instructions */
#define MLN_VM_OP_NOP		0
#define MLN_VM_OP_PUSH		1
#define MLN_VM_OP_DROP		2
#define MLN_VM_OP_DUP		3
#define MLN_VM_OP_SWAP		4
#define MLN_VM_OP_ADD		5
#define MLN_VM_OP_MUL		6
#define MLN_VM_OP_SUB		7
#define MLN_VM_OP_DIV		8
#define MLN_VM_OP_MOD		9
#define MLN_VM_OP_NEG		10
#define MLN_VM_OP_CMP		11
#define MLN_VM_OP_AND		12
#define MLN_VM_OP_OR		13
#define MLN_VM_OP_XOR		14
#define MLN_VM_OP_NOT		15
#define MLN_VM_OP_SHL		16
#define MLN_VM_OP_SHR		17
#define MLN_VM_OP_JMP		18
#define MLN_VM_OP_JSR		19
#define MLN_VM_OP_JSRR		20
#define MLN_VM_OP_RET		21
#define MLN_VM_OP_BRA		22
#define MLN_VM_OP_BRP		23
#define MLN_VM_OP_BRN		24
#define MLN_VM_OP_BRZ		25
#define MLN_VM_OP_PEEK		26
#define MLN_VM_OP_POKE		27
#define MLN_VM_OP_PEEKB		28
#define MLN_VM_OP_POKEB		29
#define MLN_VM_OP_PUSHSP	30
#define MLN_VM_OP_POPSP		31
#define MLN_VM_OP_CALL		32
#define MLN_VM_OP_STOP		255

/* the system calls of CALL */
#define MLN_VM_CALL_NO_OPERATION		0
#define MLN_VM_CALL_DEBUG_PRINT			1
#define MLN_VM_CALL_FIND_SYSTEM_CALL_BY_NAME	2
#define MLN_VM_CALL_HOST_GET_OBJECT		3
#define MLN_VM_CALL_HOST_SET_OBJECT		4
#define MLN_VM_CALL_DEBUG			0x3FFF
#define MLN_VM_CALL_HOST_GET_TRUSTED_TIME	0x4001
#define MLN_VM_CALL_HOST_GET_LOCAL_TIME		0x4002
#define MLN_VM_CALL_HOST_GET_LOCAL_TIME_OFFSET	0x4003
#define MLN_VM_CALL_HOST_GET_LOCAL_TRUSTED_TIME	0x4004
#define MLN_VM_CALL_HOST_SPAWN_VM		0x4005
#define MLN_VM_CALL_HOST_CALL_VM		0x4006
#define MLN_VM_CALL_HOST_RELEASE_VM		0x4007
#define MLN_VM_CALL_IS_NODE_REACHABLE		0x4008

#define MLN_VM_MEMORY_MIN	512
#define MLN_VM_DATA_START	256	/* the data segments start above the VM's own words */
#define MLN_VM_STACK_MAX	0x80	/* the most stack a module may ask for */

#define MLN_VM_ALIGN(x)		(((x) + 3) & ~3)

/* a big-endian word of the VM's memory or code */
#define MLN_VM_WORD(p) \
	((unsigned int)(p)[0] << 24 | (unsigned int)(p)[1] << 16 | (unsigned int)(p)[2] << 8 | \
	 (unsigned int)(p)[3])

/* the protocol and type of a code module the VM runs */
static const MlnBuffer MlnVmProtocol = {
	40, (unsigned char *)"http://www.octopus-drm.com/specs/scp-1_0"
};
static const MlnBuffer MlnVmType = {
	41, (unsigned char *)"http://www.octopus-drm.com/specs/pkcm-1_0"
};

/* a code module's segments while it is parsed (MlnImpl_8106BDA0) */
typedef struct MlnVmImage {
	int dataSize;			/* -1 until the module's pkDS */
	const unsigned char *data;
	int codeSize;			/* -1 until its pkCS */
	const unsigned char *code;
	int exportCount;		/* -1 until its pkEX */
	MlnVmExport *exports;
	int hasRequirements;		/* its pkRQ */
	unsigned int requirements[4];	/* reserved, memory, stack, reserved */
} MlnVmImage;

static inline __attribute__((always_inline)) void MlnVmSetWord(unsigned char *p, unsigned int value)
{
	p[0] = value >> 24;
	p[1] = value >> 16;
	p[2] = value >> 8;
	p[3] = value;
}

/* push a word onto a run's stack */
static inline __attribute__((always_inline)) unsigned int MlnVmPush(MlnVmContext *context,
								    unsigned int value)
{
	context->sp -= 4;
	if (context->sp < context->stackLow)
		return MLN_VM_ERROR_STACK_OVERFLOW;
	if (context->sp >= context->stackHigh)
		return MLN_VM_ERROR_STACK_UNDERFLOW;
	MlnVmSetWord(context->sp, value);
	return 0;
}

/* pop a word from a VM's stack */
static inline __attribute__((always_inline)) unsigned int MlnVmPop(MlnVm *vm, unsigned int *value)
{
	unsigned char *sp = vm->sp;

	if (sp >= vm->memory->data + vm->memory->size)
		return MLN_VM_ERROR_STACK_UNDERFLOW;
	*value = MLN_VM_WORD(sp);
	vm->sp = sp + 4;
	return 0;
}

/* the result code a routine sees for a VM result; 0x23090004 is given back */
unsigned int MlnImpl_8106A15C(unsigned int result, int *code)
{
	switch (result) {
	case 0:
	case 1:
		return 0;
	case 2:
		*code = MLN_VM_RESULT_OUT_OF_MEMORY;
		return 0;
	case 3:
	case 4:
	case 5:
		*code = MLN_VM_RESULT_INVALID_PARAMETERS;
		return 0;
	case MLN_VM_HALT:
		*code = 0;
		return 0;
	case MLN_VM_ERROR_MODULE:
		*code = MLN_VM_RESULT_INVALID_MODULE;
		return 0;
	case MLN_VM_ERROR_NOT_FOUND:
		*code = 0;
		return result;
	case MLN_VM_ERROR_STACK_UNDERFLOW:
	case MLN_VM_ERROR_STACK_OVERFLOW:
	case MLN_VM_ERROR_CALL_DEPTH:
	case MLN_VM_ERROR_DIVIDE:
	case MLN_VM_ERROR_OPCODE:
	case MLN_VM_ERROR_RANGE_LAST:
	case MLN_VM_ERROR_ACCESS:
		*code = MLN_VM_RESULT_FAULT;
		return 0;
	case MLN_VM_ERROR_SYSTEM_CALL:
		*code = MLN_VM_RESULT_NO_SUCH_ITEM;
		return 0;
	default:
		*code = MLN_VM_RESULT_FAILURE;
		return 0;
	}
}

/* free an export's name */
static inline void MlnVmExportFree(MlnVmExport *export)
{
	if (export != NULL && export->name != NULL) {
		MlnFree(MLN_HEAP_GENERAL, export->name);
		export->name = NULL;
	}
}

/* free a list of code modules */
unsigned int MlnImpl_8106A264(MlnVmModule **list)
{
	MlnVmModule *module = *list;
	int i;

	if (module == NULL)
		return 0;
	if (module->next != NULL)
		MlnImpl_8106A264(&module->next);
	if (module->exports != NULL) {
		for (i = 0; i < module->exportCount; i++)
			MlnVmExportFree(&module->exports[i]);
		MlnFree(MLN_HEAP_GENERAL, module->exports);
		module->exports = NULL;
	}
	module->signer = NULL;
	module->attributes = NULL;
	MlnFree(MLN_HEAP_GENERAL, *list);
	*list = NULL;
	return 0;
}

/*
 * Release a VM: the VMs it spawned first, then its module's Global.OnUnload, which
 * must leave one word, 0, on the stack.
 */
__attribute__((noinline)) unsigned int MlnImpl_8106A590(MlnVm *vm)
{
	unsigned char *sp;
	unsigned int value;
	unsigned int ret;
	unsigned int r;
	int i;

	if (vm == NULL)
		return 0;
	for (i = vm->index + 1; vm->children != 0; i++) {
		if (vm->children & (1 << i)) {
			if (vm->group->vms[i] != NULL)
				MlnImpl_8106A590(vm->group->vms[i]);
			vm->children ^= 1 << i;
		}
	}

	sp = vm->sp;
	r = MlnImpl_8106A780(vm, "Global.OnUnload", NULL);
	if (r != 0) {
		ret = r == MLN_VM_ERROR_NOT_FOUND ? 0 : MLN_VM_ERROR_UNLOAD;
	} else if (sp - vm->sp != 4) {
		ret = MLN_VM_ERROR_UNLOAD;
		vm->sp = sp;
	} else if (MlnVmPop(vm, &value) != 0 || value != 0) {
		ret = MLN_VM_ERROR_UNLOAD;
	} else {
		ret = 0;
	}
	MlnFree(MLN_HEAP_GENERAL, vm->memory);
	MlnImpl_8106A264(&vm->modules);
	MlnFree(MLN_HEAP_GENERAL, vm);
	return ret;
}

/*
 * Run a routine a module of the VM exports, on the VM's stack. *status gets the
 * routine's result code; a VM error resets the stack to the words 0 and that code.
 */
unsigned int MlnImpl_8106A780(MlnVm *vm, const char *routine, int *status)
{
	MlnVmContext context;
	int a;
	int b;
	int value;
	int flag;
	const unsigned char *p;
	unsigned char *q;
	const char *name;
	MlnCoreHost *host;
	unsigned int result;
	unsigned int ret;
	int code;
	int save;
	int depth;
	int op;
	int i;

	if (vm == NULL) {
		result = 1;
		goto end;
	}
	MlnMemset(&context, 0, sizeof(context));
	context.vm = vm;

	/* the routine's module and entry */
	for (context.module = vm->modules; context.module != NULL;
	     context.module = context.module->next) {
		for (i = 0; i < context.module->exportCount; i++) {
			context.routine = &context.module->exports[i];
			if (MlnStrcmp(context.routine->name, routine) == 0)
				break;
		}
		if (i < context.module->exportCount)
			break;
	}
	if (context.module == NULL) {
		result = MLN_VM_ERROR_NOT_FOUND;
		goto end;
	}

	context.ip = context.module->code + context.routine->offset;
	context.memory = vm->memory;
	context.data = context.memory->data;
	context.sp = vm->sp;
	context.codeStart = context.module->code;
	context.codeEnd = context.module->code + context.module->codeSize;
	context.dataEnd = context.memory->data + context.memory->size;
	context.stackLow = (unsigned char *)MLN_VM_ALIGN((unsigned int)context.memory->data +
							 context.memory->dataSize);
	context.calls = 0;
	context.stackHigh = (unsigned char *)((unsigned int)(context.memory->data +
							      context.memory->size) & ~3);
	context.vm->sp = context.sp;

	/* the VM's own words: the module's data segment and the data's end */
	if (context.dataEnd < context.data + 4)
		goto access;
	MlnVmSetWord(context.data, 0);
	if (context.dataEnd < context.data + 8)
		goto access;
	MlnVmSetWord(context.data + 4, context.module->dataOffset);
	if (context.dataEnd < context.data + 12)
		goto access;
	MlnVmSetWord(context.data + 8, 0);
	if (context.dataEnd < context.data + 16)
		goto access;
	MlnVmSetWord(context.data + 12, MLN_VM_ALIGN(context.memory->dataSize));

	for (;;) {
		p = context.ip;
		if (p < context.codeStart || p >= context.codeEnd)
			goto access;
		op = *p++;
		context.ip = p;
		switch (op) {
		case MLN_VM_OP_NOP:
			break;
		case MLN_VM_OP_PUSH:
			if (p < context.codeStart || p + 4 > context.codeEnd)
				goto access;
			a = MLN_VM_WORD(p);
			context.ip = p + 4;
			result = MlnVmPush(&context, a);
			if (result != 0)
				goto end;
			break;
		case MLN_VM_OP_DROP:
			result = MlnImpl_810830A4(&context, &a);
			if (result != 0)
				goto end;
			break;
		case MLN_VM_OP_DUP:
			if (context.sp < context.stackLow) {
				result = MLN_VM_ERROR_STACK_OVERFLOW;
				goto end;
			}
			if (context.sp >= context.stackHigh) {
				result = MLN_VM_ERROR_STACK_UNDERFLOW;
				goto end;
			}
			a = MLN_VM_WORD(context.sp);
			result = MlnVmPush(&context, a);
			if (result != 0)
				goto end;
			break;
		case MLN_VM_OP_SWAP:
			result = MlnImpl_81083118(&context, &b, &a);
			if (result != 0)
				goto end;
			result = MlnImpl_81083010(&context, b, a);
			if (result != 0)
				goto end;
			break;
		case MLN_VM_OP_ADD:
			result = MlnImpl_810831C0(&context, &b, &a);
			if (result != 0)
				goto end;
			MlnVmSetWord(context.sp, a + b);
			break;
		case MLN_VM_OP_MUL:
			result = MlnImpl_810831C0(&context, &b, &a);
			if (result != 0)
				goto end;
			MlnVmSetWord(context.sp, a * b);
			break;
		case MLN_VM_OP_SUB:
			result = MlnImpl_810831C0(&context, &b, &a);
			if (result != 0)
				goto end;
			MlnVmSetWord(context.sp, a - b);
			break;
		case MLN_VM_OP_DIV:
			result = MlnImpl_81083118(&context, &b, &a);
			if (result != 0)
				goto end;
			if (b == 0) {
				result = MLN_VM_ERROR_DIVIDE;
				goto end;
			}
			result = MlnVmPush(&context, a / b);
			if (result != 0)
				goto end;
			break;
		case MLN_VM_OP_MOD:
			result = MlnImpl_81083118(&context, &b, &a);
			if (result != 0)
				goto end;
			if (b == 0) {
				result = MLN_VM_ERROR_DIVIDE;
				goto end;
			}
			result = MlnVmPush(&context, a % b);
			if (result != 0)
				goto end;
			break;
		case MLN_VM_OP_NEG:
			if (context.sp < context.stackLow) {
				result = MLN_VM_ERROR_STACK_OVERFLOW;
				goto end;
			}
			if (context.sp + 4 > context.stackHigh) {
				result = MLN_VM_ERROR_STACK_UNDERFLOW;
				goto end;
			}
			a = MLN_VM_WORD(context.sp);
			MlnVmSetWord(context.sp, -a);
			break;
		case MLN_VM_OP_CMP:
			result = MlnImpl_810831C0(&context, &b, &a);
			if (result != 0)
				goto end;
			if (a > b)
				MlnVmSetWord(context.sp, 1);
			else if (a == b)
				MlnVmSetWord(context.sp, 0);
			else
				MlnVmSetWord(context.sp, -1);
			break;
		case MLN_VM_OP_AND:
			result = MlnImpl_810831C0(&context, &b, &a);
			if (result != 0)
				goto end;
			MlnVmSetWord(context.sp, a & b);
			break;
		case MLN_VM_OP_OR:
			result = MlnImpl_810831C0(&context, &b, &a);
			if (result != 0)
				goto end;
			MlnVmSetWord(context.sp, a | b);
			break;
		case MLN_VM_OP_XOR:
			result = MlnImpl_810831C0(&context, &b, &a);
			if (result != 0)
				goto end;
			MlnVmSetWord(context.sp, a ^ b);
			break;
		case MLN_VM_OP_NOT:
			if (context.sp < context.stackLow) {
				result = MLN_VM_ERROR_STACK_OVERFLOW;
				goto end;
			}
			if (context.sp + 4 > context.stackHigh) {
				result = MLN_VM_ERROR_STACK_UNDERFLOW;
				goto end;
			}
			a = MLN_VM_WORD(context.sp);
			MlnVmSetWord(context.sp, a == 0);
			break;
		case MLN_VM_OP_SHL:
			result = MlnImpl_810831C0(&context, &b, &a);
			if (result != 0)
				goto end;
			MlnVmSetWord(context.sp, a << b);
			break;
		case MLN_VM_OP_SHR:
			result = MlnImpl_810831C0(&context, &b, &a);
			if (result != 0)
				goto end;
			MlnVmSetWord(context.sp, (unsigned int)a >> b);
			break;
		case MLN_VM_OP_JMP:
			result = MlnImpl_810830A4(&context, &a);
			if (result != 0)
				goto end;
			context.ip = context.module->code + a;
			break;
		case MLN_VM_OP_JSR:
			if (context.calls >= MLN_VM_CALL_DEPTH) {
				result = MLN_VM_ERROR_CALL_DEPTH;
				goto end;
			}
			context.returns[context.calls] = p;
			context.calls++;
			result = MlnImpl_810830A4(&context, &a);
			if (result != 0)
				goto end;
			context.ip = context.module->code + a;
			break;
		case MLN_VM_OP_JSRR:
			if (context.calls >= MLN_VM_CALL_DEPTH) {
				result = MLN_VM_ERROR_CALL_DEPTH;
				goto end;
			}
			context.returns[context.calls] = p;
			context.calls++;
			result = MlnImpl_810830A4(&context, &a);
			if (result != 0)
				goto end;
			context.ip += a;
			break;
		case MLN_VM_OP_RET:
			if (context.calls <= 0) {
				result = MLN_VM_HALT;
				goto end;
			}
			context.calls--;
			context.ip = context.returns[context.calls];
			break;
		case MLN_VM_OP_BRA:
			result = MlnImpl_810830A4(&context, &a);
			if (result != 0)
				goto end;
			context.ip += a;
			break;
		case MLN_VM_OP_BRP:
		case MLN_VM_OP_BRN:
		case MLN_VM_OP_BRZ:
			result = MlnImpl_81083118(&context, &b, &a);
			if (result != 0)
				goto end;
			if ((op == MLN_VM_OP_BRP && a > 0) || (op == MLN_VM_OP_BRN && a < 0) ||
			    (op == MLN_VM_OP_BRZ && a == 0))
				context.ip += b;
			break;
		case MLN_VM_OP_PEEK:
			result = MlnImpl_810830A4(&context, &a);
			if (result != 0)
				goto end;
			q = (unsigned char *)((unsigned int)context.data + a);
			if ((unsigned int)q < (unsigned int)context.data || q + 4 > context.dataEnd)
				goto access;
			b = MLN_VM_WORD(q);
			result = MlnVmPush(&context, b);
			if (result != 0)
				goto end;
			break;
		case MLN_VM_OP_POKE:
			result = MlnImpl_81083118(&context, &b, &a);
			if (result != 0)
				goto end;
			q = (unsigned char *)((unsigned int)context.data + b);
			if ((unsigned int)q < (unsigned int)context.data || q + 4 > context.dataEnd)
				goto access;
			MlnVmSetWord(q, a);
			break;
		case MLN_VM_OP_PEEKB:
			result = MlnImpl_810830A4(&context, &a);
			if (result != 0)
				goto end;
			q = (unsigned char *)((unsigned int)context.data + a);
			if ((unsigned int)q < (unsigned int)context.data || q >= context.dataEnd)
				goto access;
			b = *q;
			result = MlnVmPush(&context, b);
			if (result != 0)
				goto end;
			break;
		case MLN_VM_OP_POKEB:
			result = MlnImpl_81083118(&context, &b, &a);
			if (result != 0)
				goto end;
			q = (unsigned char *)((unsigned int)context.data + b);
			if ((unsigned int)q < (unsigned int)context.data || q >= context.dataEnd)
				goto access;
			*q = a;
			break;
		case MLN_VM_OP_PUSHSP:
			a = context.sp - context.data;
			result = MlnVmPush(&context, a);
			if (result != 0)
				goto end;
			break;
		case MLN_VM_OP_POPSP:
			result = MlnImpl_810830A4(&context, &a);
			if (result != 0)
				goto end;
			context.sp = context.data + a;
			if (context.sp > context.stackHigh) {
				result = MLN_VM_ERROR_STACK_UNDERFLOW;
				goto end;
			}
			if (context.sp < context.stackLow) {
				result = MLN_VM_ERROR_STACK_OVERFLOW;
				goto end;
			}
			break;
		case MLN_VM_OP_CALL:
			result = MlnImpl_810830A4(&context, &a);
			if (result != 0)
				goto end;
			host = context.vm->host;
			switch (a) {
			case MLN_VM_CALL_DEBUG_PRINT:
			case MLN_VM_CALL_DEBUG:
				MlnImpl_810830A4(&context, &value);
				break;
			case MLN_VM_CALL_FIND_SYSTEM_CALL_BY_NAME:
				if (MlnImpl_810830A4(&context, &value) != 0)
					break;
				name = (const char *)((unsigned int)context.data + value);
				if ((unsigned int)name < (unsigned int)context.data ||
				    (const unsigned char *)name >= context.dataEnd)
					break;
				if (MlnStrcmp(name, "System.NoOperation") == 0)
					value = MLN_VM_CALL_NO_OPERATION;
				else if (MlnStrcmp(name, "System.DebugPrint") == 0)
					value = MLN_VM_CALL_DEBUG_PRINT;
				else if (MlnStrcmp(name, "System.FindSystemCallByName") == 0)
					value = MLN_VM_CALL_FIND_SYSTEM_CALL_BY_NAME;
				else if (MlnStrcmp(name, "System.Host.GetObject") == 0)
					value = MLN_VM_CALL_HOST_GET_OBJECT;
				else if (MlnStrcmp(name, "System.Host.SetObject") == 0)
					value = MLN_VM_CALL_HOST_SET_OBJECT;
				else if (MlnStrcmp(name, "System.Host.GetTrustedTime") == 0)
					value = MLN_VM_CALL_HOST_GET_TRUSTED_TIME;
				else if (MlnStrcmp(name, "System.Host.GetLocalTime") == 0)
					value = MLN_VM_CALL_HOST_GET_LOCAL_TIME;
				else if (MlnStrcmp(name, "System.Host.GetLocalTimeOffset") == 0)
					value = MLN_VM_CALL_HOST_GET_LOCAL_TIME_OFFSET;
				else if (MlnStrcmp(name, "System.Host.GetLocalTrustedTime") == 0)
					value = MLN_VM_CALL_HOST_GET_LOCAL_TRUSTED_TIME;
				else if (MlnStrcmp(name, "System.Host.SpawnVm") == 0)
					value = MLN_VM_CALL_HOST_SPAWN_VM;
				else if (MlnStrcmp(name, "System.Host.CallVm") == 0)
					value = MLN_VM_CALL_HOST_CALL_VM;
				else if (MlnStrcmp(name, "System.Host.ReleaseVm") == 0)
					value = MLN_VM_CALL_HOST_RELEASE_VM;
				else if (MlnStrcmp(name, "Octopus.Links.IsNodeReachable") == 0)
					value = MLN_VM_CALL_IS_NODE_REACHABLE;
				else
					value = MLN_VM_RESULT_NO_SUCH_ITEM;
				MlnVmPush(&context, value);
				break;
			case MLN_VM_CALL_HOST_GET_OBJECT:
				MlnImpl_8108325C(&context);
				break;
			case MLN_VM_CALL_HOST_SET_OBJECT:
				MlnImpl_81083408(&context);
				break;
			case MLN_VM_CALL_HOST_GET_TRUSTED_TIME:
				((MlnVmHostTimeFunction)host->functions[MLN_VM_HOST_TRUSTED_TIME])(
					host, (unsigned int *)&value, &flag);
				MlnImpl_81083010(&context, flag, value);
				break;
			case MLN_VM_CALL_HOST_GET_LOCAL_TIME:
				((MlnVmHostValueFunction)host->functions[MLN_VM_HOST_LOCAL_TIME])(host,
												  &value);
				MlnVmPush(&context, value);
				break;
			case MLN_VM_CALL_HOST_GET_LOCAL_TIME_OFFSET:
				((MlnVmHostValueFunction)host->functions[MLN_VM_HOST_LOCAL_TIME_OFFSET])(
					host, &value);
				MlnVmPush(&context, value);
				break;
			case MLN_VM_CALL_HOST_GET_LOCAL_TRUSTED_TIME:
				((MlnVmHostTimeFunction)host->functions[MLN_VM_HOST_LOCAL_TRUSTED_TIME])(
					host, (unsigned int *)&value, &flag);
				MlnImpl_81083010(&context, flag, value);
				break;
			case MLN_VM_CALL_HOST_SPAWN_VM:
				MlnImpl_810837CC(&context);
				break;
			case MLN_VM_CALL_HOST_CALL_VM:
				MlnImpl_81083A58(&context);
				break;
			case MLN_VM_CALL_HOST_RELEASE_VM:
				MlnImpl_810836DC(&context);
				break;
			case MLN_VM_CALL_IS_NODE_REACHABLE:
				MlnImpl_81083638(&context);
				break;
			default:
				break;
			}
			break;
		case MLN_VM_OP_STOP:
			result = MLN_VM_HALT;
			goto end;
		default:
			result = MLN_VM_ERROR_OPCODE;
			goto end;
		}

		depth = (context.stackHigh - context.sp) >> 2;
		if (depth > vm->maxStack)
			vm->maxStack = depth;
		if (vm->maxCalls < context.calls)
			vm->maxCalls = context.calls;
	}

access:
	result = MLN_VM_ERROR_ACCESS;
end:
	code = 0;
	for (;;) {
		ret = MlnImpl_8106A15C(result, &code);
		if (status != NULL)
			*status = code;
		if (ret != 0) {
			save = 0;
			break;
		}
		save = 1;
		if (code == 0)
			break;
		context.sp = context.stackHigh;
		result = MlnVmPush(&context, 0);
		if (result != 0)
			continue;
		result = MlnVmPush(&context, code);
		if (result == 0)
			break;
	}
	if (vm != NULL && save)
		vm->sp = context.sp;
	return ret;
}

/* free a module image's exports */
static inline void MlnVmImageFree(MlnVmImage *image)
{
	int count;
	int i;

	if (image->exports != NULL) {
		count = image->exportCount;
		for (i = 0; i < count; i++) {
			if (image->exports[i].name != NULL)
				MlnFree(MLN_HEAP_GENERAL, image->exports[i].name);
		}
		MlnFree(MLN_HEAP_GENERAL, image->exports);
	}
}

/*
 * Load the code module registered under uid: a pkCM container of a data segment
 * (pkDS), the code (pkCS), the exports (pkEX) and optional requirements (pkRQ). Its
 * Global.OnLoad must leave one word, 0, on the stack.
 */
unsigned int MlnImpl_8106BDA0(MlnVm *vm, const MlnBuffer *uid)
{
	MlnVmGroup *group = vm->group;
	MlnVmImage image;
	const MlnBuffer *codeModule;
	const MlnBuffer *signer;
	MlnOctopusArray *attributes;
	const unsigned char *signerData;
	unsigned int signerSize;
	const unsigned char *atom;
	const unsigned char *next;
	const unsigned char *entry;
	MlnVmModule *module;
	unsigned int total;
	unsigned int remaining;
	unsigned int atomSize;
	unsigned int left;
	unsigned int length;
	unsigned char *sp;
	unsigned int value;
	unsigned int ret;
	unsigned int r;
	int count;
	int i;

	MlnMemset(&image, 0, sizeof(image));
	image.dataSize = -1;
	image.codeSize = -1;
	image.exportCount = -1;

	for (i = 0; i < group->moduleCount; i++) {
		if (MlnBufferCompare(MLN_HEAP_GENERAL, uid, group->uids[i]) == 0)
			break;
	}
	if (i >= group->moduleCount)
		goto format;
	codeModule = group->codeModules[i];
	signer = group->signers[i];
	attributes = group->attributes[i];
	if (signer != NULL) {
		signerSize = signer->size;
		signerData = signer->data;
	} else {
		signerSize = 0;
		signerData = NULL;
	}

	total = MLN_VM_WORD(codeModule->data);
	if (total > codeModule->size || total < 8 || codeModule->data[4] != 'p' ||
	    codeModule->data[5] != 'k' || codeModule->data[6] != 'C' || codeModule->data[7] != 'M')
		goto format;

	atom = codeModule->data + 8;
	for (remaining = total - 8; remaining >= 8; atom = next) {
		atomSize = MLN_VM_WORD(atom);
		if (atomSize > remaining || atomSize < 8)
			goto format;
		remaining -= atomSize;
		next = atom + atomSize;
		if (MlnMemcmp(atom + 4, "pkDS", 4) == 0 && image.dataSize == -1) {
			if (atomSize == 8 || atom[8] != 0)
				goto format;
			if (atomSize - 8 > vm->memory->size - vm->memory->dataSize) {
				ret = MLN_VM_ERROR_DATA_SIZE;
				goto out;
			}
			image.dataSize = atomSize - 9;
			image.data = atom + 9;
		} else if (MlnMemcmp(atom + 4, "pkCS", 4) == 0 && image.codeSize == -1) {
			if (atomSize <= 9 || atom[8] != 0 || atom[9] != 0)
				goto format;
			image.codeSize = atomSize - 10;
			image.code = atom + 10;
		} else if (MlnMemcmp(atom + 4, "pkEX", 4) == 0 && image.exportCount == -1) {
			count = MLN_VM_WORD(atom + 8);
			image.exports = MlnAllocate(MLN_HEAP_GENERAL, count * sizeof(MlnVmExport));
			if (image.exports == NULL)
				return 2;
			MlnMemset(image.exports, 0, count * sizeof(MlnVmExport));
			image.exportCount = count;
			entry = atom + 12;
			left = atomSize - 12;
			for (i = 0; i < count; i++) {
				length = entry[0];
				if (left < length + 5 || entry[length] != 0)
					goto format;
				image.exports[i].name = MlnAllocate(MLN_HEAP_GENERAL, length);
				if (image.exports[i].name == NULL) {
					ret = 2;
					goto out;
				}
				image.exports[i].nameSize = length;
				MlnMemcpy(image.exports[i].name, entry + 1, length);
				left -= length + 5;
				image.exports[i].offset = MLN_VM_WORD(entry + 1 + length);
				entry += length + 5;
			}
			/* the next atom follows the last export, not the atom's size */
			next = entry;
		} else if (MlnMemcmp(atom + 4, "pkRQ", 4) == 0 && !image.hasRequirements) {
			if ((int)(atomSize - 8) < 16)
				goto format;
			image.hasRequirements = 1;
			image.requirements[0] = MLN_VM_WORD(atom + 8);
			image.requirements[1] = MLN_VM_WORD(atom + 12);
			image.requirements[2] = MLN_VM_WORD(atom + 16);
			image.requirements[3] = MLN_VM_WORD(atom + 20);
			if (image.requirements[0] != 0 || image.requirements[3] != 0)
				goto format;
		} else if (MlnMemcmp(atom + 4, "pkDS", 4) == 0 || MlnMemcmp(atom + 4, "pkCS", 4) == 0 ||
			   MlnMemcmp(atom + 4, "pkEX", 4) == 0 || MlnMemcmp(atom + 4, "pkRQ", 4) == 0) {
			goto format;
		}
	}

	if (image.dataSize < 0 || image.codeSize < 0 || image.exportCount < 0)
		goto format;
	if (image.hasRequirements == 1 &&
	    (vm->memory->size - vm->memory->dataSize < image.requirements[1] ||
	     image.requirements[2] > MLN_VM_STACK_MAX))
		goto format;
	if (vm->modules != NULL) {
		ret = MLN_VM_ERROR_LOADED;
		goto out;
	}

	module = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*module) + image.codeSize);
	if (module == NULL) {
		ret = 2;
		goto out;
	}
	module->codeSize = image.codeSize;
	module->exportCount = 0;
	module->exports = NULL;
	module->next = NULL;
	MlnMemcpy(module->code, image.code, image.codeSize);
	vm->modules = module;
	module->dataOffset = vm->memory->dataSize;
	MlnMemcpy(vm->memory->data + module->dataOffset, image.data, image.dataSize);
	vm->memory->dataSize += image.dataSize;
	module->attributes = attributes;
	module->exportCount = image.exportCount;
	module->exports = image.exports;
	image.exports = NULL;
	module->signerSize = signerSize;
	module->signer = signerData;

	sp = vm->sp;
	r = MlnImpl_8106A780(vm, "Global.OnLoad", NULL);
	if (r != 0) {
		ret = r == MLN_VM_ERROR_NOT_FOUND ? 0 : MLN_VM_ERROR_MODULE;
	} else if (sp - vm->sp != 4) {
		ret = MLN_VM_ERROR_MODULE;
		vm->sp = sp;
	} else if (MlnVmPop(vm, &value) != 0 || value != 0) {
		ret = MLN_VM_ERROR_MODULE;
	} else {
		ret = 0;
	}
	goto out;
format:
	ret = MLN_VM_ERROR_MODULE;
out:
	MlnVmImageFree(&image);
	return ret;
}

/* a new, empty group of VMs */
static inline unsigned int MlnVmGroupCreate(MlnVmGroup **out)
{
	MlnVmGroup *group;
	unsigned int ret;

	group = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*group));
	if (group == NULL) {
		ret = 2;
	} else {
		MlnMemset(group, 0, sizeof(*group));
		*out = group;
		group = NULL;
		ret = 0;
	}
	MlnFree(MLN_HEAP_GENERAL, group);
	return ret;
}

/* a new VM of size bytes of memory */
static inline unsigned int MlnVmCreate(int size, MlnCoreHost *host, MlnVm **out)
{
	MlnVmMemory *memory = NULL;
	MlnVm *vm = NULL;
	unsigned int ret;

	if (size < MLN_VM_MEMORY_MIN) {
		ret = MLN_VM_ERROR_MEMORY;
		goto out;
	}
	memory = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*memory) + size);
	if (memory == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(memory, 0, sizeof(*memory) + size);
	memory->size = size;
	memory->dataSize = MLN_VM_DATA_START;
	vm = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*vm));
	if (vm == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(vm, 0, sizeof(*vm));
	vm->memory = memory;
	vm->sp = (unsigned char *)((unsigned int)(memory->data + memory->size) & ~3);
	vm->host = host;
	*out = vm;
	memory = NULL;
	vm = NULL;
	ret = 0;
out:
	MlnFree(MLN_HEAP_GENERAL, memory);
	MlnFree(MLN_HEAP_GENERAL, vm);
	return ret;
}

/* make a VM of size bytes of memory, in a group of its own */
unsigned int MlnImpl_8106C54C(int size, MlnCoreHost *host, MlnVm **out)
{
	MlnVmGroup *group;
	MlnVm *vm;
	unsigned int ret;
	int count;

	ret = MlnVmGroupCreate(&group);
	if (ret != 0)
		return ret;
	ret = MlnVmCreate(size, host, &vm);
	if (ret != 0) {
		MlnFree(MLN_HEAP_GENERAL, group);
		return ret;
	}
	vm->group = group;
	count = group->vmCount;
	vm->index = count;
	group->vmCount = count + 1;
	group->vms[vm->index] = vm;
	*out = vm;
	return 0;
}

/* walk the VM's modules (what this did is compiled out) */
unsigned int MlnImpl_8106C694(MlnVm *vm)
{
	MlnVmModule *module;

	if (vm->modules != NULL) {
		for (module = vm->modules; module != NULL; module = module->next)
			;
		for (module = vm->modules; module != NULL; module = module->next)
			;
	}
	return 0;
}

/* release a VM and its group */
unsigned int MlnImpl_8106C6C4(MlnVm *vm)
{
	MlnVmGroup *group;
	unsigned int ret = 0;

	if (vm != NULL) {
		group = vm->group;
		ret = MlnImpl_8106A590(vm);
		if (group != NULL)
			MlnFree(MLN_HEAP_GENERAL, group);
	}
	return ret;
}

/* register a code module with the VM's group, to load by its uid */
unsigned int MlnImpl_8106C6F8(MlnVm *vm, MlnBuffer *codeModule, MlnBuffer *uid, MlnBuffer *protocol,
			      MlnBuffer *type, MlnBuffer *signer, MlnOctopusArray *attributes)
{
	MlnVmGroup *group = vm->group;
	int n;

	if (MlnBufferCompare(MLN_HEAP_GENERAL, protocol, &MlnVmProtocol) != 0)
		return MLN_VM_ERROR_MODULE;
	if (MlnBufferCompare(MLN_HEAP_GENERAL, type, &MlnVmType) != 0)
		return MLN_VM_ERROR_MODULE;
	n = group->moduleCount;
	if (n >= MLN_VM_MODULES)
		return MLN_VM_ERROR_MODULES;
	group->codeModules[n] = codeModule;
	group->uids[n] = uid;
	group->signers[n] = signer;
	group->attributes[n] = attributes;
	group->moduleCount = n + 1;
	return 0;
}

/* load a registered code module */
unsigned int MlnImpl_8106C788(MlnVm *vm, MlnBuffer *uid)
{
	return MlnImpl_8106BDA0(vm, uid);
}

/* run a routine */
unsigned int MlnImpl_8106C794(MlnVm *vm, const char *routine)
{
	return MlnImpl_8106A780(vm, routine, NULL);
}

/* read size bytes of the VM's memory at address */
unsigned int MlnImpl_8106C7A4(MlnVm *vm, int address, int size, unsigned char *out)
{
	MlnVmMemory *memory;

	if (vm == NULL || address < 0)
		return 3;
	memory = vm->memory;
	if (memory->size <= (unsigned int)address || size < 0 ||
	    memory->size < (unsigned int)(size + address))
		return 3;
	if (out == NULL && size > 0)
		return 3;
	if (out != NULL)
		MlnMemcpy(out, memory->data + address, size);
	return 0;
}

/* the words free between the data segments and the stack's top */
unsigned int MlnImpl_8106C828(MlnVm *vm, unsigned int *words)
{
	MlnVmMemory *memory;

	if (vm == NULL || words == NULL)
		return 3;
	memory = vm->memory;
	if (memory == NULL)
		return 3;
	*words = (memory->size - memory->dataSize) >> 2;
	return 0;
}

/* push a word onto the VM's stack */
unsigned int MlnImpl_8106C86C(MlnVm *vm, unsigned int value)
{
	unsigned char *sp = vm->sp;

	vm->sp = sp - 4;
	if (sp - 4 < vm->memory->data + vm->memory->dataSize) {
		vm->sp = sp;
		return MLN_VM_ERROR_STACK_OVERFLOW;
	}
	MlnVmSetWord(sp - 4, value);
	return 0;
}

/* push size bytes onto the VM's stack, padded to words */
unsigned int MlnImpl_8106C8CC(MlnVm *vm, unsigned int size, const unsigned char *data)
{
	unsigned char *sp;
	unsigned int aligned;

	if (size != 0 && data == NULL)
		return 3;
	if (size == 0)
		return 0;
	sp = vm->sp;
	aligned = MLN_VM_ALIGN(size);
	vm->sp = sp - aligned;
	if (sp - aligned < vm->memory->data + vm->memory->dataSize) {
		vm->sp = sp;
		return MLN_VM_ERROR_STACK_OVERFLOW;
	}
	MlnMemcpy(sp - aligned, data, size);
	MlnMemset(vm->sp + size, 0, aligned - size);
	return 0;
}

/* pop a word from the VM's stack */
unsigned int MlnImpl_8106C970(MlnVm *vm, unsigned int *value)
{
	return MlnVmPop(vm, value);
}

/* empty the VM's stack */
unsigned int MlnImpl_8106C9D4(MlnVm *vm)
{
	vm->sp = vm->memory->data + vm->memory->size;
	return 0;
}

/*
 * A copy of the extended status block a routine left at address in the VM's memory:
 * its header, then its parameters, each a word, a size and that many bytes. *type is
 * whether its category is 1.
 */
unsigned int MlnImpl_8106C9F4(MlnVm *vm, unsigned int address, MlnBuffer **out, unsigned int *type)
{
	const unsigned char *block = vm->memory->data + address;
	const unsigned char *end = vm->memory->data + vm->memory->size;
	const unsigned char *p = block + 28;
	MlnBuffer *copy = NULL;
	unsigned int category;
	unsigned int ret;
	int count;
	int i;

	if (end <= p) {
		ret = 5;
		goto out;
	}
	category = MLN_VM_WORD(block + 4);
	count = MLN_VM_WORD(block + 24);
	for (i = 0; i < count; i++) {
		if (end <= p + 8) {
			ret = 5;
			goto out;
		}
		p += 8 + MLN_VM_WORD(p + 4);
		if (end <= p) {
			ret = 5;
			goto out;
		}
	}
	copy = MlnBufferCreate(MLN_HEAP_GENERAL, p - block, block);
	if (copy == NULL) {
		ret = 2;
	} else {
		*out = copy;
		copy = NULL;
		ret = 0;
		*type = category == 1;
	}
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, copy);
	return ret;
}
