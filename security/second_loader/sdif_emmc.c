/*
 * sdif_emmc.c — SDIF Host Controller & eMMC Storage Driver
 *
 * Address range: 0x8070ea..0x80841b
 *
 * Implements the SD Host Controller (SDIF0 at 0xe0b00000, SDIF1 at 0xe0c00000)
 * register state machine, PIO and ADMA2 transfers, JEDEC eMMC probing (CMD0,
 * CMD1, CMD2, CMD3, CMD9, CMD7, CMD8, CMD16, CMD6, CMD23, CMD17/CMD18, CMD12,
 * CMD13), and the eMMC inline crypto engine setup at 0xe0070000.
 */

#include "second_loader.h"

#define SCE_SDIF_ERROR_TIMEOUT	0x80320002U
#define SCE_SDIF_ERROR_IO	0x80320003U
#define SCE_SDIF_ERROR_CARD_BASE 0x80320100U

typedef struct SceSdifCommand { /* name ours, SDIF command packet */
	u32	size;
	u32	flags;
	u32	cmd_index;
	u32	argument;
	u32	response[4];
	void	*data_buf;
	u32	block_size;
	u32	block_count;
	u32	error_code;
	u32	state;
} SceSdifCommand;

typedef struct SceSdifHostContext { /* name ours, SDIF host controller state */
	u32			ocr_voltage_mask;
	u32			base_clock_hz;
	u32			max_clock_hz;
	u16			rca;
	u8			host_id;
	u8			cmd_done_flag;
	SceSdifCommand		*cur_cmd;
	SceSdifCommand		*data_cmd;
	volatile u8		*regs;
} SceSdifHostContext;

typedef struct SceSdifMmcDev { /* name ours, eMMC device context */
	SceSdifHostContext	*host;
	u32			addr_mode_flags;
	u8			cid[16];
	u8			csd[128];
	u8			ext_csd[512];
	u8			csd_structure;
	u8			spec_vers;
	u16			pad_29a;
	u32			taac_ns;
	u32			nsac_clks;
	u32			tran_speed_hz;
	u16			ccc;
	u16			pad_2aa;
	u32			c_size_blocks;
	u8			read_bl_len;
	u8			read_bl_partial;
	u8			write_blk_misalign;
	u8			read_blk_misalign;
	u8			r2w_factor;
	u8			write_bl_len;
	u8			write_bl_partial;
	u8			pad_2b7;
	u32			sec_count;
	u32			hs_max_dtr;
} SceSdifMmcDev;

/* .rodata: 0x80c364..0x80c3c3 */
static const u8 s_csdTranSpeedMantissa[16] = {
	0, 10, 12, 13, 15, 20, 26, 30, 35, 40, 45, 52, 55, 60, 70, 80
};

static const u32 s_csdTranSpeedUnit[8] = {
	10000, 100000, 1000000, 10000000, 0, 0, 0, 0
};

static const u8 s_csdTaacMantissa[16] = {
	0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80
};

static const u32 s_csdTaacUnit[8] = {
	1, 10, 100, 1000, 10000, 100000, 1000000, 10000000
};

/* .sdata: 0x80cd10..0x80cd1b */
static u32 s_sdifAsyncResult = 0x800f002cU;
static u8 *s_sdifAdmaBuf = NULL;
static u32 s_sdifAdmaBufSize = 0;

/* .bss: 0x80d47c..0x80d78f */
static SceSdifMmcDev s_mmcDev;
static SceSdifHostContext s_sdifHost0;
static SceSdifHostContext s_sdifHost1;
static SceSdifHostContext s_sdifHost1Alt;

/* .sbss: 0x80db1c..0x80db33 */
static u32 s_sdifUseAdmaMode;
static int (*s_sdifAsyncCallback)(void);
static SceSdifMmcDev *s_activeMmcDev;
static u32 s_sdifEmmcInitialized;
static u32 s_sdifSocRevCached;
static u32 s_sdifSocRevValue;

/* 0x8070ea: sdifExtractCsdBits (name ours) */
u32 sdifExtractCsdBits(u32 bitOffset, u32 bitCount)
{
	const s8 *p = (const s8 *)s_mmcDev.csd + (bitOffset >> 3);
	u32 val = 0;
	u32 mask = (1U << (bitOffset & 7U)) & 0xffU;
	u32 shift = 0;
	u32 i;

	for (i = 0; i < bitCount; i++) {
		if (((s32)*p & mask) != 0)
			val |= (1U << shift);
		mask = (mask & 0x7fU) << 1;
		if (mask == 0) {
			p++;
			mask = 1;
		}
		shift++;
	}
	return val;
}

/* 0x80712a: sdifSetAsyncCallback (name ours) */
int sdifSetAsyncCallback(int (*cb)(void))
{
	s_sdifAsyncCallback = cb;
	return 0;
}

/* 0x807132: sdifPopAsyncResult (name ours) */
int sdifPopAsyncResult(u32 *outResult)
{
	*outResult = s_sdifAsyncResult;
	s_sdifAsyncResult = 0x800f002cU;
	return 0;
}

/* 0x80714c: sdifEnableAdmaMode (name ours) */
void sdifEnableAdmaMode(void *admaBuf, u32 admaBufSize)
{
	s_sdifUseAdmaMode = 1;
	sdifSetAdmaBuffer(admaBuf, admaBufSize);
}

/* 0x807156: sdifGetMmcDevContext (name ours) */
SceSdifMmcDev *sdifGetMmcDevContext(void)
{
	return &s_mmcDev;
}

/* 0x80715c: sdifSendMmcOpCond (name ours, CMD1) */
int sdifSendMmcOpCond(SceSdifMmcDev *dev, u32 ocrArg, u32 *outOcr)
{
	SceSdifCommand cmd;
	u32 tries = 0;
	int res;

	cmd.size = sizeof(cmd);
	cmd.flags = 0x42;
	cmd.cmd_index = 1;
	cmd.argument = ocrArg;
	cmd.data_buf = NULL;

	for (;;) {
		res = sdifExecuteCmdNoRetry(dev->host, &cmd, 0);
		if (res < 0)
			return res;

		if (s_sdifAsyncCallback != NULL) {
			s_sdifAsyncResult = (u32)s_sdifAsyncCallback();
			s_sdifAsyncCallback = NULL;
		}

		if (ocrArg == 0 || (s32)cmd.response[0] < 0)
			break;

		sdifDelayUs(1000);
		tries++;
		if (tries == 100)
			return (int)SCE_SDIF_ERROR_TIMEOUT;
	}

	if (outOcr != NULL)
		*outOcr = cmd.response[0];
	return 0;
}

/* 0x8071e4: sdifAllSendCid (name ours, CMD2) */
int sdifAllSendCid(SceSdifMmcDev *dev, void *outCid)
{
	return sdifCmd2AllSendCid(dev->host, outCid);
}

/* 0x8071ee: sdifSetRelativeAddr (name ours, CMD3) */
int sdifSetRelativeAddr(SceSdifMmcDev *dev, u32 rca)
{
	SceSdifCommand cmd;
	int res;

	cmd.size = sizeof(cmd);
	cmd.flags = 0x13;
	cmd.cmd_index = 3;
	cmd.argument = rca << 16;
	res = sdifExecuteCmdNoRetry(dev->host, &cmd, 5);
	if (res >= 0) {
		res = (int)sdifCheckCardStatusErrors(cmd.response[0]);
		if (res > 0)
			res = 0;
	}
	return res;
}

/* 0x807226: sdifMmcSwitch (name ours, CMD6) */
int sdifMmcSwitch(SceSdifMmcDev *dev, u32 cmdSet, u32 index, u32 value)
{
	SceSdifCommand cmd;
	int res;

	cmd.size = sizeof(cmd);
	cmd.flags = 0x2b;
	cmd.cmd_index = 6;
	cmd.argument = 0x03000000U | ((index & 0xffU) << 16) |
		       ((value & 0xffU) << 8) | (cmdSet & 3U);
	res = sdifExecuteCmdNoRetry(dev->host, &cmd, 5);
	if (res > 0)
		res = 0;
	return res;
}

/* 0x80726a: sdifSelectCard (name ours, CMD7) */
int sdifSelectCard(SceSdifMmcDev *dev, int select)
{
	return sdifCmd7SelectCard(dev->host, select);
}

/* 0x807274: sdifSendExtCsd (name ours, CMD8) */
int sdifSendExtCsd(SceSdifMmcDev *dev, void *extCsdBuf)
{
	SceSdifCommand cmd;
	int res;

	cmd.size = sizeof(cmd);
	cmd.flags = 0x114;
	cmd.cmd_index = 8;
	cmd.argument = 0;
	cmd.data_buf = extCsdBuf;
	cmd.block_size = 0x200;
	cmd.block_count = 0;
	res = sdifExecuteCmdNoRetry(dev->host, &cmd, 5);
	if (res > 0)
		res = 0;
	return res;
}

/* 0x8072ae: sdifSendCsd (name ours, CMD9) */
int sdifSendCsd(SceSdifMmcDev *dev, void *outCsd)
{
	return sdifCmd9SendCsd(dev->host, outCsd);
}

/* 0x8072b8: sdifSetBlockLen (name ours, CMD16) */
int sdifSetBlockLen(SceSdifMmcDev *dev, u32 blockLen)
{
	return sdifCmd16SetBlockLen(dev->host, blockLen);
}

/* 0x8072c2: sdifSetBlockCount (name ours, CMD23) */
int sdifSetBlockCount(SceSdifMmcDev *dev, u32 blockCount)
{
	return sdifCmd23SetBlockCount(dev->host, blockCount);
}

/* 0x8072cc: sdifReadSectors (name ours, CMD17/CMD18) */
int sdifReadSectors(SceSdifMmcDev *dev, u32 startSector, void *dst, u32 sectorCount)
{
	SceSdifCommand cmd;
	int res;

	cmd.flags = (s_sdifUseAdmaMode != 0) ? 0x514U : 0x114U;
	cmd.size = sizeof(cmd);
	cmd.cmd_index = (sectorCount == 1) ? 17U : 18U;
	if ((dev->addr_mode_flags & 1U) == 0)
		startSector <<= 9;
	cmd.argument = startSector;
	cmd.data_buf = dst;
	cmd.block_size = 0x200;
	cmd.block_count = sectorCount;

	if (cmd.cmd_index == 18U) {
		res = sdifCmd23SetBlockCount(dev->host, sectorCount);
		if (res < 0)
			return res;
	}

	res = sdifExecuteCmdNoRetry(dev->host, &cmd, 0);
	if (res < 0) {
		if (cmd.cmd_index == 18U &&
		    (u32)res + 0x7fcdfee3U > 1U &&
		    (u32)res != 0x8032011fU) {
			sdifCmd12StopTransmission(dev->host, 0);
		}
	} else {
		res = sdifCmd13SendStatus(dev->host, NULL);
		if (res > 0)
			res = 0;
	}
	return res;
}

/* 0x807382: sdifProbeAndInitMmc (name ours) */
int sdifProbeAndInitMmc(int hostId, SceSdifMmcDev **outDev)
{
	SceSdifHostContext *host;
	u32 ocr;
	u32 mant;
	u32 unit;
	u32 targetClock;
	int highSpeed;
	int res;

	host = sdifGetHostContext(hostId);
	if (host == NULL)
		return -1;

	res = sdifIsCardInserted(host);
	if (res < 0)
		return res;
	if (res == 0)
		return -1;

	s_mmcDev.addr_mode_flags = 0;
	s_mmcDev.host = host;

	res = sdifSupplyClock(host);
	if (res < 0)
		return res;

	res = sdifGoIdleState(host);
	if (res < 0)
		return res;

	res = sdifSendMmcOpCond(&s_mmcDev, host->ocr_voltage_mask | 0x40000000U, &ocr);
	if (res < 0)
		return res;

	res = sdifCmd2AllSendCid(s_mmcDev.host, s_mmcDev.cid);
	if (res < 0)
		return res;

	res = sdifSetRelativeAddr(&s_mmcDev, 1);
	if (res < 0)
		return res;
	host->rca = 1;

	res = sdifCmd9SendCsd(s_mmcDev.host, s_mmcDev.csd);
	if (res < 0)
		return res;

	s_mmcDev.csd_structure = (u8)sdifExtractCsdBits(126, 2);
	s_mmcDev.spec_vers = (u8)sdifExtractCsdBits(122, 4);
	mant = sdifExtractCsdBits(115, 4);
	unit = sdifExtractCsdBits(112, 3);
	s_mmcDev.taac_ns = (s_csdTaacMantissa[mant] * s_csdTaacUnit[unit]) / 10U;
	s_mmcDev.nsac_clks = sdifExtractCsdBits(104, 8) * 100U;
	mant = sdifExtractCsdBits(99, 4);
	unit = sdifExtractCsdBits(96, 3);
	s_mmcDev.tran_speed_hz = s_csdTranSpeedMantissa[mant] * s_csdTranSpeedUnit[unit];
	unit = sdifExtractCsdBits(47, 3);
	mant = sdifExtractCsdBits(62, 12);
	s_mmcDev.c_size_blocks = (mant + 1U) << (unit + 2U);
	s_mmcDev.ccc = (u16)sdifExtractCsdBits(84, 12);
	s_mmcDev.read_bl_len = (u8)sdifExtractCsdBits(80, 4);
	s_mmcDev.read_bl_partial = (u8)sdifExtractCsdBits(79, 1);
	s_mmcDev.write_blk_misalign = (u8)sdifExtractCsdBits(78, 1);
	s_mmcDev.read_blk_misalign = (u8)sdifExtractCsdBits(77, 1);
	s_mmcDev.r2w_factor = (u8)sdifExtractCsdBits(26, 3);
	s_mmcDev.write_bl_len = (u8)sdifExtractCsdBits(22, 4);
	s_mmcDev.write_bl_partial = (u8)sdifExtractCsdBits(21, 1);

	res = sdifCmd7SelectCard(s_mmcDev.host, 1);
	if (res < 0)
		return res;

	res = sdifSendExtCsd(&s_mmcDev, s_mmcDev.ext_csd);
	if (res < 0)
		return res;

	s_mmcDev.sec_count = ((u32)s_mmcDev.ext_csd[215] << 24) |
			     ((u32)s_mmcDev.ext_csd[214] << 16) |
			     ((u32)s_mmcDev.ext_csd[213] << 8) |
			     (u32)s_mmcDev.ext_csd[212];
	s_mmcDev.addr_mode_flags |= 1U;

	if ((s_mmcDev.ext_csd[196] & 3U) == 1U)
		s_mmcDev.hs_max_dtr = 26000000U;
	else if ((s_mmcDev.ext_csd[196] & 3U) != 0U)
		s_mmcDev.hs_max_dtr = 52000000U;

	res = sdifCmd16SetBlockLen(s_mmcDev.host, 0x200);
	if (res < 0)
		return res;

	if (s_mmcDev.hs_max_dtr == 0) {
		highSpeed = 0;
		targetClock = s_mmcDev.tran_speed_hz;
	} else {
		res = sdifMmcSwitch(&s_mmcDev, 0, 185, 1);
		if (res < 0)
			return res;
		highSpeed = 1;
		targetClock = s_mmcDev.hs_max_dtr;
	}

	res = sdifSetBusClock(host, targetClock, highSpeed);
	if (res < 0)
		return res;

	if (s_mmcDev.spec_vers > 3) {
		res = sdifMmcSwitch(&s_mmcDev, 0, 183, 2);
		if (res < 0)
			return res;
		res = sdifSetBusWidth(host, 8);
		if (res < 0)
			return res;
	}

	if (outDev != NULL)
		*outDev = &s_mmcDev;
	return 0;
}

/* 0x8075f0: sdifResetLinesOnError (name ours) */
void sdifResetLinesOnError(SceSdifHostContext *host, u16 errIntStatus)
{
	volatile u8 *regs = host->regs;
	u16 savedErrSignalEn;

	if (errIntStatus != 0) {
		savedErrSignalEn = *(volatile u16 *)(regs + 0x36);
		*(volatile u16 *)(regs + 0x36) = 0;
		while (*(volatile u16 *)(regs + 0x36) != 0)
			;

		if ((errIntStatus & 0x0fU) != 0) {
			*(volatile u8 *)(regs + 0x2f) = 2;
			while ((*(volatile u8 *)(regs + 0x2f) & 2U) != 0)
				;
		}
		if ((errIntStatus & 0x70U) != 0) {
			*(volatile u8 *)(regs + 0x2f) = 4;
			while ((*(volatile u8 *)(regs + 0x2f) & 4U) != 0)
				;
		}

		*(volatile u16 *)(regs + 0x32) = *(volatile u16 *)(regs + 0x32);
		while (*(volatile u16 *)(regs + 0x32) != 0)
			;

		*(volatile u16 *)(regs + 0x36) = savedErrSignalEn;
		while (*(volatile u16 *)(regs + 0x36) != savedErrSignalEn)
			;
	}
}

/* 0x80766e: sdifCompleteCmd (name ours) */
void sdifCompleteCmd(SceSdifHostContext *host)
{
	if (host->cur_cmd->state == 2) {
		host->cur_cmd->state = 3;
		host->cur_cmd = NULL;
	}
}

/* 0x807688: sdifDelay1Ms (name ours) */
void sdifDelay1Ms(void)
{
	cmepDelayUs(1000);
}

/* 0x807690: sdifDelayUs (name ours, tail-call stub to cmepDelayUs) */
void sdifDelayUs(u32 us)
{
	cmepDelayUs(us);
}

/* 0x807694: sdifGetHostContext (name ours) */
SceSdifHostContext *sdifGetHostContext(int hostId)
{
	if (hostId == 1)
		return &s_sdifHost1;
	if (hostId == 0x101)
		return &s_sdifHost1Alt;
	if (hostId == 0)
		return &s_sdifHost0;
	return NULL;
}

/* 0x8076b8: sdifSoftResetAndWaitCardDetect (name ours) */
int sdifSoftResetAndWaitCardDetect(SceSdifHostContext *host, u32 resetFlags, u32 timeoutMs)
{
	volatile u8 *regs = host->regs;
	u32 elapsed;

	if ((resetFlags & 1U) != 0) {
		*(volatile u8 *)(regs + 0x2f) = 1;
		while ((*(volatile u8 *)(regs + 0x2f) & 1U) != 0)
			;
	}

	if ((resetFlags & 2U) != 0) {
		*(volatile u16 *)(regs + 0x34) = 0x01fbU;
		*(volatile u16 *)(regs + 0x36) = 0x037fU;
		*(volatile u16 *)(regs + 0x38) = 0x01fbU;
		*(volatile u16 *)(regs + 0x3a) = 0x037fU;
		*(volatile u8 *)(regs + 0x2e) = 0x0eU;

		elapsed = 0;
		while ((*(volatile u32 *)(regs + 0x24) & 0x20000U) == 0) {
			if (timeoutMs != 0) {
				if (elapsed == timeoutMs)
					return (int)SCE_SDIF_ERROR_TIMEOUT;
				elapsed++;
				sdifDelay1Ms();
			}
		}

		*(volatile u16 *)(regs + 0x30) = *(volatile u16 *)(regs + 0x30);
		*(volatile u16 *)(regs + 0x32) = *(volatile u16 *)(regs + 0x32);
	}
	return 0;
}

/* 0x80774e: sdifInitHostController (name ours) */
int sdifInitHostController(u32 hostId, int halfSpeed, u32 timeoutMs)
{
	volatile u8 *regs = (volatile u8 *)0xe0b00000U;
	SceSdifHostContext *host;

	if ((hostId & 0xffU) != 0)
		regs = (volatile u8 *)(0xe0bf0000U + (hostId & 0xffU) * 0x10000U);

	if (hostId == 1)
		host = &s_sdifHost1;
	else if (hostId == 0x101)
		host = &s_sdifHost1Alt;
	else if (hostId == 0)
		host = &s_sdifHost0;
	else
		return -1;

	memset(host, 0, sizeof(*host));
	host->host_id = (u8)hostId;
	host->regs = regs;
	host->base_clock_hz = 48000000U;
	host->max_clock_hz = (halfSpeed != 0) ? 24000000U : 48000000U;

	if (hostId == 1) {
		host->ocr_voltage_mask = 0x00300000U;
	} else if (hostId == 0x101) {
		host->ocr_voltage_mask = 0x00300000U;
		host->regs = (volatile u8 *)0xe0b00000U;
	} else {
		host->ocr_voltage_mask = (hostId == 0) ? 0x80U : 0U;
	}

	return sdifSoftResetAndWaitCardDetect(host, 3, timeoutMs);
}

/* 0x807826: sdifSupplyClock (name ours) */
int sdifSupplyClock(SceSdifHostContext *host)
{
	volatile u8 *regs = host->regs;

	while ((*(volatile u32 *)(regs + 0x24) & 0x20000U) == 0)
		;

	if ((*(volatile u32 *)(regs + 0x24) & 0x10000U) != 0) {
		*(volatile u8 *)(regs + 0x29) |= 0x0bU;
		*(volatile u16 *)(regs + 0x2c) = 0;
		*(volatile u16 *)(regs + 0x2c) = 0x8001U;
		while ((*(volatile u16 *)(regs + 0x2c) & 2U) == 0)
			;
		*(volatile u16 *)(regs + 0x2c) |= 4U;
		*(volatile u8 *)(regs + 0x28) = 0;
	}
	return 0;
}

/* 0x807888: sdifStopClockAndPower (name ours) */
int sdifStopClockAndPower(SceSdifHostContext *host)
{
	volatile u8 *regs = host->regs;

	*(volatile u16 *)(regs + 0x2c) = 0;
	*(volatile u8 *)(regs + 0x28) = 0;
	*(volatile u8 *)(regs + 0x29) = 0;
	return 0;
}

/* 0x80789e: sdifIsCardInserted (name ours) */
int sdifIsCardInserted(SceSdifHostContext *host)
{
	volatile u8 *regs = host->regs;
	u32 timeout = 1000;

	for (;;) {
		if ((*(volatile u32 *)(regs + 0x24) & 0x20000U) != 0)
			return (int)((*(volatile u32 *)(regs + 0x24) >> 16) & 1U);
		if (timeout == 0)
			break;
		timeout--;
		sdifDelay1Ms();
	}
	return (int)SCE_SDIF_ERROR_TIMEOUT;
}

/* 0x8078e2: sdifSetBusClock (name ours) */
int sdifSetBusClock(SceSdifHostContext *host, u32 targetClockHz, int highSpeed)
{
	volatile u8 *regs = host->regs;
	u32 shift = 0;
	u32 timeout = 10;

	if (targetClockHz > host->max_clock_hz)
		targetClockHz = host->max_clock_hz;

	for (shift = 0; shift < 8; shift++) {
		if ((host->base_clock_hz >> shift) <= targetClockHz)
			break;
	}

	*(volatile u16 *)(regs + 0x2c) = 0;
	*(volatile u16 *)(regs + 0x2c) = (u16)(((0x80U << shift) & 0xff00U) | 1U);

	for (;;) {
		if ((*(volatile u16 *)(regs + 0x2c) & 2U) != 0) {
			*(volatile u16 *)(regs + 0x2c) |= 4U;
			if (highSpeed == 0)
				*(volatile u8 *)(regs + 0x28) &= ~4U;
			else
				*(volatile u8 *)(regs + 0x28) |= 4U;
			return 0;
		}
		if (timeout == 0)
			break;
		timeout--;
		sdifDelay1Ms();
	}
	return (int)SCE_SDIF_ERROR_TIMEOUT;
}

/* 0x80797c: sdifSetBusWidth (name ours) */
int sdifSetBusWidth(SceSdifHostContext *host, int busWidth)
{
	u8 hostCtl = *(volatile u8 *)(host->regs + 0x28);

	if (busWidth == 8)
		hostCtl = (hostCtl & ~0x02U) | 0x20U;
	else if (busWidth == 4)
		hostCtl = (hostCtl & ~0x20U) | 0x02U;
	else
		hostCtl &= ~0x22U;

	*(volatile u8 *)(host->regs + 0x28) = hostCtl;
	return 0;
}

/* 0x8079ae: sdifExecuteCommand (name ours) */
int sdifExecuteCommand(SceSdifHostContext *host, SceSdifCommand *cmd, int retries)
{
	void *savedDataBuf;
	u32 res;

	if ((cmd->flags & 0x400U) != 0) {
		res = (u32)sdifBuildAdmaDescriptors(cmd);
		if (res != 0)
			return (int)res;
	}

	savedDataBuf = cmd->data_buf;
	do {
		volatile u8 *regs = host->regs;
		u32 flags = cmd->flags;
		u32 cmdType = flags & 7U;
		u32 respType = flags & 0xf8U;
		u16 cmdRegFlags;

		cmd->error_code = 0;
		cmd->flags = flags & 0x3fffffffU;
		cmd->state = 2;
		host->cur_cmd = cmd;
		host->data_cmd = NULL;
		host->cmd_done_flag = 0;

		do {
			while ((*(volatile u32 *)(regs + 0x24) & 1U) != 0)
				;
		} while (cmdType == 4U && (*(volatile u32 *)(regs + 0x24) & 2U) != 0);

		if (respType == 0x40U || respType == 0x50U)
			cmdRegFlags = 0x02U;
		else if (respType == 0x28U || respType == 0x78U)
			cmdRegFlags = 0x1bU;
		else if (respType == 0x30U)
			cmdRegFlags = 0x09U;
		else if (respType == 0x10U || respType == 0x60U || respType == 0x80U || respType == 0x90U)
			cmdRegFlags = 0x1aU;
		else
			cmdRegFlags = 0x00U;

		if (cmdType == 1U || cmdType == 2U || cmdType == 3U) {
			*(volatile u32 *)(regs + 0x08) = cmd->argument;
			*(volatile u16 *)(regs + 0x0c) = 0;
			*(volatile u16 *)(regs + 0x0e) = (u16)((cmd->cmd_index << 8) | cmdRegFlags);
		} else if (cmdType == 4U) {
			if ((flags & 0x400U) != 0) {
				sdifStartAdmaTransfer(host, cmd, cmdRegFlags);
			} else {
				u32 blockCount = cmd->block_count;
				u16 xferMode = (u16)(((flags & 0x3fffffffU) >> 4) & 0x10U);

				host->data_cmd = cmd;
				*(volatile u8 *)(regs + 0x2e) = 0x0eU;
				*(volatile u8 *)(regs + 0x28) &= ~0x18U;
				*(volatile u16 *)(regs + 0x04) = (u16)cmd->block_size | 0x7000U;
				*(volatile u16 *)(regs + 0x06) = (u16)blockCount;
				if ((flags & 0x800U) != 0)
					xferMode |= 4U;
				if (blockCount == 1U)
					xferMode |= 2U;
				else if ((s32)blockCount > 1)
					xferMode |= 0x22U;
				*(volatile u32 *)(regs + 0x08) = cmd->argument;
				*(volatile u16 *)(regs + 0x0c) = xferMode;
				*(volatile u16 *)(regs + 0x0e) = (u16)((cmd->cmd_index << 8) | 0x20U | cmdRegFlags);
			}
		}

		while ((cmd->flags & 0xc0000000U) == 0) {
			if (*(volatile u16 *)(regs + 0x30) != 0 || *(volatile u16 *)(regs + 0x32) != 0) {
				volatile u8 *hregs = host->regs;
				u16 normStat = *(volatile u16 *)(hregs + 0x30);
				u16 errStat = *(volatile u16 *)(hregs + 0x32);

				if ((errStat & 0x200U) != 0)
					*(volatile u16 *)(hregs + 0x32) = 0x200U;

				normStat &= 0xfe3fU;
				if ((normStat & 1U) != 0 || (errStat & 0x0fU) != 0) {
					SceSdifCommand *cur = host->cur_cmd;

					*(volatile u16 *)(hregs + 0x30) = 1U;
					*(volatile u16 *)(hregs + 0x32) = 0x0fU;
					if (cur != NULL) {
						if (errStat != 0) {
							cur->error_code = ((errStat & 1U) != 0)
								? SCE_SDIF_ERROR_TIMEOUT
								: SCE_SDIF_ERROR_IO;
							cur->flags |= 0x40000000U;
							sdifResetLinesOnError(host, errStat);
							sdifCompleteCmd(host);
						} else if ((normStat & 1U) != 0) {
							u32 rtype = cur->flags & 0xf8U;

							host->cmd_done_flag = 1;
							cur->error_code = 0;
							if (rtype == 0x40U || rtype == 0x50U ||
							    rtype == 0x60U || rtype == 0x78U ||
							    rtype == 0x80U || rtype == 0x90U) {
								cur->response[0] = *(volatile u32 *)(hregs + 0x10);
							} else if (rtype == 0x10U || rtype == 0x28U) {
								cur->response[0] = *(volatile u32 *)(hregs + 0x10);
								if ((cur->flags & 0x800U) != 0)
									cur->response[3] = *(volatile u32 *)(hregs + 0x1c);
							} else if (rtype == 0x30U) {
								u32 r0 = *(volatile u32 *)(hregs + 0x10);
								u32 r1 = *(volatile u32 *)(hregs + 0x14);
								u32 r2 = *(volatile u32 *)(hregs + 0x18);
								u32 r3 = *(volatile u32 *)(hregs + 0x1c);

								cur->response[0] = r0 << 8;
								cur->response[1] = (r0 >> 24) | (r1 << 8);
								cur->response[2] = (r1 >> 24) | (r2 << 8);
								cur->response[3] = (r2 >> 24) | (r3 << 8);
							}
							if (host->data_cmd == NULL && (cur->flags & 8U) == 0) {
								cur->flags |= 0x80000000U;
								sdifCompleteCmd(host);
							}
						}
					}
				}

				if ((normStat & 0x3aU) != 0 || (errStat & 0x270U) != 0) {
					SceSdifCommand *cur = host->cur_cmd;

					*(volatile u16 *)(hregs + 0x30) = 0x32U;
					*(volatile u16 *)(hregs + 0x32) = 0x70U;
					if (cur != NULL) {
						if (host->data_cmd == NULL) {
							if ((normStat & 2U) != 0 && (cur->flags & 8U) != 0) {
								cur->flags |= 0x80000000U;
								sdifCompleteCmd(host);
							}
						} else if (errStat != 0) {
							host->data_cmd = NULL;
							cur->error_code = ((errStat & 0x10U) != 0)
								? SCE_SDIF_ERROR_TIMEOUT
								: SCE_SDIF_ERROR_IO;
							cur->flags |= 0x40000000U;
							sdifResetLinesOnError(host, errStat);
							sdifCompleteCmd(host);
						} else {
							if ((normStat & 8U) != 0) {
								*(volatile u32 *)host->regs = *(volatile u32 *)host->regs;
							}
							if ((normStat & 0x20U) != 0 && (cur->flags & 0x700U) == 0x100U) {
								u8 *dst = (u8 *)cur->data_buf;
								volatile u8 *pioRegs = host->regs;
								u32 offset = 0;

								while ((*(volatile u32 *)(pioRegs + 0x24) & 0x800U) == 0)
									;
								while (offset < cur->block_size) {
									u32 rem = cur->block_size - offset;

									if (rem == 1) {
										dst[offset] = *(volatile u8 *)(pioRegs + 0x20);
										offset += 1;
									} else if (rem < 4) {
										u16 v = *(volatile u16 *)(pioRegs + 0x20);
										dst[offset] = (u8)v;
										dst[offset + 1] = (u8)(v >> 8);
										offset += 2;
									} else {
										u32 v = *(volatile u32 *)(pioRegs + 0x20);
										if (((u32)dst & 3U) == 0) {
											*(u32 *)(dst + offset) = v;
										} else {
											dst[offset] = (u8)v;
											dst[offset + 1] = (u8)(v >> 8);
											dst[offset + 2] = (u8)(v >> 16);
											dst[offset + 3] = (u8)(v >> 24);
										}
										offset += 4;
									}
								}
								cur->data_buf = dst + cur->block_size;
							}
							if ((normStat & 0x10U) != 0 && (cur->flags & 0x700U) == 0x200U) {
								const u8 *src = (const u8 *)cur->data_buf;
								volatile u8 *pioRegs = host->regs;
								u32 blockSize = cur->block_size;
								u32 offset = 0;

								while ((*(volatile u32 *)(pioRegs + 0x24) & 0x400U) == 0)
									;
								while (offset < blockSize) {
									u32 rem = blockSize - offset;

									if (rem == 1) {
										*(volatile u8 *)(pioRegs + 0x20) = src[offset];
										offset += 1;
									} else if (rem < 4) {
										u16 v = (u16)src[offset] | ((u16)src[offset + 1] << 8);
										*(volatile u16 *)(pioRegs + 0x20) = v;
										offset += 2;
									} else {
										u32 v;
										if (((u32)src & 3U) == 0) {
											v = *(const u32 *)(src + offset);
										} else {
											v = (u32)src[offset] |
											    ((u32)src[offset + 1] << 8) |
											    ((u32)src[offset + 2] << 16) |
											    ((u32)src[offset + 3] << 24);
										}
										*(volatile u32 *)(pioRegs + 0x20) = v;
										offset += 4;
									}
								}
								cur->data_buf = (void *)(src + blockSize);
							}
							if ((normStat & 2U) != 0) {
								if (host->cmd_done_flag != 0) {
									cur->flags |= 0x80000000U;
									host->data_cmd = NULL;
									sdifCompleteCmd(host);
								} else {
									host->data_cmd = NULL;
								}
							}
						}
					}
				}
			}
			sdifDelay1Ms();
		}

		res = ((cmd->flags & 0x40000000U) != 0) ? cmd->error_code : 0U;
		host->cur_cmd = NULL;
		if ((s32)res >= 0)
			return (int)res;

		retries--;
		cmd->data_buf = savedDataBuf;
	} while (retries > 0);

	return (int)res;
}

/* 0x807eee: sdifExecuteCmdNoRetry (name ours) */
int sdifExecuteCmdNoRetry(SceSdifHostContext *host, SceSdifCommand *cmd, int unusedArg)
{
	(void)unusedArg;
	return sdifExecuteCommand(host, cmd, 1);
}

/* 0x807ef4: sdifExecuteCmdRetry3 (name ours) */
int sdifExecuteCmdRetry3(SceSdifHostContext *host, SceSdifCommand *cmd)
{
	return sdifExecuteCommand(host, cmd, 1);
}

/* 0x807efc: sdifGoIdleState (name ours, CMD0) */
int sdifGoIdleState(SceSdifHostContext *host)
{
	SceSdifCommand cmd;
	int res;

	cmd.size = sizeof(cmd);
	cmd.flags = 1;
	cmd.cmd_index = 0;
	cmd.argument = 0;
	cmd.data_buf = NULL;
	res = sdifExecuteCommand(host, &cmd, 0);
	if (res >= 0) {
		sdifSetBusClock(host, 400000, 0);
		sdifSetBusWidth(host, 1);
		res = 0;
	}
	return res;
}

/* 0x807f3c: sdifCmd2AllSendCid (name ours, CMD2) */
int sdifCmd2AllSendCid(SceSdifHostContext *host, void *outCid)
{
	SceSdifCommand cmd;
	int res;

	cmd.size = sizeof(cmd);
	cmd.flags = 0x32;
	cmd.cmd_index = 2;
	cmd.argument = 0;
	cmd.data_buf = NULL;
	res = sdifExecuteCmdRetry3(host, &cmd);
	if (res >= 0) {
		memcpy(outCid, cmd.response, 0x10);
		res = 0;
	}
	return res;
}

/* 0x807f76: sdifCmd9SendCsd (name ours, CMD9) */
int sdifCmd9SendCsd(SceSdifHostContext *host, void *outCsd)
{
	SceSdifCommand cmd;
	int res;

	cmd.size = sizeof(cmd);
	cmd.flags = 0x33;
	cmd.cmd_index = 9;
	cmd.argument = (u32)host->rca << 16;
	cmd.data_buf = NULL;
	res = sdifExecuteCmdRetry3(host, &cmd);
	if (res >= 0) {
		memcpy(outCsd, cmd.response, 0x10);
		res = 0;
	}
	return res;
}

/* 0x807fb6: sdifCmd7SelectCard (name ours, CMD7) */
int sdifCmd7SelectCard(SceSdifHostContext *host, int select)
{
	SceSdifCommand cmd;
	int res;

	cmd.size = sizeof(cmd);
	cmd.flags = (select == 0) ? 0x03U : 0x13U;
	cmd.cmd_index = 7;
	cmd.argument = (select == 0) ? 0U : ((u32)host->rca << 16);
	res = sdifExecuteCmdRetry3(host, &cmd);
	if (res > 0)
		res = 0;
	return res;
}

/* 0x807fee: sdifCmd12StopTransmission (name ours, CMD12) */
int sdifCmd12StopTransmission(SceSdifHostContext *host, int busyWait)
{
	SceSdifCommand cmd;
	int res;

	cmd.size = sizeof(cmd);
	cmd.flags = (busyWait == 0) ? 0x3013U : 0x302bU;
	cmd.cmd_index = 12;
	cmd.argument = 0;
	res = sdifExecuteCmdRetry3(host, &cmd);
	if (res > 0)
		res = 0;
	return res;
}

/* 0x80801e: sdifCheckCardStatusErrors (name ours) */
u32 sdifCheckCardStatusErrors(u32 cardStatus)
{
	u32 errBits = cardStatus & 0xfdffe008U;
	u32 bit;

	if (errBits == 0)
		return 0;

	for (bit = 0; bit < 32; bit++) {
		if (((errBits >> bit) & 1U) != 0)
			break;
	}
	return SCE_SDIF_ERROR_CARD_BASE + bit;
}

/* 0x80804e: sdifCmd13SendStatus (name ours, CMD13) */
int sdifCmd13SendStatus(SceSdifHostContext *host, u32 *outStatus)
{
	SceSdifCommand cmd;
	int res;

	cmd.size = sizeof(cmd);
	cmd.flags = 0x13;
	cmd.cmd_index = 13;
	cmd.argument = (u32)host->rca << 16;
	cmd.data_buf = NULL;
	res = sdifExecuteCmdRetry3(host, &cmd);
	if (res >= 0) {
		if (outStatus != NULL)
			*outStatus = cmd.response[0];
		res = (int)sdifCheckCardStatusErrors(cmd.response[0]);
		if (res > 0)
			res = 0;
	}
	return res;
}

/* 0x808092: sdifCmd16SetBlockLen (name ours, CMD16) */
int sdifCmd16SetBlockLen(SceSdifHostContext *host, u32 blockLen)
{
	SceSdifCommand cmd;
	int res;

	cmd.size = sizeof(cmd);
	cmd.flags = 0x13;
	cmd.cmd_index = 16;
	cmd.argument = blockLen;
	cmd.data_buf = NULL;
	res = sdifExecuteCmdRetry3(host, &cmd);
	if (res >= 0) {
		res = (int)sdifCheckCardStatusErrors(cmd.response[0]);
		if (res > 0)
			res = 0;
	}
	return res;
}

/* 0x8080c4: sdifCmd23SetBlockCount (name ours, CMD23) */
int sdifCmd23SetBlockCount(SceSdifHostContext *host, u32 blockCount)
{
	SceSdifCommand cmd;
	int res;

	cmd.size = sizeof(cmd);
	cmd.flags = 0x13;
	cmd.cmd_index = 23;
	cmd.argument = blockCount;
	cmd.data_buf = NULL;
	res = sdifExecuteCmdRetry3(host, &cmd);
	if (res >= 0) {
		res = (int)sdifCheckCardStatusErrors(cmd.response[0]);
		if (res > 0)
			res = 0;
	}
	return res;
}

/* 0x8080f6: sdifGetSocRevision (name ours) */
u32 sdifGetSocRevision(void)
{
	if (s_sdifSocRevCached == 0) {
		s_sdifSocRevValue = MMIO_R32(0xe3100000U);
		s_sdifSocRevCached = 1;
	}
	return s_sdifSocRevValue;
}

/* 0x808116: sdifCheckSectorAligned (name ours) */
int sdifCheckSectorAligned(const void *buf, u32 size)
{
	(void)buf;
	return ((size & 0x1ffU) == 0) ? 0 : -1;
}

/* 0x808120: sdifReadSectorsAligned (name ours) */
int sdifReadSectorsAligned(void *dst, u32 size, u32 startSector)
{
	if ((size & 0x1ffU) == 0)
		return sdifReadSectors(s_activeMmcDev, startSector, dst, size >> 9);
	return -1;
}

/* 0x808142: sdifGetTotalSectors (name ours) */
u32 sdifGetTotalSectors(void)
{
	if (s_activeMmcDev == NULL)
		return 0;
	return s_activeMmcDev->sec_count;
}

/* 0x808156: sdifPervasiveReset (name ours) */
void sdifPervasiveReset(void)
{
	u32 val = MMIO_R32(0xe3100124U) | 1U;

	MMIO_W32(0xe3100124U, val);
	while (MMIO_R32(0xe3100124U) != val)
		;

	MMIO_W32(0xe3101190U, 1);
	while (MMIO_R32(0xe3101190U) != 1)
		;

	MMIO_W32(0xe31020a0U, 1);
	while (MMIO_R32(0xe31020a0U) != 1)
		;

	MMIO_W32(0xe31010a0U, 0);
	while (MMIO_R32(0xe31010a0U) != 0)
		;

	MMIO_W32(0xe3101190U, 0);
	while (MMIO_R32(0xe3101190U) != 0)
		;
}

/* 0x8081cc: sdifInitInlineCryptoEngine (name ours) */
void sdifInitInlineCryptoEngine(void)
{
	MMIO_W32(0xe0030024U, 0x1fef020fU);
	MMIO_W32(0xe0030024U, 0x1fef020eU);
	MMIO_W32(0xe0070008U, 0x020e020fU);
	MMIO_W32(0xe0070000U, 0);
	MMIO_W32(0xe0070000U, 1);
}

/* 0x808208: sdifInitEmmc (name ours) */
int sdifInitEmmc(int isWarmBoot)
{
	int res;

	if (s_sdifEmmcInitialized != 0)
		return 0;

	if (isWarmBoot == 0) {
		sdifPervasiveReset();
	} else if ((MMIO_R32(0x30000118U) & 0x20000U) != 0) {
		MMIO_W32(0x30000208U, 0x80000001U);
	}

	sdifInitInlineCryptoEngine();
	if ((sdifGetSocRevision() & 0x00ffffffU) != 0x30U &&
	    (sdifGetSocRevision() & 0x00ffffffU) != 0x32U) {
		MMIO_W32(0xe0070014U, 6);
	}

	res = sdifInitHostController(0, 0, 0);
	if (res == 0) {
		res = sdifProbeAndInitMmc(0, &s_activeMmcDev);
		if (res == 0)
			s_sdifEmmcInitialized = 1;
	}
	return res;
}

/* 0x808292: sdifDeinitEmmc (name ours) */
int sdifDeinitEmmc(int powerOff)
{
	int res = 0;

	if (powerOff != 0)
		res = sysconControlPowerSwitch(1);

	MMIO_W32(0xe3100194U, 0);
	while (MMIO_R32(0xe3100194U) != 0)
		;
	return res;
}

/* 0x8082c0: sdifSetAdmaBuffer (name ours) */
void sdifSetAdmaBuffer(void *admaBuf, u32 admaBufSize)
{
	s_sdifAdmaBuf = (u8 *)admaBuf;
	s_sdifAdmaBufSize = admaBufSize;
}

/* 0x8082ca: sdifBuildAdmaDescriptors (name ours) */
int sdifBuildAdmaDescriptors(SceSdifCommand *cmd)
{
	u32 bufSize = s_sdifAdmaBufSize;
	u32 maxDescs = bufSize >> 3;
	u32 totalBytes = cmd->block_size * cmd->block_count;
	u8 *descBase = s_sdifAdmaBuf;
	u32 dataAddr;
	u32 descCount;
	u8 *desc;

	if (totalBytes == 0)
		totalBytes = cmd->block_size;

	dataAddr = (u32)cmd->data_buf;
	memset(descBase, 0, bufSize);

	if (totalBytes != 0) {
		descCount = 0;
		desc = descBase;
		do {
			u32 chunkLen = (totalBytes > 0x10000U) ? 0x10000U : totalBytes;

			desc[7] = (u8)(dataAddr >> 24);
			desc[4] = (u8)dataAddr;
			desc[6] = (u8)(dataAddr >> 16);
			desc[2] = (u8)(chunkLen & 0xffffU);
			desc[5] = (u8)(dataAddr >> 8);
			desc[1] = 0;
			desc[3] = (u8)((chunkLen & 0xffffU) >> 8);
			desc[0] = 0x21U;

			descCount++;
			dataAddr += chunkLen;
			maxDescs--;
			if (maxDescs == 0)
				break;
			totalBytes -= chunkLen;
			desc += 8;
		} while (totalBytes != 0);

		if (descCount != 0)
			descBase[(descCount - 1U) * 8U] |= 0x02U;
	}
	return 0;
}

/* 0x80838e: sdifStartAdmaTransfer (name ours) */
int sdifStartAdmaTransfer(SceSdifHostContext *host, SceSdifCommand *cmd, u16 cmdRegFlags)
{
	volatile u8 *regs = host->regs;
	u32 blockCount;
	u32 flags;
	u16 xferMode;

	host->data_cmd = cmd;
	*(volatile u8 *)(regs + 0x2e) = 0x0eU;
	*(volatile u8 *)(regs + 0x28) = (*(volatile u8 *)(regs + 0x28) & ~0x18U) | 0x10U;

	blockCount = cmd->block_count;
	flags = cmd->flags;
	*(volatile u16 *)(regs + 0x04) = (u16)cmd->block_size;
	*(volatile u16 *)(regs + 0x06) = (u16)blockCount;

	xferMode = (u16)((flags >> 4) & 0x10U);
	if ((flags & 0x800U) != 0)
		xferMode |= 4U;
	if (blockCount == 1U)
		xferMode |= 2U;
	else if ((s32)blockCount > 1)
		xferMode |= 0x22U;

	*(volatile u32 *)(regs + 0x58) = (u32)s_sdifAdmaBuf;
	*(volatile u32 *)(regs + 0x08) = cmd->argument;
	*(volatile u16 *)(regs + 0x0c) = xferMode | 1U;
	*(volatile u16 *)(regs + 0x0e) = (u16)((cmd->cmd_index << 8) | 0x20U | cmdRegFlags);
	return 0;
}
