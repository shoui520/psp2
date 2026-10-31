/* The modem updater (updater/updater_QMDM6200.c, updater/updater_seq_QMDM6200.c). */
#pragma once
#include <scetypes.h>
#include <kernel/types.h>
#include <kernel/iofilemgr.h>
#include <stddef.h>
#include <sysmem/cpu.h>

/*
 * name unknown: the QMDM6200 updater's image work area (firmware lists, RAM files and
 * delete list), laid out when updater_QMDM6200.c's image code is written.
 */
typedef struct UdQmdm6200Work {
	SceUInt8 data[0x74];
} UdQmdm6200Work;

/* name unknown: the client the updater asks for more image data */
typedef struct UdClient {
	int (*requestData)(SceUInt32 size);
	void *argument;
} UdClient;

/* name unknown: the updater module's context (SceBbmcUdMod), 0xA4 bytes from its heap */
typedef struct UdContext {
	SceKernelSpinlock lock;
	SceUID eventFlagId;
	SceUID heapId;
	SceUInt32 state;
	SceUInt32 hardwareType;
	SceUInt32 configurationMode;
	SceUInt32 imageVersion;
	UdQmdm6200Work work;
	SceUInt32 receivedBytes;
	SceUInt32 totalBytes;
	const SceUInt8 *imageData;
	SceUInt32 imageSize;
	int (*requestData)(SceUInt32 size);
} UdContext;

_Static_assert(sizeof(UdContext) == 0xa4, "updater context size");
_Static_assert(offsetof(UdContext, receivedBytes) == 0x90, "updater received bytes");

/* name unknown: the image header and the header of each hardware's entry after it */
typedef struct UdImageHeader {
	SceUInt8 description[28];
	SceUInt32 version;
	SceUInt32 imageSize;
	SceUInt16 entryCount;
} UdImageHeader;

typedef struct UdImageEntryHeader {
	char hardwareId[32];
	SceUInt32 size;
} UdImageEntryHeader;

#define UD_IMAGE_HEADER_SIZE	38

/* name unknown: an update file's header, its magic words 0xFEDC1234, 0xFEDC1235, 0xFEDC1237 */
typedef struct UdFileInformation {
	SceUInt32 reserved;
	SceUInt32 buildId[4];
	SceUInt32 nameLength;
	char name[32];
	SceUInt32 padding;
} UdFileInformation;

/* the flags the download sequences open an update file with */
#define UD_IO_OPEN_FLAGS	(SCE_O_RDONLY | 0x04000000)

/* updater/updater_QMDM6200.c, not written yet; names unknown except Sony's */
int udParseImage_QMDM6200(const SceUInt8 **cursor, SceUInt32 *length);
int udReleaseImage_QMDM6200(UdContext *context, UdQmdm6200Work *work);
int udPrepareImage_QMDM6200(SceUInt32 configurationMode, UdQmdm6200Work *work);
int udResetDeleteList_QMDM6200(void);
int _udCheckUpdateNoNeedFromSafeBuildId(const SceUInt8 *buildId);
int _udAddUqcnData(void);
int udGetWriteSize_QMDM6200(void);
int udGetDeleteSize_QMDM6200(void);
int udIsUpdateNeeded_QMDM6200(SceUInt32 *needed);
SceUID udRamIoOpen_QMDM6200(const char *path, int flags, SceMode mode);
int udRamIoClose_QMDM6200(SceUID fd);
SceOff udRamIoLseek_QMDM6200(SceUID fd, SceOff offset, int whence);
int udRamIoRead_QMDM6200(SceUID fd, void *data, SceSize size);
int udRamIoWrite_QMDM6200(SceUID fd, const void *data, SceSize size);
SceOff udRamIoGetSize_QMDM6200(SceUID fd);

/* names unknown: the updater module's interface */
void udSetContext(UdContext *context);
UdContext *udGetContext(void);
int udGetHardwareType(const char *hardwareId);
int udPrepareImage(SceUInt32 hardwareType, SceUInt32 configurationMode,
		   UdQmdm6200Work *work);
SceUID udRamIoOpen(const char *path, int flags, SceMode mode);
int udRamIoClose(SceUID fd);
SceOff udRamIoLseek(SceUID fd, SceOff offset, int whence);
int udRamIoRead(SceUID fd, void *data, SceSize size);
int udRamIoWrite(SceUID fd, const void *data, SceSize size);
SceOff udRamIoGetSize(SceUID fd);
void udCopyBytes(void *dst, const SceUInt8 **cursor, SceUInt32 size);
int udInitialize(SceUInt32 version, const UdClient *client);
int udTerminate(void);
int udSetImageSource(SceUInt32 totalBytes, SceUInt32 receivedBytes, const void *data,
		     SceUInt32 size);
int udSetTarget(SceUInt32 hardwareType, SceUInt32 configurationMode);
int udLoadImage(void);
int udResetDeleteList(void);
int udCheckUpdateNoNeed(const SceUInt8 *information);
int udReceiveImageData(SceUInt32 totalBytes, SceUInt32 receivedBytes, const void *data,
		       SceUInt32 size);
int udGetWriteSize(void);
int udGetDeleteSize(void);
int udGetImageVersion(SceUInt32 *version);
int udIsInitialized(void);
int udEnableRamIo(void);
int udDisableRamIo(void);
int udWaitImageData(SceUInt32 size);
int udIsUpdateNeeded(SceUInt32 *needed);
int udReadFileInformation(SceUID fd, UdFileInformation *information);

/* open, close and size an update file in RAM or through iofilemgr */
SceUID udIoOpen(const char *path, int flags, SceMode mode);
int udIoClose(SceUID fd);
SceOff udIoLseek(SceUID fd, SceOff offset, int whence);
int udIoRead(SceUID fd, void *data, SceSize size);
int udIoWrite(SceUID fd, const void *data, SceSize size);
SceOff udIoGetSize(SceUID fd);
