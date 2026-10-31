/* The modem updater (updater/updater_QMDM6200.c, updater/updater_seq_QMDM6200.c). */
#pragma once
#include <scetypes.h>
#include <kernel/types.h>
#include <kernel/iofilemgr.h>
#include <stddef.h>
#include <sysmem/cpu.h>
#define BZ_NO_STDIO
#include <bzlib.h>

/* names unknown: the firmware types of an image, by the names udRamIoOpen_QMDM6200 opens */
#define UD_FIRM_ENPRG_HEX	0x01
#define UD_FIRM_PARTITION	0x02
#define UD_FIRM_DBL		0x03
#define UD_FIRM_OSBL		0x04
#define UD_FIRM_SAFE		0x10
#define UD_FIRM_UQCN_GENERIC	0x40
#define UD_FIRM_UQCN_VF		0x41
#define UD_FIRM_UQCN_ATT	0x42
#define UD_FIRM_UQCN_DOCOMO	0x43

/* the compression of a firmware in the image */
#define UD_COMPRESSION_NONE	0
#define UD_COMPRESSION_BZIP2	1

#define UD_BACKUP_SIZE		307200	/* the modem NV backup */
#define UD_RAM_FILE_BUFFER_SIZE	0x4000

/* bzip2 calls it on an internal error (bzlib_private.h) */
void bz_internal_error(int errcode);

/* name unknown: a firmware record of the image, 50 bytes, 2-byte aligned */
#pragma pack(push, 2)
typedef struct UdFirmRecord {
	SceUInt16 type;
	char name[32];
	SceUInt16 chunkIndex;
	SceUInt32 offset;
	SceUInt16 compression;
	SceUInt32 size;
	SceUInt32 storedSize;
} UdFirmRecord;
#pragma pack(pop)

_Static_assert(sizeof(UdFirmRecord) == 50, "image firmware record size");

/* name unknown: one of a carrier record's two firmwares (a firmware record without its type) */
typedef struct __attribute__((packed)) UdCarrierFirmRecord {
	char name[32];
	SceUInt16 chunkIndex;
	SceUInt32 offset;
	SceUInt16 compression;
	SceUInt32 size;
	SceUInt32 storedSize;
} UdCarrierFirmRecord;

/* name unknown: a carrier record of the image, 98 bytes */
typedef struct UdCarrierRecord {
	SceUInt16 id;
	UdCarrierFirmRecord firm[2];
} UdCarrierRecord;

_Static_assert(sizeof(UdCarrierRecord) == 98, "image carrier record size");

/* name unknown: the header of each configuration mode's carrier records */
typedef struct UdCarrierGroup {
	SceUInt16 configurationMode;
	SceUInt16 count;
} UdCarrierGroup;

/* name unknown: a compressed firmware read through bzip2 */
typedef struct UdRamFile {
	SceUInt32 compressed;
	bz_stream stream;
	char *outBuffer;
	SceUInt32 outBufferSize;
	char *compressedData;
	SceUInt32 compressedSize;
	SceUInt32 outLength;
	SceUInt32 outBase;
	SceUInt32 outPosition;
	SceUInt32 streamEnd;
} UdRamFile;

_Static_assert(sizeof(UdRamFile) == 84, "RAM file size");

/* name unknown: a firmware of the image, 180 bytes from the updater's heap */
typedef struct UdFirmInfo {
	SceUInt32 compression;
	SceUInt32 ownsData;
	SceUInt32 storedSize;
	SceUInt32 size;
	const SceUInt8 *data;
	SceUInt8 *loadedData;
	SceUInt32 position;
	UdRamFile ramFile;
	SceUInt32 opened;
	SceUInt32 secondary;
	/* the image's record */
	SceUInt32 type;
	SceUInt32 chunkIndex;
	char name[32];
	SceUInt32 offset;
	SceUInt32 recordSize;
	SceUInt32 recordStoredSize;
	SceUInt32 recordCompression;
	struct UdFirmInfo *next;
} UdFirmInfo;

_Static_assert(sizeof(UdFirmInfo) == 180, "firmware information size");
_Static_assert(offsetof(UdFirmInfo, type) == 120, "firmware record copy");

/* name unknown: the image's firmwares, in the order of their records */
typedef struct UdFirmList {
	SceUInt32 count;
	UdFirmInfo *first;
} UdFirmList;

/* name unknown: a carrier's two firmwares */
typedef struct UdCarrier {
	SceUInt32 id;
	UdFirmInfo *firm[2];
	SceUInt32 update;
	SceUInt32 written;
} UdCarrier;

/* name unknown: the firmware of one type the update writes */
typedef struct UdFirmSlot {
	UdFirmInfo *firm;
	SceUInt32 update;
	SceUInt32 reserved;
} UdFirmSlot;

/* name unknown: a file the update deletes on the modem */
typedef struct UdDeleteEntry {
	SceUInt32 kind;
	SceUInt32 buildId[4];
	SceUInt32 nameLength;
	char name[36];
} UdDeleteEntry;

_Static_assert(sizeof(UdDeleteEntry) == 60, "delete entry size");

/* name unknown: the QMDM6200 updater's image work area (firmware lists and delete list) */
typedef struct UdQmdm6200Work {
	UdFirmList firmList;
	SceUInt32 carrierCount;
	UdCarrier *carriers;
	UdFirmSlot enprgHex;
	UdFirmSlot partition;
	UdFirmSlot dbl;
	UdFirmSlot osbl;
	UdFirmSlot safe;
	SceUInt32 hasFirmImages;
	SceUInt32 hasCarrierImages;
	SceUInt32 deleteCount;
	SceUInt32 deleteRemaining;
	UdDeleteEntry *deleteList;
	UdFirmInfo *backupFirm;
	SceUInt32 backupSaved;
	SceUInt32 backupRestored;
	SceUInt32 updateAllRequested;
	SceUInt32 updateAllMode;
} UdQmdm6200Work;

/* name unknown: the firmwares udGetFirmImages_QMDM6200 opens */
typedef struct UdFirmImages {
	UdFirmInfo *enprgHex;
	UdFirmInfo *partition;
	UdFirmInfo *dbl;
	UdFirmInfo *osbl;
	UdFirmInfo *safe;
} UdFirmImages;

/* name unknown: the carrier udGetCarrierImages_QMDM6200 opens */
typedef struct __attribute__((packed)) UdCarrierImages {
	SceUInt32 id;
	UdFirmInfo *firm[2];
} UdCarrierImages;

_Static_assert(sizeof(UdQmdm6200Work) == 0x74, "updater work area size");

/* name unknown: a firmware the modem reports, in the lists _udGetSubListOfFirmList returns */
typedef struct UdModemFirm {
	SceUInt32 status;
	SceUInt32 slot;
	SceUInt32 reserved;
	SceUInt32 buildId[4];
	SceUInt32 nameLength;
	char name[36];
} UdModemFirm;

/* name unknown: one of the modem's firmware lists (0: AMSS, 1: UQCN) */
typedef struct UdModemFirmSubList {
	SceUInt32 count;
	UdModemFirm firm[5];
	SceUInt32 reserved;
} UdModemFirmSubList;

/* name unknown: the firmwares the modem reports */
typedef struct UdModemFirmList {
	SceUInt32 reserved;
	UdModemFirmSubList subList[2];
} UdModemFirmList;

_Static_assert(sizeof(UdModemFirm) == 68, "modem firmware size");
_Static_assert(offsetof(UdModemFirmList, subList[1]) == 352, "modem UQCN list");

/* name unknown: what the modem reports of itself (the boot loaders' versions, its firmwares) */
typedef struct UdModemInformation {
	SceUInt32 reserved;
	SceUInt32 dblVersion;
	SceUInt32 osblVersion;
	SceUInt32 padding;
	UdModemFirmList *firmList;
} UdModemInformation;

/* name unknown: the client the updater asks for more image data */
typedef struct UdClient {
	int (*requestChunk)(SceUInt32 size);
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
	SceUInt32 chunkIndex;
	SceUInt32 chunkCount;
	const SceUInt8 *imageData;
	SceUInt32 imageSize;
	int (*requestChunk)(SceUInt32 size);
} UdContext;

_Static_assert(sizeof(UdContext) == 0xa4, "updater context size");
_Static_assert(offsetof(UdContext, chunkIndex) == 0x90, "updater received bytes");

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

/* updater/updater_QMDM6200.c; names unknown except Sony's */
int udParseImage_QMDM6200(const SceUInt8 **cursor, SceUInt32 *length);
void udReleaseImage_QMDM6200(UdContext *context, UdQmdm6200Work *work);
int udPrepareImage_QMDM6200(SceUInt32 configurationMode, UdQmdm6200Work *work);
int udResetDeleteList_QMDM6200(void);
int _udCompareVersionCarrierImage(UdQmdm6200Work *work, UdModemFirmList *list);
int udCheckUpdateNoNeed_QMDM6200(const UdModemInformation *information);
int udSetUpdateFirmImages_QMDM6200(void);
int udSetUpdateCarrierImages_QMDM6200(void);
int udIsFirmUpdateNeeded_QMDM6200(void);
int udIsCarrierUpdatePending_QMDM6200(void);
int udIsUpdateAllRequested_QMDM6200(void);
int udIsBackupRestoreNeeded_QMDM6200(void);
int udIsImageVersionChanged_QMDM6200(SceUInt32 version);
int udCreateBackupImage_QMDM6200(void);
int udSetBackupSaved_QMDM6200(void);
int udSetBackupRestored_QMDM6200(void);
int udReceiveImageChunk_QMDM6200(void);
int udGetFirmImages_QMDM6200(UdFirmImages *images);
int udGetCarrierImages_QMDM6200(UdCarrierImages *images);
int udGetBackupImage_QMDM6200(UdFirmInfo **image);
int udGetWriteSize_QMDM6200(void);
int udGetDeleteSize_QMDM6200(void);
int udSetCarrierWritten_QMDM6200(SceUInt32 id);
int udCreateDeleteList(UdModemFirmList *list);
int udGetDeleteEntry_QMDM6200(UdDeleteEntry *entry);
int udReleaseDeleteList_QMDM6200(void);
int udRequestUpdateAll_QMDM6200(SceUInt32 mode);
int udIsUpdateNeeded_QMDM6200(SceUInt32 *needed);
SceUID udRamIoOpen_QMDM6200(const char *path, int flags, SceMode mode);
int udRamIoClose_QMDM6200(SceUID fd);
SceOff udRamIoLseek_QMDM6200(SceUID fd, SceOff offset, int whence);
int udRamIoRead_QMDM6200(SceUID fd, void *data, SceSize size);
int udRamIoWrite_QMDM6200(SceUID fd, const void *data, SceSize size);
SceOff udRamIoGetSize_QMDM6200(SceUID fd);
int udRamFileOpen(UdContext *context, UdRamFile *file, char *data, SceUInt32 size);
int udRamFileClose(UdContext *context, UdRamFile *file);
SceOff udRamFileSeek(UdRamFile *file, SceOff offset);
int udRamFileRead(UdRamFile *file, void *data, SceSize size);

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
int udSetImageChunk(SceUInt32 chunkCount, SceUInt32 chunkIndex, const void *data,
		     SceUInt32 size);
int udSetTarget(SceUInt32 hardwareType, SceUInt32 configurationMode);
int udLoadImage(void);
int udResetDeleteList(void);
int udCheckUpdateNoNeed(const SceUInt8 *information);
int udReceiveImageChunk(SceUInt32 chunkCount, SceUInt32 chunkIndex, const void *data,
		       SceUInt32 size);
int udGetWriteSize(void);
int udGetDeleteSize(void);
int udGetImageVersion(SceUInt32 *version);
int udIsInitialized(void);
int udEnableRamIo(void);
int udDisableRamIo(void);
int udWaitImageChunk(SceUInt32 chunkIndex);
int udIsUpdateNeeded(SceUInt32 *needed);
int udReadFileInformation(SceUID fd, UdFileInformation *information);

/* open, close and size an update file in RAM or through iofilemgr */
SceUID udIoOpen(const char *path, int flags, SceMode mode);
int udIoClose(SceUID fd);
SceOff udIoLseek(SceUID fd, SceOff offset, int whence);
int udIoRead(SceUID fd, void *data, SceSize size);
int udIoWrite(SceUID fd, const void *data, SceSize size);
SceOff udIoGetSize(SceUID fd);
