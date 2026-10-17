/*
 * SceUdcd: the UDC2 USB device controller of each bus (Sony's udcd_udc2.c,
 * named by its one debug record). Endpoint commands go through the command
 * register; transfers of endpoints 1-7 use DMA descriptors in a per-bus DMA
 * block, endpoint 0 is fed through its FIFO.
 */
#include <fw.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/sysmem/address_space.h>
#include <string.h>
#include <psp2kern/lowio/gpio.h>
#include <sysmem/debug.h>
#include <intrmgr/intrmgr.h>
#include <threadmgr/threadmgr.h>
#include <usbd/usb_regs.h>
#include <udcd/udcd_imports.h>
#include "udcd_internal.h"
#include <syscon/syscon.h>

/* controller commands (command register, with the endpoint in bits 4-7) */
#define UDC2_COMMAND_SETUP_DONE		2
#define UDC2_COMMAND_FLUSH		3
#define UDC2_COMMAND_STALL		4
#define UDC2_COMMAND_RESET		5
#define UDC2_COMMAND_ENABLE		7
#define UDC2_COMMAND_DISABLE		8
#define UDC2_COMMAND_EOP		0xc
#define UDC2_COMMAND_ABORT		0xd
#define UDC2_COMMAND_FIFO_CLEAR		0xe

#define UDC2_COMMAND_TIMEOUT		1000	/* microseconds */

extern int ksceKernelPowerTick(int type);
extern int sceKernelCancelAlarm(SceUID alarm);
extern unsigned int ksceKernelGetSystemTimeLow(void);

/* the interrupts of a bus: the controller, its DMA, and VBUS */
static inline int udcdControllerIntr(int bus)
{
	if (bus == 0)
		return 0xb5;
	if (bus == 1)
		return 0x97;
	if (bus == 2)
		return 0x9b;
	return -1;
}

static inline int udcdDmaIntr(int bus)
{
	if (bus == 0)
		return 0xb6;
	if (bus == 1)
		return 0x98;
	if (bus == 2)
		return 0x9c;
	return 0;
}

static inline int udcdVbusIntr(int bus)
{
	if (bus == 0)
		return 0xbf;
	if (bus == 1)
		return 0xa5;
	if (bus == 2)
		return 0xab;
	return -1;
}

/* Original name unknown. The cookie alarm: bus 2 has no data link, only power. */
SceUInt32 udcdCookieAlarm(void *argp)
{
	struct udcdBusState *state = &udcdState.buses[2];
	struct udcdControllerState *controller = &udcdState.controllers[2];
	SceKernelIntrStatus status;
	int cookie = 0;

	status = ksceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	if (ksceGpioPortRead(0, 5) == 0)
		cookie = ksceGpioPortRead(0, 0xc);
	controller->cookieTimer = -1;
	if (ScePowerForDriver_394DE492() != 0)
		ksceKernelPowerTick(0);
	if (cookie == 0) {
		state->activationState = SCE_UDCD_STATUS_IS_CHARGING;
		udcdUpdateStateEvent(2);
		if (controller->board == 1)
			ksceGpioPortClear(0, 0x18);
		ksceGpioPortClear(1, 3);
		ScePervasiveForDriver_4F64DEF6(2);
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return 0;
}

/* the bus of one of its interrupts */
static inline int udcdIntrBus(int intr)
{
	switch (intr) {
	case 0x97:
	case 0x98:
	case 0x99:
	case 0x9a:
		return 1;
	case 0x9b:
	case 0x9c:
	case 0x9d:
	case 0x9e:
		return 2;
	case 0xb5:
	case 0xb6:
	case 0xb7:
	case 0xbd:
		return 0;
	default:
		return -1;
	}
}

/* Original name unknown. A DMA transfer of an endpoint ended (or was stopped). */
int udcdCompleteDma(struct udcdControllerEndpoint *endpoint, SceKernelIntrStatus *status, int bus)
{
	struct udcdEndpointState *queue;
	SceUdcdDeviceRequest *request;
	unsigned char attributes, controlFlags;
	unsigned int number, received;

	if (udcdState.buses[bus].controllerState != UDCD_CONTROLLER_STARTED)
		return SCE_UDCD_ERROR_BUS_DRIVER_NOT_STARTED;
	attributes = endpoint->attributes;
	queue = endpoint->queue;
	controlFlags = endpoint->controlFlags;
	request = queue->requestHead;
	number = queue->endpoint->endpointNumber;
	endpoint->attributes = attributes & ~4;
	endpoint->controlFlags = controlFlags & ~0x20;
	controlFlags &= ~0x20;
	if (request == NULL)
		return 0;
	if (controlFlags & 2) {
		udcdCancelEndpointRequests(number, status, bus);
		return 0;
	}
	if (controlFlags & 1) {
		udcdCancelRequest(request, status, bus);
		return 0;
	}
	if (endpoint->attributes & 2) {
		/* the zero-length packet after the data went out */
		endpoint->attributes &= ~2;
		request->transmitted = 0;
	} else {
		received = udcdState.controllers[bus].registers->dma[number].received;
		request->transmitted = received;
		if (attributes & 4) {
			udcdCompleteRequest(queue, -4, status, bus);
			received = request->transmitted;
		}
		if (received != 0 && !request->isControlRequest && (endpoint->descriptor->control & 0x40000))
			endpoint->attributes |= 2;
	}
	udcdCompleteRequest(queue, 0, status, bus);
	return 0;
}

/* Run a controller command and wait (bounded) for it to take effect. */
static inline void _sceUdcdDoCommand(volatile struct udcdControllerRegisters *registers, int bus,
			      unsigned int command)
{
	volatile struct udcdControllerEndpointRegisters *endpoint;
	unsigned int number, start;

	registers->command = command;
	number = (command & 0xff) >> 4;
	command &= 0xf;
	start = ksceKernelGetSystemTimeLow();
	if (!((1 << command) & (1 << UDC2_COMMAND_SETUP_DONE | 1 << UDC2_COMMAND_FLUSH | 1 << UDC2_COMMAND_STALL |
				 1 << UDC2_COMMAND_RESET | 1 << UDC2_COMMAND_ENABLE | 1 << UDC2_COMMAND_DISABLE |
				 1 << 9 | 1 << UDC2_COMMAND_ABORT | 1 << UDC2_COMMAND_FIFO_CLEAR)))
		return;
	do {
		endpoint = &udcdState.controllers[bus].registers->endpoints[number];
		switch (command) {
		case UDC2_COMMAND_SETUP_DONE:
			if (!(endpoint->status & 0x3000))
				return;
			break;
		case UDC2_COMMAND_FLUSH:
			if (!(endpoint->status & 0xf00))
				return;
			break;
		case UDC2_COMMAND_STALL:
			if ((endpoint->status & 0xe00) == 0x600)
				return;
			break;
		case UDC2_COMMAND_RESET:
			if ((endpoint->status & 0xe00) == 0xe00)
				return;
			break;
		case UDC2_COMMAND_ENABLE:
			if (endpoint->status & 0x100)
				return;
			break;
		case UDC2_COMMAND_DISABLE:
			if (!(endpoint->status & 0x100))
				return;
			break;
		case 9:
			return;
		case UDC2_COMMAND_ABORT:
			if (endpoint->count == 0 && !(endpoint->config & 0x1000))
				return;
			break;
		case UDC2_COMMAND_FIFO_CLEAR:
			if (endpoint->config & 0x8000)
				return;
			break;
		default:
			return;
		}
	} while (ksceKernelGetSystemTimeLow() - start <= UDC2_COMMAND_TIMEOUT);
	SCE_DBG_PRINTF_LEVEL(1, 6, 149, "USB#%d Command[%08x] timeout\n", number, command);
}

/* Original name unknown. The cable went away: stop the drivers and the transfers. */
void udcdControllerDisconnect(int bus, SceKernelIntrStatus *status)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	struct udcdControllerState *controller = &udcdState.controllers[bus];
	int number, intr, dmaIntr;

	dmaIntr = 0xb6;
	intr = 0xb5;
	if (bus != 0) {
		if (bus == 1) {
			dmaIntr = 0x98;
			intr = 0x97;
		} else if (bus == 2) {
			dmaIntr = 0x9c;
			intr = 0x9b;
		} else {
			dmaIntr = 0;
			intr = -1;
		}
	}
	ksceKernelDisableIntr(intr);
	ksceKernelDisableIntr(dmaIntr);
	if (state->activationState != SCE_UDCD_STATUS_IS_CHARGING) {
		if (state->connectionState == SCE_UDCD_STATUS_CONNECTION_ESTABLISHED)
			udcdDetachDrivers(status, bus);
		for (number = 0; number != 8; number++) {
			controller->endpoints[number].controlFlags &= ~0x20;
			udcdCancelEndpointRequests(number, status, bus);
		}
	}
	state->suspendState = UDCD_BUS_RESUMED;
	state->cableState = SCE_UDCD_STATUS_CABLE_DISCONNECTED;
	state->usbVersion = -1;
	state->activationState = SCE_UDCD_STATUS_DEACTIVATED;
	state->connectionState = SCE_UDCD_STATUS_CONNECTION_NEW;
	state->waitError = 0;
	udcdUpdateStateEvent(bus);
	controller->savedConnectionState = 1;
	if (controller->board != 0) {
		if ((int)controller->cookieTimer > 0)
			sceKernelCancelAlarm(controller->cookieTimer);
		if (controller->board == 1)
			ksceGpioPortClear(0, 0x18);
		kscePowerBatterySetUsbStatus(0);
		ksceGpioPortClear(1, 3);
	}
	ScePervasiveForDriver_4F64DEF6(bus);
	if (ScePowerForDriver_394DE492() != 0)
		ksceKernelPowerTick(0);
}

/* Original name unknown. Suspend: the controller's interrupts off, its clock gated. */
int udcdSuspendController(int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	SceKernelIntrStatus status;
	int intr, vbusIntr;

	if (!udcdIsDriverBus(bus))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	status = ksceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	intr = udcdControllerIntr(bus);
	vbusIntr = udcdVbusIntr(bus);
	state->controllerResumed = 0;
	if (state->controllerState == UDCD_CONTROLLER_STARTED) {
		state->deviceInformation.lastEvent = 0x100000;
		ksceKernelDisableIntr(intr);
		ksceKernelDisableIntr(intr + 1);
		ksceKernelDisableIntr(vbusIntr);
		if (udcdState.controllers[bus].board != 0)
			ksceKernelDisableSubIntr(0xfa, 5);
		if (state->cableState != SCE_UDCD_STATUS_CABLE_DISCONNECTED && bus == 2)
			udcdControllerDisconnect(2, &status);
	}
	ScePervasiveForDriver_BB67EF58(bus);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return 0;
}

/* Original name unknown. Stop the controller and release its DMA block. */
int udcdStopController(int bus, SceKernelIntrStatus *status)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	struct udcdControllerState *controller = &udcdState.controllers[bus];
	int number, intr, dmaIntr, vbusIntr;

	if (state->controllerState == UDCD_CONTROLLER_STOPPED)
		return SCE_UDCD_ERROR_DRIVER_NOT_FOUND;
	if (bus == 0) {
		dmaIntr = 0xb6;
		intr = 0xb5;
		vbusIntr = 0xbf;
	} else if (bus == 1) {
		dmaIntr = 0x98;
		intr = 0x97;
		vbusIntr = 0xa5;
	} else if (bus == 2) {
		dmaIntr = 0x9c;
		intr = 0x9b;
		vbusIntr = 0xab;
	} else {
		intr = -1;
		dmaIntr = 0;
		vbusIntr = -1;
	}
	if (controller->board != 0)
		ksceKernelDisableSubIntr(0xfa, 5);
	ksceKernelDisableIntr(intr);
	ksceKernelDisableIntr(dmaIntr);
	ksceKernelDisableIntr(vbusIntr);
	if (state->connectionState == SCE_UDCD_STATUS_CONNECTION_ESTABLISHED)
		udcdDetachDrivers(status, bus);
	state->connectionState = SCE_UDCD_STATUS_CONNECTION_NEW;
	state->waitError = 0;
	state->controllerState = UDCD_CONTROLLER_STOPPED;
	state->usbVersion = -1;
	state->activationState = SCE_UDCD_STATUS_DEACTIVATED;
	state->cableState = SCE_UDCD_STATUS_CABLE_DISCONNECTED;
	udcdUpdateStateEvent(bus);
	state->resumeControllerState = UDCD_CONTROLLER_STOPPED;
	for (number = 0; number < 8; number++) {
		controller->endpoints[number].controlFlags &= ~0x20;
		udcdCancelEndpointRequests(number, status, bus);
	}
	if (controller->board != 0) {
		kscePowerBatterySetUsbStatus(0);
		if (state->controllerResumed != 1)
			goto release;
		ksceGpioPortClear(1, 3);
	}
	if (state->controllerResumed == 1)
		ScePervasiveForDriver_4F64DEF6(bus);
release:
	return ksceKernelFreeMemBlock(controller->dmaBlock) < 0 ? SCE_UDCD_ERROR_WAIT_CANCEL : 0;
}

/* Original name unknown. The DMA descriptor of an endpoint's first request. */
int udcdSetupDma(int number, int bus)
{
	struct udcdControllerEndpoint *endpoint = &udcdState.controllers[bus].endpoints[number];
	struct udcdEndpointState *queue = endpoint->queue;
	struct udcdDmaDescriptor *descriptor = endpoint->descriptor;
	SceUdcdDeviceRequest *request = queue->requestHead;
	unsigned int in = queue->direction & 0x80;
	unsigned int type = queue->type;
	int size;

	if (in) {
		descriptor->control = type == 1 ? 0x3c : 0x2c;
	} else if (type - 2 > 1) {
		descriptor->control = type == 1 ? 0x1d : 0xc;
	} else {
		descriptor->control = 0xd;
	}
	size = request->size;
	if (size == 0) {
		descriptor->buffer = NULL;
		descriptor->size = 0;
		descriptor->next = -1;
		if (!in) {
			request->returnCode = 7;
			return 0;
		}
	} else {
		descriptor->buffer = request->physicalAddress;
		descriptor->size = size;
		descriptor->next = -1;
		if (!in) {
			request->returnCode = 5;
			return 0;
		}
		/* a full last packet: a zero-length packet ends the transfer */
		if ((unsigned int)size % queue->maxPacketSize == 0 && request->isControlRequest == 1) {
			descriptor->control = type == 1 ? 0x30 : 0x20;
			descriptor->next = endpoint->dmaAddress + sizeof(*descriptor);
			descriptor[1].control = type == 1 ? 0x3c : 0x2c;
			descriptor[1].buffer = NULL;
			descriptor[1].size = 0;
			descriptor[1].next = -1;
		}
	}
	request->returnCode = 4;
	return 0;
}

/* Original name unknown. Start the DMA of an endpoint's first request. */
void udcdSubmitDma(int number, SceKernelIntrStatus *status, int bus)
{
	struct udcdControllerState *controller = &udcdState.controllers[bus];
	struct udcdControllerEndpoint *endpoint = &controller->endpoints[number];
	volatile struct udcdControllerDmaRegisters *dma;
	unsigned char flags = endpoint->flags;
	unsigned int dmaNumber;

	if ((flags & 0x80) || (flags & 1) || number == 0)
		return;
	endpoint->flags = flags | 1;
	if (udcdState.buses[bus].controllerState == UDCD_CONTROLLER_STARTED) {
		dmaNumber = endpoint->queue->endpoint->endpointNumber;
		if (!(endpoint->attributes & 2)) {
			udcdSetupDma(dmaNumber, bus);
			dma = &controller->registers->dma[dmaNumber];
			dma->descriptor = controller->endpoints[dmaNumber].dmaAddress;
			dma->control = 1;
			endpoint->controlFlags |= 0x20;
			return;
		}
		if (udcdCompleteDma(endpoint, status, bus) >= 0)
			return;
	}
	endpoint->flags &= ~1;
}

/* Original name unknown. Reset an endpoint's state and its controller side. */
void udcdResetEndpoint(int number, int bus)
{
	struct udcdControllerState *controller = &udcdState.controllers[bus];
	struct udcdControllerEndpoint *endpoint = &controller->endpoints[number];

	endpoint->flags = 0;
	endpoint->controlFlags = 0;
	endpoint->attributes = 0;
	endpoint->spareFlags = 0;
	_sceUdcdDoCommand(controller->registers, bus, number << 4 | UDC2_COMMAND_RESET);
}

/* Original name unknown. Stop an endpoint's DMA and abort what its FIFO holds. */
int udcdAbortEndpoint(int number, int bus)
{
	struct udcdControllerState *controller;
	struct udcdControllerEndpoint *endpoint;
	volatile struct udcdControllerRegisters *registers;
	volatile struct udcdControllerDmaRegisters *dma;

	if (!udcdIsDriverBus(bus))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	if (udcdState.buses[bus].controllerState != UDCD_CONTROLLER_STARTED)
		return SCE_UDCD_ERROR_BUS_DRIVER_NOT_STARTED;
	controller = &udcdState.controllers[bus];
	endpoint = &controller->endpoints[number];
	registers = controller->registers;
	if (number != 0) {
		dma = &registers->dma[number];
		if (dma->status & 1) {
			dma->control &= ~1;
			return SCE_UDCD_ERROR_DRIVER_IN_PROGRESS;
		}
		registers = controller->registers;
		dma->control = 2;
		while (dma->control != 2 && !(registers->interruptStatus & 3))
			;
		dma->control = 0;
	}
	if (registers->interruptStatus & 4)
		_sceUdcdDoCommand(controller->registers, bus, number << 4 | UDC2_COMMAND_ABORT);
	endpoint->flags = 0;
	endpoint->controlFlags = 0;
	endpoint->attributes = 0;
	endpoint->spareFlags = 0;
	return 0;
}

/* Original name unknown. Set an endpoint up from its descriptor. */
int udcdConfigureEndpoint(const SceUdcdEndpointDescriptor *descriptor, int bus)
{
	struct udcdControllerState *controller = &udcdState.controllers[bus];
	struct udcdEndpointState *queue;
	volatile struct udcdControllerRegisters *registers;
	unsigned int number, type, maxPacketSize, size, config;

	if (descriptor == NULL)
		return 0x80243306;
	number = descriptor->bEndpointAddress & 0xf;
	type = descriptor->bmAttributes & 3;
	maxPacketSize = descriptor->wMaxPacketSize & 0x7ff;
	config = 0;
	queue = controller->endpoints[number].queue;
	queue->type = type;
	queue->maxPacketSize = maxPacketSize;
	queue->transferState = 0;
	queue->direction = descriptor->bEndpointAddress & 0x80;
	if (number != 0) {
		if ((number - 1 < 2 && maxPacketSize <= 0x400) || (number > 2 && maxPacketSize <= 0x200))
			config = 0xc000;
		else
			config = 0x4000;
	}
	if (type == 2)
		config |= 8;
	else if (type == 3)
		config |= 0xc;
	else if (type == 1)
		config |= 4;
	if (queue->direction)
		config |= 0x80;
	size = maxPacketSize;
	if (descriptor->wMaxPacketSize & 1)
		size++;
	registers = controller->registers;
	registers->endpoints[number].config = size;
	registers->endpoints[number].status = config;
	registers->dma[number].maxPacket = queue->maxPacketSize;
	_sceUdcdDoCommand(controller->registers, bus, number << 4 | UDC2_COMMAND_ENABLE);
	_sceUdcdDoCommand(controller->registers, bus, number << 4 | UDC2_COMMAND_FLUSH);
	controller->endpoints[number].flags = 0;
	controller->endpoints[number].controlFlags = 0;
	controller->endpoints[number].attributes = 0;
	controller->endpoints[number].spareFlags = 0;
	controller->registers->dma[number].endpointConfig = 0x10000;
	return 0;
}

/* Original name unknown. Bus 2 with power: a host, or only a charger (the cookie). */
int udcdDetectCookie(int wake)
{
	struct udcdBusState *state = &udcdState.buses[2];
	struct udcdControllerState *controller = &udcdState.controllers[2];
	volatile struct udcdControllerRegisters *registers;
	SceKernelIntrStatus status;

	status = ksceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	ScePervasiveForDriver_BB67EF58(2);
	if ((udcdState.registers->usbVbus & 4) && state->cableState == SCE_UDCD_STATUS_CABLE_CONNECTED) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
		return -1;
	}
	if (ksceGpioPortRead(0, 5) == 0) {
		if (!(udcdState.registers->usbVbus & 4)) {
			state->suspendState = UDCD_BUS_RESUMED;
			state->usbVersion = -1;
			state->activationState = SCE_UDCD_STATUS_ACTIVATED;
			state->cableState = SCE_UDCD_STATUS_CABLE_CONNECTED;
			state->waitError = 0x80243ffe;
			state->connectionState = SCE_UDCD_STATUS_CONNECTION_NEW;
			if (wake)
				state->deviceInformation.lastEvent = 1;
			udcdUpdateStateEvent(2);
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
			return -1;
		}
		ScePervasiveForDriver_A2EFD7AF(2, 10);
		ScePervasiveForDriver_13CC07C9(2, 10);
		registers = controller->registers;
		registers->initControl[0] = 0;
		registers->initControl[1] = 0;
		if (udcdPollBusInitialized(0xfa, 2) >= 0) {
			registers = controller->registers;
			registers->interruptMask = 0xfffffffe;
			registers->interruptEnable = 1;
			registers->eventEnable = 0x380000;
			ksceGpioPortSet(1, 3);
			controller->registers->interrupt = controller->config | 0x10;
			udcdUpdateUsbCharging(2);
			state->connectionState = SCE_UDCD_STATUS_CONNECTION_NEW;
			state->suspendState = UDCD_BUS_RESUMED;
			state->usbVersion = -1;
			state->activationState = SCE_UDCD_STATUS_DEACTIVATED;
			state->cableState = SCE_UDCD_STATUS_CABLE_CONNECTED;
			if (wake)
				state->deviceInformation.lastEvent = 1;
			udcdUpdateStateEvent(2);
			controller->savedConnectionState = 1;
			controller->registers->endpoints[0].config = 0x40;
			_sceUdcdDoCommand(controller->registers, 2, 10);
			controller->cookieTimer = ksceKernelRegisterTimer("SceUdcdAlarmCookieDetect", 500000,
									  udcdCookieAlarm, NULL);
			if (controller->board == 1)
				ksceGpioPortSet(0, 0x18);
			kscePowerBatterySetUsbStatus(2);
			ksceKernelEnableIntr(0x9b);
			ksceKernelEnableIntr(0x9c);
		}
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return -1;
}

/* Original name unknown. VBUS came up: power the controller on and wait for the host. */
void udcdVbusConnected(int bus, SceKernelIntrStatus *status)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	struct udcdControllerState *controller = &udcdState.controllers[bus];
	volatile struct udcdControllerRegisters *registers;
	unsigned int config;
	int intr;

	intr = 0xb5;
	if (bus != 0) {
		if (bus == 1)
			intr = 0x97;
		else if (bus == 2)
			intr = 0x9b;
		else
			intr = -1;
	}
	if (controller->board == 0) {
		ScePervasiveForDriver_BB67EF58(bus);
		ScePervasiveForDriver_A2EFD7AF(bus, 10);
		ScePervasiveForDriver_13CC07C9(bus, 10);
		registers = controller->registers;
		registers->initControl[0] = 0;
		registers->initControl[1] = 0;
		if (udcdPollBusInitialized(0xfa, bus) >= 0) {
			config = controller->config;
			registers = controller->registers;
			registers->interruptMask = 0xfffffffe;
			registers->interruptEnable = 1;
			registers->eventEnable = 0x380000;
			registers->interrupt = config | 0x10;
			udcdUpdateUsbCharging(bus);
			if (state->connectionState == SCE_UDCD_STATUS_CONNECTION_ESTABLISHED) {
				udcdDetachDrivers(status, bus);
				state->connectionState = SCE_UDCD_STATUS_CONNECTION_NEW;
				udcdUpdateStateEvent(bus);
			}
			state->activationState = SCE_UDCD_STATUS_DEACTIVATED;
			state->cableState = SCE_UDCD_STATUS_CABLE_CONNECTED;
			state->usbVersion = -1;
			state->suspendState = UDCD_BUS_RESUMED;
			state->connectionState = SCE_UDCD_STATUS_CONNECTION_NEW;
			state->waitError = 0;
			udcdUpdateStateEvent(bus);
			controller->savedConnectionState = 1;
			controller->registers->endpoints[0].config = 0x40;
			_sceUdcdDoCommand(controller->registers, bus, 10);
			ksceKernelEnableIntr(intr);
			ksceKernelEnableIntr(intr + 1);
		}
		if (ScePowerForDriver_394DE492() != 0)
			ksceKernelPowerTick(0);
	} else if (state->cableState == SCE_UDCD_STATUS_CABLE_DISCONNECTED && ksceGpioPortRead(0, 5) == 0) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, *status);
		udcdDetectCookie(0);
		*status = ksceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	}
}

/* Original name unknown. The VBUS interrupt of a bus: plugged in or pulled out. */
int udcdVbusInterrupt(int intr, void *argp)
{
	volatile ScePervasive2Registers *phy;
	struct udcdBusState *state;
	SceKernelIntrStatus status;
	unsigned int rise, fall, shift;
	int bus;

	if (intr == 0xbf) {
		shift = 0;
		rise = 1;
		bus = 0;
		fall = 2;
	} else if (intr == 0xa5) {
		rise = 0x10;
		fall = 0x20;
		shift = 4;
		bus = 1;
	} else if (intr == 0xab) {
		rise = 0x100;
		fall = 0x200;
		shift = 8;
		bus = 2;
	} else {
		rise = 0;
		shift = -4;
		fall = 0;
		bus = -1;
	}
	state = &udcdState.buses[bus];
	status = ksceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	phy = udcdState.registers;
	if ((phy->usbVbusInterrupt >> shift) & 1) {
		phy->usbVbusInterrupt = rise;
		if (state->cableState == SCE_UDCD_STATUS_CABLE_DISCONNECTED && ((phy->usbVbus >> bus) & 1)) {
			state->deviceInformation.lastEvent = 1;
			udcdVbusConnected(bus, &status);
			phy = udcdState.registers;
		}
	}
	if (fall & phy->usbVbusInterrupt) {
		phy->usbVbusInterrupt = fall;
		if (state->cableState != SCE_UDCD_STATUS_CABLE_DISCONNECTED && !((phy->usbVbus >> bus) & 1)) {
			state->deviceInformation.lastEvent = 0x10000;
			udcdControllerDisconnect(bus, &status);
		}
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return -1;
}

/* Original name unknown. Resume: the controller back on and VBUS watched again. */
int udcdResumeController(int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	struct udcdControllerState *controller = &udcdState.controllers[bus];
	SceKernelIntrStatus status;
	int vbusIntr;

	if (!udcdIsDriverBus(bus))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	status = ksceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	vbusIntr = udcdVbusIntr(bus);
	state->controllerResumed = 1;
	ScePervasiveForDriver_4F64DEF6(bus);
	if (state->controllerState == UDCD_CONTROLLER_STARTED) {
		if (state->deviceInformation.lastEvent != 4)
			state->deviceInformation.lastEvent = 0x10;
		if (controller->board == 0) {
			if ((udcdState.registers->usbVbus >> bus) & 1)
				udcdVbusConnected(bus, &status);
		} else if (ksceGpioPortRead(0, 5) == 0) {
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
			udcdDetectCookie(0);
			status = ksceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
		}
		ksceKernelEnableIntr(vbusIntr);
		if (controller->board != 0)
			ksceKernelEnableSubIntr(0xfa, 5);
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return 0;
}

/* Original name unknown. Stall an endpoint (endpoint 0: the control pipe). */
int udcdStallEndpoint(int number, int bus)
{
	struct udcdControllerState *controller;
	struct udcdControllerEndpoint *endpoint;
	volatile struct udcdControllerDmaRegisters *dma;

	if (!udcdIsDriverBus(bus))
		return SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT;
	if (udcdState.buses[bus].controllerState != UDCD_CONTROLLER_STARTED)
		return SCE_UDCD_ERROR_BUS_DRIVER_NOT_STARTED;
	controller = &udcdState.controllers[bus];
	endpoint = &controller->endpoints[number];
	if (number == 0) {
		_sceUdcdDoCommand(controller->registers, bus, UDC2_COMMAND_STALL);
		endpoint->flags |= 0x80;
		return 0;
	}
	dma = &controller->registers->dma[number];
	if (dma->status & 1) {
		dma->control &= ~1;
		endpoint->flags |= 0x80;
		return SCE_UDCD_ERROR_DRIVER_IN_PROGRESS;
	}
	dma->control = 2;
	while (dma->control != 2) {
		if (controller->registers->interruptStatus & 3)
			break;
		dma->control = 0;
	}
	_sceUdcdDoCommand(controller->registers, bus, number << 4 | UDC2_COMMAND_ABORT);
	_sceUdcdDoCommand(controller->registers, bus, number << 4 | UDC2_COMMAND_STALL);
	endpoint->controlFlags = 0;
	endpoint->attributes = 0;
	endpoint->spareFlags = 0;
	endpoint->flags = 0x80;
	return 0;
}

/* Original name unknown. The DMA interrupt of a bus: the events of endpoints 1-7. */
int udcdDmaInterrupt(int intr, void *argp)
{
	struct udcdBusState *state;
	struct udcdControllerState *controller;
	struct udcdControllerEndpoint *endpoint;
	struct udcdEndpointState *queue;
	SceUdcdDeviceRequest *request;
	volatile unsigned int *events;
	SceKernelIntrStatus status;
	unsigned int pending, pendingEvents;
	int bus, number;

	bus = udcdIntrBus(intr);
	state = &udcdState.buses[bus];
	controller = &udcdState.controllers[bus];
	status = ksceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	if (((udcdState.registers->usbVbus >> bus) & 1) && state->cableState == SCE_UDCD_STATUS_CABLE_CONNECTED) {
		pending = controller->registers->dmaInterrupt;
		number = 1;
		do {
			if (!((pending >> number) & 1))
				goto next;
			events = &controller->registers->dma[number].interrupt;
			pendingEvents = *events;
			endpoint = &controller->endpoints[number];
			if (pendingEvents & 1) {
				*events = 1;
				if (pendingEvents & 8)
					*events = 8;
				if (pendingEvents & 4)
					*events = 4;
				if (number & 1) {
					/* IN: the whole request went out */
					if (state->controllerState == UDCD_CONTROLLER_STARTED) {
						queue = endpoint->queue;
						request = queue->requestHead;
						if (queue->endpoint->endpointNumber != 0) {
							endpoint->controlFlags &= ~0x20;
							if (request != NULL) {
								request->transmitted = request->size;
								udcdCompleteRequest(queue, 0, &status, bus);
							}
						}
					}
				} else {
					udcdCompleteDma(endpoint, &status, bus);
				}
			} else if (pendingEvents & 4) {
				*events = 4;
				if (!(number & 1))
					udcdCompleteDma(endpoint, &status, bus);
			} else if (pendingEvents & 8) {
				*events = 8;
				if (!(number & 1))
					udcdCompleteDma(endpoint, &status, bus);
			} else if (pendingEvents & 0x10) {
				*events = 0x10;
				if (!(number & 1)) {
					endpoint->attributes |= 4;
					udcdCompleteDma(endpoint, &status, bus);
				}
			} else if (pendingEvents & 0x40) {
				*events = 0x40;
				if (endpoint->flags & 0x80)
					udcdStallEndpoint(number, bus);
				else
					udcdAbortEndpoint(number, bus);
				udcdCompleteRequest(endpoint->queue, -5, &status, bus);
			} else if (pendingEvents & 0x100) {
				*events = 0x100;
				udcdAbortEndpoint(number, bus);
				udcdCompleteRequest(endpoint->queue, -6, &status, bus);
			} else {
				if (pendingEvents & 0x80)
					*events = 0x80;
				if (pendingEvents & 2)
					controller->registers->dma[number].interrupt = 2;
				if (pendingEvents & 0x20)
					*events = 0x20;
				if (pendingEvents & 0x20000)
					*events = 0x20000;
				if (pendingEvents & 0x40000) {
					*events = 0x40000;
					endpoint->flags = 0;
					endpoint->controlFlags = 0;
					endpoint->attributes = 0;
					endpoint->spareFlags = 0;
					udcdCompleteRequest(endpoint->queue, -6, &status, bus);
				}
			}
next:
			number++;
		} while (number <= 7 && state->cableState == SCE_UDCD_STATUS_CABLE_CONNECTED);
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return -1;
}

/* the configuration in use: high speed (USB 2.0) or full speed */
static inline SceUdcdConfiguration *udcdActiveConfiguration(struct udcdBusState *state)
{
	return state->usbVersion == 2 ? state->highSpeedConfiguration : state->fullSpeedConfiguration;
}

/* Original name unknown. Stop the transfers of every endpoint the drivers use. */
static inline void udcdAbortDriverEndpoints(SceKernelIntrStatus *status, int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	struct udcdControllerState *controller = &udcdState.controllers[bus];
	int number;

	for (number = 0; number < (int)state->endpointCount; number++) {
		if (controller->endpoints[number].queue->endpoint != NULL) {
			udcdAbortEndpoint(number, bus);
			udcdCancelEndpointRequests(number, status, bus);
		}
	}
}

/* Original name unknown. The standard requests of the control pipe. */
int udcdStandardRequest(SceUdcdEP0DeviceRequest *request, SceKernelIntrStatus *status, int bus)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	struct udcdControllerState *controller = &udcdState.controllers[bus];
	SceUdcdEP0DeviceRequest *control = &state->controlRequest;
	SceUdcdDriver *driver;
	unsigned int recipient, number, alternate;
	int result;

	switch (request->bRequest) {
	case 0:		/* GET_STATUS */
		return udcdGetStatusRequest(status, bus);
	case 1:		/* CLEAR_FEATURE */
		recipient = control->bmRequestType & 0x1f;
		if (recipient == 0) {
			if (control->wValue == 1 &&
			    (state->fullSpeedConfiguration->configDescriptors->bmAttributes & 0x20) &&
			    (state->connectionState == UDCD_CONNECTION_ADDRESSED || state->connectionState == SCE_UDCD_STATUS_CONNECTION_ESTABLISHED)) {
				controller->remoteWakeup = 0;
				return 0;
			}
		} else if (recipient == 2 && state->connectionState == SCE_UDCD_STATUS_CONNECTION_ESTABLISHED && control->wValue == 0) {
			number = control->wIndex & 0xf;
			if (number == 0)
				return 0;
			udcdAbortEndpoint(number, bus);
			udcdCancelEndpointRequests(control->wIndex & 0xf, status, bus);
			_sceUdcdDoCommand(controller->registers, bus,
					  (control->wIndex & 0xf) << 4 | UDC2_COMMAND_FLUSH);
			return 0;
		}
		break;
	case 3:		/* SET_FEATURE */
		recipient = control->bmRequestType & 0x1f;
		if (recipient == 0) {
			if (control->wValue == 2) {
				/* TEST_MODE */
				if ((control->wIndex >> 8) - 1 < 4)
					return 0;
			} else if (control->wValue == 1 &&
				   (state->fullSpeedConfiguration->configDescriptors->bmAttributes & 0x20) &&
				   (state->connectionState == UDCD_CONNECTION_ADDRESSED || state->connectionState == SCE_UDCD_STATUS_CONNECTION_ESTABLISHED)) {
				controller->remoteWakeup = 1;
				return 0;
			}
		} else if (recipient == 2 && state->connectionState == SCE_UDCD_STATUS_CONNECTION_ESTABLISHED && control->wValue == 0) {
			udcdStallEndpoint(control->wIndex & 0xf, bus);
			return 0;
		}
		break;
	case 5:		/* SET_ADDRESS */
		if (state->connectionState == SCE_UDCD_STATUS_CONNECTION_NEW) {
			if (control->wValue == 0)
				return 0;
			if ((controller->registers->address & 0x7f) != control->wValue)
				controller->registers->address = (control->wValue & 0x7f) | 0x200;
		} else {
			if (state->connectionState != UDCD_CONNECTION_ADDRESSED)
				break;
			if (control->wValue == 0) {
				controller->registers->address = 0x100;
				state->connectionState = SCE_UDCD_STATUS_CONNECTION_NEW;
				udcdUpdateStateEvent(bus);
				goto address;
			}
			controller->registers->address = (control->wValue & 0x7f) | 0x200;
		}
		state->connectionState = UDCD_CONNECTION_ADDRESSED;
		udcdUpdateStateEvent(bus);
address:
		controller->address = control->wValue & 0x7f;
		return 0;
	case 6:		/* GET_DESCRIPTOR */
		return udcdGetDescriptorRequest(status, bus);
	case 8:		/* GET_CONFIGURATION */
		return udcdGetConfigurationRequest(status, bus);
	case 9:		/* SET_CONFIGURATION */
		if (state->connectionState == SCE_UDCD_STATUS_CONNECTION_NEW)
			break;
		if (control->wValue != 0) {
			if (control->wValue != 1)
				return 0x80243f01;
			controller->registers->address = (controller->address & 0x7f) | 0x400;
			if (state->connectionState == SCE_UDCD_STATUS_CONNECTION_ESTABLISHED) {
				udcdDetachDrivers(status, bus);
				udcdAbortDriverEndpoints(status, bus);
			}
			udcdInitializeInterfaceSettings(udcdActiveConfiguration(state)->configDescriptors, status, bus);
			return 0;
		}
		if (state->connectionState == UDCD_CONNECTION_ADDRESSED)
			return 0;
		if (state->connectionState == SCE_UDCD_STATUS_CONNECTION_ESTABLISHED) {
			controller->registers->address = (controller->address & 0x7f) | 0x200;
			_sceUdcdDoCommand(controller->registers, bus, 9);
			for (number = 1; number < 8; number++)
				if ((controller->registers->endpoints[number].status & 0xe00) != 0xe00)
					break;
			udcdDetachDrivers(status, bus);
			udcdAbortDriverEndpoints(status, bus);
			state->connectionState = UDCD_CONNECTION_ADDRESSED;
			udcdUpdateStateEvent(bus);
			return 0;
		}
		break;
	case 10:	/* GET_INTERFACE */
		result = udcdGetInterfaceRequest(udcdActiveConfiguration(state)->configDescriptors, status, bus);
		if (result >= 0) {
			udcdNotifyControlRequest(status, bus);
			return 0;
		}
		goto out;
	case 11:	/* SET_INTERFACE */
		number = control->wIndex;
		alternate = control->wValue;
		result = udcdSetInterfaceSetting(number, alternate, udcdActiveConfiguration(state)->configDescriptors,
						 status, bus);
		if (result >= 0) {
			for (driver = state->drivers; driver != NULL; driver = driver->link) {
				if ((int)number < driver->interface->numInterfaces + driver->interface->interfaceNumber) {
					if (driver->changeSetting != NULL) {
						ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, *status);
						driver->changeSetting(number - driver->interface->interfaceNumber,
								      alternate, bus);
						*status = ksceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
					}
					break;
				}
			}
			udcdNotifyControlRequest(status, bus);
			return 0;
		}
		goto out;
	}
	result = -1;
out:
	udcdNotifyControlRequest(status, bus);
	return result;
}

/* Original name unknown. Send the next (up to 64 byte) packet of endpoint 0's data. */
static inline void udcdSendControlPacket(struct udcdControllerState *controller, int bus)
{
	int remaining = controller->controlRemaining;
	int sent = 0;

	while (sent < (remaining < 0x40 ? remaining : 0x40)) {
		if (remaining - sent < 2) {
			controller->registers->endpoints[0].fifo.byte = controller->controlData[0];
			sent++;
			controller->controlData++;
			remaining = controller->controlRemaining;
		} else {
			controller->registers->endpoints[0].fifo.word = controller->controlData[0] |
								   controller->controlData[1] << 8;
			sent += 2;
			controller->controlData += 2;
		}
	}
	if (remaining < 0x40) {
		_sceUdcdDoCommand(controller->registers, bus, UDC2_COMMAND_EOP);
		remaining = controller->controlRemaining;
	}
	controller->controlRemaining = remaining - sent;
}

/* Original name unknown. Start the controller: DMA descriptors, PHY, VBUS watching. */
int udcdStartController(int bus, SceKernelIntrStatus *status)
{
	struct udcdBusState *state = &udcdState.buses[bus];
	struct udcdControllerState *controller = &udcdState.controllers[bus];
	SceKernelAllocMemBlockKernelOpt opt;
	volatile ScePervasive2Registers *phy;
	uintptr_t base = 0, address;
	unsigned int enabled;
	SceUID block;
	int vbusIntr, number;

	if (state->controllerState == UDCD_CONTROLLER_STARTED)
		return SCE_UDCD_ERROR_INVALID_ARGUMENT;
	vbusIntr = udcdVbusIntr(bus);
	controller->config = 0xd800;
	state->controllerState = UDCD_CONTROLLER_STARTED;
	state->usbVersion = -1;
	state->activationState = SCE_UDCD_STATUS_DEACTIVATED;
	state->cableState = SCE_UDCD_STATUS_CABLE_DISCONNECTED;
	state->connectionState = SCE_UDCD_STATUS_CONNECTION_NEW;
	state->waitError = 0;
	udcdUpdateStateEvent(bus);
	controller->savedConnectionState = 1;
	controller->remoteWakeup = 0;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 0x200000;
	block = ksceKernelAllocMemBlock("SceUdcdDmaTag", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_DEVICE_RW, 0x1000, &opt);
	if (block >= 0) {
		if (ksceKernelGetMemBlockBase(block, (void **)&base) >= 0) {
			controller->dmaBlock = block;
			for (number = 1; number < 8; number++) {
				if (controller->endpoints[number].queue->endpoint == NULL)
					continue;
				controller->endpoints[number].descriptor = (struct udcdDmaDescriptor *)base;
				base += 0x200;
				if (ksceKernelVAtoPA(controller->endpoints[number].descriptor, &address) < 0)
					address = 0x80243407;
				controller->endpoints[number].dmaAddress = address;
			}
			udcdPollBusInitialized(200, bus);
			if (controller->board != 0) {
				ksceGpioSetPortMode(0, 5, 1);
				ksceGpioSetPortMode(0, 0xc, 1);
				ksceGpioSetIntrMode(0, 5, 1);
				if (controller->board == 1) {
					ksceGpioSetPortMode(0, 0x18, 0);
					ksceGpioPortClear(0, 0x18);
				}
			}
			phy = udcdState.registers;
			enabled = state->controllerResumed;
			phy->usbVbusInterruptEnable &= ~(3 << (bus << 2));
			if (enabled != 1)
				return 0;
			if (controller->board == 0) {
				if ((phy->usbVbus >> bus) & 1)
					udcdVbusConnected(bus, status);
			} else {
				if (ksceGpioPortRead(0, 5) == 0) {
					ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, *status);
					udcdDetectCookie(0);
					*status = ksceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
				}
				if (ksceKernelEnableSubIntr(0xfa, 5) < 0)
					return 0x80243307;
			}
			ksceKernelEnableIntr(vbusIntr);
			return 0;
		}
		ksceKernelFreeMemBlock(block);
	}
	ksceKernelFreeMemBlock(state->deviceMemBlock);
	return SCE_UDCD_ERROR_WAIT_TIMEOUT;
}

/* Original name unknown. Send an endpoint's first request (endpoint 0 through its FIFO). */
void udcdSendRequest(int number, SceKernelIntrStatus *status, int bus)
{
	struct udcdControllerState *controller = &udcdState.controllers[bus];
	struct udcdControllerEndpoint *endpoint = &controller->endpoints[number];
	volatile struct udcdControllerRegisters *registers;
	volatile struct udcdControllerDmaRegisters *dma;
	struct udcdEndpointState *queue;
	SceUdcdDeviceRequest *request;
	unsigned char flags = endpoint->flags;
	unsigned int dmaNumber;

	if ((flags & 0x80) || (flags & 1))
		return;
	endpoint->flags = flags | 1;
	if (number == 0) {
		registers = controller->registers;
		request = controller->endpoints[0].queue->requestHead;
		controller->controlData = request->data;
		controller->controlRemaining = request->size;
		while (!(registers->interruptStatus & 3)) {
			if (registers->endpoints[0].count == 0) {
				udcdSendControlPacket(controller, bus);
				return;
			}
		}
		return;
	}
	if (udcdState.buses[bus].controllerState == UDCD_CONTROLLER_STARTED) {
		queue = endpoint->queue;
		request = queue->requestHead;
		dmaNumber = queue->endpoint->endpointNumber;
		if (!(endpoint->controlFlags & 0x20) && request != NULL) {
			udcdSetupDma(dmaNumber, bus);
			dma = &controller->registers->dma[dmaNumber];
			dma->descriptor = controller->endpoints[dmaNumber].dmaAddress;
			dma->control = 1;
			endpoint->controlFlags |= 0x20;
			if (request->size >= 0)
				return;
		}
	}
	endpoint->flags &= ~1;
}

/* Original name unknown. The controller interrupt of a bus: endpoint 0 and the bus events. */
int udcdControllerInterrupt(int intr, void *argp)
{
	struct udcdBusState *state;
	struct udcdControllerState *controller;
	volatile struct udcdControllerRegisters *registers;
	SceUdcdEP0DeviceRequest *control;
	struct udcdEndpointState *queue;
	SceUdcdDeviceRequest *request;
	SceUdcdDeviceDescriptor *device;
	SceKernelIntrStatus status;
	unsigned int events, config, length, received, word, recipient;
	int bus, number;

	bus = udcdIntrBus(intr);
	if (bus != 0 && bus != 1 && bus != 2)
		return -1;
	state = &udcdState.buses[bus];
	controller = &udcdState.controllers[bus];
	control = &state->controlRequest;
	status = ksceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	queue = controller->endpoints[0].queue;
	if (state->cableState != SCE_UDCD_STATUS_CABLE_CONNECTED)
		goto unlock;
	registers = controller->registers;
	events = registers->events;
	if (!((udcdState.registers->usbVbus >> bus) & 1)) {
		registers->events = events;
		goto unlock;
	}
	if (events == 0)
		goto unlock;
	if (events & 0x80)
		registers->interrupt = controller->config | 0x80;
	if (events & 0x20) {
		registers->interrupt = controller->config | 0x20;
		if (registers->endpoints[0].count == 0) {
			/* IN data stage: the next packet, or the end */
			if (controller->controlRemaining < 1 || controller->controlData == NULL) {
				request = queue->requestHead;
				if (request != NULL)
					request->transmitted = request->size;
				udcdCompleteRequest(queue, 0, &status, bus);
			} else {
				do {
					if (registers->interruptStatus & 3)
						goto setup;
				} while (registers->endpoints[0].count != 0);
				udcdSendControlPacket(controller, bus);
			}
		} else {
			/* OUT data stage */
			request = queue->requestHead;
			if (request != NULL && control->wLength <= (unsigned int)request->size) {
				length = control->wLength;
				received = 0;
				while (received < length) {
					registers = controller->registers;
					do {
						if (registers->interruptStatus & 3)
							goto setup;
					} while (registers->endpoints[0].count == 0);
					word = registers->endpoints[0].fifo.word;
					((unsigned char *)request->data)[received] = word;
					if (length - received < 2) {
						received++;
					} else {
						((unsigned char *)request->data)[received + 1] = word >> 8;
						received += 2;
					}
				}
				request->transmitted = received;
				udcdCompleteRequest(queue, 0, &status, bus);
				_sceUdcdDoCommand(controller->registers, bus, 1);
				goto setup;
			}
			_sceUdcdDoCommand(controller->registers, bus, UDC2_COMMAND_STALL);
		}
	}
setup:
	if (events & 8)
		controller->registers->interrupt = controller->config | 8;
	if (events & 1) {
		/* a SETUP packet */
		registers = controller->registers;
		registers->interrupt = controller->config | 1;
		controller->endpoints[0].flags &= 0x7f;
		control->bRequest = registers->setup[0] >> 8;
		control->bmRequestType = registers->setup[0];
		control->wValue = registers->setup[1];
		control->wIndex = registers->setup[2];
		control->wLength = registers->setup[3];
		_sceUdcdDoCommand(controller->registers, bus, 0xb);
		if (udcdDispatchControlRequest(&status, bus) < 0)
			_sceUdcdDoCommand(controller->registers, bus, UDC2_COMMAND_STALL);
		else if ((control->bRequest & 0xf5) == 1 || control->bRequest == 5)
			_sceUdcdDoCommand(controller->registers, bus, 1);
	}
	if (events & 2) {
		registers = controller->registers;
		registers->interrupt = controller->config | 2;
		if (registers->endpoints[0].count == 0 && (control->bRequest & 0xf5) != 1 && control->bRequest != 5)
			_sceUdcdDoCommand(controller->registers, bus, 1);
	}
	if (events & 4) {
		/* the status stage: the request takes effect */
		switch (control->bRequest) {
		case 3:		/* SET_FEATURE */
			if (control->bmRequestType & 0x60)
				break;
			recipient = control->bmRequestType & 0x1f;
			if (recipient == 0) {
				if (control->wValue == 2)
					controller->registers->testMode = control->wIndex >> 8;
			} else if (recipient == 2 && state->connectionState == SCE_UDCD_STATUS_CONNECTION_ESTABLISHED && control->wValue == 0 &&
				   (control->wIndex & 0xf) != 0) {
				udcdStallEndpoint(control->wIndex & 0xf, bus);
				udcdCancelEndpointRequests(control->wIndex & 0xf, &status, bus);
			}
			break;
		case 9:		/* SET_CONFIGURATION */
			if (state->connectionState != SCE_UDCD_STATUS_CONNECTION_NEW && control->wValue == 1) {
				if (controller->board != 0)
					kscePowerBatterySetUsbStatus(3);
				state->connectionState = SCE_UDCD_STATUS_CONNECTION_ESTABLISHED;
				udcdUpdateStateEvent(bus);
				udcdAttachDrivers(&status, bus);
			}
			break;
		case 1:		/* CLEAR_FEATURE */
			if (!(control->bmRequestType & 0x60) && (control->bmRequestType & 0x1f) == 2 &&
			    state->connectionState == SCE_UDCD_STATUS_CONNECTION_ESTABLISHED && control->wValue == 0)
				udcdNotifyControlRequest(&status, bus);
			break;
		}
		controller->registers->interrupt = controller->config | 4;
	}
	if (events & 0x10) {
		config = controller->config;
		controller->config = config | 0x1000;
		controller->registers->interrupt = config | 0x1010;
		state->activationState = SCE_UDCD_STATUS_ACTIVATED;
		udcdUpdateStateEvent(bus);
	}
	if (events & 0x10000) {
		/* bus reset */
		if (controller->board == 1)
			ksceGpioPortSet(0, 0x18);
		controller->registers->events = 0x10000;
		if (state->connectionState == SCE_UDCD_STATUS_CONNECTION_ESTABLISHED) {
			udcdDetachDrivers(&status, bus);
			if (controller->board != 0)
				kscePowerBatterySetUsbStatus(2);
		}
		for (number = 0; number != 8; number++)
			udcdCancelEndpointRequests(number, &status, bus);
		state->usbVersion = -1;
		state->activationState = SCE_UDCD_STATUS_DEACTIVATED;
		state->connectionState = SCE_UDCD_STATUS_CONNECTION_NEW;
		udcdUpdateStateEvent(bus);
		controller->savedConnectionState = 1;
	}
	if (events & 0x20000) {
		/* the speed is known */
		controller->registers->events = 0x20000;
		if (state->cableState == SCE_UDCD_STATUS_CABLE_CONNECTED) {
			if (controller->board == 1)
				ksceGpioPortClear(0, 0x18);
			if (state->usbVersion == (unsigned int)-1) {
				registers = controller->registers;
				state->activationState = SCE_UDCD_STATUS_ACTIVATED;
				if ((registers->address & 0x3000) == 0x2000) {
					device = state->highSpeedDevice;
					state->usbVersion = 2;
				} else {
					device = state->fullSpeedDevice;
					state->usbVersion = 1;
				}
				state->endpoints[0].maxPacketSize = device->bMaxPacketSize0;
				registers->endpointInterruptMask = 0xff;
			}
		}
	}
	if (events & 0x40000) {
		registers = controller->registers;
		registers->events = 0x40000;
		if (!(registers->interruptStatus & 2)) {
			/* resumed */
			state->suspendState = UDCD_BUS_RESUMED;
			registers->powerControl = 4;
			udcdPollBusInitialized(0xfa, bus);
			config = controller->config & ~0x1000;
			controller->config = config;
			controller->registers->interrupt = config | 0x10;
			if (state->connectionState == SCE_UDCD_STATUS_CONNECTION_SUSPENDED) {
				state->connectionState = controller->savedConnectionState;
				udcdUpdateStateEvent(bus);
				if (state->connectionState == SCE_UDCD_STATUS_CONNECTION_ESTABLISHED) {
					for (number = 0; number != 8; number++)
						if (controller->endpoints[number].queue->endpoint != NULL)
							udcdAbortEndpoint(number, bus);
					udcdAttachDrivers(&status, bus);
					if (controller->board != 0)
						kscePowerBatterySetUsbStatus(3);
					state->activationState = SCE_UDCD_STATUS_ACTIVATED;
					udcdUpdateStateEvent(bus);
				} else if (controller->board != 0) {
					kscePowerBatterySetUsbStatus(2);
				}
			}
		} else if (state->connectionState != SCE_UDCD_STATUS_CONNECTION_SUSPENDED) {
			/* suspended */
			controller->savedConnectionState = state->connectionState;
			state->connectionState = SCE_UDCD_STATUS_CONNECTION_SUSPENDED;
			udcdUpdateStateEvent(bus);
			state->suspendState = UDCD_BUS_SUSPENDED;
			if (controller->board != 0) {
				kscePowerBatteryStopUsbCharging();
				kscePowerBatterySetUsbStatus(4);
			}
			controller->registers->powerControl = 2;
			if (controller->savedConnectionState == 2)
				udcdDetachDrivers(&status, bus);
			for (number = 0; number != 8; number++)
				udcdCancelEndpointRequests(number, &status, bus);
		}
	}
	if (events & 0x200000)
		controller->registers->events = 0x200000;
	if (events & 0x100000)
		controller->registers->events = 0x100000;
	if (events & 0x80000)
		controller->registers->events = 0x80000;
unlock:
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return -1;
}

/* Original name unknown. System suspend: stop every running controller. */
void udcdSuspendAll(void)
{
	struct udcdBusState *state;
	SceKernelIntrStatus status;
	int bus;

	for (bus = 1; bus != 3; bus++) {
		state = &udcdState.buses[bus];
		status = ksceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
		state->systemSuspended = 1;
		if (state->controllerDriverState != UDCD_CONTROLLER_DRIVER_ABSENT) {
			state->controllerResumed = 0;
			if (state->controllerState == UDCD_CONTROLLER_STARTED) {
				state->deviceInformation.lastEvent = 0x40000;
				udcdStopController(bus, &status);
				state->resumeControllerState = UDCD_CONTROLLER_STARTED;
			}
		}
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	}
}

/* Original name unknown. Before suspend: report USB charging as suspended. */
int udcdSuspendCharging(void)
{
	struct udcdBusState *state = &udcdState.buses[2];

	if (udcdState.controllers[2].board != 0 && state->controllerState == UDCD_CONTROLLER_STARTED && state->cableState == SCE_UDCD_STATUS_CABLE_CONNECTED) {
		if (state->connectionState != SCE_UDCD_STATUS_CONNECTION_ESTABLISHED)
			SceSysconForDriver_59DC5938(5);
		kscePowerBatterySetUsbStatus(4);
	}
	return 0;
}

/* Original name unknown. System resume: restart the controllers that were running. */
void udcdResumeAll(void)
{
	struct udcdBusState *state;
	SceKernelIntrStatus status;
	int bus, phyIntr;

	for (bus = 0; bus != 3; bus++) {
		state = &udcdState.buses[bus];
		status = ksceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
		if (state->controllerDriverState == UDCD_CONTROLLER_DRIVER_ABSENT) {
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
			continue;
		}
		if (bus == 0)
			phyIntr = 0xbe;
		else if (bus == 1)
			phyIntr = 0xa4;
		else
			phyIntr = 0xaa;
		ksceKernelEnableIntr(phyIntr);
		if (state->resumeControllerState == UDCD_CONTROLLER_STARTED) {
			state->deviceInformation.lastEvent = 4;
			udcdStartController(bus, &status);
			state->resumeControllerState = UDCD_CONTROLLER_STOPPED;
		}
		state->systemSuspended = 0;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	}
}

/* the interrupt names of each bus */
static const struct {
	const char *controller;
	const char *dma;
	const char *phy;
	const char *vbus;
} s_intrNames[3] = {
	{ "SceUdcd0_EP0", "SceUdcd0_EP1-7", "SceUdcd0_phy-ready", "SceUdcd0_vbus" },
	{ "SceUdcd1_EP0", "SceUdcd1_EP1-7", "SceUdcd1_phy-ready", "SceUdcd1_vbus" },
	{ "SceUdcd2_EP0", "SceUdcd2_EP1-7", "SceUdcd2_phy-ready", "SceUdcd2_vbus" },
};

/* Original name unknown. Map the PHY and the controllers, install the interrupts. */
int udcdInitializeControllers(void)
{
	struct udcdBusState *state;
	struct udcdControllerState *controller;
	struct udcdControllerOperations *operations;
	SceKernelAllocMemBlockKernelOpt opt;
	unsigned int hardware;
	void *base;
	volatile ScePervasive2Registers *phy;
	SceUID block;
	int bus, result, phyIntr, intr, dmaIntr, vbusIntr;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.paddr = PERVASIVE2_BASE;
	opt.attr = 2;
	block = ksceKernelAllocMemBlock("SceUdcd", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW, 0x1000, &opt);
	if (block < 0)
		return SCE_UDCD_ERROR_WAIT_TIMEOUT;
	udcdState.phyRegisterBlock = block;
	if (ksceKernelGetMemBlockBase(block, &base) < 0) {
		result = SCE_UDCD_ERROR_WAIT_TIMEOUT;
		goto release;
	}
	phy = base;
	phy->usbVbusInterruptEnable = 0x333;
	udcdState.registers = phy;
	for (bus = 0; ; bus++) {
		state = &udcdState.buses[bus];
		controller = &udcdState.controllers[bus];
		operations = &udcdState.operations[bus];
		state->hardwareInfo = ksceSysconGetHardwareInfo();
		operations->resume = udcdResumeController;
		operations->suspend = udcdSuspendController;
		operations->initializeQueues = udcdInitializeControllerQueues;
		operations->clearQueues = udcdClearControllerQueues;
		operations->start = udcdStartController;
		operations->stop = udcdStopController;
		operations->processRequest = udcdStandardRequest;
		operations->configureEndpoint = udcdConfigureEndpoint;
		operations->resetEndpoint = udcdResetEndpoint;
		operations->abortEndpoint = udcdAbortEndpoint;
		operations->stallEndpoint = udcdStallEndpoint;
		operations->submitRequest = udcdSendRequest;
		operations->completeSend = udcdFinishControllerSend;
		operations->receiveRequest = udcdSubmitDma;
		operations->completeReceive = udcdFinishControllerReceive;
		operations->cancelRequest = udcdCancelControllerRequest;
		operations->isEndpointStalled = udcdIsControllerEndpointStalled;
		if (bus == 0) {
			/* bus 0 is not used */
			state->controllerDriverState = UDCD_CONTROLLER_DRIVER_ABSENT;
			continue;
		}
		if (bus == 2 && ksceSblAimgrIsDolce()) {
			state->controllerDriverState = UDCD_CONTROLLER_DRIVER_ABSENT;
			return 0;
		}
		if (bus == 1) {
			phyIntr = 0xa4;
			dmaIntr = 0x98;
			vbusIntr = 0xa5;
			intr = 0x97;
		} else {
			phyIntr = 0xaa;
			dmaIntr = 0x9c;
			vbusIntr = 0xab;
			intr = 0x9b;
		}
		if (ksceKernelRegisterIntrHandler(phyIntr, s_intrNames[bus].phy, 0, udcdPhyInterrupt, NULL, 0x80, 0xf,
						  NULL) < 0)
			goto failed;
		ksceKernelEnableIntr(phyIntr);
		memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = 2;
		opt.paddr = bus != 1 ? USB_DEVICE2_REG_BASE : USB_DEVICE1_REG_BASE;
		block = ksceKernelAllocMemBlock("SceUdcd", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW, USB_REG_SIZE,
						 &opt);
		if (block < 0 || ksceKernelGetMemBlockBase(block, &base) < 0) {
			result = SCE_UDCD_ERROR_WAIT_TIMEOUT;
			goto cleanup;
		}
		controller->registers = base;
		controller->registerBlock = block;
		if (ksceKernelRegisterIntrHandler(intr, s_intrNames[bus].controller, 0, udcdControllerInterrupt, NULL,
						  0x80, 0xf, NULL) < 0 ||
		    ksceKernelRegisterIntrHandler(dmaIntr, s_intrNames[bus].dma, 0, udcdDmaInterrupt, NULL, 0x80,
						  0xf, NULL) < 0 ||
		    ksceKernelRegisterIntrHandler(vbusIntr, s_intrNames[bus].vbus, 0, udcdVbusInterrupt, NULL, 0x80,
						  0xf, NULL) < 0)
			goto failed;
		if (bus == 2) {
			hardware = state->hardwareInfo;
			if ((hardware & 0xdf0000) == 0x400000 || (hardware & 0xff0000) == 0x300000) {
				controller->board = 1;
			} else if ((hardware & 0xfd0000) == 0x800000 || (hardware & 0xff0000) == 0x900000) {
				controller->board = 2;
			} else {
				controller->board = 0;
				goto done;
			}
			if (ksceKernelRegisterSubIntrHandler(0xfa, 5, "SceUdcdAttachSubIntr",
							     (SceKernelSubIntrHandler)udcdDetectCookie, NULL) < 0)
				goto failed;
		} else {
			controller->board = 0;
		}
done:
		controller->cookieTimer = -1;
		state->suspendState = UDCD_BUS_RESUMED;
		state->controllerResumed = 0;
		if (bus == 2)
			return 0;
	}
failed:
	result = 0x80243100;
cleanup:
	ksceKernelDisableIntr(0xbe);
	ksceKernelReleaseIntrHandler(0xbe);
	ksceKernelReleaseIntrHandler(0xb5);
	ksceKernelReleaseIntrHandler(0xb6);
	ksceKernelReleaseIntrHandler(0xbf);
	ksceKernelFreeMemBlock(udcdState.controllers[0].registerBlock);
	if (udcdState.controllers[0].board != 0)
		ksceKernelReleaseSubIntrHandler(0xfa, 5);
	ksceKernelDisableIntr(0xa4);
	ksceKernelReleaseIntrHandler(0xa4);
	ksceKernelReleaseIntrHandler(0x97);
	ksceKernelReleaseIntrHandler(0x98);
	ksceKernelReleaseIntrHandler(0xa5);
	ksceKernelFreeMemBlock(udcdState.controllers[1].registerBlock);
	if (udcdState.controllers[1].board != 0)
		ksceKernelReleaseSubIntrHandler(0xfa, 5);
	ksceKernelDisableIntr(0xaa);
	ksceKernelReleaseIntrHandler(0xaa);
	ksceKernelReleaseIntrHandler(0x9b);
	ksceKernelReleaseIntrHandler(0x9c);
	ksceKernelReleaseIntrHandler(0xab);
	ksceKernelFreeMemBlock(udcdState.controllers[2].registerBlock);
	if (udcdState.controllers[2].board != 0)
		ksceKernelReleaseSubIntrHandler(0xfa, 5);
release:
	ksceKernelFreeMemBlock(udcdState.phyRegisterBlock);
	return result;
}
