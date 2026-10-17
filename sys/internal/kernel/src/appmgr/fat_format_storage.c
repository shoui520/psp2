/* FAT storage helpers; the source-family filename is inferred. */

#include "fat_format_internal.h"
#include <exfatfs/fat_boot.h>
#include <psp2kern/io/fcntl.h>
#include <psp2kern/kernel/debug.h>
#include <string.h>

/* Byte stores preserve the FAT wire encoding, including unaligned members. */
#define STORE_LE16(member, value) do { \
	((unsigned char *)&(member))[0] = (unsigned char)(value); \
	((unsigned char *)&(member))[1] = (unsigned char)((value) >> 8); \
} while (0)
#define STORE_LE32(member, value) do { \
	((unsigned char *)&(member))[0] = (unsigned char)(value); \
	((unsigned char *)&(member))[1] = (unsigned char)((value) >> 8); \
	((unsigned char *)&(member))[2] = (unsigned char)((value) >> 16); \
	((unsigned char *)&(member))[3] = (unsigned char)((value) >> 24); \
} while (0)

#if FW_DEBUG
int error_notification(AppmgrFatFormatArguments *const *arguments,
		       AppmgrFatFormatNotification *notification, int error)
{
	int result;

	ksceKernelPrintfLevel(1, "%s: sceIoWrite failed[%#x]\n", __func__, error);
	if (*notification) {
		result = (*notification)(*arguments);
		if (result < 0)
			ksceKernelPrintfLevel(1,
				"%s: return sceIoWrite error[%#x] instead of return value of notification[%#x]\n",
				__func__, error, result);
	}
	return error;
}
#endif

int _prepare_FAT_PBR(AppmgrFatFormatContext *context)
{
	unsigned int sectorSize = context->bytesPerSector;
	AppmgrFatFormatArguments *arguments = context->arguments;
	uint64_t totalSectors = arguments->volumeSize / sectorSize;
	SceExfatfsBootSector *boot = arguments->buffer;
	SceExfatfsFatBootParameters *parameters = &boot->fat16.parameters;

	memset(boot, 0, sectorSize);
	parameters->jump[0] = 0xeb;
	parameters->jump[2] = 0x90;
	parameters->jump[1] = 0xfe;
	__builtin_memcpy(parameters->oemName, "SCEI    ", 8);
	parameters->bytesPerSector = context->bytesPerSector;
	parameters->sectorsPerCluster = context->arguments->clusterSize /
		context->bytesPerSector;
	STORE_LE16(parameters->reservedSectorCount, context->reservedSectors);
	parameters->numberOfFats = context->fatCount;
	STORE_LE16(parameters->rootEntryCount, context->rootEntryCount);
	if (totalSectors < 0x10000)
		STORE_LE16(parameters->totalSectors16, totalSectors);
	else
		STORE_LE32(parameters->totalSectors32, totalSectors);
	STORE_LE16(parameters->sectorsPerTrack, 63U);
	parameters->media = 0xf8;
	STORE_LE16(parameters->numberOfHeads, 255U);
	STORE_LE32(parameters->hiddenSectors, context->startLba);
	if (context->arguments->type == 0 || context->arguments->type > 3) {
#if FW_DEBUG
		ksceKernelPrintfLevel(1, "%s: unsupported type(%d)\n",
			__func__, context->arguments->type);
#endif
		return (int)0x80010086;
	}
	if (context->arguments->type == 3) {
		STORE_LE32(boot->fat32.volumeSerial, context->arguments->serial);
		STORE_LE32(boot->fat32.fatSectors32, context->fatSectors);
		STORE_LE32(boot->fat32.rootCluster, context->rootCluster);
		STORE_LE16(boot->fat32.fsInfoSector, 1U);
		STORE_LE16(boot->fat32.backupBootSector, 6U);
		boot->fat32.driveNumber = 0x80;
		boot->fat32.extendedSignature = 0x29;
		__builtin_memcpy(boot->fat32.volumeLabel, "NO NAME    FAT32   ", 19);
		if (context->arguments->volumeLabel) {
			memset(boot->fat32.volumeLabel, ' ', 11);
			strncpy((char *)boot->fat32.volumeLabel,
				context->arguments->volumeLabel, 11);
		}
	} else {
		boot->fat16.volumeSerial[0] = context->arguments->serial;
		boot->fat16.volumeSerial[1] = context->arguments->serial >> 8;
		boot->fat16.volumeSerial[2] = context->arguments->serial >> 16;
		boot->fat16.volumeSerial[3] = context->arguments->serial >> 24;
		STORE_LE16(parameters->fatSectors16, context->fatSectors);
		boot->fat16.driveNumber = 0x80;
		boot->fat16.extendedSignature = 0x29;
		if (context->arguments->type == 2)
			__builtin_memcpy(boot->fat16.volumeLabel, "NO NAME    FAT16   ", 19);
		else
			__builtin_memcpy(boot->fat16.volumeLabel, "NO NAME    FAT12   ", 19);
		if (context->arguments->volumeLabel) {
			memset(boot->fat16.volumeLabel, ' ', 11);
			strncpy((char *)boot->fat16.volumeLabel,
				context->arguments->volumeLabel, 11);
		}
	}
	STORE_LE16(boot->fat16.signature, 0xaa55U);
	return 0;
}

static inline uint64_t _start_lba(AppmgrFatFormatContext *context, int index)
{
	if (index == 1)
		return context->startLba;
	if (index == 2)
		return context->alternate.startLba;
	if (index == 0)
		return 0;
#if FW_DEBUG
	ksceKernelPrintfLevel(1, "%s: unsupported index(%d)\n", __func__, index);
#endif
	return UINT64_MAX;
}

int zero_fill(AppmgrFatFormatContext *context, unsigned int bufferOffset,
	      unsigned int length, uint64_t byteOffset, int regionIndex)
{
	uint64_t currentLba = context->currentLba;
	SceOff seekResult;
	unsigned int capacity, fillSize, written;
	unsigned char *buffer;
	int result;

	if (_start_lba(context, regionIndex) != currentLba)
		byteOffset += (_start_lba(context, regionIndex) - context->currentLba) *
			context->bytesPerSector;
	seekResult = ksceIoLseek(context->file, byteOffset, SCE_SEEK_SET);
	if (seekResult < 0) {
#if FW_DEBUG
		ksceKernelPrintfLevel(1, "%s: sceIoLseek failed[%#llx]\n",
			__func__, seekResult);
#endif
		return seekResult;
	}
	buffer = (unsigned char *)context->arguments->buffer + bufferOffset;
	capacity = context->arguments->bufferSize - bufferOffset;
	if (capacity > length)
		fillSize = length;
	else
		fillSize = context->bytesPerSector * (capacity / context->bytesPerSector);
	written = length;
	memset(buffer, 0, fillSize);
	if (length != 0) {
		written = 0;
		for (;;) {
			result = ksceIoWrite(context->file, buffer,
				fillSize < length ? fillSize : length);
			written += result;
			if (result < 0) {
#if FW_DEBUG
				ksceKernelPrintfLevel(1, "%s: sceIoWrite failed[%#llx]\n",
					__func__, (int64_t)result);
#endif
				return result;
			}
			if (result == 0 || (int64_t)result > length) {
#if FW_DEBUG
				ksceKernelPrintfLevel(1,
					"%s: unexpected result[%#llx] of sceIoWrite\n",
					__func__, (int64_t)result);
#endif
				return -1;
			}
			length -= result;
			if (length == 0)
				break;
		}
	}
	return written;
}
