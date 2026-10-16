/*
 * fw2 runtime: the cores' main loop. The mailbox interrupt takes a command into
 * the core's work slot; the main loop runs it on its context with the program
 * the context names, answers, and sleeps when there is nothing to do.
 */
#include "runtime.h"
#include "../../fw1/common/dma.h"

/* the program records of each core, and the hardware reset vipHalt calls */
static const VipProgram *const g_programs[2] = {
	VIP_PROGRAMS(0),
	VIP_PROGRAMS(1),
};
#define VIP_RESET_HOOK	(*(void (**)(void))VIP_RESET_HOOKS)

VipExceptionRecord g_exceptionRecords[2];
VipWork g_work[2];

static VipWork *currentWork(void)
{
	return &g_work[vipCoreNumber()];
}

/*
 * Write the answer for the ARM side: words[0..last] (at most 10) into response[]
 * with the status, then signal it. If the ARM side has not taken the last
 * answer, signal only whether words[3], the result, is negative.
 */
static void respond(const uint32_t *words, uint32_t last, uint32_t status)
{
	volatile uint32_t *answer = vipMailboxAnswer();
	VipCommandBlock *block;
	uint32_t i;

	if (*answer & VIP_MAILBOX_UNREAD) {
		VIP_COMMAND_BLOCK->status = status;
		*answer = (int32_t)words[3] < 0 ? VIP_MAILBOX_FAILED : VIP_MAILBOX_SUCCEEDED;
		return;
	}
	if (last >= 10)
		last = 9;
	block = VIP_COMMAND_BLOCK;
	for (i = 0; i <= last; i++)
		block->response[i] = words[i];
	block->status = status;
	*answer = VIP_MAILBOX_RESPONSE;
}

/* the time of a command, in the block's statistics */
static void recordCommandTime(VipCommandBlock *block, uint32_t time)
{
	uint32_t total = time + block->commandTotal;

	if (total < block->commandTotal)
		block->commandWraps++;
	block->commandCount++;
	if (block->commandMaximum < time)
		block->commandMaximum = time;
	block->commandTotal = total;
}

/* nothing is in local RAM for any context */
static void forgetContext(void)
{
	VipCommandBlock *block = VIP_COMMAND_BLOCK;

	block->state = -1;
	block->currentContext = 0;
	block->currentProgram = -1;
}

/*
 * Take a context for this core: VIP_RESULT_IN_USE if the other core runs it,
 * VIP_RESULT_LOCKED if the ARM side holds it, else 0, or 1 if the core did not
 * run it last.
 */
static int takeContext(VipContext *context, uint32_t core)
{
	volatile uint32_t *lock;
	int result = VIP_RESULT_IN_USE;
	uint32_t owner;

	if (context->owner >= VIP_CONTEXT_OWNER_LIMIT)
		return result;
	lock = context->lock;
	mep_di();
	if (*lock) {
		result = VIP_RESULT_LOCKED;
	} else {
		owner = context->owner;
		if (owner < VIP_CONTEXT_OWNER_LIMIT) {
			context->owner = core | VIP_CONTEXT_RUNNING;
			result = owner != core;
		}
		*lock = 0;
	}
	mep_ei();
	return result;
}

/* Save the context's local RAM data. */
static void saveContext(VipContext *context, uint32_t core)
{
	const VipProgram *program = &g_programs[core & 1][context->program];

	if (program->localDataSize)
		writeDram(context->localData, VIP_PROGRAM_DATA, program->localDataSize);
}

/* Load the program's code overlay. */
static void loadProgram(VipContext *context, uint32_t core)
{
	const VipProgram *program = &g_programs[core & 1][context->program];

	if (program->codeSize)
		readDram(program->code, VIP_PROGRAM_CODE, program->codeSize);
}

/* Load the context's local RAM data. */
static void loadContext(VipContext *context, uint32_t core)
{
	const VipProgram *program = &g_programs[core & 1][context->program];

	if (program->localDataSize)
		readDram(context->localData, VIP_PROGRAM_DATA, program->localDataSize);
}

/*
 * Run the program on the context, and give the context back to the ARM side;
 * its local RAM data is saved unless the program asks to go on.
 */
static int runProgram(VipContext *context, uint32_t core, uint32_t *output)
{
	int result = vipCallProgram(context->entry, output, VIP_CONTEXT_GP(context));

	if (result != VIP_PROGRAM_CONTINUE)
		saveContext(context, core);
	context->owner = core;
	return result;
}

/*
 * VIP_COMMAND_OPEN: set the context up for a program of this core, its globals
 * initialised and zeroed, its local RAM data initialised.
 */
static VipContext *openContext(VipContext *context, uint32_t number, volatile uint32_t *lock)
{
	const VipProgram *program = &g_programs[vipCore()][number];
	uint32_t size, *localData;

	context->lock = lock;
	context->owner = VIP_CONTEXT_NO_OWNER;
	context->program = (uint16_t)number;
	context->entry = program->entry;
	size = program->globalsSize;
	vipCopyDram((const uint32_t *)program->initialGlobals, context->globals, size);
	vipZeroDram((uint32_t)&context->globals[size / 4], program->zeroGlobalsSize);
	size += program->zeroGlobalsSize;
	localData = &context->globals[size / 4];
	context->localData = (uint32_t)localData;
	vipCopyDram((const uint32_t *)program->initialLocalData, localData,
		program->localDataSize);
	context->size = size + program->localDataSize;
	return context;
}

/*
 * Run a command; response gets its first three words, the result and what the
 * program gives back.
 */
static void runCommand(const uint32_t *command, uint32_t *response)
{
	VipCommandBlock *block;
	VipContext *context;
	uint32_t kind, core;
	int result;

	vipStartTimer();
	context = (VipContext *)command[1];
	kind = command[2];
	response[0] = command[0];
	response[1] = command[1];
	response[2] = kind;
	block = VIP_COMMAND_BLOCK;
	if (!context) {
		result = VIP_RESULT_NO_CONTEXT;
	} else if (kind == VIP_COMMAND_OPEN) {
		uint32_t number = command[3 + VIP_OPEN_PROGRAM];
		volatile uint32_t *lock = (volatile uint32_t *)command[3 + VIP_OPEN_LOCK];

		if (number < 2 && lock) {
			if (block->currentContext == (uint32_t)openContext(context, number, lock))
				forgetContext();
			result = VIP_RESULT_OK;
		} else {
			result = VIP_RESULT_BAD_OPEN;
		}
	} else {
		core = vipCoreNumber();
		result = takeContext(context, core);
		if (result >= 0) {
			if (result > 0 || block->currentContext != (uint32_t)context) {
				loadContext(context, core);
				block->currentContext = (uint32_t)context;
				vipSetState(-1);
			}
			response[4] = 0;
			if ((uint32_t)block->currentProgram != context->program) {
				loadProgram(context, core);
				block->currentProgram = context->program;
			}
			result = runProgram(context, core, &response[4]);
			if (result > 0)
				result = VIP_RESULT_OK;
		}
	}
	response[3] = result;
	vipStopTimer();
	recordCommandTime(block, vipReadTimer());
}

/*
 * The mailbox interrupt: a command, a stop or nothing. A command goes into the
 * work slot, or is refused if the slot is taken; VIP_COMMAND_EXIT and a stop end
 * the core.
 */
void vipReceiveCommand(void)
{
	volatile uint32_t *command = vipMailboxCommand();
	VipCommandBlock *block = VIP_COMMAND_BLOCK;
	uint32_t words[14], value, i;
	VipWork *work;

	value = *command;
	for (i = 0; i < 14; i++)
		words[i] = block->command[i];
	if (value & VIP_MAILBOX_STOP) {
		block->status = VIP_CORE_BUSY;
		*command = VIP_MAILBOX_IDLE;
		if (!currentWork()->pending)
			vipHalt(~0u, ~0u, 6);
		vipHalt(words[0], words[1], 6);
	}
	if (!(value & VIP_MAILBOX_COMMAND))
		return;
	if (words[2] == VIP_COMMAND_EXIT) {
		block->status = VIP_CORE_BUSY;
		*command = VIP_MAILBOX_IDLE;
		vipHalt(words[0], words[1], VIP_COMMAND_EXIT);
	}
	work = currentWork();
	if (!work->pending) {
		work->pending = 1;
		for (i = 0; i < 14; i++)
			work->command[i] = words[i];
		block->status = VIP_CORE_BUSY;
		*command = VIP_MAILBOX_ACCEPTED;
		(void)*command;
		return;
	}
	/* busy: refuse it */
	*command = VIP_MAILBOX_ACCEPTED;
	words[3] = -1;
	(void)*command;
	respond(words, 3, block->status);
}

/* Sleep, from local RAM, until an interrupt wakes the core. */
static void __attribute__((noinline)) sleepCore(void)
{
	vipLocalSleep(&VIP_COMMAND_BLOCK->status, VIP_CORE_SLEEPING, VIP_CORE_IDLE);
}

/* Sleep until the mailbox wakes the core, unless there is work. */
static void idle(VipWork *work)
{
	mep_di();
	if (!work->pending && !*vipMailboxCommand())
		sleepCore();
	mep_ei();
}

/* the reset state of the units each core drives */
static void initializeCore0Units(void)
{
	vipWriteControlBus(VIP_CB_SCANNER_CONTROL, 1);
	vipWriteControlBus(VIP_CB_STREAM_CONTROL, 2);
	vipWriteControlBus(VIP_CB_BITSTREAM_ENABLE, 1);
}

static void initializeCore1Units(void)
{
	vipWriteControlBus(VIP_CB_STREAM_CONTROL, 2);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(0), 1);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(1), 1);
	vipWriteControlBus(VIP_CB_PREDICTION_ENABLE, 1);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(2), 1);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(3), 1);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(4), 1);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(5), 1);
	vipWriteControlBus(VIP_CB_DIVIDER_ENABLE, 1);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(7), 1);
	vipWriteControlBus(VIP_CB_UNIT_ENABLE(6), 1);
}

static void initializeUnits(void)
{
	vipWriteControlBus(VIP_CB_TIMER_ENABLE, 1);
	if (!vipCore())
		initializeCore0Units();
	else
		initializeCore1Units();
}

/*
 * The vectors in local RAM: the sleep code over the reset and NMI vectors, the
 * other exception vectors from the boot block, the interrupt vectors.
 */
static void installVectors(void)
{
	uint32_t *vectors = (uint32_t *)VIP_LOCAL_VECTORS;
	const uint32_t *source;
	uint32_t i;

	for (i = 0; i < (uint32_t)vipSleepCodeSize / 4; i++)
		vectors[i] = ((const uint32_t *)vipSleepCode)[i];
	source = (const uint32_t *)8;
	for (i = 0; i < 4; i++)
		vectors[2 + i] = source[i];
	vectors = (uint32_t *)VIP_INTERRUPT_VECTORS;
	for (i = 0; i < (uint32_t)vipInterruptVectorsSize / 4; i++)
		vectors[i] = vipInterruptVectors[i];
}

/*
 * The interrupt controller: nothing pending, every interrupt at level 3, the
 * DMA-complete, mailbox and timer interrupts on, and entered in local RAM.
 */
static void configureInterrupts(void)
{
	mep_di();
	vipWriteControlBus(VIP_CB_INTERRUPT_STATUS, 0);
	vipWriteControlBus(VIP_CB_INTERRUPT_CLEAR, 0);
	vipWriteControlBus(VIP_CB_INTERRUPT_PENDING, ~(1u << VIP_INTERRUPT_MAILBOX));
	vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL0, 0x33333333);
	vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL1, 0x33333333);
	vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL2, 0x33333333);
	vipWriteControlBus(VIP_CB_INTERRUPT_LEVEL3, 0x33333333);
	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE, 1u << VIP_INTERRUPT_DMA |
		1u << VIP_INTERRUPT_MAILBOX | 1u << VIP_INTERRUPT_TIMER);
	vipWriteControl(VIP_CR_CFG, vipReadControl(VIP_CR_CFG) | VIP_CFG_LOCAL_VECTORS);
	vipWriteControl(VIP_CR_PSW, vipReadControl(VIP_CR_PSW) | VIP_PSW_IEP | VIP_PSW_UMC);
	mep_ei();
}

static void setupCore(void)
{
	vipWriteControlBus(VIP_CB_CACHE_CONTROL, 0x10);
	installVectors();
	configureInterrupts();
}

static void clearCoreRecords(void)
{
	uint32_t core = vipCoreNumber();
	VipWork *work = &g_work[core];
	uint32_t *words;
	uint32_t i;

	words = (uint32_t *)&g_exceptionRecords[core];
	for (i = 0; i < sizeof(VipExceptionRecord) / 4; i++)
		words[i] = 0;
	words = (uint32_t *)work;
	for (i = 0; i < sizeof(VipWork) / 4; i++)
		words[i] = 0;
}

/* the runtime's local RAM code and data, which the linker places */
extern char __lram_text_load[], __lram_text_start[], __lram_text_size[];
extern char __lram_data_load[], __lram_data_start[], __lram_data_size[];

/* Copy a local RAM section in, if it has anything. */
static void loadSection(uint32_t load, uint32_t start, uint32_t size)
{
	if (size)
		readDram(load, start, size);
}

static void initializeCore(void)
{
	VipCommandBlock *block;

	initializeUnits();
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE, 0);
	vipWriteControlBus(VIP_CB_CLOCK_REQUEST, 0);
	*vipMailboxCommand() = VIP_MAILBOX_IDLE;
	clearCoreRecords();
	initializeDmaQueue();
	loadSection((uint32_t)__lram_data_load, (uint32_t)__lram_data_start,
		(uint32_t)__lram_data_size);
	loadSection((uint32_t)__lram_text_load, (uint32_t)__lram_text_start,
		(uint32_t)__lram_text_size);
	vipInitializeTimer();
	forgetContext();
	block = VIP_COMMAND_BLOCK;
	block->status = VIP_CORE_IDLE;
	block->startCount++;
}

/* The core's main loop, entered from the boot block. */
void vipMain(void)
{
	uint32_t response[12];
	VipWork *work;

	setupCore();
	initializeCore();
	work = currentWork();
	for (;;) {
		while (work->pending) {
			runCommand(work->command, response);
			if (!work->pending)
				continue;
			mep_di();
			work->pending = 0;
			respond(response, 4, VIP_CORE_IDLE);
			mep_ei();
		}
		idle(work);
	}
}

/* the program's state, which the ARM side reads (VipCommandBlock.state) */
void vipSetState(int32_t state)
{
	VIP_COMMAND_BLOCK->state = state;
}

int32_t vipGetState(void)
{
	return VIP_COMMAND_BLOCK->state;
}

/* The command running, or NULL. */
uint32_t *vipGetCommand(void)
{
	VipWork *work = currentWork();

	return work->pending ? work->command : NULL;
}

/* Raise the events the ARM side asked for. */
void vipNotifyEvents(uint32_t events)
{
	events &= VIP_COMMAND_BLOCK->eventMask & VIP_MAILBOX_EVENTS;
	if (events)
		*vipMailboxAnswer() = events;
}

/* Add an entry to the trace ring, if the ARM side gave one. */
void vipTrace(uint32_t context, uint32_t event, uint32_t value)
{
	VipCommandBlock *block = VIP_COMMAND_BLOCK;
	VipTraceEntry *entry = (VipTraceEntry *)block->traceWrite;
	VipWork *work;
	uint32_t next;

	if (!entry)
		return;
	work = currentWork();
	entry->value = value;
	entry->event = event;
	entry->context = context;
	entry->sequence = work->traceCount;
	next = (uint32_t)(entry + 1);
	if (next >= block->traceEnd)
		next = block->traceStart;
	block->traceWrite = next;
	work->traceCount++;
}

void vipTraceEvent(uint32_t event, uint32_t value)
{
	vipTrace(VIP_COMMAND_BLOCK->currentContext, event, value);
}

/*
 * The answer to a stop or an exit: the command's tag and context and why, unless
 * the ARM side has not taken the last answer.
 */
static uint32_t haltAnswer(uint32_t tag, uint32_t context, uint32_t reason,
	volatile uint32_t *answer)
{
	VipCommandBlock *block;

	if (*answer & VIP_MAILBOX_UNREAD)
		return reason == 6 ? VIP_MAILBOX_ABORTED : VIP_MAILBOX_FAILED;
	block = VIP_COMMAND_BLOCK;
	block->response[0] = tag;
	block->response[1] = context;
	block->response[2] = reason;
	block->response[3] = 0;
	return VIP_MAILBOX_RESPONSE;
}

/*
 * Stop the core: reset the hardware, stop interrupts and the timer, let DMA
 * finish, and from local RAM give the ARM side the answer and sleep. Every core
 * runs the first reset hook, core 0's.
 */
void vipHalt(uint32_t tag, uint32_t context, uint32_t reason)
{
	volatile uint32_t *answer;
	uint32_t *code = (uint32_t *)VIP_HALT_CODE;
	uint32_t i;

	if (vipCoreNumber() < 0xff)
		VIP_RESET_HOOK();
	mep_di();
	vipWriteControlBus(VIP_CB_INTERRUPT_ENABLE, 0);
	vipWriteControlBus(VIP_CB_TIMER_ENABLE, 0);
	/* the original reads the DMA engine's busy flag the full count, idle or not */
	for (i = 0; i <= 0x8000000; i++)
		vipReadControlBus(VIP_CB_DMA_BUSY);
	for (i = 0; i < (uint32_t)(vipHaltCodeEnd - (const char *)vipHaltCode) / 4; i++)
		code[i] = ((const uint32_t *)vipHaltCode)[i];
	vipWriteControlBus(VIP_CB_CLOCK_REQUEST, 0);
	vipWriteControlBus(VIP_CB_CLOCK_ENABLE, 0);
	answer = vipMailboxAnswer();
	((void (*)(volatile uint32_t *, uint32_t, volatile uint32_t *))VIP_HALT_CODE)(
		answer, haltAnswer(tag, context, reason, answer),
		&VIP_COMMAND_BLOCK->status);
	for (;;)
		;
}
