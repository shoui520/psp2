/* DCMP status packets queued by the per-core polling manager. */
#include "sdfmgr_internal.h"
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/dipsw.h>
#include <sysmem/aes.h>
#include <sysmem/qafmgr.h>
#include <stdio.h>

typedef struct DcmpStatusHeader {
	SceDeci4pHeader header;
	uint8_t code;
	uint8_t kind;
	uint16_t reserved;
} DcmpStatusHeader;

typedef struct DcmpPacket {
	DcmpStatusHeader message;
	union {
		uint32_t protocol;
		struct {
			uint16_t identifier;
			uint16_t sequence;
		} echo;
	};
	uint8_t payload[];
} DcmpPacket;

typedef struct DcmpEchoPacket {
	DcmpStatusHeader message;
	uint16_t identifier;
	uint16_t sequence;
	uint8_t payload[32];
} DcmpEchoPacket;

_Static_assert(offsetof(DcmpPacket, payload) == 24, "DCMP payload offset");
_Static_assert(sizeof(DcmpEchoPacket) == 56, "DCMP echo workspace");

typedef struct DcmpBuffers {
	union {
		DcmpStatusHeader message;
		uint8_t storage[0x11000];
	} receive;
	union {
		SceDeci4pHeader header;
		uint8_t storage[0x11000];
	} systemError;
	struct {
		uint8_t data[16];
	} transmit[256];
} DcmpBuffers;

_Static_assert(offsetof(DcmpBuffers, systemError) == 0x11000, "DCMP error buffer");
_Static_assert(offsetof(DcmpBuffers, transmit) == 0x22000, "DCMP queue buffers");
_Static_assert(sizeof(DcmpBuffers) == 0x23000, "DCMP allocation size");

typedef struct DcmpProtocolStatus {
	DcmpStatusHeader message;
	uint32_t protocol;
	uint32_t version;
	uint32_t subVersion;
	uint16_t cpu;
	uint16_t reserved;
	uint8_t status;
	uint8_t padding[3];
} DcmpProtocolStatus;

typedef struct DcmpConnectionStatus {
	DcmpStatusHeader message;
	uint8_t connected;
	uint8_t padding[3];
} DcmpConnectionStatus;

typedef struct DcmpAliasStatus {
	DcmpStatusHeader message;
	uint32_t protocol;
	uint32_t alias;
} DcmpAliasStatus;

typedef struct DcmpPowerStatus {
	DcmpStatusHeader message;
	uint8_t operation;
	uint8_t state;
	uint8_t reserved16;
	uint8_t reason;
} DcmpPowerStatus;

typedef struct DcmpSpaceStatus {
	DcmpStatusHeader message;
	uint32_t protocol;
} DcmpSpaceStatus;

_Static_assert(sizeof(DcmpProtocolStatus) == 0x28, "protocol status size");
_Static_assert(sizeof(DcmpConnectionStatus) == 0x18, "connection status size");
_Static_assert(sizeof(DcmpAliasStatus) == 0x1c, "alias status size");
_Static_assert(sizeof(DcmpPowerStatus) == 0x18, "power status size");
_Static_assert(sizeof(DcmpSpaceStatus) == 0x18, "space status size");

typedef struct DcmpSystemError {
	DcmpStatusHeader message;
	uint32_t type;
	uint32_t bodySize;
	uint32_t firmware;
	uint32_t format;
	uint32_t mode;
	uint32_t payloadSize;
	uint32_t metadataSize;
	uint32_t dipsw;
	uint8_t session[16];
	uint8_t payload[];
} DcmpSystemError;

/* Init2 fills the encryption schedule; the caller appends its CBC chain. */
typedef struct DcmpAesWorkspace {
	SceAesCompactContext aes;
	uint8_t chain[32];
} DcmpAesWorkspace;

_Static_assert(offsetof(DcmpSystemError, payload) == 0x44, "system error payload");
_Static_assert(offsetof(DcmpAesWorkspace, chain) == 0x1f0, "CBC chain offset");
_Static_assert(sizeof(DcmpAesWorkspace) == 0x210, "native AES workspace");
/* Shared by the three supported firmware originals. */
static const uint8_t s_dcmpSystemErrorKey[32] = {
	0x23, 0x5a, 0x93, 0xc1, 0x02, 0x1f, 0x15, 0x53,
	0xb8, 0x3d, 0x47, 0x61, 0xec, 0xfd, 0xb4, 0xef,
	0x63, 0x19, 0xd2, 0x47, 0xe1, 0xc5, 0x29, 0x64,
	0x25, 0x73, 0xa6, 0xba, 0xbd, 0x38, 0xcb, 0x1a,
};

static const uint8_t s_dcmpSystemErrorIv[16] = {
	0x47, 0xfb, 0x05, 0x0e, 0xa6, 0xf5, 0xfe, 0xcb,
	0xcc, 0x01, 0x9d, 0xe0, 0x90, 0x1f, 0xc1, 0x67,
};
int ksceAesInit2(SceAesCompactContext *context, SceSize blockSize,
		 SceSize keySize, const void *key);
int ksceAesEncrypt2(SceAesCompactContext *context, const void *source,
		    void *destination);
int ksceSysrootGetSessionId(void *session);

#if FW_DEBUG
#define DCMP_DEBUG_INFO(_line, _lineHash, _funcHash, _func) ({ \
	static const SceKernelDebugInfo info = { \
		.fileHash = 0xc8856f26, .lineHash = (_lineHash), \
		.funcHash = (_funcHash), .file = "pd_dcmp.c", \
		.line = (_line), .func = (_func), \
	}; \
	&info; \
})
#endif

int __attribute__((noinline)) dcmpMakeStatusConnection(uint32_t connected,
						      DcmpConnectionStatus *packet)
{
	if (!packet) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			DCMP_DEBUG_INFO(1038, 0xb967d87a, 0x55e0741a,
					"dcmpMakeStatusConnection"), "invalid arg\n");
#endif
		return (int)0x80090001;
	}
	if (connected > 1) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			DCMP_DEBUG_INFO(1044, 0x6dc5cca1, 0x55e0741a,
					"dcmpMakeStatusConnection"),
			"unknown status (0x%08x)\n", connected);
#endif
		return (int)0x80090001;
	}
	SceDeci4pSDfMgrForKernel_8A3B06F2(packet, 0, sizeof(*packet));
	SceDeci4pSDfMgrForKernel_FFC95B63(&packet->message.header, sizeof(*packet));
	packet->message.header.version = SCE_DECI4P_VERSION;
	packet->message.header.src = sdfMgrCpuAddress();
	packet->connected = connected;
	packet->message.kind = 0;
	packet->message.reserved = 0;
	packet->message.header.dst = SCE_DECI4P_PORT_TARGET;
	packet->message.header.protocol = 0x100;
	packet->message.code = 2;
	return 0;
}

int __attribute__((noinline)) dcmpMakeStatusProtocol(uint32_t enabled,
				const SceSDfMgrProtocol *registration,
				DcmpProtocolStatus *packet)
{
	uint32_t source, protocol, version, subVersion;

	if (!packet || !registration) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			DCMP_DEBUG_INFO(1077, 0x2de593ba, 0xafe766c3,
					"dcmpMakeStatusProtocol"), "invalid arg\n");
#endif
		return (int)0x80090001;
	}
	if (enabled > 1) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			DCMP_DEBUG_INFO(1083, 0x023115a9, 0xafe766c3,
					"dcmpMakeStatusProtocol"),
			"unknown status (0x%08x)\n", enabled);
#endif
		return (int)0x80090001;
	}
	SceDeci4pSDfMgrForKernel_8A3B06F2(packet, 0, sizeof(*packet));
	SceDeci4pSDfMgrForKernel_FFC95B63(&packet->message.header, sizeof(*packet));
	packet->message.header.version = SCE_DECI4P_VERSION;
	source = sdfMgrCpuAddress();
	packet->message.kind = 1;
	packet->message.code = 2;
	protocol = registration->protocol;
	version = registration->version;
	subVersion = registration->subVersion;
	packet->message.header.src = source;
	packet->message.reserved = 0;
	packet->protocol = protocol;
	packet->version = version;
	packet->subVersion = subVersion;
	packet->message.header.dst = SCE_DECI4P_PORT_TARGET;
	packet->message.header.protocol = 0x100;
	packet->cpu = sdfMgrCpuAddress();
	packet->status = enabled;
	return 0;
}

void __attribute__((noinline)) sdfMgrReportProtocols(void)
{
	unsigned i;

	for (i = 2; i < 8; i++) {
		SdfMgrProtocolState *protocol = _getPdCB(i);
		uint32_t flags = protocol->flags;
		uint32_t status;

		if (!(flags & 1))
			continue;
		status = flags & 2 ? 3 : 1;
		if (protocol->registration.flags & 2)
			status |= 4;
		dcmpSendStatusProtocol(status, &protocol->registration);
		if (protocol->registration.flags & 2)
			dcmpSendStatusProtocolAlias(&protocol->registration);
	}
}

void sdfMgrDisconnect(void)
{
	DcmpConnectionStatus packet;
	unsigned i;

	sdfMgrUpdateCpuFlags(2, 0);
	dcmpMakeStatusConnection(0, &packet);
	for (i = 2; i < 8; i++) {
		SdfMgrProtocolState *protocol = _getPdCB(i);

		if (protocol->flags & 1)
			protocol->handler(5, &packet, protocol->context);
	}
}

void sdfMgrConnect(void)
{
	DcmpConnectionStatus packet;
	unsigned i;

	sdfMgrUpdateCpuFlags(2, 1);
	sdfMgrReportProtocols();
	dcmpMakeStatusConnection(1, &packet);
	for (i = 2; i < 8; i++) {
		SdfMgrProtocolState *protocol = _getPdCB(i);

		if (protocol->flags & 1)
			protocol->handler(5, &packet, protocol->context);
	}
}

int sdfmgrStartProtocol(uint32_t id)
{
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	DcmpProtocolStatus packet;
	unsigned i;

	for (i = 0; i < 8; i++) {
		SdfMgrProtocolState *protocol = &cpu->protocols[i];
		int result;

		if (!(protocol->flags & 1) || protocol->registration.protocol != id)
			continue;
		result = sdfMgrSetProtocolStatus(protocol, 2, 1);
		if (result < 0) {
#if FW_DEBUG
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				SDFMGR_DEBUG_INFO(1360, 0x6dd3d97d, 0x9a31b4b9,
						"sdfmgrStartProtocol"),
				"SetProtocolStatus failed (0x%08X)\n", result);
#endif
			return result;
		}
		result = dcmpMakeStatusProtocol(1, &protocol->registration, &packet);
		if (result < 0) {
#if FW_DEBUG
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				SDFMGR_DEBUG_INFO(1368, 0x1ebe38be, 0x9a31b4b9,
						"sdfmgrStartProtocol"),
				"couldn't make DCMP packet (0x%08x)\n", result);
#endif
			return result;
		}
		protocol->handler(5, &packet, protocol->context);
		return 0;
	}
	return (int)0x8009000e;
}

int sdfmgrStopProtocol(uint32_t id)
{
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	DcmpProtocolStatus packet;
	unsigned i;

	for (i = 0; i < 8; i++) {
		SdfMgrProtocolState *protocol = &cpu->protocols[i];
		int result;

		if (!(protocol->flags & 1) || protocol->registration.protocol != id)
			continue;
		result = sdfMgrSetProtocolStatus(protocol, 2, 0);
		if (result < 0) {
#if FW_DEBUG
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				SDFMGR_DEBUG_INFO(1395, 0xd54a5173, 0xdfedc380,
						"sdfmgrStopProtocol"),
				"SetProtocolStatus failed (0x%08X)\n", result);
#endif
			return result;
		}
		result = dcmpMakeStatusProtocol(0, &protocol->registration, &packet);
		if (result < 0) {
#if FW_DEBUG
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				SDFMGR_DEBUG_INFO(1403, 0x77029b80, 0xdfedc380,
						"sdfmgrStopProtocol"),
				"couldn't make DCMP packet (0x%08x)\n", result);
#endif
			return result;
		}
		protocol->handler(5, &packet, protocol->context);
		return 0;
	}
	return (int)0x80090001;
}

void _discardPacket(SdfMgrDcmpCpuState *cpu, SceDeci4pHeader *header)
{
#if FW_DEBUG
	void *caller = __builtin_return_address(0);
#endif
	SceSize remaining = SceDeci4pSDfMgrForKernel_68903C1D(header);
	void *buffer = header + 1;

#if FW_DEBUG
	if (ksceKernelGetAssertLevel() > 0)
		ksceKernelAssertLevel(1, remaining < 0x11000,
			DCMP_DEBUG_INFO(686, 0x236c9862, 0x68bd4e89, "_discardPacket"),
			caller);
#endif
	remaining -= 0x14;
	while (remaining) {
		SceSize size = remaining < 0x1000 ? remaining : 0x1000;
		int result = sceSDfMgrExRecv(cpu->handle, buffer, size);

		if (result < 0) {
#if FW_DEBUG
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				DCMP_DEBUG_INFO(696, 0xc58c3c26, 0x68bd4e89, "_discardPacket"),
				"error: 0x%08x\n", result);
#endif
			return;
		}
		remaining -= (uint32_t)result;
	}
}

/* Sony inlined _dcmpHandler, _dcmpSend and the packet-specific handlers here. */
void _dcmpRecv(int event, void *arg, void *context)
{
	SdfMgrDcmpCpuState *cpu = &s_sdfMgrState.dcmp[sdfMgrCoreIndex()];
	DcmpPacket *packet;
	uint32_t kind, code;
	int result;

	(void)arg;
	(void)context;
	if (event == 3) {
		SdfMgrPacketQueue *queue = &cpu->transmit;

		while (queue->count != 0) {
			SceDeci4pHeader *out;
			SceSize size;
			uint32_t direct;
			unsigned state = ksceKernelCpuSuspendIntr();
			int read;

			if (queue->count <= 0) {
				ksceKernelCpuResumeIntr(state);
#if FW_DEBUG
				SceDeci4pSDfMgrForKernel_501CE61D(15,
					DCMP_DEBUG_INFO(325, 0xf00ddcfe, 0x309bcef3, "_dcmpSend"),
					"error: 0x%08x\n", 0x80090008);
#endif
				return;
			}
			read = queue->read;
			direct = queue->directMode;
			out = queue->packets[read];
			size = queue->sizes[read];
			ksceKernelCpuResumeIntr(state);
			if (direct)
				out = cpu->systemError;
			if (out->dst == 0x6000 || sdfMgrGetCpuFlags(2)) {
				result = sceSDfMgrExSend(cpu->handle, out, size);
				if (result < 0) {
#if FW_DEBUG
					SceDeci4pSDfMgrForKernel_501CE61D(15,
						DCMP_DEBUG_INFO(345, 0x1e6ac651, 0x309bcef3,
								"_dcmpSend"),
						"error: 0x%08x\n", result);
#endif
					return;
				}
			} else {
#if FW_DEBUG
				SceDeci4pSDfMgrForKernel_501CE61D(15,
					DCMP_DEBUG_INFO(339, 0x0c45acec, 0x309bcef3, "_dcmpSend"),
					"no route (0x%04x)\n", out->dst);
#endif
			}
			state = ksceKernelCpuSuspendIntr();
			read = queue->read;
			queue->sizes[read] = 0;
			read = (int)((uint32_t)read + 1);
			queue->read = read > 15 ? 0 : read;
			queue->count = (int)((uint32_t)queue->count - 1);
			queue->directMode = 0;
			ksceKernelCpuResumeIntr(state);
		}
		return;
	}
	if (event != 1) {
#if FW_DEBUG
		if (event < 1 || event > 6)
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				DCMP_DEBUG_INFO(243, 0x82479391, 0xc3c25f41, "_dcmpHandler"),
				"unknown event (0x%08x)\n", event);
#endif
		return;
	}
	result = sceSDfMgrExRecv(cpu->handle, cpu->receive, sizeof(DcmpStatusHeader));
	if (result < 0) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			DCMP_DEBUG_INFO(368, 0xab70f176, 0xf4e00ed1, "_dcmpRecv"),
			"error: 0x%08x\n", result);
#endif
		return;
	}
	packet = cpu->receive;
	code = packet->message.code;
	kind = packet->message.kind;
	if (code == 1) {
		DcmpEchoPacket response;
		SceSize size, payload;

		if (kind != 0) {
#if FW_DEBUG
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				DCMP_DEBUG_INFO(413, 0x4f3c91b9, 0xf4e00ed1, "_dcmpRecv"),
				"unknown packet (type:ECHO, code:%02x)\n", kind);
#endif
			goto discard;
		}
		size = SceDeci4pSDfMgrForKernel_68903C1D(&packet->message.header);
		if (size > sizeof(response))
			return;
		payload = size - sizeof(DcmpStatusHeader);
		result = sceSDfMgrExRecv(cpu->handle, &packet->echo, payload);
		if (result < 0) {
#if FW_DEBUG
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				DCMP_DEBUG_INFO(485, 0x138946e0, 0x306c04e6, "_dcmpEcho"),
				"error: 0x%08x\n", result);
#endif
			return;
		}
		SceDeci4pSDfMgrForKernel_8A3B06F2(&response, 0, sizeof(response));
		SceDeci4pSDfMgrForKernel_FFC95B63(&response.message.header, size);
		response.message.header.dst = packet->message.header.src;
		response.message.code = 1;
		response.message.kind = 1;
		response.message.reserved = 0;
		response.identifier = packet->echo.identifier;
		response.sequence = packet->echo.sequence + 1;
		/* Native copies size-20 from offset24, including four excess bytes. */
		SceDeci4pSDfMgrForKernel_5996E2DB(response.payload, packet->payload, payload);
		result = sdfMgrQueuePacket(&cpu->transmit, &response, size, 0);
		if (result < 0) {
#if FW_DEBUG
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				DCMP_DEBUG_INFO(524, 0x19576fca, 0x306c04e6, "_dcmpEcho"),
				"error: 0x%08x\n", result);
#endif
			return;
		}
		sceSDfMgrExReqSend(cpu->handle);
		return;
	}
	if (code == 0) {
		switch (kind) {
		case 0:
		case 2:
			if (SceDeci4pSDfMgrForKernel_68903C1D(&packet->message.header) == 20 &&
			    !sdfMgrGetCpuFlags(2))
				sdfMgrConnect();
			return;
		case 1:
		case 3:
			if (SceDeci4pSDfMgrForKernel_68903C1D(&packet->message.header) == 20)
				sdfMgrDisconnect();
			return;
		case 4:
		case 5:
			if (SceDeci4pSDfMgrForKernel_68903C1D(&packet->message.header) != 24)
				return;
			result = sceSDfMgrExRecv(cpu->handle, &packet->protocol, 4);
			if (result < 0) {
#if FW_DEBUG
				if (kind == 4)
					SceDeci4pSDfMgrForKernel_501CE61D(15,
						DCMP_DEBUG_INFO(633, 0x83715fb2, 0x65766d57,
								"_dcmpLinkStart"),
						"error: 0x%08x\n", result);
				else
					SceDeci4pSDfMgrForKernel_501CE61D(15,
						DCMP_DEBUG_INFO(664, 0x643c7144, 0x176fd166,
								"_dcmpLinkStop"),
						"error: 0x%08x\n", result);
#endif
				return;
			}
			if (kind == 4)
				sdfmgrStartProtocol(packet->protocol);
			else
				sdfmgrStopProtocol(packet->protocol);
			return;
		default:
#if FW_DEBUG
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				DCMP_DEBUG_INFO(401, 0x78b02112, 0xf4e00ed1, "_dcmpRecv"),
				"unknown packet (type:CONNECT, code:%02x)\n", kind);
#endif
			goto discard;
		}
	}
	if (code == 2) {
		if (kind - 3 <= 3)
			return;
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			DCMP_DEBUG_INFO(432, 0xa8378737, 0xf4e00ed1, "_dcmpRecv"),
			"unknown packet (type:STATUS, code:%02x)\n", kind);
#endif
	} else {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			DCMP_DEBUG_INFO(458, 0xd57bba9c, 0xf4e00ed1, "_dcmpRecv"),
			"unknown packet (type:%02x, code:%02x)\n", code, kind);
#endif
	}
discard:
	_discardPacket(cpu, &cpu->receive->message.header);
}

int dcmpInit(SdfMgrProtocolState *protocol)
{
	static const SceSDfMgrProtocol registration = {
		.size = sizeof(SceSDfMgrProtocol),
		.protocol = 0x100,
		.alias = 0,
		.version = 0x01000400,
		.subVersion = 0x00020004,
		.destination = 0x4000,
		.flags = 1,
		.name = "DCMP",
	};
#if FW_DEBUG
	void *caller = __builtin_return_address(0);
	const SceKernelDebugInfo *errorInfo, *assertInfo;
	const char *failure;
#endif
	SdfMgrDcmpCpuState *cpu = &s_sdfMgrState.dcmp[sdfMgrCoreIndex()];
	DcmpBuffers *buffers;
	void *base;
	char name[32];
	unsigned i;
	int result;

	SceDeci4pSDfMgrForKernel_8A3B06F2(cpu, 0, sizeof(*cpu));
	cpu->handle = 0xfff;
	protocol->handler = _dcmpRecv;
	protocol->registration = registration;
	protocol->context = NULL;
	protocol->handle = 0xfff;
	protocol->flags = 0;
	snprintf(name, sizeof(name), "%s%d", "SceDeci4pSDfMgrDcmp",
		 (int)ksceKernelCpuId());
	result = ksceKernelAllocMemBlock(name, SceDeci4pSDfMgrForKernel_35238B8A(),
					 sizeof(*buffers), NULL);
	if (result < 0) {
#if FW_DEBUG
		errorInfo = DCMP_DEBUG_INFO(131, 0x2375c3b9, 0xb000cfda, "dcmpInit");
		assertInfo = DCMP_DEBUG_INFO(132, 0x703986cd, 0xb000cfda, "dcmpInit");
		failure = "AllocMemBlockForKernel failed (0x%08x)\n";
		goto error;
#else
		return result;
#endif
	}
	cpu->memblock = result;
	result = ksceKernelGetMemBlockBase(result, &base);
	if (result < 0) {
#if FW_DEBUG
		errorInfo = DCMP_DEBUG_INFO(140, 0x917b6dd8, 0xb000cfda, "dcmpInit");
		assertInfo = DCMP_DEBUG_INFO(141, 0x554cc0d8, 0xb000cfda, "dcmpInit");
		failure = "GetMemBlockBase failed (0x%08x)\n";
		goto error;
#else
		return result;
#endif
	}
	buffers = base;
	cpu->receive = (DcmpPacket *)&buffers->receive.message;
	cpu->systemError = &buffers->systemError.header;
	/* Native queue pointers advance16 bytes, despite the enqueue size cap256. */
	for (i = 0; i < 16; i++)
		cpu->transmit.packets[i] = buffers->transmit[i].data;
	result = sdfMgrSetProtocolStatus(protocol, 1, 1);
	if (result < 0) {
#if FW_DEBUG
		errorInfo = DCMP_DEBUG_INFO(162, 0x9e529606, 0xb000cfda, "dcmpInit");
		assertInfo = DCMP_DEBUG_INFO(163, 0xb7b232f0, 0xb000cfda, "dcmpInit");
		failure = "SetProtocolStatus failed (0x%08x)\n";
		goto error;
#else
		return result;
#endif
	}
	result = sdfMgrSetProtocolStatus(protocol, 2, 1);
	if (result < 0) {
#if FW_DEBUG
		errorInfo = DCMP_DEBUG_INFO(168, 0x7957597a, 0xb000cfda, "dcmpInit");
		assertInfo = DCMP_DEBUG_INFO(169, 0xe9dd9695, 0xb000cfda, "dcmpInit");
		failure = "SetProtocolStatus failed (0x%08x)\n";
		goto error;
#else
		return result;
#endif
	}
	return 0;
#if FW_DEBUG
error:
	SceDeci4pSDfMgrForKernel_501CE61D(15, errorInfo, failure, result);
	if (ksceKernelGetAssertLevel() > 0)
		ksceKernelAssertLevel(1, 0, assertInfo, caller);
	return result;
#endif
}

void dcmpSendError(SdfMgrProtocolState *protocol, uint32_t kind,
		   const SceDeci4pHeader *header)
{
	SdfMgrDcmpCpuState *cpu = &s_sdfMgrState.dcmp[sdfMgrCoreIndex()];
	DcmpStatusHeader *packet = (DcmpStatusHeader *)cpu->systemError;
	SceSize size;
	int result;

	SceDeci4pSDfMgrForKernel_8A3B06F2(packet, 0, sizeof(*packet));
	size = SceDeci4pSDfMgrForKernel_68903C1D(header);
	SceDeci4pSDfMgrForKernel_5996E2DB(packet, header, sizeof(*header));
	/* Native receives the full wrapping length before capping the reply. */
	sceSDfMgrExRecv(protocol->handle, &packet->code, size - sizeof(*header));
	packet->header = *header;
	if (size >= 0x10fec)
		size = 0x10fec;
	packet->header.src = header->dst;
	packet->header.dst = header->src;
	size += sizeof(*packet);
	SceDeci4pSDfMgrForKernel_FFC95B63(&packet->header, size);
	packet->code = 3;
	packet->reserved = 0;
	packet->kind = kind;
	result = sdfMgrQueuePacket(&cpu->transmit, NULL, size, 1);
	if (result < 0) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			DCMP_DEBUG_INFO(303, 0x35db5575, 0x7abe1e09, "dcmpSendError"),
			"error: 0x%08x\n", result);
#endif
		return;
	}
	sceSDfMgrExReqSend(cpu->handle);
	SceDeci4pSDfMgrForKernel_98AEC60E();
}

void dcmpSendStatusSystemError(uint32_t type, SceSize size, const void *source)
{
	int qa = SceQafMgrForDriver_382C71E8();
	SdfMgrDcmpCpuState *cpu = &s_sdfMgrState.dcmp[sdfMgrCoreIndex()];
	DcmpSystemError *packet = (DcmpSystemError *)cpu->systemError;
	SceSize aligned, packetSize;
	int result;

	if (size > 0xffd8) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			DCMP_DEBUG_INFO(926, 0xef12d4be, 0x148d5157,
					"dcmpSendStatusSystemError"),
			"message is too big! (0x%08x)\n", size);
#endif
		aligned = 0;
	} else {
		aligned = (size + 15) & ~15u;
		if (aligned != size)
			SceDeci4pSDfMgrForKernel_8A3B06F2(packet->payload + size,
							0, aligned - size);
	}
	if ((type & ~2u) == 1) {
		DcmpSystemError *current =
			(DcmpSystemError *)s_sdfMgrState.dcmp[sdfMgrCoreIndex()].systemError;

		SceDeci4pSDfMgrForKernel_5996E2DB(current->payload, source, aligned);
	}
	packetSize = aligned ? aligned + sizeof(*packet) : 28;
	SceDeci4pSDfMgrForKernel_FFC95B63(&packet->message.header, packetSize);
	packet->message.header.dst = 0;
	packet->message.reserved = 0;
	packet->message.code = 2;
	packet->message.kind = 7;
	packet->type = type;
	packet->bodySize = aligned ? aligned + 0x28 : 0;
	if (aligned) {
		packet->firmware = FW_VERSION;
		packet->format = 2;
		packet->mode = qa ? 0x10000001 : 0x20000001;
		packet->payloadSize = aligned;
		packet->metadataSize = 0x28;
		packet->dipsw = qa ? ksceKernelGetDipswInfo(3) : 0;
		ksceSysrootGetSessionId(packet->session);
		if (!qa) {
			DcmpAesWorkspace context;
			uint8_t block[32];
			uint32_t blockSize, offset, i;

			result = ksceAesInit2(&context.aes, 128, 256, s_dcmpSystemErrorKey);
			blockSize = context.aes.blockWords * 4;
			if (result >= 0)
				for (i = 0; i < blockSize; i++)
					context.chain[i] = s_dcmpSystemErrorIv[i];
			for (offset = 0; offset < aligned; offset += 16) {
				uint8_t *out = packet->payload + offset;

				for (i = 0; i < blockSize; i++)
					block[i] = out[i] ^ context.chain[i];
				ksceAesEncrypt2(&context.aes, block, out);
				blockSize = context.aes.blockWords * 4;
				for (i = 0; i < blockSize; i++)
					context.chain[i] = out[i];
			}
		}
	}
#if FW_DEBUG
	{
		const char *description = type == 1 ? "assert" :
			type == 2 ? "bus error" : type == 3 ? "exception" : "unknown";

		SceDeci4pSDfMgrForKernel_501CE61D(15,
			DCMP_DEBUG_INFO(1015, 0x1aee448b, 0x148d5157,
					"dcmpSendStatusSystemError"),
			"send STATUS/SYSTEM_ERROR (%s, 0x%08x, 0x%08x)\n",
			description, type, aligned);
	}
#endif
	result = sdfMgrQueuePacket(&cpu->transmit, packet, packetSize, 1);
	if (result < 0) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			DCMP_DEBUG_INFO(1023, 0x080a0a9e, 0x148d5157,
					"dcmpSendStatusSystemError"),
			"error: 0x%08x\n", result);
#endif
		return;
	}
	sceSDfMgrExReqSend(cpu->handle);
	SceDeci4pSDfMgrForKernel_98AEC60E();
}

/* Internal names inferred from the registered system-error producer hooks. */
void *dcmpGetSystemErrorBuffer(void)
{
	DcmpSystemError *packet =
		(DcmpSystemError *)s_sdfMgrState.dcmp[sdfMgrCoreIndex()].systemError;

	return packet->payload;
}

SceSize dcmpGetSystemErrorBufferSize(void)
{
	return 0xffd8;
}

void dcmpSendStatusProtocol(uint32_t status, const SceSDfMgrProtocol *registration)
{
	DcmpProtocolStatus packet;
	SdfMgrDcmpCpuState *cpu;
	uint32_t protocol, version, subVersion;
	int result;

	if (status >> 3) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			DCMP_DEBUG_INFO(718, 0xf93f083a, 0x14c66e2a,
					"dcmpSendStatusProtocol"),
			"unknown status (0x%08x)\n", status);
#endif
		return;
	}
	cpu = &s_sdfMgrState.dcmp[sdfMgrCoreIndex()];
	SceDeci4pSDfMgrForKernel_8A3B06F2(&packet, 0, sizeof(packet));
	SceDeci4pSDfMgrForKernel_FFC95B63(&packet.message.header, sizeof(packet));
	subVersion = registration->subVersion;
	protocol = registration->protocol;
	version = registration->version;
	packet.message.header.dst = 0x4000;
	packet.message.code = 2;
	packet.protocol = protocol;
	packet.version = version;
	packet.subVersion = subVersion;
	packet.message.kind = 1;
	packet.message.reserved = 0;
	packet.cpu = sdfMgrCpuAddress();
	packet.status = status;
	result = sdfMgrQueuePacket(&cpu->transmit, &packet, sizeof(packet), 0);
	if (result < 0) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			DCMP_DEBUG_INFO(756, 0x0385b18e, 0x14c66e2a,
					"dcmpSendStatusProtocol"),
			"error: 0x%08x\n", result);
#endif
		return;
	}
	sceSDfMgrExReqSend(cpu->handle);
	SceDeci4pSDfMgrForKernel_98AEC60E();
}

void dcmpSendStatusProtocolAlias(const SceSDfMgrProtocol *registration)
{
	SdfMgrDcmpCpuState *cpu = &s_sdfMgrState.dcmp[sdfMgrCoreIndex()];
	DcmpAliasStatus packet;
	int result;

	SceDeci4pSDfMgrForKernel_8A3B06F2(&packet, 0, sizeof(packet));
	SceDeci4pSDfMgrForKernel_FFC95B63(&packet.message.header, sizeof(packet));
	packet.message.header.dst = 0x4000;
	packet.message.code = 2;
	packet.message.kind = 2;
	packet.message.reserved = 0;
	packet.protocol = registration->protocol;
	packet.alias = registration->alias;
	result = sdfMgrQueuePacket(&cpu->transmit, &packet, sizeof(packet), 0);
	if (result < 0) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			DCMP_DEBUG_INFO(801, 0xdbb4c268, 0x27e61491,
					"dcmpSendStatusProtocolAlias"),
			"error: 0x%08x\n", result);
#endif
		return;
	}
	sceSDfMgrExReqSend(cpu->handle);
	SceDeci4pSDfMgrForKernel_98AEC60E();
}

void dcmpSendStatusPower(uint32_t operation, uint32_t state, uint32_t reason)
{
	SdfMgrDcmpCpuState *cpu = &s_sdfMgrState.dcmp[sdfMgrCoreIndex()];
	DcmpPowerStatus packet;
	int result;

	SceDeci4pSDfMgrForKernel_8A3B06F2(&packet, 0, sizeof(packet));
	SceDeci4pSDfMgrForKernel_FFC95B63(&packet.message.header, sizeof(packet));
	packet.message.header.dst = 0x6000;
	packet.message.code = 2;
	packet.message.kind = 4;
	packet.message.reserved = 0;
	packet.operation = operation;
	packet.state = state;
	packet.reason = operation == 0 && state == 2 ? reason : 0;
	result = sdfMgrQueuePacket(&cpu->transmit, &packet, sizeof(packet), 0);
	if (result < 0) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			DCMP_DEBUG_INFO(853, 0x2f2e5143, 0x64e63f72,
					"dcmpSendStatusPower"),
			"error: 0x%08x\n", result);
#endif
		return;
	}
	sceSDfMgrExReqSend(cpu->handle);
	SceDeci4pSDfMgrForKernel_98AEC60E();
}

void dcmpSendStatusSpace(uint32_t protocol)
{
	SdfMgrDcmpCpuState *cpu = &s_sdfMgrState.dcmp[sdfMgrCoreIndex()];
	DcmpSpaceStatus packet = { 0 };
	int result;

	SceDeci4pSDfMgrForKernel_FFC95B63(&packet.message.header, sizeof(packet));
	packet.message.header.dst = 0;
	packet.message.code = 2;
	packet.message.kind = 3;
	packet.message.reserved = 0;
	packet.protocol = protocol;
	result = sdfMgrQueuePacket(&cpu->transmit, &packet, sizeof(packet), 0);
	if (result < 0) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			DCMP_DEBUG_INFO(895, 0x0a8bc2c8, 0xdd53e642, "dcmpSendStatusSpace"),
			"error: 0x%08x\n", result);
#endif
		return;
	}
	sceSDfMgrExReqSend(cpu->handle);
	SceDeci4pSDfMgrForKernel_98AEC60E();
}
