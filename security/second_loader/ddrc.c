/*
 * ddrc.c — LPDDR2 Memory Controller (DDRC) Initialization & Training
 *
 * Address range: 0x80841c..0x808789
 *
 * Initializes the pervasive DDR controller clock/reset domain, reads LPDDR2
 * Mode Registers MR5 (manufacturer ID) and MR8 (density/I/O width), programs
 * the geometry-specific timing tables into 0xe6000000..0xe60083b4, and brings
 * all memory channels into normal operating state.
 */

#include "second_loader.h"

typedef struct SceDdrcRegPair { /* name ours, DDRC MMIO register/value pair */
	u32	reg;
	u32	val;
} SceDdrcRegPair;

typedef struct SceDdrcMrwPair { /* name ours, LPDDR2 Mode Register Write pair */
	u32	mr_addr;
	u32	mr_val;
} SceDdrcMrwPair;

typedef struct SceDdrcConfigEntry { /* name ours, LPDDR2 geometry configuration */
	u32			config_id;
	const SceDdrcRegPair	*regs;
	const SceDdrcMrwPair	*mrw;
} SceDdrcConfigEntry;

/* .data: 0x80ca10..0x80ccb3 */
static SceDdrcRegPair s_ddrcRegsConfig0[];
static SceDdrcMrwPair s_ddrcMrwConfig0[];
static SceDdrcRegPair s_ddrcRegsConfig1[];
static SceDdrcMrwPair s_ddrcMrwConfig1[];
static SceDdrcRegPair s_ddrcRegsConfig2[];
static SceDdrcMrwPair s_ddrcMrwConfig2[];
static SceDdrcRegPair s_ddrcRegsConfig3[];
static SceDdrcMrwPair s_ddrcMrwConfig3[];

static SceDdrcConfigEntry s_ddrcConfigs[5] = {
	{ 3, s_ddrcRegsConfig3, s_ddrcMrwConfig3 },
	{ 2, s_ddrcRegsConfig2, s_ddrcMrwConfig2 },
	{ 1, s_ddrcRegsConfig1, s_ddrcMrwConfig1 },
	{ 0, s_ddrcRegsConfig0, s_ddrcMrwConfig0 },
	{ 4, NULL, NULL }
};

static SceDdrcRegPair s_ddrcPreRegs[35] = {
	{ 0xe6000020U, 0x00000000U },
	{ 0xe6000024U, 0x00010003U },
	{ 0xe6000030U, 0x00000000U },
	{ 0xe6000034U, 0x00010003U },
	{ 0xe6000040U, 0x00000000U },
	{ 0xe6000044U, 0x00010003U },
	{ 0xe6000050U, 0x00000000U },
	{ 0xe6000054U, 0x00010003U },
	{ 0xe6000060U, 0x00000000U },
	{ 0xe6000064U, 0x00010003U },
	{ 0xe6000070U, 0x00000000U },
	{ 0xe6000074U, 0x00010003U },
	{ 0xe6000080U, 0x00000000U },
	{ 0xe6000084U, 0x00010003U },
	{ 0xe6000090U, 0x00000000U },
	{ 0xe6000094U, 0x00010003U },
	{ 0xe6008038U, 0x00000005U },
	{ 0xe6008040U, 0x00000004U },
	{ 0xe6008050U, 0x00000005U },
	{ 0xe6008058U, 0x00000003U },
	{ 0xe6008060U, 0x00000018U },
	{ 0xe6008068U, 0x00000003U },
	{ 0xe6008088U, 0x0000000cU },
	{ 0xe6008090U, 0x0000000cU },
	{ 0xe6008098U, 0x00000010U },
	{ 0xe60080b0U, 0x00000006U },
	{ 0xe60080e0U, 0x0000002eU },
	{ 0xe60080e8U, 0x0000002eU },
	{ 0xe60080f0U, 0x00000002U },
	{ 0xe6008150U, 0x00000002U },
	{ 0xe6008158U, 0x00000002U },
	{ 0xe6008160U, 0x0000000bU },
	{ 0xe6008168U, 0x00000005U },
	{ 0xe6008178U, 0x00000006U },
	{ 0xffffffffU, 0x00000000U }
};

static SceDdrcMrwPair s_ddrcPreMrw[2] = {
	{ 2, 3 },
	{ 0xffffffffU, 0 }
};

static SceDdrcRegPair s_ddrcRegsConfig0[4] = {
	{ 0xe6000000U, 0x230b0111U },
	{ 0xe6008140U, 0x00000002U },
	{ 0xe6008048U, 0x0000002dU },
	{ 0xffffffffU, 0x00000000U }
};

static SceDdrcMrwPair s_ddrcMrwConfig0[1] = {
	{ 0xffffffffU, 0 }
};

static SceDdrcRegPair s_ddrcRegsConfig1[4] = {
	{ 0xe6000000U, 0x220b0111U },
	{ 0xe6008140U, 0x00000004U },
	{ 0xe6008048U, 0x0000002dU },
	{ 0xffffffffU, 0x00000000U }
};

static SceDdrcMrwPair s_ddrcMrwConfig1[1] = {
	{ 0xffffffffU, 0 }
};

static SceDdrcRegPair s_ddrcRegsConfig2[4] = {
	{ 0xe6000000U, 0x130b0111U },
	{ 0xe6008140U, 0x00000004U },
	{ 0xe6008048U, 0x0000002bU },
	{ 0xffffffffU, 0x00000000U }
};

static SceDdrcMrwPair s_ddrcMrwConfig2[1] = {
	{ 0xffffffffU, 0 }
};

static SceDdrcRegPair s_ddrcRegsConfig3[4] = {
	{ 0xe6000000U, 0x000c0111U },
	{ 0xe6008140U, 0x00000006U },
	{ 0xe6008048U, 0x0000002aU },
	{ 0xffffffffU, 0x00000000U }
};

static SceDdrcMrwPair s_ddrcMrwConfig3[2] = {
	{ 3, 4 },
	{ 0xffffffffU, 0 }
};

static SceDdrcRegPair s_ddrcPostRegs[19] = {
	{ 0xe60080c8U, 0x00001fffU },
	{ 0xe6008200U, 0x00000076U },
	{ 0xe6008208U, 0x00000076U },
	{ 0xe6008210U, 0x00000076U },
	{ 0xe6008218U, 0x00000076U },
	{ 0xe6008220U, 0x0000005eU },
	{ 0xe6008228U, 0x0000005eU },
	{ 0xe6008230U, 0x0000005eU },
	{ 0xe6008238U, 0x0000005eU },
	{ 0xe6008260U, 0x00000067U },
	{ 0xe60083b0U, 0x0d068235U },
	{ 0xe60083b4U, 0x014e014eU },
	{ 0xe6008030U, 0x00000514U },
	{ 0xe60081a0U, 0x0000001eU },
	{ 0xe60081a8U, 0x00000003U },
	{ 0xe60081b0U, 0x0000001eU },
	{ 0xe60081b8U, 0x00000003U },
	{ 0xe6008070U, 0x00000006U },
	{ 0xffffffffU, 0x00000000U }
};

/* .sbss: 0x80db34..0x80db43 */
static s8 s_ddrcMr8Density;
static s8 s_ddrcMr5VendorId;
static u32 s_ddrcTotalSize;
static u32 s_ddrcSocRevCached;
static u32 s_ddrcSocRevValue;

/* 0x80841c: operatorDeleteNoop (name ours, C++ operator delete stub) */
void operatorDeleteNoop(void *ptr)
{
	(void)ptr;
}

/* 0x80841e: ddrcNoop (name ours) */
void ddrcNoop(void)
{
}

/* 0x808420: ddrcGetSocRevision (name ours) */
u32 ddrcGetSocRevision(void)
{
	if (s_ddrcSocRevCached == 0) {
		s_ddrcSocRevValue = MMIO_R32(0xe3100000U);
		s_ddrcSocRevCached = 1;
	}
	return s_ddrcSocRevValue;
}

/* 0x808440: ddrcPervasiveInit (name ours) */
int ddrcPervasiveInit(u32 clkMode)
{
	MMIO_W32(0xe3102160U, 0);
	while (MMIO_R32(0xe3102160U) != 0)
		;

	MMIO_W32(0xe3101160U, 1);
	while (MMIO_R32(0xe3101160U) != 1)
		;

	MMIO_W32(0xe3100044U, 0);
	while (MMIO_R32(0xe3100044U) != 0)
		;

	MMIO_W32(0xe3103090U, clkMode & 3U);
	while (MMIO_R32(0xe3103090U) != (clkMode & 3U))
		;

	MMIO_W32(0xe3102160U, 1);
	while (MMIO_R32(0xe3102160U) != 1)
		;

	MMIO_W32(0xe3101160U, 0);
	while (MMIO_R32(0xe3101160U) != 0)
		;

	return 0;
}

/* 0x8084d0: ddrcApplyRegTable (name ours) */
int ddrcApplyRegTable(const SceDdrcRegPair *table)
{
	for (; table->reg != 0xffffffffU; table++) {
		MMIO_W32(table->reg, table->val);
		while (MMIO_R32(table->reg) != table->val)
			;
	}
	return 0;
}

/* 0x8084f8: ddrcSetChannelState (name ours) */
void ddrcSetChannelState(u32 numChannels, u8 cmdState, u32 expectedState)
{
	volatile u8 *chReg = (volatile u8 *)0xe6008000U;
	u32 i;

	for (i = 0; i < numChannels; i++) {
		*chReg = cmdState & 7U;
		chReg += 8;
	}

	for (i = 0; i < numChannels; i++) {
		while (((MMIO_R32(0xe6008020U) >> (i * 4U)) & 0x0fU) != (expectedState & 0xffU))
			cmepDelayUs(1);
	}
}

/* 0x808560: ddrcWriteMrw (name ours) */
void ddrcWriteMrw(u32 channel, u8 mrAddr, u8 mrVal)
{
	MMIO_W32(0xe6008100U + channel * 8U, ((u32)mrVal << 8) | (u32)mrAddr);
}

/* 0x808578: ddrcApplyMrwTable (name ours) */
int ddrcApplyMrwTable(u32 numChannels, const SceDdrcMrwPair *table)
{
	u32 ch;
	const SceDdrcMrwPair *p;

	for (ch = 0; ch < numChannels; ch++) {
		for (p = table; p->mr_addr != 0xffffffffU; p++)
			ddrcWriteMrw(ch, (u8)p->mr_addr, (u8)p->mr_val);
	}
	return 0;
}

/* 0x8085c0: ddrcReadMrr (name ours) */
u32 ddrcReadMrr(u32 channel, u8 mrAddr)
{
	u32 reg = 0xe6008120U + channel * 8U;

	MMIO_W32(reg, mrAddr);
	return (MMIO_R32(reg) & 0xff00U) >> 8;
}

/* 0x8085da: ddrcDetectMemoryConfig (name ours) */
u32 ddrcDetectMemoryConfig(u32 numChannels)
{
	u32 mr5 = ddrcReadMrr(0, 5);
	u32 mr8;

	s_ddrcMr5VendorId = (s8)mr5;
	mr8 = ddrcReadMrr(0, 8);
	s_ddrcMr8Density = (s8)mr8;

	if ((mr8 & 3U) == 0) {
		if ((mr5 & 7U) == 1U) {
			u32 density = (mr8 & 0x3cU) >> 2;

			if (density == 4U) {
				if (numChannels == 4U)
					return 0;
			} else if (density == 5U) {
				if (numChannels == 2U)
					return 2;
				if (numChannels == 4U)
					return 1;
			}
		} else if ((mr5 & 7U) == 3U && (mr8 & 0x3cU) == 0x18U) {
			return 4U - (numChannels == 1U);
		}
	}
	return 4;
}

/* 0x808662: ddrcInit (name ours) */
int ddrcInit(u32 clkMode, int isWarmBoot)
{
	u32 numChannels = 4;
	u32 ch;
	u32 configId;
	const SceDdrcConfigEntry *cfg;
	int res;

	if ((s32)ddrcGetSocRevision() < 0) {
		u32 chField = ddrcGetSocRevision() & 0x30000000U;

		if (chField == 0x10000000U)
			numChannels = 1;
		else if (chField == 0x20000000U)
			numChannels = 4;
		else if (chField == 0U)
			numChannels = 2;
		else
			return 0x800f0039;
	}

	res = ddrcPervasiveInit(clkMode);
	if (res != 0)
		return res;

	if (isWarmBoot == 1)
		ddrcSetChannelState(numChannels, 5, 4);
	ddrcSetChannelState(numChannels, 2, 2);

	MMIO_W32(0xe6008180U, 1);
	while (MMIO_R32(0xe6008180U) != 1)
		;

	for (ch = 0; ch < numChannels; ch++)
		ddrcWriteMrw(ch, 2, 3);

	configId = ddrcDetectMemoryConfig(numChannels);
	if (configId == 4)
		return 0x800f0039;

	s_ddrcTotalSize = numChannels * (0x800000U << (((u8)s_ddrcMr8Density & 0x3cU) >> 2));

	for (cfg = s_ddrcConfigs; cfg->config_id != configId; cfg++) {
		if (cfg->config_id == 4)
			return SCE_SBL_ERROR_SL_ENOENT;
	}

	res = ddrcApplyRegTable(s_ddrcPreRegs);
	if (res == 0) {
		res = ddrcApplyRegTable(cfg->regs);
		if (res == 0) {
			res = ddrcApplyRegTable(s_ddrcPostRegs);
			if (res == 0) {
				res = ddrcApplyMrwTable(numChannels, s_ddrcPreMrw);
				if (res == 0) {
					res = ddrcApplyMrwTable(numChannels, cfg->mrw);
					if (res == 0)
						ddrcSetChannelState(numChannels, 1, 1);
				}
			}
		}
	}
	return res;
}

/* 0x808778: ddrcGetTotalSize (name ours) */
u32 ddrcGetTotalSize(void)
{
	return s_ddrcTotalSize;
}

/* 0x80877e: ddrcGetVendorId (name ours) */
int ddrcGetVendorId(void)
{
	return (int)s_ddrcMr5VendorId;
}

/* 0x808784: ddrcGetDensityInfo (name ours) */
int ddrcGetDensityInfo(void)
{
	return (int)s_ddrcMr8Density;
}
