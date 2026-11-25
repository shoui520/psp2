/*
 * ePSP 6.61 sceMediaSync (mediasync.prx)
 * Reimplemented from 3.650.011-TOOL pcff/elf/kd/mediasync.prx
 */

#include "sysfile.h"

#define SCE_MEDIASYNC_ERROR_NO_MEDIUM     ((int)0x80010087)
#define SCE_MEDIASYNC_ERROR_INVALID_MEDIA ((int)0x8002014e)

typedef struct SceIoStat {
	uint32_t st_mode;
	uint32_t st_attr;
	int64_t st_size;
	uint16_t st_ctime[8];
	uint16_t st_atime[8];
	uint16_t st_mtime[8];
	uint32_t st_private[6];
} SceIoStat;

typedef struct SceIoDirent {
	SceIoStat d_stat;
	char d_name[256];
	void *d_private;
	int reserved;
} SceIoDirent;

typedef struct SceRegParam {
	uint32_t regType;
	char name[256];
	uint32_t nameLen;
	uint32_t regMode;
	uint32_t regAttr;
} SceRegParam;

typedef struct SceGameInfo {
	uint32_t size;              /* 0x00 */
	uint32_t flags;             /* 0x04 */
	char umdDiscId[16];         /* 0x08 */
	char sfoDiscId[16];         /* 0x18 */
	uint8_t headerTail[0x1c];   /* 0x28 */
	char productCode[16];       /* 0x44 */
	uint8_t midFields[0x60];    /* 0x54 */
	char parentProductCode[16]; /* 0xb4 */
} SceGameInfo;

typedef struct ScePbpHeader {
	uint8_t magic[4];
	uint32_t version;
	uint32_t sfoOffset;
	uint32_t iconOffset;
	uint32_t restOffsets[6];
} ScePbpHeader;

typedef struct SceSysfileVplHeader {
	char magic[4];
	SceUID vplId;
	uint8_t padding[24];
} SceSysfileVplHeader;

typedef struct PackedUmdDiscEntry {
	uint16_t profileId;
	uint16_t prefixBits;
	uint32_t digitsBcd;
} PackedUmdDiscEntry;

typedef struct DiscQuirkPair {
	const char *discId;
	uint32_t value;
} DiscQuirkPair;

typedef struct DiscQuirkTriple {
	const char *discId;
	uint32_t first;
	uint32_t second;
} DiscQuirkTriple;

/* Imported functions */
extern SceUID sceKernelGetChunk(int chunkId);
extern int sceKernelApplicationType(void);
extern int sceKernelInitApitype(void);
extern int InitForKernel_9D33A110(void);
extern char *sceKernelInitDiscImage(void);
extern char *sceKernelInitFileName(void);
extern int InterruptManagerForKernel_E526B767(int mode);
extern SceUID sceIoOpen(const char *file, int flags, int mode);
extern int64_t sceIoLseek(SceUID fileDesc, int64_t offset, int whence);
extern int sceIoWrite(SceUID fileDesc, const void *data, SceSize size);
extern int sceIoDevctl(const char *dev, uint32_t cmd, const void *inBuf, int inLen, void *outBuf, int outLen);
extern int sceIoRead(SceUID fileDesc, void *data, SceSize size);
extern int sceIoClose(SceUID fileDesc);
extern SceUID sceIoDopen(const char *dirname);
extern int sceIoDread(SceUID dirDesc, SceIoDirent *dir);
extern int sceIoDclose(SceUID dirDesc);
extern int Kprintf(const char *fmt, ...);
extern int sceKernelDipsw(int bit);
extern int sceKernelIsDevelopmentToolMode(void);
extern SceUID sceKernelSearchModuleByName(const char *name);
extern int SysMemForKernel_13EE28DA(int val);
extern void *sceKernelMemcpy(void *dst, const void *src, SceSize size);
extern void *sceKernelAllocHeapMemory(SceUID heapId, SceSize size);
extern SceUID sceKernelCreateHeap(SceUID partId, SceSize size, int attr, const char *name);
extern SceUID sceKernelAllocPartitionMemory(SceUID partId, const char *name, int type, SceSize size, void *addr);
extern int sceKernelFreeHeapMemory(SceUID heapId, void *block);
extern void *sceKernelMemset(void *dst, int val, SceSize size);
extern int SysMemForKernel_A03CB480(const char *env);
extern int sceKernelFreePartitionMemory(SceUID blockId);
extern int sceKernelDeleteHeap(SceUID heapId);
extern SceGameInfo *sceKernelGetGameInfo(void);
extern void *sceKernelGetBlockHeadAddr(SceUID blockId);
extern void *memset(void *dst, int c, size_t n);
extern int tolower(int c);
extern char *strcat(char *dst, const char *src);
extern size_t strlen(const char *str);
extern int strncmp(const char *lhs, const char *rhs, size_t n);
extern int memcmp(const void *lhs, const void *rhs, size_t n);
extern char *strncpy(char *dst, const char *src, size_t n);
extern int strcmp(const char *lhs, const char *rhs);
extern int sceKernelSetEventFlag(SceUID evid, uint32_t bits);
extern int sceKernelWaitEventFlagCB(SceUID evid, uint32_t bits, uint32_t wait, uint32_t *outBits, uint32_t *timeout);
extern SceUID sceKernelCreateEventFlag(const char *name, int attr, int bits, void *opt);
extern SceUID sceKernelCreateVpl(const char *name, int part, int attr, SceSize size, void *opt);
extern int sceKernelDeleteVpl(SceUID uid);
extern int sceKernelTryAllocateVpl(SceUID uid, SceSize size, void **data);
extern int sceKernelFreeVpl(SceUID uid, void *data);
extern int ThreadManForKernel_D366D35A(void);
extern SceUID sceKernelCreateCallback(const char *name, int (*func)(int, int, void *), void *arg);
extern int sceKernelDeleteCallback(SceUID cb);
extern int sceKernelDeleteEventFlag(SceUID evid);
extern uint32_t sceKernelGetGPI(void);
extern int sceKernelUtilsMd5Digest(const uint8_t *data, uint32_t size, uint8_t *digest);
extern int sceAudio_driver_95AE5A2B(uint32_t quirkId, uint32_t quirkVal);
extern int sceCtrl_driver_5886194C(uint32_t quirkVal);
extern int sceKermitPeripheral_driver_4A26B7C8(void);
extern int sceKermitPeripheral_driver_675DB2C1(void);
extern int sceKermitPeripheral_driver_B93A88E2(void);
extern int sceMeCore_driver_4CD5B5BF(uint32_t quirkId, uint32_t quirkVal);
extern int sceRegCloseRegistry(uint32_t regHandle);
extern int sceRegOpenCategory(uint32_t regHandle, const char *name, int mode, uint32_t *catHandle);
extern int sceRegGetKeyInfo(uint32_t catHandle, const char *name, uint32_t *keyHandle, uint32_t *type, SceSize *size);
extern int sceRegOpenRegistry(SceRegParam *reg, int mode, uint32_t *regHandle);
extern int sceRegGetKeyValue(uint32_t catHandle, uint32_t keyHandle, void *buf, SceSize size);
extern int sceRegCloseCategory(uint32_t catHandle);
extern int sceUmdWaitDriveStat(int stat);
extern int sceUmdCheckMedium(void);
extern int sceUmdGetDriveStatus(void);
extern int sceUmdActivate(int unit, const char *drive);
extern int sceUmd9660_driver_3CC9CE54(void);
extern int sceUmd9660_driver_FE3A8B67(void);
extern int sceUmdCache_driver_576E0F06(int profileId);
extern const char *sceUmdCache_driver_A21D8213(void);
extern int sceUmdCache_driver_DB97E432(int mode);
extern int sceWlanDrv_driver_EDD207B1(int cmd, const void *arg);

/* Imported variables (linked by NID via module.yml) */
extern int g_ioDeletedError;
extern int g_ioAsyncDelayEnable;
extern int g_ioAsyncDelayCount;
extern int g_modmgrCompatMode;
extern int g_modmgrCompatFixA;
extern int g_modmgrCompatFixB;
extern int g_dmacCompatFlagA;
extern int g_dmacCompatModeB;
extern int g_dmacCompatFlagC;
extern int g_dmacCompatFlagD;
extern int g_msfsCompatAlwaysOne;
extern int g_msfsCompatDelayFlag;
extern int g_msfsCompatCacheDelay;
extern int g_msfsCompatFlagEnable;
extern int g_meCoreCompatListFlag;
extern int g_meCoreCompatSingleA;
extern int g_meCoreCompatSingleB;
extern int g_powerKermitMode;
extern int g_powerCompatFlagC;
extern int g_powerCompatSingleD;
extern int g_powerCompatDelayA;
extern int g_powerCompatFlagB;
extern int g_powerCompatTableF;
extern int g_powerCompatTableE;
extern int g_powerCompatDelayG;
extern int g_powerCompatDelayH;
extern int g_powerCompatFlagI;
extern int g_powerCompatFlagJ;
extern int g_powerCompatTableK;
extern int g_regCompatDelayB;
extern int g_regCompatDelayA;
extern int g_rtcCompatFlag;
extern int g_usbCompatModeB;
extern int g_usbCompatModeA;

/* Forward declarations */
static int __attribute__((noinline)) WaitDisc(void);
static int __attribute__((noinline)) WaitMs(void);
static int __attribute__((noinline)) MsCheckMedia(const char *filename);
static int __attribute__((noinline)) WaitEflash(void);
static int __attribute__((noinline)) WaitDiscEmu(void);
static int __attribute__((noinline)) MsCallback(int count, int arg, void *common);
static int __attribute__((noinline)) MsSystemFile(void *buf, SceSize size, SceGameInfo *gameInfo);
static void * __attribute__((noinline)) sf_malloc(SceSize size);
static void * __attribute__((noinline, used)) sf_vpl_malloc(SceSize size);
static int __attribute__((noinline)) sf_free(void *ptr);
static int __attribute__((noinline, used)) sf_vpl_free(void *ptr);
static int __attribute__((noinline)) mediasyncCheckFlash0Hash(void);
static void __attribute__((noinline)) mediasyncApplyModmgrQuirks(void);
static void __attribute__((noinline)) mediasyncApplyGameQuirks(void);
static int __attribute__((noinline)) mediasyncApplyUmdCacheQuirks(void);
static void __attribute__((noinline)) mediasyncApplyWlanQuirks(void);
static void __attribute__((noinline)) mediasyncApplyIntrQuirks(void);
static void __attribute__((noinline)) mediasyncApplyAudioQuirks(void);
static void __attribute__((noinline)) mediasyncApplyMeCoreCallQuirks(void);
static void __attribute__((noinline)) mediasyncApplyCtrlQuirks(void);
static void __attribute__((noinline)) mediasyncApplyIoQuirks(void);
static void __attribute__((noinline)) mediasyncApplyMsDevctlQuirks(void);
static void __attribute__((noinline)) mediasyncApplyMeCoreVarQuirks(void);
static void __attribute__((noinline)) mediasyncApplyPowerQuirks(void);
static void __attribute__((noinline)) mediasyncApplyDmacQuirks(void);
static void __attribute__((noinline)) mediasyncApplyRtcQuirks(void);
static void __attribute__((noinline)) mediasyncApplyUsbQuirks(void);
static void __attribute__((noinline)) mediasyncApplyRegQuirks(void);
static void __attribute__((noinline)) mediasyncApplyMsfsQuirks(void);
static int __attribute__((noinline)) mediasyncLoadNpEnvFromReg(void);
static void __attribute__((noinline)) mediasyncHashNpEnv(char *outBuf, const char *envStr);
static int __attribute__((noinline)) mediasyncLoadAdhocPrefixFromReg(void);

/* Globals */
static SceUID g_msEvfId;
static int g_msCallbackArg;
static SceUID g_sfHeapId;
static uint32_t g_fallbackChunk[8];

static const char g_vplTagMagic[4] = { '\0', 'P', 'S', 'F' };

/* Tables */
static const uint32_t g_kdMd5Table[7][4] = {
	{ 0xff785cf8U, 0x8c134305U, 0xd6702e16U, 0xb69b8745U },
	{ 0xbf773900U, 0x05c60c22U, 0xada49759U, 0xd85d93e4U },
	{ 0x3b0a5785U, 0xe16461f6U, 0xfec672ecU, 0x792ba3b1U },
	{ 0xa43476dbU, 0x6e54eef2U, 0x6c8dc6b7U, 0x8bc91913U },
	{ 0xe7baf988U, 0x013634e6U, 0x95cb1da8U, 0x1626e3f3U },
	{ 0xb48235f7U, 0x0fc7982eU, 0xbbf2d737U, 0x28b03f50U },
	{ 0x7713e0e7U, 0xd55e9a4dU, 0xdd28f173U, 0x381934c2U },
};

static const PackedUmdDiscEntry g_umdDisableTable[34] = {
	{ 0x0000, 0x0084, 0x00076024U },
	{ 0x0000, 0x0094, 0x00074600U },
	{ 0x0000, 0x00c4, 0x00071201U },
	{ 0x0000, 0x0094, 0x00074100U },
	{ 0x0000, 0x00c4, 0x00042001U },
	{ 0x0000, 0x00c4, 0x00061201U },
	{ 0x0000, 0x00a4, 0x00031100U },
	{ 0x0000, 0x0094, 0x00098100U },
	{ 0x0000, 0x0094, 0x00009100U },
	{ 0x0000, 0x0094, 0x00019100U },
	{ 0x0000, 0x00b4, 0x00073064U },
	{ 0x0000, 0x00c4, 0x00054001U },
	{ 0x0000, 0x0084, 0x00099024U },
	{ 0x0000, 0x00c4, 0x00048101U },
	{ 0x0000, 0x00b4, 0x00093164U },
	{ 0x0000, 0x00c4, 0x00070301U },
	{ 0x0000, 0x0094, 0x00050900U },
	{ 0x0000, 0x0094, 0x00060900U },
	{ 0x0000, 0x00c4, 0x00007001U },
	{ 0x0000, 0x0094, 0x00018200U },
	{ 0x0000, 0x00a4, 0x00006000U },
	{ 0x0000, 0x00a4, 0x00041100U },
	{ 0x0000, 0x0024, 0x00088150U },
	{ 0x0000, 0x0024, 0x00007050U },
	{ 0x0000, 0x0080, 0x00074104U },
	{ 0x0000, 0x00c0, 0x00076689U },
	{ 0x0000, 0x00c4, 0x00085101U },
	{ 0x0000, 0x00c4, 0x00002001U },
	{ 0x0000, 0x00a0, 0x00081001U },
	{ 0x0000, 0x0094, 0x00053100U },
	{ 0x0000, 0x0080, 0x00052004U },
	{ 0x0000, 0x00a4, 0x00095000U },
	{ 0x0000, 0x00c4, 0x00053201U },
	{ 0x0000, 0x0094, 0x00004700U },
};

static const PackedUmdDiscEntry g_umdModeThreeTable[42] = {
	{ 0x0001, 0x0024, 0x00036050U },
	{ 0x0002, 0x00c4, 0x00024001U },
	{ 0x0003, 0x00a0, 0x00016001U },
	{ 0x0004, 0x00a4, 0x00024000U },
	{ 0x0005, 0x0024, 0x00078050U },
	{ 0x0006, 0x00b4, 0x00051064U },
	{ 0x0007, 0x00a0, 0x00063001U },
	{ 0x0008, 0x0090, 0x00025100U },
	{ 0x0009, 0x00c4, 0x00059001U },
	{ 0x000a, 0x0024, 0x00080150U },
	{ 0x000b, 0x00c4, 0x00081301U },
	{ 0x000c, 0x0024, 0x00057050U },
	{ 0x000d, 0x00c4, 0x00076101U },
	{ 0x000e, 0x0094, 0x00024500U },
	{ 0x000f, 0x0084, 0x00012124U },
	{ 0x0010, 0x00c4, 0x00069201U },
	{ 0x0011, 0x0024, 0x00064050U },
	{ 0x0012, 0x0024, 0x00023150U },
	{ 0x0013, 0x00b4, 0x00002164U },
	{ 0x0014, 0x00c4, 0x00096101U },
	{ 0x0015, 0x0094, 0x00067400U },
	{ 0x0016, 0x0024, 0x00081050U },
	{ 0x0017, 0x00a0, 0x00001001U },
	{ 0x0018, 0x0080, 0x00081004U },
	{ 0x0019, 0x0020, 0x00020459U },
	{ 0x001a, 0x00c0, 0x00032689U },
	{ 0x001b, 0x00b0, 0x00011054U },
	{ 0x001c, 0x00a0, 0x00072001U },
	{ 0x001d, 0x00a4, 0x00027000U },
	{ 0x001e, 0x00a4, 0x00037000U },
	{ 0x001f, 0x00b4, 0x00019064U },
	{ 0x0020, 0x00a4, 0x00067000U },
	{ 0x0021, 0x00a4, 0x00028000U },
	{ 0x0022, 0x0024, 0x00062050U },
	{ 0x0023, 0x00a0, 0x00034001U },
	{ 0x0024, 0x0024, 0x00019050U },
	{ 0x0025, 0x0024, 0x00078250U },
	{ 0x0026, 0x0094, 0x00014800U },
	{ 0x0027, 0x00c4, 0x00077201U },
	{ 0x0028, 0x0084, 0x00081124U },
	{ 0x0029, 0x00b4, 0x00055164U },
	{ 0x002a, 0x00a0, 0x00010001U },
};

static const PackedUmdDiscEntry g_umdModeTwoTable[8] = {
	{ 0x002b, 0x0020, 0x00010459U },
	{ 0x002c, 0x0024, 0x00058150U },
	{ 0x002d, 0x0094, 0x00086400U },
	{ 0x002e, 0x00c4, 0x00074101U },
	{ 0x002f, 0x00c4, 0x00012201U },
	{ 0x0030, 0x0094, 0x00099600U },
	{ 0x0031, 0x00b4, 0x00067064U },
	{ 0x0032, 0x0084, 0x00069024U },
};

static const DiscQuirkTriple g_wlanQuirkTable[9] = {
	{ "ULUS10354", 4, 100 },
	{ "NPJH50184", 5, 1 },
	{ "ULUS10466", 5, 1 },
	{ "ULES01376", 5, 1 },
	{ "ULUS10059", 6, 4200 },
	{ "ULUS10059", 7, 1200 },
	{ "UCJS10105", 5, 1 },
	{ "ULJS00392", 8, 4 },
	{ "NPJH50185", 9, 150 },
};

static const DiscQuirkTriple g_audioQuirkTable[38] = {
	{ "ULJS00019", 1, 1 },
	{ "UCJS10104", 2, 2 },
	{ "UCKS45124", 2, 2 },
	{ "UCES01250", 2, 2 },
	{ "UCAS40266", 2, 2 },
	{ "UCUS98743", 2, 2 },
	{ "NPJH50376", 3, 1 },
	{ "ULJM05245", 3, 1 },
	{ "ULES00987", 3, 1 },
	{ "NPJH50492", 3, 1 },
	{ "NPJG00116", 4, 32000 },
	{ "UCUS98741", 4, 32000 },
	{ "UCES01327", 4, 32000 },
	{ "NPHG00080", 4, 32000 },
	{ "UCJS10107", 4, 32000 },
	{ "UCUS98744", 4, 32000 },
	{ "UCAS40262", 4, 32000 },
	{ "UCES01264", 4, 32000 },
	{ "ULUS10253", 4, 32000 },
	{ "ULES00826", 4, 32000 },
	{ "ULJM05465", 4, 32000 },
	{ "ULUS10347", 4, 32000 },
	{ "ULES01070", 4, 32000 },
	{ "ULUS10472", 4, 32000 },
	{ "ULES01381", 4, 32000 },
	{ "ULES01365", 4, 32000 },
	{ "ULJM05412", 4, 32000 },
	{ "NPHH00068", 5, 500 },
	{ "UCJS10052", 6, 4000 },
	{ "UCJS18030", 6, 4000 },
	{ "UCUS98633", 6, 4000 },
	{ "UCES00420", 6, 4000 },
	{ "ULUS10102", 7, 1 },
	{ "ULJS00167", 7, 1 },
	{ "ULJS00168", 7, 1 },
	{ "ULJS00169", 7, 1 },
	{ "ULJM05155", 7, 1 },
	{ "NPJH50492", 8, 3000 },
};

static const DiscQuirkPair g_ctrlQuirkTable[14] = {
	{ "ULJS00022", 1 },
	{ "ULJM05071", 1 },
	{ "ULUS10089", 1 },
	{ "ULES00297", 1 },
	{ "ULJM05352", 3 },
	{ "ULJS00154", 1 },
	{ "ULJM05058", 1 },
	{ "ULJM05189", 1 },
	{ "ULJM05201", 1 },
	{ "ULJS00170", 1 },
	{ "ULJM05164", 1 },
	{ "ULJM05156", 1 },
	{ "ULJM05500", 1 },
	{ "ULJM05135", 1 },
};

static const char * const g_ioMsfsDelayTable[2] = {
	"ULUS10408", "ULES01215"
};

static const char * const g_msDevctlTable[28] = {
	"UCKS45124", "UCJS10104", "UCUS98743", "UCES01250",
	"UCAS40266", "ULJS00224", "NPJH50184", "ULUS10466",
	"ULES01376", "ULAS42214", "ULKS46235", "ULJS00202",
	"ULUS10457", "ULES01298", "UCKS45126", "ULJS00217",
	"ULJM05519", "ULJM05489", "NPJH50040", "UCKS45140",
	"ULUS10512", "ULJS00238", "ULJS00236", "ULUS10518",
	"ULAS42231", "ULES01407", "ULJM05542", "ULJM05544"
};

static const char * const g_mscmhcDevctlTable[3] = {
	"NPUG80329", "UCES00044", "NPEG00025"
};

static const DiscQuirkPair g_modmgrModeTable[3] = {
	{ "UCJS10007", 1 },
	{ "ULJM05029", 2 },
	{ "UCJS10003", 3 },
};

static const char * const g_modmgrFixBTable[5] = {
	"ULUS10273", "ULES00811", "NPEZ00026", "NPUZ00048", "NPEZ00100"
};

static const char * const g_meCoreListTable[12] = {
	"ULJM05299", "NPJH50230", "NPJH50245", "NPJH50423",
	"NPEZ00319", "NPUZ00188", "NPEZ00354", "NPUZ00214",
	"UCJS10007", "UCUS98612", "UCES00001", "ULJS00216"
};

static const DiscQuirkPair g_powerDelayATable[12] = {
	{ "NPJH50043", 10000 },
	{ "NPJH50483", 100000 },
	{ "NPJG00045", 120000 },
	{ "UCES01312", 120000 },
	{ "UCUS98740", 120000 },
	{ "NPHG00025", 120000 },
	{ "UCJS10107", 100000 },
	{ "UCES01264", 100000 },
	{ "UCUS98744", 100000 },
	{ "UCAS40262", 100000 },
	{ "NPEZ00319", 60000 },
	{ "NPUZ00188", 60000 },
};

static const char * const g_powerFlagBTable[14] = {
	"NPJH50245", "UCJS10052", "UCJS18030", "UCUS98633",
	"UCES00420", "NPJH50483", "NPJG00045", "UCES01312",
	"UCUS98740", "NPHG00025", "ULJM05299", "NPJH50230",
	"NPJH50423", "ULJS00216"
};

static const char * const g_powerFlagCTable[5] = {
	"ULJM05824", "NPJH50280", "UCUS98640", "ULES01019", "ULUS10331"
};

static const DiscQuirkTriple g_powerPairGHTable[2] = {
	{ "ULUS10237", 300, 200 },
	{ "ULES00770", 300, 200 },
};

static const DiscQuirkPair g_powerTableETable[9] = {
	{ "ULJM05494", 1 },
	{ "ULUS10462", 1 },
	{ "ULES01275", 1 },
	{ "ULES00636", 1 },
	{ "ULUS10223", 1 },
	{ "NPEZ00190", 4 },
	{ "ULJM05251", 2 },
	{ "ULUS10186", 2 },
	{ "NPJG00111", 1 },
};

static const DiscQuirkPair g_powerTableFTable[24] = {
	{ "UCJS10107", 1000000 },
	{ "UCES01264", 1000000 },
	{ "UCUS98744", 1000000 },
	{ "UCAS40262", 1000000 },
	{ "UCJS10090", 1000000 },
	{ "UCUS98668", 1000000 },
	{ "UCES01184", 1000000 },
	{ "ULJM05242", 1000000 },
	{ "ULUS10236", 1000000 },
	{ "ULES00703", 1000000 },
	{ "ULAS42095", 1000000 },
	{ "ULJM05049", 1000000 },
	{ "ULUS10025", 1000000 },
	{ "ULES00125", 1000000 },
	{ "ULAS42019", 1000000 },
	{ "NPEZ00354", 1000000 },
	{ "NPUZ00214", 1000000 },
	{ "NPEZ00319", 1000000 },
	{ "NPUZ00188", 1000000 },
	{ "ULES00753", 1000000 },
	{ "UCUS98640", 1000000 },
	{ "UCES00006", 2000000 },
	{ "NPUZ00072", 500000 },
	{ "NPEZ00158", 500000 },
};

static const char * const g_powerFlagJTable[2] = {
	"NPEZ00354", "NPUZ00214"
};

static const char * const g_powerFlagITable[13] = {
	"UCUS98640", "ULUS10217", "ULES00647", "ULJS00012",
	"UCES00018", "UCUS98687", "ULES00798", "ULKS46082",
	"ULKS46083", "ULKS46033", "ULKS46090", "ULKS46098",
	"ULKS46099"
};

static const DiscQuirkPair g_powerTableKTable[6] = {
	{ "ULUS10552", 100000 },
	{ "NPJH50568", 300000 },
	{ "NPJH50569", 300000 },
	{ "NPJH50618", 300000 },
	{ "NPUZ00072", 300000 },
	{ "NPEZ00158", 300000 },
};

static const char * const g_dmacFlagCTable[2] = {
	"ULJM05836", "ULUS10538"
};

static DiscQuirkTriple g_meCoreCallQuirkTable[1] = {
	{ "NPJH50239", 1, 3500 },
};

static DiscQuirkPair g_usbModeATable[2] = {
	{ "UCJS10078", 1 },
	{ "NPUG81000", 2 },
};

static DiscQuirkPair g_usbModeBTable[1] = {
	{ "NPUG81000", 2 },
};

static DiscQuirkPair g_regDelayATable[2] = {
	{ "ULUS10214", 500 },
	{ "ULJM05958", 240000 },
};

static DiscQuirkPair g_regDelayBTable[1] = {
	{ "ULUS10214", 30000 },
};

static const char * const g_msfsFlagEnableTable[3] = {
	"ULUS10273", "ULES00811", "ULJS00093"
};

static const DiscQuirkPair g_msfsCacheDelayTable[2] = {
	{ "ULJM06022", 2000 },
	{ "ULJM05053", 2000 },
};

static const uint32_t g_npEnvMd5Retail[4] = {
	0x22368ffdU, 0x4d9bad8eU, 0x5b6dad92U, 0xcdf56413U
};

static const uint32_t g_npEnvMd5DevA[4] = {
	0x8ee5ef17U, 0x9330556fU, 0xc956e857U, 0x65b31c35U
};

static const uint32_t g_npEnvMd5DevB[4] = {
	0xf91ddd4aU, 0xe8ca515aU, 0x37778db8U, 0xcc18ff71U
};

/* === 0x0000 module_start === */
int module_start(SceSize args, void *argp)
{
	int apitype = sceKernelInitApitype();
	const char *initFile = sceKernelInitFileName();
	uint32_t syncMask = 0;
	int res = 0;

	(void)args;
	(void)argp;

	switch (apitype) {
	case 0x110:
	case 0x111:
	case 0x120:
	case 0x121:
	case 0x122:
		if (initFile != NULL && strncmp(initFile, "disc", 4) == 0) {
			syncMask = 0x01;
		}
		break;
	case 0x112:
	case 0x113:
	case 0x123:
	case 0x170:
		if (initFile != NULL && strncmp(initFile, "disc", 4) == 0) {
			syncMask = 0x82;
		}
		break;
	case 0x114:
	case 0x115:
	case 0x125:
	case 0x171:
		if (initFile != NULL && strncmp(initFile, "disc", 4) == 0) {
			syncMask = 0x88;
		}
		break;
	case 0x116:
	case 0x160:
		syncMask = 0x03;
		break;
	case 0x117:
		syncMask = 0x82;
		break;
	case 0x118:
		syncMask = 0x09;
		break;
	case 0x119:
		syncMask = 0x88;
		break;
	case 0x140:
	case 0x141:
	case 0x143:
	case 0x144:
	case 0x145:
		if (initFile != NULL) {
			if (strncmp(initFile, "ms", 2) == 0) {
				syncMask = 0x02;
			}
			if (strncmp(initFile, "flash3", 6) == 0) {
				syncMask = 0x04;
			}
		}
		break;
	case 0x151:
	case 0x152:
	case 0x154:
	case 0x155:
	case 0x156:
		if (initFile != NULL) {
			if (strncmp(initFile, "ef", 2) == 0) {
				syncMask = 0x08;
			}
			if (strncmp(initFile, "flash3", 6) == 0) {
				syncMask = 0x04;
			}
		}
		break;
	default:
		break;
	}

	if (syncMask & 0x01u) {
		res = WaitDisc();
	}
	if (res >= 0 && (syncMask & 0x02u)) {
		res = WaitMs();
	}
	if (res >= 0 && (syncMask & 0x08u)) {
		res = WaitEflash();
	}
	if (res >= 0) {
		if (syncMask & 0x04u) {
			res = 0;
		}
		if (syncMask & 0x80u) {
			res = WaitDiscEmu();
		}
	}

	if (res < 0) {
		SceUID chunk = sceKernelGetChunk(0);
		uint32_t *chunkBuf;
		if (chunk != 0) {
			chunkBuf = (uint32_t *)sceKernelGetBlockHeadAddr(chunk);
		} else {
			g_fallbackChunk[0] = 32;
			chunkBuf = g_fallbackChunk;
		}
		chunkBuf[5] = (uint32_t)res;
	}

	int foundKd = mediasyncCheckFlash0Hash();
	if (sceKernelDipsw(0x34) != 0 && ((sceKernelGetGPI() >> 5) & 1u) != 0) {
		SysMemForKernel_13EE28DA(2);
	} else if (foundKd == 0) {
		SysMemForKernel_13EE28DA(0);
	}

	mediasyncApplyGameQuirks();
	mediasyncLoadNpEnvFromReg();
	mediasyncLoadAdhocPrefixFromReg();
	return 1;
}

/* === 0x02a0 WaitDisc === */
static int WaitDisc(void)
{
	if (sceUmdCheckMedium() == 0) {
		return SCE_MEDIASYNC_ERROR_NO_MEDIUM;
	}
	int res = sceUmdActivate(1, "disc0:");
	if (res < 0) {
		Kprintf("mediasync.c:%s:sceUmdActivate failed 0x%08x\n", "WaitDisc", res);
		return res;
	}
	res = sceUmdWaitDriveStat(0x29);
	if (res < 0) {
		Kprintf("mediasync.c:%s:sceUmdWaitDriveStat failed 0x%08x\n", "WaitDisc", res);
		return res;
	}
	int status = sceUmdGetDriveStatus();
	if ((status & 0x20) == 0) {
		Kprintf("mediasync.c:%s:sceUmdGetDriveStatus:0x%08x\n", "WaitDisc", status);
		return SCE_MEDIASYNC_ERROR_NO_MEDIUM;
	}

	SceGameInfo *gameInfo = sceKernelGetGameInfo();
	if (strncmp(gameInfo->umdDiscId, gameInfo->sfoDiscId, 10) == 0) {
		Kprintf("mediasync.c:%s:UMD Media Check OK\n", "DiscCheckMedia");
		return 0;
	}
	Kprintf("mediasync.c:%s:SCE_MEDIASYNC_ERROR_INVALID_MEDIA\n", "DiscCheckMedia");
	Kprintf("mediasync.c:%s:[%16s] and [%16s]\n", "DiscCheckMedia", gameInfo->umdDiscId, gameInfo->sfoDiscId);
	Kprintf("mediasync.c:%s:DiscCheckMedia failed 0x%08x\n", "WaitDisc", SCE_MEDIASYNC_ERROR_INVALID_MEDIA);
	return SCE_MEDIASYNC_ERROR_INVALID_MEDIA;
}

/* === 0x03f8 WaitMs === */
static int WaitMs(void)
{
	uint32_t timeout = 10000000;
	uint32_t outBits;
	int res;

	SceUID evf = sceKernelCreateEventFlag("SceMediaSyncEvMs", 0, 0, NULL);
	if (evf < 0) {
		Kprintf("mediasync.c:%s:sceKernelCreateEventFlag failed 0x%08x\n", "WaitMs", evf);
		res = evf;
	} else {
		g_msEvfId = evf;
		SceUID cbid = sceKernelCreateCallback("SceMediaSyncMs", MsCallback, NULL);
		SceUID regCb = cbid;
		res = sceIoDevctl("fatms0:", 0x02415821, &regCb, 4, NULL, 0);
		if (res < 0) {
			Kprintf("mediasync.c:%s:sceFatmsRegisterNotifyCallback failed 0x%08x\n", "WaitMs", res);
		} else {
			res = sceKernelWaitEventFlagCB(g_msEvfId, 1, 0x11, &outBits, &timeout);
			if (res >= 0 && g_msCallbackArg != 1) {
				res = SCE_MEDIASYNC_ERROR_INVALID_MEDIA;
			}
		}
		SceUID unregCb = cbid;
		int unregRes = sceIoDevctl("fatms0:", 0x02415822, &unregCb, 4, NULL, 0);
		if (unregRes < 0) {
			Kprintf("mediasync.c:%s:sceFatmsUnRegisterNotifyCallback failed 0x%08x\n", "WaitMs", unregRes);
		}
		int delCbRes = sceKernelDeleteCallback(cbid);
		if (delCbRes < 0) {
			Kprintf("mediasync.c:%s:sceKernelDeleteCallback failed 0x%08x\n", "WaitMs", delCbRes);
		}
	}

	int delEvfRes = sceKernelDeleteEventFlag(g_msEvfId);
	if (delEvfRes < 0) {
		Kprintf("mediasync.c:%s:sceKernelDeleteEventFlag failed 0x%08x\n", "WaitMs", delEvfRes);
	}
	if (res < 0) {
		return res;
	}

	const char *initFile = sceKernelInitFileName();
	if (initFile == NULL) {
		return SCE_MEDIASYNC_ERROR_INVALID_MEDIA;
	}

	int apitype = sceKernelInitApitype();
	switch (apitype) {
	case 0x112:
	case 0x113:
	case 0x116:
	case 0x117:
	case 0x123:
	case 0x170:
		return 0;
	case 0x140:
		res = MsCheckMedia(initFile);
		if (res < 0) {
			Kprintf("mediasync.c:%s:Warning: MsCheckMediaFailed 0x%08x\n", "WaitMs", res);
			res = 0;
		}
		return res;
	case 0x141:
	case 0x143:
	case 0x144:
	case 0x145:
	case 0x160:
		res = MsCheckMedia(initFile);
		if (res < 0) {
			Kprintf("mediasync.c:%s:MsCheckMediaFailed 0x%08x\n", "WaitMs", res);
		}
		return res;
	default:
		Kprintf("mediasync.c:%s:unsupported apitype=0x%08x[%d]\n", "WaitMs", apitype, apitype);
		return SCE_MEDIASYNC_ERROR_INVALID_MEDIA;
	}
}

/* === 0x06a8 MsCheckMedia === */
static int MsCheckMedia(const char *filename)
{
	char hdrBlockName[32];
	char sfoBlockName[32];
	sysfileStrcpy(hdrBlockName, "SceMdiaSync:work1");
	sysfileStrcpy(sfoBlockName, "SceMdiaSync:work2");

	SceGameInfo *gameInfo = sceKernelGetGameInfo();
	SceUID memId = sceKernelAllocPartitionMemory(1, hdrBlockName, 1, sizeof(ScePbpHeader), NULL);
	if (memId < 0) {
		Kprintf("mediasync.c:%s:sceKernelAllocPartitionMemory failed %s:0x%08x\n", "MsCheckMedia", hdrBlockName, memId);
		return memId;
	}

	SceUID fileDesc = sceIoOpen(filename, 1, 0x1ff);
	if (fileDesc < 0) {
		Kprintf("mediasync.c:%s:sceIoOpen failed [%s] 0x%08x\n", "MsCheckMedia", filename, fileDesc);
		return fileDesc;
	}

	ScePbpHeader *pbpHdr = (ScePbpHeader *)sceKernelGetBlockHeadAddr(memId);
	int res = sceIoRead(fileDesc, pbpHdr, sizeof(ScePbpHeader));
	int needFree = 1;
	if (res < 0) {
		Kprintf("mediasync.c:%s:read header failed 0x%08x\n", "MsCheckMedia", res);
	} else if (pbpHdr->magic[0] == 0x00 && pbpHdr->magic[1] == 'P' &&
	           pbpHdr->magic[2] == 'B' && pbpHdr->magic[3] == 'P') {
		uint32_t sfoOffset = pbpHdr->sfoOffset;
		uint32_t sfoSize = pbpHdr->iconOffset - sfoOffset;
		res = sceKernelFreePartitionMemory(memId);
		if (res < 0) {
			Kprintf("mediasync.c:%s:sceKernelFreePartitionMemory failed %s:0x%08x\n", "MsCheckMedia", hdrBlockName, res);
			needFree = 0;
		} else {
			int64_t seekPos = sceIoLseek(fileDesc, (int64_t)(uint64_t)sfoOffset, 0);
			if (seekPos < 0) {
				Kprintf("mediasync.c:%s:sceIoLseek failed, 0x%x\n", "MsCheckMedia", (int)seekPos);
				res = (int)seekPos;
				needFree = 0;
			} else {
				memId = sceKernelAllocPartitionMemory(1, sfoBlockName, 1, sfoSize, NULL);
				if (memId < 0) {
					Kprintf("mediasync.c:%s:sceKernelAllocPartitionMemory failed %s:0x%08x:size 0x%x\n",
					        "MsCheckMedia", sfoBlockName, memId, sfoSize);
					res = memId;
				} else {
					void *sfoBuf = sceKernelGetBlockHeadAddr(memId);
					res = sceIoRead(fileDesc, sfoBuf, sfoSize);
					if (res >= 0) {
						res = MsSystemFile(sfoBuf, (SceSize)res, gameInfo);
					}
				}
			}
		}
	} else {
		Kprintf("mediasync.c:%s:This is not PBP\n", "MsCheckMedia");
		res = SCE_MEDIASYNC_ERROR_INVALID_MEDIA;
	}

	if (needFree && memId > 0) {
		int freeRes = sceKernelFreePartitionMemory(memId);
		if (freeRes < 0) {
			Kprintf("mediasync.c:%s:sceKernelFreePartitionMemory failed 0x%08x\n", "MsCheckMedia", freeRes);
		}
	}
	sceIoClose(fileDesc);
	return res;
}

/* === 0x09c8 WaitEflash === */
static int WaitEflash(void)
{
	uint32_t timeout = 10000000;
	uint32_t outBits;
	int res;

	SceUID evf = sceKernelCreateEventFlag("SceMediaSyncEvEf", 0, 0, NULL);
	if (evf < 0) {
		Kprintf("mediasync.c:%s:sceKernelCreateEventFlag failed 0x%08x\n", "WaitEflash", evf);
		res = evf;
	} else {
		g_msEvfId = evf;
		SceUID cbid = sceKernelCreateCallback("SceMediaSyncEf", MsCallback, NULL);
		SceUID regCb = cbid;
		res = sceIoDevctl("fatef0:", 0x02415821, &regCb, 4, NULL, 0);
		if (res < 0) {
			Kprintf("mediasync.c:%s:sceFatefRegisterNotifyCallback failed 0x%08x\n", "WaitEflash", res);
		} else {
			res = sceKernelWaitEventFlagCB(g_msEvfId, 1, 0x11, &outBits, &timeout);
			if (res >= 0 && g_msCallbackArg != 1) {
				res = SCE_MEDIASYNC_ERROR_INVALID_MEDIA;
			}
		}
		SceUID unregCb = cbid;
		int unregRes = sceIoDevctl("fatef0:", 0x02415822, &unregCb, 4, NULL, 0);
		if (unregRes < 0) {
			Kprintf("mediasync.c:%s:sceFatefUnRegisterNotifyCallback failed 0x%08x\n", "WaitEflash", unregRes);
		}
		int delCbRes = sceKernelDeleteCallback(cbid);
		if (delCbRes < 0) {
			Kprintf("mediasync.c:%s:sceKernelDeleteCallback failed 0x%08x\n", "WaitEflash", delCbRes);
		}
	}

	int delEvfRes = sceKernelDeleteEventFlag(g_msEvfId);
	if (delEvfRes < 0) {
		Kprintf("mediasync.c:%s:sceKernelDeleteEventFlag failed 0x%08x\n", "WaitEflash", delEvfRes);
	}
	if (res < 0) {
		return res;
	}

	const char *initFile = sceKernelInitFileName();
	if (initFile == NULL) {
		return SCE_MEDIASYNC_ERROR_INVALID_MEDIA;
	}

	int apitype = sceKernelInitApitype();
	switch (apitype) {
	case 0x114:
	case 0x115:
	case 0x118:
	case 0x119:
	case 0x125:
	case 0x171:
		return 0;
	case 0x151:
		res = MsCheckMedia(initFile);
		if (res < 0) {
			Kprintf("mediasync.c:%s:Warning: MsCheckMediaFailed 0x%08x\n", "WaitEflash", res);
			res = 0;
		}
		return res;
	case 0x152:
	case 0x154:
	case 0x155:
	case 0x156:
		res = MsCheckMedia(initFile);
		if (res < 0) {
			Kprintf("mediasync.c:%s:MsCheckMediaFailed 0x%08x\n", "WaitEflash", res);
		}
		return res;
	default:
		Kprintf("mediasync.c:%s:unsupported apitype=0x%08x[%d]\n", "WaitEflash", apitype, apitype);
		return SCE_MEDIASYNC_ERROR_INVALID_MEDIA;
	}
}

/* === 0x0c78 WaitDiscEmu === */
static int WaitDiscEmu(void)
{
	int res = sceUmdActivate(1, "disc0:");
	if (res < 0) {
		Kprintf("mediasync.c:%s:Cannot activate UMD : 0x%08x\n", "WaitDiscEmu", res);
		return res;
	}
	const char *discImg = sceKernelInitDiscImage();
	if (discImg == NULL) {
		return SCE_MEDIASYNC_ERROR_INVALID_MEDIA;
	}
	res = sceUmdWaitDriveStat(0x29);
	if (res < 0) {
		Kprintf("mediasync.c:%s:Cannot wait UMD event : 0x%08x\n", "WaitDiscEmu", res);
		return res;
	}
	int status = sceUmdGetDriveStatus();
	if ((status & 0x20) == 0) {
		Kprintf("mediasync.c:%s:Not READABLE : 0x%08x\n", "WaitDiscEmu", status);
		return SCE_MEDIASYNC_ERROR_NO_MEDIUM;
	}

	uint32_t apitype = (uint32_t)sceKernelInitApitype();
	if (apitype == 0x123 || apitype == 0x125 || apitype == 0x170 || apitype == 0x171) {
		res = MsCheckMedia(discImg);
		if (res < 0) {
			Kprintf("mediasync.c:%s:MsCheckMedia failed 0x%08x\n", "WaitDiscEmu", res);
			return res;
		}
		return 0;
	}
	if (apitype - 0x112u < 4u) {
		SceGameInfo *gameInfo = sceKernelGetGameInfo();
		if (strncmp(gameInfo->umdDiscId, gameInfo->sfoDiscId, 10) != 0) {
			Kprintf("mediasync.c:%s:SCE_MEDIASYNC_ERROR_INVALID_MEDIA\n", "DiscCheckMedia");
			Kprintf("mediasync.c:%s:[%16s] and [%16s]\n", "DiscCheckMedia", gameInfo->umdDiscId, gameInfo->sfoDiscId);
			Kprintf("mediasync.c:%s:DiscCheckMedia failed 0x%08x\n", "WaitDiscEmu", SCE_MEDIASYNC_ERROR_INVALID_MEDIA);
			return SCE_MEDIASYNC_ERROR_INVALID_MEDIA;
		}
		Kprintf("mediasync.c:%s:UMD Media Check OK\n", "DiscCheckMedia");
		return 0;
	}
	return SCE_MEDIASYNC_ERROR_INVALID_MEDIA;
}

/* === 0x0e44 MsCallback === */
static int MsCallback(int count, int arg, void *common)
{
	(void)count;
	(void)common;
	g_msCallbackArg = arg;
	if (arg != 1) {
		Kprintf("mediasync.c:%s:EVENT, arg=0x%08x\n", "MsCallback", arg);
	}
	sceKernelSetEventFlag(g_msEvfId, 1);
	return 0;
}

/* === 0x0ea4 MsSystemFile === */
static int MsSystemFile(void *buf, SceSize size, SceGameInfo *gameInfo)
{
	SceSysfileContext *ctx;
	void *entryHandle;
	char discId[16];

	g_sfHeapId = -1;
	sceSystemFileSetAllocator(sf_malloc, sf_free);
	int res = sceSystemFileLoadAll2(buf, size, &ctx);
	if (res != 0) {
		Kprintf("mediasync.c:%s:sceSystemFileLoadAll2 failed 0x%08x, buf=0x%08x, size=0x%08x\n",
		        "MsSystemFile", res, (uint32_t)(uintptr_t)buf, size);
		return res;
	}
	res = sceSystemFileGetIndex(ctx, "DISC_ID", &entryHandle);
	if (res != 0) {
		Kprintf("mediasync.c:%s:sceSystemFileGetIndex failed (res=0x%x)\n", "MsSystemFile", res);
		return res;
	}
	res = sceSystemFileGetString(ctx, entryHandle, discId, sizeof(discId));
	if (res != 0) {
		Kprintf("mediasync.c:%s:failed to get value of 'DISC_ID' (ret=0x%x)\n", "MsSystemFile", res);
		return res;
	}

	int matched = 0;
	if (gameInfo->parentProductCode[0] != '\0' && strncmp(gameInfo->parentProductCode, discId, 10) == 0) {
		matched = 1;
	}
	if (!matched) {
		const char *expected = gameInfo->productCode;
		if (strncmp(expected, discId, 13) != 0) {
			const char *mapped = expected;
			if (strncmp(expected, "ULJS00167", 9) == 0 ||
			    strncmp(expected, "ULJS00168", 9) == 0 ||
			    strncmp(expected, "ULJS00169", 9) == 0) {
				mapped = "NPJH90038";
			}
			if (strncmp(mapped, discId, 13) != 0) {
				Kprintf("mediasync.c:%s:SCE_MEDIASYNC_ERROR_INVALID_MEDIA\n", "MsSystemFile");
				Kprintf("mediasync.c:%s:[%16s] and [%16s]\n", "MsSystemFile", mapped, discId);
				res = SCE_MEDIASYNC_ERROR_INVALID_MEDIA;
			}
		}
	}

	sceSystemFileExit(ctx);
	if (sceKernelDeleteHeap(g_sfHeapId) < 0) {
		Kprintf("mediasync.c:%s:sceKernelDeleteHeap failed 0x%08x\n", "MsSystemFile", res);
	}
	g_sfHeapId = -1;
	return res;
}

/* === 0x10e4 sf_malloc === */
static void *sf_malloc(SceSize size)
{
	if (g_sfHeapId < 0) {
		g_sfHeapId = sceKernelCreateHeap(1, 4096, 1, "SceKernelMediaSyncHeap");
		if (g_sfHeapId < 0) {
			Kprintf("mediasync.c:%s:sceKernelCreateHeap failed 0x%08x\n", "sf_malloc", g_sfHeapId);
			g_sfHeapId = -1;
			return NULL;
		}
	}
	return sceKernelAllocHeapMemory(g_sfHeapId, size);
}

/* === 0x118c sf_vpl_malloc === */
static void *sf_vpl_malloc(SceSize size)
{
	void *raw;
	SceUID vpl = sceKernelCreateVpl("sceLibSysfile", 2, 0, size + 0x120u, NULL);
	if (vpl < 0) {
		return NULL;
	}
	if (sceKernelTryAllocateVpl(vpl, size + sizeof(SceSysfileVplHeader), &raw) < 0) {
		return NULL;
	}
	SceSysfileVplHeader *hdr = (SceSysfileVplHeader *)raw;
	sysfileStrcpy(hdr->magic, g_vplTagMagic);
	hdr->vplId = vpl;
	return hdr + 1;
}

/* === 0x1228 sf_free === */
static int sf_free(void *ptr)
{
	sceKernelFreeHeapMemory(g_sfHeapId, ptr);
	return 0;
}

/* === 0x1254 sf_vpl_free === */
static int sf_vpl_free(void *ptr)
{
	SceSysfileVplHeader *hdr = ((SceSysfileVplHeader *)ptr) - 1;
	if (sysfileStrcmp((const uint8_t *)hdr->magic, (const uint8_t *)g_vplTagMagic) != 0) {
		return -1;
	}
	SceUID vpl = hdr->vplId;
	int res = sceKernelFreeVpl(vpl, hdr);
	if (vpl > 0) {
		sceKernelDeleteVpl(vpl);
	}
	return res;
}

/* === 0x12c0 mediasyncCheckFlash0Hash === */
static int mediasyncCheckFlash0Hash(void)
{
	SceIoDirent dir;
	uint8_t digest[16];
	char kdPath[16];
	int found = 0;

	sysfileStrcpy(kdPath, "flash0:/kd/");
	SceUID dfd = sceIoDopen(kdPath);
	if (dfd < 0) {
		return dfd;
	}

	while (sceIoDread(dfd, &dir) > 0) {
		uint32_t len = 0;
		while (dir.d_name[len] != '\0') {
			dir.d_name[len] = (char)tolower((int)(int8_t)dir.d_name[len]);
			len++;
		}
		sceKernelUtilsMd5Digest((const uint8_t *)dir.d_name, len, digest);
		for (uint32_t i = 0; i < 7; i++) {
			if (memcmp(digest, g_kdMd5Table[i], 16) == 0) {
				found = 1;
				break;
			}
		}
	}
	sceIoDclose(dfd);
	return found;
}

/* === 0x13e8 mediasyncApplyModmgrQuirks === */
static void mediasyncApplyModmgrQuirks(void)
{
	SceGameInfo *gameInfo = sceKernelGetGameInfo();
	if (gameInfo == NULL) {
		return;
	}
	const char *discId = gameInfo->productCode;

	g_modmgrCompatMode = 0;
	for (int i = 0; i < 3; i++) {
		if (strncmp(discId, g_modmgrModeTable[i].discId, 9) == 0) {
			g_modmgrCompatMode = (int)g_modmgrModeTable[i].value;
		}
	}

	g_modmgrCompatFixA = 0;
	if (strncmp(discId, "ULJM05023", 9) == 0) {
		g_modmgrCompatFixA = 1;
	}

	g_modmgrCompatFixB = 0;
	for (int i = 0; i < 5; i++) {
		if (strncmp(discId, g_modmgrFixBTable[i], 9) == 0) {
			g_modmgrCompatFixB = 1;
		}
	}
}

/* === 0x14dc mediasyncApplyGameQuirks === */
static void mediasyncApplyGameQuirks(void)
{
	mediasyncApplyIntrQuirks();
	mediasyncApplyUmdCacheQuirks();
	mediasyncApplyAudioQuirks();
	mediasyncApplyMeCoreCallQuirks();
	mediasyncApplyWlanQuirks();
	mediasyncApplyCtrlQuirks();
	mediasyncApplyIoQuirks();
	mediasyncApplyMsDevctlQuirks();
	mediasyncApplyModmgrQuirks();
	mediasyncApplyMeCoreVarQuirks();
	mediasyncApplyPowerQuirks();
	mediasyncApplyDmacQuirks();
	mediasyncApplyRtcQuirks();
	mediasyncApplyUsbQuirks();
	mediasyncApplyRegQuirks();
	mediasyncApplyMsfsQuirks();
}

static __inline__ __attribute__((always_inline)) void decodePackedUmdDiscId(const PackedUmdDiscEntry *entry, char *out)
{
	static const char kRegionLetters[5] = { 'A', 'E', 'J', 'K', 'U' };
	uint16_t bits = entry->prefixBits;
	out[0] = ((bits & 3u) == 0) ? 'U' : (char)0x80;

	uint32_t secondSel = (bits >> 2) & 3u;
	if (secondSel == 0) {
		out[1] = 'C';
	} else if (secondSel == 1) {
		out[1] = 'L';
	} else {
		out[1] = (char)0x80;
	}

	uint32_t regionSel = (bits >> 4) & 7u;
	out[2] = (regionSel < 5u) ? kRegionLetters[regionSel] : (char)0x80;

	uint32_t fourthSel = (bits >> 7) & 3u;
	if (fourthSel == 0) {
		out[3] = 'M';
	} else if (fourthSel == 1) {
		out[3] = 'S';
	} else {
		out[3] = (char)0x80;
	}

	out[4] = '-';
	for (int d = 0; d < 5; d++) {
		out[5 + d] = (char)(((entry->digitsBcd >> (4 * d)) & 0xfu) + '0');
	}
}

/* === 0x1570 mediasyncApplyUmdCacheQuirks === */
static int mediasyncApplyUmdCacheQuirks(void)
{
	char decoded[16];
	char curId[16];

	if (sceKernelSearchModuleByName("sceUmdCache_driver") < 0) {
		return 0;
	}

	sceKernelMemset(decoded, 0, 11);
	sceKernelMemset(curId, 0, 11);
	const char *umdId = sceUmdCache_driver_A21D8213();
	sceKernelMemcpy(curId, umdId, 10);

	if (sceKernelIsDevelopmentToolMode() != 0) {
		sceUmdCache_driver_DB97E432(1);
		sceUmd9660_driver_3CC9CE54();
		return 1;
	}

	for (int tableIdx = 0; tableIdx < 3; tableIdx++) {
		const PackedUmdDiscEntry *table;
		int count;
		if (tableIdx == 0) {
			table = g_umdDisableTable;
			count = 34;
		} else if (tableIdx == 1) {
			table = g_umdModeThreeTable;
			count = 42;
		} else {
			table = g_umdModeTwoTable;
			count = 8;
		}
		for (int i = 0; i < count; i++) {
			sceKernelMemset(decoded, 0, 10);
			decodePackedUmdDiscId(&table[i], decoded);
			if (memcmp(curId, decoded, 10) == 0) {
				if (tableIdx == 0) {
					sceUmdCache_driver_DB97E432(0);
					sceUmd9660_driver_FE3A8B67();
					return 0;
				}
				if (tableIdx == 1) {
					sceUmdCache_driver_DB97E432(3);
					sceUmd9660_driver_3CC9CE54();
					sceUmdCache_driver_576E0F06((int)table[i].profileId);
					return 3;
				}
				sceUmdCache_driver_DB97E432(2);
				sceUmd9660_driver_3CC9CE54();
				sceUmdCache_driver_576E0F06((int)table[i].profileId);
				return 2;
			}
		}
	}

	sceUmdCache_driver_DB97E432(1);
	sceUmd9660_driver_3CC9CE54();
	return 1;
}

/* === 0x186c mediasyncApplyWlanQuirks === */
static void mediasyncApplyWlanQuirks(void)
{
	SceGameInfo *gameInfo = sceKernelGetGameInfo();
	if (gameInfo == NULL) {
		return;
	}
	const char *discId = gameInfo->productCode;
	for (int i = 0; i < 9; i++) {
		if (strcmp(discId, g_wlanQuirkTable[i].discId) == 0) {
			sceWlanDrv_driver_EDD207B1((int)g_wlanQuirkTable[i].first, (const void *)(uintptr_t)g_wlanQuirkTable[i].second);
		}
	}
}

/* === 0x18e0 mediasyncApplyIntrQuirks === */
static void mediasyncApplyIntrQuirks(void)
{
	SceGameInfo *gameInfo = sceKernelGetGameInfo();
	if (gameInfo != NULL && strcmp(gameInfo->productCode, "ULJM05109") == 0) {
		InterruptManagerForKernel_E526B767(1);
	}
	int appType = sceKernelApplicationType();
	if (appType == 0x100 || appType == 0x110) {
		InterruptManagerForKernel_E526B767(0);
	} else if (appType == 0x300) {
		ThreadManForKernel_D366D35A();
		InterruptManagerForKernel_E526B767(0);
	}
}

/* === 0x1978 mediasyncApplyAudioQuirks === */
static void mediasyncApplyAudioQuirks(void)
{
	SceGameInfo *gameInfo = sceKernelGetGameInfo();
	if (gameInfo == NULL) {
		return;
	}
	const char *discId = gameInfo->productCode;
	for (int i = 0; i < 38; i++) {
		if (strncmp(discId, g_audioQuirkTable[i].discId, 16) == 0) {
			sceAudio_driver_95AE5A2B(g_audioQuirkTable[i].first, g_audioQuirkTable[i].second);
		}
	}
}

/* === 0x19f0 mediasyncApplyMeCoreCallQuirks === */
static void mediasyncApplyMeCoreCallQuirks(void)
{
	SceGameInfo *gameInfo = sceKernelGetGameInfo();
	if (gameInfo != NULL && strncmp(gameInfo->productCode, g_meCoreCallQuirkTable[0].discId, 16) == 0) {
		sceMeCore_driver_4CD5B5BF(g_meCoreCallQuirkTable[0].first, g_meCoreCallQuirkTable[0].second);
	}
}

/* === 0x1a48 mediasyncApplyCtrlQuirks === */
static void mediasyncApplyCtrlQuirks(void)
{
	SceGameInfo *gameInfo = sceKernelGetGameInfo();
	if (gameInfo == NULL) {
		return;
	}
	const char *discId = gameInfo->productCode;
	for (int i = 0; i < 14; i++) {
		if (strcmp(discId, g_ctrlQuirkTable[i].discId) == 0) {
			sceCtrl_driver_5886194C(g_ctrlQuirkTable[i].value);
			break;
		}
	}
}

/* === 0x1abc mediasyncApplyIoQuirks === */
static void mediasyncApplyIoQuirks(void)
{
	SceGameInfo *gameInfo = sceKernelGetGameInfo();
	if (gameInfo == NULL) {
		return;
	}
	const char *discId = gameInfo->productCode;
	if (strncmp(discId, "ULJS00167", 9) == 0 ||
	    strncmp(discId, "ULJS00168", 9) == 0 ||
	    strncmp(discId, "ULJS00169", 9) == 0) {
		g_ioDeletedError = (int)0x80020323;
	}
	if (strncmp(discId, "ULJM05957", 9) == 0 ||
	    strncmp(discId, "NPJH50491", 9) == 0) {
		g_ioAsyncDelayEnable = 1;
		g_ioAsyncDelayCount = 19;
	}
	g_msfsCompatDelayFlag = 0;
	for (int i = 0; i < 2; i++) {
		if (strncmp(discId, g_ioMsfsDelayTable[i], 9) == 0) {
			g_msfsCompatDelayFlag = 1;
		}
	}
}

/* === 0x1c00 mediasyncApplyMsDevctlQuirks === */
static void mediasyncApplyMsDevctlQuirks(void)
{
	SceGameInfo *gameInfo = sceKernelGetGameInfo();
	if (gameInfo == NULL) {
		return;
	}
	const char *discId = gameInfo->productCode;
	for (int i = 0; i < 28; i++) {
		if (strcmp(discId, g_msDevctlTable[i]) == 0) {
			uint32_t enable = 1;
			sceIoDevctl("fatms0:", 0x0242585a, &enable, 4, NULL, 0);
			if ((InitForKernel_9D33A110() == 0x50 && sceKernelApplicationType() == 0x200) ||
			    (InitForKernel_9D33A110() == 0x30 &&
			     (sceKernelInitApitype() == 0x132 || sceKernelInitApitype() == 0x133))) {
				sceIoDevctl("fatef0:", 0x0242585a, &enable, 4, NULL, 0);
			}
		}
	}
	for (int i = 0; i < 3; i++) {
		if (strncmp(discId, g_mscmhcDevctlTable[i], 9) == 0) {
			uint32_t enable = 1;
			sceIoDevctl("mscmhc0:", 0x02025808, &enable, 4, NULL, 0);
		}
	}
}

/* === 0x1db8 mediasyncApplyMeCoreVarQuirks === */
static void mediasyncApplyMeCoreVarQuirks(void)
{
	SceGameInfo *gameInfo = sceKernelGetGameInfo();
	if (gameInfo == NULL) {
		return;
	}
	const char *discId = gameInfo->productCode;

	g_meCoreCompatSingleA = 0;
	if (strncmp(discId, "NPJH60006", 9) == 0) {
		g_meCoreCompatSingleA = 1;
	}

	g_meCoreCompatListFlag = 0;
	for (int i = 0; i < 12; i++) {
		if (strncmp(discId, g_meCoreListTable[i], 9) == 0) {
			g_meCoreCompatListFlag = 1;
		}
	}

	g_meCoreCompatSingleB = 0;
	if (strncmp(discId, "NPJH50724", 9) == 0) {
		g_meCoreCompatSingleB = 1;
	}
}

/* === 0x1e98 mediasyncApplyPowerQuirks === */
static void mediasyncApplyPowerQuirks(void)
{
	SceGameInfo *gameInfo = sceKernelGetGameInfo();
	if (gameInfo == NULL) {
		return;
	}
	const char *discId = gameInfo->productCode;

	g_powerCompatDelayA = 0;
	for (int i = 0; i < 12; i++) {
		if (strncmp(discId, g_powerDelayATable[i].discId, 9) == 0) {
			g_powerCompatDelayA = (int)g_powerDelayATable[i].value;
		}
	}

	g_powerCompatFlagB = 0;
	for (int i = 0; i < 14; i++) {
		if (strncmp(discId, g_powerFlagBTable[i], 9) == 0) {
			g_powerCompatFlagB = 1;
		}
	}

	g_powerCompatFlagC = 0;
	for (int i = 0; i < 5; i++) {
		if (strncmp(discId, g_powerFlagCTable[i], 9) == 0) {
			g_powerCompatFlagC = 1;
		}
	}

	g_powerCompatDelayG = 0;
	g_powerCompatDelayH = 0;
	for (int i = 0; i < 2; i++) {
		if (strncmp(discId, g_powerPairGHTable[i].discId, 9) == 0) {
			g_powerCompatDelayG = (int)g_powerPairGHTable[i].first;
			g_powerCompatDelayH = (int)g_powerPairGHTable[i].second;
		}
	}

	g_powerCompatTableE = 0;
	for (int i = 0; i < 9; i++) {
		if (strncmp(discId, g_powerTableETable[i].discId, 9) == 0) {
			g_powerCompatTableE = (int)g_powerTableETable[i].value;
		}
	}

	g_powerCompatTableF = 0;
	for (int i = 0; i < 24; i++) {
		if (strncmp(discId, g_powerTableFTable[i].discId, 9) == 0) {
			g_powerCompatTableF = (int)g_powerTableFTable[i].value;
		}
	}

	g_powerCompatFlagJ = 0;
	for (int i = 0; i < 2; i++) {
		if (strncmp(discId, g_powerFlagJTable[i], 9) == 0) {
			g_powerCompatFlagJ = 1;
		}
	}

	g_powerCompatSingleD = 0;
	if (strncmp(discId, "ULES00639", 9) == 0) {
		g_powerCompatSingleD = 1;
	}

	g_powerCompatFlagI = 0;
	for (int i = 0; i < 13; i++) {
		if (strncmp(discId, g_powerFlagITable[i], 9) == 0) {
			g_powerCompatFlagI = 1;
		}
	}

	g_powerKermitMode = sceKermitPeripheral_driver_B93A88E2();

	g_powerCompatTableK = 0;
	for (int i = 0; i < 6; i++) {
		if (strncmp(discId, g_powerTableKTable[i].discId, 9) == 0) {
			g_powerCompatTableK = (int)g_powerTableKTable[i].value;
		}
	}
}

/* === 0x2168 mediasyncApplyDmacQuirks === */
static void mediasyncApplyDmacQuirks(void)
{
	SceGameInfo *gameInfo = sceKernelGetGameInfo();
	if (gameInfo == NULL) {
		return;
	}
	const char *discId = gameInfo->productCode;

	g_dmacCompatFlagC = 0;
	g_dmacCompatFlagD = 0;
	for (int i = 0; i < 2; i++) {
		if (strncmp(discId, g_dmacFlagCTable[i], 9) == 0) {
			g_dmacCompatFlagC = 1;
		}
	}

	g_dmacCompatFlagA = 0;
	if (strncmp(discId, "UCES01196", 9) == 0) {
		g_dmacCompatFlagA = 1;
	}

	g_dmacCompatModeB = 0;
	if (strncmp(discId, "UCES01196", 9) == 0) {
		g_dmacCompatModeB = 2;
	}
}

/* === 0x2254 mediasyncApplyRtcQuirks === */
static void mediasyncApplyRtcQuirks(void)
{
	SceGameInfo *gameInfo = sceKernelGetGameInfo();
	if (gameInfo == NULL) {
		return;
	}
	g_rtcCompatFlag = 0;
	if (strncmp(gameInfo->productCode, "NPUH10002", 9) == 0) {
		g_rtcCompatFlag = 1;
	}
}

/* === 0x22a4 mediasyncApplyUsbQuirks === */
static void mediasyncApplyUsbQuirks(void)
{
	SceGameInfo *gameInfo = sceKernelGetGameInfo();
	if (gameInfo == NULL) {
		return;
	}
	const char *discId = gameInfo->productCode;

	g_usbCompatModeA = 0;
	for (int i = 0; i < 2; i++) {
		if (strncmp(discId, g_usbModeATable[i].discId, 9) == 0) {
			g_usbCompatModeA = (int)g_usbModeATable[i].value;
		}
	}

	g_usbCompatModeB = 0;
	if (strncmp(discId, g_usbModeBTable[0].discId, 9) == 0) {
		g_usbCompatModeB = (int)g_usbModeBTable[0].value;
	}
}

/* === 0x2354 mediasyncApplyRegQuirks === */
static void mediasyncApplyRegQuirks(void)
{
	SceGameInfo *gameInfo = sceKernelGetGameInfo();
	if (gameInfo == NULL) {
		return;
	}
	const char *discId = gameInfo->productCode;

	g_regCompatDelayA = 0;
	for (int i = 0; i < 2; i++) {
		if (strncmp(discId, g_regDelayATable[i].discId, 9) == 0) {
			g_regCompatDelayA = (int)g_regDelayATable[i].value;
		}
	}

	g_regCompatDelayB = 0;
	if (strncmp(discId, g_regDelayBTable[0].discId, 9) == 0) {
		g_regCompatDelayB = (int)g_regDelayBTable[0].value;
	}
}

/* === 0x2404 mediasyncApplyMsfsQuirks === */
static void mediasyncApplyMsfsQuirks(void)
{
	SceGameInfo *gameInfo = sceKernelGetGameInfo();
	if (gameInfo == NULL) {
		return;
	}
	const char *discId = gameInfo->productCode;

	g_msfsCompatFlagEnable = 0;
	for (int i = 0; i < 3; i++) {
		if (strncmp(discId, g_msfsFlagEnableTable[i], 9) == 0) {
			g_msfsCompatFlagEnable = 1;
		}
	}

	g_msfsCompatAlwaysOne = 1;
	g_msfsCompatCacheDelay = 0;
	for (int i = 0; i < 2; i++) {
		if (strncmp(discId, g_msfsCacheDelayTable[i].discId, 9) == 0) {
			g_msfsCompatCacheDelay = (int)g_msfsCacheDelayTable[i].value;
		}
	}
}

/* === 0x24d8 mediasyncLoadNpEnvFromReg === */
static int mediasyncLoadNpEnvFromReg(void)
{
	SceRegParam reg;
	uint32_t regHandle;
	uint32_t catHandle;
	uint32_t keyHandle;
	uint32_t keyType;
	SceSize keySize;
	char envBuf[9];
	char outBuf[9];

	memset(&reg, 0, sizeof(reg));
	reg.regType = 1;
	reg.regMode = 1;
	reg.regAttr = 1;

	int res = sceRegOpenRegistry(&reg, 2, &regHandle);
	if (res < 0) {
		return res;
	}

	if (sceRegOpenCategory(regHandle, "/CONFIG/NP", 2, &catHandle) >= 0) {
		if (sceRegGetKeyInfo(catHandle, "env", &keyHandle, &keyType, &keySize) >= 0 &&
		    keyType == 3 && keySize < 10) {
			sceKernelMemset(envBuf, 0, sizeof(envBuf));
			if (sceRegGetKeyValue(catHandle, keyHandle, envBuf, 9) >= 0) {
				envBuf[keySize] = '\0';
				sceKernelMemset(outBuf, 0, sizeof(outBuf));
				mediasyncHashNpEnv(outBuf, envBuf);
				SysMemForKernel_A03CB480(outBuf);
			}
		}
		sceRegCloseCategory(catHandle);
	}

	return sceRegCloseRegistry(regHandle);
}

/* === 0x25f4 mediasyncHashNpEnv === */
static void mediasyncHashNpEnv(char *outBuf, const char *envStr)
{
	char work[128];
	uint8_t digest[16];

	if (envStr[0] == '\0') {
		sysfileStrcpy(outBuf, "invalid");
		return;
	}
	if (sceKermitPeripheral_driver_4A26B7C8() != 0) {
		strncpy(outBuf, envStr, 8);
		return;
	}

	sceKernelMemset(work, 0, sizeof(work));
	while (strlen(work) < 100) {
		strcat(work, envStr);
	}

	sceKernelUtilsMd5Digest((const uint8_t *)work, sizeof(work), digest);
	if (sceKermitPeripheral_driver_675DB2C1() != 0) {
		if (memcmp(digest, g_npEnvMd5Retail, 16) == 0 ||
		    memcmp(digest, g_npEnvMd5DevA, 16) == 0 ||
		    memcmp(digest, g_npEnvMd5DevB, 16) == 0) {
			strncpy(outBuf, envStr, 8);
			return;
		}
	} else {
		if (memcmp(digest, g_npEnvMd5Retail, 16) == 0) {
			strncpy(outBuf, envStr, 8);
			return;
		}
	}
	sysfileStrcpy(outBuf, "invalid");
}

/* === 0x2734 mediasyncLoadAdhocPrefixFromReg === */
static int mediasyncLoadAdhocPrefixFromReg(void)
{
	SceRegParam reg;
	uint32_t regHandle;
	uint32_t catHandle;
	uint32_t keyHandle;
	uint32_t keyType;
	SceSize keySize;
	char prefixBuf[16];

	memset(&reg, 0, sizeof(reg));
	reg.regType = 1;
	reg.regMode = 1;
	reg.regAttr = 1;

	int res = sceRegOpenRegistry(&reg, 2, &regHandle);
	if (res < 0) {
		return res;
	}

	if (sceRegOpenCategory(regHandle, "/CONFIG/NETWORK/ADHOC", 2, &catHandle) >= 0) {
		if (sceRegGetKeyInfo(catHandle, "ssid_prefix", &keyHandle, &keyType, &keySize) >= 0 &&
		    keyType == 3 && keySize != 3) {
			if (sceRegGetKeyValue(catHandle, keyHandle, prefixBuf, 4) >= 0) {
				prefixBuf[3] = '\0';
				sceWlanDrv_driver_EDD207B1(0, prefixBuf);
			}
		}
		sceRegCloseCategory(catHandle);
	}

	return sceRegCloseRegistry(regHandle);
}
