/*
 * fw2 runtime: what FW2's programs share.
 *
 * FW2 is the decoder again, built to serve several decoder contexts at once. The
 * ARM side gives each core commands through the core's command block and its
 * mailbox. A command names a context: a block of DRAM the ARM side allocated,
 * which records the program that serves it and holds that program's globals and
 * a copy of its local RAM data. To run a command the runtime loads the program's
 * code overlay and the context's local RAM data, points $gp at the context's
 * globals and calls the program; afterwards it saves the local RAM data back.
 * One program so serves many contexts, each with its own state.
 *
 * Both cores run this runtime from DRAM. Core 0 has two programs, the stream
 * parsers; core 1 has one, the codec.
 */
#pragma once

#include <vip.h>

/* the core this code runs on, from $id */
#define vipCoreNumber()		(vipReadControl(VIP_CR_ID) >> 16)
#define vipCore()		(vipCoreNumber() & 1)

/*
 * The image's first 0x3000 bytes are also seen at 0, and the runtime reaches the
 * command blocks and the program records there.
 */
#define VIP_COMMAND_BLOCKS	0x1400
#define VIP_PROGRAM_RECORDS	0x2000	/* 0x100 per core */
#define VIP_RESET_HOOKS		0x2200
#define VIP_DEBUGGER_ENTRY	0x2208	/* where a core numbered 255 starts */

/* local RAM */
#define VIP_LOCAL_VECTORS	(VIP_LOCAL_RAM + 0x00)	/* $cfg VIP_CFG_LOCAL_VECTORS */
#define VIP_INTERRUPT_VECTORS	(VIP_LOCAL_RAM + 0x30)
#define VIP_PROGRAM_CODE	(VIP_LOCAL_RAM + 0x40)	/* the program's code overlay */
#define VIP_HALT_CODE		(VIP_LOCAL_RAM + 0x90)	/* vipHalt's last instructions */
#define VIP_RUNTIME_STACK	(VIP_LOCAL_RAM + 0x2400)
#define VIP_PROGRAM_DATA	(VIP_LOCAL_RAM + 0x2400)	/* the context's local RAM data */

#define VIP_CFG_LOCAL_VECTORS	0x10	/* exceptions and interrupts enter in local RAM */

/* the interrupts FW2 uses */
#define VIP_INTERRUPT_TIMER	4

#ifdef __ASSEMBLER__

/*
 * An interrupt whose handler is C: save what C may change, call it, restore.
 * The handler acknowledges the interrupt itself.
 */
	.macro	interruptEntry name, handler
	.global	\name
\name:
	add3	$sp, $sp, -56
	sw	$tp, 48($sp)
	sw	$12, 44($sp)
	sw	$11, 40($sp)
	sw	$10, 36($sp)
	sw	$9, 32($sp)
	sw	$4, 28($sp)
	sw	$3, 24($sp)
	sw	$2, 20($sp)
	sw	$1, 16($sp)
	ldc	$1, $lp
	sw	$0, 12($sp)
	sw	$1, 52($sp)
	ldc	$2, $rpb
	ldc	$3, $rpe
	sw	$2, 8($sp)
	sw	$3, 4($sp)
	ldc	$4, $rpc
	sw	$4, 0($sp)
	bsr	\handler
	lw	$1, 52($sp)
	lw	$2, 8($sp)
	lw	$3, 4($sp)
	stc	$2, $rpb
	lw	$4, 0($sp)
	lw	$0, 12($sp)
	lw	$2, 20($sp)
	stc	$3, $rpe
	lw	$3, 24($sp)
	lw	$9, 32($sp)
	lw	$10, 36($sp)
	lw	$11, 40($sp)
	lw	$12, 44($sp)
	lw	$tp, 48($sp)
	stc	$4, $rpc
	lw	$4, 28($sp)
	stc	$1, $lp
	lw	$1, 16($sp)
	add3	$sp, $sp, 56
	reti
	.endm

#else /* !__ASSEMBLER__ */

/* the runtime's code and data in local RAM, which it copies in at start */
#define VIP_LRAM_TEXT		__attribute__((section(".lram.text")))
#define VIP_LRAM_DATA		__attribute__((section(".lram.data")))

/*
 * A command block, one per core, in the image. The ARM side writes a command and
 * signals the core's mailbox; the core copies the command, answers in response[]
 * and signals back. The block also holds the core's state and statistics.
 */
typedef struct VipCommandBlock {
	uint32_t command[14];		/* VipCommand */
	uint32_t response[10];		/* the command's first words, its result, its output */
	uint32_t commandCount;		/* commands run, and their time (timer ticks) */
	uint32_t commandMaximum;
	uint32_t commandWraps;
	uint32_t commandTotal;
	uint32_t reserved[27];
	int32_t currentProgram;		/* the overlay in local RAM, or -1 */
	uint32_t eventMask;		/* the events the ARM side wants (vipNotifyEvents) */
	uint32_t startCount;
	uint32_t currentContext;	/* whose data is in local RAM, or 0 */
	uint32_t traceWrite;		/* a ring of VipTraceEntry, or 0 for none */
	uint32_t traceEnd;
	uint32_t traceStart;
	volatile uint32_t status;	/* VIP_CORE_* */
	volatile int32_t state;		/* the program's (vipSetState), or -1 */
} VipCommandBlock;

_Static_assert(sizeof(VipCommandBlock) == 0x100, "VipCommandBlock");

#define VIP_COMMAND_BLOCK	(&((VipCommandBlock *)VIP_COMMAND_BLOCKS)[vipCore()])

/* VipCommandBlock.status */
#define VIP_CORE_STOPPED	0
#define VIP_CORE_IDLE		1
#define VIP_CORE_BUSY		3
#define VIP_CORE_SLEEPING	5

/* the command words */
typedef struct VipCommand {
	uint32_t tag;			/* returned in the response */
	uint32_t context;		/* VipContext */
	uint32_t kind;			/* VIP_COMMAND_* */
	uint32_t arguments[11];		/* the program's; for VIP_COMMAND_OPEN, below */
} VipCommand;

/* VipCommand.kind */
#define VIP_COMMAND_RUN		0	/* anything else: the program runs it */
#define VIP_COMMAND_EXIT	1	/* stop the core */
#define VIP_COMMAND_OPEN	2	/* set up a context for a program */

/* VIP_COMMAND_OPEN's arguments */
#define VIP_OPEN_PROGRAM	0	/* its program number on this core */
#define VIP_OPEN_LOCK		1	/* the ARM side's lock word of the context */

/* VipCommand result codes */
#define VIP_RESULT_OK		0
#define VIP_RESULT_BAD_OPEN	-2
#define VIP_RESULT_NO_CONTEXT	-4
#define VIP_RESULT_LOCKED	-6	/* the ARM side holds the context */
#define VIP_RESULT_IN_USE	-7	/* the other core runs it */

/* the mailbox: written by the ARM side with a command (or a stop), by the core with an answer */
#define VIP_MAILBOX_COMMAND	0x80000000
#define VIP_MAILBOX_STOP	0x40000000
#define VIP_MAILBOX_ACCEPTED	0x80000000	/* the core took the command */
#define VIP_MAILBOX_IDLE	0xffffffff
#define VIP_MAILBOX_RESPONSE	0x80000000	/* the core's: response[] is filled */
#define VIP_MAILBOX_UNREAD	0x80000000	/* the ARM side has not taken the last answer */
#define VIP_MAILBOX_FAILED	0x20000000	/* the core's, when the answer could not be given */
#define VIP_MAILBOX_ABORTED	0x08000000
#define VIP_MAILBOX_SUCCEEDED	0x10000000
#define VIP_MAILBOX_EVENTS	0x4fffffff	/* the bits vipNotifyEvents may raise */

/* the core's mailbox: the ARM side writes command (debugSend), the core writes answer (debugReceive) */
#define vipMailboxAnswer()	(&VIP_MAILBOX->debugReceive[vipCore() ^ 1])
#define vipMailboxCommand()	(&VIP_MAILBOX->debugSend[vipCore() ^ 1])

/*
 * What a program is: one per program number, per core, at VIP_PROGRAM_RECORDS.
 * The ARM side reads the sizes to allocate contexts.
 */
typedef struct VipProgram {
	int (*entry)(uint32_t *output);	/* runs a command; output: response[4] on */
	uint32_t code;			/* the code overlay, for VIP_PROGRAM_CODE */
	uint32_t codeSize;
	uint32_t initialLocalData;	/* the local RAM data of a new context */
	uint32_t initialGlobals;	/* the initialised globals */
	uint32_t globalsSize;
	uint32_t zeroGlobalsSize;	/* the globals after them, zeroed */
	uint32_t localDataSize;
} VipProgram;

/* a program entry's results besides VIP_RESULT_*: 1 leaves the context running */
#define VIP_PROGRAM_CONTINUE	1

#define VIP_PROGRAMS(core)	((const VipProgram *)(VIP_PROGRAM_RECORDS + 0x100 * (core)))

/*
 * A context: the ARM side allocates it (VipProgram's sizes and this header), the
 * core sets it up on VIP_COMMAND_OPEN. Its globals follow the header, then the
 * saved local RAM data.
 */
typedef struct VipContext {
	volatile uint32_t *lock;	/* non-zero while the ARM side works on the context */
	uint32_t owner;			/* the core that ran it last; VIP_CONTEXT_RUNNING | core */
	uint32_t program;
	int (*entry)(uint32_t *output);
	uint32_t localData;		/* the saved local RAM data */
	uint32_t size;			/* of the globals and the local RAM data */
	uint32_t globals[];		/* the program's $gp data starts here */
} VipContext;

#define VIP_CONTEXT_RUNNING	0x80000000
#define VIP_CONTEXT_NO_OWNER	0xffff
#define VIP_CONTEXT_OWNER_LIMIT	0x10000	/* owners below this are cores, not running */

/* $gp of a context: GCC addresses tiny data from __sdabase, 0x8000 into it */
#define VIP_CONTEXT_GP(context)	((uint32_t)(context)->globals + 0x8000)

/* a program's variables in the running context, through $gp (after a pointer's *) */
#define VIP_GLOBAL		__attribute__((tiny))

/*
 * The context's variables past $gp's reach (a program's .context.far, after its
 * tiny ones): a variable's place in the running context.
 */
#define VIP_GLOBAL_FAR		__attribute__((section(".context.far")))

extern char __sdabase[];

static inline uint32_t vipReadGp(void)
{
	uint32_t gp;

	__asm__ ("mov %0,$gp" : "=r" (gp));
	return gp;
}

#define vipContextAddress(variable) ((__typeof__(&(variable)))((uint32_t)&(variable) - \
	(uint32_t)__sdabase + vipReadGp()))

/* a command's slot: the mailbox interrupt fills it, the core's main loop runs it */
typedef struct VipWork {
	volatile uint32_t pending;
	uint32_t command[14];
	uint32_t traceCount;		/* VipTraceEntry.sequence */
} VipWork;

extern VipWork g_work[2];

/* a trace entry (VipCommandBlock.traceWrite) */
typedef struct VipTraceEntry {
	uint32_t sequence;
	uint32_t context;
	uint32_t event;
	uint32_t value;
} VipTraceEntry;

/* each core's exception record */
typedef struct VipExceptionRecord {
	uint32_t gp;
	uint32_t lp;
	uint32_t epc;
	uint32_t npc;
} VipExceptionRecord;

extern VipExceptionRecord g_exceptionRecords[2];

/* command.c */
void vipSetState(int32_t state);
int32_t vipGetState(void);
uint32_t *vipGetCommand(void);
void vipNotifyEvents(uint32_t events);
void vipTrace(uint32_t context, uint32_t event, uint32_t value);
void vipTraceEvent(uint32_t event, uint32_t value);
void vipHalt(uint32_t tag, uint32_t context, uint32_t reason) __attribute__((noreturn));

/* timer.c */
uint32_t vipReadTimer(void);
void vipStartTimer(void);
void vipStopTimer(void);
void vipInitializeTimer(void);
void vipWaitTimer(uint32_t ticks);

/* Clear the repeat block registers, RPB, RPE and RPC. */
static inline void clearRepeat(void)
{
	__asm__ __volatile__("stc %0, $rpb\n\tstc %0, $rpe\n\tstc %0, $rpc" : : "r"(0));
}

/* fw1/common/dma.c, lram.c */
void initializeDmaQueue(void);
int readDram(uint32_t external, uint32_t local, uint32_t size);
int writeDram(uint32_t external, uint32_t local, uint32_t size);
void dmaComplete(uint32_t argument);

/* memory.c */
void vipZeroDram(uint32_t address, uint32_t size);
void vipCopyDram(const uint32_t *source, uint32_t *destination, uint32_t size);

/* start.S: what C cannot do */
int vipCallProgram(int (*entry)(uint32_t *output), uint32_t *output, uint32_t gp);
void vipSleepCode(volatile uint32_t *status, uint32_t sleeping, uint32_t awake);
void vipLocalSleep(volatile uint32_t *status, uint32_t sleeping, uint32_t awake);	/* its copy */
void vipHaltCode(volatile uint32_t *answer, uint32_t value,
	volatile uint32_t *status) __attribute__((noreturn));
extern const uint32_t vipSleepCodeSize[];
extern const char vipHaltCodeEnd[];
extern const uint32_t vipInterruptVectors[], vipInterruptVectorsSize[];

#endif /* !__ASSEMBLER__ */
