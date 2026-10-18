/*
 * Sony's msif.c: driver work area and controller callbacks.
 */

#include <skeleton.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysevent.h>
#include <sysmem/sysroot_init.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/suspend.h>
#include "msif_context.h"
#include "msif_auth.h"
#include <sysmem/debug.h>
#include <msif/msif.h>
#include <sysmem/cpu.h>
#include <sysmem/sblaimgr.h>
#include <intrmgr/intrmgr.h>
#include <syscon/syscon.h>
#include <threadmgr/fast_mutex_object.h>
#include <string.h>
#include <lowio/pervasive.h>
#include <power/power.h>
#include <sysmem/sysclib.h>

int MsproalSleep(void);
int msproal_init(void);
int sceMsifInitCallback(uint32_t kind, void *argument);
int msproal_read(uint32_t sector, int count, const void *transfer);
int msproal_write(uint32_t sector, int count, const void *transfer);
int MsproalGetMediaInformation(struct MsproalMediaInformation *information);
int MsproalFormat(int mode);
int msproal_start(struct MsproalHandle *handle);
int msproal_check_media(uint8_t *mediaType);
int msproal_mount(int busMode);
int msproal_control_power_supply(uint32_t value, uint32_t preserveMask);

_Static_assert(sizeof(SceMsInfo) == 64, "MSIF media-information result");

/* Cache-line alignment keeps both DMA bounce buffers aligned as well. */
static struct MsproalHandle gMsifHandle;
struct MsifContext gMsifContext __attribute__((aligned(64)));
static SceUID gMsifSysEventHandler;

static const uint32_t gMsifReadDelay[8] = {
	1500, 1600, 1650, 1700, 2000, 2500, 3400, 6800,
};

static const uint32_t gMsifWriteDelay[4] = {
	325, 244, 205, 205,
};

/* Convert PRO-library error codes into the driver's error domain. */
static const int gMsifLibraryErrors[205] = {
	[0 ... 204] = 0x803d0000,
	[-205 + 205] = 0x803d0084,
	[-204 + 205] = 0x803d0083,
	[-203 + 205] = 0x803d0082,
	[-202 + 205] = 0x803d0081,
	[-201 + 205] = 0x803d0080,
	[-111 + 205] = 0x803d0031,
	[-110 + 205] = 0x803d0030,
	[-107 + 205] = 0x803d002f,
	[-106 + 205] = 0x803d002e,
	[-105 + 205] = 0x803d002d,
	[-104 + 205] = 0x803d002c,
	[-103 + 205] = 0x803d002b,
	[-102 + 205] = 0x803d002a,
	[-13 + 205] = 0x803d0007,
	[-12 + 205] = 0x803d0007,
	[-11 + 205] = 0x803d0029,
	[-10 + 205] = 0x803d0028,
	[-9 + 205] = 0x803d0027,
	[-8 + 205] = 0x803d0026,
	[-7 + 205] = 0x803d0025,
	[-6 + 205] = 0x803d0024,
	[-5 + 205] = 0x803d0023,
	[-4 + 205] = 0x803d0020,
	[-3 + 205] = 0x803d0021,
	[-2 + 205] = 0x803d0022,
	[-1 + 205] = 0x803d0001,
};

int MsifSleepThread(SceSize args, void *argp)
{
	uint32_t outBits;

	for (;;) {
		sceKernelWaitEventFlag(gMsifContext.sleepEventFlag, 1, 5,
			&outBits, NULL);
		if (lockFastMutex(
			(SceKernelFastMutex *)&gMsifContext.mutex) < 0)
			continue;
		if (gMsifContext.started && gMsifContext.mediaAccessTickEnabled)
			MsproalSleep();
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
	}
}

int MsifDisableInsertionSubInterrupt(int interrupt, int subInterrupt)
{
	int state = SceCpuForDriver_D32ACE9E(&gMsifContext.lock);

	gMsifContext.insertionInterruptMask &= ~(1U << subInterrupt);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&gMsifContext.lock, state);
	return 0;
}

int MsifEnableInsertionSubInterrupt(int interrupt, int subInterrupt)
{
	int state = SceCpuForDriver_D32ACE9E(&gMsifContext.lock);

	gMsifContext.insertionInterruptMask |= 1U << subInterrupt;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&gMsifContext.lock, state);
	return 0;
}

int MsifInsertionInterrupt(int interrupt, void *argp)
{
	struct MsifContext *context = argp;
	int state = SceCpuForDriver_D32ACE9E(&gMsifContext.lock);
	uint32_t status = ScePervasiveForDriver_F9E7B023();

	if ((status & 1) && (context->insertionInterruptMask & 2))
		sceKernelCallSubIntrHandler(interrupt, 1, NULL);
	if ((status & 2) && (context->insertionInterruptMask & 1))
		sceKernelCallSubIntrHandler(interrupt, 0, NULL);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&gMsifContext.lock, state);
	return -1;
}

int MsifControllerInterrupt(int interrupt, void *argp)
{
	struct MsifContext *context = argp;
	int state = SceCpuForDriver_D32ACE9E(&gMsifContext.lock);

	gMsifContext.interruptTime = sceKernelGetSystemTimeWide();
	if (context->started)
		MsproalUserCompleteTransfer(context->handle);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&gMsifContext.lock, state);
	return -1;
}

void sceMsifIdleCallback(uint32_t event, uint32_t idle, void *argument,
	uint32_t elapsedHigh)
{
	int result;

	(void)event;
	(void)argument;
	(void)elapsedHigh;
	if (!idle)
		return;
	result = sceKernelSetEventFlag(gMsifContext.sleepEventFlag, 1);
#if FW_DEBUG
	if (result != 0)
		SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO(2287),
			"Can't set msif sleep ctrl event flag. ret:0x%x\n", result);
#endif
}

int sceMsifInitCallback(uint32_t kind, void *argument)
{
	int result;

	result = sceKernelStartThread(gMsifContext.sleepThread, 4, NULL);
	if (result < 0) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO(2306),
			"Can't start msif sleep control thread. ret:0x%x\n",
			result);
#endif
	} else {
		/* The loader resolves this address through the import reference list. */
		int (*volatile idleCallback)(uint32_t, uint32_t, uint64_t,
			PowerIdleCallback, void *) = ScePowerForDriver_1BA2FCAE;

		if (idleCallback == NULL) {
#if FW_DEBUG
			SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO(2313),
				"Can't set msif Idle Callback. link not resolved\n");
#endif
		} else {
			result = ScePowerForDriver_1BA2FCAE(4, 0x200, 1000000,
				sceMsifIdleCallback, NULL);
			if (result >= 0)
				gMsifContext.mediaAccessTickEnabled = 1;
#if FW_DEBUG
			else
				SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO(2323),
					"Can't set msif Idle Callback. ret:0x%x\n",
					result);
#endif
		}
	}
	return 0;
}

int MsifSysEventHandler(int phase, int event)
{
	switch (phase) {
	case 0:
		if (event == 0x400c) {
			int enabled = gMsifContext.mediaAccessTickEnabled;

			gMsifContext.mediaAccessTickEnabled = 0;
			gMsifContext.savedMediaAccessTickEnabled = enabled;
			sceKernelDisableIntr(60);
			sceKernelDisableIntr(61);
			ScePervasiveForDriver_A3569FF1();
			ScePervasiveForDriver_2A9778CD();
		}
		break;
	case 1:
		if (event != 0x1000c)
			break;
		gMsifContext.started = 0;
		ScePervasiveForDriver_A3569FF1();
		ScePervasiveForDriver_64ABE589(4);
		ScePervasiveForDriver_CB0F15CD();
		ScePervasiveForDriver_7704C013();
		sceKernelEnableIntr(61);
		sceKernelEnableIntr(60);
		gMsifContext.mediaAccessTickEnabled =
			gMsifContext.savedMediaAccessTickEnabled;
		break;
	}
	return 0;
}

int sceMsifPrepareDmaTable(uint8_t *data, uint32_t length, int write)
{
	struct MsifDmaDescriptor *descriptors = gMsifContext.dmaDescriptors;
	SceKernelPARange *ranges = NULL;
	SceKernelVARange virtualRange;
	SceKernelPARange physicalRange;
	SceKernelPAVector vector;
	uint8_t *alignedData;
	uint32_t alignedLength = length;
	uint32_t rangeCount = 0;
	unsigned int entries;
	unsigned int index = 0;
	unsigned int i;
	int result;

	if ((uintptr_t)data & 3)
		return 0x803d0002;
	gMsifContext.alignedLength = length;
	gMsifContext.headLength = 0;
	gMsifContext.tailLength = 0;
	if ((uintptr_t)data & 63) {
		uint32_t headLength = 64 - ((uintptr_t)data & 63);

		if (length < headLength) {
			headLength = length;
			alignedLength = 0;
		} else {
			alignedLength = length - headLength;
		}
		gMsifContext.headLength = headLength;
		gMsifContext.alignedData = data + headLength;
		gMsifContext.alignedLength = alignedLength;
		if (write) {
#if FW_DEBUG
			__memcpy_chk(gMsifContext.headBuffer, data, headLength,
				sizeof(gMsifContext) -
				offsetof(struct MsifContext, headBuffer));
#else
			memcpy(gMsifContext.headBuffer, data, headLength);
#endif
		}
	} else {
		gMsifContext.alignedData = data;
	}
	if (alignedLength) {
		uint32_t tailLength = (uintptr_t)(data + length) & 63;

		if (tailLength) {
			if (alignedLength < tailLength) {
				tailLength = alignedLength;
				alignedLength = 0;
			} else {
				alignedLength -= tailLength;
			}
			gMsifContext.tailLength = tailLength;
			gMsifContext.alignedLength = alignedLength;
			if (write) {
				const void *tail = (const void *)
					((uintptr_t)(data + length) & ~63U);
#if FW_DEBUG
				__memcpy_chk(gMsifContext.tailBuffer, tail, tailLength,
					sizeof(gMsifContext) -
					offsetof(struct MsifContext, tailBuffer));
#else
				memcpy(gMsifContext.tailBuffer, tail, tailLength);
#endif
			}
		}
	}
	alignedData = gMsifContext.alignedData;
	if (alignedLength) {
		ranges = gMsifContext.physicalRanges;
		virtualRange.addr = (uintptr_t)alignedData;
		virtualRange.size = alignedLength;
		vector = (SceKernelPAVector) {
			.size = sizeof(vector),
			.capacity = 32,
			.pRanges = ranges,
		};
		result = sceKernelVARangeToPAVector(&virtualRange, &vector);
		if (result < 0)
			return result;
		rangeCount = vector.total;
		if (rangeCount > 32)
			ranges = NULL;
	}
	entries = (gMsifContext.headLength != 0) +
		(gMsifContext.tailLength != 0);
	if (alignedLength) {
		if (alignedLength <= 0x40000) {
			entries += rangeCount;
		} else if (ranges) {
			for (i = 0; i != rangeCount; ++i) {
				uint32_t size = ranges[i].size;

				if (size > 0x40000) {
					entries += size >> 18;
					if (!(size & 0x3ffff))
						continue;
				}
				++entries;
			}
		} else {
			uint8_t *cursor = alignedData;
			uint8_t *end = alignedData + alignedLength;
			uint32_t remaining = alignedLength;

			while (cursor < end) {
				virtualRange.addr = (uintptr_t)cursor;
				virtualRange.size = remaining;
				vector = (SceKernelPAVector) {
					.size = sizeof(vector),
					.capacity = 1,
					.pRanges = &physicalRange,
				};
				result = sceKernelVARangeToPAVector(&virtualRange, &vector);
				if (result < 0)
					return result;
				SCE_DBG_ASSERT_FUNC(vector.nDataInVector == 1, 1149,
					"sceMsifGetDmaTableEntryNum");
				if (vector.nDataInVector != 1) {
					SceDebugForDriver_02B04343(0, SCE_DBGINFO_FUNC(1152,
						"sceMsifGetDmaTableEntryNum"),
						"0x%x %d 0x%x 0x%x\n", 0x803d0008,
						1152, cursor, remaining);
					return 0x803d0008;
				}
				if (physicalRange.size > 0x40000)
					entries += physicalRange.size >> 18;
				if (physicalRange.size <= 0x40000 ||
					(physicalRange.size & 0x3ffff))
					++entries;
				cursor += physicalRange.size;
				remaining -= physicalRange.size;
			}
		}
		if (entries > 8192) {
			SceDebugForDriver_02B04343(0, SCE_DBGINFO(1386),
				"0x%x %d 0x%x 0x%x %p 0x%x\n", 0x803d0005,
				1386, entries, rangeCount, data, length);
			return 0x803d0005;
		}
	}
	if (gMsifContext.headLength) {
		if (write)
			SceCpuForDriver_103872A5(gMsifContext.headBuffer, 64);
		else
			SceCpuForDriver_364E68A4(gMsifContext.headBuffer, 64);
		SCE_DBG_ASSERT((gMsifContext.headLength & 3) == 0, 1405);
		if (gMsifContext.headLength & 3) {
			SceDebugForDriver_02B04343(0, SCE_DBGINFO(1410),
				"0x%x %d 0x%x %p\n", 0x803d0001,
				1410, gMsifContext.headLength, data);
			return 0x803d0001;
		}
		descriptors->nextDescriptorAddress = 0;
		descriptors->transferControl =
			(uint16_t)(gMsifContext.headLength >> 2) | 0xc0000000;
		descriptors->dataAddress = gMsifContext.headPhysicalAddress;
		index = 1;
	}
	if (alignedLength) {
		if (write)
			SceCpuForDriver_103872A5(alignedData, alignedLength);
		else
			SceCpuForDriver_364E68A4(alignedData, alignedLength);
		if (ranges) {
			for (i = 0; i != rangeCount; ++i) {
				uintptr_t address = ranges[i].addr;
				uint32_t remaining = ranges[i].size;
				uintptr_t next = gMsifContext.dmaDescriptorsPhysicalAddress +
					index * sizeof(*descriptors);

				__builtin_prefetch(&ranges[i + 4].size);
				while (remaining) {
					uint32_t size = remaining < 0x40000 ? remaining : 0x40000;
					uint32_t mode;
					struct MsifDmaDescriptor *descriptor = &descriptors[index];

					if (index) {
						__builtin_prefetch(&descriptors[index + 6].nextDescriptorAddress);
						descriptors[index - 1].nextDescriptorAddress = next;
					}
					if (!(size << 26))
						mode = 0x70000;
					else if (!(size << 27))
						mode = 0x50000;
					else if (!(size & 15))
						mode = 0x30000;
					else
						mode = 0;
					remaining -= size;
					descriptor->dataAddress = address;
					++index;
					next += sizeof(*descriptors);
					address += size;
					descriptor->nextDescriptorAddress = 0;
					descriptor->transferControl =
						(uint16_t)(size >> 2) | 0xc0000000 | mode;
				}
			}
		} else {
			uint8_t *end = alignedData + alignedLength;
			uint32_t remaining = alignedLength;

			while (alignedData < end) {
				uintptr_t address;
				uint32_t sizeLeft;
				uintptr_t next;

				virtualRange.addr = (uintptr_t)alignedData;
				virtualRange.size = remaining;
				vector = (SceKernelPAVector) {
					.size = sizeof(vector),
					.capacity = 1,
					.pRanges = &physicalRange,
				};
				result = sceKernelVARangeToPAVector(&virtualRange, &vector);
				if (result < 0)
					goto aligned_error;
				SCE_DBG_ASSERT_FUNC(vector.nDataInVector == 1, 1302,
					"sceMsifWriteLinkListForAlignedData");
				if (vector.nDataInVector != 1) {
					result = 0x803d0008;
					SceDebugForDriver_02B04343(0, SCE_DBGINFO_FUNC(1305,
						"sceMsifWriteLinkListForAlignedData"),
						"0x%x %d 0x%x 0x%x\n", result,
						1305, alignedData, remaining);
					goto aligned_error;
				}
				address = physicalRange.addr;
				sizeLeft = physicalRange.size;
				next = gMsifContext.dmaDescriptorsPhysicalAddress +
					index * sizeof(*descriptors);
				while (sizeLeft) {
					uint32_t size = sizeLeft < 0x40000 ? sizeLeft : 0x40000;
					uint32_t mode;
					struct MsifDmaDescriptor *descriptor = &descriptors[index];

					if (index) {
						__builtin_prefetch(&descriptors[index + 6].nextDescriptorAddress);
						descriptors[index - 1].nextDescriptorAddress = next;
					}
					if (!(size << 26))
						mode = 0x70000;
					else if (!(size << 27))
						mode = 0x50000;
					else if (!(size & 15))
						mode = 0x30000;
					else
						mode = 0;
					sizeLeft -= size;
					descriptor->dataAddress = address;
					++index;
					next += sizeof(*descriptors);
					address += size;
					descriptor->nextDescriptorAddress = 0;
					descriptor->transferControl =
						(uint16_t)(size >> 2) | 0xc0000000 | mode;
				}
				alignedData += physicalRange.size;
				remaining -= physicalRange.size;
			}
		}
	}
	if (gMsifContext.tailLength) {
		struct MsifDmaDescriptor *descriptor = &descriptors[index];

		if (index)
			descriptors[index - 1].nextDescriptorAddress =
				gMsifContext.dmaDescriptorsPhysicalAddress +
				index * sizeof(*descriptors);
		if (write)
			SceCpuForDriver_103872A5(gMsifContext.tailBuffer, 64);
		else
			SceCpuForDriver_364E68A4(gMsifContext.tailBuffer, 64);
		SCE_DBG_ASSERT((gMsifContext.tailLength & 3) == 0, 1472);
		if (gMsifContext.tailLength & 3) {
			SceDebugForDriver_02B04343(0, SCE_DBGINFO(1476),
				"0x%x %d 0x%x %p\n", 0x803d0001,
				1476, gMsifContext.tailLength, data);
			return 0x803d0001;
		}
		descriptor->dataAddress = gMsifContext.tailPhysicalAddress;
		++index;
		descriptor->nextDescriptorAddress = 0;
		descriptor->transferControl =
			(uint16_t)(gMsifContext.tailLength >> 2) | 0xc0000000;
	}
	SCE_DBG_ASSERT(index == entries, 1495);
	if (index != entries) {
		SceDebugForDriver_02B04343(0, SCE_DBGINFO(1499),
			"0x%x %d %d %d %p 0x%x\n", 0x803d0008,
			1499, index, entries, data, length);
		return 0x803d0008;
	}
	if (index)
		descriptors[index - 1].transferControl &= ~0x40000000U;
	SceCpuForDriver_103872A5(descriptors,
		((entries * sizeof(*descriptors)) & ~63U) + 64);
	return index;

aligned_error:
	SceDebugForDriver_02B04343(0, SCE_DBGINFO(1449), "0x%x %d\n", result, 1449);
	return result;
}

int sceMsifReadSector(uint32_t sector, uint8_t *data, unsigned int count)
{
	uint64_t startTime;
	uint64_t endTime;
	int retries = 3;
	int result = sceMsifPrepareDmaTable(data, count << 9, 0);

	if (result < 0)
		goto unlock;
	startTime = sceKernelGetSystemTimeWide();
	for (;;) {
		result = msproal_read(sector, count, gMsifContext.dmaDescriptors);
		if (result == -12 || result == 0)
			break;
#if FW_DEBUG
		int error = 0x803d0000;

		if ((uint32_t)(result + 205) <= 204)
			error = gMsifLibraryErrors[result + 205];
		SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO(1782),
			"Msif Retry. 0x%x %d 0x%x %p 0x%x\n",
			error, 1782, sector, data, count);
#endif
		if (!--retries)
			break;
	}
	if (result) {
		if ((uint32_t)(result + 205) <= 204)
			result = gMsifLibraryErrors[result + 205];
		else
			result = 0x803d0000;
		if (result != (int)0x803d0001)
			SceDebugForDriver_02B04343(0, SCE_DBGINFO(1789),
				"0x%x %d 0x%x %p 0x%x\n", result,
				1789, sector, data, count);
		goto unlock;
	}
	endTime = sceKernelGetSystemTimeWide();
	if (gMsifContext.headLength) {
		SceCpuForDriver_02796361(gMsifContext.headBuffer, 64);
		memcpy(data, gMsifContext.headBuffer, gMsifContext.headLength);
	}
	if (gMsifContext.tailLength) {
		SceCpuForDriver_02796361(gMsifContext.tailBuffer, 64);
		memcpy(gMsifContext.alignedData + gMsifContext.alignedLength,
			gMsifContext.tailBuffer, gMsifContext.tailLength);
	}
	if (gMsifContext.alignedLength)
		SceCpuForDriver_02796361(gMsifContext.alignedData,
			gMsifContext.alignedLength);
	if (gMsifContext.slowCardMode == 1) {
		uint64_t period = startTime / 3000000;
		uint32_t delay;

		if (period != gMsifContext.slowDelayPeriod &&
			(startTime % 3000000) / 150000 ==
			gMsifContext.slowDelaySelector) {
			delay = startTime % 40 ? 350000 : 400000;
#if FW_DEBUG
			SceDebugForDriver_D9703808(1, 0,
				SCE_DBGINFO_FUNC(913, "sceMsifSlowModeDelay"),
				"MemoryCard special delay in Slow Card Mode\n");
#endif
			gMsifContext.slowDelayPeriod = period;
			gMsifContext.slowDelaySelector = startTime % 20;
		} else {
			uint64_t expected;
			uint64_t elapsed;

			if (count > 128) {
				expected = (int)(47 * count);
			} else {
				int index;

				if (count <= 1)
					index = 0;
				else if (count <= 2)
					index = 1;
				else if (count <= 4)
					index = 2;
				else if (count <= 8)
					index = 3;
				else if (count <= 16)
					index = 4;
				else if (count <= 32)
					index = 5;
				else if (count <= 64)
					index = 6;
				else
					index = 7;
				expected = gMsifReadDelay[index];
			}
			elapsed = endTime - startTime;
			if (elapsed >= expected || !(expected - elapsed))
				goto tick;
			delay = expected - elapsed;
		}
		++gMsifContext.readDelaySequence;
		sceKernelDelayThread(delay);
	}
tick:
	if (gMsifContext.mediaAccessTickEnabled) {
		int tickResult = ScePowerForDriver_EFD3C963(16);

#if FW_DEBUG
		if (tickResult < 0)
			SceDebugForDriver_D9703808(1, 0,
				SCE_DBGINFO_FUNC(2339, "sceMsifMediaAccessTick"),
				"msif power tick error. ret:0x%x\n", tickResult);
#endif
	}
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
	return 0;

unlock:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
	return result;
}

static const SceKernelIntrCallbacks gMsifInsertionHandlers = {
	.size = sizeof(SceKernelIntrCallbacks),
	.enable = MsifEnableInsertionSubInterrupt,
	.disable = MsifDisableInsertionSubInterrupt,
};

static const SceKernelIntrRegisterOpt gMsifInsertionOptions = {
	.size = sizeof(SceKernelIntrRegisterOpt),
	.num = 2,
	.handlers = (void *)&gMsifInsertionHandlers,
};

/* Sony's sceMsifInit, exported as the module entry point. */
int module_start(SceSize args, const void *argp)
{
	SceKernelAllocMemBlockKernelOpt option;
	int result, libraryResult;

	memset(&gMsifContext, 0, sizeof(gMsifContext));
	gMsifContext.lock = 0;
	gMsifContext.handle = &gMsifHandle;
	gMsifContext.readDelaySequence = sceKernelGetSystemTimeWide();
	gMsifContext.writeDelaySequence = gMsifContext.readDelaySequence;
	memset(&option, 0, sizeof(option));
	option.size = sizeof(option);
	option.paddr = MSIF_REG_BASE;
	option.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	result = sceKernelAllocMemBlockForKernel("SceMsif", 0x20100806, MSIF_REG_SIZE, &option);
	if (result < 0)
		return 2;
	gMsifContext.controllerMemBlock = result;
	sceKernelGetMemBlockBase(result, (void **)&gMsifContext.controller);
	gMsifContext.eventFlag = sceKernelCreateEventFlag("SceMsif", 0, 0, NULL);
	if (gMsifContext.eventFlag < 0)
		goto free_controller;
	if (sceKernelInitializeFastMutex((SceKernelFastMutex *)&gMsifContext.mutex,
		"SceMsif", 2, NULL) < 0)
		goto delete_event;
	memset(&option, 0, sizeof(option));
	option.size = sizeof(option);
	option.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT;
	result = sceKernelAllocMemBlockForKernel("SceMsif", 0x1020d006, 98304, &option);
	if (result < 0)
		goto finalize_mutex;
	gMsifContext.dmaMemBlock = result;
	sceKernelGetMemBlockBase(result, (void **)&gMsifContext.dmaDescriptors);
	memset(gMsifContext.dmaDescriptors, 0, 98304);
	result = sceKernelVAtoPA(gMsifContext.dmaDescriptors,
		&gMsifContext.dmaDescriptorsPhysicalAddress);
	if (result < 0)
		goto free_dma;
	result = sceKernelVAtoPA(gMsifContext.headBuffer,
		&gMsifContext.headPhysicalAddress);
	if (result < 0)
		goto free_dma;
	result = sceKernelVAtoPA(gMsifContext.tailBuffer,
		&gMsifContext.tailPhysicalAddress);
	if (result < 0)
		goto free_dma;
	result = sceKernelRegisterIntrHandler(61, "SceMsifSmshc", 0,
		MsifControllerInterrupt, &gMsifContext, 128, 15, NULL);
	if (result < 0)
		goto free_dma;
	result = sceKernelRegisterIntrHandler(60, "SceMsifIns", 0,
		MsifInsertionInterrupt, &gMsifContext, 128, 15,
		&gMsifInsertionOptions);
	if (result < 0)
		goto release_controller_interrupt;
	result = sceKernelRegisterSysEventHandler("SceMsif",
		(SceKernelSysEventHandler)MsifSysEventHandler, NULL);
	if (result < 0)
		goto release_insertion_interrupt;
	gMsifSysEventHandler = result;
	libraryResult = msproal_init();
	if (libraryResult) {
		result = (uint32_t)(libraryResult + 205) <= 204 ?
			gMsifLibraryErrors[libraryResult + 205] : (int)0x803d0000;
		goto unregister_sys_event;
	}
	gMsifContext.started = 0;
	result = sceKernelCreateEventFlag("SceMsifSleepCtrl", 0, 0, NULL);
	gMsifContext.sleepEventFlag = result;
	if (result < 0)
		goto unregister_sys_event;
	result = sceKernelCreateThread("SceMsifSleepCtrl", MsifSleepThread,
		96, 8192, 0x10000000, 0x80000, NULL);
	if (result < 0)
		goto delete_sleep_event;
	gMsifContext.sleepThread = result;
	gMsifContext.mediaAccessTickEnabled = 0;
	gMsifContext.savedMediaAccessTickEnabled = 0;
	result = sceKernelRegisterInitCallback(sceMsifInitCallback, 8, NULL, NULL);
	if (result < 0)
		goto delete_sleep_thread;
	ScePervasiveForDriver_A3569FF1();
	ScePervasiveForDriver_64ABE589(4);
	ScePervasiveForDriver_CB0F15CD();
	ScePervasiveForDriver_7704C013();
	SCE_DBG_ASSERT_FUNC(1, 297, "sceMsifInit");
	sceKernelEnableIntr(61);
	sceKernelEnableIntr(60);
	return 0;

delete_sleep_thread:
	sceKernelDeleteThread(gMsifContext.sleepThread);
delete_sleep_event:
	sceKernelDeleteEventFlag(gMsifContext.sleepEventFlag);
unregister_sys_event:
	sceKernelUnregisterSysEventHandler(gMsifSysEventHandler);
release_insertion_interrupt:
	sceKernelDisableIntr(60);
	sceKernelReleaseIntrHandler(60);
release_controller_interrupt:
	sceKernelDisableIntr(61);
	sceKernelReleaseIntrHandler(61);
free_dma:
	SceSysmemForDriver_009E1C61(gMsifContext.dmaMemBlock);
finalize_mutex:
	sceKernelFinalizeFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
	sceKernelDeleteEventFlag(gMsifContext.eventFlag);
	SceSysmemForDriver_009E1C61(gMsifContext.controllerMemBlock);
	return result < 0 ? 2 : 0;
delete_event:
	sceKernelDeleteEventFlag(gMsifContext.eventFlag);
free_controller:
	SceSysmemForDriver_009E1C61(gMsifContext.controllerMemBlock);
	return 2;
}

int sceMsifSwReset(void)
{
	uint8_t mediaType;
	uint32_t revision;
	int result = lockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);

	if (result < 0)
		return result;
	result = msproal_start(gMsifContext.handle);
	if (result) {
		if ((uint32_t)(result + 205) <= 204)
			result = gMsifLibraryErrors[result + 205];
		else
			result = 0x803d0000;
		if (result == (int)0x803d0021)
			goto unlock;
		if (result != (int)0x803d0020) {
			SceDebugForDriver_02B04343(0, SCE_DBGINFO(376),
				"0x%x %d\n", result, 376);
			goto unlock;
		}
	}
	gMsifContext.started = 1;
	revision = ScePervasiveForDriver_714EEFB7() & 0x1ff00;
	if (!revision) {
		if (SceSblAIMgrForDriver_C6E83F34())
			result = msproal_control_power_supply(0, 0xffff80ff);
		else
			result = msproal_control_power_supply(0x300, 0xffff80ff);
	} else if (revision == 0x100) {
		uint32_t value;

		if ((SceSysconForDriver_CBD6D8BC() & 0xff0000U) < 0x800000)
			value = 0x35000c;
		else
			value = 0x7000a;
		result = msproal_control_power_supply(value, 0xff00fff0);
	} else {
		goto check_media;
	}
	if (result) {
		int error;

		if ((uint32_t)(result + 205) <= 204)
			error = gMsifLibraryErrors[result + 205];
		else
			error = 0x803d0000;
		SceDebugForDriver_02B04343(0, SCE_DBGINFO(419),
			"0x%x %d\n", error, 419);
	}
check_media:
	result = msproal_check_media(&mediaType);
	if (!result) {
		int busMode;
		uint32_t type = mediaType & ~0xfU;

		if (type == 0x10)
			busMode = 2;
		else if (type == 0x20)
			busMode = 5;
		else {
			result = -1;
			goto unlock;
		}
		result = 0;
		msproal_mount(busMode);
		goto unlock;
	}
	if ((uint32_t)(result + 205) <= 204)
		result = gMsifLibraryErrors[result + 205];
	else
		result = 0x803d0000;
	if (result != (int)0x803d0020)
		SceDebugForDriver_02B04343(0, SCE_DBGINFO(427),
			"0x%x %d\n", result, 427);
unlock:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
	return result;
}

int SceMsifForDriver_D0307849(SceMsInfo *info)
{
	struct MsproalMediaInformation information;
	struct MsproalHandle *handle;
	uint32_t totalSectors;
	int result;

	if (!info)
		return 0x803d0001;
	result = lockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
	if (result < 0)
		return result;
	result = MsproalGetMediaInformation(&information);
	if (result != 0) {
		if ((uint32_t)(result + 205) <= 204)
			result = gMsifLibraryErrors[result + 205];
		else
			result = 0x803d0000;
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
		return result;
	}
	switch (information.mediaType & 0x1f0) {
	case 0x10:
		info->mediaType = 1;
		break;
	case 0x20:
		info->mediaType = 2;
		break;
	case 0x40:
		info->mediaType = 3;
		break;
	case 0x80:
		info->mediaType = 4;
		break;
	case 0x100:
		info->mediaType = 5;
		break;
	default:
		info->mediaType = 0;
		break;
	}
	totalSectors = information.totalSectors;
	handle = gMsifContext.handle;
	info->handle = handle;
	info->nbytes = (uint64_t)totalSectors << 9;
	info->nbytes2 = (uint64_t)information.userSectors << 9;
	info->blockSize = information.sectorsPerBlock << 9;
	info->writeProtected = information.writeProtected;
	info->sector_size = 512;
	info->serialNumber = handle->systemInformation.serialNumber[0];
	info->serialNumber = handle->systemInformation.serialNumber[1] +
		(info->serialNumber << 8);
	info->serialNumber = handle->systemInformation.serialNumber[2] +
		(info->serialNumber << 8);
	info->serialNumber = handle->systemInformation.serialNumber[3] +
		(info->serialNumber << 8);
	info->assemblyModel = handle->systemInformation.assemblyModel[0];
	info->assemblyModel = handle->systemInformation.assemblyModel[1] +
		(info->assemblyModel << 8);
	info->assemblyModel = handle->systemInformation.assemblyModel[2] +
		(info->assemblyModel << 8);
	info->assemblyMaker = handle->systemInformation.assemblyMaker;
	__builtin_memcpy(info->assemblyDate, handle->systemInformation.assemblyDate, 8);
	result = lockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
	if (result >= 0)
		result = sceMsifReadSector(totalSectors - 1,
			gMsifContext.dummyReadBuffer, 1);
	if (result == (int)0x803d0001) {
		uint32_t excludedBytes = 2 * info->blockSize;

		info->nbytes -= excludedBytes;
		info->nbytes2 -= excludedBytes;
	}
	handle = gMsifContext.handle;
	gMsifContext.dummyReadWorkaround =
		handle->systemInformation.assemblyDate[1] == 7 &&
		handle->systemInformation.assemblyDate[2] == 219 &&
		(uint16_t)(handle->systemInformation.assemblyDate[4] +
			(handle->systemInformation.assemblyDate[3] << 8)) <= 0x616;
	if (gMsifContext.mediaAccessTickEnabled) {
		result = ScePowerForDriver_EFD3C963(16);
#if FW_DEBUG
		if (result < 0)
			SceDebugForDriver_D9703808(1, 0,
				SCE_DBGINFO_FUNC(2339, "sceMsifMediaAccessTick"),
				"msif power tick error. ret:0x%x\n", result);
#endif
	}
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
	return 0;
}

int SceMsifForDriver_58654AA3(uint32_t sector, uint8_t *data, unsigned int count)
{
	int result;

	if (!count)
		return 0x803d0001;
	if (!data)
		return 0x803d0001;
	if ((uintptr_t)data << 30)
		return 0x803d0002;
	result = lockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
	if (result >= 0)
		return sceMsifReadSector(sector, data, count);
	return result;
}

int sceMsifWriteSector(uint32_t sector, uint8_t *data, unsigned int count)
{
	struct MsproalMediaInformation information;
	uint64_t startTime;
	uint64_t endTime;
	int retries = 3;
	int result;

	if (!count)
		return 0x803d0001;
	if (!data)
		return 0x803d0001;
	if ((uintptr_t)data << 30)
		return 0x803d0002;
	result = lockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
	if (result < 0)
		return result;
	result = MsproalGetMediaInformation(&information);
	if (result < 0) {
		if ((uint32_t)(result + 205) <= 204)
			result = gMsifLibraryErrors[result + 205];
		else
			result = 0x803d0000;
		goto unlock;
	}
	if (information.writeProtected) {
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
		return 0x803d0003;
	}
	result = sceMsifPrepareDmaTable(data, count << 9, 1);
	if (result < 0)
		goto unlock;
	startTime = sceKernelGetSystemTimeWide();
	for (;;) {
		result = msproal_write(sector, count, gMsifContext.dmaDescriptors);
		if (result == -12 || result == 0)
			break;
#if FW_DEBUG
		int error = 0x803d0000;

		if ((uint32_t)(result + 205) <= 204)
			error = gMsifLibraryErrors[result + 205];
		SceDebugForDriver_D9703808(1, 0,
			SCE_DBGINFO_FUNC(1925, "sceMsifWriteSector"),
			"Msif Retry. 0x%x %d 0x%x %p 0x%x\n",
			error, 1925, sector, data, count);
#endif
		if (!--retries)
			break;
	}
	if (result) {
		if ((uint32_t)(result + 205) <= 204)
			result = gMsifLibraryErrors[result + 205];
		else
			result = 0x803d0000;
		SceDebugForDriver_02B04343(0,
			SCE_DBGINFO_FUNC(1931, "sceMsifWriteSector"),
			"0x%x %d 0x%x %p 0x%x\n", result,
			1931, sector, data, count);
		goto unlock;
	}
	if (gMsifContext.dummyReadWorkaround) {
		uint64_t commandStartTime = gMsifContext.commandStartTime;
		int readResult = sceMsifPrepareDmaTable(
			gMsifContext.dummyReadBuffer, 512, 0);

		if (readResult >= 0) {
			readResult = msproal_read(0xc0, 1, gMsifContext.dmaDescriptors);
			if (readResult) {
				if ((uint32_t)(readResult + 205) <= 204)
					readResult = gMsifLibraryErrors[readResult + 205];
				else
					readResult = 0x803d0000;
				if (readResult != (int)0x803d0001)
					SceDebugForDriver_02B04343(0,
						SCE_DBGINFO_FUNC(1848, "sceMsifBug29526DummyRead"),
						"0x%x %d\n", readResult, 1848);
			}
		}
		gMsifContext.commandStartTime = commandStartTime;
	}
	endTime = sceKernelGetSystemTimeWide();
	if (gMsifContext.slowCardMode == 1) {
		uint64_t period = startTime / 3000000;
		uint32_t delay;

		if (period != gMsifContext.slowDelayPeriod &&
			(startTime % 3000000) / 150000 ==
			gMsifContext.slowDelaySelector) {
			delay = startTime % 40 ? 350000 : 400000;
#if FW_DEBUG
			SceDebugForDriver_D9703808(1, 0,
				SCE_DBGINFO_FUNC(913, "sceMsifSlowModeDelay"),
				"MemoryCard special delay in Slow Card Mode\n");
#endif
			gMsifContext.slowDelayPeriod = period;
			gMsifContext.slowDelaySelector = startTime % 20;
		} else {
			uint32_t coefficient = 205;
			uint32_t expected;
			uint64_t elapsed;

			if (count > 63) {
				int index;

				if (count == 64)
					index = 0;
				else if (count <= 128)
					index = 1;
				else if (count <= 256)
					index = 2;
				else if (count <= 512)
					index = 3;
				else
					goto expected_delay;
				coefficient = gMsifWriteDelay[index];
			}
expected_delay:
			expected = coefficient * count;
			elapsed = endTime - startTime;
			if (elapsed >= expected || !(expected - elapsed))
				goto tick;
			delay = expected - elapsed;
		}
		++gMsifContext.writeDelaySequence;
		sceKernelDelayThread(delay);
	}
tick:
	if (gMsifContext.mediaAccessTickEnabled) {
		int tickResult = ScePowerForDriver_EFD3C963(16);

#if FW_DEBUG
		if (tickResult < 0)
			SceDebugForDriver_D9703808(1, 0,
				SCE_DBGINFO_FUNC(2339, "sceMsifMediaAccessTick"),
				"msif power tick error. ret:0x%x\n", tickResult);
#endif
	}
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
	return 0;

unlock:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
	return result;
}

int sceMsifZerofillSector(uint32_t sector, unsigned int count)
{
	struct MsproalMediaInformation information;
	uint32_t currentSector = sector;
	unsigned int remaining = count;
	int result;

	if (!count)
		return 0x803d0001;
	result = lockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
	if (result < 0)
		return result;
	result = MsproalGetMediaInformation(&information);
	if (result < 0) {
		if ((uint32_t)(result + 205) <= 204)
			result = gMsifLibraryErrors[result + 205];
		else
			result = 0x803d0000;
		goto unlock;
	}
	if (information.writeProtected) {
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
		return 0x803d0003;
	}
	do {
		struct MsifDmaDescriptor *descriptors = gMsifContext.dmaDescriptors;
		struct MsifDmaDescriptor *descriptor = descriptors;
		uintptr_t physicalAddress = gMsifContext.dmaDescriptorsPhysicalAddress;
		unsigned int sectors = remaining < 256 ? remaining : 256;
		unsigned int length = sectors << 9;
		unsigned int bytesLeft = length;
		unsigned int entries = 0;

		gMsifContext.alignedData = NULL;
		gMsifContext.alignedLength = length;
		for (;;) {
			unsigned int bytes = bytesLeft < 4096 ? bytesLeft : 4096;

			bytesLeft -= bytes;
			descriptor->dataAddress = 0x1d000000;
			++entries;
			descriptor->nextDescriptorAddress = 0;
			physicalAddress += sizeof(*descriptor);
			descriptor->transferControl = (bytes >> 2) | 0xc0070000;
			if (!bytesLeft)
				break;
			descriptor->nextDescriptorAddress = physicalAddress;
			++descriptor;
		}
		SCE_DBG_ASSERT_FUNC(entries <= 8192, 1570, "sceMsifDevNullPrepareDmaTable");
		if (entries > 8192) {
			SceDebugForDriver_02B04343(0,
				SCE_DBGINFO_FUNC(1574, "sceMsifDevNullPrepareDmaTable"),
				"0x%x %d %d 0x%x\n", 0x803d0005, 1574, entries, length);
			sceKernelUnlockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
			return 0x803d0005;
		}
		descriptors[entries - 1].transferControl &= ~0x40000000U;
		SceCpuForDriver_103872A5(descriptors,
			((entries * sizeof(*descriptors)) & ~63U) + 64);
		result = msproal_write(currentSector, sectors, gMsifContext.dmaDescriptors);
		if (result) {
			if ((uint32_t)(result + 205) <= 204)
				result = gMsifLibraryErrors[result + 205];
			else
				result = 0x803d0000;
			SceDebugForDriver_02B04343(0, SCE_DBGINFO(2030),
				"0x%x %d 0x%x 0x%x\n", result, 2030, sector, count);
			goto unlock;
		}
		remaining = (uint16_t)(remaining - sectors);
		currentSector += sectors;
	} while (remaining);
	if (gMsifContext.mediaAccessTickEnabled) {
		int tickResult = ScePowerForDriver_EFD3C963(16);

#if FW_DEBUG
		if (tickResult < 0)
			SceDebugForDriver_D9703808(1, 0,
				SCE_DBGINFO_FUNC(2339, "sceMsifMediaAccessTick"),
				"msif power tick error. ret:0x%x\n", tickResult);
#endif
	}
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
	return 0;

unlock:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
	return result;
}

int sceMsifFormat(int mode)
{
	int result = lockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);

	if (result < 0)
		return result;
	if (mode != 0 && mode != 1) {
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
		return 0x803d0001;
	}
	result = MsproalFormat(mode);
	if (result) {
		if ((uint32_t)(result + 205) <= 204)
			result = gMsifLibraryErrors[result + 205];
		else
			result = 0x803d0000;
		SceDebugForDriver_02B04343(0, SCE_DBGINFO(2068),
			"0x%x %d\n", result, 2068);
	}
	if (gMsifContext.mediaAccessTickEnabled) {
		int tickResult = ScePowerForDriver_EFD3C963(16);

#if FW_DEBUG
		if (tickResult < 0)
			SceDebugForDriver_D9703808(1, 0,
				SCE_DBGINFO_FUNC(2339, "sceMsifMediaAccessTick"),
				"msif power tick error. ret:0x%x\n", tickResult);
#endif
	}
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
	return result;
}

int sceMsifGetMediaId(uint8_t *mediaId)
{
	struct MsproalMediaInformation information;
	uint8_t temporaryMediaId[16];
	int result = lockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);

	if (result < 0)
		return result;
	result = MsproalGetMediaInformation(&information);
	if (result < 0)
		goto library_error;
	if (!mediaId)
		mediaId = temporaryMediaId;
	result = MsifAuthenticateMedia(gMsifContext.handle, mediaId);
	if (result < 0)
		goto library_error;
	if (!result) {
		result = MsifGetSystemMediaId(mediaId);
		if (result)
			goto library_error;
		goto unlock;
	}
	result = msif_check_capacity(result);
	if (result) {
		if ((uint32_t)(result + 205) <= 204)
			result = gMsifLibraryErrors[result + 205];
		else
			result = 0x803d0000;
		SceDebugForDriver_02B04343(0, SCE_DBGINFO(2122),
			"0x%x %d\n", result, 2122);
		goto unlock;
	}
	result = msif_check_mid(gMsifContext.handle, mediaId);
	if (result) {
		if ((uint32_t)(result + 205) <= 204)
			result = gMsifLibraryErrors[result + 205];
		else
			result = 0x803d0000;
		SceDebugForDriver_02B04343(0, SCE_DBGINFO(2131),
			"0x%x %d\n", result, 2131);
		goto unlock;
	}
	if (gMsifContext.mediaAccessTickEnabled) {
		int tickResult = ScePowerForDriver_EFD3C963(16);

#if FW_DEBUG
		if (tickResult < 0)
			SceDebugForDriver_D9703808(1, 0,
				SCE_DBGINFO_FUNC(2339, "sceMsifMediaAccessTick"),
				"msif power tick error. ret:0x%x\n", tickResult);
#endif
	}
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
	return 0;

library_error:
	if ((uint32_t)(result + 205) <= 204)
		result = gMsifLibraryErrors[result + 205];
	else
		result = 0x803d0000;
unlock:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex);
	return result;
}

int sceMsifEnableSlowCardMode(void)
{
#if FW_DEBUG
	SceDebugForDriver_D9703808(1, 0,
		SCE_DBGINFO_FUNC(2151, "sceMsifEnableSlowCardMode"),
		"Enable MSIF Slow Card Mode\n");
#endif
	gMsifContext.slowCardMode = 1;
	return 0;
}

int sceMsifDisableSlowCardMode(void)
{
#if FW_DEBUG
	SceDebugForDriver_D9703808(1, 0,
		SCE_DBGINFO_FUNC(2162, "sceMsifDisableSlowCardMode"),
		"Disable MSIF Slow Card Mode\n");
#endif
	gMsifContext.slowCardMode = 0;
	return 0;
}

int SceMsifForDriver_491E25B5(void)
{
	return gMsifContext.slowCardMode;
}

uint64_t SceMsifForDriver_855E1E38(void)
{
	return gMsifContext.interruptTime - gMsifContext.commandStartTime;
}

int SceMsifForDriver_F997286B(void)
{
	sceKernelDelayThread(25000);
	if (!ScePervasiveForDriver_551EEE82() ||
		lockFastMutex((SceKernelFastMutex *)&gMsifContext.mutex) < 0 ||
		sceMsifReadSector(0, gMsifContext.dummyReadBuffer, 1)) {
		MsproalUserCancelTransfer(gMsifContext.handle);
		return 0x803d0009;
	}
	return 0;
}

