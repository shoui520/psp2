/*
 * Robin firmware system core (0x000003aa .. 0x000006f6): interrupt line
 * registry and dispatch, the fatal-error handler, the general-purpose timer
 * interrupt, and five ThreadX timer slots.
 *
 * Additional subsystem services are declared in robin/sys.h while their
 * implementations are converted in original address order.
 */
#include <stddef.h>
#include "robin/rom.h"
#include "robin/soc.h"
#include "robin/sys.h"
#include "robin/threadx.h"
#include "robin/event.h"

#define BIT(n)	(1u << (n))

struct SysIrqLine {
	SysIrqHandler handler;
	uint32_t line;
};

/* One expiry record per timer slot, passed to sysTimerExpired(). */
struct SysTimerSlot {
	uint32_t index;
	uint32_t created;
	SysTimerCallback callback;
	uint32_t arg;
};

int sysOsTimerIrq(int line, uint32_t pending);
static int sysSdioIrq(int line, uint32_t pending);
static int sysBbpEventAIrq(int line, uint32_t pending);
static int sysBbpEventBIrq(int line, uint32_t pending);

/* Set by the host-side debugger once it has read sysFatalDumpRegs. */
volatile uint8_t sysFatalDumpAck __attribute__((section(".dtcm.rw")));

/* Lines unmasked by sysIrqEnableAll(), terminated by a NULL handler. */
static struct SysIrqLine sysIrqLineTable[] __attribute__((section(".dtcm.rw"))) = {
	{ sysOsTimerIrq,	 4 },
	{ sysOsTimerIrq,	 6 },
	{ sysOsTimerIrq,	 7 },
	{ sysSdioIrq,		 8 },
	{ sysBbpEventAIrq,	15 },
	{ sysBbpEventBIrq,	16 },
	{ irq17Handler,		17 },
	{ irq22Handler,		22 },
	{ irq30Handler,		30 },
	{ irq28Handler,		28 },
	{ NULL,			 0 },
};

static SysIrqHandler sysIrqHandlers[SOC_IRQ_LINE_COUNT] __attribute__((section(".dtcm.bss")));

static TX_TIMER sysTimers[SYS_TIMER_COUNT] __attribute__((section(".dtcm.bss")));
static TX_TIMER *sysTimerTable[SYS_TIMER_COUNT] __attribute__((section(".dtcm.bss")));
static struct SysTimerSlot sysTimerSlots[SYS_TIMER_COUNT] __attribute__((section(".dtcm.bss")));

/* Indirect registers 3..14 captured by sysFatalHandler(). */
uint32_t sysFatalDumpRegs[12] __attribute__((section(".sqram1.bss")));

static uint32_t sysPendingEventDelay __attribute__((section(".itcm.rw"))) = 22000;
uint32_t sysPendingEventFlags __attribute__((section(".itcm.rw"))) = 0;
uint32_t sysPendingEventTimer __attribute__((section(".itcm.rw"))) = 0;
uint8_t sysSelectedEventPath __attribute__((section(".itcm.rw"))) = 0xff;
uint32_t sysEventPolicyInitialized __attribute__((section(".itcm.rw"))) = 0;

/* A host-controlled override that promotes the completion to an event. */
uint32_t sysForceEventDispatch __attribute__((section(".itcm.rw"))) = 1;

/* Arms the fatal line: indirect registers 0 and 1 (0x000003aa). */
static void sysFatalIrqArm(void)
{
	SOC_INDIRECT_INDEX = SOC_INDIRECT_WRITE | 0;
	SOC_INDIRECT_DATA = 0xff000007;
	SOC_INDIRECT_INDEX = SOC_INDIRECT_WRITE | 1;
	SOC_INDIRECT_DATA = 5;
}

/*
 * Line 23 handler, also called from the undefined-instruction vector
 * (0x000003c0). Without a pending fatal line it halts. Otherwise it captures
 * indirect registers 3..14 for the host, waits for the host to acknowledge,
 * and reprograms indirect registers 15, 0 and 1.
 */
int sysFatalHandler(void)
{
	int i;

	if (!(SOC_IRQ_RAW_STATUS & BIT(SOC_IRQ_LINE_FATAL)))
		for (;;)
			;

	for (i = 0; i < 12; i += 2) {
		SOC_INDIRECT_INDEX = i + 3;
		sysFatalDumpRegs[i] = SOC_INDIRECT_DATA;
		SOC_INDIRECT_INDEX = i + 4;
		sysFatalDumpRegs[i + 1] = SOC_INDIRECT_DATA;
	}

	while (sysFatalDumpAck == 0)
		for (; i < 300; i++)
			;

	SOC_INDIRECT_INDEX = SOC_INDIRECT_WRITE | 15;
	SOC_INDIRECT_DATA = 0xcff;
	SOC_INDIRECT_INDEX = SOC_INDIRECT_WRITE | 0;
	SOC_INDIRECT_DATA = 0xff800007;
	SOC_INDIRECT_INDEX = SOC_INDIRECT_WRITE | 1;
	SOC_INDIRECT_DATA = 5;
	return 1;
}

/* Init table entry (0x0000042a). */
int sysInit(void)
{
	osaInit();
	__aeabi_memclr4(sysIrqHandlers, sizeof(sysIrqHandlers));
	__aeabi_memclr4(sysTimers, sizeof(sysTimers));
	__aeabi_memclr4(sysTimerTable, sizeof(sysTimerTable));
	__aeabi_memclr4(sysTimerSlots, sizeof(sysTimerSlots));
	return 0;
}

/* Installs a line handler; a bad line or a taken line halts (0x00000458). */
void sysIrqRegister(uint32_t line, SysIrqHandler handler)
{
	if (line >= SOC_IRQ_LINE_COUNT)
		for (;;)
			;
	if (sysIrqHandlers[line] != NULL)
		for (;;)
			;
	sysIrqHandlers[line] = handler;
}

int sysIrqUnregister(uint32_t line)
{
	sysIrqHandlers[line] = NULL;
	return 0;
}

int sysInIsr(void)
{
	return _tx_thread_system_state != 0;
}

UINT sysIrqDisable(void)
{
	return tx_interrupt_control(TX_INT_DISABLE);
}

UINT sysIrqRestore(UINT posture)
{
	return tx_interrupt_control(posture);
}

/* ThreadX expiry function shared by all slots (0x0000049a). */
static void sysTimerExpired(ULONG input)
{
	struct SysTimerSlot *slot = (struct SysTimerSlot *)input;

	slot->callback(slot->index, slot->arg);
}

/*
 * Binds a slot to a callback (0x000004a4). The expiry function and its
 * input are parked in the TX_TIMER until sysTimerStart() creates it.
 */
int sysTimerRegister(uint32_t index, SysTimerCallback callback, uint32_t arg)
{
	TX_TIMER *timer;
	struct SysTimerSlot *slot;

	if (index >= SYS_TIMER_COUNT || sysTimerTable[index] != NULL)
		return 1;

	timer = &sysTimers[index];
	sysTimerTable[index] = timer;

	slot = &sysTimerSlots[index];
	slot->index = index;
	slot->arg = arg;
	slot->callback = callback;
	slot->created = 0;

	timer->tx_timer_internal.tx_timer_internal_timeout_function = sysTimerExpired;
	timer->tx_timer_internal.tx_timer_internal_timeout_param = (ULONG)slot;
	return 0;
}

/*
 * Starts a slot (0x000004e0): the first start creates the timer
 * auto-activated and returns 1; later starts reprogram it and return the
 * tx_timer_activate() status. An out-of-range index is returned as is.
 */
UINT sysTimerStart(uint32_t index, ULONG initial_ticks, ULONG reschedule_ticks)
{
	struct SysTimerSlot *slot;
	TX_TIMER *timer;
	UINT status;

	if (index >= SYS_TIMER_COUNT)
		return index;

	slot = &sysTimerSlots[index];
	timer = sysTimerTable[index];

	if (slot->created) {
		tx_timer_deactivate(timer);
		tx_timer_change(timer, initial_ticks, reschedule_ticks);
		return tx_timer_activate(timer);
	}

	status = tx_timer_create(timer, NULL,
				 timer->tx_timer_internal.tx_timer_internal_timeout_function,
				 timer->tx_timer_internal.tx_timer_internal_timeout_param,
				 initial_ticks, reschedule_ticks, TX_AUTO_ACTIVATE);
	if (status != TX_SUCCESS)
		return status;
	slot->created = 1;
	return 1;
}

UINT sysTimerStop(uint32_t index)
{
	if (index >= SYS_TIMER_COUNT)
		return index;
	return tx_timer_deactivate(sysTimerTable[index]);
}

ULONG sysGetTime(ULONG *now)
{
	ULONG ticks = tx_time_get();

	*now = ticks;
	return ticks;
}

/*
 * Lines 4, 6 and 7: the general-purpose timer block (0x00000554).
 * Timer 0 is the ThreadX tick.
 */
int sysOsTimerIrq(int line, uint32_t pending)
{
	uint32_t status = SOC_TIMER_INT_STATUS;

	if (status & BIT(0)) {
		_tx_timer_interrupt();
		osaTimer0Isr();
		SOC_TIMER_INT_STATUS = ~BIT(0);
	}
	if (status & BIT(1)) {
		SOC_TIMER_INT_ENABLE &= ~BIT(1);
		SOC_IRQ_ENABLE_CLEAR = BIT(5);
		SOC_TIMER_INT_STATUS = ~BIT(1);
		SOC_IRQ_ENABLE_SET |= BIT(5);
		osaTimer1Isr();
	}
	if (status & BIT(2)) {
		SOC_TIMER_INT_ENABLE &= ~BIT(2);
		osaTimer2Isr();
	}
	if (status & BIT(3)) {
		SOC_TIMER_INT_ENABLE &= ~BIT(3);
		SOC_IRQ_ENABLE_CLEAR = BIT(7);
		SOC_TIMER_INT_STATUS = ~BIT(3);
		SOC_IRQ_ENABLE_SET |= BIT(7);
	}
	return 1;
}

/*
 * Called from the ROM IRQ hook (0x000005b8). Line 1 is cascaded to the
 * line 22 handler; lines 4..7 are folded into line 4. Each handler gets its
 * line and the pending mask shifted down to that line.
 */
int sysIrqDispatch(void)
{
	uint32_t pending = SOC_IRQ_STATUS;
	int line;

	if (pending & BIT(1)) {
		SOC_IRQ_LINE1_ACK = 0;
		sysIrqHandlers[22](1, 0);
		pending &= ~BIT(1);
	}
	if (pending & 0xf0) {
		pending &= ~0xe0u;
		pending |= BIT(4);
	}

	for (line = 0; pending != 0; pending >>= 1, line++)
		if ((pending & 1) && sysIrqHandlers[line] != NULL)
			sysIrqHandlers[line](line, pending);
	return 0;
}

/* Line 8: SDIO function interrupt, forwarded to the open mdev (0x00000602). */
static int sysSdioIrq(int line, uint32_t pending)
{
	SOC_IRQ_ENABLE_CLEAR = BIT(8);
	mdevInterrupt(wlanSdioDev != NULL ? wlanSdioDev : btSdioDev, 0);
	SOC_IRQ_ENABLE_SET |= BIT(8);
	return 1;
}

int irq17Handler(int line, uint32_t pending)
{
	uint32_t mask = BIT(line);

	SOC_IRQ_ENABLE_CLEAR = mask;
	sysServiceWlanPackets();
	SOC_IRQ_ENABLE_SET |= mask;
	return 1;
}

static int sysBbpEventAIrq(int line, uint32_t pending)
{
	SOC_IRQ_ENABLE_CLEAR = BIT(15);
	(void)BBP_EVENT_A_STATUS;
	BBP_EVENT_A = 0;
	BBP_EVENT_A_STATUS = 0;
	SOC_IRQ_ENABLE_SET |= BIT(15);
	return 1;
}

static int sysBbpEventBIrq(int line, uint32_t pending)
{
	SOC_IRQ_ENABLE_CLEAR = BIT(16);
	(void)BBP_EVENT_B_STATUS;
	BBP_EVENT_B = 0;
	BBP_EVENT_B_STATUS = 0;
	SOC_IRQ_ENABLE_SET |= BIT(16);
	return 1;
}

int irq30Handler(int line, uint32_t pending)
{
	uint32_t mask = BIT(line);
	uint32_t status;

	SOC_IRQ_ENABLE_CLEAR = mask;
	status = sysReadInterruptStatus();
	if (status & BIT(3)) {
		sysProcessPendingEvent();
		sysCompletePendingEvent();
	}
	sysDisableInterruptSources(status);
	sysAcknowledgeInterruptSources(status);
	SOC_IRQ_ENABLE_SET |= mask;
	return 1;
}

/* Init table entry (0x000006ba): installs and unmasks every line. */
int sysIrqEnableAll(void)
{
	struct SysIrqLine *entry;

	for (entry = sysIrqLineTable; entry->handler != NULL; entry++) {
		sysIrqRegister(entry->line, entry->handler);
		SOC_IRQ_ENABLE_SET = BIT(entry->line);
	}

	sysIrqRegister(SOC_IRQ_LINE_FATAL, (SysIrqHandler)sysFatalHandler);
	sysFatalIrqArm();
	SOC_IRQ_ENABLE_SET = BIT(SOC_IRQ_LINE_FATAL);
	return 0;
}

uint32_t sysServiceEventInterrupts(void)
{
	uint32_t pending = SOC_EVENT_STATUS;
	uint32_t events;

	if (pending & BIT(0)) {
		SOC_EVENT_STATUS = ~BIT(0);
		pending &= ~BIT(0);
	}
	if (pending & BIT(2)) {
		SOC_EVENT_STATUS = ~BIT(2);
		pending &= ~BIT(2);
	}
	if (pending & BIT(7)) {
		SOC_EVENT_STATUS = ~BIT(7);
		pending &= ~BIT(7);
		sysDisableEventSources(BIT(7));
	}
	if (pending & BIT(3)) {
		SOC_EVENT_STATUS = ~BIT(3);
		pending &= ~BIT(3);
		sysUpdateEventDeadline();
		sysHandlePrimaryEvent();
	}
	if (pending & BIT(5)) {
		SOC_EVENT_STATUS = ~BIT(5);
		pending &= ~BIT(5);
		sysHandleSecondaryEvent();
	}
	if (pending & BIT(15)) {
		sysDisableEventSources(BIT(15));
		SOC_EVENT_STATUS = ~BIT(15);
		pending &= ~BIT(15);
	}
	if (pending & 0x00070000u) {
		SOC_EVENT_STATUS = ~(pending & 0x00070000u);
		pending &= ~0x00070000u;
	}
	if (pending & 0x00f00000u) {
		if (pending & BIT(23)) {
			SOC_EVENT_CONTROL |= BIT(19);
			if (sysForceEventDispatch || !(SOC_EVENT_STATE & BIT(1)))
				pending |= BIT(20);
			SOC_EVENT_LATCH = 0;
			sysSetEventFlags(BIT(7));
			sysDisableEventSources(BIT(23));
		}
		events = pending & 0x00f00000u;
		sysDispatchMiddleEventBits(events);
		SOC_EVENT_STATUS = ~events;
		pending &= ~0x00f00000u;
	}
	if (pending & 0x1c000000u) {
		events = pending & 0x1c000000u;
		sysDispatchHighEventBits(events);
		SOC_EVENT_STATUS = ~events;
		pending &= ~0x1c000000u;
	}
	if (pending & BIT(4)) {
		SOC_EVENT_STATUS = ~BIT(4);
		pending &= ~BIT(4);
		if (sysEventEngineIsActive() && sysPrepareEventDeadline())
			sysProcessEventQueues(BIT(4));
	}
	if (pending & BIT(30)) {
		SOC_EVENT_STATUS = ~BIT(30);
		pending &= ~BIT(30);
	}
	if (pending & BIT(29)) {
		SOC_EVENT_STATUS = ~BIT(29);
		pending &= ~BIT(29);
	}
	if (pending & BIT(31)) {
		sysStopEventEngine();
		SOC_EVENT_STATUS = ~BIT(31);
	}
	events = SOC_IRQ_ENABLE_SET | BIT(22);
	SOC_IRQ_ENABLE_SET = events;
	return events;
}

int irq22Handler(int line, uint32_t pending)
{
	SOC_IRQ_ENABLE_CLEAR = BIT(22);
	sysServiceEventInterrupts();
	return 1;
}

int irq28Handler(int line, uint32_t pending)
{
	SOC_IRQ_ENABLE_CLEAR = BIT(28);
	sysServiceIndirectStatus();
	SOC_IRQ_ENABLE_SET |= BIT(28);
	return 1;
}

uint32_t sysSubmitBbpCommand(uint32_t target, uint32_t value, uint32_t command)
{
	if (command == 0)
		return 0;
	sysIssueBbpCommand(target, value, command);
	return target;
}

void sysDispatchHighEventBits(uint32_t events)
{
	if (events & BIT(28))
		sysDispatchEventSource(12, BIT(28), BIT(12));
	if (events & BIT(27))
		sysDispatchEventSource(11, BIT(27), BIT(11));
	if (events & BIT(26))
		sysDispatchEventSource(10, BIT(26), BIT(10));
}

uint32_t sysStopEventEngine(void)
{
	uint32_t control;

	sysSetEventFlags(BIT(15));
	sysDisableEventSources(BIT(31));
	control = SOC_EVENT_ENGINE_GATE & ~BIT(9);
	SOC_EVENT_ENGINE_GATE = control;
	return control;
}

void sysDispatchMiddleEventBits(uint32_t events)
{
	uint32_t index;
	uint32_t mapping;

	if (events & BIT(20))
		sysHandleMappedEventSource(2, 1);
	if (events & BIT(21))
		sysHandleMappedEventSource(3, 1);
	if (events & BIT(22))
		sysHandleMappedEventSource(4, 1);
	if (events & BIT(23))
		sysHandleMappedEventSource(5, 1);
	switch (events) {
	case BIT(28):
		index = 12;
		break;
	case BIT(27):
		index = 11;
		break;
	case BIT(26):
		index = 10;
		break;
	default:
		return;
	}
	mapping = sysGetEventSourceMapping(index);
	if (mapping == 2 || mapping == 3 || mapping == 4 || mapping == 5)
		sysHandleMappedEventSource(mapping, 1);
}

uint32_t sysReadInterruptStatus(void)
{
	return SOC_SOURCE_IRQ_STATUS;
}

uint32_t sysEnableInterruptSources(uint32_t sources)
{
	SOC_SOURCE_IRQ_ENABLE |= sources;
	SOC_SOURCE_IRQ_SECONDARY_ENABLE |= sources;
	return sources;
}

uint32_t sysDisableInterruptSources(uint32_t sources)
{
	SOC_SOURCE_IRQ_ENABLE &= ~sources;
	return sources;
}

uint32_t sysAcknowledgeInterruptSources(uint32_t sources)
{
	uint32_t acknowledge = ~sources;

	SOC_SOURCE_IRQ_STATUS = acknowledge;
	return acknowledge;
}

int sysDisableEventSources(uint32_t sources)
{
	SOC_EVENT_SOURCE_ENABLE &= ~sources;
	SOC_EVENT_SECONDARY_ENABLE &= ~sources;
	return 1;
}

int sysAcknowledgeEvents(uint32_t events)
{
	SOC_EVENT_STATUS = ~events;
	return 1;
}

SysBbpCompletionHook sysBbpCompletionHook
	__attribute__((section(".itcm.rw"))) = 0;

volatile uint32_t *sysWaitForBbpCommand(void)
{
	do {
		while (BBP_EVENT_B & BIT(14))
			;
	} while (BBP_SECONDARY_COMMAND);
	return &BBP_EVENT_A;
}

SysBbpCompletionHook socEventHookInit(void)
{
	sysBbpCompletionHook = sysWaitForBbpCommand;
	return sysWaitForBbpCommand;
}

int sysIssueBbpCommand(uint32_t target, uint32_t value, uint32_t command)
{
	return bbpIssueSecondaryCommand(target, value, command);
}

int sysIssuePrimaryBbpCommand(uint32_t target, uint32_t value, uint32_t command)
{
	return bbpIssuePrimaryCommand(target, value, command);
}

int sysPendingEventIsArmed(void)
{
	return sysPendingEventTimer != 0;
}

int sysProcessPendingEvent(void)
{
	int result = sysEventGateIsSet();

	if (result != 0) {
		result = sysPendingEventIsArmed();
		if (result != 0) {
			SOC_PENDING_EVENT_CONTROL &= ~BIT(1);
			return sysDispatchPendingEventMode(sysPendingEventFlags & BIT(31));
		}
	}
	return result;
}

void sysPendingEventExpired(uint32_t timer, uint32_t unusedArgument)
{
	if (sysPendingEventTimer != timer)
		return;
	sysProcessPendingEvent();
	sysDisableInterruptSources(sysPendingEventFlags & 0x7fff);
	sysAcknowledgeInterruptSources(sysPendingEventFlags & 0x7fff);
	sysPendingEventTimer = 0;
	sysPendingEventFlags = 0;
}

int sysArmPendingEvent(uint32_t flags, uint32_t delay)
{
	sysPendingEventFlags |= flags;
	return osaCreateTimer(sysPendingEventExpired, 0, delay,
			      &sysPendingEventTimer, 8);
}

int sysDeferPendingEvent(int dispatchMode)
{
	uint32_t flags = 8;

	sysAcknowledgeInterruptSources(8);
	if ((SOC_PENDING_EVENT_STATUS & (1u << 30)) ||
	    !sysPendingEventPolicyIsEnabled()) {
		SOC_PENDING_EVENT_CONTROL &= ~2u;
		return 1;
	}
	sysEnableInterruptSources(8);
	if (!dispatchMode)
		flags |= 1u << 31;
	sysArmPendingEvent(flags, sysPendingEventDelay);
	return 0;
}

void sysEnablePendingEvent(void)
{
	SOC_PENDING_EVENT_CONTROL |= 2;
}

uint32_t sysReadPendingEventEnable(void)
{
	return SOC_PENDING_EVENT_CONTROL & 2;
}

int sysCompletePendingEvent(void)
{
	uint32_t timer;

	sysDisableInterruptSources(BIT(3));
	sysAcknowledgeInterruptSources(BIT(3));
	timer = sysPendingEventTimer;
	if (timer != 0) {
		osaCancelTimer(timer);
		sysPendingEventTimer = 0;
		sysPendingEventFlags = 0;
	}
	return 0;
}

uint32_t sysSetPendingEventMode(int enabled)
{
	uint32_t control = SOC_PENDING_EVENT_CONTROL;

	if (enabled)
		control |= 8;
	else
		control &= ~8u;
	SOC_PENDING_EVENT_CONTROL = control;
	return control;
}

uint8_t *sysResetEventPolicy(void)
{
	sysEventPolicyInitialized = 0;
	sysSelectedEventPath = 0xff;
	return &sysSelectedEventPath;
}

uint32_t sysSetEventPathEnabled(uint32_t unusedPath, int enabled)
{
	uint32_t control;

	if (enabled) {
		SOC_MAC_EVENT_PATH_CONTROL |= 0x100;
		SOC_MAC_EVENT_PATH_CONTROL &= ~0x40000u;
		SOC_EVENT_PATH_CONTROL |= 0x3000;
		control = SOC_MAC_EVENT_CONTROL | 0x2000;
	} else {
		SOC_MAC_EVENT_PATH_CONTROL &= ~0x100u;
		SOC_MAC_EVENT_PATH_CONTROL |= 0x40000;
		SOC_EVENT_PATH_CONTROL &= ~0x3000u;
		control = SOC_MAC_EVENT_CONTROL & ~0x2000u;
	}
	SOC_MAC_EVENT_CONTROL = control;
	return control;
}

int sysEventPathIsEnabled(uint32_t path)
{
	struct MacEventPolicy policy;

	if (!macGetEventPolicy(&policy))
		return path == 0;
	if (path == 0)
		return (policy.flags & MAC_EVENT_PRIMARY_FLAGS) ==
			MAC_EVENT_PRIMARY_FLAGS && (policy.pathStates & 0xf) == 1;
	if (path == 1)
		return (policy.flags & MAC_EVENT_SECONDARY_FLAGS) ==
			MAC_EVENT_SECONDARY_FLAGS && (policy.pathStates >> 4) == 1;
	return 0;
}

uint32_t sysApplyEventPolicy(uint32_t path, struct MacEventPolicy policy)
{
	int enabled;

	if ((policy.flags & 4) && !sysEventPolicyInitialized) {
		macApplyEventRegisterDefaults((volatile void *)SOC_MAC_REGISTER_BASE);
		sysEventPolicyInitialized = 1;
	}
	enabled = sysEventPathIsEnabled(path);
	sysSetEventPathEnabled(path, enabled);
	return sysSetPendingEventMode((policy.flags & 4) || (policy.mode >> 4));
}

void sysSetEventPathPattern(uint32_t path, struct MacEventPolicy policy)
{
	uint32_t state;

	if (path == 0)
		state = policy.pathStates & 0xf;
	else if (path == 1)
		state = policy.pathStates >> 4;
	else
		return;

	switch (state) {
	case 0:
		SOC_MAC_EVENT_PRIMARY_PATTERN = 0xdd55dd11;
		SOC_MAC_EVENT_SECONDARY_PATTERN = 0xdd55dd11;
		SOC_MAC_EVENT_PRIMARY_MASK = ~0xdd55dd11u;
		SOC_MAC_EVENT_SECONDARY_MASK = ~0xdd55dd11u;
		break;
	case 1:
		SOC_MAC_EVENT_PRIMARY_PATTERN = 0xdf5fdf1f;
		SOC_MAC_EVENT_SECONDARY_PATTERN = 0xdd55dd11;
		SOC_MAC_EVENT_PRIMARY_MASK = 0x2faf2fef;
		SOC_MAC_EVENT_SECONDARY_MASK = ~0xdd55dd11u;
		break;
	case 9:
		SOC_MAC_EVENT_PRIMARY_PATTERN = 0xdf5fdf1f;
		SOC_MAC_EVENT_SECONDARY_PATTERN = 0xfdf5fdf1;
		SOC_MAC_EVENT_PRIMARY_MASK = 0x2faf2fef;
		SOC_MAC_EVENT_SECONDARY_MASK = 0xf2faf2fe;
		break;
	case 15:
		SOC_MAC_EVENT_PRIMARY_PATTERN = UINT32_MAX;
		SOC_MAC_EVENT_SECONDARY_PATTERN = UINT32_MAX;
		SOC_MAC_EVENT_PRIMARY_MASK = UINT32_MAX;
		SOC_MAC_EVENT_SECONDARY_MASK = UINT32_MAX;
		break;
	}
}

void sysConfigureEventPath(const struct MacEventPathConfiguration *configuration,
			   int toggleEnabled, int toggleActive,
			   uint32_t pathStates, int mode)
{
	uint32_t toggleMask = configuration->toggleMask;
	uint32_t control = configuration->controlTogglePattern;
	uint32_t primary = configuration->primaryTogglePattern;
	uint32_t secondary = configuration->secondaryTogglePattern;

	if (configuration) {
		if (toggleEnabled == 1 && mode == 1) {
			if (toggleActive == 1) {
				control ^= toggleMask;
				if (pathStates & 1)
					primary ^= toggleMask;
				if (pathStates & 2)
					secondary ^= toggleMask;
			}
			toggleMask = control;
		}
		SOC_MAC_EVENT_CONTROL_PATTERN = (control << 24) |
			(configuration->controlPattern << 16) |
			(toggleMask << 8) | toggleMask;
		SOC_MAC_EVENT_PRIMARY_CONFIGURATION =
			(configuration->primaryControlPattern << 16) |
			(configuration->primaryPattern << 8) | primary;
		SOC_MAC_EVENT_SECONDARY_CONFIGURATION =
			(configuration->secondaryControlPattern << 16) |
			(secondary << 8) | configuration->secondaryPattern;
	}
}

int sysInitializeEventPath(uint32_t path)
{
	struct MacEventPathConfiguration configuration;
	struct MacEventPolicy policy;

	policy.mode = 0;
	if (path == 0) {
		policy.flags = 0x45;
		policy.pathStates = 1;
		configuration.controlPattern = 1;
		configuration.controlTogglePattern = 1;
		configuration.secondaryPattern = 2;
		configuration.primaryPattern = 4;
		configuration.primaryTogglePattern = 1;
		configuration.secondaryTogglePattern = 1;
		configuration.primaryControlPattern = 4;
		configuration.secondaryControlPattern = 4;
		configuration.toggleMask = 1;
	}
	/* The original only initializes the remaining fields for path zero. */
	sysApplyEventPolicy(path, policy);
	sysSetEventPathPattern(path, policy);
	sysConfigureEventPath(&configuration, 0, 0, 0, 0);
	return 1;
}

int sysSelectEventPath(uint32_t path)
{
	struct MacEventPathConfiguration configuration;
	struct MacEventPolicy policy;
	int toggleEnabled = 0;

	if (sysSelectedEventPath != path) {
		sysSelectedEventPath = path;
		if (!macGetEventPolicy(&policy))
			return sysInitializeEventPath(path);
		if ((policy.mode & 0xf) > 1 ||
		    !macGetEventPathConfiguration(path, &configuration))
			return 0;
		sysApplyEventPolicy(path, policy);
		sysSetEventPathPattern(path, policy);
		if ((policy.flags & 0x10) || (policy.flags & 0x20))
			toggleEnabled = 1;
		sysConfigureEventPath(&configuration, toggleEnabled, 0, 0,
				      policy.mode & 0xf);
	}
	return 1;
}
