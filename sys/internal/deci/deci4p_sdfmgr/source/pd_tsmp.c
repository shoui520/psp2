/* Per-core TSMP producer and transport callbacks. */
#include "sdfmgr_internal.h"

typedef struct TsmpPacket {
	DcmpHeader message;
	uint32_t id[2];				/* TSMP's request id */
	uint16_t tag;				/* the first entry's */
	uint16_t padding;
} TsmpPacket;

_Static_assert(sizeof(TsmpPacket) == 32, "TSMP header workspace");
_Static_assert(offsetof(TsmpPacket, tag) == 28, "TSMP entry tag");

typedef struct TsmpReceivePacket {
	DcmpHeader message;
	uint32_t id[2];
} TsmpReceivePacket;

_Static_assert(sizeof(TsmpReceivePacket) == 28, "TSMP received header");

/* Name unknown: registers the TSMP producer with the debug services. */
void SceDebugForKernel_78027203(void (*producer)(const void *, SceSize));

#if FW_DEBUG
#define TSMP_DEBUG_INFO(_line, _lineHash, _funcHash, _func) ({ \
	static const SceKernelDebugInfo info = { \
		.fileHash = 0x8772e42c, .lineHash = (_lineHash), \
		.funcHash = (_funcHash), .file = "pd_tsmp.c", \
		.line = (_line), .func = (_func), \
	}; \
	&info; \
})
#endif

/* Internal name inferred from the producer registration in tsmpInit. */
void sdfMgrTsmpSend(const void *payload, SceSize size)
{
	SdfMgrTsmpCpuState *cpu = &s_sdfMgrState.tsmp[sdfMgrCoreIndex()];
	TsmpPacket packet;

	SceDeci4pSDfMgrForKernel_8A3B06F2(&packet, 0, sizeof(packet));
	SceDeci4pSDfMgrForKernel_FFC95B63(&packet.message.hdr, size + sizeof(packet));
	cpu->payloadRemaining = size;
	cpu->payload = payload;
	cpu->header = &packet;
	cpu->headerRemaining = sizeof(packet);
	packet.tag = 1;
	packet.message.code = 12;
	packet.message.hdr.dst = 0x6000;
	packet.message.type = 6;
	sceSDfMgrExReqSend(cpu->handle);
	SceDeci4pSDfMgrForKernel_98AEC60E();
}

/* Sony inlined _tsmpHandler, _tsmpRecv and _discardPacket into this callback. */
void _tsmpSend(int event, void *arg, void *context)
{
#if FW_DEBUG
	void *caller = __builtin_return_address(0);
#endif
	SdfMgrTsmpCpuState *cpu = &s_sdfMgrState.tsmp[sdfMgrCoreIndex()];
	TsmpReceivePacket packet;
	uint32_t discarded;
	SceSize remaining;
	int result;

	(void)arg;
	(void)context;
	switch (event) {
	case 3:
		while (cpu->headerRemaining) {
			result = sceSDfMgrExSend(cpu->handle, cpu->header,
						 cpu->headerRemaining);
			if (result < 0) {
#if FW_DEBUG
				SceDeci4pSDfMgrForKernel_501CE61D(15,
					TSMP_DEBUG_INFO(163, 0x1302368c, 0xb6982b6b,
							"_tsmpSend"),
					"error: 0x%08x\n", result);
#endif
				return;
			}
			cpu->headerRemaining -= (uint32_t)result;
		}
		while (cpu->payloadRemaining) {
			result = sceSDfMgrExSend(cpu->handle, cpu->payload,
						 cpu->payloadRemaining);
			if (result < 0) {
#if FW_DEBUG
				SceDeci4pSDfMgrForKernel_501CE61D(15,
					TSMP_DEBUG_INFO(173, 0x87c864f7, 0xb6982b6b,
							"_tsmpSend"),
					"error: 0x%08x\n", result);
#endif
				return;
			}
			cpu->payloadRemaining -= (uint32_t)result;
		}
		return;
	case 2:
	case 4:
	case 5:
	case 6:
		return;
	case 1:
		break;
	default:
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			TSMP_DEBUG_INFO(150, 0x3ccc0a31, 0x7ec7a51d, "_tsmpHandler"),
			"unknown event (0x%08x)\n", event);
#endif
		return;
	}
	result = sceSDfMgrExRecv(cpu->handle, &packet, 28);
	if (result < 0) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			TSMP_DEBUG_INFO(189, 0xa5172fca, 0xa978ac57, "_tsmpRecv"),
			"error: 0x%08x\n", result);
#endif
		return;
	}
#if FW_DEBUG
	SceDeci4pSDfMgrForKernel_501CE61D(15,
		TSMP_DEBUG_INFO(195, 0x1223f898, 0xa978ac57, "_tsmpRecv"),
		"error (type:%02x, code:%02x)\n", packet.message.type, packet.message.code);
#endif
	remaining = SceDeci4pSDfMgrForKernel_68903C1D(&packet.message.hdr);
#if FW_DEBUG
	if (ksceKernelGetAssertLevel() > 0)
		ksceKernelAssertLevel(1, remaining < 0x11000,
			TSMP_DEBUG_INFO(211, 0x4c2050cc, 0x68bd4e89, "_discardPacket"),
			caller);
#endif
	remaining -= 28;
	while (remaining) {
		result = sceSDfMgrExRecv(cpu->handle, &discarded, sizeof(discarded));
		if (result < 0) {
#if FW_DEBUG
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				TSMP_DEBUG_INFO(218, 0x374c19bd, 0x68bd4e89,
						"_discardPacket"), "error: 0x%08x\n", result);
#endif
			return;
		}
		remaining -= (uint32_t)result;
	}
}

int tsmpInit(SdfMgrProtocolState *protocol)
{
	static const SceSDfMgrProtocol registration = {
		.size = sizeof(SceSDfMgrProtocol),
		.protocol = 0x2000,
		.alias = 0,
		.version = 0x01000000,
		.subVersion = 0,
		.destination = 0x4000,
		.flags = 1,
		.name = "TSMP",
	};
#if FW_DEBUG
	void *caller = __builtin_return_address(0);
	const SceKernelDebugInfo *errorInfo, *assertInfo;
#endif
	SdfMgrTsmpCpuState *cpu = &s_sdfMgrState.tsmp[sdfMgrCoreIndex()];
	int result;

	SceDeci4pSDfMgrForKernel_8A3B06F2(cpu, 0, sizeof(*cpu));
	cpu->handle = 0xffe;
	protocol->handler = _tsmpSend;
	protocol->registration = registration;
	protocol->handle = 0xffe;
	protocol->context = NULL;
	protocol->flags = 0;
	result = sdfMgrSetProtocolStatus(protocol, 1, 1);
	if (result < 0) {
#if FW_DEBUG
		errorInfo = TSMP_DEBUG_INFO(72, 0x8c0cb6dd, 0xb4ee34ab, "tsmpInit");
		assertInfo = TSMP_DEBUG_INFO(73, 0xf9c46be8, 0xb4ee34ab, "tsmpInit");
		goto error;
#else
		return result;
#endif
	}
	result = sdfMgrSetProtocolStatus(protocol, 2, 1);
	if (result < 0) {
#if FW_DEBUG
		errorInfo = TSMP_DEBUG_INFO(78, 0xe25aba69, 0xb4ee34ab, "tsmpInit");
		assertInfo = TSMP_DEBUG_INFO(79, 0x7c770b9f, 0xb4ee34ab, "tsmpInit");
		goto error;
#else
		return result;
#endif
	}
	if (ksceKernelCpuId() == 0)
		SceDebugForKernel_78027203(sdfMgrTsmpSend);
	return 0;
#if FW_DEBUG
error:
	SceDeci4pSDfMgrForKernel_501CE61D(15, errorInfo,
		"SetProtocolStatus failed (0x%08x)\n", result);
	if (ksceKernelGetAssertLevel() > 0)
		ksceKernelAssertLevel(1, 0, assertInfo, caller);
	return result;
#endif
}
