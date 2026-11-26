/*
 * ePSP 6.61 sceImpose_Driver (impose_kermit.prx)
 *
 * Reimplemented from the decrypted 6.61 flash0:/kd/impose_kermit.prx binary.
 */

#include <kermit.h>

/* =========================================================================
 * External Imports
 * ========================================================================= */

/* InitForKernel */
extern s32 sceKernelBootFrom(void);
extern s32 sceKernelApplicationType(void);
extern s32 sceKernelSetInitCallback(s32 (*cb)(void *, s32, void *), s32 priority, void *opt);
extern void *sceKernelInitDiscImage(void);

/* LoadExecForKernel */
extern s32 sceKernelInvokeExitCallback(void);
extern s32 sceKernelCheckExitCallback(void);

/* IoFileMgrForKernel */
extern s32 sceIoOpen(const char *file, s32 flags, s32 mode);
extern s32 sceIoClose(s32 file_id);
extern s32 sceIoRead(s32 file_id, void *data, u32 size);
extern s64 sceIoLseek(s32 file_id, s64 offset, s32 whence);
extern s32 sceIoDevctl(const char *dev, u32 cmd, void *indata, s32 inlen, void *outdata, s32 outlen);

/* KDebugForKernel */
extern s32 sceKernelIsDVDMode(void);

/* SysMemForKernel */
extern s32 sceKernelGetModel(void);
extern void *sceKernelMemset32(void *dst, s32 val, u32 size);
extern s32 SysMemForKernel_2A8B8B2D(void *tick_ptr);
extern s32 sceKernelPartitionTotalMemSize(s32 pid);

/* ThreadManForKernel */
typedef struct SceImposeThreadOpt {
    u32 size;
    s32 stack_mpid;
} SceImposeThreadOpt;

extern s32 sceKernelCreateThread(const char *name, s32 (*entry)(u32, void *), s32 prio, u32 stack, u32 attr, void *opt);
extern s32 sceKernelDeleteThread(s32 thid);
extern s32 sceKernelStartThread(s32 thid, u32 arglen, void *argp);
extern s32 sceKernelExitThread(s32 status);
extern s32 sceKernelWaitThreadEnd(s32 thid, u32 *timeout);
extern u32 sceKernelGetSystemTimeLow(void);
extern s32 sceKernelCreateFpl(const char *name, s32 pid, u32 attr, u32 size, u32 blocks, void *opt);
extern s32 sceKernelDeleteFpl(s32 fplid);
extern s32 sceKernelTryAllocateFpl(s32 fplid, void **data);
extern s32 sceKernelFreeFpl(s32 fplid, void *data);

/* sceDisplay_driver */
extern s32 sceDisplayEnable(void);
extern s32 sceDisplayDisable(void);
extern s32 sceDisplaySetHoldMode(s32 mode);
extern s32 sceDisplaySetMode(s32 mode, s32 width, s32 height);
extern s32 sceDisplayGetMode(s32 *pmode, s32 *pwidth, s32 *pheight);
extern s32 sceDisplay_driver_3E17FE8D(s32 idx, void *topaddr, s32 width, s32 psm, s32 sync);
extern s32 sceDisplay_driver_7DF044BA(s32 mode);
extern s32 sceDisplay_driver_996881D2(void);
extern s32 sceDisplay_driver_9B18DDDD(s32 color_space);
extern s32 sceDisplay_driver_D94EB277(s32 cable_type);
extern s32 sceDisplay_driver_F455917F(s32 active_backlight);
extern s32 sceDisplaySetBacklightSel(s32 sel);
extern s32 sceDisplayGetBacklightSel(s32 *sel);

/* sceCtrl_driver */
typedef struct SceCtrlData {
    u32 TimeStamp;
    u32 Buttons;
    u8 Lx;
    u8 Ly;
    u8 Rsrv[6];
} SceCtrlData;

extern s32 sceCtrlPeekBufferPositive(SceCtrlData *pad_data, s32 count);
extern s32 sceCtrlGetButtonIntercept(u32 button_mask);
extern s32 sceCtrlSetButtonIntercept(u32 button_mask, s32 mode);
extern s32 sceCtrlSetSpecialButtonCallback(u32 idx, u32 mask, void (*cb)(u32, u32, u32), u32 arg);

/* scePower_driver */
extern s32 scePower_driver_31AEA94C(void);
extern s32 scePower_driver_7A9EA6DE(void);
extern s32 scePower_driver_872F4ECE(void);
extern s32 scePower_driver_8C873AA7(void);
extern s32 scePower_driver_FA651CE1(void);

/* sceAudio_driver */
extern s32 sceAudio_driver_5182B550(u32 mask);

/* sceHprm_driver */
extern s32 sceHprm_driver_0B83352B(s32 port, s32 *status_out, s32 flags, s32 opt);
extern s32 sceHprmPeekCurrentKey(u32 *key);
extern s32 sceHprm_driver_1C3525DD(void);
extern s32 sceHprm_driver_370DDC12(void);
extern s32 sceHprm_driver_9BDE174B(void);
extern s32 sceHprm_driver_AD158331(void);
extern s32 sceHprm_driver_EFCFD0C5(void);
extern s32 sceHprm_driver_FA4A25A7(void);

/* sceKermitPeripheral_driver */
extern s32 sceKermitPeripheral_driver_76BE2E28(s32 speaker_mode, s32 headphone_mode, s32 vol_mode);

/* sceLcdc_driver */
extern s32 sceLcdc_driver_451FE1A1(s32 aspect_mode);

/* sceRtc_driver */
extern s32 sceRtc_driver_9012B140(void *date_time);
extern s32 sceRtc_driver_DFF30673(s32 date_fmt, s32 time_fmt, s32 tz_offset, s32 dst_minutes);

/* sceReg_driver */
typedef struct SceRegParam {
    u32 regtype;
    char name[256];
    u32 namelen;
    u32 open_mode;
    u32 sys_flag;
} SceRegParam;

extern s32 sceRegOpenRegistry(SceRegParam *reg, s32 mode, s32 *reg_handle);
extern s32 sceRegCloseRegistry(s32 reg_handle);
extern s32 sceRegFlushRegistry(s32 reg_handle);
extern s32 sceRegOpenCategory(s32 reg_handle, const char *name, s32 mode, s32 *cat_handle);
extern s32 sceRegCloseCategory(s32 cat_handle);
extern s32 sceRegFlushCategory(s32 cat_handle);
extern s32 sceRegGetKeyInfo(s32 cat_handle, const char *name, s32 *key_handle, u32 *key_type, u32 *key_size);
extern s32 sceRegGetKeyValue(s32 cat_handle, s32 key_handle, void *buf, u32 size);
extern s32 sceRegSetKeyValue(s32 cat_handle, const char *name, const void *buf, u32 size);

/* SysclibForKernel */
extern void *memcpy(void *dst, const void *src, u32 len);
extern void *memset(void *dst, s32 ch, u32 len);
extern char *strncpy(char *dst, const char *src, u32 len);

/* =========================================================================
 * Types & Globals
 * ========================================================================= */

typedef struct SceImposeTvOutMode {
    s32 lcd_mode;
    s32 tv_mode;
    s32 width;
    s32 height;
} SceImposeTvOutMode;

typedef struct SceImposeDateTime {
    u16 year;
    u16 month;
    u16 day;
    u16 hour;
    u16 minute;
    u16 second;
    u32 microsecond;
} SceImposeDateTime;

typedef struct SceImposeRect {
    s32 x;
    s32 y;
    s32 width;
    s32 height;
} SceImposeRect;

typedef struct SceImposeRscHeader {
    s32 palette_count;
    s32 lang_count;
    s32 icon_count;
    u32 offsets[1];
} SceImposeRscHeader;

typedef struct SceImposeLangSpriteDesc {
    s32 palette_index;
    s32 flags;
    s32 x;
    s32 y;
    s32 width;
    s32 height;
    s32 data_offset;
} SceImposeLangSpriteDesc;

typedef struct SceImposeLangHeader {
    s32 sprite_count;
    s32 cross_x;
    s32 cross_y;
    s32 circle_x;
    s32 circle_y;
    u32 reserved[12];
    SceImposeLangSpriteDesc sprites[1];
} SceImposeLangHeader;

typedef struct SceImposeSysEventBootInfo {
    u32 reserved[19];
    s32 language;
} SceImposeSysEventBootInfo;

typedef struct SceImposeSysEventParam {
    u32 reserved[5];
    SceImposeSysEventBootInfo *boot_info;
} SceImposeSysEventParam;

typedef struct SceImposeRebootStatus {
    u32 reserved[4];
    s32 status;
} SceImposeRebootStatus;

typedef struct SceImposeRebootParam {
    u32 reserved[5];
    SceImposeRebootStatus *status_block;
} SceImposeRebootParam;

typedef struct SceImposeState {
    s32 thread_partition_id;        /* 0x000 (0) */
    s32 fpl_partition_id;           /* 0x004 (1) */
    SceUID event_flag_id;           /* 0x008 (2) */
    SceUID main_thread_id;          /* 0x00C (3) */
    SceUID fb_fpl_id;               /* 0x010 (4) */
    void *fb_buf_cached;            /* 0x014 (5) */
    s32 fb_stride;                  /* 0x018 (6) */
    s32 fb_height;                  /* 0x01C (7) */
    s32 language;                   /* 0x020 (8) */
    s32 button_assign;              /* 0x024 (9) */
    s32 main_volume;                /* 0x028 (10) */
    s32 mute;                       /* 0x02C (11) */
    s32 hold_switch_state;          /* 0x030 (12) */
    s32 avls;                       /* 0x034 (13) */
    s32 equalizer_mode;             /* 0x038 (14) */
    s32 cur_brightness;             /* 0x03C (15) */
    s32 battery_percent;            /* 0x040 (16) */
    s32 battery_low;                /* 0x044 (17) */
    s32 time_format;                /* 0x048 (18) */
    s32 date_format;                /* 0x04C (19) */
    s32 headphone_connected;        /* 0x050 (20) */
    s32 audio_routing_mode;         /* 0x054 (21) */
    s32 ac_online;                  /* 0x058 (22) */
    s32 battery_charging;           /* 0x05C (23) */
    s32 battery_exist;              /* 0x060 (24) */
    s32 suspend_interval;           /* 0x064 (25) */
    s32 backlight_off_interval;     /* 0x068 (26) */
    s32 screensaver_start_time;     /* 0x06C (27) */
    s32 wlan_idle_timeout;          /* 0x070 (28) */
    u32 popup_disable_flags;        /* 0x074 (29) */
    s32 sound_reduction;            /* 0x078 (30) */
    s32 volume_mode;                /* 0x07C (31) */
    s32 effective_volume;           /* 0x080 (32) */
    s32 audio_routing_changed;      /* 0x084 (33) */
    u32 home_button_mode;           /* 0x088 (34) */
    s32 home_button_pressed;        /* 0x08C (35) */
    s32 equalizer_enabled;          /* 0x090 (36) */
    s32 aspect_ratio;               /* 0x094 (37) */
    s32 scan_mode;                  /* 0x098 (38) */
    s32 tvout_cable_type;           /* 0x09C (39) */
    s32 oem_charset;                /* 0x0A0 (40) */
    u32 menu_state;                 /* 0x0A4 (41) */
    s32 dialog_selection;           /* 0x0A8 (42) */
    u32 cur_buttons;                /* 0x0AC (43) */
    u32 cur_hprm_key;               /* 0x0B0 (44) */
    s32 brightness_level;           /* 0x0B4 (45) */
    u32 osd_dirty_flags;            /* 0x0B8 (46) */
    u32 pending_change_flags;       /* 0x0BC (47) */
    s32 suppress_notify;            /* 0x0C0 (48) */
    u32 special_btn_pending;        /* 0x0C4 (49) */
    s32 anim_phase;                 /* 0x0C8 (50) */
    s32 avls_blink_counter;         /* 0x0CC (51) */
    s32 clock_left_x;               /* 0x0D0 (52) */
    s32 brightness_dimmed;          /* 0x0D4 (53) */
    s32 brightness_cap_pending;     /* 0x0D8 (54) */
    s32 brightness_max_limit;       /* 0x0DC (55) */
    s32 reg_flush_pending;          /* 0x0E0 (56) */
    s32 suspend_active;             /* 0x0E4 (57) */
    s32 display_off_state;          /* 0x0E8 (58) */
    u32 deferred_event_bits;        /* 0x0EC (59) */
    s32 deferred_event_delay;       /* 0x0F0 (60) */
    s32 dialog_wait_release;        /* 0x0F4 (61) */
    s32 screen_hold_state;          /* 0x0F8 (62) */
    s32 tvout_switch_from_off;      /* 0x0FC (63) */
    s32 default_language;           /* 0x100 (64) */
    s32 resource_loading;           /* 0x104 (65) */
    s32 hardware_model;             /* 0x108 (66) */
    s32 app_type;                   /* 0x10C (67) */
    u32 display_caps;               /* 0x110 (68) */
    u32 hprm_caps;                  /* 0x114 (69) */
    s32 headphone_audio_mode;       /* 0x118 (70) */
    s32 disc_image_active;          /* 0x11C (71) */
    s32 tvout_suspend_done;         /* 0x120 (72) */
    s32 hprm_status;                /* 0x124 (73) */
    s32 hprm_headphone_exist;       /* 0x128 (74) */
    s32 hprm_remote_exist;          /* 0x12C (75) */
    s32 volume_target;              /* 0x130 (76) */
    s32 init_state;                 /* 0x134 (77) */
    s32 pi_blending_mode;           /* 0x138 (78) */
    s32 suspend_cleanup_mark;       /* 0x13C (79) */
    s32 hprm_hp_present;            /* 0x140 (80) */
    s32 hprm_mic_present;           /* 0x144 (81) */
    s32 yes_btn_anim_reset;         /* 0x148 (82) */
    s32 no_btn_anim_reset;          /* 0x14C (83) */
    const void *cur_yes_sprite;     /* 0x150 (84) */
    const void *cur_no_sprite;      /* 0x154 (85) */
    s32 dialog_y_offset;            /* 0x158 (86) */
    s32 battery_anim_frame;         /* 0x15C (87) */
    s32 osd_frame_counter;          /* 0x160 (88) */
    s32 blink_frame_counter;        /* 0x164 (89) */
    s32 anim_step_counter;          /* 0x168 (90) */
    s32 deferred_event_ticks;       /* 0x16C (91) */
    s32 vol_hold_ticks;             /* 0x170 (92) */
    s32 screen_hold_ticks;          /* 0x174 (93) */
    s32 note_hold_ticks;            /* 0x178 (94) */
    u32 suspend_request_time;       /* 0x17C (95) */
    u32 display_dim_time;           /* 0x180 (96) */
    u32 popup_exit_time;            /* 0x184 (97) */
    s32 osd_visible;                /* 0x188 (98) */
    u32 reserved_rtc[2];            /* 0x18C (99..100) */
    SceImposeDateTime rtc_time;     /* 0x194 (101..104) */
    s32 intercept_level;            /* 0x1A4 (105) */
    u32 saved_intercept_mask;       /* 0x1A8 (106) */
    u32 saved_home_intercept_mask;  /* 0x1AC (107) */
    u16 *fb_buf_uncached;           /* 0x1B0 (108) */
    void (*overlay_handlers[4])(void); /* 0x1B4 (109..112) */
    SceUID text_fpl_id;             /* 0x1C4 (113) */
    void *text_fpl_buf;             /* 0x1C8 (114) */
    u16 *text_fpl_cur;              /* 0x1CC (115) */
    s32 text_fpl_size;              /* 0x1D0 (116) */
    void *text_sprites[10];         /* 0x1D4 (117..126) */
    SceImposeRect text_rects[10];   /* 0x1FC (127..166) */
    s32 cross_icon_x;               /* 0x29C (167) */
    s32 cross_icon_y;               /* 0x2A0 (168) */
    s32 circle_icon_x;              /* 0x2A4 (169) */
    s32 circle_icon_y;              /* 0x2A8 (170) */
    SceUID icon_fpl_id;             /* 0x2AC (171) */
    void *icon_fpl_buf;             /* 0x2B0 (172) */
    u8 *icon_fpl_cur;               /* 0x2B4 (173) */
    s32 icon_fpl_size;              /* 0x2B8 (174) */
    u8 *icon_sprites[21];           /* 0x2BC (175..195) */
    s32 lang_change_pending;        /* 0x310 (196) */
    s32 pending_language;           /* 0x314 (197) */
    s32 pending_button_assign;      /* 0x318 (198) */
    s32 tvout_active;               /* 0x31C (199) */
    s32 tvout_allowed;              /* 0x320 (200) */
    s32 tvout_blocked;              /* 0x324 (201) */
    s32 initial_display_mode;       /* 0x328 (202) */
    s32 display_mode;               /* 0x32C (203) */
    s32 display_width;              /* 0x330 (204) */
    s32 display_height;             /* 0x334 (205) */
    s32 volume_disabled;            /* 0x338 (206) */
    s32 tvout_keep_on_reset;        /* 0x33C (207) */
    s32 popup_timer_active;         /* 0x340 (208) */
    s32 popup_timer_phase;          /* 0x344 (209) */
    s32 popup_banner_mode;          /* 0x348 (210) */
    u32 popup_expire_lo;            /* 0x34C (211) */
    u32 popup_expire_hi;            /* 0x350 (212) */
    u32 popup_warn_lo;              /* 0x354 (213) */
    u32 popup_warn_hi;              /* 0x358 (214) */
    SceImposeDateTime popup_expire_time; /* 0x35C (215..218) */
} SceImposeState;

typedef char impose_state_size_check[(sizeof(SceImposeState) == 0x36C) ? 1 : -1];

enum {
    IMPOSE_ICON_BATTERY         = 0,
    IMPOSE_ICON_BLINK_CURSOR    = 1,
    IMPOSE_ICON_CROSS           = 2,
    IMPOSE_ICON_CIRCLE          = 3,
    IMPOSE_ICON_HOLD_EVEN       = 4,
    IMPOSE_ICON_HOLD_ODD        = 5,
    IMPOSE_ICON_MUTE_SM_EVEN    = 6,
    IMPOSE_ICON_MUTE_SM_ODD     = 7,
    IMPOSE_ICON_BORDER_LINE     = 8,
    IMPOSE_ICON_TVOUT           = 9,
    IMPOSE_ICON_DIGIT_EVEN      = 10,
    IMPOSE_ICON_DIGIT_ODD       = 11,
    IMPOSE_ICON_PUNCT_EVEN      = 12,
    IMPOSE_ICON_PUNCT_ODD       = 13,
    IMPOSE_ICON_AMPM_EVEN       = 14,
    IMPOSE_ICON_AMPM_ODD        = 15,
    IMPOSE_ICON_VOL_BAR         = 16,
    IMPOSE_ICON_AVLS            = 17,
    IMPOSE_ICON_SPEAKER         = 18,
    IMPOSE_ICON_STATUS_BANNER   = 19
};

enum {
    IMPOSE_TEXT_QUESTION        = 0,
    IMPOSE_TEXT_ENTER           = 1,
    IMPOSE_TEXT_BACK            = 2,
    IMPOSE_TEXT_PLEASE_WAIT     = 3,
    IMPOSE_TEXT_YES_NORMAL      = 4,
    IMPOSE_TEXT_YES_HIGHLIGHT   = 5,
    IMPOSE_TEXT_YES_MID         = 6,
    IMPOSE_TEXT_NO_NORMAL       = 7,
    IMPOSE_TEXT_NO_HIGHLIGHT    = 8,
    IMPOSE_TEXT_NO_MID          = 9
};

#define IMPOSE_CLEAR_COLOR_WORD ((s32)0x9CE79CE7u)
#define IMPOSE_LINE_COLOR_WORD  ((s32)0xCE73CE73u)

static const s32 g_battery_thresholds[3] = { 60, 30, 12 };

static const SceImposeTvOutMode g_tvout_modes_ntsc[3] = {
    { 0x00, 0x2D2, 480, 272 },
    { 0x00, 0x3D2, 480, 272 },
    { 0xE0, 0x2E2, 480, 272 },
};

static const SceImposeTvOutMode g_tvout_modes_pal[2] = {
    { 0x00, 0x6D1, 480, 272 },
    { 0x00, 0x7D1, 480, 272 },
};

static const s32 g_brightness_levels[5] = { 24, 40, 72, 100, 0 };
static const s32 g_step_brightness_levels[5] = { 24, 40, 72, 100, 0 };

static SceImposeState g_impose_state __attribute__((aligned(16)));
SceImposeState *g_impose = &g_impose_state;
const SceImposeTvOutMode *g_tvout_mode = NULL;

/* Forward declarations */
static void imposeNotifyBrightnessEvent(s32 unused_ev, s32 disable_display);
static s32 imposeSetEventFlag(u32 bits);
static s32 imposeBrightnessToLevel(s32 val);
static s32 imposeLevelToBrightness(u32 idx);
static s32 imposeBrightnessToBacklightSel(s32 val);
static s32 imposeGetBacklightInfo(s32 *bright_out);
static s32 imposeStepBrightness(s32 mode);
static s32 imposeHandleDialogButtons(s32 trig, u32 cur_buttons, u32 prev_buttons);
static s32 imposeGetHprmStatus(void);
static void imposeSetVolumeTarget(s32 target);
static s32 imposeRequestVolumeUpdate(s32 target);
static void imposeClearOsdRegion(u32 mask);
static s32 imposeDismissActiveOverlay(void);
static s32 imposeLoadRegistrySettings(void);
static s32 imposeCloseOverlayByType(s32 open_flag);
static s32 imposeIsMenuAllowed(void);
static s32 imposeRestoreDisplayMode(void);
static s32 imposeSetTvOutMode(s32 immediate, s32 mode, s32 width, s32 height);
static void imposeResetTvOutState(void);
static s32 imposeSelectTvOutProfile(s32 profile_idx);
static int impose_sysevent_handler(int ev_id, char *ev_name, void *param, int *result);
static s32 imposeFreeTextResource(void);
static void imposeLoadLanguageText(void);
static s32 imposeFreeIconResource(void);
static s32 imposeAllocIconResource(void);
static void imposeEnableButtonIntercept(s32 intercept_home);
static void imposeRestoreButtonIntercept(void);
static void imposeHandleHomeButton(u32 cur_buttons, u32 last_buttons, u32 arg);
static void imposeHandleHoldButton(u32 cur_buttons, u32 last_buttons, u32 arg);
static void imposeNoopCallback(s32 kind, s32 enable);
static s32 imposeCtrlSubIntrHandler(s32 subintr_no, void *arg);
static void imposeHideOsdFrameBuf(void);
static s32 imposeMainThread(u32 args, void *argp);
static s32 imposeApplyRegistrySetting(u32 id);
static void imposeRedrawOsd(void);
static void imposeDrawBlinkCursor(void);
static void imposeDrawVolumeBar(void);
static void imposeShowBatteryIcon(void);
static s32 imposeLoadResourceFile(void);
static s32 imposeParseResourceHeader(s32 lang);
static s32 imposeValidateResourceMagic(void);
static void imposeShowOsdPopup(u64 expire_tick, u64 warn_sec);
static void imposeRefreshRegistryValues(s32 mode);
static void imposeSyncAudioVolume(void);
static s32 imposeTriggerPowerPoll(void);
static void imposePollHprmState(void);
static void imposePollHeadphoneState(void);
static void imposeTickPopupTimer(void);
static void imposeBlitIcon16(s32 x, s32 y, s32 width, s32 height, const void *src);
static void imposeBlitIcon16Wrapper(s32 x, s32 y, s32 width, s32 height, const void *src);
static void imposeDrawStatusBanner(s32 y);
static s32 imposeDrawMuteIcon(void);
static s32 imposeDrawClockDisplay(void);
static s32 imposeAdvanceAnimFrame(void);
static s32 imposeWriteRegistryInt(const char *category, const char *key, s32 val);
static void imposeUpdateStatusFlags(s32 set, u32 mask);
static s32 imposeInitAudioCallback(void *opt_in, s32 stage, void *opt_out);
static s32 imposeSetAudioMode(u32 mode);
static s32 imposeCalcEffectiveVolume(s32 mode);
static s32 imposeApplyHeadphoneVolume(void);
static s32 imposeApplySpeakerVolume(void);
static void imposeHandleVolumeButtons(u32 trig, u32 cur_buttons, u32 prev_buttons);
static void imposeClearAllOverlays(void);
static s32 imposeHandlePowerEvent(s32 vol_target, s32 allow_notify);
static s32 imposeRefreshHprmDevices(s32 vol_target, s32 allow_notify);
static s32 imposeUpdateHprmOnEvent(void);
static s32 imposeReadRegistrySetting(u32 id);
static s32 imposeFormatTimeString(const SceImposeDateTime *dt, s32 x, s32 y, s32 align);
static void imposeDrawBatteryLevel(void);
static s32 imposeReadRegistryInt(const char *category, const char *key, s32 *out);
static void imposeUpdateAudioRouting(void);

s32 sceImpose_driver_BB12F974(u32 state);
s32 sceImpose_driver_0F067E16(void);
s32 sceImpose_driver_E9B0E008(s32 *out);
s32 sceImpose_driver_D067235E(void);
s32 sceImpose_driver_AABEE36A(void);
s32 sceImpose_133636DD(s32 id);
s32 sceImpose_FCD44963(s32 id, u32 val);
s32 sceImpose_driver_686B040E(void);

static SceSysEventHandler g_sysevent_handler = {
    sizeof(SceSysEventHandler),
    (char *)"SceImpose",
    0x00FFFF00,
    impose_sysevent_handler,
    0,
    0,
    NULL,
    { 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

/* =========================================================================
 * Internal Helper Implementations
 * ========================================================================= */

static void imposeNotifyBrightnessEvent(s32 unused_ev, s32 disable_display)
{
    (void)unused_ev;
    if (disable_display != 0) {
        imposeEnableButtonIntercept(0);
        imposeSetEventFlag(0x2000u);
        g_impose->display_off_state = 1;
        g_impose->pending_change_flags |= 0x20000000u;
    } else if (g_impose->display_off_state != 0) {
        imposeRestoreButtonIntercept();
        g_impose->display_off_state = 0;
        imposeSetEventFlag(0x1000u);
        g_impose->pending_change_flags |= 0x20000000u;
    }
}

static s32 imposeSetEventFlag(u32 bits)
{
    return sceKernelSetEventFlag(g_impose->event_flag_id, bits);
}

static s32 imposeBrightnessToLevel(s32 val)
{
    if ((u32)(val - 73) < 28u) {
        return 3;
    }
    if ((u32)(val - 41) < 32u) {
        return 2;
    }
    if ((u32)(val - 25) < 16u) {
        return 1;
    }
    if ((u32)(val - 1) < 24u) {
        return 0;
    }
    return 4;
}

static s32 imposeLevelToBrightness(u32 idx)
{
    if (idx < 5u) {
        return g_brightness_levels[idx];
    }
    return -1;
}

static s32 imposeBrightnessToBacklightSel(s32 val)
{
    s32 sel;

    if ((u32)(val - 73) < 28u) {
        sel = 4;
    } else if ((u32)(val - 41) < 32u) {
        sel = 3;
    } else if ((u32)(val - 25) < 16u) {
        sel = 2;
    } else if ((u32)(val - 1) < 24u) {
        sel = 1;
    } else {
        sel = 0;
    }
    sceDisplaySetBacklightSel(sel);
    return 0;
}

static s32 imposeGetBacklightInfo(s32 *bright_out)
{
    s32 sel;

    sceDisplayGetBacklightSel(&sel);
    switch (sel) {
    case 0:
        *bright_out = 0;
        break;
    case 1:
        *bright_out = 24;
        break;
    case 2:
        *bright_out = 40;
        break;
    case 3:
        *bright_out = 72;
        break;
    case 4:
        *bright_out = 100;
        break;
    default:
        *bright_out = 0;
        break;
    }
    return 0;
}

static s32 imposeStepBrightness(s32 mode)
{
    if (mode == 0) {
        s32 max_lvl = imposeBrightnessToLevel(g_impose->brightness_max_limit);
        if (g_impose->brightness_dimmed == 0) {
            s32 next = g_impose->brightness_level + 1;
            g_impose->brightness_level = next;
            if (next >= 4) {
                g_impose->brightness_level = 0;
            }
            if (max_lvl < g_impose->brightness_level) {
                g_impose->brightness_level = 0;
            }
        }
        g_impose->brightness_dimmed = 0;
        g_impose->cur_brightness = g_step_brightness_levels[g_impose->brightness_level];
        if (g_impose->brightness_cap_pending != 0) {
            if (g_impose->brightness_max_limit < g_impose->cur_brightness) {
                g_impose->cur_brightness = g_impose->brightness_max_limit;
            }
            g_impose->brightness_cap_pending = 0;
        }
        if (g_impose->display_off_state != 0) {
            g_impose->display_off_state = 0;
            g_impose->display_dim_time = 0;
            imposeSetEventFlag(0x1000u);
        }
    } else if (mode == 1) {
        if (g_impose->cur_brightness >= 25) {
            g_impose->cur_brightness = 24;
        }
        g_impose->brightness_dimmed = 1;
    } else if (mode == 2) {
        g_impose->brightness_dimmed = 1;
        g_impose->display_off_state = 2;
        g_impose->cur_brightness = 0;
        g_impose->display_dim_time = sceKernelGetSystemTimeLow();
    } else if (mode == 3) {
        g_impose->brightness_dimmed = 1;
        g_impose->display_off_state = 0;
        g_impose->cur_brightness = 0;
    } else {
        return -1;
    }

    imposeBrightnessToBacklightSel(g_impose->cur_brightness);
    g_impose->pending_change_flags |= 2u;
    return 0;
}

static s32 imposeHandleDialogButtons(s32 trig, u32 cur_buttons, u32 prev_buttons)
{
    if ((s32)g_impose->menu_state < 0 || g_impose->anim_phase < 4) {
        return 0;
    }

    if (g_impose->dialog_wait_release != 0) {
        if ((cur_buttons & 0xFFFFu) == 0) {
            g_impose->dialog_wait_release = 0;
            prev_buttons = 0;
            imposeSetEventFlag(0x100u);
        }
    }

    if (g_impose->dialog_wait_release != 0) {
        return 0;
    }

    u32 confirm_btn = (g_impose->button_assign != 0) ? 0x4000u : 0x2000u;
    u32 cancel_btn  = (g_impose->button_assign != 0) ? 0x2000u : 0x4000u;

    if ((s8)trig < 0) {
        g_impose->dialog_selection = 1;
        g_impose->osd_dirty_flags &= ~0x300u;
        return 0;
    }
    if ((trig & 0x20) != 0) {
        g_impose->dialog_selection = 0;
        g_impose->osd_dirty_flags &= ~0x300u;
        return 0;
    }
    if ((cur_buttons & (confirm_btn | cancel_btn)) == 0 && (prev_buttons & cancel_btn) != 0) {
        sceImpose_driver_BB12F974(1u);
        return 0;
    }
    if ((cur_buttons & (confirm_btn | cancel_btn)) == 0 && (prev_buttons & confirm_btn) != 0) {
        if (g_impose->dialog_selection != 1) {
            sceImpose_driver_BB12F974(1u);
            return 0;
        }
        if (sceKernelCheckExitCallback() != 0) {
            g_impose->menu_state |= 0x80000000u;
            imposeSetEventFlag(8u);
        }
    }
    return 0;
}

static s32 imposeGetHprmStatus(void)
{
    s32 status;
    s32 res = sceHprm_driver_0B83352B(0, &status, 0, 0);
    if (res < 0) {
        return -1;
    }
    return status;
}

static void imposeSetVolumeTarget(s32 target)
{
    g_impose->volume_target = target;
}

static s32 imposeRequestVolumeUpdate(s32 target)
{
    imposeSetVolumeTarget(target);
    return imposeSetEventFlag(0x8000u);
}

static void imposeClearOsdRegion(u32 mask)
{
    s32 row;

    if ((mask & 0x01u) != 0) {
        u16 *dst = g_impose->fb_buf_uncached + (5 * g_impose->fb_stride + 250);
        for (row = 16; row >= 0; row--) {
            sceKernelMemset32(dst, IMPOSE_CLEAR_COLOR_WORD, 456u);
            dst += (g_impose->fb_stride / 2) * 2;
        }
    }
    if ((mask & 0x02u) != 0) {
        u16 *dst = g_impose->fb_buf_uncached + ((g_impose->dialog_y_offset + 96) * g_impose->fb_stride + 80);
        for (row = 87; row >= 0; row--) {
            sceKernelMemset32(dst, IMPOSE_CLEAR_COLOR_WORD, 656u);
            dst += (g_impose->fb_stride / 2) * 2;
        }
    }
    if ((mask & 0x04u) != 0) {
        u16 *dst = g_impose->fb_buf_uncached + (222 * g_impose->fb_stride + 12);
        for (row = 28; row >= 0; row--) {
            sceKernelMemset32(dst, IMPOSE_CLEAR_COLOR_WORD, 936u);
            dst += (g_impose->fb_stride / 2) * 2;
        }
    }
    if ((mask & 0x08u) != 0) {
        u16 *dst = g_impose->fb_buf_uncached + (253 * g_impose->fb_stride + 172);
        for (row = 17; row >= 0; row--) {
            sceKernelMemset32(dst, IMPOSE_CLEAR_COLOR_WORD, 400u);
            dst += (g_impose->fb_stride / 2) * 2;
        }
    }
    if ((mask & 0x20u) != 0) {
        u16 *dst = g_impose->fb_buf_uncached + (185 * g_impose->fb_stride + 156);
        for (row = 19; row >= 0; row--) {
            sceKernelMemset32(dst, IMPOSE_CLEAR_COLOR_WORD, 344u);
            dst += (g_impose->fb_stride / 2) * 2;
        }
    }
}

static s32 imposeDismissActiveOverlay(void)
{
    s32 idx = -1;

    if (g_impose->resource_loading != 0) {
        return -1;
    }
    u32 state = g_impose->menu_state;
    if ((state & 2u) == 0) {
        return (s32)0x80000105;
    }
    if ((state & 0x100u) != 0) {
        idx = 0;
    } else {
        if ((g_impose->menu_state & 0x800u) != 0) {
            idx = 3;
        }
        if ((g_impose->menu_state & 0x400u) != 0) {
            idx = 2;
        }
        if ((g_impose->menu_state & 0x200u) != 0) {
            idx = 1;
        }
    }
    if (idx == -1) {
        return -1;
    }
    g_impose->overlay_handlers[idx]();
    return 0;
}

static s32 imposeLoadRegistrySettings(void)
{
    imposeApplyRegistrySetting(7u);
    imposeApplyRegistrySetting(6u);
    imposeApplyRegistrySetting(3u);
    imposeApplyRegistrySetting(4u);
    if (g_impose->display_caps != 0) {
        imposeApplyRegistrySetting(16u);
        imposeApplyRegistrySetting(17u);
        imposeApplyRegistrySetting(20u);
    }
    return 0;
}

static s32 imposeCloseOverlayByType(s32 open_flag)
{
    u32 state = g_impose->menu_state;

    if ((state & 4u) != 0 || g_impose->init_state != 0 || (s32)state < 0) {
        return -1;
    }
    if (open_flag == 0) {
        if ((state & 0x100u) != 0) {
            sceImpose_driver_BB12F974(1u);
        }
        return 0;
    }
    if ((state & 0x100u) != 0) {
        return 0;
    }
    if (sceKernelCheckExitCallback() == 0) {
        return -1;
    }
    sceImpose_driver_BB12F974(0x50102u);
    return 0;
}

static s32 imposeIsMenuAllowed(void)
{
    if (g_impose->app_type == 0x300) {
        return 1;
    }
    if ((g_impose->menu_state & 4u) != 0) {
        return (g_impose->app_type != 0x400) ? 1 : 0;
    }
    return 0;
}

static s32 imposeRestoreDisplayMode(void)
{
    s32 cable = g_impose->tvout_cable_type;
    if (cable == 1) {
        sceDisplay_driver_D94EB277(2);
    } else if (cable == 2) {
        sceDisplay_driver_D94EB277(0);
    }
    return sceDisplaySetMode(g_impose->display_mode, g_impose->display_width, g_impose->display_height);
}

static s32 imposeSetTvOutMode(s32 immediate, s32 mode, s32 width, s32 height)
{
    s32 prev_mode = g_impose->display_mode;

    g_impose->display_width = width;
    g_impose->display_height = height;
    g_impose->display_mode = mode;

    if (mode != 0 && mode != 0xE0) {
        imposeStepBrightness(3);
        imposeNoopCallback(2, 0);
        imposeNoopCallback(4, 1);
        g_impose->tvout_active = 1;
        imposeSetVolumeTarget(2);
        if ((g_impose->display_caps & 0x01000000u) != 0 && g_impose->tvout_cable_type == 0) {
            if (g_impose->init_state != 2) {
                imposeSetEventFlag(0x4000u);
            }
            return 0;
        }
        if (immediate != 0) {
            imposeRestoreDisplayMode();
        } else {
            imposeSetEventFlag(0x800u);
        }
        return 0;
    }

    s32 was_tvout = g_impose->tvout_active;
    if (was_tvout != 0 || prev_mode != mode) {
        if (was_tvout != 0) {
            imposeStepBrightness(0);
        }
        imposeNoopCallback(4, 0);
        imposeNoopCallback(2, 1);
        g_impose->tvout_active = 0;
        imposeSetVolumeTarget(1);
        if (immediate != 0) {
            imposeRestoreDisplayMode();
        } else {
            imposeSetEventFlag(0x800u);
        }
    }
    return 0;
}

static void imposeResetTvOutState(void)
{
    if (g_impose->tvout_active != 0) {
        if (g_impose->display_off_state != 0) {
            imposeNotifyBrightnessEvent(0, 0);
        }
        if (!imposeIsMenuAllowed() || g_impose->tvout_keep_on_reset == 0) {
            imposeSetTvOutMode(0, g_tvout_mode->lcd_mode, g_tvout_mode->width, g_tvout_mode->height);
        }
        g_impose->tvout_active = 0;
        g_impose->pending_change_flags |= 0x200u;
    }
}

static s32 imposeSelectTvOutProfile(s32 profile_idx)
{
    s32 app = g_impose->app_type;
    const SceImposeTvOutMode *base;

    if (app == 0x300) {
        if (profile_idx == 0 || profile_idx == 2) {
            g_tvout_mode = &g_tvout_modes_ntsc[profile_idx];
            return 0;
        }
        return (s32)0x80000107;
    }
    if (app == 0x400) {
        if (profile_idx != 1) {
            return (s32)0x80000107;
        }
        base = (g_impose->tvout_cable_type == 1 || g_impose->scan_mode != 0)
             ? g_tvout_modes_pal
             : g_tvout_modes_ntsc;
        g_tvout_mode = &base[1];
        return 0;
    }
    if (profile_idx != 0) {
        return (s32)0x80000107;
    }
    base = (g_impose->tvout_cable_type == 1 || g_impose->scan_mode != 0)
         ? g_tvout_modes_pal
         : g_tvout_modes_ntsc;
    g_tvout_mode = &base[0];
    return 0;
}

static int impose_sysevent_handler(int ev_id, char *ev_name, void *param, int *result)
{
    (void)ev_name;
    (void)result;

    if (ev_id == 0x200) {
        g_impose->init_state = 1;
        if (g_impose->suspend_active == 0) {
            sceDisplayDisable();
            if ((s32)g_impose->menu_state >= 0) {
                g_impose->reg_flush_pending = 1;
                imposeSetEventFlag(8u);
                do {
                    sceKernelDelayThread(100000);
                } while (g_impose->reg_flush_pending != 0);
            }
        }
        return 0;
    }
    if (ev_id == 0x400) {
        sceImpose_driver_686B040E();
        g_impose->tvout_suspend_done = 0;
        ((SceImposeSysEventParam *)param)->boot_info->language = g_impose->pending_language;
        return 0;
    }
    if (ev_id == 0x402) {
        if (g_impose->suspend_active == 0) {
            if (g_impose->tvout_suspend_done == 0) {
                imposeResetTvOutState();
                g_impose->tvout_suspend_done = 1;
            }
            g_impose->suspend_cleanup_mark = 0;
            sceKernelDisableSubIntr(0x1E, 0x14);
            return 0;
        }
        s32 elapsed = (s32)(sceKernelGetSystemTimeLow() - g_impose->suspend_request_time);
        if ((u32)(elapsed - 3000001) <= 0x1E847Eu) {
            if (g_impose->tvout_suspend_done == 0) {
                imposeResetTvOutState();
                g_impose->tvout_suspend_done = 1;
            }
            return -1;
        }
        if (elapsed <= 4999999) {
            return -1;
        }
        sceKernelDisableSubIntr(0x1E, 0x14);
        return 0;
    }
    if (ev_id == 0x400E) {
        u32 state = g_impose->menu_state;
        if ((state & 2u) != 0 && (s32)state >= 0) {
            g_impose->menu_state = 1u;
            imposeHideOsdFrameBuf();
        }
        return 0;
    }
    if (ev_id == 0x1000E) {
        g_impose->suppress_notify = 1;
        g_impose->init_state = 2;
        g_impose->suspend_active = 0;
        g_impose->pending_change_flags = 0;
        sceDisplayEnable();
        s32 was_dimmed = g_impose->brightness_dimmed;
        g_impose->display_off_state = 0;
        g_impose->special_btn_pending = 0;
        g_impose->avls_blink_counter = 0;
        if (was_dimmed != 0) {
            imposeRestoreButtonIntercept();
            imposeStepBrightness(0);
            imposeNoopCallback(2, 1);
            g_impose->screen_hold_state = 0;
            g_impose->pending_change_flags |= 0x20000000u;
        }
        imposeSetVolumeTarget(1);
        g_impose->wlan_idle_timeout = 0;
        imposeNoopCallback(8, 0);
        sceKernelEnableSubIntr(0x1E, 0x14);
        g_impose->popup_timer_phase = 2;
        return 0;
    }
    if (ev_id == 0x100000) {
        if (g_impose->fb_buf_cached != NULL) {
            sceKernelMemset32(g_impose->fb_buf_uncached, IMPOSE_CLEAR_COLOR_WORD,
                              (u32)(2 * g_impose->fb_stride * g_impose->fb_height));
        }
        if (g_impose->display_caps != 0) {
            s32 disp_mode;
            s32 disp_width;
            s32 disp_height;
            sceDisplayGetMode(&disp_mode, &disp_width, &disp_height);
            g_impose->initial_display_mode = disp_mode;
        }
        g_impose->hprm_hp_present = sceHprm_driver_370DDC12();
        if ((g_impose->hprm_caps & 2u) != 0) {
            g_impose->hprm_mic_present = sceHprm_driver_1C3525DD();
        }
        g_impose->headphone_connected = g_impose->hprm_hp_present | g_impose->hprm_mic_present;
        g_impose->hprm_headphone_exist = sceHprm_driver_FA4A25A7();
        g_impose->hprm_remote_exist = sceHprm_driver_EFCFD0C5();
        g_impose->hprm_status = imposeGetHprmStatus();
        if (g_impose->display_caps != 0) {
            s32 profile = (g_impose->app_type == 0x400) ? 1 : 0;
            imposeSelectTvOutProfile(profile);
            imposeSyncAudioVolume();
        }
        g_impose->deferred_event_bits = 0x4064u;
        g_impose->suppress_notify = 1;
        g_impose->init_state = 0;
        g_impose->deferred_event_delay = 1;
        g_impose->deferred_event_ticks = 0;
    }
    return 0;
}

static s32 imposeFreeTextResource(void)
{
    if (g_impose->text_fpl_buf == NULL) {
        return 0;
    }
    if (sceKernelFreeFpl(g_impose->text_fpl_id, g_impose->text_fpl_buf) < 0) {
        return (s32)0x8001000C;
    }
    if (sceKernelDeleteFpl(g_impose->text_fpl_id) < 0) {
        return (s32)0x8001000C;
    }
    g_impose->text_fpl_size = 0;
    g_impose->text_fpl_id = 0;
    g_impose->text_fpl_buf = NULL;
    g_impose->text_fpl_cur = NULL;
    return 0;
}

static void imposeLoadLanguageText(void)
{
    if (g_impose->lang_change_pending == 0) {
        return;
    }
    if (g_impose->language != g_impose->pending_language) {
        s32 sz = imposeLoadResourceFile();
        if (sz < 0) {
            return;
        }
        if (imposeParseResourceHeader(g_impose->pending_language) < 0) {
            return;
        }
        s32 words = sz / 4;
        if (words > 0) {
            u32 *dst = (u32 *)g_impose->fb_buf_cached;
            do {
                *dst++ = (u32)IMPOSE_CLEAR_COLOR_WORD;
                words--;
            } while (words != 0);
        }
        sceKernelDcacheWritebackInvalidateRange(g_impose->fb_buf_cached, (u32)sz);
    }
    g_impose->lang_change_pending = 0;
    g_impose->button_assign = g_impose->pending_button_assign;
    g_impose->language = g_impose->pending_language;
}

static s32 imposeFreeIconResource(void)
{
    if (g_impose->icon_fpl_buf == NULL) {
        return 0;
    }
    if (sceKernelFreeFpl(g_impose->icon_fpl_id, g_impose->icon_fpl_buf) < 0) {
        return (s32)0x8001000C;
    }
    if (sceKernelDeleteFpl(g_impose->icon_fpl_id) < 0) {
        return (s32)0x8001000C;
    }
    g_impose->icon_fpl_size = 0;
    g_impose->icon_fpl_id = 0;
    g_impose->icon_fpl_buf = NULL;
    g_impose->icon_fpl_cur = NULL;
    return 0;
}

static s32 imposeAllocIconResource(void)
{
    s32 fpl = sceKernelCreateFpl("SceImpose_IconResource", g_impose->fpl_partition_id, 0x4100u, 0xB400u, 1, NULL);
    if (fpl < 0) {
        return (s32)0x8001000C;
    }
    g_impose->icon_fpl_size = 0xB400;
    g_impose->icon_fpl_id = fpl;
    sceKernelTryAllocateFpl(fpl, &g_impose->icon_fpl_buf);
    g_impose->icon_fpl_cur = (u8 *)g_impose->icon_fpl_buf;

    fpl = sceKernelCreateFpl("SceImpose_Resource", g_impose->fpl_partition_id, 0x4100u, 0x6800u, 1, NULL);
    if (fpl < 0) {
        return (s32)0x8001000C;
    }
    g_impose->text_fpl_size = 0x6800;
    g_impose->text_fpl_id = fpl;
    sceKernelTryAllocateFpl(fpl, &g_impose->text_fpl_buf);
    g_impose->text_fpl_cur = (u16 *)g_impose->text_fpl_buf;

    s32 sz = imposeLoadResourceFile();
    if (sz < 0) {
        return sz;
    }
    s32 res = imposeValidateResourceMagic();
    if (res < 0) {
        return res;
    }
    res = imposeParseResourceHeader(g_impose->language);
    if (res < 0) {
        return res;
    }
    sceKernelDcacheWritebackInvalidateRange(g_impose->fb_buf_cached, (u32)sz);
    return 0;
}

static void imposeEnableButtonIntercept(s32 intercept_home)
{
    s32 intr = sceKernelCpuSuspendIntr();
    s32 cur_level = g_impose->intercept_level;

    if (cur_level == 0) {
        g_impose->saved_home_intercept_mask = 0;
        g_impose->saved_intercept_mask = 0;
        s32 bit;
        for (bit = 0; bit < 32; bit++) {
            u32 mask = 1u << bit;
            s32 mode = sceCtrlGetButtonIntercept(mask);
            if (mode == 1) {
                g_impose->saved_intercept_mask |= mask;
            } else if (mode == 2) {
                g_impose->saved_home_intercept_mask |= mask;
            }
        }
        sceCtrlSetButtonIntercept(0xFFFFu, 1);
        if (intercept_home != 0) {
            sceCtrlSetButtonIntercept(0x10000u, 2);
            g_impose->intercept_level = 2;
        } else {
            g_impose->intercept_level = 1;
        }
    } else if (cur_level == 1 && intercept_home != 0) {
        sceCtrlSetButtonIntercept(0x10000u, 2);
        g_impose->intercept_level = 2;
    }
    sceKernelCpuResumeIntr(intr);
}

static void imposeRestoreButtonIntercept(void)
{
    s32 intr = sceKernelCpuSuspendIntr();
    if (g_impose->intercept_level != 0 && (g_impose->menu_state & 2u) == 0) {
        sceCtrlSetButtonIntercept(0x10000u, 0);
        sceCtrlSetButtonIntercept(0xFFFFu, 0);
        sceCtrlSetButtonIntercept(g_impose->saved_intercept_mask, 1);
        sceCtrlSetButtonIntercept(g_impose->saved_home_intercept_mask, 2);
        g_impose->intercept_level = 0;
    }
    sceKernelCpuResumeIntr(intr);
}

static void imposeHandleHomeButton(u32 cur_buttons, u32 last_buttons, u32 arg)
{
    (void)arg;
    if (g_impose->menu_state == 0) {
        return;
    }
    if ((last_buttons & 0x01000000u) != 0 && (cur_buttons & 0x01000000u) == 0) {
        if ((g_impose->popup_disable_flags & 2u) == 0 && g_impose->disc_image_active == 0) {
            imposeCloseOverlayByType(1);
        }
    }
    if ((cur_buttons & 0x00020000u) != 0) {
        return;
    }
    if ((cur_buttons & 0x00010000u) != 0) {
        if ((g_impose->home_button_mode & 1u) != 0) {
            return;
        }
        if ((g_impose->popup_disable_flags & 1u) == 0 && (g_impose->cur_buttons & 0x00010000u) == 0) {
            if ((g_impose->menu_state & 0x100u) != 0) {
                if (g_impose->anim_phase < 4) {
                    return;
                }
                imposeCloseOverlayByType(0);
            } else {
                imposeCloseOverlayByType(1);
            }
        }
    }
    if ((cur_buttons & 0x00020000u) != 0) {
        return;
    }
    if ((cur_buttons & 0x00400000u) != 0 && (g_impose->cur_buttons & 0x00400000u) == 0) {
        g_impose->screen_hold_ticks = 0;
        g_impose->special_btn_pending |= 0x00400000u;
    }
    if (g_impose->volume_mode != 0 || g_impose->deferred_event_bits != 0) {
        return;
    }
    if ((cur_buttons & 0x00800000u) != 0 && (g_impose->cur_buttons & 0x00800000u) == 0) {
        g_impose->note_hold_ticks = 0;
        g_impose->special_btn_pending |= 0x00800000u;
    }
}

static void imposeHandleHoldButton(u32 cur_buttons, u32 last_buttons, u32 arg)
{
    (void)last_buttons;
    (void)arg;

    if (g_impose->menu_state == 0 || (cur_buttons & 0x00020000u) != 0) {
        return;
    }
    if (g_impose->volume_mode != 0 || g_impose->deferred_event_bits != 0) {
        return;
    }
    if ((cur_buttons & 0x00100000u) != 0) {
        g_impose->special_btn_pending |= 0x00100000u;
    }
    if ((cur_buttons & 0x00200000u) != 0) {
        g_impose->avls_blink_counter = 0;
        g_impose->special_btn_pending |= 0x00200000u;
    }
    g_impose->vol_hold_ticks = 0;
}

static void imposeNoopCallback(s32 kind, s32 enable)
{
    (void)kind;
    (void)enable;
}

static s32 imposeCtrlSubIntrHandler(s32 subintr_no, void *arg)
{
    SceCtrlData pad;
    u32 trig = 0;
    u32 prev_buttons = 0;
    u32 sys_time;

    (void)subintr_no;
    (void)arg;

    if (g_impose->menu_state == 0) {
        return -1;
    }

    g_impose->osd_frame_counter = (g_impose->osd_frame_counter + 1) & 0x7FFFFFFF;
    g_impose->blink_frame_counter = (g_impose->blink_frame_counter + 1) & 0x7FFFFFFF;
    sys_time = sceKernelGetSystemTimeLow();

    if (g_impose->home_button_mode != 0) {
        sceCtrlPeekBufferPositive(&pad, 1);
        g_impose->home_button_pressed = 0;
        if ((g_impose->home_button_mode & 1u) != 0 && (pad.Buttons & 0x00010000u) != 0) {
            g_impose->home_button_pressed = 1;
        }
    }

    if (g_impose->special_btn_pending == 0 && g_impose->screen_hold_state == 0 && (g_impose->menu_state & 2u) == 0) {
        g_impose->cur_buttons = 0;
    } else if (g_impose->suspend_active == 0) {
        g_impose->vol_hold_ticks = (g_impose->vol_hold_ticks + 1) & 0x7FFFFFFF;
        g_impose->screen_hold_ticks = (g_impose->screen_hold_ticks + 1) & 0x7FFFFFFF;
        g_impose->note_hold_ticks = (g_impose->note_hold_ticks + 1) & 0x7FFFFFFF;
        prev_buttons = g_impose->cur_buttons;
        sceCtrlPeekBufferPositive(&pad, 1);
        u32 cur_buttons = pad.Buttons;
        g_impose->cur_buttons = cur_buttons;
        trig = cur_buttons & (cur_buttons ^ prev_buttons);
        imposeHandleVolumeButtons(trig, cur_buttons, prev_buttons);
    } else {
        g_impose->cur_buttons = 0;
    }

    if (sceHprm_driver_EFCFD0C5() != 0) {
        u32 prev_hprm = g_impose->cur_hprm_key;
        sceHprmPeekCurrentKey(&g_impose->cur_hprm_key);
        u32 hprm_trig = g_impose->cur_hprm_key & (g_impose->cur_hprm_key ^ prev_hprm);
        if ((hprm_trig & 0x10u) != 0) {
            g_impose->vol_hold_ticks = 0;
            g_impose->special_btn_pending |= 0x00100000u;
        }
        if ((hprm_trig & 0x20u) != 0) {
            g_impose->vol_hold_ticks = 0;
            g_impose->avls_blink_counter = 0;
            g_impose->special_btn_pending |= 0x00200000u;
        }
    }

    if (g_impose->init_state == 0) {
        imposePollHprmState();
    }
    if ((g_impose->hprm_caps & 0x00010000u) != 0) {
        imposePollHeadphoneState();
    }

    imposeTickPopupTimer();

    if ((g_impose->osd_frame_counter & 3) == 0 && g_impose->suspend_active == 0) {
        s32 prev_exist = g_impose->battery_exist;
        g_impose->battery_exist = scePower_driver_8C873AA7();
        if (g_impose->battery_exist != prev_exist) {
            g_impose->osd_dirty_flags &= ~1u;
        }
        if (g_impose->battery_exist != 0) {
            if (scePower_driver_FA651CE1() != 0) {
                if (g_impose->battery_low == 0) {
                    g_impose->osd_dirty_flags &= ~0x80u;
                }
                g_impose->battery_low = 1;
            } else {
                s32 prev_pct = g_impose->battery_percent;
                s32 pct = scePower_driver_31AEA94C();
                if (pct >= 0) {
                    g_impose->battery_percent = pct;
                }
                g_impose->battery_low = 0;
                if (g_impose->battery_percent != prev_pct) {
                    g_impose->osd_dirty_flags &= ~0x80u;
                }
            }
        }
        g_impose->ac_online = scePower_driver_872F4ECE();
        s32 prev_charging = g_impose->battery_charging;
        g_impose->battery_charging = scePower_driver_7A9EA6DE();
        if (g_impose->battery_charging != prev_charging) {
            g_impose->osd_dirty_flags &= ~0x80u;
        }
    }

    if ((g_impose->blink_frame_counter % 24) == 0) {
        g_impose->battery_anim_frame--;
        if (g_impose->battery_anim_frame < 0) {
            g_impose->battery_anim_frame = 3;
        }
    }

    if (g_impose->display_off_state == 2) {
        if ((u32)(sys_time - g_impose->display_dim_time) > 0x98967Fu) {
            imposeSetEventFlag(0x2000u);
            g_impose->display_off_state = 1;
        }
    }

    s32 cap_check = g_impose->brightness_max_limit;
    g_impose->brightness_max_limit = 0;
    if (cap_check != 0) {
        s32 cur_val;
        s32 target = imposeLevelToBrightness((u32)g_impose->brightness_level);
        imposeGetBacklightInfo(&cur_val);
        if (g_impose->brightness_dimmed != 0) {
            g_impose->brightness_cap_pending = 1;
        } else {
            s32 limit = g_impose->brightness_max_limit;
            if (limit < target) {
                imposeBrightnessToBacklightSel(limit);
            } else if (cur_val < target) {
                imposeBrightnessToBacklightSel(target);
            }
        }
    }

    u32 deferred = g_impose->deferred_event_bits;
    if (deferred != 0) {
        g_impose->deferred_event_ticks++;
        if (g_impose->deferred_event_ticks > g_impose->deferred_event_delay) {
            imposeSetEventFlag(deferred);
            g_impose->deferred_event_ticks = 0;
            g_impose->deferred_event_bits = 0;
        }
    }

    if (g_impose->osd_visible != 0) {
        imposeDismissActiveOverlay();
        if (g_impose->osd_frame_counter >= 120) {
            u32 state = g_impose->menu_state;
            if ((state & 4u) == 0) {
                if ((state & 0x00040000u) == 0) {
                    sceImpose_driver_BB12F974(1u);
                } else if ((state & 0x00020000u) != 0) {
                    g_impose->menu_state = state & ~0x00020000u;
                    imposeClearOsdRegion(0x10u);
                }
            }
        }
        if ((g_impose->menu_state & 0x100u) != 0) {
            imposeHandleDialogButtons((s32)trig, g_impose->cur_buttons, prev_buttons);
        }
        s32 prev_hold = g_impose->hold_switch_state;
        s32 cur_hold = ((g_impose->cur_buttons & 0x00020000u) != 0) ? 1 : 0;
        g_impose->hold_switch_state = cur_hold;
        if (prev_hold != cur_hold) {
            g_impose->osd_dirty_flags &= ~1u;
        }
    }
    return -1;
}

static void imposeHideOsdFrameBuf(void)
{
    if ((g_impose->menu_state & 1u) != 0) {
        if (g_impose->osd_visible != 0) {
            sceDisplay_driver_3E17FE8D(0, NULL, 512, 1, 1);
            g_impose->dialog_selection = 0;
            g_impose->osd_visible = 0;
            g_impose->osd_dirty_flags = 0;
            g_impose->anim_phase = 0;
            g_impose->osd_frame_counter = 0;
            g_impose->anim_step_counter = 0;
            g_impose->cur_buttons = 0;
            g_impose->dialog_wait_release = 1;
            if (g_impose->popup_banner_mode == 2) {
                g_impose->dialog_selection = 1;
            }
        }
        imposeRestoreButtonIntercept();
    }
}

static s32 imposeMainThread(u32 args, void *argp)
{
    u32 pattern;

    (void)args;
    (void)argp;

    while (1) {
        sceKernelWaitEventFlag(g_impose->event_flag_id, 0xC000FFFFu, 0x21u, &pattern, NULL);

        s32 vol_target = (g_impose->hprm_status == 0x10) ? g_impose->volume_target : -1;
        s32 allow_notify = ((pattern & 0x40000000u) == 0) ? 1 : 0;

        if ((pattern & 0x100u) != 0) {
            s32 intr = sceKernelCpuSuspendIntr();
            imposeEnableButtonIntercept(1);
            sceKernelCpuResumeIntr(intr);
        }
        if ((pattern & 0x200u) != 0) {
            g_impose->resource_loading = 1;
            imposeLoadLanguageText();
            g_impose->resource_loading = 0;
        }
        if ((pattern & 1u) != 0) {
            imposeClearAllOverlays();
        }
        if ((pattern & 2u) != 0) {
            s32 intr = sceKernelCpuSuspendIntr();
            imposeHideOsdFrameBuf();
            sceKernelCpuResumeIntr(intr);
        }
        if ((pattern & 0x800u) != 0) {
            if (g_impose->tvout_active == 1 && (g_impose->display_caps & 0x01000000u) != 0 && g_impose->tvout_cable_type == 0) {
                pattern |= 0x4000u;
            } else {
                imposeRestoreDisplayMode();
            }
        }
        if ((pattern & 0x8000u) != 0) {
            if (g_impose->volume_target == 1) {
                if (g_impose->tvout_active == 1) {
                    imposeApplySpeakerVolume();
                }
            } else if (g_impose->volume_target == 2) {
                if (sceImpose_driver_E9B0E008(NULL) == 0 && g_impose->tvout_active == 0) {
                    if (imposeApplyHeadphoneVolume() < 0) {
                        imposeSetVolumeTarget(1);
                    }
                }
            }
            pattern |= 0x20u;
            if (g_impose->hprm_status == 0x10) {
                pattern |= 0x4000u;
            }
            if (g_impose->tvout_cable_type != 0) {
                allow_notify = 0;
            }
        }
        if ((pattern & 0x4000u) != 0) {
            if (imposeHandlePowerEvent(vol_target, allow_notify) != 0) {
                pattern |= 4u;
            }
            if (g_impose->hprm_status == 0x10) {
                pattern |= 0x20u;
            }
        }
        if ((pattern & 0x20u) != 0) {
            imposeRefreshHprmDevices(vol_target, allow_notify);
        }
        if ((pattern & 0x44u) != 0) {
            imposeUpdateHprmOnEvent();
            if ((pattern & 4u) != 0) {
                g_impose->osd_dirty_flags &= ~6u;
                if (g_impose->suppress_notify == 0) {
                    g_impose->pending_change_flags |= 1u;
                }
            }
            if ((pattern & 0x40u) != 0) {
                g_impose->osd_dirty_flags &= ~1u;
                if (g_impose->suppress_notify == 0 && (g_impose->home_button_mode & 0x10u) == 0) {
                    g_impose->pending_change_flags |= 4u;
                }
            }
        }
        if ((pattern & 0x400u) != 0) {
            if (g_impose->volume_mode == 0) {
                sceKermitPeripheral_driver_76BE2E28(-1, -1, 0);
            } else if (g_impose->volume_mode == 1) {
                sceKermitPeripheral_driver_76BE2E28(-1, -1, 1);
            }
        }
        if ((pattern & 0x1000u) != 0) {
            sceDisplayEnable();
        }
        if ((pattern & 0x2000u) != 0) {
            sceDisplayDisable();
        }
        if ((pattern & 8u) != 0) {
            imposeLoadRegistrySettings();
            g_impose->reg_flush_pending = 0;
        }
        if ((pattern & 0x80u) != 0) {
            if (g_impose->fb_buf_cached == NULL) {
                return 0;
            }
            if (sceKernelFreeFpl(g_impose->fb_fpl_id, g_impose->fb_buf_cached) < 0) {
                return (s32)0x8001000C;
            }
            if (sceKernelDeleteFpl(g_impose->fb_fpl_id) < 0) {
                return (s32)0x8001000C;
            }
            if (imposeFreeIconResource() < 0) {
                return (s32)0x8001000C;
            }
            if (imposeFreeTextResource() < 0) {
                return (s32)0x8001000C;
            }
            g_impose->fb_buf_uncached = NULL;
            g_impose->fb_fpl_id = 0;
            g_impose->fb_buf_cached = NULL;
        }
        if ((pattern & 0x10u) != 0) {
            if (sceKernelInvokeExitCallback() >= 0) {
                g_impose->menu_state |= 0x80000000u;
            }
        }
        if ((s32)pattern < 0) {
            break;
        }
        g_impose->suppress_notify = 0;
    }

    sceKernelExitThread(0);
    return 0;
}

static s32 imposeApplyRegistrySetting(u32 id)
{
    const char *cat = NULL;
    const char *key = NULL;
    s32 val = -1;

    switch (id) {
    case 3:
        cat = "/CONFIG/SYSTEM/SOUND";
        key = "main_volume";
        val = g_impose->main_volume;
        break;
    case 4:
        cat = "/CONFIG/SYSTEM/SOUND";
        key = "mute";
        val = g_impose->mute;
        break;
    case 6:
        cat = "/CONFIG/SYSTEM/SOUND";
        key = "equalizer_mode";
        val = g_impose->equalizer_mode;
        break;
    case 7:
        cat = "/CONFIG/SYSTEM";
        key = "backlight_brightness";
        val = g_impose->brightness_level;
        break;
    case 16:
        cat = "/CONFIG/DISPLAY";
        key = "aspect_ratio";
        val = g_impose->aspect_ratio;
        break;
    case 17:
        cat = "/CONFIG/DISPLAY";
        key = "scan_mode";
        val = g_impose->scan_mode;
        break;
    case 20:
        cat = "/CONFIG/DISPLAY";
        key = "pi_blending_mode";
        val = g_impose->pi_blending_mode;
        break;
    default:
        break;
    }
    if (val == -1) {
        return 0;
    }
    return (imposeWriteRegistryInt(cat, key, val) == 0) ? 0 : -1;
}

static void imposeRedrawOsd(void)
{
    if ((s32)g_impose->menu_state < 0) {
        if ((s32)g_impose->osd_dirty_flags >= 0) {
            imposeClearOsdRegion(0xFFu);
            if (g_impose->text_fpl_size != 0) {
                SceImposeRect *wait_rect = &g_impose->text_rects[IMPOSE_TEXT_PLEASE_WAIT];
                imposeBlitIcon16Wrapper(wait_rect->x, wait_rect->y, wait_rect->width, wait_rect->height,
                                        g_impose->text_sprites[IMPOSE_TEXT_PLEASE_WAIT]);
                g_impose->osd_dirty_flags |= 0x80000000u;
                imposeSetEventFlag(0x10u);
            }
        }
        return;
    }

    s32 drawn = imposeDrawClockDisplay();
    if ((g_impose->osd_dirty_flags & 0x10u) == 0) {
        if (imposeAdvanceAnimFrame() == 0) {
            return;
        }
    }
    if (drawn != 0) {
        return;
    }
    if ((g_impose->menu_state & 0x00010000u) != 0) {
        drawn = imposeDrawMuteIcon();
        if (drawn != 0) {
            return;
        }
    }
    if (g_impose->text_fpl_size == 0) {
        return;
    }

    s32 blink_phase = g_impose->blink_frame_counter % 60;
    if ((g_impose->osd_dirty_flags & 8u) == 0) {
        SceImposeRect *q_rect = &g_impose->text_rects[IMPOSE_TEXT_QUESTION];
        SceImposeRect *enter_rect = &g_impose->text_rects[IMPOSE_TEXT_ENTER];
        SceImposeRect *back_rect = &g_impose->text_rects[IMPOSE_TEXT_BACK];

        imposeBlitIcon16Wrapper(q_rect->x, q_rect->y + g_impose->dialog_y_offset, q_rect->width, q_rect->height,
                                g_impose->text_sprites[IMPOSE_TEXT_QUESTION]);
        imposeBlitIcon16Wrapper(g_impose->circle_icon_x, g_impose->circle_icon_y, 16, 16,
                                g_impose->icon_sprites[IMPOSE_ICON_CIRCLE]);
        imposeBlitIcon16Wrapper(g_impose->cross_icon_x, g_impose->cross_icon_y, 16, 16,
                                g_impose->icon_sprites[IMPOSE_ICON_CROSS]);
        if (g_impose->button_assign != 0) {
            imposeBlitIcon16Wrapper(back_rect->x, back_rect->y, enter_rect->width, enter_rect->height,
                                    g_impose->text_sprites[IMPOSE_TEXT_ENTER]);
            imposeBlitIcon16Wrapper(enter_rect->x, enter_rect->y, back_rect->width, back_rect->height,
                                    g_impose->text_sprites[IMPOSE_TEXT_BACK]);
        } else {
            imposeBlitIcon16Wrapper(back_rect->x, back_rect->y, back_rect->width, back_rect->height,
                                    g_impose->text_sprites[IMPOSE_TEXT_BACK]);
            imposeBlitIcon16Wrapper(enter_rect->x, enter_rect->y, enter_rect->width, enter_rect->height,
                                    g_impose->text_sprites[IMPOSE_TEXT_ENTER]);
        }
        g_impose->osd_dirty_flags |= 8u;
        return;
    }

    const void *yes_sprite;
    const void *no_sprite;
    if (g_impose->dialog_wait_release != 0) {
        if (g_impose->dialog_selection != 0) {
            yes_sprite = g_impose->text_sprites[IMPOSE_TEXT_YES_HIGHLIGHT];
            no_sprite = g_impose->text_sprites[IMPOSE_TEXT_NO_NORMAL];
        } else {
            yes_sprite = g_impose->text_sprites[IMPOSE_TEXT_YES_NORMAL];
            no_sprite = g_impose->text_sprites[IMPOSE_TEXT_NO_HIGHLIGHT];
        }
    } else if (g_impose->dialog_selection != 0) {
        if (blink_phase < 32) {
            yes_sprite = g_impose->text_sprites[IMPOSE_TEXT_YES_HIGHLIGHT];
        } else if (blink_phase < 36 || blink_phase >= 56) {
            yes_sprite = g_impose->text_sprites[IMPOSE_TEXT_YES_MID];
        } else {
            yes_sprite = g_impose->text_sprites[IMPOSE_TEXT_YES_NORMAL];
        }
        no_sprite = g_impose->text_sprites[IMPOSE_TEXT_NO_NORMAL];
    } else {
        yes_sprite = g_impose->text_sprites[IMPOSE_TEXT_YES_NORMAL];
        if (blink_phase < 32) {
            no_sprite = g_impose->text_sprites[IMPOSE_TEXT_NO_HIGHLIGHT];
        } else if (blink_phase < 36 || blink_phase >= 56) {
            no_sprite = g_impose->text_sprites[IMPOSE_TEXT_NO_MID];
        } else {
            no_sprite = g_impose->text_sprites[IMPOSE_TEXT_NO_NORMAL];
        }
    }

    if (g_impose->cur_yes_sprite != yes_sprite) {
        g_impose->cur_yes_sprite = yes_sprite;
        g_impose->osd_dirty_flags &= ~0x100u;
        g_impose->yes_btn_anim_reset = 0;
    }
    if (g_impose->cur_no_sprite != no_sprite) {
        g_impose->cur_no_sprite = no_sprite;
        g_impose->osd_dirty_flags &= ~0x200u;
        g_impose->no_btn_anim_reset = 0;
    }
    if ((g_impose->osd_dirty_flags & 0x100u) == 0) {
        SceImposeRect *yes_rect = &g_impose->text_rects[IMPOSE_TEXT_YES_NORMAL];
        imposeBlitIcon16Wrapper(yes_rect->x, yes_rect->y + g_impose->dialog_y_offset,
                                yes_rect->width, yes_rect->height, yes_sprite);
        g_impose->osd_dirty_flags |= 0x100u;
    }
    if ((g_impose->osd_dirty_flags & 0x200u) == 0) {
        SceImposeRect *no_rect = &g_impose->text_rects[IMPOSE_TEXT_NO_NORMAL];
        imposeBlitIcon16Wrapper(no_rect->x, no_rect->y + g_impose->dialog_y_offset,
                                no_rect->width, no_rect->height, no_sprite);
        g_impose->osd_dirty_flags |= 0x200u;
    }
    imposeDrawStatusBanner(185);
}

static void imposeDrawBlinkCursor(void)
{
    if ((g_impose->blink_frame_counter % 20) < 12) {
        imposeBlitIcon16(226, 123, 28, 25, g_impose->icon_sprites[IMPOSE_ICON_BLINK_CURSOR]);
        return;
    }
    u16 *dst = g_impose->fb_buf_uncached + (123 * g_impose->fb_stride + 226);
    s32 row;
    for (row = 24; row >= 0; row--) {
        sceKernelMemset32(dst, IMPOSE_CLEAR_COLOR_WORD, 56u);
        dst += (g_impose->fb_stride / 2) * 2;
    }
}

static void imposeDrawVolumeBar(void)
{
    if ((s32)g_impose->osd_dirty_flags >= 0) {
        u32 *dst = (u32 *)(g_impose->fb_buf_uncached + (124 * g_impose->fb_stride + 202));
        const u32 *src = (const u32 *)g_impose->icon_sprites[IMPOSE_ICON_TVOUT];
        s32 y, x;
        for (y = 0; y < 32; y++) {
            for (x = 0; x < 21; x++) {
                dst[0] = src[0];
                dst[1] = src[1];
                dst += 2;
                src += 2;
            }
            dst += ((g_impose->fb_stride - 84) / 4) * 2;
        }
        g_impose->osd_dirty_flags |= 0x80000000u;
    }
    if (g_impose->anim_step_counter < 71) {
        g_impose->anim_step_counter++;
        return;
    }
    sceImpose_driver_BB12F974(1u);
    if (g_impose->tvout_active == 2) {
        imposeSetTvOutMode(0, g_tvout_mode->tv_mode, g_tvout_mode->width, g_tvout_mode->height);
    }
}

static void imposeShowBatteryIcon(void)
{
    g_impose->popup_banner_mode = 3;
    imposeDrawStatusBanner(128);
}

static s32 imposeLoadResourceFile(void)
{
    if (g_impose->fb_buf_cached == NULL) {
        return -1;
    }
    s32 file_id = sceIoOpen("flash0:/kd/resource/impose.rsc", 1, 0);
    if (file_id < 0) {
        return (s32)0x80010002;
    }
    s32 size = (s32)sceIoLseek(file_id, 0, 2);
    if (size < 0 || sceIoLseek(file_id, 0, 0) < 0 || sceIoRead(file_id, g_impose->fb_buf_cached, (u32)size) < 0) {
        sceIoClose(file_id);
        return (s32)0x80010002;
    }
    sceIoClose(file_id);
    return size;
}

static s32 imposeParseResourceHeader(s32 lang)
{
    SceImposeRscHeader *rsc = (SceImposeRscHeader *)g_impose->fb_buf_cached;
    if (rsc == NULL) {
        return -1;
    }

    u8 *rsc_bytes = (u8 *)rsc;
    u32 lang_off = rsc->offsets[rsc->palette_count + lang];
    u32 max_end = (u32)rsc_bytes + (u32)(2 * g_impose->fb_stride * g_impose->fb_height);
    if (max_end < lang_off) {
        return (s32)0x8001000E;
    }

    SceImposeLangHeader *lang_hdr = (SceImposeLangHeader *)(rsc_bytes + lang_off);
    u8 *sprite_base = (u8 *)lang_hdr->sprites;

    g_impose->cross_icon_x = lang_hdr->cross_x;
    g_impose->cross_icon_y = lang_hdr->cross_y;
    g_impose->circle_icon_x = lang_hdr->circle_x;
    g_impose->circle_icon_y = lang_hdr->circle_y;

    s32 intr = sceKernelCpuSuspendIntr();
    u16 *dst = g_impose->text_fpl_cur;
    s32 idx;
    for (idx = 0; idx < lang_hdr->sprite_count; idx++) {
        SceImposeLangSpriteDesc *desc = &lang_hdr->sprites[idx];
        const u16 *palette = (const u16 *)(rsc_bytes + rsc->offsets[desc->palette_index]);
        s32 pixels = desc->width * desc->height;

        g_impose->text_sprites[idx] = dst;
        g_impose->text_rects[idx].x = desc->x;
        g_impose->text_rects[idx].y = desc->y;
        g_impose->text_rects[idx].width = desc->width;
        g_impose->text_rects[idx].height = desc->height;

        const u16 *src = (const u16 *)(sprite_base + desc->data_offset);
        if (desc->palette_index < 0) {
            s32 pairs = pixels / 2;
            u16 *out = dst;
            while (pairs > 0) {
                u16 packed = *src++;
                out[0] = palette[packed & 0xFFu];
                out[1] = palette[packed >> 8];
                out += 2;
                pairs--;
            }
        } else {
            s32 quads = pixels / 4;
            u16 *out = dst;
            s32 q;
            for (q = 0; q < quads; q++) {
                u16 packed = *src++;
                out[0] = palette[packed & 0xFu];
                out[1] = palette[(packed >> 4) & 0xFu];
                out[2] = palette[(packed >> 8) & 0xFu];
                out[3] = palette[packed >> 12];
                out += 4;
            }
        }
        dst += pixels;
    }
    sceKernelCpuResumeIntr(intr);
    return 0;
}

static s32 imposeValidateResourceMagic(void)
{
    SceImposeRscHeader *rsc = (SceImposeRscHeader *)g_impose->fb_buf_cached;
    if (rsc == NULL) {
        return -1;
    }

    u8 *rsc_bytes = (u8 *)rsc;
    u32 max_end = (u32)rsc_bytes + (u32)(2 * g_impose->fb_stride * g_impose->fb_height);
    u32 icon_offsets[24];
    s32 i;

    for (i = 0; i < rsc->icon_count; i++) {
        u32 off = rsc->offsets[rsc->palette_count + rsc->lang_count + i];
        icon_offsets[i] = off;
        if (max_end < off) {
            return (s32)0x8001000E;
        }
    }

    s32 intr = sceKernelCpuSuspendIntr();
    u8 *dst = g_impose->icon_fpl_cur;
    for (i = 0; i < rsc->icon_count; i++) {
        const u32 *entry = (const u32 *)(rsc_bytes + icon_offsets[i]);
        u32 len = entry[0];
        memcpy(dst, entry + 1, len);
        g_impose->icon_sprites[i] = dst;
        dst += len;
    }
    sceKernelCpuResumeIntr(intr);
    return 0;
}

static void imposeShowOsdPopup(u64 expire_tick, u64 warn_sec)
{
    u64 tick_arg;

    g_impose->popup_expire_time.microsecond = 0;
    g_impose->popup_timer_active = 0;
    g_impose->popup_timer_phase = 0;
    g_impose->popup_banner_mode = 0;
    g_impose->popup_warn_lo = 0;
    g_impose->popup_warn_hi = 0;
    g_impose->popup_expire_time.year = 0;
    g_impose->popup_expire_time.month = 0;
    g_impose->popup_expire_time.day = 0;
    g_impose->popup_expire_time.hour = 0;
    g_impose->popup_expire_time.minute = 0;
    g_impose->popup_expire_time.second = 0;

    u32 prev_lo = g_impose->popup_expire_lo;
    u32 prev_hi = g_impose->popup_expire_hi;
    g_impose->popup_expire_lo = 0;
    g_impose->popup_expire_hi = 0;

    s32 prev_y_off = g_impose->dialog_y_offset;
    g_impose->popup_timer_active = 0;
    g_impose->dialog_y_offset = 0;
    g_impose->popup_timer_phase = 0;
    g_impose->popup_banner_mode = 0;

    tick_arg = expire_tick;
    SysMemForKernel_2A8B8B2D(&tick_arg);

    if (expire_tick != 0) {
        u64 warn_tick = expire_tick - warn_sec * 1000000ULL;
        g_impose->popup_expire_lo = (u32)expire_tick;
        g_impose->popup_expire_hi = (u32)(expire_tick >> 32);
        g_impose->popup_warn_lo = (u32)warn_tick;
        g_impose->popup_warn_hi = (u32)(warn_tick >> 32);
        g_impose->popup_timer_phase = 1;
        g_impose->dialog_y_offset = -16;
        g_impose->popup_timer_active = 1;
    }

    if (prev_lo != (u32)expire_tick || prev_hi != (u32)(expire_tick >> 32)) {
        if (g_impose->fb_buf_cached != NULL) {
            s32 cur_y_off = g_impose->dialog_y_offset;
            g_impose->dialog_y_offset = prev_y_off;
            imposeClearOsdRegion(0x22u);
            g_impose->dialog_y_offset = cur_y_off;
            if ((g_impose->menu_state & 0x100u) != 0) {
                g_impose->blink_frame_counter = 0;
                g_impose->cur_yes_sprite = NULL;
                g_impose->cur_no_sprite = NULL;
                g_impose->yes_btn_anim_reset = 0;
                g_impose->no_btn_anim_reset = 0;
                g_impose->osd_dirty_flags &= ~0x718u;
            }
        }
        g_impose->pending_change_flags |= 0x4000u;
    }
}

static void imposeRefreshRegistryValues(s32 mode)
{
    s32 reg_val;

    if (mode == -1) {
        reg_val = imposeReadRegistrySetting(0u);  if (reg_val != -1) g_impose->fb_stride = reg_val;
        reg_val = imposeReadRegistrySetting(1u);  if (reg_val != -1) g_impose->fb_height = reg_val;
        reg_val = imposeReadRegistrySetting(2u);  if (reg_val != -1) g_impose->language = reg_val;
        reg_val = imposeReadRegistrySetting(14u); if (reg_val != -1) g_impose->button_assign = reg_val;
        reg_val = imposeReadRegistrySetting(15u); if (reg_val != -1) g_impose->sound_reduction = reg_val;
        reg_val = imposeReadRegistrySetting(19u); if (reg_val != -1) g_impose->oem_charset = reg_val;
    }
    reg_val = imposeReadRegistrySetting(3u);  if (reg_val != -1) g_impose->main_volume = reg_val;
    reg_val = imposeReadRegistrySetting(4u);  if (reg_val != -1) g_impose->mute = reg_val;
    reg_val = imposeReadRegistrySetting(5u);  if (reg_val != -1) g_impose->avls = reg_val;
    reg_val = imposeReadRegistrySetting(6u);  if (reg_val != -1) g_impose->equalizer_mode = reg_val;
    reg_val = imposeReadRegistrySetting(8u);  if (reg_val != -1) g_impose->time_format = reg_val;
    reg_val = imposeReadRegistrySetting(9u);  if (reg_val != -1) g_impose->date_format = reg_val;
    reg_val = imposeReadRegistrySetting(12u); if (reg_val != -1) g_impose->suspend_interval = reg_val;
    reg_val = imposeReadRegistrySetting(13u); if (reg_val != -1) g_impose->backlight_off_interval = reg_val;

    if (g_impose->display_caps != 0) {
        reg_val = imposeReadRegistrySetting(16u); if (reg_val != -1) g_impose->aspect_ratio = reg_val;
        reg_val = imposeReadRegistrySetting(17u); if (reg_val != -1) g_impose->scan_mode = reg_val;
        reg_val = imposeReadRegistrySetting(18u); if (reg_val != -1) g_impose->screensaver_start_time = reg_val;
        g_impose->pi_blending_mode = 1;
        reg_val = imposeReadRegistrySetting(20u); if (reg_val != -1) g_impose->pi_blending_mode = reg_val;
        if ((g_impose->display_caps & 4u) != 0) {
            sceDisplay_driver_7DF044BA(g_impose->pi_blending_mode);
        }
    }

    if (g_impose->app_type != 0x100) {
        reg_val = imposeReadRegistrySetting(21u);
        s32 color_space = 2;
        if (reg_val == 1) {
            color_space = 1;
        } else if (reg_val == 2) {
            color_space = 0;
        }
        sceDisplay_driver_9B18DDDD(color_space);
    }

    reg_val = imposeReadRegistrySetting(22u);
    sceDisplay_driver_F455917F((reg_val == 1) ? 1 : 0);

    reg_val = imposeReadRegistrySetting(7u);
    s32 restore_level = 0;
    s32 saved_level = 0;
    if ((u32)reg_val < 4u) {
        g_impose->brightness_level = reg_val;
        s32 max_lvl = imposeBrightnessToLevel(g_impose->brightness_max_limit);
        if (max_lvl < reg_val) {
            saved_level = reg_val;
            restore_level = 1;
            g_impose->brightness_level = max_lvl;
        }
    } else {
        if (g_impose->hardware_model == 10 || g_impose->ac_online == 0) {
            g_impose->cur_brightness = 72;
            g_impose->brightness_level = 2;
        } else {
            g_impose->cur_brightness = 100;
            g_impose->brightness_level = 3;
        }
    }
    g_impose->brightness_dimmed = 1;
    imposeStepBrightness(0);
    if (restore_level != 0) {
        g_impose->brightness_level = saved_level;
    }
    if (g_impose->display_caps != 0 && g_impose->initial_display_mode != 0 && g_impose->initial_display_mode != 0xE0) {
        imposeStepBrightness(3);
    }

    s32 tz_offset = imposeReadRegistrySetting(11u);
    s32 summer_time = imposeReadRegistrySetting(10u);
    if (summer_time == -1) {
        summer_time = 0;
    }
    if (tz_offset == -1) {
        tz_offset = 0;
    }
    sceRtc_driver_DFF30673(g_impose->date_format, g_impose->time_format, tz_offset, summer_time * 60);
    sceRtc_driver_9012B140(&g_impose->rtc_time);

    if (mode == -1) {
        if ((u32)g_impose->language >= 12u) {
            g_impose->language = 0;
        }
        g_impose->pending_language = g_impose->language;
        g_impose->pending_button_assign = g_impose->button_assign;
        g_impose->default_language = g_impose->language;
    } else if (mode == 2) {
        reg_val = imposeReadRegistrySetting(2u);
        if ((u32)reg_val >= 12u) {
            reg_val = 0;
        }
        if (g_impose->default_language == g_impose->language && g_impose->default_language != reg_val) {
            g_impose->pending_language = reg_val;
            g_impose->default_language = reg_val;
        }
        if (imposeIsMenuAllowed() != 0) {
            g_impose->language = reg_val;
        } else {
            g_impose->lang_change_pending = 1;
            g_impose->language = -1;
        }
    }
}

static void imposeSyncAudioVolume(void)
{
    if (g_impose->display_caps == 0) {
        return;
    }
    if (g_impose->app_type == 0x100) {
        sceLcdc_driver_451FE1A1(0);
    } else if ((g_impose->display_caps & 0x100u) != 0) {
        if (g_impose->aspect_ratio == 0) {
            sceLcdc_driver_451FE1A1(7);
        } else if (g_impose->aspect_ratio == 1) {
            sceLcdc_driver_451FE1A1(3);
        }
    }

    s32 init_mode = g_impose->initial_display_mode;
    if (init_mode == 0 || init_mode == 0xE0) {
        g_impose->tvout_active = 0;
        imposeSetVolumeTarget(1);
        if (g_impose->init_state != 2) {
            imposeSetEventFlag(0x4020u);
        }
        return;
    }

    g_impose->tvout_active = 1;
    imposeSetVolumeTarget(2);
    if (g_impose->init_state != 2) {
        imposeSetEventFlag(0x4020u);
    }
    g_impose->tvout_cable_type = 0;
    if (g_impose->init_state == 2) {
        return;
    }
    if (g_impose->app_type != 0x200 && g_impose->app_type != 0x400) {
        return;
    }

    s32 use_tv = 0;
    switch (init_mode) {
    case 210:
    case 466:
    case 722:
    case 978:
        use_tv = 1;
        break;
    case 209:
    case 465:
    case 1233:
    case 1489:
    case 1745:
    case 2001:
        if ((g_impose->display_caps & 4u) != 0) {
            use_tv = 1;
        }
        break;
    default:
        break;
    }
    imposeSetTvOutMode(1, use_tv ? g_tvout_mode->tv_mode : g_tvout_mode->lcd_mode,
                       g_tvout_mode->width, g_tvout_mode->height);
}

static s32 imposeTriggerPowerPoll(void)
{
    return imposeSetEventFlag(0x10u);
}

static void imposePollHprmState(void)
{
    s32 changed = 0;
    s32 status = imposeGetHprmStatus();
    if (status != 0 && g_impose->hprm_status != status) {
        g_impose->hprm_status = status;
        if (status == 0x10) {
            imposeRequestVolumeUpdate((g_impose->tvout_active == 1) ? 2 : 1);
        } else {
            g_impose->hprm_remote_exist = 0;
            g_impose->hprm_headphone_exist = 0;
        }
    }
    if (g_impose->hprm_status != 0x10) {
        return;
    }
    status = sceHprm_driver_FA4A25A7();
    if (g_impose->hprm_headphone_exist != status) {
        g_impose->hprm_headphone_exist = status;
        if (g_impose->volume_target == 2 && status == 0 && g_impose->hprm_remote_exist == 0) {
            imposeRequestVolumeUpdate(1);
        }
        if (g_impose->hprm_remote_exist == 0) {
            changed = 1;
        }
    }
    status = sceHprm_driver_EFCFD0C5();
    if (g_impose->hprm_remote_exist != status) {
        g_impose->hprm_remote_exist = status;
        if (g_impose->volume_target == 2 && status == 0) {
            imposeRequestVolumeUpdate(1);
        }
        changed = 1;
    }
    if (changed != 0) {
        imposeSetEventFlag(0x20u);
    }
}

static void imposePollHeadphoneState(void)
{
    s32 audio_mode = sceHprm_driver_AD158331();
    if (audio_mode >= 0 && g_impose->headphone_audio_mode != audio_mode) {
        u32 mask = (audio_mode == 2) ? 2u : ((audio_mode == 1) ? 1u : 0u);
        sceAudio_driver_5182B550(mask);
        g_impose->headphone_audio_mode = audio_mode;
    }
}

static void imposeTickPopupTimer(void)
{
    if (g_impose->popup_timer_active == 0) {
        return;
    }
    if (g_impose->popup_timer_phase == 1) {
        if (g_impose->popup_expire_hi == 0 && g_impose->popup_expire_lo == 0) {
            g_impose->osd_dirty_flags &= ~0x400u;
            g_impose->popup_banner_mode = 2;
            g_impose->pending_change_flags |= 0x2000u;
        } else if (g_impose->popup_warn_hi == 0 && g_impose->popup_warn_lo == 0) {
            g_impose->osd_dirty_flags &= ~0x400u;
            g_impose->popup_banner_mode = 1;
            g_impose->pending_change_flags |= 0x2000u;
        }
    } else if (g_impose->popup_timer_phase == 3) {
        if ((u32)(sceKernelGetSystemTimeLow() - g_impose->popup_exit_time) > 5000000u) {
            g_impose->menu_state |= 0x80000000u;
            imposeSetEventFlag(0x10u);
            g_impose->popup_timer_phase = 4;
        }
    }
}

static void imposeBlitIcon16(s32 x, s32 y, s32 width, s32 height, const void *src)
{
    u32 *dst = (u32 *)(g_impose->fb_buf_uncached + (g_impose->fb_stride * y + x));
    const u32 *src_words = (const u32 *)src;
    s32 row, col;

    if (height <= 0) {
        return;
    }
    s32 cols = width / 4;
    for (row = 0; row < height; row++) {
        for (col = 0; col < cols; col++) {
            dst[0] = src_words[0];
            dst[1] = src_words[1];
            dst += 2;
            src_words += 2;
        }
        dst += ((g_impose->fb_stride - width) / 4) * 2;
    }
}

static void imposeBlitIcon16Wrapper(s32 x, s32 y, s32 width, s32 height, const void *src)
{
    u32 *dst = (u32 *)(g_impose->fb_buf_uncached + (g_impose->fb_stride * y + x));
    const u32 *src_words = (const u32 *)src;
    s32 row, col;

    if (height <= 0) {
        return;
    }
    s32 cols = width / 4;
    for (row = 0; row < height; row++) {
        for (col = 0; col < cols; col += 2) {
            dst[0] = src_words[0];
            dst[1] = src_words[1];
            dst[2] = src_words[2];
            dst[3] = src_words[3];
            dst += 4;
            src_words += 4;
        }
        dst += ((g_impose->fb_stride - width) / 4) * 2;
    }
}

static void imposeDrawStatusBanner(s32 y)
{
    if (g_impose->popup_timer_active == 0) {
        return;
    }
    if ((g_impose->osd_dirty_flags & 0x400u) != 0) {
        return;
    }

    s32 icon_idx = 0;
    s32 banner_mode = g_impose->popup_banner_mode;
    if (banner_mode == 1 || banner_mode == 3) {
        icon_idx = (banner_mode == 1) ? 1 : 2;
        if ((g_impose->blink_frame_counter % 75) >= 45) {
            u16 *clear_dst = g_impose->fb_buf_uncached + (y * g_impose->fb_stride + 156);
            s32 row;
            for (row = 19; row >= 0; row--) {
                sceKernelMemset32(clear_dst, IMPOSE_CLEAR_COLOR_WORD, 344u);
                clear_dst += (g_impose->fb_stride / 2) * 2;
            }
            return;
        }
    } else {
        if (banner_mode == 2) {
            icon_idx = 2;
        }
        g_impose->osd_dirty_flags |= 0x400u;
    }

    s32 text_x = imposeFormatTimeString(&g_impose->popup_expire_time, 16, y + 2, 2);
    const u32 *src = (const u32 *)(g_impose->icon_sprites[IMPOSE_ICON_STATUS_BANNER] + 800 * icon_idx);
    u32 *dst = (u32 *)(g_impose->fb_buf_uncached + (y * g_impose->fb_stride + ((text_x - 24) & ~1)));
    s32 row, col;
    for (row = 0; row < 20; row++) {
        for (col = 0; col < 5; col++) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst += 2;
            src += 2;
        }
        dst += ((g_impose->fb_stride - 20) / 4) * 2;
    }
}

static s32 imposeDrawMuteIcon(void)
{
    s32 row, col, seg;

    if (g_impose->volume_disabled == 1) {
        if ((g_impose->osd_dirty_flags & 0x20u) == 0) {
            imposeBlitIcon16Wrapper(28, 223, 32, 28, g_impose->icon_sprites[IMPOSE_ICON_SPEAKER] + 3584);
            g_impose->osd_dirty_flags |= 0x20u;
        }
        if ((g_impose->osd_dirty_flags & 2u) == 0) {
            u16 *dst = g_impose->fb_buf_uncached + (223 * g_impose->fb_stride + 62);
            for (row = 23; row >= 0; row--) {
                sceKernelMemset32(dst, IMPOSE_CLEAR_COLOR_WORD, 360u);
                dst += (g_impose->fb_stride / 2) * 2;
            }
            g_impose->osd_dirty_flags |= 2u;
            return 1;
        }
        if ((g_impose->osd_dirty_flags & 4u) == 0) {
            u16 *dst = g_impose->fb_buf_uncached + (223 * g_impose->fb_stride + 242);
            for (row = 23; row >= 0; row--) {
                sceKernelMemset32(dst, IMPOSE_CLEAR_COLOR_WORD, 360u);
                dst += (g_impose->fb_stride / 2) * 2;
            }
            g_impose->osd_dirty_flags |= 4u;
            return 1;
        }
        return 0;
    }

    if (g_impose->avls != 0 && (g_impose->osd_dirty_flags & 0x40u) == 0) {
        if (g_impose->avls_blink_counter != 0) {
            if ((g_impose->avls_blink_counter % 60) >= 36) {
                u16 *dst = g_impose->fb_buf_uncached + (224 * g_impose->fb_stride + 426);
                for (row = 23; row >= 0; row--) {
                    sceKernelMemset32(dst, IMPOSE_CLEAR_COLOR_WORD, 104u);
                    dst += (g_impose->fb_stride / 2) * 2;
                }
            } else {
                const u32 *src = (const u32 *)g_impose->icon_sprites[IMPOSE_ICON_AVLS];
                u32 *dst = (u32 *)(g_impose->fb_buf_uncached + (224 * g_impose->fb_stride + 426));
                for (row = 0; row < 24; row++) {
                    for (col = 0; col < 13; col++) {
                        dst[0] = src[0];
                        dst[1] = src[1];
                        dst += 2;
                        src += 2;
                    }
                    dst += ((g_impose->fb_stride - 52) / 4) * 2;
                }
            }
            g_impose->avls_blink_counter++;
            if (g_impose->avls_blink_counter >= 120) {
                g_impose->avls_blink_counter = 0;
            }
        } else {
            const u32 *src = (const u32 *)g_impose->icon_sprites[IMPOSE_ICON_AVLS];
            u32 *dst = (u32 *)(g_impose->fb_buf_uncached + (224 * g_impose->fb_stride + 426));
            for (row = 0; row < 24; row++) {
                for (col = 0; col < 13; col++) {
                    dst[0] = src[0];
                    dst[1] = src[1];
                    dst += 2;
                    src += 2;
                }
                dst += ((g_impose->fb_stride - 52) / 4) * 2;
            }
            g_impose->osd_dirty_flags |= 0x40u;
        }
    }

    if ((g_impose->osd_dirty_flags & 0x20u) == 0) {
        const u8 *icon = g_impose->icon_sprites[IMPOSE_ICON_SPEAKER] + ((g_impose->mute != 0) ? 1792 : 0);
        imposeBlitIcon16Wrapper(28, 223, 32, 28, icon);
        g_impose->osd_dirty_flags |= 0x20u;
    }

    if (g_impose->mute != 0) {
        if ((g_impose->osd_dirty_flags & 2u) == 0) {
            u16 *dst = g_impose->fb_buf_uncached + (223 * g_impose->fb_stride + 62);
            for (row = 23; row >= 0; row--) {
                sceKernelMemset32(dst, IMPOSE_CLEAR_COLOR_WORD, 360u);
                dst += (g_impose->fb_stride / 2) * 2;
            }
            g_impose->osd_dirty_flags |= 2u;
            return 1;
        }
        if ((g_impose->osd_dirty_flags & 4u) == 0) {
            u16 *dst = g_impose->fb_buf_uncached + (223 * g_impose->fb_stride + 242);
            for (row = 23; row >= 0; row--) {
                sceKernelMemset32(dst, IMPOSE_CLEAR_COLOR_WORD, 360u);
                dst += (g_impose->fb_stride / 2) * 2;
            }
            g_impose->osd_dirty_flags |= 4u;
            return 1;
        }
        return 0;
    }

    if ((g_impose->osd_dirty_flags & 2u) == 0) {
        s32 bar_x = 62;
        for (seg = 0; seg < 15; seg++) {
            const u8 *sprite = (seg < g_impose->main_volume)
                             ? g_impose->icon_sprites[IMPOSE_ICON_VOL_BAR]
                             : (g_impose->icon_sprites[IMPOSE_ICON_VOL_BAR] + 384);
            imposeBlitIcon16Wrapper(bar_x, 223, 8, 24, sprite);
            bar_x += 12;
        }
        g_impose->osd_dirty_flags |= 2u;
        return 1;
    }
    if ((g_impose->osd_dirty_flags & 4u) == 0) {
        s32 bar_x = 242;
        for (seg = 15; seg < 30; seg++) {
            const u8 *sprite = (seg < g_impose->main_volume)
                             ? g_impose->icon_sprites[IMPOSE_ICON_VOL_BAR]
                             : (g_impose->icon_sprites[IMPOSE_ICON_VOL_BAR] + 384);
            imposeBlitIcon16Wrapper(bar_x, 223, 8, 24, sprite);
            bar_x += 12;
        }
        g_impose->osd_dirty_flags |= 4u;
        return 1;
    }
    return 0;
}

static s32 imposeDrawClockDisplay(void)
{
    s32 ret = 0;
    if ((g_impose->osd_dirty_flags & 1u) == 0) {
        imposeClearOsdRegion(1u);
        s32 right_x = (g_impose->battery_exist != 0) ? 450 : 480;
        g_impose->clock_left_x = imposeFormatTimeString(&g_impose->rtc_time, right_x, 5, 0);

        if (g_impose->mute != 0) {
            s32 draw_x = g_impose->clock_left_x - 21;
            g_impose->clock_left_x = draw_x;
            s32 odd = draw_x & 1;
            const u32 *src = (const u32 *)g_impose->icon_sprites[odd ? IMPOSE_ICON_MUTE_SM_ODD : IMPOSE_ICON_MUTE_SM_EVEN];
            u32 *dst = (u32 *)(g_impose->fb_buf_uncached + (5 * g_impose->fb_stride + (draw_x - odd)));
            s32 row, col;
            for (row = 0; row < 17; row++) {
                for (col = 0; col < 4; col++) {
                    dst[0] = src[0];
                    dst[1] = src[1];
                    dst += 2;
                    src += 2;
                }
                dst += ((g_impose->fb_stride - 16) / 4) * 2;
            }
        }

        if (g_impose->hold_switch_state != 0) {
            s32 draw_x = g_impose->clock_left_x - 29;
            g_impose->clock_left_x = draw_x;
            s32 odd = draw_x & 1;
            const void *icon = g_impose->icon_sprites[odd ? IMPOSE_ICON_HOLD_ODD : IMPOSE_ICON_HOLD_EVEN];
            imposeBlitIcon16Wrapper(draw_x - odd, 7, 24, 12, icon);
        }

        g_impose->osd_dirty_flags |= 1u;
        if (g_impose->battery_exist != 0) {
            g_impose->osd_dirty_flags &= ~0x80u;
        }
        ret = 1;
    }
    if (g_impose->battery_exist != 0) {
        imposeDrawBatteryLevel();
    }
    return ret;
}

static s32 imposeAdvanceAnimFrame(void)
{
    s32 base_y = g_impose->dialog_y_offset;
    s32 step = g_impose->anim_step_counter;

    switch (g_impose->anim_phase) {
    case 0:
        sceKernelMemset32(g_impose->fb_buf_uncached + ((base_y - step + 135) * g_impose->fb_stride + 80),
                          IMPOSE_LINE_COLOR_WORD, 640u);
        sceKernelMemset32(g_impose->fb_buf_uncached + ((base_y + step + 136) * g_impose->fb_stride + 80),
                          IMPOSE_LINE_COLOR_WORD, 640u);
        g_impose->anim_phase++;
        g_impose->anim_step_counter += 4;
        return 0;
    case 1:
        sceKernelMemset32(g_impose->fb_buf_uncached + ((base_y - step + 139) * g_impose->fb_stride + 80),
                          IMPOSE_CLEAR_COLOR_WORD, 640u);
        sceKernelMemset32(g_impose->fb_buf_uncached + ((base_y + step + 132) * g_impose->fb_stride + 80),
                          IMPOSE_CLEAR_COLOR_WORD, 640u);
        sceKernelMemset32(g_impose->fb_buf_uncached + ((base_y - step + 135) * g_impose->fb_stride + 80),
                          IMPOSE_LINE_COLOR_WORD, 640u);
        sceKernelMemset32(g_impose->fb_buf_uncached + ((base_y + step + 136) * g_impose->fb_stride + 80),
                          IMPOSE_LINE_COLOR_WORD, 640u);
        g_impose->anim_phase++;
        g_impose->anim_step_counter += 5;
        return 0;
    case 2:
        sceKernelMemset32(g_impose->fb_buf_uncached + ((base_y - step + 140) * g_impose->fb_stride + 80),
                          IMPOSE_CLEAR_COLOR_WORD, 640u);
        sceKernelMemset32(g_impose->fb_buf_uncached + ((base_y + step + 131) * g_impose->fb_stride + 80),
                          IMPOSE_CLEAR_COLOR_WORD, 640u);
        sceKernelMemset32(g_impose->fb_buf_uncached + ((base_y - step + 135) * g_impose->fb_stride + 80),
                          IMPOSE_LINE_COLOR_WORD, 640u);
        sceKernelMemset32(g_impose->fb_buf_uncached + ((base_y + step + 136) * g_impose->fb_stride + 80),
                          IMPOSE_LINE_COLOR_WORD, 640u);
        g_impose->anim_step_counter += 5;
        if (g_impose->anim_step_counter >= 39) {
            g_impose->anim_phase++;
        }
        return 0;
    case 3:
        sceKernelMemset32(g_impose->fb_buf_uncached + ((base_y - step + 140) * g_impose->fb_stride + 80),
                          IMPOSE_CLEAR_COLOR_WORD, 640u);
        sceKernelMemset32(g_impose->fb_buf_uncached + ((base_y + step + 131) * g_impose->fb_stride + 80),
                          IMPOSE_CLEAR_COLOR_WORD, 640u);
        g_impose->anim_step_counter = 0;
        g_impose->anim_phase++;
        if ((g_impose->osd_dirty_flags & 0x10u) == 0) {
            imposeBlitIcon16Wrapper(80, g_impose->dialog_y_offset + 96, 328, 9,
                                    g_impose->icon_sprites[IMPOSE_ICON_BORDER_LINE]);
            imposeBlitIcon16Wrapper(80, g_impose->dialog_y_offset + 175, 328, 9,
                                    g_impose->icon_sprites[IMPOSE_ICON_BORDER_LINE]);
            g_impose->osd_dirty_flags |= 0x10u;
        }
        return 0;
    case 4:
    default:
        if ((g_impose->osd_dirty_flags & 0x10u) == 0) {
            imposeBlitIcon16Wrapper(80, g_impose->dialog_y_offset + 96, 328, 9,
                                    g_impose->icon_sprites[IMPOSE_ICON_BORDER_LINE]);
            imposeBlitIcon16Wrapper(80, g_impose->dialog_y_offset + 175, 328, 9,
                                    g_impose->icon_sprites[IMPOSE_ICON_BORDER_LINE]);
            g_impose->osd_dirty_flags |= 0x10u;
        }
        return 1;
    }
}

static s32 imposeWriteRegistryInt(const char *category, const char *key, s32 val)
{
    SceRegParam reg;
    s32 reg_handle;
    s32 cat_handle;
    s32 write_val;

    write_val = val;
    memset(&reg, 0, sizeof(reg));
    reg.regtype = 1;
    strncpy(reg.name, "/system", 0xFF);
    reg.namelen = 7;
    reg.open_mode = 1;
    reg.sys_flag = 1;

    if (sceRegOpenRegistry(&reg, 2, &reg_handle) != 0) {
        return -1;
    }
    if (sceRegOpenCategory(reg_handle, category, 2, &cat_handle) != 0) {
        sceRegCloseRegistry(reg_handle);
        return -1;
    }
    if (sceRegSetKeyValue(cat_handle, key, &write_val, 4) != 0) {
        sceRegCloseCategory(cat_handle);
        sceRegCloseRegistry(reg_handle);
        return -1;
    }
    sceRegFlushCategory(cat_handle);
    sceRegCloseCategory(cat_handle);
    sceRegFlushRegistry(reg_handle);
    sceRegCloseRegistry(reg_handle);
    return 0;
}

static void imposeUpdateStatusFlags(s32 set, u32 mask)
{
    s32 intr = sceKernelCpuSuspendIntr();
    if (set != 0) {
        g_impose->popup_disable_flags |= mask;
    } else {
        g_impose->popup_disable_flags &= ~mask;
    }
    sceKernelCpuResumeIntr(intr);
}

static s32 imposeInitAudioCallback(void *opt_in, s32 stage, void *opt_out)
{
    (void)opt_in;
    (void)stage;
    (void)opt_out;
    imposeSyncAudioVolume();
    g_impose->init_state = 0;
    return 0;
}

static s32 imposeSetAudioMode(u32 mode)
{
    if (mode >= 4u) {
        return (s32)0x802C01FE;
    }
    s32 prev = (s32)mode;
    if ((u32)g_impose->audio_routing_mode != mode) {
        s32 intr = sceKernelCpuSuspendIntr();
        prev = g_impose->audio_routing_mode;
        g_impose->audio_routing_changed = 1;
        g_impose->audio_routing_mode = (s32)mode;
        imposeSetEventFlag(0x20u);
        sceKernelCpuResumeIntr(intr);
    }
    return prev;
}

static s32 imposeCalcEffectiveVolume(s32 mode)
{
    s32 eff_vol;
    if (mode == 0) {
        eff_vol = (g_impose->mute != 0) ? 0 : g_impose->main_volume;
    } else if (mode == 1) {
        eff_vol = 30;
    } else {
        return (s32)0x802C01FE;
    }
    s32 intr = sceKernelCpuSuspendIntr();
    g_impose->effective_volume = eff_vol;
    g_impose->volume_mode = mode;
    g_impose->special_btn_pending = 0;
    imposeSetEventFlag(0x400u);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

static s32 imposeApplyHeadphoneVolume(void)
{
    g_impose->special_btn_pending &= ~0x00400000u;
    if (g_impose->display_caps == 0 || g_impose->tvout_allowed == 0 || g_impose->tvout_blocked == 1) {
        return -1;
    }
    if (sceImpose_driver_E9B0E008(NULL) < 0) {
        return -1;
    }
    g_impose->tvout_active = 2;
    g_impose->pending_change_flags |= 0x200u;
    if (g_impose->tvout_switch_from_off != 0) {
        imposeRestoreButtonIntercept();
        imposeStepBrightness(0);
        imposeNoopCallback(2, 1);
        g_impose->screen_hold_state = 0;
        g_impose->pending_change_flags |= 0x20000000u;
    }
    g_impose->tvout_switch_from_off = 0;
    if (!imposeIsMenuAllowed()) {
        sceImpose_driver_BB12F974(0x40402u);
    }
    return 0;
}

static s32 imposeApplySpeakerVolume(void)
{
    g_impose->special_btn_pending &= ~0x00400000u;
    if (g_impose->display_caps == 0) {
        return -1;
    }
    if (g_impose->tvout_allowed == 1) {
        if (!imposeIsMenuAllowed()) {
            imposeSetTvOutMode(0, g_tvout_mode->lcd_mode, g_tvout_mode->width, g_tvout_mode->height);
        }
        g_impose->pending_change_flags |= 0x200u;
    }
    return 0;
}

static void imposeHandleVolumeButtons(u32 trig, u32 cur_buttons, u32 prev_buttons)
{
    s32 woke_from_off = 0;
    s32 hold_state = g_impose->screen_hold_state;

    (void)trig;

    if (hold_state != 0) {
        if ((cur_buttons & 0x00F1FFFFu) == 0) {
            if (hold_state == 1) {
                woke_from_off = 1;
                imposeRestoreButtonIntercept();
                g_impose->screen_hold_state = 0;
            } else if (hold_state == 2) {
                g_impose->screen_hold_state = 3;
            }
        } else if (hold_state == 3) {
            imposeStepBrightness(0);
            imposeNoopCallback(2, 1);
            g_impose->screen_hold_state = 1;
            g_impose->pending_change_flags |= 0x20000000u;
        }
    }

    if ((g_impose->cur_hprm_key & 0x10u) != 0) {
        cur_buttons |= 0x00100000u;
    }
    if ((g_impose->cur_hprm_key & 0x20u) != 0) {
        cur_buttons |= 0x00200000u;
    }

    if ((g_impose->special_btn_pending & 0x00300000u) == 0x00300000u) {
        if (g_impose->mute != 0) {
            g_impose->osd_dirty_flags &= ~0x21u;
            g_impose->pending_change_flags |= 8u;
            g_impose->mute = 0;
            imposeSetEventFlag(4u);
        }
        g_impose->special_btn_pending &= ~0x00300000u;
    }

    if ((g_impose->special_btn_pending & 0x00200000u) != 0) {
        if (g_impose->mute != 0) {
            g_impose->osd_dirty_flags &= ~0x21u;
            g_impose->special_btn_pending &= ~0x00200000u;
            g_impose->pending_change_flags |= 8u;
            g_impose->mute = 0;
            imposeSetEventFlag(4u);
        } else if (g_impose->volume_disabled == 1) {
            g_impose->special_btn_pending &= ~0x00200000u;
            g_impose->pending_change_flags |= 1u;
        } else if ((cur_buttons & 0x00200000u) == 0) {
            if ((cur_buttons & 0x00020000u) == 0 || (prev_buttons & 0x00200000u) == 0) {
                g_impose->main_volume--;
                if (g_impose->main_volume < 0) {
                    g_impose->main_volume = 0;
                }
                g_impose->special_btn_pending &= ~0x00200000u;
                imposeSetEventFlag(4u);
            } else {
                g_impose->special_btn_pending &= ~0x00200000u;
            }
        } else if (g_impose->vol_hold_ticks >= 61) {
            g_impose->main_volume--;
            if (g_impose->main_volume < 0) {
                g_impose->main_volume = 0;
            }
            g_impose->vol_hold_ticks = 55;
            g_impose->osd_frame_counter = 0;
            imposeSetEventFlag(4u);
        }
    }

    if ((g_impose->special_btn_pending & 0x00100000u) != 0) {
        s32 max_vol = (g_impose->avls == 0) ? 30 : 20;
        if (g_impose->mute != 0) {
            g_impose->osd_dirty_flags &= ~0x21u;
            g_impose->special_btn_pending &= ~0x00100000u;
            g_impose->pending_change_flags |= 8u;
            g_impose->mute = 0;
            imposeSetEventFlag(4u);
        } else if (g_impose->volume_disabled == 1) {
            g_impose->special_btn_pending &= ~0x00100000u;
            g_impose->pending_change_flags |= 1u;
        } else if ((cur_buttons & 0x00100000u) == 0) {
            if ((cur_buttons & 0x00020000u) == 0 || (prev_buttons & 0x00100000u) == 0) {
                g_impose->main_volume++;
                if (g_impose->main_volume > max_vol) {
                    g_impose->main_volume = max_vol;
                    if (g_impose->avls_blink_counter == 0) {
                        g_impose->avls_blink_counter = 1;
                    }
                    if (g_impose->avls != 0) {
                        g_impose->osd_dirty_flags &= ~0x40u;
                        g_impose->pending_change_flags |= 0x40000000u;
                    }
                }
                g_impose->special_btn_pending &= ~0x00100000u;
                imposeSetEventFlag(4u);
            } else {
                g_impose->special_btn_pending &= ~0x00100000u;
            }
        } else if (g_impose->vol_hold_ticks >= 61) {
            g_impose->main_volume++;
            if (g_impose->main_volume > max_vol) {
                g_impose->main_volume = max_vol;
                if (g_impose->avls_blink_counter == 0) {
                    g_impose->avls_blink_counter = 1;
                }
                if (g_impose->avls != 0) {
                    g_impose->osd_dirty_flags &= ~0x40u;
                    g_impose->pending_change_flags |= 0x40000000u;
                }
            }
            g_impose->vol_hold_ticks = 55;
            g_impose->osd_frame_counter = 0;
            imposeSetEventFlag(4u);
        }
    }

    if ((g_impose->special_btn_pending & 0x00800000u) != 0) {
        if (g_impose->mute != 0) {
            g_impose->special_btn_pending &= ~0x00800000u;
            g_impose->pending_change_flags |= 8u;
            g_impose->osd_dirty_flags &= ~0x21u;
            g_impose->mute = 0;
            imposeSetEventFlag(4u);
        } else if (g_impose->volume_disabled == 1) {
            g_impose->special_btn_pending &= ~0x00800000u;
        } else if ((cur_buttons & 0x00800000u) == 0) {
            s32 allow_eq = 1;
            if ((cur_buttons & 0x00020000u) != 0) {
                allow_eq = 0;
            } else if (g_impose->hprm_status == 0x10 && g_impose->hprm_headphone_exist == 0 && g_impose->hprm_remote_exist == 0) {
                allow_eq = 0;
            } else if (g_impose->tvout_cable_type != 0 || g_impose->headphone_connected == 0) {
                allow_eq = 0;
            }
            if (g_impose->equalizer_enabled == 0 || (g_impose->home_button_mode & 0x10u) != 0) {
                allow_eq = 0;
            }
            if (allow_eq != 0) {
                g_impose->equalizer_mode++;
                if (g_impose->equalizer_mode >= 5) {
                    g_impose->equalizer_mode = 0;
                }
                g_impose->osd_dirty_flags &= ~1u;
                imposeSetEventFlag(0x40u);
            }
            g_impose->special_btn_pending &= ~0x00800000u;
        } else if (g_impose->note_hold_ticks >= 61) {
            g_impose->special_btn_pending &= ~0x00800000u;
            g_impose->mute = 1;
            g_impose->pending_change_flags |= 8u;
            g_impose->osd_dirty_flags &= ~0x21u;
        }
    }

    if ((g_impose->special_btn_pending & 0x00400000u) == 0) {
        return;
    }

    if (g_impose->screen_hold_state == 0) {
        if (g_impose->init_state != 0) {
            g_impose->special_btn_pending &= ~0x00400000u;
            return;
        }
        if (g_impose->tvout_active == 1) {
            g_impose->special_btn_pending &= ~0x00400000u;
            imposeRequestVolumeUpdate(1);
            return;
        }
        if (g_impose->tvout_active == 2 || woke_from_off != 0) {
            g_impose->special_btn_pending &= ~0x00400000u;
            return;
        }
        if ((cur_buttons & 0x00400000u) == 0) {
            if ((cur_buttons & 0x00020000u) == 0 && imposeBrightnessToLevel(g_impose->brightness_max_limit) > 0) {
                g_impose->display_off_state = 2;
                imposeStepBrightness(0);
            }
            g_impose->special_btn_pending &= ~0x00400000u;
            return;
        }
        if (g_impose->screen_hold_ticks < 61) {
            return;
        }
        imposeEnableButtonIntercept(0);
        imposeStepBrightness(2);
        imposeNoopCallback(2, 0);
        g_impose->screen_hold_state = 2;
        g_impose->pending_change_flags |= 0x20000000u;
        if (g_impose->display_caps == 0) {
            g_impose->special_btn_pending &= ~0x00400000u;
            return;
        }
        if (g_impose->tvout_allowed == 0 || g_impose->tvout_blocked == 1) {
            g_impose->special_btn_pending &= ~0x00400000u;
        }
        if (sceImpose_driver_E9B0E008(NULL) < 0) {
            g_impose->special_btn_pending &= ~0x00400000u;
        }
    } else if (g_impose->display_caps != 0) {
        if (g_impose->screen_hold_state == 1) {
            g_impose->special_btn_pending &= ~0x00400000u;
        } else if (g_impose->screen_hold_ticks >= 181 && (cur_buttons & 0x00400000u) != 0) {
            g_impose->tvout_switch_from_off = 1;
            g_impose->special_btn_pending &= ~0x00400000u;
            imposeRequestVolumeUpdate(2);
        }
    }
}

static void imposeClearAllOverlays(void)
{
    if ((g_impose->menu_state & 1u) != 0 || g_impose->osd_visible != 0) {
        return;
    }
    imposeClearOsdRegion(0xFFu);
    s32 intr = sceKernelCpuSuspendIntr();
    sceDisplay_driver_3E17FE8D(0, g_impose->fb_buf_uncached, 512, 1, 1);
    g_impose->dialog_selection = 0;
    g_impose->osd_visible = 1;
    g_impose->osd_dirty_flags = 0;
    g_impose->anim_phase = 0;
    g_impose->osd_frame_counter = 0;
    g_impose->anim_step_counter = 0;
    g_impose->cur_buttons = 0;
    g_impose->dialog_wait_release = 1;
    if (g_impose->popup_banner_mode == 2) {
        g_impose->dialog_selection = 1;
    }
    sceKernelCpuResumeIntr(intr);
}

static s32 imposeHandlePowerEvent(s32 vol_target, s32 allow_notify)
{
    s32 unmuted = 0;
    s32 external_audio = 0;

    if (g_impose->hprm_status == 0x10) {
        if (vol_target == 2 && g_impose->tvout_cable_type > 0 && g_impose->tvout_cable_type < 3) {
            external_audio = 1;
        }
    } else if (g_impose->tvout_cable_type > 0 && g_impose->tvout_cable_type < 3) {
        external_audio = 1;
    }

    if (external_audio != 0) {
        if (g_impose->equalizer_enabled != 0 &&
            g_impose->equalizer_mode != 0 &&
            g_impose->headphone_connected != 0 &&
            g_impose->suppress_notify == 0 &&
            (g_impose->home_button_mode & 0x10u) == 0 &&
            allow_notify != 0) {
            g_impose->pending_change_flags |= 4u;
        }
        g_impose->volume_disabled = 1;
        g_impose->pending_change_flags |= 0x400u;
        if (g_impose->mute != 0) {
            g_impose->mute = 0;
            g_impose->pending_change_flags |= 0x408u;
            unmuted = 1;
        }
    } else {
        g_impose->volume_disabled = g_impose->sound_reduction;
        g_impose->pending_change_flags |= 0x400u;
        if ((g_impose->display_caps & 0x02000000u) == 0) {
            if (g_impose->tvout_active == 1) {
                if (!imposeIsMenuAllowed()) {
                    imposeSetTvOutMode(1, g_tvout_mode->lcd_mode, g_tvout_mode->width, g_tvout_mode->height);
                }
                g_impose->pending_change_flags |= 0x200u;
            } else if (g_impose->tvout_active == 2) {
                g_impose->tvout_active = 0;
            }
        }
        imposeSetVolumeTarget(1);
    }

    g_impose->osd_dirty_flags &= ~0x27u;
    return unmuted;
}

static s32 imposeRefreshHprmDevices(s32 vol_target, s32 allow_notify)
{
    (void)vol_target;
    g_impose->hprm_headphone_exist = sceHprm_driver_FA4A25A7();
    g_impose->hprm_remote_exist = sceHprm_driver_EFCFD0C5();
    g_impose->hprm_hp_present = sceHprm_driver_370DDC12();
    if ((g_impose->hprm_caps & 2u) != 0) {
        g_impose->hprm_mic_present = sceHprm_driver_1C3525DD();
    }
    if (g_impose->equalizer_enabled != 0 &&
        g_impose->volume_disabled != 1 &&
        g_impose->suppress_notify == 0 &&
        g_impose->audio_routing_changed == 0 &&
        (g_impose->home_button_mode & 0x10u) == 0 &&
        allow_notify != 0 &&
        g_impose->equalizer_mode != 0) {
        g_impose->pending_change_flags |= 4u;
    }
    imposeUpdateAudioRouting();
    g_impose->audio_routing_changed = 0;
    return 0;
}

static s32 imposeUpdateHprmOnEvent(void)
{
    g_impose->hprm_headphone_exist = sceHprm_driver_FA4A25A7();
    g_impose->hprm_remote_exist = sceHprm_driver_EFCFD0C5();
    imposeUpdateAudioRouting();
    return 0;
}

static s32 imposeReadRegistrySetting(u32 id)
{
    s32 val = 0;
    const char *cat = NULL;
    const char *key = NULL;

    switch (id) {
    case 0:  val = 512; break;
    case 1:  val = 272; break;
    case 2:  cat = "/CONFIG/SYSTEM/XMB";          key = "language"; break;
    case 3:  cat = "/CONFIG/SYSTEM/SOUND";        key = "main_volume"; break;
    case 4:  cat = "/CONFIG/SYSTEM/SOUND";        key = "mute"; break;
    case 5:  cat = "/CONFIG/SYSTEM/SOUND";        key = "avls"; break;
    case 6:  cat = "/CONFIG/SYSTEM/SOUND";        key = "equalizer_mode"; break;
    case 7:  cat = "/CONFIG/SYSTEM";              key = "backlight_brightness"; break;
    case 8:  cat = "/CONFIG/DATE";                key = "time_format"; break;
    case 9:  cat = "/CONFIG/DATE";                key = "date_format"; break;
    case 10: cat = "/CONFIG/DATE";                key = "summer_time"; break;
    case 11: cat = "/CONFIG/DATE";                key = "time_zone_offset"; break;
    case 12: cat = "/CONFIG/SYSTEM/POWER_SAVING"; key = "suspend_interval"; break;
    case 13: cat = "/CONFIG/SYSTEM/POWER_SAVING"; key = "backlight_off_interval"; break;
    case 14: cat = "/CONFIG/SYSTEM/XMB";          key = "button_assign"; break;
    case 15: cat = "/SYSPROFILE";                 key = "sound_reduction"; break;
    case 16: cat = "/CONFIG/DISPLAY";             key = "aspect_ratio"; break;
    case 17: cat = "/CONFIG/DISPLAY";             key = "scan_mode"; break;
    case 18: cat = "/CONFIG/DISPLAY";             key = "screensaver_start_time"; break;
    case 19: cat = "/CONFIG/SYSTEM/CHARACTER_SET"; key = "oem"; break;
    case 20: cat = "/CONFIG/DISPLAY";             key = "pi_blending_mode"; break;
    case 21: cat = "/CONFIG/DISPLAY";             key = "color_space_mode"; break;
    case 22: cat = "/CONFIG/SYSTEM/POWER_SAVING"; key = "active_backlight_mode"; break;
    default: val = -1; break;
    }

    if (val != 0) {
        return val;
    }
    if (imposeReadRegistryInt(cat, key, &val) == 0) {
        return val;
    }
    return -1;
}

static s32 imposeFormatTimeString(const SceImposeDateTime *dt, s32 x, s32 y, s32 align)
{
    s32 glyphs[16];
    s32 len = 0;

    if (g_impose->date_format == 2) {
        u16 day = dt->day;
        if (day / 10u != 0) {
            glyphs[len++] = day / 10u;
        }
        glyphs[len++] = day % 10u;
        glyphs[len++] = '/';
        u16 month = dt->month;
        if (month / 10u != 0) {
            glyphs[len++] = month / 10u;
        }
        glyphs[len++] = month % 10u;
    } else {
        u16 month = dt->month;
        if (month / 10u != 0) {
            glyphs[len++] = month / 10u;
        }
        glyphs[len++] = month % 10u;
        glyphs[len++] = '/';
        u16 day = dt->day;
        if (day / 10u != 0) {
            glyphs[len++] = day / 10u;
        }
        glyphs[len++] = day % 10u;
    }

    glyphs[len++] = ' ';

    if (g_impose->time_format == 1) {
        s32 hour = dt->hour;
        s32 is_pm = 0;
        if ((u32)hour >= 12u) {
            hour -= 12;
            is_pm = 1;
        }
        if (hour == 0) {
            hour = 12;
        }
        if (hour / 10 != 0) {
            glyphs[len++] = hour / 10;
        }
        glyphs[len++] = hour % 10;
        glyphs[len++] = ':';
        u16 minute = dt->minute;
        glyphs[len++] = minute / 10u;
        glyphs[len++] = minute % 10u;
        glyphs[len++] = '!';
        glyphs[len++] = 'A' + is_pm;
    } else {
        u16 hour = dt->hour;
        if (hour / 10u != 0) {
            glyphs[len++] = hour / 10u;
        }
        glyphs[len++] = hour % 10u;
        glyphs[len++] = ':';
        u16 minute = dt->minute;
        glyphs[len++] = minute / 10u;
        glyphs[len++] = minute % 10u;
    }

    glyphs[len++] = ' ';
    glyphs[len++] = 255;

    s32 total_width = 0;
    s32 idx;
    for (idx = 0; idx < 16; idx++) {
        s32 ch = glyphs[idx];
        if (ch == 0 || (u32)(ch - 3) < 7u) {
            total_width += 12;
        } else if (ch == 1) {
            total_width += 10;
        } else if (ch == 2) {
            total_width += 11;
        } else if (ch == '/') {
            total_width += 7;
        } else if (ch == ':') {
            total_width += 6;
        } else if (ch == 'A' || ch == 'B') {
            total_width += 29;
        } else if (ch == ' ') {
            total_width += 5;
        } else if (ch == '!') {
            total_width += 1;
        } else if (ch == 255) {
            break;
        }
    }

    s32 cur_x;
    if (align == 0) {
        cur_x = x - total_width;
    } else if (align == 1) {
        cur_x = x;
    } else {
        cur_x = (480 - total_width) / 2 + x;
    }
    s32 start_x = cur_x;

    for (idx = 0; idx < len; idx++) {
        s32 ch = glyphs[idx];
        if ((u32)ch < 10u) {
            s32 odd = cur_x & 1;
            const u8 *sprite = g_impose->icon_sprites[odd ? IMPOSE_ICON_DIGIT_ODD : IMPOSE_ICON_DIGIT_EVEN] + 480 * ch;
            imposeBlitIcon16Wrapper(cur_x - odd, y, 16, 15, sprite);
            if (ch == 1) {
                cur_x += 10;
            } else if (ch == 2) {
                cur_x += 11;
            } else {
                cur_x += 12;
            }
        } else if (ch == '/' || ch == ':') {
            s32 odd = cur_x & 1;
            s32 sprite_off = (ch == ':') ? 360 : 0;
            const u32 *src = (const u32 *)(g_impose->icon_sprites[odd ? IMPOSE_ICON_PUNCT_ODD : IMPOSE_ICON_PUNCT_EVEN] + sprite_off);
            u32 *dst = (u32 *)(g_impose->fb_buf_uncached + (g_impose->fb_stride * y + (cur_x - odd)));
            s32 row, col;
            for (row = 0; row < 15; row++) {
                for (col = 0; col < 3; col++) {
                    dst[0] = src[0];
                    dst[1] = src[1];
                    dst += 2;
                    src += 2;
                }
                dst += ((g_impose->fb_stride - 12) / 4) * 2;
            }
            cur_x += (ch == ':') ? 6 : 7;
        } else if (ch == 'A' || ch == 'B') {
            s32 odd = cur_x & 1;
            s32 sprite_off = (ch == 'B') ? 1024 : 0;
            const u8 *sprite = g_impose->icon_sprites[odd ? IMPOSE_ICON_AMPM_ODD : IMPOSE_ICON_AMPM_EVEN] + sprite_off;
            imposeBlitIcon16Wrapper(cur_x - odd, y, 32, 16, sprite);
            cur_x += 29;
        } else if (ch == ' ') {
            cur_x += 5;
        } else if (ch == '!') {
            cur_x += 1;
        } else if (ch == 255) {
            break;
        }
    }

    return start_x;
}

static void imposeDrawBatteryLevel(void)
{
    if ((g_impose->osd_dirty_flags & 0x80u) != 0) {
        return;
    }

    s32 lvl = 3;
    if (g_impose->battery_low == 0) {
        s32 pct = g_impose->battery_percent;
        s32 idx;
        lvl = 0;
        for (idx = 0; idx < 3; idx++) {
            if (pct >= g_battery_thresholds[idx]) {
                break;
            }
            lvl++;
        }
    }

    s32 frame = (g_impose->battery_charging != 0) ? g_impose->battery_anim_frame : lvl;
    const u32 *src = (const u32 *)(g_impose->icon_sprites[IMPOSE_ICON_BATTERY] + 952 * frame);
    u32 *dst = (u32 *)(g_impose->fb_buf_uncached + (5 * g_impose->fb_stride + 450));
    s32 row, col;
    for (row = 0; row < 17; row++) {
        for (col = 0; col < 7; col++) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst += 2;
            src += 2;
        }
        dst += ((g_impose->fb_stride - 28) / 4) * 2;
    }

    if (g_impose->battery_charging == 0) {
        g_impose->osd_dirty_flags |= 0x80u;
    }
}

static s32 imposeReadRegistryInt(const char *category, const char *key, s32 *out)
{
    SceRegParam reg;
    s32 reg_handle;
    s32 cat_handle;
    s32 key_handle;
    u32 key_type;
    u32 key_size;
    s32 key_val;

    memset(&reg, 0, sizeof(reg));
    reg.regtype = 1;
    strncpy(reg.name, "/system", 0xFF);
    reg.namelen = 7;
    reg.open_mode = 1;
    reg.sys_flag = 1;

    if (sceRegOpenRegistry(&reg, 2, &reg_handle) != 0) {
        return -1;
    }
    if (sceRegOpenCategory(reg_handle, category, 2, &cat_handle) != 0) {
        sceRegCloseRegistry(reg_handle);
        return -1;
    }
    if (sceRegGetKeyInfo(cat_handle, key, &key_handle, &key_type, &key_size) != 0 ||
        sceRegGetKeyValue(cat_handle, key_handle, &key_val, 4) != 0) {
        sceRegCloseCategory(cat_handle);
        sceRegCloseRegistry(reg_handle);
        return -1;
    }
    sceRegFlushCategory(cat_handle);
    sceRegCloseCategory(cat_handle);
    sceRegFlushRegistry(reg_handle);
    sceRegCloseRegistry(reg_handle);
    *out = key_val;
    return 0;
}

static void imposeUpdateAudioRouting(void)
{
    s32 route = g_impose->audio_routing_mode;
    s32 speaker = 0;
    s32 headphone = 0;

    if (route == 0) {
        speaker = 0;
        headphone = 0;
    } else if (route == 1) {
        speaker = 1;
        headphone = 1;
    } else if (route == 2) {
        speaker = 0;
        headphone = 1;
    } else if (route == 3) {
        speaker = 1;
        headphone = 0;
    } else {
        return;
    }
    sceKermitPeripheral_driver_76BE2E28(speaker, headphone, -1);
}

/* =========================================================================
 * Exported Functions: sceImpose & sceImpose_driver
 * ========================================================================= */

s32 sceImposeGetHomePopup(void)
{
    s32 intr = sceKernelCpuSuspendIntr();
    s32 res = (s32)((g_impose->popup_disable_flags ^ 1u) & 1u);
    sceKernelCpuResumeIntr(intr);
    return res;
}

s32 sceImpose_driver_E9A42056(void) __attribute__((alias("sceImposeGetHomePopup")));

s32 sceImposeSetHomePopup(s32 val)
{
    imposeUpdateStatusFlags(val ^ 1, 1u);
    return 0;
}

s32 sceImposeGetUMDPopup(void)
{
    s32 intr = sceKernelCpuSuspendIntr();
    s32 res = (s32)(((g_impose->popup_disable_flags ^ 2u) >> 1) & 1u);
    sceKernelCpuResumeIntr(intr);
    return res;
}

s32 sceImpose_driver_EF4D1BC5(void) __attribute__((alias("sceImposeGetUMDPopup")));

s32 sceImposeSetUMDPopup(s32 val)
{
    imposeUpdateStatusFlags(val ^ 1, 2u);
    return 0;
}

s32 sceImposeGetLanguageMode(s32 *lang, s32 *button)
{
    u32 old_k1 = pspGetK1();
    u32 shifted = old_k1 << 11;

    if (((s32)(shifted & (u32)lang) < 0) || ((s32)(shifted & (u32)button) < 0)) {
        pspSetK1(old_k1);
        return (s32)0x80000023;
    }
    if (lang != NULL) {
        *lang = g_impose->pending_language;
    }
    if (button != NULL) {
        *button = g_impose->button_assign;
    }
    return 0;
}

s32 sceImpose_driver_24FD7BCF(s32 *lang, s32 *button) __attribute__((alias("sceImposeGetLanguageMode")));

s32 sceImposeSetLanguageMode(s32 lang, s32 button)
{
    if ((u32)lang >= 12u) {
        lang = g_impose->default_language;
    }
    if ((u32)button >= 2u) {
        return (s32)0x800001FE;
    }
    if (g_impose->lang_change_pending == 1) {
        if (g_impose->pending_language != lang || g_impose->pending_button_assign != button) {
            g_impose->lang_change_pending = 1;
            g_impose->pending_language = lang;
            g_impose->pending_button_assign = button;
        }
    } else {
        if (g_impose->language != lang || g_impose->button_assign != button) {
            g_impose->lang_change_pending = 1;
            g_impose->pending_language = lang;
            g_impose->pending_button_assign = button;
        }
    }
    return 0;
}

s32 sceImposeGetBacklightOffTime(void)
{
    s32 intr = sceKernelCpuSuspendIntr();
    s32 res = sceImpose_133636DD((s32)0x80000002);
    sceKernelCpuResumeIntr(intr);
    return res;
}

s32 sceImpose_driver_E1A76241(void) __attribute__((alias("sceImposeGetBacklightOffTime")));

s32 sceImposeSetBacklightOffTime(s32 val)
{
    if (val < 30) {
        return (s32)0x800001FE;
    }
    s32 intr = sceKernelCpuSuspendIntr();
    s32 res = sceImpose_FCD44963((s32)0x80000002, (u32)val);
    sceKernelCpuResumeIntr(intr);
    return res;
}

s32 sceImpose_driver_37F3F1D2(s32 val) __attribute__((alias("sceImposeSetBacklightOffTime")));

s32 sceImpose_133636DD(s32 id)
{
    switch ((u32)id) {
    case 1u:
        return (g_impose->mute == 0) ? g_impose->main_volume : 0;
    case 2u:
        if (g_impose->brightness_dimmed == 0) {
            return g_impose->brightness_level;
        }
        return (g_impose->display_off_state != 0) ? 4 : 0;
    case 4u:
        if (g_impose->volume_disabled == 1) {
            return 0;
        }
        if (g_impose->hprm_status == 0x10 && g_impose->hprm_headphone_exist == 0 && g_impose->hprm_remote_exist == 0) {
            return 0;
        }
        if (g_impose->tvout_cable_type == 0 && g_impose->headphone_connected == 1) {
            return g_impose->equalizer_mode;
        }
        return 0;
    case 8u:
        return g_impose->mute;
    case 0x10u:
        return g_impose->avls;
    case 0x20u:
        return g_impose->time_format;
    case 0x40u:
        return g_impose->date_format;
    case 0x80u:
        return g_impose->language;
    case 0x100u:
        return g_impose->equalizer_enabled;
    case 0x200u:
        return g_impose->tvout_active;
    case 0x400u:
        return (g_impose->volume_disabled == 1) ? 1 : 0;
    case 0x2000u:
        return g_impose->popup_banner_mode;
    case 0x20000000u:
        if (g_impose->tvout_active != 0) {
            return g_impose->display_off_state;
        }
        return g_impose->brightness_dimmed;
    case 0x80000002u:
        return g_impose->backlight_off_interval;
    case 0x80000003u:
        return g_impose->sound_reduction;
    case 0x80000004u:
        return g_impose->aspect_ratio;
    case 0x80000005u:
        return g_impose->scan_mode;
    case 0x80000006u:
        return g_impose->tvout_allowed;
    case 0x80000007u:
        return g_impose->tvout_keep_on_reset;
    case 0x80000008u:
        return g_impose->tvout_blocked;
    case 0x80000009u:
        return g_impose->screensaver_start_time;
    case 0x8000000Au:
        return g_impose->pi_blending_mode;
    case 0x8000000Bu:
        return g_impose->wlan_idle_timeout;
    default:
        return (s32)0x80000107;
    }
}

s32 sceImpose_driver_DC3BECFF(s32 id) __attribute__((alias("sceImpose_133636DD")));

s32 sceImpose_FCD44963(s32 id, u32 val)
{
    switch ((u32)id) {
    case 1u:
        if (val >= 31u) {
            return (s32)0x800001FE;
        }
        if (g_impose->volume_mode != 0) {
            return 0;
        }
        {
            s32 max_vol = (g_impose->avls == 0) ? 30 : 20;
            g_impose->main_volume = ((s32)val > max_vol) ? max_vol : (s32)val;
            if ((g_impose->menu_state & 2u) != 0) {
                g_impose->menu_state |= 0x10000u;
                g_impose->osd_frame_counter = 0;
            }
            imposeSetEventFlag(4u);
            imposeApplyRegistrySetting(3u);
        }
        return 0;
    case 2u:
        if (val >= 5u) {
            return (s32)0x800001FE;
        }
        if (val == 4u) {
            if ((u32)g_impose->brightness_level >= 4u) {
                g_impose->brightness_level = (g_impose->ac_online != 0) ? 3 : 2;
            }
            g_impose->cur_brightness = 0;
            imposeBrightnessToBacklightSel(0);
        } else {
            g_impose->brightness_dimmed = 1;
            g_impose->brightness_level = (s32)val;
            g_impose->cur_brightness = 0;
            imposeStepBrightness(0);
        }
        imposeApplyRegistrySetting(7u);
        return 0;
    case 4u:
        if (val >= 5u) {
            return (s32)0x800001FE;
        }
        g_impose->equalizer_mode = (s32)val;
        imposeSetEventFlag(0x40u);
        imposeApplyRegistrySetting(6u);
        return 0;
    case 8u:
        if (val >= 2u) {
            return (s32)0x800001FE;
        }
        g_impose->mute = (s32)val;
        g_impose->pending_change_flags |= 9u;
        imposeSetEventFlag(4u);
        imposeApplyRegistrySetting(4u);
        return 0;
    case 0x10u:
        if (val >= 2u) {
            return (s32)0x800001FE;
        }
        g_impose->avls = (s32)val;
        if (val != 0 && g_impose->main_volume >= 21) {
            g_impose->main_volume = 20;
            g_impose->pending_change_flags |= 1u;
        }
        g_impose->pending_change_flags |= 0x10u;
        imposeSetEventFlag(4u);
        return 0;
    case 0x20u:
        if (val >= 2u) {
            return (s32)0x800001FE;
        }
        g_impose->time_format = (s32)val;
        g_impose->pending_change_flags |= 0x20u;
        return 0;
    case 0x40u:
        if (val >= 3u) {
            return (s32)0x800001FE;
        }
        g_impose->date_format = (s32)val;
        g_impose->pending_change_flags |= 0x40u;
        return 0;
    case 0x80u:
        if (val >= 12u) {
            return (s32)0x800001FE;
        }
        g_impose->language = (s32)val;
        g_impose->pending_change_flags |= 0x80u;
        return 0;
    case 0x100u:
        if (val >= 2u) {
            return (s32)0x800001FE;
        }
        g_impose->equalizer_enabled = (s32)val;
        g_impose->pending_change_flags |= 0x100u;
        return 0;
    case 0x80000001u:
        if (val >= 901u) {
            return (s32)0x800001FE;
        }
        g_impose->suspend_interval = (s32)val;
        imposeNoopCallback(1, 1);
        return 0;
    case 0x80000002u:
        if (val >= 601u) {
            return (s32)0x800001FE;
        }
        g_impose->backlight_off_interval = (s32)val;
        if (g_impose->tvout_active != 1) {
            imposeNoopCallback(2, 1);
        }
        return 0;
    case 0x80000003u:
        if (val >= 2u) {
            return (s32)0x800001FE;
        }
        g_impose->sound_reduction = (s32)val;
        return 0;
    case 0x80000004u:
        if (val >= 2u) {
            return (s32)0x800001FE;
        }
        g_impose->aspect_ratio = (s32)val;
        return 0;
    case 0x80000005u:
        if (val >= 2u) {
            return (s32)0x800001FE;
        }
        g_impose->scan_mode = (s32)val;
        return 0;
    case 0x80000006u:
        if (val >= 2u) {
            return (s32)0x800001FE;
        }
        g_impose->tvout_allowed = (s32)val;
        if (val == 0) {
            g_impose->special_btn_pending &= ~0x00400000u;
        }
        return 0;
    case 0x80000007u:
        g_impose->tvout_keep_on_reset = (s32)val;
        return 0;
    case 0x80000008u:
        g_impose->tvout_blocked = (s32)val;
        if (val == 1u) {
            imposeResetTvOutState();
        }
        return 0;
    case 0x80000009u:
        if (val >= 901u) {
            return (s32)0x800001FE;
        }
        g_impose->screensaver_start_time = (s32)val;
        if (g_impose->tvout_active != 0) {
            imposeNoopCallback(4, 1);
        }
        return 0;
    case 0x8000000Au:
        if (val >= 2u) {
            return (s32)0x800001FE;
        }
        if ((u32)g_impose->pi_blending_mode != val) {
            sceDisplay_driver_7DF044BA((s32)val);
        }
        g_impose->pi_blending_mode = (s32)val;
        return 0;
    case 0x8000000Bu:
        if (val >= 7201u) {
            return (s32)0x800001FE;
        }
        g_impose->wlan_idle_timeout = (s32)val;
        imposeNoopCallback(8, 1);
        return 0;
    default:
        return (s32)0x80000107;
    }
}

s32 sceImpose_driver_3C318569(s32 id, u32 val) __attribute__((alias("sceImpose_FCD44963")));

s32 sceImpose_8C943191(s32 *is_charging, s32 *icon_status)
{
    u32 old_k1 = pspGetK1();
    u32 shifted = old_k1 << 11;
    pspSetK1(shifted);

    if (((s32)(shifted & (u32)is_charging) < 0) || ((s32)(shifted & (u32)icon_status) < 0)) {
        pspSetK1(old_k1);
        return (s32)0x80000023;
    }

    s32 intr = sceKernelCpuSuspendIntr();
    s32 charge_state = 0;
    s32 icon_lvl;
    if (g_impose->battery_exist != 0) {
        s32 idx = 3;
        if (g_impose->battery_low == 0) {
            s32 pct = g_impose->battery_percent;
            s32 step;
            idx = 0;
            for (step = 0; step < 3; step++) {
                if (pct >= g_battery_thresholds[step]) {
                    break;
                }
                idx++;
            }
        }
        icon_lvl = 3 - idx;
        if (g_impose->battery_charging != 0) {
            charge_state = 1;
            icon_lvl = 3 - g_impose->battery_anim_frame;
        }
    } else {
        charge_state = (s32)0x80000000u;
        icon_lvl = 0;
    }

    if (is_charging != NULL) {
        *is_charging = charge_state;
    }
    if (icon_status != NULL) {
        *icon_status = icon_lvl;
    }
    sceKernelCpuResumeIntr(intr);
    pspSetK1(old_k1);
    return 0;
}

s32 sceImpose_driver_FF2D7FD2(s32 *is_charging, s32 *icon_status) __attribute__((alias("sceImpose_8C943191")));

s32 sceImpose_381BD9E7(s32 open_flag)
{
    u32 old_k1 = pspGetK1();
    pspSetK1(old_k1 << 11);

    s32 res = 0;
    if ((g_impose->popup_disable_flags & 1u) == 0) {
        res = imposeCloseOverlayByType(open_flag);
    }
    pspSetK1(old_k1);
    return res;
}

s32 sceImpose_9DBCE0C4(u32 val)
{
    s32 intr = sceKernelCpuSuspendIntr();
    s32 prev = (s32)g_impose->home_button_mode;
    g_impose->home_button_mode = val;
    sceKernelCpuResumeIntr(intr);
    return prev;
}

s32 sceImpose_driver_BCF1D254(u32 val) __attribute__((alias("sceImpose_9DBCE0C4")));

s32 sceImpose_7084E72C(void)
{
    return (s32)g_impose->home_button_mode;
}

s32 sceImpose_9BA61B49(void)
{
    return g_impose->home_button_pressed;
}

s32 sceImpose_A9884B00(s32 profile_idx)
{
    s32 res = imposeSelectTvOutProfile(profile_idx);
    if (res == 0) {
        s32 mode = (g_impose->tvout_active != 0) ? g_tvout_mode->tv_mode : g_tvout_mode->lcd_mode;
        imposeSetTvOutMode(1, mode, g_tvout_mode->width, g_tvout_mode->height);
    }
    return res;
}

s32 sceImpose_BB3F5DEC(void)
{
    if ((g_impose->display_caps & 0x00010000u) != 0 && g_impose->app_type != 0x200) {
        return 0;
    }
    return (s32)0x80000004;
}

s32 sceImpose_FF1A2F07(void)
{
    if ((g_impose->display_caps & 0x00020000u) != 0 && g_impose->app_type != 0x200) {
        return (g_impose->tvout_active != 0) ? 0 : -1;
    }
    return (s32)0x80000004;
}

s32 sceImpose_driver_CE16DF6D(void) __attribute__((alias("sceImpose_FF1A2F07")));

/* =========================================================================
 * Exported Functions: sceImpose_driver
 * ========================================================================= */

s32 sceImpose_driver_0F067E16(void)
{
    s32 flags = (s32)g_impose->pending_change_flags;
    g_impose->pending_change_flags = 0;
    return flags;
}

s32 sceImpose_driver_133DB99A(void)
{
    return 0;
}

s32 sceImpose_driver_139500A7(void)
{
    return 0;
}

s32 sceImpose_driver_28B97035(s32 mode, s32 width, s32 height)
{
    if (g_impose->init_state == 1 && mode != 0 && mode != 0xE0) {
        g_impose->tvout_active = 0;
        return (s32)0x80000107;
    }
    s32 res = imposeSetTvOutMode(1, mode, width, height);
    if (g_impose->hprm_status == 0x10) {
        imposeSetEventFlag(0x40004020u);
    }
    return res;
}

s32 sceImpose_driver_686B040E(void)
{
    if (g_impose->suspend_active != 0) {
        u32 state = g_impose->menu_state;
        if ((state & 4u) == 0 && (state & 0x200u) == 0) {
            sceImpose_driver_BB12F974(0x40202u);
        }
        g_impose->suspend_request_time = sceKernelGetSystemTimeLow();
    }
    return 0;
}

s32 sceImpose_driver_9E921836(void)
{
    g_impose->suppress_notify = 1;
    g_impose->suspend_active = 0;
    g_impose->pending_change_flags = 0;
    sceDisplayEnable();
    s32 was_dimmed = g_impose->brightness_dimmed;
    g_impose->display_off_state = 0;
    g_impose->special_btn_pending = 0;
    g_impose->avls_blink_counter = 0;
    if (was_dimmed != 0) {
        imposeRestoreButtonIntercept();
        imposeStepBrightness(0);
        imposeNoopCallback(2, 1);
        g_impose->screen_hold_state = 0;
        g_impose->pending_change_flags |= 0x20000000u;
    }
    imposeSetVolumeTarget(1);
    g_impose->wlan_idle_timeout = 0;
    imposeNoopCallback(8, 0);
    sceKernelEnableSubIntr(0x1E, 0x14);
    return 0;
}

s32 sceImpose_driver_AABEE36A(void)
{
    if ((s32)g_impose->menu_state >= 0) {
        g_impose->reg_flush_pending = 1;
        imposeSetEventFlag(8u);
        do {
            sceKernelDelayThread(100000);
        } while (g_impose->reg_flush_pending != 0);
    }
    sceKernelUnregisterSysEventHandler(&g_sysevent_handler);
    sceKernelSetEventFlag(g_impose->event_flag_id, 0x80000000u);
    sceKernelWaitThreadEnd(g_impose->main_thread_id, NULL);
    sceKernelDeleteThread(g_impose->main_thread_id);
    sceKernelDeleteEventFlag(g_impose->event_flag_id);
    sceKernelReleaseSubIntrHandler(0x1E, 0x14);
    return 0;
}

s32 sceImpose_driver_AF5DC020(s32 id, void *out)
{
    u32 old_k1 = pspGetK1();
    u32 shifted = old_k1 << 11;

    (void)id;
    if (((s32)(shifted & (u32)out) < 0) || (out == NULL)) {
        pspSetK1(old_k1);
        return (s32)0x80000023;
    }
    return (s32)0x80000107;
}

s32 sceImpose_driver_B3EAE8F0(void)
{
    return (s32)g_impose->menu_state;
}

s32 sceImpose_driver_B497314D(s32 id, void *out)
{
    u32 old_k1 = pspGetK1();
    u32 shifted = old_k1 << 11;
    pspSetK1(shifted);

    if (((s32)(shifted & (u32)out) < 0) || (out == NULL)) {
        pspSetK1(old_k1);
        return (s32)0x80000023;
    }
    if (id != 0x4000) {
        pspSetK1(old_k1);
        return (s32)0x80000107;
    }
    if ((s32)((((u32)out + 8u) | (u32)out) & shifted) < 0) {
        pspSetK1(old_k1);
        return (s32)0x80000023;
    }

    u32 tick_words[2];
    tick_words[0] = g_impose->popup_expire_lo;
    tick_words[1] = g_impose->popup_expire_hi;
    u8 *dst = (u8 *)out;
    const u8 *src = (const u8 *)tick_words;
    s32 i;
    for (i = 0; i < 8; i++) {
        dst[i] = src[i];
    }
    pspSetK1(old_k1);
    return 0;
}

s32 sceImpose_driver_BB12F974(u32 state)
{
    u32 ev = 0;

    if (state == 0 || (state & 1u) != 0) {
        if ((g_impose->menu_state & 2u) != 0) {
            ev = 2u;
        }
    } else if ((state & 4u) != 0) {
        ev = g_impose->menu_state & 2u;
        if (g_impose->fb_buf_cached != NULL) {
            ev |= 0x80u;
        }
        imposeNoopCallback(1, 1);
        if (g_impose->display_caps != 0 && g_impose->initial_display_mode != 0) {
            imposeNoopCallback(2, 0);
            imposeNoopCallback(4, 1);
        } else {
            imposeNoopCallback(2, 1);
        }
        g_impose->equalizer_enabled = 1;
    } else if ((state & 2u) != 0) {
        if (imposeIsMenuAllowed() != 0) {
            return (s32)0x80000105;
        }
        u32 cur = g_impose->menu_state;
        if (cur < 2u) {
            ev = ((state & 0x200u) != 0) ? 0x101u : 1u;
        } else if ((state & 0x200u) != 0 || (state & 0x800u) != 0) {
            imposeClearOsdRegion(0xFFu);
            ev = 0x100u;
        } else if ((state & 0x400u) != 0) {
            imposeClearOsdRegion(0xFFu);
        } else if ((cur & 0x00040000u) != 0) {
            return (s32)0x80000105;
        }
        if (g_impose->lang_change_pending != 0) {
            ev |= 0x200u;
        }
        g_impose->cur_no_sprite = NULL;
        g_impose->blink_frame_counter = 0;
        g_impose->cur_yes_sprite = NULL;
    }

    g_impose->menu_state = state;
    if (ev != 0) {
        imposeSetEventFlag(ev);
    }
    return 0;
}

s32 sceImpose_driver_CBF27B10(void)
{
    return 0;
}

s32 sceImpose_driver_D067235E(void)
{
    SceImposeThreadOpt thread_opt;
    s32 disp_mode;
    s32 disp_width;
    s32 disp_height;
    s32 oem_charset;

    memset(&g_impose_state, 0, sizeof(g_impose_state));
    g_impose = &g_impose_state;

    g_impose_state.hardware_model = sceKernelGetModel();
    g_impose->app_type = sceKernelApplicationType();
    g_impose->fpl_partition_id = 1;
    g_impose->thread_partition_id = 1;
    if (g_impose->hardware_model != 0) {
        if (sceKernelPartitionTotalMemSize(8) > 0) {
            g_impose->fpl_partition_id = 8;
            if (g_impose->hardware_model < 4) {
                g_impose->thread_partition_id = 8;
            }
        }
    }

    g_impose->display_caps = (u32)sceDisplay_driver_996881D2();
    if (g_impose->display_caps != 0) {
        sceDisplayGetMode(&disp_mode, &disp_width, &disp_height);
        g_impose->initial_display_mode = disp_mode;
    }

    g_impose->hprm_caps = (u32)sceHprm_driver_9BDE174B();
    g_impose->init_state = -1;
    g_impose->menu_state = 1u;
    g_impose->fb_stride = 512;
    g_impose->fb_height = 272;
    g_impose->main_volume = 20;
    g_impose->suspend_interval = 600;
    g_impose->screensaver_start_time = 300;
    g_impose->language = 0;
    g_impose->mute = 0;
    g_impose->avls = 0;
    g_impose->equalizer_mode = 0;
    g_impose->time_format = 0;
    g_impose->date_format = 0;
    g_impose->backlight_off_interval = 300;

    g_impose->battery_exist = scePower_driver_8C873AA7();
    if (g_impose->battery_exist != 0) {
        s32 pct = scePower_driver_31AEA94C();
        g_impose->battery_percent = (pct < 0) ? 100 : pct;
    }
    g_impose->ac_online = scePower_driver_872F4ECE();
    g_impose->battery_charging = scePower_driver_7A9EA6DE();

    g_impose->tvout_allowed = 1;
    g_impose->tvout_blocked = 0;
    g_impose->brightness_max_limit = 0;
    g_impose->tvout_active = 0;

    imposeRefreshRegistryValues(-1);

    g_impose->hprm_hp_present = sceHprm_driver_370DDC12();
    if ((g_impose->hprm_caps & 2u) != 0) {
        g_impose->hprm_mic_present = sceHprm_driver_1C3525DD();
    }
    g_impose->headphone_connected = g_impose->hprm_hp_present | g_impose->hprm_mic_present;
    g_impose->hprm_headphone_exist = sceHprm_driver_FA4A25A7();
    g_impose->hprm_remote_exist = sceHprm_driver_EFCFD0C5();
    if (g_impose->display_caps != 0) {
        g_impose->tvout_cable_type = 0;
    }
    g_impose->hprm_status = imposeGetHprmStatus();

    if (sceKernelBootFrom() != 0x20) {
        s32 intr = sceKernelCpuSuspendIntr();
        g_impose->popup_disable_flags |= 2u;
        sceKernelCpuResumeIntr(intr);
    }

    thread_opt.size = sizeof(thread_opt);
    thread_opt.stack_mpid = g_impose->thread_partition_id;
    s32 thid = sceKernelCreateThread("SceImpose", imposeMainThread, 0x14, 0x800u, 0x00100000u, &thread_opt);
    if (thid < 0) {
        return (s32)0x80000040;
    }
    g_impose->main_thread_id = thid;

    s32 fb_fpl = sceKernelCreateFpl("SceImpose_FrameBuffer", g_impose->fpl_partition_id, 0x4100u,
                                    (u32)(2 * g_impose->fb_stride * g_impose->fb_height), 1, NULL);
    if (fb_fpl < 0) {
        sceKernelDeleteThread(g_impose->main_thread_id);
        return (s32)0x8001000C;
    }
    g_impose->fb_fpl_id = fb_fpl;
    sceKernelTryAllocateFpl(fb_fpl, &g_impose->fb_buf_cached);
    g_impose->fb_buf_uncached = (u16 *)(((u32)g_impose->fb_buf_cached & 0x1FFFFFFFu) | 0xA0000000u);

    s32 rsc_res = imposeAllocIconResource();
    if (rsc_res < 0) {
        sceKernelDeleteFpl(g_impose->fb_fpl_id);
        sceKernelDeleteThread(g_impose->main_thread_id);
        return rsc_res;
    }

    g_impose->overlay_handlers[0] = imposeRedrawOsd;
    g_impose->overlay_handlers[1] = imposeDrawBlinkCursor;
    g_impose->overlay_handlers[2] = imposeDrawVolumeBar;
    g_impose->overlay_handlers[3] = imposeShowBatteryIcon;

    s32 evf = sceKernelCreateEventFlag("SceImpose", 1, 0, NULL);
    if (evf < 0) {
        sceKernelDeleteFpl(g_impose->fb_fpl_id);
        sceKernelDeleteThread(g_impose->main_thread_id);
        return (s32)0x80000042;
    }
    g_impose->event_flag_id = evf;

    sceCtrlSetSpecialButtonCallback(0, 0x01C00000u, imposeHandleHomeButton, 0);
    sceCtrlSetSpecialButtonCallback(1, 0x00300000u, imposeHandleHoldButton, 0);

    if (sceKernelStartThread(g_impose->main_thread_id, 0, NULL) != 0) {
        sceKernelDeleteEventFlag(g_impose->event_flag_id);
        sceKernelDeleteFpl(g_impose->fb_fpl_id);
        sceKernelDeleteThread(g_impose->main_thread_id);
        return (s32)0x80000040;
    }

    if (g_impose->display_caps != 0 && g_impose->initial_display_mode != 0) {
        imposeNoopCallback(2, 0);
        imposeNoopCallback(4, 1);
    } else {
        imposeNoopCallback(2, 1);
    }

    sceKernelRegisterSysEventHandler(&g_sysevent_handler);
    sceKernelMemset32(g_impose->fb_buf_uncached, IMPOSE_CLEAR_COLOR_WORD,
                      (u32)(2 * g_impose->fb_stride * g_impose->fb_height));
    sceKernelRegisterSubIntrHandler(0x1E, 0x14, imposeCtrlSubIntrHandler, NULL);
    sceKernelEnableSubIntr(0x1E, 0x14);
    sceKermit_driver_5280B410(9, imposeTriggerPowerPoll);

    oem_charset = g_impose->oem_charset;
    sceIoDevctl("fatms0:", 0x02425856u, &oem_charset, 4, NULL, 0);

    if (g_impose->app_type == 0x300) {
        if (g_impose->display_caps != 0) {
            imposeSelectTvOutProfile(0);
        }
        s32 intr = sceKernelCpuSuspendIntr();
        g_impose->home_button_mode = 1u;
        sceKernelCpuResumeIntr(intr);
        g_impose->tvout_keep_on_reset = 1;
    } else if (g_impose->app_type == 0x400) {
        imposeNoopCallback(1, 1);
        if (g_impose->display_caps != 0) {
            imposeSelectTvOutProfile(1);
            if (g_impose->initial_display_mode != 0) {
                imposeNoopCallback(2, 0);
                imposeNoopCallback(4, 1);
            } else {
                imposeNoopCallback(2, 1);
            }
        }
        s32 intr = sceKernelCpuSuspendIntr();
        g_impose->home_button_mode = 1u;
        sceKernelCpuResumeIntr(intr);
    } else {
        if (g_impose->display_caps != 0) {
            imposeSelectTvOutProfile(0);
        }
        if (sceKernelInitDiscImage() != NULL) {
            g_impose->disc_image_active = 1;
        }
        sceKernelIsDVDMode();
    }

    imposeShowOsdPopup(0ULL, 0ULL);
    g_impose->volume_target = 1;
    imposeSetEventFlag(0x4000u);
    g_impose->deferred_event_bits = 100u;
    g_impose->deferred_event_delay = 5;
    g_impose->suppress_notify = 0;
    g_impose->deferred_event_ticks = 0;
    sceKernelSetInitCallback(imposeInitAudioCallback, 3, NULL);
    return 0;
}

s32 sceImpose_driver_D3C10F9B(void)
{
    return 0;
}

s32 sceImpose_driver_D7B9C4C8(void)
{
    return 0;
}

s32 sceImpose_driver_E9B0E008(s32 *out)
{
    if ((s32)(pspGetK1() & (u32)out) < 0) {
        return (s32)0x80000023;
    }
    s32 intr = sceKernelCpuSuspendIntr();
    s32 res = (s32)0x80000107;
    if ((g_impose->display_caps & 0x02000000u) != 0) {
        res = 0;
    }
    if (out != NULL) {
        *out = 0;
    }
    sceKernelCpuResumeIntr(intr);
    return res;
}

/* =========================================================================
 * Exported Functions: sceAudioRouting
 * ========================================================================= */

s32 sceAudioRoutingSetMode(s32 mode)
{
    return imposeSetAudioMode((u32)mode);
}

s32 sceAudioRoutingGetMode(void)
{
    return g_impose->audio_routing_mode;
}

s32 sceAudioRoutingSetVolumeMode(s32 mode)
{
    return imposeCalcEffectiveVolume(mode);
}

s32 sceAudioRoutingGetVolumeMode(void)
{
    return g_impose->volume_mode;
}

/* =========================================================================
 * Module Lifecycle
 * ========================================================================= */

s32 module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    return (sceImpose_driver_D067235E() < 0) ? 1 : 0;
}

s32 module_reboot_phase(s32 phase, void *argp)
{
    (void)argp;
    if (phase == 2) {
        u32 state = g_impose->menu_state;
        if ((s32)state < 0) {
            sceDisplaySetHoldMode(2);
        } else if ((state & 2u) != 0) {
            sceImpose_driver_BB12F974(1u);
        }
        sceImpose_driver_AABEE36A();
    }
    return 0;
}

s32 module_reboot_before(void *reboot_param)
{
    if (g_impose->popup_timer_active != 0 && g_impose->popup_timer_phase == 4) {
        SceImposeRebootStatus *status_blk = ((SceImposeRebootParam *)reboot_param)->status_block;
        if (status_blk != NULL) {
            status_blk->status = (s32)0x80020152;
        }
    }
    return 0;
}
