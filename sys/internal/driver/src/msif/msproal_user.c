/* Sony debug records identify this source as msproal_user.c. */
#include "library.h"
#include "msif_context.h"
#include <sysmem/debug.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/lowio/pervasive.h>
#include <string.h>

static struct MsproalErrorRecord gMsproalErrorRecords[16];

int MsproalUserGetRegisters(struct MsifRegisterWindow **registers)
{
	*registers = &gMsifContext.controller->transfer;
	return 0;
}

void MsproalUserCancelTransfer(struct MsproalHandle *handle)
{
	struct MsifControllerRegisters *controller =
		MSPROAL_CONTROLLER(handle);
	SceUID eventFlag = gMsifContext.eventFlag;
	int result;

	controller->sequencer.control &= 0xfffe;
	handle->interruptFlags |= 0x80000000;
	result = ksceKernelSetEventFlag(eventFlag, 0x80000000);
	SCE_DBG_ASSERT_FUNC(result == 0, 1171, "msproal_user_set_flg");
}

int MsproalUserSetMode(int mode)
{
	return 0;
}

int MsproalUserSetBusClock(int busMode)
{
	unsigned int clock;

	if (busMode == 1)
		clock = 4;
	else if ((unsigned int)(busMode - 2) <= 2)
		clock = 5;
	else
		clock = 6;
	return kscePervasiveMsifSetClock(clock);
}

void MsproalUserCompleteTransfer(struct MsproalHandle *handle)
{
	struct MsifControllerRegisters *controller = MSPROAL_CONTROLLER(handle);
	uint32_t bits;
	int result;

	if (controller->sequencer.control & 4) {
		controller->sequencer.control |= 8;
		handle->interruptFlags |= 0x20000000;
		result = ksceKernelSetEventFlag(gMsifContext.eventFlag, 0x20000000);
	} else {
		bits = handle->registers->interruptStatus;
		handle->registers->interruptStatus = bits;
		handle->interruptFlags |= bits;
		result = ksceKernelSetEventFlag(gMsifContext.eventFlag, bits);
	}
	SCE_DBG_ASSERT_FUNC(result == 0, 1171, "msproal_user_set_flg");
}

int MsproalUserInitialize(void)
{
	return 0;
}

void msproal_user_clear_flg(uint32_t preserveMask)
{
	int result = ksceKernelClearEventFlag(gMsifContext.eventFlag, preserveMask);

	SCE_DBG_ASSERT(result == 0, 1192);
}

int MsproalUserPollEventFlag(uint32_t bits)
{
	uint32_t outBits;
	int result;

	result = ksceKernelPollEventFlag(gMsifContext.eventFlag, bits, 1, &outBits);
	return result < 0 ? -101 : 0;
}

int MsproalUserWaitEventFlag(uint32_t bits, int milliseconds)
{
	uint32_t outBits;
	SceUInt timeout = (SceUInt)milliseconds * 1000;
	int result;

	result = ksceKernelWaitEventFlag(gMsifContext.eventFlag, bits, 5,
		&outBits, &timeout);
	return result < 0 ? -104 : 0;
}

int MsproalUserWaitMilliseconds(int milliseconds)
{
	return sceKernelDelayThread(milliseconds * 1000);
}

struct MsproalErrorRecord **MsproalUserGetErrorLogBuffer(
	struct MsproalErrorRecord **records, uint16_t *capacity)
{
	*records = gMsproalErrorRecords;
	*capacity = 16;
	return records;
}

void MsproalUserInvalidateCache(void *address, unsigned int length)
{
	SceCpuForDriver_02796361(address, length);
}

void msproal_user_virt_to_bus(const void *address, uintptr_t *physicalAddress)
{
	int result = SceSysmemForDriver_8D160E65(address, physicalAddress);

	SCE_DBG_ASSERT(result == 1, 1442);
}

void *MsproalUserFill(void *destination, int value, unsigned int length)
{
	return memset(destination, value, length);
}

void *MsproalUserCopy(void *destination, const void *source, unsigned int length)
{
	return memcpy(destination, source, length);
}
