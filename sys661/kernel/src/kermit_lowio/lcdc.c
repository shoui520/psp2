#include "lowio_int.h"

/* =========================================================================
 * Part 4: sceLcdc_driver (20 exported functions)
 * ========================================================================= */

typedef struct {
	u16 fPrch;
	u16 bPrch;
	u16 sync;
	u16 res;
} SceLcdcModeline;

typedef struct {
	s32 id;
	float sync;
	u8 clk[2];
	u8 syncDiff;
	u8 flags;
	SceLcdcModeline x;
	SceLcdcModeline y;
} SceLcdcDispMode;

typedef struct {
	s32 id;
	s16 xres, v1_6, v1_8, yres, v1_12, v1_14;
} SceLcdcUnkElem2;

typedef struct {
	s32 v2_0;
	s32 syncDiff;
	s32 v2_8;
	s32 unused1;
	s32 xbPrch, xsync, xfPrch, xres;
	s32 ybPrch, ysync, yfPrch, yres;
	s32 hpc;
	s32 vpc;
	s32 unused3[2];
	s32 shiftY, shiftX;
	s32 realX, realY;
	s32 v2_80;
	s32 unused4[7];
	s32 v2_112;
} SceLcdcHwController;

typedef struct {
	s32 v3_0;
	s32 mobo;
	s32 v3_8;
	s32 scale;
	s32 dispId;
	s32 scaledXres;
	s32 yres;
	s32 v3_28;
	SceLcdcHwController *hwCtl;
	SceLcdcDispMode disp;
	s32 v3_64;
	s32 shiftX, shiftY;
	s32 realX, realY;
	s32 tabId;
	s32 v3_88;
	s32 hwData[7];
} SceLcdc;

static SceLcdc g_lcdc;

static SceLcdcDispMode g_lcdcDispModeList[13] = {
	{  0,  9000000.0f, { 0, 3 }, 0, 0, {  2, 41,  2, 480 }, { 10, 2,  2, 272 } },
	{  1, 27000000.0f, { 0, 1 }, 1, 0, { 16, 61, 61, 720 }, {  9, 6, 30, 480 } },
	{  2, 25175000.0f, { 1, 1 }, 1, 0, { 32, 96, 32, 640 }, { 10, 2, 33, 480 } },
	{  3, 27000000.0f, { 0, 1 }, 1, 0, { 61, 96, 61, 640 }, { 10, 2, 33, 480 } },
	{  4, 13500000.0f, { 0, 2 }, 1, 0, { 16, 61, 61, 720 }, {  5, 3, 14, 240 } },
	{  5, 13500000.0f, { 0, 2 }, 1, 0, { 16, 61, 61, 720 }, {  6, 3, 14, 240 } },
	{  6, 13500000.0f, { 0, 2 }, 1, 0, { 16, 61, 61, 720 }, {  3, 3, 14, 505 } },
	{ 11,  9000000.0f, { 0, 3 }, 0, 0, {  3, 41,  2, 480 }, { 10, 2,  2, 272 } },
	{ 12, 27000000.0f, { 0, 1 }, 1, 0, { 16, 61, 61, 720 }, { 10, 6, 30, 480 } },
	{ 14,  9000000.0f, { 0, 2 }, 0, 0, { 10, 51,  4, 320 }, {  5, 5,  5, 180 } },
	{ 15, 18000000.0f, { 1, 1 }, 0, 0, {  2, 41,  2, 480 }, { 10, 2,  2, 272 } },
	{ 16, 18000000.0f, { 1, 1 }, 0, 0, {  3, 41,  2, 480 }, { 10, 2,  2, 272 } },
	{ 17, 13500000.0f, { 2, 4 }, 1, 7, { 16, 61, 61, 720 }, { 10, 6, 30, 479 } }
};

static SceLcdcUnkElem2 g_lcdcTab[8] = {
	{ 1, 0x02BA, 0x0AFC, 0x0580, 0x0168, 0x0C18, 0x060C },
	{ 2, 0x0298, 0x0B90, 0x05C8, 0x0156, 0x0CB8, 0x065C },
	{ 3, 0x0274, 0x0C34, 0x061C, 0x0144, 0x0D70, 0x06B8 },
	{ 4, 0x0251, 0x0CEC, 0x0678, 0x0132, 0x0E38, 0x071C },
	{ 5, 0x02BA, 0x0AFC, 0x0580, 0x01E0, 0x0910, 0x0488 },
	{ 6, 0x0298, 0x0B90, 0x05C8, 0x01C8, 0x098C, 0x04C4 },
	{ 7, 0x0274, 0x0C34, 0x061C, 0x01B0, 0x0A14, 0x0508 },
	{ 8, 0x0251, 0x0CEC, 0x0678, 0x0198, 0x0AAC, 0x0554 }
};

static int lcdc_event_handler(int ev_id, char *ev_name, void *param, int *result);
s32 sceLcdc_driver_5DF2F52A(void);

static SceSysEventHandler g_lcdcEvent = {
	sizeof(SceSysEventHandler),
	"SceLcdc",
	0x00FFFF00,
	lcdc_event_handler,
	0, 0, NULL,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

static s32 lcdcResetApbTimer0(void)
{
	sceSysregApbTimerClkDisable(0);
	sceSysregApbTimerClkSelect(0, 4);
	sceSysregApbTimerClkEnable(0);
	while ((REG32(0xBE700020) & 4) != 0)
		;
	REG32(0xBE700000) = 0;
	while ((REG32(0xBE700020) & 4) != 0)
		;
	while ((REG32(0xBE700020) & 2) != 0)
		;
	REG32(0xBE700004) = 0;
	while ((REG32(0xBE700020) & 8) != 0)
		;
	REG32(0xBE70000C) = -1;
	while ((REG32(0xBE700020) & 0x10) != 0)
		;
	REG32(0xBE700010) = 0;
	while ((REG32(0xBE700020) & 0x20) != 0)
		;
	REG32(0xBE700014) = 0;
	while ((REG32(0xBE700020) & 0x3A) != 0)
		;
	REG32(0xBE700000) = 1;
	while ((REG32(0xBE700020) & 4) != 0)
		;
	sceSysregApbTimerClkDisable(0);
	sceSysregApbTimerClkSelect(0, 6);
	sceSysregApbTimerClkEnable(0);
	REG32(0xBE700024) = 1;
	return 0;
}

static s32 lcdcInit(void)
{
	memset(&g_lcdc, 0, sizeof(g_lcdc));
	g_lcdc.v3_0 = 1;
	sysregLcdcClkEnable();
	sysregLcdcIoEnable();
	g_lcdc.scale = 3;
	g_lcdc.mobo = 0x00600000;
	g_lcdc.v3_8 = 1;
	g_lcdcDispModeList[8].clk[1] = 1;
	g_lcdcDispModeList[3].id = -1;
	g_lcdcDispModeList[0].clk[1] = 1;
	g_lcdcDispModeList[1].clk[0] = 1;
	g_lcdcDispModeList[1].clk[1] = 1;
	g_lcdcDispModeList[2].id = -1;
	g_lcdcDispModeList[4].clk[0] = 1;
	g_lcdcDispModeList[4].clk[1] = 1;
	g_lcdcDispModeList[5].clk[0] = 1;
	g_lcdcDispModeList[5].clk[1] = 1;
	g_lcdcDispModeList[6].clk[0] = 1;
	g_lcdcDispModeList[6].clk[1] = 1;
	g_lcdcDispModeList[7].clk[1] = 1;
	g_lcdcDispModeList[8].clk[0] = 1;
	REG32(0xBE140070) = 1;
	g_lcdc.hwCtl = (SceLcdcHwController *)0xBE140000;
	sceLcdc_driver_5DF2F52A();
	sceKernelRegisterSysEventHandler(&g_lcdcEvent);
	return 0;
}

static s32 lcdcConfigureTiming(void)
{
	sysregLcdcClkDisable();
	sceSysreg_driver_9746F3B2(0, g_lcdcDispModeList[0].clk[1]);
	sysregLcdcClkEnable();
	if (g_lcdc.mobo > 0x7FFFFF) {
		REG32(0xBE140000) = 0;
		REG32(0xBE140200) = 1;
	}
	g_lcdc.hwCtl->syncDiff = g_lcdc.disp.syncDiff;
	if (g_lcdc.mobo > 0x7FFFFF) {
		if ((REG32(0xBE1401A0) & 1) == 0 && (g_lcdc.disp.flags & 1) != 0)
			lcdcResetApbTimer0();
		REG32(0xBE1401A0) = g_lcdc.disp.flags & 0xFE;
		REG32(0xBE140180) = g_lcdc.hwData[0];
		REG32(0xBE140184) = g_lcdc.hwData[1];
		REG32(0xBE140188) = g_lcdc.hwData[2];
		REG32(0xBE14018C) = g_lcdc.hwData[3];
		REG32(0xBE140190) = g_lcdc.hwData[4];
		REG32(0xBE140194) = g_lcdc.hwData[5];
		REG32(0xBE140198) = g_lcdc.hwData[6];
	}
	g_lcdc.hwCtl->v2_8 = g_lcdc.v3_28;
	g_lcdc.hwCtl->xbPrch = g_lcdc.disp.x.bPrch;
	g_lcdc.hwCtl->xsync = g_lcdc.disp.x.sync;
	g_lcdc.hwCtl->xfPrch = g_lcdc.disp.x.fPrch;
	g_lcdc.hwCtl->xres = g_lcdc.disp.x.res;
	g_lcdc.hwCtl->ybPrch = g_lcdc.disp.y.bPrch;
	g_lcdc.hwCtl->ysync = g_lcdc.disp.y.sync;
	g_lcdc.hwCtl->yfPrch = g_lcdc.disp.y.fPrch;
	g_lcdc.hwCtl->yres = g_lcdc.disp.y.res;
	g_lcdc.hwCtl->shiftY = g_lcdc.shiftY;
	g_lcdc.hwCtl->shiftX = g_lcdc.shiftX;
	g_lcdc.hwCtl->realX = g_lcdc.realX;
	g_lcdc.hwCtl->realY = g_lcdc.realY;
	if (g_lcdc.mobo > 0x7FFFFF)
		REG32(0xBE1401A0) = g_lcdc.disp.flags;
	sceKermit_driver_D69C50BB();
	if (g_lcdc.v3_8 != 0)
		g_lcdc.hwCtl->v2_112 = 1;
	sysregLcdcClkDisable();
	if (g_lcdc.mobo > 0x7FFFFF) {
		sceSysreg_driver_9746F3B2(g_lcdc.disp.clk[0], 1);
		REG32(0xBE1401B0) = g_lcdc.disp.clk[1] - 1;
	} else {
		sceSysreg_driver_9746F3B2(g_lcdc.disp.clk[0], g_lcdc.disp.clk[1]);
	}
	sysregLcdcClkEnable();
	if (g_lcdc.v3_0 != 0)
		sysregLcdcIoEnable();
	return 0;
}

s32 sceLcdc_driver_5DF2F52A(void)
{
	g_lcdc.disp.syncDiff = g_lcdc.hwCtl->syncDiff & 0xFF;
	if (g_lcdc.mobo > 0x7FFFFF) {
		g_lcdc.disp.flags = REG32(0xBE1401A0) & 0xFF;
		g_lcdc.hwData[0] = REG32(0xBE140180);
		g_lcdc.hwData[1] = REG32(0xBE140184);
		g_lcdc.hwData[2] = REG32(0xBE140188);
		g_lcdc.hwData[3] = REG32(0xBE14018C);
		g_lcdc.hwData[4] = REG32(0xBE140190);
		g_lcdc.hwData[5] = REG32(0xBE140194);
		g_lcdc.hwData[6] = REG32(0xBE140198);
	}
	g_lcdc.v3_28 = g_lcdc.hwCtl->v2_8;
	s32 v28 = g_lcdc.hwCtl->xres;
	s32 v32 = g_lcdc.hwCtl->ybPrch;
	s32 v36 = g_lcdc.hwCtl->ysync;
	s32 v40 = g_lcdc.hwCtl->yfPrch;
	s32 v44 = g_lcdc.hwCtl->yres;
	g_lcdc.disp.x.bPrch = g_lcdc.hwCtl->xbPrch & 0xFFFF;
	g_lcdc.shiftY = g_lcdc.hwCtl->shiftY;
	g_lcdc.disp.x.sync = g_lcdc.hwCtl->xsync & 0xFFFF;
	g_lcdc.shiftX = g_lcdc.hwCtl->shiftX;
	g_lcdc.disp.x.fPrch = g_lcdc.hwCtl->xfPrch & 0xFFFF;
	s32 v72 = g_lcdc.hwCtl->realX;
	g_lcdc.realX = v72;
	s32 v76 = g_lcdc.hwCtl->realY;
	g_lcdc.disp.x.res = v28 & 0xFFFF;
	g_lcdc.disp.y.bPrch = v32 & 0xFFFF;
	g_lcdc.disp.y.sync = v36 & 0xFFFF;
	g_lcdc.disp.y.fPrch = v40 & 0xFFFF;
	g_lcdc.disp.y.res = v44 & 0xFFFF;
	g_lcdc.disp.sync = 9000000.0f;
	g_lcdc.realY = v76;
	if (g_lcdc.disp.syncDiff != 0) {
		if (g_lcdc.hwData[0] == 1) {
			g_lcdc.scaledXres = g_lcdc.hwData[1];
			g_lcdc.yres = g_lcdc.hwData[2];
		} else {
			g_lcdc.yres = v76;
			g_lcdc.scaledXres = v72;
		}
	} else {
		g_lcdc.yres = v44 & 0xFFFF;
		g_lcdc.scaledXres = v28 & 0xFFFF;
	}
	u32 hw = REG32(0xBC100060);
	u8 hwBit = (hw >> 22) & 1;
	g_lcdc.disp.clk[0] = hwBit;
	switch (hw & 0x300000) {
	case 0x100000:
		g_lcdc.disp.clk[1] = 1;
		break;
	case 0x200000:
		g_lcdc.disp.clk[1] = 2;
		break;
	case 0x300000:
		g_lcdc.disp.clk[1] = 1;
		g_lcdc.disp.clk[0] = 2;
		break;
	case 0:
		g_lcdc.disp.clk[1] = 3;
		break;
	}
	if (g_lcdc.mobo > 0x7FFFFF)
		g_lcdc.disp.clk[1] *= (REG32(0xBE1401B0) + 1);
	s32 xLine = g_lcdc.disp.x.fPrch + g_lcdc.disp.x.bPrch + g_lcdc.disp.x.sync + g_lcdc.disp.x.res;
	s32 zoom = g_lcdc.scale;
	s32 yLine = g_lcdc.disp.y.fPrch + g_lcdc.disp.y.bPrch + g_lcdc.disp.y.sync + g_lcdc.disp.y.res;
	u8 param = g_lcdc.disp.flags & 7;
	s32 i;
	for (i = 0; i < 13; i++) {
		SceLcdcDispMode *cur = &g_lcdcDispModeList[i];
		if (xLine == (cur->x.fPrch + cur->x.bPrch + cur->x.sync + cur->x.res) * zoom) {
			if (yLine == cur->y.fPrch + cur->y.bPrch + cur->y.sync + cur->y.res &&
			    cur->y.bPrch == hwBit && param == cur->flags) {
				g_lcdc.disp.sync = cur->sync;
				g_lcdc.dispId = cur->id;
				if (cur->id != 0 && g_lcdc.disp.syncDiff == 0)
					g_lcdc.shiftY = (g_lcdc.disp.x.sync / zoom) - cur->x.sync;
				break;
			}
		}
	}
	g_lcdc.v3_88 = 0;
	g_lcdc.tabId = 0;
	if (g_lcdc.hwData[0] == 1) {
		for (i = 0; i < 8; i++) {
			if (g_lcdc.hwData[6] == g_lcdcTab[i].v1_12) {
				g_lcdc.tabId = g_lcdcTab[i].id;
				break;
			}
		}
		if (g_lcdc.tabId == 0)
			g_lcdc.v3_88 = 2;
	} else if ((g_lcdc.disp.flags & 0x20) != 0) {
		g_lcdc.v3_88 = 1;
	}
	return 0;
}

static int lcdc_event_handler(int ev_id, char *ev_name, void *param, int *result)
{
	(void)ev_name;
	(void)result;
	if (ev_id == 0x4004) {
		if (((s32 *)param)[1] != 2) {
			sysregLcdcIoDisable();
			sysregLcdcClkDisable();
		}
	} else if (ev_id == 0x10004) {
		if (((u32 *)((s32 *)param)[1])[1] == 0x80000000)
			sceLcdc_driver_5DF2F52A();
		else
			lcdcConfigureTiming();
	}
	return 0;
}

s32 sceLcdc_driver_BAF165CE(s32 id, s32 xres, s32 yres, s32 arg3)
{
	if ((u32)arg3 >= 4)
		return 0x80000108;
	s32 i;
	SceLcdcDispMode *dispMode = NULL;
	for (i = 0; i < 13; i++) {
		if (g_lcdcDispModeList[i].id == id) {
			dispMode = &g_lcdcDispModeList[i];
			break;
		}
	}
	if (dispMode == NULL)
		return 0x80000107;
	if (xres >= 480) {
		if (dispMode->x.res < xres)
			return 0x80000104;
	} else if (dispMode->x.res >= 480) {
		return 0x80000104;
	}
	if ((xres & 3) != 0)
		return 0x80000104;
	if (yres >= 272) {
		if (dispMode->y.res < yres)
			return 0x80000104;
	} else if (dispMode->y.res >= 272) {
		return 0x80000104;
	}
	return 0;
}

s32 sceLcdc_driver_E9DBD35F(s32 id, s32 xres, s32 yres, s32 arg3)
{
	s32 ret = sceLcdc_driver_BAF165CE(id, xres, yres, arg3);
	if (ret < 0)
		return ret;
	s32 i;
	SceLcdcDispMode *disp = NULL;
	for (i = 0; i < 13; i++) {
		if (g_lcdcDispModeList[i].id == id) {
			disp = &g_lcdcDispModeList[i];
			break;
		}
	}
	s32 oldIntr = sceKernelCpuSuspendIntr();
	g_lcdc.scaledXres = xres * g_lcdc.scale;
	g_lcdc.yres = yres;
	g_lcdc.v3_28 = arg3;
	g_lcdc.dispId = id;
	if (*(u16 *)g_lcdc.disp.clk != *(u16 *)disp->clk) {
		sysregLcdcClkDisable();
		if (g_lcdc.mobo > 0x7FFFFF) {
			sceSysreg_driver_9746F3B2(disp->clk[0], 1);
			REG32(0xBE1401B0) = disp->clk[1] - 1;
		} else {
			sceSysreg_driver_9746F3B2(disp->clk[0], disp->clk[1]);
		}
		sysregLcdcClkEnable();
	}
	g_lcdc.disp = *disp;
	if (g_lcdc.scale != 1) {
		g_lcdc.disp.x.bPrch *= g_lcdc.scale;
		g_lcdc.disp.x.res *= g_lcdc.scale;
		g_lcdc.disp.x.sync *= g_lcdc.scale;
		g_lcdc.disp.x.fPrch *= g_lcdc.scale;
	}
	if (g_lcdc.disp.syncDiff == 1 && g_lcdc.disp.x.res != g_lcdc.scaledXres)
		g_lcdc.disp.syncDiff = 0;
	if (g_lcdc.v3_88 == 1 && (g_lcdc.disp.flags & 1) != 0)
		g_lcdc.disp.flags |= 0x20;
	g_lcdc.hwData[0] = 0;
	if (g_lcdc.disp.syncDiff == 0) {
		g_lcdc.shiftY = 0;
		g_lcdc.shiftX = 0;
		s32 corr = (g_lcdc.dispId != 0) ? (g_lcdc.v3_64 * g_lcdc.scale) : 0;
		g_lcdc.disp.x.fPrch -= corr;
		g_lcdc.disp.x.sync += corr;
		if (g_lcdc.yres < g_lcdc.disp.y.res) {
			u16 ny = (u16)g_lcdc.yres;
			g_lcdc.disp.y.fPrch += g_lcdc.disp.y.res - ny;
			g_lcdc.disp.y.res = ny;
		}
	}
	g_lcdc.hwCtl->syncDiff = g_lcdc.disp.syncDiff;
	g_lcdc.hwCtl->v2_8 = g_lcdc.v3_28;
	g_lcdc.hwCtl->xbPrch = g_lcdc.disp.x.bPrch;
	g_lcdc.hwCtl->xsync = g_lcdc.disp.x.sync;
	g_lcdc.hwCtl->xfPrch = g_lcdc.disp.x.fPrch;
	g_lcdc.hwCtl->xres = g_lcdc.disp.x.res;
	g_lcdc.hwCtl->ybPrch = g_lcdc.disp.y.bPrch;
	g_lcdc.hwCtl->ysync = g_lcdc.disp.y.sync;
	g_lcdc.hwCtl->yfPrch = g_lcdc.disp.y.fPrch;
	g_lcdc.hwCtl->yres = g_lcdc.disp.y.res;
	g_lcdc.hwCtl->shiftY = g_lcdc.shiftY;
	g_lcdc.hwCtl->shiftX = g_lcdc.shiftX;
	g_lcdc.hwCtl->realX = g_lcdc.realX;
	g_lcdc.hwCtl->realY = g_lcdc.realY;
	if (id == 14) {
		g_lcdc.v3_8 = 0;
		g_lcdc.hwCtl->v2_112 = 0;
	}
	sceKernelCpuResumeIntr(oldIntr);
	return 0;
}

s32 sceLcdc_driver_88C3D167(s32 *arg0, s32 *arg1, s32 *arg2, s32 *arg3)
{
	if (arg0 != NULL) *arg0 = g_lcdc.dispId;
	if (arg1 != NULL) *arg1 = g_lcdc.scaledXres / g_lcdc.scale;
	if (arg2 != NULL) *arg2 = g_lcdc.yres;
	if (arg3 != NULL) *arg3 = g_lcdc.v3_28;
	return 0;
}

s32 sceLcdc_driver_C004BBE0(void)
{
	s32 oldIntr = sceKernelCpuSuspendIntr();
	s32 old = g_lcdc.hwCtl->v2_0;
	g_lcdc.hwCtl->v2_0 = old & ~1;
	if (g_lcdc.mobo > 0x7FFFFF) {
		u32 first = sceKernelGetSystemTimeLow();
		while (sceKernelGetSystemTimeLow() - first < 64)
			;
	}
	g_lcdc.hwCtl->v2_0 = old;
	sceKernelCpuResumeIntr(oldIntr);
	return 0;
}

s32 sceLcdc_driver_2D80BB2E(s32 *arg0, s32 *arg1, s32 *arg2, s32 *arg3, s32 *arg4)
{
	if (arg0 != NULL) *arg0 = g_lcdc.disp.y.fPrch;
	if (arg1 != NULL) *arg1 = g_lcdc.disp.y.bPrch;
	if (arg2 != NULL) *arg2 = g_lcdc.disp.y.sync;
	if (arg3 != NULL) *arg3 = g_lcdc.disp.y.fPrch + g_lcdc.disp.y.bPrch + g_lcdc.disp.y.sync;
	if (arg4 != NULL) *arg4 = g_lcdc.disp.y.fPrch + g_lcdc.disp.y.bPrch + g_lcdc.disp.y.sync + g_lcdc.disp.y.res;
	return 0;
}

s32 sceLcdc_driver_525E7EB1(void)
{
	s32 oldIntr = sceKernelCpuSuspendIntr();
	s32 i = 1;
	s32 ret;
	do {
		if (i >= 101) {
			ret = -1;
			break;
		}
		i++;
		ret = g_lcdc.hwCtl->hpc;
	} while (ret != g_lcdc.hwCtl->hpc);
	sceKernelCpuResumeIntr(oldIntr);
	return ret;
}

s32 sceLcdc_driver_35F45B50(void)
{
	s32 oldIntr = sceKernelCpuSuspendIntr();
	s32 i = 1;
	s32 ret;
	do {
		if (i >= 4) {
			ret = -1;
			break;
		}
		i++;
		ret = g_lcdc.hwCtl->vpc;
	} while (ret != g_lcdc.hwCtl->vpc);
	sceKernelCpuResumeIntr(oldIntr);
	return ret;
}

s32 lcdcModuleStart(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	lcdcInit();
	return 0;
}

s32 lcdcModuleStop(SceSize args)
{
	(void)args;
	sceKernelUnregisterSysEventHandler(&g_lcdcEvent);
	return 0;
}

s32 sceLcdc_driver_0BFF31C5(void)
{
	sysregLcdcIoEnable();
	g_lcdc.v3_0 = 1;
	return 0;
}

s32 sceLcdc_driver_3EDBFCB9(void)
{
	sysregLcdcIoDisable();
	g_lcdc.v3_0 = 0;
	return 0;
}

s32 sceLcdc_driver_451FE1A1(u32 arg)
{
	if (arg >= 9)
		return 0x80000107;
	g_lcdc.tabId = arg;
	return 0;
}

s32 sceLcdc_driver_A3DCEF64(u32 arg)
{
	if (arg >= 3)
		return 0x80000107;
	g_lcdc.v3_88 = arg;
	return 0;
}

s32 sceLcdc_driver_94F83F74(s32 arg0, s32 arg1)
{
	(void)arg0;
	s32 oldIntr = sceKernelCpuSuspendIntr();
	s32 diff = arg1 - g_lcdc.v3_64;
	g_lcdc.v3_64 = arg1;
	if (g_lcdc.dispId != 0 && g_lcdc.disp.syncDiff == 0) {
		s32 m = diff * g_lcdc.scale;
		s32 d1 = g_lcdc.disp.x.fPrch - m;
		s32 d2 = g_lcdc.disp.x.sync + m;
		g_lcdc.hwCtl->xfPrch = d1 & 0xFFFF;
		g_lcdc.disp.x.sync = d2;
		g_lcdc.hwCtl->xres = g_lcdc.disp.x.res;
		g_lcdc.disp.x.fPrch = d1;
	}
	sceKernelCpuResumeIntr(oldIntr);
	return 0;
}

float sceLcdc_driver_1F01BD08(void)
{
	return g_lcdc.disp.sync * (float)g_lcdc.scale;
}

s32 sceLcdc_driver_BB839185(void)
{
	return g_lcdc.scale;
}

float sceLcdc_driver_56307DBC(void)
{
	return g_lcdc.disp.sync;
}

float sceLcdc_driver_C943B9A2(void)
{
	float numPix = (float)(g_lcdc.disp.x.fPrch + g_lcdc.disp.x.bPrch + g_lcdc.disp.x.sync + g_lcdc.disp.x.res);
	return (g_lcdc.disp.sync * (float)g_lcdc.scale) / numPix;
}

float sceLcdc_driver_9D4FB8B1(void)
{
	s32 x = g_lcdc.disp.x.fPrch + g_lcdc.disp.x.bPrch + g_lcdc.disp.x.sync + g_lcdc.disp.x.res;
	s32 y = g_lcdc.disp.y.fPrch + g_lcdc.disp.y.bPrch + g_lcdc.disp.y.sync + g_lcdc.disp.y.res;
	if ((g_lcdc.disp.flags & 5) == 5)
		x >>= 1;
	return (g_lcdc.disp.sync * (float)g_lcdc.scale) / (float)(x * y);
}

s32 sceLcdc_driver_75C4DA9A(void)
{
	s32 v = g_lcdc.hwCtl->v2_80;
	if (v == 1)
		g_lcdc.hwCtl->v2_80 = v;
	return v;
}

s32 sceLcdc_driver_90AA786C(void)
{
	u32 v;
	do {
		v = REG32(0xBE700004);
	} while (v != REG32(0xBE700004));
	return (v ^ 1) & 1;
}

