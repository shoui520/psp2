#include <kermit.h>

/* Imported functions declared in scetables.S */
extern int sceKernelBootFrom(void);
extern int sceKernelApplicationType(void);
extern int sceKernelInitDiscImage(void);
extern int sceIoDevctl(const char *dev, u32 cmd, void *indata, int inlen, void *outdata, int outlen);
extern int sceKernelIsToolMode(void);
extern int sceKernelDipsw(int bit);
extern int sceKernelIsDevelopmentToolMode(void);
extern int sceKernelUnloadModule(SceUID modid);
extern int sceKernelStartModule(SceUID modid, SceSize argsize, void *argp, int *status, void *opt);
extern SceUID sceKernelLoadModuleForKernel(const char *path, int flags, void *opt);
extern SceUID sceKernelLoadModuleToBlock(const char *path, SceUID block_id, SceUID *rem_block, int flags, void *opt);
extern SceUID sceKernelSearchModuleByName(const char *name);
extern int sceKernelStopModule(SceUID modid, SceSize argsize, void *argp, int *status, void *opt);
extern int sceKernelGetModel(void);
extern int sceKernelJointMemoryBlock(SceUID first_block, SceUID second_block);
extern int sceKernelMemset32(void *dst, u32 fill_word, SceSize size);
extern SceUID sceKernelAllocPartitionMemory(int mpid, const char *name, int type, SceSize size, void *addr);
extern int sceKernelGetCompiledSdkVersion(void);
extern int sceKernelFreePartitionMemory(SceUID block_id);
extern int sceKernelQueryMemoryBlockInfo(SceUID block_id, void *mem_info);
extern void *sceKernelGetGameInfo(void);
extern void *memset(void *dst, int ch, SceSize count);
extern SceSize strlen(const char *text);
extern char *strncpy(char *dst, const char *src, SceSize count);
extern int strcmp(const char *lhs, const char *rhs);
extern int snprintf(char *out_buf, SceSize size, const char *fmt, ...);
extern int sceKernelWaitThreadEnd(SceUID thid, SceUInt *timeout);
extern SceUID sceKernelGetThreadId(void);
extern SceUID sceKernelCreateThread(const char *name, int (*entry)(SceSize, void *), int prio, int stack_size, SceUInt attr, void *opt);
extern int sceKernelExitDeleteThread(int status);
extern int sceKernelDeleteThread(SceUID thid);
extern int sceKernelExtendKernelStack(SceSize stack_size, int (*callback)(void *), void *arg);
extern int sceKernelStartThread(SceUID thid, SceSize arglen, void *argp);
extern int sceCtrlPeekBufferPositive(void *pad_buf, int count);
extern int sceCtrlSetSamplingMode(int sampling_mode);
extern int sceCtrlGetSamplingMode(int *out_mode);
extern int sceKermitPeripheral_driver_61BEA1D2(int enable_flag);
extern int sceMeBootStart(int booter_type);
extern int scePower_driver_315B8CB6(int power_arg);
extern int scePower_driver_766CD857(int power_arg);
extern int sceNpDrmSetDebugMode(int debug_mode);
extern int sceRegGetKeyValueByName(u32 cat, const char *name, void *out_buf, SceSize size);
extern int sceRegCreateKey(u32 cat, const char *name, int key_type, SceSize size);
extern int sceRegSetKeyValue(u32 cat, const char *name, const void *value_buf, SceSize size);
extern int sceRegCloseRegistry(u32 reg);
extern int sceRegOpenCategory(u32 reg, const char *dir_path, int access_mode, u32 *out_cat);
extern int sceRegFlushRegistry(u32 reg);
extern int sceRegRemoveCategory(u32 reg, const char *dir_path);
extern int sceRegGetKeyInfo(u32 cat, const char *name, u32 *out_handle, u32 *out_type, SceSize *out_size);
extern int sceRegFlushCategory(u32 cat);
extern int sceRegOpenRegistry(void *reg_param, int access_mode, u32 *out_reg);
extern int sceRegGetKeyInfoByName(u32 cat, const char *name, u32 *out_type, SceSize *out_size);
extern int sceRegGetKeyValue(u32 cat, u32 key_handle, void *out_buf, SceSize size);
extern int sceRegCloseCategory(u32 cat);
extern int sceKernelVolatileMemLock(int lock_type, void **out_addr, int *out_size);
extern int sceKernelVolatileMemUnlock(int lock_type);

#define UTILITY_NOINLINE __attribute__((noinline))

#define SCE_UTILITY_ERROR_INVALID_STATUS        ((int)0x80110001)
#define SCE_UTILITY_ERROR_INVALID_ADDR          ((int)0x80110002)
#define SCE_UTILITY_ERROR_INVALID_TYPE          ((int)0x80110003)
#define SCE_UTILITY_ERROR_INVALID_SIZE          ((int)0x80110004)
#define SCE_UTILITY_ERROR_INVALID_DIALOG        ((int)0x80110005)

#define SCE_UTILITY_SYSPARAM_ERROR_INVALID_TYPE ((int)0x80110101)
#define SCE_UTILITY_SYSPARAM_ERROR_INVALID_SIZE ((int)0x80110102)
#define SCE_UTILITY_SYSPARAM_ERROR_INVALID_ID   ((int)0x80110103)
#define SCE_UTILITY_SYSPARAM_ERROR_INVALID_VAL  ((int)0x80110104)

#define SCE_UTILITY_NETPARAM_ERROR_NOT_FOUND    ((int)0x80110601)
#define SCE_UTILITY_NETPARAM_ERROR_INVALID_ID   ((int)0x80110602)
#define SCE_UTILITY_NETPARAM_ERROR_INVALID_PTR  ((int)0x80110603)
#define SCE_UTILITY_NETPARAM_ERROR_INVALID_KEY  ((int)0x80110604)
#define SCE_UTILITY_NETPARAM_ERROR_INVALID_VAL  ((int)0x80110605)
#define SCE_UTILITY_NETPARAM_ERROR_REG_MISMATCH ((int)0x80110606)
#define SCE_UTILITY_NETPARAM_ERROR_EXISTS       ((int)0x80110681)
#define SCE_UTILITY_NETPARAM_ERROR_PATH_SMALL   ((int)0x80110682)
#define SCE_UTILITY_NETPARAM_ERROR_TYPE_MISMATCH ((int)0x80110683)
#define SCE_UTILITY_NETPARAM_ERROR_KEY_TOO_BIG  ((int)0x80110684)
#define SCE_UTILITY_NETPARAM_ERROR_BUF_TOO_SMALL ((int)0x80110685)

#define SCE_UTILITY_NET_ERROR_INVALID_ID        ((int)0x80110801)
#define SCE_UTILITY_USB_ERROR_INVALID_ID        ((int)0x80110E01)
#define SCE_UTILITY_AV_ERROR_INVALID_ID         ((int)0x80110F01)

#define SCE_UTILITY_MOD_ERROR_INVALID_ID        ((int)0x80111101)
#define SCE_UTILITY_MOD_ERROR_ALREADY_LOADED    ((int)0x80111102)
#define SCE_UTILITY_MOD_ERROR_NOT_LOADED        ((int)0x80111103)
#define SCE_UTILITY_MOD_ERROR_START_FAILED      ((int)0x80111104)

#define SCE_KERNEL_ERROR_NO_MEMORY              ((int)0x80000042)
#define SCE_KERNEL_ERROR_UNSUPPORTED            ((int)0x80000004)
#define SCE_KERNEL_ERROR_NO_FILE                ((int)0x80010002)
#define SCE_KERNEL_ERROR_INVALID_PTR            ((int)0x8001000E)
#define SCE_KERNEL_ERROR_NO_FREE_SLOT           ((int)0x8001001C)
#define SCE_KERNEL_ERROR_ILLEGAL_ADDR           ((int)0x800200D3)
#define SCE_KERNEL_ERROR_CANNOT_STOP            ((int)0x8002012E)
#define SCE_REG_ERROR_CAT_NOT_FOUND             ((int)0x80082718)
#define SCE_REG_ERROR_KEY_NOT_FOUND             ((int)0x8008271D)

#define KERMIT_OSK_SHARED_BASE                  0xABD68000u
#define KERMIT_OSK_WORK_BASE                    0xABD6C000u
#define KERMIT_OSK_SHARED_SIZE                  0x8000u
#define KERMIT_OSK_WORK_SIZE                    0x4000u

static inline u32 utilityEnterKernel(void)
{
	u32 old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);
	return old_k1;
}

static inline void utilityLeaveKernel(u32 old_k1)
{
	pspSetK1(old_k1);
}

static inline int utilityIsBadRange(const void *ptr, u32 size)
{
	u32 addr = (u32)ptr;
	return ((s32)(((addr + size) | addr) & pspGetK1())) < 0;
}

static inline int utilityIsBadSpan(const void *ptr, u32 size)
{
	u32 addr = (u32)ptr;
	return ((s32)(((addr + size) | addr | size) & pspGetK1())) < 0;
}

static inline int utilityIsBadPtr(const void *ptr)
{
	return ((s32)((u32)ptr & pspGetK1())) < 0;
}

/* Kernel and utility parameter structures */
typedef struct SceKernelLMOption {
	SceSize size;
	SceUID mpid_text;
	SceUID mpid_data;
	u32 flags;
	u8 position;
	u8 access;
	u8 c_reserved[2];
} SceKernelLMOption;

typedef struct SceKernelSMOption {
	SceSize size;
	SceUID mpid_stack;
	SceSize stack_size;
	int priority;
	u32 attribute;
} SceKernelSMOption;

typedef struct SceKernelSysMemInfo {
	SceSize size;
	char name[32];
	u32 attr;
	void *addr;
	u32 mem_size;
	u32 other_size;
} SceKernelSysMemInfo;

typedef struct SceKernelGameInfo {
	u32 size;
	u32 flags;
	char str_umd_data[16];
	char str_reserved[4];
	u32 sdk_version;
	u32 compiler_version;
	u32 dnas_flags;
	u32 utility_flags;
	char boot_path[24];
	char product_code[16];
	u32 game_type;
	char title_code[24];
	u32 emu_flags;
} SceKernelGameInfo;

typedef struct SceRegParam {
	u32 reg_type;
	char name[256];
	u32 name_len;
	u32 sys_flag_open;
	u32 sys_flag_compat;
} SceRegParam;

typedef struct SceCtrlData {
	u32 time_stamp;
	u32 buttons;
	u8 analog_x;
	u8 analog_y;
	u8 reserved[6];
} SceCtrlData;

typedef struct SceUtilityDialogCommon {
	u32 size;
	int language;
	int button_swap;
	int graphics_thread;
	int access_thread;
	int font_thread;
	int sound_thread;
	int result;
	int reserved[4];
} SceUtilityDialogCommon;

typedef struct SceUtilitySavedataHeader {
	SceUtilityDialogCommon base;
	u32 operation_mode;
} SceUtilitySavedataHeader;

typedef struct SceUtilityGameSharingHeader {
	SceUtilityDialogCommon base;
	u32 game_info_words[6];
	void *data_ptr;
	u32 data_size;
	u32 extra_words[3];
	void *ext_buf;
	u32 ext_size;
} SceUtilityGameSharingHeader;

typedef struct SceUtilityStoreHeader {
	SceUtilityDialogCommon base;
	u32 header_words[4];
	void *buffer_ptr;
	u32 item_count;
	u32 middle_words[4];
	u32 item_stride;
} SceUtilityStoreHeader;

typedef struct SceUtilityFriendHeader {
	SceUtilityDialogCommon base;
	u32 request_kind;
} SceUtilityFriendHeader;

typedef struct SceUtilityHtmlViewerHeader {
	SceUtilityDialogCommon base;
	void *mem_addr;
	u32 mem_size;
	u32 viewer_words[5];
	u32 init_flags;
} SceUtilityHtmlViewerHeader;

typedef struct SceUtilityDialogStartArg {
	u32 size;
	int model;
	u32 dialog_type;
	u32 dialog_submode;
	void *params;
	u32 api_version;
	int boot_from;
} SceUtilityDialogStartArg;

typedef struct SceUtilityAppletStartArg {
	u32 size;
	int model;
	u32 applet_type;
	void *params;
	u32 api_version;
	char product_code[16];
	int boot_from;
} SceUtilityAppletStartArg;

typedef struct SceUtilityDialogState {
	SceUID dialog_thid;
	SceUID suspend_evf;
	SceUID worktime_evf;
	u32 active_type;
	u32 dialog_submode;
	u32 status;
	u32 in_suspend;
	u32 abort_flag;
	void *params;
	u32 api_version;
	int speed;
	SceUID chnnlsv_modid;
	SceUID psheet_modid;
	SceUID common_gui_modid;
	SceUID common_util_modid;
	SceUID paf_modid;
	SceUID main_modid;
	SceUID init_thid;
	int devctl_result;
	u32 devctl_out[4];
	int emulated_ms;
	void *vmem_addr;
	int vmem_size;
} SceUtilityDialogState;

typedef struct SceUtilityAppletState {
	SceUID applet_thid;
	SceUID worktime_evf;
	u32 active_type;
	u32 status;
	void *params;
	u32 api_version;
	int speed;
	SceUID main_modid;
	SceUID block_id;
	const char *mod_path;
	SceUID init_thid;
} SceUtilityAppletState;

typedef struct SceUtilityAppletModSlot {
	SceUID block_id;
	SceUID modid;
} SceUtilityAppletModSlot;

typedef struct SceUtilityDialogMemNode {
	struct SceUtilityDialogMemNode *next;
	struct SceUtilityDialogMemNode *prev;
	SceUID block_id;
	SceUID modid;
} SceUtilityDialogMemNode;

typedef struct SceUtilityModStartOpt {
	int mpid;
	SceSize argsize;
	void *argp;
	const SceKernelLMOption *lm_opt;
} SceUtilityModStartOpt;

typedef struct SceUtilityKernMod {
	const char *path;
	const SceUtilityModStartOpt *opt;
	SceUID modid;
} SceUtilityKernMod;

typedef struct SceUtilityUserMod {
	const char *path;
	const SceUtilityModStartOpt *opt;
	SceUID modid;
	SceUID block_id;
} SceUtilityUserMod;

typedef struct SceUtilityModuleGroup {
	u16 ref_count;
	u8 kern_count;
	u8 user_count;
	SceUtilityKernMod *kern_mods;
	SceUtilityUserMod *user_mods;
	SceSize user_mem_size;
	SceUID rem_block;
	u32 guard_seed;
} SceUtilityModuleGroup;

typedef struct SceUtilityOskItem {
	int input_config[4];
	u16 *desc_ptr;
	u16 *init_text_ptr;
	int out_text_limit;
	u16 *out_text_ptr;
	int result;
	int out_text_len;
} SceUtilityOskItem;

typedef struct SceUtilityOskParams {
	SceUtilityDialogCommon base;
	int item_count;
	SceUtilityOskItem *items;
	int state;
	int error_code;
} SceUtilityOskParams;

typedef struct SceUtilityOskSharedArea {
	SceUtilityOskParams params;
	SceUtilityOskItem item;
	u32 pad_words[3];
	u16 init_text[544];
	u16 desc_text[544];
	u16 out_text[512];
} SceUtilityOskSharedArea;

typedef struct SceUtilityOskState {
	SceUtilityOskParams *user_params;
	SceUtilityOskSharedArea *shared_params;
	int status;
	int current_item;
	int item_count;
	int pending_next;
	SceUID mutex_id;
	void *shared_base;
	SceUtilityOskSharedArea *work_area;
	u32 work_size;
} SceUtilityOskState;

typedef struct SceUtilityNetParamKey {
	u32 reg_type;
	s32 key_size;
	const char *name;
} SceUtilityNetParamKey;

typedef struct SceUtilityNetParamHandle {
	u32 main_cat;
	u32 child_cat;
} SceUtilityNetParamHandle;

typedef struct SceUtilityNetParamSlotArg {
	int slot;
	int result;
} SceUtilityNetParamSlotArg;

typedef struct SceUtilityNetParamCopyArg {
	int src_slot;
	int dst_slot;
	int result;
} SceUtilityNetParamCopyArg;

typedef struct SceUtilityNetParamGetArg {
	int slot;
	int param;
	s32 buf_size;
	void *out_buf;
	int result;
} SceUtilityNetParamGetArg;

typedef struct SceUtilityNetParamSetArg {
	int slot;
	int param;
	const void *value;
	int result;
} SceUtilityNetParamSetArg;

typedef struct SceUtilityCompatEntry {
	const char *product_code;
	u32 dialog_type;
	u32 dialog_submode;
	u32 flags;
	u32 delays[1];
} SceUtilityCompatEntry;

/* Forward declarations */
int sceUtility_driver_F062AEA6(void);
int sceUtility_driver_20C68C34(void);
int sceUtility_private_59EB3A17(int flag);
int sceUtility_private_5ED16EBA(const char *name, int flags, const void *opt);
int sceUtility_private_5DCBD3C0(void);
int sceUtility_private_390A738F(void);
int sceUtility_ECE1D3E5(const char *name_buf);
int sceUtility_70267ADF(const char *key_buf);

static UTILITY_NOINLINE int utilityDialogInitStart(u32 dialog_type, void *params);
static UTILITY_NOINLINE int utilityDialogShutdownStart(u32 dialog_type);
static UTILITY_NOINLINE int utilityDialogResume(u32 dialog_type, void *params);
static UTILITY_NOINLINE int utilitySysEventHandler(int ev_id, char *ev_name, void *param, int *result);
static UTILITY_NOINLINE int utilityDialogUpdate(u32 dialog_type, int speed);
static UTILITY_NOINLINE int utilityDialogGetStatus(u32 dialog_type);
static UTILITY_NOINLINE int utilityDialogAbort(u32 dialog_type);
static UTILITY_NOINLINE int utilityDialogLoadThread(SceSize args, void *argp);
static UTILITY_NOINLINE int utilityAppletLoadThread(SceSize args, void *argp);
static UTILITY_NOINLINE int utilityDialogUnloadThread(SceSize args, void *argp);
static UTILITY_NOINLINE int utilityAppletUnloadThread(SceSize args, void *argp);
static UTILITY_NOINLINE int utilityDialogStopUnloadModule(SceUID modid, int mpid);
static UTILITY_NOINLINE int utilityAppletStopUnloadModule(SceUID modid, int mpid);
static UTILITY_NOINLINE int utilityIsDialogMemGuardEnabled(void);
static UTILITY_NOINLINE int utilityAppletInitStart(u32 applet_type, void *params);
static UTILITY_NOINLINE int utilityAppletShutdownStart(u32 applet_type);
static UTILITY_NOINLINE int utilityAppletInit(void);
static UTILITY_NOINLINE int utilityAppletTerm(void);
static UTILITY_NOINLINE int utilityAppletUpdate(u32 applet_type, int speed);
static UTILITY_NOINLINE int utilityAppletGetStatus(u32 applet_type);
static UTILITY_NOINLINE SceSize utilityAppletGetPartitionSize(u32 applet_type, u32 api_version, const SceUtilityHtmlViewerHeader *params);
static UTILITY_NOINLINE SceUID utilityAppletLoadModule(const char *path, SceUID *block_p, int flags, void *opt);
static UTILITY_NOINLINE int utilityAppletUnloadModule(SceUID modid);
static UTILITY_NOINLINE SceUID utilityAppletLoadStartModule(const char *path, int mpid, SceSize argsize, void *argp, SceUID *block_p);
static UTILITY_NOINLINE int utilityIsDialogModuleGuardEnabled(void);
static UTILITY_NOINLINE int utilityFillDialogModuleGuard(SceUID block_id);
static UTILITY_NOINLINE int utilityCheckDialogModuleGuard(SceUID block_id);

static UTILITY_NOINLINE int utilityLoadNetModuleGroup(u32 mod_id);
static UTILITY_NOINLINE int utilityInitNetModules(void);
static UTILITY_NOINLINE int utilityUnloadNetModuleGroup(u32 mod_id);
static UTILITY_NOINLINE int utilityInitDnasModules(void);
static UTILITY_NOINLINE SceUtilityModuleGroup *utilityFindNetModuleGroup(u32 mod_id);
static UTILITY_NOINLINE SceUtilityModuleGroup *utilityFindUsbModuleGroup(u32 mod_id);
static UTILITY_NOINLINE SceUtilityModuleGroup *utilityFindAvModuleGroup(u32 mod_id);
static UTILITY_NOINLINE int utilityInitUsbModules(void);
static UTILITY_NOINLINE int utilityLoadUsbModuleGroup(u32 mod_id);
static UTILITY_NOINLINE int utilityUnloadUsbModuleGroup(u32 mod_id);
static UTILITY_NOINLINE SceUtilityModuleGroup *utilityFindLegacyUsbModuleGroup(u32 mod_id);
static UTILITY_NOINLINE SceUtilityModuleGroup *utilityFindUsbAccessoryGroup(u32 mod_id);
static UTILITY_NOINLINE int utilityIsUsbGpsSerialDriver(void);
static UTILITY_NOINLINE int utilityLoadAvModuleGroup(u32 mod_id);
static UTILITY_NOINLINE int utilityUnloadAvModuleGroup(u32 mod_id);
static UTILITY_NOINLINE int utilityInitAvModules(void);
static UTILITY_NOINLINE int utilityInitNpModules(void);
static UTILITY_NOINLINE int utilityLoadNpModuleGroup(u32 mod_id);
static UTILITY_NOINLINE int utilityUnloadNpModuleGroup(u32 mod_id);
static UTILITY_NOINLINE int utilityLoadNpDrmModuleGroup(u32 mod_id);
static UTILITY_NOINLINE int utilityUnloadNpDrmModuleGroup(u32 mod_id);
static UTILITY_NOINLINE int utilityIsNpDrmCheckEnabled(void);
static UTILITY_NOINLINE int utilityInitNpDrmModules(void);
static UTILITY_NOINLINE int utilityLoadIrModuleGroup(u32 mod_id);
static UTILITY_NOINLINE int utilityInitIrModules(void);
static UTILITY_NOINLINE int utilityUnloadIrModuleGroup(u32 mod_id);
static UTILITY_NOINLINE int utilityInitNpInstModules(void);
static UTILITY_NOINLINE int utilityLoadModuleGroup(SceUtilityModuleGroup *group);
static UTILITY_NOINLINE int utilityUnloadModuleGroup(SceUtilityModuleGroup *group);
static UTILITY_NOINLINE int utilityInitModuleGroup(SceUtilityModuleGroup *group);
static UTILITY_NOINLINE int utilityLoadKernelModule(SceUtilityKernMod *entry);
static UTILITY_NOINLINE int utilityUnloadKernelModule(SceUtilityKernMod *entry);
static UTILITY_NOINLINE int utilityLoadUserModule(SceUtilityUserMod *entry, SceUID block_id, SceUID *rem_block);
static UTILITY_NOINLINE int utilityUnloadUserModule(SceUtilityUserMod *entry);
static UTILITY_NOINLINE int utilityIsModuleGuardEnabled(void);

static UTILITY_NOINLINE int utilityKermitOskInit(void);
static UTILITY_NOINLINE int utilityKermitOskStartItem(SceUtilityOskParams *params, int item_index);
static UTILITY_NOINLINE int utilityKermitOskFinishItem(int item_index);

static UTILITY_NOINLINE int utilityRegGetSysParamInt(const char *cat_path, const char *key_name, s32 *out_val);
static UTILITY_NOINLINE int utilityRegGetNetInt(const char *cat_path, const char *key_name, s32 *out_val);
static UTILITY_NOINLINE int utilityRegSetSysParamInt(const char *cat_path, const char *key_name, s32 value);
static UTILITY_NOINLINE int utilityRegSetNetInt(const char *cat_path, const char *key_name, s32 value);
static UTILITY_NOINLINE int utilityRegGetSysParamString(const char *cat_path, const char *key_name, char *out_buf, s32 buf_size);
static UTILITY_NOINLINE int utilityRegGetMessengerString(const char *cat_path, const char *key_name, char *out_buf, s32 buf_size);

static UTILITY_NOINLINE int utilityNetParamCheckCb(void *arg);
static UTILITY_NOINLINE int utilityNetParamCreateCb(void *arg);
static UTILITY_NOINLINE int utilityNetParamDeleteCb(void *arg);
static UTILITY_NOINLINE int utilityNetParamCopyCb(void *arg);
static UTILITY_NOINLINE int utilityNetParamGetCb(void *arg);
static UTILITY_NOINLINE int utilityNetParamSetCb(void *arg);
static UTILITY_NOINLINE int utilityRegSetMessengerString(const char *cat_path, const char *key_name, const char *str_buf, s32 size);
static UTILITY_NOINLINE int utilityNetParamOpenSlot(int slot, SceUtilityNetParamHandle *handle, int create);
static UTILITY_NOINLINE int utilityNetParamRemoveSlot(int slot);
static UTILITY_NOINLINE int utilityNetParamInitSlot(int slot);
static UTILITY_NOINLINE int utilityNetParamCopyHandle(SceUtilityNetParamHandle *src, SceUtilityNetParamHandle *dst);
static UTILITY_NOINLINE int utilityNetParamFormatPath(int slot, char *out_buf, u32 buf_size);
static UTILITY_NOINLINE int utilityNetParamCloseSlot(SceUtilityNetParamHandle *handle);
static UTILITY_NOINLINE int utilityNetParamReadParam(SceUtilityNetParamHandle *handle, int param, s32 buf_size, void *out_buf);
static UTILITY_NOINLINE int utilityNetParamWriteParamWrap(SceUtilityNetParamHandle *handle, int param, const void *value);
static UTILITY_NOINLINE int utilityNetParamWriteParam(SceUtilityNetParamHandle *handle, int param, const void *value);
static UTILITY_NOINLINE int utilityNetParamCreateDefaults(SceUtilityNetParamHandle *handle);
static UTILITY_NOINLINE int utilityNetParamCopyKey(SceUtilityNetParamHandle *src, SceUtilityNetParamHandle *dst, int param, s32 buf_size, void *scratch);
static UTILITY_NOINLINE int utilityNetParamValidateValue(int param, u32 key_size, const u32 *value);
static UTILITY_NOINLINE int utilityNetParamUpsertKey(SceUtilityNetParamHandle *handle, int param, const void *value);
static UTILITY_NOINLINE int utilityNetParamSlotExists(int slot);
static UTILITY_NOINLINE int utilityNetParamKeyExists(u32 cat, const char *key_name);
static UTILITY_NOINLINE int utilityNetParamGetRegKey(u32 cat, const char *key_name, u32 expected_type, s32 max_key_size, s32 buf_size, void *out_buf);
static UTILITY_NOINLINE int utilityNetParamSetOrCreateKey(u32 cat, const char *key_name, u32 key_type, s32 key_size, s32 write_size, const void *value);
static UTILITY_NOINLINE int utilityNetParamOpenCategory(const char *path, int create, u32 *out_cat);
static UTILITY_NOINLINE int utilityNetParamOpenRegistry(void);
static UTILITY_NOINLINE int utilityNetParamCloseRegistry(void);
static UTILITY_NOINLINE int utilityNetParamFlushCloseCategory(u32 cat);
static UTILITY_NOINLINE int utilityNetParamFlushRegistry(void);
static UTILITY_NOINLINE int utilityNetParamSetRegValue(u32 cat, const char *key_name, s32 size, const void *value);
static UTILITY_NOINLINE int utilityNetParamCreateAndSetKey(u32 cat, const char *key_name, u32 key_type, s32 key_size, s32 write_size, const void *value);
static UTILITY_NOINLINE int utilityNetParamRemoveCategory(const char *path);

static UTILITY_NOINLINE void utilityCompatMatchGame(u32 dialog_type, u32 dialog_submode);
static UTILITY_NOINLINE int utilityKermitSetDialogActive(int active);
static UTILITY_NOINLINE int utilityKermitFetchCapabilities(void);
static UTILITY_NOINLINE void utilityCompatClear(void);
static UTILITY_NOINLINE u32 utilityCompatGetFlags(void);
static UTILITY_NOINLINE int utilityCompatDelay(u32 dialog_type, int delay_slot);
static UTILITY_NOINLINE u64 utilityKermitQueryCapability(int cap_index);

/* Read-only tables */
static const SceKernelLMOption g_net_adhoc_compat_lm_opt = {
	sizeof(SceKernelLMOption), 0, 0, 0, 1, 1, { 0, 0 }
};

static const SceUtilityModStartOpt g_net_adhoc_compat_opt = {
	0, 0, NULL, &g_net_adhoc_compat_lm_opt
};

static const u16 g_osk_empty_utf16 = 0;

typedef struct SceUtilityNetParamDefaults {
	const char *cnf_name;
	const char *ssid;
	u32 auth_proto;
	u8 wep_key[16];
	s32 wep_key_len;
	u32 how_to_set_ip;
	const char *ip_address;
	const char *netmask;
	const char *default_route;
	u32 dns_flag;
	const char *dns_main;
	const char *dns_alt;
	const char *auth_name;
	const char *auth_key;
	u32 http_proxy_flag;
	const char *http_proxy_server;
	u32 http_proxy_port;
	u32 version;
	u32 device;
	u32 auth_dot1x_type;
	const char *auth_dot1x_name;
	const char *auth_dot1x_key;
	u32 wpa_key_type;
	u8 wpa_key[64];
	u32 browser_flag;
	u32 wifisvc_config;
	const char *wifisvc_auth_name;
	const char *wifisvc_auth_key;
	u32 wifisvc_option;
	const char *last_leased_dhcp;
	u32 bt_id;
	const char *at_command;
	const char *phone_number;
} SceUtilityNetParamDefaults;

extern const SceUtilityNetParamKey g_netparam_keys[32];
extern const SceUtilityNetParamDefaults g_netparam_defaults;

static const SceUtilityCompatEntry g_game_compat_table[17] = {
	{ "ULJM05005", 1, 1, 0x0, { 250000 } },
	{ "ULUS10005", 1, 1, 0x0, { 250000 } },
	{ "ULES00016", 1, 1, 0x0, { 250000 } },
	{ "ULJM05026", 1, 0, 0x1, { 0 } },
	{ "ULJM05026", 1, 1, 0x1, { 0 } },
	{ "ULUS10021", 6, 0, 0x1, { 0 } },
	{ "ULUS10021", 1, 0, 0x1, { 0 } },
	{ "ULJM05289", 1, 1, 0x2, { 0 } },
	{ "ULAS42113", 1, 1, 0x2, { 0 } },
	{ "ULUS10294", 1, 1, 0x2, { 0 } },
	{ "ULES00920", 1, 1, 0x2, { 0 } },
	{ "ULES00921", 1, 1, 0x2, { 0 } },
	{ "ULJM05289", 6, 0, 0x2, { 0 } },
	{ "ULAS42113", 6, 0, 0x2, { 0 } },
	{ "ULUS10294", 6, 0, 0x2, { 0 } },
	{ "ULES00920", 6, 0, 0x2, { 0 } },
	{ "ULES00921", 6, 0, 0x2, { 0 } },
};

/* Initialized module tables (.data) */
static SceSysEventHandler g_sys_ev_handler = {
	sizeof(SceSysEventHandler),
	"SceUtility",
	0x00FFFF00,
	utilitySysEventHandler,
	0, 0, NULL,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

/* Net module groups */
static SceUtilityKernMod g_net_common_kern[1] = {
	{ "flash0:/kd/ifhandle.prx", NULL, 0 },
};
static SceUtilityUserMod g_net_common_user[1] = {
	{ "flash0:/kd/pspnet.prx", NULL, 0, 0 },
};
static SceUtilityKernMod g_net_adhoc_kern[2] = {
	{ "flash0:/kd/memab.prx", NULL, 0 },
	{ "flash0:/kd/pspnet_adhoc_auth.prx", NULL, 0 },
};
static SceUtilityUserMod g_net_adhoc_user[5] = {
	{ "flash0:/kd/pspnet_adhoc.prx", NULL, 0, 0 },
	{ "flash0:/kd/pspnet_adhocctl.prx", NULL, 0, 0 },
	{ "flash0:/kd/pspnet_adhoc_matching.prx", NULL, 0, 0 },
	{ "flash0:/kd/pspnet_adhoc_download.prx", NULL, 0, 0 },
	{ "flash0:/kd/pspnet_adhoc_discover.prx", NULL, 0, 0 },
};
static SceUtilityUserMod g_net_inet_user[3] = {
	{ "flash0:/kd/pspnet_inet.prx", NULL, 0, 0 },
	{ "flash0:/kd/pspnet_apctl.prx", NULL, 0, 0 },
	{ "flash0:/kd/pspnet_resolver.prx", NULL, 0, 0 },
};
static SceUtilityUserMod g_net_parseuri_user[1] = {
	{ "flash0:/kd/libparse_uri.prx", NULL, 0, 0 },
};
static SceUtilityUserMod g_net_parsehttp_user[1] = {
	{ "flash0:/kd/libparse_http.prx", NULL, 0, 0 },
};
static SceUtilityKernMod g_net_http_kern[1] = {
	{ "flash0:/kd/http_storage.prx", NULL, 0 },
};
static SceUtilityUserMod g_net_http_user[1] = {
	{ "flash0:/kd/libhttp.prx", NULL, 0, 0 },
};
static SceUtilityKernMod g_net_ssl_kern[1] = {
	{ "flash0:/kd/cert_loader.prx", NULL, 0 },
};
static SceUtilityUserMod g_net_ssl_user[1] = {
	{ "flash0:/kd/libssl.prx", NULL, 0, 0 },
};
static SceUtilityUserMod g_net_upnp_user[1] = {
	{ "flash0:/kd/pspnet_upnp.prx", NULL, 0, 0 },
};
static SceUtilityUserMod g_net_gameupdate_user[1] = {
	{ "flash0:/kd/libgameupdate.prx", NULL, 0, 0 },
};

static SceUtilityModuleGroup g_net_groups[9] = {
	{ 0, 1, 1, g_net_common_kern, g_net_common_user, 0x14000, 0, 0 },
	{ 0, 2, 5, g_net_adhoc_kern, g_net_adhoc_user, 0x20000, 0, 0 },
	{ 0, 0, 3, NULL, g_net_inet_user, 0x58000, 0, 0 },
	{ 0, 0, 1, NULL, g_net_parseuri_user, 0x6000, 0, 0 },
	{ 0, 0, 1, NULL, g_net_parsehttp_user, 0x2000, 0, 0 },
	{ 0, 1, 1, g_net_http_kern, g_net_http_user, 0x28000, 0, 0 },
	{ 0, 1, 1, g_net_ssl_kern, g_net_ssl_user, 0x44000, 0, 0 },
	{ 0, 0, 1, NULL, g_net_upnp_user, 0x10000, 0, 0 },
	{ 0, 0, 1, NULL, g_net_gameupdate_user, 0x8000, 0, 0 },
};

/* DNAS module group */
static SceUtilityKernMod g_dnas_kern[2] = {
	{ "flash0:/kd/mcctrl.prx", NULL, 0 },
	{ "flash0:/kd/libdnas_core.prx", NULL, 0 },
};
static SceUtilityUserMod g_dnas_user[1] = {
	{ "flash0:/kd/libdnas.prx", NULL, 0, 0 },
};
static SceUtilityModuleGroup g_dnas_group = {
	0, 2, 1, g_dnas_kern, g_dnas_user, 0x8000, 0, 0
};

/* USB module groups */
static SceUtilityKernMod g_usb_pspcm_kern[1] = { { "flash0:/kd/usbpspcm.prx", NULL, 0 } };
static SceUtilityKernMod g_usb_mic_kern[1]   = { { "flash0:/kd/usbmic.prx", NULL, 0 } };
static SceUtilityKernMod g_usb_cam_kern[1]   = { { "flash0:/kd/usbcam.prx", NULL, 0 } };
static SceUtilityKernMod g_usb_gps_kern[1]   = { { "flash0:/kd/usbgps.prx", NULL, 0 } };
static char g_usbdmb_path[] = "flash0:/kd/usbdmb.prx";
static SceUtilityKernMod g_usb_dmb_kern[1]   = { { g_usbdmb_path, NULL, 0 } };
static SceUtilityKernMod g_usb_acc_kern[1]   = { { "flash0:/kd/usbacc.prx", NULL, 0 } };
static SceUtilityKernMod g_usb_gps_serial_kern[1] = { { "flash0:/kd/usbgps_serial.prx", NULL, 0 } };

static SceUtilityModuleGroup g_usb_groups[4] = {
	{ 0, 1, 0, g_usb_pspcm_kern, NULL, 0, 0, 0 },
	{ 0, 1, 0, g_usb_mic_kern, NULL, 0, 0, 0 },
	{ 0, 1, 0, g_usb_cam_kern, NULL, 0, 0, 0 },
	{ 0, 1, 0, g_usb_gps_kern, NULL, 0, 0, 0 },
};
static SceUtilityModuleGroup g_usb_dmb_group = {
	0, 1, 0, g_usb_dmb_kern, NULL, 0, 0, 0
};
static SceUtilityModuleGroup g_usb_aux_groups[3] = {
	{ 0, 1, 0, g_usb_acc_kern, NULL, 0, 0, 0 },
	{ 0, 1, 0, g_usb_gps_serial_kern, NULL, 0, 0, 0 },
	{ 0, 0, 0, NULL, NULL, 0, 0, 0 },
};

/* AV module groups */
static SceUtilityKernMod g_av_codec_kern[1]   = { { "flash0:/kd/avcodec.prx", NULL, 0 } };
static SceUtilityKernMod g_av_sascore_kern[1] = { { "flash0:/kd/sc_sascore.prx", NULL, 0 } };
static SceUtilityKernMod g_av_vaudio_kern[1]  = { { "flash0:/kd/vaudio.prx", NULL, 0 } };
static SceUtilityUserMod g_av_atrac_user[1]   = { { "flash0:/kd/libatrac3plus.prx", NULL, 0, 0 } };
static SceUtilityUserMod g_av_mpeg_user[1]    = { { "flash0:/kd/mpeg.prx", NULL, 0, 0 } };
static SceUtilityUserMod g_av_asf_user[1]     = { { "flash0:/kd/libasfparser.prx", NULL, 0, 0 } };
static int g_mp3_compat_flag = 1;
static SceUtilityUserMod g_av_mp3_user[1]     = { { "flash0:/kd/libmp3.prx", NULL, 0, 0 } };
static SceUtilityUserMod g_av_aac_user[1]     = { { "flash0:/kd/libaac.prx", NULL, 0, 0 } };
static SceUtilityKernMod g_av_g729_kern[1]    = { { "flash0:/kd/g729.prx", NULL, 0 } };
static SceUtilityUserMod g_av_mp4_user[2]     = {
	{ "flash0:/kd/mp4msv.prx", NULL, 0, 0 },
	{ "flash0:/kd/libmp4.prx", NULL, 0, 0 },
};

static SceUtilityModuleGroup g_av_groups[9] = {
	{ 0, 1, 0, g_av_codec_kern, NULL, 0, 0, 0 },
	{ 0, 1, 0, g_av_sascore_kern, NULL, 0, 0, 0 },
	{ 0, 0, 1, NULL, g_av_atrac_user, 0x8000, 0, 0 },
	{ 0, 0, 1, NULL, g_av_mpeg_user, 0xC000, 0, 0 },
	{ 0, 0, 1, NULL, g_av_mp3_user, 0x4000, 0, 0 },
	{ 0, 1, 0, g_av_vaudio_kern, NULL, 0, 0, 0 },
	{ 0, 0, 1, NULL, g_av_aac_user, 0x4000, 0, 0 },
	{ 0, 1, 0, g_av_g729_kern, NULL, 0, 0, 0 },
	{ 0, 0, 2, NULL, g_av_mp4_user, 0x3C000, 0, 0 },
};
static SceUtilityModuleGroup g_av_wma_group = {
	0, 0, 1, NULL, g_av_asf_user, 0x14000, 0, 0
};

/* NP module groups */
static SceUtilityKernMod g_np_common_kern[1]   = { { "flash0:/kd/np_core.prx", NULL, 0 } };
static SceUtilityUserMod g_np_common_user[2]   = {
	{ "flash0:/kd/np.prx", NULL, 0, 0 },
	{ "flash0:/kd/np_auth.prx", NULL, 0, 0 },
};
static SceUtilityUserMod g_np_service_user[1]  = { { "flash0:/kd/np_service.prx", NULL, 0, 0 } };
static SceUtilityUserMod g_np_matching_user[1] = { { "flash0:/kd/np_matching2.prx", NULL, 0, 0 } };
static SceUtilityUserMod g_np_commerce_user[1] = { { "flash0:/kd/np_commerce2.prx", NULL, 0, 0 } };

static SceUtilityModuleGroup g_np_groups[4] = {
	{ 0, 1, 2, g_np_common_kern, g_np_common_user, 0xC000, 0, 0 },
	{ 0, 0, 1, NULL, g_np_service_user, 0x18000, 0, 0 },
	{ 0, 0, 1, NULL, g_np_matching_user, 0x48000, 0, 0 },
	{ 0, 0, 1, NULL, g_np_commerce_user, 0xE000, 0, 0 },
};

/* NPDRM module groups */
static SceUtilityKernMod g_npdrm_retail_kern[1] = { { "flash0:/kd/npdrm.prx", NULL, 0 } };
static SceUtilityKernMod g_npdrm_tool_kern[1]   = { { "flash0:/kd/npdrm_tool.prx", NULL, 0 } };
static SceUtilityModuleGroup g_npdrm_groups[2] = {
	{ 0, 1, 0, g_npdrm_retail_kern, NULL, 0, 0, 0 },
	{ 0, 1, 0, g_npdrm_tool_kern, NULL, 0, 0, 0 },
};

/* IrDA / SIRCS module groups */
static SceUtilityKernMod g_irda_kern[1]  = { { "flash0:/kd/irda.prx", NULL, 0 } };
static SceUtilityKernMod g_sircs_kern[1] = { { "flash0:/kd/sircs.prx", NULL, 0 } };
static SceUtilityModuleGroup g_ir_groups[2] = {
	{ 0, 1, 0, g_irda_kern, NULL, 0, 0, 0 },
	{ 0, 1, 0, g_sircs_kern, NULL, 0, 0, 0 },
};

/* NP installer module group */
static SceUtilityKernMod g_np_inst_kern[1] = { { "flash0:/kd/np_inst.prx", NULL, 0 } };
static SceUtilityModuleGroup g_np_inst_group = {
	0, 1, 0, g_np_inst_kern, NULL, 0, 0, 0
};

/* Uninitialized globals (.bss) */
static SceUtilityDialogState g_dialog;
static SceUtilityAppletState g_applet;
static SceUtilityAppletModSlot g_applet_mods[8];
static int g_dialog_mem_inited;
static SceUtilityModStartOpt g_mp3_compat_opt;
static SceUtilityOskState g_osk;
static int g_netparam_reg_refs;
static u32 g_netparam_reg_handle;
static const SceUtilityCompatEntry *g_active_compat;
static u64 g_kermit_caps[3];
static SceUtilityDialogMemNode g_dialog_mem_nodes[3];
static SceUtilityDialogMemNode g_dialog_mem_head;
static u32 g_dialog_mem_guard_seed;

/* Module entry points */
int sceUtility_driver_F062AEA6(void)
{
	int ret;

	ret = utilityKermitOskInit();
	if (ret < 0)
		return ret;

	ret = utilityKermitFetchCapabilities();
	if (ret < 0)
		return ret;

	g_dialog.worktime_evf = sceKernelCreateEventFlag("SceUtilityWorktime", 0x200, 0, NULL);
	if (g_dialog.worktime_evf < 0)
		return SCE_KERNEL_ERROR_NO_MEMORY;

	g_dialog.suspend_evf = sceKernelCreateEventFlag("SceUtilitySuspend", 0x200, 3, NULL);
	if (g_dialog.suspend_evf < 0)
		return SCE_KERNEL_ERROR_NO_MEMORY;

	g_dialog.emulated_ms = -1;
	g_dialog.active_type = 0;
	g_dialog.dialog_submode = 0;
	g_dialog.status = 0;
	g_dialog.in_suspend = 0;
	g_dialog.abort_flag = 0;
	g_dialog.params = NULL;
	g_dialog.api_version = 0;

	sceKernelRegisterSysEventHandler(&g_sys_ev_handler);
	utilityAppletInit();
	utilityInitNetModules();
	utilityInitUsbModules();
	utilityInitAvModules();
	utilityInitNpModules();
	utilityInitNpDrmModules();
	utilityInitIrModules();
	utilityInitDnasModules();
	utilityInitNpInstModules();
	return 0;
}

static UTILITY_NOINLINE int utilityDialogInitStart(u32 dialog_type, void *params)
{
	u32 dialog_submode = 0;
	u32 api_version = 0;
	int ret;
	SceUtilityDialogCommon *common = (SceUtilityDialogCommon *)params;
	const char *ms_dev;

	if (g_dialog.status != 0)
		return SCE_UTILITY_ERROR_INVALID_STATUS;

	if (utilityIsBadRange(params, sizeof(SceUtilityDialogCommon)))
		return SCE_UTILITY_ERROR_INVALID_ADDR;

	switch (dialog_type) {
	case 1: {
		SceUtilitySavedataHeader *sd = (SceUtilitySavedataHeader *)params;
		u32 sz = common->size;
		u32 mode;

		if (sz == 1536) {
			api_version = 200;
		} else if (sz == 1500) {
			api_version = 150;
		} else if (sz == 1480) {
			api_version = 103;
		} else {
			return SCE_UTILITY_ERROR_INVALID_SIZE;
		}
		if (utilityIsBadRange(params, sz))
			return SCE_UTILITY_ERROR_INVALID_ADDR;

		mode = sd->operation_mode;
		if (mode < 2 || mode == 8 || mode == 9 || (mode >= 11 && mode <= 22))
			dialog_submode = 1;
		break;
	}
	case 3: {
		SceUtilityGameSharingHeader *gs = (SceUtilityGameSharingHeader *)params;
		u32 sz = common->size;

		if (sz == 100) {
			api_version = 570;
			if (utilityIsBadRange(params, 100))
				return SCE_UTILITY_ERROR_INVALID_ADDR;
			if (utilityIsBadSpan(gs->ext_buf, gs->ext_size))
				return SCE_UTILITY_ERROR_INVALID_ADDR;
		} else {
			if (sz == 84)
				api_version = 150;
			else if (sz == 80)
				api_version = 103;
			else
				return SCE_UTILITY_ERROR_INVALID_SIZE;

			if (utilityIsBadRange(params, sz))
				return SCE_UTILITY_ERROR_INVALID_ADDR;
			if (utilityIsBadRange(gs->data_ptr, 4))
				return SCE_UTILITY_ERROR_INVALID_ADDR;
		}
		break;
	}
	case 4: {
		u32 sz = common->size;
		if (sz == 68)
			api_version = 300;
		else if (sz == 64)
			api_version = 200;
		else if (sz == 56)
			api_version = 150;
		else
			return SCE_UTILITY_ERROR_INVALID_SIZE;
		if (utilityIsBadRange(params, sz))
			return SCE_UTILITY_ERROR_INVALID_ADDR;
		break;
	}
	case 5: {
		u32 sz = common->size;
		if (sz == 708)
			api_version = 370;
		else if (sz == 580)
			api_version = 150;
		else if (sz == 572)
			api_version = 103;
		else
			return SCE_UTILITY_ERROR_INVALID_SIZE;
		if (utilityIsBadRange(params, sz))
			return SCE_UTILITY_ERROR_INVALID_ADDR;
		break;
	}
	case 6: {
		u32 sz = common->size;
		if (sz == 64)
			api_version = 103;
		else if (sz == 68)
			api_version = 1001;
		else
			return SCE_UTILITY_ERROR_INVALID_SIZE;
		if (utilityIsBadRange(params, sz))
			return SCE_UTILITY_ERROR_INVALID_ADDR;
		break;
	}
	case 7:
		if (common->size != 56)
			return SCE_UTILITY_ERROR_INVALID_SIZE;
		api_version = 200;
		if (utilityIsBadRange(params, 56))
			return SCE_UTILITY_ERROR_INVALID_ADDR;
		break;
	case 8:
		if (common->size != 52)
			return SCE_UTILITY_ERROR_INVALID_SIZE;
		api_version = 200;
		if (utilityIsBadRange(params, 52))
			return SCE_UTILITY_ERROR_INVALID_ADDR;
		break;
	case 9:
		if (common->size == 100) {
			api_version = 200;
			if (utilityIsBadRange(params, 100))
				return SCE_UTILITY_ERROR_INVALID_ADDR;
		}
		break;
	case 10:
		if (common->size != 48)
			return SCE_UTILITY_ERROR_INVALID_SIZE;
		api_version = 280;
		if (utilityIsBadRange(params, 48))
			return SCE_UTILITY_ERROR_INVALID_ADDR;
		break;
	case 11: {
		SceUtilityStoreHeader *st = (SceUtilityStoreHeader *)params;
		u32 sz = common->size;
		u32 check_size;

		if (sz == 932) {
			api_version = 600;
			check_size = 932;
		} else if (sz == 928) {
			api_version = 380;
			check_size = 928;
		} else if (sz == 436) {
			api_version = 370;
			check_size = 932;
		} else {
			return SCE_UTILITY_ERROR_INVALID_SIZE;
		}
		if (utilityIsBadRange(params, check_size))
			return SCE_UTILITY_ERROR_INVALID_ADDR;
		if (utilityIsBadSpan(st->buffer_ptr, st->item_count * st->item_stride))
			return SCE_UTILITY_ERROR_INVALID_ADDR;
		break;
	}
	case 12:
		if (common->size != 64)
			return SCE_UTILITY_ERROR_INVALID_SIZE;
		if (((u32)params & 7) != 0)
			return SCE_UTILITY_ERROR_INVALID_ADDR;
		api_version = 370;
		if (utilityIsBadRange(params, 64))
			return SCE_UTILITY_ERROR_INVALID_ADDR;
		break;
	case 13:
		if (common->size != 224)
			return SCE_UTILITY_ERROR_INVALID_SIZE;
		api_version = 380;
		if (utilityIsBadRange(params, 224))
			return SCE_UTILITY_ERROR_INVALID_ADDR;
		break;
	case 14:
		if (common->size != 84)
			return SCE_UTILITY_ERROR_INVALID_SIZE;
		api_version = 380;
		if (utilityIsBadRange(params, 84))
			return SCE_UTILITY_ERROR_INVALID_ADDR;
		break;
	case 15:
		if (common->size != 112)
			return SCE_UTILITY_ERROR_INVALID_SIZE;
		api_version = 391;
		if (utilityIsBadRange(params, 112))
			return SCE_UTILITY_ERROR_INVALID_ADDR;
		break;
	case 16: {
		u32 sz = common->size;
		sceKermitPeripheral_driver_61BEA1D2(0);
		if (sz == 80)
			api_version = 600;
		else if (sz == 76)
			api_version = 500;
		else
			return SCE_UTILITY_ERROR_INVALID_SIZE;
		if (utilityIsBadRange(params, sz))
			return SCE_UTILITY_ERROR_INVALID_ADDR;
		break;
	}
	case 17: {
		u32 sz = common->size;
		if (sz == 1432)
			api_version = 630;
		else if (sz == 1424)
			api_version = 620;
		else
			return SCE_UTILITY_ERROR_INVALID_SIZE;
		if (utilityIsBadRange(params, sz))
			return SCE_UTILITY_ERROR_INVALID_ADDR;
		break;
	}
	case 18: {
		SceUtilityFriendHeader *fr = (SceUtilityFriendHeader *)params;
		u32 sz = common->size;
		if (sz == 84) {
			if (fr->request_kind == 1)
				sceKermitPeripheral_driver_61BEA1D2(0);
			api_version = 635;
		} else if (sz == 80) {
			if (fr->request_kind == 1)
				sceKermitPeripheral_driver_61BEA1D2(0);
			api_version = 630;
		} else {
			return SCE_UTILITY_ERROR_INVALID_SIZE;
		}
		if (utilityIsBadRange(params, sz))
			return SCE_UTILITY_ERROR_INVALID_ADDR;
		break;
	}
	case 19:
		if (common->size != 56)
			return SCE_UTILITY_ERROR_INVALID_SIZE;
		api_version = 600;
		if (utilityIsBadRange(params, 56))
			return SCE_UTILITY_ERROR_INVALID_ADDR;
		break;
	default:
		return SCE_UTILITY_ERROR_INVALID_TYPE;
	}

	g_dialog.params = params;
	g_dialog.dialog_submode = dialog_submode;
	g_dialog.api_version = api_version;
	g_dialog.status = 1;
	g_dialog.active_type = dialog_type;

	utilityCompatMatchGame(dialog_type, dialog_submode);

	ms_dev = "mscmhc0:";
	if (dialog_type == 1) {
		if (sceUtility_private_59EB3A17(2) == 1)
			ms_dev = "mscmhcemu0:";
	} else if (dialog_type == 9) {
		if (sceUtility_private_59EB3A17(4) == 1)
			ms_dev = "mscmhcemu0:";
	}
	g_dialog.devctl_result = sceIoDevctl(ms_dev, 0x0202580A, NULL, 0, g_dialog.devctl_out, 16);

	g_dialog.init_thid = sceKernelCreateThread(
		"SceUtilityInit", utilityDialogLoadThread,
		common->access_thread, 2048, 0x00100000, NULL);
	if (g_dialog.init_thid <= 0) {
		ret = g_dialog.init_thid;
		g_dialog.init_thid = 0;
		return ret;
	}

	ret = sceKernelStartThread(g_dialog.init_thid, 0, NULL);
	if (ret != 0) {
		sceKernelDeleteThread(g_dialog.init_thid);
		g_dialog.init_thid = 0;
		return ret;
	}
	return 0;
}

static UTILITY_NOINLINE int utilityDialogShutdownStart(u32 dialog_type)
{
	int ret;
	SceUtilityDialogCommon *common;

	if (g_dialog.active_type != dialog_type)
		return SCE_UTILITY_ERROR_INVALID_DIALOG;
	if (g_dialog.status != 3)
		return SCE_UTILITY_ERROR_INVALID_STATUS;

	if (g_dialog.active_type == 16 || g_dialog.active_type == 18)
		sceKermitPeripheral_driver_61BEA1D2(1);

	g_dialog.status = 4;
	common = (SceUtilityDialogCommon *)g_dialog.params;
	g_dialog.init_thid = sceKernelCreateThread(
		"SceUtilityShutdown", utilityDialogUnloadThread,
		common->access_thread, 2048, 0x00100000, NULL);
	if (g_dialog.init_thid <= 0) {
		ret = g_dialog.init_thid;
		g_dialog.init_thid = 0;
		return ret;
	}

	ret = sceKernelStartThread(g_dialog.init_thid, 0, NULL);
	if (ret != 0) {
		sceKernelDeleteThread(g_dialog.init_thid);
		g_dialog.init_thid = 0;
		return ret;
	}
	return 0;
}

static UTILITY_NOINLINE int utilityDialogResume(u32 dialog_type, void *params)
{
	u32 api_version = 0;
	SceUtilityDialogCommon *common = (SceUtilityDialogCommon *)params;

	if (g_dialog.status != 5)
		return SCE_UTILITY_ERROR_INVALID_STATUS;
	if (utilityIsBadRange(params, sizeof(SceUtilityDialogCommon)))
		return SCE_UTILITY_ERROR_INVALID_ADDR;
	if (g_dialog.active_type != dialog_type)
		return SCE_UTILITY_ERROR_INVALID_DIALOG;

	if (dialog_type == 11) {
		SceUtilityStoreHeader *st = (SceUtilityStoreHeader *)params;
		u32 sz = common->size;

		if (sz == 932)
			api_version = 600;
		else if (sz == 928)
			api_version = 380;
		else
			return SCE_UTILITY_ERROR_INVALID_SIZE;

		if (utilityIsBadRange(params, sz))
			return SCE_UTILITY_ERROR_INVALID_ADDR;
		if (utilityIsBadSpan(st->buffer_ptr, st->item_count * st->item_stride))
			return SCE_UTILITY_ERROR_INVALID_ADDR;
	} else if (dialog_type == 15) {
		if (common->size != 112)
			return SCE_UTILITY_ERROR_INVALID_SIZE;
		api_version = 391;
		if (utilityIsBadRange(params, 112))
			return SCE_UTILITY_ERROR_INVALID_ADDR;
	}

	g_dialog.active_type = dialog_type;
	g_dialog.params = params;
	g_dialog.api_version = api_version;
	g_dialog.status = 2;
	g_dialog.dialog_submode = 0;

	sceKernelSetEventFlag(g_dialog.worktime_evf, 0x10);
	sceKernelWaitEventFlag(g_dialog.worktime_evf, 0x20, 0x20, NULL, NULL);
	return 0;
}

int sceUtility_private_363A12E2(u32 *out_buf)
{
	u32 old_k1 = pspGetK1();

	if (((s32)((((u32)out_buf + 16) | (u32)out_buf) & (old_k1 << 11))) < 0)
		return SCE_UTILITY_ERROR_INVALID_ADDR;
	if (g_dialog.devctl_result != 0)
		return g_dialog.devctl_result;

	out_buf[0] = g_dialog.devctl_out[0];
	out_buf[1] = g_dialog.devctl_out[1];
	out_buf[2] = g_dialog.devctl_out[2];
	out_buf[3] = g_dialog.devctl_out[3];
	return 0;
}

int sceUtility_private_59EB3A17(int flag)
{
	u32 old_k1 = utilityEnterKernel();
	int mask = flag;
	int ret;

	if ((u32)g_dialog.emulated_ms < 2 && flag == 2) {
		ret = g_dialog.emulated_ms;
		utilityLeaveKernel(old_k1);
		return ret;
	}

	if (flag != 2 && flag != 4 && flag != 8 && flag != 16 && flag != 32 && flag != 64)
		mask = 1;

	{
		SceKernelGameInfo *game_info = (SceKernelGameInfo *)sceKernelGetGameInfo();
		ret = (game_info->emu_flags & (u32)mask) != 0;
	}
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtility_private_78EC982D(int kind, int value)
{
	if (kind != 2)
		return -1;
	g_dialog.emulated_ms = value;
	return value;
}

int sceUtility_private_5ED16EBA(const char *name, int flags, const void *opt)
{
	u32 old_k1 = utilityEnterKernel();
	const char *path = NULL;
	int ret;

	if (utilityIsBadPtr(name) || utilityIsBadRange(opt, sizeof(SceKernelLMOption))) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_ERROR_INVALID_ADDR;
	}

	switch (name[0]) {
	case 'a':
		if (strcmp(name, "auth_plugin") == 0)
			path = "flash0:/vsh/module/auth_plugin.prx";
		else if (strcmp(name, "auto_connect") == 0)
			path = "flash0:/vsh/module/auto_connect.prx";
		break;
	case 'd':
		if (strcmp(name, "dnas_plugin") == 0)
			path = "flash0:/vsh/module/dnas_plugin.prx";
		else if (strcmp(name, "dd_helper") == 0)
			path = "flash0:/vsh/module/dd_helper_utility.prx";
		break;
	case 'f':
		if (strcmp(name, "file_parser_base") == 0)
			path = "flash0:/vsh/module/file_parser_base.prx";
		break;
	case 'g':
		if (strcmp(name, "game_install_plugin") == 0 || strcmp(name, "gamedata_install_plugin") == 0)
			path = "flash0:/vsh/module/game_install_plugin.prx";
		break;
	case 'l':
		if (strcmp(name, "libupdown") == 0)
			path = "flash0:/kd/libupdown.prx";
		break;
	case 'm':
		if (strcmp(name, "msgdialog_plugin") == 0)
			path = "flash0:/vsh/module/msgdialog_plugin.prx";
		break;
	case 'n':
		if (strcmp(name, "netconf_plugin") == 0)
			path = "flash0:/vsh/module/netconf_plugin.prx";
		else if (strcmp(name, "netconf_bt_plugin") == 0)
			path = "flash0:/vsh/module/netconf_bt_plugin.prx";
		else if (strcmp(name, "netconf_plugin_auto_nec") == 0)
			path = "flash0:/vsh/module/netconf_plugin_auto_nec.prx";
		else if (strcmp(name, "netconf_plugin_auto_bfl") == 0)
			path = "flash0:/vsh/module/netconf_plugin_auto_bfl.prx";
		else if (strcmp(name, "netplay_server_utility") == 0)
			path = "flash0:/vsh/module/netplay_server_utility.prx";
		else if (strcmp(name, "netplay_server2_utility") == 0)
			path = "flash0:/vsh/module/netplay_server2_utility.prx";
		else if (strcmp(name, "netplay_server_plus_utility") == 0)
			path = "flash0:/vsh/module/netplay_server_plus_utility.prx";
		else if (strcmp(name, "npsignin_plugin") == 0)
			path = "flash0:/vsh/module/npsignin_plugin.prx";
		else if (strcmp(name, "npinstaller_plugin") == 0)
			path = "flash0:/vsh/module/npinstaller_plugin.prx";
		else if (strcmp(name, "np_commerce2_regcam") == 0)
			path = "flash0:/kd/np_commerce2_regcam.prx";
		else if (strcmp(name, "np_commerce2_store") == 0)
			path = "flash0:/kd/np_commerce2_store.prx";
		break;
	case 'o':
		if (strcmp(name, "osk_plugin") == 0)
			path = "flash0:/vsh/module/kermit_osk_plugin.prx";
		break;
	case 'p':
		if (strcmp(name, "pspnet_wispr") == 0)
			path = "flash0:/kd/pspnet_wispr.prx";
		else if (strcmp(name, "ps3scan_plugin") == 0)
			path = "flash0:/vsh/module/ps3scan_plugin.prx";
		else if (strcmp(name, "psn_utility") == 0)
			path = "flash0:/vsh/module/psn_utility.prx";
		break;
	case 'r':
		if (strcmp(name, "rss_subscriber") == 0)
			path = "flash0:/vsh/module/rss_subscriber.prx";
		else if (strcmp(name, "rss_common") == 0)
			path = "flash0:/vsh/module/rss_common.prx";
		else if (strcmp(name, "rss_reader") == 0)
			path = "flash0:/vsh/module/rss_reader.prx";
		break;
	case 's':
		if (strcmp(name, "savedata_utility") == 0)
			path = "flash0:/vsh/module/savedata_utility.prx";
		else if (strcmp(name, "savedata_auto_dialog") == 0)
			path = "flash0:/vsh/module/savedata_auto_dialog.prx";
		else if (strcmp(name, "screenshot_plugin") == 0)
			path = "flash0:/vsh/module/screenshot_plugin.prx";
		else if (strcmp(name, "store_checkout_utility") == 0)
			path = "flash0:/vsh/module/store_checkout_utility.prx";
		break;
	default:
		break;
	}

	if (path == NULL)
		ret = SCE_KERNEL_ERROR_NO_FILE;
	else
		ret = sceKernelLoadModuleForKernel(path, flags, (void *)opt);

	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtility_private_E8EE4E87(SceCtrlData *pad_buf, int count)
{
	u32 old_k1 = utilityEnterKernel();
	int ret;
	int idx;

	if (utilityIsBadSpan(pad_buf, (u32)(count * 16))) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_ERROR_INVALID_ADDR;
	}

	ret = sceCtrlPeekBufferPositive(pad_buf, count);
	for (idx = 0; idx < count; idx++)
		pad_buf[idx].buttons &= 0x0002F3F9u;

	utilityLeaveKernel(old_k1);
	return ret;
}

static UTILITY_NOINLINE int utilitySysEventHandler(int ev_id, char *ev_name, void *param, int *result)
{
	u32 compat_flags;
	int should_suspend;
	(void)ev_name;
	(void)param;
	(void)result;

	compat_flags = utilityCompatGetFlags();
	should_suspend = ((((compat_flags ^ 1u) & 1u) & (u32)(ev_id == 0x20F)) != 0 ||
	                  ((compat_flags & 1u) & (u32)(ev_id == 0x210)) != 0);
	if (should_suspend) {
		sceKernelClearEventFlag(g_dialog.suspend_evf, ~1u);
		g_dialog.in_suspend = 1;
		sceKernelWaitEventFlag(g_dialog.suspend_evf, 2, 0, NULL, NULL);
		g_dialog.in_suspend = 0;
	} else if (ev_id == 0x400000) {
		sceKernelSetEventFlag(g_dialog.suspend_evf, 1);
	}
	return 0;
}

int module_start(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	sceUtility_driver_F062AEA6();
	return 0;
}

int sceUtility_driver_20C68C34(void)
{
	utilityAppletTerm();
	sceKernelUnregisterSysEventHandler(&g_sys_ev_handler);
	return 0;
}

static UTILITY_NOINLINE int utilityDialogUpdate(u32 dialog_type, int speed)
{
	g_dialog.speed = speed;
	if (g_dialog.active_type != dialog_type)
		return SCE_UTILITY_ERROR_INVALID_DIALOG;
	if (g_dialog.status != 2 && g_dialog.status != 5)
		return SCE_UTILITY_ERROR_INVALID_STATUS;

	sceKernelSetEventFlag(g_dialog.worktime_evf, 0x10);
	sceKernelWaitEventFlag(g_dialog.worktime_evf, 0x20, 0x20, NULL, NULL);
	return 0;
}

static UTILITY_NOINLINE int utilityDialogGetStatus(u32 dialog_type)
{
	if (g_dialog.active_type != dialog_type)
		return SCE_UTILITY_ERROR_INVALID_DIALOG;
	utilityCompatDelay(dialog_type, 0);
	return (int)g_dialog.status;
}

static UTILITY_NOINLINE int utilityDialogAbort(u32 dialog_type)
{
	if (g_dialog.active_type != dialog_type)
		return SCE_UTILITY_ERROR_INVALID_DIALOG;
	if (g_dialog.status != 2)
		return SCE_UTILITY_ERROR_INVALID_STATUS;
	g_dialog.abort_flag = 1;
	g_dialog.in_suspend = 1;
	return 0;
}

int sceUtility_private_14E94D25(void)
{
	u32 old_k1 = utilityEnterKernel();
	sceKernelWaitEventFlag(g_dialog.worktime_evf, 1, 0x20, NULL, NULL);
	utilityLeaveKernel(old_k1);
	return 0;
}

int sceUtility_private_0D6B23A4(void)
{
	u32 old_k1 = utilityEnterKernel();
	sceKernelSetEventFlag(g_dialog.worktime_evf, 2);
	utilityLeaveKernel(old_k1);
	return 0;
}

int sceUtility_private_0027D953(void)
{
	u32 old_k1 = utilityEnterKernel();
	sceKernelWaitEventFlag(g_dialog.worktime_evf, 4, 0x20, NULL, NULL);
	utilityLeaveKernel(old_k1);
	return 0;
}

int sceUtility_private_C1916D25(void)
{
	u32 old_k1 = utilityEnterKernel();
	sceKernelSetEventFlag(g_dialog.worktime_evf, 8);
	utilityLeaveKernel(old_k1);
	return 0;
}

int sceUtility_private_AC7FD75C(void)
{
	u32 old_k1 = utilityEnterKernel();
	sceKernelWaitEventFlag(g_dialog.worktime_evf, 0x10, 0x20, NULL, NULL);
	utilityLeaveKernel(old_k1);
	return 0;
}

int sceUtility_private_8D681EE0(void)
{
	u32 old_k1 = utilityEnterKernel();
	sceKernelSetEventFlag(g_dialog.worktime_evf, 0x20);
	utilityLeaveKernel(old_k1);
	return 0;
}

int sceUtility_private_39E7692B(u32 status)
{
	g_dialog.status = status;
	return 0;
}

int sceUtility_private_EA446CD5(void)
{
	return (int)g_dialog.status;
}

int sceUtility_private_A25E0194(void)
{
	return (int)g_dialog.active_type;
}

void *sceUtility_private_DF025E50(void)
{
	return g_dialog.params;
}

u32 sceUtility_private_5809B44A(void)
{
	return g_dialog.api_version;
}

int sceUtility_private_1E82FB49(void)
{
	return g_dialog.speed;
}

int sceUtility_private_5DCBD3C0(void)
{
	return (int)g_dialog.in_suspend;
}

u32 sceUtility_private_10D1B3FF(void)
{
	return g_dialog.abort_flag;
}

int sceUtility_private_A1348D82(SceUID thid)
{
	g_dialog.dialog_thid = thid;
	return 0;
}

int sceUtility_private_421E2726(void)
{
	u32 old_k1 = utilityEnterKernel();
	SceKernelGameInfo *game_info = (SceKernelGameInfo *)sceKernelGetGameInfo();
	int ret = (int)game_info->game_type;
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtility_private_A6B323E1(char *out_buf)
{
	u32 old_k1 = utilityEnterKernel();

	if (utilityIsBadRange(out_buf, 8)) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_ERROR_INVALID_ADDR;
	}

	{
		SceKernelGameInfo *game_info = (SceKernelGameInfo *)sceKernelGetGameInfo();
		strncpy(out_buf, game_info->title_code, 7);
		out_buf[7] = '\0';
	}

	utilityLeaveKernel(old_k1);
	return 0;
}

int sceUtility_private_031D0944(int lock_type)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = sceKernelPowerLockForUser(lock_type);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtility_private_B9D9C78F(int lock_type)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = sceKernelPowerUnlockForUser(lock_type);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtility_private_31516141(int sampling_mode)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = sceCtrlSetSamplingMode(sampling_mode);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtility_private_966912D2(int *out_mode)
{
	u32 old_k1 = utilityEnterKernel();
	int ret;

	if (utilityIsBadRange(out_mode, 4)) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_ERROR_INVALID_ADDR;
	}

	ret = sceCtrlGetSamplingMode(out_mode);
	utilityLeaveKernel(old_k1);
	return ret;
}

static inline __attribute__((always_inline)) SceUID utilityLoadStartDialogModule(const char *path, int mpid, SceSize argsize, void *argp)
{
	SceKernelLMOption lm_opt;
	SceKernelSMOption sm_opt;
	SceUID modid;
	int status;
	int ret;
	SceUtilityDialogCommon *common = (SceUtilityDialogCommon *)g_dialog.params;

	lm_opt.size = sizeof(SceKernelLMOption);
	lm_opt.mpid_text = mpid;
	lm_opt.mpid_data = mpid;
	lm_opt.position = 0;
	lm_opt.access = 1;

	modid = sceKernelLoadModuleForKernel(path, 0, &lm_opt);
	if (modid < 0)
		return modid;

	sm_opt.size = sizeof(SceKernelSMOption);
	sm_opt.mpid_stack = mpid;
	sm_opt.stack_size = 4096;
	sm_opt.priority = common->access_thread;
	sm_opt.attribute = 0;

	ret = sceKernelStartModule(modid, argsize, argp, &status, &sm_opt);
	if (ret < 0)
		return ret;
	return modid;
}

static UTILITY_NOINLINE int utilityDialogLoadThread(SceSize args, void *argp)
{
	SceUtilityDialogStartArg start_arg;
	(void)args;
	(void)argp;

	sceKernelWaitEventFlag(g_dialog.suspend_evf, 3, 0x20, NULL, NULL);
	sceKernelSetEventFlag(g_dialog.suspend_evf, 1);
	sceKernelVolatileMemLock(0, &g_dialog.vmem_addr, &g_dialog.vmem_size);
	utilityKermitSetDialogActive(1);

	if ((g_dialog.active_type == 1 && sceKernelApplicationType() != 0x300) ||
	    g_dialog.active_type == 9 || g_dialog.active_type == 17) {
		g_dialog.chnnlsv_modid = utilityLoadStartDialogModule("flash0:/kd/chnnlsv.prx", 1, 0, NULL);
	}

	if (g_dialog.active_type == 9 || g_dialog.active_type == 17) {
		g_dialog.psheet_modid = utilityLoadStartDialogModule("flash0:/kd/psheet.prx", 1, 0, NULL);
	}

	start_arg.size = sizeof(SceUtilityDialogStartArg);
	start_arg.model = sceKernelGetModel();
	start_arg.boot_from = sceKernelBootFrom();
	start_arg.params = g_dialog.params;
	start_arg.api_version = g_dialog.api_version;
	start_arg.dialog_type = g_dialog.active_type;
	start_arg.dialog_submode = g_dialog.dialog_submode;

	if (g_dialog.active_type == 1 && g_dialog.dialog_submode == 1) {
		g_dialog.main_modid = utilityLoadStartDialogModule(
			"flash0:/vsh/module/savedata_auto_dialog.prx", 5,
			sizeof(SceUtilityDialogStartArg), &start_arg);
	} else {
		if (sceKernelSearchModuleByName("scePaf_Module") < 0) {
			do {
				g_dialog.paf_modid = utilityLoadStartDialogModule("flash0:/vsh/module/pafmini.prx", 5, 0, NULL);
			} while (g_dialog.paf_modid <= 0);
		} else {
			g_dialog.paf_modid = 0;
		}
		g_dialog.common_gui_modid = utilityLoadStartDialogModule("flash0:/vsh/module/common_gui.prx", 5, 0, NULL);
		g_dialog.common_util_modid = utilityLoadStartDialogModule("flash0:/vsh/module/common_util.prx", 5, 0, NULL);
		g_dialog.main_modid = utilityLoadStartDialogModule(
			"flash0:/vsh/module/dialogmain.prx", 5,
			sizeof(SceUtilityDialogStartArg), &start_arg);
	}

	sceKernelSetEventFlag(g_dialog.worktime_evf, 1);
	sceKernelWaitEventFlag(g_dialog.worktime_evf, 2, 0x20, NULL, NULL);
	sceKernelExitDeleteThread(0);
	return 0;
}

static UTILITY_NOINLINE int utilityAppletLoadThread(SceSize args, void *argp)
{
	g_applet.main_modid = utilityAppletLoadStartModule(g_applet.mod_path, 2, args, argp, &g_applet.block_id);
	sceKernelSetEventFlag(g_applet.worktime_evf, 1);
	sceKernelWaitEventFlag(g_applet.worktime_evf, 2, 0x20, NULL, NULL);
	sceKernelExitDeleteThread(0);
	return 0;
}

static UTILITY_NOINLINE int utilityDialogUnloadThread(SceSize args, void *argp)
{
	int intr;
	(void)args;
	(void)argp;

	sceKernelSetEventFlag(g_dialog.worktime_evf, 4);
	sceKernelWaitEventFlag(g_dialog.worktime_evf, 8, 0x20, NULL, NULL);
	sceKernelWaitThreadEnd(g_dialog.dialog_thid, NULL);

	if (g_dialog.active_type == 1 && g_dialog.dialog_submode == 1) {
		utilityDialogStopUnloadModule(g_dialog.main_modid, 5);
		g_dialog.main_modid = 0;
	} else {
		utilityDialogStopUnloadModule(g_dialog.main_modid, 5);
		g_dialog.main_modid = 0;
		utilityDialogStopUnloadModule(g_dialog.common_util_modid, 5);
		g_dialog.common_util_modid = 0;
		utilityDialogStopUnloadModule(g_dialog.common_gui_modid, 5);
		g_dialog.common_gui_modid = 0;
		if (g_dialog.paf_modid != 0) {
			utilityDialogStopUnloadModule(g_dialog.paf_modid, 5);
			g_dialog.paf_modid = 0;
		}
	}

	if ((g_dialog.active_type == 1 && sceKernelApplicationType() != 0x300) ||
	    g_dialog.active_type == 9 || g_dialog.active_type == 17) {
		utilityDialogStopUnloadModule(g_dialog.chnnlsv_modid, 1);
		g_dialog.chnnlsv_modid = 0;
	}

	if (g_dialog.active_type == 9 || g_dialog.active_type == 17) {
		utilityDialogStopUnloadModule(g_dialog.psheet_modid, 1);
		g_dialog.psheet_modid = 0;
	}

	if ((utilityCompatGetFlags() & 2) != 0) {
		sceKernelMemset32(g_dialog.vmem_addr, 0, (SceSize)g_dialog.vmem_size);
	} else if (g_dialog.active_type == 1 && g_dialog.dialog_submode == 1 &&
	           sceKernelGetCompiledSdkVersion() <= 0x0307FFFF) {
		sceKernelMemset32(g_dialog.vmem_addr, 0, 0x40000);
	}

	if (utilityIsDialogMemGuardEnabled() != 0) {
		u32 seed = sceKernelGetSystemTimeLow() + (u32)sceKernelGetThreadId();
		sceKernelMemset32(g_dialog.vmem_addr, seed, (SceSize)g_dialog.vmem_size);
	}

	utilityCompatClear();

	intr = sceKernelCpuSuspendIntr();
	g_dialog.dialog_submode = 0;
	g_dialog.params = NULL;
	g_dialog.api_version = 0;
	g_dialog.status = 0;
	g_dialog.in_suspend = 0;
	g_dialog.abort_flag = 0;
	sceKernelCpuResumeIntr(intr);

	utilityKermitSetDialogActive(0);
	sceKernelVolatileMemUnlock(0);
	sceKernelSetEventFlag(g_dialog.suspend_evf, 2);
	sceKernelExitDeleteThread(0);
	return 0;
}

static UTILITY_NOINLINE int utilityAppletUnloadThread(SceSize args, void *argp)
{
	int intr;
	(void)args;
	(void)argp;

	sceKernelSetEventFlag(g_applet.worktime_evf, 4);
	sceKernelWaitEventFlag(g_applet.worktime_evf, 8, 0x20, NULL, NULL);
	sceKernelWaitThreadEnd(g_applet.applet_thid, NULL);

	utilityAppletStopUnloadModule(g_applet.main_modid, 0);
	if (g_applet.block_id > 0) {
		sceKernelFreePartitionMemory(g_applet.block_id);
		g_applet.block_id = 0;
	}

	intr = sceKernelCpuSuspendIntr();
	g_applet.status = 0;
	g_applet.params = NULL;
	g_applet.api_version = 0;
	sceKernelCpuResumeIntr(intr);

	sceKernelExitDeleteThread(0);
	return 0;
}

static UTILITY_NOINLINE int utilityDialogStopUnloadModule(SceUID modid, int mpid)
{
	SceKernelSMOption sm_opt;
	SceUtilityDialogCommon *common = (SceUtilityDialogCommon *)g_dialog.params;
	int status;
	int ret;

	sm_opt.size = sizeof(SceKernelSMOption);
	sm_opt.mpid_stack = mpid;
	sm_opt.stack_size = 4096;
	sm_opt.priority = common->access_thread;
	sm_opt.attribute = 0;

	ret = sceKernelStopModule(modid, 0, NULL, &status, &sm_opt);
	if (ret >= 0) {
		ret = sceKernelUnloadModule(modid);
		if (ret > 0)
			ret = 0;
	}
	return ret;
}

static UTILITY_NOINLINE int utilityAppletStopUnloadModule(SceUID modid, int mpid)
{
	SceKernelSMOption sm_opt;
	SceUtilityDialogCommon *common = (SceUtilityDialogCommon *)g_applet.params;
	int status;
	int ret;

	sm_opt.size = sizeof(SceKernelSMOption);
	sm_opt.mpid_stack = mpid;
	sm_opt.stack_size = 4096;
	sm_opt.priority = common->access_thread;
	sm_opt.attribute = 0;

	ret = sceKernelStopModule(modid, 0, NULL, &status, &sm_opt);
	if (ret >= 0) {
		ret = utilityAppletUnloadModule(modid);
		if (ret > 0)
			ret = 0;
	}
	return ret;
}

static UTILITY_NOINLINE int utilityIsDialogMemGuardEnabled(void)
{
	if (!sceKernelIsToolMode())
		return 0;
	if (sceKernelIsDevelopmentToolMode()) {
		if (sceKernelDipsw(25) != 0)
			return 0;
	} else {
		if (sceKernelDipsw(24) == 0)
			return 0;
	}
	return sceKernelGetCompiledSdkVersion() > 0x0307FFFF;
}

/* Applet core (HtmlViewer) */
static UTILITY_NOINLINE int utilityAppletInitStart(u32 applet_type, void *params)
{
	SceUtilityHtmlViewerHeader *hv = (SceUtilityHtmlViewerHeader *)params;
	u32 sz;
	u32 api_version;
	SceSize part_size;
	SceUID block;
	SceUtilityAppletStartArg start_arg;
	SceKernelGameInfo *game_info;
	int ret;

	if (g_applet.status != 0)
		return SCE_UTILITY_ERROR_INVALID_STATUS;
	if (utilityIsBadRange(params, sizeof(SceUtilityDialogCommon)))
		return SCE_UTILITY_ERROR_INVALID_ADDR;
	if (applet_type != 1)
		return SCE_UTILITY_ERROR_INVALID_TYPE;

	sz = hv->base.size;
	g_applet.mod_path = "flash0:/vsh/module/htmlviewer_utility.prx";
	switch (sz) {
	case 112: api_version = 200; break;
	case 120: api_version = 250; break;
	case 128: api_version = 260; break;
	case 152: api_version = 270; break;
	case 164: api_version = 280; break;
	case 168: api_version = 300; break;
	default:
		return SCE_UTILITY_ERROR_INVALID_SIZE;
	}

	if (utilityIsBadSpan(params, sz))
		return SCE_UTILITY_ERROR_INVALID_ADDR;
	if (utilityIsBadSpan(hv->mem_addr, hv->mem_size))
		return SCE_UTILITY_ERROR_INVALID_ADDR;

	part_size = utilityAppletGetPartitionSize(1, api_version, hv);
	block = sceKernelAllocPartitionMemory(2, "SceUtilityAppletPartition", 0, part_size, NULL);
	if (block <= 0)
		return block;

	g_applet.status = 1;
	g_applet.active_type = 1;
	g_applet.api_version = api_version;
	g_applet.block_id = block;
	g_applet.params = params;

	start_arg.size = sizeof(SceUtilityAppletStartArg);
	start_arg.model = sceKernelGetModel();
	start_arg.boot_from = sceKernelBootFrom();
	start_arg.applet_type = 1;
	start_arg.api_version = api_version;
	start_arg.params = params;
	game_info = (SceKernelGameInfo *)sceKernelGetGameInfo();
	strncpy(start_arg.product_code, game_info->product_code, 13);
	start_arg.product_code[13] = '\0';

	g_applet.init_thid = sceKernelCreateThread(
		"SceUtilityAppletInit", utilityAppletLoadThread,
		hv->base.access_thread, 2048, 0x00100000, NULL);
	if (g_applet.init_thid <= 0) {
		ret = g_applet.init_thid;
		g_applet.init_thid = 0;
		return ret;
	}

	ret = sceKernelStartThread(g_applet.init_thid, sizeof(SceUtilityAppletStartArg), &start_arg);
	if (ret != 0) {
		sceKernelDeleteThread(g_applet.init_thid);
		g_applet.init_thid = 0;
		return ret;
	}
	return 0;
}

static UTILITY_NOINLINE int utilityAppletShutdownStart(u32 applet_type)
{
	SceUtilityDialogCommon *common;
	int ret;

	if (g_applet.active_type != applet_type)
		return SCE_UTILITY_ERROR_INVALID_DIALOG;
	if (g_applet.status != 3)
		return SCE_UTILITY_ERROR_INVALID_STATUS;

	g_applet.status = 4;
	common = (SceUtilityDialogCommon *)g_applet.params;
	g_applet.init_thid = sceKernelCreateThread(
		"SceUtilityAppletShutdown", utilityAppletUnloadThread,
		common->access_thread, 2048, 0x00100000, NULL);
	if (g_applet.init_thid <= 0) {
		ret = g_applet.init_thid;
		g_applet.init_thid = 0;
		return ret;
	}

	ret = sceKernelStartThread(g_applet.init_thid, 0, NULL);
	if (ret != 0) {
		sceKernelDeleteThread(g_applet.init_thid);
		g_applet.init_thid = 0;
		return ret;
	}
	return 0;
}

int sceUtility_private_048BFC46(const char *name, int flags, void *opt)
{
	u32 old_k1 = utilityEnterKernel();
	const char *path = NULL;
	int ret;

	if (utilityIsBadPtr(name) || utilityIsBadRange(opt, sizeof(SceKernelLMOption))) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_ERROR_INVALID_ADDR;
	}

	switch (name[0]) {
	case 'h':
		if (strcmp(name, "htmlviewer_ui") == 0)
			path = "flash0:/vsh/module/htmlviewer_ui.prx";
		else if (strcmp(name, "hvauth_r") == 0)
			path = "flash0:/vsh/module/hvauth_r.prx";
		else if (strcmp(name, "hvauth_t") == 0)
			path = "flash0:/vsh/module/hvauth_t.prx";
		break;
	case 'l':
		if (strcmp(name, "libslim") == 0)
			path = "flash0:/vsh/module/libslim.prx";
		else if (strcmp(name, "libwww") == 0)
			path = "flash0:/vsh/module/libwww.prx";
		else if (strcmp(name, "libfont_hv") == 0)
			path = "flash0:/vsh/module/libfont_hv.prx";
		break;
	case 'm':
		if (strcmp(name, "mgvideo") == 0)
			path = "flash0:/kd/mgvideo.prx";
		else if (strcmp(name, "mm_flash") == 0)
			path = "flash0:/vsh/module/mm_flash.prx";
		break;
	case 'n':
		if (strcmp(name, "netfront") == 0)
			path = "flash0:/vsh/module/netfront.prx";
		break;
	default:
		break;
	}

	if (path == NULL)
		ret = SCE_KERNEL_ERROR_NO_FILE;
	else
		ret = utilityAppletLoadModule(path, &g_applet.block_id, flags, opt);

	utilityLeaveKernel(old_k1);
	return ret;
}

static UTILITY_NOINLINE int utilityAppletInit(void)
{
	g_applet.worktime_evf = sceKernelCreateEventFlag("SceUtilityAppletWorktime", 0x200, 0, NULL);
	if (g_applet.worktime_evf < 0)
		return SCE_KERNEL_ERROR_NO_MEMORY;

	g_applet.api_version = 0;
	g_applet.active_type = 0;
	g_applet.status = 0;
	g_applet.params = NULL;
	return 0;
}

static UTILITY_NOINLINE int utilityAppletTerm(void)
{
	if (g_applet.worktime_evf > 0)
		sceKernelDeleteEventFlag(g_applet.worktime_evf);
	return 0;
}

static UTILITY_NOINLINE int utilityAppletUpdate(u32 applet_type, int speed)
{
	g_applet.speed = speed;
	if (g_applet.active_type != applet_type)
		return SCE_UTILITY_ERROR_INVALID_DIALOG;
	if (g_applet.status != 2)
		return SCE_UTILITY_ERROR_INVALID_STATUS;

	sceKernelSetEventFlag(g_applet.worktime_evf, 0x10);
	sceKernelWaitEventFlag(g_applet.worktime_evf, 0x20, 0x20, NULL, NULL);
	return 0;
}

static UTILITY_NOINLINE int utilityAppletGetStatus(u32 applet_type)
{
	if (g_applet.active_type != applet_type)
		return SCE_UTILITY_ERROR_INVALID_DIALOG;
	return (int)g_applet.status;
}

int sceUtility_private_186A2C84(void)
{
	u32 old_k1 = utilityEnterKernel();
	sceKernelWaitEventFlag(g_applet.worktime_evf, 1, 0x20, NULL, NULL);
	utilityLeaveKernel(old_k1);
	return 0;
}

int sceUtility_private_29AFE096(void)
{
	u32 old_k1 = utilityEnterKernel();
	sceKernelSetEventFlag(g_applet.worktime_evf, 2);
	utilityLeaveKernel(old_k1);
	return 0;
}

int sceUtility_private_E0F5DEC1(void)
{
	u32 old_k1 = utilityEnterKernel();
	sceKernelWaitEventFlag(g_applet.worktime_evf, 4, 0x20, NULL, NULL);
	utilityLeaveKernel(old_k1);
	return 0;
}

int sceUtility_private_8024FCA7(void)
{
	u32 old_k1 = utilityEnterKernel();
	sceKernelSetEventFlag(g_applet.worktime_evf, 8);
	utilityLeaveKernel(old_k1);
	return 0;
}

int sceUtility_private_7E0D4BCD(void)
{
	u32 old_k1 = utilityEnterKernel();
	sceKernelWaitEventFlag(g_applet.worktime_evf, 0x10, 0x20, NULL, NULL);
	utilityLeaveKernel(old_k1);
	return 0;
}

int sceUtility_private_CDEEBEE2(void)
{
	u32 old_k1 = utilityEnterKernel();
	sceKernelSetEventFlag(g_applet.worktime_evf, 0x20);
	utilityLeaveKernel(old_k1);
	return 0;
}

int sceUtility_private_86AA70BC(u32 status)
{
	g_applet.status = status;
	return 0;
}

u32 sceUtility_private_FC841AC3(void)
{
	return g_applet.active_type;
}

void *sceUtility_private_369D25E4(void)
{
	return g_applet.params;
}

u32 sceUtility_private_ED4E37CA(void)
{
	return g_applet.api_version;
}

int sceUtility_private_BAFFB9D5(void)
{
	return g_applet.speed;
}

int sceUtility_private_69AB28E8(SceUID thid)
{
	g_applet.applet_thid = thid;
	return 0;
}

int sceUtility_private_78A2FE0C(SceUID modid)
{
	return utilityAppletUnloadModule(modid);
}

int sceUtility_private_4F6FFC7C(int power_arg)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = scePower_driver_766CD857(power_arg);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtility_private_34340686(int power_arg)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = scePower_driver_315B8CB6(power_arg);
	utilityLeaveKernel(old_k1);
	return ret;
}

static UTILITY_NOINLINE SceSize utilityAppletGetPartitionSize(u32 applet_type, u32 api_version, const SceUtilityHtmlViewerHeader *params)
{
	if (applet_type == 1) {
		if (api_version >= 270 && (params->init_flags & 0x400) != 0)
			return 0x480000;
		return 0x380000;
	}
	return 0;
}

static UTILITY_NOINLINE SceUID utilityAppletLoadModule(const char *path, SceUID *block_p, int flags, void *opt)
{
	SceUID rem_block;
	SceUID modid;
	int idx;

	if (*block_p != 0)
		modid = sceKernelLoadModuleToBlock(path, *block_p, &rem_block, flags, opt);
	else
		modid = sceKernelLoadModuleForKernel(path, flags, opt);

	if (modid > 0) {
		for (idx = 0; idx < 8; idx++) {
			if (g_applet_mods[idx].modid == 0) {
				g_applet_mods[idx].block_id = *block_p;
				g_applet_mods[idx].modid = modid;
				break;
			}
		}
		if (*block_p != 0)
			*block_p = rem_block;
	}
	return modid;
}

static UTILITY_NOINLINE int utilityAppletUnloadModule(SceUID modid)
{
	int ret = sceKernelUnloadModule(modid);
	int idx;

	for (idx = 0; idx < 8; idx++) {
		if (g_applet_mods[idx].modid == modid) {
			if (g_applet_mods[idx].block_id != 0)
				sceKernelFreePartitionMemory(g_applet_mods[idx].block_id);
			g_applet_mods[idx].modid = 0;
			break;
		}
	}
	return ret;
}

static UTILITY_NOINLINE SceUID utilityAppletLoadStartModule(const char *path, int mpid, SceSize argsize, void *argp, SceUID *block_p)
{
	SceKernelLMOption lm_opt;
	SceKernelSMOption sm_opt;
	SceUtilityDialogCommon *common = (SceUtilityDialogCommon *)g_applet.params;
	SceUID modid;
	int status;
	int ret;

	lm_opt.size = sizeof(SceKernelLMOption);
	lm_opt.mpid_text = mpid;
	lm_opt.mpid_data = mpid;
	lm_opt.position = 0;
	lm_opt.access = 1;

	modid = utilityAppletLoadModule(path, block_p, 0, &lm_opt);
	if (modid < 0)
		return modid;

	sm_opt.size = sizeof(SceKernelSMOption);
	sm_opt.mpid_stack = mpid;
	sm_opt.stack_size = 4096;
	sm_opt.priority = common->access_thread;
	sm_opt.attribute = 0;

	ret = sceKernelStartModule(modid, argsize, argp, &status, &sm_opt);
	if (ret < 0)
		return ret;
	return modid;
}

/* Dialog module memory management */
int sceUtility_private_0C0AE566(SceSize size)
{
	u32 old_k1 = utilityEnterKernel();
	int idx;
	SceUID block;

	if (g_dialog_mem_inited != 0) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_ERROR_INVALID_STATUS;
	}

	for (idx = 0; idx < 3; idx++) {
		g_dialog_mem_nodes[idx].next = NULL;
		g_dialog_mem_nodes[idx].prev = NULL;
		g_dialog_mem_nodes[idx].block_id = -1;
		g_dialog_mem_nodes[idx].modid = -1;
	}
	g_dialog_mem_head.next = &g_dialog_mem_head;
	g_dialog_mem_head.prev = &g_dialog_mem_head;

	block = sceKernelAllocPartitionMemory(2, "SceUtilityDialogModule", 0, size, NULL);
	if (block < 0) {
		utilityLeaveKernel(old_k1);
		return block;
	}

	g_dialog_mem_nodes[0].block_id = block;
	g_dialog_mem_nodes[0].next = g_dialog_mem_head.next;
	g_dialog_mem_nodes[0].prev = &g_dialog_mem_head;
	g_dialog_mem_head.next->prev = &g_dialog_mem_nodes[0];
	g_dialog_mem_head.next = &g_dialog_mem_nodes[0];

	if (utilityIsDialogModuleGuardEnabled() != 0) {
		g_dialog_mem_guard_seed = sceKernelGetSystemTimeLow();
		utilityFillDialogModuleGuard(g_dialog_mem_nodes[0].block_id);
	}

	g_dialog_mem_inited = 1;
	utilityLeaveKernel(old_k1);
	return 0;
}

int sceUtility_private_B34EC4AD(void)
{
	u32 old_k1 = utilityEnterKernel();
	SceUtilityDialogMemNode *node;

	if (g_dialog_mem_inited != 1) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_ERROR_INVALID_STATUS;
	}

	while (g_dialog_mem_head.prev != &g_dialog_mem_head) {
		node = g_dialog_mem_head.prev;
		if (node->modid < 0 && node->block_id >= 0) {
			if (utilityIsDialogModuleGuardEnabled() != 0)
				utilityCheckDialogModuleGuard(node->block_id);
		}
		if (node->modid >= 0) {
			sceKernelUnloadModule(node->modid);
			node->modid = -1;
		}
		if (node->block_id >= 0) {
			sceKernelFreePartitionMemory(node->block_id);
			node->block_id = -1;
		}
		node->next->prev = node->prev;
		node->prev->next = node->next;
		node->prev = NULL;
		node->next = NULL;
	}

	g_dialog_mem_inited = 0;
	utilityLeaveKernel(old_k1);
	return 0;
}

SceUID sceUtility_private_ACB67A01(const char *name, int flags, const SceKernelLMOption *opt)
{
	u32 old_k1 = utilityEnterKernel();
	const char *path = NULL;
	SceUtilityDialogMemNode *free_node = NULL;
	SceUtilityDialogMemNode *head_node;
	SceKernelLMOption load_opt;
	SceUID rem_block;
	SceUID modid;
	int idx;

	if (g_dialog_mem_inited != 1) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_ERROR_INVALID_STATUS;
	}

	if (utilityIsBadPtr(name) || utilityIsBadRange(opt, sizeof(SceKernelLMOption))) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_ERROR_INVALID_ADDR;
	}

	if (name[0] == 'n') {
		if (strcmp(name, "npsignup_plugin") == 0)
			path = "flash0:/vsh/module/npsignup_plugin.prx";
	} else if (name[0] == 's') {
		if (strcmp(name, "store_browser_plugin") == 0)
			path = "flash0:/vsh/module/store_browser_plugin.prx";
		if (strcmp(name, "store_checkout_plugin") == 0)
			path = "flash0:/vsh/module/store_checkout_plugin.prx";
	}

	if (path == NULL) {
		utilityLeaveKernel(old_k1);
		return sceUtility_private_5ED16EBA(name, flags, opt);
	}

	for (idx = 0; idx < 3; idx++) {
		if (g_dialog_mem_nodes[idx].block_id < 0 && g_dialog_mem_nodes[idx].modid < 0) {
			free_node = &g_dialog_mem_nodes[idx];
			break;
		}
	}

	if (free_node == NULL || g_dialog_mem_head.next == &g_dialog_mem_head) {
		utilityLeaveKernel(old_k1);
		return SCE_KERNEL_ERROR_NO_FREE_SLOT;
	}

	head_node = g_dialog_mem_head.next;
	if (utilityIsDialogModuleGuardEnabled() != 0)
		utilityCheckDialogModuleGuard(head_node->block_id);

	load_opt = *opt;
	load_opt.mpid_text = 2;
	load_opt.mpid_data = 2;

	modid = sceKernelLoadModuleToBlock(path, head_node->block_id, &rem_block, flags, &load_opt);
	if (modid < 0) {
		utilityLeaveKernel(old_k1);
		return modid;
	}

	head_node->modid = modid;
	if (utilityIsDialogModuleGuardEnabled() != 0)
		utilityFillDialogModuleGuard(rem_block);

	free_node->block_id = rem_block;
	free_node->next = g_dialog_mem_head.next;
	free_node->prev = &g_dialog_mem_head;
	g_dialog_mem_head.next->prev = free_node;
	g_dialog_mem_head.next = free_node;

	utilityLeaveKernel(old_k1);
	return modid;
}

int sceUtility_private_BBCD1885(SceUID modid)
{
	u32 old_k1 = utilityEnterKernel();
	SceUtilityDialogMemNode *node;
	int ret;

	if (g_dialog_mem_inited != 1) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_ERROR_INVALID_STATUS;
	}

	ret = sceKernelUnloadModule(modid);
	if (ret < 0) {
		utilityLeaveKernel(old_k1);
		return ret;
	}

	for (node = g_dialog_mem_head.prev; node != &g_dialog_mem_head; node = node->prev) {
		if (node->modid == modid) {
			node->modid = -1;
			if (utilityIsDialogModuleGuardEnabled() != 0)
				utilityFillDialogModuleGuard(node->block_id);

			for (node = g_dialog_mem_head.prev; node != &g_dialog_mem_head; ) {
				if (node->block_id >= 0 && node->modid < 0) {
					SceUtilityDialogMemNode *cur = node->prev;
					while (cur != &g_dialog_mem_head && cur->modid < 0) {
						SceUtilityDialogMemNode *prev_link = cur->prev;
						SceUID merged = sceKernelJointMemoryBlock(node->block_id, cur->block_id);
						if (merged >= 0) {
							node->block_id = merged;
							cur->block_id = -1;
							cur->next->prev = cur->prev;
							cur->prev->next = cur->next;
							cur->prev = NULL;
							cur->next = NULL;
						}
						cur = prev_link;
					}
					if (cur == &g_dialog_mem_head)
						break;
					node = cur->prev;
				} else {
					node = node->prev;
				}
			}
			break;
		}
	}

	utilityLeaveKernel(old_k1);
	return ret;
}

static UTILITY_NOINLINE int utilityIsDialogModuleGuardEnabled(void)
{
	int enabled;

	if (!sceKernelIsToolMode())
		return 0;
	if (sceKernelIsDevelopmentToolMode())
		enabled = (sceKernelDipsw(25) == 0);
	else
		enabled = sceKernelDipsw(24);
	return enabled != 0;
}

static UTILITY_NOINLINE int utilityFillDialogModuleGuard(SceUID block_id)
{
	SceKernelSysMemInfo mem_info;
	int ret;

	mem_info.size = sizeof(SceKernelSysMemInfo);
	ret = sceKernelQueryMemoryBlockInfo(block_id, &mem_info);
	if (ret >= 0)
		return sceKernelMemset32(mem_info.addr, g_dialog_mem_guard_seed, mem_info.mem_size);
	return ret;
}

static UTILITY_NOINLINE int utilityCheckDialogModuleGuard(SceUID block_id)
{
	SceKernelSysMemInfo mem_info;
	int ret;

	mem_info.size = sizeof(SceKernelSysMemInfo);
	ret = sceKernelQueryMemoryBlockInfo(block_id, &mem_info);
	if (ret >= 0) {
		u32 *words = (u32 *)mem_info.addr;
		u32 count = mem_info.mem_size >> 2;
		u32 idx;

		for (idx = 0; idx < count; idx++) {
			if (words[idx] != g_dialog_mem_guard_seed) {
				Kprintf("SceUtilityDialogModule reserved memory has been violated 0x%p ... stop\n", &words[idx]);
				__asm__ volatile("break 0");
			}
		}
	}
	return ret;
}

/* Unified module loader */
static UTILITY_NOINLINE int utilityLoadNetModuleGroup(u32 mod_id)
{
	u32 old_k1 = utilityEnterKernel();
	SceUtilityModuleGroup *group = utilityFindNetModuleGroup(mod_id);
	int ret;

	if (mod_id == 0x101 || mod_id == 2) {
		SceKernelGameInfo *game_info = (SceKernelGameInfo *)sceKernelGetGameInfo();
		const char *code = game_info->product_code;

		if (strcmp(code, "ULJS00048") == 0 ||
		    strcmp(code, "ULUS10139") == 0 ||
		    strcmp(code, "UCES00356") == 0 ||
		    strcmp(code, "UCKS45027") == 0 ||
		    strcmp(code, "UCAS40099") == 0) {
			group->user_mods[4].opt = &g_net_adhoc_compat_opt;
			group->user_mods[3].opt = &g_net_adhoc_compat_opt;
		}
	}

	if (group == NULL)
		ret = SCE_UTILITY_MOD_ERROR_INVALID_ID;
	else
		ret = utilityLoadModuleGroup(group);

	utilityLeaveKernel(old_k1);
	return ret;
}

static UTILITY_NOINLINE int utilityInitNetModules(void)
{
	u32 idx;
	for (idx = 0; idx < 9; idx++)
		utilityInitModuleGroup(&g_net_groups[idx]);
	return 0;
}

static UTILITY_NOINLINE int utilityUnloadNetModuleGroup(u32 mod_id)
{
	u32 old_k1 = utilityEnterKernel();
	SceUtilityModuleGroup *group = utilityFindNetModuleGroup(mod_id);
	int ret;

	if (group == NULL)
		ret = SCE_UTILITY_MOD_ERROR_INVALID_ID;
	else
		ret = utilityUnloadModuleGroup(group);

	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilityLoadNetModule(u32 mod_id)
{
	int ret;

	if (mod_id - 1 >= 7)
		return SCE_UTILITY_NET_ERROR_INVALID_ID;
	ret = utilityLoadNetModuleGroup(mod_id);
	if (ret < 0)
		return (int)(((u32)ret & 0xFFFF00FFu) | 0x0800u);
	return ret;
}

int sceUtilityUnloadNetModule(u32 mod_id)
{
	int ret;

	if (mod_id - 1 >= 7)
		return SCE_UTILITY_NET_ERROR_INVALID_ID;
	ret = utilityUnloadNetModuleGroup(mod_id);
	if (ret < 0)
		return (int)(((u32)ret & 0xFFFF00FFu) | 0x0800u);
	return ret;
}

static UTILITY_NOINLINE int utilityInitDnasModules(void)
{
	utilityInitModuleGroup(&g_dnas_group);
	return 0;
}

int sceUtility_private_C7D5983F(void)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityLoadModuleGroup(&g_dnas_group);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtility_private_5C494AC6(void)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityUnloadModuleGroup(&g_dnas_group);
	utilityLeaveKernel(old_k1);
	return ret;
}

static UTILITY_NOINLINE SceUtilityModuleGroup *utilityFindNetModuleGroup(u32 mod_id)
{
	u32 idx = mod_id & 0xFFFF00FFu;
	if ((mod_id & 0x100u) == 0)
		idx = mod_id - 1u;
	if (idx >= 9)
		return NULL;
	return &g_net_groups[idx];
}

static UTILITY_NOINLINE SceUtilityModuleGroup *utilityFindUsbModuleGroup(u32 mod_id)
{
	if (mod_id == 0x203 && utilityIsUsbGpsSerialDriver() != 0)
		return &g_usb_aux_groups[1];

	if (mod_id == 0x2FF) {
		if (sceKernelGetModel() != 0 || sceKernelIsDevelopmentToolMode() != 0)
			return &g_usb_dmb_group;
		return NULL;
	}

	{
		u32 idx = mod_id & 0xFFFF00FFu;
		if (idx < 4)
			return &g_usb_groups[idx];
	}
	return NULL;
}

static UTILITY_NOINLINE SceUtilityModuleGroup *utilityFindAvModuleGroup(u32 mod_id)
{
	u32 idx = ((mod_id & 0x300u) != 0) ? (mod_id & 0xFFFF00FFu) : mod_id;

	if (idx >= 9)
		return NULL;

	if (idx == 4) {
		SceKernelGameInfo *game_info = (SceKernelGameInfo *)sceKernelGetGameInfo();
		if (game_info != NULL) {
			if (strcmp(game_info->product_code, "ULUS10322") != 0) {
				g_mp3_compat_flag = 1;
				g_mp3_compat_opt.argsize = 0;
				g_mp3_compat_opt.argp = NULL;
				g_av_groups[4].user_mods[0].opt = NULL;
			} else {
				g_mp3_compat_flag = 0;
				g_mp3_compat_opt.argsize = 4;
				g_mp3_compat_opt.argp = &g_mp3_compat_flag;
				g_av_groups[4].user_mods[0].opt = &g_mp3_compat_opt;
			}
		}
	}
	return &g_av_groups[idx];
}

static UTILITY_NOINLINE int utilityInitUsbModules(void)
{
	u32 idx;
	for (idx = 0; idx < 4; idx++)
		utilityInitModuleGroup(&g_usb_groups[idx]);
	for (idx = 0; idx < 3; idx++)
		utilityInitModuleGroup(&g_usb_aux_groups[idx]);
	utilityInitModuleGroup(&g_usb_dmb_group);
	return 0;
}

static UTILITY_NOINLINE int utilityLoadUsbModuleGroup(u32 mod_id)
{
	u32 old_k1 = utilityEnterKernel();
	SceUtilityModuleGroup *group = utilityFindUsbModuleGroup(mod_id);
	SceUtilityModuleGroup *acc_group;
	int ret;

	if (group == NULL) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_MOD_ERROR_INVALID_ID;
	}

	acc_group = utilityFindUsbAccessoryGroup(mod_id);
	if (acc_group != NULL) {
		if (acc_group->ref_count != 0) {
			acc_group->ref_count++;
		} else {
			ret = utilityLoadModuleGroup(acc_group);
			if (ret < 0) {
				utilityLeaveKernel(old_k1);
				return ret;
			}
		}
	}

	ret = utilityLoadModuleGroup(group);
	if (ret < 0 && acc_group != NULL) {
		if (acc_group->ref_count < 2)
			utilityUnloadModuleGroup(acc_group);
		else
			acc_group->ref_count--;
	}

	utilityLeaveKernel(old_k1);
	return ret;
}

static UTILITY_NOINLINE int utilityUnloadUsbModuleGroup(u32 mod_id)
{
	u32 old_k1 = utilityEnterKernel();
	SceUtilityModuleGroup *group = utilityFindUsbModuleGroup(mod_id);
	int ret;

	if (group == NULL) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_MOD_ERROR_INVALID_ID;
	}

	ret = utilityUnloadModuleGroup(group);
	if (ret >= 0) {
		SceUtilityModuleGroup *acc_group = utilityFindUsbAccessoryGroup(mod_id);
		if (acc_group != NULL) {
			if (acc_group->ref_count < 2)
				ret = utilityUnloadModuleGroup(acc_group);
			else {
				acc_group->ref_count--;
				ret = 0;
			}
		}
	}

	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilityLoadUsbModule(u32 mod_id)
{
	u32 old_k1;
	SceUtilityModuleGroup *group;
	int ret;

	if (mod_id - 1 >= 5)
		return SCE_UTILITY_USB_ERROR_INVALID_ID;

	old_k1 = utilityEnterKernel();
	group = utilityFindLegacyUsbModuleGroup(mod_id);
	if (group == NULL)
		ret = SCE_UTILITY_MOD_ERROR_INVALID_ID;
	else
		ret = utilityLoadModuleGroup(group);
	utilityLeaveKernel(old_k1);

	if (ret < 0)
		return (int)(((u32)ret & 0xFFFF00FFu) | 0x0E00u);
	return ret;
}

int sceUtilityUnloadUsbModule(u32 mod_id)
{
	u32 old_k1;
	SceUtilityModuleGroup *group;
	int ret;

	if (mod_id - 1 >= 5)
		return SCE_UTILITY_USB_ERROR_INVALID_ID;

	old_k1 = utilityEnterKernel();
	group = utilityFindLegacyUsbModuleGroup(mod_id);
	if (group == NULL)
		ret = SCE_UTILITY_MOD_ERROR_INVALID_ID;
	else
		ret = utilityUnloadModuleGroup(group);
	utilityLeaveKernel(old_k1);

	if (ret < 0)
		return (int)(((u32)ret & 0xFFFF00FFu) | 0x0E00u);
	return ret;
}

static UTILITY_NOINLINE SceUtilityModuleGroup *utilityFindLegacyUsbModuleGroup(u32 mod_id)
{
	if (mod_id == 5 && utilityIsUsbGpsSerialDriver() != 0)
		return &g_usb_aux_groups[1];
	if (mod_id == 2) {
		if (utilityIsUsbGpsSerialDriver() == 0)
			return &g_usb_aux_groups[0];
		return &g_usb_aux_groups[2];
	}
	if (mod_id == 1)
		return &g_usb_groups[0];
	if (mod_id - 3 < 3)
		return &g_usb_groups[mod_id - 2];
	return NULL;
}

static UTILITY_NOINLINE SceUtilityModuleGroup *utilityFindUsbAccessoryGroup(u32 mod_id)
{
	if (mod_id == 0x203) {
		if (utilityIsUsbGpsSerialDriver() != 0)
			return NULL;
		return &g_usb_aux_groups[0];
	}
	if (mod_id == 0x2FF || (mod_id >= 0x201 && mod_id < 0x204))
		return &g_usb_aux_groups[0];
	return NULL;
}

static UTILITY_NOINLINE int utilityIsUsbGpsSerialDriver(void)
{
	SceRegParam reg_param;
	u32 reg;
	u32 cat;
	u32 key_type;
	SceSize key_size;
	u32 value;
	int is_serial = 0;

	memset(&reg_param, 0, sizeof(reg_param));
	reg_param.reg_type = 1;
	strncpy(reg_param.name, "flash1/registry/system", sizeof(reg_param.name));
	reg_param.name_len = 22;
	reg_param.sys_flag_open = 1;
	reg_param.sys_flag_compat = 1;

	if (sceRegOpenRegistry(&reg_param, 2, &reg) == 0) {
		if (sceRegOpenCategory(reg, "/TOOL/CONFIG", 2, &cat) == 0) {
			if (sceRegGetKeyInfoByName(cat, "usbgps_driver_type", &key_type, &key_size) == 0 &&
			    key_type == 2 && key_size == 4 &&
			    sceRegGetKeyValueByName(cat, "usbgps_driver_type", &value, 4) == 0) {
				is_serial = (value == 1);
			}
			sceRegCloseCategory(cat);
		}
		sceRegCloseRegistry(reg);
	}
	return is_serial;
}

static UTILITY_NOINLINE int utilityLoadAvModuleGroup(u32 mod_id)
{
	u32 old_k1 = utilityEnterKernel();
	SceUtilityModuleGroup *group;
	int ret;

	if (mod_id == 0x3FF) {
		if (sceKernelGetModel() == 0)
			sceKernelIsDevelopmentToolMode();
		ret = sceMeBootStart(4);
		utilityLeaveKernel(old_k1);
		return ret;
	}

	if (mod_id == 0x3FE) {
		s32 wma_play;
		if (utilityRegGetNetInt("/CONFIG/MUSIC", "wma_play", &wma_play) < 0 || wma_play != 1) {
			utilityLeaveKernel(old_k1);
			return SCE_KERNEL_ERROR_UNSUPPORTED;
		}
		sceMeBootStart(3);
		group = &g_av_wma_group;
	} else {
		group = utilityFindAvModuleGroup(mod_id);
	}

	if (group == NULL)
		ret = SCE_UTILITY_MOD_ERROR_INVALID_ID;
	else
		ret = utilityLoadModuleGroup(group);

	utilityLeaveKernel(old_k1);
	return ret;
}

static UTILITY_NOINLINE int utilityUnloadAvModuleGroup(u32 mod_id)
{
	u32 old_k1 = utilityEnterKernel();
	SceUtilityModuleGroup *group;
	int ret;

	if (mod_id == 0x3FF) {
		if (sceKernelGetModel() == 0)
			sceKernelIsDevelopmentToolMode();
		ret = sceMeBootStart(2);
		utilityLeaveKernel(old_k1);
		return ret;
	}

	if (mod_id == 0x3FE)
		group = &g_av_wma_group;
	else
		group = utilityFindAvModuleGroup(mod_id);

	if (group == NULL)
		ret = SCE_UTILITY_MOD_ERROR_INVALID_ID;
	else
		ret = utilityUnloadModuleGroup(group);

	utilityLeaveKernel(old_k1);
	return ret;
}

static UTILITY_NOINLINE int utilityInitAvModules(void)
{
	u32 idx;
	for (idx = 0; idx < 9; idx++)
		utilityInitModuleGroup(&g_av_groups[idx]);
	return 0;
}

int sceUtilityLoadAvModule(u32 mod_id)
{
	int ret;

	if (mod_id >= 4)
		return SCE_UTILITY_AV_ERROR_INVALID_ID;
	ret = utilityLoadAvModuleGroup(mod_id);
	if (ret < 0)
		return (int)(((u32)ret & 0xFFFF00FFu) | 0x0F00u);
	return ret;
}

int sceUtilityUnloadAvModule(u32 mod_id)
{
	int ret;

	if (mod_id >= 4)
		return SCE_UTILITY_AV_ERROR_INVALID_ID;
	ret = utilityUnloadAvModuleGroup(mod_id);
	if (ret < 0)
		return (int)(((u32)ret & 0xFFFF00FFu) | 0x0F00u);
	return ret;
}

static UTILITY_NOINLINE int utilityInitNpModules(void)
{
	u32 idx;
	for (idx = 0; idx < 4; idx++)
		utilityInitModuleGroup(&g_np_groups[idx]);
	return 0;
}

static UTILITY_NOINLINE int utilityLoadNpModuleGroup(u32 mod_id)
{
	u32 old_k1 = utilityEnterKernel();
	u32 idx = mod_id & 0xFFFF00FFu;
	SceUtilityModuleGroup *group = (idx < 4) ? &g_np_groups[idx] : NULL;
	int ret;

	if (group == NULL)
		ret = SCE_UTILITY_MOD_ERROR_INVALID_ID;
	else
		ret = utilityLoadModuleGroup(group);

	utilityLeaveKernel(old_k1);
	return ret;
}

static UTILITY_NOINLINE int utilityUnloadNpModuleGroup(u32 mod_id)
{
	u32 old_k1 = utilityEnterKernel();
	u32 idx = mod_id & 0xFFFF00FFu;
	SceUtilityModuleGroup *group = (idx < 4) ? &g_np_groups[idx] : NULL;
	int ret;

	if (group == NULL)
		ret = SCE_UTILITY_MOD_ERROR_INVALID_ID;
	else
		ret = utilityUnloadModuleGroup(group);

	utilityLeaveKernel(old_k1);
	return ret;
}

static UTILITY_NOINLINE int utilityLoadNpDrmModuleGroup(u32 mod_id)
{
	u32 old_k1 = utilityEnterKernel();
	SceUtilityModuleGroup *group = NULL;
	int ret;

	if (mod_id == 0x500 && sceKernelIsToolMode() != 0)
		group = &g_npdrm_groups[1];
	else if ((mod_id & 0xFFFF00FFu) == 0)
		group = &g_npdrm_groups[0];

	if (group == NULL) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_MOD_ERROR_INVALID_ID;
	}

	ret = 0;
	if (sceKernelInitDiscImage() == 0) {
		if (group->ref_count == 0)
			ret = utilityLoadModuleGroup(group);
		else
			group->ref_count++;
	}
	if (ret == 0)
		sceNpDrmSetDebugMode(utilityIsNpDrmCheckEnabled());

	utilityLeaveKernel(old_k1);
	return ret;
}

static UTILITY_NOINLINE int utilityUnloadNpDrmModuleGroup(u32 mod_id)
{
	u32 old_k1 = utilityEnterKernel();
	SceUtilityModuleGroup *group = NULL;
	int ret;

	if (mod_id == 0x500 && sceKernelIsToolMode() != 0)
		group = &g_npdrm_groups[1];
	else if ((mod_id & 0xFFFF00FFu) == 0)
		group = &g_npdrm_groups[0];

	if (group == NULL) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_MOD_ERROR_INVALID_ID;
	}

	ret = 0;
	if (sceKernelInitDiscImage() == 0) {
		if (group->ref_count < 2)
			ret = utilityUnloadModuleGroup(group);
		else
			group->ref_count--;
	}

	utilityLeaveKernel(old_k1);
	return ret;
}

static UTILITY_NOINLINE int utilityIsNpDrmCheckEnabled(void)
{
	SceRegParam reg_param;
	u32 reg;
	u32 cat;
	u32 key_type;
	SceSize key_size;
	u32 value;
	int check_drm = 0;

	memset(&reg_param, 0, sizeof(reg_param));
	reg_param.reg_type = 1;
	strncpy(reg_param.name, "flash1/registry/system", sizeof(reg_param.name));
	reg_param.name_len = 22;
	reg_param.sys_flag_open = 1;
	reg_param.sys_flag_compat = 1;

	if (sceRegOpenRegistry(&reg_param, 2, &reg) == 0) {
		if (sceRegOpenCategory(reg, "/CONFIG/NP", 2, &cat) == 0) {
			if (sceRegGetKeyInfoByName(cat, "check_drm", &key_type, &key_size) == 0 &&
			    key_type == 2 && key_size == 4 &&
			    sceRegGetKeyValueByName(cat, "check_drm", &value, 4) == 0) {
				check_drm = (value == 1);
			}
			sceRegCloseCategory(cat);
		}
		sceRegCloseRegistry(reg);
	}
	return check_drm;
}

static UTILITY_NOINLINE int utilityInitNpDrmModules(void)
{
	u32 idx;
	for (idx = 0; idx < 2; idx++)
		utilityInitModuleGroup(&g_npdrm_groups[idx]);
	return 0;
}

static UTILITY_NOINLINE int utilityLoadIrModuleGroup(u32 mod_id)
{
	u32 old_k1 = utilityEnterKernel();
	u32 idx = mod_id & 0xFFFF00FFu;
	u32 peer_idx = (mod_id ^ 1u) & 0xFFFF00FFu;
	SceUtilityModuleGroup *group = (idx < 2) ? &g_ir_groups[idx] : NULL;
	SceUtilityModuleGroup *peer_group;
	int ret;

	if (group == NULL) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_MOD_ERROR_INVALID_ID;
	}

	peer_group = (peer_idx < 2) ? &g_ir_groups[peer_idx] : NULL;
	if (peer_group->kern_mods[0].modid >= 0) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_MOD_ERROR_ALREADY_LOADED;
	}

	ret = utilityLoadModuleGroup(group);
	utilityLeaveKernel(old_k1);
	return ret;
}

static UTILITY_NOINLINE int utilityInitIrModules(void)
{
	u32 idx;
	for (idx = 0; idx < 2; idx++)
		utilityInitModuleGroup(&g_ir_groups[idx]);
	return 0;
}

static UTILITY_NOINLINE int utilityUnloadIrModuleGroup(u32 mod_id)
{
	u32 old_k1 = utilityEnterKernel();
	u32 idx = mod_id & 0xFFFF00FFu;
	SceUtilityModuleGroup *group = (idx < 2) ? &g_ir_groups[idx] : NULL;
	int ret;

	if (group == NULL)
		ret = SCE_UTILITY_MOD_ERROR_INVALID_ID;
	else
		ret = utilityUnloadModuleGroup(group);

	utilityLeaveKernel(old_k1);
	return ret;
}

static UTILITY_NOINLINE int utilityInitNpInstModules(void)
{
	utilityInitModuleGroup(&g_np_inst_group);
	return 0;
}

int sceUtility_private_7EBD6208(void)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityLoadModuleGroup(&g_np_inst_group);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtility_private_40FBA532(void)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityUnloadModuleGroup(&g_np_inst_group);
	utilityLeaveKernel(old_k1);
	return ret;
}

static UTILITY_NOINLINE int utilityLoadModuleGroup(SceUtilityModuleGroup *group)
{
	int idx;
	int ret = 0;

	if (group->ref_count != 0)
		return SCE_UTILITY_MOD_ERROR_ALREADY_LOADED;

	for (idx = 0; idx < (int)group->kern_count; idx++) {
		ret = utilityLoadKernelModule(&group->kern_mods[idx]);
		if (ret < 0)
			break;
	}

	if (ret >= 0 && group->user_count != 0) {
		SceUID cur_block = sceKernelAllocPartitionMemory(2, "SceUtilityModule", 0, group->user_mem_size, NULL);
		ret = cur_block;
		if (cur_block >= 0) {
			SceUID rem_block = -1;
			for (idx = 0; idx < (int)group->user_count; idx++) {
				rem_block = -1;
				ret = utilityLoadUserModule(&group->user_mods[idx], cur_block, &rem_block);
				if (ret < 0) {
					int rev;
					for (rev = (int)group->user_count - 1; rev >= 0; rev--)
						utilityUnloadUserModule(&group->user_mods[rev]);
					if (rem_block >= 0)
						sceKernelFreePartitionMemory(rem_block);
					sceKernelFreePartitionMemory(cur_block);
					break;
				}
				cur_block = rem_block;
			}

			if (ret >= 0) {
				group->rem_block = rem_block;
				if (utilityIsModuleGuardEnabled() != 0) {
					SceKernelSysMemInfo mem_info;
					group->guard_seed = sceKernelGetSystemTimeLow();
					mem_info.size = sizeof(SceKernelSysMemInfo);
					if (sceKernelQueryMemoryBlockInfo(rem_block, &mem_info) >= 0)
						sceKernelMemset32(mem_info.addr, group->guard_seed, mem_info.mem_size);
				}
			}
		}
	}

	if (ret >= 0) {
		group->ref_count = 1;
		return 0;
	}

	for (idx = (int)group->kern_count - 1; idx >= 0; idx--)
		utilityUnloadKernelModule(&group->kern_mods[idx]);
	return ret;
}

static UTILITY_NOINLINE int utilityUnloadModuleGroup(SceUtilityModuleGroup *group)
{
	int ret = 0;
	int idx;

	if (group->ref_count == 0)
		return SCE_UTILITY_MOD_ERROR_NOT_LOADED;

	for (idx = (int)group->user_count - 1; idx >= 0; idx--)
		ret = utilityUnloadUserModule(&group->user_mods[idx]);

	if (group->rem_block >= 0) {
		if (utilityIsModuleGuardEnabled() != 0) {
			SceKernelSysMemInfo mem_info;
			u32 *words;
			u32 count;
			u32 pos;

			mem_info.size = sizeof(SceKernelSysMemInfo);
			sceKernelQueryMemoryBlockInfo(group->rem_block, &mem_info);
			words = (u32 *)mem_info.addr;
			count = mem_info.mem_size >> 2;
			for (pos = 0; pos < count; pos++) {
				if (words[pos] != group->guard_seed) {
					Kprintf("utility_module reserved memory has been violated 0x%p ... stop\n", &words[pos]);
					__asm__ volatile("break 0");
				}
			}
		}
		sceKernelFreePartitionMemory(group->rem_block);
		group->rem_block = -1;
	}

	for (idx = (int)group->kern_count - 1; idx >= 0; idx--)
		ret = utilityUnloadKernelModule(&group->kern_mods[idx]);

	if (ret < 0 && (int)group->user_count + (int)group->kern_count == 1)
		return ret;

	group->ref_count = 0;
	return 0;
}

static UTILITY_NOINLINE int utilityInitModuleGroup(SceUtilityModuleGroup *group)
{
	int idx;

	group->rem_block = -1;
	group->ref_count = 0;

	for (idx = 0; idx < (int)group->kern_count; idx++)
		group->kern_mods[idx].modid = -1;

	for (idx = 0; idx < (int)group->user_count; idx++) {
		group->user_mods[idx].modid = -1;
		group->user_mods[idx].block_id = -1;
	}
	return 0;
}

int sceUtilityLoadModule(u32 mod_id)
{
	switch (mod_id & 0xFF00u) {
	case 0x100: return utilityLoadNetModuleGroup(mod_id);
	case 0x200: return utilityLoadUsbModuleGroup(mod_id);
	case 0x300: return utilityLoadAvModuleGroup(mod_id);
	case 0x400: return utilityLoadNpModuleGroup(mod_id);
	case 0x500: return utilityLoadNpDrmModuleGroup(mod_id);
	case 0x600: return utilityLoadIrModuleGroup(mod_id);
	default:    return SCE_UTILITY_MOD_ERROR_INVALID_ID;
	}
}

int sceUtilityUnloadModule(u32 mod_id)
{
	switch (mod_id & 0xFF00u) {
	case 0x100: return utilityUnloadNetModuleGroup(mod_id);
	case 0x200: return utilityUnloadUsbModuleGroup(mod_id);
	case 0x300: return utilityUnloadAvModuleGroup(mod_id);
	case 0x400: return utilityUnloadNpModuleGroup(mod_id);
	case 0x500: return utilityUnloadNpDrmModuleGroup(mod_id);
	case 0x600: return utilityUnloadIrModuleGroup(mod_id);
	default:    return SCE_UTILITY_MOD_ERROR_INVALID_ID;
	}
}

static UTILITY_NOINLINE int utilityLoadKernelModule(SceUtilityKernMod *entry)
{
	SceKernelLMOption lm_opt;
	SceSize argsize = 0;
	void *argp = NULL;
	SceUID modid;
	int status;
	int ret;

	lm_opt.size = sizeof(SceKernelLMOption);
	lm_opt.mpid_text = 0;
	lm_opt.mpid_data = 0;
	lm_opt.flags = 0;
	lm_opt.position = 0;
	lm_opt.access = 0;
	lm_opt.c_reserved[0] = 0;
	lm_opt.c_reserved[1] = 0;

	if (entry->opt != NULL) {
		lm_opt.mpid_text = entry->opt->mpid;
		argsize = entry->opt->argsize;
		argp = entry->opt->argp;
	} else {
		lm_opt.mpid_text = 1;
	}

	modid = sceKernelLoadModuleForKernel(entry->path, 0, &lm_opt);
	if (modid < 0)
		return modid;

	ret = sceKernelStartModule(modid, argsize, argp, &status, NULL);
	if (ret <= 0) {
		sceKernelUnloadModule(modid);
		if (ret == 0)
			return SCE_UTILITY_MOD_ERROR_START_FAILED;
		return ret;
	}

	entry->modid = modid;
	return 0;
}

static UTILITY_NOINLINE int utilityUnloadKernelModule(SceUtilityKernMod *entry)
{
	int status;

	if (entry->modid >= 0) {
		if (sceKernelStopModule(entry->modid, 0, NULL, &status, NULL) == SCE_KERNEL_ERROR_CANNOT_STOP) {
			if (sceKernelGetCompiledSdkVersion() > 0x0307FFFF)
				return SCE_KERNEL_ERROR_CANNOT_STOP;
		}
		sceKernelUnloadModule(entry->modid);
	}
	entry->modid = -1;
	return 0;
}

static UTILITY_NOINLINE int utilityLoadUserModule(SceUtilityUserMod *entry, SceUID block_id, SceUID *rem_block)
{
	SceSize argsize = 0;
	void *argp = NULL;
	const SceKernelLMOption *lm_opt = NULL;
	SceUID modid;
	int status;
	int ret;

	if (entry->opt != NULL) {
		argsize = entry->opt->argsize;
		argp = entry->opt->argp;
		lm_opt = entry->opt->lm_opt;
	}

	modid = sceKernelLoadModuleToBlock(entry->path, block_id, rem_block, 0, (void *)lm_opt);
	if (modid < 0)
		return modid;

	ret = sceKernelStartModule(modid, argsize, argp, &status, NULL);
	if (ret <= 0) {
		sceKernelUnloadModule(modid);
		if (ret == 0)
			return SCE_UTILITY_MOD_ERROR_START_FAILED;
		return ret;
	}

	entry->block_id = block_id;
	entry->modid = modid;
	return 0;
}

static UTILITY_NOINLINE int utilityUnloadUserModule(SceUtilityUserMod *entry)
{
	int status;

	if (entry->modid >= 0) {
		if (sceKernelStopModule(entry->modid, 0, NULL, &status, NULL) == SCE_KERNEL_ERROR_CANNOT_STOP) {
			if (sceKernelGetCompiledSdkVersion() > 0x0307FFFF)
				return SCE_KERNEL_ERROR_CANNOT_STOP;
		}
		sceKernelUnloadModule(entry->modid);
	}
	if (entry->block_id >= 0)
		sceKernelFreePartitionMemory(entry->block_id);
	entry->block_id = -1;
	entry->modid = -1;
	return 0;
}

static UTILITY_NOINLINE int utilityIsModuleGuardEnabled(void)
{
	if (!sceKernelIsToolMode())
		return 0;
	if (sceKernelIsDevelopmentToolMode()) {
		if (sceKernelDipsw(25) != 0)
			return 0;
	} else {
		if (sceKernelDipsw(24) == 0)
			return 0;
	}
	return sceKernelGetCompiledSdkVersion() > 0x0306FFFF;
}

/* Kermit OSK bridge */
int sceUtility_private_3B6D7CED(SceUtilityOskParams *params)
{
	u32 old_k1 = utilityEnterKernel();
	int ret;

	if (utilityIsBadRange(params, sizeof(SceUtilityOskParams))) {
		utilityLeaveKernel(old_k1);
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}

	g_osk.status = 2;
	g_osk.current_item = 0;

	if (sceUtility_private_5DCBD3C0() != 0) {
		utilityLeaveKernel(old_k1);
		return -1;
	}

	ret = utilityKermitOskStartItem(params, g_osk.current_item);
	if (ret < 0)
		g_osk.status = 0;

	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtility_private_CF6799A3(void)
{
	u32 old_k1 = utilityEnterKernel();
	int idx;

	utilityKermitOskFinishItem(g_osk.current_item);
	g_osk.user_params->base.result = 2;
	for (idx = 0; idx < g_osk.item_count; idx++) {
		SceUtilityOskItem *item = &g_osk.user_params->items[idx];
		if (!utilityIsBadRange(&item->result, sizeof(item->result)))
			item->result = 1;
	}
	sceUtility_private_390A738F();
	utilityLeaveKernel(old_k1);
	return 0;
}

int sceUtility_private_390A738F(void)
{
	u32 old_k1 = utilityEnterKernel();
	g_osk.item_count = 0;
	g_osk.user_params = NULL;
	g_osk.shared_params = NULL;
	g_osk.status = 0;
	utilityLeaveKernel(old_k1);
	return 0;
}

int sceUtility_private_48B0FBCD(void)
{
	u32 old_k1 = utilityEnterKernel();
	u8 req_buf[128];
	u32 aligned;
	SceKermitRequest *req;
	u64 resp;
	int status;
	int ret;

	if (sceUtility_private_5DCBD3C0() != 0) {
		utilityLeaveKernel(old_k1);
		return -1;
	}

	aligned = ((u32)req_buf + 63u) & ~63u;
	sceKernelDcacheInvalidateRange((void *)aligned, 64);
	req = KERMIT_UNCACHED(aligned);
	ret = sceKermit_driver_36666181(req, KERMIT_MODE_UTILITY, 270, 0, 0, &resp);
	status = (ret == 0) ? (int)resp : ret;
	if (status < 0) {
		utilityLeaveKernel(old_k1);
		return status;
	}

	if (g_osk.pending_next == 0) {
		if (status == 3) {
			if (sceUtility_private_5DCBD3C0() != 0) {
				utilityLeaveKernel(old_k1);
				return -1;
			}
			ret = utilityKermitOskFinishItem(g_osk.current_item);
			if (ret < 0) {
				utilityLeaveKernel(old_k1);
				return ret;
			}
			if (g_osk.user_params->base.result == 1) {
				int idx;
				for (idx = 0; idx < g_osk.item_count; idx++) {
					SceUtilityOskItem *item = &g_osk.user_params->items[idx];
					if (!utilityIsBadRange(&item->result, sizeof(item->result)))
						item->result = 1;
				}
				g_osk.status = 3;
				utilityLeaveKernel(old_k1);
				return 0;
			}
			g_osk.current_item++;
			if (g_osk.current_item < g_osk.item_count) {
				g_osk.pending_next = 1;
				g_osk.status = 2;
				utilityLeaveKernel(old_k1);
				return 0;
			}
			g_osk.status = 3;
		} else if (status == 2) {
			g_osk.status = 2;
		}
		utilityLeaveKernel(old_k1);
		return 0;
	}

	if (status == 0) {
		if (sceUtility_private_5DCBD3C0() != 0) {
			utilityLeaveKernel(old_k1);
			return -1;
		}
		ret = utilityKermitOskStartItem(g_osk.user_params, g_osk.current_item);
		if (ret < 0) {
			utilityLeaveKernel(old_k1);
			return ret;
		}
		g_osk.pending_next = 0;
	}

	g_osk.status = 2;
	utilityLeaveKernel(old_k1);
	return 0;
}

/* Public dialog & applet wrappers */
int sceUtilityGameSharingInitStart(void *params)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityDialogInitStart(3, params);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilityGameSharingShutdownStart(void)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityDialogShutdownStart(3);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilityGameSharingUpdate(int speed)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityDialogUpdate(3, speed);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilityGameSharingGetStatus(void)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityDialogGetStatus(3);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilityNetconfInitStart(void *params)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityDialogInitStart(4, params);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilityNetconfShutdownStart(void)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityDialogShutdownStart(4);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilityNetconfUpdate(int speed)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityDialogUpdate(4, speed);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilityNetconfGetStatus(void)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityDialogGetStatus(4);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilitySavedataInitStart(void *params)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityDialogInitStart(1, params);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilitySavedataShutdownStart(void)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityDialogShutdownStart(1);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilitySavedataUpdate(int speed)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityDialogUpdate(1, speed);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilitySavedataGetStatus(void)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityDialogGetStatus(1);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilityMsgDialogInitStart(void *params)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityDialogInitStart(5, params);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilityMsgDialogShutdownStart(void)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityDialogShutdownStart(5);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilityMsgDialogUpdate(int speed)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityDialogUpdate(5, speed);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilityMsgDialogGetStatus(void)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityDialogGetStatus(5);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilityMsgDialogAbort(void)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityDialogAbort(5);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilityOskInitStart(void *params)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityDialogInitStart(6, params);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilityOskShutdownStart(void)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityDialogShutdownStart(6);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilityOskUpdate(int speed)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityDialogUpdate(6, speed);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilityOskGetStatus(void)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityDialogGetStatus(6);
	utilityLeaveKernel(old_k1);
	return ret;
}

static UTILITY_NOINLINE int utilityKermitOskInit(void)
{
	sceKernelMemset((void *)KERMIT_OSK_SHARED_BASE, 0, KERMIT_OSK_SHARED_SIZE);
	g_osk.shared_base = (void *)KERMIT_OSK_SHARED_BASE;
	g_osk.work_area = (SceUtilityOskSharedArea *)KERMIT_OSK_WORK_BASE;
	g_osk.work_size = KERMIT_OSK_WORK_SIZE;
	g_osk.mutex_id = sceKernelCreateMutex("SceKermitOsk", 0x100, 0, NULL);
	return g_osk.mutex_id < 1;
}

int sceUtility_private_B08B2B48(void)
{
	return g_osk.status;
}

int sceUtility_DDE5389D(void *params) { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogInitStart(7, params); utilityLeaveKernel(old_k1); return r; }
int sceUtility_149A7895(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogShutdownStart(7); utilityLeaveKernel(old_k1); return r; }
int sceUtility_4A833BA4(int speed)    { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogUpdate(7, speed); utilityLeaveKernel(old_k1); return r; }
int sceUtility_A50E5B30(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogGetStatus(7); utilityLeaveKernel(old_k1); return r; }

int sceUtility_943CBA46(void *params) { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogInitStart(8, params); utilityLeaveKernel(old_k1); return r; }
int sceUtility_0F3EEAAC(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogShutdownStart(8); utilityLeaveKernel(old_k1); return r; }
int sceUtility_147F7C85(int speed)    { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogUpdate(8, speed); utilityLeaveKernel(old_k1); return r; }
int sceUtility_16A1A8D8(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogGetStatus(8); utilityLeaveKernel(old_k1); return r; }

int sceUtility_1281DA8E(void *params) { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogInitStart(9, params); utilityLeaveKernel(old_k1); return r; }
int sceUtility_5EF1C24A(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogShutdownStart(9); utilityLeaveKernel(old_k1); return r; }
int sceUtility_A03D29BA(int speed)    { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogUpdate(9, speed); utilityLeaveKernel(old_k1); return r; }
int sceUtility_C4700FA3(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogGetStatus(9); utilityLeaveKernel(old_k1); return r; }

int sceUtility_24AC31EB(void *params) { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogInitStart(17, params); utilityLeaveKernel(old_k1); return r; }
int sceUtility_32E32DCB(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogShutdownStart(17); utilityLeaveKernel(old_k1); return r; }
int sceUtility_4AECD179(int speed)    { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogUpdate(17, speed); utilityLeaveKernel(old_k1); return r; }
int sceUtility_B57E95D9(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogGetStatus(17); utilityLeaveKernel(old_k1); return r; }
int sceUtility_180F7B62(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogAbort(17); utilityLeaveKernel(old_k1); return r; }

int sceUtility_private_756A93B3(void *params) { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogInitStart(10, params); utilityLeaveKernel(old_k1); return r; }
int sceUtility_private_26D0B065(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogShutdownStart(10); utilityLeaveKernel(old_k1); return r; }
int sceUtility_private_CE484145(int speed)    { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogUpdate(10, speed); utilityLeaveKernel(old_k1); return r; }
int sceUtility_private_981FFD72(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogGetStatus(10); utilityLeaveKernel(old_k1); return r; }

int sceUtility_0251B134(void *params) { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogInitStart(11, params); utilityLeaveKernel(old_k1); return r; }
int sceUtility_F9E0008C(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogShutdownStart(11); utilityLeaveKernel(old_k1); return r; }
int sceUtility_AB083EA9(int speed)    { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogUpdate(11, speed); utilityLeaveKernel(old_k1); return r; }
int sceUtility_D81957B7(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogGetStatus(11); utilityLeaveKernel(old_k1); return r; }

int sceUtility_16D02AF0(void *params) { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogInitStart(12, params); utilityLeaveKernel(old_k1); return r; }
int sceUtility_E19C97D6(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogShutdownStart(12); utilityLeaveKernel(old_k1); return r; }
int sceUtility_F3FBC572(int speed)    { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogUpdate(12, speed); utilityLeaveKernel(old_k1); return r; }
int sceUtility_86ABDB1B(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogGetStatus(12); utilityLeaveKernel(old_k1); return r; }

int sceUtility_42071A83(void *params) { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogInitStart(13, params); utilityLeaveKernel(old_k1); return r; }
int sceUtility_D17A0573(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogShutdownStart(13); utilityLeaveKernel(old_k1); return r; }
int sceUtility_D852CDCE(int speed)    { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogUpdate(13, speed); utilityLeaveKernel(old_k1); return r; }
int sceUtility_89317C8F(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogGetStatus(13); utilityLeaveKernel(old_k1); return r; }

int sceUtility_4B0A8FE5(void *params) { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogInitStart(14, params); utilityLeaveKernel(old_k1); return r; }
int sceUtility_06A48659(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogShutdownStart(14); utilityLeaveKernel(old_k1); return r; }
int sceUtility_A084E056(int speed)    { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogUpdate(14, speed); utilityLeaveKernel(old_k1); return r; }
int sceUtility_2B96173B(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogGetStatus(14); utilityLeaveKernel(old_k1); return r; }

int sceUtility_81C44706(void *params) { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogInitStart(15, params); utilityLeaveKernel(old_k1); return r; }
int sceUtility_E7B778D8(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogShutdownStart(15); utilityLeaveKernel(old_k1); return r; }
int sceUtility_6F56F9CF(int speed)    { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogUpdate(15, speed); utilityLeaveKernel(old_k1); return r; }
int sceUtility_8326AB05(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogGetStatus(15); utilityLeaveKernel(old_k1); return r; }

int sceUtility_DA97F1AA(void *params) { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogInitStart(16, params); utilityLeaveKernel(old_k1); return r; }
int sceUtility_54A5C62F(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogShutdownStart(16); utilityLeaveKernel(old_k1); return r; }
int sceUtility_B8592D5F(int speed)    { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogUpdate(16, speed); utilityLeaveKernel(old_k1); return r; }
int sceUtility_3AAD51DC(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogGetStatus(16); utilityLeaveKernel(old_k1); return r; }

int sceUtility_A7BB7C67(void *params) { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogInitStart(18, params); utilityLeaveKernel(old_k1); return r; }
int sceUtility_C130D441(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogShutdownStart(18); utilityLeaveKernel(old_k1); return r; }
int sceUtility_0940A1B9(int speed)    { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogUpdate(18, speed); utilityLeaveKernel(old_k1); return r; }
int sceUtility_094198B8(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogGetStatus(18); utilityLeaveKernel(old_k1); return r; }

int sceUtility_3A15CD0A(void *params) { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogInitStart(19, params); utilityLeaveKernel(old_k1); return r; }
int sceUtility_9F313D14(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogShutdownStart(19); utilityLeaveKernel(old_k1); return r; }
int sceUtility_D23665F4(int speed)    { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogUpdate(19, speed); utilityLeaveKernel(old_k1); return r; }
int sceUtility_D4C2BD73(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogGetStatus(19); utilityLeaveKernel(old_k1); return r; }
int sceUtility_0E0C27AF(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogAbort(19); utilityLeaveKernel(old_k1); return r; }

int sceUtility_B0FB7FF5(void *params) { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogResume(15, params); utilityLeaveKernel(old_k1); return r; }
int sceUtility_86A03A27(void *params) { u32 old_k1 = utilityEnterKernel(); int r = utilityDialogResume(11, params); utilityLeaveKernel(old_k1); return r; }

int sceUtilityHtmlViewerInitStart(void *params) { u32 old_k1 = utilityEnterKernel(); int r = utilityAppletInitStart(1, params); utilityLeaveKernel(old_k1); return r; }
int sceUtilityHtmlViewerShutdownStart(void)     { u32 old_k1 = utilityEnterKernel(); int r = utilityAppletShutdownStart(1); utilityLeaveKernel(old_k1); return r; }
int sceUtilityHtmlViewerUpdate(int speed)       { u32 old_k1 = utilityEnterKernel(); int r = utilityAppletUpdate(1, speed); utilityLeaveKernel(old_k1); return r; }
int sceUtilityHtmlViewerGetStatus(void)         { u32 old_k1 = utilityEnterKernel(); int r = utilityAppletGetStatus(1); utilityLeaveKernel(old_k1); return r; }

void sceUtility_3AD50AE7(void) {}
void sceUtility_BC6B6296(void) {}
void sceUtility_417BED54(void) {}
void sceUtility_B6CEE597(void) {}
void sceUtility_2995D020(void) {}
void sceUtility_B62A4061(void) {}
void sceUtility_ED0FAD38(void) {}
void sceUtility_88BC7406(void) {}

static UTILITY_NOINLINE int utilityKermitOskStartItem(SceUtilityOskParams *params, int item_index)
{
	u8 req_buf[128];
	u32 aligned;
	SceKermitRequest *req;
	SceUtilityOskSharedArea *area;
	SceUtilityOskItem *user_item;
	const u16 *src_ptr;
	int chars;
	u32 copy_bytes;
	u64 resp;
	int lock_ret;
	int ret;

	if (utilityIsBadSpan(params, sizeof(SceUtilityOskParams)))
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

	aligned = ((u32)req_buf + 63u) & ~63u;
	sceKernelDcacheInvalidateRange((void *)aligned, 64);
	req = KERMIT_UNCACHED(aligned);

	lock_ret = sceKernelLockMutex(g_osk.mutex_id, 1, NULL);
	if (lock_ret != 0)
		return lock_ret;

	g_osk.item_count = params->item_count;
	area = g_osk.work_area;
	g_osk.user_params = params;

	ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (!utilityIsBadSpan(params, sizeof(SceUtilityOskParams))) {
		sceKernelMemcpy(&area->params, params, sizeof(SceUtilityOskParams));
		user_item = &params->items[item_index];
		if (!utilityIsBadRange(user_item, sizeof(SceUtilityOskItem))) {
			sceKernelMemcpy(&area->item, user_item, sizeof(SceUtilityOskItem));

			src_ptr = area->item.init_text_ptr;
			if (src_ptr == NULL)
				src_ptr = &g_osk_empty_utf16;
			chars = 0;
			while (chars < 512 && src_ptr[chars] != 0)
				chars++;
			memset(area->init_text, 0, sizeof(area->init_text));
			copy_bytes = (u32)(chars * 2 + 2);
			if (!utilityIsBadSpan(src_ptr, copy_bytes) ||
			    (src_ptr == &g_osk_empty_utf16 && copy_bytes < 3)) {
				sceKernelMemcpy(area->init_text, src_ptr, copy_bytes);

				src_ptr = area->item.desc_ptr;
				if (src_ptr == NULL)
					src_ptr = &g_osk_empty_utf16;
				chars = 0;
				while (chars < 512 && src_ptr[chars] != 0)
					chars++;
				memset(area->desc_text, 0, sizeof(area->desc_text));
				copy_bytes = (u32)(chars * 2 + 2);
				if (!utilityIsBadSpan(src_ptr, copy_bytes) ||
				    (src_ptr == &g_osk_empty_utf16 && copy_bytes < 3)) {
					sceKernelMemcpy(area->desc_text, src_ptr, copy_bytes);

					area->item.desc_ptr = area->desc_text;
					area->item.init_text_ptr = area->init_text;
					area->item.out_text_ptr = area->out_text;
					area->params.items = &area->item;

					sceKermitMemory_driver_AAF047AC(req, 0, area, sizeof(SceUtilityOskParams), 3);
					g_osk.shared_params = area;
					ret = sceKermit_driver_36666181(req, KERMIT_MODE_UTILITY, 269, 1, 0, &resp);
					if (ret == 0)
						ret = (int)resp;
				}
			}
		}
	}

	lock_ret = sceKernelUnlockMutex(g_osk.mutex_id, 1);
	if (lock_ret != 0)
		return lock_ret;
	return ret;
}

static UTILITY_NOINLINE int utilityKermitOskFinishItem(int item_index)
{
	u8 req_buf[128];
	u32 aligned;
	SceKermitRequest *req;
	u64 resp;
	int lock_ret;
	int ret;

	aligned = ((u32)req_buf + 63u) & ~63u;
	sceKernelDcacheInvalidateRange((void *)aligned, 64);
	req = KERMIT_UNCACHED(aligned);

	lock_ret = sceKernelLockMutex(g_osk.mutex_id, 1, NULL);
	if (lock_ret != 0)
		return lock_ret;

	ret = sceKermit_driver_36666181(req, KERMIT_MODE_UTILITY, 268, 0, 0, &resp);
	if (ret == 0) {
		SceUtilityOskItem *user_item;
		ret = (int)resp;

		sceKermitMemory_driver_90B662D0(g_osk.shared_params->params.items, 64);
		user_item = &g_osk.user_params->items[item_index];
		if (!utilityIsBadRange(&user_item->result, sizeof(user_item->result)))
			user_item->result = g_osk.shared_params->params.items->result;

		sceKermitMemory_driver_90B662D0(g_osk.shared_params, sizeof(SceUtilityOskParams));
		g_osk.user_params->base.result = g_osk.shared_params->params.base.result;

		if (g_osk.shared_params->params.base.result == 0) {
			u16 *shared_out = g_osk.shared_params->params.items->out_text_ptr;
			sceKermitMemory_driver_90B662D0(shared_out, 1024);

			if (utilityIsBadPtr(user_item)) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
			} else if (user_item->out_text_ptr != NULL) {
				u32 max_bytes = (u32)(user_item->out_text_limit * 2);
				int chars = 0;
				u32 copy_bytes;

				if (max_bytes > 1024)
					max_bytes = 1024;
				while (chars < 512 && shared_out[chars] != 0)
					chars++;
				copy_bytes = (u32)(chars * 2 + 2);
				if (max_bytes < copy_bytes)
					copy_bytes = max_bytes;

				if (utilityIsBadSpan(user_item->out_text_ptr, copy_bytes)) {
					ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
				} else {
					sceKernelMemcpy(user_item->out_text_ptr, shared_out, copy_bytes);
					ret = 0;
				}
			}
		}
	}

	lock_ret = sceKernelUnlockMutex(g_osk.mutex_id, 1);
	if (lock_ret != 0)
		return lock_ret;
	return ret;
}

/* SystemParam */
int sceUtilitySetSystemParamInt(int sysparam_id, u32 value)
{
	u32 old_k1 = utilityEnterKernel();
	int ret;

	if (sysparam_id == 2) {
		if (value == 0 || value == 1 || value == 6 || value == 11)
			ret = utilityRegSetSysParamInt("/CONFIG/NETWORK/ADHOC", "channel", (s32)value);
		else
			ret = SCE_UTILITY_SYSPARAM_ERROR_INVALID_VAL;
	} else if (sysparam_id == 3) {
		u32 clamped = (value < 2) ? value : 1u;
		ret = utilityRegSetSysParamInt("/CONFIG/SYSTEM/POWER_SAVING", "wlan_mode", (s32)clamped);
	} else {
		ret = SCE_UTILITY_SYSPARAM_ERROR_INVALID_ID;
	}

	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilityGetSystemParamInt(int sysparam_id, s32 *out_val)
{
	u32 old_k1 = utilityEnterKernel();
	int ret;

	switch (sysparam_id) {
	case 2:  ret = utilityRegGetSysParamInt("/CONFIG/NETWORK/ADHOC", "channel", out_val); break;
	case 3:  ret = utilityRegGetSysParamInt("/CONFIG/SYSTEM/POWER_SAVING", "wlan_mode", out_val); break;
	case 4:  ret = utilityRegGetSysParamInt("/CONFIG/DATE", "date_format", out_val); break;
	case 5:  ret = utilityRegGetSysParamInt("/CONFIG/DATE", "time_format", out_val); break;
	case 6:  ret = utilityRegGetSysParamInt("/CONFIG/DATE", "time_zone_offset", out_val); break;
	case 7:  ret = utilityRegGetSysParamInt("/CONFIG/DATE", "summer_time", out_val); break;
	case 8:  ret = utilityRegGetSysParamInt("/CONFIG/SYSTEM/XMB", "language", out_val); break;
	case 9:  ret = utilityRegGetSysParamInt("/CONFIG/SYSTEM/XMB", "button_assign", out_val); break;
	case 10: ret = utilityRegGetSysParamInt("/CONFIG/SYSTEM/LOCK", "parental_level", out_val); break;
	default: ret = SCE_UTILITY_SYSPARAM_ERROR_INVALID_ID; break;
	}

	utilityLeaveKernel(old_k1);
	return ret;
}

static UTILITY_NOINLINE int utilityRegGetSysParamInt(const char *cat_path, const char *key_name, s32 *out_val)
{
	SceRegParam reg_param;
	u32 reg;
	u32 cat;
	u32 key_handle;
	u32 key_type;
	SceSize key_size;
	int ret;

	memset(&reg_param, 0, sizeof(reg_param));
	reg_param.reg_type = 1;
	reg_param.sys_flag_open = 1;
	reg_param.sys_flag_compat = 1;

	ret = sceRegOpenRegistry(&reg_param, 2, &reg);
	if (ret != 0) {
		Kprintf("couldn't open registry - 0x%x\n", ret);
		return ret;
	}

	ret = sceRegOpenCategory(reg, cat_path, 2, &cat);
	if (ret == 0) {
		ret = sceRegGetKeyInfo(cat, key_name, &key_handle, &key_type, &key_size);
		if (ret == 0) {
			if (key_type != 2) {
				sceRegCloseCategory(cat);
				sceRegCloseRegistry(reg);
				return SCE_UTILITY_SYSPARAM_ERROR_INVALID_TYPE;
			}
			if (key_size != 4) {
				sceRegCloseCategory(cat);
				sceRegCloseRegistry(reg);
				return SCE_UTILITY_SYSPARAM_ERROR_INVALID_SIZE;
			}
			if (utilityIsBadRange(out_val, 4)) {
				sceRegCloseCategory(cat);
				sceRegCloseRegistry(reg);
				return SCE_UTILITY_ERROR_INVALID_ADDR;
			}
			ret = sceRegGetKeyValue(cat, key_handle, out_val, 4);
		}
		sceRegCloseCategory(cat);
	}
	sceRegCloseRegistry(reg);
	return ret;
}

static UTILITY_NOINLINE int utilityRegGetNetInt(const char *cat_path, const char *key_name, s32 *out_val)
{
	SceRegParam reg_param;
	u32 reg;
	u32 cat;
	u32 key_handle;
	u32 key_type;
	SceSize key_size;
	int ret;

	memset(&reg_param, 0, sizeof(reg_param));
	reg_param.reg_type = 1;
	reg_param.sys_flag_open = 1;
	reg_param.sys_flag_compat = 1;

	ret = sceRegOpenRegistry(&reg_param, 2, &reg);
	if (ret != 0) {
		Kprintf("couldn't open registry - 0x%x\n", ret);
		return ret;
	}

	ret = sceRegOpenCategory(reg, cat_path, 2, &cat);
	if (ret == 0) {
		ret = sceRegGetKeyInfo(cat, key_name, &key_handle, &key_type, &key_size);
		if (ret == 0) {
			if (key_type != 2 || key_size != 4) {
				sceRegCloseCategory(cat);
				sceRegCloseRegistry(reg);
				return SCE_UTILITY_NETPARAM_ERROR_REG_MISMATCH;
			}
			if (utilityIsBadRange(out_val, 4))
				return SCE_KERNEL_ERROR_INVALID_PTR;
			ret = sceRegGetKeyValue(cat, key_handle, out_val, 4);
		}
		sceRegCloseCategory(cat);
	}
	sceRegCloseRegistry(reg);
	return ret;
}

static UTILITY_NOINLINE int utilityRegSetSysParamInt(const char *cat_path, const char *key_name, s32 value)
{
	SceRegParam reg_param;
	u32 reg;
	u32 cat;
	s32 write_val = value;
	int ret;

	memset(&reg_param, 0, sizeof(reg_param));
	reg_param.reg_type = 1;
	reg_param.sys_flag_open = 1;
	reg_param.sys_flag_compat = 1;

	ret = sceRegOpenRegistry(&reg_param, 2, &reg);
	if (ret != 0) {
		Kprintf("couldn't open registry - 0x%x\n", ret);
		return ret;
	}

	ret = sceRegOpenCategory(reg, cat_path, 2, &cat);
	if (ret == 0) {
		ret = sceRegSetKeyValue(cat, key_name, &write_val, 4);
		if (ret == 0) {
			sceRegFlushCategory(cat);
			sceRegCloseCategory(cat);
			sceRegFlushRegistry(reg);
			sceRegCloseRegistry(reg);
			return 0;
		}
		sceRegCloseCategory(cat);
	}
	sceRegCloseRegistry(reg);
	return ret;
}

static UTILITY_NOINLINE int utilityRegSetNetInt(const char *cat_path, const char *key_name, s32 value)
{
	SceRegParam reg_param;
	u32 reg;
	u32 cat;
	s32 write_val = value;
	int ret;

	memset(&reg_param, 0, sizeof(reg_param));
	reg_param.reg_type = 1;
	reg_param.sys_flag_open = 1;
	reg_param.sys_flag_compat = 1;

	ret = sceRegOpenRegistry(&reg_param, 2, &reg);
	if (ret != 0) {
		Kprintf("couldn't open registry - 0x%x\n", ret);
		return ret;
	}

	ret = sceRegOpenCategory(reg, cat_path, 2, &cat);
	if (ret == 0) {
		ret = sceRegSetKeyValue(cat, key_name, &write_val, 4);
		if (ret == 0) {
			sceRegFlushCategory(cat);
			sceRegCloseCategory(cat);
			sceRegFlushRegistry(reg);
			sceRegCloseRegistry(reg);
			return 0;
		}
		sceRegCloseCategory(cat);
	}
	sceRegCloseRegistry(reg);
	return ret;
}

int sceUtilitySetSystemParamString(int sysparam_id, const char *str_buf)
{
	u32 old_k1 = utilityEnterKernel();
	(void)sysparam_id;
	(void)str_buf;
	utilityLeaveKernel(old_k1);
	return SCE_UTILITY_SYSPARAM_ERROR_INVALID_ID;
}

int sceUtilityGetSystemParamString(int sysparam_id, char *out_buf, s32 buf_size)
{
	u32 old_k1 = utilityEnterKernel();
	int ret;

	if (sysparam_id == 1)
		ret = utilityRegGetSysParamString("/CONFIG/SYSTEM", "owner_name", out_buf, buf_size);
	else
		ret = SCE_UTILITY_SYSPARAM_ERROR_INVALID_ID;

	utilityLeaveKernel(old_k1);
	return ret;
}

static UTILITY_NOINLINE int utilityRegGetSysParamString(const char *cat_path, const char *key_name, char *out_buf, s32 buf_size)
{
	SceRegParam reg_param;
	u32 reg;
	u32 cat;
	u32 key_handle;
	u32 key_type;
	SceSize key_size;
	int ret;

	memset(&reg_param, 0, sizeof(reg_param));
	reg_param.reg_type = 1;
	reg_param.sys_flag_open = 1;
	reg_param.sys_flag_compat = 1;

	ret = sceRegOpenRegistry(&reg_param, 2, &reg);
	if (ret != 0) {
		Kprintf("couldn't open registry - 0x%x\n", ret);
		return ret;
	}

	ret = sceRegOpenCategory(reg, cat_path, 2, &cat);
	if (ret == 0) {
		ret = sceRegGetKeyInfo(cat, key_name, &key_handle, &key_type, &key_size);
		if (ret == 0) {
			if (key_type != 3) {
				sceRegCloseCategory(cat);
				sceRegCloseRegistry(reg);
				return SCE_UTILITY_SYSPARAM_ERROR_INVALID_TYPE;
			}
			if (buf_size < (s32)key_size) {
				sceRegCloseCategory(cat);
				sceRegCloseRegistry(reg);
				return SCE_UTILITY_SYSPARAM_ERROR_INVALID_SIZE;
			}
			if (utilityIsBadSpan(out_buf, (u32)buf_size)) {
				sceRegCloseCategory(cat);
				sceRegCloseRegistry(reg);
				return SCE_UTILITY_ERROR_INVALID_ADDR;
			}
			ret = sceRegGetKeyValue(cat, key_handle, out_buf, (SceSize)buf_size);
			if (ret == 0) {
				if (out_buf[key_size - 1] != '\0') {
					Kprintf("registry entry is not terminated correctly.\n");
					out_buf[key_size - 1] = '\0';
				}
			}
		}
		sceRegCloseCategory(cat);
	}
	sceRegCloseRegistry(reg);
	return ret;
}

static UTILITY_NOINLINE int utilityRegGetMessengerString(const char *cat_path, const char *key_name, char *out_buf, s32 buf_size)
{
	SceRegParam reg_param;
	u32 reg;
	u32 cat;
	u32 key_handle;
	u32 key_type;
	SceSize key_size;
	int ret;

	memset(&reg_param, 0, sizeof(reg_param));
	reg_param.reg_type = 1;
	reg_param.sys_flag_open = 1;
	reg_param.sys_flag_compat = 1;

	ret = sceRegOpenRegistry(&reg_param, 2, &reg);
	if (ret != 0) {
		Kprintf("couldn't open registry - 0x%x\n", ret);
		return ret;
	}

	ret = sceRegOpenCategory(reg, cat_path, 2, &cat);
	if (ret == 0) {
		ret = sceRegGetKeyInfo(cat, key_name, &key_handle, &key_type, &key_size);
		if (ret == 0) {
			if (key_type != 3 || buf_size < (s32)key_size) {
				sceRegCloseCategory(cat);
				sceRegCloseRegistry(reg);
				return SCE_UTILITY_NETPARAM_ERROR_REG_MISMATCH;
			}
			ret = sceRegGetKeyValue(cat, key_handle, out_buf, (SceSize)buf_size);
			if (ret == 0) {
				if (out_buf[key_size - 1] != '\0') {
					Kprintf("registry entry is not terminated correctly.\n");
					out_buf[key_size - 1] = '\0';
				}
			}
		}
		sceRegCloseCategory(cat);
	}
	sceRegCloseRegistry(reg);
	return ret;
}

/* NetParam */
int sceUtilityCheckNetParam(int slot)
{
	u32 old_k1 = utilityEnterKernel();
	SceUtilityNetParamSlotArg arg;
	int ret;

	if (slot < 0) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_NETPARAM_ERROR_INVALID_ID;
	}

	arg.slot = slot;
	arg.result = 0;
	ret = sceKernelExtendKernelStack(4096, utilityNetParamCheckCb, &arg);
	if (ret >= 0)
		ret = arg.result;

	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtility_netparam_internal_6B1BC62C(int slot)
{
	u32 old_k1 = utilityEnterKernel();
	SceUtilityNetParamSlotArg arg;
	int ret;

	if (slot < 0) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_NETPARAM_ERROR_INVALID_ID;
	}

	arg.slot = slot;
	arg.result = 0;
	ret = sceKernelExtendKernelStack(4096, utilityNetParamCreateCb, &arg);
	if (ret >= 0)
		ret = arg.result;

	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtility_netparam_internal_61D0686E(int slot)
{
	u32 old_k1 = utilityEnterKernel();
	SceUtilityNetParamSlotArg arg;
	int ret;

	if (slot < 0) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_NETPARAM_ERROR_INVALID_ID;
	}

	arg.slot = slot;
	arg.result = 0;
	ret = sceKernelExtendKernelStack(4096, utilityNetParamDeleteCb, &arg);
	if (ret >= 0)
		ret = arg.result;

	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtility_netparam_internal_4CB183A4(int src_slot, int dst_slot)
{
	u32 old_k1 = utilityEnterKernel();
	SceUtilityNetParamCopyArg arg;
	int ret;

	if ((src_slot | dst_slot) < 0) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_NETPARAM_ERROR_INVALID_ID;
	}

	arg.src_slot = src_slot;
	arg.dst_slot = dst_slot;
	arg.result = 0;
	ret = sceKernelExtendKernelStack(4096, utilityNetParamCopyCb, &arg);
	if (ret >= 0)
		ret = arg.result;

	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtilityGetNetParam(int slot, int param, void *out_buf)
{
	u32 old_k1 = utilityEnterKernel();
	SceUtilityNetParamGetArg arg;
	int ret;

	if (slot < 0) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_NETPARAM_ERROR_INVALID_ID;
	}
	if (out_buf == NULL) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_NETPARAM_ERROR_INVALID_PTR;
	}
	if (utilityIsBadRange(out_buf, 128)) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_ERROR_INVALID_ADDR;
	}
	if (param >= 32 || (u32)(param - 25) < 7) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_NETPARAM_ERROR_INVALID_KEY;
	}

	arg.slot = slot;
	arg.param = param;
	arg.buf_size = 128;
	arg.out_buf = out_buf;
	arg.result = 0;
	ret = sceKernelExtendKernelStack(4096, utilityNetParamGetCb, &arg);
	if (ret >= 0)
		ret = arg.result;

	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtility_netparam_internal_67C2105B(int slot, int param, void *out_buf)
{
	u32 old_k1 = utilityEnterKernel();
	SceUtilityNetParamGetArg arg;
	int ret;

	if (slot < 0) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_NETPARAM_ERROR_INVALID_ID;
	}
	if (out_buf == NULL) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_NETPARAM_ERROR_INVALID_PTR;
	}
	if (utilityIsBadRange(out_buf, 128)) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_ERROR_INVALID_ADDR;
	}
	if (param >= 32) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_NETPARAM_ERROR_INVALID_KEY;
	}

	arg.slot = slot;
	arg.param = param;
	arg.buf_size = 128;
	arg.out_buf = out_buf;
	arg.result = 0;
	ret = sceKernelExtendKernelStack(4096, utilityNetParamGetCb, &arg);
	if (ret >= 0)
		ret = arg.result;

	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtility_netparam_internal_5D63AA06(int param, const void *value)
{
	u32 old_k1 = utilityEnterKernel();
	SceUtilityNetParamSetArg arg;
	int ret;

	if (value == NULL) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_NETPARAM_ERROR_INVALID_PTR;
	}
	if (utilityIsBadRange(value, 128)) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_ERROR_INVALID_ADDR;
	}
	if (param >= 32 || (u32)(param - 25) < 7) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_NETPARAM_ERROR_INVALID_KEY;
	}

	arg.slot = 0;
	arg.param = param;
	arg.value = value;
	arg.result = 0;
	ret = sceKernelExtendKernelStack(4096, utilityNetParamSetCb, &arg);
	if (ret >= 0)
		ret = arg.result;

	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtility_netparam_internal_239F260D(int param, const void *value)
{
	u32 old_k1 = utilityEnterKernel();
	SceUtilityNetParamSetArg arg;
	int ret;

	if (value == NULL) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_NETPARAM_ERROR_INVALID_PTR;
	}
	if (utilityIsBadRange(value, 128)) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_ERROR_INVALID_ADDR;
	}
	if (param >= 32) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_NETPARAM_ERROR_INVALID_KEY;
	}

	arg.slot = 0;
	arg.param = param;
	arg.value = value;
	arg.result = 0;
	ret = sceKernelExtendKernelStack(4096, utilityNetParamSetCb, &arg);
	if (ret >= 0)
		ret = arg.result;

	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtility_ECE1D3E5(const char *name_buf)
{
	u32 old_k1 = utilityEnterKernel();
	int ret;

	if (name_buf == NULL) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_NETPARAM_ERROR_INVALID_PTR;
	}
	if (utilityIsBadRange(name_buf, 64)) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_ERROR_INVALID_ADDR;
	}

	ret = utilityRegSetMessengerString("/CONFIG/NETWORK/GO_MESSENGER", "auth_name", name_buf, 64);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtility_28D35634(char *out_buf)
{
	u32 old_k1 = utilityEnterKernel();
	int ret;

	if (out_buf == NULL) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_NETPARAM_ERROR_INVALID_PTR;
	}
	if (utilityIsBadRange(out_buf, 64)) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_ERROR_INVALID_ADDR;
	}

	ret = utilityRegGetMessengerString("/CONFIG/NETWORK/GO_MESSENGER", "auth_name", out_buf, 64);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtility_70267ADF(const char *key_buf)
{
	u32 old_k1 = utilityEnterKernel();
	int ret;

	if (key_buf == NULL) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_NETPARAM_ERROR_INVALID_PTR;
	}
	if (utilityIsBadRange(key_buf, 64)) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_ERROR_INVALID_ADDR;
	}

	ret = utilityRegSetMessengerString("/CONFIG/NETWORK/GO_MESSENGER", "auth_key", key_buf, 64);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtility_EF3582B2(char *out_buf)
{
	u32 old_k1 = utilityEnterKernel();
	int ret;

	if (out_buf == NULL) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_NETPARAM_ERROR_INVALID_PTR;
	}
	if (utilityIsBadRange(out_buf, 64)) {
		utilityLeaveKernel(old_k1);
		return SCE_UTILITY_ERROR_INVALID_ADDR;
	}

	ret = utilityRegGetMessengerString("/CONFIG/NETWORK/GO_MESSENGER", "auth_key", out_buf, 64);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtility_netparam_internal_02598DB8(void)
{
	u32 old_k1 = utilityEnterKernel();
	SceUtilityNetParamGetArg get_arg;
	SceUtilityNetParamSlotArg del_arg;
	u32 cfg_buf[32];
	char auth_name[64];
	char auth_key[64];
	int slot;

	for (slot = 0; slot < 10; slot++) {
		get_arg.slot = slot;
		get_arg.param = 24;
		get_arg.buf_size = 128;
		get_arg.out_buf = cfg_buf;
		get_arg.result = 0;
		if (sceKernelExtendKernelStack(4096, utilityNetParamGetCb, &get_arg) >= 0 && cfg_buf[0] == 3) {
			del_arg.slot = slot;
			del_arg.result = 0;
			sceKernelExtendKernelStack(4096, utilityNetParamDeleteCb, &del_arg);
		}
	}

	memset(auth_name, 0, sizeof(auth_name));
	sceUtility_ECE1D3E5(auth_name);
	memset(auth_key, 0, sizeof(auth_key));
	sceUtility_70267ADF(auth_key);

	utilityLeaveKernel(old_k1);
	return 0;
}

int sceUtility_netparam_internal_09905636(void)
{
	u32 old_k1 = utilityEnterKernel();
	SceUtilityNetParamGetArg get_arg;
	SceUtilityNetParamSetArg set_arg;
	s32 dev_buf[32];
	int slot;

	for (slot = 0; slot < 10; slot++) {
		get_arg.slot = slot;
		get_arg.param = 17;
		get_arg.buf_size = 128;
		get_arg.out_buf = dev_buf;
		get_arg.result = 0;
		if (sceKernelExtendKernelStack(4096, utilityNetParamGetCb, &get_arg) >= 0 && dev_buf[0] <= 0) {
			dev_buf[0] = 1;
			set_arg.slot = slot;
			set_arg.param = 17;
			set_arg.value = dev_buf;
			set_arg.result = 0;
			sceKernelExtendKernelStack(4096, utilityNetParamSetCb, &set_arg);
		}
	}

	utilityLeaveKernel(old_k1);
	return 0;
}

int sceUtility_netparam_internal_6D77B975(s32 latest_id)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityRegSetNetInt("/CONFIG/NETWORK/INFRASTRUCTURE", "latest_id", latest_id);
	utilityLeaveKernel(old_k1);
	return ret;
}

int sceUtility_4FED24D8(s32 *out_latest_id)
{
	u32 old_k1 = utilityEnterKernel();
	int ret = utilityRegGetNetInt("/CONFIG/NETWORK/INFRASTRUCTURE", "latest_id", out_latest_id);
	utilityLeaveKernel(old_k1);
	return ret;
}

static UTILITY_NOINLINE int utilityNetParamCheckCb(void *arg_ptr)
{
	SceUtilityNetParamSlotArg *arg = (SceUtilityNetParamSlotArg *)arg_ptr;
	SceUtilityNetParamHandle handle;
	int ret = utilityNetParamOpenRegistry();

	if (ret >= 0) {
		ret = utilityNetParamOpenSlot(arg->slot, &handle, 0);
		if (ret >= 0)
			utilityNetParamCloseSlot(&handle);
	}
	utilityNetParamCloseRegistry();
	arg->result = ret;
	return ret;
}

static UTILITY_NOINLINE int utilityNetParamCreateCb(void *arg_ptr)
{
	SceUtilityNetParamSlotArg *arg = (SceUtilityNetParamSlotArg *)arg_ptr;
	int ret = utilityNetParamOpenRegistry();

	if (ret >= 0)
		ret = utilityNetParamInitSlot(arg->slot);
	utilityNetParamFlushRegistry();
	utilityNetParamCloseRegistry();
	arg->result = ret;
	return ret;
}

static UTILITY_NOINLINE int utilityNetParamDeleteCb(void *arg_ptr)
{
	SceUtilityNetParamSlotArg *arg = (SceUtilityNetParamSlotArg *)arg_ptr;
	int ret = utilityNetParamOpenRegistry();

	if (ret >= 0)
		ret = utilityNetParamRemoveSlot(arg->slot);
	utilityNetParamFlushRegistry();
	utilityNetParamCloseRegistry();
	arg->result = ret;
	return ret;
}

static UTILITY_NOINLINE int utilityNetParamCopyCb(void *arg_ptr)
{
	SceUtilityNetParamCopyArg *arg = (SceUtilityNetParamCopyArg *)arg_ptr;
	SceUtilityNetParamHandle src_handle;
	SceUtilityNetParamHandle dst_handle;
	int opened_dst = 0;
	int ret = utilityNetParamOpenRegistry();

	if (ret >= 0) {
		ret = utilityNetParamOpenSlot(arg->src_slot, &src_handle, 0);
		if (ret >= 0) {
			ret = utilityNetParamOpenSlot(arg->dst_slot, &dst_handle, 0);
			if (ret >= 0) {
				opened_dst = 1;
				ret = utilityNetParamCopyHandle(&src_handle, &dst_handle);
			}
			utilityNetParamCloseSlot(&src_handle);
			if (opened_dst > 0)
				utilityNetParamCloseSlot(&dst_handle);
		}
	}
	utilityNetParamFlushRegistry();
	utilityNetParamCloseRegistry();
	arg->result = ret;
	return ret;
}

static UTILITY_NOINLINE int utilityNetParamGetCb(void *arg_ptr)
{
	SceUtilityNetParamGetArg *arg = (SceUtilityNetParamGetArg *)arg_ptr;
	SceUtilityNetParamHandle handle;
	int ret = utilityNetParamOpenRegistry();

	if (ret >= 0) {
		ret = utilityNetParamOpenSlot(arg->slot, &handle, 0);
		if (ret >= 0) {
			ret = utilityNetParamReadParam(&handle, arg->param, arg->buf_size, arg->out_buf);
			utilityNetParamCloseSlot(&handle);
		}
	}
	utilityNetParamCloseRegistry();
	arg->result = ret;
	return ret;
}

static UTILITY_NOINLINE int utilityNetParamSetCb(void *arg_ptr)
{
	SceUtilityNetParamSetArg *arg = (SceUtilityNetParamSetArg *)arg_ptr;
	SceUtilityNetParamHandle handle;
	int ret = utilityNetParamOpenRegistry();

	if (ret >= 0) {
		ret = utilityNetParamOpenSlot(arg->slot, &handle, 0);
		if (ret >= 0) {
			ret = utilityNetParamWriteParamWrap(&handle, arg->param, arg->value);
			utilityNetParamCloseSlot(&handle);
		}
	}
	utilityNetParamFlushRegistry();
	utilityNetParamCloseRegistry();
	arg->result = ret;
	return ret;
}

static UTILITY_NOINLINE int utilityRegSetMessengerString(const char *cat_path, const char *key_name, const char *str_buf, s32 size)
{
	SceRegParam reg_param;
	u32 reg;
	u32 cat;
	int ret;

	memset(&reg_param, 0, sizeof(reg_param));
	reg_param.reg_type = 1;
	reg_param.sys_flag_open = 1;
	reg_param.sys_flag_compat = 1;

	ret = sceRegOpenRegistry(&reg_param, 2, &reg);
	if (ret != 0) {
		Kprintf("couldn't open registry - 0x%x\n", ret);
		return ret;
	}

	ret = sceRegOpenCategory(reg, cat_path, 2, &cat);
	if (ret == 0) {
		ret = sceRegSetKeyValue(cat, key_name, str_buf, (SceSize)size);
		if (ret == 0) {
			sceRegFlushCategory(cat);
			sceRegCloseCategory(cat);
			sceRegFlushRegistry(reg);
			sceRegCloseRegistry(reg);
			return 0;
		}
		sceRegCloseCategory(cat);
	}
	sceRegCloseRegistry(reg);
	return ret;
}

static UTILITY_NOINLINE int utilityNetParamOpenSlot(int slot, SceUtilityNetParamHandle *handle, int create)
{
	char path[64];
	int ret;

	if (slot < 0)
		return SCE_UTILITY_NETPARAM_ERROR_INVALID_ID;

	snprintf(path, 50, "%s/%d", "/CONFIG/NETWORK/INFRASTRUCTURE", slot);
	ret = utilityNetParamOpenCategory(path, create, &handle->main_cat);
	if (ret < 0)
		return ret;

	snprintf(path, 50, "%s/%d/%s", "/CONFIG/NETWORK/INFRASTRUCTURE", slot, "SUB1");
	ret = utilityNetParamOpenCategory(path, create, &handle->child_cat);
	if (ret == SCE_UTILITY_NETPARAM_ERROR_NOT_FOUND)
		ret = utilityNetParamOpenCategory(path, 1, &handle->child_cat);
	if (ret < 0) {
		utilityNetParamFlushCloseCategory(handle->main_cat);
		return ret;
	}
	return 0;
}

static UTILITY_NOINLINE int utilityNetParamRemoveSlot(int slot)
{
	char path[64];
	int ret;

	if (slot < 0)
		return SCE_UTILITY_NETPARAM_ERROR_INVALID_ID;

	snprintf(path, 50, "%s/%d/%s", "/CONFIG/NETWORK/INFRASTRUCTURE", slot, "SUB1");
	ret = utilityNetParamRemoveCategory(path);
	if ((ret < 0) & (ret != SCE_REG_ERROR_CAT_NOT_FOUND))
		return ret;

	snprintf(path, 50, "%s/%d", "/CONFIG/NETWORK/INFRASTRUCTURE", slot);
	return utilityNetParamRemoveCategory(path);
}

static UTILITY_NOINLINE int utilityNetParamInitSlot(int slot)
{
	SceUtilityNetParamHandle handle;
	int exists = utilityNetParamSlotExists(slot);
	int ret;

	if (exists < 0)
		return exists;
	if (exists > 0)
		return SCE_UTILITY_NETPARAM_ERROR_EXISTS;

	ret = utilityNetParamOpenSlot(slot, &handle, 1);
	if (ret < 0)
		return ret;

	ret = utilityNetParamCreateDefaults(&handle);
	utilityNetParamFlushCloseCategory(handle.child_cat);
	utilityNetParamFlushCloseCategory(handle.main_cat);
	if (ret < 0)
		utilityNetParamRemoveSlot(slot);
	return ret;
}

static UTILITY_NOINLINE int utilityNetParamCopyHandle(SceUtilityNetParamHandle *src, SceUtilityNetParamHandle *dst)
{
	u32 child_scratch[32];
	u32 main_scratch[32];
	int param;
	int ret = 0;

	for (param = 0; param < 32; param++) {
		void *scratch = ((u32)(param - 25) < 7) ? (void *)child_scratch : (void *)main_scratch;
		ret = utilityNetParamCopyKey(src, dst, param, 128, scratch);
		if (ret < 0)
			break;
	}
	return ret;
}

static UTILITY_NOINLINE int utilityNetParamFormatPath(int slot, char *out_buf, u32 buf_size)
{
	if (buf_size < 50)
		return SCE_UTILITY_NETPARAM_ERROR_PATH_SMALL;
	snprintf(out_buf, buf_size, "%s/%d", "/CONFIG/NETWORK/INFRASTRUCTURE", slot);
	return 0;
}

static UTILITY_NOINLINE int utilityNetParamCloseSlot(SceUtilityNetParamHandle *handle)
{
	utilityNetParamFlushCloseCategory(handle->child_cat);
	return utilityNetParamFlushCloseCategory(handle->main_cat);
}

static UTILITY_NOINLINE int utilityNetParamReadParam(SceUtilityNetParamHandle *handle, int param, s32 buf_size, void *out_buf)
{
	u32 cat = ((u32)(param - 25) < 7) ? handle->child_cat : handle->main_cat;
	const SceUtilityNetParamKey *key = &g_netparam_keys[param];
	return utilityNetParamGetRegKey(cat, key->name, key->reg_type, key->key_size, buf_size, out_buf);
}

static UTILITY_NOINLINE int utilityNetParamWriteParamWrap(SceUtilityNetParamHandle *handle, int param, const void *value)
{
	return utilityNetParamWriteParam(handle, param, value);
}

static UTILITY_NOINLINE int utilityNetParamWriteParam(SceUtilityNetParamHandle *handle, int param, const void *value)
{
	u32 cat = ((u32)(param - 25) < 7) ? handle->child_cat : handle->main_cat;
	const SceUtilityNetParamKey *key = &g_netparam_keys[param];
	s32 key_size = key->key_size;
	int ret = utilityNetParamValidateValue(param, (u32)key_size, (const u32 *)value);

	if (ret < 0)
		return ret;
	return utilityNetParamSetRegValue(cat, key->name, key_size, value);
}

static UTILITY_NOINLINE int utilityNetParamCreateDefaults(SceUtilityNetParamHandle *handle)
{
	int ret;

	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[16].name, g_netparam_keys[16].reg_type, g_netparam_keys[16].key_size, 4, &g_netparam_defaults.version);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[17].name, g_netparam_keys[17].reg_type, g_netparam_keys[17].key_size, 4, &g_netparam_defaults.device);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[0].name, g_netparam_keys[0].reg_type, g_netparam_keys[0].key_size, (s32)strlen(g_netparam_defaults.cnf_name) + 1, g_netparam_defaults.cnf_name);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[1].name, g_netparam_keys[1].reg_type, g_netparam_keys[1].key_size, (s32)strlen(g_netparam_defaults.ssid) + 1, g_netparam_defaults.ssid);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[2].name, g_netparam_keys[2].reg_type, g_netparam_keys[2].key_size, 4, &g_netparam_defaults.auth_proto);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[3].name, g_netparam_keys[3].reg_type, g_netparam_keys[3].key_size, g_netparam_defaults.wep_key_len, g_netparam_defaults.wep_key);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[4].name, g_netparam_keys[4].reg_type, g_netparam_keys[4].key_size, 4, &g_netparam_defaults.how_to_set_ip);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[5].name, g_netparam_keys[5].reg_type, g_netparam_keys[5].key_size, (s32)strlen(g_netparam_defaults.ip_address) + 1, g_netparam_defaults.ip_address);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[6].name, g_netparam_keys[6].reg_type, g_netparam_keys[6].key_size, (s32)strlen(g_netparam_defaults.netmask) + 1, g_netparam_defaults.netmask);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[7].name, g_netparam_keys[7].reg_type, g_netparam_keys[7].key_size, (s32)strlen(g_netparam_defaults.default_route) + 1, g_netparam_defaults.default_route);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[8].name, g_netparam_keys[8].reg_type, g_netparam_keys[8].key_size, 4, &g_netparam_defaults.dns_flag);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[9].name, g_netparam_keys[9].reg_type, g_netparam_keys[9].key_size, (s32)strlen(g_netparam_defaults.dns_main) + 1, g_netparam_defaults.dns_main);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[10].name, g_netparam_keys[10].reg_type, g_netparam_keys[10].key_size, (s32)strlen(g_netparam_defaults.dns_alt) + 1, g_netparam_defaults.dns_alt);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[11].name, g_netparam_keys[11].reg_type, g_netparam_keys[11].key_size, (s32)strlen(g_netparam_defaults.auth_name) + 1, g_netparam_defaults.auth_name);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[12].name, g_netparam_keys[12].reg_type, g_netparam_keys[12].key_size, (s32)strlen(g_netparam_defaults.auth_key) + 1, g_netparam_defaults.auth_key);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[13].name, g_netparam_keys[13].reg_type, g_netparam_keys[13].key_size, 4, &g_netparam_defaults.http_proxy_flag);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[14].name, g_netparam_keys[14].reg_type, g_netparam_keys[14].key_size, (s32)strlen(g_netparam_defaults.http_proxy_server) + 1, g_netparam_defaults.http_proxy_server);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[15].name, g_netparam_keys[15].reg_type, g_netparam_keys[15].key_size, 4, &g_netparam_defaults.http_proxy_port);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[18].name, g_netparam_keys[18].reg_type, g_netparam_keys[18].key_size, 4, &g_netparam_defaults.auth_dot1x_type);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[19].name, g_netparam_keys[19].reg_type, g_netparam_keys[19].key_size, (s32)strlen(g_netparam_defaults.auth_dot1x_name) + 1, g_netparam_defaults.auth_dot1x_name);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[20].name, g_netparam_keys[20].reg_type, g_netparam_keys[20].key_size, (s32)strlen(g_netparam_defaults.auth_dot1x_key) + 1, g_netparam_defaults.auth_dot1x_key);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[21].name, g_netparam_keys[21].reg_type, g_netparam_keys[21].key_size, 4, &g_netparam_defaults.wpa_key_type);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[22].name, g_netparam_keys[22].reg_type, g_netparam_keys[22].key_size, 64, g_netparam_defaults.wpa_key);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[23].name, g_netparam_keys[23].reg_type, g_netparam_keys[23].key_size, 4, &g_netparam_defaults.browser_flag);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->main_cat, g_netparam_keys[24].name, g_netparam_keys[24].reg_type, g_netparam_keys[24].key_size, 4, &g_netparam_defaults.wifisvc_config);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->child_cat, g_netparam_keys[25].name, g_netparam_keys[25].reg_type, g_netparam_keys[25].key_size, (s32)strlen(g_netparam_defaults.wifisvc_auth_name) + 1, g_netparam_defaults.wifisvc_auth_name);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->child_cat, g_netparam_keys[26].name, g_netparam_keys[26].reg_type, g_netparam_keys[26].key_size, (s32)strlen(g_netparam_defaults.wifisvc_auth_key) + 1, g_netparam_defaults.wifisvc_auth_key);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->child_cat, g_netparam_keys[27].name, g_netparam_keys[27].reg_type, g_netparam_keys[27].key_size, 4, &g_netparam_defaults.wifisvc_option);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->child_cat, g_netparam_keys[28].name, g_netparam_keys[28].reg_type, g_netparam_keys[28].key_size, (s32)strlen(g_netparam_defaults.last_leased_dhcp) + 1, g_netparam_defaults.last_leased_dhcp);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->child_cat, g_netparam_keys[29].name, g_netparam_keys[29].reg_type, g_netparam_keys[29].key_size, 4, &g_netparam_defaults.bt_id);
	if (ret < 0) return ret;
	ret = utilityNetParamCreateAndSetKey(handle->child_cat, g_netparam_keys[30].name, g_netparam_keys[30].reg_type, g_netparam_keys[30].key_size, (s32)strlen(g_netparam_defaults.at_command) + 1, g_netparam_defaults.at_command);
	if (ret < 0) return ret;
	return utilityNetParamCreateAndSetKey(handle->child_cat, g_netparam_keys[31].name, g_netparam_keys[31].reg_type, g_netparam_keys[31].key_size, (s32)strlen(g_netparam_defaults.phone_number) + 1, g_netparam_defaults.phone_number);
}

static UTILITY_NOINLINE int utilityNetParamCopyKey(SceUtilityNetParamHandle *src, SceUtilityNetParamHandle *dst, int param, s32 buf_size, void *scratch)
{
	u32 src_cat = ((u32)(param - 25) < 7) ? src->child_cat : src->main_cat;
	const SceUtilityNetParamKey *key = &g_netparam_keys[param];
	int ret;

	ret = utilityNetParamKeyExists(src_cat, key->name);
	if (ret <= 0)
		return ret;

	ret = utilityNetParamGetRegKey(src_cat, key->name, key->reg_type, key->key_size, buf_size, scratch);
	if (ret < 0)
		return ret;

	if (param == 16) {
		s32 dst_ver[32];
		ret = utilityNetParamGetRegKey(dst->main_cat, "version", 2, 4, 128, dst_ver);
		if (ret < 0 || *(const s32 *)scratch < dst_ver[0])
			return ret;
	}
	return utilityNetParamUpsertKey(dst, param, scratch);
}

static UTILITY_NOINLINE int utilityNetParamValidateValue(int param, u32 key_size, const u32 *value)
{
	switch (param) {
	case 2:
		if (*value >= 5)
			return SCE_UTILITY_NETPARAM_ERROR_INVALID_VAL;
		break;
	case 3:
		if (key_size != 5 && key_size != 13)
			return SCE_UTILITY_NETPARAM_ERROR_INVALID_VAL;
		break;
	case 4:
		if (*value >= 3)
			return SCE_UTILITY_NETPARAM_ERROR_INVALID_VAL;
		break;
	case 8:
	case 13:
	case 18:
	case 21:
	case 23:
		if (*value >= 2)
			return SCE_UTILITY_NETPARAM_ERROR_INVALID_VAL;
		break;
	case 15:
		if (*value - 1u > 0xFFFEu)
			return SCE_UTILITY_NETPARAM_ERROR_INVALID_VAL;
		break;
	case 17:
		if (*value - 1u >= 2)
			return SCE_UTILITY_NETPARAM_ERROR_INVALID_VAL;
		break;
	case 22:
		if (key_size < 9)
			return SCE_UTILITY_NETPARAM_ERROR_INVALID_VAL;
		break;
	case 24:
		if (*value >= 4)
			return SCE_UTILITY_NETPARAM_ERROR_INVALID_VAL;
		break;
	case 27:
		if ((*value & ~1u) != 0)
			return SCE_UTILITY_NETPARAM_ERROR_INVALID_VAL;
		break;
	default:
		break;
	}
	return 0;
}

static UTILITY_NOINLINE int utilityNetParamUpsertKey(SceUtilityNetParamHandle *handle, int param, const void *value)
{
	u32 cat = ((u32)(param - 25) < 7) ? handle->child_cat : handle->main_cat;
	const SceUtilityNetParamKey *key = &g_netparam_keys[param];
	s32 key_size = key->key_size;
	int ret = utilityNetParamValidateValue(param, (u32)key_size, (const u32 *)value);

	if (ret < 0)
		return ret;
	return utilityNetParamSetOrCreateKey(cat, key->name, key->reg_type, key_size, key_size, value);
}

static UTILITY_NOINLINE int utilityNetParamSlotExists(int slot)
{
	char path[64];
	u32 cat;
	int ret = utilityNetParamFormatPath(slot, path, 50);

	if (ret < 0)
		return ret;

	ret = sceRegOpenCategory(g_netparam_reg_handle, path, 2, &cat);
	if (ret == 0) {
		sceRegCloseCategory(cat);
		return 1;
	}
	if (ret == SCE_REG_ERROR_CAT_NOT_FOUND)
		return 0;
	return ret;
}

static UTILITY_NOINLINE int utilityNetParamKeyExists(u32 cat, const char *key_name)
{
	u32 key_type;
	SceSize key_size;
	int ret = sceRegGetKeyInfoByName(cat, key_name, &key_type, &key_size);

	if (ret == 0)
		return 1;
	if (ret == SCE_REG_ERROR_KEY_NOT_FOUND)
		return 0;
	return ret;
}

static UTILITY_NOINLINE int utilityNetParamGetRegKey(u32 cat, const char *key_name, u32 expected_type, s32 max_key_size, s32 buf_size, void *out_buf)
{
	u32 key_type;
	SceSize key_size;
	int ret = sceRegGetKeyInfoByName(cat, key_name, &key_type, &key_size);

	if (ret < 0)
		return ret;
	if (key_type != expected_type)
		return SCE_UTILITY_NETPARAM_ERROR_TYPE_MISMATCH;
	if (max_key_size < (s32)key_size)
		return SCE_UTILITY_NETPARAM_ERROR_KEY_TOO_BIG;
	if (buf_size < (s32)key_size)
		return SCE_UTILITY_NETPARAM_ERROR_BUF_TOO_SMALL;
	return sceRegGetKeyValueByName(cat, key_name, out_buf, (SceSize)buf_size);
}

static UTILITY_NOINLINE int utilityNetParamSetOrCreateKey(u32 cat, const char *key_name, u32 key_type, s32 key_size, s32 write_size, const void *value)
{
	u32 existing_type;
	SceSize existing_size;
	int ret = sceRegGetKeyInfoByName(cat, key_name, &existing_type, &existing_size);

	if (ret == 0)
		return sceRegSetKeyValue(cat, key_name, value, (SceSize)write_size);
	if (ret == SCE_REG_ERROR_KEY_NOT_FOUND) {
		ret = sceRegCreateKey(cat, key_name, (int)key_type, (SceSize)key_size);
		if (ret >= 0)
			return sceRegSetKeyValue(cat, key_name, value, (SceSize)write_size);
	}
	return ret;
}

static UTILITY_NOINLINE int utilityNetParamOpenCategory(const char *path, int create, u32 *out_cat)
{
	int access_mode = (create > 0) ? 1 : 2;
	int ret = sceRegOpenCategory(g_netparam_reg_handle, path, access_mode, out_cat);

	if ((create == 0) & (ret == SCE_REG_ERROR_CAT_NOT_FOUND))
		return SCE_UTILITY_NETPARAM_ERROR_NOT_FOUND;
	return ret;
}

static UTILITY_NOINLINE int utilityNetParamOpenRegistry(void)
{
	SceRegParam reg_param;
	u32 reg;
	int ret;

	memset(&reg_param, 0, sizeof(reg_param));
	reg_param.reg_type = 1;
	strncpy(reg_param.name, "flash2/registry/system", sizeof(reg_param.name));
	reg_param.name_len = 22;
	reg_param.sys_flag_open = 1;
	reg_param.sys_flag_compat = 1;

	ret = sceRegOpenRegistry(&reg_param, 2, &reg);
	if (ret >= 0) {
		g_netparam_reg_handle = reg;
		g_netparam_reg_refs++;
	}
	return ret;
}

static UTILITY_NOINLINE int utilityNetParamCloseRegistry(void)
{
	if (g_netparam_reg_refs > 0) {
		sceRegFlushRegistry(g_netparam_reg_handle);
		sceRegCloseRegistry(g_netparam_reg_handle);
		g_netparam_reg_refs--;
	}
	return 0;
}

static UTILITY_NOINLINE int utilityNetParamFlushCloseCategory(u32 cat)
{
	sceRegFlushCategory(cat);
	sceRegCloseCategory(cat);
	return 0;
}

static UTILITY_NOINLINE int utilityNetParamFlushRegistry(void)
{
	sceRegFlushRegistry(g_netparam_reg_handle);
	return 0;
}

static UTILITY_NOINLINE int utilityNetParamSetRegValue(u32 cat, const char *key_name, s32 size, const void *value)
{
	return sceRegSetKeyValue(cat, key_name, value, (SceSize)size);
}

static UTILITY_NOINLINE int utilityNetParamCreateAndSetKey(u32 cat, const char *key_name, u32 key_type, s32 key_size, s32 write_size, const void *value)
{
	int ret = sceRegCreateKey(cat, key_name, (int)key_type, (SceSize)key_size);
	if (ret >= 0)
		return sceRegSetKeyValue(cat, key_name, value, (SceSize)write_size);
	return ret;
}

static UTILITY_NOINLINE int utilityNetParamRemoveCategory(const char *path)
{
	return sceRegRemoveCategory(g_netparam_reg_handle, path);
}

/* Game compatibility and Kermit peripheral helpers */
static UTILITY_NOINLINE void utilityCompatMatchGame(u32 dialog_type, u32 dialog_submode)
{
	SceKernelGameInfo *game_info = (SceKernelGameInfo *)sceKernelGetGameInfo();
	const char *code = game_info->product_code;
	int idx;

	g_active_compat = NULL;
	for (idx = 0; idx < 17; idx++) {
		if (g_game_compat_table[idx].dialog_type == dialog_type &&
		    g_game_compat_table[idx].dialog_submode == dialog_submode) {
			if (strcmp(code, g_game_compat_table[idx].product_code) == 0)
				g_active_compat = &g_game_compat_table[idx];
		}
	}
}

static UTILITY_NOINLINE int utilityKermitSetDialogActive(int active)
{
	u32 old_k1 = utilityEnterKernel();
	u8 req_buf[128];
	u32 aligned = ((u32)req_buf + 63u) & ~63u;
	SceKermitRequest *req;
	u64 resp;
	int ret;

	sceKernelDcacheInvalidateRange((void *)aligned, 64);
	req = KERMIT_UNCACHED(aligned);
	req->cmd = 1063;
	req->argWords[0].lo = (u32)active;
	ret = sceKermit_driver_36666181(req, KERMIT_MODE_PERIPHERAL, 1063, 1, 0, &resp);
	if (ret == 0)
		ret = (int)resp;

	utilityLeaveKernel(old_k1);
	return ret;
}

u64 sceUtility_private_74E69D03(int cap_index)
{
	if (cap_index >= 0) {
		if (cap_index < 3)
			return g_kermit_caps[cap_index];
	}
	return 0;
}

static UTILITY_NOINLINE int utilityKermitFetchCapabilities(void)
{
	int idx;
	for (idx = 0; idx < 3; idx++)
		g_kermit_caps[idx] = utilityKermitQueryCapability(idx);
	return 0;
}

static UTILITY_NOINLINE void utilityCompatClear(void)
{
	g_active_compat = NULL;
}

static UTILITY_NOINLINE u32 utilityCompatGetFlags(void)
{
	if (g_active_compat != NULL)
		return g_active_compat->flags;
	return 0;
}

static UTILITY_NOINLINE int utilityCompatDelay(u32 dialog_type, int delay_slot)
{
	(void)dialog_type;
	if (g_active_compat != NULL && g_active_compat->delays[delay_slot] != 0)
		return sceKernelDelayThread(g_active_compat->delays[delay_slot]);
	return delay_slot * 4;
}

static UTILITY_NOINLINE u64 utilityKermitQueryCapability(int cap_index)
{
	u32 old_k1 = utilityEnterKernel();
	u8 req_buf[128];
	u32 aligned = ((u32)req_buf + 63u) & ~63u;
	SceKermitRequest *req;
	u64 resp;
	int ret;

	sceKernelDcacheInvalidateRange((void *)aligned, 64);
	req = KERMIT_UNCACHED(aligned);
	req->cmd = 1042;
	req->argWords[0].lo = (u32)cap_index;
	ret = sceKermit_driver_36666181(req, KERMIT_MODE_PERIPHERAL, 1042, 1, 0, &resp);
	utilityLeaveKernel(old_k1);
	if (ret == 0)
		return resp;
	return 0;
}

const SceUtilityNetParamKey g_netparam_keys[32] = {
	{ 3,  64, "cnf_name" },
	{ 3,  33, "ssid" },
	{ 2,   4, "auth_proto" },
	{ 4,  13, "wep_key" },
	{ 2,   4, "how_to_set_ip" },
	{ 3,  16, "ip_address" },
	{ 3,  16, "netmask" },
	{ 3,  16, "default_route" },
	{ 2,   4, "dns_flag" },
	{ 3,  16, "primary_dns" },
	{ 3,  16, "secondary_dns" },
	{ 3, 128, "auth_name" },
	{ 3, 128, "auth_key" },
	{ 2,   4, "http_proxy_flag" },
	{ 3, 128, "http_proxy_server" },
	{ 2,   4, "http_proxy_port" },
	{ 2,   4, "version" },
	{ 2,   4, "device" },
	{ 2,   4, "auth_8021x_type" },
	{ 3, 128, "auth_8021x_auth_name" },
	{ 3, 128, "auth_8021x_auth_key" },
	{ 2,   4, "wpa_key_type" },
	{ 4,  64, "wpa_key" },
	{ 2,   4, "browser_flag" },
	{ 2,   4, "wifisvc_config" },
	{ 3, 128, "wifisvc_auth_name" },
	{ 3, 128, "wifisvc_auth_key" },
	{ 2,   4, "wifisvc_option" },
	{ 3,  16, "last_leased_dhcp_addr" },
	{ 2,   4, "bt_id" },
	{ 3,  65, "at_command" },
	{ 3,  33, "phone_number" },
};



static const char g_netparam_empty_str[] = "";

const SceUtilityNetParamDefaults g_netparam_defaults = {
	g_netparam_empty_str,
	g_netparam_empty_str,
	0,
	{ 0 },
	5,
	0,
	g_netparam_empty_str,
	g_netparam_empty_str,
	g_netparam_empty_str,
	0,
	g_netparam_empty_str,
	g_netparam_empty_str,
	g_netparam_empty_str,
	g_netparam_empty_str,
	0,
	g_netparam_empty_str,
	8080,
	5,
	1,
	0,
	g_netparam_empty_str,
	g_netparam_empty_str,
	0,
	{ 0 },
	0,
	0,
	g_netparam_empty_str,
	g_netparam_empty_str,
	0,
	g_netparam_empty_str,
	0,
	g_netparam_empty_str,
	g_netparam_empty_str,
};
