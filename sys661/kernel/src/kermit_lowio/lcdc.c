#include "lowio_int.h"

/* =========================================================================
 * Part 4: sceLcdc_driver (20 exported functions)
 * ========================================================================= */

typedef struct SceLcdcAxisTiming {
	u16 front_porch;
	u16 back_porch;
	u16 sync_width;
	u16 active_size;
} SceLcdcAxisTiming;

typedef struct SceLcdcDispMode {
	s32 mode_id;
	float pixel_clock;
	u8 clk_cfg[2];
	u8 sync_diff;
	u8 ctrl_flags;
	SceLcdcAxisTiming horiz;
	SceLcdcAxisTiming vert;
} SceLcdcDispMode;

typedef struct SceLcdcScalePreset {
	s32 preset_id;
	u16 out_width;
	u16 h_step;
	u16 h_phase;
	u16 out_height;
	u16 v_step;
	u16 v_phase;
} SceLcdcScalePreset;

typedef struct SceLcdcHwRegs {
	u32 enable_ctrl;
	u32 sync_diff;
	u32 pixel_format;
	u32 reserved_ctrl;
	u32 h_back_porch;
	u32 h_sync_width;
	u32 h_front_porch;
	u32 h_active;
	u32 v_back_porch;
	u32 v_sync_width;
	u32 v_front_porch;
	u32 v_active;
	u32 h_pixel_counter;
	u32 v_line_counter;
	u32 reserved_counter[2];
	u32 shift_y;
	u32 shift_x;
	u32 display_width;
	u32 display_height;
	u32 vblank_status;
	u32 reserved_tail[7];
	u32 output_enable;
} SceLcdcHwRegs;

typedef struct SceLcdcState {
	s32 io_enabled;
	u32 board_ver;
	s32 output_active;
	s32 pixel_scale;
	s32 disp_id;
	s32 scaled_width;
	s32 height;
	s32 pixel_format;
	volatile SceLcdcHwRegs *hw_regs;
	SceLcdcDispMode cur_mode;
	s32 h_shift_adjust;
	s32 shift_x;
	s32 shift_y;
	s32 display_width;
	s32 display_height;
	s32 scale_preset_id;
	s32 scale_policy;
	s32 scaler_enable;
	s32 scaler_in_width;
	s32 scaler_in_height;
	s32 scaler_h_phase;
	s32 scaler_v_phase;
	s32 scaler_h_step;
	s32 scaler_v_step;
} SceLcdcState;

static SceLcdcState g_lcdc;

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

static SceLcdcScalePreset g_lcdcScalePresets[8] = {
	{ 1, 0x02BA, 0x0AFC, 0x0580, 0x0168, 0x0C18, 0x060C },
	{ 2, 0x0298, 0x0B90, 0x05C8, 0x0156, 0x0CB8, 0x065C },
	{ 3, 0x0274, 0x0C34, 0x061C, 0x0144, 0x0D70, 0x06B8 },
	{ 4, 0x0251, 0x0CEC, 0x0678, 0x0132, 0x0E38, 0x071C },
	{ 5, 0x02BA, 0x0AFC, 0x0580, 0x01E0, 0x0910, 0x0488 },
	{ 6, 0x0298, 0x0B90, 0x05C8, 0x01C8, 0x098C, 0x04C4 },
	{ 7, 0x0274, 0x0C34, 0x061C, 0x01B0, 0x0A14, 0x0508 },
	{ 8, 0x0251, 0x0CEC, 0x0678, 0x0198, 0x0AAC, 0x0554 }
};

static int lcdcSysEventHandler(int ev_id, char *ev_name, void *ev_param, int *result);
s32 sceLcdc_driver_5DF2F52A(void);

static SceSysEventHandler g_lcdcEvent = {
	sizeof(SceSysEventHandler),
	"SceLcdc",
	0x00FFFF00,
	lcdcSysEventHandler,
	0, 0, NULL,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

static s32 __attribute__((noinline)) lcdcResetApbTimer(void)
{
	sceSysregApbTimerClkDisable(0);
	sceSysregApbTimerClkSelect(0, 4);
	sceSysregApbTimerClkEnable(0);
	while ((REG32(0xBE700020) & 4U) != 0)
		;
	REG32(0xBE700000) = 0;
	while ((REG32(0xBE700020) & 4U) != 0)
		;
	while ((REG32(0xBE700020) & 2U) != 0)
		;
	REG32(0xBE700004) = 0;
	while ((REG32(0xBE700020) & 8U) != 0)
		;
	REG32(0xBE70000C) = 0xFFFFFFFFU;
	while ((REG32(0xBE700020) & 0x10U) != 0)
		;
	REG32(0xBE700010) = 0;
	while ((REG32(0xBE700020) & 0x20U) != 0)
		;
	REG32(0xBE700014) = 0;
	while ((REG32(0xBE700020) & 0x3AU) != 0)
		;
	REG32(0xBE700000) = 1;
	while ((REG32(0xBE700020) & 4U) != 0)
		;
	sceSysregApbTimerClkDisable(0);
	sceSysregApbTimerClkSelect(0, 6);
	sceSysregApbTimerClkEnable(0);
	REG32(0xBE700024) = 1;
	return 0;
}

static s32 __attribute__((noinline)) lcdcInit(void)
{
	memset(&g_lcdc, 0, sizeof(g_lcdc));
	g_lcdc.io_enabled = 1;
	sysregLcdcClkEnable();
	sysregLcdcIoEnable();
	g_lcdc.pixel_scale = 3;
	g_lcdc.board_ver = 0x00600000;
	g_lcdc.output_active = 1;
	g_lcdcDispModeList[8].clk_cfg[1] = 1;
	g_lcdcDispModeList[3].mode_id = -1;
	g_lcdcDispModeList[0].clk_cfg[1] = 1;
	g_lcdcDispModeList[1].clk_cfg[0] = 1;
	g_lcdcDispModeList[1].clk_cfg[1] = 1;
	g_lcdcDispModeList[2].mode_id = -1;
	g_lcdcDispModeList[4].clk_cfg[0] = 1;
	g_lcdcDispModeList[4].clk_cfg[1] = 1;
	g_lcdcDispModeList[5].clk_cfg[0] = 1;
	g_lcdcDispModeList[5].clk_cfg[1] = 1;
	g_lcdcDispModeList[6].clk_cfg[0] = 1;
	g_lcdcDispModeList[6].clk_cfg[1] = 1;
	g_lcdcDispModeList[7].clk_cfg[1] = 1;
	g_lcdcDispModeList[8].clk_cfg[0] = 1;
	REG32(0xBE140070) = 1;
	g_lcdc.hw_regs = (volatile SceLcdcHwRegs *)0xBE140000;
	sceLcdc_driver_5DF2F52A();
	sceKernelRegisterSysEventHandler(&g_lcdcEvent);
	return 0;
}

static s32 __attribute__((noinline)) lcdcConfigureTiming(void)
{
	sysregLcdcClkDisable();
	sceSysreg_driver_9746F3B2(0, 3);
	sysregLcdcClkEnable();
	if (g_lcdc.board_ver > 0x007FFFFFU) {
		REG32(0xBE140000) = 0;
		REG32(0xBE140200) = 1;
	}
	volatile SceLcdcHwRegs *regs = g_lcdc.hw_regs;
	regs->sync_diff = g_lcdc.cur_mode.sync_diff;
	if (g_lcdc.board_ver > 0x007FFFFFU) {
		if ((REG32(0xBE1401A0) & 1U) == 0 && (g_lcdc.cur_mode.ctrl_flags & 1U) != 0)
			lcdcResetApbTimer();
		REG32(0xBE1401A0) = (u32)(g_lcdc.cur_mode.ctrl_flags & 0xFEU);
		REG32(0xBE140180) = (u32)g_lcdc.scaler_enable;
		REG32(0xBE140184) = (u32)g_lcdc.scaler_in_width;
		REG32(0xBE140188) = (u32)g_lcdc.scaler_in_height;
		REG32(0xBE14018C) = (u32)g_lcdc.scaler_h_phase;
		REG32(0xBE140190) = (u32)g_lcdc.scaler_v_phase;
		REG32(0xBE140194) = (u32)g_lcdc.scaler_h_step;
		REG32(0xBE140198) = (u32)g_lcdc.scaler_v_step;
	}
	regs->pixel_format = (u32)g_lcdc.pixel_format;
	regs->h_back_porch = g_lcdc.cur_mode.horiz.back_porch;
	regs->h_sync_width = g_lcdc.cur_mode.horiz.sync_width;
	regs->h_front_porch = g_lcdc.cur_mode.horiz.front_porch;
	regs->h_active = g_lcdc.cur_mode.horiz.active_size;
	regs->v_back_porch = g_lcdc.cur_mode.vert.back_porch;
	regs->v_sync_width = g_lcdc.cur_mode.vert.sync_width;
	regs->v_front_porch = g_lcdc.cur_mode.vert.front_porch;
	regs->v_active = g_lcdc.cur_mode.vert.active_size;
	regs->shift_y = (u32)g_lcdc.shift_y;
	regs->shift_x = (u32)g_lcdc.shift_x;
	regs->display_width = (u32)g_lcdc.display_width;
	regs->display_height = (u32)g_lcdc.display_height;
	if (g_lcdc.board_ver > 0x007FFFFFU)
		REG32(0xBE1401A0) = g_lcdc.cur_mode.ctrl_flags;
	sceKermit_driver_D69C50BB();
	if (g_lcdc.output_active != 0)
		regs->output_enable = 1;
	sysregLcdcClkDisable();
	if (g_lcdc.board_ver > 0x007FFFFFU) {
		sceSysreg_driver_9746F3B2(g_lcdc.cur_mode.clk_cfg[0], 1);
		REG32(0xBE1401B0) = (u32)(g_lcdc.cur_mode.clk_cfg[1] - 1);
	} else {
		sceSysreg_driver_9746F3B2(g_lcdc.cur_mode.clk_cfg[0], g_lcdc.cur_mode.clk_cfg[1]);
	}
	sysregLcdcClkEnable();
	if (g_lcdc.io_enabled != 0)
		sysregLcdcIoEnable();
	return 0;
}

s32 sceLcdc_driver_5DF2F52A(void)
{
	volatile SceLcdcHwRegs *regs = g_lcdc.hw_regs;
	g_lcdc.cur_mode.sync_diff = (u8)regs->sync_diff;
	if (g_lcdc.board_ver > 0x007FFFFFU) {
		g_lcdc.cur_mode.ctrl_flags = (u8)REG32(0xBE1401A0);
		g_lcdc.scaler_enable = (s32)REG32(0xBE140180);
		g_lcdc.scaler_in_width = (s32)REG32(0xBE140184);
		g_lcdc.scaler_in_height = (s32)REG32(0xBE140188);
		g_lcdc.scaler_h_phase = (s32)REG32(0xBE14018C);
		g_lcdc.scaler_v_phase = (s32)REG32(0xBE140190);
		g_lcdc.scaler_h_step = (s32)REG32(0xBE140194);
		g_lcdc.scaler_v_step = (s32)REG32(0xBE140198);
	}
	g_lcdc.pixel_format = (s32)regs->pixel_format;
	u32 h_sync = regs->h_sync_width;
	u32 h_fp = regs->h_front_porch;
	u32 h_act = regs->h_active;
	u32 v_bp = regs->v_back_porch;
	u32 v_sync = regs->v_sync_width;
	u32 v_fp = regs->v_front_porch;
	u32 v_act = regs->v_active;
	u32 sh_y = regs->shift_y;
	g_lcdc.cur_mode.horiz.back_porch = (u16)regs->h_back_porch;
	g_lcdc.shift_y = (s32)sh_y;
	u32 sh_x = regs->shift_x;
	g_lcdc.cur_mode.horiz.sync_width = (u16)h_sync;
	g_lcdc.shift_x = (s32)sh_x;
	u32 disp_w = regs->display_width;
	g_lcdc.cur_mode.horiz.front_porch = (u16)h_fp;
	g_lcdc.display_width = (s32)disp_w;
	u32 disp_h = regs->display_height;
	g_lcdc.cur_mode.horiz.active_size = (u16)h_act;
	g_lcdc.cur_mode.vert.back_porch = (u16)v_bp;
	g_lcdc.cur_mode.vert.sync_width = (u16)v_sync;
	g_lcdc.cur_mode.vert.front_porch = (u16)v_fp;
	g_lcdc.cur_mode.vert.active_size = (u16)v_act;
	g_lcdc.cur_mode.pixel_clock = 9000000.0f;
	g_lcdc.display_height = (s32)disp_h;
	if (g_lcdc.cur_mode.sync_diff != 0) {
		if (g_lcdc.scaler_enable == 1) {
			g_lcdc.scaled_width = g_lcdc.scaler_in_width;
			g_lcdc.height = g_lcdc.scaler_in_height;
		} else {
			g_lcdc.height = (s32)disp_h;
			g_lcdc.scaled_width = (s32)disp_w;
		}
	} else {
		g_lcdc.height = (s32)(u16)v_act;
		g_lcdc.scaled_width = (s32)(u16)h_act;
	}
	u32 clk_reg = REG32(0xBC100060);
	u32 div_bits = clk_reg & 0x300000U;
	g_lcdc.cur_mode.clk_cfg[0] = (u8)((clk_reg >> 22) & 1U);
	switch (div_bits) {
	case 0x100000U:
		g_lcdc.cur_mode.clk_cfg[1] = 1;
		break;
	case 0x200000U:
		g_lcdc.cur_mode.clk_cfg[1] = 2;
		break;
	case 0x300000U:
		g_lcdc.cur_mode.clk_cfg[0] = 2;
		g_lcdc.cur_mode.clk_cfg[1] = 1;
		break;
	case 0U:
		g_lcdc.cur_mode.clk_cfg[1] = 3;
		break;
	}
	if (g_lcdc.board_ver > 0x007FFFFFU)
		g_lcdc.cur_mode.clk_cfg[1] = (u8)(g_lcdc.cur_mode.clk_cfg[1] * (REG32(0xBE1401B0) + 1U));
	s32 h_total = g_lcdc.cur_mode.horiz.front_porch + g_lcdc.cur_mode.horiz.back_porch +
	              g_lcdc.cur_mode.horiz.sync_width + g_lcdc.cur_mode.horiz.active_size;
	s32 v_total = g_lcdc.cur_mode.vert.front_porch + g_lcdc.cur_mode.vert.back_porch +
	              g_lcdc.cur_mode.vert.sync_width + g_lcdc.cur_mode.vert.active_size;
	u8 mode_flags = g_lcdc.cur_mode.ctrl_flags & 7U;
	s32 idx;
	for (idx = 0; idx < 13; idx++) {
		SceLcdcDispMode *cur = &g_lcdcDispModeList[idx];
		s32 cur_h = (cur->horiz.front_porch + cur->horiz.back_porch +
		             cur->horiz.sync_width + cur->horiz.active_size) * g_lcdc.pixel_scale;
		if (h_total == cur_h) {
			s32 cur_v = cur->vert.front_porch + cur->vert.back_porch +
			            cur->vert.sync_width + cur->vert.active_size;
			if (v_total == cur_v &&
			    g_lcdc.cur_mode.vert.back_porch == cur->vert.back_porch &&
			    mode_flags == cur->ctrl_flags) {
				g_lcdc.cur_mode.pixel_clock = cur->pixel_clock;
				g_lcdc.disp_id = cur->mode_id;
				if (cur->mode_id != 0 && g_lcdc.cur_mode.sync_diff == 0)
					g_lcdc.h_shift_adjust = ((s32)g_lcdc.cur_mode.horiz.sync_width / g_lcdc.pixel_scale) -
					                        (s32)cur->horiz.sync_width;
				break;
			}
		}
	}
	g_lcdc.scale_policy = 0;
	g_lcdc.scale_preset_id = 0;
	if (g_lcdc.scaler_enable == 1) {
		for (idx = 0; idx < 8; idx++) {
			if (g_lcdc.scaler_v_step == (s32)g_lcdcScalePresets[idx].v_step) {
				g_lcdc.scale_preset_id = g_lcdcScalePresets[idx].preset_id;
				break;
			}
		}
		if (g_lcdc.scale_preset_id == 0)
			g_lcdc.scale_policy = 2;
	} else if ((g_lcdc.cur_mode.ctrl_flags & 0x20U) != 0) {
		g_lcdc.scale_policy = 1;
	}
	return 0;
}

static int lcdcSysEventHandler(int ev_id, char *ev_name, void *ev_param, int *result)
{
	(void)ev_name;
	(void)result;
	const SceLowioSysEventParam *ev = (const SceLowioSysEventParam *)ev_param;
	if (ev_id == 0x4004) {
		if (ev->body.suspend_kind != 2) {
			sysregLcdcIoDisable();
			sysregLcdcClkDisable();
		}
	} else if (ev_id == 0x10004) {
		if (ev->body.resume_flags[1] == 0x80000000U)
			sceLcdc_driver_5DF2F52A();
		else
			lcdcConfigureTiming();
	}
	return 0;
}

s32 sceLcdc_driver_BAF165CE(s32 mode_id, s32 width, s32 height, s32 pixel_format)
{
	if ((u32)pixel_format >= 4U)
		return (s32)0x80000108;
	s32 idx;
	SceLcdcDispMode *disp_mode = NULL;
	for (idx = 0; idx < 13; idx++) {
		if (g_lcdcDispModeList[idx].mode_id == mode_id) {
			disp_mode = &g_lcdcDispModeList[idx];
			break;
		}
	}
	if (disp_mode == NULL)
		return (s32)0x80000107;
	if (width >= 480) {
		if ((s32)disp_mode->horiz.active_size < width)
			return (s32)0x80000104;
	} else if (disp_mode->horiz.active_size >= 480U) {
		return (s32)0x80000104;
	}
	if ((width & 3) != 0)
		return (s32)0x80000104;
	if (height >= 272) {
		if ((s32)disp_mode->vert.active_size < height)
			return (s32)0x80000104;
	} else if (disp_mode->vert.active_size >= 272U) {
		return (s32)0x80000104;
	}
	return 0;
}

s32 sceLcdc_driver_E9DBD35F(s32 mode_id, s32 width, s32 height, s32 pixel_format)
{
	s32 ret = sceLcdc_driver_BAF165CE(mode_id, width, height, pixel_format);
	if (ret < 0)
		return ret;
	s32 idx;
	SceLcdcDispMode *disp = NULL;
	for (idx = 0; idx < 13; idx++) {
		if (g_lcdcDispModeList[idx].mode_id == mode_id) {
			disp = &g_lcdcDispModeList[idx];
			break;
		}
	}
	s32 scaled_w = width * g_lcdc.pixel_scale;
	s32 intr = sceKernelCpuSuspendIntr();
	g_lcdc.scaled_width = scaled_w;
	g_lcdc.height = height;
	g_lcdc.pixel_format = pixel_format;
	g_lcdc.disp_id = mode_id;
	if (g_lcdc.cur_mode.clk_cfg[0] != disp->clk_cfg[0] ||
	    g_lcdc.cur_mode.clk_cfg[1] != disp->clk_cfg[1]) {
		sysregLcdcClkDisable();
		if (g_lcdc.board_ver > 0x007FFFFFU) {
			sceSysreg_driver_9746F3B2(disp->clk_cfg[0], 1);
			REG32(0xBE1401B0) = (u32)(disp->clk_cfg[1] - 1);
		} else {
			sceSysreg_driver_9746F3B2(disp->clk_cfg[0], disp->clk_cfg[1]);
		}
		sysregLcdcClkEnable();
	}
	g_lcdc.cur_mode = *disp;
	if (g_lcdc.pixel_scale != 1) {
		g_lcdc.cur_mode.horiz.back_porch = (u16)(g_lcdc.cur_mode.horiz.back_porch * g_lcdc.pixel_scale);
		g_lcdc.cur_mode.horiz.active_size = (u16)(g_lcdc.cur_mode.horiz.active_size * g_lcdc.pixel_scale);
		g_lcdc.cur_mode.horiz.sync_width = (u16)(g_lcdc.cur_mode.horiz.sync_width * g_lcdc.pixel_scale);
		g_lcdc.cur_mode.horiz.front_porch = (u16)(g_lcdc.cur_mode.horiz.front_porch * g_lcdc.pixel_scale);
	}
	if (g_lcdc.cur_mode.sync_diff == 1 && (s32)g_lcdc.cur_mode.horiz.active_size == g_lcdc.scaled_width)
		g_lcdc.cur_mode.sync_diff = 0;
	if (g_lcdc.scale_policy == 1 && (g_lcdc.cur_mode.ctrl_flags & 1U) != 0)
		g_lcdc.cur_mode.ctrl_flags |= 0x20U;
	g_lcdc.scaler_enable = 0;
	if (g_lcdc.cur_mode.sync_diff == 0) {
		g_lcdc.shift_y = 0;
		g_lcdc.shift_x = 0;
		s32 corr = (g_lcdc.disp_id != 0) ? (g_lcdc.h_shift_adjust * g_lcdc.pixel_scale) : 0;
		g_lcdc.cur_mode.horiz.sync_width = (u16)(g_lcdc.cur_mode.horiz.sync_width + corr);
		g_lcdc.cur_mode.horiz.front_porch = (u16)(g_lcdc.cur_mode.horiz.front_porch - corr);
		if (g_lcdc.height < (s32)g_lcdc.cur_mode.vert.active_size) {
			u16 old_h = g_lcdc.cur_mode.vert.active_size;
			g_lcdc.cur_mode.vert.active_size = (u16)g_lcdc.height;
			g_lcdc.cur_mode.vert.front_porch = (u16)(g_lcdc.cur_mode.vert.front_porch + old_h - (u16)g_lcdc.height);
		}
	} else if (g_lcdc.cur_mode.horiz.active_size >= 481U && g_lcdc.scale_preset_id != 0) {
		const SceLcdcScalePreset *preset = &g_lcdcScalePresets[g_lcdc.scale_preset_id - 1];
		s32 out_w = (s32)preset->out_width;
		s32 out_h = (s32)preset->out_height;
		g_lcdc.scaler_enable = 1;
		g_lcdc.shift_y = ((s32)g_lcdc.cur_mode.vert.active_size - out_h) / 2;
		g_lcdc.scaler_in_width = g_lcdc.scaled_width;
		g_lcdc.shift_x = ((s32)g_lcdc.cur_mode.horiz.active_size - out_w) / 2;
		g_lcdc.scaler_in_height = g_lcdc.height;
		g_lcdc.display_width = out_w;
		g_lcdc.scaler_h_phase = (s32)preset->h_phase;
		g_lcdc.display_height = out_h;
		g_lcdc.scaler_v_phase = (s32)preset->v_phase;
		g_lcdc.scaler_h_step = (s32)preset->h_step;
		g_lcdc.scaler_v_step = (s32)preset->v_step;
	} else if (g_lcdc.scale_policy == 2) {
		g_lcdc.scaler_enable = 1;
		g_lcdc.shift_y = ((s32)g_lcdc.cur_mode.vert.active_size - g_lcdc.height) / 2;
		g_lcdc.scaler_in_width = g_lcdc.scaled_width;
		g_lcdc.shift_x = ((s32)g_lcdc.cur_mode.horiz.active_size - g_lcdc.scaled_width) / 2;
		g_lcdc.scaler_in_height = g_lcdc.height;
		g_lcdc.display_width = g_lcdc.scaled_width;
		g_lcdc.scaler_h_phase = 2048;
		g_lcdc.display_height = g_lcdc.height;
		g_lcdc.scaler_v_phase = 2048;
		g_lcdc.scaler_h_step = 4096;
		g_lcdc.scaler_v_step = 4096;
	} else {
		s32 sy = ((s32)g_lcdc.cur_mode.vert.active_size - g_lcdc.height) / 2;
		if (mode_id == 6)
			sy = 0;
		if (sy == 0) {
			u16 delta = ((g_lcdc.cur_mode.ctrl_flags & 1U) != 0) ? 2U : 1U;
			sy = 1;
			g_lcdc.cur_mode.vert.sync_width = (u16)(g_lcdc.cur_mode.vert.sync_width - delta);
			g_lcdc.cur_mode.vert.active_size = (u16)(g_lcdc.cur_mode.vert.active_size + delta);
		}
		s32 step_unit = 8 * g_lcdc.pixel_scale;
		s32 sx = (((s32)g_lcdc.cur_mode.horiz.active_size - g_lcdc.scaled_width) / 2) / step_unit * step_unit;
		if (sx >= 240) {
			s32 excess = sx - 240;
			g_lcdc.cur_mode.horiz.active_size = (u16)(g_lcdc.cur_mode.horiz.active_size - excess);
			sx = 240;
			g_lcdc.cur_mode.horiz.sync_width = (u16)(g_lcdc.cur_mode.horiz.sync_width + excess);
			if (g_lcdc.cur_mode.horiz.sync_width >= 256U) {
				u16 over = (u16)(g_lcdc.cur_mode.horiz.sync_width - 255U);
				g_lcdc.cur_mode.horiz.sync_width = 255U;
				if (g_lcdc.board_ver > 0x004FFFFFU) {
					g_lcdc.cur_mode.horiz.back_porch = (u16)(g_lcdc.cur_mode.horiz.back_porch + over);
					over = 0;
				}
				g_lcdc.cur_mode.horiz.active_size = (u16)(g_lcdc.cur_mode.horiz.active_size + over);
			}
		}
		g_lcdc.shift_y = sy;
		g_lcdc.display_height = g_lcdc.height;
		g_lcdc.shift_x = sx;
		g_lcdc.display_width = g_lcdc.scaled_width;
	}
	volatile SceLcdcHwRegs *regs = g_lcdc.hw_regs;
	regs->sync_diff = g_lcdc.cur_mode.sync_diff;
	if (g_lcdc.board_ver > 0x007FFFFFU) {
		if ((REG32(0xBE1401A0) & 1U) == 0 && (g_lcdc.cur_mode.ctrl_flags & 1U) != 0)
			lcdcResetApbTimer();
		REG32(0xBE1401A0) = (u32)(g_lcdc.cur_mode.ctrl_flags & 0xFEU);
		REG32(0xBE140180) = (u32)g_lcdc.scaler_enable;
		REG32(0xBE140184) = (u32)g_lcdc.scaler_in_width;
		REG32(0xBE140188) = (u32)g_lcdc.scaler_in_height;
		REG32(0xBE14018C) = (u32)g_lcdc.scaler_h_phase;
		REG32(0xBE140190) = (u32)g_lcdc.scaler_v_phase;
		REG32(0xBE140194) = (u32)g_lcdc.scaler_h_step;
		REG32(0xBE140198) = (u32)g_lcdc.scaler_v_step;
	}
	regs->pixel_format = (u32)g_lcdc.pixel_format;
	regs->h_back_porch = g_lcdc.cur_mode.horiz.back_porch;
	regs->h_sync_width = g_lcdc.cur_mode.horiz.sync_width;
	regs->h_front_porch = g_lcdc.cur_mode.horiz.front_porch;
	regs->h_active = g_lcdc.cur_mode.horiz.active_size;
	regs->v_back_porch = g_lcdc.cur_mode.vert.back_porch;
	regs->v_sync_width = g_lcdc.cur_mode.vert.sync_width;
	regs->v_front_porch = g_lcdc.cur_mode.vert.front_porch;
	regs->v_active = g_lcdc.cur_mode.vert.active_size;
	regs->shift_y = (u32)g_lcdc.shift_y;
	regs->shift_x = (u32)g_lcdc.shift_x;
	regs->display_width = (u32)g_lcdc.display_width;
	regs->display_height = (u32)g_lcdc.display_height;
	if (g_lcdc.board_ver > 0x007FFFFFU)
		REG32(0xBE1401A0) = g_lcdc.cur_mode.ctrl_flags;
	if (mode_id == 14) {
		g_lcdc.output_active = 0;
		regs->output_enable = 0;
	}
	sceKernelCpuResumeIntr(intr);
	return 0;
}

s32 sceLcdc_driver_88C3D167(s32 *out_mode_id, s32 *out_width, s32 *out_height, s32 *out_format)
{
	if (out_mode_id != NULL)
		*out_mode_id = g_lcdc.disp_id;
	if (out_width != NULL)
		*out_width = g_lcdc.scaled_width / g_lcdc.pixel_scale;
	if (out_height != NULL)
		*out_height = g_lcdc.height;
	if (out_format != NULL)
		*out_format = g_lcdc.pixel_format;
	return 0;
}

s32 sceLcdc_driver_C004BBE0(void)
{
	s32 intr = sceKernelCpuSuspendIntr();
	volatile SceLcdcHwRegs *regs = g_lcdc.hw_regs;
	u32 prev = regs->enable_ctrl;
	regs->enable_ctrl = prev & ~1U;
	if (g_lcdc.board_ver > 0x007FFFFFU) {
		u32 start_us = sceKernelGetSystemTimeLow();
		while ((u32)(sceKernelGetSystemTimeLow() - start_us) < 64U)
			;
	}
	regs->enable_ctrl = prev;
	sceKernelCpuResumeIntr(intr);
	return 0;
}

s32 sceLcdc_driver_2D80BB2E(s32 *out_fp, s32 *out_bp, s32 *out_sync, s32 *out_blank, s32 *out_total)
{
	if (out_fp != NULL)
		*out_fp = g_lcdc.cur_mode.vert.front_porch;
	if (out_bp != NULL)
		*out_bp = g_lcdc.cur_mode.vert.back_porch;
	if (out_sync != NULL)
		*out_sync = g_lcdc.cur_mode.vert.sync_width;
	if (out_blank != NULL)
		*out_blank = g_lcdc.cur_mode.vert.front_porch + g_lcdc.cur_mode.vert.back_porch +
		             g_lcdc.cur_mode.vert.sync_width;
	if (out_total != NULL)
		*out_total = g_lcdc.cur_mode.vert.front_porch + g_lcdc.cur_mode.vert.back_porch +
		             g_lcdc.cur_mode.vert.sync_width + g_lcdc.cur_mode.vert.active_size;
	return 0;
}

s32 sceLcdc_driver_525E7EB1(void)
{
	volatile SceLcdcHwRegs *regs = g_lcdc.hw_regs;
	s32 intr = sceKernelCpuSuspendIntr();
	s32 tries = 1;
	s32 val;
	do {
		if (tries >= 101) {
			val = -1;
			break;
		}
		tries++;
		val = (s32)regs->h_pixel_counter;
	} while (val != (s32)regs->h_pixel_counter);
	sceKernelCpuResumeIntr(intr);
	return val;
}

s32 sceLcdc_driver_35F45B50(void)
{
	volatile SceLcdcHwRegs *regs = g_lcdc.hw_regs;
	s32 intr = sceKernelCpuSuspendIntr();
	s32 tries = 1;
	s32 val;
	do {
		if (tries >= 4) {
			val = -1;
			break;
		}
		tries++;
		val = (s32)regs->v_line_counter;
	} while (val != (s32)regs->v_line_counter);
	sceKernelCpuResumeIntr(intr);
	return val;
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
	g_lcdc.io_enabled = 1;
	return 0;
}

s32 sceLcdc_driver_3EDBFCB9(void)
{
	sysregLcdcIoDisable();
	g_lcdc.io_enabled = 0;
	return 0;
}

s32 sceLcdc_driver_451FE1A1(u32 preset_id)
{
	if (preset_id >= 9U)
		return (s32)0x80000107;
	g_lcdc.scale_preset_id = (s32)preset_id;
	return 0;
}

s32 sceLcdc_driver_A3DCEF64(u32 policy)
{
	if (policy >= 3U)
		return (s32)0x80000107;
	g_lcdc.scale_policy = (s32)policy;
	return 0;
}

s32 sceLcdc_driver_94F83F74(s32 unused_channel, s32 h_shift)
{
	(void)unused_channel;
	s32 intr = sceKernelCpuSuspendIntr();
	s32 delta = h_shift - g_lcdc.h_shift_adjust;
	g_lcdc.h_shift_adjust = h_shift;
	if (g_lcdc.disp_id != 0 && g_lcdc.cur_mode.sync_diff == 0) {
		s32 scaled_delta = delta * g_lcdc.pixel_scale;
		u16 new_fp = (u16)(g_lcdc.cur_mode.horiz.front_porch - scaled_delta);
		u16 new_sync = (u16)(g_lcdc.cur_mode.horiz.sync_width + scaled_delta);
		g_lcdc.hw_regs->h_front_porch = new_fp;
		g_lcdc.cur_mode.horiz.sync_width = new_sync;
		g_lcdc.hw_regs->h_active = g_lcdc.cur_mode.horiz.active_size;
		g_lcdc.cur_mode.horiz.front_porch = new_fp;
	}
	sceKernelCpuResumeIntr(intr);
	return 0;
}

float sceLcdc_driver_1F01BD08(void)
{
	return g_lcdc.cur_mode.pixel_clock * (float)g_lcdc.pixel_scale;
}

s32 sceLcdc_driver_BB839185(void)
{
	return g_lcdc.pixel_scale;
}

float sceLcdc_driver_56307DBC(void)
{
	return g_lcdc.cur_mode.pixel_clock;
}

float sceLcdc_driver_C943B9A2(void)
{
	float line_pixels = (float)(g_lcdc.cur_mode.horiz.front_porch + g_lcdc.cur_mode.horiz.back_porch +
	                            g_lcdc.cur_mode.horiz.sync_width + g_lcdc.cur_mode.horiz.active_size);
	return (g_lcdc.cur_mode.pixel_clock * (float)g_lcdc.pixel_scale) / line_pixels;
}

float sceLcdc_driver_9D4FB8B1(void)
{
	s32 h_total = g_lcdc.cur_mode.horiz.front_porch + g_lcdc.cur_mode.horiz.back_porch +
	              g_lcdc.cur_mode.horiz.sync_width + g_lcdc.cur_mode.horiz.active_size;
	s32 v_total = g_lcdc.cur_mode.vert.front_porch + g_lcdc.cur_mode.vert.back_porch +
	              g_lcdc.cur_mode.vert.sync_width + g_lcdc.cur_mode.vert.active_size;
	if ((g_lcdc.cur_mode.ctrl_flags & 5U) == 5U)
		h_total >>= 1;
	return (g_lcdc.cur_mode.pixel_clock * (float)g_lcdc.pixel_scale) / (float)(h_total * v_total);
}

s32 sceLcdc_driver_75C4DA9A(void)
{
	s32 status = (s32)g_lcdc.hw_regs->vblank_status;
	if (status == 1)
		g_lcdc.hw_regs->vblank_status = (u32)status;
	return status;
}

s32 sceLcdc_driver_90AA786C(void)
{
	u32 val;
	do {
		val = REG32(0xBE700004);
	} while (val != REG32(0xBE700004));
	return (s32)((val ^ 1U) & 1U);
}
