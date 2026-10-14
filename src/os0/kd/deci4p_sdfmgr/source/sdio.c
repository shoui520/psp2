/* SDIO byte-stream transport and its per-core packet accounting. */
#include "sdfmgr_internal.h"
#include <sce/sdbgsdio.h>
#include <sce/intrmgr.h>

/* ARM context wrapper retained with the module's exception entries. */
extern int sdioIntrEntry(int code, void *context);

const uint32_t s_sdioInterrupts[4] = { 0xb0, 0xb1, 0xb2, 0xb3 };

#if FW_DEBUG
#define SDIO_DEBUG_INFO(_line, _lineHash, _funcHash, _func) ({ \
	static const SceKernelDebugInfo info = { \
		.fileHash = 0x01a46c1c, .lineHash = (_lineHash), \
		.funcHash = (_funcHash), .file = "if_sdio.c", \
		.line = (_line), .func = (_func), \
	}; \
	&info; \
})
#endif

/* Internal names inferred from the transport's callback table. */
int sdioReset(SdfMgrTransport *transport)
{
	uint32_t events = transport->events;
	uint32_t interrupts = transport->interruptFlags;

	transport->events = events & ~0x21u;
	transport->interruptFlags = interrupts & ~1u;
	return 0;
}

int sdioPause(SdfMgrTransport *transport)
{
	transport->events |= 0x10;
	return 0;
}

int sdioResume(SdfMgrTransport *transport)
{
	transport->events &= ~0x10u;
	return 0;
}

void sdioIdle(void)
{
}

void sdioEnableInterrupts(SdfMgrTransport *transport)
{
	ksceKernelEnableIntr(0xb0);
	ksceKernelEnableIntr(0xb1);
	ksceKernelEnableIntr(0xb2);
	ksceKernelEnableIntr(0xb3);
	transport->events |= 1;
	transport->interruptFlags |= 1;
}

void sdioDisableInterrupts(SdfMgrTransport *transport)
{
	(void)transport;
	ksceKernelDisableIntr(0xb0);
	ksceKernelDisableIntr(0xb1);
	ksceKernelDisableIntr(0xb2);
	ksceKernelDisableIntr(0xb3);
}

int recvStart(SdfMgrTransport *transport)
{
#if FW_DEBUG
	void *caller = __builtin_return_address(0);

	if (ksceKernelGetAssertLevel() > 0)
		ksceKernelAssertLevel(1, transport != NULL,
			SDIO_DEBUG_INFO(231, 0xfa73a99f, 0x050cf044, "recvStart"),
			caller);
#endif
	transport->events |= 0x100;
	return 0;
}

int recvPause(SdfMgrTransport *transport)
{
	transport->events |= 0x1000;
	ksceKernelDisableIntr(s_sdioInterrupts[ksceKernelCpuId()]);
	return 0;
}

int recvFinish(SdfMgrTransport *transport)
{
	uint32_t events = transport->events;
	uint32_t interrupts = transport->interruptFlags;

	transport->events = events & ~0x2100u;
	transport->interruptFlags = interrupts & ~0x100u;
	SceDeci4pSDfMgrForKernel_FFC95B63(&transport->receiveHeader, 0);
	if (!(transport->events & 0x1000))
		ksceKernelEnableIntr(s_sdioInterrupts[ksceKernelCpuId()]);
	transport->receiveSize = 0;
	transport->receiveProgress = 0;
	return 0;
}

int recvResume(SdfMgrTransport *transport)
{
	uint32_t events = transport->events;

	transport->events = events & ~0x1000u;
	if (!(events & 0x6000))
		ksceKernelEnableIntr(s_sdioInterrupts[ksceKernelCpuId()]);
	return 0;
}

void sdioKickSend(SdfMgrTransport *transport, const uint32_t *interruptFlags)
{
	unsigned state = ksceKernelCpuSuspendIntr();
	uint32_t events = transport->events;

	if (events & 1) {
		if (*interruptFlags & 1)
			transport->events = events | 0x20;
		else
			ksceKernelEnableIntr(s_sdioInterrupts[ksceKernelCpuId()]);
	}
	ksceKernelCpuResumeIntr(state);
}

int sendStart(SdfMgrTransport *transport)
{
	uint32_t events = transport->events;
	uint32_t interrupts = transport->interruptFlags;

	transport->sendProgress = 0;
	transport->events = events | 1;
	transport->interruptFlags = interrupts | 1;
	sdioKickSend(transport, &transport->interruptFlags);
	return 0;
}

void sdioDispatchEvents(SdfMgrTransport *transport)
{
	uint32_t events = transport->events;

	for (;;) {
		int progressed = 0;

		if (events & 0x40) {
			uint32_t count = transport->sendProgress;

			transport->events = events & ~0x40u;
			if (sceSDfMgrIf2MgrEvent(transport, 4, count, 0, 0) < 0)
				return;
			events = transport->events;
			if (events & 1) {
				sdioKickSend(transport, &transport->interruptFlags);
				events = transport->events;
			}
			progressed = 1;
		}
		/* Native checks a fresh word here, separately from its saved flags. */
		if (!(events & 0x10) &&
		    (((volatile SdfMgrTransport *)transport)->events & 0x21) == 0x21) {
			transport->events = events & ~0x20u;
			if (sceSDfMgrIf2MgrEvent(transport, 3,
					       (uintptr_t)transport, 0, 0) < 0)
				return;
			events = transport->events;
			progressed = 1;
		}
		if (events & 0x4000) {
			uint32_t count = transport->receiveProgress;

			transport->events = events & ~0x4000u;
			if (sceSDfMgrIf2MgrEvent(transport, 2, count, 0, 0) < 0)
				return;
			events = transport->events;
			progressed = 1;
		}
		if (!(events & 0x1000) && (events & 0x2000)) {
			SceSize size;

			transport->events = events & ~0x2000u;
			size = SceDeci4pSDfMgrForKernel_68903C1D(
							&transport->receiveHeader);
			if (sceSDfMgrIf2MgrEvent(transport, 1, size,
					transport->receiveHeader.protocol,
					transport->receiveHeader.dst) < 0)
				return;
			events = transport->events;
			continue;
		}
		if (!progressed)
			return;
	}
}

void _sdioRecv(SdfMgrTransport *transport)
{
	int result = SceSDbgSdioForKernel_2EC24C58(ksceKernelCpuId(),
			&transport->receiveHeader, sizeof(transport->receiveHeader));

	if (result < 0) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDIO_DEBUG_INFO(575, 0xa60bb46e, 0xe4007b60, "_sdioRecv"),
			"sceSDbgSdioRead failed (0x%08x)\n", result);
#endif
		return;
	}
	if (result > 0) {
		transport->events |= 0x2000;
		sdioDispatchEvents(transport);
	}
}

void sdioPollInterrupt(void)
{
	unsigned state = ksceKernelCpuSuspendIntr();
	unsigned cpu = ksceKernelCpuId();

	if (sceKernelGetIntrPendingStatusForDebugger(
			s_sdioInterrupts[ksceKernelCpuId()])) {
		_sdioRecv(&s_sdfMgrState.sdio[cpu]);
		sceKernelClearIntrPendingStatusForDebugger(
				s_sdioInterrupts[ksceKernelCpuId()]);
	}
	ksceKernelCpuResumeIntr(state);
}

int sdioPoll(SdfMgrTransport *transport)
{
	sdioPollInterrupt();
	sdioDispatchEvents(transport);
	return 0;
}

int sdioProcessReceive(SdfMgrTransport *transport)
{
	_sdioRecv(transport);
	return 0;
}

int recvRead(SdfMgrTransport *transport, void *buffer, SceSize size)
{
	SceSize total = transport->receiveSize;
	SceSize progress = transport->receiveProgress;
	SceSize count = total - progress;

	if (size < count)
		count = size;
	if (count) {
		SceSize done = 0;

		do {
			int result = SceSDbgSdioForKernel_2EC24C58(ksceKernelCpuId(),
					(uint8_t *)buffer + done, count - done);

			done += (uint32_t)result;
			if (result < 0) {
#if FW_DEBUG
				SceDeci4pSDfMgrForKernel_501CE61D(15,
					SDIO_DEBUG_INFO(285, 0x2af253cf, 0xf9d7eaa6,
							"recvRead"),
					"sceSDbgSdioRead failed (0x%08x)\n", result);
#endif
				return result;
			}
		} while (done < count);
		total = transport->receiveSize;
		progress = transport->receiveProgress;
	}
	progress += count;
	transport->receiveProgress = progress;
	transport->events |= progress < total ? 0x2000 : 0x4000;
	return (int)count;
}

int sendWrite(SdfMgrTransport *transport, const void *buffer, SceSize size)
{
	SceSize count;
	int result;

	if (transport->sendProgress == 0)
		SceDeci4pSDfMgrForKernel_5996E2DB(&transport->sendHeader, buffer,
						 sizeof(transport->sendHeader));
	count = SceDeci4pSDfMgrForKernel_68903C1D(&transport->sendHeader) -
		transport->sendProgress;
	if (size <= count)
		count = size;
	if (!count) {
		transport->events |= 0x40;
		transport->sendProgress =
			SceDeci4pSDfMgrForKernel_68903C1D(&transport->sendHeader);
		return 0;
	}
	result = SceSDbgSdioForKernel_7876806C(ksceKernelCpuId(), buffer, count);
	if (result < 0) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDIO_DEBUG_INFO(374, 0x1ae20d42, 0x8b71df9a, "sendWrite"),
			"sceSDbgSdioWrite failed (0x%08x)\n", result);
#endif
		return result;
	}
	/* Native accounts the requested count for every nonnegative write result. */
	transport->sendProgress += count;
	size = SceDeci4pSDfMgrForKernel_68903C1D(&transport->sendHeader);
	transport->events |= transport->sendProgress < size ? 0x20 : 0x40;
	return (int)count;
}

int sdioInit(void)
{
	SdfMgrTransport *transport = &s_sdfMgrState.sdio[ksceKernelCpuId()];
	unsigned code, target;
	int result;
#if FW_DEBUG
	void *caller = __builtin_return_address(0);
#endif

	transport->name = "SDIO";
	transport->beginReceive = recvStart;
	transport->receive = recvRead;
	transport->finishReceive = recvFinish;
	transport->beginSend = sendStart;
	transport->send = sendWrite;
	transport->reset = sdioReset;
	transport->poll = sdioPoll;
	transport->processReceive = sdioProcessReceive;
	transport->pauseReceive = recvPause;
	transport->resumeReceive = recvResume;
	transport->pause = sdioPause;
	transport->resume = sdioResume;
	transport->reserved = NULL;
	transport->idle = sdioIdle;
	transport->disableInterrupts = sdioDisableInterrupts;
	transport->enableInterrupts = sdioEnableInterrupts;
	code = s_sdioInterrupts[ksceKernelCpuId()];
	target = s_sdfMgrCpuTargets[ksceKernelCpuId()];
	result = ksceKernelRegisterIntrHandler(code, "SceSDbgSdio0Mbox", 0,
				       sdioIntrEntry, transport, 0x10, target, NULL);
	if (result < 0) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDIO_DEBUG_INFO(127, 0x55933570, 0x019bb6f4, "sdioInit"),
			"RegisterIntrHandler failed (0x%08x)\n", result);
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, 0,
				SDIO_DEBUG_INFO(128, 0x1daa83bf, 0x019bb6f4,
						"sdioInit"), caller);
#endif
		return result;
	}
	result = sdfMgrSetTransport(NULL, transport);
	if (result < 0) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDIO_DEBUG_INFO(135, 0x2de51a8b, 0x019bb6f4, "sdioInit"),
			"RegisterIf failed (0x%08x)\n", result);
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, 0,
				SDIO_DEBUG_INFO(136, 0xe1e27a59, 0x019bb6f4,
						"sdioInit"), caller);
#endif
		return result;
	}
	result = SceDeci4pSDfMgrForKernel_B22B42FB(sdioPollInterrupt);
	if (result < 0) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDIO_DEBUG_INFO(144, 0xdf928b90, 0x019bb6f4, "sdioInit"),
			"RegisterBreakHook failed (0x%08x)\n", result);
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, 0,
				SDIO_DEBUG_INFO(145, 0x18cfa0ac, 0x019bb6f4,
						"sdioInit"), caller);
#endif
		return result;
	}
	result = sceSDfMgrIf2MgrEvent(transport, 5, 0, 0, 0);
	if (result < 0) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDIO_DEBUG_INFO(153, 0x23d2bb8d, 0x019bb6f4, "sdioInit"),
			"If2MgrEvent failed (0x%08x)\n", result);
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, 0,
				SDIO_DEBUG_INFO(154, 0xf33d24d0, 0x019bb6f4,
						"sdioInit"), caller);
#endif
		return result;
	}
	transport->interruptFlags |= 1;
	result = ksceKernelEnableIntr(s_sdioInterrupts[ksceKernelCpuId()]);
	if (result < 0) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDIO_DEBUG_INFO(164, 0xbe3ff066, 0x019bb6f4, "sdioInit"),
			"sceKernelEnableIntr failed (0x%08x)\n", result);
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, 0,
				SDIO_DEBUG_INFO(165, 0x3476882a, 0x019bb6f4,
						"sdioInit"), caller);
#endif
		return result;
	}
	return 0;
}

int sdioIntrHandler(int code, SdfMgrTransport *transport)
{
	unsigned state = ksceKernelCpuSuspendIntr();
	SdfMgrDebugCpuState **debugCpu;
	SdfMgrDebugCpuState *records;

	if (s_sdioInterrupts[ksceKernelCpuId()] != (unsigned)code) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDIO_DEBUG_INFO(618, 0x6f6155cc, 0xef172c42,
					"sdioIntrHandler"), "Unknown intrcode\n");
#endif
		ksceKernelCpuResumeIntr(state);
		return -2;
	}
	_sdioRecv(transport);
	while (sdfMgrGetCpuFlags(4))
		sdfMgrPollCurrentBreakHooks();
	debugCpu = SceDeci4pSDfMgrForKernel_BFB482D8();
	if (debugCpu && (records = *debugCpu) != NULL &&
	    records[ksceKernelCpuId()].stepMode == SDFMGR_STEP_ARM_POLL)
		return SDFMGR_STEP_ARM_POLL;
	ksceKernelCpuResumeIntr(state);
	return -1;
}
