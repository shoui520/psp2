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

#ifdef __cplusplus
extern "C" {
#endif

/* POSIX / Vita error codes used by ScePspemu */
#define PSPEMU_ERROR_NO_MEMORY          ((int)0x8001000C)
#define PSPEMU_ERROR_FAULT              ((int)0x8001000E)
#define PSPEMU_ERROR_INVALID_ARG        ((int)0x80010016)
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
int SceShellUtil_EC5881A5(void);
int SceShellUtil_CE35B2B8(void *callback, void *userArg);

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

/* Shared paths owned by remote_msfs.c */
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

/* remotefs.c */
void remotefsSetCdramBase(void *cdramBase);
void *remotefsTranslateGuestAddr(uint32_t guestAddr, int cacheMode, SceSize size);

/* remote_msfs.c */
void remoteMsfsSuspendReset(void);
int remoteMsfsParseAppParam(char *discId, uint32_t *parentalLevel, char *gamedataId,
                            char *appVer, uint32_t *bootable, int *isPs1Mode, int *isPocsLaunch);
int remoteMsfsCheckPocketStationParam(void);
int remoteMsfsPreInit(void);
int remoteMsfsInit(void);

/* remote_flashfs.c */
int remoteFlashfsInit(void *workBuf);

/* remote_audioout.c & remote_audioin.c */
int remoteAudioOutInit(void);
int remoteAudioOutSuspend(int suspend);
int remoteAudioInInit(void);

/* remote_me.c, remote_me_avcdec.c, me_edram.c, me_wrapper_api */
int remoteMeInit(void);
int remoteMeAvcdecNotifyResume(void);
int meEdramInit(uint32_t size);
void *meEdramGetBase(uint32_t offset);
int meWrapperApiInit(void);

/* remote_dmacplus.c & remote_sas2.c */
int remoteDmacplusInit(void);
int remoteSas2Init(void);

/* remote_net.c */
int remoteNetInit(void);
int remoteNetSuspend(void);
int remoteNetResume(void);
int remoteNetIsAdhocActive(void);

/* remote_usb.c & remote_usbpspcm.c & scejpeg */
int remoteUsbInit(void *workBuf);
int remoteUsbPspcmInit(void);
int remoteUsbPspcmSuspend(void);
void *sceJpegWorkReset(void);
void *sceJpegWorkInit(void *edramBase);

/* remote_peripheral.c & remote_osk.c */
int remotePeripheralIsBusy(void);
int remotePeripheralInit(void);
int remotePeripheralUpdateState(void);
int remoteOskInit(void);

/* menu_dlg.c */
int pspemuMenuDialogSetup(void);
int pspemuMenuDialogInit(void);

/* remote_low_priority.c */
int remoteLowPriorityLockPsButton(void);
int remoteLowPriorityUnlockPsButton(void);
int remoteLowPriorityInit(void);
int remoteLowPriorityWaitIdle(void);

/* compat_list.c */
int pspemuCompatHasFlag(uint32_t flagMask);
int pspemuCompatGetParam(int index);
int pspemuCompatIsListOverrideActive(void);
int pspemuTitleInfoThread(SceSize args, void *argp);
int pspemuCompatLoadTitleEntry(const char *discId);
const char *pspemuCompatGetTitleId(void);

/* settings.c */
int pspemuSettingsUpdateController(SceCtrlData *padData);
int pspemuSettingsGetDialogActive(void);
int pspemuSettingsSetMenuEnabled(int enable);
int pspemuSettingsApplyDisplay(void);
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
