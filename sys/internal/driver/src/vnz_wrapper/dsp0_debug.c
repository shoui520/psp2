/* Venezia debugger: unpack the hardware's three control records. */
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <sysmem/sysmem.h>
#include <scetypes.h>
#include <sysmem/memblock.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <lowio/pervasive_regs.h>
#include <lowio/mailbox_regs.h>
#include <intrmgr/intrmgr.h>
#include <processmgr/processmgr.h>
#include <intrmgr/syscall.h>
#include <dmacmgr/dmacmgr.h>
#include "venezia_internal.h"
#include <vnz_wrapper/venezia_debug.h>
#include <string.h>
#include <lowio/pervasive.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysroot.h>
#include <threadmgr/threadmgr.h>
#include <power/power.h>
#include <post_ss_mgr/sblfwloader.h>

/* Physical addresses handed to or checked for the Venezia: a mailbox word,
 * and the word after the lock in a TAS page */
#define MAILBOX_PADDR(field) \
	(PERVASIVE_MAILBOX_BASE + offsetof(ScePervasiveMailboxRegisters, field))
#define TAS_SIGNAL_PADDR(channel) \
	(PERVASIVE_TAS_BASE(channel) + offsetof(ScePervasiveTasChannel, unidentifiedRegisters))

extern void *veneziaDebugMemset(void *destination, int value, SceSize size)
	__asm__("memset");

/* name unknown: debugger ring descriptor shared with the selected hardware bank. */
typedef struct VeneziaDebugRingCursor {
	uint32_t control;
	uint32_t address;
} VeneziaDebugRingCursor;

typedef struct VeneziaDebugRingConfig {
	uint32_t control;
	uint32_t base;
	uint32_t capacity;
	uint32_t request;
	VeneziaDebugRingCursor cursors[4];
	uint32_t parameters[4];
} VeneziaDebugRingConfig;

typedef union VeneziaDebugRingWork {
	VeneziaDebugRingConfig config;
	uint32_t words[17];
} VeneziaDebugRingWork;

typedef struct VeneziaDebugTracePacket {
	uint32_t header[3];
	uint32_t context;
} VeneziaDebugTracePacket;

/* name unknown: captured instruction and execution control records. */
typedef struct VeneziaDebugContextRecord {
	uint32_t header[2];
	uint32_t encodedInstruction;
	uint32_t contextControl;
	uint32_t instruction;
	uint32_t operand;
	uint32_t address;
	uint32_t result;
	uint32_t execution[2];
	uint32_t accumulator;
	uint32_t source;
	uint32_t destination;
	uint32_t flags;
	uint32_t trailer;
	uint32_t capturePhysicalAddress;
} VeneziaDebugContextRecord;

typedef struct VeneziaDebugCaptureHeader {
	uint32_t header;
	uint32_t control;
	uint32_t payload[2];
	uint32_t flags;
} VeneziaDebugCaptureHeader;

typedef struct VeneziaDebugRegisterPair {
	uint32_t low;
	uint32_t high;
} VeneziaDebugRegisterPair;

typedef struct VeneziaDebugRegisterBankRecord {
	uint32_t enabled;
	uint32_t control;
	VeneziaDebugRegisterPair registers[32];
	uint32_t validMask;
	uint32_t selectors;
} VeneziaDebugRegisterBankRecord;

typedef struct VeneziaDebugParameterRecord {
	uint32_t header;
	uint32_t control;
	uint32_t values[3][4];
	uint32_t status;
	uint32_t reserved;
} VeneziaDebugParameterRecord;

/* PLACEHOLDER: the capture protocol prefix and individual control bits are unnamed. */
typedef struct VeneziaDebugRegisterCapture {
	VeneziaDebugCaptureHeader header;
	uint32_t captureControl;
	uint32_t validity;
	uint32_t vectorControl;
	VeneziaDebugRegisterPair vectorRegisters[2][32];
	uint32_t channelHeader;
	VeneziaDebugChannelRecord channels[2];
	uint32_t parameterControl;
	uint32_t parameterHeader;
	VeneziaDebugParameterRecord parameters[2];
	uint32_t executionControl;
	uint32_t executionStatus;
	uint32_t pipelineControl;
	uint32_t programCounter;
	uint32_t stackPointer;
	uint32_t indexControl;
	VeneziaDebugRegisterBankRecord banks[2];
} VeneziaDebugRegisterCapture;

_Static_assert(offsetof(VeneziaDebugRegisterCapture, executionControl) == 780,
	"Venezia register capture control position");
_Static_assert(sizeof(VeneziaDebugRegisterBankRecord) == 272,
	"Venezia register capture bank size");
_Static_assert(sizeof(VeneziaDebugRegisterBank) == 296,
	"Venezia register bank output size");
_Static_assert(sizeof(VeneziaDebugRegisterInfo) == 632,
	"Venezia register output size");

/* The DSP bus swaps the two word lanes of every eight-byte pair. */
static inline uint32_t readVeneziaDebugContextWord(const uint32_t *word)
{
	return *(const uint32_t *)((uintptr_t)word ^ 4);
}

_Static_assert(sizeof(VeneziaDebugRingConfig) == 64, "Venezia debug ring configuration size");

/* name unknown: debugger capture command and its selected-record mask. */
typedef union VeneziaDebugCaptureRequest {
	struct {
		uint32_t command;
		uint32_t destination;
		uint32_t arguments[2];
		uint32_t selectionMask[2];
		uint32_t parameters[2];
	};
	uint32_t words[9];
} VeneziaDebugCaptureRequest;

typedef union VeneziaDebugExecutionWork {
	struct {
		uint32_t command;
		uint32_t mode;
		uint32_t arguments[3];
		uint32_t result;
		uint32_t parameters[10];
	};
	VeneziaDebugCaptureRequest capture;
	uint32_t words[17];
} VeneziaDebugExecutionWork;

uint32_t g_veneziaDebugCaptureIndices[4][6] = {
	{ ~0u, ~0u, ~0u, ~0u, ~0u, ~0u },
	{ ~0u, ~0u, ~0u, ~0u, ~0u, ~0u },
	{ 0, 1, 32, ~0u, ~0u, ~0u },
	{ ~0u, ~0u, ~0u, ~0u, ~0u, ~0u },
};

/* name unknown: selected debugger ring's hardware register offsets. */
enum VeneziaDebugRingRegister {
	VENEZIA_DEBUG_RING_CAPACITY = offsetof(VeneziaDebugRingConfig, capacity),
	VENEZIA_DEBUG_RING_REQUEST = offsetof(VeneziaDebugRingConfig, request),
	VENEZIA_DEBUG_RING_CURSOR_VALID = offsetof(VeneziaDebugRingConfig, cursors[0].control),
	VENEZIA_DEBUG_RING_CURSOR = offsetof(VeneziaDebugRingConfig, cursors[0].address),
	VENEZIA_DEBUG_RING_READER_VALID = offsetof(VeneziaDebugRingConfig, cursors[1].control),
	VENEZIA_DEBUG_RING_READER_CURSOR = offsetof(VeneziaDebugRingConfig, cursors[1].address),
	VENEZIA_DEBUG_RING_READER_STRIDE = sizeof(VeneziaDebugRingCursor),
	VENEZIA_DEBUG_TRACE_RING_FIRST = 28,
};

/* name unknown: FIFO slots waiting for their selected capture masks to clear. */
typedef struct VeneziaDebugCaptureQueueEntry {
	int slot;
	int mode;
} VeneziaDebugCaptureQueueEntry;

/* name unknown: host operations supplied by the Venezia boot controller. */
typedef struct VeneziaDebugCallbacks {
	int (*lock)(int resource);
	void (*unlock)(int resource);
	int (*readMailbox)(unsigned int channel, uint32_t *value);
	int (*writeMailbox)(unsigned int channel, uint32_t value);
	int (*signalMailbox)(unsigned int channel, uint32_t value);
	int (*clearReset)(void);
	int (*setReset)(void);
	int (*checkReset)(void);
} VeneziaDebugCallbacks;

/* name unknown: DSP capture geometry, control word, and protocol options. */
typedef struct VeneziaDebugCaptureConfig {
	SceSize size;
	uint32_t startOffset;
	uint32_t parameters[3];
	uint32_t control;
	uint32_t format;
	uint32_t options;
	uint32_t extraParameters[2];
} VeneziaDebugCaptureConfig;

typedef union VeneziaDebugCaptureCommandWork {
	struct {
		uint32_t command;
		uint32_t mode;
		uint32_t addresses[5];
		uint32_t imageAddress;
		SceSize imageSize;
		uint32_t mapBase;
		SceSize mapSize;
		uint32_t firmwareBase;
		SceSize firmwareSize;
		uint32_t reserved[3];
	};
	uint32_t words[17];
} VeneziaDebugCaptureCommandWork;

typedef union VeneziaDebugTraceState {
	struct {
		uint32_t imageVirtualBase;
		uint32_t imagePhysicalBase;
		SceSize imageSize;
		uint32_t bufferVirtualBase;
		uint32_t bufferPhysicalBase;
		SceSize bufferSize;
		uint32_t auxiliaryVirtualBase;
		uint32_t auxiliaryPhysicalBase;
		SceSize auxiliarySize;
		uint32_t flags;
	};
	uint32_t words[10];
} VeneziaDebugTraceState;

/* PLACEHOLDER: individual capture protocol parameter meanings are unknown. */
typedef struct VeneziaDebuggerState {
	unsigned int mode;
	uint32_t memoryVirtualBase;
	uint32_t memoryPhysicalBase;
	SceSize memorySize;
	VeneziaDebugCallbacks callbacks;
	VeneziaDebugCaptureConfig captureConfig[2];
	uint16_t captureControl[2];
	int captureGroup[2];
	uint32_t auxiliaryTraceEnabled[2];
	VeneziaDebugTraceState trace;
	VeneziaDebugCaptureQueueEntry captureQueue[4];
	uint32_t captureSlotBusy[4];
	unsigned int selectedBank;
	int captureInvalid;
	SceKernelSpinlock spinlock;
	void (*log)(unsigned int event, unsigned int count, ...);
} VeneziaDebuggerState;

_Static_assert(sizeof(VeneziaDebuggerState) == 0xfc, "Venezia debugger state size");
VeneziaDebuggerState g_veneziaDebugger;
uint32_t g_veneziaDebugBankOffsets[2] = { 0xc4000, 0xc8000 };

typedef struct VeneziaDebugMemoryRegion {
	SceSize size;
	uint32_t offset;
} VeneziaDebugMemoryRegion;

/* PLACEHOLDER: the DSP-owned protocol header's individual words are unknown. */
typedef struct VeneziaDebugMemoryHeader {
	uint32_t protocol[140];
	VeneziaDebugMemoryRegion regions[];
} VeneziaDebugMemoryHeader;

void writeVeneziaDebugMemoryWord(unsigned int offset, uint32_t value);
void writeVeneziaDebugCommandWord(unsigned int offset, uint32_t value);
void copyVeneziaDebugCommand(unsigned int offset, const void *data, SceSize size);
void updateVeneziaDebugCommandWord(unsigned int offset, uint32_t value,
	unsigned int shift, uint32_t mask);
void writeVeneziaDebugBankRegister(unsigned int offset, uint32_t value);
void writeVeneziaDebugStatusWord(unsigned int offset, uint32_t value);
void writeVeneziaDebugRegister(unsigned int offset, uint32_t value);
void updateVeneziaDebugRegister(unsigned int offset, uint32_t value,
	unsigned int shift, uint32_t mask);
void writeVeneziaDebugSelectedWord(unsigned int offset, uint32_t value);
void updateVeneziaDebugSelectedWord(unsigned int offset, uint32_t value,
	unsigned int shift, uint32_t mask);
void writeVeneziaDebugRingConfig(unsigned int offset, const VeneziaDebugRingConfig *config);
uint32_t readVeneziaDebugCommandWord(unsigned int offset);
uint32_t readVeneziaDebugCommandBits(unsigned int offset, unsigned int shift,
	uint32_t mask);
uint32_t readVeneziaDebugStatusWord(unsigned int offset);
uint32_t readVeneziaDebugChannelWord(unsigned int offset);
uint32_t readVeneziaDebugChannelBits(unsigned int offset, unsigned int shift,
	uint32_t mask);
uint32_t readVeneziaDebugSelectedWord(unsigned int offset);
uint32_t readVeneziaDebugSelectedBits(unsigned int offset, unsigned int shift,
	uint32_t mask);
void switchVeneziaDebugBank(void);

/* PLACEHOLDER: Sony's individual hardware control-field names are unknown.
 * The debugger returns thirteen control values for each of three records.
 */
_Static_assert(sizeof(VeneziaDebugChannelState) == 64, "Venezia debugger channel state size");

VeneziaVipState g_veneziaVipState;

_Static_assert(sizeof(VeneziaDebugSnapshot) == 1472, "debug capture snapshot");
_Static_assert(sizeof(VeneziaDebugSnapshotOutput) == 1488, "debug output snapshot");
_Static_assert(sizeof(VeneziaDebugProcessCapture) == 336, "process capture record");
_Static_assert(sizeof(VeneziaVipProcessStorage) == 0x78c, "VIP process storage");
_Static_assert(sizeof(VeneziaDebugCallbackState) == 200, "debug callback state");
_Static_assert(offsetof(VeneziaVipProcessStorage, callbackState.callbacks) == 1524,
	"process callback records");
_Static_assert(sizeof(VeneziaVipState) == 0x20c, "VIP module state");

/* name unknown: expand one packed hardware control record into a debugger column. */
VeneziaDebugChannelState *decodeVeneziaDebugControlFields(VeneziaDebugChannelState *state,
						const uint32_t words[2], unsigned int column)
{
	state->control[0][column] = words[0] >> 30;
	state->control[1][column] = !!(words[0] & 0x20000000);
	state->control[2][column] = (words[0] >> 24) & 0x1f;
	state->control[3][column] = !!(words[0] & 0x00800000);
	state->control[4][column] = !!(words[0] & 0x00400000);
	state->control[5][column] = !!(words[0] & 0x00200000);
	state->control[6][column] = words[0] >> 13;
	state->control[7][column] = !!(words[0] & 0x00001000);
	state->control[8][column] = words[1] >> 26;
	state->control[9][column] = !!(words[0] & 0x00000800);
	state->control[10][column] = (words[1] >> 20) & 0x3f;
	state->control[11][column] = !!(words[0] & 0x00000400);
	state->control[12][column] = (words[1] >> 15) & 0x1f;
	return state;
}

/* name unknown: initialize one debugger capture slot from a record-index list. */
int initVeneziaDebugCaptureRequest(unsigned int slot, int group)
{
	VeneziaDebugCaptureRequest request;
	uint32_t lowerMask = 0, upperMask = 0;
	uint32_t *indices;
	unsigned int i, index;

	for (i = 0; i != 8; i++)
		request.words[i] = 0;
	indices = g_veneziaDebugCaptureIndices[group];
	request.arguments[1] = 0;
	request.command |= 1;
	request.destination = g_veneziaDebugger.memoryPhysicalBase + ((slot + 3057) << 8);
	for (i = 0; ; i++) {
		index = indices[i];
		if (index == ~0u)
			break;
		if (index > 31)
			upperMask |= 1u << (index - 32);
		else
			lowerMask |= 1u << index;
	}
	request.selectionMask[0] = lowerMask;
	request.selectionMask[1] = upperMask;
	copyVeneziaDebugCommand(32 * slot, &request, 32);
	return 0;
}

/* name unknown: publish the physical destinations selected by a capture group. */
int initVeneziaDebugCaptureAddresses(unsigned int slot, int group)
{
	uint32_t *indices = g_veneziaDebugCaptureIndices[group];
	unsigned int commandOffset = (slot + 1) << 8;
	unsigned int destinationIndex = (slot << 6) + 12244;
	unsigned int i = 0, index;

	for (;;) {
		index = indices[i++];
		if (index == ~0u)
			break;
		writeVeneziaDebugCommandWord(commandOffset + 4 * index,
			g_veneziaDebugger.memoryPhysicalBase + ((destinationIndex + index) << 6));
	}
	return 0;
}

/* name unknown: retire completed captures and advance the hardware queue head. */
int drainVeneziaDebugCaptureQueue(void)
{
	int count, completed, i;
	unsigned int offset, pendingMask;
	uint32_t next;

	for (count = 0; count != 4; count++) {
		if (g_veneziaDebugger.captureQueue[count].slot == -1)
			break;
	}
	for (i = count; i != 4; i++) {
		if (g_veneziaDebugger.captureQueue[i].slot != -1)
			return 2;
	}
	if (!count)
		return 0;
	completed = 0;
	for (;;) {
		offset = 32 * g_veneziaDebugger.captureQueue[completed].slot;
		pendingMask = readVeneziaDebugCommandWord(offset + 16);
		pendingMask |= readVeneziaDebugCommandWord(offset + 20);
		if (pendingMask)
			break;
		completed++;
		updateVeneziaDebugCommandWord(offset, 0, 0, 1);
		if (completed >= count)
			break;
	}
	if (!completed)
		return 0;
	if (count == completed)
		next = 0;
	else
		next = g_veneziaDebugger.memoryPhysicalBase +
			32 * (g_veneziaDebugger.captureQueue[completed].slot + 24448);
	if (g_veneziaDebugger.callbacks.lock(2) != 0)
		return 1;
	writeVeneziaDebugRegister(12, next);
	g_veneziaDebugger.callbacks.unlock(2);
	for (i = 0; i < completed; i++)
		g_veneziaDebugger.captureSlotBusy[g_veneziaDebugger.captureQueue[i].slot] = 0;
	for (i = 0; i != 4; i++) {
		if (completed > 3) {
			g_veneziaDebugger.captureQueue[i].slot = -1;
			g_veneziaDebugger.captureQueue[i].mode = 0;
		} else {
			g_veneziaDebugger.captureQueue[i].slot =
				g_veneziaDebugger.captureQueue[completed].slot;
			g_veneziaDebugger.captureQueue[i].mode =
				g_veneziaDebugger.captureQueue[completed].mode;
		}
		completed++;
	}
	return 0;
}

/* name unknown: choose the nearest valid reader cursor ahead in a circular trace buffer. */
int getVeneziaDebugNextTraceCursor(unsigned int registers, uint32_t *next,
				 unsigned int *distance)
{
	uint32_t candidates[3];
	uint32_t capacity, current, wrap, nearest = ~0u, selected = ~0u;
	unsigned int delta;
	int i, ret = 1;

	capacity = readVeneziaDebugSelectedWord(registers + VENEZIA_DEBUG_RING_CAPACITY);
	if (!(uint8_t)readVeneziaDebugSelectedBits(registers + VENEZIA_DEBUG_RING_CURSOR_VALID,
		24, 255))
		goto out;
	candidates[0] = candidates[1] = candidates[2] = ~0u;
	current = g_veneziaDebugger.trace.bufferVirtualBase - g_veneziaDebugger.trace.bufferPhysicalBase +
		readVeneziaDebugSelectedWord(registers + VENEZIA_DEBUG_RING_CURSOR);
	if (readVeneziaDebugSelectedBits(registers + VENEZIA_DEBUG_RING_READER_VALID, 24, 255) << 24)
		candidates[0] = readVeneziaDebugSelectedWord(registers + VENEZIA_DEBUG_RING_READER_CURSOR) +
			g_veneziaDebugger.trace.bufferVirtualBase - g_veneziaDebugger.trace.bufferPhysicalBase;
	if (readVeneziaDebugSelectedBits(registers + VENEZIA_DEBUG_RING_READER_VALID +
		VENEZIA_DEBUG_RING_READER_STRIDE, 24, 255) << 24)
		candidates[1] = readVeneziaDebugSelectedWord(registers + VENEZIA_DEBUG_RING_READER_CURSOR +
			VENEZIA_DEBUG_RING_READER_STRIDE) + g_veneziaDebugger.trace.bufferVirtualBase -
			g_veneziaDebugger.trace.bufferPhysicalBase;
	if (readVeneziaDebugSelectedBits(registers + VENEZIA_DEBUG_RING_READER_VALID +
		2 * VENEZIA_DEBUG_RING_READER_STRIDE, 24, 255) << 24)
		candidates[2] = readVeneziaDebugSelectedWord(registers + VENEZIA_DEBUG_RING_READER_CURSOR +
			2 * VENEZIA_DEBUG_RING_READER_STRIDE) + g_veneziaDebugger.trace.bufferVirtualBase -
			g_veneziaDebugger.trace.bufferPhysicalBase;
	wrap = capacity - current;
	for (i = 0; i != 3; i++) {
		if (candidates[i] != ~0u) {
			delta = current < candidates[i] ? candidates[i] - current : wrap + candidates[i];
			if (nearest > delta) {
				nearest = delta;
				selected = candidates[i];
			}
		}
	}
	if (selected == ~0u)
		goto out;
	*next = selected;
	*distance = nearest;
	ret = 0;
out:
	return ret;
}

/* name unknown: zero capture status invalidates the cached debugger state. */
__attribute__((noinline))
int checkVeneziaDebugCaptureValidity(uintptr_t capture)
{
	enum { VENEZIA_DEBUG_CAPTURE_STATE = 24 };
	const volatile uint32_t *status =
		(const volatile uint32_t *)((capture + VENEZIA_DEBUG_CAPTURE_STATE) ^ 4);

	if (*status & 0x00ff0000)
		return 0;
	g_veneziaDebugger.captureInvalid = 1;
	return 11;
}

static inline __attribute__((always_inline)) int getVeneziaDebugQueuedCaptureMode(void)
{
	int mode = g_veneziaDebugger.captureQueue[0].mode;

	if (g_veneziaDebugger.captureQueue[1].slot != -1) {
		mode = g_veneziaDebugger.captureQueue[1].mode;
		if (g_veneziaDebugger.captureQueue[2].slot != -1) {
			mode = g_veneziaDebugger.captureQueue[2].mode;
			if (g_veneziaDebugger.captureQueue[3].slot != -1)
				mode = g_veneziaDebugger.captureQueue[3].mode;
		}
	}
	return mode;
}

static inline __attribute__((always_inline)) int readVeneziaDebugCaptureFlag(unsigned int channel)
{
	if (g_veneziaDebugger.captureQueue[0].slot == -1)
		return (readVeneziaDebugChannelWord((channel << 9) + 256) & 0x80) != 0;
	return getVeneziaDebugQueuedCaptureMode() == 1;
}

/* name unknown: read the pending capture flag, falling back to the channel hardware. */
__attribute__((noinline)) int getVeneziaDebugCaptureFlag(unsigned int channel)
{
	return readVeneziaDebugCaptureFlag(channel);
}

/* name unknown: test whether the pending flag matches a set(1) or clear(2) request. */
unsigned int testVeneziaDebugCaptureMode(unsigned int channel, unsigned int mode)
{
	uint8_t flags;

	if (mode - 1 > 1)
		return 0;
	if (g_veneziaDebugger.captureQueue[0].slot == -1) {
		flags = readVeneziaDebugChannelWord((channel << 9) + 256);
		if ((mode == 1) & (flags >> 7))
			return 1;
		return (mode == 2) & ((unsigned int)~flags >> 7);
	}
	return mode == getVeneziaDebugQueuedCaptureMode();
}

static inline __attribute__((always_inline)) uint32_t readVeneziaDebugSwizzledWord(uintptr_t address)
{
	return *(const volatile uint32_t *)(address ^ 4);
}

/* name unknown: retrieve the context address from the pending trace packet. */
int getVeneziaDebugPendingTraceContext(unsigned int channel, void **context)
{
	unsigned int registers = (channel + VENEZIA_DEBUG_TRACE_RING_FIRST) *
		sizeof(VeneziaDebugRingConfig);
	uint32_t current, pending;

	if (!(readVeneziaDebugSelectedBits(registers + VENEZIA_DEBUG_RING_READER_VALID, 24, 255) << 24))
		return 10;
	current = g_veneziaDebugger.memoryVirtualBase - g_veneziaDebugger.memoryPhysicalBase +
		readVeneziaDebugSelectedWord(registers + VENEZIA_DEBUG_RING_CURSOR);
	pending = readVeneziaDebugSelectedWord(registers + VENEZIA_DEBUG_RING_READER_CURSOR) +
		g_veneziaDebugger.memoryVirtualBase - g_veneziaDebugger.memoryPhysicalBase;
	if (current == pending)
		return 3;
	*context = (void *)(g_veneziaDebugger.memoryVirtualBase - g_veneziaDebugger.memoryPhysicalBase +
		readVeneziaDebugSwizzledWord(pending + offsetof(VeneziaDebugTracePacket, context)));
	return 0;
}

/* name unknown: retrieve the three optional words in the pending trace header. */
int getVeneziaDebugPendingTraceHeader(unsigned int channel, uint32_t *first,
				    uint32_t *second, uint32_t *third)
{
	unsigned int registers = (channel + VENEZIA_DEBUG_TRACE_RING_FIRST) *
		sizeof(VeneziaDebugRingConfig);
	uint32_t current, pending;

	if (!(readVeneziaDebugSelectedBits(registers + VENEZIA_DEBUG_RING_READER_VALID, 24, 255) << 24))
		return 4;
	current = g_veneziaDebugger.memoryVirtualBase - g_veneziaDebugger.memoryPhysicalBase +
		readVeneziaDebugSelectedWord(registers + VENEZIA_DEBUG_RING_CURSOR);
	pending = readVeneziaDebugSelectedWord(registers + VENEZIA_DEBUG_RING_READER_CURSOR) +
		g_veneziaDebugger.memoryVirtualBase - g_veneziaDebugger.memoryPhysicalBase;
	if (current == pending)
		return 3;
	if (first)
		*first = readVeneziaDebugSwizzledWord(pending + offsetof(VeneziaDebugTracePacket, header[0]));
	if (second)
		*second = readVeneziaDebugSwizzledWord(pending + offsetof(VeneziaDebugTracePacket, header[1]));
	if (third)
		*third = readVeneziaDebugSwizzledWord(pending + offsetof(VeneziaDebugTracePacket, header[2]));
	return 0;
}

/* name unknown: initialize the ring's four cursors at a shared buffer base. */
int initVeneziaDebugTraceRing(unsigned int ring, uint32_t buffer, unsigned int capacity,
			    uint8_t enabledCursors)
{
	VeneziaDebugRingWork work;
	unsigned int i;

	for (i = 0; i != 16; i++)
		work.words[i] = 0;
	if (buffer)
		buffer += g_veneziaDebugger.memoryPhysicalBase;
	work.config.base = buffer;
	work.config.capacity = capacity;
	work.config.cursors[0].address = buffer;
	work.config.cursors[1].address = buffer;
	work.config.cursors[2].address = buffer;
	work.config.cursors[3].address = buffer;
	work.config.control = TAS_SIGNAL_PADDR(3);
	if (buffer) {
		if (enabledCursors & 1)
			work.config.cursors[0].control |= 0x01000000;
		if (enabledCursors & 2)
			work.config.cursors[1].control |= 0x01000000;
		if (enabledCursors & 4)
			work.config.cursors[2].control |= 0x01000000;
		if (enabledCursors & 8)
			work.config.cursors[3].control |= 0x01000000;
	}
	writeVeneziaDebugRingConfig(ring * sizeof(VeneziaDebugRingConfig), &work.config);
	return 0;
}

/* name unknown: validate DSP0's debugger mode and obtain the current context. */
int getVeneziaDebugCurrentContext(unsigned int processor, void **context)
{
	unsigned int mode, alternate;

	if (processor)
		return 9;
	if (!g_veneziaDebugger.callbacks.checkReset || g_veneziaDebugger.callbacks.checkReset() != 0)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode != alternate) {
		if (mode - 1 <= 1 && alternate - 1 <= 1) {
			g_veneziaDebugger.mode = 2;
			goto getContext;
		}
	} else if (mode - 1 <= 4) {
		g_veneziaDebugger.mode = mode;
		goto checkMode;
	}
	mode = g_veneziaDebugger.mode;
checkMode:
	if (mode - 1 > 1)
		return 5;
getContext:
	if (!readVeneziaDebugCaptureFlag(0))
		return 3;
	return getVeneziaDebugPendingTraceContext(0, context);
}

/* name unknown: bind the host callbacks and initialize the debugger's shared memory. */
int initVeneziaDebugger(const VeneziaDebugCallbacks *callbacks, uint32_t virtualBase,
		       uint32_t physicalBase, SceSize size)
{
	unsigned int i;

	if (!virtualBase || !physicalBase || ((virtualBase | physicalBase) & 7) ||
	    size != 0x100000 || !callbacks || !callbacks->lock || !callbacks->unlock ||
	    !callbacks->readMailbox || !callbacks->writeMailbox || !callbacks->signalMailbox ||
	    !callbacks->clearReset || !callbacks->setReset || !callbacks->checkReset)
		return 9;
	if (g_veneziaDebugger.callbacks.checkReset &&
	    g_veneziaDebugger.callbacks.checkReset() == 0)
		return 6;
	g_veneziaDebugger.callbacks.setReset = callbacks->setReset;
	g_veneziaDebugger.callbacks.unlock = callbacks->unlock;
	g_veneziaDebugger.callbacks.readMailbox = callbacks->readMailbox;
	g_veneziaDebugger.callbacks.writeMailbox = callbacks->writeMailbox;
	g_veneziaDebugger.callbacks.signalMailbox = callbacks->signalMailbox;
	g_veneziaDebugger.callbacks.clearReset = callbacks->clearReset;
	g_veneziaDebugger.callbacks.checkReset = callbacks->checkReset;
	g_veneziaDebugger.callbacks.lock = callbacks->lock;
	g_veneziaDebugger.memoryVirtualBase = virtualBase;
	g_veneziaDebugger.memoryPhysicalBase = physicalBase;
	g_veneziaDebugger.memorySize = 0x100000;
	for (i = 0; i != 4; i++) {
		g_veneziaDebugger.captureQueue[i].slot = -1;
		g_veneziaDebugger.captureQueue[i].mode = 0;
		g_veneziaDebugger.captureSlotBusy[i] = 0;
	}
	memset(&g_veneziaDebugger.captureConfig[0], 0, sizeof(g_veneziaDebugger.captureConfig[0]));
	g_veneziaDebugger.captureControl[0] = 0;
	g_veneziaDebugger.captureGroup[0] = -1;
	g_veneziaDebugger.auxiliaryTraceEnabled[0] = 0;
	memset(&g_veneziaDebugger.captureConfig[1], 0, sizeof(g_veneziaDebugger.captureConfig[1]));
	g_veneziaDebugger.captureControl[1] = 0;
	g_veneziaDebugger.captureGroup[1] = -1;
	g_veneziaDebugger.auxiliaryTraceEnabled[1] = 0;
	g_veneziaDebugger.trace.flags = 0;
	g_veneziaDebugger.trace.imageVirtualBase = 0;
	g_veneziaDebugger.trace.imagePhysicalBase = 0;
	g_veneziaDebugger.trace.imageSize = 0;
	g_veneziaDebugger.trace.bufferVirtualBase = 0;
	g_veneziaDebugger.trace.bufferPhysicalBase = 0;
	g_veneziaDebugger.trace.bufferSize = 0;
	g_veneziaDebugger.trace.auxiliaryVirtualBase = 0;
	g_veneziaDebugger.trace.auxiliaryPhysicalBase = 0;
	g_veneziaDebugger.trace.auxiliarySize = 0;
	g_veneziaDebugger.selectedBank = 0;
	g_veneziaDebugger.mode = 0;
	g_veneziaDebugger.captureInvalid = 0;
	for (i = 0; i != 4; i++) {
		g_veneziaDebugger.captureQueue[i].slot = -1;
		g_veneziaDebugger.captureQueue[i].mode = 0;
		g_veneziaDebugger.captureSlotBusy[i] = 0;
	}
	for (i = 0xbf000; i != 0xd88b0; i += 4)
		writeVeneziaDebugMemoryWord(i, 0);
	writeVeneziaDebugBankRegister(52, TAS_SIGNAL_PADDR(0));
	writeVeneziaDebugBankRegister(116, TAS_SIGNAL_PADDR(0));
	writeVeneziaDebugRegister(8, TAS_SIGNAL_PADDR(2));
	writeVeneziaDebugRegister(712, TAS_SIGNAL_PADDR(5));
	writeVeneziaDebugRegister(708, ~0u);
	writeVeneziaDebugRegister(40, 0x80000);
	writeVeneziaDebugRegister(316, 34);
	writeVeneziaDebugRegister(328, 1081472);
	writeVeneziaDebugRegister(332, 1);
	writeVeneziaDebugRegister(340, 0);
	writeVeneziaDebugRegister(344, 0);
	writeVeneziaDebugRegister(828, 34);
	writeVeneziaDebugRegister(840, 1081472);
	writeVeneziaDebugRegister(844, 1);
	writeVeneziaDebugRegister(852, 0);
	writeVeneziaDebugRegister(856, 0);
	g_veneziaDebugger.callbacks.setReset();
	return 0;
}

/* name unknown: request a debugger mode and notify both DSP mailbox channels. */
__attribute__((noinline)) int setVeneziaDebuggerMode(unsigned int request)
{
	unsigned int mode, alternate, bit;

	if (request > 5)
		return 9;
	if (!g_veneziaDebugger.memoryVirtualBase)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode == alternate) {
		if (mode - 1 <= 4)
			g_veneziaDebugger.mode = mode;
	} else if (mode - 1 <= 1) {
		if (alternate - 1 <= 1)
			g_veneziaDebugger.mode = 2;
	} else if (alternate - 1 > 1) {
		return 7;
	}
	if (mode != 3 && mode != 5 &&
	    (!g_veneziaDebugger.callbacks.checkReset ||
	     g_veneziaDebugger.callbacks.checkReset() != 0))
		return 8;
	switch (request) {
	case 1:
		if (mode == 4)
			return 6;
		if (mode - 1 > 1)
			return 7;
		goto writeMode;
	case 2:
		if (mode != 4)
			return 7;
		goto signalChannels;
	case 3:
		if (mode == 5)
			return 6;
		if (mode != 4 && mode - 1 > 1)
			return 7;
		goto signalChannels;
	case 4:
		if (mode != 5 || (g_veneziaDebugger.callbacks.checkReset &&
		    g_veneziaDebugger.callbacks.checkReset() == 0))
			return 7;
		goto writeMode;
	case 5:
		if (mode != 5 || !g_veneziaDebugger.callbacks.checkReset ||
		    g_veneziaDebugger.callbacks.checkReset() != 0)
			return 7;
		goto signalChannels;
	default:
		if (mode == 3)
			return 6;
		if (mode - 4 > 1 && mode - 1 > 1)
			return 7;
		if (mode == 5 && (!g_veneziaDebugger.callbacks.checkReset ||
		    g_veneziaDebugger.callbacks.checkReset() != 0)) {
			writeVeneziaDebugStatusWord(0, 3);
			writeVeneziaDebugStatusWord(32, 3);
			g_veneziaDebugger.mode = 3;
			return 0;
		}
		writeVeneziaDebugStatusWord(8, request + 1);
		writeVeneziaDebugStatusWord(40, request + 1);
		bit = 1u << request;
		if (bit & 0x2d)
			goto notifyChannels;
		goto checkReset;
	}
writeMode:
	writeVeneziaDebugStatusWord(8, request + 1);
	writeVeneziaDebugStatusWord(40, request + 1);
	bit = 1u << request;
checkReset:
	if (bit & 0x10) {
		g_veneziaDebugger.callbacks.setReset();
		return 0;
	}
	return bit & 0x10;
signalChannels:
	writeVeneziaDebugStatusWord(8, request + 1);
	writeVeneziaDebugStatusWord(40, request + 1);
notifyChannels:
	g_veneziaDebugger.callbacks.signalMailbox(0, 1);
	g_veneziaDebugger.callbacks.signalMailbox(1, 1);
	return 0;
}

/* name unknown: read the paired DSP mode registers and update the cached mode. */
__attribute__((noinline)) int getVeneziaDebuggerModes(unsigned int *first, unsigned int *second)
{
	unsigned int mode, alternate;

	if (!second || !first)
		return 9;
	if (!g_veneziaDebugger.memoryVirtualBase)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode == alternate) {
		if (mode - 1 <= 4)
			g_veneziaDebugger.mode = mode;
	} else if (mode - 1 <= 1 && alternate - 1 <= 1) {
		g_veneziaDebugger.mode = 2;
	}
	*first = mode;
	*second = alternate;
	return 0;
}

/* name unknown: four fixed areas precede the DSP's terminated region list. */
int getVeneziaDebugMemoryRegionCount(unsigned int *count)
{
	VeneziaDebugMemoryHeader *memory;
	unsigned int mode, alternate, regions;

	if (!count)
		return 9;
	if (!g_veneziaDebugger.memoryVirtualBase)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode != alternate) {
		if (mode - 1 <= 1 && alternate - 1 <= 1)
			g_veneziaDebugger.mode = 2;
		return 7;
	}
	if (mode - 1 <= 4)
		g_veneziaDebugger.mode = mode;
	else if (!mode)
		return 7;
	if (mode == 3)
		return 7;
	memory = (VeneziaDebugMemoryHeader *)(uintptr_t)g_veneziaDebugger.memoryVirtualBase;
	for (regions = 0; memory->regions[regions].size; regions++)
		;
	*count = regions + 4;
	return 0;
}

/* name unknown: expose fixed debugger buffers and DSP-described memory regions. */
int getVeneziaDebugMemoryRegion(unsigned int region, uint32_t *physical,
			       uint32_t *virtual, SceSize *size)
{
	VeneziaDebugMemoryHeader *memory;
	VeneziaDebugMemoryRegion *entry;
	unsigned int mode, alternate, index;
	uint32_t offset;
	SceSize length;

	if (!virtual || !physical || !size)
		return 9;
	if (!g_veneziaDebugger.memoryVirtualBase)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode != alternate) {
		if (mode - 1 <= 1 && alternate - 1 <= 1)
			g_veneziaDebugger.mode = 2;
		return 7;
	}
	if (mode - 1 <= 4)
		g_veneziaDebugger.mode = mode;
	else if (!mode)
		return 7;
	if (mode == 3)
		return 7;
	switch (region) {
	case 0:
		*physical = g_veneziaDebugger.memoryPhysicalBase + 0xbf000;
		*virtual = g_veneziaDebugger.memoryVirtualBase + 0xbf000;
		*size = 104624;
		return 0;
	case 1:
		*physical = g_veneziaDebugger.memoryPhysicalBase + 888832;
		*virtual = g_veneziaDebugger.memoryVirtualBase + 888832;
		*size = 512;
		return 0;
	case 2:
		*physical = g_veneziaDebugger.memoryPhysicalBase + 889856;
		*virtual = g_veneziaDebugger.memoryVirtualBase + 889856;
		*size = 512;
		return 0;
	case 3:
		*physical = g_veneziaDebugger.memoryPhysicalBase + 892928;
		*virtual = g_veneziaDebugger.memoryVirtualBase + 892928;
		*size = 0x8000;
		return 0;
	default:
		memory = (VeneziaDebugMemoryHeader *)(uintptr_t)g_veneziaDebugger.memoryVirtualBase;
		entry = memory->regions;
		length = entry->size;
		if (!length)
			return 9;
		if (region != 4) {
			index = 4;
			do {
				entry++;
				length = entry->size;
				index++;
				if (!length)
					return 9;
			} while (region != index);
		}
		offset = entry->offset;
		if (offset & 7) {
			length += entry->offset & 7;
			offset &= 0xffff8;
		} else {
			offset &= 0xfffff;
		}
		*physical = g_veneziaDebugger.memoryPhysicalBase + offset;
		*virtual = g_veneziaDebugger.memoryVirtualBase + offset;
		*size = (length + 7) & ~7u;
		return 0;
	}
}

/* name unknown: this debugger exposes one DSP processor. */
int getVeneziaDebugProcessorCount(unsigned int *count)
{
	unsigned int mode, alternate;

	if (!count)
		return 9;
	if (!g_veneziaDebugger.memoryVirtualBase)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode != alternate) {
		if (mode - 1 <= 1 && alternate - 1 <= 1)
			g_veneziaDebugger.mode = 2;
		return 7;
	}
	if (mode - 1 > 4) {
		if (!mode)
			return 7;
	} else {
		g_veneziaDebugger.mode = mode;
	}
	if (mode == 3)
		return 7;
	*count = 1;
	return 0;
}

/* name unknown: the debugger's externally exposed state excludes lock and logger. */
int getVeneziaDebuggerState(unsigned int processor, VeneziaDebuggerState **state,
			   SceSize *size)
{
	unsigned int mode, alternate;

	if (!size || !state || processor)
		return 9;
	if (!g_veneziaDebugger.memoryVirtualBase)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode != alternate) {
		if (mode - 1 <= 1 && alternate - 1 <= 1)
			g_veneziaDebugger.mode = 2;
		return 7;
	}
	if (mode - 1 <= 4)
		g_veneziaDebugger.mode = mode;
	else if (!mode)
		return 7;
	if (mode == 3)
		return 7;
	*state = &g_veneziaDebugger;
	*size = offsetof(VeneziaDebuggerState, spinlock);
	return 0;
}

/* name unknown: read the DSP0 debugger's host mailbox callback. */
__attribute__((noinline))
int readVeneziaDebuggerMailbox(unsigned int processor, uint32_t *value)
{
	if (processor || !value)
		return 9;
	if (!g_veneziaDebugger.callbacks.checkReset ||
	    g_veneziaDebugger.callbacks.checkReset() != 0)
		return 8;
	return g_veneziaDebugger.callbacks.readMailbox(0, value);
}

/* name unknown: write the DSP0 debugger's host mailbox callback. */
__attribute__((noinline))
int writeVeneziaDebuggerMailbox(unsigned int processor, uint32_t value)
{
	if (processor)
		return 9;
	if (!g_veneziaDebugger.callbacks.checkReset ||
	    g_veneziaDebugger.callbacks.checkReset() != 0)
		return 8;
	return g_veneziaDebugger.callbacks.writeMailbox(0, value);
}

/* name unknown: serialize updates to the debugger event-mask register. */
int setVeneziaDebuggerEventMask(unsigned int processor, uint32_t mask)
{
	if (processor)
		return 9;
	if (!g_veneziaDebugger.callbacks.checkReset ||
	    g_veneziaDebugger.callbacks.checkReset() != 0)
		return 8;
	if (g_veneziaDebugger.callbacks.lock(5) != 0)
		return 10;
	writeVeneziaDebugRegister(708, mask);
	g_veneziaDebugger.callbacks.unlock(5);
	return 0;
}

/* name unknown: execute a three-argument DSP debugger command in a capture slot. */
__attribute__((noinline)) int executeVeneziaDebuggerCommand(unsigned int processor, uint32_t first,
				 uint32_t second, uint32_t third, uint32_t *result)
{
	VeneziaDebugExecutionWork work;
	unsigned int mode, alternate, slot, destination, command, i, completed;
	uint32_t lowerMask, upperMask, status;
	int last;

	if (processor || !result)
		return 9;
	if (!g_veneziaDebugger.callbacks.checkReset ||
	    g_veneziaDebugger.callbacks.checkReset() != 0)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode == alternate) {
		if (mode - 1 <= 4) {
			g_veneziaDebugger.mode = mode;
			goto checkMode;
		}
	} else if (mode - 1 <= 1 && alternate - 1 <= 1) {
		g_veneziaDebugger.mode = 2;
		goto allocateSlot;
	}
	mode = g_veneziaDebugger.mode;
checkMode:
	if (mode - 1 > 1)
		return 5;
allocateSlot:
	if (readVeneziaDebugCaptureFlag(0))
		return 6;
	for (slot = 0; slot != 4; slot++)
		if (!g_veneziaDebugger.captureSlotBusy[slot])
			break;
	if (slot == 4)
		return 10;
	g_veneziaDebugger.captureSlotBusy[slot] = 1;
	memset(&work, 0, 32);
	destination = (slot + 3057) << 8;
	work.capture.selectionMask[1] = 1;
	work.capture.destination = g_veneziaDebugger.memoryPhysicalBase + destination;
	work.capture.command = 1;
	copyVeneziaDebugCommand(32 * slot, &work, 32);
	command = (slot << 12) + 3328;
	writeVeneziaDebugCommandWord(destination - 782208,
		g_veneziaDebugger.memoryPhysicalBase + (slot << 12) + 785664);
	for (i = 0; i != 16; i++)
		work.words[i] = 0;
	work.arguments[0] = first;
	work.arguments[1] = second;
	work.arguments[2] = third;
	work.command |= 0x144000;
	work.mode |= 0x3020000;
	copyVeneziaDebugCommand(command, &work, 64);
	if (drainVeneziaDebugCaptureQueue() != 0 ||
	    g_veneziaDebugger.captureQueue[0].slot != -1)
		return 10;
	writeVeneziaDebugRegister(12, 32 * slot + 782336 +
		g_veneziaDebugger.memoryPhysicalBase);
	g_veneziaDebugger.captureQueue[0].slot = slot;
	g_veneziaDebugger.captureQueue[0].mode = 3;
	for (;;) {
		last = -1;
		for (i = 0; i != 4 && g_veneziaDebugger.captureQueue[i].slot != -1; i++)
			last = i;
		for (; i != 4; i++)
			if (g_veneziaDebugger.captureQueue[i].slot != -1)
				return 9;
		if (last == -1 || g_veneziaDebugger.captureQueue[last].mode != 3)
			return 9;
		completed = 32 * g_veneziaDebugger.captureQueue[last].slot;
		lowerMask = readVeneziaDebugCommandWord(completed +
			offsetof(VeneziaDebugCaptureRequest, selectionMask[0]));
		upperMask = readVeneziaDebugCommandWord(completed +
			offsetof(VeneziaDebugCaptureRequest, selectionMask[1]));
		if (!(lowerMask | upperMask)) {
			status = readVeneziaDebugCommandBits(command, 0, 255);
			if (status) {
				if (status != 1)
					return 9;
				*result = readVeneziaDebugCommandWord(command +
					offsetof(VeneziaDebugExecutionWork, result));
				return 0;
			}
		}
	}
}

/* name unknown: configure trace buffers and enqueue a capture-bank transition. */
int startVeneziaDebugCapture(unsigned int processor,
	uint32_t imageVirtual, uint32_t imagePhysical, SceSize imageSize,
	uint32_t bufferVirtual, uint32_t bufferPhysical, SceSize bufferSize,
	uint32_t scratchVirtual, uint32_t scratchPhysical, SceSize scratchSize,
	uint32_t auxiliaryVirtual, uint32_t auxiliaryPhysical, SceSize auxiliarySize,
	const VeneziaDebugCaptureConfig *config)
{
	VeneziaDebugCaptureCommandWork work;
	VeneziaDebuggerState *state = &g_veneziaDebugger;
	VeneziaDebugCaptureQueueEntry previous;
	unsigned int mode, alternate, slot, i, index, registers, commandAddress;
	uint32_t imageAddress, firstCommand, secondCommand, secondAddress, pending;
	uint32_t *indices;
	uint16_t control;
	int ret, oldSlot, matches;
	unsigned char *destination;
	const unsigned char *source;

	(void)scratchVirtual;
	if (processor)
		return 9;
	if (!state->callbacks.checkReset || state->callbacks.checkReset() != 0)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode == alternate) {
		if (mode - 1 <= 4) {
			state->mode = mode;
			goto checkMode;
		}
	} else if (mode - 1 <= 1 && alternate - 1 <= 1) {
		state->mode = 2;
		goto validate;
	}
	mode = state->mode;
checkMode:
	if (mode - 1 > 1)
		return 5;
validate:
	if (readVeneziaDebugCaptureFlag(0))
		return 6;
	if ((imagePhysical & 0xfffff) || imageSize < 0x40000 || (imageSize & 0x3ffff) ||
	    !bufferPhysical || (bufferPhysical & 7) || bufferSize <= 15 || (bufferSize & 7) ||
	    !scratchPhysical || (scratchPhysical & 7) || scratchSize <= 15 || (scratchSize & 7) ||
	    ((auxiliaryPhysical | auxiliarySize) & 7) || !config)
		return 9;
	control = config->control;
	if ((control & 0xc000) != 0x8000 ||
	    ((config->control & 0x200) && (control & 0x300) != 0x300) ||
	    ((control & 0x70) != 0x70 && (control & 0x70) != 0x40) || (control & 15) != 2 ||
	    (int)config->startOffset <= 768 || (int)config->startOffset >= (int)config->size ||
	    config->size >= bufferSize || config->format != 17 || (config->options & ~3u))
		return 9;
	if (auxiliaryPhysical) {
		if (!auxiliarySize)
			return 9;
		state->auxiliaryTraceEnabled[0] = 1;
	} else {
		auxiliarySize = 0;
		state->auxiliaryTraceEnabled[0] = 0;
	}
	if (!(control & 0x400) && (readVeneziaDebugChannelBits(276, 16, 0xffff) & 0x8000))
		return 7;
	if (testVeneziaDebugCaptureMode(0, 1))
		return 6;
	for (slot = 0; slot != 4; slot++)
		if (!state->captureSlotBusy[slot])
			break;
	if (slot == 4)
		return 10;
	state->captureControl[0] = config->control;
	state->captureSlotBusy[slot] = 1;
	writeVeneziaDebugRegister(268, config->control);
	writeVeneziaDebugRegister(272, config->format);
	writeVeneziaDebugRegister(280, config->options);
	writeVeneziaDebugRegister(1832, config->startOffset);
	writeVeneziaDebugRegister(1836, config->size);
	if (state->callbacks.lock(2))
		return 10;
	writeVeneziaDebugRegister(288, 0x80000000);
	writeVeneziaDebugRegister(292, 0);
	writeVeneziaDebugRegister(320, 0x80000000);
	writeVeneziaDebugRegister(324, 0);
	state->callbacks.unlock(2);
	if (state->callbacks.lock(0))
		return 10;
	writeVeneziaDebugBankRegister(0, 0x80000000);
	writeVeneziaDebugBankRegister(4, 0);
	writeVeneziaDebugBankRegister(8, 0);
	writeVeneziaDebugBankRegister(12, 0x80000000);
	writeVeneziaDebugBankRegister(16, 0);
	writeVeneziaDebugBankRegister(20, 0);
	writeVeneziaDebugBankRegister(36, 0x80000000);
	writeVeneziaDebugBankRegister(40, 0);
	writeVeneziaDebugBankRegister(44, 0);
	state->callbacks.unlock(0);
	switchVeneziaDebugBank();
	initVeneziaDebugTraceRing(24, 889856, 512, 2);
	initVeneziaDebugTraceRing(16, 909312, 0x4000, 0);
	initVeneziaDebugTraceRing(28, 888832, 512, 2);
	initVeneziaDebugTraceRing(12, auxiliaryPhysical - state->memoryPhysicalBase, auxiliarySize, 2);
	state->captureGroup[0] = 2;
	initVeneziaDebugTraceRing(0, bufferPhysical - state->memoryPhysicalBase, bufferSize, 1);
	initVeneziaDebugTraceRing(4, scratchPhysical - state->memoryPhysicalBase, scratchSize >> 1, 0);
	initVeneziaDebugTraceRing(8, scratchPhysical + (scratchSize >> 1) - state->memoryPhysicalBase,
		scratchSize - (scratchSize >> 1), 0);
	initVeneziaDebugTraceRing(20, 892928, 0x4000, 0);
	ret = initVeneziaDebugCaptureRequest(slot, state->captureGroup[0]);
	if (ret)
		goto restoreBank;
	ret = initVeneziaDebugCaptureAddresses(slot, state->captureGroup[0]);
	if (ret)
		goto restoreBank;
	imageAddress = imagePhysical ? ((imagePhysical & 0x1fffe000) >> 10) |
		((imagePhysical & 0x1ff8) << 16) | (imagePhysical & 0xe0000007) : 0;
	indices = g_veneziaDebugCaptureIndices[state->captureGroup[0]];
	for (i = 0; indices[i] != ~0u; i++) {
		index = indices[i];
		for (registers = 0; registers != 16; registers++)
			work.words[registers] = 0;
		work.command |= 0x4000;
		work.mode |= 0x1010000;
		registers = g_veneziaDebugBankOffsets[state->selectedBank];
		if (index == 1) {
			work.command |= 0x184000;
			work.addresses[0] = state->memoryPhysicalBase + 256 + registers;
			work.addresses[1] = state->memoryPhysicalBase + 1024 + registers;
			work.addresses[2] = state->memoryPhysicalBase + 512 + registers;
			work.addresses[3] = state->memoryPhysicalBase + 1280 + registers;
		} else if (index) {
			if (index != 32)
				goto restoreBank;
			work.command |= 0x344000;
			work.imageAddress = imageAddress;
			work.imageSize = imageSize;
			work.addresses[0] = state->memoryPhysicalBase + 512 + registers;
			work.addresses[1] = state->memoryPhysicalBase + 1280 + registers;
			work.firmwareSize = 51376;
			work.addresses[2] = state->memoryPhysicalBase + 768 + registers;
			work.addresses[3] = state->memoryPhysicalBase + 1536 + registers;
			work.mapSize = 0x2000;
			work.addresses[4] = state->memoryPhysicalBase + 1792 + registers;
			work.mapBase = state->memoryPhysicalBase + 0x2000 + registers;
			work.firmwareBase = state->memoryPhysicalBase + 835584;
		} else {
			work.command |= 0x144000;
			work.addresses[0] = state->memoryPhysicalBase + registers;
			work.addresses[1] = state->memoryPhysicalBase + 256 + registers;
			work.addresses[2] = state->memoryPhysicalBase + 1024 + registers;
		}
		copyVeneziaDebugCommand(((slot << 6) + 20 + index) << 6, &work, 64);
	}
	ret = drainVeneziaDebugCaptureQueue();
	if (ret || state->captureQueue[2].slot != -1 || state->captureQueue[3].slot != -1)
		goto restoreBank;
	for (i = 0; i != 4 && state->captureQueue[i].slot != -1; i++)
		;
	for (; i != 4; i++)
		if (state->captureQueue[i].slot != -1)
			goto restoreBank;
	commandAddress = state->memoryPhysicalBase + 32 * (slot + 24448);
	if (state->captureQueue[0].slot == -1) {
		writeVeneziaDebugRegister(12, commandAddress);
		state->captureQueue[0].slot = slot;
		state->captureQueue[0].mode = 1;
	} else if (state->captureQueue[1].slot == -1) {
		if (state->captureQueue[0].mode != 2)
			goto restoreBank;
		writeVeneziaDebugCommandWord(32 * state->captureQueue[0].slot +
			offsetof(VeneziaDebugCaptureRequest, arguments[1]), commandAddress);
		state->captureQueue[1].slot = slot;
		state->captureQueue[1].mode = 1;
	} else {
		if (state->captureQueue[0].mode != 1 || state->captureQueue[1].mode != 2)
			goto restoreBank;
		matches = state->captureConfig[0].size == config->size &&
			state->captureConfig[0].startOffset == config->startOffset &&
			state->captureConfig[0].control == config->control &&
			state->captureConfig[0].format == config->format &&
			state->captureConfig[0].options == config->options;
		firstCommand = 32 * state->captureQueue[0].slot;
		secondCommand = 32 * state->captureQueue[1].slot;
		secondAddress = secondCommand + 782336 + state->memoryPhysicalBase;
		ret = state->callbacks.lock(2);
		if (ret)
			goto restoreBank;
		pending = readVeneziaDebugCommandWord(firstCommand +
			offsetof(VeneziaDebugCaptureRequest, selectionMask[0]));
		pending |= readVeneziaDebugCommandWord(firstCommand +
			offsetof(VeneziaDebugCaptureRequest, selectionMask[1]));
		if (pending) {
			pending = readVeneziaDebugCommandWord(firstCommand +
				offsetof(VeneziaDebugCaptureRequest, parameters[0]));
			pending |= readVeneziaDebugCommandWord(firstCommand +
				offsetof(VeneziaDebugCaptureRequest, parameters[1]));
			if (pending) {
				if (matches) {
					writeVeneziaDebugCommandWord(firstCommand +
						offsetof(VeneziaDebugCaptureRequest, arguments[1]), 0);
					state->callbacks.unlock(2);
					oldSlot = state->captureQueue[1].slot;
					state->captureQueue[1].slot = -1;
					state->captureQueue[1].mode = 0;
					state->captureSlotBusy[slot] = 0;
					state->captureSlotBusy[oldSlot] = 0;
				} else {
					writeVeneziaDebugCommandWord(secondCommand +
						offsetof(VeneziaDebugCaptureRequest, arguments[1]), commandAddress);
					state->callbacks.unlock(2);
					state->captureQueue[2].slot = slot;
					state->captureQueue[2].mode = 1;
				}
			} else {
				writeVeneziaDebugRegister(12, commandAddress);
				state->callbacks.unlock(2);
				state->captureQueue[1].mode = 0;
				oldSlot = state->captureQueue[0].slot;
				state->captureQueue[0].slot = slot;
				state->captureQueue[0].mode = 1;
				state->captureSlotBusy[oldSlot] = 0;
				oldSlot = state->captureQueue[1].slot;
				state->captureQueue[1].slot = -1;
				state->captureSlotBusy[oldSlot] = 0;
			}
		} else {
			writeVeneziaDebugRegister(12, secondAddress);
			writeVeneziaDebugCommandWord(secondCommand +
				offsetof(VeneziaDebugCaptureRequest, arguments[1]), commandAddress);
			state->callbacks.unlock(2);
			previous = state->captureQueue[1];
			state->captureQueue[1].mode = 1;
			oldSlot = state->captureQueue[0].slot;
			state->captureQueue[0] = previous;
			state->captureQueue[1].slot = slot;
			state->captureSlotBusy[oldSlot] = 0;
		}
	}
	destination = (unsigned char *)&state->captureConfig[0];
	source = (const unsigned char *)config;
	for (i = 0; i != sizeof(*config); i++)
		destination[i] = source[i];
	state->trace.imageVirtualBase = imageVirtual;
	state->trace.imagePhysicalBase = imagePhysical;
	state->trace.imageSize = imageSize;
	state->trace.flags |= 1;
	state->trace.bufferVirtualBase = bufferVirtual;
	state->trace.bufferPhysicalBase = bufferPhysical;
	state->trace.bufferSize = bufferSize;
	state->trace.auxiliaryVirtualBase = auxiliaryVirtual;
	state->trace.auxiliaryPhysicalBase = auxiliaryPhysical;
	state->trace.auxiliarySize = auxiliarySize;
	return 0;
restoreBank:
	switchVeneziaDebugBank();
	return 10;
}

/* name unknown: enqueue the stop command and retire completed capture transitions. */
int stopVeneziaDebugCapture(unsigned int processor)
{
	VeneziaDebugCaptureCommandWork work;
	VeneziaDebuggerState *state = &g_veneziaDebugger;
	VeneziaDebugCaptureQueueEntry previous;
	unsigned int mode, alternate, slot, group, skip, i, j, count, target;
	uint32_t commandAddress, targetCommand, previousCommand, pending;
	uint32_t *indices;
	int oldSlot, secondSlot;

	if (processor)
		return 9;
	if (!state->callbacks.checkReset || state->callbacks.checkReset() != 0)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode == alternate) {
		if (mode - 1 <= 4) {
			state->mode = mode;
			goto checkMode;
		}
	} else if (mode - 1 <= 1 && alternate - 1 <= 1) {
		state->mode = 2;
		goto allocateSlot;
	}
	mode = state->mode;
checkMode:
	if (mode - 1 > 1)
		return 5;
allocateSlot:
	if (!readVeneziaDebugCaptureFlag(0))
		return 6;
	if (testVeneziaDebugCaptureMode(0, 2))
		return 6;
	for (slot = 0; slot != 4; slot++)
		if (!state->captureSlotBusy[slot])
			break;
	if (slot == 4)
		return 10;
	state->captureSlotBusy[slot] = 1;
	if (initVeneziaDebugCaptureRequest(slot, state->captureGroup[0]) ||
	    initVeneziaDebugCaptureAddresses(slot, state->captureGroup[0]))
		return 10;
	group = state->captureGroup[0];
	skip = 0;
	if (group == 4) {
		group = 2;
		skip = 1;
	}
	indices = &g_veneziaDebugCaptureIndices[group][skip];
	for (i = 0; indices[i] != ~0u; i++) {
		for (j = 0; j != 16; j++)
			work.words[j] = 0;
		work.command |= 0x84000;
		work.mode |= 0x2010000;
		copyVeneziaDebugCommand((indices[i] + (slot << 6) + 20) << 6, &work, 64);
	}
	if (drainVeneziaDebugCaptureQueue() || state->captureQueue[3].slot != -1)
		return 10;
	for (count = 0; count != 4 && state->captureQueue[count].slot != -1; count++)
		;
	for (i = count; i != 4; i++)
		if (state->captureQueue[i].slot != -1)
			return 10;
	commandAddress = state->memoryPhysicalBase + 32 * (slot + 24448);
	if (!count) {
		writeVeneziaDebugRegister(12, commandAddress);
		state->captureQueue[0].slot = slot;
		state->captureQueue[0].mode = 2;
	} else {
		if ((count == 1 && state->captureQueue[0].mode != 1) ||
		    (count == 2 && (state->captureQueue[0].mode != 2 || state->captureQueue[1].mode != 1)) ||
		    (count == 3 && (state->captureQueue[0].mode != 1 || state->captureQueue[1].mode != 2 ||
				   state->captureQueue[2].mode != 1)))
			return 10;
		target = count - 1;
		targetCommand = 32 * state->captureQueue[target].slot;
		previousCommand = count > 1 ? 32 * state->captureQueue[target - 1].slot : 0;
		if (state->callbacks.lock(2))
			return 10;
		pending = readVeneziaDebugCommandWord(targetCommand +
			offsetof(VeneziaDebugCaptureRequest, selectionMask[0]));
		pending |= readVeneziaDebugCommandWord(targetCommand +
			offsetof(VeneziaDebugCaptureRequest, selectionMask[1]));
		if (pending) {
			pending = readVeneziaDebugCommandWord(targetCommand +
				offsetof(VeneziaDebugCaptureRequest, parameters[0]));
			pending |= readVeneziaDebugCommandWord(targetCommand +
				offsetof(VeneziaDebugCaptureRequest, parameters[1]));
			if (pending) {
				if (count > 1)
					writeVeneziaDebugRegister(12, targetCommand + 782336 + state->memoryPhysicalBase);
				writeVeneziaDebugCommandWord(targetCommand +
					offsetof(VeneziaDebugCaptureRequest, arguments[1]), commandAddress);
				state->callbacks.unlock(2);
				if (count == 1) {
					state->captureQueue[1].slot = slot;
					state->captureQueue[1].mode = 2;
				} else {
					previous = state->captureQueue[target];
					oldSlot = state->captureQueue[0].slot;
					secondSlot = state->captureQueue[1].slot;
					state->captureQueue[0] = previous;
					state->captureQueue[1].slot = slot;
					state->captureQueue[1].mode = 2;
					state->captureSlotBusy[oldSlot] = 0;
					if (count == 3) {
						state->captureSlotBusy[secondSlot] = 0;
						state->captureQueue[2].slot = -1;
						state->captureQueue[2].mode = 0;
					}
				}
			} else {
				if (count == 1)
					writeVeneziaDebugRegister(12, 0);
				else
					writeVeneziaDebugCommandWord(previousCommand +
						offsetof(VeneziaDebugCaptureRequest, arguments[1]), 0);
				state->callbacks.unlock(2);
				oldSlot = state->captureQueue[target].slot;
				state->captureQueue[target].slot = -1;
				state->captureQueue[target].mode = 0;
				state->captureSlotBusy[slot] = 0;
				state->captureSlotBusy[oldSlot] = 0;
			}
		} else {
			writeVeneziaDebugRegister(12, commandAddress);
			state->callbacks.unlock(2);
			oldSlot = state->captureQueue[0].slot;
			state->captureQueue[0].slot = slot;
			state->captureQueue[0].mode = 2;
			state->captureSlotBusy[oldSlot] = 0;
			for (i = 1; i != count; i++) {
				oldSlot = state->captureQueue[i].slot;
				state->captureQueue[i].slot = -1;
				state->captureQueue[i].mode = 0;
				state->captureSlotBusy[oldSlot] = 0;
			}
		}
	}
	writeVeneziaDebugRegister(276, 0);
	if (state->trace.flags & 12) {
		if ((state->captureControl[0] & 0xc000) != 0x8000)
			return 7;
		if (state->callbacks.lock(3))
			return 10;
		updateVeneziaDebugSelectedWord(VENEZIA_DEBUG_RING_REQUEST, 0, 16, 0xffff);
		state->callbacks.unlock(3);
		state->trace.flags &= ~12u;
	}
	state->captureGroup[0] = -1;
	state->captureInvalid = 0;
	state->trace.flags |= 2;
	return 0;
}

/* name unknown: validate a capture-control command before writing its packed register. */
int setVeneziaDebugCaptureCommand(unsigned int processor, uint16_t control)
{
	VeneziaDebuggerState *state = &g_veneziaDebugger;
	uint8_t argument = control;
	unsigned int operation = control & 0xff00;
	unsigned int filtered = control & 0xfd00;
	unsigned int mode, alternate, previous;
	int invalid;

	if (processor)
		return 9;
	invalid = filtered != 0xc100 && operation != 0;
	if (invalid || (control & 0xc0))
		return 9;
	if (((control & 0xf700) == 0xa000 || (control & 0xfb00) == 0xb000 ||
	     operation == 0xac00 || operation == 0xb800 || filtered == 0xc900) && !(control & 15))
		return 9;
	if (!state->callbacks.checkReset || state->callbacks.checkReset() != 0)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode == alternate) {
		if (mode - 1 <= 4) {
			state->mode = mode;
			goto checkMode;
		}
	} else if (mode - 1 <= 1 && alternate - 1 <= 1) {
		state->mode = 2;
		goto writeControl;
	}
	mode = state->mode;
checkMode:
	if (mode - 1 > 1)
		return 5;
writeControl:
	previous = readVeneziaDebugChannelBits(276, 16, 0xffff) & 0xff00;
	invalid = previous != 0x8000;
	if (operation != 0x9000)
		invalid = 0;
	if (invalid)
		return 7;
	if (getVeneziaDebugCaptureFlag(0)) {
		if (!(state->captureControl[0] & 0x400) && (operation & 0x8000))
			return 7;
		if (previous == 0x9000)
			return 3;
	}
	writeVeneziaDebugRegister(276, argument | (operation << 16));
	return 0;
}

/* name unknown: return the low status byte while capture is inactive. */
int getVeneziaDebugCommandStatus(unsigned int processor, unsigned int *status)
{
	VeneziaDebuggerState *state = &g_veneziaDebugger;
	unsigned int mode, alternate;

	if (processor || !status)
		return 9;
	if (!state->callbacks.checkReset || state->callbacks.checkReset() != 0)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode == alternate) {
		if (mode - 1 <= 4) {
			state->mode = mode;
			goto checkMode;
		}
	} else if (mode - 1 <= 1 && alternate - 1 <= 1) {
		state->mode = 2;
		goto readStatus;
	}
	mode = state->mode;
checkMode:
	if (mode - 1 > 1)
		return 5;
readStatus:
	if (readVeneziaDebugCaptureFlag(0))
		return 7;
	*status = (uint8_t)readVeneziaDebugChannelWord(264);
	return 0;
}

/* name unknown: inspect a pending trace event without consuming its ring entry. */
__attribute__((noinline))
int getVeneziaDebugTraceEvent(unsigned int processor, VeneziaDebugTraceEvent *event)
{
	VeneziaDebuggerState *state = &g_veneziaDebugger;
	uint32_t first __attribute__((aligned(8)));
	uint32_t second __attribute__((aligned(8)));
	uint32_t third __attribute__((aligned(8)));
	unsigned int mode, alternate;
	int ret;

	if (processor) {
		ret = 9;
		goto out;
	}
	if (!state->callbacks.checkReset || state->callbacks.checkReset() != 0) {
		ret = 8;
		goto out;
	}
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode == alternate) {
		if (mode - 1 <= 4) {
			state->mode = mode;
			goto checkMode;
		}
	} else if (mode - 1 <= 1 && alternate - 1 <= 1) {
		state->mode = 2;
		goto readEvent;
	}
	mode = state->mode;
checkMode:
	if (mode - 1 > 1) {
		ret = 5;
		goto out;
	}
readEvent:
	if (!readVeneziaDebugCaptureFlag(0)) {
		ret = 4;
		goto out;
	}
	ret = getVeneziaDebugPendingTraceHeader(0, &first, &second, &third);
	if (!ret && event) {
		ret = 0;
		event->marked = !!(first & 0x2000);
		event->value = second;
	}
out:
	/* Sony also tests this untouched stack word on the early error paths. */
	if (third & 0x10000)
		ret = 12;
	return ret;
}

/* name unknown: update the active trace ring's virtual cursor after range checks. */
int setVeneziaDebugTraceCursor(unsigned int processor, uint32_t cursor)
{
	VeneziaDebuggerState *state = &g_veneziaDebugger;
	unsigned int mode, alternate;
	uint32_t base, capacity;

	if (processor || !cursor)
		return 9;
	if (!state->callbacks.checkReset || state->callbacks.checkReset() != 0)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode == alternate) {
		if (mode - 1 <= 4) {
			state->mode = mode;
			goto checkMode;
		}
	} else if (mode - 1 <= 1 && alternate - 1 <= 1) {
		state->mode = 2;
		goto setCursor;
	}
	mode = state->mode;
checkMode:
	if (mode - 1 > 1)
		return 5;
setCursor:
	if (state->captureQueue[0].slot == -1) {
		if (!(readVeneziaDebugChannelWord(256) & 0x80))
			return 4;
	} else if (getVeneziaDebugQueuedCaptureMode() != 1) {
		return 4;
	}
	if ((state->captureControl[0] & 0xc000) != 0x8000)
		return 7;
	base = state->trace.bufferVirtualBase - state->trace.bufferPhysicalBase +
		readVeneziaDebugSelectedWord(offsetof(VeneziaDebugRingConfig, base));
	capacity = readVeneziaDebugSelectedWord(offsetof(VeneziaDebugRingConfig, capacity));
	if (base > cursor || cursor >= base + capacity ||
	    !(readVeneziaDebugSelectedBits(VENEZIA_DEBUG_RING_CURSOR_VALID, 24, 255) << 24))
		return 7;
	writeVeneziaDebugSelectedWord(VENEZIA_DEBUG_RING_CURSOR,
		state->trace.bufferPhysicalBase - state->trace.bufferVirtualBase + cursor);
	return 0;
}

/* name unknown: return the active trace ring's cursor in the host address space. */
int getVeneziaDebugTraceCursor(unsigned int processor, uint32_t *cursor)
{
	VeneziaDebuggerState *state = &g_veneziaDebugger;
	unsigned int mode, alternate;

	if (processor || !cursor)
		return 9;
	if (state->callbacks.checkReset && state->callbacks.checkReset() == 0) {
		mode = readVeneziaDebugStatusWord(0);
		alternate = readVeneziaDebugStatusWord(32);
		if (mode == alternate) {
			if (mode - 1 <= 4) {
				state->mode = mode;
				goto checkMode;
			}
		} else if (mode - 1 <= 1 && alternate - 1 <= 1) {
			state->mode = 2;
			goto readCursor;
		}
		mode = state->mode;
checkMode:
		if (mode - 1 > 1)
			return 5;
readCursor:
		if (state->captureQueue[0].slot == -1) {
			if (!(readVeneziaDebugChannelWord(256) & 0x80))
				return 4;
		} else if (getVeneziaDebugQueuedCaptureMode() != 1) {
			return 4;
		}
		if ((state->captureControl[0] & 0xc000) != 0x8000 ||
		    !(readVeneziaDebugSelectedBits(VENEZIA_DEBUG_RING_CURSOR_VALID, 24, 255) << 24))
			return 7;
		*cursor = readVeneziaDebugSelectedWord(VENEZIA_DEBUG_RING_CURSOR) +
			state->trace.bufferVirtualBase - state->trace.bufferPhysicalBase;
		return 0;
	}
	return 8;
}

/* name unknown: report the distance to the closest valid trace reader cursor. */
__attribute__((noinline)) int getVeneziaDebugTraceAvailable(unsigned int processor, SceSize *available)
{
	VeneziaDebuggerState *state = &g_veneziaDebugger;
	uint32_t next __attribute__((aligned(8)));
	unsigned int distance __attribute__((aligned(8)));
	unsigned int mode, alternate;

	if (processor || !available)
		return 9;
	if (!state->callbacks.checkReset || state->callbacks.checkReset() != 0)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode == alternate) {
		if (mode - 1 <= 4) {
			state->mode = mode;
			goto checkMode;
		}
	} else if (mode - 1 <= 1 && alternate - 1 <= 1) {
		state->mode = 2;
		goto getDistance;
	}
	mode = state->mode;
checkMode:
	if (mode - 1 > 1)
		return 5;
getDistance:
	if (state->captureQueue[0].slot == -1) {
		if (!(readVeneziaDebugChannelWord(256) & 0x80))
			return 4;
	} else if (getVeneziaDebugQueuedCaptureMode() != 1) {
		return 4;
	}
	if ((state->captureControl[0] & 0xc000) != 0x8000 ||
	    getVeneziaDebugNextTraceCursor(0, &next, &distance))
		return 7;
	*available = distance;
	return 0;
}

/* name unknown: write the trace engine's single-bit option register. */
int setVeneziaDebugTraceFlag(unsigned int processor, uint32_t flag)
{
	VeneziaDebuggerState *state = &g_veneziaDebugger;
	unsigned int mode, alternate;

	if (processor || (flag & ~1u))
		return 9;
	if (!state->callbacks.checkReset || state->callbacks.checkReset() != 0)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode == alternate) {
		if (mode - 1 <= 4) {
			state->mode = mode;
			goto checkMode;
		}
	} else if (mode - 1 <= 1 && alternate - 1 <= 1) {
		state->mode = 2;
		goto writeFlag;
	}
	mode = state->mode;
checkMode:
	if (mode - 1 > 1)
		return 5;
writeFlag:
	if ((state->captureControl[0] & 0xc000) != 0x8000)
		return 7;
	writeVeneziaDebugRegister(1808, flag);
	return 0;
}

/* name unknown: clear the trace engine's status register. */
int clearVeneziaDebugTraceStatus(unsigned int processor)
{
	VeneziaDebuggerState *state = &g_veneziaDebugger;
	unsigned int mode, alternate;

	if (processor)
		return 9;
	if (!state->callbacks.checkReset || state->callbacks.checkReset() != 0)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode == alternate) {
		if (mode - 1 <= 4) {
			state->mode = mode;
			goto checkMode;
		}
	} else if (mode - 1 <= 1 && alternate - 1 <= 1) {
		state->mode = 2;
		goto clearStatus;
	}
	mode = state->mode;
checkMode:
	if (mode - 1 > 1)
		return 5;
clearStatus:
	if ((state->captureControl[0] & 0xc000) != 0x8000)
		return 7;
	writeVeneziaDebugRegister(1816, 0);
	return 0;
}

/* name unknown: set the ring request byte and remember the outstanding host request. */
int requestVeneziaDebugTrace(unsigned int processor)
{
	VeneziaDebuggerState *state = &g_veneziaDebugger;
	unsigned int mode, alternate;

	if (processor)
		return 9;
	if (!state->callbacks.checkReset || state->callbacks.checkReset() != 0)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode == alternate) {
		if (mode - 1 <= 4) {
			state->mode = mode;
			goto checkMode;
		}
	} else if (mode - 1 <= 1 && alternate - 1 <= 1) {
		state->mode = 2;
		goto requestTrace;
	}
	mode = state->mode;
checkMode:
	if (mode - 1 > 1)
		return 5;
requestTrace:
	if ((state->captureControl[0] & 0xc000) != 0x8000)
		return 7;
	if (state->callbacks.lock(3))
		return 10;
	updateVeneziaDebugSelectedWord(VENEZIA_DEBUG_RING_REQUEST, 1, 16, 255);
	state->callbacks.unlock(3);
	state->trace.flags |= 8;
	return 0;
}

/* name unknown: return the active/pending capture bits and retire completed host flags. */
int getVeneziaDebugCaptureState(unsigned int processor, uint32_t *captureState)
{
	VeneziaDebuggerState *state = &g_veneziaDebugger;
	unsigned int mode, alternate;
	int lastIndex;
	uint32_t result, command, pending;
	VeneziaDebugCaptureQueueEntry *last;

	if (processor || !captureState)
		return 9;
	if (!state->callbacks.checkReset || state->callbacks.checkReset() != 0)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode == alternate) {
		if (mode - 1 <= 4) {
			state->mode = mode;
			goto checkMode;
		}
	} else if (mode - 1 <= 1 && alternate - 1 <= 1) {
		state->mode = 2;
		goto readState;
	}
	mode = state->mode;
checkMode:
	if (mode - 1 > 1)
		return 5;
readState:
	if (!(state->trace.flags & 3))
		return 7;
	if (state->captureQueue[0].slot == -1) {
		lastIndex = -1;
		if (state->captureQueue[1].slot != -1 ||
		    state->captureQueue[2].slot != -1 || state->captureQueue[3].slot != -1)
			return 7;
	} else if (state->captureQueue[1].slot == -1) {
		lastIndex = 0;
		if (state->captureQueue[2].slot != -1 || state->captureQueue[3].slot != -1)
			return 7;
	} else if (state->captureQueue[2].slot == -1) {
		lastIndex = 1;
		if (state->captureQueue[3].slot != -1)
			return 7;
	} else {
		lastIndex = state->captureQueue[3].slot == -1 ? 2 : 3;
	}
	if (lastIndex == -1) {
		result = (readVeneziaDebugChannelWord(256) & 0x80) ? 2 : 0;
	} else {
		last = &state->captureQueue[lastIndex];
		if (last->mode == 1)
			result = 2;
		else if (last->mode == 2)
			result = 0;
		else
			return 7;
		command = 32 * last->slot;
		pending = readVeneziaDebugCommandWord(command +
			offsetof(VeneziaDebugCaptureRequest, selectionMask[0]));
		pending |= readVeneziaDebugCommandWord(command +
			offsetof(VeneziaDebugCaptureRequest, selectionMask[1]));
		if (pending)
			result |= 1;
		else
			result &= ~1u;
	}
	if (result == 2)
		state->trace.flags &= ~1u;
	if (!result)
		state->trace.flags &= ~2u;
	*captureState = result;
	return 0;
}

/* name unknown: retire the trace request after the DSP clears its request byte. */
int getVeneziaDebugTraceRequestState(unsigned int processor, int *pending)
{
	VeneziaDebuggerState *state = &g_veneziaDebugger;
	unsigned int mode, alternate;
	uint8_t request;

	if (processor || !pending)
		return 9;
	if (!state->callbacks.checkReset || state->callbacks.checkReset() != 0)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode == alternate) {
		if (mode - 1 <= 4) {
			state->mode = mode;
			goto checkMode;
		}
	} else if (mode - 1 <= 1 && alternate - 1 <= 1) {
		state->mode = 2;
		goto readRequest;
	}
	mode = state->mode;
checkMode:
	if (mode - 1 > 1)
		return 5;
readRequest:
	if (!(state->trace.flags & 8) ||
	    (state->captureControl[0] & 0xc000) != 0x8000)
		return 7;
	request = readVeneziaDebugSelectedBits(VENEZIA_DEBUG_RING_REQUEST, 16, 255);
	if (request) {
		*pending = 1;
		return 0;
	}
	*pending = request;
	state->trace.flags &= ~8u;
	return request;
}

/* PLACEHOLDER: the meaning of capture control register 344 bit 1 is unknown. */
int setVeneziaDebugCaptureOption(unsigned int processor, unsigned int enabled)
{
	VeneziaDebuggerState *state = &g_veneziaDebugger;
	unsigned int mode, alternate;
	int captureMode;

	if (enabled > 1 || processor)
		return 9;
	if (!state->callbacks.checkReset || state->callbacks.checkReset() != 0)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode == alternate) {
		if (mode - 1 <= 4) {
			state->mode = mode;
			goto checkMode;
		}
	} else if (mode - 1 <= 1 && alternate - 1 <= 1) {
		state->mode = 2;
		goto checkCapture;
	}
	mode = state->mode;
checkMode:
	if (mode - 1 > 1)
		return 5;
checkCapture:
	if (state->captureQueue[0].slot == -1) {
		if (readVeneziaDebugChannelWord(256) & 0x80)
			return 7;
	} else {
		captureMode = state->captureQueue[0].mode;
		if (state->captureQueue[1].slot != -1) {
			captureMode = state->captureQueue[1].mode;
			if (state->captureQueue[2].slot != -1) {
				captureMode = state->captureQueue[2].mode;
				if (state->captureQueue[3].slot != -1)
					captureMode = state->captureQueue[3].mode;
			}
		}
		if (captureMode == 1)
			return 7;
	}
	updateVeneziaDebugRegister(344, enabled, 1, 1);
	return 0;
}

/* name unknown: decode the current DSP capture into the host context format. */
__attribute__((noinline))
int getVeneziaDebugContextInfo(unsigned int processor, uint32_t *physicalAddress,
	uint32_t *virtualAddress, VeneziaDebugContextInfo *info)
{
	VeneziaDebuggerState *state = &g_veneziaDebugger;
	const VeneziaDebugContextRecord *context;
	const VeneziaDebugCaptureHeader *capture;
	void *contextAddress[1] __attribute__((aligned(8)));
	uint32_t capturePhysical, instruction, flags, result, contextResult;
	uint32_t accumulator, address, operand, source, destination, captureFlags;
	uint32_t memoryPhysical, memoryVirtual;
	int extended, flagA, flagB, flagC, ret;

	if (!physicalAddress || !virtualAddress || !info)
		return 9;
	if (!state->callbacks.checkReset || state->callbacks.checkReset() != 0)
		return 8;
	ret = getVeneziaDebugCurrentContext(processor, contextAddress);
	if (ret)
		return ret;
	context = contextAddress[0];
	capturePhysical = readVeneziaDebugContextWord(&context->capturePhysicalAddress);
	if (!capturePhysical)
		return 3;
	instruction = readVeneziaDebugContextWord(&context->instruction);
	result = readVeneziaDebugContextWord(&context->encodedInstruction) + 0x400010;
	result = ((result >> 4) & 0x1ff8000) | ((result << 10) & 0x1e000000) |
		(result & 0xe0007fff);
	memoryPhysical = state->memoryPhysicalBase;
	memoryVirtual = state->memoryVirtualBase;
	flags = readVeneziaDebugContextWord(&context->flags);
	contextResult = readVeneziaDebugContextWord(&context->result);
	*physicalAddress = result;
	*virtualAddress = result + state->trace.imageVirtualBase - state->trace.imagePhysicalBase;
	info->controlZero = 0;
	info->instruction.low = instruction;
	info->instruction.high = instruction >> 16;
	info->controlZeros[0] = 0;
	info->controlZeros[1] = 0;
	extended = !!(flags & 0x800000);
	info->controlHigh = flags >> 28;
	info->controlLow = (flags >> 24) & 15;
	info->controlExtended = extended;
	info->executionMode = (flags >> 17) & 7;
	info->result.low = contextResult;
	info->result.high = contextResult >> 16;
	info->instructionWidth = (flags & 0xc00000) ? 3 : 2;
	flagA = !!(flags & 0x200000);
	flagB = !!(flags & 0x100000);
	flagC = !!(flags & 0x400000);
	info->flagA = flagA;
	info->flagB = flagB;
	info->flagC = flagC;
	accumulator = readVeneziaDebugContextWord(&context->accumulator);
	address = readVeneziaDebugContextWord(&context->address);
	operand = readVeneziaDebugContextWord(&context->operand);
	source = readVeneziaDebugContextWord(&context->source);
	destination = readVeneziaDebugContextWord(&context->destination);
	result = readVeneziaDebugContextWord(&context->capturePhysicalAddress);
	info->accumulator.low = accumulator;
	info->address.low = address;
	info->address.high = address >> 16;
	info->operand.low = operand;
	info->operand.high = operand >> 16;
	info->accumulator.high = accumulator >> 16;
	info->source.low = source;
	info->source.high = source >> 16;
	info->destination.low = destination;
	info->destination.high = destination >> 16;
	info->accumulatorZero = 0;
	if (!result)
		return 3;
	/* The capture trailer follows the three-word trace header. */
	capture = (const VeneziaDebugCaptureHeader *)(uintptr_t)
		(result + memoryVirtual - memoryPhysical);
	captureFlags = readVeneziaDebugContextWord(&capture->flags);
	info->captureMode = captureFlags >> 16;
	result = readVeneziaDebugContextWord(&capture->header);
	info->captureFlags = captureFlags >> 24;
	info->header = result;
	result = readVeneziaDebugContextWord(&capture->control);
	info->headerMode = result >> 29;
	info->headerGroup = (readVeneziaDebugContextWord(&capture->control) >> 26) & 7;
	if ((extended && flagA) || (!extended && flagC && flagB))
		info->executionClass = 3;
	else if (extended && !flagB)
		info->executionClass = 1;
	else
		info->executionClass = 2;
	if (state->captureInvalid)
		return 11;
	return checkVeneziaDebugCaptureValidity(memoryVirtual - memoryPhysical + capturePhysical);
}

/* name unknown: acknowledge one trace packet and advance its reader cursor. */
int consumeVeneziaDebugTracePacket(unsigned int processor)
{
	VeneziaDebuggerState *state = &g_veneziaDebugger;
	VeneziaDebugTracePacket *packet;
	unsigned int mode, alternate;
	uint32_t base, capacity, memoryVirtual, memoryPhysical, cursor, next, context;
	int captureMode;

	if (processor)
		return 9;
	if (!state->callbacks.checkReset || state->callbacks.checkReset() != 0)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode == alternate) {
		if (mode - 1 <= 4) {
			state->mode = mode;
			goto checkMode;
		}
	} else if (mode - 1 <= 1 && alternate - 1 <= 1) {
		state->mode = 2;
		goto checkCapture;
	}
	mode = state->mode;
checkMode:
	if (mode - 1 > 1)
		return 5;
checkCapture:
	if (state->captureQueue[0].slot == -1) {
		if (!(readVeneziaDebugChannelWord(256) & 0x80))
			return 3;
	} else {
		captureMode = state->captureQueue[0].mode;
		if (state->captureQueue[1].slot != -1) {
			captureMode = state->captureQueue[1].mode;
			if (state->captureQueue[2].slot != -1) {
				captureMode = state->captureQueue[2].mode;
				if (state->captureQueue[3].slot != -1)
					captureMode = state->captureQueue[3].mode;
			}
		}
		if (captureMode != 1)
			return 3;
	}
	if (!(readVeneziaDebugSelectedBits(1816, 24, 255) << 24))
		return 10;
	base = readVeneziaDebugSelectedWord(1796);
	memoryVirtual = state->memoryVirtualBase;
	memoryPhysical = state->memoryPhysicalBase;
	capacity = readVeneziaDebugSelectedWord(1800);
	cursor = state->memoryVirtualBase - state->memoryPhysicalBase +
		readVeneziaDebugSelectedWord(1820);
	if (readVeneziaDebugSelectedWord(1812) + state->memoryVirtualBase -
	    state->memoryPhysicalBase == cursor)
		return 3;
	packet = (VeneziaDebugTracePacket *)(uintptr_t)cursor;
	context = readVeneziaDebugContextWord(&packet->context);
	base += memoryVirtual - memoryPhysical;
	next = cursor + sizeof(*packet);
	if (next >= base + capacity)
		next = base;
	*(uint32_t *)(uintptr_t)((state->memoryVirtualBase - state->memoryPhysicalBase +
		context) ^ 4) = 1;
	writeVeneziaDebugSelectedWord(1820, state->memoryPhysicalBase -
		state->memoryVirtualBase + next);
	return 0;
}

/* name unknown: unpack both captured DSP register banks and execution controls. */
__attribute__((noinline))
int getVeneziaDebugRegisters(unsigned int processor, VeneziaDebugRegisterInfo *info)
{
	VeneziaDebuggerState *state = &g_veneziaDebugger;
	const VeneziaDebugContextRecord *context;
	const VeneziaDebugRegisterCapture *capture;
	void *contextAddress[1] __attribute__((aligned(8)));
	unsigned int mode, alternate, i;
	uint32_t physical, validity, control, pipeline, status, programCounter, mask, selectors;
	int ret;

	if (processor || !info)
		return 9;
	if (!state->callbacks.checkReset || state->callbacks.checkReset() != 0)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode == alternate) {
		if (mode - 1 <= 4) {
			state->mode = mode;
			goto checkMode;
		}
	} else if (mode - 1 <= 1 && alternate - 1 <= 1) {
		state->mode = 2;
		goto getContext;
	}
	mode = state->mode;
checkMode:
	if (mode - 1 > 1)
		return 5;
getContext:
	if (!getVeneziaDebugCaptureFlag(0))
		return 3;
	ret = getVeneziaDebugPendingTraceContext(0, contextAddress);
	if (ret)
		return ret;
	context = contextAddress[0];
	physical = readVeneziaDebugContextWord(&context->capturePhysicalAddress);
	if (!physical)
		return 3;
	capture = (const VeneziaDebugRegisterCapture *)(uintptr_t)
		(state->memoryVirtualBase - state->memoryPhysicalBase + physical);
	validity = readVeneziaDebugContextWord(&capture->validity);
	if (!(validity >> 24))
		return 3;
	if (state->captureInvalid)
		ret = 11;
	else if (!(validity & 0xff0000)) {
		ret = 11;
		state->captureInvalid = 1;
	}
	control = readVeneziaDebugContextWord(&capture->executionControl);
	info->control[2] = !!(control & 0x400000);
	info->control[0] = !!(control & 0x80000000);
	info->control[1] = control >> 23;
	info->control[3] = !!(control & 0x200000);
	info->control[11] = !!(control & 0x100000);
	info->control[12] = (control >> 17) & 7;
	info->control[13] = (control >> 14) & 7;
	info->control[14] = !!(control & 0x2000);
	info->control[15] = !!(control & 0x1000);
	info->executionFlags[0] = !!(control & 0x800);
	info->executionFlags[1] = !!(control & 0x400);
	pipeline = readVeneziaDebugContextWord(&capture->pipelineControl);
	status = readVeneziaDebugContextWord(&capture->executionStatus);
	info->control[5] = (pipeline >> 28) & 7;
	info->control[4] = !!(pipeline & 0x80000000);
	info->control[8] = pipeline >> 18;
	info->control[9] = pipeline >> 10;
	info->control[10] = pipeline >> 2;
	info->control[6] = !!(pipeline & 0x8000000);
	info->control[7] = !!(pipeline & 0x4000000);
	programCounter = readVeneziaDebugContextWord(&capture->programCounter);
	info->executionStatus.low = status;
	info->executionStatus.high = status >> 16;
	info->programCounter = programCounter;
	info->stackPointer = readVeneziaDebugContextWord(&capture->stackPointer);
	info->bankEnabled[0] = readVeneziaDebugContextWord(&capture->banks[0].enabled) >> 31;
	info->bankEnabled[1] = readVeneziaDebugContextWord(&capture->banks[1].enabled) >> 31;
	if (info->bankEnabled[0]) {
		control = readVeneziaDebugContextWord(&capture->banks[0].control);
		info->banks[0].control[1] = (control >> 23) & 15;
		info->banks[0].control[2] = (control >> 19) & 15;
		info->banks[0].control[0] = control >> 27;
		mask = readVeneziaDebugContextWord(&capture->banks[0].validMask);
		for (i = 0; i != 32; i++) {
			info->banks[0].valid[i] = (mask >> (31 - i)) & 1;
			info->banks[0].low[i] = readVeneziaDebugContextWord(&capture->banks[0].registers[i].low);
			info->banks[0].high[i] = readVeneziaDebugContextWord(&capture->banks[0].registers[i].high);
		}
		selectors = readVeneziaDebugContextWord(&capture->banks[0].selectors);
		info->banks[0].selectors[1] = (selectors >> 22) & 31;
		info->banks[0].selectors[0] = selectors >> 27;
		info->banks[0].selectors[2] = (selectors >> 17) & 31;
		info->banks[0].selectors[3] = (selectors >> 12) & 31;
	} else {
		veneziaDebugMemset(&info->banks[0], 0, sizeof(info->banks[0]));
	}
	if (info->bankEnabled[1]) {
		control = readVeneziaDebugContextWord(&capture->banks[1].control);
		info->banks[1].control[1] = (control >> 23) & 15;
		info->banks[1].control[2] = (control >> 19) & 15;
		info->banks[1].control[0] = control >> 27;
		mask = readVeneziaDebugContextWord(&capture->banks[1].validMask);
		for (i = 0; i != 32; i++) {
			info->banks[1].valid[i] = (mask >> (31 - i)) & 1;
			info->banks[1].low[i] = readVeneziaDebugContextWord(&capture->banks[1].registers[i].low);
			info->banks[1].high[i] = readVeneziaDebugContextWord(&capture->banks[1].registers[i].high);
		}
		selectors = readVeneziaDebugContextWord(&capture->banks[1].selectors);
		info->banks[1].selectors[1] = (selectors >> 22) & 31;
		info->banks[1].selectors[0] = selectors >> 27;
		info->banks[1].selectors[2] = (selectors >> 17) & 31;
		info->banks[1].selectors[3] = (selectors >> 12) & 31;
	} else {
		veneziaDebugMemset(&info->banks[1], 0, sizeof(info->banks[1]));
	}
	control = readVeneziaDebugContextWord(&capture->indexControl);
	info->indexControl[1] = !!(control & 0x40000000);
	info->indexControl[0] = !!(control & 0x80000000);
	info->indexControl[2] = (control >> 25) & 31;
	info->indexControl[3] = (control >> 20) & 31;
	info->indexControl[4] = (control >> 15) & 31;
	info->indexControl[5] = (control >> 10) & 31;
	info->indexControl[6] = (control >> 5) & 31;
	info->indexControl[7] = control & 31;
	return ret;
}

/* name unknown: copy the captured vector register pairs into host word planes. */
__attribute__((noinline))
int getVeneziaDebugVectorRegisters(unsigned int processor, unsigned int bank,
	VeneziaDebugVectorRegisters *info)
{
	VeneziaDebuggerState *state = &g_veneziaDebugger;
	const VeneziaDebugContextRecord *context;
	const VeneziaDebugRegisterCapture *capture;
	void *contextAddress[1] __attribute__((aligned(8)));
	unsigned int mode, alternate, i;
	uint32_t physical;
	int ret;

	if (processor || !info || bank)
		return 9;
	if (!state->callbacks.checkReset || state->callbacks.checkReset() != 0)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode == alternate) {
		if (mode - 1 <= 4)
			state->mode = mode;
	} else if (mode - 1 <= 1 && alternate - 1 <= 1) {
		state->mode = 2;
		goto getContext;
	}
	if (state->mode - 1 > 1)
		return 5;
getContext:
	if (!getVeneziaDebugCaptureFlag(0))
		return 3;
	ret = getVeneziaDebugPendingTraceContext(0, contextAddress);
	if (ret)
		return ret;
	context = contextAddress[0];
	physical = readVeneziaDebugContextWord(&context->capturePhysicalAddress);
	if (!physical)
		return 3;
	capture = (const VeneziaDebugRegisterCapture *)(uintptr_t)
		(state->memoryVirtualBase - state->memoryPhysicalBase + physical);
	if (!(readVeneziaDebugContextWord(&capture->captureControl) >> 24))
		return 3;
	ret = 11;
	if (!state->captureInvalid)
		ret = checkVeneziaDebugCaptureValidity((uintptr_t)capture);
	info->flags = *(const volatile uint8_t *)
		(((uintptr_t)&capture->vectorControl ^ 4) + 3);
	for (i = 0; i != 32; i++) {
		info->banks[0].low[i] = readVeneziaDebugContextWord(&capture->vectorRegisters[0][i].low);
		info->banks[0].high[i] = readVeneziaDebugContextWord(&capture->vectorRegisters[0][i].high);
		info->banks[1].low[i] = readVeneziaDebugContextWord(&capture->vectorRegisters[1][i].low);
		info->banks[1].high[i] = readVeneziaDebugContextWord(&capture->vectorRegisters[1][i].high);
	}
	return ret;
}

/* name unknown: expand a captured channel's packed execution control records. */
__attribute__((noinline))
int getVeneziaDebugChannelState(unsigned int processor, unsigned int channel,
	VeneziaDebugChannelState *info)
{
	VeneziaDebuggerState *state = &g_veneziaDebugger;
	const VeneziaDebugContextRecord *context;
	const VeneziaDebugRegisterCapture *capture;
	const VeneziaDebugChannelRecord *record;
	void *contextAddress[1] __attribute__((aligned(8)));
	uint32_t work[2] __attribute__((aligned(8)));
	uint32_t physical, flags;
	int ret;

	if (processor || !info || channel > 1)
		return 9;
	ret = getVeneziaDebugCurrentContext(0, contextAddress);
	if (ret)
		return ret;
	context = contextAddress[0];
	physical = readVeneziaDebugContextWord(&context->capturePhysicalAddress);
	if (!physical)
		return 3;
	capture = (const VeneziaDebugRegisterCapture *)(uintptr_t)
		(state->memoryVirtualBase - state->memoryPhysicalBase + physical);
	work[0] = readVeneziaDebugContextWord(&capture->captureControl);
	if (!(work[0] & 0x30000))
		return 3;
	ret = 11;
	if (!state->captureInvalid)
		ret = checkVeneziaDebugCaptureValidity((uintptr_t)capture);
	if (!channel)
		record = &capture->channels[0];
	else if (work[0] & 0x20000)
		record = &capture->channels[1];
	else
		return 7;
	info->header[0] = readVeneziaDebugContextWord(&record->header[0]);
	info->header[1] = readVeneziaDebugContextWord(&record->header[1]);
	flags = readVeneziaDebugContextWord(&record->flags);
	info->status[1] = !!(flags & 0x8000000);
	info->status[0] = flags >> 28;
	info->status[2] = !!(flags & 0x4000000);
	info->status[3] = !!(flags & 0x2000000);
	work[0] = readVeneziaDebugContextWord(&record->records[0].control[0]);
	work[1] = readVeneziaDebugContextWord(&record->records[0].control[1]);
	decodeVeneziaDebugControlFields(info, work, 0);
	work[0] = readVeneziaDebugContextWord(&record->records[1].control[0]);
	work[1] = readVeneziaDebugContextWord(&record->records[1].control[1]);
	decodeVeneziaDebugControlFields(info, work, 1);
	work[0] = readVeneziaDebugContextWord(&record->records[2].control[0]);
	work[1] = readVeneziaDebugContextWord(&record->records[2].control[1]);
	decodeVeneziaDebugControlFields(info, work, 2);
	info->parameters[0] = readVeneziaDebugContextWord(&record->records[0].parameter);
	info->parameters[1] = readVeneziaDebugContextWord(&record->records[1].parameter);
	info->parameters[2] = readVeneziaDebugContextWord(&record->records[2].parameter);
	return ret;
}

/* name unknown: transpose one captured channel's four parameter triples. */
__attribute__((noinline))
int getVeneziaDebugParameters(unsigned int processor, unsigned int channel,
	VeneziaDebugParameterInfo *info)
{
	VeneziaDebuggerState *state = &g_veneziaDebugger;
	const VeneziaDebugContextRecord *context;
	const VeneziaDebugRegisterCapture *capture;
	const VeneziaDebugParameterRecord *record;
	void *contextAddress[1] __attribute__((aligned(8)));
	uint32_t physical, flags, control;
	int ret;

	if (processor || !info || channel > 1)
		return 9;
	ret = getVeneziaDebugCurrentContext(0, contextAddress);
	if (ret)
		return ret;
	context = contextAddress[0];
	physical = readVeneziaDebugContextWord(&context->capturePhysicalAddress);
	if (!physical)
		return 3;
	capture = (const VeneziaDebugRegisterCapture *)(uintptr_t)
		(state->memoryVirtualBase - state->memoryPhysicalBase + physical);
	flags = readVeneziaDebugContextWord(&capture->captureControl);
	if (!(flags << 30))
		return 3;
	ret = 11;
	if (!state->captureInvalid)
		ret = checkVeneziaDebugCaptureValidity((uintptr_t)capture);
	record = &capture->parameters[0];
	if (channel) {
		if (!(flags & 2))
			return 7;
		record = &capture->parameters[1];
	}
	info->header = readVeneziaDebugContextWord(&record->header);
	control = readVeneziaDebugContextWord(&record->control);
	info->mode = (control >> 29) & 3;
	info->enabled = !!(control & 0x80000000);
	info->values[0][0] = readVeneziaDebugContextWord(&record->values[0][0]);
	info->values[1][0] = readVeneziaDebugContextWord(&record->values[0][1]);
	info->values[2][0] = readVeneziaDebugContextWord(&record->values[0][2]);
	info->values[3][0] = readVeneziaDebugContextWord(&record->values[0][3]);
	info->values[0][1] = readVeneziaDebugContextWord(&record->values[1][0]);
	info->values[1][1] = readVeneziaDebugContextWord(&record->values[1][1]);
	info->values[2][1] = readVeneziaDebugContextWord(&record->values[1][2]);
	info->values[3][1] = readVeneziaDebugContextWord(&record->values[1][3]);
	info->values[0][2] = readVeneziaDebugContextWord(&record->values[2][0]);
	info->values[1][2] = readVeneziaDebugContextWord(&record->values[2][1]);
	info->values[2][2] = readVeneziaDebugContextWord(&record->values[2][2]);
	info->values[3][2] = readVeneziaDebugContextWord(&record->values[2][3]);
	info->status = readVeneziaDebugContextWord(&record->status);
	return ret;
}

/* name unknown: unpack the capture's parameter control and status bits. */
__attribute__((noinline))
int getVeneziaDebugParameterState(unsigned int processor, unsigned int channel,
	VeneziaDebugParameterState *info)
{
	VeneziaDebuggerState *state = &g_veneziaDebugger;
	const VeneziaDebugContextRecord *context;
	const VeneziaDebugRegisterCapture *capture;
	void *contextAddress[1] __attribute__((aligned(8)));
	unsigned int mode, alternate;
	uint32_t physical, control;
	int ret;

	if (processor || !info || channel)
		return 9;
	if (!state->callbacks.checkReset || state->callbacks.checkReset() != 0)
		return 8;
	mode = readVeneziaDebugStatusWord(0);
	alternate = readVeneziaDebugStatusWord(32);
	if (mode == alternate) {
		if (mode - 1 <= 4)
			state->mode = mode;
	} else if (mode - 1 <= 1 && alternate - 1 <= 1) {
		state->mode = 2;
		goto getContext;
	}
	if (state->mode - 1 > 1)
		return 5;
getContext:
	if (!getVeneziaDebugCaptureFlag(0))
		return 3;
	ret = getVeneziaDebugPendingTraceContext(0, contextAddress);
	if (ret)
		return ret;
	context = contextAddress[0];
	physical = readVeneziaDebugContextWord(&context->capturePhysicalAddress);
	if (!physical)
		return 3;
	capture = (const VeneziaDebugRegisterCapture *)(uintptr_t)
		(state->memoryVirtualBase - state->memoryPhysicalBase + physical);
	if (!(readVeneziaDebugContextWord(&capture->captureControl) & 0xff00))
		return 3;
	ret = 11;
	if (!state->captureInvalid)
		ret = checkVeneziaDebugCaptureValidity((uintptr_t)capture);
	control = readVeneziaDebugContextWord(&capture->parameterControl);
	info->flags[0] = !!(control & 0x8000);
	info->status = control >> 16;
	info->flags[1] = !!(control & 0x4000);
	info->mode = (control >> 12) & 3;
	return ret;
}

__attribute__((noinline))
void writeVeneziaDebugMemoryWord(unsigned int offset, uint32_t value)
{
	*(volatile uint32_t *)(uintptr_t)
		(g_veneziaDebugger.memoryVirtualBase + (offset ^ 4)) = value;
}

__attribute__((noinline))
void writeVeneziaDebugCommandWord(unsigned int offset, uint32_t value)
{
	*(volatile uint32_t *)(uintptr_t)
		(g_veneziaDebugger.memoryVirtualBase + ((offset + 0xbf000) ^ 4)) = value;
}

__attribute__((noinline))
void copyVeneziaDebugCommand(unsigned int offset, const void *data, SceSize size)
{
	const uint32_t *source = data;
	uintptr_t destination = g_veneziaDebugger.memoryVirtualBase + offset + 0xbf000;
	unsigned int count = size >> 2, copied = 0;

	if (!count)
		return;
	if (count > 8) {
		do {
			uint32_t first = source[0];

			__builtin_prefetch(source + 23);
			*(uint32_t *)(destination ^ 4) = first;
			*(uint32_t *)((destination + 4) ^ 4) = source[1];
			*(uint32_t *)((destination + 8) ^ 4) = source[2];
			*(uint32_t *)((destination + 12) ^ 4) = source[3];
			*(uint32_t *)((destination + 16) ^ 4) = source[4];
			*(uint32_t *)((destination + 20) ^ 4) = source[5];
			*(uint32_t *)((destination + 24) ^ 4) = source[6];
			*(uint32_t *)((destination + 28) ^ 4) = source[7];
			source += 8;
			destination += 32;
			copied += 8;
		} while (count - 7 > copied + 1);
	}
	do {
		*(uint32_t *)(destination ^ 4) = *source++;
		destination += 4;
	} while (++copied < count);
}

__attribute__((noinline))
void updateVeneziaDebugCommandWord(unsigned int offset, uint32_t value,
	unsigned int shift, uint32_t mask)
{
	volatile uint32_t *word = (volatile uint32_t *)(uintptr_t)
		(g_veneziaDebugger.memoryVirtualBase + ((offset + 0xbf000) ^ 4));

	*word = ((value & mask) << shift) | (*word & ~(mask << shift));
}

__attribute__((noinline))
void writeVeneziaDebugBankRegister(unsigned int offset, uint32_t value)
{
	*(volatile uint32_t *)(uintptr_t)
		(g_veneziaDebugger.memoryVirtualBase + ((offset + 0xc3500) ^ 4)) = value;
}

__attribute__((noinline))
void writeVeneziaDebugStatusWord(unsigned int offset, uint32_t value)
{
	*(volatile uint32_t *)(uintptr_t)
		(g_veneziaDebugger.memoryVirtualBase + ((offset + 0xc3580) ^ 4)) = value;
}

__attribute__((noinline))
void writeVeneziaDebugRegister(unsigned int offset, uint32_t value)
{
	*(volatile uint32_t *)(uintptr_t)
		(g_veneziaDebugger.memoryVirtualBase + ((offset + 0xc3800) ^ 4)) = value;
}

__attribute__((noinline))
void updateVeneziaDebugRegister(unsigned int offset, uint32_t value,
	unsigned int shift, uint32_t mask)
{
	volatile uint32_t *word = (volatile uint32_t *)(uintptr_t)
		(g_veneziaDebugger.memoryVirtualBase + ((offset + 0xc3800) ^ 4));

	*word = ((value & mask) << shift) | (*word & ~(mask << shift));
}

__attribute__((noinline))
void writeVeneziaDebugSelectedWord(unsigned int offset, uint32_t value)
{
	offset += g_veneziaDebugBankOffsets[g_veneziaDebugger.selectedBank];
	*(volatile uint32_t *)(uintptr_t)
		(g_veneziaDebugger.memoryVirtualBase + (offset ^ 4)) = value;
}

__attribute__((noinline))
void updateVeneziaDebugSelectedWord(unsigned int offset, uint32_t value,
	unsigned int shift, uint32_t mask)
{
	volatile uint32_t *word;

	offset += g_veneziaDebugBankOffsets[g_veneziaDebugger.selectedBank];
	word = (volatile uint32_t *)(uintptr_t)
		(g_veneziaDebugger.memoryVirtualBase + (offset ^ 4));
	*word = ((value & mask) << shift) | (*word & ~(mask << shift));
}

__attribute__((noinline))
void writeVeneziaDebugRingConfig(unsigned int offset, const VeneziaDebugRingConfig *config)
{
	uintptr_t address = offset +
		g_veneziaDebugBankOffsets[g_veneziaDebugger.selectedBank] +
		g_veneziaDebugger.memoryVirtualBase;

#define WRITE_RING_FIELD(field) \
	(*(uint32_t *)((address + offsetof(VeneziaDebugRingConfig, field)) ^ 4) = config->field)
	WRITE_RING_FIELD(control);
	WRITE_RING_FIELD(base);
	WRITE_RING_FIELD(capacity);
	WRITE_RING_FIELD(request);
	WRITE_RING_FIELD(cursors[0].control);
	WRITE_RING_FIELD(cursors[0].address);
	WRITE_RING_FIELD(cursors[1].control);
	WRITE_RING_FIELD(cursors[1].address);
	WRITE_RING_FIELD(cursors[2].control);
	WRITE_RING_FIELD(cursors[2].address);
	WRITE_RING_FIELD(cursors[3].control);
	WRITE_RING_FIELD(cursors[3].address);
	WRITE_RING_FIELD(parameters[0]);
	WRITE_RING_FIELD(parameters[1]);
	WRITE_RING_FIELD(parameters[2]);
	WRITE_RING_FIELD(parameters[3]);
#undef WRITE_RING_FIELD
}

__attribute__((noinline))
uint32_t readVeneziaDebugCommandWord(unsigned int offset)
{
	return *(volatile uint32_t *)(uintptr_t)
		(g_veneziaDebugger.memoryVirtualBase + ((offset + 0xbf000) ^ 4));
}

__attribute__((noinline))
uint32_t readVeneziaDebugCommandBits(unsigned int offset, unsigned int shift,
	uint32_t mask)
{
	return (*(volatile uint32_t *)(uintptr_t)
		(g_veneziaDebugger.memoryVirtualBase + ((offset + 0xbf000) ^ 4)) >> shift) & mask;
}

__attribute__((noinline))
uint32_t readVeneziaDebugStatusWord(unsigned int offset)
{
	return *(volatile uint32_t *)(uintptr_t)
		(g_veneziaDebugger.memoryVirtualBase + ((offset + 0xc3580) ^ 4));
}

__attribute__((noinline))
uint32_t readVeneziaDebugChannelWord(unsigned int offset)
{
	return *(volatile uint32_t *)(uintptr_t)
		(g_veneziaDebugger.memoryVirtualBase + ((offset + 0xc3800) ^ 4));
}

__attribute__((noinline))
uint32_t readVeneziaDebugChannelBits(unsigned int offset, unsigned int shift,
	uint32_t mask)
{
	return (*(volatile uint32_t *)(uintptr_t)
		(g_veneziaDebugger.memoryVirtualBase + ((offset + 0xc3800) ^ 4)) >> shift) & mask;
}

__attribute__((noinline))
uint32_t readVeneziaDebugSelectedWord(unsigned int offset)
{
	offset += g_veneziaDebugBankOffsets[g_veneziaDebugger.selectedBank];
	return *(volatile uint32_t *)(uintptr_t)
		(g_veneziaDebugger.memoryVirtualBase + (offset ^ 4));
}

__attribute__((noinline))
uint32_t readVeneziaDebugSelectedBits(unsigned int offset, unsigned int shift,
	uint32_t mask)
{
	offset += g_veneziaDebugBankOffsets[g_veneziaDebugger.selectedBank];
	return (*(volatile uint32_t *)(uintptr_t)
		(g_veneziaDebugger.memoryVirtualBase + (offset ^ 4)) >> shift) & mask;
}

__attribute__((noinline))
void switchVeneziaDebugBank(void)
{
	g_veneziaDebugger.selectedBank ^= 1u;
}

/* name unknown: debugger callbacks for the Pervasive test-and-set and reset pages. */
int unlockVeneziaDebugChannel(unsigned int channel)
{
	if (channel > 7)
		return 9;
	g_veneziaVipState.tasRegisters->channels[channel].lock = 0;
	return 0;
}

int checkVeneziaDebugReset(void)
{
	uint32_t saved, reset;

	saved = sceKernelSpinlockLowLockCpuSuspendIntr(&g_veneziaDebugger.spinlock);
	reset = g_veneziaVipState.resetRegisters->vip;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_veneziaDebugger.spinlock, saved);
	return reset & 1;
}

/* name unknown: collect successive captures into the caller's snapshot list. */
int collectVeneziaDebugSnapshots(unsigned int channel, VeneziaVipProcessStorage *storage,
	VeneziaDebugSnapshotList *list, unsigned int first)
{
	VeneziaDebugProcessCapture *capture = &storage->captures[channel];
	VeneziaDebugSnapshotOutput *output;
	unsigned int processor = storage->processors[channel];
	unsigned int count;
	uint32_t valid = 0;
	int ret, traceRet;

	if (first >= list->count)
		return 0;
	for (;;) {
		ret = getVeneziaDebugContextInfo(processor, &capture->capturePhysicalAddress,
			&capture->captureVirtualAddress, &capture->snapshot->context);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(43, 6, processor, ret,
				capture->capturePhysicalAddress, capture->captureVirtualAddress,
				(int16_t)capture->snapshot->context.instruction.high,
				(int16_t)capture->snapshot->context.instruction.low);
		capture->captureVirtualAddress = capture->imagePhysicalAddress;
		if (ret)
			break;
		if (!getVeneziaDebugRegisters(processor, &capture->snapshot->data.registers))
			valid |= 1;
		if (!getVeneziaDebugVectorRegisters(processor, 0, &capture->snapshot->data.vectorRegisters))
			valid |= 2;
		if (!getVeneziaDebugChannelState(processor, 0, &capture->snapshot->data.channels[0])) {
			if (getVeneziaDebugChannelState(processor, 1, &capture->snapshot->data.channels[1]))
				valid |= 4;
			else
				valid |= 0x24;
		}
		if (!getVeneziaDebugParameters(processor, 0, &capture->snapshot->data.parameters[0])) {
			if (getVeneziaDebugParameters(processor, 1, &capture->snapshot->data.parameters[1]))
				valid |= 0x10;
			else
				valid |= 0x50;
		}
		if (!getVeneziaDebugParameterState(processor, 0, &capture->snapshot->data.parameterState))
			valid |= 8;
		traceRet = getVeneziaDebugTraceEvent(processor, &capture->snapshot->data.trace);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(21, 4, processor, traceRet,
				capture->snapshot->data.trace.marked, capture->snapshot->data.trace.value);
		if (!traceRet)
			valid |= 0x80;
		output = list->snapshots[first];
		output->virtualAddress = capture->captureVirtualAddress;
		output->physicalAddress = capture->capturePhysicalAddress;
		output->valid = valid;
		output->event = 0;
		output->context = capture->snapshot->context;
		if (valid & 1)
			memcpy(&output->data.registers, &capture->snapshot->data.registers,
				sizeof(output->data.registers));
		if (valid & 2)
			memcpy(&output->data.vectorRegisters, &capture->snapshot->data.vectorRegisters,
				sizeof(output->data.vectorRegisters));
		if (valid & 4)
			output->data.channels[0] = capture->snapshot->data.channels[0];
		if (valid & 0x20)
			output->data.channels[1] = capture->snapshot->data.channels[1];
		if (valid & 8)
			output->data.parameterState = capture->snapshot->data.parameterState;
		if (valid & 0x10)
			output->data.parameters[0] = capture->snapshot->data.parameters[0];
		if (valid & 0x40)
			output->data.parameters[1] = capture->snapshot->data.parameters[1];
		count = list->count;
		if (valid & 0x80)
			output->data.trace = capture->snapshot->data.trace;
		else {
			output->data.trace.marked = UINT32_MAX;
			output->data.trace.value = UINT32_MAX;
		}
		++first;
		++capture->completedCount;
		if (first >= count)
			return 0;
	}
	if (ret == 3)
		return 0;
	if (ret == 11)
		return -2141060852;
	if (ret == 10)
		return -2141060854;
	return -2141060856;
}

/* name unknown: translate mailbox status into debugger events and notifications. */
int veneziaDebugInterruptHandler(int interrupt, VeneziaVipState *state)
{
	VeneziaDebugCallbackState *callbacks = state->callbackState;
	uint32_t mailbox, events = 0, saved;
	VeneziaDebugCallback *callback;
	unsigned int i;

	(void)interrupt;
	readVeneziaDebuggerMailbox(0, &mailbox);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(11, 2, 0, mailbox);
	if (callbacks) {
		saved = sceKernelSpinlockLowLockCpuSuspendIntr(&g_veneziaDebugger.spinlock);
		if (mailbox & 0xf) {
			events |= 1;
			++callbacks->counters[3].count;
		}
		if (mailbox & 0x80) {
			events |= 2;
			++callbacks->counters[5].count;
		}
		if (mailbox & 0x1000000) {
			events |= 4;
			++callbacks->counters[6].count;
		}
		if (mailbox & 0x8000) {
			events |= 8;
			++callbacks->counters[7].count;
		}
		if (mailbox & 0x4700) {
			++callbacks->counters[0].count;
			if (mailbox & 0x4000)
				events |= 0x1000;
			else
				events |= 0x100;
			if (mailbox & 0x100)
				++callbacks->counters[8].count;
			if (mailbox & 0x200)
				++callbacks->counters[9].count;
			if (mailbox & 0x400)
				++callbacks->counters[10].count;
			if (mailbox & 0x800)
				++callbacks->counters[11].count;
			if (mailbox & 0x4000)
				++callbacks->counters[12].count;
		}
		if (mailbox & 0x1000) {
			events |= 0x200;
			++callbacks->counters[1].count;
		}
		if (mailbox & 0x2000) {
			events |= 0x2000;
			++callbacks->counters[2].count;
		}
		if (mailbox & 0x800)
			++callbacks->counters[4].count;
		for (i = 0; i < 6; ++i) {
			callback = &callbacks->callbacks[i];
			if (callback->uid && (callback->eventMask & events)) {
				callback->pendingEvents = callback->eventMask & events;
				sceKernelNotifyCallback(callback->uid, callback->pendingEvents);
			}
		}
		sceKernelSpinlockLowUnlockCpuResumeIntr(&g_veneziaDebugger.spinlock, saved);
	}
	writeVeneziaDebuggerMailbox(0, mailbox);
	sceKernelSetEventFlag(state->eventFlag, events);
	__asm__ volatile("dmb sy" ::: "memory");
	return -1;
}

/* name unknown: auxiliary processor stop information returned by the VIP core. */
typedef struct VeneziaDebugStopInfo {
	uint32_t contextValue;
	void *context;
	uint32_t flags;
	int result;
	uint32_t control;
} VeneziaDebugStopInfo;

int getVeneziaVipStopInfo(unsigned int processor, VeneziaDebugStopInfo *info);
int clearVeneziaVipInterruptStatus(unsigned int processor, uint32_t *status);

int veneziaDebugAuxiliaryInterruptHandler(int interrupt, VeneziaVipState *state)
{
	VeneziaDebugCallbackState *callbacks = state->callbackState;
	VeneziaDebugStopInfo info;
	uint32_t status, events, saved;
	int ret;

	(void)interrupt;
	ret = getVeneziaVipStopInfo(0, &info);
	if (ret != 0 && ret != 6)
		info.result = INT32_MIN;
	ret = clearVeneziaVipInterruptStatus(0, &status);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(95, 3, 0, status, ret);
	if (!ret) {
		saved = sceKernelSpinlockLowLockCpuSuspendIntr(&g_veneziaDebugger.spinlock);
		++callbacks->counters[0].count;
		switch ((uint32_t)info.result) {
		case -12: events = 0x80000000; break;
		case -11: events = 0x40000000; break;
		case -10: events = 0x20000000; break;
		case 0:
			events = 0x10000000;
			if (info.flags & 0x20000)
				events |= 0x80000;
			break;
		case INT32_MIN: events = 0; break;
		case -1: events = 1; break;
		case -2: events = 2; break;
		case -3: events = 4; break;
		case -4: events = 8; break;
		case -5: events = 16; break;
		case -6: events = 32; break;
		default: events = 64; break;
		}
		if (status & 0x1000000) {
			events |= 0x20000;
			++callbacks->counters[0].unidentifiedControl;
		}
		sceKernelSpinlockLowUnlockCpuResumeIntr(&g_veneziaDebugger.spinlock, saved);
		sceKernelSetEventFlag(state->auxiliaryEventFlag, events);
	}
	__asm__ volatile("dmb sy" ::: "memory");
	return -1;
}

int lockVeneziaDebugChannel(unsigned int channel)
{
	volatile uint32_t *lock;
	uint32_t value;

	if (channel > 7)
		return 9;
	lock = &g_veneziaVipState.tasRegisters->channels[channel].lock;
	do {
		value = *lock;
		__asm__ volatile("dmb sy" ::: "memory");
	} while (value == 1);
	return 0;
}

int clearVeneziaDebugReset(void)
{
	return ScePervasiveForDriver_E2D8F6C3();
}

int setVeneziaDebugReset(void)
{
	return ScePervasiveForDriver_31C0A98B();
}

int readVeneziaVipDebugMailbox(unsigned int processor, uint32_t *value)
{
	if (processor > 1)
		return 9;
	if (processor)
		*value = g_veneziaVipState.mailboxRegisters->debugReceive[1];
	else
		*value = g_veneziaVipState.mailboxRegisters->debugReceive[0];
	return 0;
}

int acknowledgeVeneziaVipDebugMailbox(unsigned int processor, uint32_t value)
{
	if (processor > 1)
		return 9;
	if (processor)
		g_veneziaVipState.mailboxRegisters->debugAcknowledge[1] = value;
	else
		g_veneziaVipState.mailboxRegisters->debugAcknowledge[0] = value;
	return 0;
}

int writeVeneziaVipDebugMailbox(unsigned int processor, uint32_t value)
{
	if (processor > 1)
		return 9;
	if (processor)
		g_veneziaVipState.mailboxRegisters->debugSend[1] = value;
	else
		g_veneziaVipState.mailboxRegisters->debugSend[0] = value;
	return 0;
}

extern int getVeneziaVipCaptureSize(unsigned int processor, VeneziaVipState *state,
	const VeneziaDebugImageFormat *geometry, uint32_t *size);
__attribute__((noinline)) SceSize getVeneziaVipRingSize(SceSize size);
__attribute__((noinline)) int prepareVeneziaVipCapture(VeneziaVipState *state, VeneziaVipProcessStorage *storage);

int startVeneziaVipCapture(unsigned int processor, VeneziaVipProcessStorage *storage,
	uint32_t *capacity, const VeneziaDebugImageFormat **geometry)
{
	VeneziaDebugProcessCapture *capture = &storage->captures[processor];
	VeneziaDebugCaptureConfig config;
	uint32_t required, imagePhysical, captureState = 1;
	uint32_t imageVirtual, imageSize, bufferVirtual, bufferPhysical, bufferSize;
	uint32_t scratchVirtual, scratchPhysical, scratchSize, alternate;
	int ret, started;

	if (setVeneziaDebugCaptureCommand(processor, 0))
		return -2141060860;
	ret = getVeneziaVipCaptureSize(0, &g_veneziaVipState, *geometry, &required);
	if (ret < 0)
		return ret;
	if (*capacity < required)
		return -2141060863;
	capture->snapshot = (VeneziaDebugSnapshot *)capture->snapshotRegion.virtualAddress;
	imagePhysical = capture->imageMappingMode == 1 ? 0x20000000 : 0;
	imageVirtual = capture->imageVirtualAddress;
	imageSize = capture->imageSize;
	bufferVirtual = capture->bufferRegion.virtualAddress;
	bufferPhysical = capture->bufferRegion.physicalAddress;
	bufferSize = capture->bufferRegion.size;
	scratchVirtual = capture->scratchRegion.virtualAddress;
	scratchPhysical = capture->scratchRegion.physicalAddress;
	scratchSize = capture->scratchRegion.size;
	config.size = getVeneziaVipRingSize(bufferSize);
	config.startOffset = 4096;
	config.parameters[0] = 0;
	config.parameters[1] = 0;
	config.parameters[2] = 0;
	config.control = 163954;
	config.format = 17;
	config.options = 0;
	config.extraParameters[0] = 0;
	config.extraParameters[1] = 0;
	prepareVeneziaVipCapture(&g_veneziaVipState, storage);
	alternate = !!(capture->flags & 0x1000000);
	started = setVeneziaDebugCaptureOption(processor, alternate);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(42, 3, processor, alternate, started);
	if (started)
		return -2141060860;
	started = startVeneziaDebugCapture(processor, imageVirtual, imagePhysical, imageSize,
		bufferVirtual, bufferPhysical, bufferSize, scratchVirtual, scratchPhysical,
		scratchSize, 0, 0, 0, &config);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(17, 12, processor, imageVirtual, capture->imagePhysicalAddress,
			imageSize, bufferVirtual, bufferPhysical, bufferSize, scratchVirtual,
			scratchPhysical, scratchSize, 0, 0, 0, started);
	if (started)
		return -2141060860;
	while (captureState & 1) {
		started = getVeneziaDebugCaptureState(processor, &captureState);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(38, 3, processor, started, captureState);
		if (started)
			return -2141060860;
		sceKernelDelayThread(1000);
	}
	storage->captures[0].completedCount = 0;
	storage->captures[0].bufferEnd = bufferVirtual + bufferSize;
	return ret;
}

/* name unknown: request debugger suspension and wait for both hardware banks. */
int suspendVeneziaVipDebugger(void)
{
	uint32_t mode __attribute__((aligned(8)));
	uint32_t alternateMode __attribute__((aligned(8)));
	int ret;

	ret = setVeneziaDebuggerMode(3);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(3, 7, 3, 0, 0, 0, ret, 0, 0);
	if (ret)
		return ret == 6 ? 0 : -2141060856;
	for (;;) {
		ret = getVeneziaDebuggerModes(&mode, &alternateMode);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(4, 3, ret, mode, alternateMode);
		if (ret)
			return -2141060856;
		if (mode == 5 && alternateMode == 5)
			return ret;
		sceKernelDelayThread(1000);
	}
}

/* name unknown: validate capture dimensions before programming the debugger. */
int validateVeneziaVipCaptureGeometry(VeneziaVipState *state,
	const VeneziaDebugImageFormat *geometry, uint32_t *size)
{
	uint32_t width, height, imageCount;
	int ret;

	if (!size || !geometry)
		return -2141060862;
	width = geometry->width;
	if (width - 64 > 0x740)
		return -2141060863;
	height = geometry->height;
	if (height - 64 > 0x400)
		return -2141060863;
	imageCount = geometry->imageCount;
	if (imageCount > 16 || imageCount * 3 * height * width > 0x1800001)
		return -2141060863;
	if (width == 1920 && height == 1088 && imageCount > 3)
		width = height = imageCount = 0;
	ret = executeVeneziaDebuggerCommand(0, width, height, imageCount, size);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(16, 6, 0, width, height, imageCount, ret, *size);
	return ret ? -2141060863 : 0;
}

static inline __attribute__((always_inline)) void clearVeneziaVipCallback(VeneziaDebugCallback *callback,
	SceUID uid, SceUID pid)
{
	if ((!uid || callback->uid == uid) && callback->pid == pid) {
		callback->pid = 0;
		callback->uid = 0;
		callback->pendingEvents = 0;
		callback->eventMask = 0;
	}
}

/* name unknown: remove one process's matching debugger notifications. */
int unregisterVeneziaVipCallbacks(unsigned int processor, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage, SceUID uid, SceUID pid)
{
	VeneziaDebugCallback *callbacks = storage->callbackState.callbacks;
	int saved = sceKernelSpinlockLowLockCpuSuspendIntr(&g_veneziaDebugger.spinlock);

	if (!uid) {
		clearVeneziaVipCallback(&callbacks[0], 0, pid);
		clearVeneziaVipCallback(&callbacks[1], 0, pid);
		clearVeneziaVipCallback(&callbacks[2], 0, pid);
		clearVeneziaVipCallback(&callbacks[3], 0, pid);
		clearVeneziaVipCallback(&callbacks[4], 0, pid);
		clearVeneziaVipCallback(&callbacks[5], 0, pid);
	} else {
		clearVeneziaVipCallback(&callbacks[0], uid, pid);
		clearVeneziaVipCallback(&callbacks[1], uid, pid);
		clearVeneziaVipCallback(&callbacks[2], uid, pid);
		clearVeneziaVipCallback(&callbacks[3], uid, pid);
		clearVeneziaVipCallback(&callbacks[4], uid, pid);
		clearVeneziaVipCallback(&callbacks[5], uid, pid);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_veneziaDebugger.spinlock, saved);
	return 0;
}
__attribute__((noinline)) int lockVeneziaVipProcess(struct ThreadmgrFastMutex *mutex);
__attribute__((noinline)) int unlockVeneziaVipProcess(struct ThreadmgrFastMutex *mutex);

/* name unknown: register a callback in the current process's debugger state. */
int registerVeneziaVipCallback(unsigned int processor, SceUID uid, uint32_t eventMask)
{
	VeneziaVipProcessStorage *storage;
	VeneziaDebugCallback *callback;
	unsigned int index;
	int ret, kind, saved;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	if (processor != 1 || (eventMask & 0xffffc4f9))
		return -2141060863;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	ret = scePUIDtoGUID(0, uid);
	if (ret >= 0) {
		kind = sceKernelGetThreadmgrUIDClass(ret);
		if (kind < 0) {
			ret = kind;
		} else if (kind != 8) {
			ret = -2141060863;
		} else {
			saved = sceKernelSpinlockLowLockCpuSuspendIntr(&g_veneziaDebugger.spinlock);
			for (index = 0; index < 6; index++)
				if (!storage->callbackState.callbacks[index].uid)
					break;
			if (index == 6) {
				ret = -2141060858;
			} else {
				callback = &storage->callbackState.callbacks[index];
				callback->pid = sceKernelGetProcessId();
				callback->uid = ret;
				callback->pendingEvents = 0;
				callback->eventMask = eventMask;
				ret = 0;
			}
			sceKernelSpinlockLowUnlockCpuResumeIntr(&g_veneziaDebugger.spinlock, saved);
		}
	}
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

extern int removeVeneziaVipCallbacks(unsigned int processor, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage, SceUID uid, SceUID pid);

/* name unknown: remove the current process's selected debugger callback. */
int unregisterVeneziaVipCallback(unsigned int processor, SceUID uid)
{
	VeneziaVipProcessStorage *storage;
	int ret, kind;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	if (processor != 1)
		return -2141060863;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	if (uid) {
		uid = scePUIDtoGUID(0, uid);
		if (uid < 0) {
			ret = uid;
			goto unlock;
		}
		kind = sceKernelGetThreadmgrUIDClass(uid);
		if (kind < 0) {
			ret = kind;
			goto unlock;
		}
		if (kind != 8) {
			ret = -2141060863;
			goto unlock;
		}
	}
	ret = removeVeneziaVipCallbacks(1, &g_veneziaVipState, storage, uid,
		sceKernelGetProcessId());
unlock:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

extern int beginVeneziaVipCaptureConfiguration(VeneziaVipState *state,
	VeneziaVipProcessStorage *storage);
extern int endVeneziaVipCaptureConfiguration(VeneziaVipState *state,
	VeneziaVipProcessStorage *storage);

/* name unknown: query capture storage while no capture owns processor zero. */
int getVeneziaVipCaptureRequirements(const VeneziaDebugImageFormat *geometry,
	uint32_t *size)
{
	VeneziaVipProcessStorage *storage;
	int ret;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	if (storage->processors[0] == -1) {
		ret = beginVeneziaVipCaptureConfiguration(&g_veneziaVipState, storage);
		if (ret >= 0) {
			ret = -2141060860;
			if (!setVeneziaDebugCaptureCommand(0, 0)) {
				ret = getVeneziaVipCaptureSize(0, &g_veneziaVipState, geometry, size);
				endVeneziaVipCaptureConfiguration(&g_veneziaVipState, storage);
			}
		}
	} else {
		ret = -2141060858;
	}
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

__attribute__((noinline)) int calculateVeneziaVipCaptureStorage(unsigned int processorCount,
	unsigned int mode, const VeneziaDebugImageGeometry *geometry, SceSize *total,
	SceSize *image, SceSize *buffer, SceSize *scratch);

/* name unknown: query total capture storage for a single processor. */
int getVeneziaVipCaptureStorageSize(const VeneziaDebugImageGeometry *geometry,
	SceSize *size)
{
	VeneziaVipProcessStorage *storage;
	SceSize image __attribute__((aligned(8)));
	SceSize buffer __attribute__((aligned(8)));
	SceSize scratch;
	int ret;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	if (storage->processors[0] == -1)
		ret = calculateVeneziaVipCaptureStorage(1, 0, geometry, size,
			&image, &buffer, &scratch);
	else
		ret = -2141060858;
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

__attribute__((noinline)) int isVeneziaVipImageInMainMemory(uint32_t address, SceSize size);
int distributeVeneziaVipCaptureMemory(VeneziaVipProcessStorage *storage,
	unsigned int processorCount, VeneziaVipMemoryLayout *layout);
__attribute__((noinline)) int setVeneziaVipCaptureImage(VeneziaDebugProcessCapture *capture,
	const VeneziaVipCaptureParam *param);

/* name unknown: initialize processor-zero capture buffers and saved parameters. */
int registerVeneziaVipCapture(unsigned int *processor, const VeneziaVipCaptureParam *param)
{
	VeneziaVipProcessStorage *storage;
	VeneziaDebugProcessCapture *capture;
	VeneziaVipMemoryRegion region;
	SceSize total __attribute__((aligned(8)));
	SceSize image __attribute__((aligned(8)));
	SceSize buffer __attribute__((aligned(8)));
	SceSize scratch __attribute__((aligned(8)));
	int ret;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	if (!param || !processor || !param->imageVirtualAddress || !param->imageFormat)
		return -2141060862;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	if (storage->processors[0] != -1) {
		ret = -2141060858;
		goto unlock;
	}
	capture = &storage->captures[0];
	ret = isVeneziaVipImageInMainMemory(param->imageVirtualAddress, param->imageSize);
	if (ret < 0)
		goto invalid;
	capture->imageMappingMode = !!ret;
	if (!param->storageVirtualAddress)
		goto invalid;
	ret = calculateVeneziaVipCaptureStorage(1, 0, param->geometry, &total,
		&image, &buffer, &scratch);
	if (ret < 0)
		goto unlock;
	if (param->storageSize < total) {
		ret = -2141060863;
		goto unlock;
	}
	if (sceKernelVAtoPA((const void *)param->storageVirtualAddress,
		&region.physicalAddress) < 0)
		goto invalid;
	region.virtualAddress = param->storageVirtualAddress;
	region.size = param->storageSize;
	storage->captureMemory.control = 1;
	setVeneziaVipMemoryRegion(&storage->captureMemory.scratch, &region, 0, image);
	setVeneziaVipMemoryRegion(&storage->captureMemory.buffer, &region, image, buffer);
	setVeneziaVipMemoryRegion(&storage->captureMemory.snapshot, &region, image + buffer, scratch);
	storage->memoryLayout = &storage->captureMemory;
	ret = distributeVeneziaVipCaptureMemory(storage, param->geometry->processorCount,
		&storage->captureMemory);
	if (ret < 0)
		goto unlock;
	capture->snapshot = (VeneziaDebugSnapshot *)capture->snapshotRegion.virtualAddress;
	ret = setVeneziaVipCaptureImage(capture, param);
	if (ret < 0)
		goto unlock;
	ret = beginVeneziaVipCaptureConfiguration(&g_veneziaVipState, storage);
	if (ret < 0)
		goto unlock;
	capture->captureParam = *param;
	capture->imageFormat = *param->imageFormat;
	capture->captureParam.imageFormat = &capture->imageFormat;
	storage->processors[0] = 0;
	*processor = 1;
	capture->flags = 271;
	if (storage->state & 0x1000000)
		capture->flags = 0x100010f;
	storage->state |= 0x100;
	capture->completedCount = 0;
	goto unlock;
invalid:
	ret = -2141060862;
unlock:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

extern int stopVeneziaVipCapture(unsigned int processor, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage);
int releaseVeneziaVipCaptureMemory(VeneziaVipProcessStorage *storage);

/* name unknown: detach a process's capture and clear its ownership flags. */
int unregisterVeneziaVipCapture(unsigned int processor, int wait, SceUID pid)
{
	VeneziaVipProcessStorage *storage;
	int captureProcessor, ret = 0;

	if (wait) {
		ret = waitVeneziaVipProcessReady();
		if (ret < 0)
			return ret;
	}
	storage = _sceVipGetVipProcessResource(pid);
	if (!storage)
		return -2141060856;
	if (processor != 1)
		return -2141060863;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	captureProcessor = storage->processors[0];
	if (captureProcessor == -1) {
		ret = -2141060859;
	} else {
		removeVeneziaVipCallbacks(1, &g_veneziaVipState, storage, 0, pid);
		stopVeneziaVipCapture(captureProcessor, &g_veneziaVipState, storage);
		releaseVeneziaVipCaptureMemory(storage);
		storage->memoryLayout = 0;
		endVeneziaVipCaptureConfiguration(&g_veneziaVipState, storage);
	}
	storage->processors[0] = -1;
	storage->captures[0].flags &= ~0x100;
	storage->state &= ~0x100;
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

extern int dispatchVeneziaVipCaptureRemoval(unsigned int implementation,
	unsigned int processor, int wait, SceUID pid);

/* name unknown: select the primary implementation's capture teardown. */
int unregisterVeneziaVipCaptureForProcess(unsigned int processor, int wait, SceUID pid)
{
	return dispatchVeneziaVipCaptureRemoval(0, processor, wait, pid);
}

/* name unknown: report usable trace bytes after the reserved ring prefix. */
int getVeneziaVipTraceAvailable(unsigned int processor)
{
	VeneziaVipProcessStorage *storage;
	SceSize available;
	int ret, captureProcessor, capacity;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	if (processor != 1)
		return -2141060863;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	captureProcessor = storage->processors[0];
	ret = getVeneziaDebugTraceAvailable(captureProcessor, &available);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(31, 3, captureProcessor, ret, available);
	if (ret) {
		if (!(storage->captures[0].flags & 0x107)) {
			ret = -2141060856;
			goto unlock;
		}
		capacity = storage->captures[0].bufferRegion.size;
	} else {
		capacity = available;
	}
	ret = capacity <= 1024 ? 0 : capacity - 1024;
unlock:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

/* name unknown: this entry rejects the operation after validating process state. */
int rejectVeneziaVipProcessOperation(void)
{
	int ret = waitVeneziaVipProcessReady();

	if (ret < 0)
		return ret;
	return _sceVipGetVipProcessResource(0) ? -2141060863 : -2141060856;
}

int calculateVeneziaVipVideoInputSize(const VeneziaVipVideoInput *input);
extern int submitVeneziaVipVideoInput(unsigned int processor, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage, const VeneziaVipVideoInput *input);

/* name unknown: submit video input, wait for capture events, and collect snapshots. */
int processVeneziaVipCapture(unsigned int processor, const VeneziaVipVideoInput *input,
	VeneziaDebugSnapshotList *list, unsigned int *completed)
{
	VeneziaVipProcessStorage *storage;
	VeneziaDebugProcessCapture *capture;
	unsigned int captureProcessor, activeProcessor, consumed;
	SceSize inputSize = 0, event;
	SceUInt timeout = 8000;
	int ret, capacity, deferredError = 0;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	if (!completed)
		return -2141060863;
	*completed = 0;
	if (!list || processor != 1)
		return -2141060863;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	capture = &storage->captures[0];
	if (input && !((input->flags & 1) && !input->additionalData[0].data) &&
	    !((input->flags & 2) && !input->additionalData[1].data) && input->payload.data) {
		ret = calculateVeneziaVipVideoInputSize(input);
		if (ret < 0)
			goto unlock;
		inputSize = ret;
	}
	captureProcessor = storage->processors[0];
	if (captureProcessor == (unsigned int)-1) {
		ret = -2141060859;
		goto unlock;
	}
	if (capture->flags & 7) {
		capture->flags &= ~7;
		stopVeneziaVipCapture(captureProcessor, &g_veneziaVipState, storage);
		ret = startVeneziaVipCapture(captureProcessor, storage,
			&capture->captureParam.imageSize, &capture->captureParam.imageFormat);
		if (ret < 0)
			goto unlock;
		activeProcessor = storage->processors[0];
		capture->flags &= ~8;
	} else {
		activeProcessor = captureProcessor;
	}
	for (consumed = 0; consumed < capture->completedCount; consumed++) {
		ret = consumeVeneziaDebugTracePacket(activeProcessor);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(44, 2, activeProcessor, ret);
		if (ret == 10) {
			ret = -2141060854;
			capture->completedCount = 0;
			goto finished;
		}
		if (ret != 3 && ret != 0) {
			ret = -2141060856;
			capture->completedCount = 0;
			goto finished;
		}
	}
	capture->completedCount = 0;
	ret = collectVeneziaDebugSnapshots(0, storage, list, 0);
	if (ret < 0)
		goto finished;
	if (inputSize) {
		activeProcessor = storage->processors[0];
		ret = getVeneziaDebugTraceAvailable(activeProcessor, &event);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(31, 3, activeProcessor, ret, event);
		if (ret) {
			if (!(capture->flags & 0x107)) {
				ret = -2141060856;
				goto finished;
			}
			event = capture->bufferRegion.size;
		}
		capacity = event;
		if (capacity <= 1024 || inputSize > (unsigned int)(capacity - 1024)) {
			deferredError = -2141060855;
		} else {
			ret = submitVeneziaVipVideoInput(0, &g_veneziaVipState, storage, input);
			if (ret < 0)
				goto finished;
		}
	}
	if (list->count && list->count <= capture->completedCount)
		timeout = 1;
	event = 0;
	if (setVeneziaDebugTraceFlag(0, 1))
		goto hardwareError;
	ret = sceKernelWaitEventFlag(g_veneziaVipState.eventFlag, 15110, 5, &event, &timeout);
	if (setVeneziaDebugTraceFlag(0, 0) || clearVeneziaDebugTraceStatus(0))
		goto hardwareError;
	if (ret < 0) {
		if (ret != -2147319803)
			goto finished;
		ret = 0;
	}
	if (event & 0x200) {
		ret = stopVeneziaVipCapture(captureProcessor, &g_veneziaVipState, storage);
		ret = ret == -2141060851 ? -2141060851 : -2141060856;
		capture->flags |= 7;
		goto finished;
	}
	if (event & 0x800) {
		ret = -2141060854;
		goto finished;
	}
	if (event & 0x100) {
		ret = -2141060853;
		goto finished;
	}
	if (event & 0x1000) {
		ret = -2141060851;
		capture->flags |= 7;
		goto finished;
	}
	if (event & 0x2000) {
		ret = -2141060850;
		goto finished;
	}
	if (event & 2) {
		ret = collectVeneziaDebugSnapshots(0, storage, list, capture->completedCount);
		if (ret < 0)
			goto finished;
	}
	if (deferredError)
		ret = deferredError;
	goto finished;
hardwareError:
	ret = -2141060860;
finished:
	*completed = capture->completedCount;
unlock:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

/* name unknown: submit video input with an optional caller-selected timeout. */
int submitVeneziaVipCapture(unsigned int processor, const VeneziaVipVideoInput *input,
	const SceUInt *timeoutValue)
{
	VeneziaDebugSnapshotList snapshots;
	VeneziaDebugSnapshotList *list = &snapshots;
	VeneziaVipProcessStorage *storage;
	VeneziaDebugProcessCapture *capture;
	unsigned int captureProcessor, activeProcessor, consumed;
	SceSize inputSize = 0, event;
	SceUInt timeout = timeoutValue ? *timeoutValue : 8000;
	int ret, capacity, deferredError = 0;

	snapshots.count = 0;
	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	if (processor != 1)
		return -2141060863;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	capture = &storage->captures[0];
	if (input && !((input->flags & 1) && !input->additionalData[0].data) &&
	    !((input->flags & 2) && !input->additionalData[1].data) && input->payload.data) {
		ret = calculateVeneziaVipVideoInputSize(input);
		if (ret < 0)
			goto unlock;
		inputSize = ret;
	}
	captureProcessor = storage->processors[0];
	if (captureProcessor == (unsigned int)-1) {
		ret = -2141060859;
		goto unlock;
	}
	if (capture->flags & 7) {
		capture->flags &= ~7;
		stopVeneziaVipCapture(captureProcessor, &g_veneziaVipState, storage);
		ret = startVeneziaVipCapture(captureProcessor, storage,
			&capture->captureParam.imageSize, &capture->captureParam.imageFormat);
		if (ret < 0)
			goto unlock;
		activeProcessor = storage->processors[0];
		capture->flags &= ~8;
	} else {
		activeProcessor = captureProcessor;
	}
	for (consumed = 0; consumed < capture->completedCount; consumed++) {
		ret = consumeVeneziaDebugTracePacket(activeProcessor);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(44, 2, activeProcessor, ret);
		if (ret == 10) {
			ret = -2141060854;
			capture->completedCount = 0;
			goto finished;
		}
		if (ret != 3 && ret != 0) {
			ret = -2141060856;
			capture->completedCount = 0;
			goto finished;
		}
	}
	capture->completedCount = 0;
	ret = collectVeneziaDebugSnapshots(0, storage, list, 0);
	if (ret < 0)
		goto finished;
	if (inputSize) {
		activeProcessor = storage->processors[0];
		ret = getVeneziaDebugTraceAvailable(activeProcessor, &event);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(31, 3, activeProcessor, ret, event);
		if (ret) {
			if (!(capture->flags & 0x107)) {
				ret = -2141060856;
				goto finished;
			}
			event = capture->bufferRegion.size;
		}
		capacity = event;
		if (capacity <= 1024 || inputSize > (unsigned int)(capacity - 1024)) {
			deferredError = -2141060855;
		} else {
			ret = submitVeneziaVipVideoInput(0, &g_veneziaVipState, storage, input);
			if (ret < 0)
				goto finished;
		}
	}
	if (list->count && list->count <= capture->completedCount)
		timeout = 1;
	event = 0;
	if (setVeneziaDebugTraceFlag(0, 1))
		goto hardwareError;
	ret = sceKernelWaitEventFlag(g_veneziaVipState.eventFlag, 15108, 5, &event, &timeout);
	if (setVeneziaDebugTraceFlag(0, 0) || clearVeneziaDebugTraceStatus(0))
		goto hardwareError;
	if (ret < 0) {
		if (ret != -2147319803)
			goto finished;
		ret = 0;
	}
	if (event & 0x200) {
		ret = stopVeneziaVipCapture(captureProcessor, &g_veneziaVipState, storage);
		ret = ret == -2141060851 ? -2141060851 : -2141060856;
		capture->flags |= 7;
		goto finished;
	}
	if (event & 0x800) {
		ret = -2141060854;
		goto finished;
	}
	if (event & 0x100) {
		ret = -2141060853;
		goto finished;
	}
	if (event & 0x1000) {
		ret = -2141060851;
		capture->flags |= 7;
		goto finished;
	}
	if (event & 0x2000) {
		ret = -2141060850;
		goto finished;
	}
	if (event & 2) {
		ret = collectVeneziaDebugSnapshots(0, storage, list, capture->completedCount);
		if (ret < 0)
			goto finished;
	}
	if (deferredError)
		ret = deferredError;
	goto finished;
hardwareError:
	ret = -2141060860;
finished:
unlock:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

typedef struct VeneziaVipRawInput {
	uint64_t presentationTimestamp;
	uint64_t decodingTimestamp;
	VeneziaVipInputBuffer payload;
} VeneziaVipRawInput;

typedef struct VeneziaVipPesHeader {
	uint8_t startCode[3];
	uint8_t streamId;
	uint16_t packetLength;
	uint8_t flags;
	uint8_t timestampFlags;
	uint8_t headerLength;
	uint8_t presentationTimestamp[5];
	uint8_t decodingTimestamp[5];
	uint8_t stuffing[12];
	uint8_t payloadPrefix;
} VeneziaVipPesHeader;

static const VeneziaVipPesHeader g_veneziaVipPesHeader = {
	.startCode = { 0, 0, 1 },
	.streamId = 0xe0,
	.packetLength = 0,
	.flags = 0x85,
	.timestampFlags = 0,
	.headerLength = 22,
	.presentationTimestamp = { 255, 255, 255, 255, 255 },
	.decodingTimestamp = { 255, 255, 255, 255, 255 },
	.stuffing = { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
	.payloadPrefix = 0,
};

__attribute__((noinline)) int encodeVeneziaVipPesTimestamps(VeneziaVipPesHeader *header,
	const uint64_t *pts, const uint64_t *dts);
extern int submitVeneziaVipInputBuffers(unsigned int processor, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage, const VeneziaVipInputBufferList *buffers);

/* name unknown: submit a raw payload with its PES timestamps and collect snapshots. */
int processVeneziaVipRawCapture(unsigned int processor, const VeneziaVipRawInput *input,
	VeneziaDebugSnapshotList *list, unsigned int *completed)
{
	VeneziaVipPesHeader header;
	VeneziaVipInputBuffer buffers[2];
	VeneziaVipInputBufferList inputBuffers;
	VeneziaVipProcessStorage *storage;
	VeneziaDebugProcessCapture *capture;
	unsigned int captureProcessor, activeProcessor, consumed;
	SceSize inputSize = 0, event;
	SceUInt timeout = 8000;
	int ret, capacity, deferredError = 0;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	if (!completed)
		return -2141060863;
	*completed = 0;
	if (!list || processor != 1)
		return -2141060863;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	capture = &storage->captures[0];
	captureProcessor = storage->processors[0];
	if (captureProcessor == (unsigned int)-1) {
		ret = -2141060859;
		goto unlock;
	}
	inputSize = input && input->payload.data ? input->payload.size : 0;
	if (capture->flags & 7) {
		capture->flags &= ~7;
		stopVeneziaVipCapture(captureProcessor, &g_veneziaVipState, storage);
		ret = startVeneziaVipCapture(captureProcessor, storage,
			&capture->captureParam.imageSize, &capture->captureParam.imageFormat);
		if (ret < 0)
			goto unlock;
		activeProcessor = storage->processors[0];
		capture->flags &= ~8;
	} else {
		activeProcessor = captureProcessor;
	}
	for (consumed = 0; consumed < capture->completedCount; consumed++) {
		ret = consumeVeneziaDebugTracePacket(activeProcessor);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(44, 2, activeProcessor, ret);
		if (ret == 10) {
			ret = -2141060854;
			capture->completedCount = 0;
			goto finished;
		}
		if (ret != 3 && ret != 0) {
			ret = -2141060856;
			capture->completedCount = 0;
			goto finished;
		}
	}
	capture->completedCount = 0;
	ret = collectVeneziaDebugSnapshots(0, storage, list, 0);
	if (ret < 0)
		goto finished;
	if (inputSize) {
		activeProcessor = storage->processors[0];
		ret = getVeneziaDebugTraceAvailable(activeProcessor, &event);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(31, 3, activeProcessor, ret, event);
		if (ret) {
			if (!(capture->flags & 0x107)) {
				ret = -2141060856;
				goto finished;
			}
			event = capture->bufferRegion.size;
		}
		capacity = event;
		capacity = capacity > 1024 ? capacity - 1024 : 0;
		if (inputSize + sizeof(header) <= (unsigned int)capacity) {
			header = g_veneziaVipPesHeader;
			ret = encodeVeneziaVipPesTimestamps(&header,
				input->presentationTimestamp == UINT64_MAX ? 0 : &input->presentationTimestamp,
				input->decodingTimestamp == UINT64_MAX ? 0 : &input->decodingTimestamp);
			if (ret < 0)
				goto finished;
			buffers[0].data = &header;
			buffers[0].size = sizeof(header);
			buffers[1].data = input->payload.data;
			buffers[1].size = inputSize;
			inputBuffers.count = 2;
			inputBuffers.buffers = buffers;
			ret = submitVeneziaVipInputBuffers(0, &g_veneziaVipState, storage, &inputBuffers);
			if (ret < 0)
				goto finished;
		} else {
			deferredError = -2141060855;
		}
	}
	if (list->count && list->count <= capture->completedCount)
		timeout = 1;
	event = 0;
	if (setVeneziaDebugTraceFlag(0, 1))
		goto hardwareError;
	ret = sceKernelWaitEventFlag(g_veneziaVipState.eventFlag, 15110, 5, &event, &timeout);
	if (setVeneziaDebugTraceFlag(0, 0) || clearVeneziaDebugTraceStatus(0))
		goto hardwareError;
	if (ret < 0) {
		if (ret != -2147319803)
			goto finished;
		ret = 0;
	}
	if (event & 0x200) {
		ret = stopVeneziaVipCapture(captureProcessor, &g_veneziaVipState, storage);
		ret = ret == -2141060851 ? -2141060851 : -2141060856;
		capture->flags |= 7;
		goto finished;
	}
	if (event & 0x800) {
		ret = -2141060854;
		goto finished;
	}
	if (event & 0x100) {
		ret = -2141060853;
		goto finished;
	}
	if (event & 0x1000) {
		ret = -2141060851;
		capture->flags |= 7;
		goto finished;
	}
	if (event & 0x2000) {
		ret = -2141060850;
		goto finished;
	}
	if (event & 2) {
		ret = collectVeneziaDebugSnapshots(0, storage, list, capture->completedCount);
		if (ret < 0)
			goto finished;
	}
	if (deferredError)
		ret = deferredError;
	goto finished;
hardwareError:
	ret = -2141060860;
finished:
	*completed = capture->completedCount;
unlock:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

/* name unknown: submit timestamped raw data using the supplied timeout. */
int submitVeneziaVipRawCapture(unsigned int processor, const VeneziaVipRawInput *input,
	const SceUInt *timeoutValue)
{
	VeneziaDebugSnapshotList snapshots;
	VeneziaDebugSnapshotList *list = &snapshots;
	VeneziaVipPesHeader header;
	VeneziaVipInputBuffer buffers[2];
	VeneziaVipInputBufferList inputBuffers;
	VeneziaVipProcessStorage *storage;
	VeneziaDebugProcessCapture *capture;
	unsigned int captureProcessor, activeProcessor, consumed;
	SceSize inputSize = 0, event;
	SceUInt timeout = timeoutValue ? *timeoutValue : 8000;
	int ret, capacity, deferredError = 0;

	snapshots.count = 0;
	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	if (processor != 1)
		return -2141060863;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	capture = &storage->captures[0];
	captureProcessor = storage->processors[0];
	if (captureProcessor == (unsigned int)-1) {
		ret = -2141060859;
		goto unlock;
	}
	inputSize = input && input->payload.data ? input->payload.size : 0;
	if (capture->flags & 7) {
		capture->flags &= ~7;
		stopVeneziaVipCapture(captureProcessor, &g_veneziaVipState, storage);
		ret = startVeneziaVipCapture(captureProcessor, storage,
			&capture->captureParam.imageSize, &capture->captureParam.imageFormat);
		if (ret < 0)
			goto unlock;
		activeProcessor = storage->processors[0];
		capture->flags &= ~8;
	} else {
		activeProcessor = captureProcessor;
	}
	for (consumed = 0; consumed < capture->completedCount; consumed++) {
		ret = consumeVeneziaDebugTracePacket(activeProcessor);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(44, 2, activeProcessor, ret);
		if (ret == 10) {
			ret = -2141060854;
			capture->completedCount = 0;
			goto finished;
		}
		if (ret != 3 && ret != 0) {
			ret = -2141060856;
			capture->completedCount = 0;
			goto finished;
		}
	}
	capture->completedCount = 0;
	ret = collectVeneziaDebugSnapshots(0, storage, list, 0);
	if (ret < 0)
		goto finished;
	if (inputSize) {
		activeProcessor = storage->processors[0];
		ret = getVeneziaDebugTraceAvailable(activeProcessor, &event);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(31, 3, activeProcessor, ret, event);
		if (ret) {
			if (!(capture->flags & 0x107)) {
				ret = -2141060856;
				goto finished;
			}
			event = capture->bufferRegion.size;
		}
		capacity = event;
		capacity = capacity > 1024 ? capacity - 1024 : 0;
		if (inputSize + sizeof(header) <= (unsigned int)capacity) {
			header = g_veneziaVipPesHeader;
			ret = encodeVeneziaVipPesTimestamps(&header,
				input->presentationTimestamp == UINT64_MAX ? 0 : &input->presentationTimestamp,
				input->decodingTimestamp == UINT64_MAX ? 0 : &input->decodingTimestamp);
			if (ret < 0)
				goto finished;
			buffers[0].data = &header;
			buffers[0].size = sizeof(header);
			buffers[1].data = input->payload.data;
			buffers[1].size = inputSize;
			inputBuffers.count = 2;
			inputBuffers.buffers = buffers;
			ret = submitVeneziaVipInputBuffers(0, &g_veneziaVipState, storage, &inputBuffers);
			if (ret < 0)
				goto finished;
		} else {
			deferredError = -2141060855;
		}
	}
	if (list->count && list->count <= capture->completedCount)
		timeout = 1;
	event = 0;
	if (setVeneziaDebugTraceFlag(0, 1))
		goto hardwareError;
	ret = sceKernelWaitEventFlag(g_veneziaVipState.eventFlag, 15108, 5, &event, &timeout);
	if (setVeneziaDebugTraceFlag(0, 0) || clearVeneziaDebugTraceStatus(0))
		goto hardwareError;
	if (ret < 0) {
		if (ret != -2147319803)
			goto finished;
		ret = 0;
	}
	if (event & 0x200) {
		ret = stopVeneziaVipCapture(captureProcessor, &g_veneziaVipState, storage);
		ret = ret == -2141060851 ? -2141060851 : -2141060856;
		capture->flags |= 7;
		goto finished;
	}
	if (event & 0x800) {
		ret = -2141060854;
		goto finished;
	}
	if (event & 0x100) {
		ret = -2141060853;
		goto finished;
	}
	if (event & 0x1000) {
		ret = -2141060851;
		capture->flags |= 7;
		goto finished;
	}
	if (event & 0x2000) {
		ret = -2141060850;
		goto finished;
	}
	if (event & 2) {
		ret = collectVeneziaDebugSnapshots(0, storage, list, capture->completedCount);
		if (ret < 0)
			goto finished;
	}
	if (deferredError)
		ret = deferredError;
	goto finished;
hardwareError:
	ret = -2141060860;
finished:
unlock:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

/* name unknown: collect pending snapshots and wait for capture completion events. */
int waitVeneziaVipCapture(unsigned int processor, VeneziaDebugSnapshotList *list,
	unsigned int *completed, const SceUInt *timeoutValue)
{
	VeneziaVipProcessStorage *storage;
	VeneziaDebugProcessCapture *capture;
	unsigned int captureProcessor, activeProcessor, consumed;
	SceSize event;
	SceUInt timeout = timeoutValue ? *timeoutValue : 8000;
	int ret;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	if (!completed)
		return -2141060863;
	*completed = 0;
	if (!list || processor != 1)
		return -2141060863;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	capture = &storage->captures[0];
	captureProcessor = storage->processors[0];
	if (captureProcessor == (unsigned int)-1) {
		ret = -2141060859;
		goto unlock;
	}
	if (capture->flags & 8) {
		ret = 0;
		goto unlock;
	}
	activeProcessor = captureProcessor;
	for (consumed = 0; consumed < capture->completedCount; consumed++) {
		ret = consumeVeneziaDebugTracePacket(activeProcessor);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(44, 2, activeProcessor, ret);
		if (ret == 10) {
			ret = -2141060854;
			capture->completedCount = 0;
			goto finished;
		}
		if (ret != 3 && ret != 0) {
			ret = -2141060856;
			capture->completedCount = 0;
			goto finished;
		}
	}
	capture->completedCount = 0;
	ret = collectVeneziaDebugSnapshots(0, storage, list, 0);
	if (ret < 0)
		goto finished;
	if (list->count && list->count <= capture->completedCount)
		timeout = 1;
	event = 0;
	if (setVeneziaDebugTraceFlag(0, 1))
		goto hardwareError;
	ret = sceKernelWaitEventFlag(g_veneziaVipState.eventFlag, 15110, 5, &event, &timeout);
	if (setVeneziaDebugTraceFlag(0, 0) || clearVeneziaDebugTraceStatus(0))
		goto hardwareError;
	if (ret < 0) {
		if (ret != -2147319803)
			goto finished;
		ret = 0;
	}
	if (event & 0x200) {
		ret = stopVeneziaVipCapture(captureProcessor, &g_veneziaVipState, storage);
		ret = ret == -2141060851 ? -2141060851 : -2141060856;
		capture->flags |= 7;
		goto finished;
	}
	if (event & 0x800) {
		ret = -2141060854;
		goto finished;
	}
	if (event & 0x100) {
		ret = -2141060853;
		goto finished;
	}
	if (event & 0x1000) {
		ret = -2141060851;
		capture->flags |= 7;
		goto finished;
	}
	if (event & 0x2000) {
		ret = -2141060850;
		goto finished;
	}
	if (event & 2) {
		ret = collectVeneziaDebugSnapshots(0, storage, list, capture->completedCount);
		if (ret < 0)
			goto finished;
	}
	goto finished;
hardwareError:
	ret = -2141060860;
finished:
	*completed = capture->completedCount;
unlock:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

/* name unknown: request trace draining and collect until the output list is full. */
int drainVeneziaVipCapture(unsigned int processor, VeneziaDebugSnapshotList *list,
	unsigned int *completed)
{
	VeneziaVipProcessStorage *storage;
	VeneziaDebugProcessCapture *capture;
	unsigned int captureProcessor, consumed, collected = 0, previous = 0, current;
	SceSize event;
	SceUInt timeout;
	int requestState;
	int ret, requestRet;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	if (!completed)
		return -2141060863;
	*completed = 0;
	if (!list || processor != 1)
		return -2141060863;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	capture = &storage->captures[0];
	if (!(capture->flags & 1)) {
		captureProcessor = storage->processors[0];
		capture->flags |= 1;
		ret = requestVeneziaDebugTrace(captureProcessor);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(37, 2, captureProcessor, ret);
		if (ret) {
			ret = -2141060856;
			goto unlock;
		}
	}
	captureProcessor = storage->processors[0];
	for (consumed = 0; consumed < capture->completedCount; consumed++) {
		ret = consumeVeneziaDebugTracePacket(captureProcessor);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(44, 2, captureProcessor, ret);
		if (ret == 10) {
			ret = -2141060854;
			capture->completedCount = 0;
			goto finished;
		}
		if (ret != 3 && ret != 0) {
			ret = -2141060856;
			capture->completedCount = 0;
			goto finished;
		}
	}
	capture->completedCount = 0;
	for (;;) {
		ret = collectVeneziaDebugSnapshots(0, storage, list, collected);
		if (ret < 0)
			break;
		current = capture->completedCount;
		collected += current - previous;
		if (collected >= list->count || ((capture->flags & 7) == 7 && current == previous))
			break;
		event = 0;
		timeout = 8000;
		if (setVeneziaDebugTraceFlag(0, 1))
			goto hardwareError;
		ret = sceKernelWaitEventFlag(g_veneziaVipState.eventFlag, 15114, 5, &event, &timeout);
		if (setVeneziaDebugTraceFlag(0, 0) || clearVeneziaDebugTraceStatus(0))
			goto hardwareError;
		if (ret < 0 && ret != -2147319803)
			break;
		if (!(capture->flags & 2)) {
			captureProcessor = storage->processors[0];
			requestRet = getVeneziaDebugTraceRequestState(captureProcessor, &requestState);
			if (g_veneziaDebugger.log)
				g_veneziaDebugger.log(39, 3, captureProcessor, requestRet, requestState);
			if (!requestRet) {
				if (!requestState)
					capture->flags |= 2;
			} else if (requestRet != 7) {
				ret = -2141060856;
				break;
			}
		}
		if (capture->flags & 2) {
			if (event & 8)
				capture->flags |= 4;
		} else if (event & 0x200) {
			ret = stopVeneziaVipCapture(0, &g_veneziaVipState, storage);
			ret = ret == -2141060851 ? -2141060851 : -2141060856;
			capture->flags |= 7;
			break;
		}
		if (event & 0x800) {
			ret = -2141060854;
			break;
		}
		if (event & 0x100) {
			ret = -2141060853;
			break;
		}
		if (event & 0x1000) {
			ret = -2141060851;
			capture->flags |= 7;
			break;
		}
		if (event & 0x2000) {
			ret = -2141060850;
			break;
		}
		previous = capture->completedCount;
	}
	goto finished;
hardwareError:
	ret = -2141060860;
finished:
	*completed = collected;
unlock:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

/* name unknown: mark the capture for restart at its next processing call. */
int resetVeneziaVipCapture(unsigned int processor)
{
	VeneziaVipProcessStorage *storage;
	int ret;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	if (processor != 1)
		return -2141060863;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	if (storage->processors[0] == -1)
		ret = -2141060859;
	else
		storage->captures[0].flags |= 7;
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

/* name unknown: consume every packet corresponding to previously returned snapshots. */
int consumeVeneziaVipCapture(unsigned int processor)
{
	VeneziaVipProcessStorage *storage;
	VeneziaDebugProcessCapture *capture;
	unsigned int captureProcessor, consumed = 0;
	int ret;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	if (processor != 1)
		return -2141060863;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	if (!g_veneziaVipState.callbackState) {
		ret = -2141060856;
		goto unlock;
	}
	capture = &storage->captures[0];
	captureProcessor = storage->processors[0];
	if (capture->completedCount) {
		do {
			++consumed;
			ret = consumeVeneziaDebugTracePacket(captureProcessor);
			if (g_veneziaDebugger.log)
				g_veneziaDebugger.log(44, 2, captureProcessor, ret);
			if (ret == 10) {
				ret = -2141060854;
				goto consumed;
			}
			if (ret != 3 && ret != 0) {
				ret = -2141060856;
				goto consumed;
			}
		} while (consumed < capture->completedCount);
	}
	ret = 0;
consumed:
	capture->completedCount = 0;
unlock:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

__attribute__((noinline)) int stopVeneziaVipDebugger(VeneziaVipState *state,
	VeneziaVipProcessStorage *storage);

/* name unknown: suspend the debugger and copy its exposed regions into capture storage. */
int snapshotVeneziaVipDebugger(VeneziaVipProcessStorage *storage, unsigned int flags)
{
	VeneziaVipSnapshotStorage *snapshot = &storage->snapshotStorage;
	VeneziaVipSnapshotRegion *region;
	VeneziaDebuggerState *debuggerState;
	unsigned int memoryCount, processorCount, index, processor;
	uint32_t physical, virtual;
	SceSize size, copied = 0;
	char *destination;
	int ret;

	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	ret = suspendVeneziaVipDebugger();
	if (ret < 0)
		goto unlock;
	if (flags & 1) {
		setVeneziaVipMemoryRegion(&snapshot->buffer,
			&storage->captures[0].snapshotRegion, 4096, 520192);
		destination = (char *)snapshot->buffer.virtualAddress;
		ret = getVeneziaDebugMemoryRegionCount(&memoryCount);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(5, 2, ret, memoryCount);
		if (ret != 0)
			goto failed;
		snapshot->memoryRegionCount = memoryCount;
		for (index = 0; index < memoryCount; ++index) {
			ret = getVeneziaDebugMemoryRegion(index, &physical, &virtual, &size);
			if (g_veneziaDebugger.log)
				g_veneziaDebugger.log(6, 5, index, ret, physical, virtual, size);
			if (ret != 0 || index == 16)
				goto failed;
			region = &snapshot->regions[index];
			region->physicalAddress = physical;
			region->virtualAddress = virtual;
			region->size = size;
			region->copyAddress = (uint32_t)destination;
			if (copied + size > snapshot->buffer.size)
				goto failed;
			memcpy(destination, (const void *)region->virtualAddress, size);
			destination += size;
			copied += size;
		}
		ret = getVeneziaDebugProcessorCount(&processorCount);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(9, 2, ret, processorCount);
		if (ret != 0)
			goto failed;
		snapshot->processorCount = processorCount;
		for (processor = 0; processor < processorCount; ++processor) {
			ret = getVeneziaDebuggerState(processor, &debuggerState, &size);
			if (g_veneziaDebugger.log)
				g_veneziaDebugger.log(10, 4, processor, ret, debuggerState, size);
			if (ret != 0)
				goto failed;
			ret = sceKernelVAtoPA(debuggerState, &physical);
			if (ret < 0 || processor == 16 - index)
				goto failed;
			region = &snapshot->regions[index + processor];
			region->physicalAddress = physical;
			region->copyAddress = (uint32_t)destination;
			region->virtualAddress = (uint32_t)debuggerState;
			region->size = size;
			if (copied + size > snapshot->buffer.size)
				goto failed;
			memcpy(destination, debuggerState, size);
			destination += size;
			copied += size;
		}
	}
	if (flags & 2)
		stopVeneziaVipDebugger(&g_veneziaVipState, storage);
	goto unlock;
failed:
	ret = -2141060856;
unlock:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

__attribute__((noinline)) int activateVeneziaVipDebugger(VeneziaVipState *state,
	VeneziaVipProcessStorage *storage);

/* name unknown: restore captured debugger memory and wait for its running mode. */
int restoreVeneziaVipDebugger(VeneziaVipProcessStorage *storage)
{
	VeneziaVipSnapshotStorage *snapshot = &storage->snapshotStorage;
	VeneziaVipSnapshotRegion *region;
	unsigned int index, processor, mode, alternate;
	int ret;

	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	activateVeneziaVipDebugger(&g_veneziaVipState, storage);
	prepareVeneziaVipCapture(&g_veneziaVipState, storage);
	setVeneziaVipMemoryRegion(&snapshot->buffer,
		&storage->captures[0].snapshotRegion, 4096, 520192);
	for (index = 0; index < snapshot->memoryRegionCount; ++index) {
		region = &snapshot->regions[index];
		memcpy((void *)region->virtualAddress,
		       (const void *)region->copyAddress, region->size);
	}
	for (processor = 0; processor < snapshot->processorCount; ++processor) {
		region = &snapshot->regions[index + processor];
		memcpy((void *)region->virtualAddress,
		       (const void *)region->copyAddress, region->size);
	}
	ret = setVeneziaDebuggerMode(4);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(3, 7, 4, 0, 0, 0, ret, 0, 0);
	if (ret != 0) {
		ret = ret == 6 ? 0 : -2141060856;
		goto unlock;
	}
	for (;;) {
		ret = getVeneziaDebuggerModes(&mode, &alternate);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(4, 3, ret, mode, alternate);
		if (ret != 0) {
			ret = -2141060856;
			break;
		}
		if ((mode == 1 && alternate == 1) ||
		    (mode == 2 && alternate == 2))
			break;
		sceKernelDelayThread(1000);
	}
unlock:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

/* name unknown: wait for the currently locked VIP process operation to finish. */
int synchronizeVeneziaVipProcess(void)
{
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return 0;
}

/* name unknown: remove the VIP primary interrupt handler. */
int releaseVeneziaVipInterrupt(void)
{
	sceKernelDisableIntr(56);
	sceKernelReleaseIntrHandler(56);
	return 0;
}

/* name unknown: publish the debugger memory base to the VIP boot registers. */
int setVeneziaVipDebuggerBase(VeneziaVipState *state)
{
	state->bootRegisters->debuggerPhysicalBase = state->debuggerMemory.physicalAddress;
	return 0;
}

/* name unknown: the original boot-base cleanup has no work to perform. */
int releaseVeneziaVipDebuggerBase(void)
{
	return 0;
}

/* name unknown: clear callback counters and install the primary VIP mailbox IRQ. */
int initializeVeneziaVipInterrupt(VeneziaVipState *state)
{
	VeneziaDebugEventCounter *counters = state->callbackState->counters;
	int ret;

	counters[0].count = 0;
	counters[1].count = 0;
	counters[2].count = 0;
	counters[3].count = 0;
	counters[4].count = 0;
	counters[5].count = 0;
	counters[6].count = 0;
	counters[7].count = 0;
	counters[8].count = 0;
	counters[9].count = 0;
	counters[10].count = 0;
	counters[11].count = 0;
	counters[12].count = 0;
	ret = sceKernelRegisterIntrHandler(56, "SceVipMailbox0", 0,
		(SceKernelIntrHandler)veneziaDebugInterruptHandler, state, 208, 15, NULL);
	if (ret < 0)
		return ret;
	ret = sceKernelEnableIntr(56);
	if (ret < 0) {
		sceKernelDisableIntr(56);
		sceKernelReleaseIntrHandler(56);
		return ret;
	}
	return 0;
}

/* name unknown: bind VIP hardware callbacks and initialize debugger event delivery. */
int initializeVeneziaVipDebugger(VeneziaVipState *state,
				VeneziaVipProcessStorage *storage)
{
	VeneziaDebugCallbacks callbacks;
	unsigned int mode, alternate;
	uint32_t virtualBase, physicalBase;
	int ret;

	if (activateVeneziaVipDebugger(state, storage) < 0)
		return -2141060860;
	virtualBase = state->debuggerMemory.virtualAddress;
	physicalBase = state->debuggerMemory.physicalAddress;
	callbacks.lock = (int (*)(int))lockVeneziaDebugChannel;
	callbacks.unlock = (void (*)(int))unlockVeneziaDebugChannel;
	callbacks.readMailbox = readVeneziaVipDebugMailbox;
	callbacks.clearReset = setVeneziaDebugReset;
	callbacks.setReset = clearVeneziaDebugReset;
	callbacks.checkReset = checkVeneziaDebugReset;
	callbacks.writeMailbox = acknowledgeVeneziaVipDebugMailbox;
	callbacks.signalMailbox = writeVeneziaVipDebugMailbox;
	ret = initVeneziaDebugger(&callbacks, virtualBase, physicalBase, 0x100000);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(2, 4, virtualBase, physicalBase, 0x100000, ret);
	if (ret == 6)
		return 0;
	if (ret != 0)
		return -2141060860;
	for (;;) {
		ret = getVeneziaDebuggerModes(&mode, &alternate);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(4, 3, ret, mode, alternate);
		if (ret != 0)
			return -2141060860;
		if ((mode == 1 && alternate == 1) ||
		    (mode == 2 && alternate == 2))
			break;
		sceKernelDelayThread(1000);
	}
	ret = setVeneziaDebuggerEventMask(0, (uint32_t)-16842640);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(13, 3, 0, (uint32_t)-16842640, ret);
	return ret == 0 ? 0 : -2141060860;
}

/* name unknown: suspend the debugger before releasing its VIP hardware state. */
int finalizeVeneziaVipDebugger(VeneziaVipState *state,
			      VeneziaVipProcessStorage *storage)
{
	int ret = suspendVeneziaVipDebugger();

	if (ret < 0)
		return ret;
	stopVeneziaVipDebugger(state, storage);
	return 0;
}

/* name unknown: stop capture, wait for completion and clear the mailbox event. */
int stopVeneziaVipCaptureChannel(unsigned int processor, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage)
{
	unsigned int captureState = 1, status = 0;
	int ret;

	ret = stopVeneziaDebugCapture(processor);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(18, 2, processor, ret);
	if (ret != 6 && ret != 0)
		return -2141060860;
	do {
		ret = getVeneziaDebugCaptureState(processor, &captureState);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(38, 3, processor, ret, captureState);
		if (ret != 0) {
			if (ret != 7)
				return -2141060860;
			captureState = 0;
		}
		sceKernelDelayThread(1000);
	} while (captureState & 1);
	ret = getVeneziaDebugCommandStatus(processor, &status);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(20, 3, processor, ret, status);
	if (ret != 0)
		return -2141060860;
	sceKernelClearEventFlag(state->eventFlag, 0);
	return status == 4 ? -2141060851 : 0;
}

/* name unknown: copy input into the trace ring, splitting at its end address. */
int copyVeneziaVipInputToTrace(unsigned int processor, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage, const VeneziaVipInputBufferList *list)
{
	VeneziaDebugProcessCapture *capture = &storage->captures[processor];
	unsigned int captureProcessor = storage->processors[processor];
	VeneziaVipInputIterator iterator;
	SceSize remaining, first, copied;
	uint32_t cursor;
	int ret, copyRet;

	initializeVeneziaVipInputIterator(&iterator, list);
	remaining = getVeneziaVipInputRemaining(&iterator);
	ret = getVeneziaDebugTraceCursor(captureProcessor, &cursor);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(29, 3, captureProcessor, ret, cursor);
	if (ret != 0)
		return -2141060856;
	if (remaining + cursor >= capture->bufferEnd) {
		first = capture->bufferEnd - cursor;
		copyRet = copyVeneziaVipInput(cursor, first, &copied, &iterator);
		if (copyRet < 0)
			return -2141060856;
		cursor = capture->bufferRegion.virtualAddress;
		ret = setVeneziaDebugTraceCursor(captureProcessor, cursor);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(28, 3, captureProcessor, cursor, ret);
		if (ret != 0)
			return ret == 7 ? -2141060862 : -2141060856;
		remaining -= first;
		if (!remaining)
			return copyRet;
		ret = getVeneziaDebugTraceCursor(captureProcessor, &cursor);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(29, 3, captureProcessor, ret, cursor);
		if (ret != 0)
			return -2141060856;
	}
	copyRet = copyVeneziaVipInput(cursor, remaining, &copied, &iterator);
	if (copyRet < 0 || remaining > copied)
		return -2141060856;
	cursor += remaining;
	ret = setVeneziaDebugTraceCursor(captureProcessor, cursor);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(28, 3, captureProcessor, cursor, ret);
	if (ret != 0)
		return ret == 7 ? -2141060862 : -2141060856;
	return copyRet;
}

typedef struct VeneziaVipCommandQueueStorage {
	VeneziaVipCommandQueue queues[6];
	uint32_t unidentifiedAfterQueues[304];
} VeneziaVipCommandQueueStorage;

typedef union VeneziaVipCommandBuffer {
	uint32_t words[64];
	struct {
		uint32_t payloadAddress;
		VeneziaVipCommandContext *context;
		uint32_t processorMode;
		uint32_t operation;
		uint32_t captureMode;
		uint32_t commandValue;
		uint32_t unidentifiedAfterValue[58];
	} request;
	struct {
		uint32_t requestWords[15];
		uint32_t contextAddress;
		int result;
		uint32_t operation;
		uint32_t unidentifiedResponseControl;
		uint32_t control;
		uint32_t unidentifiedBeforeEventMask[37];
		uint32_t eventMask;
		uint32_t unidentifiedBeforeStatus[4];
		uint32_t status;
		uint32_t mode;
	};
} VeneziaVipCommandBuffer;

typedef struct VeneziaVipCommandMemoryInfo {
	uint32_t unidentifiedBeforeSizes[4];
	SceSize primarySize;
	uint32_t unidentifiedControl;
	SceSize secondarySize;
	SceSize auxiliarySize;
} VeneziaVipCommandMemoryInfo;

/* Unknown reported values: the original only returns/logs them. */
typedef struct VeneziaVipCommandReportedValues {
	uint32_t auxiliary;
	uint32_t unidentified[2];
	uint32_t main;
} VeneziaVipCommandReportedValues;

typedef struct VeneziaVipCommandMemory {
	uint32_t unidentifiedBeforeCommands[1280];
	VeneziaVipCommandBuffer commands[2];
	uint32_t unidentifiedBeforeMemoryInfo[640];
	VeneziaVipCommandMemoryInfo processorMemory[2];
	uint32_t unidentifiedBeforeSharedMemory[48];
	VeneziaVipCommandMemoryInfo sharedMemory;
	uint32_t unidentifiedBeforeReportedValues[950];
	VeneziaVipCommandReportedValues reportedValues;
	uint32_t unidentifiedAfterReportedValues[46078];
} VeneziaVipCommandMemory;

typedef struct VeneziaVipCommandState {
	uint32_t magic;
	/* the stop request passes a third word (Sony's declaration is unprototyped) */
	uint32_t (*write)(uint32_t address, uint32_t value, ...);
	uint32_t (*read)(uint32_t address);
	VeneziaVipCommandMemory *memory;
	uint32_t physicalBase;
} VeneziaVipCommandState;

_Static_assert(sizeof(VeneziaVipCommandMemory) == 196608, "VIP command memory");
_Static_assert(sizeof(VeneziaVipCommandState) == 20, "VIP command state");
VeneziaVipCommandState g_veneziaVipCommandState;

/* name unknown: copy interchanged word pairs and notify the selected VIP mailbox. */
__attribute__((noinline))
int submitVeneziaVipCommand(unsigned int processor, const uint32_t *command,
			   unsigned int wordCount)
{
	uint32_t *destination = g_veneziaVipCommandState.memory->commands[processor].words;
	ScePervasiveMailboxRegisters *mailbox =
		(ScePervasiveMailboxRegisters *)PERVASIVE_MAILBOX_BASE;
	uint32_t address = (uint32_t)&mailbox->debugSend[processor];
	unsigned int index;

	index = 0;
	if (wordCount > 8) {
		const uint32_t *prefetch = command + 20;

		do {
			__builtin_prefetch(prefetch);
			destination[index ^ 1] = prefetch[-20];
			destination[(index + 1) ^ 1] = prefetch[-19];
			destination[(index + 2) ^ 1] = prefetch[-18];
			destination[(index + 3) ^ 1] = prefetch[-17];
			destination[(index + 4) ^ 1] = prefetch[-16];
			destination[(index + 5) ^ 1] = prefetch[-15];
			destination[(index + 6) ^ 1] = prefetch[-14];
			destination[(index + 7) ^ 1] = prefetch[-13];
			prefetch += 8;
			index += 8;
		} while (index + 1 < wordCount - 7);
	}
	if (index < wordCount) {
		do {
			destination[index ^ 1] = command[index];
			++index;
		} while (index < wordCount);
	}
	g_veneziaVipCommandState.read(address);
	return g_veneziaVipCommandState.write(address, 0x80000000, destination[0]);
}

/* name unknown: consume a command response and update its caller-owned context. */
__attribute__((noinline))
int getVeneziaVipStopInfo(unsigned int processor, VeneziaDebugStopInfo *info)
{
	ScePervasiveMailboxRegisters *mailbox =
		(ScePervasiveMailboxRegisters *)PERVASIVE_MAILBOX_BASE;
	VeneziaVipCommandBuffer *response;
	VeneziaVipCommandContext *context;
	uint32_t address, operation, control, status;
	int mailboxStatus, ret;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (processor > 1)
		return 1;
	address = (uint32_t)&mailbox->debugReceive[processor];
	mailboxStatus = g_veneziaVipCommandState.read(address);
	if (mailboxStatus >= 0) {
		if (mailboxStatus & 0x08000000) {
			if (info) {
				info->contextValue = 0;
				info->context = NULL;
				info->result = 0;
				info->control = 0;
				info->flags = 6;
			}
			ret = 0;
			g_veneziaVipCommandState.write(address, 0x08000000);
		} else {
			ret = 3;
		}
	} else {
		response = &g_veneziaVipCommandState.memory->commands[processor];
		context = response->contextAddress == UINT32_MAX ? NULL :
			(VeneziaVipCommandContext *)response->contextAddress;
		operation = response->operation;
		control = response->control;
		if (operation == 2) {
			if (response->result >= 0)
				context->processorState[processor] = 1;
		} else if (operation == 1 || operation == 6) {
			if (response->result >= 0 && context)
				context->magic = 0;
		} else if (operation == 0x40000) {
			if (response->result >= 0)
				context->processorState[processor] = 0;
		} else {
			status = response->status;
			if ((int)status > 0) {
				if (processor == 0) {
					context->resultFlags = status >> 16;
					status &= 3;
				}
				if (response->result >= 0)
					context->processorState[processor] = 1 << status;
			}
		}
		if (info) {
			info->contextValue = context ? context->contextValue : 0;
			info->context = context;
			info->flags = operation;
			info->result = response->result;
			info->control = control;
		}
		if (mailboxStatus & 0x08000000) {
			ret = 6;
			g_veneziaVipCommandState.write(address, 0x88000000);
		} else {
			ret = 0;
			g_veneziaVipCommandState.write(address, 0x80000000);
		}
	}
	if (mailboxStatus & 0x30000000) {
		ret = 5;
		g_veneziaVipCommandState.write(address, 0x30000000);
	}
	g_veneziaVipCommandState.read(address);
	return ret;
}

/* name unknown: select command events without reserved mailbox control bits. */
__attribute__((noinline)) int setVeneziaVipCommandEventMask(unsigned int processor, uint32_t mask)
{
	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (processor > 1)
		return 1;
	g_veneziaVipCommandState.memory->commands[processor].eventMask = mask & 0x4fffffff;
	return 0;
}

/* name unknown: acknowledge and return the VIP command mailbox interrupt bits. */
__attribute__((noinline))
int clearVeneziaVipInterruptStatus(unsigned int processor, uint32_t *status)
{
	ScePervasiveMailboxRegisters *mailbox =
		(ScePervasiveMailboxRegisters *)PERVASIVE_MAILBOX_BASE;
	uint32_t address, pending;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (processor > 1)
		return 1;
	address = (uint32_t)&mailbox->debugReceive[processor];
	pending = g_veneziaVipCommandState.read(address) & 0x47ffffff;
	*status = pending;
	g_veneziaVipCommandState.write(address, pending);
	return 0;
}

typedef struct VeneziaVipCommandCallbacks {
	uint32_t (*write)(uint32_t address, uint32_t value, ...);
	uint32_t (*read)(uint32_t address);
} VeneziaVipCommandCallbacks;

/* name unknown: bind generic register access and initialize both command slots. */
int initializeVeneziaVipCommands(const VeneziaVipCommandCallbacks *callbacks,
				const VeneziaVipMemoryRegion *memory)
{
	ScePervasiveMailboxRegisters *mailbox =
		(ScePervasiveMailboxRegisters *)PERVASIVE_MAILBOX_BASE;

	VeneziaVipCommandMemory *virtualAddress;
	uint32_t physicalAddress;
	uint32_t (*write)(uint32_t address, uint32_t value, ...);
	uint32_t (*read)(uint32_t);

	if (g_veneziaVipCommandState.magic == 5261654)
		return 2;
	physicalAddress = memory->physicalAddress;
	if (physicalAddress & 15)
		return 1;
	g_veneziaVipCommandState.read = NULL;
	g_veneziaVipCommandState.memory = NULL;
	g_veneziaVipCommandState.magic = 0;
	g_veneziaVipCommandState.write = NULL;
	g_veneziaVipCommandState.physicalBase = 0;
	virtualAddress = (VeneziaVipCommandMemory *)memory->virtualAddress;
	write = callbacks->write;
	read = callbacks->read;
	g_veneziaVipCommandState.memory = virtualAddress;
	g_veneziaVipCommandState.physicalBase = physicalAddress;
	g_veneziaVipCommandState.write = write;
	g_veneziaVipCommandState.read = read;
	memset(&g_veneziaVipCommandState.memory->commands[0], 0, 256);
	g_veneziaVipCommandState.write((uint32_t)&mailbox->debugReceive[0], UINT32_MAX);
	memset(&g_veneziaVipCommandState.memory->commands[1], 0, 256);
	g_veneziaVipCommandState.write((uint32_t)&mailbox->debugReceive[1], UINT32_MAX);
	g_veneziaVipCommandState.magic = 5261654;
	return 0;
}

/* name unknown: size one context header and its largest processor/shared payload. */
int getVeneziaVipCommandContextSize(SceSize *size)
{
	const VeneziaVipCommandMemoryInfo *memory;
	SceSize first, second, shared;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	memory = g_veneziaVipCommandState.memory->processorMemory;
	second = memory[1].auxiliarySize + memory[1].primarySize + 24 + memory[1].secondarySize;
	first = memory[0].auxiliarySize + memory[0].primarySize + 24 + memory[0].secondarySize;
	memory = &g_veneziaVipCommandState.memory->sharedMemory;
	shared = memory->primarySize + memory->auxiliarySize + 143 + memory->secondarySize;
	*size = ((second < first ? first : second) + shared) & ~15u;
	return 0;
}

typedef struct VeneziaVipCommandContextParam {
	uint32_t commandValues[2];
	uint32_t physicalBase;
	VeneziaVipCommandContext *context;
	SceSize size;
} VeneziaVipCommandContextParam;

/* name unknown: validate context storage and initialize its 96-byte header. */
VeneziaVipCommandContext *initializeVeneziaVipCommandContext(
	const VeneziaVipCommandContextParam *param, unsigned int processor,
	uint32_t contextValue)
{
	const VeneziaVipCommandMemoryInfo *memory;
	VeneziaVipCommandContext *context;
	SceSize processorSize, sharedSize, size;

	if (g_veneziaVipCommandState.magic != 5261654 || processor > 1)
		return NULL;
	size = param->size;
	memory = &g_veneziaVipCommandState.memory->processorMemory[processor];
	processorSize = memory->auxiliarySize + memory->primarySize + 24 + memory->secondarySize;
	if (param->commandValues[0] & 3)
		return NULL;
	if (param->commandValues[1] & 3)
		return NULL;
	memory = &g_veneziaVipCommandState.memory->sharedMemory;
	sharedSize = memory->secondarySize + memory->auxiliarySize + memory->primarySize + 128;
	if (size < sharedSize + processorSize || (size & 15))
		return NULL;
	if (param->physicalBase & 15)
		return NULL;
	context = param->context;
	context->processorMode[0] = processor;
	context->processorMode[1] = 0;
	context->processorState[0] = 0;
	context->processorState[1] = 0;
	context->physicalBase = param->physicalBase;
	context->resultFlags = 0;
	context->payloadOffsets[1] = processorSize + 96;
	context->activeQueue = NULL;
	context->commandValues[0] = param->commandValues[0];
	context->commandValues[1] = param->commandValues[1];
	context->contextValue = contextValue;
	context->magic = 1129335126;
	context->payloadOffsets[0] = 96;
	return context;
}

typedef struct VeneziaVipCommandQueueRegion {
	uint32_t base;
	SceSize offset;
	SceSize capacity;
} VeneziaVipCommandQueueRegion;

typedef struct VeneziaVipCommandQueueParam {
	uint32_t bufferBase;
	VeneziaVipCommandQueueStorage *storage;
	SceSize storageSize;
	uint32_t captureAddress;
	uint32_t captureParameter;
	uint32_t captureOption;
	uint32_t physicalBase;
	VeneziaVipCommandQueueRegion regions[6];
} VeneziaVipCommandQueueParam;

/* name unknown: expose the command queue storage requirement. */
__attribute__((noinline)) int getVeneziaVipCommandQueueStorageSize(SceSize *size)
{
	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	*size = 1504;
	return 0;
}

/* name unknown: validate capture queues and reserve primary/auxiliary slots. */
int initializeVeneziaVipCommandQueues(VeneziaVipCommandContext *context,
				     const VeneziaVipCommandQueueParam *param)
{
	VeneziaVipCommandQueue *queues, *auxiliary;
	VeneziaVipCommandQueueSlot *slot;
	unsigned int count, auxiliaryIndex, index;
	uint32_t captureAddress;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	auxiliaryIndex = context->processorMode[0] ? 5 : 4;
	count = auxiliaryIndex + 1;
	if (param->bufferBase & 7)
		return 1;
	if (param->storageSize < 1504)
		return 1;
	captureAddress = param->captureAddress;
	if ((captureAddress & 0x01ff80ff) ||
	    captureAddress - 0x02000000 < 0x1e000000 || captureAddress >= 0x22000000)
		return 1;
	for (index = 0; index < count; ++index) {
		if ((param->regions[index].capacity & 15) ||
		    param->regions[index].capacity <= 512 || (param->regions[index].base & 7))
			return 1;
	}
	if (param->physicalBase & 3)
		return 1;
	queues = param->storage->queues;
	memset(param->storage, 0, 1504);
	for (index = 0; index < count; ++index) {
		queues[index].physicalBase = param->physicalBase;
		queues[index].base = param->regions[index].base;
		queues[index].slots[0].address = param->regions[index].base;
		queues[index].slots[1].address = param->regions[index].base;
		queues[index].size = param->regions[index].capacity;
		queues[index].slots[2].address = param->regions[index].base;
		queues[index].slots[3].address = param->regions[index].base;
	}
	context->captureParameter = param->captureParameter;
	context->captureOption = param->captureOption;
	context->queues = queues;
	context->auxiliaryQueue = &queues[auxiliaryIndex];
	context->bufferOffset = param->regions[0].offset;
	context->auxiliaryBufferOffset = param->regions[auxiliaryIndex].offset;
	context->captureAddress = captureAddress;
	context->bufferBase = param->bufferBase;
	context->captureMemory.physicalAddress = 0;
	context->captureMemory.virtualAddress = 0;
	context->captureMemory.size = 0;
	context->captureLength = 0;
	if ((unsigned int)(context->primarySlot - 1) <= 3) {
		slot = &queues->slots[context->primarySlot - 1];
		if (slot && slot->busy)
			return 1;
	}
	if (queues->slots[0].busy) {
		context->primarySlot = -1;
		return 1;
	}
	queues->slots[0].busy = 1;
	queues->slots[0].address = queues->base;
	queues->slots[1].address = queues->base;
	queues->slots[2].address = queues->base;
	queues->slots[3].address = queues->base;
	context->primarySlot = 1;
	auxiliary = context->auxiliaryQueue;
	if ((unsigned int)(context->secondarySlot - 2) > 2 ||
	    (uint32_t)&auxiliary->slots[context->secondarySlot - 1] == 0 ||
	    !auxiliary->slots[context->secondarySlot - 1].busy) {
		for (index = 1; index < 4; ++index) {
			if (!auxiliary->slots[index].busy) {
				auxiliary->slots[index].busy = 1;
				auxiliary->slots[index].address = auxiliary->base;
				context->secondarySlot = index + 1;
				context->activeQueue = queues;
				return 0;
			}
		}
		context->secondarySlot = -1;
	}
	if ((uint32_t)context->queues != (uint32_t)-16) {
		queues = context->queues;
		if (!queues->slots[0].busy) {
			queues->flags[1] = 0;
		} else {
			queues->slots[0].busy = 0;
			if (context->primarySlot == 1)
				queues->flags[1] = 0;
			else if (!queues->slots[1].busy && !queues->slots[2].busy &&
				 !queues->slots[3].busy) {
				queues->flags[2] = 0;
				context->queues->flags[3] = 0;
			}
		}
	}
	context->primarySlot = -1;
	return 1;
}

/* name unknown: issue a stop command when the processor is idle in mode1/5. */
int requestVeneziaVipCommandStop(unsigned int processor)
{
	VeneziaVipCommandBuffer *commands, *command;
	uint32_t address;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (processor > 1)
		return 1;
	address = MAILBOX_PADDR(debugSend[0]) + (processor << 2);
	if (g_veneziaVipCommandState.read(address) != 0)
		return 2;
	commands = g_veneziaVipCommandState.memory->commands;
	command = &commands[processor];
	switch (command->mode) {
	case 1:
	case 5:
		break;
	default:
		return 2;
	}
	command->request.context = (VeneziaVipCommandContext *)UINT32_MAX;
	command->request.payloadAddress = UINT32_MAX;
	command->request.operation = 1;
	g_veneziaVipCommandState.read(address);
	g_veneziaVipCommandState.write(address, 0x80000000,
				       command->request.payloadAddress);
	return 0;
}

/* name unknown: notify a processor to interrupt a pending command. */
int interruptVeneziaVipCommand(unsigned int processor)
{
	uint32_t address;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (processor > 1)
		return 1;
	address = MAILBOX_PADDR(debugSend[0]) + (processor << 2);
	if (g_veneziaVipCommandState.read(address) == 0 &&
	    g_veneziaVipCommandState.memory->commands[processor].mode == 0)
		return 3;
	g_veneziaVipCommandState.write(address, 0x40000000);
	return 0;
}

/* name unknown: return the two values reported by command firmware initialization. */
__attribute__((noinline)) int getVeneziaVipCommandReportedValues(uint32_t *main, uint32_t *auxiliary)
{
	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	*main = g_veneziaVipCommandState.memory->reportedValues.main;
	*auxiliary = g_veneziaVipCommandState.memory->reportedValues.auxiliary;
	return 0;
}

/* name unknown: translate command mailbox/mode into the caller's processor status. */
int getVeneziaVipCommandProcessorStatus(unsigned int processor, unsigned int *status)
{
	uint32_t magic = g_veneziaVipCommandState.magic;

	*status = 0;
	if (magic != 5261654)
		return 2;
	if (processor > 1)
		return 1;
	if (g_veneziaVipCommandState.read(MAILBOX_PADDR(debugSend[0]) + (processor << 2)) != 0) {
		*status = 6;
	} else {
		switch (g_veneziaVipCommandState.memory->commands[processor].mode) {
		case 0:
			*status = 1;
			break;
		case 1:
			*status = 2;
			break;
		case 3:
			*status = 6;
			break;
		case 5:
			*status = 10;
			break;
		}
	}
	return 0;
}

/* name unknown: encode the command memory window's aligned base and offset. */
__attribute__((noinline)) int encodeVeneziaVipCommandAddress(uint32_t base, uint32_t offset, uint32_t *encoded)
{
	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if ((base & 0x000fffff) || (offset & 0x0003ffff) || offset >= 0x02000000)
		return 1;
	*encoded = (offset >> 10) + (base >= 0x30000000 ? 0 : 0x20000000);
	return 0;
}

/* name unknown: calculate tiled image storage by packing two image components. */
int calculateVeneziaVipTiledImageSize(unsigned int width, unsigned int height,
				    unsigned int imageCount, SceSize *size)
{
	uint32_t bands[20];
	unsigned int widthBlocks, heightBlocks, firstWidth, firstHeight, secondWidth, secondHeight;
	unsigned int bandCount = 0, rowY = 0, rowBase = 0, column = 0;
	unsigned int index, band, row, rows, placed = 0, componentY, componentX;
	uint32_t firstCode = 0, lastCode = 0, previousCode = 0, currentCode;
	SceSize firstEnd, secondEnd, end;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (width <= 15 || height <= 15 || !imageCount)
		return 1;
	widthBlocks = (width + 15) >> 4;
	heightBlocks = (height + 15) >> 4;
	if (heightBlocks - 1 > 67 || widthBlocks - 1 > 119 || imageCount > 19)
		return 1;
	firstWidth = 32 * (widthBlocks + 2);
	firstHeight = 12 * heightBlocks + 4;
	for (index = 0; index < imageCount; ++index) {
		if (column + firstWidth + 128 <= 8192) {
			firstCode = ((column << 16) & 0x1fc00000) | (rowBase & 0x0007fff8);
			column = (column + firstWidth + (column > 63 ? 63 : 127)) & ~63u;
		} else {
			rowY = (rowY + 12 * heightBlocks + 19) & ~15u;
			bands[++bandCount] = (((column + 64) << 16) & 0x1ff80000) |
					    (rowBase & 0x0007fff8);
			if (firstHeight + rowY > 65536)
				return 1;
			rowBase = 8 * rowY;
			column = (firstWidth + 127) & ~63u;
			firstCode = rowBase & 0x0007fff8;
		}
	}
	secondHeight = 5 * heightBlocks;
	secondWidth = 16 * widthBlocks;
	bands[bandCount + 1] = (((column + 64) << 16) & 0x1ff80000) |
			       (rowBase & 0x0007fff8);
	rows = firstHeight / secondHeight;
	bands[0] = 0;
	for (band = 0; band <= bandCount; ++band) {
		componentY = (uint16_t)(bands[band + 1] >> 3);
		for (row = 0; row != rows; ++row) {
			componentX = (bands[band + 1] & 7) |
				     ((bands[band + 1] & 0x1ff80007) >> 16);
			for (index = 0; index != imageCount; ++index) {
				if (componentX + secondWidth > 8192)
					break;
				if (imageCount > placed) {
					previousCode = lastCode;
					++placed;
					lastCode = (componentX & 3) | ((8 * componentY) & 0x0007fff8) |
						   ((componentX << 16) & 0x1ff80000);
					componentX += secondWidth;
				}
			}
			componentY += secondHeight;
		}
	}
	firstEnd = (uint16_t)(firstCode >> 3) + firstHeight;
	componentY = (firstEnd + 15) & ~15u;
	if (imageCount > placed) {
		componentX = 0;
		for (;;) {
			if (componentX + secondWidth <= 8192) {
				currentCode = ((componentX << 16) & 0x1ff80000) |
					      ((8 * componentY) & 0x0007fff8);
				componentX += secondWidth;
			} else {
				componentY += secondHeight;
				if (componentY + secondHeight > 65536)
					return 1;
				componentX = secondWidth;
				currentCode = (8 * componentY) & 0x0007fff8;
			}
			if (imageCount <= ++placed)
				break;
			lastCode = currentCode;
		}
	} else {
		currentCode = lastCode;
		lastCode = previousCode;
	}
	secondEnd = (uint16_t)((imageCount == 1 ? currentCode : lastCode) >> 3) + secondHeight;
	end = secondEnd >= firstEnd ? secondEnd : firstEnd;
	if (end > 65536)
		return 1;
	*size = end << 13;
	return 0;
}

/* name unknown: bind aligned storage for command capture records. */
int configureVeneziaVipCaptureMemory(VeneziaVipCommandContext *context,
				   SceSize length, const VeneziaVipMemoryRegion *memory)
{
	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (!context || context->magic != 1129335126 || !context->activeQueue)
		return 1;
	if (context->processorState[1] == 2 || context->processorState[1] == 4)
		return 2;
	if ((length & 7) || length >= 0x04000000)
		return 1;
	if ((memory->physicalAddress & 7) || (memory->virtualAddress & 7) ||
	    (memory->size & 7) || memory->size < 39 * length)
		return 1;
	context->captureMemory = *memory;
	context->captureLength = length;
	return 0;
}

/* name unknown: submit a validated context to an idle command processor. */
int submitVeneziaVipCommandContext(unsigned int processor, VeneziaVipCommandContext *context)
{
	ScePervasiveMailboxRegisters *mailbox =
		(ScePervasiveMailboxRegisters *)PERVASIVE_MAILBOX_BASE;
	VeneziaVipCommandBuffer *command;
	uint32_t address, payloadAddress;
	uint32_t processorMode, commandValue;
	unsigned int status;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (processor > 1)
		return 1;
	address = (uint32_t)&mailbox->debugSend[processor];
	if (g_veneziaVipCommandState.read(address) != 0) {
		status = 6;
	} else {
		switch (g_veneziaVipCommandState.memory->commands[processor].mode) {
		case 0:
			status = 1;
			break;
		case 1:
		case 5:
			status = 2;
			break;
		case 3:
			status = 6;
			break;
		default:
			status = 0;
		}
	}
	if (!context || context->magic != 1129335126)
		return 1;
	if (status != 2)
		return 2;
	command = &g_veneziaVipCommandState.memory->commands[processor];
	processorMode = context->processorMode[processor];
	commandValue = context->commandValues[processor];
	payloadAddress = context->payloadOffsets[processor] + context->physicalBase;
	command->request.context = context;
	command->request.payloadAddress = payloadAddress;
	command->request.processorMode = processorMode;
	command->request.operation = 2;
	command->request.commandValue = commandValue;
	g_veneziaVipCommandState.read(address);
	g_veneziaVipCommandState.write(address, 0x80000000,
				       command->request.payloadAddress);
	return 0;
}

/* name unknown: continue a prepared context with operation4 and a command value. */
int continueVeneziaVipCommandContext(unsigned int processor,
				    VeneziaVipCommandContext *context, uint32_t value)
{
	ScePervasiveMailboxRegisters *mailbox =
		(ScePervasiveMailboxRegisters *)PERVASIVE_MAILBOX_BASE;
	VeneziaVipCommandBuffer *command;
	uint32_t address, payloadAddress;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (processor > 1)
		return 1;
	address = (uint32_t)&mailbox->debugSend[processor];
	if (g_veneziaVipCommandState.read(address) != 0)
		return 2;
	command = &g_veneziaVipCommandState.memory->commands[processor];
	if (command->mode != 1 && command->mode != 5)
		return 2;
	if (!context)
		return 3;
	if (context->processorState[processor] != 1)
		return 2;
	payloadAddress = context->payloadOffsets[processor] + context->physicalBase;
	command->request.context = context;
	command->request.payloadAddress = payloadAddress;
	command->request.processorMode = 0;
	command->request.operation = 4;
	command->request.commandValue = value;
	g_veneziaVipCommandState.read(address);
	g_veneziaVipCommandState.write(address, 0x80000000,
				       command->request.payloadAddress);
	return 0;
}

typedef struct VeneziaVipCaptureCommand {
	VeneziaVipCommandContext *context;
	uint32_t payloadAddress;
	uint32_t operation;
	union {
		struct {
			uint32_t queueStart;
			uint32_t queueNext;
			uint32_t queueLast;
			uint32_t queueSecond;
			uint32_t extraQueue;
		} primary;
		struct {
			uint32_t queueStart;
			uint32_t queueNext;
			uint32_t queueLast;
			uint32_t auxiliaryStorage;
			uint32_t captureAddress;
			uint32_t captureOption;
			uint32_t control;
			uint32_t captureBase;
			SceSize captureLength;
		} secondary;
	};
} VeneziaVipCaptureCommand;

/* name unknown: send the queue/capture region addresses required by operation10000. */
int configureVeneziaVipCommandCapture(unsigned int processor, VeneziaVipCommandContext *context)
{
	VeneziaVipCommandQueueStorage *physicalStorage;
	VeneziaVipCaptureCommand packet;
	unsigned int queueIndex, wordCount, mode;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (processor > 1)
		return 1;
	if (g_veneziaVipCommandState.read(MAILBOX_PADDR(debugSend[0]) + (processor << 2)) != 0)
		return 2;
	mode = g_veneziaVipCommandState.memory->commands[processor].mode;
	switch (mode) {
	case 1:
	case 5:
		break;
	default:
		return 2;
	}
	if (!context || context->magic != 1129335126 || !context->activeQueue)
		return 3;
	if (context->processorState[processor] != 1)
		return 2;
	packet.context = context;
	packet.payloadAddress = context->physicalBase + context->payloadOffsets[processor];
	packet.operation = 0x10000;
	physicalStorage = (VeneziaVipCommandQueueStorage *)context->bufferBase;
	queueIndex = context->processorMode[0] ? 3 : 2;
	if (processor) {
		packet.secondary.queueStart = (uint32_t)&physicalStorage->queues[queueIndex];
		packet.secondary.queueNext = (uint32_t)&physicalStorage->queues[queueIndex + 1];
		packet.secondary.queueLast = (uint32_t)&physicalStorage->queues[queueIndex + 2];
		packet.secondary.auxiliaryStorage = (uint32_t)physicalStorage->unidentifiedAfterQueues;
		packet.secondary.captureAddress = context->captureAddress;
		packet.secondary.captureOption = context->captureOption;
		packet.secondary.control = 0;
		packet.secondary.captureBase = context->captureMemory.physicalAddress;
		packet.secondary.captureLength = context->captureLength;
		submitVeneziaVipCommand(1, (const uint32_t *)&packet, 12);
	} else {
		packet.primary.queueStart = (uint32_t)&physicalStorage->queues[0];
		packet.primary.queueNext = (uint32_t)&physicalStorage->queues[queueIndex];
		packet.primary.queueLast = (uint32_t)&physicalStorage->queues[queueIndex + 1];
		packet.primary.queueSecond = (uint32_t)&physicalStorage->queues[1];
		wordCount = 7;
		if (context->processorMode[0]) {
			packet.primary.extraQueue = (uint32_t)&physicalStorage->queues[2];
			wordCount = 8;
		}
		submitVeneziaVipCommand(0, (const uint32_t *)&packet, wordCount);
	}
	return 0;
}

/* name unknown: request capture operation20000 for a context in state2. */
int startVeneziaVipCommandCapture(unsigned int processor,
				 VeneziaVipCommandContext *context, unsigned int captureMode)
{
	ScePervasiveMailboxRegisters *mailbox =
		(ScePervasiveMailboxRegisters *)PERVASIVE_MAILBOX_BASE;
	VeneziaVipCommandBuffer *command;
	uint32_t address;
	int ret;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (processor > 1)
		return 1;
	address = (uint32_t)&mailbox->debugSend[processor];
	if (g_veneziaVipCommandState.read(address) != 0) {
		ret = 6;
	} else {
		switch (g_veneziaVipCommandState.memory->commands[processor].mode) {
		case 0:
			ret = 1;
			break;
		case 1:
		case 5:
			ret = 2;
			break;
		case 3:
			ret = 6;
			break;
		default:
			ret = 0;
		}
	}
	if (!context)
		return 3;
	if (captureMode != 0 && (captureMode != 1 || processor != 1))
		return 1;
	if (ret != 2)
		return 2;
	if (context->processorState[processor] == 2) {
		command = &g_veneziaVipCommandState.memory->commands[processor];
		command->request.context = context;
		command->request.payloadAddress = context->physicalBase + context->payloadOffsets[processor];
		command->request.operation = 0x20000;
		command->request.captureMode = captureMode;
		command->request.processorMode = 0;
		command->request.commandValue = 0;
		g_veneziaVipCommandState.read(address);
		g_veneziaVipCommandState.write(address, 0x80000000,
					       command->request.payloadAddress);
		ret = 0;
	}
	return ret;
}

/* name unknown: return a context's state for the selected processor. */
__attribute__((noinline)) int getVeneziaVipCommandContextState(unsigned int processor, const VeneziaVipCommandContext *context)
{
	if (processor <= 1 && context->magic == 1129335126)
		return context->processorState[processor];
	return 0;
}

/* name unknown: position the primary input cursor within its permitted queue range. */
int setVeneziaVipCommandInputOffset(VeneziaVipCommandContext *context, SceSize offset)
{
	VeneziaVipCommandQueue *queue;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (!context || context->magic != 1129335126)
		return 1;
	if (context->processorState[0] != 2)
		return 2;
	queue = context->queues;
	if (queue->accessBlocked)
		return 4;
	if (context->bufferOffset > offset || offset >= queue->size + context->bufferOffset ||
	    context->primarySlot != 1)
		return 1;
	queue->slots[0].address = queue->base + offset - context->bufferOffset;
	return queue->accessBlocked;
}

/* name unknown: calculate free primary input space behind occupied queue cursors. */
int getVeneziaVipCommandInputSpace(VeneziaVipCommandContext *context, SceSize *space)
{
	VeneziaVipCommandQueue *queue;
	uint32_t current, address, distance, candidate, candidateDistance;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (!context || context->magic != 1129335126)
		return 1;
	if (context->processorState[0] <= 1)
		return 2;
	queue = context->queues;
	address = queue->slots[0].busy;
	current = queue->slots[0].address;
	if (address) {
		address = queue->base;
		distance = 0;
		if (queue->slots[1].busy) {
			address = queue->slots[1].address;
			distance = current >= address ? current - address : queue->size - address - current;
		}
		if (queue->slots[2].busy) {
			candidate = queue->slots[2].address;
			candidateDistance = current >= candidate ? current - candidate :
				queue->size - candidate - current;
			if (candidateDistance >= distance) {
				distance = candidateDistance;
				address = candidate;
			}
		}
		if (queue->slots[3].busy) {
			candidate = queue->slots[3].address;
			candidateDistance = current >= candidate ? current - candidate :
				queue->size - candidate - current;
			if (candidateDistance >= distance)
				address = candidate;
		}
		if (address) {
			current -= queue->base;
			address -= queue->base;
			address = current >= address ? address + queue->size - 8 - current :
				address - 8 - current;
		}
	}
	*space = address;
	return 0;
}

/* name unknown: block further primary queue access until the current operation completes. */
__attribute__((noinline)) int blockVeneziaVipCommandInput(VeneziaVipCommandContext *context)
{
	VeneziaVipCommandQueue *queue;
	int blocked;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (context->magic != 1129335126)
		return 1;
	if (context->processorState[0] != 2)
		return 2;
	queue = context->queues;
	blocked = queue->accessBlocked;
	if (blocked)
		return 4;
	queue->accessBlocked = 1;
	return blocked;
}

/* name unknown: report whether primary queue access is currently blocked. */
__attribute__((noinline)) int getVeneziaVipCommandInputBlocked(VeneziaVipCommandContext *context)
{
	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (context->magic != 1129335126)
		return 1;
	if (context->processorState[0] != 2)
		return 2;
	return context->queues->accessBlocked ? 0 : 3;
}

/* name unknown: set the primary queue activity request; its consumer is not yet named. */
__attribute__((noinline)) int requestVeneziaVipCommandInputActivity(VeneziaVipCommandContext *context)
{
	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (context->magic != 1129335126)
		return 1;
	if (context->processorState[0] != 2)
		return 2;
	context->queues->unidentifiedActivity[1] = 1;
	return 0;
}

/* name unknown: report data pending behind any occupied active-queue cursor. */
int getVeneziaVipCommandInputPending(VeneziaVipCommandContext *context)
{
	VeneziaVipCommandQueue *queue;
	uint32_t current, address, distance, maximum = 0;
	int index, count;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (context->magic != 1129335126)
		return 1;
	if (context->processorState[0] != 2)
		return 2;
	queue = context->activeQueue;
	count = context->processorMode[0];
	index = 0;
	do {
		if (!queue->slots[0].busy)
			return 2;
		current = queue->slots[0].address;
		if (queue->slots[1].busy) {
			address = queue->slots[1].address;
			distance = current >= address ? current - address : queue->size - address - current;
			if (maximum < distance)
				maximum = distance;
		}
		if (queue->slots[2].busy) {
			address = queue->slots[2].address;
			distance = current >= address ? current - address : queue->size - address - current;
			if (maximum < distance)
				maximum = distance;
		}
		if (queue->slots[3].busy) {
			address = queue->slots[3].address;
			distance = current >= address ? current - address : queue->size - address - current;
			if (maximum < distance)
				maximum = distance;
		}
		++index;
		++queue;
	} while (index <= count);
	return maximum ? 0 : 3;
}

/* name unknown: return available sixteen-byte units in the selected auxiliary cursor. */
int getVeneziaVipCommandAuxiliaryCount(VeneziaVipCommandContext *context, SceSize *count)
{
	VeneziaVipCommandQueue *queue;
	VeneziaVipCommandQueueSlot *slot;
	uint32_t current, position, size = 0;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (!context || context->magic != 1129335126)
		return 1;
	if (context->processorState[1] <= 1)
		return 2;
	if ((unsigned int)(context->secondarySlot - 2) <= 2) {
		queue = context->auxiliaryQueue;
		slot = &queue->slots[context->secondarySlot - 1];
		if (slot) {
			position = slot->address;
			current = position;
			if (queue->slots[0].busy)
				current = queue->slots[0].address;
			current -= queue->base;
			position -= queue->base;
			if (position <= current) {
				size = (current - position) >> 4;
			} else {
				size = queue->size - position;
				if (current)
					size += current;
				size >>= 4;
			}
		}
	}
	*count = size;
	return 0;
}

/* name unknown: locate the next complete auxiliary sixteen-byte record. */
int getVeneziaVipCommandAuxiliaryOffset(VeneziaVipCommandContext *context, SceSize *offset)
{
	VeneziaVipCommandQueue *queue;
	VeneziaVipCommandQueueSlot *slot;
	uint32_t position, current, size;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (!context || context->magic != 1129335126)
		return 1;
	if (context->processorState[1] <= 1)
		return 2;
	if ((unsigned int)(context->secondarySlot - 2) <= 2) {
		queue = context->auxiliaryQueue;
		slot = &queue->slots[context->secondarySlot - 1];
		if (slot) {
			position = slot->address;
			current = position;
			if (queue->slots[0].busy)
				current = queue->slots[0].address;
			position -= queue->base;
			current -= queue->base;
			size = position > current ? queue->size - position : current - position;
			if (size > 15) {
				*offset = position + context->auxiliaryBufferOffset;
				return 0;
			}
		}
	}
	*offset = 0;
	return 3;
}

/* name unknown: full result record, including the metadata copied by its accessor. */
typedef struct VeneziaVipCommandResultRecord {
	uint32_t unidentifiedControl;
	uint32_t released;
	uint32_t unidentifiedStatus;
	uint32_t imageAddress;
	VeneziaVipCommandResultMetadata metadata;
} VeneziaVipCommandResultRecord;

/* name unknown: release an auxiliary record and advance its cursor by one record. */
int releaseVeneziaVipCommandAuxiliaryRecord(VeneziaVipCommandContext *context,
					  const VeneziaVipCommandAuxiliaryRecord *record)
{
	VeneziaVipCommandQueue *queue;
	VeneziaVipCommandQueueSlot *slot;
	VeneziaVipCommandResultRecord *result;
	uint32_t address = (uint32_t)record, position, size;
	int selected;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (!context || context->magic != 1129335126)
		return 1;
	if (context->processorState[1] <= 1)
		return 2;
	queue = context->auxiliaryQueue;
	size = queue->size;
	if (address < context->auxiliaryBufferOffset ||
	    address > context->auxiliaryBufferOffset + size)
		return 1;
	selected = context->secondarySlot;
	result = (VeneziaVipCommandResultRecord *)((uint32_t)context->activeQueue +
		record->resultAddress - context->bufferBase);
	result->released = 1;
	if ((unsigned int)(selected - 2) > 2)
		return 1;
	slot = &queue->slots[selected - 1];
	if (!slot)
		return 1;
	position = slot->address - queue->base + sizeof(*record);
	if (size <= position)
		position -= size;
	slot->address = queue->base + position;
	return 0;
}

/* name unknown: each debug data bank has a720-byte stride; this accessor copies130words. */
typedef struct VeneziaVipCommandImageDebugBank {
	uint32_t debugData[130];
	uint32_t auxiliaryData[12];
	uint32_t secondaryData[16];
	uint32_t statusData[2];
	uint32_t controlData[4];
	uint32_t captureMetadata[16];
} VeneziaVipCommandImageDebugBank;

/* name unknown: context-local image information addressed by the result metadata. */
typedef struct VeneziaVipCommandImageInfo {
	uint32_t unidentifiedControl;
	uint16_t width;
	uint16_t height;
	uint32_t unidentifiedParameters[4];
	uint32_t captureBufferOffset;
	uint32_t resultFlags;
	uint32_t commonDebugData[142];
	VeneziaVipCommandImageDebugBank debugBanks[];
} VeneziaVipCommandImageInfo;

/* name unknown: unpack the image location and metadata for an auxiliary result. */
int getVeneziaVipCommandResultMetadata(VeneziaVipCommandContext *context,
	const VeneziaVipCommandAuxiliaryRecord *record, uint32_t *imageAddress,
	uint32_t *captureAddress, VeneziaVipCommandResultMetadata *metadata, uint32_t *value)
{
	VeneziaVipCommandResultRecord *result;
	VeneziaVipCommandImageInfo *image;
	uint32_t address, translated, captureOffset;
	unsigned int index;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (!context || context->magic != 1129335126)
		return 1;
	result = (VeneziaVipCommandResultRecord *)((uint32_t)context->activeQueue +
		record->resultAddress - context->bufferBase);
	captureOffset = context->captureParameter - context->captureAddress;
	address = result->imageAddress + 4194320;
	translated = (address & 0xe0007fff) + ((address & 0x78000) << 10) +
		((address & 0x1ff80000) >> 4);
	*imageAddress = translated;
	*captureAddress = captureOffset + translated;
	for (index = 0; index < 12; ++index)
		metadata->words[index] = result->metadata.words[index];
	image = (VeneziaVipCommandImageInfo *)((uint32_t)context +
		result->metadata.contextAddress - context->physicalBase);
	metadata->height = image->height;
	metadata->width = image->width;
	*value = image->resultFlags;
	return 0;
}

_Static_assert(sizeof(VeneziaVipCaptureImageHeader) == 0x1000, "VIP capture image page");
_Static_assert(offsetof(VeneziaVipCaptureImageHeader, captureBase) == 0x790,
	"VIP capture image buffer control");

/* name unknown: decode an auxiliary value only when its valid control bit is set. */
__attribute__((noinline)) int getVeneziaVipCommandAuxiliaryStatus(const VeneziaVipCommandAuxiliaryRecord *record,
				      VeneziaVipCommandAuxiliaryStatus *status)
{
	uint16_t control;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	control = record->control;
	if (!(control & 0x8000))
		return 3;
	status->controlFlag = !!(control & 0x2000);
	status->value = record->value;
	return 0;
}

/* name unknown: copy pair-swapped image debug data for the selected processor. */
int copyVeneziaVipCommandImageDebugData(VeneziaVipCommandContext *context,
	const VeneziaVipCommandAuxiliaryRecord *record, unsigned int processor, uint32_t *data)
{
	VeneziaVipCommandResultRecord *result;
	VeneziaVipCommandImageInfo *image;
	const uint32_t *source;
	unsigned int index;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (!context || processor > 1 || context->magic != 1129335126)
		return 1;
	result = (VeneziaVipCommandResultRecord *)((uint32_t)context->activeQueue +
		record->resultAddress - context->bufferBase);
	image = (VeneziaVipCommandImageInfo *)((uint32_t)context +
		result->metadata.contextAddress - context->physicalBase);
	if (!(image->resultFlags & (4 << processor)))
		return 3;
	source = image->debugBanks[processor].debugData;
	index = 0;
	do {
		__builtin_prefetch(data + index + 19);
		data[index] = source[index ^ 1];
		data[index + 1] = source[(index + 1) ^ 1];
		data[index + 2] = source[(index + 2) ^ 1];
		data[index + 3] = source[(index + 3) ^ 1];
		data[index + 4] = source[(index + 4) ^ 1];
		data[index + 5] = source[(index + 5) ^ 1];
		data[index + 6] = source[(index + 6) ^ 1];
		data[index + 7] = source[(index + 7) ^ 1];
		index += 8;
	} while (index != 128);
	do {
		data[index] = source[index ^ 1];
		++index;
	} while (index != 130);
	return 0;
}

/* name unknown: copy the auxiliary image data bank in host word-pair order. */
int copyVeneziaVipCommandImageAuxiliaryData(VeneziaVipCommandContext *context,
	const VeneziaVipCommandAuxiliaryRecord *record, unsigned int processor, uint32_t *data)
{
	VeneziaVipCommandResultRecord *result;
	VeneziaVipCommandImageInfo *image;
	const uint32_t *source;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (!context || processor > 1 || context->magic != 1129335126)
		return 1;
	result = (VeneziaVipCommandResultRecord *)((uint32_t)context->activeQueue +
		record->resultAddress - context->bufferBase);
	image = (VeneziaVipCommandImageInfo *)((uint32_t)context +
		result->metadata.contextAddress - context->physicalBase);
	if (!(image->resultFlags & (16 << processor)))
		return 3;
	source = image->debugBanks[processor].auxiliaryData;
	data[0] = source[1];
	data[1] = source[0];
	data[2] = source[3];
	data[3] = source[2];
	data[4] = source[5];
	data[5] = source[4];
	data[6] = source[7];
	data[7] = source[6];
	data[8] = source[9];
	data[9] = source[8];
	data[10] = source[11];
	data[11] = source[10];
	return 0;
}

/* name unknown: copy the second auxiliary image bank in host word-pair order. */
int copyVeneziaVipCommandImageSecondaryData(VeneziaVipCommandContext *context,
	const VeneziaVipCommandAuxiliaryRecord *record, unsigned int processor, uint32_t *data)
{
	VeneziaVipCommandResultRecord *result;
	VeneziaVipCommandImageInfo *image;
	const uint32_t *source;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (!context || processor > 1 || context->magic != 1129335126)
		return 1;
	result = (VeneziaVipCommandResultRecord *)((uint32_t)context->activeQueue +
		record->resultAddress - context->bufferBase);
	image = (VeneziaVipCommandImageInfo *)((uint32_t)context +
		result->metadata.contextAddress - context->physicalBase);
	if (!(image->resultFlags & (64 << processor)))
		return 3;
	source = image->debugBanks[processor].secondaryData;
	data[0] = source[1];
	data[1] = source[0];
	data[2] = source[3];
	data[3] = source[2];
	data[4] = source[5];
	data[5] = source[4];
	data[6] = source[7];
	data[7] = source[6];
	data[8] = source[9];
	data[9] = source[8];
	data[10] = source[11];
	data[11] = source[10];
	data[12] = source[13];
	data[13] = source[12];
	data[14] = source[15];
	data[15] = source[14];
	return 0;
}

/* name unknown: return the image status word pair in host order. */
int getVeneziaVipCommandImageStatusData(VeneziaVipCommandContext *context,
	const VeneziaVipCommandAuxiliaryRecord *record, unsigned int processor, uint32_t *data)
{
	VeneziaVipCommandResultRecord *result;
	VeneziaVipCommandImageInfo *image;
	const uint32_t *source;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (!context || processor > 1 || context->magic != 1129335126)
		return 1;
	result = (VeneziaVipCommandResultRecord *)((uint32_t)context->activeQueue +
		record->resultAddress - context->bufferBase);
	image = (VeneziaVipCommandImageInfo *)((uint32_t)context +
		result->metadata.contextAddress - context->physicalBase);
	if (!(image->resultFlags & (256 << processor)))
		return 3;
	source = image->debugBanks[processor].statusData;
	data[0] = source[1];
	data[1] = source[0];
	return 0;
}

/* name unknown: copy the unswapped four-word image control bank. */
int copyVeneziaVipCommandImageControlData(VeneziaVipCommandContext *context,
	const VeneziaVipCommandAuxiliaryRecord *record, unsigned int processor, uint32_t *data)
{
	VeneziaVipCommandResultRecord *result;
	VeneziaVipCommandImageInfo *image;
	const uint32_t *source;
	unsigned int index;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (!context || processor > 1 || context->magic != 1129335126)
		return 1;
	result = (VeneziaVipCommandResultRecord *)((uint32_t)context->activeQueue +
		record->resultAddress - context->bufferBase);
	image = (VeneziaVipCommandImageInfo *)((uint32_t)context +
		result->metadata.contextAddress - context->physicalBase);
	if (!(image->resultFlags & (1024 << processor)))
		return 3;
	source = image->debugBanks[processor].controlData;
	for (index = 0; index < 4; ++index)
		data[index] = source[index];
	return 0;
}

/* name unknown: copy the processor-independent image debug bank in host order. */
int copyVeneziaVipCommandCommonDebugData(VeneziaVipCommandContext *context,
				       const VeneziaVipCommandAuxiliaryRecord *record, uint32_t *data)
{
	VeneziaVipCommandResultRecord *result;
	VeneziaVipCommandImageInfo *image;
	unsigned int index;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (!context || context->magic != 1129335126)
		return 1;
	result = (VeneziaVipCommandResultRecord *)((uint32_t)context->activeQueue +
		record->resultAddress - context->bufferBase);
	image = (VeneziaVipCommandImageInfo *)((uint32_t)context +
		result->metadata.contextAddress - context->physicalBase);
	if (!(image->resultFlags & 2))
		return 3;
	index = 0;
	do {
		__builtin_prefetch(data + index + 19);
		data[index] = image->commonDebugData[index ^ 1];
		data[index + 1] = image->commonDebugData[(index + 1) ^ 1];
		data[index + 2] = image->commonDebugData[(index + 2) ^ 1];
		data[index + 3] = image->commonDebugData[(index + 3) ^ 1];
		data[index + 4] = image->commonDebugData[(index + 4) ^ 1];
		data[index + 5] = image->commonDebugData[(index + 5) ^ 1];
		data[index + 6] = image->commonDebugData[(index + 6) ^ 1];
		data[index + 7] = image->commonDebugData[(index + 7) ^ 1];
		index += 8;
	} while (index != 136);
	do {
		data[index] = image->commonDebugData[index ^ 1];
		++index;
	} while (index != 142);
	return 0;
}

/* name unknown: sized capture metadata header followed by a byte-order-corrected payload. */
typedef struct VeneziaVipCommandCaptureOutput {
	SceSize size;
	SceSize payloadSize;
	uint32_t metadata[16];
	uint8_t payload[];
} VeneziaVipCommandCaptureOutput;

/* name unknown: copy capture metadata and the requested available payload bytes. */
int copyVeneziaVipCommandCaptureOutput(VeneziaVipCommandContext *context,
	const VeneziaVipCommandAuxiliaryRecord *record, unsigned int processor,
	VeneziaVipCommandCaptureOutput *output, SceSize capacity)
{
	VeneziaVipCommandResultRecord *result;
	VeneziaVipCommandImageInfo *image;
	const uint32_t *metadata;
	const uint8_t *source;
	uint32_t offset;
	SceSize size, index;

	if (g_veneziaVipCommandState.magic != 5261654)
		return 2;
	if (!context || processor > 1 || !record || context->magic != 1129335126)
		return 1;
	result = (VeneziaVipCommandResultRecord *)((uint32_t)context->activeQueue +
		record->resultAddress - context->bufferBase);
	image = (VeneziaVipCommandImageInfo *)((uint32_t)context +
		result->metadata.contextAddress - context->physicalBase);
	if (!(image->resultFlags & (4096 << processor)))
		return 3;
	output->size = sizeof(*output);
	metadata = image->debugBanks[processor].captureMetadata;
	output->metadata[0] = metadata[1];
	output->metadata[1] = metadata[0];
	output->metadata[2] = metadata[3];
	output->metadata[3] = metadata[2];
	output->metadata[4] = metadata[5];
	output->metadata[5] = metadata[4];
	output->metadata[6] = metadata[7];
	output->metadata[7] = metadata[6];
	output->metadata[8] = metadata[9];
	output->metadata[9] = metadata[8];
	output->metadata[10] = metadata[11];
	output->metadata[11] = metadata[10];
	output->metadata[12] = metadata[13];
	output->metadata[13] = metadata[12];
	output->metadata[14] = metadata[15];
	output->metadata[15] = metadata[14];
	offset = image->captureBufferOffset;
	if (processor)
		offset += context->captureLength;
	source = (const uint8_t *)(context->captureMemory.virtualAddress + offset);
	size = 8 * (output->metadata[0] & 0x03ffffff);
	if (capacity < size)
		size = capacity;
	output->payloadSize = size;
	for (index = 0; index != size; ++index)
		output->payload[index] = source[index ^ 7];
	return 0;
}

int initializeVeneziaVipResources(void);
int startVeneziaVipDriver(int mode);

int initializeVeneziaVipProcessStorage(VeneziaVipProcessStorage *storage);

extern int dispatchVeneziaVipProcessSuspend(VeneziaVipProcessStorage *storage, int reason);

extern int dispatchVeneziaVipProcessResume(VeneziaVipProcessStorage *storage);
extern int dispatchVeneziaVipProcessReset(VeneziaVipProcessStorage *storage);

__attribute__((noinline)) int resumeVeneziaVipState(void);

/* name unknown: resume hook forwarded to the VIP state helper. */
int resumeHwd(void)
{
	return resumeVeneziaVipState();
}

/* name unknown: divide the three capture memory regions evenly among processors. */
int distributeVeneziaVipCaptureMemory(VeneziaVipProcessStorage *storage,
				    unsigned int processorCount, VeneziaVipMemoryLayout *layout)
{
	SceSize scratchSize = layout->scratch.size / processorCount;
	SceSize bufferSize = layout->buffer.size / processorCount;
	SceSize snapshotSize = layout->snapshot.size / processorCount;
	SceSize scratchOffset = 0, bufferOffset = 0, snapshotOffset = 0;
	unsigned int index;
	VeneziaDebugProcessCapture *capture = storage->captures;

	for (index = 0; index != processorCount; ++index, ++capture) {
		setVeneziaVipMemoryRegion(&capture->scratchRegion, &layout->scratch, scratchOffset, scratchSize);
		setVeneziaVipMemoryRegion(&capture->bufferRegion, &layout->buffer, bufferOffset, bufferSize);
		setVeneziaVipMemoryRegion(&capture->snapshotRegion, &layout->snapshot, snapshotOffset, snapshotSize);
		scratchOffset += scratchSize;
		bufferOffset += bufferSize;
		snapshotOffset += snapshotSize;
	}
	return 0;
}

/* name unknown: this firmware retains no separate capture-memory release work. */
__attribute__((noinline)) int releaseVeneziaVipCaptureMemory(VeneziaVipProcessStorage *storage)
{
	(void)storage;
	return 0;
}

extern int initializeVeneziaVipProcessQueue(VeneziaVipProcessQueue *queue);

/* name unknown: reset a process's capture flags, mode, counters and queues. */
int initializeVeneziaVipProcessStorage(VeneziaVipProcessStorage *storage)
{
	storage->mode = 6;
	storage->processors[0] = -1;
	storage->processors[1] = -1;
	storage->processors[2] = -1;
	storage->processors[3] = -1;
	storage->captures[0].flags = 0;
	storage->captures[1].flags = 0;
	storage->captures[2].flags = 0;
	storage->captures[3].flags = 0;
	storage->state = 0;
	storage->status[0] = 0;
	storage->status[1] = 0;
	storage->status[2] = 0;
	storage->count = 0;
	storage->pendingCounters[0] = 0;
	storage->pendingCounters[1] = 0;
	initializeVeneziaVipProcessQueue(&storage->queues[0]);
	initializeVeneziaVipProcessQueue(&storage->queues[1]);
	return 0;
}

/* name unknown: allocate the driver code region and map its peripheral pages. */
int initializeVeneziaVipResources(void)
{
	VeneziaVipState *state = &g_veneziaVipState;
	SceKernelPhyMemRangeInfo info;
	SceKernelAllocMemBlockKernelOpt opt;
	void *base;
	SceUID block;

	memset(state, 0, sizeof(*state));
	state->magic = 0x56495044;
	state->eventFlag = -1;
	info.size = sizeof(info);
	info.base = 0;
	info.rangeSize = 0;
	sceKernelGetPhyMemInfo(1, &info);
	state->physicalRanges[0].base = info.base;
	state->physicalRanges[0].size = info.rangeSize;
	info.size = sizeof(info);
	info.base = 0;
	info.rangeSize = 0;
	sceKernelGetPhyMemInfo(9, &info);
	state->physicalRanges[1].base = info.base;
	state->physicalRanges[1].size = info.rangeSize;
	if (info.rangeSize < 0x100000)
		return -2141060856;
	if (allocateVeneziaVipMemoryBlock(&state->codeMemory, "SceVipLpddrCode",
			0x20108006, 0x100000, info.base + info.rangeSize - 0x100000) < 0)
		return -2141060862;
	offsetVeneziaVipMemoryRegion(&state->debuggerMemory, &state->codeMemory.region, 0);

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	opt.paddr = PERVASIVE_RESET_BASE;
	block = sceKernelAllocMemBlockForKernel("ScePervasiveResetReg", 0x20100206, 0x1000, &opt);
	if (block < 0)
		return block;
	state->resetBlock = block;
	sceKernelGetMemBlockBase(block, &base);
	state->resetRegisters = base;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	opt.paddr = PERVASIVE_MAILBOX_BASE;
	block = sceKernelAllocMemBlockForKernel("ScePervasiveMailboxReg", 0x20100206, 0x1000, &opt);
	if (block < 0)
		return block;
	state->mailboxBlock = block;
	sceKernelGetMemBlockBase(block, &base);
	state->mailboxRegisters = base;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	opt.paddr = PERVASIVE_TAS_BASE(0);
	block = sceKernelAllocMemBlockForKernel("ScePervasiveTasReg", 0x20100206, 0x8000, &opt);
	if (block < 0)
		return block;
	state->tasBlock = block;
	sceKernelGetMemBlockBase(block, &base);
	state->tasRegisters = base;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	opt.paddr = PERVASIVE2_BASE;
	state->bootRegisters = (ScePervasive2Registers *)getVeneziaBootRegisters();
	return 0;
}

extern int veneziaVipProcessorThread0(SceSize size, void *arg);
extern int veneziaVipProcessorThread1(SceSize size, void *arg);
extern int waitVeneziaVipProcessorResponse(VeneziaVipState *state, unsigned int processor,
	uint32_t *resultFlags);
extern int finishVeneziaVipStartup(VeneziaVipState *state);

/* name unknown: start processor workers and load the VIP firmware. */
int startVeneziaVipDriver(int mode)
{
	VeneziaVipState *state = &g_veneziaVipState;
	SceUID uid;
	int ret;
	unsigned int loadResult;
	void *firmwareBase;

	(void)mode;
	uid = sceKernelCreateEventFlag("SceVipDriver", 0x1000, 0, NULL);
	if (uid < 0)
		goto failed;
	state->eventFlag = uid;
	ret = sceKernelInitializeFastMutex(&state->processMutex, "SceVipDriver", 2, NULL);
	if (ret < 0)
		goto cleanup;
	state->processLock = &state->processMutex;
	ret = sceKernelInitializeFastMutex(&state->apiWrapperMutex, "SceVipFW2ApiWrapper", 2, NULL);
	if (ret < 0)
		goto cleanup;
	state->apiWrapperLock = &state->apiWrapperMutex;
	ret = sceKernelInitializeFastMutex(&state->requestMutex, "SceVipFW2Request", 2, NULL);
	if (ret < 0)
		goto cleanup;
	state->requestLock = &state->requestMutex;
	ret = sceKernelInitializeFastMutex(&state->standardBufferMutex, "SceVipStdBuf", 2, NULL);
	if (ret < 0)
		goto cleanup;
	state->standardBufferLock = &state->standardBufferMutex;
	ret = sceKernelInitializeFastMutex(&state->commonProcessorMutex, "SceVipCommonPu0Pu1", 2, NULL);
	if (ret < 0)
		goto cleanup;
	state->commonProcessorLock = &state->commonProcessorMutex;
	uid = sceKernelCreateEventFlag("SceVipDriverPu0", 0x1000, 0, NULL);
	if (uid < 0)
		goto failed;
	state->processorEvents[0] = uid;
	uid = sceKernelCreateEventFlag("SceVipDriverPu1", 0x1000, 0, NULL);
	if (uid < 0)
		goto failed;
	state->processorEvents[1] = uid;
	uid = sceKernelCreateEventFlag("SceVipDriverInt0", 0x1000, 0, NULL);
	if (uid < 0)
		goto failed;
	state->auxiliaryEventFlag = uid;
	uid = sceKernelCreateEventFlag("SceVipDriverInt1", 0x1000, 0, NULL);
	if (uid < 0)
		goto failed;
	state->secondaryInterruptEvent = uid;
	uid = sceKernelCreateThread("SceVipDriverPu0", veneziaVipProcessorThread0,
		72, 0x1000, 0, 0x80000, NULL);
	if (uid < 0)
		goto cleanup;
	state->processorThreads[0] = uid;
	uid = sceKernelCreateThread("SceVipDriverPu1", veneziaVipProcessorThread1,
		72, 0x1000, 0, 0x80000, NULL);
	if (uid < 0)
		goto cleanup;
	state->processorThreads[1] = uid;
	ret = sceKernelStartThread(state->processorThreads[0], sizeof(*state), &state);
	if (ret < 0)
		goto cleanup;
	ret = sceKernelStartThread(state->processorThreads[1], sizeof(*state), &state);
	if (ret < 0)
		goto cleanup;
	waitVeneziaVipProcessorResponse(state, 0, 0);
	waitVeneziaVipProcessorResponse(state, 1, 0);
	loadResult = 0;
	firmwareBase = (void *)state->debuggerMemory.virtualAddress;
	if (sceSblFwLoaderSetPath("os0:kd/vipimg.skprx", 0) < 0)
		goto failed;
	if (sceSblFwLoaderLoad(1, firmwareBase, 0xbf000, &loadResult) < 0) {
		sceSblFwLoaderClose();
		goto failed;
	}
	if (sceSblFwLoaderClose() < 0)
		goto failed;
	memset((uint8_t *)firmwareBase + 0xbf000, 0, 0x10000);
	if (initializeVeneziaHwdDma() < 0)
		goto failed;
	g_veneziaDebugger.log = (void (*)(unsigned int, unsigned int, ...))SceSysrootForDriver_483EF108();
	return finishVeneziaVipStartup(state);
failed:
	ret = -2141060860;
cleanup:
	releaseVeneziaVipMemoryBlock(&state->codeMemory);
	if (state->resetBlock > 0) {
		state->resetRegisters = NULL;
		sceKernelFreeMemBlock(state->resetBlock);
		state->resetBlock = 0;
	}
	if (state->mailboxBlock > 0) {
		state->mailboxRegisters = NULL;
		sceKernelFreeMemBlock(state->mailboxBlock);
		state->mailboxBlock = 0;
	}
	if (state->tasBlock > 0) {
		state->tasRegisters = NULL;
		sceKernelFreeMemBlock(state->tasBlock);
		state->tasBlock = 0;
	}
	state->bootRegisters = NULL;
	return ret;
}

/* PLACEHOLDER: Sony export name unknown; select this process's VIP operating mode. */
int SceCodecEngineWrapperForDriver_2B7CC150(unsigned int mode)
{
	VeneziaVipProcessStorage *storage;
	int ret = waitVeneziaVipProcessReady();

	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	if (mode - 1 > 5)
		return -2141060863;
	storage->mode = mode;
	return 0;
}

/* PLACEHOLDER: Sony export name unknown; change the idle process's capture count. */
int SceCodecEngineWrapperForDriver_86861877(unsigned int flags, int wait, SceUID pid)
{
	VeneziaVipProcessStorage *storage;
	int ret = -2147418017;

	if (wait) {
		ret = waitVeneziaVipProcessReady();
		if (ret < 0)
			return ret;
	}
	storage = _sceVipGetVipProcessResource(pid);
	if (!storage)
		return -2141060856;
	lockFastMutex(g_veneziaVipState.processLock);
	if (storage->status[0] || storage->processors[0] != -1) {
		ret = -2141060858;
	} else if (flags && !(flags & ~3u) && flags != 3) {
		if (flags & 2)
			storage->count = 1;
		if (flags & 1)
			storage->count = 0;
	} else {
		sceKernelUnlockFastMutex(g_veneziaVipState.processLock);
		return -2141060863;
	}
	sceKernelUnlockFastMutex(g_veneziaVipState.processLock);
	return ret;
}

/* PLACEHOLDER: Sony export name unknown; update permitted idle-process state bits. */
int SceCodecEngineWrapperForDriver_62996F94(unsigned int value, unsigned int mask)
{
	VeneziaVipProcessStorage *storage;
	int ret = waitVeneziaVipProcessReady();

	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	lockFastMutex(g_veneziaVipState.processLock);
	if (storage->count == 1)
		mask &= 0x1060000;
	else
		mask &= 0x1000000;
	if (~mask & value) {
		ret = -2141060863;
	} else if (!storage->status[0] && storage->processors[0] == -1) {
		storage->state = (~mask & storage->state) | (mask & value);
	} else {
		sceKernelUnlockFastMutex(g_veneziaVipState.processLock);
		return -2141060858;
	}
	sceKernelUnlockFastMutex(g_veneziaVipState.processLock);
	return ret;
}

/* PLACEHOLDER: Sony export name unknown; set the idle capture transfer limit. */
int SceCodecEngineWrapperForDriver_54F39D1D(unsigned int size)
{
	VeneziaVipProcessStorage *storage;
	int ret = waitVeneziaVipProcessReady();

	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	lockFastMutex(g_veneziaVipState.processLock);
	if (storage->count != 1)
		ret = -2147418017;
	if (storage->status[0] || storage->processors[0] != -1)
		ret = -2141060858;
	else if (size > 0x1000 || size << 29)
		ret = -2141060863;
	else
		storage->status[2] = size;
	sceKernelUnlockFastMutex(g_veneziaVipState.processLock);
	return ret;
}

/* name unknown: select the usable ring capacity for a capture allocation. */
__attribute__((noinline)) SceSize getVeneziaVipRingSize(SceSize size)
{
	if (size <= 0x17e80)
		return size - 16;
	if (size <= 0x80000)
		return 97920;
	if (size <= 0x100000)
		return 691200;
	return 1566720;
}

extern SceSize getVeneziaVipCaptureWorkSize(unsigned int mode);

/* name unknown: calculate complete capture storage from dimensions and processor count. */
__attribute__((noinline)) int calculateVeneziaVipCaptureStorage(unsigned int processorCount,
	unsigned int mode, const VeneziaDebugImageGeometry *geometry, SceSize *total,
	SceSize *image, SceSize *buffer, SceSize *scratch)
{
	unsigned int imageCount, frameSize, totalFrameSize;

	if (!total || !geometry)
		return -2141060862;
	if (!buffer || !image || !scratch)
		return -2141060862;
	if (geometry->width - 64 > 0x740 || geometry->height - 64 > 0x400)
		return -2141060863;
	imageCount = geometry->imageCount;
	if (imageCount > 16)
		return -2141060863;
	if (!imageCount)
		imageCount = 1;
	if (geometry->processorCount > processorCount)
		return -2141060863;
	frameSize = 3 * geometry->height * geometry->width;
	totalFrameSize = frameSize * imageCount;
	frameSize >>= 1;
	totalFrameSize >>= 1;
	if (totalFrameSize <= 0x1bd800 && frameSize <= 0x4a400) {
		*image = 0x140000;
		*buffer = 0x80000;
	} else if (totalFrameSize <= 0x697800 && frameSize <= 0x151800) {
		*image = 0x400000;
		*buffer = 0x100000;
	} else if (totalFrameSize <= 0xc00000) {
		*image = 0x700000;
		*buffer = 0x200000;
	} else {
		return -2141060863;
	}
	*scratch = 0x80000;
	*scratch = (((getVeneziaVipCaptureWorkSize(mode) + 0x3fc00 + 0x3ff) >> 18) << 18) + 0x80000;
	*image *= geometry->processorCount;
	*buffer *= geometry->processorCount;
	*scratch *= geometry->processorCount;
	*total = *scratch + *buffer + *image;
	return 0;
}

__attribute__((noinline)) int lockVeneziaVipProcess(struct ThreadmgrFastMutex *mutex)
{
	return lockFastMutex(mutex);
}

__attribute__((noinline)) int unlockVeneziaVipProcess(struct ThreadmgrFastMutex *mutex)
{
	return sceKernelUnlockFastMutex(mutex);
}

/* name unknown: retain the image allocation and resolve its physical address. */
__attribute__((noinline)) int setVeneziaVipCaptureImage(VeneziaDebugProcessCapture *capture,
	const VeneziaVipCaptureParam *param)
{
	int ret;

	capture->imageVirtualAddress = param->imageVirtualAddress;
	capture->imageSize = param->imageSize;
	ret = sceKernelVAtoPA((const void *)param->imageVirtualAddress,
		&capture->imagePhysicalAddress);
	return ret < 0 ? -2141060862 : ret;
}

/* name unknown: retained no-op VIP resume hook. */
__attribute__((noinline)) int resumeVeneziaVipState(void)
{
	return 0;
}

/* name unknown: test whether the complete physical image lies in main memory. */
__attribute__((noinline)) int isVeneziaVipImageInMainMemory(uint32_t address, SceSize size)
{
	uintptr_t physical;
	int ret = sceKernelVAtoPA((const void *)address, &physical);

	if (ret >= 0)
		ret = physical >= 0x20000000 && physical + size <= 0x30000000;
	return ret;
}

/* PLACEHOLDER: Sony export name unknown; select a 1 MiB physical memory page. */
int SceCodecEngineWrapperForDriver_18197103(uint32_t physicalAddress)
{
	if (physicalAddress >= 0x20000000 && physicalAddress + 1 <= 0x30000000)
		g_veneziaVipState.bootRegisters->mainMemoryPageBase = physicalAddress >> 20 << 20;
	else
		g_veneziaVipState.bootRegisters->externalMemoryPageBase = physicalAddress >> 20 << 20;
	return 0;
}

/* name unknown: program the image address for its selected memory class. */
int programVeneziaVipCaptureImage(VeneziaDebugProcessCapture *capture, VeneziaVipState *state)
{
	ScePervasive2Registers *registers = state->bootRegisters;
	uint32_t physicalAddress = capture->imagePhysicalAddress;

	if (capture->imageMappingMode == 1) {
		registers->captureImagePhysicalAddress[0] = physicalAddress;
		registers->unidentifiedCaptureImageControl[0] = 0;
	} else {
		registers->captureImagePhysicalAddress[1] = physicalAddress;
		registers->unidentifiedCaptureImageControl[1] = 0;
	}
	__asm__ volatile("dmb sy" ::: "memory");
	return 0;
}

const unsigned int g_veneziaVipClockFrequencies[7] = {
	0xffffff, 41, 55, 83, 111, 166, 222,
};
extern int prepareVeneziaVipProcessorCapture(unsigned int count, VeneziaVipState *state);
extern int configureVeneziaVipProcessorCapture(unsigned int count, VeneziaVipState *state);
extern int startVeneziaVipProcessorCapture(unsigned int count, VeneziaVipState *state);

/* name unknown: select the process callbacks and activate its capture memory mapping. */
__attribute__((noinline)) int activateVeneziaVipDebugger(VeneziaVipState *state, VeneziaVipProcessStorage *storage)
{
	ScePervasive2Registers *registers;
	uint32_t lowerBase, upperBase, upperEnd, lowerEnd;

	state->callbackState = &storage->callbackState;
	if (prepareVeneziaVipProcessorCapture(storage->count, state) < 0) {
		state->callbackState = NULL;
		return -2141060860;
	}
	state->activeProcess = storage;
	ScePervasiveForDriver_B1CFA18F();
	ScePervasiveForDriver_8CF567AD();
	configureVeneziaVipProcessorCapture(storage->count, state);
	__asm__ volatile("dmb sy" ::: "memory");
	registers = state->bootRegisters;
	programVeneziaVipCaptureImage(&storage->captures[0], state);
	lowerBase = state->physicalRanges[0].base;
	upperBase = state->physicalRanges[1].base;
	registers->unidentifiedCapturePageControl[1] = 0;
	registers->unidentifiedCapturePageControl[0] = 0;
	registers->captureAperturePhysicalBase = 0x1d000000;
	registers->capturePagePhysicalBase = 0x1d000000;
	registers->captureWindowControl = 0x1d001100;
	if (lowerBase < upperBase) {
		upperEnd = upperBase + state->physicalRanges[1].size;
		lowerEnd = lowerBase + state->physicalRanges[0].size;
		if (upperEnd < lowerEnd) {
			registers->lowerRangeBase = lowerBase;
			registers->upperRangeBase = upperBase;
			registers->upperRangeEnd = upperEnd;
			registers->lowerRangeEnd = lowerEnd;
		}
	}
	__asm__ volatile("dmb sy" ::: "memory");
	ScePowerForDriver_E5573571(g_veneziaVipClockFrequencies[storage->mode]);
	startVeneziaVipProcessorCapture(storage->count, state);
	return 0;
}

extern int stopVeneziaVipProcessorCapture(unsigned int count, VeneziaVipState *state);

/* name unknown: stop the capture clock and detach the active process callbacks. */
__attribute__((noinline)) int stopVeneziaVipDebugger(VeneziaVipState *state, VeneziaVipProcessStorage *storage)
{
	ScePervasiveForDriver_31C0A98B();
	ScePervasiveForDriver_EBE9C84E();
	ScePowerForDriver_E5573571(0);
	if (state->callbackState) {
		stopVeneziaVipProcessorCapture(storage->count, state);
		state->activeProcess = NULL;
		state->callbackState = NULL;
	}
	return 0;
}

/* name unknown: retained capture-preparation hook has no work in these firmwares. */
__attribute__((noinline)) int prepareVeneziaVipCapture(VeneziaVipState *state, VeneziaVipProcessStorage *storage)
{
	(void)state;
	(void)storage;
	return 0;
}

/* name unknown: encode the supplied word-swapped timestamps into a PES header. */
__attribute__((noinline)) int encodeVeneziaVipPesTimestamps(VeneziaVipPesHeader *header,
	const uint64_t *pts, const uint64_t *dts)
{
	uint8_t flags;

	if (pts) {
		header->presentationTimestamp[0] = (((uint32_t)*pts & 1) << 3) | 1 |
			(((uint32_t)(*pts >> 32) >> 30) << 1) | (dts ? 0x30 : 0x20);
		header->presentationTimestamp[1] = (uint32_t)(*pts >> 32) >> 22;
		header->presentationTimestamp[2] = (((uint32_t)(*pts >> 32) >> 15) << 1) | 1;
		header->presentationTimestamp[3] = (uint32_t)(*pts >> 32) >> 7;
		header->presentationTimestamp[4] = ((uint32_t)(*pts >> 32) << 1) | 1;
		if (dts) {
			flags = 0xc0;
			header->decodingTimestamp[0] = (((uint32_t)*dts & 1) << 3) | 0x11 |
				(((uint32_t)(*dts >> 32) >> 30) << 1);
			header->decodingTimestamp[1] = (uint32_t)(*dts >> 32) >> 22;
			header->decodingTimestamp[2] = (((uint32_t)(*dts >> 32) >> 15) << 1) | 1;
			header->decodingTimestamp[3] = (uint32_t)(*dts >> 32) >> 7;
			header->decodingTimestamp[4] = ((uint32_t)(*dts >> 32) << 1) | 1;
		} else {
			flags = 0x80;
		}
	} else {
		if (dts)
			return -2141060863;
		flags = 0;
	}
	header->timestampFlags = flags;
	return 0;
}

/* name unknown: size the Annex B input after replacing length prefixes. */
int calculateVeneziaVipVideoInputSize(const VeneziaVipVideoInput *input)
{
	uint8_t bytes[8];
	unsigned int lengthSize = input->nalLengthSize;
	unsigned int peekSize, nalSize, firstSize, offset, required, nextOffset, type;
	unsigned int total;

	if (lengthSize - 1 > 3 || !((1u << (lengthSize - 1)) & 0xb))
		return -2141060863;
	if (lengthSize > input->payload.size)
		goto invalid;
	peekSize = lengthSize + 1;
	if (sceKernelCopyFromUser(bytes, input->payload.data, peekSize) < 0)
		goto invalid;
	firstSize = decodeVeneziaVipNalLength(bytes, lengthSize);
	if (firstSize + lengthSize > input->payload.size)
		goto invalid;
	type = bytes[lengthSize] & 0x1f;
	if (type == 9) {
		if (lengthSize > input->payload.size ||
		    sceKernelCopyFromUser(bytes, input->payload.data, lengthSize) < 0)
			goto invalid;
		offset = decodeVeneziaVipNalLength(bytes, lengthSize) + lengthSize;
		if (offset >= input->payload.size)
			goto invalid;
		total = firstSize + 292;
	} else {
		offset = 0;
		total = 288;
	}
	if (input->flags & 1)
		total += input->additionalData[0].size + 4;
	if (input->flags & 2)
		total += input->additionalData[1].size + 4;
	required = offset + lengthSize;
	if (required > input->payload.size ||
	    sceKernelCopyFromUser(bytes, (const uint8_t *)input->payload.data + offset, peekSize) < 0)
		goto invalid;
	for (;;) {
		nalSize = decodeVeneziaVipNalLength(bytes, lengthSize);
		if (nalSize + required > input->payload.size)
			goto invalid;
		type = bytes[lengthSize] & 0x1f;
		total += nalSize + ((unsigned int)(type - 7) <= 1 ? 4 : 3);
		if (required > input->payload.size ||
		    sceKernelCopyFromUser(bytes, (const uint8_t *)input->payload.data + offset, lengthSize) < 0)
			return total;
		nalSize = decodeVeneziaVipNalLength(bytes, lengthSize);
		nextOffset = nalSize + lengthSize + offset;
		if (nextOffset >= input->payload.size)
			return total;
		required = nextOffset + lengthSize;
		if (required > input->payload.size)
			goto invalid;
		offset = nextOffset;
		if (sceKernelCopyFromUser(bytes, (const uint8_t *)input->payload.data + nextOffset, peekSize) < 0)
			goto invalid;
	}
invalid:
	return -2141060853;
}

typedef struct VeneziaVipInputBatch {
	unsigned int processor;
	VeneziaVipState *state;
	VeneziaVipProcessStorage *storage;
	VeneziaVipInputBufferList list;
	VeneziaVipInputBuffer buffers[8];
} VeneziaVipInputBatch;

typedef struct VeneziaVipVideoInputWork {
	VeneziaVipPesHeader header;
	VeneziaVipInputBatch batch;
	uint8_t peek[8];
} VeneziaVipVideoInputWork;

const uint8_t g_veneziaVipNalStartCode[4] = { 0, 0, 0, 1 };

static __attribute__((always_inline)) inline int appendVeneziaVipInputBuffer(
	VeneziaVipInputBatch *batch, const void *data, SceSize size)
{
	unsigned int count = batch->list.count;
	int ret;

	if (count > 7) {
		ret = submitVeneziaVipInputBuffers(batch->processor, batch->state,
			batch->storage, &batch->list);
		if (ret < 0)
			return ret;
		count = 0;
		batch->list.buffers = batch->buffers;
	}
	batch->list.count = count + 1;
	batch->buffers[count].data = data;
	batch->buffers[count].size = size;
	return 0;
}

/* name unknown: replace AVC length prefixes and submit a bounded buffer list. */
int submitVeneziaVipVideoInput(unsigned int processor, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage, const VeneziaVipVideoInput *input)
{
	VeneziaVipVideoInputWork work;
	unsigned int lengthSize = input->nalLengthSize;
	unsigned int firstSize, nalSize, type, offset, required;
	const uint8_t *firstPayload, *address, *nalData, *prefix;
	const void *additionalData;
	SceSize prefixSize;
	unsigned int flushPrefix;
	int ret;

	if (lengthSize - 1 > 3 || !((1u << (lengthSize - 1)) & 0xb))
		return -2141060863;
	work.batch.processor = processor;
	work.batch.state = state;
	work.batch.storage = storage;
	work.batch.list.count = 0;
	work.batch.list.buffers = work.batch.buffers;
	work.header = g_veneziaVipPesHeader;
	ret = encodeVeneziaVipPesTimestamps(&work.header,
		input->presentationTimestamp == UINT64_MAX ? NULL : &input->presentationTimestamp,
		input->decodingTimestamp == UINT64_MAX ? NULL : &input->decodingTimestamp);
	if (ret < 0)
		return ret;
	ret = appendVeneziaVipInputBuffer(&work.batch, &work.header, sizeof(work.header));
	if (ret < 0)
		return ret;
	firstPayload = input->payload.data;
	if (lengthSize > input->payload.size ||
	    sceKernelCopyFromUser(work.peek, firstPayload, lengthSize + 1) < 0)
		goto invalid;
	firstSize = decodeVeneziaVipNalLength(work.peek, lengthSize);
	type = work.peek[lengthSize] & 0x1f;
	if (firstSize + lengthSize > input->payload.size)
		goto invalid;
	if (type == 9) {
		ret = appendVeneziaVipInputBuffer(&work.batch, g_veneziaVipNalStartCode, 4);
		if (ret < 0)
			return ret;
		ret = appendVeneziaVipInputBuffer(&work.batch, firstPayload + lengthSize, firstSize);
		if (ret < 0)
			return ret;
		if (lengthSize > input->payload.size ||
		    sceKernelCopyFromUser(work.peek, input->payload.data, lengthSize) < 0)
			goto invalid;
		offset = decodeVeneziaVipNalLength(work.peek, lengthSize) + lengthSize;
		if (offset >= input->payload.size)
			goto invalid;
	} else {
		offset = 0;
	}
	if ((input->flags & 1) && input->additionalData[0].data) {
		additionalData = input->additionalData[0].data;
		flushPrefix = work.batch.list.count > 7;
		ret = appendVeneziaVipInputBuffer(&work.batch, g_veneziaVipNalStartCode, 4);
		if (ret < 0)
			return ret;
		if (flushPrefix)
			additionalData = input->additionalData[0].data;
		ret = appendVeneziaVipInputBuffer(&work.batch,
			additionalData, input->additionalData[0].size);
		if (ret < 0)
			return ret;
	}
	if ((input->flags & 2) && input->additionalData[1].data) {
		additionalData = input->additionalData[1].data;
		flushPrefix = work.batch.list.count > 7;
		ret = appendVeneziaVipInputBuffer(&work.batch, g_veneziaVipNalStartCode, 4);
		if (ret < 0)
			return ret;
		if (flushPrefix)
			additionalData = input->additionalData[1].data;
		ret = appendVeneziaVipInputBuffer(&work.batch,
			additionalData, input->additionalData[1].size);
		if (ret < 0)
			return ret;
	}
	required = offset + lengthSize;
	address = (const uint8_t *)input->payload.data + offset;
	if (required > input->payload.size)
		goto invalid;
	for (;;) {
		if (sceKernelCopyFromUser(work.peek, address, lengthSize + 1) < 0)
			goto invalid;
		nalSize = decodeVeneziaVipNalLength(work.peek, lengthSize);
		nalData = address + lengthSize;
		if (required + nalSize > input->payload.size)
			goto invalid;
		type = work.peek[lengthSize] & 0x1f;
		prefixSize = (unsigned int)(type - 7) <= 1 ? 4 : 3;
		prefix = g_veneziaVipNalStartCode + (prefixSize == 3);
		ret = appendVeneziaVipInputBuffer(&work.batch, prefix, prefixSize);
		if (ret < 0)
			return ret;
		ret = appendVeneziaVipInputBuffer(&work.batch, nalData, nalSize);
		if (ret < 0)
			return ret;
		if (required > input->payload.size ||
		    sceKernelCopyFromUser(work.peek,
			(const uint8_t *)input->payload.data + offset, lengthSize) < 0)
			break;
		offset += decodeVeneziaVipNalLength(work.peek, lengthSize) + lengthSize;
		if (offset >= input->payload.size)
			break;
		required = offset + lengthSize;
		address = (const uint8_t *)input->payload.data + offset;
		if (required > input->payload.size)
			goto invalid;
	}
	return submitVeneziaVipInputBuffers(work.batch.processor, work.batch.state,
		work.batch.storage, &work.batch.list);
invalid:
	return -2141060853;
}

/* name unknown: collect processor-one stops and signal its interrupt events. */
int veneziaDebugSecondaryInterruptHandler(int interrupt, VeneziaVipState *state)
{
	VeneziaDebugCallbackState *callbacks = state->callbackState;
	VeneziaDebugStopInfo info;
	uint32_t status[2], events, interruptEvents, saved;
	int ret;

	(void)interrupt;
	ret = getVeneziaVipStopInfo(1, &info);
	if (ret != 0 && ret != 6)
		info.result = INT32_MIN;
	ret = clearVeneziaVipInterruptStatus(1, status);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(95, 3, 1, status[0], ret);
	if (!ret) {
		saved = sceKernelSpinlockLowLockCpuSuspendIntr(&g_veneziaDebugger.spinlock);
		++callbacks->counters[1].count;
		switch (info.result) {
		case -12: events = 0x80000000; break;
		case -11: events = 0x40000000; break;
		case -10: events = 0x20000000; break;
		case 0:
			events = 0x10000000;
			if (info.flags & 0x20000)
				events |= 0x80000;
			break;
		case INT32_MIN: events = 0; break;
		case -1: events = 1; break;
		case -2: events = 2; break;
		case -3: events = 4; break;
		case -4: events = 8; break;
		case -5: events = 16; break;
		case -6: events = 32; break;
		default: events = 64; break;
		}
		interruptEvents = events | 0x200000;
		if (status[0] & 0x1ff8000) {
			if (status[0] & 0x1000000) {
				events = interruptEvents;
				++callbacks->counters[1].unidentifiedControl;
			}
			if (status[0] & 0x800000) {
				events = interruptEvents;
				++callbacks->counters[2].count;
			}
			if (status[0] & 0x400000) {
				events |= 0x400000;
				++callbacks->counters[2].unidentifiedControl;
			}
			if (status[0] & 0x200000) {
				events |= 0x400000;
				++callbacks->counters[3].count;
			}
			if (status[0] & 0x100000) {
				events |= 0x400000;
				++callbacks->counters[3].unidentifiedControl;
			}
			if (status[0] & 0x80000) {
				events |= 0x800000;
				++callbacks->counters[4].count;
			}
			if (status[0] & 0x40000) {
				events |= 0x1000000;
				++callbacks->counters[4].unidentifiedControl;
			}
			if (status[0] & 0x20000) {
				events |= 0x2000000;
				++callbacks->counters[5].count;
			}
			if (status[0] & 0x10000) {
				events |= 0x4000000;
				++callbacks->counters[5].unidentifiedControl;
			}
			if (status[0] & 0x8000) {
				events |= 0x8000000;
				++callbacks->counters[6].count;
			}
		}
		sceKernelSpinlockLowUnlockCpuResumeIntr(&g_veneziaDebugger.spinlock, saved);
		sceKernelSetEventFlag(state->secondaryInterruptEvent, events);
	}
	__asm__ volatile("dmb sy" ::: "memory");
	return -1;
}

/* name unknown: physical-address access permits selected mailbox words and the
 * mapped peripheral span beginning at the TAS page. */
uint32_t writeVeneziaVipPhysicalWord(uint32_t address, uint32_t value)
{
	if (address - PERVASIVE_MAILBOX_BASE >= PERVASIVE_PAGE_SIZE) {
		if (address - PERVASIVE_TAS_BASE(0) < sizeof(ScePervasiveTasRegisters))
			*(volatile uint32_t *)((uintptr_t)g_veneziaVipState.tasRegisters +
				address - PERVASIVE_TAS_BASE(0)) = value;
		return address;
	}
	switch (address) {
	case MAILBOX_PADDR(debugReceive[0]): g_veneziaVipState.mailboxRegisters->debugReceive[0] = value; break;
	case MAILBOX_PADDR(debugReceive[1]): g_veneziaVipState.mailboxRegisters->debugReceive[1] = value; break;
	case MAILBOX_PADDR(debugAcknowledge[0]): g_veneziaVipState.mailboxRegisters->debugAcknowledge[0] = value; break;
	case MAILBOX_PADDR(debugAcknowledge[1]): g_veneziaVipState.mailboxRegisters->debugAcknowledge[1] = value; break;
	case MAILBOX_PADDR(debugSend[0]): g_veneziaVipState.mailboxRegisters->debugSend[0] = value; break;
	case MAILBOX_PADDR(debugSend[1]): g_veneziaVipState.mailboxRegisters->debugSend[1] = value; break;
	case MAILBOX_PADDR(unidentifiedDebugWriteControl[0]): g_veneziaVipState.mailboxRegisters->unidentifiedDebugWriteControl[0] = value; break;
	case MAILBOX_PADDR(unidentifiedDebugWriteControl[1]): g_veneziaVipState.mailboxRegisters->unidentifiedDebugWriteControl[1] = value; break;
	default: break;
	}
	return address;
}

uint32_t readVeneziaVipPhysicalWord(uint32_t address)
{
	if (address - PERVASIVE_MAILBOX_BASE >= PERVASIVE_PAGE_SIZE) {
		if (address - PERVASIVE_TAS_BASE(0) < sizeof(ScePervasiveTasRegisters))
			return *(volatile uint32_t *)((uintptr_t)g_veneziaVipState.tasRegisters +
				address - PERVASIVE_TAS_BASE(0));
		return 0;
	}
	switch (address) {
	case MAILBOX_PADDR(debugReceive[0]): return g_veneziaVipState.mailboxRegisters->debugReceive[0];
	case MAILBOX_PADDR(debugReceive[1]): return g_veneziaVipState.mailboxRegisters->debugReceive[1];
	case MAILBOX_PADDR(debugSend[0]): return g_veneziaVipState.mailboxRegisters->debugSend[0];
	case MAILBOX_PADDR(debugSend[1]): return g_veneziaVipState.mailboxRegisters->debugSend[1];
	default: return 0;
	}
}

typedef struct VeneziaVipCaptureImageLayout {
	SceSize imageSize;
	SceSize primarySize;
	SceSize secondarySize;
} VeneziaVipCaptureImageLayout;

const VeneziaVipCaptureImageLayout g_veneziaVipCaptureImageLayouts[3] = {
	{ 0x140000, 0x80000, 0xc0000 },
	{ 0x400000, 0x199800, 0x266800 },
	{ 0x700000, 0x2cd000, 0x433000 },
};

#if FW_VERSION >= 0x04000000
#define VIP_CAPTURE_SYMBOL(debug, release) debug
#else
#define VIP_CAPTURE_SYMBOL(debug, release) release
#endif

extern int beginVeneziaVipCaptureSetup(unsigned int processor, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage);
extern int endVeneziaVipCaptureSetup(unsigned int processor, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage);
extern int setVeneziaVipCaptureContext(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, VeneziaVipCommandContext *context);
extern int getVeneziaVipContextStorageSizeLocked(VeneziaVipState *state, SceSize *size);
extern int getVeneziaVipQueueStorageSizeLocked(VeneziaVipState *state, SceSize *size);
extern int encodeVeneziaVipCaptureAddressLocked(VeneziaVipState *state,
	uint32_t base, uint32_t offset, uint32_t *encoded);
extern VeneziaVipCommandContext *initializeVeneziaVipCaptureContextLocked(
	VeneziaVipState *state, const VeneziaVipCommandContextParam *param,
	unsigned int processor, uint32_t contextValue);
extern int initializeVeneziaVipCaptureQueuesLocked(VeneziaVipState *state,
	VeneziaVipCommandContext *context, const VeneziaVipCommandQueueParam *param);
extern int configureVeneziaVipCaptureMemoryLocked(VeneziaVipState *state,
	VeneziaVipCommandContext *context, SceSize length, const VeneziaVipMemoryRegion *memory);
extern int prepareVeneziaVipProcessorContext(unsigned int processor, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage);
extern int configureVeneziaVipProcessorQueues(unsigned int processor, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage, uint32_t flags);
extern int startVeneziaVipProcessorContext(unsigned int processor, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage);
#undef VIP_CAPTURE_SYMBOL

/* name unknown: bind the processor-one image and its command/capture queues. */
int setupVeneziaVipCapture(unsigned int processor, VeneziaVipProcessStorage *storage,
	const SceSize *size, const VeneziaDebugImageFormat *const *format)
{
	VeneziaDebugProcessCapture *capture = &storage->captures[processor];
	VeneziaVipCommandContextParam contextParam;
	VeneziaVipCommandQueueParam queueParam;
	VeneziaVipCommandContext *context;
	const VeneziaVipCaptureImageLayout *layout;
	SceSize captureSize, contextSize, queueSize, firstSize, secondSize, length;
	uint32_t mode, physical, virtualAddress;
	int ret, status;

	if (beginVeneziaVipCaptureSetup(processor, &g_veneziaVipState, storage) < 0) {
		ret = -2141060856;
		goto out;
	}
	setVeneziaVipCaptureContext(capture, &g_veneziaVipState, NULL);
	ret = getVeneziaVipCaptureSize(1, &g_veneziaVipState, *format, &captureSize);
	if (ret < 0)
		goto out;
	if (*size < captureSize) {
		ret = -2141060863;
		goto out;
	}
	status = getVeneziaVipContextStorageSizeLocked(&g_veneziaVipState, &contextSize);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(60, 2, contextSize, status);
	if (status || contextSize > 0x50000)
		goto invalid;
	status = getVeneziaVipQueueStorageSizeLocked(&g_veneziaVipState, &queueSize);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(61, 2, queueSize, status);
	if (status || queueSize > 0x800)
		goto invalid;
	switch (capture->scratchRegion.size) {
	case 0x140000: mode = 0; break;
	case 0x400000: mode = 1; break;
	case 0x700000: mode = 2; break;
	default: goto invalid;
	}
	capture->imageHeader = (VeneziaVipCaptureImageHeader *)capture->snapshotRegion.virtualAddress;
	contextParam.commandValues[0] = PERVASIVE_TAS_BASE(0);
	contextParam.commandValues[1] = PERVASIVE_TAS_BASE(1);
	contextParam.physicalBase = capture->snapshotRegion.physicalAddress + 0x1000;
	contextParam.context = (VeneziaVipCommandContext *)(capture->snapshotRegion.virtualAddress + 0x1000);
	contextParam.size = 0x50000;
	queueParam.bufferBase = capture->snapshotRegion.physicalAddress + 0x51000;
	queueParam.storage = (VeneziaVipCommandQueueStorage *)(capture->snapshotRegion.virtualAddress + 0x51000);
	queueParam.storageSize = 0x800;
	queueParam.captureParameter = capture->imageVirtualAddress;
	status = encodeVeneziaVipCaptureAddressLocked(&g_veneziaVipState,
		capture->imagePhysicalAddress, 0, &queueParam.captureAddress);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(62, 4, capture->imagePhysicalAddress, 0, &queueParam.captureAddress, status);
	if (status)
		goto invalid;
	firstSize = (capture->flags & 0x40000) ? 0x14000 : 0x4000;
	secondSize = (capture->flags & 0x40000) ? 0x14000 : 0x6000;
	layout = &g_veneziaVipCaptureImageLayouts[mode];
	queueParam.captureOption = capture->imageSize;
	queueParam.physicalBase = contextParam.commandValues[0];
	queueParam.regions[0].base = capture->bufferRegion.physicalAddress;
	queueParam.regions[0].offset = capture->bufferRegion.virtualAddress;
	queueParam.regions[0].capacity = capture->bufferRegion.size;
	queueParam.regions[1].base = capture->scratchRegion.physicalAddress;
	queueParam.regions[1].offset = capture->scratchRegion.virtualAddress;
	queueParam.regions[1].capacity = layout->primarySize;
	queueParam.regions[2].base = queueParam.bufferBase + queueParam.storageSize;
	queueParam.regions[2].offset = (uint32_t)queueParam.storage + queueParam.storageSize;
	queueParam.regions[2].capacity = firstSize;
	queueParam.regions[3].base = capture->scratchRegion.physicalAddress + layout->primarySize;
	queueParam.regions[3].offset = capture->scratchRegion.virtualAddress + layout->primarySize;
	queueParam.regions[3].capacity = layout->secondarySize;
	queueParam.regions[4].base = queueParam.regions[2].base + firstSize;
	queueParam.regions[4].offset = queueParam.regions[2].offset + firstSize;
	queueParam.regions[4].capacity = secondSize;
	queueParam.regions[5].base = queueParam.regions[4].base + secondSize;
	queueParam.regions[5].offset = queueParam.regions[4].offset + secondSize;
	queueParam.regions[5].capacity = 0x1000;
	context = initializeVeneziaVipCaptureContextLocked(&g_veneziaVipState, &contextParam, 1, 0);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(63, 3, contextParam.commandValues[0], contextParam.commandValues[1],
			contextParam.physicalBase, contextParam.context, contextParam.size, 1, context);
	setVeneziaVipCaptureContext(capture, &g_veneziaVipState, context);
	if (!context || prepareVeneziaVipProcessorContext(processor, &g_veneziaVipState, storage) < 0)
		goto invalid;
	status = initializeVeneziaVipCaptureQueuesLocked(&g_veneziaVipState, context, &queueParam);
	if (g_veneziaDebugger.log) {
		g_veneziaDebugger.log(64, 9, context, queueParam.bufferBase, queueParam.storage,
			queueParam.storageSize, queueParam.captureAddress, queueParam.captureParameter,
			queueParam.captureOption, queueParam.physicalBase, status);
		g_veneziaDebugger.log(89, 3, queueParam.regions[0].base, queueParam.regions[0].offset, queueParam.regions[0].capacity);
		g_veneziaDebugger.log(90, 3, queueParam.regions[1].base, queueParam.regions[1].offset, queueParam.regions[1].capacity);
		g_veneziaDebugger.log(91, 3, queueParam.regions[2].base, queueParam.regions[2].offset, queueParam.regions[2].capacity);
		g_veneziaDebugger.log(92, 3, queueParam.regions[3].base, queueParam.regions[3].offset, queueParam.regions[3].capacity);
		g_veneziaDebugger.log(93, 3, queueParam.regions[4].base, queueParam.regions[4].offset, queueParam.regions[4].capacity);
		g_veneziaDebugger.log(94, 3, queueParam.regions[5].base, queueParam.regions[5].offset, queueParam.regions[5].capacity);
	}
	if (status)
		goto invalid;
	if (storage->status[2]) {
		length = (storage->status[2] + 7) & ~7u;
		virtualAddress = queueParam.regions[5].offset + queueParam.regions[5].capacity;
		physical = queueParam.regions[5].base + queueParam.regions[5].capacity;
		capture->imageHeader->captureBase = virtualAddress;
		capture->imageHeader->captureNext = virtualAddress + length + 80;
		capture->imageHeader->captureLength = length;
		capture->captureMemory.virtualAddress = virtualAddress + 2 * (length + 80);
		capture->captureMemory.physicalAddress = physical + 2 * (length + 80);
		capture->captureMemory.size = 39 * length;
		if (configureVeneziaVipCaptureMemoryLocked(&g_veneziaVipState, context,
			length, &capture->captureMemory))
			goto invalid;
	}
	if (configureVeneziaVipProcessorQueues(processor, &g_veneziaVipState,
		storage, capture->flags & 0x1000000) < 0)
		goto invalid;
	ret = startVeneziaVipProcessorContext(processor, &g_veneziaVipState, storage);
	if (ret < 0)
		goto invalid;
	prepareVeneziaVipCapture(&g_veneziaVipState, storage);
	lockVeneziaVipProcess(g_veneziaVipState.standardBufferLock);
	physical = capture->bufferRegion.virtualAddress;
	capture->bufferEnd = physical + capture->bufferRegion.size;
	capture->ringReadAddress = physical;
	capture->ringWriteAddress = physical;
	capture->ringCommitAddress = physical;
	capture->ringPendingAddress = physical;
	capture->ringEndAddress = capture->bufferEnd;
	capture->unidentifiedRingPendingCount = 0;
	capture->unidentifiedRingResult = 0;
	capture->ringSize = getVeneziaVipRingSize(capture->bufferRegion.size);
	capture->unidentifiedCaptureCounter = 0;
	capture->unidentifiedCaptureStatus = 0;
	capture->ringMode = 1;
	unlockVeneziaVipProcess(g_veneziaVipState.standardBufferLock);
	capture->completedCount = 0;
	goto out;
invalid:
	ret = -2141060860;
out:
	endVeneziaVipCaptureSetup(processor, &g_veneziaVipState, storage);
	return ret;
}

extern int getVeneziaVipCaptureContext(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, VeneziaVipCommandContext **context);
extern int releaseVeneziaVipAuxiliaryRecordLocked(VeneziaVipState *state,
	VeneziaVipCommandContext *context, const VeneziaVipCommandAuxiliaryRecord *record);
extern int notifyVeneziaVipProcessor(VeneziaVipState *state, int processor,
	unsigned int channel, uint32_t events);

/* name unknown: release every completed auxiliary result and wake its processor. */
int releaseVeneziaVipCaptureResults(unsigned int processor, VeneziaVipProcessStorage *storage)
{
	VeneziaDebugProcessCapture *capture = &storage->captures[processor];
	VeneziaVipCommandContext *context;
	unsigned int index;
	int ret = 0, status;

	getVeneziaVipCaptureContext(capture, &g_veneziaVipState, &context);
	if (capture->completedCount) {
		index = 0;
		do {
			lockVeneziaVipProcess(g_veneziaVipState.commonProcessorLock);
			status = releaseVeneziaVipAuxiliaryRecordLocked(&g_veneziaVipState,
				context, capture->auxiliaryRecord);
			if (g_veneziaDebugger.log)
				g_veneziaDebugger.log(65, 3, context, capture->auxiliaryRecord, status);
			if (status)
				ret = -2141060856;
			if (capture->queuedAuxiliaryCount)
				--capture->queuedAuxiliaryCount;
			++index;
			unlockVeneziaVipProcess(g_veneziaVipState.commonProcessorLock);
			notifyVeneziaVipProcessor(&g_veneziaVipState, processor, 0, 0x1000);
		} while (index < capture->completedCount);
	}
	capture->completedCount = 0;
	return ret;
}

typedef struct VeneziaVipCaptureSnapshot {
	VeneziaDebugSnapshotOutput snapshot;
	uint32_t captureAddresses[2];
	SceSize captureLength;
} VeneziaVipCaptureSnapshot;

#if FW_VERSION >= 0x04000000
#define VIP_RESULT_SYMBOL(debug, release) debug
#else
#define VIP_RESULT_SYMBOL(debug, release) release
#endif
extern int getVeneziaVipAuxiliaryRecordLocked(VeneziaVipState *state,
	VeneziaVipCommandContext *context, const VeneziaVipCommandAuxiliaryRecord **record);
extern int getVeneziaVipResultMetadataLocked(VeneziaVipState *state,
	VeneziaVipCommandContext *context, const VeneziaVipCommandAuxiliaryRecord *record,
	uint32_t *imageAddress, uint32_t *captureAddress,
	VeneziaVipCommandResultMetadata *metadata, uint32_t *value);
extern int getVeneziaVipAuxiliaryStatusLocked(VeneziaVipState *state,
	const VeneziaVipCommandAuxiliaryRecord *record, VeneziaVipCommandAuxiliaryStatus *status);
extern int copyVeneziaVipCommonDebugDataLocked(VeneziaVipState *state,
	VeneziaVipCommandContext *context, const VeneziaVipCommandAuxiliaryRecord *record,
	VeneziaVipCommonDebugData *data);
extern int copyVeneziaVipDebugDataLocked(VeneziaVipState *state,
	VeneziaVipCommandContext *context, const VeneziaVipCommandAuxiliaryRecord *record,
	unsigned int processor, uint32_t *data);
extern int copyVeneziaVipChannelDataLocked(VeneziaVipState *state,
	VeneziaVipCommandContext *context, const VeneziaVipCommandAuxiliaryRecord *record,
	unsigned int processor, VeneziaDebugChannelRecord *data);
extern int copyVeneziaVipSecondaryDataLocked(VeneziaVipState *state,
	VeneziaVipCommandContext *context, const VeneziaVipCommandAuxiliaryRecord *record,
	unsigned int processor, uint32_t *data);
extern int copyVeneziaVipStatusDataLocked(VeneziaVipState *state,
	VeneziaVipCommandContext *context, const VeneziaVipCommandAuxiliaryRecord *record,
	unsigned int processor, uint32_t *data);
extern int copyVeneziaVipControlDataLocked(VeneziaVipState *state,
	VeneziaVipCommandContext *context, const VeneziaVipCommandAuxiliaryRecord *record,
	unsigned int processor, uint32_t *data);
extern int copyVeneziaVipCaptureMetadataLocked(VeneziaVipState *state,
	VeneziaVipCommandContext *context, const VeneziaVipCommandAuxiliaryRecord *record,
	unsigned int processor, uint32_t destination, SceSize length);
#undef VIP_RESULT_SYMBOL

/* These expansions are inlined in the original result collector. */
static __attribute__((always_inline)) inline void decodeVeneziaVipChannelColumn(
	VeneziaDebugChannelState *state, const VeneziaDebugChannelRecord *record, unsigned int column)
{
	uint32_t first, second;

	first = record->records[column].control[0];
	second = record->records[column].control[1];
	state->control[0][column] = first >> 30;
	state->control[1][column] = !!(first & 0x20000000);
	state->control[2][column] = (first >> 24) & 31;
	state->control[3][column] = !!(first & 0x00800000);
	state->control[4][column] = !!(first & 0x00400000);
	state->control[5][column] = !!(first & 0x00200000);
	state->control[6][column] = first >> 13;
	state->control[7][column] = !!(first & 0x1000);
	state->control[8][column] = second >> 26;
	state->control[9][column] = !!(first & 0x800);
	state->control[10][column] = (second >> 20) & 63;
	state->control[11][column] = !!(first & 0x400);
	state->control[12][column] = (second >> 15) & 31;
	state->parameters[column] = record->records[column].parameter;
}

static __attribute__((always_inline)) inline void decodeVeneziaVipChannelState(
	VeneziaDebugChannelState *state, const VeneziaDebugChannelRecord *record)
{
	state->header[0] = (int8_t)record->header[0];
	state->header[1] = (int8_t)record->header[1];
	state->status[0] = record->flags >> 28;
	state->status[1] = !!(record->flags & 0x08000000);
	state->status[2] = !!(record->flags & 0x04000000);
	state->status[3] = !!(record->flags & 0x02000000);
	decodeVeneziaVipChannelColumn(state, record, 0);
	decodeVeneziaVipChannelColumn(state, record, 1);
	decodeVeneziaVipChannelColumn(state, record, 2);
}

static __attribute__((always_inline)) inline void decodeVeneziaVipCommonRegisters(
	VeneziaDebugRegisterInfo *info, const VeneziaVipCommonDebugData *data)
{
	info->control[14] = !!(data->executionControl & 0x2000);
	info->control[15] = !!(data->executionControl & 0x1000);
	info->programCounter = data->programCounter;
	info->stackPointer = data->stackPointer;
	info->control[0] = !!(data->executionControl & 0x80000000);
	info->control[1] = data->executionControl >> 23;
	info->executionStatus.high = data->executionStatus >> 16;
	info->executionStatus.low = data->executionStatus;
	info->control[7] = !!(data->pipelineControl & 0x04000000);
	info->control[8] = data->pipelineControl >> 18;
	info->control[9] = data->pipelineControl >> 10;
	info->control[10] = data->pipelineControl >> 2;
	info->control[4] = !!(data->pipelineControl & 0x80000000);
	info->control[6] = !!(data->pipelineControl & 0x08000000);
	info->executionFlags[1] = !!(data->executionControl & 0x400);
}

/* name unknown: decode queued processor-one results into host snapshots. */
int collectVeneziaVipCaptureResults(unsigned int processor, VeneziaVipProcessStorage *storage,
	const VeneziaDebugSnapshotList *list, unsigned int index)
{
	VeneziaDebugProcessCapture *capture = &storage->captures[processor];
	VeneziaVipCommandContext *context;
	VeneziaVipCaptureImageHeader *image;
	VeneziaVipCaptureSnapshot *output;
	uint32_t flags, valid = 0;
	int ret;

	getVeneziaVipCaptureContext(capture, &g_veneziaVipState, &context);
	if (!context || index >= list->count)
		return 0;
	for (;;) {
		ret = getVeneziaVipAuxiliaryRecordLocked(&g_veneziaVipState, context,
			&capture->auxiliaryRecord);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(68, 3, context, &capture->auxiliaryRecord, ret);
		if (ret)
			return ret == 3 ? 0 : -2141060856;
		flags = 0;
		ret = getVeneziaVipResultMetadataLocked(&g_veneziaVipState, context,
			capture->auxiliaryRecord, &capture->capturePhysicalAddress,
			&capture->captureVirtualAddress, &capture->imageHeader->metadata, &flags);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(69, 7, context, capture->auxiliaryRecord,
				&capture->capturePhysicalAddress, &capture->captureVirtualAddress,
				capture->imageHeader, flags, ret);
		capture->captureVirtualAddress = capture->imagePhysicalAddress;
		if (ret)
			return -2141060856;
		if (flags & 2) {
			if (!copyVeneziaVipCommonDebugDataLocked(&g_veneziaVipState, context,
				capture->auxiliaryRecord, &capture->imageHeader->commonDebugData)) {
				valid |= 1;
				capture->debugResultFlags |= 2;
			}
		} else if (capture->debugResultFlags & 2) {
			valid |= 1;
		}
		if ((flags & 4) && !copyVeneziaVipDebugDataLocked(&g_veneziaVipState,
			context, capture->auxiliaryRecord, 0, capture->imageHeader->debugData[0])) {
			valid |= 2;
			if (flags & 8)
				copyVeneziaVipDebugDataLocked(&g_veneziaVipState, context,
					capture->auxiliaryRecord, 1, capture->imageHeader->debugData[1]);
		}
		if ((flags & 0x10) && !copyVeneziaVipChannelDataLocked(&g_veneziaVipState,
			context, capture->auxiliaryRecord, 0, &capture->imageHeader->channels[0])) {
			valid |= 4;
			if ((flags & 0x20) && !copyVeneziaVipChannelDataLocked(&g_veneziaVipState,
				context, capture->auxiliaryRecord, 1, &capture->imageHeader->channels[1]))
				valid |= 0x20;
		}
		if ((flags & 0x40) && !copyVeneziaVipSecondaryDataLocked(&g_veneziaVipState,
			context, capture->auxiliaryRecord, 0, capture->imageHeader->secondaryData[0])) {
			valid |= 0x10;
			if ((flags & 0x80) && !copyVeneziaVipSecondaryDataLocked(&g_veneziaVipState,
				context, capture->auxiliaryRecord, 1, capture->imageHeader->secondaryData[1]))
				valid |= 0x40;
		}
		if ((flags & 0x100) && !copyVeneziaVipStatusDataLocked(&g_veneziaVipState,
			context, capture->auxiliaryRecord, 0, capture->imageHeader->statusData[0])) {
			valid |= 8;
			if (flags & 0x200)
				copyVeneziaVipStatusDataLocked(&g_veneziaVipState, context,
					capture->auxiliaryRecord, 1, capture->imageHeader->statusData[1]);
		}
		if ((flags & 0x400) && !copyVeneziaVipControlDataLocked(&g_veneziaVipState,
			context, capture->auxiliaryRecord, 0, capture->imageHeader->controlData[0]) &&
		    (flags & 0x800))
			copyVeneziaVipControlDataLocked(&g_veneziaVipState, context,
				capture->auxiliaryRecord, 1, capture->imageHeader->controlData[1]);
		if (flags & 0x1000) {
			ret = copyVeneziaVipCaptureMetadataLocked(&g_veneziaVipState, context,
				capture->auxiliaryRecord, 0, capture->imageHeader->captureBase,
				capture->imageHeader->captureLength);
			if (!ret || (unsigned int)(ret - 7) <= 1) {
				valid |= 0x1000;
				if (flags & 0x2000) {
					ret = copyVeneziaVipCaptureMetadataLocked(&g_veneziaVipState, context,
						capture->auxiliaryRecord, 1, capture->imageHeader->captureNext,
						capture->imageHeader->captureLength);
					if (!ret || (unsigned int)(ret - 7) <= 1)
						valid |= 0x2000;
				}
			}
		}
		ret = getVeneziaVipAuxiliaryStatusLocked(&g_veneziaVipState,
			capture->auxiliaryRecord, &capture->imageHeader->auxiliaryStatus);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(70, 4, capture->auxiliaryRecord,
				capture->imageHeader->auxiliaryStatus.value,
				capture->imageHeader->auxiliaryStatus.controlFlag, ret);
		if (!ret)
			valid |= 0x80;
		output = (VeneziaVipCaptureSnapshot *)list->snapshots[index];
		output->snapshot.event = 0;
		output->snapshot.virtualAddress = capture->captureVirtualAddress;
		output->snapshot.physicalAddress = capture->capturePhysicalAddress;
		output->snapshot.valid = valid;
		image = capture->imageHeader;
		if (image->metadata.contextControl & 0x400)
			output->snapshot.event = 1;
		image = capture->imageHeader;
		output->snapshot.context.instructionWidth = (image->metadata.contextControl & 0x400000) ? 3 : 2;
		output->snapshot.context.flagA = !!(image->metadata.contextControl & 0x200000);
		output->snapshot.context.executionMode = (image->metadata.contextControl >> 17) & 7;
		output->snapshot.context.instruction.high = image->metadata.instructionHigh;
		output->snapshot.context.instruction.low = image->metadata.instructionLow;
		output->snapshot.context.operand.high = image->metadata.operandHigh;
		output->snapshot.context.operand.low = image->metadata.operandLow;
		output->snapshot.context.address.high = image->metadata.addressHigh;
		output->snapshot.context.address.low = image->metadata.addressLow;
		if (valid & 1)
			decodeVeneziaVipCommonRegisters(&output->snapshot.data.registers,
				&capture->imageHeader->commonDebugData);
		if (valid & 4)
			decodeVeneziaVipChannelState(&output->snapshot.data.channels[0],
				&capture->imageHeader->channels[0]);
		if (valid & 0x20)
			decodeVeneziaVipChannelState(&output->snapshot.data.channels[1],
				&capture->imageHeader->channels[1]);
		if (valid & 0x1000) {
			output->captureAddresses[0] = capture->imageHeader->captureBase;
			output->captureLength = capture->imageHeader->captureLength;
		}
		if (valid & 0x2000)
			output->captureAddresses[1] = capture->imageHeader->captureNext;
		if (valid & 0x80) {
			output->snapshot.data.trace.marked = capture->imageHeader->auxiliaryStatus.controlFlag;
			output->snapshot.data.trace.value = capture->imageHeader->auxiliaryStatus.value;
		} else {
			output->snapshot.data.trace.value = -1;
			output->snapshot.data.trace.marked = -1;
		}
		if (capture->imageHeader->metadata.contextControl & 0x200) {
			++capture->completedCount;
			return -2141060852;
		}
		++index;
		++capture->completedCount;
		if (index >= list->count)
			return 0;
	}
}

extern int initializeVeneziaVipCommandsLocked(VeneziaVipState *state,
	const VeneziaVipCommandCallbacks *callbacks, const VeneziaVipMemoryRegion *memory);
extern int getVeneziaVipReportedValuesLocked(VeneziaVipState *state,
	uint32_t *main, uint32_t *auxiliary);
extern int calculateVeneziaVipTiledImageSizeLocked(VeneziaVipState *state,
	unsigned int width, unsigned int height, unsigned int imageCount, SceSize *size);

int finishVeneziaVipStartup(VeneziaVipState *state)
{
	VeneziaVipCommandCallbacks callbacks;
	VeneziaVipMemoryRegion memory;
	uint32_t main, auxiliary;
	int ret;

	memory.virtualAddress = state->debuggerMemory.virtualAddress + 0x80000;
	memory.physicalAddress = state->debuggerMemory.physicalAddress + 0x80000;
	memory.size = 0x30000;
	/* the command code passes a third word on one path; the write ignores it */
	callbacks.write = (uint32_t (*)(uint32_t, uint32_t, ...))writeVeneziaVipPhysicalWord;
	callbacks.read = readVeneziaVipPhysicalWord;
	ret = initializeVeneziaVipCommandsLocked(state, &callbacks, &memory);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(54, 4, memory.virtualAddress, memory.physicalAddress,
			memory.size, ret);
	if (ret)
		return -2141060860;
	ret = getVeneziaVipReportedValuesLocked(state, &main, &auxiliary);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(56, 3, main, auxiliary, ret);
	return ret ? -2141060860 : 0;
}

/* name unknown: validate image dimensions and reserve two extra tiled images. */
int getVeneziaVipImageStorageSize(VeneziaVipState *state,
	const VeneziaDebugImageFormat *format, SceSize *size)
{
	unsigned int width, height, count;
	int ret;

	if (!format || !size)
		return -2141060862;
	width = format->width;
	if (width - 64 > 0x740)
		return -2141060863;
	height = format->height;
	if (height - 64 > 0x400)
		return -2141060863;
	count = format->imageCount;
	if (count > 16 || count * 3 * height * width > 0x1800001)
		return -2141060863;
	count += 2;
	ret = calculateVeneziaVipTiledImageSizeLocked(state, width, height, count, size);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(57, 5, width, height, count, *size, ret);
	if (ret)
		return -2141060863;
	if (format->width == 1920 && format->height == 1088 && format->imageCount > 3)
		*size = 0x2000000;
	return ret;
}

extern int getVeneziaVipCaptureCallbacks(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, VeneziaDebugCallback **callbacks);

static __attribute__((always_inline)) inline void clearVeneziaVipCaptureCallback(
	VeneziaDebugCallback *callback, SceUID uid, SceUID pid)
{
	if ((!uid || callback->uid == uid) && callback->pid == pid) {
		callback->pid = 0;
		callback->uid = 0;
		callback->pendingEvents = 0;
		callback->eventMask = 0;
	}
}

/* name unknown: remove one process's selected callback from a capture channel. */
int clearVeneziaVipCaptureCallbacks(unsigned int processor, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage, SceUID uid, SceUID pid)
{
	VeneziaDebugCallback *callbacks;
	VeneziaDebugProcessCapture *capture = &storage->captures[processor - 1];

	lockVeneziaVipProcess(state->commonProcessorLock);
	getVeneziaVipCaptureCallbacks(capture, state, &callbacks);
	if (!uid) {
		clearVeneziaVipCaptureCallback(&callbacks[0], 0, pid);
		clearVeneziaVipCaptureCallback(&callbacks[1], 0, pid);
		clearVeneziaVipCaptureCallback(&callbacks[2], 0, pid);
		clearVeneziaVipCaptureCallback(&callbacks[3], 0, pid);
		clearVeneziaVipCaptureCallback(&callbacks[4], 0, pid);
		clearVeneziaVipCaptureCallback(&callbacks[5], 0, pid);
	} else {
		clearVeneziaVipCaptureCallback(&callbacks[0], uid, pid);
		clearVeneziaVipCaptureCallback(&callbacks[1], uid, pid);
		clearVeneziaVipCaptureCallback(&callbacks[2], uid, pid);
		clearVeneziaVipCaptureCallback(&callbacks[3], uid, pid);
		clearVeneziaVipCaptureCallback(&callbacks[4], uid, pid);
		clearVeneziaVipCaptureCallback(&callbacks[5], uid, pid);
	}
	unlockVeneziaVipProcess(state->commonProcessorLock);
	return 0;
}

/* name unknown: register a callback for one capture processor. */
int registerVeneziaVipCaptureCallback(unsigned int processor, SceUID uid,
	uint32_t eventMask)
{
	VeneziaVipProcessStorage *storage;
	VeneziaDebugCallback *callbacks, *callback;
	unsigned int index = processor - 1;
	int ret, kind;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	if (eventMask & 0xffffc4f9)
		return -2141060863;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	if (index >= storage->status[0] || storage->processors[index] == -1) {
		ret = -2141060863;
		goto unlockProcess;
	}
	ret = scePUIDtoGUID(0, uid);
	if (ret < 0)
		goto unlockProcess;
	kind = sceKernelGetThreadmgrUIDClass(ret);
	if (kind < 0) {
		ret = kind;
		goto unlockProcess;
	}
	if (kind != 8) {
		ret = -2141060863;
		goto unlockProcess;
	}
	getVeneziaVipCaptureCallbacks(&storage->captures[index],
		&g_veneziaVipState, &callbacks);
	lockVeneziaVipProcess(g_veneziaVipState.commonProcessorLock);
	if (!callbacks[0].uid)
		callback = &callbacks[0];
	else if (!callbacks[1].uid)
		callback = &callbacks[1];
	else if (!callbacks[2].uid)
		callback = &callbacks[2];
	else if (!callbacks[3].uid)
		callback = &callbacks[3];
	else if (!callbacks[4].uid)
		callback = &callbacks[4];
	else if (!callbacks[5].uid)
		callback = &callbacks[5];
	else {
		ret = -2141060858;
		goto unlockCallbacks;
	}
	callback->pid = sceKernelGetProcessId();
	callback->uid = ret;
	callback->pendingEvents = 0;
	callback->eventMask = eventMask;
	ret = 0;
unlockCallbacks:
	unlockVeneziaVipProcess(g_veneziaVipState.commonProcessorLock);
unlockProcess:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

/* name unknown: remove a selected capture callback owned by the current process. */
int unregisterVeneziaVipCaptureCallback(unsigned int processor, SceUID uid)
{
	VeneziaVipProcessStorage *storage;
	unsigned int index = processor - 1;
	int ret, kind;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	if (index >= storage->status[0] || storage->processors[index] == -1) {
		ret = -2141060863;
		goto unlock;
	}
	if (uid) {
		uid = scePUIDtoGUID(0, uid);
		if (uid < 0) {
			ret = uid;
			goto unlock;
		}
		kind = sceKernelGetThreadmgrUIDClass(uid);
		if (kind < 0) {
			ret = kind;
			goto unlock;
		}
		if (kind != 8) {
			ret = -2141060863;
			goto unlock;
		}
	}
	ret = removeVeneziaVipCallbacks(processor, &g_veneziaVipState, storage,
		uid, sceKernelGetProcessId());
unlock:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

/* name unknown: query capture storage while the process has capacity. */
int queryVeneziaVipCaptureSize(const VeneziaDebugImageFormat *geometry,
	uint32_t *size)
{
	VeneziaVipProcessStorage *storage;
	uint32_t count;
	int ret;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	count = storage->status[0];
	if (count && count <= storage->status[1])
		ret = -2141060858;
	else
		ret = getVeneziaVipCaptureSize(1, &g_veneziaVipState, geometry, size);
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

/* name unknown: query storage for all four capture processors. */
int getVeneziaVipMultiCaptureStorageSize(const VeneziaDebugImageGeometry *geometry,
	SceSize *size)
{
	VeneziaVipProcessStorage *storage;
	SceSize image __attribute__((aligned(8)));
	SceSize buffer __attribute__((aligned(8)));
	SceSize scratch;
	int ret;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	if (storage->status[0])
		ret = -2141060858;
	else
		ret = calculateVeneziaVipCaptureStorage(4, storage->status[2], geometry,
			size, &image, &buffer, &scratch);
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

#if FW_VERSION >= 0x04000000
#define VIP_CAPTURE_SYMBOL(debug, release) debug
#else
#define VIP_CAPTURE_SYMBOL(debug, release) release
#endif

extern int setVeneziaVipCaptureControl(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, uint32_t control);
extern int clearVeneziaVipCaptureStatus(VeneziaVipState *state,
	VeneziaDebugProcessCapture *capture);

/* name unknown: reserve a free processor and initialize multi-capture storage. */
int registerVeneziaVipMultiCapture(unsigned int *processor,
	const VeneziaVipCaptureParam *param)
{
	VeneziaVipProcessStorage *storage;
	VeneziaDebugProcessCapture *capture;
	VeneziaVipMemoryRegion region;
	SceSize total __attribute__((aligned(8)));
	SceSize image __attribute__((aligned(8)));
	SceSize buffer __attribute__((aligned(8)));
	SceSize scratch __attribute__((aligned(8)));
	char name[32];
	unsigned int index = 0, count;
	uint32_t state, secondaryFlags, primaryFlags, combinedFlags;
	int ret, initialize;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	if (!param || !processor || !param->imageVirtualAddress || !param->imageFormat)
		return -2141060862;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	count = storage->status[0];
	initialize = !count;
	if (count) {
		if (count <= storage->status[1])
			goto full;
		if (storage->processors[0] == -1)
			index = 0;
		else if (count > 1 && storage->processors[1] == -1)
			index = 1;
		else if (count > 2 && storage->processors[2] == -1)
			index = 2;
		else if (count > 3 && storage->processors[3] == -1)
			index = 3;
		else
			goto full;
	}
	ret = isVeneziaVipImageInMainMemory(param->imageVirtualAddress, param->imageSize);
	if (ret < 0)
		goto invalid;
	capture = &storage->captures[index];
	capture->imageMappingMode = !!ret;
	if (initialize) {
		if (!param->storageVirtualAddress)
			goto invalid;
		ret = calculateVeneziaVipCaptureStorage(4, storage->status[2], param->geometry,
			&total, &image, &buffer, &scratch);
		if (ret < 0)
			goto unlock;
		if (param->storageSize < total) {
			ret = -2141060863;
			goto unlock;
		}
		if (sceKernelVAtoPA((const void *)param->storageVirtualAddress,
			&region.physicalAddress) < 0)
			goto invalid;
		region.virtualAddress = param->storageVirtualAddress;
		region.size = param->storageSize;
		storage->captureMemory.control = 1;
		setVeneziaVipMemoryRegion(&storage->captureMemory.scratch, &region, 0, image);
		setVeneziaVipMemoryRegion(&storage->captureMemory.buffer, &region, image, buffer);
		setVeneziaVipMemoryRegion(&storage->captureMemory.snapshot, &region,
			image + buffer, scratch);
		storage->memoryLayout = &storage->captureMemory;
		ret = distributeVeneziaVipCaptureMemory(storage, param->geometry->processorCount,
			&storage->captureMemory);
		if (ret < 0)
			goto unlock;
	}
	capture->imageHeader = (VeneziaVipCaptureImageHeader *)capture->snapshotRegion.virtualAddress;
	ret = setVeneziaVipCaptureImage(capture, param);
	if (ret < 0)
		goto unlock;
	if (initialize) {
		ret = beginVeneziaVipCaptureConfiguration(&g_veneziaVipState, storage);
		if (ret < 0)
			goto unlock;
	}
	capture->captureParam = *param;
	capture->imageFormat = *param->imageFormat;
	capture->captureParam.imageFormat = &capture->imageFormat;
	if (initialize) {
		storage->status[1] = 1;
		storage->state |= 0x100;
		storage->status[0] = param->geometry->processorCount;
	} else {
		storage->status[1]++;
	}
	snprintf(name, sizeof(name), "SceVipFW2_%08x_%02ld", sceKernelGetProcessId(), index + 1);
	ret = sceKernelCreateEventFlag(name, 0x1000, 0, NULL);
	if (ret >= 0) {
		capture->eventFlag = ret;
		storage->processors[index] = index;
		*processor = index + 1;
		state = storage->state;
		capture->flags = 0x10f;
		if (state & 0x20000) {
			secondaryFlags = 0x102010f;
			primaryFlags = 0x60000 | 0x10f;
			combinedFlags = 0x1020000 | 0x40000 | 0x10f;
			capture->flags = 0x20000 | 0x10f;
		} else {
			secondaryFlags = 0x1000000 | 0x10f;
			primaryFlags = 0x40000 | 0x10f;
			combinedFlags = 0x1040000 | 0x10f;
		}
		if (state & 0x40000) {
			secondaryFlags = combinedFlags;
			capture->flags = primaryFlags;
		}
		if (state & 0x1000000)
			capture->flags = secondaryFlags;
		ret = beginVeneziaVipCaptureSetup(index, &g_veneziaVipState, storage);
		if (ret >= 0) {
			setVeneziaVipCaptureContext(capture, &g_veneziaVipState, NULL);
			setVeneziaVipCaptureControl(capture, &g_veneziaVipState, 0);
			endVeneziaVipCaptureSetup(index, &g_veneziaVipState, storage);
			clearVeneziaVipCaptureStatus(&g_veneziaVipState, capture);
			capture->completedCount = 0;
			goto unlock;
		}
		ret = -2141060856;
		sceKernelDeleteEventFlag(capture->eventFlag);
		storage->processors[index] = -1;
	}
	if (initialize)
		endVeneziaVipCaptureConfiguration(&g_veneziaVipState, storage);
	goto unlock;
full:
	ret = -2141060858;
	goto unlock;
invalid:
	ret = -2141060862;
unlock:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

/* name unknown: detach one capture processor from the selected process. */
int unregisterVeneziaVipMultiCapture(unsigned int processor, int wait, SceUID pid)
{
	VeneziaVipProcessStorage *storage;
	VeneziaDebugProcessCapture *capture;
	unsigned int index = processor - 1;
	int ret = 0;

	if (wait) {
		ret = waitVeneziaVipProcessReady();
		if (ret < 0)
			return ret;
	}
	storage = _sceVipGetVipProcessResource(pid);
	if (!storage)
		return -2141060856;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	if (!storage->status[0])
		goto absent;
	if (index >= storage->status[0]) {
		ret = -2141060863;
		goto unlock;
	}
	if (storage->processors[index] == -1)
		goto absent;
	removeVeneziaVipCallbacks(processor, &g_veneziaVipState, storage, 0, pid);
	stopVeneziaVipCapture(index, &g_veneziaVipState, storage);
	if (storage->status[1] <= 1) {
		releaseVeneziaVipCaptureMemory(storage);
		storage->memoryLayout = NULL;
		endVeneziaVipCaptureConfiguration(&g_veneziaVipState, storage);
		storage->status[0] = 0;
		storage->status[2] = 0;
		storage->state &= ~0x100;
	}
	capture = &storage->captures[index];
	capture->flags &= ~0x100;
	sceKernelDeleteEventFlag(capture->eventFlag);
	storage->processors[index] = -1;
	storage->status[1]--;
	goto unlock;
absent:
	ret = -2141060859;
unlock:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

/* name unknown: select the multi-capture implementation's process teardown. */
int unregisterVeneziaVipMultiCaptureForProcess(unsigned int processor, int wait,
	SceUID pid)
{
	return dispatchVeneziaVipCaptureRemoval(1, processor, wait, pid);
}

extern int getVeneziaVipCaptureInputCapacity(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, VeneziaVipCommandContext *context, SceSize *available);

/* name unknown: report a capture channel's usable input capacity. */
int getVeneziaVipMultiTraceAvailable(unsigned int processor)
{
	VeneziaVipProcessStorage *storage;
	VeneziaDebugProcessCapture *capture;
	VeneziaVipCommandContext *context;
	SceSize available;
	unsigned int index = processor - 1, capacity;
	int ret;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	if (index >= storage->status[0] || storage->processors[index] == -1) {
		ret = -2141060863;
		goto unlock;
	}
	capture = &storage->captures[index];
	getVeneziaVipCaptureContext(capture, &g_veneziaVipState, &context);
	if (!getVeneziaVipCaptureInputCapacity(capture, &g_veneziaVipState, context, &available)) {
		capacity = available;
	} else if (capture->flags & 0x107) {
		capacity = capture->bufferRegion.size;
	} else {
		ret = -2141060856;
		goto unlock;
	}
	ret = capacity <= 1024 ? 0 : capacity - 1024;
unlock:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

/* name unknown: validate and set a channel's binary capture control. */
int configureVeneziaVipCaptureControl(unsigned int processor, unsigned int control)
{
	VeneziaVipProcessStorage *storage;
	unsigned int index = processor - 1;
	int ret;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	if (control > 1)
		return -2141060863;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	if (index >= storage->status[0] || storage->processors[index] == -1) {
		unlockVeneziaVipProcess(g_veneziaVipState.processLock);
		return -2141060863;
	}
	setVeneziaVipCaptureControl(&storage->captures[index], &g_veneziaVipState, control);
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

extern int recoverVeneziaVipProcessors(VeneziaVipState *state,
	VeneziaVipProcessStorage *storage);
extern int commitVeneziaVipCaptureInput(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, VeneziaVipCommandContext *context);
extern int requestVeneziaVipCaptureProcessing(unsigned int processor,
	VeneziaVipState *state, unsigned int mode);

/* name unknown: submit input and collect snapshots for one multi-capture channel. */
int processVeneziaVipMultiCapture(unsigned int processor, const VeneziaVipVideoInput *input,
	const VeneziaDebugSnapshotList *list, uint32_t *completed)
{
	VeneziaVipProcessStorage *storage;
	VeneziaDebugProcessCapture *capture;
	VeneziaVipCommandContext *context;
	unsigned int index = processor - 1, captureProcessor;
	SceSize inputSize = 0, available;
	SceUInt timeout = 8000, event;
	int ret, capacity;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	if (!completed)
		return -2141060863;
	*completed = 0;
	if (!list)
		return -2141060863;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	if (index >= storage->status[0] ||
	    (captureProcessor = storage->processors[index]) == (unsigned int)-1) {
		ret = -2141060863;
		goto unlock;
	}
	if (input && !((input->flags & 1) && !input->additionalData[0].data) &&
	    !((input->flags & 2) && !input->additionalData[1].data) && input->payload.data) {
		ret = calculateVeneziaVipVideoInputSize(input);
		if (ret < 0)
			goto unlock;
		captureProcessor = storage->processors[index];
		if (captureProcessor == (unsigned int)-1) {
			ret = -2141060859;
			goto unlock;
		}
		inputSize = ret;
	}
	event = 0;
	sceKernelPollEventFlag(g_veneziaVipState.eventFlag, 0x2000000, 1, &event);
	if (event & 0x2000000)
		recoverVeneziaVipProcessors(&g_veneziaVipState, storage);
	capture = &storage->captures[index];
	if (capture->flags & 7) {
		capture->flags &= ~7;
		capture->debugResultFlags = 0;
		stopVeneziaVipCapture(captureProcessor, &g_veneziaVipState, storage);
		ret = setupVeneziaVipCapture(captureProcessor, storage,
			&capture->captureParam.imageSize, &capture->captureParam.imageFormat);
		if (ret < 0)
			goto unlock;
		capture->flags &= ~8;
	}
	ret = releaseVeneziaVipCaptureResults(index, storage);
	if (ret < 0)
		goto finished;
	ret = collectVeneziaVipCaptureResults(index, storage, list, capture->completedCount);
	if (ret < 0)
		goto collectionError;
	if (inputSize) {
		getVeneziaVipCaptureContext(capture, &g_veneziaVipState, &context);
		if (getVeneziaVipCaptureInputCapacity(capture, &g_veneziaVipState, context,
			&available)) {
			if (!(capture->flags & 0x107)) {
				ret = -2141060856;
				goto finished;
			}
			available = capture->bufferRegion.size;
		}
		if (available > 1024) {
			capacity = available - 1024;
			ret = capacity;
			if (capacity < 0) {
				if (available != (unsigned int)-2141059831)
					goto finished;
			} else if (inputSize <= (unsigned int)capacity) {
				ret = submitVeneziaVipVideoInput(index, &g_veneziaVipState, storage, input);
				if (ret < 0)
					goto finished;
				getVeneziaVipCaptureContext(capture, &g_veneziaVipState, &context);
				inputSize = 0;
				commitVeneziaVipCaptureInput(capture, &g_veneziaVipState, context);
				goto inputReady;
			}
		}
		inputSize = -2141060855;
	}
inputReady:
	if (capture->flags & 0x20000) {
		ret = requestVeneziaVipCaptureProcessing(index, &g_veneziaVipState, 0);
		if (ret < 0) {
			if (ret == -2141060856)
				ret = 0;
			goto finished;
		}
	} else {
		notifyVeneziaVipProcessor(&g_veneziaVipState, index, 0, 4096);
	}
	if (list->count && list->count <= capture->completedCount)
		timeout = 1;
	event = 0;
	ret = sceKernelWaitEventFlag(capture->eventFlag, 0x3f06, 5, &event, &timeout);
	if (ret < 0) {
		if (ret != -2147319803)
			goto finished;
		ret = 0;
	}
	if (event & 0x1000) {
		ret = -2141060851;
		capture->flags |= 7;
		goto finished;
	}
	if (event & 0x200) {
		ret = -2141060856;
		capture->flags |= 7;
		goto finished;
	}
	if (event & 0x2000) {
		ret = -2141060850;
		goto finished;
	}
	if (event & 0x100) {
		ret = -2141060853;
		goto finished;
	}
	if (event & 2) {
		ret = collectVeneziaVipCaptureResults(index, storage, list, capture->completedCount);
		if (ret < 0)
			goto collectionError;
	}
	if (inputSize)
		ret = inputSize;
	goto finished;
collectionError:
	if (ret == -2141060852)
		ret = -2141060853;
finished:
	*completed = capture->completedCount;
unlock:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

/* name unknown: submit multi-capture input using an optional timeout. */
int submitVeneziaVipMultiCapture(unsigned int processor, const VeneziaVipVideoInput *input,
	const SceUInt *timeoutValue)
{
	VeneziaDebugSnapshotList snapshots;
	const VeneziaDebugSnapshotList *list = &snapshots;
	VeneziaVipProcessStorage *storage;
	VeneziaDebugProcessCapture *capture;
	VeneziaVipCommandContext *context;
	unsigned int index = processor - 1, captureProcessor;
	SceSize inputSize = 0, available;
	SceUInt timeout, event;
	int ret, capacity;

	snapshots.count = 0;
	timeout = timeoutValue ? *timeoutValue : 8000;
	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	if (index >= storage->status[0] ||
	    (captureProcessor = storage->processors[index]) == (unsigned int)-1) {
		ret = -2141060863;
		goto unlock;
	}
	if (input && !((input->flags & 1) && !input->additionalData[0].data) &&
	    !((input->flags & 2) && !input->additionalData[1].data) && input->payload.data) {
		ret = calculateVeneziaVipVideoInputSize(input);
		if (ret < 0)
			goto unlock;
		captureProcessor = storage->processors[index];
		if (captureProcessor == (unsigned int)-1) {
			ret = -2141060859;
			goto unlock;
		}
		inputSize = ret;
	}
	event = 0;
	sceKernelPollEventFlag(g_veneziaVipState.eventFlag, 0x2000000, 1, &event);
	if (event & 0x2000000)
		recoverVeneziaVipProcessors(&g_veneziaVipState, storage);
	capture = &storage->captures[index];
	if (capture->flags & 7) {
		capture->flags &= ~7;
		capture->debugResultFlags = 0;
		stopVeneziaVipCapture(captureProcessor, &g_veneziaVipState, storage);
		ret = setupVeneziaVipCapture(captureProcessor, storage,
			&capture->captureParam.imageSize, &capture->captureParam.imageFormat);
		if (ret < 0)
			goto unlock;
		capture->flags &= ~8;
	}
	ret = releaseVeneziaVipCaptureResults(index, storage);
	if (ret < 0)
		goto finished;
	ret = collectVeneziaVipCaptureResults(index, storage, list, capture->completedCount);
	if (ret < 0)
		goto collectionError;
	if (inputSize) {
		getVeneziaVipCaptureContext(capture, &g_veneziaVipState, &context);
		if (getVeneziaVipCaptureInputCapacity(capture, &g_veneziaVipState, context,
			&available)) {
			if (!(capture->flags & 0x107)) {
				ret = -2141060856;
				goto finished;
			}
			available = capture->bufferRegion.size;
		}
		if (available > 1024) {
			capacity = available - 1024;
			ret = capacity;
			if (capacity < 0) {
				if (available != (unsigned int)-2141059831)
					goto finished;
			} else if (inputSize <= (unsigned int)capacity) {
				ret = submitVeneziaVipVideoInput(index, &g_veneziaVipState, storage, input);
				if (ret < 0)
					goto finished;
				getVeneziaVipCaptureContext(capture, &g_veneziaVipState, &context);
				inputSize = 0;
				commitVeneziaVipCaptureInput(capture, &g_veneziaVipState, context);
				goto inputReady;
			}
		}
		inputSize = -2141060855;
	}
inputReady:
	if (capture->flags & 0x20000) {
		ret = requestVeneziaVipCaptureProcessing(index, &g_veneziaVipState, 0);
		if (ret < 0) {
			if (ret == -2141060856)
				ret = 0;
			goto finished;
		}
	} else {
		notifyVeneziaVipProcessor(&g_veneziaVipState, index, 0, 4096);
	}
	if (list->count && list->count <= capture->completedCount)
		timeout = 1;
	event = 0;
	ret = sceKernelWaitEventFlag(capture->eventFlag, 0x3f06, 5, &event, &timeout);
	if (ret < 0) {
		if (ret != -2147319803)
			goto finished;
		ret = 0;
	}
	if (event & 0x1000) {
		ret = -2141060851;
		capture->flags |= 7;
		goto finished;
	}
	if (event & 0x200) {
		ret = -2141060856;
		capture->flags |= 7;
		goto finished;
	}
	if (event & 0x2000) {
		ret = -2141060850;
		goto finished;
	}
	if (event & 0x100) {
		ret = -2141060853;
		goto finished;
	}
	if (event & 2) {
		ret = collectVeneziaVipCaptureResults(index, storage, list, capture->completedCount);
		if (ret < 0)
			goto collectionError;
	}
	if (inputSize)
		ret = inputSize;
	goto finished;
collectionError:
	if (ret == -2141060852)
		ret = -2141060853;
finished:
unlock:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

/* name unknown: submit raw PES payload and collect a multi-capture channel. */
int processVeneziaVipMultiRawCapture(unsigned int processor, const VeneziaVipRawInput *input,
	const VeneziaDebugSnapshotList *list, uint32_t *completed)
{
	VeneziaVipPesHeader header;
	VeneziaVipInputBuffer buffers[2];
	VeneziaVipInputBufferList inputBuffers;
	VeneziaVipProcessStorage *storage;
	VeneziaDebugProcessCapture *capture;
	VeneziaVipCommandContext *context;
	unsigned int index = processor - 1, captureProcessor;
	SceSize inputSize = 0, available;
	SceUInt timeout = 8000, event;
	int ret, capacity, deferredError = 0;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	if (!completed)
		return -2141060863;
	*completed = 0;
	if (!list)
		return -2141060863;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	if (index >= storage->status[0] ||
	    (captureProcessor = storage->processors[index]) == (unsigned int)-1) {
		ret = -2141060863;
		goto unlock;
	}
	if (input && input->payload.data)
		inputSize = input->payload.size;
	event = 0;
	sceKernelPollEventFlag(g_veneziaVipState.eventFlag, 0x2000000, 1, &event);
	if (event & 0x2000000)
		recoverVeneziaVipProcessors(&g_veneziaVipState, storage);
	capture = &storage->captures[index];
	if (capture->flags & 7) {
		capture->flags &= ~7;
		capture->debugResultFlags = 0;
		stopVeneziaVipCapture(captureProcessor, &g_veneziaVipState, storage);
		ret = setupVeneziaVipCapture(captureProcessor, storage,
			&capture->captureParam.imageSize, &capture->captureParam.imageFormat);
		if (ret < 0)
			goto unlock;
		capture->flags &= ~8;
	}
	ret = releaseVeneziaVipCaptureResults(index, storage);
	if (ret < 0)
		goto finished;
	ret = collectVeneziaVipCaptureResults(index, storage, list, capture->completedCount);
	if (ret < 0)
		goto collectionError;
	if (inputSize) {
		getVeneziaVipCaptureContext(capture, &g_veneziaVipState, &context);
		if (getVeneziaVipCaptureInputCapacity(capture, &g_veneziaVipState, context,
			&available)) {
			if (!(capture->flags & 0x107)) {
				ret = -2141060856;
				goto finished;
			}
			available = capture->bufferRegion.size;
		}
		capacity = 0;
		if (available > 1024) {
			ret = available - 1024;
			if (ret < 0) {
				if (available != (unsigned int)-2141059831)
					goto finished;
				deferredError = -2141060855;
				goto inputReady;
			}
			capacity = ret;
		}
		if (inputSize + sizeof(header) <= (unsigned int)capacity) {
			header = g_veneziaVipPesHeader;
			ret = encodeVeneziaVipPesTimestamps(&header,
				input->presentationTimestamp == UINT64_MAX ? NULL : &input->presentationTimestamp,
				input->decodingTimestamp == UINT64_MAX ? NULL : &input->decodingTimestamp);
			if (ret < 0)
				goto finished;
			buffers[0].data = &header;
			buffers[0].size = sizeof(header);
			buffers[1].data = input->payload.data;
			buffers[1].size = inputSize;
			inputBuffers.count = 2;
			inputBuffers.buffers = buffers;
			ret = submitVeneziaVipInputBuffers(index, &g_veneziaVipState, storage, &inputBuffers);
			if (ret < 0)
				goto finished;
			getVeneziaVipCaptureContext(capture, &g_veneziaVipState, &context);
			commitVeneziaVipCaptureInput(capture, &g_veneziaVipState, context);
		} else {
			deferredError = -2141060855;
		}
	}
inputReady:
	if (capture->flags & 0x20000) {
		ret = requestVeneziaVipCaptureProcessing(index, &g_veneziaVipState, 0);
		if (ret < 0) {
			if (ret == -2141060856)
				ret = 0;
			goto finished;
		}
	} else {
		notifyVeneziaVipProcessor(&g_veneziaVipState, index, 0, 4096);
	}
	if (list->count && list->count <= capture->completedCount)
		timeout = 1;
	event = 0;
	ret = sceKernelWaitEventFlag(capture->eventFlag, 0x3f06, 5, &event, &timeout);
	if (ret < 0) {
		if (ret != -2147319803)
			goto finished;
		ret = 0;
	}
	if (event & 0x1000) {
		ret = -2141060851;
		capture->flags |= 7;
		goto finished;
	}
	if (event & 0x200) {
		ret = -2141060856;
		capture->flags |= 7;
		goto finished;
	}
	if (event & 0x2000) {
		ret = -2141060850;
		goto finished;
	}
	if (event & 0x100) {
		ret = -2141060853;
		goto finished;
	}
	if (event & 2) {
		ret = collectVeneziaVipCaptureResults(index, storage, list, capture->completedCount);
		if (ret < 0)
			goto collectionError;
	}
	if (deferredError)
		ret = deferredError;
	goto finished;
collectionError:
	if (ret == -2141060852)
		ret = -2141060853;
finished:
	*completed = capture->completedCount;
unlock:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

/* name unknown: submit raw multi-capture payload using an optional timeout. */
int submitVeneziaVipMultiRawCapture(unsigned int processor, const VeneziaVipRawInput *input,
	const SceUInt *timeoutValue)
{
	VeneziaVipPesHeader header;
	VeneziaVipInputBuffer buffers[2];
	VeneziaVipInputBufferList inputBuffers;
	VeneziaDebugSnapshotList snapshots;
	const VeneziaDebugSnapshotList *list = &snapshots;
	VeneziaVipProcessStorage *storage;
	VeneziaDebugProcessCapture *capture;
	VeneziaVipCommandContext *context;
	unsigned int index = processor - 1, captureProcessor;
	SceSize inputSize = 0, available;
	SceUInt timeout, event;
	int ret, capacity, deferredError = 0;

	snapshots.count = 0;
	timeout = timeoutValue ? *timeoutValue : 8000;
	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	if (index >= storage->status[0] ||
	    (captureProcessor = storage->processors[index]) == (unsigned int)-1) {
		ret = -2141060863;
		goto unlock;
	}
	if (input && input->payload.data)
		inputSize = input->payload.size;
	event = 0;
	sceKernelPollEventFlag(g_veneziaVipState.eventFlag, 0x2000000, 1, &event);
	if (event & 0x2000000)
		recoverVeneziaVipProcessors(&g_veneziaVipState, storage);
	capture = &storage->captures[index];
	if (capture->flags & 7) {
		capture->flags &= ~7;
		capture->debugResultFlags = 0;
		stopVeneziaVipCapture(captureProcessor, &g_veneziaVipState, storage);
		ret = setupVeneziaVipCapture(captureProcessor, storage,
			&capture->captureParam.imageSize, &capture->captureParam.imageFormat);
		if (ret < 0)
			goto unlock;
		capture->flags &= ~8;
	}
	ret = releaseVeneziaVipCaptureResults(index, storage);
	if (ret < 0)
		goto finished;
	ret = collectVeneziaVipCaptureResults(index, storage, list, capture->completedCount);
	if (ret < 0)
		goto collectionError;
	if (inputSize) {
		getVeneziaVipCaptureContext(capture, &g_veneziaVipState, &context);
		if (getVeneziaVipCaptureInputCapacity(capture, &g_veneziaVipState, context,
			&available)) {
			if (!(capture->flags & 0x107)) {
				ret = -2141060856;
				goto finished;
			}
			available = capture->bufferRegion.size;
		}
		capacity = 0;
		if (available > 1024) {
			ret = available - 1024;
			if (ret < 0) {
				if (available != (unsigned int)-2141059831)
					goto finished;
				deferredError = -2141060855;
				goto inputReady;
			}
			capacity = ret;
		}
		if (inputSize + sizeof(header) <= (unsigned int)capacity) {
			header = g_veneziaVipPesHeader;
			ret = encodeVeneziaVipPesTimestamps(&header,
				input->presentationTimestamp == UINT64_MAX ? NULL : &input->presentationTimestamp,
				input->decodingTimestamp == UINT64_MAX ? NULL : &input->decodingTimestamp);
			if (ret < 0)
				goto finished;
			buffers[0].data = &header;
			buffers[0].size = sizeof(header);
			buffers[1].data = input->payload.data;
			buffers[1].size = inputSize;
			inputBuffers.count = 2;
			inputBuffers.buffers = buffers;
			ret = submitVeneziaVipInputBuffers(index, &g_veneziaVipState, storage, &inputBuffers);
			if (ret < 0)
				goto finished;
			getVeneziaVipCaptureContext(capture, &g_veneziaVipState, &context);
			commitVeneziaVipCaptureInput(capture, &g_veneziaVipState, context);
		} else {
			deferredError = -2141060855;
		}
	}
inputReady:
	if (capture->flags & 0x20000) {
		ret = requestVeneziaVipCaptureProcessing(index, &g_veneziaVipState, 0);
		if (ret < 0) {
			if (ret == -2141060856)
				ret = 0;
			goto finished;
		}
	} else {
		notifyVeneziaVipProcessor(&g_veneziaVipState, index, 0, 4096);
	}
	if (list->count && list->count <= capture->completedCount)
		timeout = 1;
	event = 0;
	ret = sceKernelWaitEventFlag(capture->eventFlag, 0x3f06, 5, &event, &timeout);
	if (ret < 0) {
		if (ret != -2147319803)
			goto finished;
		ret = 0;
	}
	if (event & 0x1000) {
		ret = -2141060851;
		capture->flags |= 7;
		goto finished;
	}
	if (event & 0x200) {
		ret = -2141060856;
		capture->flags |= 7;
		goto finished;
	}
	if (event & 0x2000) {
		ret = -2141060850;
		goto finished;
	}
	if (event & 0x100) {
		ret = -2141060853;
		goto finished;
	}
	if (event & 2) {
		ret = collectVeneziaVipCaptureResults(index, storage, list, capture->completedCount);
		if (ret < 0)
			goto collectionError;
	}
	if (deferredError)
		ret = deferredError;
	goto finished;
collectionError:
	if (ret == -2141060852)
		ret = -2141060853;
finished:
unlock:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

/* name unknown: collect available snapshots without waiting for new capture input. */
int pollVeneziaVipMultiCapture(unsigned int processor, const VeneziaDebugSnapshotList *list,
	uint32_t *completed)
{
	VeneziaVipProcessStorage *storage;
	VeneziaDebugProcessCapture *capture;
	unsigned int index = processor - 1;
	SceUInt event;
	int ret;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	if (!completed)
		return -2141060863;
	*completed = 0;
	if (!list)
		return -2141060863;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	if (index >= storage->status[0] || storage->processors[index] == -1) {
		ret = -2141060863;
		goto unlock;
	}
	event = 0;
	sceKernelPollEventFlag(g_veneziaVipState.eventFlag, 0x2000000, 1, &event);
	if (event & 0x2000000)
		recoverVeneziaVipProcessors(&g_veneziaVipState, storage);
	capture = &storage->captures[index];
	if (capture->flags & 8) {
		ret = 0;
	} else {
		ret = releaseVeneziaVipCaptureResults(index, storage);
		if (ret >= 0) {
			ret = collectVeneziaVipCaptureResults(index, storage, list,
				capture->completedCount);
			if (ret == -2141060852)
				ret = -2141060853;
		}
		*completed = capture->completedCount;
	}
unlock:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

extern int endVeneziaVipCaptureInput(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, VeneziaVipCommandContext *context);
extern int getVeneziaVipCaptureProcessorStatus(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, unsigned int processor, uint32_t *status);

/* name unknown: signal end of input and drain snapshots until capture finishes. */
int finishVeneziaVipMultiCapture(unsigned int processor,
	const VeneziaDebugSnapshotList *list, uint32_t *completed)
{
	VeneziaVipProcessStorage *storage;
	VeneziaDebugProcessCapture *capture;
	VeneziaVipCommandContext *context;
	unsigned int index = processor - 1, collected = 0, before, after;
	SceUInt timeout, event, status;
	int ret;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	if (!completed)
		return -2141060863;
	*completed = 0;
	if (!list)
		return -2141060863;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	if (index >= storage->status[0] || storage->processors[index] == -1) {
		ret = -2141060863;
		goto unlock;
	}
	capture = &storage->captures[index];
	if (!(capture->flags & 1)) {
		capture->flags |= 1;
		getVeneziaVipCaptureContext(capture, &g_veneziaVipState, &context);
		if (endVeneziaVipCaptureInput(capture, &g_veneziaVipState, context)) {
			ret = -2141060856;
			goto unlock;
		}
		notifyVeneziaVipProcessor(&g_veneziaVipState, index, 0, 4096);
	}
	ret = releaseVeneziaVipCaptureResults(index, storage);
	if (ret < 0)
		goto finished;
	for (;;) {
		before = capture->completedCount;
		notifyVeneziaVipProcessor(&g_veneziaVipState, index, 0, 4096);
		ret = collectVeneziaVipCaptureResults(index, storage, list, collected);
		if (ret < 0)
			break;
		after = capture->completedCount;
		collected += after - before;
		if (collected >= list->count || ((capture->flags & 7) == 7 && before == after))
			break;
		status = 0;
		sceKernelPollEventFlag(g_veneziaVipState.eventFlag, 0x2000000, 1, &status);
		if (status & 0x2000000)
			recoverVeneziaVipProcessors(&g_veneziaVipState, storage);
		event = 0;
		timeout = 8000;
		ret = sceKernelWaitEventFlag(capture->eventFlag, 0x3f0a, 5, &event, &timeout);
		if (ret < 0 && ret != -2147319803)
			break;
		if (!(capture->flags & 2)) {
			getVeneziaVipCaptureContext(capture, &g_veneziaVipState, &context);
			getVeneziaVipCaptureProcessorStatus(capture, &g_veneziaVipState, 1, &status);
			if (status == 4)
				capture->flags |= 6;
		}
		if (event & 0x1000) {
			ret = -2141060851;
			capture->flags |= 7;
			break;
		}
		if (event & 0x200) {
			ret = -2141060856;
			capture->flags |= 7;
			break;
		}
		if (event & 0x2000) {
			ret = -2141060850;
			break;
		}
		if (event & 0x100) {
			ret = -2141060853;
			break;
		}
	}
finished:
	*completed = collected;
unlock:
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

/* name unknown: request reinitialization before the channel's next capture. */
int restartVeneziaVipMultiCapture(unsigned int processor)
{
	VeneziaVipProcessStorage *storage;
	unsigned int index = processor - 1;
	int ret;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	if (index >= storage->status[0] || storage->processors[index] == -1) {
		unlockVeneziaVipProcess(g_veneziaVipState.processLock);
		return -2141060863;
	}
	storage->captures[index].flags |= 7;
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

extern int getVeneziaVipActiveProcess(VeneziaVipState *state,
	VeneziaVipProcessStorage **storage);

/* name unknown: release channel results while the debugger owns an active process. */
int releaseVeneziaVipCaptureResultsForProcessor(unsigned int processor)
{
	VeneziaVipProcessStorage *storage, *active;
	unsigned int index = processor - 1;
	int ret;

	ret = waitVeneziaVipProcessReady();
	if (ret < 0)
		return ret;
	storage = _sceVipGetVipProcessResource(0);
	if (!storage)
		return -2141060856;
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	if (index >= storage->status[0] || storage->processors[index] == -1)
		ret = -2141060863;
	else if (getVeneziaVipActiveProcess(&g_veneziaVipState, &active) >= 0 && active)
		ret = releaseVeneziaVipCaptureResults(index, storage);
	else
		ret = -2141060856;
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

extern int requestVeneziaVipProcessorControl(VeneziaVipState *state,
	VeneziaVipProcessStorage *storage);
extern int stopVeneziaVipProcessors(VeneziaVipState *state,
	VeneziaVipProcessStorage *storage);

/* name unknown: apply selected processor controls while holding the process lock. */
int controlVeneziaVipProcess(VeneziaVipProcessStorage *storage, unsigned int control)
{
	int ret;

	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	ret = control & 1;
	if (control & 1)
		ret = requestVeneziaVipProcessorControl(&g_veneziaVipState, storage);
	if (control & 2) {
		ret = stopVeneziaVipProcessors(&g_veneziaVipState, storage);
		stopVeneziaVipDebugger(&g_veneziaVipState, storage);
	}
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

extern int startVeneziaVipProcessors(VeneziaVipState *state);

/* name unknown: initialize capture configuration and start both processors. */
int prepareVeneziaVipProcess(VeneziaVipProcessStorage *storage)
{
	int ret;

	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	beginVeneziaVipCaptureConfiguration(&g_veneziaVipState, storage);
	prepareVeneziaVipCapture(&g_veneziaVipState, storage);
	ret = startVeneziaVipProcessors(&g_veneziaVipState);
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

/* name unknown: serialize with all work holding the process mutex. */
int synchronizeVeneziaVipMultiCapture(void)
{
	lockVeneziaVipProcess(g_veneziaVipState.processLock);
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return 0;
}

/* name unknown: release the secondary and primary debugger interrupt handlers. */
int releaseVeneziaVipInterrupts(void)
{
	sceKernelDisableIntr(57);
	sceKernelReleaseIntrHandler(57);
	sceKernelDisableIntr(56);
	sceKernelReleaseIntrHandler(56);
	return 0;
}

/* name unknown: select the command area within debugger physical memory. */
int programVeneziaVipDebuggerCommandBase(VeneziaVipState *state)
{
	state->bootRegisters->debuggerPhysicalBase = state->debuggerMemory.physicalAddress + 0x80000;
	return 0;
}

/* name unknown: issue the peripheral reset and ignore its status. */
int resetVeneziaVipPeripheral(void)
{
	ScePervasiveForDriver_E2D8F6C3();
	return 0;
}

/* name unknown: clear callback words and install both VIP mailbox interrupts. */
int initializeVeneziaVipInterrupts(VeneziaVipState *state)
{
	VeneziaDebugEventCounter *counters = state->callbackState->counters;
	int ret;

	counters[0].count = 0;
	counters[0].unidentifiedControl = 0;
	counters[1].count = 0;
	counters[1].unidentifiedControl = 0;
	counters[2].count = 0;
	counters[2].unidentifiedControl = 0;
	counters[3].count = 0;
	counters[3].unidentifiedControl = 0;
	counters[4].count = 0;
	counters[4].unidentifiedControl = 0;
	counters[5].count = 0;
	counters[5].unidentifiedControl = 0;
	ret = sceKernelRegisterIntrHandler(56, "SceVipMailbox0", 0,
		(SceKernelIntrHandler)veneziaDebugAuxiliaryInterruptHandler,
		state, 208, 15, NULL);
	if (ret < 0)
		return ret;
	ret = sceKernelEnableIntr(56);
	if (ret < 0)
		goto releasePrimary;
	ret = sceKernelRegisterIntrHandler(57, "SceVipMailbox1", 0,
		(SceKernelIntrHandler)veneziaDebugSecondaryInterruptHandler,
		state, 208, 15, NULL);
	if (ret < 0)
		goto releasePrimary;
	ret = sceKernelEnableIntr(57);
	if (ret >= 0)
		return 0;
	sceKernelDisableIntr(57);
	sceKernelReleaseIntrHandler(57);
releasePrimary:
	sceKernelDisableIntr(56);
	sceKernelReleaseIntrHandler(56);
	return ret;
}

extern int getVeneziaVipProcessorStatusLocked(VeneziaVipState *state,
	unsigned int processor, unsigned int *status);
extern int setVeneziaVipProcessorEventMaskLocked(VeneziaVipState *state,
	unsigned int processor, uint32_t mask);

/* name unknown: wait for both processors and enable every command event. */
int initializeVeneziaVipProcessorEvents(VeneziaVipState *state,
	VeneziaVipProcessStorage *storage)
{
	unsigned int status;
	int count, ret;

	if (activateVeneziaVipDebugger(state, storage) < 0)
		return -2141060860;
	for (count = 1000; count; count--) {
		ret = getVeneziaVipProcessorStatusLocked(state, 0, &status);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(59, 5, 0, status, 2, 10, ret);
		if (ret != 0)
			return -2141060860;
		if ((status & ~8u) == 2)
			goto primaryReady;
		sceKernelDelayThread(1000);
	}
	return -2141060860;
primaryReady:
	for (count = 1000; count; count--) {
		ret = getVeneziaVipProcessorStatusLocked(state, 1, &status);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(59, 5, 1, status, 2, 10, ret);
		if (ret != 0)
			return -2141060860;
		if ((status & ~8u) == 2)
			goto secondaryReady;
		sceKernelDelayThread(1000);
	}
	return -2141060860;
secondaryReady:
	ret = setVeneziaVipProcessorEventMaskLocked(state, 0, -1);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(58, 3, 0, -1, ret);
	if (ret != 0)
		return -2141060860;
	ret = setVeneziaVipProcessorEventMaskLocked(state, 1, -1);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(58, 3, 1, -1, ret);
	if (ret != 0)
		return -2141060860;
	return 0;
}

/* name unknown: stop both processor commands and their debugger. */
int shutdownVeneziaVipProcessors(VeneziaVipState *state,
	VeneziaVipProcessStorage *storage)
{
	stopVeneziaVipProcessors(state, storage);
	stopVeneziaVipDebugger(state, storage);
	return 0;
}

/* name unknown: calculate storage for an aligned capture byte capacity. */
__attribute__((noinline)) unsigned int getVeneziaVipCaptureWorkSize(unsigned int capacity)
{
	if (capacity)
		return 41 * ((capacity + 7) & ~7u) + 4288;
	return 0;
}

extern int detachVeneziaVipCaptureContext(unsigned int index, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage);

/* name unknown: detach a capture command context and acknowledge its event. */
int resetVeneziaVipCaptureContext(unsigned int index, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage)
{
	VeneziaDebugProcessCapture *capture = &storage->captures[index];
	VeneziaVipCommandContext *context;
	int ret;

	getVeneziaVipCaptureContext(capture, state, &context);
	ret = (int)context;
	if (context) {
		if (beginVeneziaVipCaptureSetup(index, state, storage) < 0) {
			ret = -2141060856;
		} else {
			ret = detachVeneziaVipCaptureContext(index, state, storage);
			if (ret < 0)
				ret = -2141060860;
			setVeneziaVipCaptureContext(capture, state, NULL);
			endVeneziaVipCaptureSetup(index, state, storage);
		}
	}
	sceKernelClearEventFlag(capture->eventFlag, 512);
	return ret;
}

extern int getVeneziaVipCaptureWriteCursor(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, VeneziaVipCommandContext *context, uint32_t *cursor);
extern int setVeneziaVipCaptureWriteCursor(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, VeneziaVipCommandContext *context, uint32_t cursor);

/* name unknown: copy scatter/gather input into the capture ring with wraparound. */
int copyVeneziaVipInputToCaptureRing(unsigned int index, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage, const VeneziaVipInputBufferList *list)
{
	VeneziaDebugProcessCapture *capture = &storage->captures[index];
	VeneziaVipInputIterator iterator;
	VeneziaVipCommandContext *context;
	SceSize remaining, first, copied;
	uint32_t cursor;
	int ret;

	getVeneziaVipCaptureContext(capture, state, &context);
	initializeVeneziaVipInputIterator(&iterator, list);
	remaining = getVeneziaVipInputRemaining(&iterator);
	if (getVeneziaVipCaptureWriteCursor(capture, state, context, &cursor) != 0)
		return -2141060856;
	if (remaining + cursor >= capture->bufferEnd) {
		first = capture->bufferEnd - cursor;
		ret = copyVeneziaVipInput(cursor, first, &copied, &iterator);
		if (ret < 0)
			return -2141060856;
		remaining -= first;
		cursor = capture->bufferRegion.virtualAddress;
		if (setVeneziaVipCaptureWriteCursor(capture, state, context, cursor) != 0)
			return -2141060856;
		if (!remaining)
			return ret;
		if (getVeneziaVipCaptureWriteCursor(capture, state, context, &cursor) != 0)
			return -2141060856;
	}
	ret = copyVeneziaVipInput(cursor, remaining, &copied, &iterator);
	if (ret < 0 || remaining > copied)
		return -2141060856;
	cursor += remaining;
	if (setVeneziaVipCaptureWriteCursor(capture, state, context, cursor) != 0)
		return -2141060856;
	return ret;
}

/* name unknown: empty the four-entry process scheduling queue. */
__attribute__((noinline))
int initializeVeneziaVipProcessQueue(VeneziaVipProcessQueue *queue)
{
	queue->entries[0] = 0;
	queue->entries[1] = 0;
	queue->entries[2] = 0;
	queue->entries[3] = 0;
	queue->count = 0;
	queue->writeIndex = 0;
	queue->reserved[0] = 0;
	queue->reserved[1] = 0;
	return 0;
}

/* name unknown: enqueue one unique process byte, failing when four are queued. */
int enqueueVeneziaVipProcess(VeneziaVipProcessQueue *queue, int value)
{
	unsigned int count = queue->count;
	unsigned int write;
	unsigned int distance;
	uint8_t index;

	if (count > 3)
		return -1;
	write = queue->writeIndex;
	if (count) {
		index = count <= write ? write - count : write + 4 - count;
		if (queue->entries[index] == value)
			return -2;
		if (count > 1) {
			distance = (uint8_t)(count - 1);
			index = write >= distance ? write - distance : write + 4 - distance;
			if (queue->entries[index] == value)
				return -2;
			if (count > 2) {
				index = write - 1;
				if (!write)
					index = 3;
				if (queue->entries[index] == value)
					return -2;
			}
		}
	}
	queue->entries[write] = value;
	queue->count = count + 1;
	queue->writeIndex = (write + 1) & 3;
	return 0;
}

/* name unknown: dequeue the oldest queued byte, or return the empty sentinel. */
int dequeueVeneziaVipProcess(VeneziaVipProcessQueue *queue, uint8_t *value)
{
	unsigned int count = queue->count;
	unsigned int write;
	uint8_t index;

	if (count) {
		write = queue->writeIndex;
		index = count <= write ? write - count : 4 - count + write;
		*value = queue->entries[index];
		queue->count--;
		return 0;
	}
	*value = -1;
	return -1;
}

/* name unknown: remove matching process bytes using the original circular scan. */
int removeVeneziaVipProcess(VeneziaVipProcessQueue *queue, int value)
{
	unsigned int count = queue->count;
	unsigned int item, write, shift, distance;
	uint8_t index, source, destination, entry, wrappedWrite;

	if (!count)
		return 0;
	item = 1;
	for (;;) {
		distance = (uint8_t)(item - 1);
		if (count) {
			write = queue->writeIndex;
			if (write < distance)
				write = (uint8_t)(write + 4);
			index = (uint8_t)(write - distance);
			if (queue->entries[index] != value)
				goto next;
		} else if (queue->entries[0] != value) {
			goto next;
		}
		if (count > item && count) {
			write = queue->writeIndex;
			shift = item;
			wrappedWrite = write + 4;
			do {
				source = write - shift;
				if (write < shift)
					source = wrappedWrite - shift;
				distance = (uint8_t)(shift - 1);
				entry = queue->entries[source];
				shift = (uint8_t)(shift + 1);
				destination = write >= distance ?
					write - distance : wrappedWrite - distance;
				queue->entries[destination] = entry;
			} while (shift != count);
		}
		count = (uint8_t)(count - 1);
		queue->count = count;
		goto advance;
next:
		count = queue->count;
advance:
		if (item >= count)
			return 0;
		item = (uint8_t)(item + 1);
	}
}

/* name unknown: return the number of queued process bytes. */
int getVeneziaVipProcessQueueCount(const VeneziaVipProcessQueue *queue)
{
	return queue->count;
}

extern int getVeneziaVipCommandContextStateLocked(VeneziaVipState *state,
	unsigned int processor, const VeneziaVipCommandContext *context);

/* name unknown: wait for processor capture events and refresh its context state. */
int waitVeneziaVipCaptureInterrupt(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, unsigned int processor, unsigned int flags,
	uint32_t *eventsOut, unsigned int *statusOut)
{
	uint32_t events = 0, currentEvents;
	unsigned int status = 0;
	SceUInt timeout;
	VeneziaVipCommandContext *context;
	unsigned int expected = flags & 2 ? 1 : 2;
	uint32_t count;
	int ret, statusRet, contextState;

	do {
		timeout = 500000;
		currentEvents = 0;
		ret = sceKernelWaitEventFlag(state->captureInterruptEvents[processor],
			0xf00800ff, 3, &currentEvents, &timeout);
		if (ret < 0) {
			if (ret != (int)0x80028005) {
				ret = -2141060856;
				goto output;
			}
			events |= currentEvents & 0xffff00ff;
			break;
		}
		events |= currentEvents & 0xffff00ff;
	} while (!(currentEvents & 0xf008007f));
	if (!processor && (events & 0x20000))
		events |= 0x100;
	statusRet = getVeneziaVipProcessorStatusLocked(state, processor, &status);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(59, 5, processor, status, expected, 10, statusRet);
	if (statusRet != 0) {
		ret = -2141060856;
		goto output;
	}
	if (events & 0x80000) {
		lockVeneziaVipProcess(state->commonProcessorLock);
		count = capture->processorCompletionCounts[processor];
		unlockVeneziaVipProcess(state->commonProcessorLock);
		lockVeneziaVipProcess(state->commonProcessorLock);
		capture->processorCompletionCounts[processor] = count + 1;
		unlockVeneziaVipProcess(state->commonProcessorLock);
	}
	if (events & 0xe0000000) {
		lockVeneziaVipProcess(state->commonProcessorLock);
		count = capture->processorPendingCounts[processor];
		unlockVeneziaVipProcess(state->commonProcessorLock);
		lockVeneziaVipProcess(state->commonProcessorLock);
		capture->processorPendingCounts[processor] = count - 1;
		unlockVeneziaVipProcess(state->commonProcessorLock);
	}
output:
	if (eventsOut)
		*eventsOut = events;
	if (statusOut)
		*statusOut = status;
	if (!(flags & 1)) {
		lockVeneziaVipProcess(state->commonProcessorLock);
		context = capture->commandContext;
		unlockVeneziaVipProcess(state->commonProcessorLock);
		if (!context)
			return -2141060856;
		contextState = getVeneziaVipCommandContextStateLocked(state, processor, context);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(79, 3, processor, context, contextState);
		lockVeneziaVipProcess(state->commonProcessorLock);
		capture->processorContextState[processor] = contextState;
		unlockVeneziaVipProcess(state->commonProcessorLock);
	}
	if (!timeout)
		ret = -2141060856;
	return ret;
}

/* name unknown: wait for primary capture events and refresh its context state. */
int waitVeneziaVipPrimaryCaptureInterrupt(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, uint32_t *eventsOut, unsigned int *statusOut)
{
	uint32_t events = 0, currentEvents;
	unsigned int status = 0;
	SceUInt timeout;
	VeneziaVipCommandContext *context;
	uint32_t count;
	int ret, statusRet, contextState;

	do {
		timeout = 500000;
		currentEvents = 0;
		ret = sceKernelWaitEventFlag(state->captureInterruptEvents[0],
			0xf00800ff, 3, &currentEvents, &timeout);
		if (ret < 0) {
			if (ret != (int)0x80028005) {
				ret = -2141060856;
				goto output;
			}
			events |= currentEvents & 0xffff00ff;
			break;
		}
		events |= currentEvents & 0xffff00ff;
	} while (!(currentEvents & 0xf008007f));
	if (!0 && (events & 0x20000))
		events |= 0x100;
	statusRet = getVeneziaVipProcessorStatusLocked(state, 0, &status);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(59, 5, 0, status, 2, 10, statusRet);
	if (statusRet != 0) {
		ret = -2141060856;
		goto output;
	}
	if (events & 0x80000) {
		lockVeneziaVipProcess(state->commonProcessorLock);
		count = capture->processorCompletionCounts[0];
		unlockVeneziaVipProcess(state->commonProcessorLock);
		lockVeneziaVipProcess(state->commonProcessorLock);
		capture->processorCompletionCounts[0] = count + 1;
		unlockVeneziaVipProcess(state->commonProcessorLock);
	}
	if (events & 0xe0000000) {
		lockVeneziaVipProcess(state->commonProcessorLock);
		count = capture->processorPendingCounts[0];
		unlockVeneziaVipProcess(state->commonProcessorLock);
		lockVeneziaVipProcess(state->commonProcessorLock);
		capture->processorPendingCounts[0] = count - 1;
		unlockVeneziaVipProcess(state->commonProcessorLock);
	}
output:
	if (eventsOut)
		*eventsOut = events;
	if (statusOut)
		*statusOut = status;
	{
		lockVeneziaVipProcess(state->commonProcessorLock);
		context = capture->commandContext;
		unlockVeneziaVipProcess(state->commonProcessorLock);
		if (!context)
			return -2141060856;
		contextState = getVeneziaVipCommandContextStateLocked(state, 0, context);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(79, 3, 0, context, contextState);
		lockVeneziaVipProcess(state->commonProcessorLock);
		capture->processorContextState[0] = contextState;
		unlockVeneziaVipProcess(state->commonProcessorLock);
	}
	if (!timeout)
		ret = -2141060856;
	return ret;
}

/* name unknown: wait for secondary capture events and refresh its context state. */
int waitVeneziaVipSecondaryCaptureInterrupt(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, uint32_t *eventsOut, unsigned int *statusOut)
{
	uint32_t events = 0, currentEvents;
	unsigned int status = 0;
	SceUInt timeout;
	VeneziaVipCommandContext *context;
	uint32_t count;
	int ret, statusRet, contextState;

	do {
		timeout = 500000;
		currentEvents = 0;
		ret = sceKernelWaitEventFlag(state->captureInterruptEvents[1],
			0xf00800ff, 3, &currentEvents, &timeout);
		if (ret < 0) {
			if (ret != (int)0x80028005) {
				ret = -2141060856;
				goto output;
			}
			events |= currentEvents & 0xffff00ff;
			break;
		}
		events |= currentEvents & 0xffff00ff;
	} while (!(currentEvents & 0xf008007f));
	statusRet = getVeneziaVipProcessorStatusLocked(state, 1, &status);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(59, 5, 1, status, 2, 10, statusRet);
	if (statusRet != 0) {
		ret = -2141060856;
		goto output;
	}
	if (events & 0x80000) {
		lockVeneziaVipProcess(state->commonProcessorLock);
		count = capture->processorCompletionCounts[1];
		unlockVeneziaVipProcess(state->commonProcessorLock);
		lockVeneziaVipProcess(state->commonProcessorLock);
		capture->processorCompletionCounts[1] = count + 1;
		unlockVeneziaVipProcess(state->commonProcessorLock);
	}
	if (events & 0xe0000000) {
		lockVeneziaVipProcess(state->commonProcessorLock);
		count = capture->processorPendingCounts[1];
		unlockVeneziaVipProcess(state->commonProcessorLock);
		lockVeneziaVipProcess(state->commonProcessorLock);
		capture->processorPendingCounts[1] = count - 1;
		unlockVeneziaVipProcess(state->commonProcessorLock);
	}
output:
	if (eventsOut)
		*eventsOut = events;
	if (statusOut)
		*statusOut = status;
	{
		lockVeneziaVipProcess(state->commonProcessorLock);
		context = capture->commandContext;
		unlockVeneziaVipProcess(state->commonProcessorLock);
		if (!context)
			return -2141060856;
		contextState = getVeneziaVipCommandContextStateLocked(state, 1, context);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(79, 3, 1, context, contextState);
		lockVeneziaVipProcess(state->commonProcessorLock);
		capture->processorContextState[1] = contextState;
		unlockVeneziaVipProcess(state->commonProcessorLock);
	}
	if (!timeout)
		ret = -2141060856;
	return ret;
}

/* name unknown: mark every capture for recovery and acknowledge a pending command. */
int markVeneziaVipCaptureRecovery(VeneziaVipState *state, unsigned int processor,
	int *command)
{
	VeneziaVipProcessStorage *storage = state->activeProcess;
	VeneziaDebugProcessCapture *capture;
	unsigned int index;
	int pending;

	lockVeneziaVipProcess(state->commonProcessorLock);
	if (*command < 0)
		*command &= 0x7fffffff;
	pending = state->processorPendingCommands[processor];
	if (pending < 0)
		state->processorRequests[processor].resultFlags = 2;
	if (storage->status[0]) {
		capture = storage->captures;
		index = 0;
		do {
			capture->recoveryFlags |= 6;
			capture++;
			index++;
		} while (index < storage->status[0]);
	}
	if (pending < 0) {
		state->processorPendingCommands[processor] = 0;
		unlockVeneziaVipProcess(state->commonProcessorLock);
		return sceKernelSetEventFlag(state->processorEvents[processor], 0x100000);
	}
	unlockVeneziaVipProcess(state->commonProcessorLock);
	return 0;
}

/* name unknown: complete a primary request and deliver capture recovery callbacks. */
int completeVeneziaVipCaptureRequest(VeneziaVipState *state, int index,
	uint32_t flags, int *command)
{
	VeneziaVipProcessStorage *storage = state->activeProcess;
	VeneziaDebugProcessCapture *capture = index == -1 ? NULL : &storage->captures[index];
	uint32_t merged, event;
	VeneziaDebugProcessCapture *current;
	unsigned int slot;
	int pending;

	lockVeneziaVipProcess(state->commonProcessorLock);
	if (*command < 0)
		*command &= 0x7fffffff;
	pending = state->processorPendingCommands[0];
	if (pending < 0) {
		state->processorRequests[0].resultFlags = flags;
		if (capture) {
			merged = flags;
			if (capture->recoveryFlags & 6) {
				merged |= capture->recoveryFlags;
				state->processorRequests[0].resultFlags = merged;
			}
			capture->recoveryFlags = 0;
			if (merged & 6)
				goto notify;
			if (flags & 6)
				goto notify;
			goto acknowledge;
		}
	} else if (capture) {
		capture->recoveryFlags |= flags;
		if (flags & 6)
			goto notify;
		goto acknowledge;
	}
	if ((flags & 6) && storage->status[0]) {
		current = storage->captures;
		slot = 0;
		do {
			current->recoveryFlags |= 6;
			current++;
			slot++;
		} while (slot < storage->status[0]);
	}
	goto acknowledge;
notify:
	event = 0;
	sceKernelPollEventFlag(state->processorEvents[0], 0x2000000, 1, &event);
	if (event != 0x2000000) {
		sceKernelSetEventFlag(capture->eventFlag, 512);
		lockVeneziaVipProcess(state->commonProcessorLock);
		if (capture->callbacks[0].uid && (capture->callbacks[0].eventMask & 512)) {
			capture->callbacks[0].pendingEvents = capture->callbacks[0].eventMask & 512;
			sceKernelNotifyCallback(capture->callbacks[0].uid,
				capture->callbacks[0].pendingEvents);
		}
		if (capture->callbacks[1].uid && (capture->callbacks[1].eventMask & 512)) {
			capture->callbacks[1].pendingEvents = capture->callbacks[1].eventMask & 512;
			sceKernelNotifyCallback(capture->callbacks[1].uid,
				capture->callbacks[1].pendingEvents);
		}
		if (capture->callbacks[2].uid && (capture->callbacks[2].eventMask & 512)) {
			capture->callbacks[2].pendingEvents = capture->callbacks[2].eventMask & 512;
			sceKernelNotifyCallback(capture->callbacks[2].uid,
				capture->callbacks[2].pendingEvents);
		}
		if (capture->callbacks[3].uid && (capture->callbacks[3].eventMask & 512)) {
			capture->callbacks[3].pendingEvents = capture->callbacks[3].eventMask & 512;
			sceKernelNotifyCallback(capture->callbacks[3].uid,
				capture->callbacks[3].pendingEvents);
		}
		if (capture->callbacks[4].uid && (capture->callbacks[4].eventMask & 512)) {
			capture->callbacks[4].pendingEvents = capture->callbacks[4].eventMask & 512;
			sceKernelNotifyCallback(capture->callbacks[4].uid,
				capture->callbacks[4].pendingEvents);
		}
		if (capture->callbacks[5].uid && (capture->callbacks[5].eventMask & 512)) {
			capture->callbacks[5].pendingEvents = capture->callbacks[5].eventMask & 512;
			sceKernelNotifyCallback(capture->callbacks[5].uid,
				capture->callbacks[5].pendingEvents);
		}
		unlockVeneziaVipProcess(state->commonProcessorLock);
	}
	pending = state->processorPendingCommands[0];
acknowledge:
	if (pending < 0) {
		state->processorPendingCommands[0] = 0;
		unlockVeneziaVipProcess(state->commonProcessorLock);
		return sceKernelSetEventFlag(state->processorEvents[0], 0x100000);
	}
	unlockVeneziaVipProcess(state->commonProcessorLock);
	return 0;
}

/* name unknown: complete a secondary request and deliver capture recovery callbacks. */
int completeVeneziaVipSecondaryCaptureRequest(VeneziaVipState *state, int index,
	uint32_t flags, int *command)
{
	VeneziaVipProcessStorage *storage = state->activeProcess;
	VeneziaDebugProcessCapture *capture = index == -1 ? NULL : &storage->captures[index];
	uint32_t merged, event;
	VeneziaDebugProcessCapture *current;
	unsigned int slot;
	int pending;

	lockVeneziaVipProcess(state->commonProcessorLock);
	if (*command < 0)
		*command &= 0x7fffffff;
	pending = state->processorPendingCommands[1];
	if (pending < 0) {
		state->processorRequests[1].resultFlags = flags;
		if (capture) {
			merged = flags;
			if (capture->recoveryFlags & 6) {
				merged |= capture->recoveryFlags;
				state->processorRequests[1].resultFlags = merged;
			}
			capture->recoveryFlags = 0;
			if (merged & 6)
				goto notify;
			if (flags & 6)
				goto notify;
			goto acknowledge;
		}
	} else if (capture) {
		capture->recoveryFlags |= flags;
		if (flags & 6)
			goto notify;
		goto acknowledge;
	}
	if ((flags & 6) && storage->status[0]) {
		current = storage->captures;
		slot = 0;
		do {
			current->recoveryFlags |= 6;
			current++;
			slot++;
		} while (slot < storage->status[0]);
	}
	goto acknowledge;
notify:
	event = 0;
	sceKernelPollEventFlag(state->processorEvents[1], 0x2000000, 1, &event);
	if (event != 0x2000000) {
		sceKernelSetEventFlag(capture->eventFlag, 512);
		lockVeneziaVipProcess(state->commonProcessorLock);
		if (capture->callbacks[0].uid && (capture->callbacks[0].eventMask & 512)) {
			capture->callbacks[0].pendingEvents = capture->callbacks[0].eventMask & 512;
			sceKernelNotifyCallback(capture->callbacks[0].uid,
				capture->callbacks[0].pendingEvents);
		}
		if (capture->callbacks[1].uid && (capture->callbacks[1].eventMask & 512)) {
			capture->callbacks[1].pendingEvents = capture->callbacks[1].eventMask & 512;
			sceKernelNotifyCallback(capture->callbacks[1].uid,
				capture->callbacks[1].pendingEvents);
		}
		if (capture->callbacks[2].uid && (capture->callbacks[2].eventMask & 512)) {
			capture->callbacks[2].pendingEvents = capture->callbacks[2].eventMask & 512;
			sceKernelNotifyCallback(capture->callbacks[2].uid,
				capture->callbacks[2].pendingEvents);
		}
		if (capture->callbacks[3].uid && (capture->callbacks[3].eventMask & 512)) {
			capture->callbacks[3].pendingEvents = capture->callbacks[3].eventMask & 512;
			sceKernelNotifyCallback(capture->callbacks[3].uid,
				capture->callbacks[3].pendingEvents);
		}
		if (capture->callbacks[4].uid && (capture->callbacks[4].eventMask & 512)) {
			capture->callbacks[4].pendingEvents = capture->callbacks[4].eventMask & 512;
			sceKernelNotifyCallback(capture->callbacks[4].uid,
				capture->callbacks[4].pendingEvents);
		}
		if (capture->callbacks[5].uid && (capture->callbacks[5].eventMask & 512)) {
			capture->callbacks[5].pendingEvents = capture->callbacks[5].eventMask & 512;
			sceKernelNotifyCallback(capture->callbacks[5].uid,
				capture->callbacks[5].pendingEvents);
		}
		unlockVeneziaVipProcess(state->commonProcessorLock);
	}
	pending = state->processorPendingCommands[1];
acknowledge:
	if (pending < 0) {
		state->processorPendingCommands[1] = 0;
		unlockVeneziaVipProcess(state->commonProcessorLock);
		return sceKernelSetEventFlag(state->processorEvents[1], 0x100000);
	}
	unlockVeneziaVipProcess(state->commonProcessorLock);
	return 0;
}

/* name unknown: queue capture events and wake the selected processor worker. */
int notifyVeneziaVipProcessor(VeneziaVipState *state, int processor,
	unsigned int channel, uint32_t events)
{
	VeneziaVipProcessStorage *storage;
	VeneziaDebugProcessCapture *capture;

	if (processor == -1)
		return sceKernelSetEventFlag(state->processorEvents[channel], 0x20000);
	if (!events)
		return 0;
	lockVeneziaVipProcess(state->commonProcessorLock);
	storage = state->activeProcess;
	if (storage) {
		capture = &storage->captures[processor];
		enqueueVeneziaVipProcess(&storage->queues[channel], (uint8_t)processor);
		capture->pendingProcessorEvents[channel] |= events;
		unlockVeneziaVipProcess(state->commonProcessorLock);
		return sceKernelSetEventFlag(state->processorEvents[channel], 0x20000);
	}
	unlockVeneziaVipProcess(state->commonProcessorLock);
	return 0;
}

/* name unknown: clear capture state and recovery flags under the common lock. */
int clearVeneziaVipCaptureStatus(VeneziaVipState *state,
	VeneziaDebugProcessCapture *capture)
{
	uint8_t *captureState = (uint8_t *)capture->unidentifiedCaptureState;
	uint8_t *recoveryFlags = (uint8_t *)&capture->recoveryFlags;

	lockVeneziaVipProcess(state->commonProcessorLock);
	captureState[0] = 0;
	captureState[1] = 0;
	captureState[2] = 0;
	captureState[3] = 0;
	captureState[4] = 0;
	captureState[5] = 0;
	captureState[6] = 0;
	captureState[7] = 0;
	recoveryFlags[0] = 0;
	recoveryFlags[1] = 0;
	recoveryFlags[2] = 0;
	recoveryFlags[3] = 0;
	unlockVeneziaVipProcess(state->commonProcessorLock);
	return 0;
}

/* name unknown: wait for a processor request reply and optionally return its flags. */
int waitVeneziaVipProcessorResponse(VeneziaVipState *state, unsigned int processor,
	uint32_t *resultFlags)
{
	uint32_t event;
	int ret;

	ret = sceKernelWaitEventFlag(state->processorEvents[processor],
		0x100000, 5, &event, NULL);
	if (resultFlags) {
		lockVeneziaVipProcess(state->commonProcessorLock);
		*resultFlags = state->processorRequests[processor].resultFlags;
		unlockVeneziaVipProcess(state->commonProcessorLock);
	}
	return ret;
}

extern int interruptVeneziaVipCommandLocked(VeneziaVipState *state,
	unsigned int processor);

/* name unknown: interrupt stalled processors and restart capture configuration. */
int recoverVeneziaVipProcessors(VeneziaVipState *state,
	VeneziaVipProcessStorage *storage)
{
	VeneziaDebugProcessCapture *capture;
	unsigned int recovered[2] = { 0, 0 };
	unsigned int processor, status, nextStatus;
	int index, count, ret, pollRet;

	for (processor = 0; processor < 2; processor++) {
		ret = getVeneziaVipProcessorStatusLocked(state, processor, &status) == 0 ?
			0 : -2141060856;
		if (status != 6)
			continue;
		recovered[processor] = 1;
		if (interruptVeneziaVipCommandLocked(state, processor) == 0) {
			for (count = 100; count; count--) {
				pollRet = getVeneziaVipProcessorStatusLocked(state, processor, &nextStatus);
				if (g_veneziaDebugger.log)
					g_veneziaDebugger.log(59, 5, processor, nextStatus, 0, 0, pollRet);
				if (pollRet != 0 || nextStatus != 6)
					break;
				sceKernelDelayThread(1000);
			}
		}
		sceKernelClearEventFlag(state->captureInterruptEvents[processor], 0);
		index = state->processorActiveCaptures[processor];
		if (index == -1)
			continue;
		if (processor == 1 && recovered[0] == 1)
			goto removePrimary;
		capture = &storage->captures[index];
		sceKernelSetEventFlag(capture->eventFlag, 512);
		lockVeneziaVipProcess(state->commonProcessorLock);
		if (capture->callbacks[0].uid && (capture->callbacks[0].eventMask & 512)) {
			capture->callbacks[0].pendingEvents = capture->callbacks[0].eventMask & 512;
			sceKernelNotifyCallback(capture->callbacks[0].uid,
				capture->callbacks[0].pendingEvents);
		}
		if (capture->callbacks[1].uid && (capture->callbacks[1].eventMask & 512)) {
			capture->callbacks[1].pendingEvents = capture->callbacks[1].eventMask & 512;
			sceKernelNotifyCallback(capture->callbacks[1].uid,
				capture->callbacks[1].pendingEvents);
		}
		if (capture->callbacks[2].uid && (capture->callbacks[2].eventMask & 512)) {
			capture->callbacks[2].pendingEvents = capture->callbacks[2].eventMask & 512;
			sceKernelNotifyCallback(capture->callbacks[2].uid,
				capture->callbacks[2].pendingEvents);
		}
		if (capture->callbacks[3].uid && (capture->callbacks[3].eventMask & 512)) {
			capture->callbacks[3].pendingEvents = capture->callbacks[3].eventMask & 512;
			sceKernelNotifyCallback(capture->callbacks[3].uid,
				capture->callbacks[3].pendingEvents);
		}
		if (capture->callbacks[4].uid && (capture->callbacks[4].eventMask & 512)) {
			capture->callbacks[4].pendingEvents = capture->callbacks[4].eventMask & 512;
			sceKernelNotifyCallback(capture->callbacks[4].uid,
				capture->callbacks[4].pendingEvents);
		}
		if (capture->callbacks[5].uid && (capture->callbacks[5].eventMask & 512)) {
			capture->callbacks[5].pendingEvents = capture->callbacks[5].eventMask & 512;
			sceKernelNotifyCallback(capture->callbacks[5].uid,
				capture->callbacks[5].pendingEvents);
		}
		unlockVeneziaVipProcess(state->commonProcessorLock);
		capture->flags |= 7;
	}
	if (recovered[0] == 1)
		goto removePrimary;
	if (recovered[1] == 1)
		goto removeSecondary;
	goto restart;
removePrimary:
	removeVeneziaVipProcess(&storage->queues[0], (uint8_t)state->processorActiveCaptures[0]);
	removeVeneziaVipProcess(&storage->queues[1], (uint8_t)state->processorActiveCaptures[0]);
	if (recovered[1] != 1)
		goto restart;
removeSecondary:
	removeVeneziaVipProcess(&storage->queues[0], (uint8_t)state->processorActiveCaptures[1]);
	removeVeneziaVipProcess(&storage->queues[1], (uint8_t)state->processorActiveCaptures[1]);
restart:
	stopVeneziaVipDebugger(state, storage);
	sceKernelClearEventFlag(state->captureInterruptEvents[0], 0);
	sceKernelClearEventFlag(state->captureInterruptEvents[1], 0);
	beginVeneziaVipCaptureConfiguration(state, storage);
	prepareVeneziaVipCapture(state, storage);
	sceKernelClearEventFlag(state->eventFlag, 0);
	sceKernelSetEventFlag(state->processorEvents[0], 0x4000000);
	sceKernelSetEventFlag(state->processorEvents[1], 0x4000000);
	return ret;
}

/* name unknown: submit context preparation to both processor workers. */
int prepareVeneziaVipProcessorContext(unsigned int processor, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage)
{
	uint32_t event, response;
	unsigned int channel;
	int ret;

	lockVeneziaVipProcess(state->requestLock);
	do {
		event = 0;
		sceKernelPollEventFlag(state->eventFlag, 0x2000000, 1, &event);
		if (event & 0x2000000)
			recoverVeneziaVipProcessors(state, storage);
		for (channel = 0; channel < 2; channel++) {
			lockVeneziaVipProcess(state->commonProcessorLock);
			state->processorRequests[channel].captureIndex = processor;
			state->processorRequests[channel].command = 0x80000010;
			unlockVeneziaVipProcess(state->commonProcessorLock);
			sceKernelClearEventFlag(state->processorEvents[channel], ~0x100000u);
			ret = sceKernelSetEventFlag(state->processorEvents[channel], 0x10000);
			if (ret < 0)
				break;
			ret = sceKernelWaitEventFlag(state->processorEvents[channel],
				0x100000, 5, &response, NULL);
			lockVeneziaVipProcess(state->commonProcessorLock);
			event = state->processorRequests[channel].resultFlags;
			unlockVeneziaVipProcess(state->commonProcessorLock);
			if (ret < 0)
				break;
			if (event & 2)
				goto retry;
		}
		if (channel == 2 || ret != -2141060856)
			goto out;
retry:
		event = 0;
		sceKernelPollEventFlag(state->eventFlag, 0x2000000, 1, &event);
	} while (event & 0x2000000);
	ret = -2141060856;
out:
	unlockVeneziaVipProcess(state->requestLock);
	return ret;
}

/* name unknown: post queue configuration to both workers before awaiting replies. */
int configureVeneziaVipProcessorQueues(unsigned int processor, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage, uint32_t flags)
{
	uint32_t event, response;
	unsigned int channel;
	uint32_t command;
	int ret;

	lockVeneziaVipProcess(state->requestLock);
	do {
		event = 0;
		sceKernelPollEventFlag(state->eventFlag, 0x2000000, 1, &event);
		if (event & 0x2000000)
			recoverVeneziaVipProcessors(state, storage);
		for (channel = 0; channel < 2; channel++) {
			command = flags & 0x1000000 ? 0xc0000020 : 0x80000020;
			lockVeneziaVipProcess(state->commonProcessorLock);
			state->processorRequests[channel].captureIndex = processor;
			state->processorRequests[channel].command = command;
			unlockVeneziaVipProcess(state->commonProcessorLock);
			sceKernelClearEventFlag(state->processorEvents[channel], ~0x100000u);
			ret = sceKernelSetEventFlag(state->processorEvents[channel], 0x10000);
			if (ret < 0)
				goto failed;
		}
		for (channel = 0; channel < 2; channel++) {
			ret = sceKernelWaitEventFlag(state->processorEvents[channel],
				0x100000, 5, &response, NULL);
			lockVeneziaVipProcess(state->commonProcessorLock);
			event = state->processorRequests[channel].resultFlags;
			unlockVeneziaVipProcess(state->commonProcessorLock);
			if (ret < 0)
				goto failed;
			if (event & 2)
				goto retry;
		}
		goto out;
failed:
		if (ret != -2141060856)
			goto out;
retry:
		event = 0;
		sceKernelPollEventFlag(state->eventFlag, 0x2000000, 1, &event);
	} while (event & 0x2000000);
	ret = -2141060856;
out:
	unlockVeneziaVipProcess(state->requestLock);
	return ret;
}

/* name unknown: start the prepared capture context on both processors. */
int startVeneziaVipProcessorContext(unsigned int processor, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage)
{
	uint32_t event, response;
	unsigned int channel;
	int ret;

	lockVeneziaVipProcess(state->requestLock);
	do {
		event = 0;
		sceKernelPollEventFlag(state->eventFlag, 0x2000000, 1, &event);
		if (event & 0x2000000)
			recoverVeneziaVipProcessors(state, storage);
		for (channel = 0; channel < 2; channel++) {
			lockVeneziaVipProcess(state->commonProcessorLock);
			state->processorRequests[channel].captureIndex = processor;
			state->processorRequests[channel].command = 0x80000040;
			unlockVeneziaVipProcess(state->commonProcessorLock);
			sceKernelClearEventFlag(state->processorEvents[channel], ~0x100000u);
			ret = sceKernelSetEventFlag(state->processorEvents[channel], 0x10000);
			if (ret < 0)
				break;
			ret = sceKernelWaitEventFlag(state->processorEvents[channel],
				0x100000, 5, &response, NULL);
			lockVeneziaVipProcess(state->commonProcessorLock);
			event = state->processorRequests[channel].resultFlags;
			unlockVeneziaVipProcess(state->commonProcessorLock);
			if (ret < 0)
				break;
			if (event & 2)
				goto retry;
		}
		if (channel == 2 || ret != -2141060856)
			goto out;
retry:
		event = 0;
		sceKernelPollEventFlag(state->eventFlag, 0x2000000, 1, &event);
	} while (event & 0x2000000);
	ret = -2141060856;
out:
	unlockVeneziaVipProcess(state->requestLock);
	return ret;
}

/* name unknown: send the original capture request to both processor workers. */
int detachVeneziaVipCaptureContext(unsigned int processor, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage)
{
	uint32_t event, response;
	unsigned int channel;
	int ret;

	lockVeneziaVipProcess(state->requestLock);
	do {
		event = 0;
		sceKernelPollEventFlag(state->eventFlag, 0x2000000, 1, &event);
		if (event & 0x2000000)
			recoverVeneziaVipProcessors(state, storage);
		for (channel = 0; channel < 2; channel++) {
			lockVeneziaVipProcess(state->commonProcessorLock);
			state->processorRequests[channel].captureIndex = processor;
			state->processorRequests[channel].command = 0x80000080;
			unlockVeneziaVipProcess(state->commonProcessorLock);
			sceKernelClearEventFlag(state->processorEvents[channel], ~0x100000u);
			ret = sceKernelSetEventFlag(state->processorEvents[channel], 0x10000);
			if (ret < 0)
				break;
			ret = sceKernelWaitEventFlag(state->processorEvents[channel],
				0x100000, 5, &response, NULL);
			lockVeneziaVipProcess(state->commonProcessorLock);
			event = state->processorRequests[channel].resultFlags;
			unlockVeneziaVipProcess(state->commonProcessorLock);
			if (ret < 0)
				break;
			if (event & 2)
				goto retry;
		}
		if (channel == 2 || ret != -2141060856)
			goto out;
retry:
		event = 0;
		sceKernelPollEventFlag(state->eventFlag, 0x2000000, 1, &event);
	} while (event & 0x2000000);
	ret = -2141060856;
out:
	unlockVeneziaVipProcess(state->requestLock);
	return ret;
}

/* name unknown: send the original capture request to both processor workers. */
int beginVeneziaVipCaptureSetup(unsigned int processor, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage)
{
	uint32_t event, response;
	unsigned int channel;
	int ret;

	lockVeneziaVipProcess(state->requestLock);
	do {
		event = 0;
		sceKernelPollEventFlag(state->eventFlag, 0x2000000, 1, &event);
		if (event & 0x2000000)
			recoverVeneziaVipProcessors(state, storage);
		for (channel = 0; channel < 2; channel++) {
			lockVeneziaVipProcess(state->commonProcessorLock);
			state->processorRequests[channel].captureIndex = processor;
			state->processorRequests[channel].command = 0x80020000;
			unlockVeneziaVipProcess(state->commonProcessorLock);
			sceKernelClearEventFlag(state->processorEvents[channel], ~0x100000u);
			ret = sceKernelSetEventFlag(state->processorEvents[channel], 0x10000);
			if (ret < 0)
				break;
			ret = sceKernelWaitEventFlag(state->processorEvents[channel],
				0x100000, 5, &response, NULL);
			lockVeneziaVipProcess(state->commonProcessorLock);
			event = state->processorRequests[channel].resultFlags;
			unlockVeneziaVipProcess(state->commonProcessorLock);
			if (ret < 0)
				break;
			if (event & 2)
				goto retry;
		}
		if (channel == 2 || ret != -2141060856)
			goto out;
retry:
		event = 0;
		sceKernelPollEventFlag(state->eventFlag, 0x2000000, 1, &event);
	} while (event & 0x2000000);
	ret = -2141060856;
out:
	unlockVeneziaVipProcess(state->requestLock);
	return ret;
}

/* name unknown: send the original capture request to both processor workers. */
int endVeneziaVipCaptureSetup(unsigned int processor, VeneziaVipState *state,
	VeneziaVipProcessStorage *storage)
{
	uint32_t event, response;
	unsigned int channel;
	int ret;

	lockVeneziaVipProcess(state->requestLock);
	do {
		event = 0;
		sceKernelPollEventFlag(state->eventFlag, 0x2000000, 1, &event);
		if (event & 0x2000000)
			recoverVeneziaVipProcessors(state, storage);
		for (channel = 0; channel < 2; channel++) {
			lockVeneziaVipProcess(state->commonProcessorLock);
			state->processorRequests[channel].captureIndex = processor;
			state->processorRequests[channel].command = 0x80040000;
			unlockVeneziaVipProcess(state->commonProcessorLock);
			sceKernelClearEventFlag(state->processorEvents[channel], ~0x100000u);
			ret = sceKernelSetEventFlag(state->processorEvents[channel], 0x10000);
			if (ret < 0)
				break;
			ret = sceKernelWaitEventFlag(state->processorEvents[channel],
				0x100000, 5, &response, NULL);
			lockVeneziaVipProcess(state->commonProcessorLock);
			event = state->processorRequests[channel].resultFlags;
			unlockVeneziaVipProcess(state->commonProcessorLock);
			if (ret < 0)
				break;
			if (event & 2)
				goto retry;
		}
		if (channel == 2 || ret != -2141060856)
			goto out;
retry:
		event = 0;
		sceKernelPollEventFlag(state->eventFlag, 0x2000000, 1, &event);
	} while (event & 0x2000000);
	ret = -2141060856;
out:
	unlockVeneziaVipProcess(state->requestLock);
	return ret;
}

/* name unknown: stop both processor workers with capture recovery. */
int stopVeneziaVipProcessors(VeneziaVipState *state,
	VeneziaVipProcessStorage *storage)
{
	uint32_t event, response;
	unsigned int channel;
	int ret;

	lockVeneziaVipProcess(state->requestLock);
	do {
		event = 0;
		sceKernelPollEventFlag(state->eventFlag, 0x2000000, 1, &event);
		if (event & 0x2000000)
			recoverVeneziaVipProcessors(state, storage);
		for (channel = 0; channel < 2; channel++) {
			lockVeneziaVipProcess(state->commonProcessorLock);
			state->processorRequests[channel].captureIndex = -1;
			state->processorRequests[channel].command = 0x80000100;
			unlockVeneziaVipProcess(state->commonProcessorLock);
			sceKernelClearEventFlag(state->processorEvents[channel], ~0x100000u);
			ret = sceKernelSetEventFlag(state->processorEvents[channel], 0x10000);
			if (ret < 0)
				break;
			ret = sceKernelWaitEventFlag(state->processorEvents[channel],
				0x100000, 5, &response, NULL);
			lockVeneziaVipProcess(state->commonProcessorLock);
			event = state->processorRequests[channel].resultFlags;
			unlockVeneziaVipProcess(state->commonProcessorLock);
			if (ret < 0)
				break;
			if (event & 2)
				goto retry;
		}
		if (channel == 2 || ret != -2141060856)
			goto out;
retry:
		event = 0;
		sceKernelPollEventFlag(state->eventFlag, 0x2000000, 1, &event);
	} while (event & 0x2000000);
	ret = -2141060856;
out:
	unlockVeneziaVipProcess(state->requestLock);
	return ret;
}

/* name unknown: request both processor controls with capture recovery. */
int requestVeneziaVipProcessorControl(VeneziaVipState *state,
	VeneziaVipProcessStorage *storage)
{
	uint32_t event, response;
	unsigned int channel;
	int ret;

	lockVeneziaVipProcess(state->requestLock);
	do {
		event = 0;
		sceKernelPollEventFlag(state->eventFlag, 0x2000000, 1, &event);
		if (event & 0x2000000)
			recoverVeneziaVipProcessors(state, storage);
		for (channel = 0; channel < 2; channel++) {
			lockVeneziaVipProcess(state->commonProcessorLock);
			state->processorRequests[channel].captureIndex = -1;
			state->processorRequests[channel].command = 0x80000200;
			unlockVeneziaVipProcess(state->commonProcessorLock);
			sceKernelClearEventFlag(state->processorEvents[channel], ~0x100000u);
			ret = sceKernelSetEventFlag(state->processorEvents[channel], 0x10000);
			if (ret < 0)
				break;
			ret = sceKernelWaitEventFlag(state->processorEvents[channel],
				0x100000, 5, &response, NULL);
			lockVeneziaVipProcess(state->commonProcessorLock);
			event = state->processorRequests[channel].resultFlags;
			unlockVeneziaVipProcess(state->commonProcessorLock);
			if (ret < 0)
				break;
			if (event & 2)
				goto retry;
		}
		if (channel == 2 || ret != -2141060856)
			goto out;
retry:
		event = 0;
		sceKernelPollEventFlag(state->eventFlag, 0x2000000, 1, &event);
	} while (event & 0x2000000);
	ret = -2141060856;
out:
	unlockVeneziaVipProcess(state->requestLock);
	return ret;
}

/* name unknown: start both processor workers and check their responses. */
int startVeneziaVipProcessors(VeneziaVipState *state)
{
	uint32_t response, event;
	unsigned int channel;
	int ret;

	lockVeneziaVipProcess(state->requestLock);
	for (channel = 0; channel < 2; channel++) {
		lockVeneziaVipProcess(state->commonProcessorLock);
		state->processorRequests[channel].captureIndex = -1;
		state->processorRequests[channel].command = 0x80000400;
		unlockVeneziaVipProcess(state->commonProcessorLock);
		sceKernelClearEventFlag(state->processorEvents[channel], ~0x100000u);
		ret = sceKernelSetEventFlag(state->processorEvents[channel], 0x10000);
		if (ret < 0)
			break;
		ret = sceKernelWaitEventFlag(state->processorEvents[channel],
			0x100000, 5, &response, NULL);
		lockVeneziaVipProcess(state->commonProcessorLock);
		event = state->processorRequests[channel].resultFlags;
		unlockVeneziaVipProcess(state->commonProcessorLock);
		if (ret < 0)
			break;
		if (event & 2) {
			ret = -2141060856;
			break;
		}
	}
	unlockVeneziaVipProcess(state->requestLock);
	return ret;
}

/* name unknown: post capture processing to the first worker and await both. */
int requestVeneziaVipCaptureProcessing(unsigned int processor,
	VeneziaVipState *state, unsigned int mode)
{
	uint32_t response, event, command;
	unsigned int channel;
	int ret;

	lockVeneziaVipProcess(state->requestLock);
	command = mode ? 0xa0001000 : 0x80001000;
	lockVeneziaVipProcess(state->commonProcessorLock);
	state->processorRequests[0].captureIndex = processor;
	state->processorRequests[0].command = command;
	unlockVeneziaVipProcess(state->commonProcessorLock);
	sceKernelClearEventFlag(state->processorEvents[0], ~0x100000u);
	ret = sceKernelSetEventFlag(state->processorEvents[0], 0x10000);
	if (ret >= 0) {
		for (channel = 0; channel < 2; channel++) {
			ret = sceKernelWaitEventFlag(state->processorEvents[channel],
				0x100000, 5, &response, NULL);
			lockVeneziaVipProcess(state->commonProcessorLock);
			event = state->processorRequests[channel].resultFlags;
			unlockVeneziaVipProcess(state->commonProcessorLock);
			if (ret < 0)
				break;
			if (event & 4) {
				ret = -2141060856;
				break;
			}
		}
	}
	unlockVeneziaVipProcess(state->requestLock);
	return ret;
}

/* name unknown: getVeneziaVipActiveProcess follows the original capture accessor. */
__attribute__((noinline)) int getVeneziaVipActiveProcess(VeneziaVipState *state,
	VeneziaVipProcessStorage **storage)
{
	lockVeneziaVipProcess(state->commonProcessorLock);
	*storage = state->activeProcess;
	unlockVeneziaVipProcess(state->commonProcessorLock);
	return 0;
}

/* name unknown: getVeneziaVipCaptureProcessorStatus follows the original capture accessor. */
int getVeneziaVipCaptureProcessorStatus(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, unsigned int processor, uint32_t *status)
{
	lockVeneziaVipProcess(state->commonProcessorLock);
	*status = capture->processorContextState[processor];
	unlockVeneziaVipProcess(state->commonProcessorLock);
	return 0;
}

/* name unknown: getVeneziaVipCaptureContext follows the original capture accessor. */
__attribute__((noinline))
int getVeneziaVipCaptureContext(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, VeneziaVipCommandContext **context)
{
	lockVeneziaVipProcess(state->commonProcessorLock);
	*context = capture->commandContext;
	unlockVeneziaVipProcess(state->commonProcessorLock);
	return 0;
}

/* name unknown: setVeneziaVipCaptureContext follows the original capture accessor. */
__attribute__((noinline)) int setVeneziaVipCaptureContext(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, VeneziaVipCommandContext *context)
{
	lockVeneziaVipProcess(state->commonProcessorLock);
	capture->commandContext = context;
	unlockVeneziaVipProcess(state->commonProcessorLock);
	return 0;
}

/* name unknown: setVeneziaVipCaptureControl follows the original capture accessor. */
__attribute__((noinline)) int setVeneziaVipCaptureControl(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, uint32_t control)
{
	lockVeneziaVipProcess(state->commonProcessorLock);
	capture->unidentifiedBeforeCaptureMemory = control;
	unlockVeneziaVipProcess(state->commonProcessorLock);
	return 0;
}

/* name unknown: getVeneziaVipCaptureCallbacks follows the original capture accessor. */
__attribute__((noinline)) int getVeneziaVipCaptureCallbacks(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, VeneziaDebugCallback **callbacks)
{
	*callbacks = capture->callbacks;
	return 0;
}

extern int getVeneziaVipCommandInputSpaceLocked(VeneziaVipState *state,
	VeneziaVipCommandContext *context, SceSize *space);

/* name unknown: deduct capture input awaiting commit from command input space. */
int getVeneziaVipCaptureInputCapacity(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, VeneziaVipCommandContext *context, SceSize *available)
{
	uint32_t commit, write;
	int ret;

	lockVeneziaVipProcess(state->standardBufferLock);
	ret = getVeneziaVipCommandInputSpaceLocked(state, context, available);
	if (g_veneziaDebugger.log)
		g_veneziaDebugger.log(72, 3, context, *available, ret);
	if (!ret) {
		commit = capture->ringCommitAddress;
		write = capture->ringWriteAddress;
		if (write > commit)
			commit = commit + capture->ringEndAddress - capture->ringReadAddress;
		*available -= commit - write;
	}
	unlockVeneziaVipProcess(state->standardBufferLock);
	return ret;
}

/* name unknown: update capture ring state under its standard-buffer lock. */
__attribute__((noinline)) int getVeneziaVipCaptureWriteCursor(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, VeneziaVipCommandContext *context, uint32_t *cursor)
{
	lockVeneziaVipProcess(state->standardBufferLock);
	*cursor = capture->ringCommitAddress;
	unlockVeneziaVipProcess(state->standardBufferLock);
	return 0;
}

/* name unknown: update capture ring state under its standard-buffer lock. */
__attribute__((noinline)) int setVeneziaVipCaptureWriteCursor(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, VeneziaVipCommandContext *context, uint32_t cursor)
{
	lockVeneziaVipProcess(state->standardBufferLock);
	capture->ringCommitAddress = cursor;
	if (cursor == capture->ringReadAddress)
		capture->unidentifiedRingPendingCount |= 1;
	unlockVeneziaVipProcess(state->standardBufferLock);
	return 0;
}

/* name unknown: update capture ring state under its standard-buffer lock. */
int endVeneziaVipCaptureInput(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, VeneziaVipCommandContext *context)
{
	lockVeneziaVipProcess(state->standardBufferLock);
	capture->unidentifiedRingPendingCount |= 2;
	capture->unidentifiedRingResult |= capture->unidentifiedRingPendingCount;
	unlockVeneziaVipProcess(state->standardBufferLock);
	return 0;
}

/* name unknown: publish the capture ring cursor and pending input flags. */
int commitVeneziaVipCaptureInput(VeneziaDebugProcessCapture *capture,
	VeneziaVipState *state, VeneziaVipCommandContext *context)
{
	lockVeneziaVipProcess(state->standardBufferLock);
	capture->ringPendingAddress = capture->ringCommitAddress;
	capture->unidentifiedRingResult |= capture->unidentifiedRingPendingCount;
	unlockVeneziaVipProcess(state->standardBufferLock);
	return 0;
}

extern int submitVeneziaVipCommandContextLocked(VeneziaVipState *state,
	unsigned int processor, VeneziaVipCommandContext *context);
extern int configureVeneziaVipCommandCaptureLocked(VeneziaVipState *state,
	unsigned int processor, VeneziaVipCommandContext *context);
extern int requestVeneziaVipCommandStopLocked(VeneziaVipState *state,
	unsigned int processor);
extern int getVeneziaVipCommandInputBlockedLocked(VeneziaVipState *state,
	VeneziaVipCommandContext *context);
extern int setVeneziaVipCommandInputOffsetLocked(VeneziaVipState *state,
	VeneziaVipCommandContext *context, SceSize offset);
extern int blockVeneziaVipCommandInputLocked(VeneziaVipState *state,
	VeneziaVipCommandContext *context);
extern int requestVeneziaVipCommandInputActivityLocked(VeneziaVipState *state,
	VeneziaVipCommandContext *context);
extern int getVeneziaVipCommandInputPendingLocked(VeneziaVipState *state,
	VeneziaVipCommandContext *context);
extern int startVeneziaVipCommandCaptureLocked(VeneziaVipState *state,
	unsigned int processor, VeneziaVipCommandContext *context, unsigned int mode);

/* name unknown: service the original primary processor command/event loop. */
int veneziaVipProcessorThread0(SceSize size, void *arg)
{
	VeneziaVipState *state = *(VeneziaVipState **)arg;
	VeneziaVipProcessStorage *storage, *active;
	VeneziaDebugProcessCapture *capture;
	VeneziaVipCommandContext *context, *startContext;
	uint32_t events, deferred, response, status, interruptEvents;
	uint32_t contextState, secondaryState, completionCount, secondaryCount;
	uint32_t flags, pending, cursor, queued, forwardCommand, signedRequest;
	SceSize available;
	uint8_t value;
	int command = 0, captureIndex = -1, recovering, ret, inputState, inputResult;

	ret = sceKernelSetEventFlag(state->processorEvents[0], 0x100000);
	if (ret < 0)
		return ret;
	for (;;) {
		deferred = 0;
		recovering = 0;
		events = 0;
receive:
		command = 0;
		if (recovering == 1)
			goto recoveryWait;
		sceKernelWaitEventFlag(state->processorEvents[0], 0x1030000, 5,
			&events, NULL);
		lockVeneziaVipProcess(state->commonProcessorLock);
		storage = state->activeProcess;
		if (!storage)
			goto noProcess;
		if (events & 0x1000000) {
			state->processorRecoveryEvents[0] = events;
			unlockVeneziaVipProcess(state->commonProcessorLock);
			sceKernelWaitEventFlag(state->processorEvents[0], 0x2000000, 1,
				&response, NULL);
			lockVeneziaVipProcess(state->commonProcessorLock);
			sceKernelSetEventFlag(state->processorEvents[1], 0x2000000);
			sceKernelSetEventFlag(state->eventFlag, 0x2000000);
			deferred = events & 0x20000;
			if ((events & 0x10000) || (int)state->processorRequests[0].command < 0 ||
			    (int)state->processorActiveCommands[0] < 0)
				sceKernelSetEventFlag(state->processorEvents[0], 0x10000);
			recovering = 1;
			unlockVeneziaVipProcess(state->commonProcessorLock);
recoveryWait:
			command = 0;
			sceKernelWaitEventFlag(state->processorEvents[0], 0x4010000, 5,
				&events, NULL);
			lockVeneziaVipProcess(state->commonProcessorLock);
			if (!state->activeProcess)
				goto noProcess;
			if (events & 0x4000000) {
				sceKernelSetEventFlag(state->processorEvents[0],
					deferred | (events & 0x10000));
				deferred = 0;
				state->processorActiveCaptures[0] = -1;
				state->processorActiveEvents[0] = 0;
				recovering = 0;
				state->processorActiveCommands[0] = 0;
				sceKernelClearEventFlag(state->processorEvents[0], ~0x6000000u);
			} else if ((events & 0x10000) &&
				   ((int)state->processorRequests[0].command < 0 ||
				    (int)state->processorActiveCommands[0] < 0)) {
				state->processorRequests[0].resultFlags = 6;
				sceKernelSetEventFlag(state->processorEvents[0], 0x100000);
			}
			unlockVeneziaVipProcess(state->commonProcessorLock);
			goto receive;
		}
		if (state->unidentifiedProcessorControls[0] == 1 || storage->pendingCounters[0] == 1)
			events &= ~0x20000u;
		queued = events & 0x10000;
		if (queued) {
			command = state->processorRequests[0].command;
			captureIndex = state->processorRequests[0].captureIndex;
			if ((unsigned int)captureIndex > 3)
				captureIndex = -1;
			if (command & 0x20000) {
				events &= ~0x20000u;
				state->unidentifiedProcessorControls[0] = 1;
			}
			if (command & 0x40000) {
				state->unidentifiedProcessorControls[0] = 0;
				if (storage->pendingCounters[0] != 1 &&
				    getVeneziaVipProcessQueueCount(&storage->queues[0]) > 0)
					events |= 0x20000;
			}
			if ((command & 0x400) && getVeneziaVipProcessQueueCount(&storage->queues[0]) > 0)
				events |= 0x20000;
			if (events & 0x20000)
				sceKernelSetEventFlag(state->processorEvents[0], 0x20000);
		} else if (events & 0x20000) {
			captureIndex = -1;
			if (!dequeueVeneziaVipProcess(&storage->queues[0], &value)) {
				captureIndex = value;
				queued = storage->captures[value].pendingProcessorEvents[0];
				storage->captures[value].pendingProcessorEvents[0] = 0;
			}
			command = queued;
			if (getVeneziaVipProcessQueueCount(&storage->queues[0]) > 0)
				sceKernelSetEventFlag(state->processorEvents[0], 0x20000);
		}
		state->processorPendingCommands[0] = command < 0 ? 0x80000000 : 0;
		if ((command & 0x90) && captureIndex != -1)
			removeVeneziaVipProcess(&storage->queues[0], (uint8_t)captureIndex);
		unlockVeneziaVipProcess(state->commonProcessorLock);
		recovering = events;
		if (!events)
			goto receive;
		if (!command) {
			recovering = 0;
			goto receive;
		}
		storage = state->activeProcess;
		state->processorActiveCaptures[0] = captureIndex;
		state->processorActiveEvents[0] = events;
		state->processorActiveCommands[0] = command;
		if (!storage)
			goto invalidCapture;
		if (captureIndex == -1) {
			capture = NULL;
			if ((command & 0xf9f8f0) || (int)storage->status[0] < 0)
				goto invalidCapture;
		} else {
			if ((int)storage->status[0] <= captureIndex)
				goto invalidCapture;
			capture = &storage->captures[captureIndex];
		}
		if (storage->pendingCounters[0] == 1) {
			flags = command & 0x60580;
			command &= 0xff06058f;
			if (!flags) {
				sceKernelClearEventFlag(state->processorEvents[0], ~0x7030000u);
				completeVeneziaVipCaptureRequest(state, captureIndex, 2, &command);
				continue;
			}
		}
		if (command & 0x10) {
			getVeneziaVipCaptureContext(capture, state, &context);
			ret = submitVeneziaVipCommandContextLocked(state, 0, context);
			if (g_veneziaDebugger.log)
				g_veneziaDebugger.log(82, 3, 0, context, ret);
			if (ret || waitVeneziaVipPrimaryCaptureInterrupt(capture, state, NULL, NULL) < 0) {
				completeVeneziaVipCaptureRequest(state, captureIndex, 2, &command);
			} else {
				lockVeneziaVipProcess(state->commonProcessorLock);
				capture->processorCompletionCounts[0] = 0;
				unlockVeneziaVipProcess(state->commonProcessorLock);
				lockVeneziaVipProcess(state->commonProcessorLock);
				capture->processorPendingCounts[0] = 0;
				unlockVeneziaVipProcess(state->commonProcessorLock);
				lockVeneziaVipProcess(state->commonProcessorLock);
				capture->queuedAuxiliaryCount = 0;
				unlockVeneziaVipProcess(state->commonProcessorLock);
				completeVeneziaVipCaptureRequest(state, captureIndex, 1, &command);
			}
		}
		if (command & 0x20) {
			lockVeneziaVipProcess(state->commonProcessorLock);
			unlockVeneziaVipProcess(state->commonProcessorLock);
			completeVeneziaVipCaptureRequest(state, captureIndex, 1, &command);
		}
		if (command & 0x40) {
			getVeneziaVipCaptureContext(capture, state, &context);
			ret = configureVeneziaVipCommandCaptureLocked(state, 0, context);
			if (g_veneziaDebugger.log)
				g_veneziaDebugger.log(84, 2, 0, ret);
			if (ret || waitVeneziaVipPrimaryCaptureInterrupt(capture, state, NULL, NULL) < 0)
				completeVeneziaVipCaptureRequest(state, captureIndex, 2, &command);
			else
				completeVeneziaVipCaptureRequest(state, captureIndex, 1, &command);
		}
		if (command & 0x80)
			completeVeneziaVipCaptureRequest(state, captureIndex, 1, &command);
		if (command & 0x100) {
			ret = getVeneziaVipProcessorStatusLocked(state, 0, &status);
			if (!ret && status != 1) {
				ret = requestVeneziaVipCommandStopLocked(state, 0);
				if (g_veneziaDebugger.log)
					g_veneziaDebugger.log(85, 2, 0, ret);
				if (!ret) {
					ret = waitVeneziaVipCaptureInterrupt(NULL, state, 0, 3, NULL, NULL);
					if (ret >= 0)
						ret = 0;
				}
			}
			sceKernelClearEventFlag(state->processorEvents[0], ~0x7030000u);
			completeVeneziaVipCaptureRequest(state, captureIndex, ret ? 2 : 1, &command);
		}
		if (command & 0x200) {
			sceKernelClearEventFlag(state->processorEvents[0], ~0x7030000u);
			storage->pendingCounters[0] = 1;
			completeVeneziaVipCaptureRequest(state, captureIndex, 1, &command);
		}
		if (command & 0x400) {
			storage->pendingCounters[0] = 0;
			completeVeneziaVipCaptureRequest(state, captureIndex, 1, &command);
		}
		if (command & 0x20000)
			completeVeneziaVipCaptureRequest(state, captureIndex, 1, &command);
		if (command & 0x40000)
			completeVeneziaVipCaptureRequest(state, captureIndex, 1, &command);
		if (!(command & 0x1000))
			continue;
		getVeneziaVipCaptureProcessorStatus(capture, state, 0, &contextState);
		getVeneziaVipCaptureContext(capture, state, &context);
		if (!context)
			continue;
		if (contextState != 2 && contextState != 4)
			goto captureFailure;
		lockVeneziaVipProcess(state->standardBufferLock);
		inputState = getVeneziaVipCommandInputBlockedLocked(state, context);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(73, 2, context, inputState);
		if (inputState != 3) {
			if (!inputState) {
				unlockVeneziaVipProcess(state->standardBufferLock);
				goto checkCompletion;
			}
			if (inputState == 2 && (capture->unidentifiedRingResult & 0x200)) {
				getVeneziaVipCaptureProcessorStatus(capture, state, 1, &secondaryState);
				if (secondaryState == 4)
					capture->unidentifiedRingResult &= ~0x200u;
				inputResult = 1;
			} else {
				inputResult = -2141060856;
			}
			goto inputDone;
		}
		inputResult = 0;
		if (capture->unidentifiedRingResult & 1) {
			ret = setVeneziaVipCommandInputOffsetLocked(state, context, capture->ringReadAddress);
			if (g_veneziaDebugger.log)
				g_veneziaDebugger.log(67, 3, context, capture->ringReadAddress, ret);
			if (ret)
				goto inputFailure;
			inputResult = 1;
		}
		cursor = capture->ringPendingAddress;
		if (cursor != capture->ringWriteAddress) {
			ret = setVeneziaVipCommandInputOffsetLocked(state, context, cursor);
			if (g_veneziaDebugger.log)
				g_veneziaDebugger.log(67, 3, context, capture->ringPendingAddress, ret);
			if (ret)
				goto inputFailure;
			cursor = capture->ringPendingAddress;
			inputResult = 1;
		}
		flags = capture->unidentifiedRingResult;
		if ((flags & 1) || capture->ringWriteAddress != cursor) {
			capture->unidentifiedRingResult = flags &= ~1u;
			capture->ringWriteAddress = cursor;
			capture->unidentifiedRingPendingCount &= ~1u;
			if (inputResult) {
				ret = blockVeneziaVipCommandInputLocked(state, context);
				if (g_veneziaDebugger.log)
					g_veneziaDebugger.log(74, 2, context, ret);
				if (ret)
					goto inputFailure;
				flags = capture->unidentifiedRingResult;
			}
		}
		if (flags & 2) {
			ret = requestVeneziaVipCommandInputActivityLocked(state, context);
			if (g_veneziaDebugger.log)
				g_veneziaDebugger.log(75, 2, context, ret);
			if (ret)
				goto inputFailure;
			capture->unidentifiedRingResult = flags = capture->unidentifiedRingResult | 0x200;
		}
		if (inputResult) {
			inputState = 1;
			inputResult = 0;
		} else if (flags & 2) {
			inputState = 1;
		} else {
			ret = getVeneziaVipCommandInputPendingLocked(state, context);
			if (g_veneziaDebugger.log)
				g_veneziaDebugger.log(76, 2, context, ret);
			flags = capture->unidentifiedRingResult;
			inputState = ret ? 3 : 0;
			inputResult = ret ? 2 : -1;
		}
		capture->unidentifiedRingResult = flags & ~2u;
		capture->unidentifiedRingPendingCount &= ~2u;
		if (flags & 0x200) {
			inputState = 2;
			inputResult = 1;
		}
		lockVeneziaVipProcess(state->standardBufferLock);
		ret = getVeneziaVipCommandInputSpaceLocked(state, context, &available);
		if (g_veneziaDebugger.log)
			g_veneziaDebugger.log(72, 3, context, available, ret);
		if (ret) {
			unlockVeneziaVipProcess(state->standardBufferLock);
			goto inputFailure;
		}
		cursor = capture->ringCommitAddress;
		pending = capture->ringWriteAddress;
		if (pending > cursor)
			cursor += capture->ringEndAddress - capture->ringReadAddress;
		available -= cursor - pending;
		unlockVeneziaVipProcess(state->standardBufferLock);
		unlockVeneziaVipProcess(state->standardBufferLock);
		if ((unsigned int)inputState <= 1) {
			inputState = 0;
			goto checkCompletion;
		}
		if (inputResult < 0) {
			inputState = 1;
			goto checkCompletion;
		}
		goto inputResultReady;
inputFailure:
		inputResult = -2141060856;
inputDone:
		unlockVeneziaVipProcess(state->standardBufferLock);
		if (inputResult < 0) {
			inputState = 1;
			goto checkCompletion;
		}
inputResultReady:
		if (inputResult == 1)
			command |= 0x20000000;
		inputState = contextState == 2 && inputResult != 1;
checkCompletion:
		if (contextState == 4)
			inputState = 1;
		lockVeneziaVipProcess(state->commonProcessorLock);
		completionCount = capture->processorCompletionCounts[0];
		unlockVeneziaVipProcess(state->commonProcessorLock);
		lockVeneziaVipProcess(state->commonProcessorLock);
		secondaryCount = capture->processorCompletionCounts[1];
		unlockVeneziaVipProcess(state->commonProcessorLock);
		lockVeneziaVipProcess(state->commonProcessorLock);
		unlockVeneziaVipProcess(state->commonProcessorLock);
		lockVeneziaVipProcess(state->commonProcessorLock);
		unlockVeneziaVipProcess(state->commonProcessorLock);
		signedRequest = command < 0 ? 0x80001000 : 0x1000;
		forwardCommand = command < 0 ? 0x80001000 : 0;
		if (completionCount > secondaryCount && completionCount - secondaryCount >= 8) {
			forwardCommand = signedRequest;
			inputState = 1;
		}
		if (command & 0x20000000) {
			if (contextState == 2)
				inputState = 0;
			forwardCommand = signedRequest | 0x20000000;
		}
		if (inputState) {
			if (captureIndex == -1) {
				sceKernelSetEventFlag(state->processorEvents[1], 0x20000);
			} else if (forwardCommand) {
				lockVeneziaVipProcess(state->commonProcessorLock);
				active = state->activeProcess;
				if (active) {
					enqueueVeneziaVipProcess(&active->queues[1], (uint8_t)captureIndex);
					active->captures[captureIndex].pendingProcessorEvents[1] |= forwardCommand;
					unlockVeneziaVipProcess(state->commonProcessorLock);
					sceKernelSetEventFlag(state->processorEvents[1], 0x20000);
				} else {
					unlockVeneziaVipProcess(state->commonProcessorLock);
				}
			}
			goto acknowledge;
		}
		if (captureIndex != -1) {
			lockVeneziaVipProcess(state->commonProcessorLock);
			active = state->activeProcess;
			if (!active) {
				unlockVeneziaVipProcess(state->commonProcessorLock);
				goto startCapture;
			}
			enqueueVeneziaVipProcess(&active->queues[1], (uint8_t)captureIndex);
			active->captures[captureIndex].pendingProcessorEvents[1] |= 0x40001000 | forwardCommand;
			unlockVeneziaVipProcess(state->commonProcessorLock);
		}
		sceKernelSetEventFlag(state->processorEvents[1], 0x20000);
startCapture:
		if (getVeneziaVipProcessorStatusLocked(state, 0, &status))
			goto captureFailure;
		if (status != 6) {
			getVeneziaVipCaptureContext(capture, state, &startContext);
			getVeneziaVipCaptureProcessorStatus(capture, state, 0, &contextState);
			if (contextState == 2 || contextState == 4) {
				ret = startVeneziaVipCommandCaptureLocked(state, 0, startContext, 0);
				if (g_veneziaDebugger.log)
					g_veneziaDebugger.log(86, 2, 0, ret);
				if (ret)
					goto captureFailure;
				lockVeneziaVipProcess(state->commonProcessorLock);
				pending = capture->processorPendingCounts[0];
				unlockVeneziaVipProcess(state->commonProcessorLock);
				lockVeneziaVipProcess(state->commonProcessorLock);
				capture->processorPendingCounts[0] = pending + 1;
				unlockVeneziaVipProcess(state->commonProcessorLock);
			}
		}
		do {
			ret = waitVeneziaVipPrimaryCaptureInterrupt(capture, state, &interruptEvents, &status);
			if (status == 6) {
				interruptEvents |= 0x80000;
				sceKernelSetEventFlag(state->processorEvents[0], 0x1000000);
				sceKernelSetEventFlag(state->processorEvents[1], 0x1000000);
				deferred = 0;
				recovering = 0;
				goto receive;
			}
			if (ret < 0)
				completeVeneziaVipCaptureRequest(state, captureIndex, 4, &command);
		} while (ret >= 0 && !(interruptEvents & 0xffff00ff));
		if (interruptEvents & 0x80000) {
			lockVeneziaVipProcess(state->standardBufferLock);
			ret = getVeneziaVipCommandInputSpaceLocked(state, context, &available);
			if (g_veneziaDebugger.log)
				g_veneziaDebugger.log(72, 3, context, available, ret);
			if (!ret) {
				cursor = capture->ringCommitAddress;
				pending = capture->ringWriteAddress;
				if (pending > cursor)
					cursor += capture->ringEndAddress - capture->ringReadAddress;
				available -= cursor - pending;
				unlockVeneziaVipProcess(state->standardBufferLock);
				lockVeneziaVipProcess(state->standardBufferLock);
				if (capture->ringMode && capture->ringSize < available) {
					capture->unidentifiedCaptureCounter++;
					sceKernelSetEventFlag(capture->eventFlag, 4);
					lockVeneziaVipProcess(state->commonProcessorLock);
					if (capture->callbacks[0].uid &&
					    (capture->callbacks[0].eventMask & 4)) {
						capture->callbacks[0].pendingEvents =
							capture->callbacks[0].eventMask & 4;
						sceKernelNotifyCallback(capture->callbacks[0].uid,
							capture->callbacks[0].pendingEvents);
					}
					if (capture->callbacks[1].uid &&
					    (capture->callbacks[1].eventMask & 4)) {
						capture->callbacks[1].pendingEvents =
							capture->callbacks[1].eventMask & 4;
						sceKernelNotifyCallback(capture->callbacks[1].uid,
							capture->callbacks[1].pendingEvents);
					}
					if (capture->callbacks[2].uid &&
					    (capture->callbacks[2].eventMask & 4)) {
						capture->callbacks[2].pendingEvents =
							capture->callbacks[2].eventMask & 4;
						sceKernelNotifyCallback(capture->callbacks[2].uid,
							capture->callbacks[2].pendingEvents);
					}
					if (capture->callbacks[3].uid &&
					    (capture->callbacks[3].eventMask & 4)) {
						capture->callbacks[3].pendingEvents =
							capture->callbacks[3].eventMask & 4;
						sceKernelNotifyCallback(capture->callbacks[3].uid,
							capture->callbacks[3].pendingEvents);
					}
					if (capture->callbacks[4].uid &&
					    (capture->callbacks[4].eventMask & 4)) {
						capture->callbacks[4].pendingEvents =
							capture->callbacks[4].eventMask & 4;
						sceKernelNotifyCallback(capture->callbacks[4].uid,
							capture->callbacks[4].pendingEvents);
					}
					if (capture->callbacks[5].uid &&
					    (capture->callbacks[5].eventMask & 4)) {
						capture->callbacks[5].pendingEvents =
							capture->callbacks[5].eventMask & 4;
						sceKernelNotifyCallback(capture->callbacks[5].uid,
							capture->callbacks[5].pendingEvents);
					}
					unlockVeneziaVipProcess(state->commonProcessorLock);
				}
				unlockVeneziaVipProcess(state->standardBufferLock);
			} else {
				unlockVeneziaVipProcess(state->standardBufferLock);
				completeVeneziaVipCaptureRequest(state, captureIndex, 4, &command);
			}
			if (interruptEvents & 0x100) {
				lockVeneziaVipProcess(state->standardBufferLock);
				cursor = capture->ringPendingAddress;
				pending = capture->ringWriteAddress;
				unlockVeneziaVipProcess(state->standardBufferLock);
				if (cursor == pending)
					goto acknowledge;
			}
			if (interruptEvents & 0x200)
				goto acknowledge;
			getVeneziaVipCaptureProcessorStatus(capture, state, 0, &contextState);
			if (contextState != 2 && contextState != 4)
				goto acknowledge;
			if (captureIndex == -1) {
				sceKernelSetEventFlag(state->processorEvents[0], 0x20000);
			} else {
				lockVeneziaVipProcess(state->commonProcessorLock);
				active = state->activeProcess;
				if (active) {
					enqueueVeneziaVipProcess(&active->queues[0], (uint8_t)captureIndex);
					active->captures[captureIndex].pendingProcessorEvents[0] |= 0x1000;
					unlockVeneziaVipProcess(state->commonProcessorLock);
					sceKernelSetEventFlag(state->processorEvents[0], 0x20000);
				} else {
					unlockVeneziaVipProcess(state->commonProcessorLock);
				}
			}
			sceKernelDelayThread(1);
		}
acknowledge:
		if (command < 0)
			completeVeneziaVipCaptureRequest(state, captureIndex, 1, &command);
		continue;
captureFailure:
		completeVeneziaVipCaptureRequest(state, captureIndex, 4, &command);
		continue;
invalidCapture:
		markVeneziaVipCaptureRecovery(state, 0, &command);
		continue;
noProcess:
		if ((int)state->processorRequests[0].command < 0) {
			state->processorRequests[0].resultFlags = 2;
			sceKernelSetEventFlag(state->processorEvents[0], 0x100000);
		}
		unlockVeneziaVipProcess(state->commonProcessorLock);
		goto receive;
	}
}

extern int continueVeneziaVipCommandContextLocked(VeneziaVipState *state,
	unsigned int processor, VeneziaVipCommandContext *context, uint32_t value);
extern int getVeneziaVipCommandAuxiliaryCountLocked(VeneziaVipState *state,
	VeneziaVipCommandContext *context, SceSize *count);

/* name unknown: service secondary processor captures and auxiliary callbacks. */
int veneziaVipProcessorThread1(SceSize size, void *arg)
{
	VeneziaVipState *state = *(VeneziaVipState **)arg;
	VeneziaVipProcessStorage *storage, *active;
	VeneziaDebugProcessCapture *capture;
	VeneziaVipCommandContext *context;
	uint32_t events, deferred, response, status, interruptEvents;
	uint32_t contextState, completionCount, secondaryCount;
	uint32_t primaryPending, secondaryPending, auxiliaryDelta, auxiliaryCount;
	uint32_t notification, callbackEvents, sequence;
	uint32_t flags, pending, cursor, queued, forwardCommand, signedRequest;
	SceSize available;
	uint8_t value;
	int command = 0, captureIndex = -1, recovering, ret, inputState, inputResult;

	ret = sceKernelSetEventFlag(state->processorEvents[1], 0x100000);
	if (ret < 0)
		return ret;
	for (;;) {
		deferred = 0;
		recovering = 0;
		events = 0;
receive:
		command = 0;
		if (recovering == 1)
			goto recoveryWait;
		sceKernelWaitEventFlag(state->processorEvents[1], 0x1030000, 5,
			&events, NULL);
		lockVeneziaVipProcess(state->commonProcessorLock);
		storage = state->activeProcess;
		if (!storage)
			goto noProcess;
		if (events & 0x1000000) {
			state->processorRecoveryEvents[1] = events;
			sceKernelSetEventFlag(state->processorEvents[0], 0x2000000);
			unlockVeneziaVipProcess(state->commonProcessorLock);
			sceKernelWaitEventFlag(state->processorEvents[1], 0x2000000, 1,
				&response, NULL);
			lockVeneziaVipProcess(state->commonProcessorLock);
			deferred = events & 0x20000;
			if ((events & 0x10000) || (int)state->processorRequests[1].command < 0 ||
			    (int)state->processorActiveCommands[1] < 0)
				sceKernelSetEventFlag(state->processorEvents[1], 0x10000);
			recovering = 1;
			unlockVeneziaVipProcess(state->commonProcessorLock);
recoveryWait:
			command = 0;
			sceKernelWaitEventFlag(state->processorEvents[1], 0x4010000, 5,
				&events, NULL);
			lockVeneziaVipProcess(state->commonProcessorLock);
			if (!state->activeProcess)
				goto noProcess;
			if (events & 0x4000000) {
				sceKernelSetEventFlag(state->processorEvents[1],
					deferred | (events & 0x10000));
				deferred = 0;
				state->processorActiveCaptures[1] = -1;
				state->processorActiveEvents[1] = 0;
				recovering = 0;
				state->processorActiveCommands[1] = 0;
				sceKernelClearEventFlag(state->processorEvents[1], ~0x6000000u);
			} else if ((events & 0x10000) &&
				   ((int)state->processorRequests[1].command < 0 ||
				    (int)state->processorActiveCommands[1] < 0)) {
				state->processorRequests[1].resultFlags = 6;
				sceKernelSetEventFlag(state->processorEvents[1], 0x100000);
			}
			unlockVeneziaVipProcess(state->commonProcessorLock);
			goto receive;
		}
		if (state->unidentifiedProcessorControls[1] == 1 || storage->pendingCounters[1] == 1)
			events &= ~0x20000u;
		queued = events & 0x10000;
		if (queued) {
			command = state->processorRequests[1].command;
			captureIndex = state->processorRequests[1].captureIndex;
			if ((unsigned int)captureIndex > 3)
				captureIndex = -1;
			if (command & 0x20000) {
				events &= ~0x20000u;
				state->unidentifiedProcessorControls[1] = 1;
			}
			if (command & 0x40000) {
				state->unidentifiedProcessorControls[1] = 0;
				if (storage->pendingCounters[1] != 1 &&
				    getVeneziaVipProcessQueueCount(&storage->queues[1]) > 0)
					events |= 0x20000;
			}
			if ((command & 0x400) && getVeneziaVipProcessQueueCount(&storage->queues[1]) > 0)
				events |= 0x20000;
			if (events & 0x20000)
				sceKernelSetEventFlag(state->processorEvents[1], 0x20000);
		} else if (events & 0x20000) {
			captureIndex = -1;
			if (!dequeueVeneziaVipProcess(&storage->queues[1], &value)) {
				captureIndex = value;
				queued = storage->captures[value].pendingProcessorEvents[1];
				storage->captures[value].pendingProcessorEvents[1] = 0;
			}
			command = queued;
			if (getVeneziaVipProcessQueueCount(&storage->queues[1]) > 0)
				sceKernelSetEventFlag(state->processorEvents[1], 0x20000);
		}
		state->processorPendingCommands[1] = command < 0 ? 0x80000000 : 0;
		if ((command & 0x90) && captureIndex != -1)
			removeVeneziaVipProcess(&storage->queues[1], (uint8_t)captureIndex);
		unlockVeneziaVipProcess(state->commonProcessorLock);
		recovering = events;
		if (!events)
			goto receive;
		if (!command) {
			recovering = 0;
			goto receive;
		}
		storage = state->activeProcess;
		state->processorActiveCaptures[1] = captureIndex;
		state->processorActiveEvents[1] = events;
		state->processorActiveCommands[1] = command;
		if (!storage)
			goto invalidCapture;
		if (captureIndex == -1) {
			capture = NULL;
			if ((command & 0xf9f8f0) || (int)storage->status[0] < 0)
				goto invalidCapture;
		} else {
			if (storage->status[0] <= (unsigned int)captureIndex)
				goto invalidCapture;
			capture = &storage->captures[captureIndex];
		}
		if (storage->pendingCounters[1] == 1) {
			flags = command & 0x60580;
			command &= 0xff06058f;
			if (!flags) {
				sceKernelClearEventFlag(state->processorEvents[1], ~0x7030000u);
				completeVeneziaVipSecondaryCaptureRequest(state, captureIndex, 2, &command);
				continue;
			}
		}
		if (command & 0x10) {
			getVeneziaVipCaptureContext(capture, state, &context);
			ret = submitVeneziaVipCommandContextLocked(state, 1, context);
			if (g_veneziaDebugger.log)
				g_veneziaDebugger.log(82, 3, 1, context, ret);
			if (ret || waitVeneziaVipSecondaryCaptureInterrupt(capture, state, NULL, NULL) < 0) {
				completeVeneziaVipSecondaryCaptureRequest(state, captureIndex, 2, &command);
			} else {
				lockVeneziaVipProcess(state->commonProcessorLock);
				capture->processorCompletionCounts[1] = 0;
				unlockVeneziaVipProcess(state->commonProcessorLock);
				lockVeneziaVipProcess(state->commonProcessorLock);
				capture->processorPendingCounts[1] = 0;
				unlockVeneziaVipProcess(state->commonProcessorLock);
				lockVeneziaVipProcess(state->commonProcessorLock);
				capture->queuedAuxiliaryCount = 0;
				unlockVeneziaVipProcess(state->commonProcessorLock);
				completeVeneziaVipSecondaryCaptureRequest(state, captureIndex, 1, &command);
			}
		}
		if (command & 0x20) {
			getVeneziaVipCaptureContext(capture, state, &context);
			flags = command & 0x40000000 ? 0xc8000000 : 0x88000000;
			ret = continueVeneziaVipCommandContextLocked(state, 1, context, flags);
			if (g_veneziaDebugger.log)
				g_veneziaDebugger.log(83, 2, flags, ret);
			if (!ret && waitVeneziaVipSecondaryCaptureInterrupt(capture, state, NULL, NULL) >= 0)
				completeVeneziaVipSecondaryCaptureRequest(state, captureIndex, 1, &command);
			else
				completeVeneziaVipSecondaryCaptureRequest(state, captureIndex, 2, &command);
		}
		if (command & 0x40) {
			getVeneziaVipCaptureContext(capture, state, &context);
			ret = configureVeneziaVipCommandCaptureLocked(state, 1, context);
			if (g_veneziaDebugger.log)
				g_veneziaDebugger.log(84, 2, 1, ret);
			if (ret || waitVeneziaVipSecondaryCaptureInterrupt(capture, state, NULL, NULL) < 0)
				completeVeneziaVipSecondaryCaptureRequest(state, captureIndex, 2, &command);
			else
				completeVeneziaVipSecondaryCaptureRequest(state, captureIndex, 1, &command);
		}
		if (command & 0x80)
			completeVeneziaVipSecondaryCaptureRequest(state, captureIndex, 1, &command);
		if (command & 0x100) {
			ret = getVeneziaVipProcessorStatusLocked(state, 1, &status);
			if (!ret && status != 1) {
				ret = requestVeneziaVipCommandStopLocked(state, 1);
				if (g_veneziaDebugger.log)
					g_veneziaDebugger.log(85, 2, 1, ret);
				if (!ret) {
					ret = waitVeneziaVipCaptureInterrupt(NULL, state, 1, 3, NULL, NULL);
					if (ret >= 0)
						ret = 0;
				}
			}
			sceKernelClearEventFlag(state->processorEvents[1], ~0x7030000u);
			completeVeneziaVipSecondaryCaptureRequest(state, captureIndex, ret ? 2 : 1, &command);
		}
		if (command & 0x200) {
			sceKernelClearEventFlag(state->processorEvents[1], ~0x7030000u);
			storage->pendingCounters[1] = 1;
			completeVeneziaVipSecondaryCaptureRequest(state, captureIndex, 1, &command);
		}
		if (command & 0x400) {
			storage->pendingCounters[1] = 0;
			completeVeneziaVipSecondaryCaptureRequest(state, captureIndex, 1, &command);
		}
		if (command & 0x20000)
			completeVeneziaVipSecondaryCaptureRequest(state, captureIndex, 1, &command);
		if (command & 0x40000)
			completeVeneziaVipSecondaryCaptureRequest(state, captureIndex, 1, &command);		if (!(command & 0x1000))
			continue;
		interruptEvents = 0;
		getVeneziaVipCaptureProcessorStatus(capture, state, 1, &contextState);
		getVeneziaVipCaptureContext(capture, state, &context);
		if (!context)
			continue;
		if (contextState != 2 && contextState != 4)
			goto captureFailure;
		lockVeneziaVipProcess(state->commonProcessorLock);
		completionCount = capture->processorCompletionCounts[0];
		unlockVeneziaVipProcess(state->commonProcessorLock);
		lockVeneziaVipProcess(state->commonProcessorLock);
		secondaryCount = capture->processorCompletionCounts[1];
		unlockVeneziaVipProcess(state->commonProcessorLock);
		lockVeneziaVipProcess(state->commonProcessorLock);
		primaryPending = capture->processorPendingCounts[0];
		unlockVeneziaVipProcess(state->commonProcessorLock);
		lockVeneziaVipProcess(state->commonProcessorLock);
		secondaryPending = capture->processorPendingCounts[1];
		unlockVeneziaVipProcess(state->commonProcessorLock);
		if (primaryPending <= secondaryPending || completionCount < secondaryCount) {
			if (!(command & 0x60000000))
				goto acknowledge;
			if (contextState == 4 && completionCount <= secondaryCount)
				goto acknowledge;
		} else if (completionCount == secondaryCount) {
			if (command & 0x40000000) {
				if (contextState == 4)
					goto acknowledge;
			} else {
				if (!(command & 0x60000000))
					goto acknowledge;
				if (contextState == 4 && completionCount <= secondaryCount)
					goto acknowledge;
			}
		} else if (contextState == 4 && completionCount <= secondaryCount) {
			goto acknowledge;
		}
		if (getVeneziaVipProcessorStatusLocked(state, 1, &status))
			goto captureFailure;
		if (status != 6) {
			getVeneziaVipCaptureContext(capture, state, &context);
			getVeneziaVipCaptureProcessorStatus(capture, state, 1, &contextState);
			if (contextState == 2 || contextState == 4) {
				programVeneziaVipCaptureImage(capture, state);
				lockVeneziaVipProcess(state->commonProcessorLock);
				flags = capture->unidentifiedBeforeCaptureMemory;
				unlockVeneziaVipProcess(state->commonProcessorLock);
				ret = startVeneziaVipCommandCaptureLocked(state, 1, context, flags);
				if (g_veneziaDebugger.log)
					g_veneziaDebugger.log(86, 2, 1, ret);
				if (!ret) {
					lockVeneziaVipProcess(state->commonProcessorLock);
					pending = capture->processorPendingCounts[1];
					unlockVeneziaVipProcess(state->commonProcessorLock);
					lockVeneziaVipProcess(state->commonProcessorLock);
					capture->processorPendingCounts[1] = pending + 1;
					unlockVeneziaVipProcess(state->commonProcessorLock);
				} else if (ret == 2) {
					interruptEvents |= 0x2000000;
					goto notifyCapture;
				} else {
					goto captureFailure;
				}
			}
		}
		for (;;) {
			ret = waitVeneziaVipSecondaryCaptureInterrupt(capture, state, &interruptEvents, &status);
			if (status == 6) {
				interruptEvents = 0x40000000;
				sceKernelSetEventFlag(state->processorEvents[0], 0x1000000);
				sceKernelSetEventFlag(state->processorEvents[1], 0x1000000);
				goto nextCommand;
			}
			if (ret < 0) {
				completeVeneziaVipSecondaryCaptureRequest(state, captureIndex, 4, &command);
				interruptEvents = 0;
			} else {
				if (!(interruptEvents & 0xffff00ff))
					continue;
				if (interruptEvents & 0xe0000000)
					goto notifyCapture;
			}
			if (captureIndex == -1) {
				sceKernelSetEventFlag(state->processorEvents[0], 0x20000);
			} else {
				lockVeneziaVipProcess(state->commonProcessorLock);
				active = state->activeProcess;
				if (active) {
					enqueueVeneziaVipProcess(&active->queues[0], (uint8_t)captureIndex);
					active->captures[captureIndex].pendingProcessorEvents[0] |= 0x1000;
					unlockVeneziaVipProcess(state->commonProcessorLock);
					sceKernelSetEventFlag(state->processorEvents[0], 0x20000);
				} else {
					unlockVeneziaVipProcess(state->commonProcessorLock);
				}
			}
			lockVeneziaVipProcess(state->commonProcessorLock);
			completionCount = capture->processorCompletionCounts[0];
			unlockVeneziaVipProcess(state->commonProcessorLock);
			lockVeneziaVipProcess(state->commonProcessorLock);
			secondaryCount = capture->processorCompletionCounts[1];
			unlockVeneziaVipProcess(state->commonProcessorLock);
			lockVeneziaVipProcess(state->commonProcessorLock);
			primaryPending = capture->processorPendingCounts[0];
			unlockVeneziaVipProcess(state->commonProcessorLock);
			lockVeneziaVipProcess(state->commonProcessorLock);
			secondaryPending = capture->processorPendingCounts[1];
			unlockVeneziaVipProcess(state->commonProcessorLock);
			if (completionCount > secondaryCount && primaryPending > secondaryPending) {
				if (captureIndex == -1) {
					sceKernelSetEventFlag(state->processorEvents[1], 0x20000);
				} else {
					lockVeneziaVipProcess(state->commonProcessorLock);
					active = state->activeProcess;
					if (active) {
						enqueueVeneziaVipProcess(&active->queues[1], (uint8_t)captureIndex);
						active->captures[captureIndex].pendingProcessorEvents[1] |= 0x1000;
						unlockVeneziaVipProcess(state->commonProcessorLock);
						sceKernelSetEventFlag(state->processorEvents[1], 0x20000);
					} else {
						unlockVeneziaVipProcess(state->commonProcessorLock);
					}
				}
			}
			break;
		}
notifyCapture:
		auxiliaryDelta = interruptEvents & 0x80000;
		auxiliaryCount = 0;
		notification = 1;
		flags = 0;
		if (interruptEvents & 0x80000) {
			getVeneziaVipCaptureContext(capture, state, &context);
			lockVeneziaVipProcess(state->commonProcessorLock);
			ret = getVeneziaVipCommandAuxiliaryCountLocked(state, context, &auxiliaryCount);
			if (g_veneziaDebugger.log)
				g_veneziaDebugger.log(88, 3, context, auxiliaryCount, ret);
			if (ret) {
				auxiliaryDelta = 0;
				unlockVeneziaVipProcess(state->commonProcessorLock);
			} else {
				pending = capture->queuedAuxiliaryCount;
				capture->queuedAuxiliaryCount = auxiliaryCount;
				auxiliaryDelta = auxiliaryCount - pending;
				unlockVeneziaVipProcess(state->commonProcessorLock);
				if (auxiliaryDelta) {
					notification = 3;
					flags = 2;
					capture->unidentifiedCaptureStatus++;
				}
			}
		}
		if (!(interruptEvents & 0x10000000))
			notification = flags;
		if (interruptEvents & 0x20000000)
			notification |= 0x100;
		if (interruptEvents >> 30)
			notification |= 1;
		if (interruptEvents & 0x200000)
			notification |= 0x100;
		if (interruptEvents & 0x400000)
			notification |= 0x100;
		if (interruptEvents & 0x800000)
			notification |= 0x400;
		if (interruptEvents & 0x2000000)
			notification |= 0x1000;
		if (interruptEvents & 0x8000000)
			notification |= 0x2000;
		if (notification) {
			sceKernelSetEventFlag(capture->eventFlag, notification);
			lockVeneziaVipProcess(state->commonProcessorLock);
			if (capture->callbacks[0].uid) {
				callbackEvents = notification & capture->callbacks[0].eventMask;
				if (callbackEvents) {
					capture->callbacks[0].pendingEvents = callbackEvents;
					sceKernelNotifyCallback(capture->callbacks[0].uid, callbackEvents);
				}
			}
			if (capture->callbacks[1].uid) {
				callbackEvents = notification & capture->callbacks[1].eventMask;
				if (callbackEvents) {
					capture->callbacks[1].pendingEvents = callbackEvents;
					sceKernelNotifyCallback(capture->callbacks[1].uid, callbackEvents);
				}
			}
			if (capture->callbacks[2].uid) {
				callbackEvents = notification & capture->callbacks[2].eventMask;
				if (callbackEvents) {
					capture->callbacks[2].pendingEvents = callbackEvents;
					sceKernelNotifyCallback(capture->callbacks[2].uid, callbackEvents);
				}
			}
			if (capture->callbacks[3].uid) {
				callbackEvents = notification & capture->callbacks[3].eventMask;
				if (callbackEvents) {
					capture->callbacks[3].pendingEvents = callbackEvents;
					sceKernelNotifyCallback(capture->callbacks[3].uid, callbackEvents);
				}
			}
			if (capture->callbacks[4].uid) {
				callbackEvents = notification & capture->callbacks[4].eventMask;
				if (callbackEvents) {
					capture->callbacks[4].pendingEvents = callbackEvents;
					sceKernelNotifyCallback(capture->callbacks[4].uid, callbackEvents);
				}
			}
			if (capture->callbacks[5].uid) {
				callbackEvents = notification & capture->callbacks[5].eventMask;
				if (callbackEvents) {
					capture->callbacks[5].pendingEvents = callbackEvents;
					sceKernelNotifyCallback(capture->callbacks[5].uid, callbackEvents);
				}
			}
			unlockVeneziaVipProcess(state->commonProcessorLock);
		}
		for (sequence = 1; sequence < auxiliaryDelta; sequence++) {
			lockVeneziaVipProcess(state->commonProcessorLock);
			if (capture->callbacks[0].uid) {
				callbackEvents = capture->callbacks[0].eventMask & 2;
				if (callbackEvents) {
					capture->callbacks[0].pendingEvents = callbackEvents;
					sceKernelNotifyCallback(capture->callbacks[0].uid, callbackEvents);
				}
			}
			if (capture->callbacks[1].uid) {
				callbackEvents = capture->callbacks[1].eventMask & 2;
				if (callbackEvents) {
					capture->callbacks[1].pendingEvents = callbackEvents;
					sceKernelNotifyCallback(capture->callbacks[1].uid, callbackEvents);
				}
			}
			if (capture->callbacks[2].uid) {
				callbackEvents = capture->callbacks[2].eventMask & 2;
				if (callbackEvents) {
					capture->callbacks[2].pendingEvents = callbackEvents;
					sceKernelNotifyCallback(capture->callbacks[2].uid, callbackEvents);
				}
			}
			if (capture->callbacks[3].uid) {
				callbackEvents = capture->callbacks[3].eventMask & 2;
				if (callbackEvents) {
					capture->callbacks[3].pendingEvents = callbackEvents;
					sceKernelNotifyCallback(capture->callbacks[3].uid, callbackEvents);
				}
			}
			if (capture->callbacks[4].uid) {
				callbackEvents = capture->callbacks[4].eventMask & 2;
				if (callbackEvents) {
					capture->callbacks[4].pendingEvents = callbackEvents;
					sceKernelNotifyCallback(capture->callbacks[4].uid, callbackEvents);
				}
			}
			if (capture->callbacks[5].uid) {
				callbackEvents = capture->callbacks[5].eventMask & 2;
				if (callbackEvents) {
					capture->callbacks[5].pendingEvents = callbackEvents;
					sceKernelNotifyCallback(capture->callbacks[5].uid, callbackEvents);
				}
			}
			unlockVeneziaVipProcess(state->commonProcessorLock);
			capture->unidentifiedCaptureStatus++;
		}
acknowledge:
		if (command < 0)
			completeVeneziaVipSecondaryCaptureRequest(state, captureIndex, 1, &command);
nextCommand:
		continue;
captureFailure:
		completeVeneziaVipSecondaryCaptureRequest(state, captureIndex, 4, &command);
		continue;
invalidCapture:
		markVeneziaVipCaptureRecovery(state, 1, &command);
		continue;
noProcess:
		if ((int)state->processorRequests[1].command < 0) {
			state->processorRequests[1].resultFlags = 2;
			sceKernelSetEventFlag(state->processorEvents[1], 0x100000);
		}
		unlockVeneziaVipProcess(state->commonProcessorLock);
		goto receive;
	}
}

/* name unknown: serialize the command API with the wrapper mutex. */
int setVeneziaVipProcessorEventMaskLocked(VeneziaVipState *state, unsigned int processor, uint32_t mask)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = setVeneziaVipCommandEventMask(processor, mask);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int initializeVeneziaVipCommandsLocked(VeneziaVipState *state, const VeneziaVipCommandCallbacks *callbacks, const VeneziaVipMemoryRegion *memory)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = initializeVeneziaVipCommands(callbacks, memory);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int getVeneziaVipContextStorageSizeLocked(VeneziaVipState *state, SceSize *size)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = getVeneziaVipCommandContextSize(size);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
VeneziaVipCommandContext * initializeVeneziaVipCaptureContextLocked(VeneziaVipState *state, const VeneziaVipCommandContextParam *param, unsigned int processor, uint32_t contextValue)
{
	VeneziaVipCommandContext * ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = initializeVeneziaVipCommandContext(param, processor, contextValue);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int getVeneziaVipQueueStorageSizeLocked(VeneziaVipState *state, SceSize *size)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = getVeneziaVipCommandQueueStorageSize(size);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int initializeVeneziaVipCaptureQueuesLocked(VeneziaVipState *state, VeneziaVipCommandContext *context, const VeneziaVipCommandQueueParam *param)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = initializeVeneziaVipCommandQueues(context, param);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int requestVeneziaVipCommandStopLocked(VeneziaVipState *state, unsigned int processor)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = requestVeneziaVipCommandStop(processor);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int interruptVeneziaVipCommandLocked(VeneziaVipState *state, unsigned int processor)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = interruptVeneziaVipCommand(processor);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int getVeneziaVipReportedValuesLocked(VeneziaVipState *state, uint32_t *main, uint32_t *auxiliary)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = getVeneziaVipCommandReportedValues(main, auxiliary);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int getVeneziaVipProcessorStatusLocked(VeneziaVipState *state, unsigned int processor, unsigned int *status)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = getVeneziaVipCommandProcessorStatus(processor, status);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int encodeVeneziaVipCaptureAddressLocked(VeneziaVipState *state, uint32_t base, uint32_t offset, uint32_t *encoded)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = encodeVeneziaVipCommandAddress(base, offset, encoded);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int calculateVeneziaVipTiledImageSizeLocked(VeneziaVipState *state, unsigned int width, unsigned int height, unsigned int imageCount, SceSize *size)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = calculateVeneziaVipTiledImageSize(width, height, imageCount, size);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int configureVeneziaVipCaptureMemoryLocked(VeneziaVipState *state, VeneziaVipCommandContext *context, SceSize length, const VeneziaVipMemoryRegion *memory)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = configureVeneziaVipCaptureMemory(context, length, memory);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int submitVeneziaVipCommandContextLocked(VeneziaVipState *state, unsigned int processor, VeneziaVipCommandContext *context)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = submitVeneziaVipCommandContext(processor, context);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int continueVeneziaVipCommandContextLocked(VeneziaVipState *state, unsigned int processor, VeneziaVipCommandContext *context, uint32_t value)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = continueVeneziaVipCommandContext(processor, context, value);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int configureVeneziaVipCommandCaptureLocked(VeneziaVipState *state, unsigned int processor, VeneziaVipCommandContext *context)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = configureVeneziaVipCommandCapture(processor, context);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int startVeneziaVipCommandCaptureLocked(VeneziaVipState *state, unsigned int processor, VeneziaVipCommandContext *context, unsigned int mode)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = startVeneziaVipCommandCapture(processor, context, mode);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int getVeneziaVipCommandContextStateLocked(VeneziaVipState *state, unsigned int processor, const VeneziaVipCommandContext *context)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = getVeneziaVipCommandContextState(processor, context);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int getVeneziaVipCommandInputSpaceLocked(VeneziaVipState *state, VeneziaVipCommandContext *context, SceSize *space)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = getVeneziaVipCommandInputSpace(context, space);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int setVeneziaVipCommandInputOffsetLocked(VeneziaVipState *state, VeneziaVipCommandContext *context, SceSize offset)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = setVeneziaVipCommandInputOffset(context, offset);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int requestVeneziaVipCommandInputActivityLocked(VeneziaVipState *state, VeneziaVipCommandContext *context)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = requestVeneziaVipCommandInputActivity(context);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int blockVeneziaVipCommandInputLocked(VeneziaVipState *state, VeneziaVipCommandContext *context)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = blockVeneziaVipCommandInput(context);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int getVeneziaVipCommandInputBlockedLocked(VeneziaVipState *state, VeneziaVipCommandContext *context)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = getVeneziaVipCommandInputBlocked(context);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int getVeneziaVipCommandInputPendingLocked(VeneziaVipState *state, VeneziaVipCommandContext *context)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = getVeneziaVipCommandInputPending(context);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int getVeneziaVipCommandAuxiliaryCountLocked(VeneziaVipState *state, VeneziaVipCommandContext *context, SceSize *count)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = getVeneziaVipCommandAuxiliaryCount(context, count);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int getVeneziaVipAuxiliaryRecordLocked(VeneziaVipState *state, VeneziaVipCommandContext *context, const VeneziaVipCommandAuxiliaryRecord **record)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = getVeneziaVipCommandAuxiliaryOffset(context, (SceSize *)record);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int releaseVeneziaVipAuxiliaryRecordLocked(VeneziaVipState *state, VeneziaVipCommandContext *context, const VeneziaVipCommandAuxiliaryRecord *record)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = releaseVeneziaVipCommandAuxiliaryRecord(context, record);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int getVeneziaVipResultMetadataLocked(VeneziaVipState *state, VeneziaVipCommandContext *context, const VeneziaVipCommandAuxiliaryRecord *record, uint32_t *imageAddress, uint32_t *captureAddress, VeneziaVipCommandResultMetadata *metadata, uint32_t *value)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = getVeneziaVipCommandResultMetadata(context, record, imageAddress, captureAddress, metadata, value);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int getVeneziaVipAuxiliaryStatusLocked(VeneziaVipState *state, const VeneziaVipCommandAuxiliaryRecord *record, VeneziaVipCommandAuxiliaryStatus *status)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = getVeneziaVipCommandAuxiliaryStatus(record, status);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command API with the wrapper mutex. */
int copyVeneziaVipDebugDataLocked(VeneziaVipState *state, VeneziaVipCommandContext *context, const VeneziaVipCommandAuxiliaryRecord *record, unsigned int processor, uint32_t *data)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = copyVeneziaVipCommandImageDebugData(context, record, processor, data);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command data API with the wrapper mutex. */
int copyVeneziaVipChannelDataLocked(VeneziaVipState *state, VeneziaVipCommandContext *context, const VeneziaVipCommandAuxiliaryRecord *record, unsigned int processor, VeneziaDebugChannelRecord *data)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = copyVeneziaVipCommandImageAuxiliaryData(context, record, processor, (uint32_t *)data);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command data API with the wrapper mutex. */
int copyVeneziaVipSecondaryDataLocked(VeneziaVipState *state, VeneziaVipCommandContext *context, const VeneziaVipCommandAuxiliaryRecord *record, unsigned int processor, uint32_t *data)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = copyVeneziaVipCommandImageSecondaryData(context, record, processor, data);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command data API with the wrapper mutex. */
int copyVeneziaVipStatusDataLocked(VeneziaVipState *state, VeneziaVipCommandContext *context, const VeneziaVipCommandAuxiliaryRecord *record, unsigned int processor, uint32_t *data)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = getVeneziaVipCommandImageStatusData(context, record, processor, data);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command data API with the wrapper mutex. */
int copyVeneziaVipControlDataLocked(VeneziaVipState *state, VeneziaVipCommandContext *context, const VeneziaVipCommandAuxiliaryRecord *record, unsigned int processor, uint32_t *data)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = copyVeneziaVipCommandImageControlData(context, record, processor, data);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command data API with the wrapper mutex. */
int copyVeneziaVipCommonDebugDataLocked(VeneziaVipState *state, VeneziaVipCommandContext *context, const VeneziaVipCommandAuxiliaryRecord *record, VeneziaVipCommonDebugData *data)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = copyVeneziaVipCommandCommonDebugData(context, record, (uint32_t *)data);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: serialize the command data API with the wrapper mutex. */
int copyVeneziaVipCaptureMetadataLocked(VeneziaVipState *state, VeneziaVipCommandContext *context, const VeneziaVipCommandAuxiliaryRecord *record, unsigned int processor, uint32_t destination, SceSize length)
{
	int ret;

	lockVeneziaVipProcess(state->apiWrapperLock);
	ret = copyVeneziaVipCommandCaptureOutput(context, record, processor, (VeneziaVipCommandCaptureOutput *)(uintptr_t)destination, length);
	unlockVeneziaVipProcess(state->apiWrapperLock);
	return ret;
}

/* name unknown: the full original single/multi-capture implementation interface. */
typedef struct VeneziaVipImplementationOperations {
	int (*removeCallbacks)(unsigned int processor, VeneziaVipState *state,
		VeneziaVipProcessStorage *storage, SceUID uid, SceUID pid);
	int (*suspend)(VeneziaVipProcessStorage *storage, unsigned int control);
	int (*resume)(VeneziaVipProcessStorage *storage);
	int (*synchronize)(void);
	int (*getCaptureSize)(VeneziaVipState *state,
		const VeneziaDebugImageFormat *format, SceSize *size);
	int (*removeCapture)(unsigned int processor, int wait, SceUID pid);
	int (*setDebuggerBase)(VeneziaVipState *state);
	int (*resetPeripheral)(void);
	int (*initializeInterrupts)(VeneziaVipState *state);
	int (*releaseInterrupts)(void);
	int (*beginCaptureConfiguration)(VeneziaVipState *state,
		VeneziaVipProcessStorage *storage);
	int (*endCaptureConfiguration)(VeneziaVipState *state,
		VeneziaVipProcessStorage *storage);
	int (*stopCapture)(unsigned int processor, VeneziaVipState *state,
		VeneziaVipProcessStorage *storage);
	int (*submitInputBuffers)(unsigned int processor, VeneziaVipState *state,
		VeneziaVipProcessStorage *storage, const VeneziaVipInputBufferList *buffers);
} VeneziaVipImplementationOperations;
const VeneziaVipImplementationOperations g_veneziaVipImplementations[2] = {
	{
		.removeCallbacks = unregisterVeneziaVipCallbacks,
		.suspend = snapshotVeneziaVipDebugger,
		.resume = restoreVeneziaVipDebugger,
		.synchronize = synchronizeVeneziaVipProcess,
		.getCaptureSize = validateVeneziaVipCaptureGeometry,
		.removeCapture = unregisterVeneziaVipCapture,
		.setDebuggerBase = setVeneziaVipDebuggerBase,
		.resetPeripheral = releaseVeneziaVipDebuggerBase,
		.initializeInterrupts = initializeVeneziaVipInterrupt,
		.releaseInterrupts = releaseVeneziaVipInterrupt,
		.beginCaptureConfiguration = initializeVeneziaVipDebugger,
		.endCaptureConfiguration = finalizeVeneziaVipDebugger,
		.stopCapture = stopVeneziaVipCaptureChannel,
		.submitInputBuffers = copyVeneziaVipInputToTrace,
	},
	{
		.removeCallbacks = clearVeneziaVipCaptureCallbacks,
		.suspend = controlVeneziaVipProcess,
		.resume = prepareVeneziaVipProcess,
		.synchronize = synchronizeVeneziaVipMultiCapture,
		.getCaptureSize = getVeneziaVipImageStorageSize,
		.removeCapture = unregisterVeneziaVipMultiCapture,
		.setDebuggerBase = programVeneziaVipDebuggerCommandBase,
		.resetPeripheral = resetVeneziaVipPeripheral,
		.initializeInterrupts = initializeVeneziaVipInterrupts,
		.releaseInterrupts = releaseVeneziaVipInterrupts,
		.beginCaptureConfiguration = initializeVeneziaVipProcessorEvents,
		.endCaptureConfiguration = shutdownVeneziaVipProcessors,
		.stopCapture = resetVeneziaVipCaptureContext,
		.submitInputBuffers = copyVeneziaVipInputToCaptureRing,
	},
};

/* name unknown: dispatch through the selected capture implementation. */
__attribute__((noinline)) int removeVeneziaVipCallbacks(unsigned int processor, VeneziaVipState *state, VeneziaVipProcessStorage *storage, SceUID uid, SceUID pid)
{
	unsigned int implementationIndex = storage->count & 15;

	if (implementationIndex > 1)
		return -2141060856;
	return g_veneziaVipImplementations[implementationIndex].removeCallbacks(processor, state, storage, uid, pid);
}

/* name unknown: dispatch through the selected capture implementation. */
int dispatchVeneziaVipProcessSuspend(VeneziaVipProcessStorage *storage, int reason)
{
	unsigned int implementationIndex = storage->count & 15;

	if (implementationIndex > 1)
		return -2141060856;
	return g_veneziaVipImplementations[implementationIndex].suspend(storage, reason);
}

/* name unknown: dispatch through the selected capture implementation. */
int dispatchVeneziaVipProcessResume(VeneziaVipProcessStorage *storage)
{
	unsigned int implementationIndex = storage->count & 15;

	if (implementationIndex > 1)
		return -2141060856;
	return g_veneziaVipImplementations[implementationIndex].resume(storage);
}

/* name unknown: dispatch through the selected capture implementation. */
int dispatchVeneziaVipProcessReset(VeneziaVipProcessStorage *storage)
{
	unsigned int implementationIndex = storage->count & 15;

	if (implementationIndex > 1)
		return -2141060856;
	return g_veneziaVipImplementations[implementationIndex].synchronize();
}

/* name unknown: dispatch through the selected capture implementation. */
__attribute__((noinline)) int getVeneziaVipCaptureSize(unsigned int processor, VeneziaVipState *state, const VeneziaDebugImageFormat *geometry, uint32_t *size)
{
	unsigned int implementationIndex = processor & 15;

	if (implementationIndex > 1)
		return -2141060856;
	return g_veneziaVipImplementations[implementationIndex].getCaptureSize(state, geometry, size);
}

/* name unknown: dispatch through the selected capture implementation. */
__attribute__((noinline))
int dispatchVeneziaVipCaptureRemoval(unsigned int implementation, unsigned int processor, int wait, SceUID pid)
{
	unsigned int implementationIndex = implementation & 15;

	if (implementationIndex > 1)
		return -2141060856;
	return g_veneziaVipImplementations[implementationIndex].removeCapture(processor, wait, pid);
}

/* name unknown: dispatch through the selected capture implementation. */
__attribute__((noinline)) int configureVeneziaVipProcessorCapture(unsigned int count, VeneziaVipState *state)
{
	unsigned int implementationIndex = count & 15;

	if (implementationIndex > 1)
		return -2141060856;
	return g_veneziaVipImplementations[implementationIndex].setDebuggerBase(state);
}

/* name unknown: dispatch through the selected capture implementation. */
__attribute__((noinline)) int startVeneziaVipProcessorCapture(unsigned int count, VeneziaVipState *state)
{
	unsigned int implementationIndex = count & 15;

	if (implementationIndex > 1)
		return -2141060856;
	return g_veneziaVipImplementations[implementationIndex].resetPeripheral();
}

/* name unknown: dispatch through the selected capture implementation. */
__attribute__((noinline)) int prepareVeneziaVipProcessorCapture(unsigned int count, VeneziaVipState *state)
{
	unsigned int implementationIndex = count & 15;

	if (implementationIndex > 1)
		return -2141060856;
	return g_veneziaVipImplementations[implementationIndex].initializeInterrupts(state);
}

/* name unknown: dispatch through the selected capture implementation. */
__attribute__((noinline)) int stopVeneziaVipProcessorCapture(unsigned int count, VeneziaVipState *state)
{
	unsigned int implementationIndex = count & 15;

	if (implementationIndex > 1)
		return -2141060856;
	return g_veneziaVipImplementations[implementationIndex].releaseInterrupts();
}

/* name unknown: dispatch through the selected capture implementation. */
__attribute__((noinline)) int beginVeneziaVipCaptureConfiguration(VeneziaVipState *state,
	VeneziaVipProcessStorage *storage)
{
	unsigned int implementationIndex = storage->count & 15;

	if (implementationIndex > 1)
		return -2141060856;
	return g_veneziaVipImplementations[implementationIndex].beginCaptureConfiguration(state, storage);
}

/* name unknown: dispatch through the selected capture implementation. */
__attribute__((noinline)) int endVeneziaVipCaptureConfiguration(VeneziaVipState *state, VeneziaVipProcessStorage *storage)
{
	unsigned int implementationIndex = storage->count & 15;

	if (implementationIndex > 1)
		return -2141060856;
	return g_veneziaVipImplementations[implementationIndex].endCaptureConfiguration(state, storage);
}

/* name unknown: dispatch through the selected capture implementation. */
__attribute__((noinline)) int stopVeneziaVipCapture(unsigned int processor, VeneziaVipState *state, VeneziaVipProcessStorage *storage)
{
	unsigned int implementationIndex = storage->count & 15;

	if (implementationIndex > 1)
		return -2141060856;
	return g_veneziaVipImplementations[implementationIndex].stopCapture(processor, state, storage);
}

/* name unknown: dispatch through the selected capture implementation. */
int submitVeneziaVipInputBuffers(unsigned int processor, VeneziaVipState *state, VeneziaVipProcessStorage *storage, const VeneziaVipInputBufferList *buffers)
{
	unsigned int implementationIndex = storage->count & 15;

	if (implementationIndex > 1)
		return -2141060856;
	return g_veneziaVipImplementations[implementationIndex].submitInputBuffers(processor, state, storage, buffers);
}

/* name unknown: complete public capture interface for both implementations. */
typedef struct VeneziaVipCaptureOperations {
	int (*getRequirements)(const VeneziaDebugImageFormat *geometry, uint32_t *size);
	int (*getStorageSize)(const VeneziaDebugImageGeometry *geometry, SceSize *size);
	int (*registerCapture)(unsigned int *processor, const VeneziaVipCaptureParam *param);
	int (*unregisterCapture)(unsigned int processor, int wait, SceUID pid);
	int (*getTraceAvailable)(unsigned int processor);
	int (*configureControl)(unsigned int processor, unsigned int control);
	int (*processRaw)(unsigned int processor, const VeneziaVipRawInput *input,
		VeneziaDebugSnapshotList *list, unsigned int *completed);
	int (*submitRaw)(unsigned int processor, const VeneziaVipRawInput *input,
		const SceUInt *timeout);
	int (*wait)(unsigned int processor, VeneziaDebugSnapshotList *list,
		unsigned int *completed, const SceUInt *timeout);
	int (*process)(unsigned int processor, const VeneziaVipVideoInput *input,
		VeneziaDebugSnapshotList *list, unsigned int *completed);
	int (*submit)(unsigned int processor, const VeneziaVipVideoInput *input,
		const SceUInt *timeout);
	int (*drain)(unsigned int processor, VeneziaDebugSnapshotList *list,
		unsigned int *completed);
	int (*reset)(unsigned int processor);
	int (*consume)(unsigned int processor);
	int (*registerCallback)(unsigned int processor, SceUID uid, uint32_t eventMask);
	int (*unregisterCallback)(unsigned int processor, SceUID uid);
} VeneziaVipCaptureOperations;
const VeneziaVipCaptureOperations g_veneziaVipCaptureOperations[2] = {
	{
		.getRequirements = getVeneziaVipCaptureRequirements,
		.getStorageSize = getVeneziaVipCaptureStorageSize,
		.registerCapture = registerVeneziaVipCapture,
		.unregisterCapture = unregisterVeneziaVipCaptureForProcess,
		.getTraceAvailable = getVeneziaVipTraceAvailable,
		.configureControl = (int (*)(unsigned int, unsigned int))rejectVeneziaVipProcessOperation,
		.processRaw = processVeneziaVipRawCapture,
		.submitRaw = submitVeneziaVipRawCapture,
		.wait = waitVeneziaVipCapture,
		.process = processVeneziaVipCapture,
		.submit = submitVeneziaVipCapture,
		.drain = drainVeneziaVipCapture,
		.reset = resetVeneziaVipCapture,
		.consume = consumeVeneziaVipCapture,
		.registerCallback = registerVeneziaVipCallback,
		.unregisterCallback = unregisterVeneziaVipCallback,
	},
	{
		.getRequirements = queryVeneziaVipCaptureSize,
		.getStorageSize = getVeneziaVipMultiCaptureStorageSize,
		.registerCapture = registerVeneziaVipMultiCapture,
		.unregisterCapture = unregisterVeneziaVipMultiCaptureForProcess,
		.getTraceAvailable = getVeneziaVipMultiTraceAvailable,
		.configureControl = configureVeneziaVipCaptureControl,
		.processRaw = (int (*)(unsigned int, const VeneziaVipRawInput *, VeneziaDebugSnapshotList *, unsigned int *))processVeneziaVipMultiRawCapture,
		.submitRaw = submitVeneziaVipMultiRawCapture,
		.wait = (int (*)(unsigned int, VeneziaDebugSnapshotList *, unsigned int *, const SceUInt *))pollVeneziaVipMultiCapture,
		.process = (int (*)(unsigned int, const VeneziaVipVideoInput *, VeneziaDebugSnapshotList *, unsigned int *))processVeneziaVipMultiCapture,
		.submit = submitVeneziaVipMultiCapture,
		.drain = (int (*)(unsigned int, VeneziaDebugSnapshotList *, unsigned int *))finishVeneziaVipMultiCapture,
		.reset = restartVeneziaVipMultiCapture,
		.consume = releaseVeneziaVipCaptureResultsForProcessor,
		.registerCallback = registerVeneziaVipCaptureCallback,
		.unregisterCallback = unregisterVeneziaVipCaptureCallback,
	},
};

int SceCodecEngineWrapperForDriver_2B74706E(const VeneziaDebugImageFormat *geometry,
	uint32_t *size)
{
	VeneziaVipProcessStorage *storage = _sceVipGetVipProcessResource(0);

	unsigned int index;

	if (!storage)
		return -2141060856;
	index = storage->count & 15;
	if (storage->count <= 1u)
		return g_veneziaVipCaptureOperations[index].getRequirements(geometry, size);
	return -2141060856;
}

int SceCodecEngineWrapperForDriver_8CD35820(const VeneziaDebugImageGeometry *geometry, SceSize *size)
{
	VeneziaVipProcessStorage *storage = _sceVipGetVipProcessResource(0);

	unsigned int index;

	if (!storage)
		return -2141060856;
	index = storage->count & 15;
	if (storage->count <= 1u)
		return g_veneziaVipCaptureOperations[index].getStorageSize(geometry, size);
	return -2141060856;
}

int SceCodecEngineWrapperForDriver_9D3B1425(unsigned int *processor, const VeneziaVipCaptureParam *param)
{
	VeneziaVipProcessStorage *storage = _sceVipGetVipProcessResource(0);

	unsigned int index;

	if (!storage)
		return -2141060856;
	index = storage->count & 15;
	if (storage->count <= 1u)
		return g_veneziaVipCaptureOperations[index].registerCapture(processor, param);
	return -2141060856;
}

int SceCodecEngineWrapperForDriver_B280BA55(unsigned int processor, int wait, SceUID pid)
{
	VeneziaVipProcessStorage *storage = _sceVipGetVipProcessResource(pid);

	unsigned int index;

	if (!storage)
		return -2141060856;
	index = storage->count & 15;
	if (storage->count <= 1u)
		return g_veneziaVipCaptureOperations[index].unregisterCapture(processor, wait, pid);
	return -2141060856;
}

int SceCodecEngineWrapperForDriver_6A3638A8(unsigned int processor)
{
	VeneziaVipProcessStorage *storage = _sceVipGetVipProcessResource(0);

	unsigned int index;

	if (!storage)
		return -2141060856;
	index = storage->count & 15;
	if (storage->count <= 1u)
		return g_veneziaVipCaptureOperations[index].getTraceAvailable(processor);
	return -2141060856;
}

int SceCodecEngineWrapperForDriver_8DEF9631(unsigned int processor, unsigned int control)
{
	VeneziaVipProcessStorage *storage = _sceVipGetVipProcessResource(0);

	unsigned int index;

	if (!storage)
		return -2141060856;
	index = storage->count & 15;
	if (storage->count <= 1u)
		return g_veneziaVipCaptureOperations[index].configureControl(processor, control);
	return -2141060856;
}

int SceCodecEngineWrapperForDriver_BB02EB22(unsigned int processor, const VeneziaVipRawInput *input, VeneziaDebugSnapshotList *list, unsigned int *completed)
{
	VeneziaVipProcessStorage *storage = _sceVipGetVipProcessResource(0);

	unsigned int index;

	if (!storage)
		return -2141060856;
	index = storage->count & 15;
	if (storage->count <= 1u)
		return g_veneziaVipCaptureOperations[index].processRaw(processor, input, list, completed);
	return -2141060856;
}

int SceCodecEngineWrapperForDriver_B420ABAC(unsigned int processor, const VeneziaVipRawInput *input, const SceUInt *timeout)
{
	VeneziaVipProcessStorage *storage = _sceVipGetVipProcessResource(0);

	unsigned int index;

	if (!storage)
		return -2141060856;
	index = storage->count & 15;
	if (storage->count <= 1u)
		return g_veneziaVipCaptureOperations[index].submitRaw(processor, input, timeout);
	return -2141060856;
}

int SceCodecEngineWrapperForDriver_30F6AFD4(unsigned int processor, VeneziaDebugSnapshotList *list, unsigned int *completed, const SceUInt *timeout)
{
	VeneziaVipProcessStorage *storage = _sceVipGetVipProcessResource(0);

	unsigned int index;

	if (!storage)
		return -2141060856;
	index = storage->count & 15;
	if (storage->count <= 1u)
		return g_veneziaVipCaptureOperations[index].wait(processor, list, completed, timeout);
	return -2141060856;
}

int SceCodecEngineWrapperForDriver_BB388700(unsigned int processor, const VeneziaVipVideoInput *input, VeneziaDebugSnapshotList *list, unsigned int *completed)
{
	VeneziaVipProcessStorage *storage = _sceVipGetVipProcessResource(0);

	unsigned int index;

	if (!storage)
		return -2141060856;
	index = storage->count & 15;
	if (storage->count <= 1u)
		return g_veneziaVipCaptureOperations[index].process(processor, input, list, completed);
	return -2141060856;
}

int SceCodecEngineWrapperForDriver_972F0D0F(unsigned int processor, const VeneziaVipVideoInput *input, const SceUInt *timeout)
{
	VeneziaVipProcessStorage *storage = _sceVipGetVipProcessResource(0);

	unsigned int index;

	if (!storage)
		return -2141060856;
	index = storage->count & 15;
	if (storage->count <= 1u)
		return g_veneziaVipCaptureOperations[index].submit(processor, input, timeout);
	return -2141060856;
}

int SceCodecEngineWrapperForDriver_B48B02A3(unsigned int processor, VeneziaDebugSnapshotList *list, unsigned int *completed)
{
	VeneziaVipProcessStorage *storage = _sceVipGetVipProcessResource(0);

	unsigned int index;

	if (!storage)
		return -2141060856;
	index = storage->count & 15;
	if (storage->count <= 1u)
		return g_veneziaVipCaptureOperations[index].drain(processor, list, completed);
	return -2141060856;
}

int SceCodecEngineWrapperForDriver_9EB3D63E(unsigned int processor)
{
	VeneziaVipProcessStorage *storage = _sceVipGetVipProcessResource(0);

	unsigned int index;

	if (!storage)
		return -2141060856;
	index = storage->count & 15;
	if (storage->count <= 1u)
		return g_veneziaVipCaptureOperations[index].reset(processor);
	return -2141060856;
}

int SceCodecEngineWrapperForDriver_0D140628(unsigned int processor)
{
	VeneziaVipProcessStorage *storage = _sceVipGetVipProcessResource(0);

	unsigned int index;

	if (!storage)
		return -2141060856;
	index = storage->count & 15;
	if (storage->count <= 1u)
		return g_veneziaVipCaptureOperations[index].consume(processor);
	return -2141060856;
}

int SceCodecEngineWrapperForDriver_16506603(unsigned int processor, SceUID uid, uint32_t eventMask)
{
	VeneziaVipProcessStorage *storage = _sceVipGetVipProcessResource(0);

	unsigned int index;

	if (!storage)
		return -2141060856;
	index = storage->count & 15;
	if (storage->count <= 1u)
		return g_veneziaVipCaptureOperations[index].registerCallback(processor, uid, eventMask);
	return -2141060856;
}

int SceCodecEngineWrapperForDriver_8FE370E6(unsigned int processor, SceUID uid)
{
	VeneziaVipProcessStorage *storage = _sceVipGetVipProcessResource(0);

	unsigned int index;

	if (!storage)
		return -2141060856;
	index = storage->count & 15;
	if (storage->count <= 1u)
		return g_veneziaVipCaptureOperations[index].unregisterCallback(processor, uid);
	return -2141060856;
}

_Static_assert(sizeof(CodecEngineMemory) == 52, "CodecEngine memory object size");

int SceCodecEngineWrapperForDebugger_771190E7(unsigned int command,
	VeneziaRpcHistoryEntry *history)
{
	if (!history)
		return (int)0x80600000;
	if (command == 0x1000)
		return _sceVeneziaRpcGetHistory(history);
	return (int)0x80600009;
}

SceUID _sceCodecEngineOpenUnmapMemBlock(void *base, SceSize size)
{
	return sceCodecEngineOpenUnmapMemBlockForUser(base, size);
}

int _sceCodecEngineCloseUnmapMemBlock(SceUID puid)
{
	return _closeUnmapMemBlock(puid);
}

void *_sceCodecEngineAllocMemoryFromUnmapMemBlock(SceUID puid, SceSize size,
	SceSize alignment)
{
	return sceCodecEngineAllocMemoryFromUnmapMemBlockForUser(puid, size, alignment);
}

int _sceCodecEngineFreeMemoryFromUnmapMemBlock(SceUID puid, void *address)
{
	return sceCodecEngineFreeMemoryFromUnmapMemBlockForUser(puid, address);
}

int _sceCodecEngineChangeNumWorkerCoresDefault(void)
{
	return sceCodecEngineChangeNumWorkerCoresDefaultForUser();
}

int _sceCodecEngineChangeNumWorkerCoresMax(void)
{
	return sceCodecEngineChangeNumWorkerCoresMaxForUser();
}

/* name unknown: preserve the original non-exported monitor wrapper. */
__attribute__((noinline)) int startCodecEnginePerformanceMonitorThunk(void)
{
	return startCodecEnginePerformanceMonitor();
}

/* name unknown: preserve the original non-exported monitor wrapper. */
__attribute__((noinline)) int stopCodecEnginePerformanceMonitorThunk(void)
{
	return stopCodecEnginePerformanceMonitor();
}

/* name unknown: compact CPU-load output contains its size and average load. */
typedef struct CodecEngineProcessorLoad {
	SceSize size;
	unsigned int average;
} CodecEngineProcessorLoad;

/* name unknown: expose the average from the complete DSP performance result. */
__attribute__((noinline)) int getCodecEngineAverageProcessorLoad(CodecEngineProcessorLoad *load)
{
	VeneziaPerformanceResult result;
	int ret;

	if (!load)
		return (int)0x80600000;
	if (load->size != sizeof(*load))
		return (int)0x80600001;
	ret = getCodecEnginePerformanceResults(&result);
	load->average = ret < 0 ? 0 : result.average;
	return ret;
}

/* name unknown: preserve the original counter-reset wrapper. */
__attribute__((noinline)) int clearCodecEnginePerformanceCountersThunk(void)
{
	return clearCodecEnginePerformanceCounters();
}

/* name unknown: this firmware reports no accumulated RPC calls. */
__attribute__((noinline)) int getCodecEngineRpcCallCount(void)
{
	return 0;
}

/* name unknown: this firmware has no RPC-call counter to reset. */
__attribute__((noinline)) int resetCodecEngineRpcCallCount(void)
{
	return 0;
}

/* name unknown: this firmware returns an all-zero processor-load record. */
__attribute__((noinline)) int getCodecEngineProcessorLoad(void *load)
{
	if (load)
		memset(load, 0, 40);
	return 0;
}

/* name unknown: this firmware returns an all-zero memory-state record. */
__attribute__((noinline)) int getCodecEngineMemoryState(void *state)
{
	if (state)
		memset(state, 0, 24);
	return 0;
}

/* name unknown: worker-core changes are inactive in this firmware. */
__attribute__((noinline)) int changeCodecEngineWorkerCoreCount(unsigned int cores)
{
	return 0;
}

/* name unknown: clock-frequency changes are inactive in this firmware. */
__attribute__((noinline)) int setCodecEngineClockFrequency(unsigned int frequency)
{
	return 0;
}

int _sceCodecEnginePmonStart(void)
{
	return startCodecEnginePerformanceMonitorThunk();
}

int _sceCodecEnginePmonStop(void)
{
	return stopCodecEnginePerformanceMonitorThunk();
}

int _sceCodecEnginePmonGetProcessorLoad(CodecEngineProcessorLoad *load,
	const CodecEngineProcessorLoad *descriptor)
{
	CodecEngineProcessorLoad request;
	CodecEngineProcessorLoad *result;
	int ret, error;

	ret = sceKernelCopyFromUser(&request, descriptor, sizeof(request));
	if (ret < 0)
		return ret;
	if (!load)
		return getCodecEngineAverageProcessorLoad(NULL);
	if (request.size > sizeof(*result))
		return (int)0x8002710b;
	result = __builtin_alloca(sizeof(*result));
	result->size = 0;
	result->average = 0;
	ret = sceKernelCopyFromUser(result, load, request.size);
	if (ret < 0)
		return ret;
	ret = getCodecEngineAverageProcessorLoad(result);
	error = sceKernelCopyToUser(load, result, request.size);
	if (error < 0)
		ret = error;
	return ret;
}

int _sceCodecEnginePmonReset(void)
{
	return clearCodecEnginePerformanceCountersThunk();
}

int _sceCodecEngineGetNumRpcCalled(void)
{
	return getCodecEngineRpcCallCount();
}

int _sceCodecEngineResetNumRpcCalled(void)
{
	return resetCodecEngineRpcCallCount();
}

int _sceCodecEngineGetProcessorLoad(void *load)
{
	void *result;
	int ret, error;

	if (!load)
		return getCodecEngineProcessorLoad(NULL);
	result = __builtin_alloca(40);
	__builtin_memset(result, 0, 40);
	ret = getCodecEngineProcessorLoad(result);
	error = sceKernelCopyToUser(load, result, 40);
	if (error < 0)
		ret = error;
	return ret;
}

int _sceCodecEngineGetMemoryState(void *state)
{
	void *result;
	int ret, error;

	if (!state)
		return getCodecEngineMemoryState(NULL);
	result = __builtin_alloca(24);
	__builtin_memset(result, 0, 24);
	ret = getCodecEngineMemoryState(result);
	error = sceKernelCopyToUser(state, result, 24);
	if (error < 0)
		ret = error;
	return ret;
}

int _sceCodecEngineChangeNumWorkerCores(unsigned int cores)
{
	return changeCodecEngineWorkerCoreCount(cores);
}

int _sceCodecEngineSetClockFrequency(unsigned int frequency)
{
	return setCodecEngineClockFrequency(frequency);
}
