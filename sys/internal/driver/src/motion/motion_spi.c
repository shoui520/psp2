/* The motion sensor's SPI1 mapping and transfer state. */
#include "motion_command.h"
#include <threadmgr/threadmgr.h>
#include <sysmem/sysmem.h>
#include <lowio/spi_regs.h>
#include <sysmem/barrier.h>
#include <lowio/pervasive.h>
#include <string.h>

typedef struct MotionSpiContext {
	volatile SceSpiRegisters *registers;
	SceUID memblock;
	int clockDivider;
	union {
		uint32_t configuration;
		struct {
			uint16_t delay;
			uint8_t receiveThreshold;
			uint8_t mode;
		};
	};
} MotionSpiContext;

static MotionSpiContext motionSpi;

_Static_assert(sizeof(MotionSpiContext) == 16, "motion SPI state size");

int motionSpiInitialize(int clockDivider, uint16_t delay,
	uint8_t receiveThreshold, uint8_t mode)
{
	SceKernelAllocMemBlockKernelOpt option;
	void *base;
	int result;
	volatile SceSpiRegisters *registers;

	motionSpi.registers = NULL;
	motionSpi.memblock = 0;
	motionSpi.clockDivider = 0;
	motionSpi.configuration = 0;
	memset(&option, 0, sizeof(option));
	option.size = sizeof(option);
	option.paddr = SPI_REG_BASE(1);
	option.attr = 2;
	result = SceSysmemForDriver_C94850C9("SceSpi1Reg", 0x20100806,
		0x1000, &option);
	if (result < 0)
		return result;
	motionSpi.memblock = result;
	SceSysmemForDriver_A841EDDA(result, &base);
	motionSpi.delay = delay;
	motionSpi.registers = base;
	motionSpi.clockDivider = clockDivider;
	motionSpi.receiveThreshold = receiveThreshold;
	motionSpi.mode = mode;
	ScePervasiveForDriver_E4B145AE(1);
	ScePervasiveForDriver_DFD96BFC(1);
	registers = motionSpi.registers;
	registers->control = 0x30001;
	registers->intControl = motionSpi.clockDivider;
	registers->dmaControl = 0x20007;
	registers->mode = 0;
	registers->intStatus = 0xf01;
	dsb();
	ScePervasiveForDriver_A85BF98A(1);
	return 0;
}

static uint8_t motionReverseSpiByte(uint8_t value)
{
	value = ((value & 0x55) << 1) | ((value >> 1) & 0x55);
	value = ((value & 0x33) << 2) | ((value >> 2) & 0x33);
	return (value << 4) | (value >> 4);
}

int motionSpiTransfer(const MotionSensorPacket *request,
	MotionSensorPacket *response)
{
	volatile SceSpiRegisters *registers;
	uint32_t size, previous = 0, sent = 0;
	int started = 0;

	response->size = 0;
	ScePervasiveForDriver_DFD96BFC(1);
	size = request->size;
	if (size == 0)
		goto out;
	registers = motionSpi.registers;
	registers->transferControl |= 2;
	if (registers->receiveCount != 0)
		goto out;
	registers->transferControl |= 1;
	if ((registers->transmitCount & 0x1f8) != 0)
		goto out;
	for (;;) {
		int count = size - previous;
		int transferred;
		uint32_t remaining, received, responseSize;
		volatile SceSpiRegisters *receiveRegisters;

		if (count >= motionSpi.receiveThreshold)
			count = motionSpi.receiveThreshold;
		if (count <= 0) {
			if (started == 0)
				goto out;
			break;
		}
		for (transferred = 0; transferred != count; ++transferred) {
			uint8_t value = request->data[previous + transferred];

			if (motionSpi.mode != 0)
				value = motionReverseSpiByte(value);
			registers->transmit = value;
		}
		sent += transferred;
		registers = motionSpi.registers;
		if (started == 0) {
			registers->enable = 1;
			while ((registers->enable & 1) == 0)
				;
		}
		if (motionSpi.delay == 500)
			sceKernelDelayThread(1300);
		while (registers->transmitCount != 0)
			sceKernelDelayThread(motionSpi.delay);
		receiveRegisters = motionSpi.registers;
		remaining = sent;
		received = 0;
		responseSize = response->size;
		while (remaining != 0) {
			uint8_t value;

			while (receiveRegisters->receiveCount == 0) {
				sceKernelDelayThread(100);
				if (remaining == 0)
					goto received;
			}
			responseSize = response->size;
			value = receiveRegisters->receive;
			if (motionSpi.mode != 0)
				value = motionReverseSpiByte(value);
			response->data[responseSize + received] = value;
			++received;
			--remaining;
			responseSize += received;
			if (responseSize > 127)
				break;
		}
received:
		registers = motionSpi.registers;
		response->size = responseSize;
		size = request->size;
		if (sent >= size)
			break;
		registers->transferControl |= 2;
		if (registers->receiveCount != 0)
			break;
		previous = sent;
		registers->transferControl |= 1;
		if ((registers->transmitCount & 0x1f8) != 0)
			break;
		started = 1;
	}
	registers->enable = 0;
	while ((registers->enable & 1) != 0)
		;
out:
	ScePervasiveForDriver_A85BF98A(1);
	return sent == request->size ? 0 : (int)0x803600ff;
}

int motionSpiSuspend(void)
{
	ScePervasiveForDriver_7B16F900(1);
	return 0;
}

int motionSpiResume(void)
{
	volatile SceSpiRegisters *registers;

	ScePervasiveForDriver_E4B145AE(1);
	ScePervasiveForDriver_DFD96BFC(1);
	registers = motionSpi.registers;
	registers->control = 0x30001;
	registers->intControl = motionSpi.clockDivider;
	registers->dmaControl = 0x20007;
	registers->mode = 0;
	registers->intStatus = 0xf01;
	dsb();
	ScePervasiveForDriver_A85BF98A(1);
	return 0;
}

int motionSpiDeactivate(void)
{
	return (int)ScePervasiveForDriver_FDD445C3();
}

int motionSpiActivate(void)
{
	return (int)ScePervasiveForDriver_132E514A();
}
