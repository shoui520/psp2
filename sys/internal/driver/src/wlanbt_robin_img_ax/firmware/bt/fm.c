/*
 * The FM radio driver (0x494f4..0x49d3c). fmInit() creates the "fm" mdev
 * driver, its message pool and its task; the host's FM commands arrive as
 * messages (fmCoexProcessState()) and the task answers each one
 * (fmCoexConfigureState()). The FM block's interrupt runs fmCoexTaskMain()
 * through the mdev framework, which turns what the block reports into events
 * the host asked for.
 *
 * Marvell published no source; names are inferred from use.
 */
#include <stddef.h>
#include "robin/btrom.h"
#include "robin/btcontext.h"
#include "robin/buffer.h"
#include "robin/eventpower.h"
#include "robin/fm.h"
#include "robin/mdev.h"
#include "robin/rom.h"
#include "robin/soc.h"
#include "robin/sys.h"
#include "robin/threadx.h"

/* The FM task's event flags: bit 0 is a message for fmCoexConfigureState(). */
#define FM_TASK_EVENTS		0x7fff
#define FM_TASK_STACK_SIZE	0x400
#define FM_TASK_PRIORITY	12

/* The command (vendor opcode) the host sends to end a transfer, see fmCoexCheckState(). */
#define FM_COMMAND_END		0x57

static uint8_t fmGatePending __attribute__((section(".itcm.rw")));		/* 0x55344 */
static uint8_t fmLevel __attribute__((section(".itcm.rw")));			/* 0x55345 */
static const char *fmDriverName __attribute__((section(".itcm.rw"))) = "fm";	/* 0x55348 */
void *fmDevice __attribute__((section(".itcm.rw")));			/* 0x5534c */
static struct OsaPoolConfig fmPoolConfig __attribute__((section(".itcm.rw"))) = { 0, 0x118, 2 }; /* 0x55350 */
static uint32_t fmPoolControl[2] __attribute__((section(".itcm.rw")));		/* 0x55358 */
struct OsaBufferPool fmPool __attribute__((section(".itcm.rw"))) = { { { 1, 0, 0 } } }; /* 0x55360 */
static struct FmEventHandler fmEventHandlers[] __attribute__((section(".itcm.rw"))) = { /* 0x5536c */
	{ fmCoexConfigureState, 0x0001 },
	{ fmCoexParseState, 0x0008 },
	{ fmCoexBuildState, 0x0002 },
	{ btSbcRomInspectContext, 0x0004 },
	{ btSbcRomTrackContext, 0x0010 },
	{ fmCoexComputeState, 0x0100 },
	{ fmCoexQueryState, 0x0200 },
	{ fmCoexResetState, 0x0400 },
	{ fmCoexSyncState, 0x0800 },
	{ btSbcRomManageContext, 0x1000 },
	{ btSbcRomAcquireContext, 0x2000 },
	{ NULL, 0xff },
};

static uint8_t fmPoolMemory[0x238] __attribute__((section(".itcm.bss")));	/* 0x5a320 */
static struct MdevDriver fmDriver __attribute__((section(".itcm.bss")));	/* 0x5a558 */
static uint8_t fmTaskStack[FM_TASK_STACK_SIZE] __attribute__((section(".itcm.bss"))); /* 0x5a588 */

struct FmReply fmReply __attribute__((section(".sqram1.bss")));		/* 0xc0033328 */
static struct OsaMsgList fmMessages __attribute__((section(".sqram1.bss")));	/* 0xc0033424 */
/* The event flags group; the space reaches up to fmSettings. */
uint32_t fmFlags[22] __attribute__((section(".sqram1.bss")));		/* 0xc0033438 */
struct FmSettings fmSettings __attribute__((section(".sqram1.bss")));		/* 0xc0033490 */

static inline uint32_t fmCommandWord(const uint8_t *bytes)
{
	return bytes[0] | bytes[1] << 8 | bytes[2] << 16 | bytes[3] << 24;
}

/* The interrupt is rearmed now in modes 0 and 1; otherwise when the task has run. */
static inline int fmRearmNow(void)
{
	unsigned int mode = (fmSettings.policy >> 4) & 0xf;

	return mode == 0 || mode == 1;
}

/* Line 27: the FM block's interrupt goes to the driver's task. */
int btLmpLcDispatchDescriptor(int line, uint32_t pending)
{
	SOC_IRQ_ENABLE_CLEAR = FM_IRQ_MASK;
	mdevInterrupt(fmDevice, 0);
	SOC_IRQ_ENABLE_SET = FM_IRQ_MASK;
	if (fmRearmNow())
		FM_CONTROL &= ~FM_CONTROL_ARMED;
	return 1;
}

/* Marks the secondary gate to open when the host wants the event a status bit raises. */
__attribute__((noinline)) void btLmpLcParseDescriptor(uint32_t status)
{
	uint32_t mask;

	switch (status) {
	case FM_STATUS_LEVEL:
		if (!(fmSettings.policy & 0x20000))
			return;
		mask = 0x100;
		break;
	case FM_STATUS_TUNED:
		mask = 0xd2;
		break;
	case FM_STATUS_RDS:
		mask = 0x4;
		break;
	case FM_STATUS_STEREO:
		mask = 0x200;
		break;
	default:
		return;
	}
	if (fmCoexApplyState(mask))
		fmGatePending = 1;
}

/* The driver's interrupt callback: handles what the FM block reports. */
int fmCoexTaskMain(void)
{
	uint16_t enable = FM_INTERRUPT_ENABLE;
	uint16_t status;

	FM_INTERRUPT_ENABLE = 0;
	status = FM_STATUS;
	if (status & FM_STATUS_SEARCH) {
		btSbcConvertEntry();
		btLmpLcParseDescriptor(FM_STATUS_SEARCH);
	}
	if ((fmSettings.features >> 4) & 1 && status & FM_STATUS_TUNED) {
		btSbcSelectEntry();
		btLmpLcParseDescriptor(FM_STATUS_TUNED);
	}
	if (status & FM_STATUS_RDS) {
		btSbcFilterEntry();
		btLmpLcParseDescriptor(FM_STATUS_RDS);
	}
	if ((fmSettings.features >> 4) & 2) {
		if (status & FM_STATUS_LEVEL) {
			btSbcAdjustEntry(status);
			btLmpLcParseDescriptor(FM_STATUS_LEVEL);
		}
		if (status & FM_STATUS_STEREO) {
			btSbcRecordEntry();
			btLmpLcParseDescriptor(FM_STATUS_STEREO);
		}
	}
	if (fmGatePending) {
		fmGatePending = 0;
		sysSecondaryGateOpen();
	}
	FM_INTERRUPT_ENABLE = enable;
	return 0;
}

int fmCoexHandleState(void)
{
	btSbcResetEntry();
	return 0;
}

/* The driver's message callback: a command of type 1 goes to the task. */
int fmCoexProcessState(void *device, void *message, uint32_t argument, uint32_t type)
{
	if (type == 1) {
		osa_MsgQueueSend(&fmMessages, message);
		tx_event_flags_set((TX_EVENT_FLAGS_GROUP *)fmFlags, 1, TX_OR);
	}
	return 0;
}

/* The FM task: runs the handler of each event flag set. */
void fmCoexUpdateState(void)
{
	ULONG events = 0;
	unsigned int i;

	btSbcReleaseEntry();
	for (;;) {
		tx_event_flags_get((TX_EVENT_FLAGS_GROUP *)fmFlags, FM_TASK_EVENTS, TX_OR_CLEAR, &events,
				   TX_WAIT_FOREVER);
		for (i = 0; events != 0; i++) {
			if (fmEventHandlers[i].mask & events) {
				fmEventHandlers[i].handler(i);
				events &= ~fmEventHandlers[i].mask;
			}
		}
	}
}

int fmInit(void)
{
	int error;

	error = osa_BlockPoolCreate(&fmPool, &fmPoolConfig, fmPoolControl, fmPoolMemory, osaPoolEntryCreate);
	if (error != 0)
		return error;
	__aeabi_memclr4(&fmMessages, sizeof(fmMessages));
	btSbcRomApplyContext(fmTaskStack, FM_TASK_STACK_SIZE, FM_TASK_PRIORITY, fmCoexUpdateState);
	btSbcQueryEntry();
	tx_event_flags_create((TX_EVENT_FLAGS_GROUP *)fmFlags, "FmFlags");
	fmDriver.name = fmDriverName;
	fmDriver.callbacks[0] = btSbcRomResolveContext;
	fmDriver.callbacks[1] = btSbcRomScheduleContext;
	fmDriver.callbacks[2] = fmCoexHandleState;
	fmDriver.callbacks[3] = NULL;
	fmDriver.callbacks[4] = fmCoexProcessState;
	fmDriver.callbacks[5] = NULL;
	fmDriver.callbacks[6] = fmCoexTaskMain;
	if (mdevRegisterDriver(&fmDriver, fmCoexTaskMain) != 0)
		return 1;
	error = sysIrqRegister(FM_IRQ_LINE, btLmpLcDispatchDescriptor);
	if (error != 0)
		return error;
	return 0;
}

static inline void fmReplyError(int error)
{
	fmReply.status = -1;
	fmReply.result = error;
}

static inline void fmReplyWord(uint32_t value)
{
	fmReply.data[0] = value;
	fmReply.data[1] = value >> 8;
	fmReply.data[2] = value >> 16;
	fmReply.data[3] = value >> 24;
}

/* Handler of event flag 0: answers the next FM command. */
int fmCoexConfigureState(unsigned int index)
{
	OsaBuffer *message;
	const uint8_t *command;
	uint16_t handle;
	uint8_t active[4];
	uint8_t length[4];
	uint32_t value;
	uint32_t argument;
	unsigned int replyLength = 3;
	int error;
	struct FmClockSelection selection;

	message = osa_MsgQueueRecv(&fmMessages);
	if (message == NULL)
		return 1;
	btSbcConvertTable(0);
	command = (const uint8_t *)message + message->dataOffset;
	fmReply.status = 0;
	fmReply.command = command[0];
	fmReply.result = 0;
	switch (command[0]) {
	case 0x00:
	case 0x57:
		break;
	case 0x01:
		SOC_IRQ_ENABLE_CLEAR = FM_IRQ_MASK;
		SOC_IRQ_ENABLE_CLEAR = FM_IRQ_SECONDARY_MASK;
		error = btSbcClearEntry((uint8_t)(command[1] | command[2] << 8));
		if (error != 0) {
			fmReplyError(error);
			SOC_IRQ_ENABLE_CLEAR = FM_IRQ_MASK;
			SOC_IRQ_ENABLE_CLEAR = FM_IRQ_SECONDARY_MASK;
		} else {
			btSbcRomSetupEntry();
			SOC_IRQ_ENABLE_SET = FM_IRQ_MASK;
			SOC_IRQ_ENABLE_SET = FM_IRQ_SECONDARY_MASK;
		}
		break;
	case 0x02:
		error = btSbcFormatTable(command[1]);
		if (error != 0)
			fmReplyError(error);
		break;
	case 0x03:
		argument = fmCommandWord(&command[1]);
		btSbcRomVerifyEntry();
		if (btSbcRomSelectContext() != 0) {
			error = btSbcValidateTable(argument);
			value = argument;
		} else {
			error = btSbcScheduleTable(argument, &value, command[5]);
		}
		if (error != 0) {
			fmReply.status = -1;
		} else {
			fmReply.status = 0;
			fmReply.command = 3;
		}
		fmReply.result = error;
		fmReplyWord(value);
		replyLength = 7;
		break;
	case 0x05:
		error = btSbcClearTable(fmCommandWord(&command[1]), &value, command[5]);
		goto word;
	case 0x0b:
		btSbcValidateBuffer(command[1]);
		break;
	case 0x0c:
		fmReply.data[0] = btSbcPrepareBuffer();
		replyLength = 4;
		break;
	case 0x1a:
		error = btSbcRomQueryEntry(command[1], command[2], command[3]);
		if (error != 0)
			fmReplyError(error);
		break;
	case 0x30:
		fmReplyWord(hostCmd0003Version());
		replyLength = 7;
		break;
	case 0x34:
		error = btSbcTrackTable(&handle, active);
		goto half;
	case 0x37:
		error = btSbcReleaseTable(&value);
		goto word;
	case 0x38:
		error = btSbcSetupTable(&value);
word:
		btSbcRomVerifyEntry();
		if (error != 0)
			fmReplyError(error);
		fmReplyWord(value);
		replyLength = 7;
		break;
	case 0x3e:
		fmReply.data[0] = (fmSettings.features >> 13) & 1;
		replyLength = 4;
		break;
	case 0x40:
		error = btSbcNotifyBuffer(command[1]);
		if (error != 0)
			fmReplyError(error);
		break;
	case 0x41:
		btSbcFormatBuffer(fmCommandWord(&command[1]), fmCommandWord(&command[5]),
				  fmCommandWord(&command[9]), fmCommandWord(&command[13]));
		break;
	case 0x45:
		argument = fmCommandWord(&command[1]);
		btSbcCheckTable();
		btSbcRomApplyEntry(argument);
		btSbcRomRecordContext(1);
		break;
	case 0x51:
		btSbcRomManageEntry(command[1]);
		break;
	case 0x55:
		btSbcRomSyncEntry(command[1]);
		break;
	case 0x56:
		error = btSbcRomFormatEntry(command[1], command[2]);
		if (error != 0)
			fmReplyError(error);
		break;
	case 0x5a:
		handle = command[1] | command[2] << 8;
		error = btSbcInspectTable(handle, command[3]);
		if (error != 0)
			fmReplyError(error);
		break;
	case 0x5b:
		error = btSbcRomNotifyEntry(command[1]);
		if (error != 0) {
			fmReply.command = 0xff;
			fmReply.result = error;
		}
		break;
	case 0x5c:
		error = btSbcRomBuildEntry(command[1]);
		if (error != 0)
			fmReplyError(error);
		break;
	case 0x5d:
		btSbcRomInspectEntry(command[1]);
		break;
	case 0x5f:
		error = btSbcAcquireTable(&handle, active);
half:
		if (error != 0) {
			fmReplyError(error);
			break;
		}
		fmReply.data[1] = handle >> 8;
		fmReply.data[0] = handle;
		replyLength = 5;
		break;
	case 0x66:
		value = btSbcRomUpdateEntry();
		fmReply.data[1] = value >> 8;
		fmReply.data[0] = value;
		replyLength = 5;
		break;
	case 0x67:
		btSbcConfigureBuffer(command[1], command[2]);
		break;
	case 0x68:
		selection = btSbcSelectTable(fmCommandWord(&command[1]));
		fmReply.data[0] = selection.code;
		fmReply.data[1] = selection.flags;
		replyLength = 5;
		break;
	case 0x69:
		fmReply.data[0] = btSbcRomResolveEntry();
		replyLength = 4;
		break;
	case 0x70:
		btSbcAdjustTable(command[1]);
		break;
	case 0x71:
		btSbcDispatchBuffer(command[1]);
		break;
	case 0x72:
		fmReply.data[0] = btSbcParseBuffer();
		replyLength = 4;
		break;
	case 0x98:
		btSbcQueryTable(command[1], command[2], command[3], command[4]);
		break;
	case 0x99:
		btSbcResetTable(&fmReply.data[0], &fmReply.data[1], &fmReply.data[2], &fmReply.data[3]);
		replyLength = 7;
		break;
	case 0x9a:
		btSbcSyncTable(command[1], command[2], command[3]);
		break;
	case 0x9b:
		btSbcApplyTable(&fmReply.data[0], &fmReply.data[1], &fmReply.data[2]);
		replyLength = 6;
		break;
	case 0x9c: {
		/* Reads or writes a halfword of the FM block's memory. */
		volatile uint16_t *address = (volatile uint16_t *)fmCommandWord(&command[2]);

		fmReply.data[0] = command[1];
		if (command[1] != 0) {
			*address = command[6] | command[7] << 8;
		} else {
			value = *address;
			fmReply.data[4] = value >> 24;
			fmReply.data[3] = value >> 16;
			fmReply.data[2] = value >> 8;
			fmReply.data[1] = value;
		}
		replyLength = 6;
		break;
	}
	case 0x9d:
		btSbcComputeBuffer(fmCommandWord(&command[1]));
		break;
	case 0x9e:
		error = btSbcQueryBuffer(&fmReply.data[0], length);
		if (error != 0)
			fmReplyError(error);
		replyLength = length[0] + 3;
		break;
	case 0x9f:
		btSbcConvertTable(1);
		break;
	default:
		btSbcRomFormatContext(command);
		goto done;
	}
	fmEventSend(&fmReply, replyLength, FM_REPLY_COMMAND);
done:
	osa_BufferFree(message);
	return 1;
}

/* Whether the next command waiting is the end of a transfer. */
int fmCoexCheckState(void)
{
	OsaBuffer *message = osa_MsgQueuePeek(&fmMessages);

	if (message != NULL && ((const uint8_t *)message)[message->dataOffset] == FM_COMMAND_END)
		return 1;
	return 0;
}

int fmCoexValidateState(uint32_t *state)
{
	*state = 0;
	return 1;
}

/* Whether the secondary gate may close; bit 13 of the features keeps it open. */
int fmCoexPrepareState(uint32_t *state)
{
	*state = 1;
	if (fmRearmNow())
		*state = 0;
	return (fmSettings.features >> 13) & 1;
}

int fmCoexDispatchState(void)
{
	return btSbcVerifyTable();
}

/* Handler of event flag 3: reports the RDS data. */
int fmCoexParseState(unsigned int index)
{
	uint32_t count = 0;

	if (fmSettings.eventMask & 0x2) {
		fmReply.status = 0;
		fmReply.command = FM_EVENT_RDS;
		if (btSbcRomSelectEntry(&fmReply.result, &count) != 0)
			count = 0;
		fmEventSend(&fmReply, count * 2 + 2, FM_REPLY_EVENT);
	}
	return 1;
}

/* Handler of event flag 1: reports the end of a search. */
int fmCoexBuildState(unsigned int index)
{
	uint16_t handle;
	uint8_t active[4];
	UINT posture;

	posture = tx_interrupt_control(TX_INT_DISABLE);
	if (fmTuner.searchCount <= fmTuner.searchLimit)
		fmTuner.searchCount++;
	tx_interrupt_control(posture);
	btSbcTrackTable(&handle, active);
	if (fmSettings.eventMask & 0x4) {
		fmReply.status = 0;
		fmReply.command = FM_EVENT_SEARCH;
		fmReply.data[2] = handle >> 24;
		fmReply.data[1] = handle >> 16;
		fmReply.data[0] = handle >> 8;
		fmReply.result = handle;
		fmEventSend(&fmReply, 6, FM_REPLY_EVENT);
		btSbcRomVerifyContext(4);
	}
	if (fmSettings.eventMask & 0x1 && active[0] != 0) {
		fmReply.status = 0;
		fmReply.command = FM_EVENT_SEARCH_DONE;
		fmEventSend(&fmReply, 2, FM_REPLY_EVENT);
	}
	return 1;
}

/* Handler of event flag 8: reports a change of the signal level. */
int fmCoexComputeState(unsigned int index)
{
	uint8_t level = btSbcRomValidateEntry();

	if (level != fmLevel) {
		fmLevel = level;
		btSbcRomConvertContext(0x100);
		if (fmSettings.policy & 0x20000 && fmSettings.eventMask & 0x100) {
			fmReply.status = 0;
			fmReply.command = FM_EVENT_LEVEL;
			fmReply.result = level;
			fmEventSend(&fmReply, 3, FM_REPLY_EVENT);
		}
	}
	return 1;
}

int fmCoexQueryState(unsigned int index)
{
	int mode = 0;

	if (fmTuner.levelMode != 0)
		mode = 3;
	else if (fmSettings.policy & 0x20000)
		mode = btSbcRomParseEntry(0);
	btSbcHandleBuffer(mode);
	return 1;
}

int fmCoexResetState(unsigned int index)
{
	if (fmSettings.policy & 0x20000)
		btSbcHandleBuffer(1);
	return 1;
}

int fmCoexSyncState(unsigned int index)
{
	btSbcRomComputeEntry(0x7f);
	return 1;
}

/* Whether the FM block reports an event of the mask the host wants. */
__attribute__((noinline)) int fmCoexApplyState(uint32_t mask)
{
	return (btSbcRomClearContext() & mask & fmSettings.eventMask) != 0;
}
