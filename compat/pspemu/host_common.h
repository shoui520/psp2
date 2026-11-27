/*
 * host_common.h: shared structures, constants, and cross-module interfaces
 * for ScePspemu (vs0:app/NPXS10028/eboot.bin).
 */
#ifndef PSPEMU_HOST_COMMON_H
#define PSPEMU_HOST_COMMON_H

#include <scetypes.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ctrl.h>
#include <display.h>
#include <appmgr.h>
#include <apputil.h>
#include <compat/compat.h>
#include <kernel/threadmgr.h>
#include <kernel/iofilemgr.h>
#include <kernel/processmgr.h>
#include <kernel/sysmem.h>
#include <kernel/libkernel.h>
#include <modulemgr/modulemgr_syscall.h>
#include <dmacmgr/dmacmgr.h>
#include <power/power.h>
#include <screenshot.h>
#include <vshbridge/vshbridge.h>
#include <libmd5.h>
#include <ces/libces_jis.h>
#include <audioout.h>
#include <audioin.h>
#include <avcdec.h>
#include <audiodec.h>
#include <scejpeg.h>
#include <rtc.h>

#ifdef __cplusplus
extern "C" {
#endif

/* POSIX / Vita error codes used by ScePspemu */
#define PSPEMU_ERROR_KERNEL_NO_ENTRY    ((int)0x80000202)
#define PSPEMU_ERROR_NO_ENTRY           ((int)0x80010002)
#define PSPEMU_ERROR_NO_MEMORY          ((int)0x8001000C)
#define PSPEMU_ERROR_ACCESS_DENIED      ((int)0x8001000D)
#define PSPEMU_ERROR_FAULT              ((int)0x8001000E)
#define PSPEMU_ERROR_ALREADY_EXISTS     ((int)0x80010011)
#define PSPEMU_ERROR_INVALID_ARG        ((int)0x80010016)
#define PSPEMU_ERROR_TOO_MANY_FILES     ((int)0x80010018)
#define PSPEMU_ERROR_NO_SPACE           ((int)0x8001001C)
#define PSPEMU_ERROR_CORRUPTED          ((int)0x8001002F)
#define PSPEMU_ERROR_NOT_SUPPORTED      ((int)0x80010086)
#define PSPEMU_ERROR_FATAL              ((int)0x80010089)
#define PSPEMU_ERROR_QUIT_REQUEST       ((int)0x8001008C)

/* AppMgr event IDs handled by ScePspemu */
#define PSPEMU_APPMGR_EVENT_RESUME_START   0x10000001U
#define PSPEMU_APPMGR_EVENT_RESUME_FINISH  0x10000003U
#define PSPEMU_APPMGR_EVENT_QUIT           0x20000001U
#define PSPEMU_APPMGR_EVENT_APP_PARAM      0x20000004U

/* Memory block types used by ScePspemu */
#define PSPEMU_MEMBLOCK_CDRAM_RW           0x09408060U
#define PSPEMU_MEMBLOCK_PHYCONT_NC_RW      0x0C208060U
#define PSPEMU_MEMBLOCK_PHYCONT_RW         0x0C20D060U

/* PSP guest memory map offsets relative to uncached CDRAM base */
#define PSPEMU_RAM_USB_WORK_OFFSET         0x00300000U
#define PSPEMU_RAM_SYS_SETTINGS_OFFSET     0x003FF000U
#define PSPEMU_RAM_BOOT_BLOCK_OFFSET       0x01FF0000U
#define PSPEMU_RAM_FLASH_WORK_OFFSET       0x03000000U
#define PSPEMU_RAM_POPS_TITLE_OFFSET       0x03D13F80U
#define PSPEMU_RAM_POPS_DISC_OFFSET        0x03D13FC0U
#define PSPEMU_RAM_CDRAM_SIZE              0x04000000U

/* PSP guest virtual address of user memory start checked by integrity thread */
#define PSPEMU_GUEST_USER_BASE             0x08804000U
#define PSPEMU_MEMCHECK_MAX_SIZE           0x01800000U

/* Framebuffer geometry */
#define PSPEMU_SCREEN_WIDTH                480U
#define PSPEMU_SCREEN_HEIGHT               272U
#define PSPEMU_SCREEN_PITCH                512U
#define PSPEMU_SCREEN_STRIDE_BYTES         (PSPEMU_SCREEN_PITCH * 4U)
#define PSPEMU_SCREEN_ROW_BYTES            (PSPEMU_SCREEN_WIDTH * 4U)
#define PSPEMU_FRAMEBUF_CLEAR_SIZE         0x00088000U

/* SceCompat shared 32-bit register slots */
#define PSPEMU_SHARED_POWER_TICK_SEQ_A     4
#define PSPEMU_SHARED_POWER_TICK_SEQ_B     5
#define PSPEMU_SHARED_POWER_TICK_KIND      6
#define PSPEMU_SHARED_HEARTBEAT_COUNTER    7

/* SceCompat status values */
#define PSPEMU_COMPAT_STATUS_STOPPED       0
#define PSPEMU_COMPAT_STATUS_SUSPENDED     1
#define PSPEMU_COMPAT_STATUS_RUNNING       2

/* SceCompat boot API types and boot modes */
#define PSPEMU_API_TYPE_UMD_ISO            0x123U
#define PSPEMU_API_TYPE_MS_PBP             0x124U
#define PSPEMU_API_TYPE_POPS               0x144U
#define PSPEMU_BOOT_MODE_POPS              4U
#define PSPEMU_BOOT_MODE_PSP               7U

/* SceCompat cache operations */
#define PSPEMU_COMPAT_CACHE_WRITEBACK      2

/* Title list binary format (ur0:emu_list/title_list.bin) */
#define PSPEMU_TITLE_LIST_PATH             "ur0:emu_list/title_list.bin"
#define PSPEMU_TITLE_LIST_SIZE             0x64U
#define PSPEMU_TITLE_LIST_COUNT            8
#define PSPEMU_TITLE_ID_LEN                9

/* VMC buffer geometry */
#define PSPEMU_VMC_WORK_SIZE               122880
#define PSPEMU_VMC_SLOT_SIZE               131200

/* PBP header (0x28 bytes) */
typedef struct PspPbpHeader {
	uint32_t magic;
	uint32_t version;
	uint32_t paramSfoOffset;
	uint32_t iconPngOffset;
	uint32_t iconPmfOffset;
	uint32_t picOverlayOffset;
	uint32_t picBackOffset;
	uint32_t sndAt3Offset;
	uint32_t dataPspOffset;
	uint32_t dataPsarOffset;
} PspPbpHeader;

/* PS1 single-disc header at dataPsarOffset ("PSISOIMG0000") */
typedef struct PsIsoImgHeader {
	uint32_t magicWords[3];
	uint32_t startDatOffsetLo;
	uint32_t startDatOffsetHi;
	uint8_t  remainder[0x400 - 20];
} PsIsoImgHeader;

/* PS1 multi-disc header at dataPsarOffset ("PSTITLEIMG000000") */
typedef struct PsTitleImgHeader {
	uint32_t magicWords[4];
	uint32_t startDatOffsetLo;
	uint32_t startDatOffsetHi;
	uint32_t discMeta[4];
	uint8_t  remainder[0x400 - 40];
} PsTitleImgHeader;

typedef union PsImgHeader {
	PsIsoImgHeader   singleDisc;
	PsTitleImgHeader multiDisc;
} PsImgHeader;

/* STARTDAT splash image table (0x50 bytes) */
typedef struct PspStartDatImage {
	uint32_t offset;
	uint32_t size;
} PspStartDatImage;

typedef struct PspStartDatHeader {
	char             magic[8];
	uint32_t         version;
	uint32_t         imageCount;
	PspStartDatImage images[8];
} PspStartDatHeader;

/* PSP NPUMD header at dataPsarOffset (0x40 bytes) */
typedef struct PspNpUmdHeader {
	char     magic[8];
	uint32_t licenseFlags;
	uint32_t blockSize;
	char     contentId[36];
	uint8_t  tail[12];
} PspNpUmdHeader;

/* PSP SceGameInfo (0xDC bytes, matches PSP sysmem_kernel.h) */
typedef struct PspemuGameInfo {
	uint32_t size;
	uint32_t flags;
	char     umdDataString[16];
	char     expectUmdData[16];
	char     qtgpTwo[8];
	char     qtgpThree[16];
	uint32_t allowReplaceUmd;
	char     sfoProductString[16];
	uint32_t sfoParentalLevel;
	char     vshVersion[8];
	uint32_t umdCacheOn;
	uint32_t compiledSdkVersion;
	uint32_t compilerVersion;
	uint32_t dnas;
	uint32_t utilityLocation;
	char     vshBootfilename[64];
	char     sfoGameDataId[16];
	char     sfoAppVer[8];
	uint8_t  subscriptionValidity[8];
	uint32_t sfoBootable;
	uint32_t sfoOpnssmpVer;
} PspemuGameInfo;

/* Argument passed to ScePspemuMain (0xE0 bytes) */
typedef struct PspemuMainThreadArg {
	PspemuGameInfo gameInfo;
	SceUID         titleInfoThreadId;
} PspemuMainThreadArg;

/* Boot parameter block in PSP RAM at 0x01FF0000 */
typedef struct PspemuBootBlock {
	uint32_t       apiType;
	uint32_t       bootMode;
	char           execPath[64];
	char           umdImagePath[64];
	char           bootPath[64];
	uint8_t        paramSfo[0x1400];
	uint32_t       paramSfoSize;
	PspemuGameInfo gameInfo;
} PspemuBootBlock;

/* System settings block shared with PSP guest at 0x003FF000 (0x198 bytes) */
typedef struct PspemuSystemSettings {
	uint32_t language;
	uint32_t buttonAssign;
	uint32_t regionCode;
	uint32_t regionDefault;
	uint32_t timeFormat;
	uint32_t dateFormat;
	uint32_t summerTime;
	int32_t  timeZoneOffset;
	uint32_t adhocChannel;
	uint32_t regionFormatA;
	uint32_t regionFormatB;
	uint32_t regionFormatC;
	uint32_t npNavOnly;
	int32_t  npAdClockDiff;
	uint32_t npGeoFiltering;
	uint32_t npYob;
	uint32_t npMob;
	uint32_t npDob;
	uint32_t parentalContentStartControl;
	uint32_t parentalBrowserStartControl;
	char     nickname[0x80];
	char     timeZoneName[64];
	char     npEnv[9];
	char     npAccountIdHex[16];
	char     npLoginId[65];
	char     npPassword[31];
	char     npLang[3];
	char     npCountry[3];
	char     parentalPasscode[4];
	char     adhocSsidPrefix[4];
	uint8_t  pad;
} PspemuSystemSettings;

/* Title list records (12-byte current format and 10-byte legacy format) */
typedef struct PspemuTitleRecord {
	char titleId[9];
	char flagsChar;
	char statusChar;
	char separator;
} PspemuTitleRecord;

typedef struct PspemuLegacyTitleRecord {
	char titleId[9];
	char separator;
} PspemuLegacyTitleRecord;

/* Lightweight PNG decoder header parsed from STARTDAT */
typedef struct PspemuPngInfo {
	uint32_t width;
	uint32_t height;
	uint32_t pixelFormat;
} PspemuPngInfo;

/* User-mode SDK / internal functions not yet declared in shared headers */
#define SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN          0x00000001U
#define SCE_SHELL_UTIL_LOCK_TYPE_USB_CONNECTION  0x00000010U

#define SCE_COMMON_DIALOG_STATUS_NONE            0
#define SCE_COMMON_DIALOG_STATUS_RUNNING         1
#define SCE_COMMON_DIALOG_STATUS_FINISHED        2

#define SCE_SYSMODULE_HTTP                       0x0002U
#define SCE_SYSMODULE_LOCATION                   0x0014U
#define SCE_SYSMODULE_PHOTO_EXPORT               0x0016U
#define SCE_SYSMODULE_SCREEN_SHOT                0x001DU
#define SCE_SYSMODULE_NET_ADHOC_MATCHING         0x002AU
#define SCE_SYSMODULE_LOCATION_EXTENSION         0x004BU

#define SCE_SYSMODULE_INTERNAL_JPEG_ENC_ARM      0x80000001U
#define SCE_SYSMODULE_INTERNAL_AUDIOCODEC        0x80000002U
#define SCE_SYSMODULE_INTERNAL_JPEG_ARM          0x80000003U
#define SCE_SYSMODULE_INTERNAL_G729              0x80000004U
#define SCE_SYSMODULE_INTERNAL_MUSIC_EXPORT      0x8000001FU
#define SCE_SYSMODULE_INTERNAL_VIDEO_EXPORT      0x80000020U
#define SCE_SYSMODULE_INTERNAL_LOCATION_INTERNAL 0x80000028U

int sceSysmoduleLoadModule(uint32_t id);
int sceSysmoduleLoadModuleInternal(uint32_t id);

int sceDisplaySetFrameBuf(const SceDisplayFrameBuf *pParam, int sync);
int sceDisplayWaitVblankStartCB(void);

int sceCtrlSetSamplingMode(int mode);
int sceCtrlPeekBufferPositive(int port, SceCtrlData *padData, int count);

int sceShellUtilInitEvents(int mode);
int sceShellUtilLock(uint32_t type);
int sceShellUtilUnlock(uint32_t type);
int sceShellUtilRegisterEventHandler(int (*handler)(int result, int mode, int type, void *userArg), void *userArg);
int SceShellUtil_EC5881A5(void);
int SceShellUtil_CE35B2B8(void *callback, void *userArg);

int sceAppMgrAcquireSoundOutExclusive3(int mode);
int sceAppMgrReleaseSoundOutExclusive3(void);
int vshSblQafMgrIsAllowPSPEmuSelectNPEnv(void);
int vshSblQafMgrIsAllowPSPEmuScreenShot(void);

typedef int (*PspemuImeTextFilter)(uint16_t *outText, uint32_t *outTextLength,
                                   const uint16_t *srcText, uint32_t srcTextLength);

typedef struct PspemuImeDialogParam {
	uint32_t            sdkVersion;
	uint32_t            inputMethod;
	uint64_t            supportedLanguages;
	int                 languagesForced;
	uint32_t            type;
	uint32_t            option;
	PspemuImeTextFilter filter;
	uint32_t            dialogMode;
	uint32_t            textBoxMode;
	const uint16_t     *title;
	uint32_t            maxTextLength;
	uint16_t           *initialText;
	uint16_t           *inputTextBuffer;
	uint8_t             commonParamReserved[0x48];
	uint32_t            commonParamMagic;
	uint8_t             tailReserved[0x24];
} PspemuImeDialogParam;

typedef struct PspemuImeDialogResult {
	int32_t result;
	int32_t button;
	uint8_t reserved[28];
} PspemuImeDialogResult;

int sceImeDialogInit(const PspemuImeDialogParam *param);
int sceImeDialogAbort(void);
int sceImeDialogGetResult(PspemuImeDialogResult *result);
int sceImeDialogTerm(void);

SceUInt64 sceRtcGetAccumulativeTime(void);
uint16_t *wcsncpy(uint16_t *dst, const uint16_t *src, size_t count);
int sceCompatGetCurrentSecureTick(SceRtcTick *tick);

int SceCamera_99B5FB62(unsigned int mode);

int SceCommonDialog_FA26EB30(void); /* sceCompatDialogGetStatus */
int SceCommonDialog_A68A44EA(void); /* sceCompatDialogAbort */
int SceCommonDialog_ED2F1495(void); /* sceCompatDialogResume */
int SceCommonDialog_500229F0(void); /* scePocketStationDialogGetStatus */
int SceCommonDialog_7030E4A6(void); /* scePocketStationDialogAbort */
int scePhotoImportDialogGetStatus(void);
int scePhotoImportDialogAbort(void);
int sceImeDialogGetStatus(void);

int sceAppMgrAcquireBgmPort(void);
int sceAppMgrExitToLiveboardForGameApp(void);
int sceAppMgrGetAppParam(char *param);
int sceAppMgrWorkDirMount(uint32_t id, char *mountPoint);

SceUInt32 vshKernelCheckModelCapability(int capability);

SceInt32 sceKernelAtomicGetAndSet8(volatile SceInt8 *ptr, SceInt8 value);
SceUInt32 sceKernelGetProcessTimeLow(void);
int sceKernelPowerUnlock(int type);
int sceClibStrncasecmp(const char *sLeft, const char *sRight, SceSize len);
int sceAppUtilMusicMount(void);

int sceCesMbcsStrGetUtf8Len(SceCesMbcsUcsContext *context,
                            const uint8_t *mbcsStr, uint32_t mbcsMax, uint32_t *mbcsLen,
                            uint32_t *utf8Len);
int sceCesMbcsStrToUtf8Str(SceCesMbcsUcsContext *context,
                           const uint8_t *mbcsStr, uint32_t mbcsMax, uint32_t *mbcsLen,
                           uint8_t *utf8Buf, uint32_t utf8Max, uint32_t *utf8Len);

/* Shared paths owned by remote_msfs.c */
extern char g_msfsRootPrefix[16];
extern char g_originalPath[1024];
extern char g_selfPath[1024];

/* Functions exported by main.c */
int pspemuIsPocketStationLaunch(void);
int pspemuFinishSplashAndLockModules(void);
int pspemuDrawDebugColorSquare(uint32_t color, int columnOffset);
int pspemuIsCachedCdramAddress(uintptr_t addr);
void *pspemuGetActiveFrameBuffer(void);
int pspemuSetUseTempFrameBuffer(int useTemp);
int pspemuConfirmPocketStationFirstBoot(void);
int pspemuIsPocketStationEnabled(void);
int pspemuIsPocketStationInstalled(void);
int pspemuIsPs1Mode(void);
int pspemuLoadSysmodules(void);
void pspemuTriggerFatalError(int errorCode) __attribute__((noreturn));
int pspemuIsRebootReady(void);
int pspemuSetRebootReady(void);
int pspemuPowerUnlock(void);
int pspemuPowerLock(void);
int pspemuRequestSuspend(void);
void pspemuApplyTitlePatchAudioDelayAlt(void);
void pspemuApplyTitlePatchAudioDelay(void);
void pspemuApplyTitlePatchReturnZero(void);
void pspemuApplyTitlePatchReturnZeroAlt(void);
void pspemuApplyTitlePatchClearCallbacks(void);
void pspemuApplyTitlePatchMemoryHook(void);
void pspemuApplyTitlePatchFrameSync(void);
void pspemuApplyTitlePatchShiftPrologue(void);
void pspemuApplyTitlePatchShiftPrologueWithHook(void);
void pspemuApplyTitlePatchBranchTrampoline(void);
void pspemuApplyTitlePatchBranchTrampolineAlt(void);
void pspemuApplyAvcPatchReturnZero(void);
void pspemuApplyAvcPatchReturnZeroAlt(void);
void pspemuApplyTitlePatchRelocStub(void);
void pspemuApplyTitlePatchRelocStubAlt(void);
void pspemuApplyTitlePatchStackCallStub(void);
void pspemuApplyTitlePatchInlineJump(void);
void pspemuApplyTitlePatchInlineJumpAlt(void);
int pspemuMountPs1VmcSlots(void);
int pspemuCopyPopsTitleStrings(void);
int pspemuGetPopsTitleStrings(const char **outTitle, const char **outDisc);
SceSSize pspemuRecordTitleListFlags(unsigned int flagBits);
SceSSize pspemuSyncTitleList(void);
int sceCompatMain(SceSize argLen, void *argp);
int pspemuValidatePathPrefix(const char *path, unsigned int pathLen, unsigned int maxLen, const char *prefix);
int pspemuHasCameraCapability(void);
int pspemuIsVitaModel(void);
int pspemuHasRearTouchCapability(void);

/* Cross-module callees from main.c */
/* Lightweight PNG decoder (0x8100186c..0x81002747) */
int pspemuPngDecodeImage(void *outBuf, uint32_t outSize, const void *pngData, uint32_t pngSize,
                         uint32_t *outWidth, uint32_t *outHeight, uint32_t *outFormat);
int pspemuPngConvertToAbgr(void *dstBuf, const void *srcBuf);

/* Kermit IPC request header in PSP shared RAM */
typedef struct PspemuKermitRequest {
	uint16_t cmd;
	uint16_t pad;
	uint32_t semaId;
	uint32_t retAddr;
	uint32_t padWord;
	uint64_t args[13];
} PspemuKermitRequest;

/* Virtual filesystem operations table for compatFsLoop */
typedef struct PspemuFsOps {
	int (*init)(void *arg);
	int (*exit)(void *arg);
	int (*open)(const char *path, int flags, SceMode mode);
	int (*close)(SceUID fd);
	SceSSize (*read)(SceUID fd, void *buf, SceSize size);
	SceSSize (*write)(SceUID fd, const void *buf, SceSize size);
	SceOff (*lseek)(SceUID fd, SceOff offset, int whence);
	int (*ioctl)(SceUID fd, unsigned int cmd, void *inData, SceSize inLen, void *outData, SceSize outLen);
	int (*remove)(const char *path);
	int (*mkdir)(const char *path, SceMode mode);
	int (*rmdir)(const char *path);
	SceUID (*dopen)(const char *path);
	int (*dclose)(SceUID dfd);
	int (*dread)(SceUID dfd, SceIoDirent *dir);
	int (*getstat)(const char *path, SceIoStat *stat);
	int (*chstat)(const char *path, const SceIoStat *stat, int bits);
	int (*rename)(const char *oldPath, const char *newPath);
	int (*chdir)(const char *path);
	int (*devctl)(const char *dev, unsigned int cmd, void *inData, SceSize inLen, void *outData, SceSize outLen);
	int (*postOp)(void);
} PspemuFsOps;

/* remotefs.c */
int pspemuIsAlternateDisplayHead(void);
void *remotefsSetCdramBase(void *activeCdram, void *cachedCdram, void *uncachedCdram);
void *remotefsGetCdramBase(void);
void *remotefsTranslateGuestAddr(uint32_t guestAddr, int cacheMode, SceSize size);
void *remotefsTranslateUserGuestAddr(uint32_t guestAddr, int cacheMode, SceSize size);
int remotefsFlushGuestAddr(void *hostPtr, SceSize size);
int remotefsWaitAndGetRequest(int mode, PspemuKermitRequest **outReq);
int remotefsReturnValue(int mode, const PspemuKermitRequest *req, int64_t retVal);
int remotefsConvertLocalToUtc(SceDateTime *dateTime);
int remotefsConvertUtcToLocal(SceDateTime *dateTime);
void compatFsLoop(int channel, const PspemuFsOps *ops) __attribute__((noreturn));

/* remote_msfs.c */
int remoteMsfsSetBootAllowed(int allowed);
int remoteMsfsCheckMusicPath(const char *path, int flags);
int remoteMsfsCloseCachedEboot(void);
void *remoteMsfsSuspendReset(void);
int remoteMsfsParseAppParam(char *discId, uint32_t *parentalLevel, char *gamedataId,
                            char *appVer, uint32_t *bootable, int *isPs1Mode, int *isPocsLaunch);
int remoteMsfsCheckPocketStationParam(void);
int remoteMsfsPreInit(void);
int remoteMsfsInit(void);

/* remote_flashfs.c */
int remoteFlashfsClearGuest(void);
int remoteFlashfsReset(void);
int remoteFlashfsInit(void *workBuf);

/* iocache.c */
SceSSize iocacheRead(SceUID fd, void *buf, SceSize size);
SceUID iocacheOpenBackingFile(const char *path, int flags, SceMode mode);
int iocacheRequestClose(void);
int iocacheInit(uint32_t cacheSize);
int iocachePrefetchRange(SceOff offset, uint32_t length);
int iocacheFileInit(SceSize totalSize, SceSize slotSize);
SceSSize iocacheFileLoad(const char *path, int flags, SceMode mode, SceUID fd);
int iocacheFileIsCached(SceUID fd);
int iocacheFileCanCache(const char *path, int flags, SceMode mode);
SceSSize iocacheFileRead(SceUID fd, void *buf, SceSize size);
SceOff iocacheFileLseek(SceUID fd, SceOff offset, int whence);
int iocacheFileClose(SceUID fd);

/* Additional cross-module helpers used by FS & iocache */
int pspemuLockLwMutexOrFatal(SceKernelLwMutexWork *work);
int pspemuUnlockLwMutexOrFatal(SceKernelLwMutexWork *work);
int remoteNetIsThrottleActive(void);
int remotePeripheralIsKernelOpenAllowed(void);
int remoteLowPriorityIsPromotablePath(const char *path);
int remoteLowPriorityEnqueuePromote(const char *path);
int pspemuCompatIsRestrictedTitle(void);
int pspemuSettingsIsClearMediaEnabled(void);
int pspemuSettingsGetEmuListFlag(void);
int pspemuSettingsSetEmuListFlag(void);
extern int g_compatMsfsSeekFlag;

/* remote_audioout.c & remote_audioin.c */
typedef struct PspemuAudioResampler {
	int      consumedFrames;
	int      producedFrames;
	uint32_t phaseFrac;
	uint32_t phaseStep;
	int      srcRate;
	int      dstRate;
	int      ringReadIdx;
	int      ringWriteIdx;
	int16_t  ringBuf[64][2];
} PspemuAudioResampler;

extern int g_audioFadeStep;
extern int g_audioFadeShift;
extern int g_audioFadeDir;
extern int g_audioFadeHoldCount;

int remoteAudioOutInit(void);
int remoteAudioOutSuspend(int suspend);
uint32_t audioResamplerInit(PspemuAudioResampler *resampler, int srcRate, int dstRate);
int audioResamplerProcess(PspemuAudioResampler *resampler, const int16_t *inSamples, int inFrames,
                          int16_t *outSamples, int maxOutFrames, unsigned int volume);
int remoteAudioInInit(void);

/* Vita internal codec imports used by Milestone 4 */
typedef struct SceAvcdecPictureInternal {
	SceSize              size;
	SceUInt32            pixelType;
	SceUInt32            framePitch;
	SceUInt32            frameWidth;
	SceUInt32            frameHeight;
	SceUInt32            horizontalSize;
	SceUInt32            verticalSize;
	SceUInt32            frameCropLeftOffset;
	SceUInt32            frameCropRightOffset;
	SceUInt32            frameCropTopOffset;
	SceUInt32            frameCropBottomOffset;
	SceAvcdecFrameOption opt;
	void                *pPicture[2];
	void                *yPlane;
	void                *cbPlane;
	void                *crPlane;
	void                *extraPlanes[3];
	SceAvcdecInfo        info;
} SceAvcdecPictureInternal;

int sceAvcdecCscInternal(const SceAvcdecPictureInternal *dstPic, const SceAvcdecPictureInternal *srcPic);
int sceAvcdecDecodeAuInternal(const SceAvcdecCtrl *ctrl, const SceAvcdecAu *au, int *outStatus);
int sceAvcdecDecodeGetPictureInternal(const SceAvcdecCtrl *ctrl, SceAvcdecArrayPicture *arrayPic, SceUInt *opt);
int sceAvcdecGetSeiPictureTimingInternal(const SceAvcdecCtrl *ctrl, int index, void *outSei);
int sceAvcdecRegisterCallbackInternal(const SceAvcdecCtrl *ctrl, SceUID cbid, int mode);
int sceAvcdecUnregisterCallbackInternal(const SceAvcdecCtrl *ctrl);
int sceAvcdecSetDecodeModeInternal(uint32_t codecType, int mode);
int sceVideodecSetConfigInternal(uint32_t codecType, int config);

int sceJpegArmDecodeMJpegYCbCr(const SceUInt8 *pJpeg, SceSize isize, SceUInt8 *pYCbCr, SceSize osize,
                               int decodeMode, void *pCoefBuffer, SceSize coefBufferSize);

int SceAudiocodec_E2AF3A54(void *work, int codecType);
int SceAudiocodec_1F9A3722(void *work, int codecType);
int SceAudiocodec_E827E04F(void *work, int codecType);

int sceG729EncodeGetContextSize(void);
int sceG729DecodeGetContextSize(void);
int sceG729EncodeInit(void *ctx, unsigned int mode, unsigned int flags);
int sceG729EncodeCore(void *ctx, const void *pcmIn, void *bitstreamOut);
int sceG729DecodeInit(void *ctx);
int sceG729DecodeCore(void *ctx, int frameType, int frameLen, const void *bitstreamIn, void *pcmOut);

/* remote_me.c, remote_me_avcdec.c, remote_me_jpegdec.c, me_edram.c, me_wrapper_api.c */
int remoteMeInit(void);
int remoteMeAtxDispatch(int cmd, uint32_t argA, uint32_t argB, uint32_t argC, uint32_t argD, uint32_t argE);
int remoteMeAt3Dispatch(int cmd, uint32_t argA, uint32_t argB, uint32_t argC, uint32_t argD, uint32_t argE);
int remoteMeMp3Dispatch(int cmd, uint32_t argA, uint32_t argB, uint32_t argC, uint32_t argD);
int remoteMeAacDispatch(int cmd, uint32_t argA, uint32_t argB, uint32_t argC, uint32_t argD, uint32_t argE);
int remoteMeG729Dispatch(int cmd, uint32_t argA, uint32_t argB, uint32_t argC, uint32_t argD, uint32_t argE);
int remoteMeMixerDispatch(int cmd, uint32_t argA, uint32_t argB, uint32_t argC);
int remoteMeSasDispatch(int cmd, uint32_t argA, uint32_t argB, uint32_t argC, uint32_t argD, uint32_t argE, uint32_t argF);

int remoteMeAvcdecNotifyResume(void);
int remoteMeAvcdecCsc(void *dstRgb, uint32_t dstSize, void *srcYuv, uint32_t *params);
int remoteMeAvcdecSetTimestamp(uint32_t ptsHi, uint32_t ptsLo, uint32_t dtsHi, uint32_t dtsLo);
int remoteMeAvcdecCopyFrameToHost(void *dstHost, void *srcEdram, uint32_t frameBytes);
int remoteMeAvcdecDispatch(int cmd, uint32_t argA, uint32_t argB, uint32_t argC, uint32_t argD, uint32_t argE);
int remoteMeAvcdecSetActive(int active);
int remoteMeAvcdecSetPatchFlag(int patchId);
int remoteMeAvcdecInit(void);
int remoteMeAvcdecUpscale(void *dstBuf, void *srcBuf);
int remoteMeAvcdecHasDecoded(void);
int remoteMeAvcdecZoomFrame(void *dstBuf, void *srcBuf, int zoomScale);

int remoteMeUnusedVideoDispatch(int cmd);
int remoteMeWorkDispatch(int cmd, uint32_t argA, uint32_t argB);
int remoteMeJpegInit(void);
int remoteMeJpegFinish(void);
int remoteMeJpegCsc(void *dstBuf, uint32_t dstSize, void *srcYuv, int pixelFormat, int width, int height, int pitch);
int remoteMeJpegDispatch(int cmd, uint32_t argA, uint32_t argB, uint32_t argC, uint32_t argD, uint32_t argE, uint32_t argF);
int remoteMeCscDispatch(int cmd, uint32_t argA, uint32_t argB, uint32_t argC, uint32_t argD, uint32_t argE, uint32_t argF, uint32_t argG, uint32_t argH);

int meEdramInit(uint32_t size);
void *meEdramGetBase(uint32_t offset);

int meWrapperApiInit(void);
int meAt3DecodeFrame(void *codecBuf);
int meAt3GetInternalError(uint32_t *outErr, void *workCtx);
int meAt3InitContext(uint32_t channelMode, uint32_t sampleRate, uint32_t frameBytes, void *workCtx, uint32_t outputMode);
int meAtxDecodeFrame(void *codecBuf);
int meAtxParseHeader(uint32_t *outSampleRate, uint32_t *outChannelMode, uint32_t *outFrameBytes, const uint8_t *headerBytes);
int meAtxGetOutputSize(uint32_t *outSize, const void *workCtx);
int meAtxInitContext(uint32_t sampleRate, uint32_t channelMode, uint32_t frameBytes, uint32_t outputMode, void *workCtx);
int meAtxSetChannels(const uint32_t *inChannels, void *workCtx);
int meAtxGetInternalError(uint32_t *outErr, void *workCtx);
int meMp3FindSyncAndParse(const uint8_t *esBuf, uint32_t *outBitOffset, void *outInfo, void *workCtx);
int meMp3DecodeFrame(void *codecBuf);
int meMp3SetFrameSize(uint32_t esSize, void *workCtx);
int meMp3ResetContext(void *workCtx);
int meAacInitContext(uint32_t sampleRate, void *workCtx);
int meAacDecodeFrame(const void *esBuf, uint32_t *outConsumed, void *pcmBuf, uint32_t *outPcmBytes, void *workCtx);
int meAacGetInternalError(uint32_t *outErr, void *workCtx);
int meAacSetSampleRate(uint8_t rateCode, void *workCtx);
int meAacSetOption(uint32_t optA, uint32_t optB, uint32_t optC, void *workCtx);
int meG729EncodeInit(uint32_t *workCtx, uint32_t mode, uint32_t flags);
int meG729EncodeCore(uint32_t *workCtx, const void *pcmIn, void *bitstreamOut);
int meG729DecodeInit(void *workCtx);
int meG729DecodeCore(void *workCtx, int frameType, int frameLen, const void *bitstreamIn, void *pcmOut);
int meAudioMixerMix(const void *mixerParams, void **inStreams, int16_t *outStereo, uint32_t streamCount, int sampleCount);
int meSasInit(void *ctx, uint32_t optA, uint32_t optB);
int meSasSetOutputSize(void *ctx, uint32_t grainSamples, uint32_t optA, uint32_t optB, uint32_t optC, uint32_t optD);
int meSasCopyBuffer(void *ctx, const void *srcBuf, void *dstBuf);
int meSasTerm(void *ctx, uint32_t optA, uint32_t optB);

/* Cross-module callees from Milestone 4 into Milestone 5 (dmacplus, sas2, scejpeg) */
int remoteDmacplusIsEnabled(void);
int remoteDmacplusSetEnabled(int enabled);
int remoteSas2MeDispatch(int cmd, uint32_t ctxGuestAddr, uint32_t outGuestAddr,
                         uint32_t argC, uint32_t argD, uint32_t argE);
int pspemuSas2CoreMix(void *ctx, void *outBuf, uint32_t argC);
int pspemuSas2CoreMixWithVoice(void *ctx, void *outBuf, uint32_t argC, uint32_t argD, uint32_t argE);
int pspemuSas2CoreInit(void *ctx);
int sceJpegMjpegCsc(void *dstRgb, const void *srcYuv, uint32_t widthHeight, int pitch, int pixelFormat, int yuvFormat);
void *sceJpegWorkReset(void);
void *sceJpegWorkAlloc(uint32_t size);
int sceJpegWorkFree(void *hostPtr);
int sceJpegWorkGetFreeSize(void);
void *sceJpegWorkAllocZero(uint32_t size);
int sceJpegWorkHostToGuest(void *hostPtr, uint32_t *outGuestAddr);
int sceJpegWorkGuestToHost(uint32_t guestAddr, void **outHostPtr);
int sceJpegDecInit(void *ctx, const void *srcBuf, uint32_t srcSize, uint32_t widthHeight, int opt);
int sceJpegDecDecode(void *ctx);
int sceJpegDecFinish(void);
int sceJpegDecGetOutputInfo(const void *srcBuf, uint32_t srcSize, void *outYuv, uint32_t outSize, int mode);
int sceJpegDecTerm(void *ctx);
int sceJpegDecClose(void *ctx, void *outBuf);
int sceJpegDecReset(void *ctx);
int sceJpegDecDelete(void *ctx);

/* remote_dmacplus.c & remote_sas2.c */
int remoteDmacplusInit(void);
int remoteSas2Init(void);

/* remote_net.c */
int64_t remoteNetFetchTick(const PspemuKermitRequest *req, int (*tickFn)(SceRtcTick *));
int remoteNetInit(void);
int remoteNetSuspend(void);
int remoteNetResume(void);
int remoteNetResetOnReboot(void);
int remoteNetIsAdhocActive(void);
int remoteNetSetThrottle(int enabled);

/* remote_usb.c & remote_usbpspcm.c & scejpeg */
int remoteUsbInit(void *workBuf);
int remoteUsbPspcmInit(void);
int remoteUsbPspcmSuspend(void);
void *sceJpegWorkInit(void *edramBase);

/* remote_peripheral.c & remote_osk.c & osk_filter.c */
int remotePeripheralIsBusy(void);
int remotePeripheralInit(void);
int remotePeripheralUpdateState(void);
int remoteOskInit(void);
int remoteOskAbort(void);
int oskIsCharAllowed(uint16_t ch, const int *langMask, int language, int kanaOnly);
int oskNeedsFullwidthLatin(uint32_t inputTypes);
int oskNeedsFullwidthDigits(uint32_t inputTypes);
int oskPostProcessResult(uint16_t *text, int maxChars, uint32_t inputTypes, int maxLines);
uint16_t oskConvertHiraToKata(uint16_t ch, const int *langMask);

/* menu_dlg.c */
int pspemuMenuPreparePhotoExport(void);
int pspemuMenuDialogSetup(void);
int pspemuMenuSetImeDeadline(SceUInt64 deadline);
int pspemuMenuClearImeDeadline(void);
int pspemuMenuPollDialog(int mode);
int pspemuMenuDialogInit(void);
int pspemuMenuLockDialog(int id);
int pspemuMenuUnlockDialog(int id);

/* remote_low_priority.c */
int remoteLowPriorityLockPsButton(void);
int remoteLowPriorityUnlockPsButton(void);
int remoteLowPriorityInit(void);
int remoteLowPriorityEnqueuePromoteWithCallback(const char *path, int (*doneCb)(int result));
int remoteLowPriorityWaitIdle(void);

/* compat_list.c */
int pspemuCompatHasFlag(uint32_t flagMask);
int pspemuCompatGetParam(int index);
int pspemuCompatIsListOverrideActive(void);
int pspemuTitleInfoThread(SceSize args, void *argp);
int pspemuCompatLoadTitleEntry(const char *discId);
const char *pspemuCompatGetTitleId(void);
int pspemuCompatIsRestrictedBootTitle(void);

/* settings.c */
int pspemuSettingsUpdateController(SceCtrlData *padData);
int pspemuSettingsGetDialogActive(void);
int pspemuSettingsSetMenuEnabled(int enable);
int pspemuSettingsApplyDisplay(void);
int pspemuSettingsNotifyPopsReady(void);
int pspemuSettingsDialogInit(void);
int pspemuSettingsShowGuideDialog(int dialogId);
int pspemuSettingsShowErrorDialog(int errorCode);
int pspemuSettingsShowFatalNotice(int errorCode);
int pspemuSettingsGetPocsFirstBootDone(void);
int pspemuSettingsMarkPocsFirstBootDone(void);
int pspemuSettingsGetFirstBootDone(void);
int pspemuSettingsMarkFirstBootDone(void);
int pspemuSettingsRefreshDynamic(PspemuSystemSettings *settings);
int pspemuSettingsLoadAll(PspemuSystemSettings *settings);
int pspemuSettingsShowPocsFirstBootDialog(int *outButton);
int pspemuBgImageInit(void);

/* remote_pocs.c */
int remotePocsInit(void);
int remotePocsPollPowerKeepAlive(void);
int remotePocsSetDisplayLock(int unlock);
void remotePocsNotifyDisplayReset(void);
int remotePocsMountSaveData(const char *titleId);
int remotePocsEnterMode(void);
int remotePocsExitMode(void);
int remotePocsHasMountedMedia(void);
int remotePocsIsActive(void);
int remotePocsInstallAppSupport(void);

/* pops_host.c */
SceBool popsHostIsOverlayActive(void);
int popsHostHandlePeripheralCmd(uint32_t cmd, uint32_t arg);
void *popsHostGetVmcBuffer(unsigned int slot);
int popsHostReloadVmc(unsigned int slot);
int popsHostSetDisplayMode(int pocsMode);
int popsHostSuspend(void);
int popsHostSetCdramOffset(int offset);
int popsHostInit(void);

/* vmc_manager.c */
int vmcManagerInitWork(void *workBuf, int workSize);
int vmcManagerRegisterSlot(unsigned int slot, void *slotBuf, int slotSize);
int vmcManagerMountFile(unsigned int slot, int flags, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* PSPEMU_HOST_COMMON_H */
