/* SceSdif DMA descriptors (Sony source: adma.c). */
#include "sdif_internal.h"
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <string.h>

int setAdmaDescriptorsForAlignedData(SdifCommand *command)
{
	SceKernelVARange range;
	SceKernelPAVector vector;
	SceKernelAllocMemBlockKernelOpt option;
	SdifAdmaDescriptor *descriptor, last;
	uint32_t flags = command->flags;
	uint32_t buffer, total, leading, trailing, aligned, remaining;
	uint32_t count, entries, index, length, address, chunk, bytes, allocation;
	uint32_t cursor, end;
	void *alignedBuffer;
	int result;

	if (flags & 0x1000000) {
		remaining = command->blockSize;
		if (command->blockCount)
			remaining *= command->blockCount;
		command->alignedLength = remaining;
		command->leadingLength = 0;
		command->trailingLength = 0;
		command->descriptors = command->defaultDescriptors;
		if (!remaining)
			return 0x8032000e;
		entries = 0;
		do {
			chunk = remaining >= 4096 ? 4096 : remaining;
			descriptor = &command->descriptors[entries++];
			remaining -= chunk;
			descriptor->address = 0x1d000000;
			descriptor->length = chunk;
			descriptor->attributes = 0x21;
		} while (remaining);
		if (entries) {
			last = command->descriptors[entries - 1];
			last.attributes |= 2;
			command->descriptors[entries - 1] = last;
		}
		SceCpuForDriver_103872A5(command->descriptors, 128);
		command->descriptorPhysicalAddress = command->defaultDescriptorPhysicalAddress;
		return 0;
	}
	buffer = (uintptr_t)command->buffer;
	if (buffer & 3)
		return 0x8032001b;
	total = command->blockSize;
	if (command->blockCount)
		total *= command->blockCount;
	if ((command->flags & 0x100200) == 0x100000 && ((buffer | total) & 63))
		return 0x8032001e;
	command->alignedLength = total;
	if (buffer & 63) {
		leading = 64 - (buffer & 63);
		if (total < leading) {
			leading = total;
			aligned = 0;
		} else {
			aligned = total - leading;
		}
		command->leadingLength = leading;
		command->alignedBuffer = (void *)(buffer + leading);
		command->alignedLength = aligned;
		if (flags & 0x200) {
			memcpy(command->leadingBuffer, (void *)buffer, leading);
			aligned = command->alignedLength;
		}
	} else {
		aligned = total;
		command->leadingLength = 0;
		command->alignedBuffer = (void *)buffer;
	}
	if (aligned) {
		trailing = (buffer + total) & 63;
		if (trailing) {
			remaining = 0;
			if (trailing <= aligned) {
				remaining = aligned - trailing;
				command->trailingLength = trailing;
				aligned = trailing;
			} else {
				command->trailingLength = aligned;
			}
			command->alignedLength = remaining;
			if (command->flags & 0x200) {
				memcpy(command->trailingBuffer,
				       (void *)((buffer + total) & ~63U), aligned);
				aligned = command->alignedLength;
			} else {
				aligned = remaining;
			}
		} else {
			command->trailingLength = 0;
		}
	} else {
		command->trailingLength = aligned;
	}
	alignedBuffer = command->alignedBuffer;
	count = 0;
	if (aligned) {
		range.addr = (uintptr_t)alignedBuffer;
		range.size = aligned;
		vector.size = sizeof(vector);
		vector.capacity = 16;
		vector.nDataInVector = 0;
		vector.total = 0;
		vector.pRanges = command->physicalRanges;
		result = sceKernelVARangeToPAVector(&range, &vector);
		if (result < 0) {
			SCE_DBG_PRINTF_FUNC(0, 80, "getPaRangeForAlignedData",
				"%s.%d VARangeToPAVector error:0x%x(%d)\n",
				"getPaRangeForAlignedData", 80, result, result);
			SCE_DBG_PRINTF_FUNC(0, 82, "getPaRangeForAlignedData",
				"%s. VARangeToPAVector base:0x%x size:0x%x\n",
				"getPaRangeForAlignedData", command->alignedBuffer,
				command->alignedLength);
			return result;
		}
		count = vector.total;
	}
	entries = !!command->leadingLength;
	if (command->trailingLength)
		entries++;
	remaining = command->alignedLength;
	if (remaining) {
		if (remaining <= 65536) {
			entries += count;
		} else if (count <= 16) {
			for (index = 0; index != count; index++) {
				length = command->physicalRanges[index].size;
				if (length > 65536) {
					entries += length >> 16;
					if (!(length & 65535))
						continue;
				}
				entries++;
			}
		} else {
			cursor = (uintptr_t)command->alignedBuffer;
			end = cursor + remaining;
			while (cursor < end) {
				range.addr = cursor;
				range.size = remaining;
				vector.size = sizeof(vector);
				vector.capacity = 1;
				vector.nDataInVector = 0;
				vector.total = 0;
				vector.pRanges = command->physicalRanges;
				result = sceKernelVARangeToPAVector(&range, &vector);
				if (result < 0) {
					SCE_DBG_PRINTF_FUNC(0, 159, "getAdmaDescriptorEntryNum",
						"%s.%d VARangeToPAVector error:0x%x(%d)\n",
						"getAdmaDescriptorEntryNum", 159, result, result);
					SCE_DBG_PRINTF_FUNC(0, 161, "getAdmaDescriptorEntryNum",
						"%s. VARangeToPAVector base:%p size:0x%x\n",
						"getAdmaDescriptorEntryNum", (void *)cursor, remaining);
					return result;
				}
				SCE_DBG_ASSERT_FUNC(vector.nDataInVector == 1, 164,
					"getAdmaDescriptorEntryNum");
				length = command->physicalRanges[0].size;
				if (length > 65536) {
					entries += length >> 16;
					if (!(length & 65535))
						goto countedRange;
				}
				entries++;
countedRange:
				cursor += length;
				remaining -= length;
			}
		}
	}
	if (!command->alignedLength || entries <= 16) {
		bytes = 128;
		command->descriptors = command->defaultDescriptors;
	} else {
		bytes = ((entries & ~7U) + 8) * 8;
		if (bytes > 65536) {
			SCE_DBG_PRINTF_FUNC(0, 391, "allocateAdmaDescriptorArea",
				"%s %d Ex ADMA desc area is too large:%d\n",
				"allocateAdmaDescriptorArea", 391, bytes);
			return 0x8032000e;
		}
		if (command->flags & 0x100000) {
			SCE_DBG_PRINTF_FUNC(0, 403, "allocateAdmaDescriptorArea",
				"%s %d Warning: allocate memblock for ADMA descriptor.%d\n",
				"allocateAdmaDescriptorArea", 403, bytes);
			allocation = bytes >> 12;
			if (bytes & 4095)
				allocation += 4096;
			memset(&option, 0, sizeof(option));
			option.attr = 0x200000;
			option.size = sizeof(option);
			result = SceSysmemForDriver_C94850C9("SceSdif", 0x1020d006, allocation, &option);
			if (result < 0) {
				SCE_DBG_PRINTF_FUNC(0, 417, "allocateAdmaDescriptorArea",
					"%s %d Ex ADMA desc area is too large:%d\n",
					"allocateAdmaDescriptorArea", 417, allocation);
				return result;
			}
			command->descriptorMemblock = result;
			SceSysmemForDriver_A841EDDA(result, (void **)&command->descriptors);
		} else {
			command->descriptors = command->context->port.descriptorBuffer;
		}
	}
	entries = 0;
	if (command->leadingLength) {
		SceCpuForDriver_103872A5(command->leadingBuffer, 64);
		descriptor = command->descriptors;
		descriptor->address = command->leadingPhysicalAddress;
		descriptor->length = command->leadingLength;
		descriptor->attributes = 0x21;
		entries = 1;
	}
	if (aligned) {
		SceCpuForDriver_103872A5(alignedBuffer, aligned);
		if (count <= 16) {
			for (index = 0; index != count; index++) {
				address = command->physicalRanges[index].addr;
				length = command->physicalRanges[index].size;
				while (length) {
					chunk = length >= 65536 ? 65536 : length;
					descriptor = &command->descriptors[entries++];
					length -= chunk;
					descriptor->address = address;
					address += chunk;
					descriptor->length = chunk;
					descriptor->attributes = 0x21;
				}
			}
		} else {
			cursor = (uintptr_t)command->alignedBuffer;
			remaining = command->alignedLength;
			end = cursor + remaining;
			while (cursor < end) {
				range.addr = cursor;
				range.size = remaining;
				vector.size = sizeof(vector);
				vector.capacity = 1;
				vector.nDataInVector = 0;
				vector.total = 0;
				vector.pRanges = command->physicalRanges;
				result = sceKernelVARangeToPAVector(&range, &vector);
				if (result < 0) {
					SCE_DBG_PRINTF_FUNC(0, 250, "setAdmaDescriptorsForAlignedData",
						"%s.%d VARangeToPAVector error:0x%x(%d)\n",
						"setAdmaDescriptorsForAlignedData", 250, result, result);
					SCE_DBG_PRINTF_FUNC(0, 252, "setAdmaDescriptorsForAlignedData",
						"%s. VARangeToPAVector base:%p size:0x%x\n",
						"setAdmaDescriptorsForAlignedData", (void *)cursor, remaining);
					goto freeArea;
				}
				SCE_DBG_ASSERT_FUNC(vector.nDataInVector == 1, 255,
					"setAdmaDescriptorsForAlignedData");
				SCE_DBG_ASSERT_FUNC(remaining >= command->physicalRanges[0].size,
					256, "setAdmaDescriptorsForAlignedData");
				length = command->physicalRanges[0].size;
				address = command->physicalRanges[0].addr;
				while (length) {
					chunk = length >= 65536 ? 65536 : length;
					descriptor = &command->descriptors[entries++];
					length -= chunk;
					descriptor->address = address;
					address += chunk;
					descriptor->length = chunk;
					descriptor->attributes = 0x21;
				}
				length = command->physicalRanges[0].size;
				remaining -= length;
				cursor += length;
			}
		}
	}
	if (command->trailingLength) {
		SceCpuForDriver_103872A5(command->trailingBuffer, 64);
		descriptor = &command->descriptors[entries++];
		descriptor->address = command->trailingPhysicalAddress;
		descriptor->length = command->trailingLength;
		descriptor->attributes = 0x21;
	}
	if (entries) {
		last = command->descriptors[entries - 1];
		last.attributes |= 2;
		command->descriptors[entries - 1] = last;
	} else {
		SCE_DBG_PRINTF_FUNC(0, 559, "prepareAdmaDescriptorTable",
			"%s %d Warning: descriptor entry num is zero\n",
			"prepareAdmaDescriptorTable", 559);
	}
	SceCpuForDriver_103872A5(command->descriptors, bytes);
	if (command->descriptors == command->defaultDescriptors) {
		command->descriptorPhysicalAddress = command->defaultDescriptorPhysicalAddress;
		return 0;
	}
	result = SceSysmemForDriver_8D160E65(command->descriptors, &command->descriptorPhysicalAddress);
	if (result >= 0)
		return 0;
	SCE_DBG_PRINTF_FUNC(0, 590, "prepareAdmaDescriptorTable",
		"VA to PA error:0x%x  VA:%p PA:0x%x\n", result,
		command->descriptors, command->descriptorPhysicalAddress);
freeArea:
	if (command->flags & 0x100000)
		SceSysmemForDriver_009E1C61(command->descriptorMemblock);
	else
		SCE_DBG_ASSERT_FUNC(command->descriptors == command->context->port.descriptorBuffer,
			434, "freeAdmaDescriptorArea");
	return result;
}

int sceSdifAdmaKick(SdifCommand *command, uint16_t commandFlags)
{
	SdifPortState *port;
	volatile SdifHostRegisters *registers;
	uint32_t flags, count, argument;
	uint16_t mode, autoMode;
	uint8_t timeout;

	SCE_DBG_ASSERT(command != NULL, 694);
	SCE_DBG_ASSERT(command->context != NULL, 695);
	SCE_DBG_ASSERT(command->context->port.registers != NULL, 696);
	port = &command->context->port;
	timeout = port->timeoutControl;
	registers = port->registers;
	port->dataCommand = command;
	registers->timeoutControl = timeout;
	registers->hostControl = (registers->hostControl & 0xe7) | 0x10;
	flags = command->flags;
	count = command->blockCount;
	autoMode = (flags & 0x100) ? 20 : 4;
	mode = (flags & 0x100) ? 16 : 0;
	registers->blockSize = command->blockSize;
	if (flags & 0x800)
		mode = autoMode;
	registers->blockCount = count;
	if (count == 1)
		mode |= 2;
	else if (count > 1)
		mode |= 0x22;
	argument = command->argument;
	registers->admaSystemAddress = command->descriptorPhysicalAddress;
	registers->argument = argument;
	registers->transferMode = mode | 1;
	command->startTime = sceKernelGetSystemTimeWide();
	registers->command = commandFlags | (command->command << 8) | 0x20;
	return 0;
}

int freeAdmaDescriptorArea(SdifCommand *command)
{
	uint32_t flags = command->flags;

	if ((flags & 0x400) && command->descriptors != command->defaultDescriptors) {
		if (flags & 0x100000)
			SceSysmemForDriver_009E1C61(command->descriptorMemblock);
		else
			SCE_DBG_ASSERT_FUNC(command->descriptors == command->context->port.descriptorBuffer,
					    434, "freeAdmaDescriptorArea");
	}
	if (command->state == 3 && !(command->flags & 0x100200)) {
		if (command->leadingLength) {
			SceCpuForDriver_02796361(command->leadingBuffer, 64);
			memcpy(command->buffer, command->leadingBuffer, command->leadingLength);
		}
		if (command->trailingLength) {
			SceCpuForDriver_02796361(command->trailingBuffer, 64);
			memcpy((char *)command->alignedBuffer + command->alignedLength,
			       command->trailingBuffer, command->trailingLength);
		}
		if (command->state == 3 && !(command->flags & 0x100200) &&
		    command->alignedLength)
			SceCpuForDriver_02796361(command->alignedBuffer, command->alignedLength);
	}
	return 0;
}

void sceSdifDumpAdmaInfo(SdifCommand *command)
{
	uint32_t physicalAddress;
	union {
		SdifAdmaDescriptor fields;
		uint64_t value;
	} descriptor;
	unsigned index;
	int result;

	SCE_DBG_PRINTF(11, 824,
		"CMD:%d arg:0x%x buff:%p size:%d flag:0x%x blksize:%d blkcount:%d\n",
		command->command, command->argument, command->buffer, command->size,
		command->flags, command->blockSize, command->blockCount);
	if (command->descriptors == command->defaultDescriptors) {
		SCE_DBG_PRINTF(0, 829, "Default descriptor area was used. VA:%p PA:0x%x\n",
			command->descriptors, command->descriptorPhysicalAddress);
		for (index = 0; index < 16; index++) {
			descriptor.fields = command->descriptors[index];
			SCE_DBG_PRINTF(0, 831, "descriptor[%d]:0x%llx\n", index, descriptor.value);
		}
		SCE_DBG_PRINTF(0, 834, "# Invalidate descriptor area to read real descriptor.\n");
		SceCpuForDriver_02796361(command->descriptors, 128);
		for (index = 0; index < 16; index++) {
			descriptor.fields = command->descriptors[index];
			SCE_DBG_PRINTF(0, 839, "descriptor[%d]:0x%llx\n", index, descriptor.value);
		}
		result = SceSysmemForDriver_8D160E65(command->descriptors, &physicalAddress);
		if (result < 0)
			SCE_DBG_PRINTF(0, 845, "VA to PA error:0x%x  VA:%p PA:0x%x\n",
				result, command->descriptors, physicalAddress);
		if (physicalAddress != command->descriptorPhysicalAddress)
			SCE_DBG_PRINTF(0, 850, "PA(0x%x) differed from the previous PA(0x%x)\n",
				physicalAddress, command->descriptorPhysicalAddress);
	} else {
		SCE_DBG_PRINTF(0, 855, "Temporary descriptor area was used. VA:%p PA:0x%x\n",
			command->descriptors, command->descriptorPhysicalAddress);
	}
	SCE_DBG_PRINTF(0, 859, "alignedAddr         :0x%x\n", command->alignedBuffer);
	result = SceSysmemForDriver_8D160E65(command->alignedBuffer, &physicalAddress);
	if (result < 0)
		SCE_DBG_PRINTF(0, 863, "VA to PA error:0x%x  VA:0x%x PA:0x%x\n",
			result, command->alignedBuffer, physicalAddress);
	SCE_DBG_PRINTF(0, 865, "alignedPhyAddr      :0x%x\n", physicalAddress);
	SCE_DBG_PRINTF(0, 866, "alignedLength       :%d\n", command->alignedLength);
	SCE_DBG_PRINTF(0, 867, "unalignedHeadLength :%d\n", command->leadingLength);
	SCE_DBG_PRINTF(0, 868, "unalignedTailLength :%d\n", command->trailingLength);
	SCE_DBG_PRINTF(0, 869, "unalignedHeadPhyAddr:0x%x\n", command->leadingPhysicalAddress);
	SCE_DBG_PRINTF(0, 870, "unalignedTailPhyAddr:0x%x\n", command->trailingPhysicalAddress);
}
