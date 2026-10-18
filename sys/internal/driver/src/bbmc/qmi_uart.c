/* UART interrupt handling and port lifetime. */
#include <string.h>
#include <sysmem/sysmem.h>
#include <intrmgr/intrmgr.h>
#include <sysmem/cpu.h>
#include <threadmgr/threadmgr.h>
#include "qmi_device.h"
#include <lowio/pervasive.h>

static SceUID g_qmiUartMemBlock;
static volatile SceUartRegisters *g_qmiUartRegisters;
static QmiUartPort g_qmiUartPort;

/* name unknown: service the port's receive and transmit interrupts. */
int qmiUartInterrupt(unsigned int interrupt, QmiUartPort *port)
{
	volatile SceUartRegisters *registers;
	unsigned int status, receiveDone, transmitDone;
	SceKernelIntrStatus intr;
	int result;

	if (port == NULL)
		return -2;
	intr = SceCpuForDriver_D32ACE9E(&port->lock);
	if (!port->enabled) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&port->lock, intr);
		return 0x80332002;
	}
	registers = port->registers;
	if (registers == NULL) {
		result = -2;
		goto unlock;
	}
	status = registers->interruptStatus;
	receiveDone = status & 0x10;
	if (status & 0x10) {
		receiveDone = 1;
		port->receiveFlags |= 1;
	}
	if (status & 8) {
		receiveDone = 1;
		port->receiveFlags |= 2;
	}
	if (status & 4) {
		receiveDone = 1;
		port->receiveFlags |= 4;
	}
	if (status & 2) {
		receiveDone = 1;
		port->receiveFlags |= 8;
	}
	transmitDone = 0;
	if ((status & 0x200) && port->transmitBuffer != NULL) {
		const unsigned char *buffer = port->transmitBuffer;
		unsigned int length = port->transmitLength;
		unsigned int count = port->transmitCount;

		if (count == length) {
			transmitDone = 1;
			if (!port->keepTransmitControlCleared)
				registers->transmitControl |= 1;
		} else {
			unsigned int available =
				32 - ((registers->fifoStatus >> 8) & 0x3f);
			unsigned int start = count;

			port->transmitInterruptCount = 0;
			while (available != 0 && count < length) {
				registers->tx = buffer[count++];
				port->transmitCount = count;
				port->transmitInterruptCount = count - start;
				--available;
			}
		}
	}
	if ((status & 0x100) && port->transmitBuffer != NULL) {
		const unsigned char *buffer = port->transmitBuffer;
		unsigned int length = port->transmitLength;
		unsigned int count = port->transmitCount;

		if (count < length && (registers->status & 0x100)) {
			unsigned int available =
				32 - ((registers->fifoStatus >> 8) & 0x3f);
			unsigned int start = count;

			port->transmitInterruptCount = 0;
			while (available != 0 && count < length) {
				registers->tx = buffer[count++];
				port->transmitCount = count;
				port->transmitInterruptCount = count - start;
				--available;
			}
		}
	}
	if (status & 0x40) {
		unsigned int fifoStatus = registers->fifoStatus;
		unsigned int count;

		port->receiveInterruptCount = 0;
		count = port->receiveCount;
		while (fifoStatus & 0x3f) {
			unsigned int value;

			if (count >= port->receiveLength)
				break;
			value = registers->rx;
			if (value & 0x400)
				port->receiveFlags |= 1;
			if (value & 0x200)
				port->receiveFlags |= 2;
			if (value & 0x100)
				port->receiveFlags |= 4;
			port->receiveBuffer[count] = value;
			fifoStatus = registers->fifoStatus;
			count = port->receiveCount + 1;
			port->receiveCount = count;
			++port->receiveInterruptCount;
		}
		receiveDone = 1;
	}
	if (status & 1) {
		volatile SceUartRegisters *receiveRegisters = port->registers;
		unsigned int fifoStatus;
		unsigned int count;

		port->receiveInterruptCount = 0;
		fifoStatus = receiveRegisters->fifoStatus;
		count = port->receiveCount;
		while (fifoStatus & 0x3f) {
			unsigned int value;

			if (count >= port->receiveLength)
				break;
			value = receiveRegisters->rx;
			if (value & 0x400)
				port->receiveFlags |= 1;
			if (value & 0x200)
				port->receiveFlags |= 2;
			if (value & 0x100)
				port->receiveFlags |= 4;
			port->receiveBuffer[count] = value;
			fifoStatus = receiveRegisters->fifoStatus;
			count = port->receiveCount + 1;
			port->receiveCount = count;
			++port->receiveInterruptCount;
		}
		if (port->receiveLength == count)
			receiveDone = 1;
	}
	if (transmitDone) {
		int (*completed)(unsigned int, unsigned int, unsigned int, void *) =
			port->transmitCompleted;

		registers->irqMask |= 0x700;
		if (completed != NULL) {
			void *context = port->transmitContext;
			unsigned int id = port->transmitId;
			unsigned int count = port->transmitCount;
			unsigned int flags = port->transmitFlags;

			port->transmitBuffer = NULL;
			port->transmitCompleted = NULL;
			port->transmitContext = NULL;
			completed(id, count, flags, context);
		} else if (port->transmitSema > 0) {
			sceKernelSignalSema(port->transmitSema, 1);
		}
	}
	if (receiveDone) {
		int (*completed)(unsigned int, unsigned int, unsigned int, void *) =
			port->receiveCompleted;

		registers->irqMask |= 0x7f;
		if (completed != NULL) {
			void *context = port->receiveContext;
			unsigned int id = port->receiveId;
			unsigned int count = port->receiveCount;
			unsigned int flags = port->receiveFlags;

			port->receiveBuffer = NULL;
			port->receiveCompleted = NULL;
			port->receiveContext = NULL;
			completed(id, count, flags, context);
		} else if (port->receiveSema > 0) {
			sceKernelSignalSema(port->receiveSema, 1);
		}
	}
	result = -1;
	registers->rxAck = status;
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&port->lock, intr);
	return result;
}

int qmiInitializeUart(void)
{
	SceKernelAllocMemBlockKernelOpt options;
	int result;

	memset(&g_qmiUartPort, 0, sizeof(g_qmiUartPort));
	g_qmiUartPort.index = -1;
	memset(&options, 0, sizeof(options));
	options.size = sizeof(options);
	options.paddr = UART_REG_BASE + 5 * UART_REG_SIZE;
	options.attr = 2;
	result = SceSysmemForDriver_C94850C9("SceUart5Reg", 0x20100806,
		UART_REG_SIZE, &options);
	if (result < 0)
		return result;
	g_qmiUartMemBlock = result;
	SceSysmemForDriver_A841EDDA(result, (void **)&g_qmiUartRegisters);
	result = sceKernelRegisterIntrHandler(229, "SceUart5", 0,
		(SceKernelIntrHandler)qmiUartInterrupt, &g_qmiUartPort,
		176, 15, NULL);
	if (result < 0) {
		SceSysmemForDriver_009E1C61(g_qmiUartMemBlock);
		return result;
	}
	g_qmiUartRegisters->enable = 0;
	return 0;
}

int qmiFinalizeUart(void)
{
	sceKernelDisableIntr(229);
	if (g_qmiUartPort.registers != NULL) {
		int index = g_qmiUartPort.index;

		g_qmiUartPort.registers->enable = 0;
		ScePervasiveForDriver_18DD8043(index);
		ScePervasiveForDriver_788B6C61(g_qmiUartPort.index);
		g_qmiUartPort.registers = NULL;
		g_qmiUartPort.enabled = 0;
		g_qmiUartPort.index = 255;
	}
	sceKernelReleaseIntrHandler(229);
	SceSysmemForDriver_009E1C61(g_qmiUartMemBlock);
	return 0;
}

int qmiOpenUart(unsigned int index, QmiUartPort **port,
	const QmiUartConfig *config)
{
	volatile SceUartRegisters *registers;
	unsigned int parity, stopBits, dataBits, flow;
	unsigned int receiveTrigger, transmitTrigger, fifo;
	unsigned int keepTransmitControlCleared, baudRate;
	int result;

	if (index != 5 || port == NULL || config == NULL)
		return 0x80010016;
	registers = g_qmiUartRegisters;
	g_qmiUartPort.receiveSema = -1;
	g_qmiUartPort.transmitSema = -1;
	g_qmiUartPort.nextTransferId = 1;
	g_qmiUartPort.receiveBuffer = NULL;
	g_qmiUartPort.registers = registers;
	g_qmiUartPort.receiveLength = 0;
	g_qmiUartPort.receiveCount = 0;
	g_qmiUartPort.transmitBuffer = NULL;
	g_qmiUartPort.transmitLength = 0;
	g_qmiUartPort.transmitCount = 0;
	g_qmiUartPort.lock = 0;
	keepTransmitControlCleared = config->keepTransmitControlCleared;
	g_qmiUartPort.index = 5;
	g_qmiUartPort.interruptNumber = 229;
	g_qmiUartPort.keepTransmitControlCleared =
		keepTransmitControlCleared;
	ScePervasiveForDriver_34537B12();
	ScePervasiveForDriver_A7CE7DCC(g_qmiUartPort.index);
	ScePervasiveForDriver_EFD084D8(g_qmiUartPort.index);
	baudRate = config->baudRate;
	index = g_qmiUartPort.index;
	registers->control = 1;
	result = ScePervasiveForDriver_8BAB45F8(index, baudRate);
	if (result < 0) {
		ScePervasiveForDriver_18DD8043(g_qmiUartPort.index);
		ScePervasiveForDriver_788B6C61(g_qmiUartPort.index);
		return result;
	}
	parity = config->parity;
	if (parity > 4)
		return 0x80010016;
	stopBits = config->stopBits;
	if (stopBits - 1 > 1)
		return 0x80010016;
	dataBits = config->dataBits - 5;
	if ((unsigned char)dataBits > 3)
		return 0x80010016;
	{
		unsigned int parityModes[5] = { 0, 8, 24, 40, 56 };
		unsigned int dataModes[4] = { 0, 1, 2, 3 };

		registers->mode = parityModes[parity] | dataModes[dataBits] |
			(stopBits == 1 ? 0 : 4);
	}
	flow = config->flowControl & 1;
	if (config->flowControl & 2)
		flow = (unsigned char)flow | 2;
	registers->flowControl = flow;
	receiveTrigger = config->receiveFifoTrigger;
	transmitTrigger = config->transmitFifoTrigger;
	if (receiveTrigger != 1 && receiveTrigger != 4 &&
	    receiveTrigger != 8 && receiveTrigger != 16 &&
	    transmitTrigger != 1 && transmitTrigger != 4 &&
	    transmitTrigger != 8 && transmitTrigger != 16)
		return 0x80010016;
	switch (receiveTrigger) {
	case 4:
		fifo = 0x100;
		break;
	case 8:
		fifo = 0x200;
		break;
	case 16:
		fifo = 0x300;
		break;
	default:
		fifo = 0;
		break;
	}
	if (transmitTrigger != 1) {
		if (transmitTrigger == 4)
			fifo |= 1;
		else {
			if (transmitTrigger == 16)
				fifo |= 3;
			if (transmitTrigger == 8)
				fifo |= 2;
		}
	}
	registers->fifoConfig = fifo;
	registers->fifoControl = 65537;
	index = g_qmiUartPort.interruptNumber;
	registers->transmitControl |= 1;
	registers->irqConfig = config->receiveTimeoutControl;
	registers->irqMask |= 0x77f;
	registers->enable = 1;
	g_qmiUartPort.enabled = 1;
	*port = &g_qmiUartPort;
	sceKernelEnableIntr(index);
	return 0;
}

int qmiCloseUart(QmiUartPort *port)
{
	if (port != NULL && port->enabled) {
		SceKernelIntrStatus intr =
			SceCpuForDriver_D32ACE9E(&port->lock);

		if (port->interruptNumber != 0)
			sceKernelDisableIntr(port->interruptNumber);
		port->interruptNumber = 0;
		port->registers->enable = 0;
		ScePervasiveForDriver_18DD8043(port->index);
		ScePervasiveForDriver_788B6C61(port->index);
		ScePervasiveForDriver_4B99558F();
		port->registers = NULL;
		port->enabled = 0;
		port->index = 255;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&port->lock, intr);
	}
	return 0;
}

int qmiUartTransmit(QmiUartPort *port, const void *data, SceSize length,
	int (*completed)(unsigned int, unsigned int, unsigned int, void *),
	void *context)
{
	const unsigned char *buffer = data;
	volatile SceUartRegisters *registers;
	SceKernelIntrStatus intr;
	unsigned int requestId;
	int result;

	if (port != &g_qmiUartPort || length == 0 || data == NULL)
		return 0x80330002;
	intr = SceCpuForDriver_D32ACE9E(&g_qmiUartPort.lock);
	if (!g_qmiUartPort.enabled) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&g_qmiUartPort.lock, intr);
		return 0x80332002;
	}
	result = 0x80332001;
	registers = g_qmiUartPort.registers;
	if (g_qmiUartPort.transmitBuffer != NULL)
		goto unlockTransmit;
	requestId = g_qmiUartPort.nextTransferId;
	g_qmiUartPort.transmitCount = 0;
	g_qmiUartPort.transmitInterruptCount = 0;
	g_qmiUartPort.transmitFlags = 0;
	g_qmiUartPort.transmitBuffer = buffer;
	g_qmiUartPort.transmitLength = length;
	++g_qmiUartPort.nextTransferId;
	g_qmiUartPort.transmitId = requestId;
	if (requestId == 0x7ffffffe)
		g_qmiUartPort.nextTransferId = 1;
	if (g_qmiUartPort.registers->transmitControl & 1)
		g_qmiUartPort.registers->transmitControl &= ~1u;
	if (registers->status & 0x100) {
		unsigned int available =
			32 - ((registers->fifoStatus >> 8) & 0x3f);
		unsigned int count = 0;

		if (available != 0) {
			do {
				unsigned int value = buffer[count++];

				g_qmiUartPort.transmitCount = count;
				g_qmiUartPort.transmitInterruptCount = count;
				registers->tx = value;
			} while (count != available && count != length);
		}
	}
	if (completed != NULL) {
		result = requestId;
		g_qmiUartPort.transmitCompleted = completed;
		g_qmiUartPort.transmitContext = context;
	} else {
		result = sceKernelCreateSema("SceQmiUartSendSync", 0, 0, 1, NULL);
		if (result < 0) {
			g_qmiUartPort.transmitFlags |= 0x80000000;
			goto unlockTransmit;
		}
		g_qmiUartPort.transmitSema = result;
		result = g_qmiUartPort.transmitId;
	}
	if (registers->irqMask & 0x700)
		registers->irqMask &= ~0x700u;
unlockTransmit:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_qmiUartPort.lock, intr);
	return result;
}

int qmiUartReceive(QmiUartPort *port, void *data, SceSize length,
	int (*completed)(unsigned int, unsigned int, unsigned int, void *),
	void *context)
{
	volatile SceUartRegisters *registers;
	SceKernelIntrStatus intr;
	unsigned int requestId;
	int result;

	if (port != &g_qmiUartPort || length == 0 || data == NULL)
		return 0x80330002;
	intr = SceCpuForDriver_D32ACE9E(&g_qmiUartPort.lock);
	if (!g_qmiUartPort.enabled) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&g_qmiUartPort.lock, intr);
		return 0x80332002;
	}
	result = 0x80332001;
	registers = g_qmiUartPort.registers;
	if (g_qmiUartPort.receiveBuffer != NULL)
		goto unlockReceive;
	requestId = g_qmiUartPort.nextTransferId;
	g_qmiUartPort.receiveCount = 0;
	g_qmiUartPort.receiveInterruptCount = 0;
	g_qmiUartPort.receiveFlags = 0;
	g_qmiUartPort.receiveBuffer = data;
	g_qmiUartPort.receiveLength = length;
	++g_qmiUartPort.nextTransferId;
	g_qmiUartPort.receiveId = requestId;
	if (requestId == 0x7ffffffe)
		g_qmiUartPort.nextTransferId = 1;
	if (g_qmiUartPort.registers->fifoStatus << 26) {
		unsigned char *buffer = data;
		unsigned int count = 0;

		for (;;) {
			unsigned int value = registers->rx;
			unsigned int fifoStatus;

			if (value & 0x400)
				g_qmiUartPort.receiveFlags |= 1;
			if (value & 0x200)
				g_qmiUartPort.receiveFlags |= 2;
			if (value & 0x100)
				g_qmiUartPort.receiveFlags |= 4;
			buffer[count] = value;
			fifoStatus = registers->fifoStatus;
			count = ++g_qmiUartPort.receiveCount;
			++g_qmiUartPort.receiveInterruptCount;
			if (!(fifoStatus << 26) || count >= g_qmiUartPort.receiveLength)
				break;
			buffer = g_qmiUartPort.receiveBuffer;
		}
	}
	if (completed != NULL) {
		g_qmiUartPort.receiveCompleted = completed;
		g_qmiUartPort.receiveContext = context;
	} else {
		result = sceKernelCreateSema("SceQmiUartRecvSync", 0, 0, 1, NULL);
		if (result < 0) {
			g_qmiUartPort.receiveFlags |= 0x80000000;
			goto unlockReceive;
		}
		g_qmiUartPort.receiveSema = result;
	}
	result = g_qmiUartPort.receiveId;
	if (registers->irqMask << 25)
		registers->irqMask &= ~0x7fu;
unlockReceive:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_qmiUartPort.lock, intr);
	return result;
}
