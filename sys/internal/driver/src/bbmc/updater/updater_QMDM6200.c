/*
 * The modem updater (SceBbmcUdMod): its context, the image the download sequence feeds
 * it, and update files read from RAM or through iofilemgr.
 */
#include <string.h>
#include <iofilemgr/iofilemgr.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include "updater.h"

/* name unknown: the hardware an image entry is for, by its HWID string */
typedef struct UdHardware {
	SceUInt32 type;
	char hardwareId[32];
} UdHardware;

/* name unknown: an update file's trailer, its last 104 bytes */
typedef struct UdFileTrailer {
	SceUInt8 reserved[40];
	SceUInt32 buildIdMagic;
	SceUInt32 buildId[4];
	SceUInt32 nameMagic;
	char name[32];
	SceUInt32 magic;
	SceUInt32 padding;
} UdFileTrailer;

_Static_assert(sizeof(UdFileTrailer) == 104, "update file trailer size");

static const UdHardware udHardwares[2] = {
	{ 0, "HWID_QUALCOMM_MDM6200" },
	{ 1, "HWID_QUALCOMM_MDM6600" },
};

UdContext *udContext;
SceUInt32 udRamIoEnabled;
SceUInt8 udSizeBuffer[1024];

__attribute__((noinline)) void udSetContext(UdContext *context)
{
	udContext = context;
}

__attribute__((noinline)) UdContext *udGetContext(void)
{
	return udContext;
}

int udGetHardwareType(const char *hardwareId)
{
	int i;

	for (i = 0; i < 2; i++)
		if (strncmp(udHardwares[i].hardwareId, hardwareId, 31) == 0)
			return udHardwares[i].type;
	return -1;
}

int udPrepareImage(SceUInt32 hardwareType, SceUInt32 configurationMode,
			  UdQmdm6200Work *work)
{
	if (hardwareType <= 1)
		return udPrepareImage_QMDM6200(configurationMode, work);
	return 0;
}

int udWaitImageChunk(SceUInt32 chunkIndex)
{
	UdContext *context = udContext;
	SceUInt32 timeout = 60000000;
	unsigned int pattern = 0;
	int result;

	result = sceKernelClearEventFlag(context->eventFlagId, ~1U);
	if (result < 0)
		return result;
	while (context->chunkIndex < chunkIndex) {
		if (context->requestChunk == NULL)
			return (int)0x80330002;
		result = context->requestChunk(chunkIndex);
		if (result < 0)
			return result;
		result = sceKernelWaitEventFlag(context->eventFlagId, 1, 1, &pattern,
						&timeout);
		if (result < 0)
			return result;
		if ((pattern & 1) == 0)
			return (int)0x80330002;
		result = sceKernelClearEventFlag(context->eventFlagId, ~pattern);
		if (result < 0)
			return result;
	}
	if (chunkIndex < context->chunkIndex)
		result = (int)0x80330002;
	return result;
}

SceUID udRamIoOpen(const char *path, int flags, SceMode mode)
{
	if (udContext == NULL)
		return (int)0x80330003;
	if (udContext->state <= 2 || udContext->hardwareType > 1)
		return (int)0x80330002;
	return udRamIoOpen_QMDM6200(path, flags, mode);
}

int udRamIoClose(SceUID fd)
{
	if (udContext == NULL)
		return (int)0x80330003;
	if (udContext->state <= 2 || udContext->hardwareType > 1)
		return (int)0x80330002;
	return udRamIoClose_QMDM6200(fd);
}

SceOff udRamIoLseek(SceUID fd, SceOff offset, int whence)
{
	if (udContext == NULL)
		return (int)0x80330003;
	if (udContext->state <= 2 || udContext->hardwareType > 1)
		return (int)0x80330002;
	return udRamIoLseek_QMDM6200(fd, offset, whence);
}

int udRamIoRead(SceUID fd, void *data, SceSize size)
{
	if (udContext == NULL)
		return (int)0x80330003;
	if (udContext->state <= 2 || udContext->hardwareType > 1)
		return (int)0x80330002;
	return udRamIoRead_QMDM6200(fd, data, size);
}

int udRamIoWrite(SceUID fd, const void *data, SceSize size)
{
	if (udContext == NULL)
		return (int)0x80330003;
	if (udContext->state <= 2 || udContext->hardwareType > 1)
		return (int)0x80330002;
	return udRamIoWrite_QMDM6200(fd, data, size);
}

SceOff udRamIoGetSize(SceUID fd)
{
	if (udContext == NULL)
		return (int)0x80330003;
	if (udContext->state <= 2 || udContext->hardwareType > 1)
		return (int)0x80330002;
	return udRamIoGetSize_QMDM6200(fd);
}

int udInitialize(SceUInt32 version, const UdClient *client)
{
	UdContext *context;
	SceUID heapId;
	SceUID eventFlagId;

	if (udGetContext() != NULL)
		return (int)0x80330004;
	if (version != 4 || client == NULL)
		return (int)0x80330002;
	heapId = sceKernelCreateHeap("SceUpdaterHeap", 492544, NULL);
	if (heapId < 0)
		return heapId;
	context = sceKernelAllocHeapMemory(heapId, sizeof(*context));
	if (context == NULL) {
		eventFlagId = (int)0x80330005;
	} else {
		memset(context, 0, sizeof(*context));
		context->lock = 0;
		eventFlagId = sceKernelCreateEventFlag("SceBbmcUdMod", 4096, 0, NULL);
		if (eventFlagId >= 0) {
			context->eventFlagId = eventFlagId;
			context->heapId = heapId;
			context->requestChunk = client->requestChunk;
			udSetContext(context);
			return eventFlagId;
		}
		sceKernelFreeHeapMemory(heapId, context);
	}
	sceKernelDeleteHeap(heapId);
	return eventFlagId;
}

int udTerminate(void)
{
	UdContext *context = udGetContext();
	SceUID heapId;

	if (context == NULL)
		return (int)0x80330003;
	heapId = context->heapId;
	udSetContext(NULL);
	if (context->hardwareType <= 1) {
		udReleaseImage_QMDM6200(context, &context->work);
		memset(&context->work, 0, sizeof(context->work));
	}
	sceKernelDeleteEventFlag(context->eventFlagId);
	sceKernelFreeHeapMemory(heapId, context);
	sceKernelDeleteHeap(heapId);
	return 0;
}

int udSetImageChunk(SceUInt32 chunkCount, SceUInt32 chunkIndex, const void *data,
		     SceUInt32 size)
{
	UdContext *context = udGetContext();
	SceUInt32 state;

	if (context == NULL)
		return (int)0x80330003;
	if (size == 0 || data == NULL)
		return (int)0x80330002;
	state = context->state;
	if (state != 0)
		return (int)0x80330002;
	sceKernelSpinlockLowLock(&context->lock);
	context->chunkIndex = chunkIndex;
	context->chunkCount = chunkCount;
	context->imageData = data;
	context->imageSize = size;
	sceKernelSpinlockLowUnlock(&context->lock);
	return state;
}

int udSetTarget(SceUInt32 hardwareType, SceUInt32 configurationMode)
{
	UdContext *context = udGetContext();

	if (context == NULL)
		return (int)0x80330003;
	sceKernelSpinlockLowLock(&context->lock);
	context->hardwareType = hardwareType;
	context->configurationMode = configurationMode;
	context->state = 2;
	sceKernelSpinlockLowUnlock(&context->lock);
	return 0;
}

int udLoadImage(void)
{
	UdContext *context = udGetContext();
	UdContext *current;
	UdImageHeader header;
	UdImageEntryHeader entry;
	const SceUInt8 *image;
	const SceUInt8 *cursor;
	const SceUInt8 *data;
	SceUInt32 imageSize;
	SceUInt32 remaining;
	SceUInt32 hardwareType;
	SceUInt32 length;
	int entryType;
	int count;
	int i;
	int result;

	if (context == NULL)
		return (int)0x80330003;
	if (context->state != 2 || context->chunkIndex != 0)
		return (int)0x80330002;
	sceKernelSpinlockLowLock(&context->lock);
	memset(&context->work, 0, sizeof(context->work));
	sceKernelSpinlockLowUnlock(&context->lock);

	image = context->imageData;
	imageSize = context->imageSize;
	current = udGetContext();
	if (image == NULL || imageSize <= 37)
		goto invalid;
	memcpy(&header, image, UD_IMAGE_HEADER_SIZE);
	cursor = image + UD_IMAGE_HEADER_SIZE;
	remaining = imageSize - UD_IMAGE_HEADER_SIZE;
	if (imageSize != header.imageSize)
		goto invalid;
	count = header.entryCount;
	if (count == 0)
		goto invalid;
	current->imageVersion = header.version;
	current = udGetContext();
	hardwareType = current->hardwareType;
	if (remaining == 0 || remaining <= 35)
		goto invalid;

	for (i = 0; ; ) {
		udCopyBytes(&entry, &cursor, sizeof(entry));
		entryType = udGetHardwareType(entry.hardwareId);
		length = entry.size - sizeof(entry);
		if (hardwareType == entryType) {
			data = NULL;
			if (cursor == NULL || entry.size == sizeof(entry))
				goto invalid;
			data = cursor;
			if (hardwareType > 1)
				goto invalid;
			result = udParseImage_QMDM6200(&data, &length);
			if (result < 0)
				goto failed;
			break;
		}
		if (current->imageVersion <= 7 &&
		    ((entryType == 1 && hardwareType == 0) ||
		     (entryType == 0 && hardwareType == 1))) {
			data = NULL;
			if (cursor == NULL || entry.size == sizeof(entry))
				goto invalid;
			data = cursor;
			result = udParseImage_QMDM6200(&data, &length);
			if (result < 0)
				goto failed;
			break;
		}
		i++;
		remaining -= entry.size;
		cursor += length;
		if (count <= i)
			break;
		if (remaining <= 35)
			goto invalid;
	}

	result = udPrepareImage(context->hardwareType, context->configurationMode,
				&context->work);
	if (result < 0)
		goto failed;
	sceKernelSpinlockLowLock(&context->lock);
	context->state = 3;
	sceKernelSpinlockLowUnlock(&context->lock);
	return result;

invalid:
	result = (int)0x80330002;
failed:
	if (context->hardwareType <= 1) {
		udReleaseImage_QMDM6200(context, &context->work);
		memset(&context->work, 0, sizeof(context->work));
	}
	return result;
}

int udResetDeleteList(void)
{
	UdContext *context = udGetContext();
	int result;

	if (context == NULL || context->state <= 3)
		return (int)0x80330003;
	if (context->hardwareType > 1)
		return (int)0x80330002;
	result = udResetDeleteList_QMDM6200();
	if (result < 0)
		return result;
	sceKernelSpinlockLowLock(&context->lock);
	context->state = 3;
	sceKernelSpinlockLowUnlock(&context->lock);
	return result;
}

int udCheckUpdateNoNeed(const SceUInt8 *information)
{
	UdContext *context = udGetContext();
	int result;

	if (context == NULL)
		return (int)0x80330003;
	if (information == NULL)
		return (int)0x80330002;
	if (context->state <= 2)
		return (int)0x80330003;
	if (context->hardwareType > 1)
		return (int)0x80330002;
	result = _udCheckUpdateNoNeedFromSafeBuildId(information + 16);
	if (result < 0)
		return result;
	sceKernelSpinlockLowLock(&context->lock);
	context->state = 4;
	sceKernelSpinlockLowUnlock(&context->lock);
	return result;
}

int udReceiveImageChunk(SceUInt32 chunkCount, SceUInt32 chunkIndex, const void *data,
		       SceUInt32 size)
{
	UdContext *context = udGetContext();
	int result;

	if (context == NULL)
		return (int)0x80330003;
	if (context->state <= 2 || chunkCount <= chunkIndex ||
	    context->chunkCount != chunkCount)
		return (int)0x80330002;
	sceKernelSpinlockLowLock(&context->lock);
	context->chunkIndex = chunkIndex;
	context->imageData = data;
	context->imageSize = size;
	sceKernelSpinlockLowUnlock(&context->lock);
	if (context->hardwareType > 1)
		return (int)0x80330002;
	result = _udAddUqcnData();
	if (result < 0)
		return result;
	return sceKernelSetEventFlag(context->eventFlagId, 1);
}

int udGetWriteSize(void)
{
	UdContext *context = udGetContext();

	if (context == NULL)
		return 0;
	if (context->state > 3 && context->hardwareType <= 1)
		return udGetWriteSize_QMDM6200();
	return 0;
}

int udGetDeleteSize(void)
{
	UdContext *context = udGetContext();

	if (context == NULL)
		return 0;
	if (context->state > 3 && context->hardwareType <= 1)
		return udGetDeleteSize_QMDM6200();
	return 0;
}

int udGetImageVersion(SceUInt32 *version)
{
	UdContext *context = udGetContext();

	if (context == NULL)
		return (int)0x80330003;
	if (version == NULL)
		return (int)0x80330002;
	if (context->state <= 2)
		return (int)0x80330003;
	*version = context->imageVersion;
	return 0;
}

int udIsInitialized(void)
{
	return udGetContext() != NULL;
}

int udEnableRamIo(void)
{
	if (udGetContext() == NULL)
		return (int)0x80330003;
	udRamIoEnabled = 1;
	return 0;
}

int udDisableRamIo(void)
{
	udRamIoEnabled = 0;
	return 0;
}

SceUID udIoOpen(const char *path, int flags, SceMode mode)
{
	if (udRamIoEnabled == 1)
		return udRamIoOpen(path, flags, mode);
	return sceIoOpen(path, flags, mode);
}

int udIoClose(SceUID fd)
{
	if (udRamIoEnabled == 1)
		return udRamIoClose(fd);
	return sceIoClose(fd);
}

SceOff udIoLseek(SceUID fd, SceOff offset, int whence)
{
	if (udRamIoEnabled == 1)
		return udRamIoLseek(fd, offset, whence);
	return sceIoLseek(fd, offset, whence);
}

int udIoRead(SceUID fd, void *data, SceSize size)
{
	if (udRamIoEnabled == 1)
		return udRamIoRead(fd, data, size);
	return sceIoRead(fd, data, size);
}

int udIoWrite(SceUID fd, const void *data, SceSize size)
{
	if (udRamIoEnabled == 1)
		return udRamIoWrite(fd, data, size);
	return sceIoWrite(fd, data, size);
}

SceOff udIoGetSize(SceUID fd)
{
	SceOff result;
	SceOff size;
	int read;

	if (udRamIoEnabled == 1)
		return udRamIoGetSize(fd);
	size = 0;
	for (;;) {
		read = sceIoRead(fd, udSizeBuffer, sizeof(udSizeBuffer));
		if (read < 0)
			return read;
		size += read;
		if (read == 0)
			break;
	}
	result = sceIoLseek(fd, 0, SCE_SEEK_SET);
	if (result < 0)
		return result;
	return size;
}

/* name unknown: copy size bytes from a cursor and advance it, or only advance it */
void udCopyBytes(void *dst, const SceUInt8 **cursor, SceUInt32 size)
{
	SceUInt8 *out = dst;

	if (out == NULL) {
		*cursor += size;
		return;
	}
	while (size-- != 0)
		*out++ = *(*cursor)++;
}

int udReadFileInformation(SceUID fd, UdFileInformation *information)
{
	UdFileTrailer trailer;
	SceOff offset;
	int result;

	if (information == NULL)
		return (int)0x80330003;
	if (udRamIoEnabled == 1)
		offset = udRamIoLseek(fd, -(SceOff)sizeof(trailer), SCE_SEEK_END);
	else
		offset = sceIoLseek(fd, -(SceOff)sizeof(trailer), SCE_SEEK_END);
	if (offset < 0)
		return offset;
	if (udRamIoEnabled == 1)
		result = udRamIoRead(fd, &trailer, sizeof(trailer));
	else
		result = sceIoRead(fd, &trailer, sizeof(trailer));
	if (result < 0)
		return result;
	if (result != sizeof(trailer))
		return (int)0x8033070C;
	if (udRamIoEnabled == 1)
		offset = udRamIoLseek(fd, 0, SCE_SEEK_SET);
	else
		offset = sceIoLseek(fd, 0, SCE_SEEK_SET);
	if (offset < 0)
		return offset;
	memset(information, 0, sizeof(*information));
	if (trailer.magic != 0xFEDC1234 || trailer.nameMagic != 0xFEDC1235)
		return (int)0x8033070B;
	memcpy(information->name, trailer.name, sizeof(information->name));
	information->nameLength = strnlen(information->name, 32);
	if (trailer.buildIdMagic != 0xFEDC1237)
		return (int)0x8033070B;
	memcpy(information->buildId, trailer.buildId, sizeof(information->buildId));
	return 0;
}

int udIsUpdateNeeded(SceUInt32 *needed)
{
	UdContext *context = udGetContext();

	if (needed == NULL || context == NULL)
		return (int)0x80330002;
	if (context->state <= 2)
		return (int)0x80330002;
	if (context->hardwareType <= 1)
		return udIsUpdateNeeded_QMDM6200(needed);
	*needed = 0;
	return 0;
}
