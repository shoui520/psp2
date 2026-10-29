#include <kermit.h>

#define SCE_ERROR_NOT_FOUND		0x80000004
#define SCE_ERROR_ALREADY		0x80000020
#define SCE_ERROR_NO_MEMORY		0x80000022
#define SCE_ERROR_PRIV_REQUIRED		0x80000023
#define SCE_ERROR_NOT_REGISTERED	0x80000025
#define SCE_ERROR_INVALID_ID		0x80000100
#define SCE_ERROR_INVALID_INDEX		0x80000102
#define SCE_ERROR_INVALID_MODE		0x80000107
#define SCE_ERROR_INVALID_VALUE		0x800001FE

#define SCE_POWER_ERROR_NO_BATTERY	0x802B0100
#define SCE_POWER_ERROR_DETECTING	0x802B0101
#define SCE_POWER_ERROR_VMEM_IN_USE	0x802B0200
#define SCE_POWER_ERROR_CLOCK_INVALID	0x802B0300

typedef struct {
	u8 pad[0x16];
	u16 attr;
} SceUidControlBlock;

typedef struct {
	SceUID cbId;
	u32 flags4;
	u32 flags8;
	u32 flagsC;
} ScePowerCallbackSlot;

typedef struct {
	ScePowerCallbackSlot slots[32];	/* 0x000..0x1ff */
	u32 statusFlags;		/* 0x200 */
	u32 statusMask;			/* 0x204 */
	u8 pad208;			/* 0x208 */
	u8 unk209;			/* 0x209 (0x5759) */
	u8 unk20a;			/* 0x20a (0x575a) */
	u8 unk20b;			/* 0x20b (0x575b) */
	u8 unk20c;			/* 0x20c (0x575c) */
	u8 unk20d;			/* 0x20d (0x575d) */
	u8 unk20e;			/* 0x20e (0x575e) */
	u8 pad20f[3];			/* 0x20f..0x211 */
	u16 initPllFreq;		/* 0x212 */
	u16 initBusFreq;		/* 0x214 */
	u16 initCpuFreq;		/* 0x216 */
} ScePowerCallbackContext;

typedef struct {
	SceUID evfId;			/* 0x00 */
	SceUID vmemSemaId;		/* 0x04 */
	SceUID mainThreadId;		/* 0x08 */
	u32 swFlags;			/* 0x0c */
	u32 suspending;			/* 0x10 */
	s32 userLockCount;		/* 0x14 */
	s32 kernelLockCount;		/* 0x18 */
	u32 vmemAddr;			/* 0x1c */
	u32 vmemSize;			/* 0x20 */
	u32 vmemLocked;			/* 0x24 */
	u32 reqState1;			/* 0x28 */
	u32 reqState2;			/* 0x2c */
	u32 shutdownLocked;		/* 0x30 */
	u32 coldResetMode;		/* 0x34 */
	u32 pad38;			/* 0x38 */
	u32 param3c;			/* 0x3c */
	u32 resumeCount;		/* 0x40 */
} ScePowerSuspendContext;

typedef struct {
	u32 state0;			/* 0x00 (offset +0x10 in ctx) */
	s32 state4;			/* 0x04 (offset +0x14 in ctx) */
	u64 startTime;			/* 0x08 (offset +0x18 in ctx) */
	u64 timeout;			/* 0x10 (offset +0x20 in ctx) */
	u32 cbArg;			/* 0x18 (offset +0x28 in ctx) */
	void (*cbFunc)(int slot, u32 diff, u32 opt); /* 0x1c (offset +0x2c in ctx) */
	u32 (*gpFunc)(void);		/* 0x20 (offset +0x30 in ctx) */
	u32 userOpt;			/* 0x24 (offset +0x34 in ctx) */
} ScePowerIdleSlot;

typedef struct {
	u64 lastSysTime;		/* 0x00 */
	u32 enabledMask;		/* 0x08 */
	u32 pad0c;			/* 0x0c */
	ScePowerIdleSlot slots[8];	/* 0x10..0x14f */
} ScePowerIdleTimerContext;

typedef struct {
	SceUID mutexId;			/* 0x00 (0xae5c) */
	void *sm1Ops;			/* 0x04 (0xae60) */
	s32 pllOutSelect;		/* 0x08 (0xae64) */
	u32 pllMask;			/* 0x0c (0xae68) */
	s32 pllFreqInt;			/* 0x10 (0xae6c) */
	s32 cpuFreqInt;			/* 0x14 (0xae70) */
	s32 busFreqInt;			/* 0x18 (0xae74) */
	float pllFreqFloat;		/* 0x1c (0xae78) */
	float cpuFreqFloat;		/* 0x20 (0xae7c) */
	float busFreqFloat;		/* 0x24 (0xae80) */
	u32 savedSysreg[4];		/* 0x28..0x37 (0xae84..0xae93) */
	s32 mode38;			/* 0x38 (0xae94) */
	s16 val3c;			/* 0x3c (0xae98) */
	s16 val3e;			/* 0x3e (0xae9a) */
	s32 mode40;			/* 0x40 (0xae9c) */
	s16 val44;			/* 0x44 (0xaea0) */
	s16 val46;			/* 0x46 (0xaea2) */
	s32 mode48;			/* 0x48 (0xaea4) */
	s16 val4c;			/* 0x4c (0xaea8) */
	s16 val4e;			/* 0x4e (0xaeaa) */
	s32 geRefreshMode;		/* 0x50 (0xaeac) */
	s32 savedGeRefreshMode;		/* 0x54 (0xaeb0) */
	s16 pad58;			/* 0x58 (0xaeb4) */
	s16 cpuMin;			/* 0x5a (0xaeb6) */
	s16 cpuMax;			/* 0x5c (0xaeb8) */
	s16 busMin;			/* 0x5e (0xaeba) */
	s16 busMax;			/* 0x60 (0xaebc) */
	s16 pllMin;			/* 0x62 (0xaebe) */
	s16 pllMax;			/* 0x64 (0xaec0) */
	s16 pad66;			/* 0x66 (0xaec2) */
} ScePowerClockContext;

typedef struct {
	u32 unk00;			/* 0x00 (0xaec4) */
	u32 unk04;			/* 0x04 (0xaec8) */
	u32 unk08;			/* 0x08 (0xaecc) */
	u32 unk0c;			/* 0x0c (0xaed0) */
	u32 unk10;			/* 0x10 (0xaed4) */
	u32 unk14;			/* 0x14 (0xaed8) */
	u32 unk18;			/* 0x18 (0xaedc) */
	SceUID alarmId;			/* 0x1c (0xaee0) */
	u32 unk20;			/* 0x20 (0xaee4) */
	u32 disabledFlag;		/* 0x24 (0xaee8) */
	u32 unk28;			/* 0x28 (0xaeec) */
	u32 unk2c;			/* 0x2c (0xaef0) */
	s32 unk30;			/* 0x30 (0xaef4) */
	u32 status34;			/* 0x34 (0xaef8) */
	u32 state38;			/* 0x38 (0xaefc) */
	u32 unk3c;			/* 0x3c (0xaf00) */
	s32 remainCap;			/* 0x40 (0xaf04) */
	s32 lifePercent;		/* 0x44 (0xaf08) */
	s32 fullCap;			/* 0x48 (0xaf0c) */
	s32 unk4c;			/* 0x4c (0xaf10) */
	s32 cycleCount;			/* 0x50 (0xaf14) */
	s32 unk54;			/* 0x54 (0xaf18) */
	s32 temp;			/* 0x58 (0xaf1c) */
	s32 elec;			/* 0x5c (0xaf20) */
	s32 volt;			/* 0x60 (0xaf24) */
	u32 pad64[27];			/* 0x64..0xcf */
} ScePowerBatteryContext;

typedef struct {
	s32 freq;
	s32 pllSel;
} ScePowerPllEntry;

typedef struct {
	u32 size;
	void *tick;
	void *lockKernel;
	void *unlockKernel;
	void *lockUser;
	void *unlockUser;
	void *lockShutdown;
	void *vmemLock;
	void *vmemTryLock;
	void *vmemUnlock;
} ScePowerHandlers;

static inline int pspShiftK1(void)
{
	u32 oldK1 = pspGetK1();
	pspSetK1(oldK1 << 11);
	return (int)oldK1;
}

static inline int pspK1PtrOk(const void *ptr)
{
	return ((s32)(pspGetK1() & (u32)ptr)) >= 0;
}

void *memset(void *dst, int c, SceSize len);
s64 sceKernelGetSystemTimeWide(void);
int sceKernelCpuResumeIntrWithSync(int state);
int sceKernelGetModel(void);
int sceKernelSetDdrMemoryProtection(u32 addr, u32 size, int prot);
int sceKernelGetCompiledSdkVersion(void);
int sceKernelQueryMemoryPartitionInfo(int pid, void *info);
void *sceKernelGetGameInfo(void);
int sceKernelDispatchResumeHandlers(int unk);
int sceSuspendForKernel_67B59042(int unk);
int sceKernelDispatchSuspendHandlers(int unk);
int sceSuspendForKernel_B2C9640B(int unk);
int sceKernelRegisterPowerHandlers(const ScePowerHandlers *handlers);
int sceKernelSysEventDispatch(int type_mask, int ev_id, const char *ev_name,
			      void *param, int *result, int break_nonzero,
			      SceSysEventHandler **broken_handler);
int sceKernelWaitThreadEnd(SceUID thid, SceUInt *timeout);
int sceKernelGetThreadId(void);
int sceKernelGetCallbackCount(SceUID cbid);
int sceKernelPollEventFlag(SceUID evid, u32 bits, u32 wait, u32 *out_bits);
int sceKernelGetThreadExitStatus(SceUID thid);
SceUID sceKernelCreateThread(const char *name, int (*entry)(SceSize, void *),
			     int init_pri, int stack_size, SceUInt attr, void *opt);
int sceKernelCancelAlarm(SceUID alarmid);
int sceKernelStartThread(SceUID thid, SceSize arglen, void *argp);
int sceKernelGetUserLevel(void);
int UtilsForKernel_39FFB756(int arg);
void sceKernelDcacheWritebackAll(void);
int sceKernelUtilsSha1Digest(const void *data, u32 size, u8 *digest);
void UtilsForKernel_A6B0A6B8(void);
int sceKernelApplicationType(void);
int sceKernelSetInitCallback(void *func, int flag, int *out);
void *sceKernelInitParamSfo(void *buf);
int sceKernelDipsw(int bit);
void *sceKernelSm1ReferOperations(void);
int sceSysregGpioIoDisable(int port);
int sceSysregPllGetOutSelect(void);
int sceSysreg_driver_2458B6AC(u32 *a, u32 *b);
int sceSysreg_driver_3F6F2CC7(int a, int b);
u64 sceSysregGetFuseId(void);
int sceSysreg_driver_63E1EE9C(int a, int b);
float sceSysreg_driver_73EBC752(void);
int sceSysreg_driver_7CF05E81(u32 *a, u32 *b);
int sceSysreg_driver_85B74FDA(float freq);
u32 sceSysregGetFuseConfig(void);
float sceSysreg_driver_96D74557(void);
int sceSysregSpiIoDisable(int port);
float sceSysregPllGetFrequency(void);
int sceSysreg_driver_B413B041(float freq);
int sceSysregUartIoDisable(int port);
u32 sceSysregGetTachyonVersion(void);
int sceDdr_driver_00E36648(void);
int sceDdr_driver_0BAAE4C5(void);
int sceDdr_driver_CF07B9E2(int pllSel);
int sceDdr_driver_E0A39D3E(int arg);
int sceGeEdramSetRefreshParam(int mode, int arg1, int arg2, int arg3);
int sceCtrlSetButtonIntercept(int mask, int mode);
int sceDisplayIsVblank(void);
int sceDisplayWaitVblankStart(void);
int sceDisplay_driver_CE8A328E(void);
int sceDisplayAdjustAccumulatedHcount(int hcount);
int sceDisplayGetCurrentHcount(void);
int sceDisplayGetAccumulatedHcount(void);
int sceKermitPeripheral_driver_0648E1A3(void);
int sceKermitPeripheral_driver_15F5C107(void);
int sceKermitPeripheral_driver_65D98A21(void);
int sceKermitPeripheral_driver_6D6AA215(void);

s32 scePower_driver_9CE06934(void);
s32 scePowerRegisterCallback(int slot, SceUID cbId);
s32 scePowerUnregisterCallback(int slot);
s32 scePower_A9D22232(int slot, u32 flagsC);
s32 scePower_BAFA3DF0(int slot, u32 *outFlagsC);
s32 scePower_driver_2638EF48(void);
s32 scePower_442BFBAC(void);
s32 scePower_driver_AD5BB433(void);
s32 scePower_driver_23BDDD8B(void);
s32 scePower_A85880D0(void);
s32 scePower_driver_114B75AB(u8 val);
s32 scePower_driver_E52B4362(int freq);
s32 scePower_driver_8C6BEFD9(void);
u8 scePower_driver_4E32E9B8(void);
u8 scePower_2B51FE2F(void);
u8 scePower_driver_C463E7F2(void);
s32 scePower_23C31FFE(int mode, u32 *outAddr, u32 *outSize);
s32 scePower_FA97A599(int mode, u32 *outAddr, u32 *outSize);
s32 scePower_B3EDD801(int mode);
s32 scePowerLock(int mode);
s32 scePower_driver_6CF50928(int mode);
s32 scePowerUnlock(int mode);
s32 scePower_driver_C3024FE6(int mode);
s32 scePower_3951AF53(void);
s32 scePower_0CD21B1F(int mode);
s32 scePower_165CE085(void);
s32 scePower_driver_1EC2D4E4(void);
s32 scePower_7FA406DD(void);
u32 scePower_DB62C9CF(void);
s32 scePowerRequestStandby(void);
s32 scePowerRequestSuspend(void);
s32 scePower_driver_D79B0122(void);
s32 scePower_2875994B(void);
s32 scePowerRequestColdReset(int mode);
u32 scePower_0074EF9B(void);
s32 scePower_driver_BA566CD0(u32 val);
s32 scePowerGetIdleTimer(int slot, u64 *elapsedOut, u64 *remainOut);
s32 scePower_driver_1BA2FCAE(int slot, u32 cbArg, u64 timeout,
			     void (*cbFunc)(int, u32, u32), u32 userOpt);
s32 scePowerTick(u32 flags);
s32 scePowerIdleTimerEnable(int slot);
s32 scePowerIdleTimerDisable(int slot);
s32 scePowerSetCpuClockFrequency(int cpufreq);
s32 scePowerSetBusClockFrequency(int busfreq);
s32 scePower_driver_100A77A1(int mode);
s32 scePower_driver_C520F5DC(void);
s32 scePower_driver_D7DD9D38(void);
s32 scePower_driver_BADA8332(s32 *out0, s32 *out1);
s32 scePower_driver_12F8302D(s32 a0, s32 a1);
s32 scePower_driver_9127E5B2(void);
s32 scePower_driver_75906F9A(s32 *out0, s32 *out1);
s32 scePower_driver_018AB235(s32 a0, s32 a1);
s32 scePower_driver_0655D7C3(void);
s32 scePower_driver_16F965C9(s32 *out0, s32 *out1);
s32 scePower_driver_D13377F7(s32 a0, s32 a1);
s32 scePower_driver_DF904CDE(s32 a0, s32 a1);
s32 scePower_driver_EEFB2ACF(s32 a0, s32 a1);
s32 scePower_driver_B7000C75(s32 a0, s32 a1);
s32 scePower_driver_13D7CCE4(u32 mask);
s32 scePowerGetCpuClockFrequencyInt(void);
float scePowerGetCpuClockFrequencyFloat(void);
s32 scePowerGetBusClockFrequencyInt(void);
float scePowerGetBusClockFrequencyFloat(void);
s32 scePower_34F9C463(void);
float scePower_EA382A27(void);
s32 scePowerSetClockFrequency(int pllfreq, int cpufreq, int busfreq);
s32 scePower_A4E93389(int pllfreq, int cpufreq, int busfreq);
s32 scePower_545A7F3C(int pllfreq, int cpufreq, int busfreq);
s32 scePower_EBD177D6(int pllfreq, int cpufreq, int busfreq);
s32 scePower_469989AD(int pllfreq, int cpufreq, int busfreq);
s32 scePowerGetBatteryLifePercent(void);
s32 scePowerGetBatteryTemp(void);
s32 scePowerGetBatteryElec(s32 *outElec);
s32 scePower_CB49F5CE(void);
s32 scePower_27F3292C(void);
s32 scePower_E8E4E204(void);
s32 scePower_B999184C(void);
s32 scePower_driver_5F5006D2(void);
s32 scePower_driver_10CE273F(void);
s32 scePower_driver_EF751B4A(void);
s32 scePower_driver_72D1B53A(void);
s32 scePower_driver_7EAA4247(void);
s32 scePowerIsPowerOnline(void);
s32 scePowerIsBatteryExist(void);
s32 scePowerIsBatteryCharging(void);
s32 scePowerGetBatteryChargingStatus(void);
s32 scePowerIsLowBattery(void);
s32 scePowerIsSuspendRequired(void);
u32 scePower_driver_071160B1(void);
s32 scePowerGetBatteryRemainCapacity(void);
s32 scePowerGetBatteryFullCapacity(void);
s32 scePowerGetBatteryLifeTime(void);
s32 scePowerGetBatteryVolt(void);
s32 scePower_23436A4A(void);

static int __attribute__((noipa)) powerSysEventHandler(int ev_id, char *ev_name, void *param, int *result);
static s32 __attribute__((noipa)) initSuspendSubsys(void);
static s32 __attribute__((noipa)) exitSuspendSubsys(void);
static s32 __attribute__((noipa)) initIdleSubsys(void);
static s32 __attribute__((noipa)) exitIdleSubsys(void);
static s32 __attribute__((noipa)) initClockSubsys(void);
static s32 __attribute__((noipa)) exitClockSubsys(void);
static s32 __attribute__((noipa)) lockClockMutex(void);
static s32 __attribute__((noipa)) unlockClockMutex(void);
static s32 __attribute__((noipa)) clockRebootPhase(int phase);
static s32 __attribute__((noipa)) changeClockFrequency(int pllfreq, int cpufreq, int busfreq);
static s32 __attribute__((noipa)) initBatterySubsys(u32 a0, u32 a1);
static s32 __attribute__((noipa)) exitBatterySubsys(void);
static s32 __attribute__((noipa)) setBatteryThresholds(u32 a0, u32 a1);
static s32 __attribute__((noipa)) resetBatteryState(void);
static s32 __attribute__((noipa)) updateBatteryEvent(const u32 *evData, u32 *statusFlags);

static const ScePowerHandlers g_power_handlers = {
	sizeof(ScePowerHandlers),
	scePowerTick,
	scePower_driver_6CF50928,
	scePower_driver_C3024FE6,
	scePowerLock,
	scePowerUnlock,
	scePower_driver_1EC2D4E4,
	scePower_23C31FFE,
	scePower_FA97A599,
	scePower_B3EDD801
};

static const ScePowerPllEntry g_pll_table[12] = {
	{  19,  8 }, {  37,  0 }, {  74,  9 }, {  95, 10 },
	{ 111, 11 }, { 133, 12 }, { 148,  1 }, { 166, 13 },
	{ 190,  2 }, { 222,  3 }, { 266,  4 }, { 333,  5 }
};

static u32 g_init_clocks[3] = { 333, 166, 333 };

static SceSysEventHandler g_power_sys_event = {
	sizeof(SceSysEventHandler),
	"ScePower",
	0x00ffff00,
	powerSysEventHandler,
	0,
	0,
	NULL,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

static ScePowerCallbackContext g_cb_ctx;
u32 scePower_driver_var_2EDA246C;
u32 scePower_driver_var_3080D4C0;
u32 scePower_driver_var_18DFA43A;
u32 scePower_driver_var_33399B15;
static u32 g_kermit_periph_0648;
u32 scePower_driver_var_C7998FA4;
u32 scePower_driver_var_2A28BB48;
u32 scePower_driver_var_94FA3195;
u32 scePower_driver_var_07FC869E;
u32 scePower_driver_var_E59CA666;
static ScePowerSuspendContext g_susp_ctx;
static SceUID g_lock_threads[256];
static u8 g_suspend_save_buf[0x5128];
static ScePowerIdleTimerContext g_idle_ctx;
u32 scePower_driver_var_8B317890;
u32 scePower_driver_var_93E16800;
u32 scePower_driver_var_4A03DA50;
static ScePowerClockContext g_clock_ctx;
static ScePowerBatteryContext g_batt_ctx;

static inline u32 readKermitSeqVal(volatile u32 *base)
{
	u32 seq1, val, seq2;

	do {
		seq1 = base[1];
		val = base[2];
		seq2 = base[0];
	} while (seq1 != seq2);

	return val;
}

static s32 initCallbackFn(void *arg)
{
	int appType;

	(void)arg;
	appType = sceKernelApplicationType();
	if (appType != 0x100 && appType != 0x300) {
		changeClockFrequency(g_cb_ctx.initCpuFreq,
				     g_cb_ctx.initPllFreq,
				     g_cb_ctx.initBusFreq);
	}
	return 0;
}

static int kermitPowerIntrHandler(void)
{
	u32 val = readKermitSeqVal((volatile u32 *)0xbfc00100U);

	if ((val & 0x40U) != 0)
		scePowerUnlock(0);

	sceKernelSetEventFlag(g_susp_ctx.evfId, 0x20U);
	return 0;
}

s32 scePower_driver_9CE06934(void)
{
	int out;

	initSuspendSubsys();
	initClockSubsys();
	sceKermit_driver_5280B410(10, kermitPowerIntrHandler);
	if (sceKermitPeripheral_driver_0648E1A3() != 0) {
		g_init_clocks[1] = 111;
		g_init_clocks[2] = 222;
		g_init_clocks[0] = 222;
	}
	g_cb_ctx.initPllFreq = (u16)g_init_clocks[0];
	g_cb_ctx.initBusFreq = (u16)g_init_clocks[1];
	g_cb_ctx.initCpuFreq = (u16)g_init_clocks[2];
	g_cb_ctx.unk20a = 0;
	g_cb_ctx.unk20b = 0;
	g_cb_ctx.unk20c = 0;
	g_cb_ctx.unk20e = 0;
	g_cb_ctx.unk20d = 0;
	initBatterySubsys(0, 0);
	setBatteryThresholds(0x48, 0xd8);
	g_cb_ctx.statusMask = 0xffffffffU;
	initIdleSubsys();
	sceKernelRegisterSysEventHandler(&g_power_sys_event);
	sceKernelSetInitCallback(initCallbackFn, 2, &out);
	return 0;
}

static int powerSysEventHandler(int ev_id, char *ev_name, void *param, int *result)
{
	(void)ev_name;
	(void)result;

	if (ev_id == 0x402)
		return 0;
	if (ev_id >= 0x403) {
		if (ev_id == 0x10009) {
			u32 *ev = ((u32 **)param)[1];
			if ((ev[6] & 1U) != 0)
				g_cb_ctx.statusFlags |= 0x1000U;
			else
				g_cb_ctx.statusFlags &= ~0x1000U;

			g_cb_ctx.statusFlags &= ~0x40000000U;
			if (sceKernelGetCompiledSdkVersion() <= 0x06000000 &&
			    (ev[8] & 0x2000U) != 0) {
				g_cb_ctx.statusFlags |= 0x40000000U;
			}
			g_cb_ctx.statusFlags &= ~0x80000000U;
			updateBatteryEvent(ev, &g_cb_ctx.statusFlags);
		} else if (ev_id == 0x100000) {
			resetBatteryState();
		}
	}
	return 0;
}

s32 scePowerRegisterCallback(int slot, SceUID cbId)
{
	int oldK1;
	SceUidControlBlock *cb;
	int intr;
	s32 res;

	oldK1 = pspShiftK1();
	if ((u32)(slot + 1) >= 33U) {
		pspSetK1(oldK1);
		return SCE_ERROR_INVALID_INDEX;
	}
	if (pspGetK1() >= 0x80000000U && slot >= 16) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}
	if (sceKernelGetThreadmanIdType(cbId) != 8) {
		pspSetK1(oldK1);
		return SCE_ERROR_INVALID_ID;
	}
	if (sceKernelGetUIDcontrolBlock(cbId, (void **)&cb) != 0) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}
	if (((u32)pspGetK1() >> 31) != 0 && (cb->attr & 2) == 0) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	intr = sceKernelCpuSuspendIntr();
	if (slot != -1) {
		if (g_cb_ctx.slots[slot].cbId >= 0) {
			res = SCE_ERROR_ALREADY;
		} else {
			g_cb_ctx.slots[slot].cbId = cbId;
			g_cb_ctx.slots[slot].flags4 = 0;
			g_cb_ctx.slots[slot].flagsC = 0;
			g_cb_ctx.slots[slot].flags8 = g_cb_ctx.statusFlags;
			sceKernelNotifyCallback(cbId,
						(int)(g_cb_ctx.statusFlags & g_cb_ctx.statusMask));
			res = 0;
		}
	} else if (pspGetK1() < 0x80000000U) {
		res = SCE_ERROR_NOT_FOUND;
	} else {
		int i;
		res = SCE_ERROR_NO_MEMORY;
		for (i = 0; i < 16; i++) {
			if (g_cb_ctx.slots[i].cbId < 0) {
				g_cb_ctx.slots[i].cbId = cbId;
				g_cb_ctx.slots[i].flags4 = 0;
				g_cb_ctx.slots[i].flagsC = 0;
				g_cb_ctx.slots[i].flags8 = g_cb_ctx.statusFlags;
				sceKernelNotifyCallback(cbId,
							(int)(g_cb_ctx.statusFlags & g_cb_ctx.statusMask));
				res = i;
				break;
			}
		}
	}
	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return res;
}

s32 scePowerUnregisterCallback(int slot)
{
	int oldK1 = pspShiftK1();

	if ((u32)slot >= 32U) {
		pspSetK1(oldK1);
		return SCE_ERROR_INVALID_INDEX;
	}
	if (pspGetK1() >= 0x80000000U && slot >= 16) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}
	pspSetK1(oldK1);
	if (g_cb_ctx.slots[slot].cbId < 0)
		return SCE_ERROR_NOT_REGISTERED;

	g_cb_ctx.slots[slot].cbId = -1;
	return 0;
}

s32 scePower_A9D22232(int slot, u32 flagsC)
{
	int oldK1 = pspShiftK1();

	if ((u32)slot >= 32U) {
		pspSetK1(oldK1);
		return SCE_ERROR_INVALID_INDEX;
	}
	if (pspGetK1() >= 0x80000000U && slot >= 16) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}
	pspSetK1(oldK1);
	if (g_cb_ctx.slots[slot].cbId < 0)
		return SCE_ERROR_NOT_REGISTERED;

	g_cb_ctx.slots[slot].flagsC = flagsC;
	return 0;
}

s32 scePower_BAFA3DF0(int slot, u32 *outFlagsC)
{
	int oldK1 = pspShiftK1();

	if (!pspK1PtrOk(outFlagsC)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}
	if ((u32)slot >= 32U) {
		pspSetK1(oldK1);
		return SCE_ERROR_INVALID_INDEX;
	}
	if (pspGetK1() >= 0x80000000U && slot >= 16) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}
	if (g_cb_ctx.slots[slot].cbId < 0) {
		pspSetK1(oldK1);
		return SCE_ERROR_NOT_REGISTERED;
	}
	if (outFlagsC != NULL)
		*outFlagsC = g_cb_ctx.slots[slot].flagsC;

	pspSetK1(oldK1);
	return 0;
}

static void notifyPowerCallbacks(u32 clearMask, u32 setMask, u32 eventMask)
{
	int intr = sceKernelCpuSuspendIntr();
	u32 flags;
	s32 life;
	int i;

	flags = (g_cb_ctx.statusFlags & ~clearMask) | setMask;
	if (scePowerIsPowerOnline() != 0)
		flags |= 0x1000U;
	else
		flags &= ~0x1000U;

	if (scePowerIsBatteryExist() != 0)
		flags |= 0x80U;
	else
		flags &= ~0x80U;

	if (scePowerIsLowBattery() != 0)
		flags |= 0x100U;
	else
		flags &= ~0x100U;

	life = scePowerGetBatteryLifePercent();
	if ((u32)life >= 101U)
		life = 0;
	flags = (flags & ~0x3fU) | (u32)life;
	sceKernelGetCompiledSdkVersion();
	g_cb_ctx.statusFlags = flags;

	for (i = 0; i < 32; i++) {
		SceUID cbId = g_cb_ctx.slots[i].cbId;
		if (cbId >= 0) {
			s32 cnt = sceKernelGetCallbackCount(cbId);
			if (cnt >= 0) {
				u32 f4, f8;
				if (cnt == 0)
					g_cb_ctx.slots[i].flags4 = 0;
				f4 = g_cb_ctx.slots[i].flags4 | eventMask;
				f8 = flags | f4;
				g_cb_ctx.slots[i].flags4 = f4;
				g_cb_ctx.slots[i].flags8 = f8;
				sceKernelNotifyCallback(cbId, (int)(f8 & g_cb_ctx.statusMask));
			}
		}
	}
	sceKernelCpuResumeIntr(intr);
}

static s32 checkBusyPowerCallbacks(u32 mask, SceUID *outCbId)
{
	int intr = sceKernelCpuSuspendIntr();
	int i;

	for (i = 0; i < 32; i++) {
		SceUID cbId = g_cb_ctx.slots[i].cbId;
		if (cbId >= 0 && (g_cb_ctx.slots[i].flags8 & mask) != 0 &&
		    g_cb_ctx.slots[i].flagsC != 0) {
			if (sceKernelGetCallbackCount(cbId) > 0) {
				if (outCbId != NULL)
					*outCbId = cbId;
				sceKernelCpuResumeIntr(intr);
				return 1;
			}
		}
	}
	sceKernelCpuResumeIntr(intr);
	return 0;
}

s32 scePower_driver_2638EF48(void)
{
	s32 cpuFreq;

	lockClockMutex();
	scePower_34F9C463();
	cpuFreq = scePowerGetCpuClockFrequencyInt();
	scePowerGetBusClockFrequencyInt();
	if ((g_cb_ctx.unk20d == 1 && cpuFreq >= 223) ||
	    (g_cb_ctx.unk20d == 2 && cpuFreq >= 267)) {
		unlockClockMutex();
		return SCE_POWER_ERROR_CLOCK_INVALID;
	}
	g_cb_ctx.unk209 = 1;
	unlockClockMutex();
	if (g_cb_ctx.unk20b != 0)
		scePower_driver_10CE273F();
	return 0;
}

s32 scePower_442BFBAC(void)
{
	u32 val = (scePowerIsPowerOnline() != 0) ? 4 : 3;

	if (g_cb_ctx.unk20c != 0) {
		u32 m = (val < g_cb_ctx.unk20c) ? val : g_cb_ctx.unk20c;
		if (g_cb_ctx.unk209 != 0)
			val = m;
	}
	return (s32)val;
}

int module_start(SceSize args, void *argp)
{
	int i;

	(void)args;
	(void)argp;

	memset(&g_cb_ctx, 0, sizeof(g_cb_ctx));
	for (i = 0; i < 32; i++)
		g_cb_ctx.slots[i].cbId = -1;

	scePower_driver_9CE06934();
	return 0;
}

int module_reboot_phase(SceSize args, void *argp)
{
	int phase = (int)args;

	(void)argp;
	clockRebootPhase(phase);
	if (phase == 1)
		changeClockFrequency(333, 333, 166);
	return 0;
}

int module_reboot_before(void *arg0)
{
	u32 *rebootParam = *(u32 **)((u8 *)arg0 + 0x2c);

	if (rebootParam != NULL) {
		rebootParam[5] = scePower_driver_071160B1();
		scePower_driver_BADA8332((s32 *)&rebootParam[6], (s32 *)&rebootParam[7]);
		scePower_driver_16F965C9((s32 *)&rebootParam[8], (s32 *)&rebootParam[9]);
	}
	sceKernelUnregisterSysEventHandler(&g_power_sys_event);
	exitIdleSubsys();
	exitClockSubsys();
	exitBatterySubsys();
	exitSuspendSubsys();
	return 0;
}

s32 scePower_driver_AD5BB433(void)
{
	sceKernelUnregisterSysEventHandler(&g_power_sys_event);
	exitIdleSubsys();
	exitClockSubsys();
	exitBatterySubsys();
	exitSuspendSubsys();
	return 0;
}

s32 scePower_driver_23BDDD8B(void)
{
	g_cb_ctx.statusMask &= ~0x40000000U;
	return 0;
}

s32 scePower_A85880D0(void)
{
	return 1;
}

s32 scePower_driver_114B75AB(u8 val)
{
	g_cb_ctx.unk20d = val;
	return 0;
}

s32 scePower_driver_E52B4362(int freq)
{
	if ((g_cb_ctx.unk20d == 1 && freq >= 223) ||
	    (g_cb_ctx.unk20d == 2 && freq >= 267))
		return SCE_POWER_ERROR_CLOCK_INVALID;
	return 0;
}

s32 scePower_driver_8C6BEFD9(void)
{
	u8 b = g_cb_ctx.unk20b;

	g_cb_ctx.unk209 = 0;
	if (b != 0)
		scePower_driver_EF751B4A();
	return 0;
}

u8 scePower_driver_4E32E9B8(void)
{
	return g_cb_ctx.unk20a;
}

u8 scePower_2B51FE2F(void)
{
	return g_cb_ctx.unk209;
}

u8 scePower_driver_C463E7F2(void)
{
	return g_cb_ctx.unk20e;
}

static int powerMainThread(SceSize args, void *argp);

static s32 initSuspendSubsys(void)
{
	u32 partInfo[4];
	int intr;

	memset(&g_susp_ctx, 0, sizeof(g_susp_ctx));
	memset(g_lock_threads, 0, sizeof(g_lock_threads));
	g_susp_ctx.swFlags = 2;
	g_susp_ctx.param3c = 8;
	g_susp_ctx.evfId = sceKernelCreateEventFlag("ScePowerSw", 0x201, 0x50000, NULL);
	g_susp_ctx.vmemSemaId = sceKernelCreateSema("ScePowerVmem", 1, 0, 1, NULL);
	g_susp_ctx.mainThreadId = sceKernelCreateThread("ScePowerMain", powerMainThread,
							4, 0x800, 0x100001, NULL);
	sceKernelStartThread(g_susp_ctx.mainThreadId, 0, NULL);
	partInfo[0] = 16;
	sceKernelQueryMemoryPartitionInfo(5, partInfo);
	g_susp_ctx.vmemSize = partInfo[2];
	g_susp_ctx.vmemAddr = partInfo[1];
	intr = sceKernelCpuSuspendIntr();
	scePower_driver_6CF50928(0);
	g_susp_ctx.kernelLockCount += sceKernelRegisterPowerHandlers(&g_power_handlers);
	sceKernelCpuResumeIntr(intr);
	g_kermit_periph_0648 = (u32)sceKermitPeripheral_driver_0648E1A3();
	return 0;
}

s32 scePower_23C31FFE(int mode, u32 *outAddr, u32 *outSize)
{
	int oldK1;
	s32 res;
	int intr;

	if (mode != 0)
		return SCE_ERROR_INVALID_MODE;

	oldK1 = pspShiftK1();
	if (!pspK1PtrOk(outAddr) || !pspK1PtrOk(outSize)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	res = sceKernelWaitSema(g_susp_ctx.vmemSemaId, 1, NULL);
	if (outAddr != NULL)
		*outAddr = g_susp_ctx.vmemAddr;
	if (outSize != NULL)
		*outSize = g_susp_ctx.vmemSize;

	intr = sceKernelCpuSuspendIntr();
	if (((g_susp_ctx.vmemAddr >> 23) & 0x3fU) == 0x10U) {
		u32 top = (g_susp_ctx.vmemAddr >> 29) & 7U;
		if (((0x35U >> top) & 1U) != 0)
			sceKernelSetDdrMemoryProtection(g_susp_ctx.vmemAddr, g_susp_ctx.vmemSize, 0xf);
	}
	g_susp_ctx.vmemLocked = 1;
	sceKernelCpuResumeIntrWithSync(intr);
	pspSetK1(oldK1);
	return res;
}

s32 scePower_FA97A599(int mode, u32 *outAddr, u32 *outSize)
{
	int oldK1;
	s32 res;
	int intr;

	if (mode != 0)
		return SCE_ERROR_INVALID_MODE;

	oldK1 = pspShiftK1();
	if (!pspK1PtrOk(outAddr) || !pspK1PtrOk(outSize)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	res = sceKernelPollSema(g_susp_ctx.vmemSemaId, 1);
	if (res != 0) {
		if (res == (s32)0x800201adU)
			res = SCE_POWER_ERROR_VMEM_IN_USE;
		pspSetK1(oldK1);
		return res;
	}

	if (outAddr != NULL)
		*outAddr = g_susp_ctx.vmemAddr;
	if (outSize != NULL)
		*outSize = g_susp_ctx.vmemSize;

	intr = sceKernelCpuSuspendIntr();
	if (((g_susp_ctx.vmemAddr >> 23) & 0x3fU) == 0x10U) {
		u32 top = (g_susp_ctx.vmemAddr >> 29) & 7U;
		if (((0x35U >> top) & 1U) != 0)
			sceKernelSetDdrMemoryProtection(g_susp_ctx.vmemAddr, g_susp_ctx.vmemSize, 0xf);
	}
	g_susp_ctx.vmemLocked = 1;
	sceKernelCpuResumeIntrWithSync(intr);
	pspSetK1(oldK1);
	return 0;
}

s32 scePower_B3EDD801(int mode)
{
	int oldK1 = (int)pspGetK1();
	int intr;
	s32 res;

	if (mode != 0)
		return SCE_ERROR_INVALID_MODE;

	pspShiftK1();
	sceKernelDcacheWritebackInvalidateRange((const void *)g_susp_ctx.vmemAddr,
						g_susp_ctx.vmemSize);
	intr = sceKernelCpuSuspendIntr();
	if (((g_susp_ctx.vmemAddr >> 23) & 0x3fU) == 0x10U) {
		u32 top = (g_susp_ctx.vmemAddr >> 29) & 7U;
		if (((0x35U >> top) & 1U) != 0)
			sceKernelSetDdrMemoryProtection(g_susp_ctx.vmemAddr, g_susp_ctx.vmemSize, 0);
	}
	res = sceKernelSignalSema(g_susp_ctx.vmemSemaId, 1);
	g_susp_ctx.vmemLocked = 0;
	sceKernelCpuResumeIntrWithSync(intr);
	pspSetK1(oldK1);
	return res;
}

static s32 powerLockInternal(int mode, int isUser)
{
	int intr;

	if (mode != 0)
		return SCE_ERROR_INVALID_MODE;
	if (g_susp_ctx.shutdownLocked != 0)
		return 0;

	intr = sceKernelCpuSuspendIntr();
	if (g_susp_ctx.userLockCount == 0 && g_susp_ctx.kernelLockCount == 0)
		sceKernelClearEventFlag(g_susp_ctx.evfId, ~0x10000U);

	if (isUser != 0) {
		g_susp_ctx.userLockCount++;
		sceKernelCpuResumeIntr(intr);
		if (g_susp_ctx.suspending != 0) {
			return sceKernelWaitEventFlag(g_susp_ctx.evfId, 0x40000U, 1, NULL, NULL);
		}
		return 0;
	}

	g_susp_ctx.kernelLockCount++;
	sceKernelCpuResumeIntr(intr);
	return 0;
}

s32 scePowerLock(int mode)
{
	int oldK1;
	s32 res;

	if (scePower_driver_var_2A28BB48 != 0) {
		scePower_driver_var_2A28BB48 = 0;
		return 0;
	}
	if (scePower_driver_var_94FA3195 != 0)
		return 0;

	oldK1 = pspShiftK1();
	res = powerLockInternal(mode, 1);
	pspSetK1(oldK1);
	return res;
}

s32 scePower_driver_6CF50928(int mode)
{
	if (mode == 1) {
		if (scePower_driver_var_07FC869E != 0) {
			int i;
			for (i = 0; i < 256; i++) {
				if (g_lock_threads[i] == 0) {
					g_lock_threads[i] = sceKernelGetThreadId();
					break;
				}
			}
		}
		mode = 0;
	}
	return powerLockInternal(mode, 0);
}

static s32 powerUnlockInternal(int mode, int isUser)
{
	int intr;
	s32 count;

	(void)mode;
	if (g_susp_ctx.shutdownLocked != 0)
		return 0;

	intr = sceKernelCpuSuspendIntr();
	if (isUser != 0) {
		count = g_susp_ctx.userLockCount;
		if (count > 0) {
			count--;
			g_susp_ctx.userLockCount = count;
		}
	} else {
		count = g_susp_ctx.kernelLockCount;
		if (count > 0) {
			count--;
			g_susp_ctx.kernelLockCount = count;
		}
	}
	if (g_susp_ctx.userLockCount == 0 && g_susp_ctx.kernelLockCount == 0)
		sceKernelSetEventFlag(g_susp_ctx.evfId, 0x10000U);

	sceKernelCpuResumeIntr(intr);
	return count;
}

s32 scePowerUnlock(int mode)
{
	int oldK1;
	s32 res;

	if (scePower_driver_var_94FA3195 != 0)
		return 0;

	oldK1 = pspShiftK1();
	if (mode != 0) {
		pspSetK1(oldK1);
		return SCE_ERROR_INVALID_MODE;
	}
	res = powerUnlockInternal(0, 1);
	pspSetK1(oldK1);
	return res;
}

s32 scePower_driver_C3024FE6(int mode)
{
	if (mode == 1) {
		if (scePower_driver_var_07FC869E != 0) {
			SceUID thid = sceKernelGetThreadId();
			int i;
			for (i = 0; i < 256; i++) {
				if (g_lock_threads[i] == thid) {
					g_lock_threads[i] = 0;
					break;
				}
			}
		}
		mode = 0;
	}
	return powerUnlockInternal(mode, 0);
}

s32 scePower_3951AF53(void)
{
	u32 outBits = 0;
	int oldK1 = pspShiftK1();
	s32 res;

	res = sceKernelPollEventFlag(g_susp_ctx.evfId, 0xffffffffU, 1, &outBits);
	if (res < 0 && res != (s32)0x800201afU) {
		pspSetK1(oldK1);
		return res;
	}
	if ((outBits & 0x1f0U) == 0) {
		pspSetK1(oldK1);
		return 0;
	}
	if ((outBits & 0x20000U) == 0) {
		res = sceKernelWaitEventFlag(g_susp_ctx.evfId, 0x20000U, 1, &outBits, NULL);
		if (res < 0) {
			pspSetK1(oldK1);
			return res;
		}
	}
	res = sceKernelWaitEventFlag(g_susp_ctx.evfId, 0x40000U, 1, &outBits, NULL);
	pspSetK1(oldK1);
	return (res < 0) ? res : 0;
}

static void __attribute__((noipa)) disablePeripheralsForSuspend(void)
{
	int i;

	for (i = 0; i < 6; i++) {
		if ((u32)(i - 2) >= 2U)
			sceSysregUartIoDisable(i);
	}
	for (i = 0; i < 6; i++) {
		if (i != 0)
			sceSysregSpiIoDisable(i);
	}
	for (i = 0; i < 32; i++) {
		if ((u32)(i - 3) >= 2U && i != 7)
			sceSysregGpioIoDisable(i);
	}
}

static void __attribute__((noipa)) syncKermitDma(void)
{
	__asm__ volatile("sync");
	HW(0xbd000004U) = 0xf;
	while (HW(0xbd000004U) != 0) {
	}
	while (HW(0xbd000000U) != 0xf) {
	}
}

static void __attribute__((noipa)) saveRestoreClockSysreg(int save);
static s32 __attribute__((noipa)) restoreClockSysregPhase(int phase);
static void __attribute__((noipa)) powerResumeEntry(u32 *arg);

static s32 __attribute__((noipa)) executePowerTransition(int transMode)
{
	u32 spBuf[0x160 / 4];
	int intr;
	int i;
	s32 retry;
	s32 savedHcount;
	u64 sysTime;
	s32 savedPll = 0, savedCpu = 0, savedBus = 0;

	memset(&spBuf[0], 0, 0x40);
	memset(&spBuf[0x40 / 4], 0, 0x120);
	spBuf[0x00 / 4] = 0x40;
	spBuf[0x40 / 4] = 0x38;
	spBuf[0x80 / 4] = 0x80;
	spBuf[0x84 / 4] = 0x06060110U;
	spBuf[0xb4 / 4] = (u32)sceKernelGetGameInfo();
	spBuf[0xc4 / 4] = (u32)sceKernelInitParamSfo(&spBuf[0xc8 / 4]);
	spBuf[0x130 / 4] = g_susp_ctx.param3c;
	spBuf[0x10 / 4] = (u32)&spBuf[0x130 / 4];
	spBuf[0x14 / 4] = (u32)&spBuf[0x80 / 4];

	scePower_driver_C463E7F2();
	if (g_kermit_periph_0648 != 0) {
		scePower_34F9C463();
		savedCpu = scePowerGetCpuClockFrequencyInt();
		savedPll = savedCpu;
		savedBus = scePowerGetBusClockFrequencyInt();
	}

	sceCtrlSetButtonIntercept(0x10000, (scePower_driver_var_C7998FA4 != 0) ? 2 : 1);
	if ((transMode & 0xf) == 1)
		spBuf[0x04 / 4] = 1;

	intr = sceKernelCpuSuspendIntr();
	spBuf[0x134 / 4] = 0;
	spBuf[0x138 / 4] = 0;
	if (sceKernelSysEventDispatch(0xff00, 0x100, "query", spBuf,
				      (int *)&spBuf[0x134 / 4], 1,
				      (SceSysEventHandler **)&spBuf[0x138 / 4]) < 0) {
		sceKernelSysEventDispatch(0xff00, 0x101, "query", spBuf, NULL, 0, NULL);
		sceKernelCpuResumeIntr(intr);
		return -1;
	}
	sceKernelSysEventDispatch(0xff00, 0x102, "query", spBuf, NULL, 0, NULL);
	sceKernelCpuResumeIntr(intr);

	sceKernelSysEventDispatch(0xff00, 0x210, "phase2", spBuf, NULL, 0, NULL);
	notifyPowerCallbacks(0, (spBuf[0x04 / 4] == 1) ? 0x80000U : 0x10000U, 0);
	while (scePower_driver_var_E59CA666 != 0)
		sceKernelDelayThread(scePower_driver_var_E59CA666);

	g_susp_ctx.suspending = 1;
	for (i = 15; i >= 0; i--) {
		while (scePower_driver_var_2EDA246C != 0)
			sceKernelDelayThread(scePower_driver_var_2EDA246C);
		sceKernelSysEventDispatch(0xff00, 0x200 + i, "phase2", spBuf, NULL, 0, NULL);
	}

	intr = sceKernelCpuSuspendIntr();
	sceKernelSysEventDispatch(0xff00, 0x400, "phase1", spBuf, NULL, 0, NULL);
	retry = 0;
	for (;;) {
		sceKernelSysEventDispatch(0xff00, 0x401, "phase1", spBuf, NULL, 0, NULL);
		if (scePower_driver_var_3080D4C0 != 0 && retry == 499)
			sceKermitPeripheral_driver_15F5C107();
		if (sceKernelSysEventDispatch(0xff00, 0x402, "phase1", spBuf,
					      (int *)&spBuf[0x13c / 4], 1,
					      (SceSysEventHandler **)&spBuf[0x140 / 4]) >= 0 &&
		    g_susp_ctx.kernelLockCount == 0 &&
		    g_susp_ctx.vmemLocked == 0 &&
		    checkBusyPowerCallbacks(0x90000U, (SceUID *)&spBuf[0x144 / 4]) == 0) {
			break;
		}
		retry++;
		sceKernelCpuResumeIntr(intr);
		sceKernelDelayThread(10000);
		intr = sceKernelCpuSuspendIntr();
	}

	sceDisplay_driver_CE8A328E();
	savedHcount = sceDisplayGetAccumulatedHcount();
	sysTime = (u64)sceKernelGetSystemTimeWide();
	spBuf[0xa0 / 4] = (u32)sysTime;
	spBuf[0xa4 / 4] = (u32)(sysTime >> 32);
	spBuf[0x08 / 4] = (u32)sysTime;
	spBuf[0x0c / 4] = (u32)(sysTime >> 32);
	sceKernelSysEventDispatch(0xff00, 0x1000, "freeze", spBuf, NULL, 0, NULL);
	scePower_FA97A599(0, NULL, NULL);
	disablePeripheralsForSuspend();
	for (i = 15; i >= 0; i--)
		sceKernelSysEventDispatch(0xff00, 0x4000 + i, "phase0", spBuf, NULL, 0, NULL);

	sceKernelDcacheWritebackAll();
	{
		u8 seed[8] = { 0x12, 0x34, 0x56, 0x78, 0x9a, 0x00, 0xde, 0xf0 };
		u32 sha1[5];
		sceKernelUtilsSha1Digest(seed, 8, (u8 *)sha1);
		u64 fuseId = sceSysregGetFuseId();
		sha1[0] ^= (u32)fuseId;
		sha1[1] ^= (u32)(fuseId >> 32);
		sceKernelUtilsSha1Digest((u8 *)sha1, 20, (u8 *)sha1);
		u32 resumePoint = (((u32)&spBuf[0x80 / 4]) | 1U) ^
				  (sha1[0] ^ sha1[1] ^ sha1[2] ^ sha1[3] ^ sha1[4]);
		Kprintf("uiResumePoint: 0x%08X 0x%08x\n", resumePoint, (u32)powerResumeEntry);
		spBuf[0xb0 / 4] = (u32)powerResumeEntry;
		HW(0xbfc001fcU) = resumePoint;
		u32 pllOut = (u32)sceSysregPllGetOutSelect();
		*(u32 *)(g_suspend_save_buf + 0x5000) = pllOut;
		u32 ddrVal = (u32)sceDdr_driver_00E36648();
		*(u32 *)(g_suspend_save_buf + 0x5004) = ddrVal;
		spBuf[0xac / 4] = ddrVal;
		spBuf[0xa8 / 4] = pllOut;
	}
	saveRestoreClockSysreg(1);
	sceKernelDispatchSuspendHandlers(0);
	g_susp_ctx.suspending = 0;

	if ((transMode & 0xf00) == 0x200) {
		UtilsForKernel_A6B0A6B8();
		int st = UtilsForKernel_39FFB756(0);
		if (sceSuspendForKernel_67B59042(0) == 0) {
			sceKernelDcacheWritebackAll();
			syncKermitDma();
			HW(0xabd78000U) = 1;
			syncKermitDma();
		}
		UtilsForKernel_39FFB756(st);
	} else if ((transMode & 0xf00) == 0x300) {
		UtilsForKernel_A6B0A6B8();
		int st = UtilsForKernel_39FFB756(0);
		if (sceSuspendForKernel_67B59042(0) == 0) {
			memset(g_suspend_save_buf + 0x4e08, 0, 0xd4);
			g_susp_ctx.resumeCount++;
			sceSuspendForKernel_B2C9640B(1);
		}
		UtilsForKernel_39FFB756(st);
	}

	sceKernelDispatchResumeHandlers(0);
	restoreClockSysregPhase(0);
	sceDdr_driver_E0A39D3E(*(int *)(g_suspend_save_buf + 0x5004));
	for (i = 0; i < 16; i++)
		sceKernelSysEventDispatch(0xff0000, 0x10000 + i, "phase0", &spBuf[0x40 / 4], NULL, 0, NULL);

	restoreClockSysregPhase(1);
	scePower_B3EDD801(0);
	sceKernelClearEventFlag(g_susp_ctx.evfId, (u32)(-0x1f4));
	notifyPowerCallbacks(0x10000U, 0x20000U, 0);
	scePowerTick(0);
	while (sceDisplayIsVblank() == 0) {
	}
	while (sceDisplayIsVblank() != 0) {
	}
	while (sceDisplayGetCurrentHcount() < savedHcount) {
	}
	sceDisplayAdjustAccumulatedHcount((int)sysTime);
	sceKernelSysEventDispatch(0xff0000, 0x40000, "melt", &spBuf[0x40 / 4], NULL, 0, NULL);
	sceKernelCpuResumeIntr(intr);

	sceKernelSysEventDispatch(0xff0000, 0x100000, "phase1", &spBuf[0x40 / 4], NULL, 0, NULL);
	sceKernelSysEventDispatch(0xff0000, 0x100001, "phase1", &spBuf[0x40 / 4], NULL, 0, NULL);
	sceKernelSysEventDispatch(0xff0000, 0x100002, "phase1", &spBuf[0x40 / 4], NULL, 0, NULL);
	sceKernelSysEventDispatch(0xff0000, 0x400000, "completed", &spBuf[0x40 / 4], NULL, 0, NULL);

	if (g_kermit_periph_0648 != 0) {
		if (savedCpu == 333)
			changeClockFrequency(222, 222, 111);
		else
			changeClockFrequency(333, 333, 166);
		changeClockFrequency(savedPll, savedCpu, savedBus);
	}
	notifyPowerCallbacks(0x20000U, 0, 0x40000U);
	sceKermitPeripheral_driver_6D6AA215();
	sceCtrlSetButtonIntercept(0x10000, 0);
	return 0;
}

static void __attribute__((noipa)) powerResumeEntry(u32 *arg)
{
	if (arg != NULL) {
		s32 len = (s32)arg[0];
		u8 *src = (u8 *)arg;
		u8 *dst = g_suspend_save_buf + 0x5008;
		s32 i;
		for (i = 0; i < len; i++)
			dst[i] = src[i];
		*(u32 *)(g_suspend_save_buf + 0x501c) = 0;
		void (*fn)(u32) = (void (*)(u32))arg[5];
		if (fn != NULL)
			fn(*(u32 *)(g_suspend_save_buf + 0x5000));
	}
	u32 r0 = HW(0xbfc00200U);
	u32 r1 = HW(0xbfc00204U);
	u32 r2 = HW(0xbfc00208U);
	u32 r3 = HW(0xbfc0020cU);
	u32 r4 = HW(0xbfc00210U);
	u32 r5 = HW(0xbfc00214U);
	u32 r6 = HW(0xbfc00218U);
	u32 r7 = HW(0xbfc0021cU);
	g_susp_ctx.resumeCount++;
	HW(0xbfc00200U) = r0;
	HW(0xbfc00204U) = r1;
	HW(0xbfc00208U) = r2;
	HW(0xbfc0020cU) = r3;
	HW(0xbfc00210U) = r4;
	HW(0xbfc00214U) = r5;
	HW(0xbfc00218U) = r6;
	HW(0xbfc0021cU) = r7;
	sceSuspendForKernel_B2C9640B(1);
}

static int powerMainThread(SceSize args, void *argp)
{
	u32 outBits = 0;

	(void)args;
	(void)argp;
	(void)g_suspend_save_buf;

	for (;;) {
		sceKernelWaitEventFlag(g_susp_ctx.evfId, 0x800001f1U, 1, &outBits, NULL);
		if ((s32)outBits < 0)
			break;

		if (scePower_driver_var_33399B15 != 0)
			sceKernelDelayThread(scePower_driver_var_33399B15);

		if ((outBits & 1U) != 0) {
			SceUInt timeout = 2000000;
			sceKernelWaitEventFlag(g_susp_ctx.evfId, 0x800001f2U, 1, &outBits, &timeout);
			if ((s32)outBits < 0)
				break;
			if ((g_susp_ctx.swFlags & 1U) != 0) {
				notifyPowerCallbacks(0, 0,
						     ((outBits & 2U) != 0) ? 0x20000000U : 0x10000000U);
			}
			if ((g_susp_ctx.swFlags & 2U) != 0) {
				if ((outBits & 2U) != 0)
					g_susp_ctx.reqState1 = 2;
				else
					g_susp_ctx.reqState1 = 1;
			}
		}

		if ((outBits & 0x10U) != 0) {
			g_susp_ctx.reqState2 = 1;
		} else if ((outBits & 0x20U) != 0) {
			g_susp_ctx.reqState2 = 2;
		} else if ((outBits & 0x40U) != 0) {
			g_susp_ctx.reqState2 = 3;
		} else if ((outBits & 0x80U) != 0) {
			g_susp_ctx.reqState2 = 4;
		} else if (g_susp_ctx.reqState1 == 0) {
			continue;
		}

		if (scePower_driver_var_07FC869E != 0) {
			int i;
			for (i = 0; i < 256; i++) {
				if (g_lock_threads[i] != 0) {
					if (sceKernelGetThreadExitStatus(g_lock_threads[i]) != (int)0x800201a4U)
						scePower_driver_C3024FE6(1);
				}
			}
			memset(g_lock_threads, 0, sizeof(g_lock_threads));
		}

		sceKernelWaitEventFlag(g_susp_ctx.evfId, 0x80010000U, 1, &outBits, NULL);
		if ((s32)outBits < 0)
			break;
		if ((outBits & 0x80U) != 0)
			g_susp_ctx.reqState2 = 4;

		sceKernelClearEventFlag(g_susp_ctx.evfId, ~0x40000U);
		sceKernelSetEventFlag(g_susp_ctx.evfId, 0x20000U);

		if (g_susp_ctx.shutdownLocked == 0) {
			if (g_susp_ctx.reqState1 == 1 || g_susp_ctx.reqState2 == 1) {
				executePowerTransition(0x101);
			} else if (g_susp_ctx.reqState1 == 2 || g_susp_ctx.reqState2 == 2) {
				executePowerTransition(0x202);
				g_susp_ctx.reqState1 = 0;
				g_susp_ctx.reqState2 = 0;
			} else if (g_susp_ctx.reqState2 == 3) {
				executePowerTransition(0x303);
				g_susp_ctx.reqState1 = 0;
				g_susp_ctx.reqState2 = 0;
			} else if (g_susp_ctx.reqState2 == 4) {
				executePowerTransition(0x404);
			}
		}

		sceKernelClearEventFlag(g_susp_ctx.evfId, ~0x20000U);
		sceKernelSetEventFlag(g_susp_ctx.evfId, 0x40000U);
	}
	return 0;
}

static s32 exitSuspendSubsys(void)
{
	sceKernelSetEventFlag(g_susp_ctx.evfId, 0x80000000U);
	sceKernelWaitThreadEnd(g_susp_ctx.mainThreadId, NULL);
	sceKernelRegisterPowerHandlers(NULL);
	sceKernelDeleteSema(g_susp_ctx.vmemSemaId);
	sceKernelDeleteEventFlag(g_susp_ctx.evfId);
	if (((g_susp_ctx.vmemAddr >> 23) & 0x3fU) == 0x10U) {
		u32 top = (g_susp_ctx.vmemAddr >> 29) & 7U;
		if (((0x35U >> top) & 1U) != 0)
			sceKernelSetDdrMemoryProtection(g_susp_ctx.vmemAddr, g_susp_ctx.vmemSize, 0xf);
	}
	return 0;
}

s32 scePower_0CD21B1F(int mode)
{
	g_susp_ctx.swFlags = (u32)(mode & 3);
	return 0;
}

s32 scePower_165CE085(void)
{
	return (s32)g_susp_ctx.swFlags;
}

s32 scePower_driver_1EC2D4E4(void)
{
	int intr;

	scePowerLock(0);
	intr = sceKernelCpuSuspendIntr();
	g_susp_ctx.shutdownLocked = 1;
	sceKernelClearEventFlag(g_susp_ctx.evfId, ~0x10000U);
	sceKernelSetEventFlag(g_susp_ctx.evfId, 0x40000U);
	sceKernelCpuResumeIntr(intr);
	return 0;
}

s32 scePower_7FA406DD(void)
{
	return (g_susp_ctx.reqState1 | g_susp_ctx.reqState2) != 0;
}

u32 scePower_DB62C9CF(void)
{
	int intr = sceKernelCpuSuspendIntr();
	u32 st = g_susp_ctx.reqState1;
	g_susp_ctx.reqState1 = 0;
	sceKernelCpuResumeIntr(intr);
	return st;
}

s32 scePowerRequestStandby(void)
{
	int oldK1 = pspShiftK1();
	sceKernelSetEventFlag(g_susp_ctx.evfId, 0x10U);
	pspSetK1(oldK1);
	return 0;
}

s32 scePowerRequestSuspend(void)
{
	int oldK1 = pspShiftK1();
	s32 res = sceKermitPeripheral_driver_65D98A21();
	pspSetK1(oldK1);
	return res;
}

s32 scePower_driver_D79B0122(void)
{
	return 0;
}

s32 scePower_2875994B(void)
{
	int oldK1 = pspShiftK1();
	sceKernelSetEventFlag(g_susp_ctx.evfId, 0x40U);
	pspSetK1(oldK1);
	return 0;
}

s32 scePowerRequestColdReset(int mode)
{
	int oldK1 = pspShiftK1();

	if (pspGetK1() >= 0x80000000U && mode != 0) {
		pspSetK1(oldK1);
		return SCE_ERROR_INVALID_MODE;
	}
	g_susp_ctx.coldResetMode = (u32)mode;
	sceKernelSetEventFlag(g_susp_ctx.evfId, 0x80U);
	pspSetK1(oldK1);
	return 0;
}

u32 scePower_0074EF9B(void)
{
	return g_susp_ctx.resumeCount;
}

s32 scePower_driver_BA566CD0(u32 val)
{
	g_susp_ctx.param3c = val;
	return 0;
}

s32 scePowerGetIdleTimer(int slot, u64 *elapsedOut, u64 *remainOut)
{
	int oldK1 = pspShiftK1();
	u64 now, elapsed;

	if ((u32)slot >= 8U) {
		pspSetK1(oldK1);
		return SCE_ERROR_INVALID_INDEX;
	}
	if (!pspK1PtrOk(elapsedOut) || !pspK1PtrOk(remainOut)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	now = (u64)sceKernelGetSystemTimeWide();
	elapsed = now - g_idle_ctx.slots[slot].startTime;
	if (elapsedOut != NULL)
		*elapsedOut = elapsed;
	if (remainOut != NULL)
		*remainOut = g_idle_ctx.slots[slot].timeout - elapsed;

	pspSetK1(oldK1);
	return (s32)elapsed;
}

static u32 getModuleGp(void)
{
	u32 gpVal;
	__asm__ volatile("move %0, $gp" : "=r"(gpVal));
	return gpVal;
}

s32 scePower_driver_1BA2FCAE(int slot, u32 cbArg, u64 timeout,
			     void (*cbFunc)(int, u32, u32), u32 userOpt)
{
	int intr;

	if ((u32)slot >= 8U)
		return SCE_ERROR_INVALID_INDEX;

	intr = sceKernelCpuSuspendIntr();
	if (cbFunc != NULL && g_idle_ctx.slots[slot].cbFunc != NULL) {
		sceKernelCpuResumeIntr(intr);
		return SCE_ERROR_ALREADY;
	}

	g_idle_ctx.slots[slot].cbArg = cbArg;
	g_idle_ctx.slots[slot].startTime = (u64)sceKernelGetSystemTimeWide();
	g_idle_ctx.slots[slot].cbFunc = cbFunc;
	g_idle_ctx.slots[slot].state4 = -1;
	g_idle_ctx.slots[slot].timeout = timeout;
	g_idle_ctx.slots[slot].gpFunc = getModuleGp;
	g_idle_ctx.slots[slot].userOpt = userOpt;
	g_idle_ctx.slots[slot].state0 = 0;
	sceKernelCpuResumeIntr(intr);
	return 0;
}

static int idleVblankSubIntr(int sub_intr, void *arg)
{
	u64 now = (u64)sceKernelGetSystemTimeWide();
	int i;

	(void)sub_intr;
	(void)arg;

	g_idle_ctx.lastSysTime = now;
	for (i = 0; i < 8; i++) {
		ScePowerIdleSlot *s = &g_idle_ctx.slots[i];
		if (s->cbFunc == NULL)
			continue;
		if ((g_idle_ctx.enabledMask & (1U << i)) == 0)
			continue;

		if (s->state0 != 0) {
			if (s->state4 != 0) {
				s->state0 = 0;
				s->state4 = 0;
				s->cbFunc(i, 0, s->userOpt);
			}
		} else if (s->state4 != 1) {
			u64 diff = now - s->startTime;
			if (diff >= s->timeout) {
				s->state4 = 1;
				s->cbFunc(i, (u32)diff, s->userOpt);
			}
		}
	}
	return -1;
}

static s32 initIdleSubsys(void)
{
	memset(&g_idle_ctx, 0, sizeof(g_idle_ctx));
	g_idle_ctx.lastSysTime = (u64)sceKernelGetSystemTimeWide();
	g_idle_ctx.enabledMask = 0xffU;
	sceKernelRegisterSubIntrHandler(30, 26, idleVblankSubIntr, NULL);
	sceKernelEnableSubIntr(30, 26);
	return 0;
}

static s32 exitIdleSubsys(void)
{
	sceKernelReleaseSubIntrHandler(30, 26);
	return 0;
}

s32 scePowerTick(u32 flags)
{
	volatile u32 *base = (volatile u32 *)0xbfc0010cU;
	u32 seq = base[0] + 1;

	base[0] = seq;
	base[2] = flags;
	base[1] = seq;
	return 0;
}

s32 scePowerIdleTimerEnable(int slot)
{
	int intr;
	u32 prev;

	if ((u32)slot >= 8U)
		return SCE_ERROR_INVALID_INDEX;

	intr = sceKernelCpuSuspendIntr();
	prev = (g_idle_ctx.enabledMask >> slot) & 1U;
	g_idle_ctx.enabledMask |= (1U << slot);
	sceKernelCpuResumeIntr(intr);
	return (s32)prev;
}

s32 scePowerIdleTimerDisable(int slot)
{
	int intr;
	u32 prev;

	if ((u32)slot >= 8U)
		return SCE_ERROR_INVALID_INDEX;

	intr = sceKernelCpuSuspendIntr();
	prev = (g_idle_ctx.enabledMask >> slot) & 1U;
	g_idle_ctx.enabledMask &= ~(1U << slot);
	sceKernelCpuResumeIntr(intr);
	return (s32)prev;
}

static s32 initClockSubsys(void)
{
	float pllF, cpuF, busF;

	memset(&g_clock_ctx, 0, sizeof(g_clock_ctx));
	g_clock_ctx.sm1Ops = sceKernelSm1ReferOperations();
	g_clock_ctx.busMin = 24;
	g_clock_ctx.pllMin = 1;
	g_clock_ctx.pllMax = 333;
	g_clock_ctx.cpuMin = 1;
	g_clock_ctx.cpuMax = 333;
	g_clock_ctx.busMax = 166;

	pllF = sceSysregPllGetFrequency();
	g_clock_ctx.pllFreqFloat = pllF;
	g_clock_ctx.pllFreqInt = (s32)pllF;
	g_clock_ctx.pllOutSelect = sceSysregPllGetOutSelect();

	cpuF = sceSysreg_driver_73EBC752();
	g_clock_ctx.cpuFreqFloat = cpuF;
	g_clock_ctx.cpuFreqInt = (s32)cpuF;

	busF = sceSysreg_driver_96D74557();
	g_clock_ctx.pllMask = 0x3f3fU;
	g_clock_ctx.busFreqInt = (s32)busF;
	g_clock_ctx.busFreqFloat = busF;

	if (g_clock_ctx.pllOutSelect == 5) {
		g_clock_ctx.mode48 = 1;
		g_clock_ctx.mode38 = 1;
		g_clock_ctx.mode40 = 1;
	} else if (g_clock_ctx.pllOutSelect == 4) {
		g_clock_ctx.mode38 = 1;
		g_clock_ctx.mode48 = 0;
		g_clock_ctx.mode40 = 0;
	} else {
		g_clock_ctx.mode48 = 0;
		g_clock_ctx.mode38 = 0;
		g_clock_ctx.mode40 = 0;
	}

	g_clock_ctx.val4e = -1;
	g_clock_ctx.val44 = -1;
	g_clock_ctx.val46 = -1;
	g_clock_ctx.val4c = -1;

	if (sceSysregGetTachyonVersion() <= 0x0013ffffU) {
		g_clock_ctx.val3e = 0x400;
		g_clock_ctx.val3c = 0;
	} else {
		u32 fuseCfg = sceSysregGetFuseConfig();
		g_clock_ctx.val3e = (s16)(0xb00U - ((fuseCfg & 0x3800U) >> 3));
		g_clock_ctx.val3c = (s16)((~fuseCfg) & 0x700U);
	}

	scePower_driver_100A77A1(1);
	g_clock_ctx.mutexId = sceKernelCreateMutex("ScePowerClock", 1, 0, NULL);
	return 0;
}

s32 scePowerSetCpuClockFrequency(int cpufreq)
{
	int oldK1;
	int intr;
	float cpuF;

	if (scePower_driver_var_8B317890 != 0 && cpufreq == 333)
		cpufreq = (int)scePower_driver_var_8B317890;
	if (scePower_driver_var_93E16800 != 0 && cpufreq == 222)
		cpufreq = (int)scePower_driver_var_93E16800;

	if (cpufreq <= 0 || cpufreq > g_clock_ctx.pllFreqInt)
		return SCE_ERROR_INVALID_VALUE;

	oldK1 = pspShiftK1();
	intr = sceKernelCpuSuspendIntr();
	if (cpufreq < g_clock_ctx.cpuMin)
		cpufreq = g_clock_ctx.cpuMin;
	else if (cpufreq > g_clock_ctx.cpuMax)
		cpufreq = g_clock_ctx.cpuMax;

	sceSysreg_driver_B413B041((float)cpufreq);
	cpuF = sceSysreg_driver_73EBC752();
	g_clock_ctx.cpuFreqFloat = cpuF;
	g_clock_ctx.cpuFreqInt = (s32)cpuF;
	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return 0;
}

s32 scePowerSetBusClockFrequency(int busfreq)
{
	int oldK1;
	int intr;
	int minQuarter;
	s32 res;
	float busF;

	if (busfreq <= 0 || busfreq > (g_clock_ctx.pllFreqInt / 2))
		return SCE_ERROR_INVALID_VALUE;

	oldK1 = pspShiftK1();
	intr = sceKernelCpuSuspendIntr();
	if (sceKernelGetUserLevel() < 4)
		busfreq = g_clock_ctx.pllFreqInt / 2;

	if (busfreq < g_clock_ctx.busMin)
		busfreq = g_clock_ctx.busMin;
	else if (busfreq > g_clock_ctx.busMax)
		busfreq = g_clock_ctx.busMax;

	minQuarter = g_clock_ctx.pllFreqInt / 4;
	if (busfreq < minQuarter)
		busfreq = minQuarter;

	res = sceSysreg_driver_85B74FDA((float)busfreq);
	busF = sceSysreg_driver_96D74557();
	g_clock_ctx.busFreqFloat = busF;
	g_clock_ctx.busFreqInt = (s32)busF;
	scePower_driver_100A77A1(scePower_driver_C520F5DC());
	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return res;
}

static s32 changeClockFrequency(int pllfreq, int cpufreq, int busfreq)
{
	int oldK1;
	int i;
	s32 targetPllSel = -1;
	s32 res;

	if (scePower_driver_var_4A03DA50 != 0 &&
	    pllfreq == 222 && cpufreq == 222 && busfreq == 111) {
		switch (scePower_driver_var_4A03DA50) {
		case 1:
			pllfreq = 190; cpufreq = 190; busfreq = 95;
			break;
		case 2:
			pllfreq = 166; cpufreq = 166; busfreq = 83;
			break;
		case 3:
			pllfreq = 148; cpufreq = 148; busfreq = 74;
			break;
		case 4:
			pllfreq = 133; cpufreq = 133; busfreq = 66;
			break;
		default:
			Kprintf("illegal _scePowerChangeLowerClock222 setting\n");
			break;
		}
	}

	if ((u32)(pllfreq - 19) >= 315U || cpufreq < 1 || cpufreq >= 334 ||
	    busfreq < 1 || busfreq >= 168 || pllfreq < cpufreq || pllfreq < (busfreq * 2))
		return SCE_ERROR_INVALID_VALUE;

	if (pllfreq < g_clock_ctx.pllMin)
		pllfreq = g_clock_ctx.pllMin;
	else if (pllfreq > g_clock_ctx.pllMax)
		pllfreq = g_clock_ctx.pllMax;

	for (i = 0; i < 12; i++) {
		if ((g_clock_ctx.pllMask & (1U << g_pll_table[i].pllSel)) != 0) {
			if (g_pll_table[i].freq >= pllfreq) {
				pllfreq = g_pll_table[i].freq;
				targetPllSel = g_pll_table[i].pllSel;
				break;
			}
		}
	}
	if (targetPllSel < 0) {
		pllfreq = 333;
		targetPllSel = 5;
	}

	if (cpufreq < g_clock_ctx.cpuMin)
		cpufreq = g_clock_ctx.cpuMin;
	else if (cpufreq > g_clock_ctx.cpuMax)
		cpufreq = g_clock_ctx.cpuMax;
	if (cpufreq > pllfreq)
		cpufreq = pllfreq;

	if (busfreq < g_clock_ctx.busMin)
		busfreq = g_clock_ctx.busMin;
	else if (busfreq > g_clock_ctx.busMax)
		busfreq = g_clock_ctx.busMax;
	if (busfreq > (pllfreq / 2))
		busfreq = pllfreq / 2;

	oldK1 = pspShiftK1();
	res = sceKernelLockMutex(g_clock_ctx.mutexId, 1, NULL);
	if (res < 0) {
		pspSetK1(oldK1);
		return res;
	}

	if (scePower_2B51FE2F() != 0) {
		res = scePower_driver_E52B4362(pllfreq);
		if (res < 0) {
			if (sceKernelGetCompiledSdkVersion() > 0x0200000f) {
				sceKernelUnlockMutex(g_clock_ctx.mutexId, 1);
				pspSetK1(oldK1);
				return res;
			}
			pllfreq = 222;
			targetPllSel = 3;
			cpufreq = 222;
			busfreq = 111;
		}
	}

	if (sceKernelGetUserLevel() < 4)
		busfreq = pllfreq / 2;

	scePower_driver_6CF50928(0);
	if (g_clock_ctx.pllOutSelect != targetPllSel) {
		u32 evParam[6];
		int intr;
		float newPllF;

		evParam[0] = 8;
		evParam[1] = (u32)pllfreq;
		evParam[4] = 0;
		evParam[5] = 0;
		res = sceKernelSysEventDispatch(0x1000000, 0x1000000, "query",
						evParam, (int *)&evParam[4], 1,
						(SceSysEventHandler **)&evParam[5]);
		if (res < 0) {
			sceKernelSysEventDispatch(0x1000000, 0x1000001, "cancel", evParam, NULL, 0, NULL);
			scePower_driver_C3024FE6(0);
			sceKernelUnlockMutex(g_clock_ctx.mutexId, 1);
			pspSetK1(oldK1);
			return res;
		}

		if (targetPllSel == 5 && g_clock_ctx.mode40 == 0)
			g_clock_ctx.mode40 = 1;
		if ((u32)(targetPllSel - 4) < 2U && g_clock_ctx.mode38 == 0 && g_clock_ctx.val3c >= 0)
			g_clock_ctx.mode38 = 1;
		if (targetPllSel == 5 && g_clock_ctx.mode48 == 0) {
			if (g_clock_ctx.val4c >= 0 && g_clock_ctx.val4c != g_clock_ctx.val4e)
				sceDdr_driver_0BAAE4C5();
			g_clock_ctx.mode48 = 1;
		}

		sceKernelSysEventDispatch(0x1000000, 0x1000002, "start", evParam, NULL, 0, NULL);
		sceDisplayWaitVblankStart();
		intr = sceKernelCpuSuspendIntr();
		sceSysreg_driver_3F6F2CC7(0x1ff, 0x1ff);
		sceSysreg_driver_63E1EE9C(0x1ff, 0x1ff);
		sceKernelSysEventDispatch(0x1000000, 0x1000010, "phase0", evParam, NULL, 0, NULL);
		sceDdr_driver_CF07B9E2(targetPllSel);
		g_clock_ctx.pllOutSelect = targetPllSel;
		newPllF = sceSysregPllGetFrequency();
		g_clock_ctx.pllFreqFloat = newPllF;
		g_clock_ctx.pllFreqInt = (s32)newPllF;
		sceKernelSysEventDispatch(0x1000000, 0x1000011, "phase1", evParam, NULL, 0, NULL);
		sceKernelCpuResumeIntr(intr);
		sceKernelSysEventDispatch(0x1000000, 0x1000020, "end", evParam, NULL, 0, NULL);

		if (targetPllSel != 5 && g_clock_ctx.mode48 == 1) {
			if (g_clock_ctx.val4e >= 0 && g_clock_ctx.val4c != g_clock_ctx.val4e)
				sceDdr_driver_0BAAE4C5();
			g_clock_ctx.mode48 = 0;
		}
		if ((u32)(targetPllSel - 4) >= 2U && g_clock_ctx.mode38 == 1 && g_clock_ctx.val3e >= 0)
			g_clock_ctx.mode38 = 0;
		if (targetPllSel != 5 && g_clock_ctx.mode40 == 1)
			g_clock_ctx.mode40 = 0;
	}

	if ((pllfreq / 2) == busfreq) {
		float busF;
		sceSysreg_driver_63E1EE9C(0x1ff, 0x1ff);
		busF = sceSysreg_driver_96D74557();
		g_clock_ctx.busFreqFloat = busF;
		g_clock_ctx.busFreqInt = (s32)busF;
		scePower_driver_100A77A1(scePower_driver_C520F5DC());
	} else {
		scePowerSetBusClockFrequency(busfreq);
	}
	scePowerSetCpuClockFrequency(cpufreq);
	scePower_driver_C3024FE6(0);
	sceKernelUnlockMutex(g_clock_ctx.mutexId, 1);
	pspSetK1(oldK1);
	return 0;
}

s32 scePower_driver_100A77A1(int mode)
{
	int intr = sceKernelCpuSuspendIntr();
	int arg1 = 1, arg2 = 8, arg3 = 6;
	s32 res;

	if (mode == 0) {
		s32 bus = g_clock_ctx.busFreqInt;
		if (bus >= 75) {
			arg1 = (bus * 8000 - 75) / 1024;
			arg2 = 1;
			arg3 = 6;
		} else if (bus >= 50) {
			arg1 = (bus * 8000 - 75) / 512;
			arg2 = 2;
			arg3 = 3;
		} else if (bus >= 25) {
			arg1 = (bus * 8000 - 75) / 384;
			arg2 = 3;
			arg3 = 2;
		} else {
			arg1 = (bus * 8000 - 75) / 192;
			arg2 = 6;
			arg3 = 1;
		}
	}

	res = sceGeEdramSetRefreshParam(mode, arg1, arg2, arg3);
	sceKernelCpuResumeIntr(intr);
	if (res >= 0) {
		g_clock_ctx.geRefreshMode = mode;
		return 0;
	}
	return res;
}

s32 scePower_driver_C520F5DC(void)
{
	return g_clock_ctx.geRefreshMode;
}

static s32 exitClockSubsys(void)
{
	sceKernelDeleteMutex(g_clock_ctx.mutexId);
	return 0;
}

static s32 lockClockMutex(void)
{
	return sceKernelLockMutex(g_clock_ctx.mutexId, 1, NULL);
}

static s32 unlockClockMutex(void)
{
	return sceKernelUnlockMutex(g_clock_ctx.mutexId, 1);
}

static s32 clockRebootPhase(int phase)
{
	if (phase == 1)
		sceKernelUnlockMutex(g_clock_ctx.mutexId, 1);
	else if (phase == 2)
		sceKernelLockMutex(g_clock_ctx.mutexId, 1, NULL);
	return 0;
}

static void saveRestoreClockSysreg(int save)
{
	(void)save;
	sceSysreg_driver_2458B6AC(&g_clock_ctx.savedSysreg[0], &g_clock_ctx.savedSysreg[1]);
	sceSysreg_driver_7CF05E81(&g_clock_ctx.savedSysreg[2], &g_clock_ctx.savedSysreg[3]);
	g_clock_ctx.savedGeRefreshMode = g_clock_ctx.geRefreshMode;
	scePower_driver_100A77A1(0);
}

static s32 restoreClockSysregPhase(int phase)
{
	if (phase == 0) {
		sceSysreg_driver_3F6F2CC7((int)g_clock_ctx.savedSysreg[0],
					  (int)g_clock_ctx.savedSysreg[1]);
		sceSysreg_driver_63E1EE9C((int)g_clock_ctx.savedSysreg[2],
					  (int)g_clock_ctx.savedSysreg[3]);
	} else if (phase == 1) {
		scePower_driver_100A77A1(g_clock_ctx.savedGeRefreshMode);
	}
	return 0;
}

s32 scePower_driver_D7DD9D38(void)
{
	return (g_clock_ctx.mode38 != 0) ? g_clock_ctx.val3c : g_clock_ctx.val3e;
}

s32 scePower_driver_BADA8332(s32 *out0, s32 *out1)
{
	if (out0 != NULL)
		*out0 = g_clock_ctx.val3c;
	if (out1 != NULL)
		*out1 = g_clock_ctx.val3e;
	return 0;
}

s32 scePower_driver_12F8302D(s32 a0, s32 a1)
{
	if (a0 != -1)
		g_clock_ctx.val3c = (s16)a0;
	if (a1 != -1)
		g_clock_ctx.val3e = (s16)a1;
	return 0;
}

s32 scePower_driver_9127E5B2(void)
{
	return (g_clock_ctx.mode40 != 0) ? g_clock_ctx.val44 : g_clock_ctx.val46;
}

s32 scePower_driver_75906F9A(s32 *out0, s32 *out1)
{
	if (out0 != NULL)
		*out0 = g_clock_ctx.val44;
	if (out1 != NULL)
		*out1 = g_clock_ctx.val46;
	return 0;
}

s32 scePower_driver_018AB235(s32 a0, s32 a1)
{
	if (a0 != -1)
		g_clock_ctx.val44 = (s16)a0;
	if (a1 != -1)
		g_clock_ctx.val46 = (s16)a1;
	return 0;
}

s32 scePower_driver_0655D7C3(void)
{
	return (g_clock_ctx.mode48 != 0) ? g_clock_ctx.val4c : g_clock_ctx.val4e;
}

s32 scePower_driver_16F965C9(s32 *out0, s32 *out1)
{
	if (out0 != NULL)
		*out0 = g_clock_ctx.val4c;
	if (out1 != NULL)
		*out1 = g_clock_ctx.val4e;
	return 0;
}

s32 scePower_driver_D13377F7(s32 a0, s32 a1)
{
	if (a0 != -1)
		g_clock_ctx.val4c = (s16)a0;
	if (a1 != -1)
		g_clock_ctx.val4e = (s16)a1;
	return 0;
}

s32 scePower_driver_DF904CDE(s32 a0, s32 a1)
{
	if (a0 != -1)
		g_clock_ctx.cpuMin = (s16)a0;
	if (a1 != -1)
		g_clock_ctx.cpuMax = (s16)a1;
	return 0;
}

s32 scePower_driver_EEFB2ACF(s32 a0, s32 a1)
{
	if (a0 != -1)
		g_clock_ctx.busMin = (s16)a0;
	if (a1 != -1)
		g_clock_ctx.busMax = (s16)a1;
	return 0;
}

s32 scePower_driver_B7000C75(s32 a0, s32 a1)
{
	if (a0 != -1)
		g_clock_ctx.pllMin = (s16)a0;
	if (a1 != -1)
		g_clock_ctx.pllMax = (s16)a1;
	return 0;
}

s32 scePower_driver_13D7CCE4(u32 mask)
{
	g_clock_ctx.pllMask = mask;
	return 0;
}

s32 scePowerGetCpuClockFrequencyInt(void)
{
	return g_clock_ctx.cpuFreqInt;
}

float scePowerGetCpuClockFrequencyFloat(void)
{
	return g_clock_ctx.cpuFreqFloat;
}

s32 scePowerGetBusClockFrequencyInt(void)
{
	return g_clock_ctx.busFreqInt;
}

float scePowerGetBusClockFrequencyFloat(void)
{
	return g_clock_ctx.busFreqFloat;
}

s32 scePower_34F9C463(void)
{
	return g_clock_ctx.pllFreqInt;
}

float scePower_EA382A27(void)
{
	return g_clock_ctx.pllFreqFloat;
}

s32 scePowerSetClockFrequency(int pllfreq, int cpufreq, int busfreq)
{
	if (g_clock_ctx.sm1Ops != NULL)
		sceKernelDelayThread(60000000);
	return changeClockFrequency(pllfreq, cpufreq, busfreq);
}

s32 scePower_A4E93389(int pllfreq, int cpufreq, int busfreq)
{
	return changeClockFrequency(pllfreq, cpufreq, busfreq);
}

s32 scePower_545A7F3C(int pllfreq, int cpufreq, int busfreq)
{
	return changeClockFrequency(pllfreq, cpufreq, busfreq);
}

s32 scePower_EBD177D6(int pllfreq, int cpufreq, int busfreq)
{
	return changeClockFrequency(pllfreq, cpufreq, busfreq);
}

s32 scePower_469989AD(int pllfreq, int cpufreq, int busfreq)
{
	if (sceKernelGetModel() != 0 || sceKernelDipsw(11) == 1)
		scePower_driver_114B75AB(0);
	return changeClockFrequency(pllfreq, cpufreq, busfreq);
}

static s32 updateBatteryEvent(const u32 *evData, u32 *statusFlags)
{
	u32 st = evData[9];

	g_batt_ctx.unk54 = -1;
	g_batt_ctx.status34 = st;
	g_batt_ctx.temp = -1;
	g_batt_ctx.elec = -1;
	g_batt_ctx.volt = -1;

	if ((st & 2U) != 0) {
		g_batt_ctx.state38 = 1;
		if (g_batt_ctx.disabledFlag == 0) {
			g_batt_ctx.unk3c = evData[10];
			g_batt_ctx.remainCap = (s32)evData[11];
			g_batt_ctx.unk4c = (s32)evData[11];
			g_batt_ctx.cycleCount = -1;
			g_batt_ctx.fullCap = (s32)evData[12];
			g_batt_ctx.lifePercent = scePowerGetBatteryLifePercent();
		}
		*statusFlags = (*statusFlags & ~0x7fU) | (u32)g_batt_ctx.lifePercent | 0x80U;
	} else {
		g_batt_ctx.unk4c = -1;
		g_batt_ctx.state38 = 0;
		g_batt_ctx.unk3c = 0;
		g_batt_ctx.remainCap = -1;
		g_batt_ctx.lifePercent = -1;
		g_batt_ctx.fullCap = -1;
		g_batt_ctx.cycleCount = -1;
		*statusFlags &= ~0xffU;
	}
	return 0;
}

s32 scePowerGetBatteryLifePercent(void)
{
	return (s32)readKermitSeqVal((volatile u32 *)0xbfc00170U);
}

s32 scePowerGetBatteryTemp(void)
{
	if (g_batt_ctx.disabledFlag != 0)
		return SCE_ERROR_NOT_FOUND;
	if (g_batt_ctx.state38 == 0)
		return SCE_POWER_ERROR_NO_BATTERY;
	if (g_batt_ctx.state38 == 1)
		return SCE_POWER_ERROR_DETECTING;
	return g_batt_ctx.temp;
}

s32 scePowerGetBatteryElec(s32 *outElec)
{
	if (g_batt_ctx.disabledFlag != 0)
		return SCE_ERROR_NOT_FOUND;
	if (((s32)((pspGetK1() << 11) & (u32)outElec)) < 0)
		return SCE_ERROR_PRIV_REQUIRED;
	if (g_batt_ctx.state38 == 0)
		return SCE_POWER_ERROR_NO_BATTERY;
	if (g_batt_ctx.state38 == 1)
		return SCE_POWER_ERROR_DETECTING;
	*outElec = g_batt_ctx.elec;
	return 0;
}

s32 scePower_CB49F5CE(void)
{
	if (g_batt_ctx.disabledFlag != 0)
		return SCE_ERROR_NOT_FOUND;
	if (g_batt_ctx.state38 == 0)
		return SCE_POWER_ERROR_NO_BATTERY;
	if (g_batt_ctx.state38 == 1)
		return SCE_POWER_ERROR_DETECTING;
	return g_batt_ctx.cycleCount;
}

static s32 initBatterySubsys(u32 a0, u32 a1)
{
	memset(&g_batt_ctx, 0, sizeof(g_batt_ctx));
	g_batt_ctx.disabledFlag = a1;
	g_batt_ctx.unk10 = a0;
	g_batt_ctx.alarmId = -1;
	g_batt_ctx.unk30 = -1;
	g_batt_ctx.fullCap = -1;
	g_batt_ctx.cycleCount = -1;
	return 0;
}

static s32 exitBatterySubsys(void)
{
	if (g_batt_ctx.alarmId > 0) {
		sceKernelCancelAlarm(g_batt_ctx.alarmId);
		g_batt_ctx.alarmId = -1;
	}
	return 0;
}

static s32 setBatteryThresholds(u32 a0, u32 a1)
{
	g_batt_ctx.unk00 = a0;
	g_batt_ctx.unk04 = a1;
	return 0;
}

static s32 resetBatteryState(void)
{
	int intr = sceKernelCpuSuspendIntr();
	g_batt_ctx.unk30 = -1;
	g_batt_ctx.state38 = 0;
	g_batt_ctx.unk08 = 0;
	sceKernelCpuResumeIntr(intr);
	return 0;
}

s32 scePower_27F3292C(void)
{
	return 0;
}

s32 scePower_E8E4E204(void)
{
	return 0;
}

s32 scePower_B999184C(void)
{
	return 0;
}

s32 scePower_driver_5F5006D2(void)
{
	return 0;
}

s32 scePower_driver_10CE273F(void)
{
	return 0;
}

s32 scePower_driver_EF751B4A(void)
{
	return 0;
}

s32 scePower_driver_72D1B53A(void)
{
	return SCE_ERROR_NOT_FOUND;
}

s32 scePower_driver_7EAA4247(void)
{
	return SCE_ERROR_NOT_FOUND;
}

s32 scePowerIsPowerOnline(void)
{
	u32 val = readKermitSeqVal((volatile u32 *)0xbfc00100U);
	return (val >> 1) & 1;
}

s32 scePowerIsBatteryExist(void)
{
	u32 val = readKermitSeqVal((volatile u32 *)0xbfc00100U);
	return (val >> 2) & 1;
}

s32 scePowerIsBatteryCharging(void)
{
	u32 val = readKermitSeqVal((volatile u32 *)0xbfc00100U);
	return (val >> 3) & 1;
}

s32 scePowerGetBatteryChargingStatus(void)
{
	u32 val = readKermitSeqVal((volatile u32 *)0xbfc00100U);
	return (val >> 3) & 1;
}

s32 scePowerIsLowBattery(void)
{
	u32 val = readKermitSeqVal((volatile u32 *)0xbfc00100U);
	return (val >> 4) & 1;
}

s32 scePowerIsSuspendRequired(void)
{
	u32 val = readKermitSeqVal((volatile u32 *)0xbfc00100U);
	return (val >> 5) & 1;
}

u32 scePower_driver_071160B1(void)
{
	return g_batt_ctx.disabledFlag;
}

s32 scePowerGetBatteryRemainCapacity(void)
{
	if (g_batt_ctx.state38 == 0)
		return SCE_POWER_ERROR_NO_BATTERY;
	if (g_batt_ctx.state38 == 1)
		return SCE_POWER_ERROR_DETECTING;
	return g_batt_ctx.remainCap;
}

s32 scePowerGetBatteryFullCapacity(void)
{
	if (g_batt_ctx.state38 == 0)
		return SCE_POWER_ERROR_NO_BATTERY;
	if (g_batt_ctx.state38 == 1)
		return SCE_POWER_ERROR_DETECTING;
	return g_batt_ctx.fullCap;
}

s32 scePowerGetBatteryLifeTime(void)
{
	return (s32)readKermitSeqVal((volatile u32 *)0xbfc0017cU);
}

s32 scePowerGetBatteryVolt(void)
{
	if (g_batt_ctx.state38 == 0)
		return SCE_POWER_ERROR_NO_BATTERY;
	if (g_batt_ctx.state38 == 1)
		return SCE_POWER_ERROR_DETECTING;
	return g_batt_ctx.volt;
}

s32 scePower_23436A4A(void)
{
	return 0x802B0010;
}
