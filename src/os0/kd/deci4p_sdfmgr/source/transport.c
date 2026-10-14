/* Polling transport and per-core protocol operations. */
#include "sdfmgr_internal.h"

int sceSDfMgrIf2MgrEvent(SdfMgrTransport *transport, int event,
			uint32_t arg, uint32_t protocol, uint32_t destination)
{
	SdfMgrProtocolState *current;
	uint32_t size, progress;

	switch (event) {
	case 1:
		current = transport->receiveProtocol;
		if (!current) {
			SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
			unsigned index;
			int result;

			transport->receiveSize = 0;
			transport->receiveProgress = 0;
			if ((destination & ~0x2000u) != 0x8000 &&
			    destination != 0xa200 && destination != sdfMgrCpuAddress()) {
#if FW_DEBUG
				SceDeci4pSDfMgrForKernel_501CE61D(15,
					SDFMGR_DEBUG_INFO(1913, 0x9c2dae5f, 0x87394e08,
							"_deliverRecvPacket"),
					"invalid destination (0x%04x)\n", destination);
#endif
				return (int)0x8009000c;
			}
			for (index = 0; index < 8; index++)
				if (cpu->protocols[index].registration.protocol == protocol)
					break;
			if (index == 8) {
				dcmpSendError(_getPdCB(0), 2, &transport->receiveHeader);
				return (int)0x8009000e;
			}
			transport->receiveProtocol = &cpu->protocols[index];
			result = transport->beginReceive(transport);
			if (result < 0)
				return result;
			current = transport->receiveProtocol;
		} else if (!transport->receiveSize) {
			transport->receiveSize =
				*(const uint16_t *)transport->receiveHeader.size |
				(uint32_t)transport->receiveHeader.size[2] << 16;
		}
		transport->flags |= 4;
		current->handler(1, (void *)(uintptr_t)arg, current->context);
		transport->flags &= ~4u;
		break;
	case 2:
		if (!transport->receiveProtocol) {
#if FW_DEBUG
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				SDFMGR_DEBUG_INFO(1228, 0x302f85c8, 0xfa325223,
						"sceSDfMgrIf2MgrEvent"),
				"IF_EVENT_INDONE: receiving PD was not found.\n");
#endif
			break;
		}
		size = transport->receiveSize;
		if (!size) {
			size = *(const uint16_t *)transport->receiveHeader.size |
				(uint32_t)transport->receiveHeader.size[2] << 16;
			transport->receiveSize = size;
		}
		progress = transport->receiveProgress;
		if (progress > size) {
#if FW_DEBUG
			if (progress > size + 3) {
				SceDeci4pSDfMgrForKernel_501CE61D(15,
					SDFMGR_DEBUG_INFO(1237, 0xf44d4c2f, 0xfa325223,
							"sceSDfMgrIf2MgrEvent"),
					"IFM_INDONE: Receive packet too large %d > %d.\n",
					progress, size);
				size = transport->receiveSize;
				progress = transport->receiveProgress;
			}
#endif
			arg = size - progress;
		}
		progress += arg;
		transport->receiveProgress = progress;
		if (progress >= size) {
			current = transport->receiveProtocol;
			current->handler(2, (void *)(uintptr_t)arg, current->context);
			transport->receiveProtocol = NULL;
			transport->finishReceive(transport);
		}
		break;
	case 3:
		current = transport->sendProtocol;
		if (!current) {
#if FW_DEBUG
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				SDFMGR_DEBUG_INFO(1252, 0xabe1986e, 0xfa325223,
						"sceSDfMgrIf2MgrEvent"),
				"IFM_OUT: Sending PD was not found.\n");
#endif
			break;
		}
		transport->operationFlags = (transport->operationFlags & ~0x10u) | 8;
		current->handler(3, (void *)(uintptr_t)arg, current->context);
		transport->operationFlags &= ~8u;
		break;
	case 4:
		current = transport->sendProtocol;
		if (!current) {
#if FW_DEBUG
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				SDFMGR_DEBUG_INFO(1264, 0x4b3bf246, 0xfa325223,
						"sceSDfMgrIf2MgrEvent"),
				"IFM_OUTDONE: Sending PD was not found.\n");
#endif
			break;
		}
		size = transport->sendSize;
		progress = transport->sendProgress + arg;
		transport->sendProgress = progress;
		if (size && progress >= size) {
			unsigned cpu = sdfMgrCoreIndex();

			transport->sendProtocol = NULL;
			transport->reset(transport);
			current->handler(4, NULL, current->context);
			sdfMgrScheduleSend(&s_sdfMgrState.cpu[cpu]);
		}
		break;
	case 5:
		transport->operationFlags |= 1;
		break;
	case 6:
		transport->operationFlags &= ~1u;
		break;
	}
	return 0;
}

uint32_t sdfMgrCpuAddress(void)
{
	unsigned shift = ksceKernelCpuId() & 0xff;
	uint32_t mask = shift < 32 ? 1U << shift : 0;

	return 0xa200 + mask;
}

__attribute__((noinline)) int sdfMgrQueuePacket(SdfMgrPacketQueue *queue,
			const void *packet, SceSize size, int direct)
{
	unsigned state;
	int result, write, count;

	if (size > 0x100 && !direct)
		return (int)0x80090001;
	state = ksceKernelCpuSuspendIntr();
	count = queue->count;
	if (count > 15) {
		result = (int)0x8009000b;
		goto out;
	}
	if (direct) {
		queue->directMode = 1;
	} else {
		write = queue->write;
		queue->directMode = 0;
		SceDeci4pSDfMgrForKernel_5996E2DB(queue->packets[write], packet, size);
		count = queue->count;
	}
	write = queue->write;
	queue->sizes[write] = size;
	queue->count = (int)((uint32_t)count + 1);
	write = (int)((uint32_t)write + 1);
	queue->write = write > 15 ? 0 : write;
	result = 0;
out:
	ksceKernelCpuResumeIntr(state);
	return result;
}

#if FW_DEBUG
__attribute__((noinline)) SdfMgrProtocolState *_getPdCB(unsigned index)
{
	void *caller = __builtin_return_address(0);
	unsigned core = sdfMgrCoreIndex();

	if (index < 8)
		return &s_sdfMgrState.cpu[core].protocols[index];
	SceDeci4pSDfMgrForKernel_501CE61D(15,
		SDFMGR_DEBUG_INFO(1631, 0xea012585, 0x89e87bb7, "_getPdCB"),
		"invalid argument (id: %d)\n", index);
	if (ksceKernelGetAssertLevel() > 0)
		ksceKernelAssertLevel(1, 0,
			SDFMGR_DEBUG_INFO(1630, 0xbf05bcf0, 0x89e87bb7, "_getPdCB"),
			caller);
	return NULL;
}
#endif

int sceSDfMgrExOpen(SceSDfMgrHandler handler, const SceSDfMgrProtocol *registration,
		   void *context)
{
	SdfMgrProtocolState *protocol;
	SdfMgrCpuState *cpu;
	uint16_t flags;
	uint32_t handle, status;
	unsigned core, i;

	if (!registration) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDFMGR_DEBUG_INFO(630, 0x3c31089b, 0xaefc5866, "sceSDfMgrExOpen"),
			"invalid argument\n");
#endif
		return (int)0x80090001;
	}
	if (!registration->protocol) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDFMGR_DEBUG_INFO(634, 0xd6748032, 0xaefc5866, "sceSDfMgrExOpen"),
			"invalid argument\n");
#endif
		return (int)0x80090001;
	}
	if (registration->size != sizeof(*registration)) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDFMGR_DEBUG_INFO(638, 0xf4c8fc85, 0xaefc5866, "sceSDfMgrExOpen"),
			"invalid argument\n");
#endif
		return (int)0x80090001;
	}
	flags = registration->flags;
	core = sdfMgrCoreIndex();
	for (i = 0; i < 8; i++) {
		protocol = _getPdCB(i);
		if ((protocol->flags & 1) &&
		    protocol->registration.protocol == registration->protocol)
			return (int)0x80090003;
	}
	for (i = 2; i < 8; i++) {
		protocol = _getPdCB(i);
		if (!(protocol->flags & 1))
			break;
	}
	if (i == 8) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDFMGR_DEBUG_INFO(690, 0xe26bb55d, 0xaefc5866, "sceSDfMgrExOpen"),
			"no entry\n");
#endif
		return (int)0x80090004;
	}
	SceDeci4pSDfMgrForKernel_8A3B06F2(protocol, 0, sizeof(*protocol));
	cpu = &s_sdfMgrState.cpu[core];
	handle = cpu->nextHandle;
	protocol->handler = handler;
	protocol->registration = *registration;
	protocol->context = context;
	cpu->nextHandle = handle + 1;
	protocol->handle = (int)handle;
	status = flags & 1 ? 3 : 1;
	protocol->flags = status;
	if (registration->flags & 2)
		status |= 4;
	dcmpSendStatusProtocol(status, registration);
	if (registration->flags & 2)
		dcmpSendStatusProtocolAlias(registration);
	return (int)handle;
}

int SceDeci4pSDfMgrForKernel_A55F5F0D(SceSDfMgrHandler handler,
				  const SceSDfMgrProtocol *registration, void *context)
{
	unsigned state = ksceKernelCpuSuspendIntr();
	int result = sceSDfMgrExOpen(handler, registration, context);

	ksceKernelCpuResumeIntr(state);
	return result;
}

int sceSDfMgrExClose(int handle)
{
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	unsigned i;

	for (i = 0; i < 8; i++) {
		SdfMgrProtocolState *protocol = &cpu->protocols[i];

		if ((protocol->flags & 1) && protocol->handle == handle) {
			SceSDfMgrProtocol registration = protocol->registration;

			SceDeci4pSDfMgrForKernel_8A3B06F2(protocol, 0, sizeof(*protocol));
			dcmpSendStatusProtocol(0, &registration);
			return 0;
		}
	}
#if FW_DEBUG
	SceDeci4pSDfMgrForKernel_501CE61D(15,
		SDFMGR_DEBUG_INFO(739, 0xa6449b1f, 0xba595424, "sceSDfMgrExClose"),
		"invalid argument (sock: 0x%08X)\n", handle);
#endif
	return (int)0x80090002;
}

int SceDeci4pSDfMgrForKernel_86CFC8C6(int handle)
{
	unsigned state = ksceKernelCpuSuspendIntr();
	int result = sceSDfMgrExClose(handle);

	ksceKernelCpuResumeIntr(state);
	return result;
}

int sceSDfMgrSendSpace(int handle)
{
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	unsigned i;

	for (i = 0; i < 8; i++) {
		SdfMgrProtocolState *protocol = &cpu->protocols[i];

		if ((protocol->flags & 1) && protocol->handle == handle) {
			dcmpSendStatusSpace(protocol->registration.protocol);
			return 0;
		}
	}
#if FW_DEBUG
	SceDeci4pSDfMgrForKernel_501CE61D(15,
		SDFMGR_DEBUG_INFO(1573, 0x1c9f20a9, 0x27f1eed5,
				 "sceSDfMgrSendSpace"),
		"invalid argument (sock: 0x%08X)\n", handle);
#endif
	return (int)0x80090002;
}

int _threadSignal(int handle)
{
	unsigned state = ksceKernelCpuSuspendIntr();
	int result = sceSDfMgrExReqSend(handle);
	SdfMgrTransport *transport;

	if (result < 0)
		goto out;
	transport = s_sdfMgrState.cpu[sdfMgrCoreIndex()].transport;
	if (transport && (transport->operationFlags & 0x10))
		transport->poll(transport);
	if (s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 8) {
		int signalResult = ksceKernelTriggerSGI(1, 0,
					s_sdfMgrCpuTargets[ksceKernelCpuId()]);

#if FW_DEBUG
		if (signalResult < 0)
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				SDFMGR_DEBUG_INFO(1866, 0x3066a038, 0x0e527fa8,
						 "_threadSignal"),
				"generate SGI failed. (0x%08x)\n", signalResult);
#else
		(void)signalResult;
#endif
	}
	result = 0;
out:
	ksceKernelCpuResumeIntr(state);
	return result;
}

int sceSDfMgrExSend(int handle, const void *buffer, SceSize size)
{
	SdfMgrCpuState *cpu;
	SdfMgrProtocolState *protocol = NULL;
	SdfMgrTransport *transport;
	SceDeci4pHeader *header = (SceDeci4pHeader *)(uintptr_t)buffer;
	unsigned i;

	if (!buffer) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDFMGR_DEBUG_INFO(950, 0xc00fa542, 0x730426c3, "sceSDfMgrExSend"),
			"invalid argument\n");
#endif
		return (int)0x80090005;
	}
	cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	for (i = 0; i < 8; i++) {
		SdfMgrProtocolState *candidate = &cpu->protocols[i];

		if ((candidate->flags & 1) && candidate->handle == handle) {
			protocol = candidate;
			break;
		}
	}
	if (!protocol) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDFMGR_DEBUG_INFO(957, 0x2442ea6b, 0x730426c3, "sceSDfMgrExSend"),
			"invalid argument (sock: 0x%08X)\n", handle);
#endif
		return (int)0x80090001;
	}
	transport = s_sdfMgrState.cpu[sdfMgrCoreIndex()].transport;
	if (!transport) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDFMGR_DEBUG_INFO(964, 0x1dcfadcb, 0x730426c3, "sceSDfMgrExSend"),
			"no host if\n");
#endif
		return (int)0x8009000d;
	}
	if (!(transport->operationFlags & 8)) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDFMGR_DEBUG_INFO(969, 0x16de41f4, 0x730426c3, "sceSDfMgrExSend"),
			"blocked\n");
#endif
		return (int)0x80090007;
	}
	if (protocol->registration.protocol != 0x100 &&
	    protocol->registration.protocol != 0x2000) {
		if (!(s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 2) ||
		    !(protocol->flags & 2))
			return (int)0x8009000a;
	}
	if (!transport->sendSize) {
		unsigned shift;
		uint32_t cpuMask, wireProtocol, destinationProtocol;

		if (size < sizeof(*header)) {
#if FW_DEBUG
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				SDFMGR_DEBUG_INFO(999, 0x6dde7b56, 0x730426c3,
						 "sceSDfMgrExSend"),
				"invalid packet size %d (%s)\n", size, protocol->registration.name);
#endif
			return (int)0x80090006;
		}
		transport->sendSize = *(const uint16_t *)header->size |
				      (uint32_t)header->size[2] << 16;
		header->version = SCE_DECI4P_VERSION;
		shift = ksceKernelCpuId() & 0xff;
		wireProtocol = protocol->registration.protocol;
		header->reserved0 = 0;
		cpuMask = shift < 32 ? 1U << shift : 0;
		header->reserved1 = 0;
		destinationProtocol = protocol->registration.protocol;
		header->protocol = wireProtocol;
		header->src = (uint16_t)(0xa200 + cpuMask);
		header->reserved2 = 0;
		if (destinationProtocol != 0x100)
			header->dst = protocol->registration.destination;
	}
	return transport->send(transport, buffer, size);
}

void sdfMgrScheduleSend(SdfMgrCpuState *cpu)
{
	SdfMgrTransport *transport = cpu->transport;
	SdfMgrProtocolState *protocol;
	unsigned i;

	if (transport->sendProtocol)
		return;
	protocol = &cpu->protocols[cpu->lastSendIndex];
	for (i = 1; i <= 8; i++) {
		uint32_t flags = protocol->flags;

		if (flags & 4) {
			uint32_t operationFlags = transport->operationFlags;
			SdfMgrTransport *current = cpu->transport;

			protocol->flags = flags & ~4;
			transport->sendProtocol = protocol;
			transport->sendSize = 0;
			transport->operationFlags = operationFlags | 0x10;
			transport->sendProgress = 0;
			current->beginSend(transport);
			cpu->lastSendIndex = i == 8 ? 0 : i;
		}
		protocol++;
		if (protocol >= cpu->protocols + 8)
			protocol = cpu->protocols;
	}
}

int sceSDfMgrExReqSend(int handle)
{
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	SdfMgrCpuState *lookupCpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	SdfMgrProtocolState *protocol = NULL;
	uint32_t flags;
	unsigned i;

	for (i = 0; i < 8; i++) {
		SdfMgrProtocolState *candidate = &lookupCpu->protocols[i];

		if ((candidate->flags & 1) && candidate->handle == handle) {
			protocol = candidate;
			break;
		}
	}
	if (!protocol) {
#if FW_DEBUG
		static const SceKernelDebugInfo info = {
			.fileHash = 0x6364a003,
			.lineHash = 0xfee5ad50,
			.funcHash = 0xbe51d1c0,
			.file = "sdfmgr.c",
			.line = 848,
			.func = "sceSDfMgrExReqSend",
		};

		SceDeci4pSDfMgrForKernel_501CE61D(15, &info,
					 "invalid argument (sock: 0x%08X)\n", handle);
#endif
		return (int)0x80090002;
	}
	if (!s_sdfMgrState.cpu[sdfMgrCoreIndex()].transport) {
#if FW_DEBUG
		static const SceKernelDebugInfo info = {
			.fileHash = 0x6364a003,
			.lineHash = 0xab2b2ccb,
			.funcHash = 0xbe51d1c0,
			.file = "sdfmgr.c",
			.line = 855,
			.func = "sceSDfMgrExReqSend",
		};

		SceDeci4pSDfMgrForKernel_501CE61D(15, &info, "no host if\n");
#endif
		return (int)0x8009000d;
	}
	flags = protocol->flags;
	if (flags & 4)
		return (int)0x80090007;
	if (protocol->registration.protocol != 0x100) {
		if (!(s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 2) || !(flags & 2))
			return (int)0x8009000a;
	}
	protocol->flags = flags | 4;
	sdfMgrScheduleSend(cpu);
	return 0;
}

int sceSDfMgrExRecv(int handle, void *buffer, SceSize size)
{
	SdfMgrCpuState *cpu;
	SdfMgrTransport *transport;
	unsigned i;

	if (!buffer) {
#if FW_DEBUG
		static const SceKernelDebugInfo info = {
			.fileHash = 0x6364a003,
			.lineHash = 0xcf5df67e,
			.funcHash = 0xda0b64b9,
			.file = "sdfmgr.c",
			.line = 773,
			.func = "sceSDfMgrExRecv",
		};

		SceDeci4pSDfMgrForKernel_501CE61D(15, &info, "illegal buffer\n");
#endif
		return (int)0x80090005;
	}
	cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	for (i = 0; i < 8; i++) {
		if ((cpu->protocols[i].flags & 1) && cpu->protocols[i].handle == handle)
			break;
	}
	if (i == 8) {
#if FW_DEBUG
		static const SceKernelDebugInfo info = {
			.fileHash = 0x6364a003,
			.lineHash = 0xc25f26e9,
			.funcHash = 0xda0b64b9,
			.file = "sdfmgr.c",
			.line = 780,
			.func = "sceSDfMgrExRecv",
		};

		SceDeci4pSDfMgrForKernel_501CE61D(15, &info,
					 "invalid argument (sock: 0x%08X)\n", handle);
#endif
		return (int)0x80090002;
	}
	transport = s_sdfMgrState.cpu[sdfMgrCoreIndex()].transport;
	if (!transport) {
#if FW_DEBUG
		static const SceKernelDebugInfo info = {
			.fileHash = 0x6364a003,
			.lineHash = 0x76e351b2,
			.funcHash = 0xda0b64b9,
			.file = "sdfmgr.c",
			.line = 787,
			.func = "sceSDfMgrExRecv",
		};

		SceDeci4pSDfMgrForKernel_501CE61D(15, &info, "no host if\n");
#endif
		return (int)0x8009000d;
	}
	if (!(transport->flags & 4))
		return (int)0x80090007;
	if (transport->receiveSize)
		return transport->receive(transport, buffer, size);
	if (size < sizeof(transport->receiveHeader))
		return (int)0x80090006;
	for (i = 0; i < sizeof(transport->receiveHeader); i++)
		((uint8_t *)buffer)[i] = ((const uint8_t *)&transport->receiveHeader)[i];
	transport->receiveSize = *(const uint16_t *)transport->receiveHeader.size |
				 (uint32_t)transport->receiveHeader.size[2] << 16;
	transport->receiveProgress += sizeof(transport->receiveHeader);
	return (int)((uint32_t)transport->receive(transport,
			(uint8_t *)buffer + sizeof(transport->receiveHeader),
			size - sizeof(transport->receiveHeader)) +
		    sizeof(transport->receiveHeader));
}
