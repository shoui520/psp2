#include <stdint.h>
#include <stddef.h>

typedef int32_t SceUID;
typedef uint32_t SceSize;

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
	int dummy;
} SceIoDirent;

typedef struct RegParam {
	uint32_t regtype;
	char name[256];
	uint32_t namelen;
	uint32_t unk2;
	uint32_t unk3;
} RegParam;

typedef struct SceSysfileHeader {
	uint32_t magic;
	uint32_t version;
	uint32_t key_table_offset;
	uint32_t data_table_offset;
	uint32_t num_entries;
} SceSysfileHeader;

typedef struct SceSysfileEntry {
	uint16_t key_offset;
	uint8_t alignment;
	uint8_t data_type;
	uint32_t value_size;
	uint32_t total_size;
	uint32_t data_offset;
} SceSysfileEntry;

typedef struct SceSysfileCtx {
	uint32_t flags;
	uint32_t reserved;
	SceSysfileHeader *hdr;
	SceSize buf_size;
} SceSysfileCtx;

/* Imported functions */
extern SceUID sceKernelGetChunk(int chunk_id);
extern int sceKernelApplicationType(void);
extern int sceKernelInitApitype(void);
extern int InitForKernel_9D33A110(void);
extern char *sceKernelInitDiscImage(void);
extern char *sceKernelInitFileName(void);
extern int InterruptManagerForKernel_E526B767(int a0);
extern SceUID sceIoOpen(const char *file, int flags, int mode);
extern int64_t sceIoLseek(SceUID fd, int64_t offset, int whence);
extern int sceIoWrite(SceUID fd, const void *data, SceSize size);
extern int sceIoDevctl(const char *dev, uint32_t cmd, const void *indata, int inlen, void *outdata, int outlen);
extern int sceIoRead(SceUID fd, void *data, SceSize size);
extern int sceIoClose(SceUID fd);
extern SceUID sceIoDopen(const char *dirname);
extern int sceIoDread(SceUID fd, SceIoDirent *dir);
extern int sceIoDclose(SceUID fd);
extern int Kprintf(const char *fmt, ...);
extern int sceKernelDipsw(int bit);
extern int sceKernelIsDevelopmentToolMode(void);
extern SceUID sceKernelSearchModuleByName(const char *name);
extern int SysMemForKernel_13EE28DA(int val);
extern void *sceKernelMemcpy(void *dst, const void *src, SceSize size);
extern void *sceKernelAllocHeapMemory(SceUID heapid, SceSize size);
extern SceUID sceKernelCreateHeap(SceUID partid, SceSize size, int attr, const char *name);
extern SceUID sceKernelAllocPartitionMemory(SceUID partid, const char *name, int type, SceSize size, void *addr);
extern int sceKernelFreeHeapMemory(SceUID heapid, void *block);
extern void *sceKernelMemset(void *dst, int val, SceSize size);
extern int SysMemForKernel_A03CB480(const char *env);
extern int sceKernelFreePartitionMemory(SceUID blockid);
extern int sceKernelDeleteHeap(SceUID heapid);
extern uint8_t *sceKernelGetGameInfo(void);
extern void *sceKernelGetBlockHeadAddr(SceUID blockid);
extern void *memset(void *s, int c, size_t n);
extern int tolower(int c);
extern char *strcat(char *dst, const char *src);
extern size_t strlen(const char *s);
extern int strncmp(const char *s1, const char *s2, size_t n);
extern int memcmp(const void *s1, const void *s2, size_t n);
extern char *strncpy(char *dst, const char *src, size_t n);
extern int strcmp(const char *s1, const char *s2);
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
extern int sceAudio_driver_95AE5A2B(uint32_t a0, uint32_t a1);
extern int sceCtrl_driver_5886194C(uint32_t a0);
extern int sceKermitPeripheral_driver_4A26B7C8(uint32_t a0);
extern int sceKermitPeripheral_driver_675DB2C1(uint32_t a0);
extern int sceKermitPeripheral_driver_B93A88E2(void);
extern int sceMeCore_driver_4CD5B5BF(uint32_t a0, uint32_t a1);
extern int sceRegCloseRegistry(uint32_t reg_handle);
extern int sceRegOpenCategory(uint32_t reg_handle, const char *name, int mode, uint32_t *cat_handle);
extern int sceRegGetKeyInfo(uint32_t cat_handle, const char *name, uint32_t *key_handle, uint32_t *type, SceSize *size);
extern int sceRegOpenRegistry(RegParam *reg, int mode, uint32_t *reg_handle);
extern int sceRegGetKeyValue(uint32_t cat_handle, uint32_t key_handle, void *buf, SceSize size);
extern int sceRegCloseCategory(uint32_t cat_handle);
extern int sceUmdWaitDriveStat(int stat);
extern int sceUmdCheckMedium(void);
extern int sceUmdGetDriveStatus(void);
extern int sceUmdActivate(int unit, const char *drive);
extern int sceUmd9660_driver_3CC9CE54(int a0);
extern int sceUmd9660_driver_FE3A8B67(int a0);
extern int sceUmdCache_driver_576E0F06(int a0);
extern int sceUmdCache_driver_A21D8213(int a0);
extern int sceUmdCache_driver_DB97E432(int a0);
extern int sceWlanDrv_driver_EDD207B1(int cmd, const void *arg);

/* Imported variables */
extern int g_deleted_error;
extern int IoFileMgrForKernel_var_E6F7A8DF;
extern int IoFileMgrForKernel_var_F659661B;
extern int ModuleMgrForKernel_var_57F4B35E;
extern int ModuleMgrForKernel_var_99BFFDB8;
extern int ModuleMgrForKernel_var_DB08B7FF;
extern int sceDmacplus_driver_var_374689B0;
extern int sceDmacplus_driver_var_9D3DDDF0;
extern int sceDmacplus_driver_var_AA2E7E54;
extern int sceDmacplus_driver_var_C10B947A;
extern int sceKermitMsfs_driver_var_7668D90C;
extern int sceKermitMsfs_driver_var_93B61A29;
extern int sceKermitMsfs_driver_var_BE954163;
extern int sceKermitMsfs_driver_var_C1B4213A;
extern int sceMeCore_driver_var_385A4CC6;
extern int sceMeCore_driver_var_61FF4837;
extern int sceMeCore_driver_var_AB354CA1;
extern int scePower_driver_var_07FC869E;
extern int scePower_driver_var_18DFA43A;
extern int scePower_driver_var_2A28BB48;
extern int scePower_driver_var_2EDA246C;
extern int scePower_driver_var_3080D4C0;
extern int scePower_driver_var_33399B15;
extern int scePower_driver_var_4A03DA50;
extern int scePower_driver_var_8B317890;
extern int scePower_driver_var_93E16800;
extern int scePower_driver_var_94FA3195;
extern int scePower_driver_var_C7998FA4;
extern int scePower_driver_var_E59CA666;
extern int sceReg_driver_var_5105E3FF;
extern int sceReg_driver_var_867D5F60;
extern int sceRtc_driver_var_5A64FA55;
extern int sceUsb_driver_var_3EA085BF;
extern int sceUsb_driver_var_A942D291;

/* Forward declarations */
static int __attribute__((noinline)) WaitDisc(void);
static int __attribute__((noinline)) WaitMs(void);
static int __attribute__((noinline)) MsCheckMedia(const char *filename);
static int __attribute__((noinline)) WaitEflash(void);
static int __attribute__((noinline)) WaitDiscEmu(void);
static int __attribute__((noinline)) MsCallback(int count, int arg, void *common);
static int __attribute__((noinline)) MsSystemFile(void *buf, SceSize size, uint8_t *game_info);
static void * __attribute__((noinline)) sf_malloc(void *ctx, SceSize size);
static void * __attribute__((noinline)) sf_vpl_malloc(void *ctx, SceSize size);
static void __attribute__((noinline)) sf_free(void *ctx, void *ptr);
static int __attribute__((noinline)) sf_vpl_free(void *ctx, void *ptr);
static int __attribute__((noinline)) sub_12c0(void);
static void __attribute__((noinline)) sub_13e8(const char *disc_id);
static int __attribute__((noinline)) sub_14dc(void);
static int __attribute__((noinline)) sub_1570(const char *disc_id);
static int __attribute__((noinline)) sub_186c(const char *disc_id);
static int __attribute__((noinline)) sub_18e0(const char *disc_id);
static int __attribute__((noinline)) sub_1978(const char *disc_id);
static int __attribute__((noinline)) sub_19f0(const char *disc_id);
static int __attribute__((noinline)) sub_1a48(const char *disc_id);
static void __attribute__((noinline)) sub_1abc(const char *disc_id);
static int __attribute__((noinline)) sub_1c00(const char *disc_id);
static void __attribute__((noinline)) sub_1db8(const char *disc_id);
static int __attribute__((noinline)) sub_1e98(const char *disc_id);
static void __attribute__((noinline)) sub_2168(const char *disc_id);
static void __attribute__((noinline)) sub_2254(const char *disc_id);
static void __attribute__((noinline)) sub_22a4(const char *disc_id);
static void __attribute__((noinline)) sub_2354(const char *disc_id);
static void __attribute__((noinline)) sub_2404(const char *disc_id);
static int __attribute__((noinline)) sub_24d8(void);
static void __attribute__((noinline)) sub_25f4(char *out_buf, const char *env_str);
static int __attribute__((noinline)) sub_2734(void);

static int __attribute__((noinline, used)) sub_2824(SceSysfileCtx *ctx, int index, SceSysfileEntry **out_entry);
static int __attribute__((noinline, used)) sub_2980(SceSysfileCtx *ctx, int index, void *buf, SceSize buf_size);
static int __attribute__((noinline, used)) sub_2a14(const void *buf, SceSize size);
static int __attribute__((noinline, used)) sub_2bd0(void *buf, SceSize size, SceSysfileCtx **out_ctx);
static int __attribute__((noinline, used)) sub_2c80(SceSysfileCtx *ctx);
static int __attribute__((noinline, used)) sub_2d24(SceSysfileCtx *ctx, uint32_t *out_version, uint32_t *out_count);
static int __attribute__((noinline, used)) sub_2ed4(SceSysfileCtx *ctx, int index, void *out_val);
static int __attribute__((noinline, used)) sub_2f8c(SceSysfileCtx *ctx, int index, void **out_ptr);
static int __attribute__((noinline, used)) sub_3054(SceSysfileCtx *ctx, int index, void *buf, SceSize buf_size);
static int __attribute__((noinline, used)) sub_30f4(SceSysfileCtx *ctx, int index, char *key_buf, SceSize key_buf_size, uint32_t *out_type, SceSize *out_val_size, SceSize *out_total_size);
static int __attribute__((noinline, used)) sub_3340(SceSysfileCtx *ctx, const char *key, int *out_index);
static int __attribute__((noinline, used)) sub_3494(SceSysfileCtx *ctx, const char *key, int start_index, int *out_index);
static int __attribute__((noinline, used)) sub_35e4(const void *a, const void *b);
static int __attribute__((noinline, used)) sub_3628(const void *a, const void *b, SceSize len);
static SceSize __attribute__((noinline, used)) sub_364c(const char *s, SceSize max_len);
static int __attribute__((noinline, used)) sub_3680(void *(*alloc_fn)(void *, SceSize), void (*free_fn)(void *, void *));
static void * __attribute__((noinline, used)) sub_36b4(SceSize size);
static void __attribute__((noinline, used)) sub_3708(void *ptr);
static void * __attribute__((noinline, used)) sub_3734(void *dst, const void *src, SceSize size);
static void * __attribute__((noinline, used)) sub_37e0(void *dst, int val, SceSize size);
static char * __attribute__((noinline, used)) sub_3914(char *dst, const char *src);

/* Globals */
static void *(*g_sf_alloc_fn)(void *, SceSize);
static int (*g_sf_free_fn)(void *, void *);

static SceUID g_ms_evfid;
static SceUID g_ef_evfid;
static SceUID g_vpl_uid;
static SceUID g_heap_id;
static uint32_t g_dummy_chunk[6];
static void *g_sf_alloc_ctx;
static void *g_sf_free_ctx;

/* Tables */
static const uint32_t g_kd_md5_table[7][4] = {
	{ 0xff785cf8U, 0x8c134305U, 0xd6702e16U, 0xb69b8745U },
	{ 0xbf773900U, 0x05c60c22U, 0xada49759U, 0xd85d93e4U },
	{ 0x3b0a5785U, 0xe16461f6U, 0xfec672ecU, 0x792ba3b1U },
	{ 0xa43476dbU, 0x6e54eef2U, 0x6c8dc6b7U, 0x8bc91913U },
	{ 0xe7baf988U, 0x013634e6U, 0x95cb1da8U, 0x1626e3f3U },
	{ 0xb48235f7U, 0x0fc7982eU, 0xbbf2d737U, 0x28b03f50U },
	{ 0x7713e0e7U, 0xd55e9a4dU, 0xdd28f173U, 0x381934c2U },
};

typedef struct PackedDiscEntry {
	uint32_t hi;
	uint32_t lo;
} PackedDiscEntry;

static const PackedDiscEntry g_umd_table1[34] = {
	{ 0x00840000U, 0x00076024U },
	{ 0x00940000U, 0x00074600U },
	{ 0x00c40000U, 0x00071201U },
	{ 0x00940000U, 0x00074100U },
	{ 0x00c40000U, 0x00042001U },
	{ 0x00c40000U, 0x00061201U },
	{ 0x00a40000U, 0x00031100U },
	{ 0x00940000U, 0x00098100U },
	{ 0x00940000U, 0x00009100U },
	{ 0x00940000U, 0x00019100U },
	{ 0x00b40000U, 0x00073064U },
	{ 0x00c40000U, 0x00054001U },
	{ 0x00840000U, 0x00099024U },
	{ 0x00c40000U, 0x00048101U },
	{ 0x00b40000U, 0x00093164U },
	{ 0x00c40000U, 0x00070301U },
	{ 0x00940000U, 0x00050900U },
	{ 0x00940000U, 0x00060900U },
	{ 0x00c40000U, 0x00007001U },
	{ 0x00940000U, 0x00018200U },
	{ 0x00a40000U, 0x00006000U },
	{ 0x00a40000U, 0x00041100U },
	{ 0x00240000U, 0x00088150U },
	{ 0x00240000U, 0x00007050U },
	{ 0x00800000U, 0x00074104U },
	{ 0x00c00000U, 0x00076689U },
	{ 0x00c40000U, 0x00085101U },
	{ 0x00c40000U, 0x00002001U },
	{ 0x00a00000U, 0x00081001U },
	{ 0x00940000U, 0x00053100U },
	{ 0x00800000U, 0x00052004U },
	{ 0x00a40000U, 0x00095000U },
	{ 0x00c40000U, 0x00053201U },
	{ 0x00940000U, 0x00004700U },
};

static const PackedDiscEntry g_umd_table2[42] = {
	{ 0x00240001U, 0x00036050U },
	{ 0x00c40002U, 0x00024001U },
	{ 0x00a00003U, 0x00016001U },
	{ 0x00a40004U, 0x00024000U },
	{ 0x00240005U, 0x00078050U },
	{ 0x00b40006U, 0x00051064U },
	{ 0x00a00007U, 0x00063001U },
	{ 0x00900008U, 0x00025100U },
	{ 0x00c40009U, 0x00059001U },
	{ 0x0024000aU, 0x00080150U },
	{ 0x00c4000bU, 0x00081301U },
	{ 0x0024000cU, 0x00057050U },
	{ 0x00c4000dU, 0x00076101U },
	{ 0x0094000eU, 0x00024500U },
	{ 0x0084000fU, 0x00012124U },
	{ 0x00c40010U, 0x00069201U },
	{ 0x00240011U, 0x00064050U },
	{ 0x00240012U, 0x00023150U },
	{ 0x00b40013U, 0x00002164U },
	{ 0x00c40014U, 0x00096101U },
	{ 0x00940015U, 0x00067400U },
	{ 0x00240016U, 0x00081050U },
	{ 0x00a00017U, 0x00001001U },
	{ 0x00800018U, 0x00081004U },
	{ 0x00200019U, 0x00020459U },
	{ 0x00c0001aU, 0x00032689U },
	{ 0x00b0001bU, 0x00011054U },
	{ 0x00a0001cU, 0x00072001U },
	{ 0x00a4001dU, 0x00027000U },
	{ 0x00a4001eU, 0x00037000U },
	{ 0x00b4001fU, 0x00019064U },
	{ 0x00a40020U, 0x00067000U },
	{ 0x00a40021U, 0x00028000U },
	{ 0x00240022U, 0x00062050U },
	{ 0x00a00023U, 0x00034001U },
	{ 0x00240024U, 0x00019050U },
	{ 0x00240025U, 0x00078250U },
	{ 0x00940026U, 0x00014800U },
	{ 0x00c40027U, 0x00077201U },
	{ 0x00840028U, 0x00081124U },
	{ 0x00b40029U, 0x00055164U },
	{ 0x00a0002aU, 0x00010001U },
};

static const PackedDiscEntry g_umd_table3[8] = {
	{ 0x0020002bU, 0x00010459U },
	{ 0x0024002cU, 0x00058150U },
	{ 0x0094002dU, 0x00086400U },
	{ 0x00c4002eU, 0x00074101U },
	{ 0x00c4002fU, 0x00012201U },
	{ 0x00940030U, 0x00099600U },
	{ 0x00b40031U, 0x00067064U },
	{ 0x00840032U, 0x00069024U },
};

typedef struct DiscPair3 {
	const char *id;
	uint32_t a;
	uint32_t b;
} DiscPair3;

typedef struct DiscPair2 {
	const char *id;
	uint32_t a;
} DiscPair2;

static const DiscPair3 g_wlan_table[9] = {
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

static const DiscPair3 g_audio_table[38] = {
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

static const DiscPair3 g_mecore_call_table[1] = {
	{ "NPJH50239", 1, 3500 },
};

static const DiscPair2 g_ctrl_table[14] = {
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

static const char * const g_io_table1[2] = {
	"ULUS10408", "ULES01215"
};

static const char * const g_ms_devctl_table[28] = {
	"UCKS45124", "UCJS10104", "UCUS98743", "UCES01250",
	"UCAS40266", "ULJS00224", "NPJH50184", "ULUS10466",
	"ULES01376", "ULAS42214", "ULKS46235", "ULJS00202",
	"ULUS10457", "ULES01298", "UCKS45126", "ULJS00217",
	"ULJM05519", "ULJM05489", "NPJH50040", "UCKS45140",
	"ULUS10512", "ULJS00238", "ULJS00236", "ULUS10518",
	"ULAS42231", "ULES01407", "ULJM05542", "ULJM05544"
};

static const char * const g_mscmhc_devctl_table[3] = {
	"NPUG80329", "UCES00044", "NPEG00025"
};

static const DiscPair2 g_modmgr_table1[3] = {
	{ "UCJS10007", 1 },
	{ "ULJM05029", 2 },
	{ "UCJS10003", 3 },
};

static const char * const g_modmgr_table2[1] = {
	"ULJM05023"
};

static const char * const g_modmgr_table3[14] = {
	"ULUS10273", "ULES00811", "NPEZ00026", "NPUZ00048",
	"NPEZ00100", "NPJH60006", "ULJM05299", "NPJH50230",
	"NPJH50245", "NPJH50423", "NPEZ00319", "NPUZ00188",
	"NPEZ00354", "NPUZ00214"
};

static const char * const g_mecore_var1_table[4] = {
	"UCJS10007", "UCUS98612", "UCES00001", "ULJS00216"
};

static const DiscPair2 g_mecore_var2_table[13] = {
	{ "NPJH50724", 0 },
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

static const char * const g_mecore_var3_table[1] = {
	"NPJH50245"
};

static const char * const g_power_table1[4] = {
	"UCJS10052", "UCJS18030", "UCUS98633", "UCES00420"
};

static const char * const g_power_table2[5] = {
	"NPJH50483", "NPJG00045", "UCES01312", "UCUS98740", "NPHG00025"
};

static const char * const g_power_table3[9] = {
	"ULJM05299", "NPJH50230", "NPJH50423", "ULJS00216",
	"ULJM05824", "NPJH50280", "UCUS98640", "ULES01019",
	"ULUS10331"
};

static const DiscPair3 g_power_table4[2] = {
	{ "ULUS10237", 300, 200 },
	{ "ULES00770", 300, 200 },
};

static const DiscPair2 g_power_table5[5] = {
	{ "ULJM05494", 1 },
	{ "ULUS10462", 1 },
	{ "ULES01275", 1 },
	{ "ULES00636", 1 },
	{ "ULUS10223", 1 },
};

static const DiscPair2 g_power_table6[4] = {
	{ "NPEZ00190", 4 },
	{ "ULJM05251", 2 },
	{ "ULUS10186", 2 },
	{ "NPJG00111", 1 },
};

static const DiscPair2 g_power_table7[24] = {
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

static const char * const g_power_table8[2] = {
	"NPEZ00354", "NPUZ00214"
};

static const char * const g_dmac_table1[11] = {
	"ULES00639", "UCUS98640", "ULUS10217", "ULES00647",
	"ULJS00012", "UCES00018", "UCUS98687", "ULES00798",
	"ULKS46082", "ULKS46083", "ULKS46033"
};

static const char * const g_dmac_table2[3] = {
	"ULKS46090", "ULKS46098", "ULKS46099"
};

static const DiscPair2 g_dmac_table3[6] = {
	{ "ULUS10552", 100000 },
	{ "NPJH50568", 300000 },
	{ "NPJH50569", 300000 },
	{ "NPJH50618", 300000 },
	{ "NPUZ00072", 300000 },
	{ "NPEZ00158", 300000 },
};

static const char * const g_rtc_table[3] = {
	"ULJM05836", "ULUS10538", "UCES01196"
};

static const DiscPair2 g_usb_table1[1] = {
	{ "UCES01196", 2 },
};

static const char * const g_usb_table2[1] = {
	"NPUH10002"
};

static const DiscPair2 g_reg_table1[2] = {
	{ "UCJS10078", 1 },
	{ "NPUG81000", 2 },
};

static const DiscPair2 g_reg_table2[1] = {
	{ "NPUG81000", 2 },
};

static const DiscPair2 g_msfs_table1[3] = {
	{ "ULUS10214", 500 },
	{ "ULJM05958", 240000 },
	{ "ULUS10214", 30000 },
};

static const char * const g_msfs_table2[3] = {
	"ULUS10273", "ULES00811", "ULJS00093"
};

static const DiscPair2 g_msfs_table3[2] = {
	{ "ULJM06022", 2000 },
	{ "ULJM05053", 2000 },
};

static const uint32_t g_np_env_md5_1[4] = {
	0x22368ffdU, 0x4d9bad8eU, 0x5b6dad92U, 0xcdf56413U
};

static const uint32_t g_np_env_md5_2[4] = {
	0x8ee5ef17U, 0x9330556fU, 0xc956e857U, 0x65b31c35U
};

static const uint32_t g_np_env_md5_3[4] = {
	0xf91ddd4aU, 0xe8ca515aU, 0x37778db8U, 0xcc18ff71U
};

static const char g_sf_XorTable[24] = {
	0x7c, 0x79, 0x72, 0x3d, 0x60, 0x63, 0x60, 0x30,
	0x7e, 0x7b, 0x74, 0x85, 0x8b, 0x85, 0x78, 0x7b,
	0x7e, 0x77, 0x32, 0x32, 0x42, 0x42, 0x42, 0x42
};

int module_start(SceSize args, void *argp)
{
	int apitype;
	const char *fn;
	int s0 = 0;
	uint32_t s1 = 0;

	(void)args;
	(void)argp;

	apitype = sceKernelInitApitype();
	fn = sceKernelInitFileName();

	switch (apitype) {
	case 0x110:
	case 0x111:
	case 0x120:
	case 0x121:
	case 0x122:
		if (fn != NULL && strncmp(fn, "disc", 4) == 0)
			s1 = 0x01;
		break;
	case 0x112:
	case 0x113:
	case 0x123:
		if (fn != NULL && strncmp(fn, "disc", 4) == 0)
			s1 = 0x82;
		break;
	case 0x114:
	case 0x115:
	case 0x125:
		if (fn != NULL && strncmp(fn, "disc", 4) == 0)
			s1 = 0x88;
		break;
	case 0x116:
	case 0x160:
		s1 = 0x03;
		break;
	case 0x117:
	case 0x170:
		s1 = 0x82;
		break;
	case 0x118:
		s1 = 0x09;
		break;
	case 0x119:
	case 0x171:
		s1 = 0x88;
		break;
	case 0x140:
	case 0x141:
	case 0x143:
	case 0x144:
	case 0x145:
		if (fn != NULL) {
			if (strncmp(fn, "ms", 2) == 0)
				s1 = 0x02;
			else if (strncmp(fn, "flash3", 6) == 0)
				s1 = 0x04;
		}
		break;
	case 0x151:
	case 0x152:
	case 0x154:
	case 0x155:
	case 0x156:
		if (fn != NULL) {
			if (strncmp(fn, "ef", 2) == 0)
				s1 = 0x08;
			else if (strncmp(fn, "flash3", 6) == 0)
				s1 = 0x04;
		}
		break;
	default:
		break;
	}

	if (s1 & 0x01)
		s0 = WaitDisc();

	if (s0 >= 0 && (s1 & 0x02))
		s0 = WaitMs();

	if (s0 >= 0 && (s1 & 0x08))
		s0 = WaitEflash();

	if (s1 & 0x04)
		s0 = 0;

	if (s0 >= 0 && (s1 & 0x80))
		s0 = WaitDiscEmu();

	if (s0 < 0) {
		SceUID chunk = sceKernelGetChunk(0);
		uint32_t *ptr;
		if (chunk < 0)
			ptr = g_dummy_chunk;
		else
			ptr = (uint32_t *)sceKernelGetBlockHeadAddr(chunk);
		ptr[5] = (uint32_t)s0;
	}

	sub_12c0();

	if (sceKernelDipsw(0x34) == 1 && (sceKernelGetGPI() & 1) != 0)
		SysMemForKernel_13EE28DA(1);
	else
		SysMemForKernel_13EE28DA(0);

	sub_14dc();
	sub_24d8();
	sub_2734();
	return 1;
}

static int WaitDisc(void)
{
	int res = 0;
	int status;
	uint8_t *game_info;

	if (sceUmdCheckMedium() == 0) {
		res = sceUmdActivate(1, "disc0:");
		if (res < 0) {
			Kprintf("mediasync.c:%s:sceUmdActivate failed 0x%08x\n", "WaitDisc", res);
			return res;
		}
	}

	res = sceUmdWaitDriveStat(0x29);
	if (res < 0) {
		Kprintf("mediasync.c:%s:sceUmdWaitDriveStat failed 0x%08x\n", "WaitDisc", res);
		return res;
	}

	status = sceUmdGetDriveStatus();
	if ((status & 0x20) == 0) {
		Kprintf("mediasync.c:%s:sceUmdGetDriveStatus:0x%08x\n", "WaitDisc", status);
		return (status >= 0) ? (int)0x8001007b : status;
	}

	game_info = sceKernelGetGameInfo();
	if (game_info != NULL && game_info[0x18] != 0) {
		if (strncmp((const char *)(game_info + 8), (const char *)(game_info + 0x18), 10) == 0) {
			Kprintf("mediasync.c:%s:UMD Media Check OK\n", "DiscCheckMedia");
			return 0;
		}
		res = (int)0x80020464;
		Kprintf("mediasync.c:%s:SCE_MEDIASYNC_ERROR_INVALID_MEDIA\n", "DiscCheckMedia");
		Kprintf("mediasync.c:%s:[%16s] and [%16s]\n", "DiscCheckMedia", game_info + 8, game_info + 0x18);
		Kprintf("mediasync.c:%s:DiscCheckMedia failed 0x%08x\n", "WaitDisc", res);
	}
	return res;
}

static int WaitMs(void)
{
	SceUID cbid;
	int res;
	uint32_t state = 0;
	uint32_t timeout;
	int apitype;

	g_ms_evfid = sceKernelCreateEventFlag("SceMediaSyncEvMs", 0, 0, NULL);
	if (g_ms_evfid < 0) {
		Kprintf("mediasync.c:%s:sceKernelCreateEventFlag failed 0x%08x\n", "WaitMs", g_ms_evfid);
		return g_ms_evfid;
	}

	cbid = sceKernelCreateCallback("SceMediaSyncMs", MsCallback, (void *)0);
	if (cbid < 0) {
		res = cbid;
		goto cleanup_evf;
	}

	res = sceIoDevctl("fatms0:", 0x02415821, &cbid, 4, NULL, 0);
	if (res < 0) {
		Kprintf("mediasync.c:%s:sceFatmsRegisterNotifyCallback failed 0x%08x\n", "WaitMs", res);
		goto cleanup_cb;
	}

	while (1) {
		res = sceIoDevctl("fatms0:", 0x02425823, NULL, 0, &state, 4);
		if (res >= 0 && (state & 1) != 0)
			break;
		timeout = 10000000;
		res = sceKernelWaitEventFlagCB(g_ms_evfid, 1, 0x11, NULL, &timeout);
		if (res < 0)
			goto cleanup_unreg;
	}

	apitype = sceKernelInitApitype();
	switch (apitype) {
	case 0x112:
	case 0x113:
	case 0x116:
	case 0x117:
	case 0x140:
		res = MsCheckMedia(sceKernelInitFileName());
		if (res < 0)
			Kprintf("mediasync.c:%s:MsCheckMediaFailed 0x%08x\n", "WaitMs", res);
		break;
	case 0x141:
	case 0x143:
	case 0x144:
	case 0x145:
		res = MsCheckMedia(sceKernelInitFileName());
		if (res < 0)
			Kprintf("mediasync.c:%s:Warning: MsCheckMediaFailed 0x%08x\n", "WaitMs", res);
		res = 0;
		break;
	case 0x123:
	case 0x160:
	case 0x170:
		break;
	default:
		Kprintf("mediasync.c:%s:unsupported apitype=0x%08x[%d]\n", "WaitMs", apitype, apitype);
		break;
	}

cleanup_unreg:
	{
		int r2 = sceIoDevctl("fatms0:", 0x02415822, &cbid, 4, NULL, 0);
		if (r2 < 0)
			Kprintf("mediasync.c:%s:sceFatmsUnRegisterNotifyCallback failed 0x%08x\n", "WaitMs", r2);
	}
cleanup_cb:
	{
		int r2 = sceKernelDeleteCallback(cbid);
		if (r2 < 0)
			Kprintf("mediasync.c:%s:sceKernelDeleteCallback failed 0x%08x\n", "WaitMs", r2);
	}
cleanup_evf:
	{
		int r2 = sceKernelDeleteEventFlag(g_ms_evfid);
		if (r2 < 0)
			Kprintf("mediasync.c:%s:sceKernelDeleteEventFlag failed 0x%08x\n", "WaitMs", r2);
	}
	return res;
}

static int MsCheckMedia(const char *filename)
{
	uint8_t *game_info;
	SceUID work1_id;
	uint32_t *work1;
	SceUID fd;
	int res;
	uint32_t sfo_size;
	SceUID work2_id;
	void *work2;

	game_info = sceKernelGetGameInfo();
	if (game_info == NULL)
		return 0;

	if (filename == NULL) {
		Kprintf("mediasync.c:%s:Specify NULL check file\n", "MsCheckMedia");
		return (int)0x80020001;
	}

	work1_id = sceKernelAllocPartitionMemory(1, "SceMdiaSync:work1", 1, 0x28, NULL);
	if (work1_id < 0) {
		Kprintf("mediasync.c:%s:work_id1 allocation failed 0x%08x\n", "MsCheckMedia", work1_id);
		return work1_id;
	}

	work1 = (uint32_t *)sceKernelGetBlockHeadAddr(work1_id);
	fd = sceIoOpen(filename, 1, 0x1ff);
	if (fd < 0) {
		Kprintf("mediasync.c:%s:cannot open [%s] 0x%08x\n", "MsCheckMedia", filename, fd);
		res = fd;
		goto free_work1;
	}

	res = sceIoRead(fd, work1, 0x28);
	if (res != 0x28) {
		Kprintf("mediasync.c:%s:Header Read error 0x%08x\n", "MsCheckMedia", res);
		if (res >= 0)
			res = (int)0x80020001;
		sceIoClose(fd);
		goto free_work1;
	}

	if (((uint8_t *)work1)[0] != 0x00 ||
	    ((uint8_t *)work1)[1] != 'P' ||
	    ((uint8_t *)work1)[2] != 'B' ||
	    ((uint8_t *)work1)[3] != 'P') {
		Kprintf("mediasync.c:%s:Invalid Format File\n", "MsCheckMedia");
		sceIoClose(fd);
		res = (int)0x80020001;
		goto free_work1;
	}

	sfo_size = work1[3] - work1[2];
	work2_id = sceKernelAllocPartitionMemory(1, "SceMdiaSync:work2", 1, sfo_size, NULL);
	if (work2_id < 0) {
		Kprintf("mediasync.c:%s:work_id2 allocation failed 0x%08x\n", "MsCheckMedia", work2_id);
		sceIoClose(fd);
		res = work2_id;
		goto free_work1;
	}

	work2 = sceKernelGetBlockHeadAddr(work2_id);
	res = (int)sceIoLseek(fd, (int64_t)(int32_t)work1[2], 0);
	if (res < 0) {
		Kprintf("mediasync.c:%s:seek failed 0x%08x\n", "MsCheckMedia", res);
		sceIoClose(fd);
		goto free_work2;
	}

	res = sceIoRead(fd, work2, sfo_size);
	if ((uint32_t)res != sfo_size) {
		Kprintf("mediasync.c:%s:read failed 0x%08x\n", "MsCheckMedia", res);
		if (res >= 0)
			res = (int)0x80020001;
		sceIoClose(fd);
		goto free_work2;
	}

	res = sceIoClose(fd);
	if (res < 0) {
		Kprintf("mediasync.c:%s:close failed 0x%08x\n", "MsCheckMedia", res);
		goto free_work2;
	}

	res = MsSystemFile(work2, sfo_size, game_info);

free_work2:
	{
		int r2 = sceKernelFreePartitionMemory(work2_id);
		if (r2 < 0)
			Kprintf("mediasync.c:%s:sceKernelFreePartitionMemory failed 0x%08x\n", "MsCheckMedia", r2);
	}
free_work1:
	{
		int r2 = sceKernelFreePartitionMemory(work1_id);
		if (r2 < 0)
			Kprintf("mediasync.c:%s:sceKernelFreePartitionMemory failed 0x%08x\n", "MsCheckMedia", r2);
	}
	return res;
}

static int WaitEflash(void)
{
	SceUID cbid;
	int res;
	uint32_t state = 0;
	uint32_t timeout;
	int apitype;

	g_ef_evfid = sceKernelCreateEventFlag("SceMediaSyncEvEf", 0, 0, NULL);
	if (g_ef_evfid < 0) {
		Kprintf("mediasync.c:%s:sceKernelCreateEventFlag failed 0x%08x\n", "WaitEflash", g_ef_evfid);
		return g_ef_evfid;
	}

	cbid = sceKernelCreateCallback("SceMediaSyncEf", MsCallback, (void *)1);
	if (cbid < 0) {
		res = cbid;
		goto cleanup_evf;
	}

	res = sceIoDevctl("fatef0:", 0x02415821, &cbid, 4, NULL, 0);
	if (res < 0) {
		Kprintf("mediasync.c:%s:sceFatmsRegisterNotifyCallback failed 0x%08x\n", "WaitEflash", res);
		goto cleanup_cb;
	}

	while (1) {
		res = sceIoDevctl("fatef0:", 0x02425823, NULL, 0, &state, 4);
		if (res >= 0 && (state & 1) != 0)
			break;
		timeout = 10000000;
		res = sceKernelWaitEventFlagCB(g_ef_evfid, 1, 0x11, NULL, &timeout);
		if (res < 0)
			goto cleanup_unreg;
	}

	apitype = sceKernelInitApitype();
	switch (apitype) {
	case 0x114:
	case 0x115:
	case 0x118:
	case 0x119:
	case 0x151:
		res = MsCheckMedia(sceKernelInitFileName());
		if (res < 0)
			Kprintf("mediasync.c:%s:MsCheckMediaFailed 0x%08x\n", "WaitEflash", res);
		break;
	case 0x152:
	case 0x154:
	case 0x155:
	case 0x156:
		res = MsCheckMedia(sceKernelInitFileName());
		if (res < 0)
			Kprintf("mediasync.c:%s:Warning: MsCheckMediaFailed 0x%08x\n", "WaitEflash", res);
		res = 0;
		break;
	case 0x125:
	case 0x171:
		break;
	default:
		Kprintf("mediasync.c:%s:unsupported apitype=0x%08x[%d]\n", "WaitEflash", apitype, apitype);
		break;
	}

cleanup_unreg:
	{
		int r2 = sceIoDevctl("fatef0:", 0x02415822, &cbid, 4, NULL, 0);
		if (r2 < 0)
			Kprintf("mediasync.c:%s:sceFatmsUnRegisterNotifyCallback failed 0x%08x\n", "WaitEflash", r2);
	}
cleanup_cb:
	{
		int r2 = sceKernelDeleteCallback(cbid);
		if (r2 < 0)
			Kprintf("mediasync.c:%s:sceKernelDeleteCallback failed 0x%08x\n", "WaitEflash", r2);
	}
cleanup_evf:
	{
		int r2 = sceKernelDeleteEventFlag(g_ef_evfid);
		if (r2 < 0)
			Kprintf("mediasync.c:%s:sceKernelDeleteEventFlag failed 0x%08x\n", "WaitEflash", r2);
	}
	return res;
}

static int WaitDiscEmu(void)
{
	char *disc_img;
	int res;
	int status;
	int apitype;
	uint8_t *game_info;

	res = sceUmdActivate(1, "disc0:");
	if (res < 0) {
		Kprintf("mediasync.c:%s:sceUmdActivate failed 0x%08x\n", "WaitDiscEmu", res);
		return res;
	}

	disc_img = sceKernelInitDiscImage();
	if (disc_img == NULL)
		return (int)0x80020001;

	res = sceUmdWaitDriveStat(0x29);
	if (res < 0) {
		Kprintf("mediasync.c:%s:sceUmdWaitDriveStat failed 0x%08x\n", "WaitDiscEmu", res);
		return res;
	}

	status = sceUmdGetDriveStatus();
	if ((status & 0x20) == 0) {
		Kprintf("mediasync.c:%s:sceUmdGetDriveStatus:0x%08x\n", "WaitDiscEmu", status);
		return (status >= 0) ? (int)0x8001007b : status;
	}

	apitype = sceKernelInitApitype();
	if ((uint32_t)(apitype - 0x112) < 4U) {
		game_info = sceKernelGetGameInfo();
		if (game_info == NULL || game_info[0x18] == 0)
			return 0;
		if (strncmp((const char *)(game_info + 8), (const char *)(game_info + 0x18), 10) == 0) {
			Kprintf("mediasync.c:%s:UMD Media Check OK\n", "DiscCheckMedia");
			return 0;
		}
		res = (int)0x80020464;
		Kprintf("mediasync.c:%s:SCE_MEDIASYNC_ERROR_INVALID_MEDIA\n", "DiscCheckMedia");
		Kprintf("mediasync.c:%s:[%16s] and [%16s]\n", "DiscCheckMedia", game_info + 8, game_info + 0x18);
		Kprintf("mediasync.c:%s:DiscCheckMedia failed 0x%08x\n", "WaitDiscEmu", res);
		return res;
	}

	if (apitype == 0x123 || apitype == 0x125 || apitype == 0x170 || apitype == 0x171) {
		res = MsCheckMedia(disc_img);
		if (res < 0)
			Kprintf("mediasync.c:%s:MsCheckMedia failed 0x%08x\n", "WaitDiscEmu", res);
	}
	return res;
}

static int MsCallback(int count, int arg, void *common)
{
	int res;

	(void)count;
	if ((arg & 1) == 0)
		return 0;

	res = sceKernelSetEventFlag((common == NULL) ? g_ms_evfid : g_ef_evfid, 1);
	if (res < 0)
		Kprintf("mediasync.c:%s:sceKernelSetEventFlag failed 0x%08x\n", "MsCallback", res);
	return 0;
}

static int MsSystemFile(void *buf, SceSize size, uint8_t *game_info)
{
	SceSysfileCtx *ctx = NULL;
	int index = 0;
	char disc_id[16];
	int res;
	int i;

	g_heap_id = -1;
	sub_3680(sf_malloc, sf_free);
	res = sub_2bd0(buf, size, &ctx);
	if (res != 0) {
		Kprintf("mediasync.c:%s:sceSysfileInit failed 0x%08x\n", "MsSystemFile", res);
		goto cleanup;
	}

	res = sub_3340(ctx, "DISC_ID", &index);
	if (res != 0) {
		Kprintf("mediasync.c:%s:sceSysfileSearchKey failed 0x%08x\n", "MsSystemFile", res);
		goto cleanup;
	}

	res = sub_2980(ctx, index, disc_id, sizeof(disc_id));
	if (res != 0) {
		Kprintf("mediasync.c:%s:sceSysfileGetString failed 0x%08x\n", "MsSystemFile", res);
		goto cleanup;
	}

	if (game_info[0x18] == 0) {
		strncpy((char *)(game_info + 0x18), disc_id, 16);
	} else {
		if (strncmp(disc_id, (const char *)(game_info + 0x18), 16) != 0) {
			Kprintf("mediasync.c:%s:SCE_MEDIASYNC_ERROR_INVALID_MEDIA\n", "MsSystemFile");
			Kprintf("mediasync.c:%s:[%s] and [%s]\n", "MsSystemFile", disc_id, game_info + 0x18);
			res = (int)0x80020464;
			goto cleanup;
		}
	}

	for (i = 0; i < 3; i++) {
		static const char * const s_special_ids[3] = {
			"ULJS00167", "ULJS00168", "ULJS00169"
		};
		if (strcmp((const char *)(game_info + 0x18), s_special_ids[i]) == 0) {
			if (sceKernelApplicationType() == 0x20)
				strncpy((char *)(game_info + 0x18), "NPJH90038", 16);
			break;
		}
	}

cleanup:
	if (ctx != NULL) {
		int r2 = sub_2c80(ctx);
		if (r2 != 0)
			Kprintf("mediasync.c:%s:sceSysfileExit failed 0x%08x\n", "MsSystemFile", r2);
	}
	if (g_heap_id >= 0) {
		int r2 = sceKernelDeleteHeap(g_heap_id);
		if (r2 < 0)
			Kprintf("mediasync.c:%s:sceKernelDeleteHeap failed 0x%08x\n", "MsSystemFile", r2);
		else
			g_heap_id = -1;
	}
	return res;
}

static void *sf_malloc(void *ctx, SceSize size)
{
	(void)ctx;
	if (g_heap_id < 0) {
		g_heap_id = sceKernelCreateHeap(1, 0x800, 1, "SceKernelMediaSyncHeap");
		if (g_heap_id < 0) {
			Kprintf("mediasync.c:%s:sceKernelCreateHeap failed 0x%08x\n", "sf_malloc", g_heap_id);
			return NULL;
		}
	}
	return sceKernelAllocHeapMemory(g_heap_id, size);
}

static void *sf_vpl_malloc(void *ctx, SceSize size)
{
	void *data = NULL;
	SceUID vpl;

	(void)ctx;
	vpl = sceKernelCreateVpl("sceLibSysfile", 2, 0, size + 0x120, NULL);
	if (vpl < 0)
		return NULL;
	if (sceKernelTryAllocateVpl(vpl, size + 0x20, &data) < 0)
		return NULL;
	sub_3914((char *)data, "SFV");
	((SceUID *)data)[1] = vpl;
	return (uint8_t *)data + 0x20;
}

static void sf_free(void *ctx, void *ptr)
{
	(void)ctx;
	if (g_heap_id >= 0)
		sceKernelFreeHeapMemory(g_heap_id, ptr);
}

static int sf_vpl_free(void *ctx, void *ptr)
{
	uint8_t *hdr = (uint8_t *)ptr - 0x20;
	SceUID vpl;
	int res;

	(void)ctx;
	if (sub_35e4(hdr, "SFV") != 0)
		return -1;
	vpl = ((SceUID *)hdr)[1];
	res = sceKernelFreeVpl(vpl, hdr);
	if (vpl > 0)
		sceKernelDeleteVpl(vpl);
	return res;
}

static int sub_12c0(void)
{
	SceUID dfd;
	SceIoDirent dir;
	uint8_t digest[16];
	uint32_t path_words[3];
	uint32_t i;
	int found = 0;

	path_words[0] = 0x73616c66U;
	path_words[1] = 0x2f3a3068U;
	path_words[2] = 0x002f646bU;

	dfd = sceIoDopen((const char *)path_words);
	if (dfd < 0)
		return 0;

	while (sceIoDread(dfd, &dir) > 0) {
		for (i = 0; dir.d_name[i] != '\0'; i++)
			dir.d_name[i] = (char)tolower((uint8_t)dir.d_name[i]);
		sceKernelUtilsMd5Digest((const uint8_t *)dir.d_name, i, digest);
		for (i = 0; i < 7; i++) {
			if (memcmp(digest, g_kd_md5_table[i], 16) == 0) {
				found = 1;
				break;
			}
		}
	}
	sceIoDclose(dfd);
	return found;
}

static void sub_13e8(const char *disc_id)
{
	int i;

	for (i = 0; i < 3; i++) {
		if (strncmp(disc_id, g_modmgr_table1[i].id, 10) == 0) {
			ModuleMgrForKernel_var_57F4B35E = (int)g_modmgr_table1[i].a;
			break;
		}
	}

	if (strncmp(disc_id, g_modmgr_table2[0], 10) == 0)
		ModuleMgrForKernel_var_DB08B7FF = 1;

	for (i = 0; i < 14; i++) {
		if (strncmp(disc_id, g_modmgr_table3[i], 10) == 0) {
			ModuleMgrForKernel_var_99BFFDB8 = 1;
			break;
		}
	}
}

static int sub_14dc(void)
{
	uint8_t *game_info = sceKernelGetGameInfo();
	const char *disc_id;

	if (game_info == NULL)
		return 0;

	disc_id = (const char *)(game_info + 0x18);
	if (*disc_id == '\0')
		return 0;

	sub_18e0(disc_id);
	sub_1570(disc_id);
	sub_1978(disc_id);
	sub_19f0(disc_id);
	sub_186c(disc_id);
	sub_1a48(disc_id);
	sub_1abc(disc_id);
	sub_1c00(disc_id);
	sub_13e8(disc_id);
	sub_1db8(disc_id);
	sub_1e98(disc_id);
	sub_2168(disc_id);
	sub_2254(disc_id);
	sub_22a4(disc_id);
	sub_2354(disc_id);
	sub_2404(disc_id);
	return 0;
}

static inline __attribute__((always_inline)) void decode_packed_disc_id(const PackedDiscEntry *e, char *out)
{
	uint32_t w0 = e->hi;
	uint32_t w1 = e->lo;
	uint32_t b3 = (w0 >> 20) & 0x0f;

	out[0] = (char)(((w1 >> 2) & 0x1f) + 0x41);
	out[1] = (char)(((w1 & 3) << 3) | (w0 >> 29) | 0x40);
	out[2] = (char)(((w0 >> 24) & 0x1f) + 0x41);
	out[3] = (b3 < 10) ? (char)(b3 + '0') : (char)(b3 + 0x37);
	out[4] = '-';
	out[5] = (char)(((w1 >> 20) & 0x0f) + '0');
	out[6] = (char)(((w1 >> 16) & 0x0f) + '0');
	out[7] = (char)(((w1 >> 12) & 0x0f) + '0');
	out[8] = (char)(((w1 >> 8) & 0x0f) + '0');
	out[9] = (char)(((w1 >> 24) & 0x0f) + '0');
	out[10] = '\0';
}

static int sub_1570(const char *disc_id)
{
	char decoded[16];
	char cur_id[16];
	int i;
	const char *p;

	if (sceKernelSearchModuleByName("sceUmdCache_driver") < 0)
		return 0;

	p = (const char *)(uintptr_t)sceUmdCache_driver_A21D8213(0);
	for (i = 0; i < 10; i++)
		cur_id[i] = p ? p[i] : disc_id[i];
	cur_id[10] = '\0';

	if (sceKernelIsDevelopmentToolMode() != 0) {
		sceUmdCache_driver_DB97E432(1);
		sceUmd9660_driver_3CC9CE54(0);
		return 1;
	}

	for (i = 0; i < 34; i++) {
		decode_packed_disc_id(&g_umd_table1[i], decoded);
		if (memcmp(cur_id, decoded, 10) == 0) {
			sceUmdCache_driver_DB97E432(0);
			sceUmd9660_driver_FE3A8B67(0);
			return 0;
		}
	}

	for (i = 0; i < 42; i++) {
		decode_packed_disc_id(&g_umd_table2[i], decoded);
		if (memcmp(cur_id, decoded, 10) == 0) {
			sceUmdCache_driver_DB97E432(3);
			sceUmd9660_driver_3CC9CE54(0);
			sceUmdCache_driver_576E0F06((int)(g_umd_table2[i].hi & 0xffff));
			return 3;
		}
	}

	for (i = 0; i < 8; i++) {
		decode_packed_disc_id(&g_umd_table3[i], decoded);
		if (memcmp(cur_id, decoded, 10) == 0) {
			sceUmdCache_driver_DB97E432(2);
			sceUmd9660_driver_3CC9CE54(0);
			sceUmdCache_driver_576E0F06((int)(g_umd_table3[i].hi & 0xffff));
			return 2;
		}
	}

	sceUmdCache_driver_DB97E432(1);
	sceUmd9660_driver_3CC9CE54(0);
	return 1;
}

static int sub_186c(const char *disc_id)
{
	int i;

	for (i = 0; i < 9; i++) {
		if (strcmp(disc_id, g_wlan_table[i].id) == 0)
			sceWlanDrv_driver_EDD207B1((int)g_wlan_table[i].a, (const void *)(uintptr_t)g_wlan_table[i].b);
	}
	return 0;
}

static int sub_18e0(const char *disc_id)
{
	int apptype;

	if (strcmp(disc_id, "ULJM05109") == 0)
		InterruptManagerForKernel_E526B767(1);

	apptype = sceKernelApplicationType();
	if (apptype == 0x100 || apptype == 0x110) {
		InterruptManagerForKernel_E526B767(0);
	} else if (apptype == 0x300) {
		ThreadManForKernel_D366D35A();
		InterruptManagerForKernel_E526B767(0);
	}
	return 0;
}

static int sub_1978(const char *disc_id)
{
	int i;

	for (i = 0; i < 38; i++) {
		if (strncmp(disc_id, g_audio_table[i].id, 10) == 0)
			sceAudio_driver_95AE5A2B(g_audio_table[i].a, g_audio_table[i].b);
	}
	return 0;
}

static int sub_19f0(const char *disc_id)
{
	if (strncmp(disc_id, g_mecore_call_table[0].id, 10) == 0)
		sceMeCore_driver_4CD5B5BF(g_mecore_call_table[0].a, g_mecore_call_table[0].b);
	return 0;
}

static int sub_1a48(const char *disc_id)
{
	int i;

	for (i = 0; i < 14; i++) {
		if (strncmp(disc_id, g_ctrl_table[i].id, 10) == 0) {
			sceCtrl_driver_5886194C(g_ctrl_table[i].a);
			break;
		}
	}
	return 0;
}

static void sub_1abc(const char *disc_id)
{
	int i;

	for (i = 0; i < 2; i++) {
		if (strncmp(disc_id, g_io_table1[i], 10) == 0) {
			g_deleted_error = 1;
			break;
		}
	}

	if (strncmp(disc_id, "ULJM05957", 10) == 0) {
		IoFileMgrForKernel_var_E6F7A8DF = 50000;
		IoFileMgrForKernel_var_F659661B = 45000;
		sceKermitMsfs_driver_var_93B61A29 = 45000;
	}

	if (strncmp(disc_id, "NPJH50491", 10) == 0)
		sceKermitMsfs_driver_var_93B61A29 = 5000;
}

static int sub_1c00(const char *disc_id)
{
	int i;

	for (i = 0; i < 28; i++) {
		if (strcmp(disc_id, g_ms_devctl_table[i]) == 0) {
			uint32_t val = 1;
			sceIoDevctl("fatms0:", 0x0242585a, &val, 4, NULL, 0);
			if ((InitForKernel_9D33A110() == 0x50 && sceKernelApplicationType() == 0x200) ||
			    (InitForKernel_9D33A110() == 0x30 &&
			     (sceKernelInitApitype() == 0x132 || sceKernelInitApitype() == 0x133))) {
				sceIoDevctl("fatef0:", 0x0242585a, &val, 4, NULL, 0);
			}
		}
	}

	for (i = 0; i < 3; i++) {
		if (strncmp(disc_id, g_mscmhc_devctl_table[i], 9) == 0) {
			uint32_t val = 1;
			sceIoDevctl("mscmhc0:", 0x02025808, &val, 4, NULL, 0);
		}
	}
	return 0;
}

static void sub_1db8(const char *disc_id)
{
	int i;

	for (i = 0; i < 4; i++) {
		if (strncmp(disc_id, g_mecore_var1_table[i], 10) == 0) {
			sceMeCore_driver_var_61FF4837 = 1;
			break;
		}
	}

	for (i = 0; i < 13; i++) {
		if (strncmp(disc_id, g_mecore_var2_table[i].id, 10) == 0) {
			sceMeCore_driver_var_385A4CC6 = (int)g_mecore_var2_table[i].a;
			break;
		}
	}

	if (strncmp(disc_id, g_mecore_var3_table[0], 10) == 0)
		sceMeCore_driver_var_AB354CA1 = 0x3b;
}

static int sub_1e98(const char *disc_id)
{
	int i;

	for (i = 0; i < 4; i++) {
		if (strncmp(disc_id, g_power_table1[i], 10) == 0) {
			scePower_driver_var_2EDA246C = 1;
			break;
		}
	}

	for (i = 0; i < 5; i++) {
		if (strncmp(disc_id, g_power_table2[i], 10) == 0) {
			scePower_driver_var_3080D4C0 = 150000;
			break;
		}
	}

	for (i = 0; i < 9; i++) {
		if (strncmp(disc_id, g_power_table3[i], 10) == 0) {
			scePower_driver_var_18DFA43A = 1000000;
			break;
		}
	}

	for (i = 0; i < 2; i++) {
		if (strncmp(disc_id, g_power_table4[i].id, 10) == 0) {
			scePower_driver_var_8B317890 = (int)g_power_table4[i].a;
			scePower_driver_var_93E16800 = (int)g_power_table4[i].b;
			break;
		}
	}

	for (i = 0; i < 5; i++) {
		if (strncmp(disc_id, g_power_table5[i].id, 10) == 0) {
			scePower_driver_var_4A03DA50 = (int)g_power_table5[i].a;
			break;
		}
	}

	for (i = 0; i < 4; i++) {
		if (strncmp(disc_id, g_power_table6[i].id, 10) == 0) {
			int val = (int)g_power_table6[i].a;
			if (val & 1) {
				scePower_driver_var_33399B15 = 1;
			} else if (val & 2) {
				scePower_driver_var_C7998FA4 = 5000;
			} else if (val & 4) {
				sceKermitPeripheral_driver_B93A88E2();
			}
			break;
		}
	}

	for (i = 0; i < 24; i++) {
		if (strncmp(disc_id, g_power_table7[i].id, 10) == 0) {
			scePower_driver_var_2A28BB48 = (int)g_power_table7[i].a;
			scePower_driver_var_94FA3195 = 200000;
			break;
		}
	}

	for (i = 0; i < 2; i++) {
		if (strncmp(disc_id, g_power_table8[i], 10) == 0) {
			scePower_driver_var_07FC869E = 1;
			scePower_driver_var_E59CA666 = 1000000;
			break;
		}
	}
	return 0;
}

static void sub_2168(const char *disc_id)
{
	int i;

	for (i = 0; i < 11; i++) {
		if (strncmp(disc_id, g_dmac_table1[i], 10) == 0) {
			sceDmacplus_driver_var_AA2E7E54 = 0x12c;
			break;
		}
	}

	for (i = 0; i < 3; i++) {
		if (strncmp(disc_id, g_dmac_table2[i], 10) == 0) {
			sceDmacplus_driver_var_C10B947A = 1;
			break;
		}
	}

	for (i = 0; i < 6; i++) {
		if (strncmp(disc_id, g_dmac_table3[i].id, 10) == 0) {
			sceDmacplus_driver_var_374689B0 = (int)g_dmac_table3[i].a;
			sceDmacplus_driver_var_9D3DDDF0 = 200000;
			break;
		}
	}
}

static void sub_2254(const char *disc_id)
{
	int i;

	for (i = 0; i < 3; i++) {
		if (strncmp(disc_id, g_rtc_table[i], 10) == 0) {
			sceRtc_driver_var_5A64FA55 = 1;
			break;
		}
	}
}

static void sub_22a4(const char *disc_id)
{
	if (strncmp(disc_id, g_usb_table1[0].id, 10) == 0)
		sceUsb_driver_var_A942D291 = (int)g_usb_table1[0].a;

	if (strncmp(disc_id, g_usb_table2[0], 10) == 0)
		sceUsb_driver_var_3EA085BF = 1;
}

static void sub_2354(const char *disc_id)
{
	int i;

	for (i = 0; i < 2; i++) {
		if (strncmp(disc_id, g_reg_table1[i].id, 10) == 0) {
			sceReg_driver_var_867D5F60 = (int)g_reg_table1[i].a;
			break;
		}
	}

	if (strncmp(disc_id, g_reg_table2[0].id, 10) == 0)
		sceReg_driver_var_5105E3FF = (int)g_reg_table2[0].a;
}

static void sub_2404(const char *disc_id)
{
	int i;

	for (i = 0; i < 3; i++) {
		if (strncmp(disc_id, g_msfs_table1[i].id, 10) == 0) {
			sceKermitMsfs_driver_var_C1B4213A = (int)g_msfs_table1[i].a;
			break;
		}
	}

	for (i = 0; i < 3; i++) {
		if (strncmp(disc_id, g_msfs_table2[i], 10) == 0) {
			sceKermitMsfs_driver_var_7668D90C = 1;
			break;
		}
	}

	for (i = 0; i < 2; i++) {
		if (strncmp(disc_id, g_msfs_table3[i].id, 10) == 0) {
			sceKermitMsfs_driver_var_BE954163 = (int)g_msfs_table3[i].a;
			break;
		}
	}
}

static int sub_24d8(void)
{
	RegParam reg;
	uint32_t reg_handle = 0;
	uint32_t cat_handle = 0;
	uint32_t key_handle = 0;
	uint32_t type = 0;
	SceSize size = 0;
	char env_buf[12];
	char out_buf[12];
	int res;

	memset(&reg, 0, sizeof(reg));
	reg.regtype = 1;
	reg.unk2 = 1;
	reg.unk3 = 1;

	res = sceRegOpenRegistry(&reg, 2, &reg_handle);
	if (res < 0)
		return res;

	res = sceRegOpenCategory(reg_handle, "/CONFIG/NP", 2, &cat_handle);
	if (res < 0) {
		sceRegCloseRegistry(reg_handle);
		return res;
	}

	res = sceRegGetKeyInfo(cat_handle, "env", &key_handle, &type, &size);
	if (res >= 0 && type == 3 && size < 10) {
		env_buf[0] = '\0';
		res = sceRegGetKeyValue(cat_handle, key_handle, env_buf, 9);
		if (res >= 0) {
			out_buf[0] = '\0';
			sub_25f4(out_buf, env_buf);
			SysMemForKernel_A03CB480(out_buf);
		}
	}

	sceRegCloseCategory(cat_handle);
	sceRegCloseRegistry(reg_handle);
	return res;
}

static void sub_25f4(char *out_buf, const char *env_str)
{
	char work[128];
	uint8_t digest[16];

	if (env_str[0] == '\0') {
		strncpy(out_buf, "invalid", 8);
		return;
	}

	if (sceKermitPeripheral_driver_4A26B7C8(0) != 0) {
		strncpy(out_buf, env_str, 8);
		return;
	}

	sceKernelMemset(work, 0, sizeof(work));
	while (strlen(work) < 100)
		strcat(work, env_str);

	sceKernelUtilsMd5Digest((const uint8_t *)work, sizeof(work), digest);
	if (sceKermitPeripheral_driver_675DB2C1(0) != 0) {
		if (memcmp(digest, g_np_env_md5_1, 16) == 0 ||
		    memcmp(digest, g_np_env_md5_2, 16) == 0 ||
		    memcmp(digest, g_np_env_md5_3, 16) == 0) {
			strncpy(out_buf, env_str, 8);
			return;
		}
	} else {
		if (memcmp(digest, g_np_env_md5_1, 16) == 0) {
			strncpy(out_buf, env_str, 8);
			return;
		}
	}
	strncpy(out_buf, "invalid", 8);
}

static int sub_2734(void)
{
	RegParam reg;
	uint32_t reg_handle = 0;
	uint32_t cat_handle = 0;
	uint32_t key_handle = 0;
	uint32_t type = 0;
	SceSize size = 0;
	char prefix_buf[8];
	int res;

	memset(&reg, 0, sizeof(reg));
	reg.regtype = 1;
	reg.unk2 = 1;
	reg.unk3 = 1;

	res = sceRegOpenRegistry(&reg, 2, &reg_handle);
	if (res < 0)
		return res;

	res = sceRegOpenCategory(reg_handle, "/CONFIG/NETWORK/ADHOC", 2, &cat_handle);
	if (res < 0) {
		sceRegCloseRegistry(reg_handle);
		return res;
	}

	res = sceRegGetKeyInfo(cat_handle, "ssid_prefix", &key_handle, &type, &size);
	if (res >= 0 && type == 3 && size != 3) {
		res = sceRegGetKeyValue(cat_handle, key_handle, prefix_buf, 4);
		if (res >= 0) {
			prefix_buf[3] = '\0';
			res = sceWlanDrv_driver_EDD207B1(0, prefix_buf);
		}
	}

	sceRegCloseCategory(cat_handle);
	sceRegCloseRegistry(reg_handle);
	return res;
}

static int sub_2824(SceSysfileCtx *ctx, int index, SceSysfileEntry **out_entry)
{
	SceSysfileHeader *hdr;
	SceSysfileEntry *entry;
	const char *key;
	SceSize key_len;
	int res;

	if (ctx == NULL || out_entry == NULL || index < 0)
		return (int)0x80090001;

	if ((ctx->flags & 2) != 0 && (int32_t)ctx->reserved >= 0) {
		if (sceIoLseek((SceUID)ctx->reserved, 0, 0) < 0)
			return (int)0x80120002;
		if ((SceSize)sceIoRead((SceUID)ctx->reserved, ctx->hdr, ctx->buf_size) != ctx->buf_size)
			return (int)0x80120002;
	}

	res = sub_2a14(ctx->hdr, ctx->buf_size);
	if (res < 0)
		return res;

	hdr = ctx->hdr;
	if ((uint32_t)index >= hdr->num_entries)
		return (int)0x80090003;

	entry = (SceSysfileEntry *)((uint8_t *)hdr + sizeof(SceSysfileHeader) + (uint32_t)index * sizeof(SceSysfileEntry));
	if (entry->alignment != 4)
		return (int)0x80090002;
	if (entry->data_type != 0 && entry->data_type != 2 && entry->data_type != 4)
		return (int)0x80090002;
	if (entry->total_size < entry->value_size)
		return (int)0x80090002;
	if ((entry->total_size & 3) != 0)
		return (int)0x80090002;
	if (entry->data_type == 4 && entry->value_size != 4)
		return (int)0x80090002;
	if (hdr->key_table_offset + entry->key_offset >= hdr->data_table_offset)
		return (int)0x80090002;
	if ((entry->data_offset & 3) != 0)
		return (int)0x80090002;
	if (ctx->buf_size < hdr->data_table_offset + entry->data_offset + entry->total_size)
		return (int)0x80090002;

	key = (const char *)hdr + hdr->key_table_offset + entry->key_offset;
	key_len = sub_364c(key, 512);
	if (key_len == 0 || key_len >= 512)
		return (int)0x80090002;
	if (hdr->key_table_offset + entry->key_offset + key_len >= hdr->data_table_offset)
		return (int)0x80090002;

	*out_entry = entry;
	return 0;
}

static int sub_2980(SceSysfileCtx *ctx, int index, void *buf, SceSize buf_size)
{
	SceSysfileEntry *entry = NULL;
	int res;

	if (buf == NULL || buf_size == 0)
		return (int)0x80090001;

	res = sub_2824(ctx, index, &entry);
	if (res < 0)
		return res;

	if (entry->data_type != 2)
		return (int)0x80090006;
	if (buf_size < entry->value_size)
		return (int)0x80090005;

	sceKernelMemcpy(buf, (const uint8_t *)ctx->hdr + ctx->hdr->data_table_offset + entry->data_offset, entry->value_size);
	((uint8_t *)buf)[entry->value_size - 1] = 0;
	return 0;
}

static int __attribute__((noinline, used)) sub_2a14(const void *buf, SceSize size)
{
	const SceSysfileHeader *hdr = (const SceSysfileHeader *)buf;
	uint8_t magic_chk[8];
	int i;

	if (hdr == NULL || ((uintptr_t)hdr & 3) != 0 || size < sizeof(SceSysfileHeader) || size >= 0x40000000U)
		return (int)0x80090001;

	for (i = 0; i < 6; i++)
		magic_chk[i] = (uint8_t)(g_sf_XorTable[i * 4] ^ g_sf_XorTable[20]);

	if (sub_3628(&hdr->magic, magic_chk, 4) != 0)
		return (int)0x80090002;

	if ((hdr->version & 0xffff0000U) != 0x00000000U &&
	    ((hdr->version >> 8) != 1 || (hdr->version & 0xffff0000U) != 0)) {
		if ((hdr->version & 0xffffff00U) != 0x00000100U)
			return (int)0x80090007;
	}
	if ((hdr->version & 0xffffff00U) != 0x00000100U)
		return (int)0x80090007;

	if (hdr->num_entries >= 0x10000U)
		return (int)0x80090002;
	if ((hdr->key_table_offset & 3) != 0)
		return (int)0x80090002;
	if (hdr->key_table_offset < sizeof(SceSysfileHeader) + hdr->num_entries * sizeof(SceSysfileEntry))
		return (int)0x80090002;
	if ((hdr->data_table_offset & 3) != 0)
		return (int)0x80090002;
	if (hdr->data_table_offset < hdr->key_table_offset)
		return (int)0x80090002;
	if (hdr->num_entries != 0 && hdr->data_table_offset == hdr->key_table_offset)
		return (int)0x80090002;
	if (size < hdr->data_table_offset)
		return (int)0x80090002;

	return 0;
}

static int __attribute__((noinline, used)) sub_2bd0(void *buf, SceSize size, SceSysfileCtx **out_ctx)
{
	int res;
	SceSysfileCtx *ctx;

	if (out_ctx == NULL)
		return (int)0x80090001;

	res = sub_2a14(buf, size);
	if (res < 0)
		return res;

	ctx = (SceSysfileCtx *)sub_36b4(sizeof(SceSysfileCtx));
	if (ctx == NULL)
		return (int)0x80090008;

	sub_37e0(ctx, 0, sizeof(SceSysfileCtx));
	ctx->flags = 1;
	ctx->hdr = (SceSysfileHeader *)buf;
	ctx->buf_size = size;
	*out_ctx = ctx;
	return 0;
}

static int __attribute__((noinline, used)) sub_2c80(SceSysfileCtx *ctx)
{
	int res;

	if (ctx == NULL)
		return (int)0x80090001;

	res = sub_2a14(ctx->hdr, ctx->buf_size);
	if (res < 0)
		return res;

	if ((ctx->flags & 2) != 0)
		sub_3708(ctx->hdr);

	sub_37e0(ctx, 0, sizeof(SceSysfileCtx));
	sub_3708(ctx);
	return 0;
}

static int __attribute__((noinline, used)) sub_2d24(SceSysfileCtx *ctx, uint32_t *out_version, uint32_t *out_count)
{
	int res;

	if (ctx == NULL)
		return (int)0x80090001;

	res = sub_2a14(ctx->hdr, ctx->buf_size);
	if (res < 0)
		return res;

	if (out_version != NULL)
		*out_version = ctx->hdr->version;
	if (out_count != NULL)
		*out_count = ctx->hdr->num_entries;
	return 0;
}

static int __attribute__((noinline, used)) sub_2ed4(SceSysfileCtx *ctx, int index, void *out_val)
{
	SceSysfileEntry *entry = NULL;
	int res;

	if (out_val == NULL)
		return (int)0x80090001;

	res = sub_2824(ctx, index, &entry);
	if (res < 0)
		return res;

	if (entry->data_type != 4)
		return (int)0x80090006;

	sub_3734(out_val, (const uint8_t *)ctx->hdr + ctx->hdr->data_table_offset + entry->data_offset, 4);
	return 0;
}

static int __attribute__((noinline, used)) sub_2f8c(SceSysfileCtx *ctx, int index, void **out_ptr)
{
	SceSysfileEntry *entry = NULL;
	const uint8_t *data_ptr;
	int res;

	if (out_ptr == NULL)
		return (int)0x80090001;

	res = sub_2824(ctx, index, &entry);
	if (res < 0)
		return res;

	if (entry->data_type != 2)
		return (int)0x80090006;
	if (entry->value_size == 0)
		return (int)0x80090002;

	data_ptr = (const uint8_t *)ctx->hdr + ctx->hdr->data_table_offset + entry->data_offset;
	if (data_ptr[entry->value_size - 1] != 0)
		return (int)0x80090002;

	*out_ptr = (void *)data_ptr;
	return 0;
}

static int __attribute__((noinline, used)) sub_3054(SceSysfileCtx *ctx, int index, void *buf, SceSize buf_size)
{
	SceSysfileEntry *entry = NULL;
	int res;

	if (buf == NULL || buf_size == 0)
		return (int)0x80090001;

	res = sub_2824(ctx, index, &entry);
	if (res < 0)
		return res;

	if (entry->data_type != 0)
		return (int)0x80090006;
	if (buf_size < entry->value_size)
		return (int)0x80090005;

	sub_3734(buf, (const uint8_t *)ctx->hdr + ctx->hdr->data_table_offset + entry->data_offset, entry->value_size);
	return 0;
}

static int __attribute__((noinline, used)) sub_30f4(SceSysfileCtx *ctx, int index, char *key_buf, SceSize key_buf_size, uint32_t *out_type, SceSize *out_val_size, SceSize *out_total_size)
{
	SceSysfileEntry *entry = NULL;
	const char *key;
	SceSize key_len;
	int res;

	res = sub_2824(ctx, index, &entry);
	if (res < 0)
		return res;

	if (key_buf != NULL) {
		key = (const char *)ctx->hdr + ctx->hdr->key_table_offset + entry->key_offset;
		key_len = sub_364c(key, 512);
		if (key_buf_size < key_len + 1)
			return (int)0x80090005;
		sub_3734(key_buf, key, key_len + 1);
	}
	if (out_type != NULL)
		*out_type = entry->data_type;
	if (out_val_size != NULL)
		*out_val_size = entry->value_size;
	if (out_total_size != NULL)
		*out_total_size = entry->total_size;
	return 0;
}

static int __attribute__((noinline, used)) sub_3340(SceSysfileCtx *ctx, const char *key, int *out_index)
{
	return sub_3494(ctx, key, 0, out_index);
}

static int __attribute__((noinline, used)) sub_3494(SceSysfileCtx *ctx, const char *key, int start_index, int *out_index)
{
	SceSize key_len;
	uint32_t num_entries;
	int i;
	int res;

	if (ctx == NULL || key == NULL || out_index == NULL || start_index < 0)
		return (int)0x80090001;

	key_len = sub_364c(key, 512);
	if (key_len == 0 || key_len >= 512)
		return (int)0x80090001;

	res = sub_2a14(ctx->hdr, ctx->buf_size);
	if (res < 0)
		return res;

	num_entries = ctx->hdr->num_entries;
	for (i = start_index; (uint32_t)i < num_entries; i++) {
		SceSysfileEntry *entry = NULL;
		const char *entry_key;

		res = sub_2824(ctx, i, &entry);
		if (res < 0)
			return res;

		entry_key = (const char *)ctx->hdr + ctx->hdr->key_table_offset + entry->key_offset;
		if (sub_35e4(entry_key, key) == 0) {
			*out_index = i;
			return 0;
		}
	}
	return (int)0x80090003;
}

static int __attribute__((noinline, used)) sub_35e4(const void *a, const void *b)
{
	const uint8_t *p1 = (const uint8_t *)a;
	const uint8_t *p2 = (const uint8_t *)b;

	while (*p1 == *p2) {
		if (*p1 == 0)
			return 0;
		p1++;
		p2++;
	}
	return (int)*p1 - (int)*p2;
}

static int __attribute__((noinline, used)) sub_3628(const void *a, const void *b, SceSize len)
{
	const uint8_t *p1 = (const uint8_t *)a;
	const uint8_t *p2 = (const uint8_t *)b;
	SceSize i;

	for (i = 0; i < len; i++) {
		if (p1[i] != p2[i])
			return (int)p1[i] - (int)p2[i];
	}
	return 0;
}

static SceSize __attribute__((noinline, used)) sub_364c(const char *s, SceSize max_len)
{
	SceSize len = 0;

	if (s == NULL)
		return 0;
	while (len < max_len && s[len] != '\0')
		len++;
	return len;
}

static int __attribute__((noinline, used)) sub_3680(void *(*alloc_fn)(void *, SceSize), void (*free_fn)(void *, void *))
{
	if ((alloc_fn == NULL && free_fn != NULL) || (alloc_fn != NULL && free_fn == NULL))
		return (int)0x80090001;

	if (alloc_fn == NULL) {
		g_sf_alloc_fn = sf_vpl_malloc;
		g_sf_free_fn = sf_vpl_free;
	} else {
		g_sf_alloc_fn = alloc_fn;
		g_sf_free_fn = (int (*)(void *, void *))free_fn;
	}
	g_sf_alloc_ctx = NULL;
	g_sf_free_ctx = NULL;
	return 0;
}

static void * __attribute__((noinline, used)) sub_36b4(SceSize size)
{
	if (size == 0 || g_sf_alloc_fn == NULL)
		return NULL;
	return g_sf_alloc_fn(g_sf_alloc_ctx, size);
}

static void __attribute__((noinline, used)) sub_3708(void *ptr)
{
	if (ptr != NULL && g_sf_free_fn != NULL)
		g_sf_free_fn(g_sf_free_ctx, ptr);
}

static void * __attribute__((noinline, used)) sub_3734(void *dst, const void *src, SceSize size)
{
	uint8_t *d = (uint8_t *)dst;
	const uint8_t *s = (const uint8_t *)src;
	SceSize i;

	for (i = 0; i < size; i++)
		d[i] = s[i];
	return dst;
}

static void * __attribute__((noinline, used)) sub_37e0(void *dst, int val, SceSize size)
{
	uint8_t *d = (uint8_t *)dst;
	SceSize i;

	for (i = 0; i < size; i++)
		d[i] = (uint8_t)val;
	return dst;
}

static char * __attribute__((noinline, used)) sub_3914(char *dst, const char *src)
{
	char *d = dst;
	char c;

	do {
		c = *src++;
		*d++ = c;
	} while (c != '\0');
	return dst;
}
