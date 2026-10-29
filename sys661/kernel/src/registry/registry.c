/*
 * ePSP 6.61 sceRegistry_Service (registry.prx)
 */

#define NULL ((void *)0)

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;
typedef unsigned int SceSize;
typedef u32 REGHANDLE;

struct RegParam {
	u32 regtype;
	char name[256];
	u32 namelen;
	u32 unk2;
	u32 unk3;
};

struct RegKeyEntry {
	const char *name;
	u32 id;
	u32 type;
	u32 size;
};

struct RegCatEntry {
	const char *name;
	u32 id;
	struct RegKeyEntry *keys;
	u32 num_keys;
	u32 open_flag;
};

extern int sceKernelDelayThread(u32 delay);
extern void *memset(void *s, int c, u32 n);
extern int strncmp(const char *s1, const char *s2, u32 n);
extern u32 strnlen(const char *s, u32 maxlen);
extern void *memcpy(void *dst, const void *src, u32 n);
extern char *strncpy(char *dst, const char *src, u32 n);

u32 sceReg_driver_var_867D5F60;
u32 sceReg_driver_var_5105E3FF;
static u32 g_reg_open;

static char g_home_uri[0x400];
static u32 g_cookie_mode;
static u32 g_proxy_mode;
static char g_proxy_address[0x80];
static u32 g_proxy_port;
static u32 g_picture;
static u32 g_animation;
static u32 g_javascript;
static u32 g_cache_size;
static u32 g_char_size;
static u32 g_disp_mode;
static u32 g_connect_mode;
static u32 g_proxy_protect;
static u32 g_proxy_autoauth;
static char g_proxy_user[0x80];
static char g_proxy_password[0x80];
static u32 g_webpage_quality;
static u32 g_tm_ec_ttl;
static u8 g_tm_ec_ttl_update_time[8];
static u32 g_tm_service_sub_status;
static char g_np_env[12];
static u8 g_np_account_id[0x10];
static char g_np_login_id[0x44];
static char g_np_password[0x20];
static u32 g_np_auto_sign_in_enable;
static u32 g_np_nav_only;
static u32 g_np_ad_clock_diff;
static u32 g_np_view_mode;
static u32 g_np_geo_filtering;
static char g_np_guest_country[4];
static char g_np_guest_lang[4];
static u32 g_np_guest_yob;
static u32 g_np_guest_mob;
static u32 g_np_guest_dob;
static u32 g_np_check_drm;
static char g_owner_name[0x80];
static u32 g_language;
static u32 g_button_assign;
static u8 g_lock_password[4];
static u32 g_parental_level;
static u32 g_browser_start;
static u32 g_char_oem;
static u32 g_char_ansi;
static u32 g_time_format;
static u32 g_date_format;
static u32 g_summer_time;
static u32 g_time_zone_offset;
static char g_time_zone_area[0x30];
static char g_adhoc_ssid_prefix[4];
static u32 g_infra_latest_id;
static u32 g_infra_eap_md5;
static u32 g_infra_auto_setting;
static u32 g_infra_wifisvc_setting;
static u32 g_infra_btdun_warnings_check;
static u32 g_osk_version_id;
static u32 g_osk_disp_locale;
static u32 g_osk_writing_locale;
static u32 g_osk_input_char_mask;
static u32 g_osk_keytop_index;
static u32 g_count_boot;
static u32 g_count_game_exec;
static u32 g_count_slide;
static u32 g_count_usb_connect;
static u32 g_count_wifi_connect;
static u32 g_count_psn_access;

static struct RegKeyEntry g_keys[104] = {
	/*   0 (0x7cf0) */ { "category_version", 0x026, 0x2, 0x4 },
	/*   1 (0x7d00) */ { "home_uri", 0x027, 0x3, 0x400 },
	/*   2 (0x7d10) */ { "cookie_mode", 0x028, 0x2, 0x4 },
	/*   3 (0x7d20) */ { "proxy_mode", 0x029, 0x2, 0x4 },
	/*   4 (0x7d30) */ { "proxy_address", 0x02a, 0x3, 0x80 },
	/*   5 (0x7d40) */ { "proxy_port", 0x02b, 0x2, 0x4 },
	/*   6 (0x7d50) */ { "picture", 0x02c, 0x2, 0x4 },
	/*   7 (0x7d60) */ { "animation", 0x02d, 0x2, 0x4 },
	/*   8 (0x7d70) */ { "javascript", 0x02e, 0x2, 0x4 },
	/*   9 (0x7d80) */ { "cache_size", 0x02f, 0x2, 0x4 },
	/*  10 (0x7d90) */ { "char_size", 0x030, 0x2, 0x4 },
	/*  11 (0x7da0) */ { "disp_mode", 0x031, 0x2, 0x4 },
	/*  12 (0x7db0) */ { "connect_mode", 0x032, 0x2, 0x4 },
	/*  13 (0x7dc0) */ { "flash_activated", 0x033, 0x2, 0x4 },
	/*  14 (0x7dd0) */ { "flash_play", 0x034, 0x2, 0x4 },
	/*  15 (0x7de0) */ { "proxy_protect", 0x035, 0x2, 0x4 },
	/*  16 (0x7df0) */ { "proxy_autoauth", 0x036, 0x2, 0x4 },
	/*  17 (0x7e00) */ { "proxy_user", 0x037, 0x3, 0x80 },
	/*  18 (0x7e10) */ { "proxy_password", 0x038, 0x3, 0x80 },
	/*  19 (0x7e20) */ { "webpage_quality", 0x039, 0x2, 0x4 },
	/*  20 (0x7e30) */ { "tm_service", 0x03a, 0x2, 0x4 },
	/*  21 (0x7e40) */ { "tm_ec_ttl", 0x03b, 0x2, 0x4 },
	/*  22 (0x7e50) */ { "tm_ec_ttl_update_time", 0x03c, 0x10004, 0x8 },
	/*  23 (0x7e60) */ { "tm_service_sub_status", 0x03d, 0x2, 0x4 },
	/*  24 (0x7e70) */ { "color_space_mode", 0x03e, 0x2, 0x4 },
	/*  25 (0x7e80) */ { "env", 0x03f, 0x3, 0x9 },
	/*  26 (0x7e90) */ { "account_id", 0x040, 0x4, 0x10 },
	/*  27 (0x7ea0) */ { "login_id", 0x041, 0x3, 0x41 },
	/*  28 (0x7eb0) */ { "password", 0x042, 0x3, 0x1f },
	/*  29 (0x7ec0) */ { "auto_sign_in_enable", 0x043, 0x2, 0x4 },
	/*  30 (0x7ed0) */ { "nav_only", 0x044, 0x2, 0x4 },
	/*  31 (0x7ee0) */ { "np_ad_clock_diff", 0x045, 0x2, 0x4 },
	/*  32 (0x7ef0) */ { "view_mode", 0x046, 0x2, 0x4 },
	/*  33 (0x7f00) */ { "np_geo_filtering", 0x047, 0x2, 0x4 },
	/*  34 (0x7f10) */ { "guest_country", 0x048, 0x3, 0x3 },
	/*  35 (0x7f20) */ { "guest_lang", 0x049, 0x3, 0x3 },
	/*  36 (0x7f30) */ { "guest_yob", 0x04a, 0x2, 0x4 },
	/*  37 (0x7f40) */ { "guest_mob", 0x04b, 0x2, 0x4 },
	/*  38 (0x7f50) */ { "guest_dob", 0x04c, 0x2, 0x4 },
	/*  39 (0x7f60) */ { "check_drm", 0x04d, 0x2, 0x4 },
	/*  40 (0x7f70) */ { "owner_name", 0x04e, 0x3, 0x80 },
	/*  41 (0x7f80) */ { "umd_cache", 0x04f, 0x2, 0x4 },
	/*  42 (0x7f90) */ { "slide_action", 0x050, 0x2, 0x4 },
	/*  43 (0x7fa0) */ { "backlight_brightness", 0x051, 0x2, 0x4 },
	/*  44 (0x7fb0) */ { "language", 0x052, 0x2, 0x4 },
	/*  45 (0x7fc0) */ { "button_assign", 0x053, 0x2, 0x4 },
	/*  46 (0x7fd0) */ { "password", 0x054, 0x4, 0x4 },
	/*  47 (0x7fe0) */ { "parental_level", 0x055, 0x2, 0x4 },
	/*  48 (0x7ff0) */ { "browser_start", 0x056, 0x2, 0x4 },
	/*  49 (0x8000) */ { "oem", 0x05b, 0x2, 0x4 },
	/*  50 (0x8010) */ { "ansi", 0x05c, 0x2, 0x4 },
	/*  51 (0x8020) */ { "time_format", 0x060, 0x2, 0x4 },
	/*  52 (0x8030) */ { "date_format", 0x061, 0x2, 0x4 },
	/*  53 (0x8040) */ { "summer_time", 0x062, 0x2, 0x4 },
	/*  54 (0x8050) */ { "time_zone_offset", 0x063, 0x2, 0x4 },
	/*  55 (0x8060) */ { "time_zone_area", 0x064, 0x3, 0x30 },
	/*  56 (0x8070) */ { "channel", 0x065, 0x2, 0x4 },
	/*  57 (0x8080) */ { "ssid_prefix", 0x066, 0x3, 0x4 },
	/*  58 (0x8090) */ { "latest_id", 0x067, 0x2, 0x4 },
	/*  59 (0x80a0) */ { "eap_md5", 0x068, 0x2, 0x4 },
	/*  60 (0x80b0) */ { "auto_setting", 0x069, 0x2, 0x4 },
	/*  61 (0x80c0) */ { "wifisvc_setting", 0x06a, 0x2, 0x4 },
	/*  62 (0x80d0) */ { "btdun_warnings_check", 0x06b, 0x2, 0x4 },
	/*  63 (0x80e0) */ { "auth_name", 0x06c, 0x3, 0x40 },
	/*  64 (0x80f0) */ { "auth_key", 0x06d, 0x3, 0x40 },
	/*  65 (0x8100) */ { "version_id", 0x06e, 0x2, 0x4 },
	/*  66 (0x8110) */ { "disp_locale", 0x06f, 0x2, 0x4 },
	/*  67 (0x8120) */ { "writing_locale", 0x070, 0x2, 0x4 },
	/*  68 (0x8130) */ { "input_char_mask", 0x071, 0x2, 0x4 },
	/*  69 (0x8140) */ { "keytop_index", 0x072, 0x2, 0x4 },
	/*  70 (0x8150) */ { "path_name", 0x079, 0x3, 0x40 },
	/*  71 (0x8160) */ { "num_fonts", 0x07a, 0x2, 0x4 },
	/*  72 (0x8170) */ { "boot_count", 0x073, 0x2, 0x4 },
	/*  73 (0x8180) */ { "game_exec_count", 0x074, 0x2, 0x4 },
	/*  74 (0x8190) */ { "slide_count", 0x075, 0x2, 0x4 },
	/*  75 (0x81a0) */ { "usb_connect_count", 0x076, 0x2, 0x4 },
	/*  76 (0x81b0) */ { "wifi_connect_count", 0x077, 0x2, 0x4 },
	/*  77 (0x81c0) */ { "psn_access_count", 0x078, 0x2, 0x4 },
	/*  78 (0x81d0) */ { "h_size", 0x07b, 0x2, 0x4 },
	/*  79 (0x81e0) */ { "v_size", 0x07c, 0x2, 0x4 },
	/*  80 (0x81f0) */ { "h_resolution", 0x07d, 0x2, 0x4 },
	/*  81 (0x8200) */ { "v_resolution", 0x07e, 0x2, 0x4 },
	/*  82 (0x8210) */ { "extra_attributes", 0x07f, 0x2, 0x4 },
	/*  83 (0x8220) */ { "weight", 0x080, 0x2, 0x4 },
	/*  84 (0x8230) */ { "family_code", 0x081, 0x2, 0x4 },
	/*  85 (0x8240) */ { "style", 0x082, 0x2, 0x4 },
	/*  86 (0x8250) */ { "sub_style", 0x083, 0x2, 0x4 },
	/*  87 (0x8260) */ { "language_code", 0x084, 0x2, 0x4 },
	/*  88 (0x8270) */ { "region_code", 0x085, 0x2, 0x4 },
	/*  89 (0x8280) */ { "country_code", 0x086, 0x2, 0x4 },
	/*  90 (0x8290) */ { "font_name", 0x087, 0x3, 0x40 },
	/*  91 (0x82a0) */ { "file_name", 0x088, 0x3, 0x40 },
	/*  92 (0x82b0) */ { "expire_date", 0x089, 0x2, 0x4 },
	/*  93 (0x82c0) */ { "shadow_option", 0x08a, 0x2, 0x4 },
	/*  94 (0x82d0) */ { "sound_reduction", 0x19b, 0x2, 0x4 },
	/*  95 (0x82e0) */ { "horizontal", 0x19c, 0x2, 0x4 },
	/*  96 (0x82f0) */ { "vertical", 0x19d, 0x2, 0x4 },
	/*  97 (0x8300) */ { "main_volume", 0x057, 0x2, 0x4 },
	/*  98 (0x8310) */ { "mute", 0x058, 0x2, 0x4 },
	/*  99 (0x8320) */ { "avls", 0x059, 0x2, 0x4 },
	/* 100 (0x8330) */ { "equalizer_mode", 0x05a, 0x2, 0x4 },
	/* 101 (0x8340) */ { "suspend_interval", 0x05d, 0x2, 0x4 },
	/* 102 (0x8350) */ { "backlight_off_interval", 0x05e, 0x2, 0x4 },
	/* 103 (0x8360) */ { "active_backlight_mode", 0x05f, 0x2, 0x4 },
};

static struct RegCatEntry g_cats[38] = {
	/*  0 */ { "/REGISTRY", 0x00, &g_keys[-1999], 1, 0 },
	/*  1 */ { "/CONFIG/BROWSER", 0x01, &g_keys[-1998], 19, 0 },
	/*  2 */ { "/CONFIG/BROWSER2", 0x02, &g_keys[-1979], 4, 0 },
	/*  3 */ { "/CONFIG/DISPLAY", 0x03, &g_keys[-1975], 1, 0 },
	/*  4 */ { "/CONFIG/NP", 0x04, &g_keys[-1974], 15, 0 },
	/*  5 */ { "/CONFIG/SYSTEM", 0x05, &g_keys[-1959], 4, 0 },
	/*  6 */ { "/CONFIG/SYSTEM/XMB", 0x06, &g_keys[-1955], 2, 0 },
	/*  7 */ { "/CONFIG/SYSTEM/LOCK", 0x07, &g_keys[-1953], 3, 0 },
	/*  8 */ { "/CONFIG/SYSTEM/CHARACTER_SET", 0x08, &g_keys[-1950], 2, 0 },
	/*  9 */ { "/CONFIG/SYSTEM/SOUND", 0x09, &g_keys[-1902], 4, 0 },
	/* 10 */ { "/CONFIG/SYSTEM/POWER_SAVING", 0x0a, &g_keys[-1898], 3, 0 },
	/* 11 */ { "/CONFIG/DATE", 0x0b, &g_keys[-1948], 5, 0 },
	/* 12 */ { "/CONFIG/NETWORK/ADHOC", 0x0c, &g_keys[-1943], 2, 0 },
	/* 13 */ { "/CONFIG/NETWORK/INFRASTRUCTURE", 0x0d, &g_keys[-1941], 5, 0 },
	/* 14 */ { "/CONFIG/NETWORK/GO_MESSENGER", 0x0e, &g_keys[-1936], 2, 0 },
	/* 15 */ { "/CONFIG/OSK", 0x0f, &g_keys[-1934], 5, 0 },
	/* 16 */ { "/DATA/COUNT", 0x10, &g_keys[-1927], 6, 0 },
	/* 17 */ { "/DATA/FONT", 0x11, &g_keys[-1929], 2, 0 },
	/* 18 */ { "/DATA/FONT/PROPERTY/INFO0", 0x12, &g_keys[-1921], 16, 0 },
	/* 19 */ { "/DATA/FONT/PROPERTY/INFO1", 0x13, &g_keys[-1921], 16, 0 },
	/* 20 */ { "/DATA/FONT/PROPERTY/INFO2", 0x14, &g_keys[-1921], 16, 0 },
	/* 21 */ { "/DATA/FONT/PROPERTY/INFO3", 0x15, &g_keys[-1921], 16, 0 },
	/* 22 */ { "/DATA/FONT/PROPERTY/INFO4", 0x16, &g_keys[-1921], 16, 0 },
	/* 23 */ { "/DATA/FONT/PROPERTY/INFO5", 0x17, &g_keys[-1921], 16, 0 },
	/* 24 */ { "/DATA/FONT/PROPERTY/INFO6", 0x18, &g_keys[-1921], 16, 0 },
	/* 25 */ { "/DATA/FONT/PROPERTY/INFO7", 0x19, &g_keys[-1921], 16, 0 },
	/* 26 */ { "/DATA/FONT/PROPERTY/INFO8", 0x1a, &g_keys[-1921], 16, 0 },
	/* 27 */ { "/DATA/FONT/PROPERTY/INFO9", 0x1b, &g_keys[-1921], 16, 0 },
	/* 28 */ { "/DATA/FONT/PROPERTY/INFO10", 0x1c, &g_keys[-1921], 16, 0 },
	/* 29 */ { "/DATA/FONT/PROPERTY/INFO11", 0x1d, &g_keys[-1921], 16, 0 },
	/* 30 */ { "/DATA/FONT/PROPERTY/INFO12", 0x1e, &g_keys[-1921], 16, 0 },
	/* 31 */ { "/DATA/FONT/PROPERTY/INFO13", 0x1f, &g_keys[-1921], 16, 0 },
	/* 32 */ { "/DATA/FONT/PROPERTY/INFO14", 0x20, &g_keys[-1921], 16, 0 },
	/* 33 */ { "/DATA/FONT/PROPERTY/INFO15", 0x21, &g_keys[-1921], 16, 0 },
	/* 34 */ { "/DATA/FONT/PROPERTY/INFO16", 0x22, &g_keys[-1921], 16, 0 },
	/* 35 */ { "/DATA/FONT/PROPERTY/INFO17", 0x23, &g_keys[-1921], 16, 0 },
	/* 36 */ { "/SYSPROFILE", 0x24, &g_keys[-1905], 1, 0 },
	/* 37 */ { "/SYSPROFILE/RESOLUTION", 0x25, &g_keys[-1904], 2, 0 },
};

static void sub_5da8(void);
static void sub_6164(void);
static int sub_6190(u32 cat_id);
static int sub_61c0(u32 cat_id);
static int sub_6204(const char *name);
static int sub_6280(u32 cat_id, u32 *out_num);
static int sub_62bc(int cat_id, const char *name, void *buf, u32 size);
static int sub_08d0(u32 cat_id, const char *name, u32 *out_id, u32 *out_type, u32 *out_size);
static int sub_0a64(int cat_id, u32 key_id, void *buf, u32 size);
static int sub_4ce4(int cat_id, const char *name, const void *buf, u32 size);
static int sub_5d08(u32 cat_id, char *buf, int count);

static inline u32 get_k1(void)
{
	u32 k1;
	__asm__ volatile ("move %0, $k1" : "=r"(k1));
	return k1;
}

static inline int check_uptr(const void *p, u32 size)
{
	u32 addr = (u32)p;
	u32 end = addr + size;
	return (s32)((addr | size | end) & (get_k1() << 11)) < 0;
}

int sceRegOpenRegistry(struct RegParam *reg, int mode, REGHANDLE *h)
{
	if (sceReg_driver_var_867D5F60 != 0)
		sceKernelDelayThread(sceReg_driver_var_867D5F60);
	if (reg == NULL || h == NULL)
		return 0x8008273b;
	if (check_uptr(reg, sizeof(*reg)) || check_uptr(h, sizeof(*h)))
		return 0x8008273b;
	if (mode != 2)
		return 0x80082719;
	if (reg->regtype != 1)
		return 0x800001ff;
	g_reg_open = 1;
	*h = 0;
	return 0;
}

int sceRegOpenCategory(REGHANDLE h, const char *name, int mode, REGHANDLE *cat_h)
{
	if (name == NULL)
		return 0x80082738;
	if (check_uptr(name, 0x10) || cat_h == NULL || check_uptr(cat_h, 4))
		return 0x8008273b;
	if (mode != 2)
		return 0x80082719;
	if (h != 0 || g_reg_open == 0)
		return 0x80082715;
	*cat_h = sub_6204(name);
	return 0;
}

int sceRegGetKeyInfo(REGHANDLE cat_h, const char *name, REGHANDLE *key_h, u32 *type, u32 *size)
{
	u32 raw_id;
	int res;

	if (name == NULL || check_uptr(name, 0x1b))
		return 0x8008273b;
	if (key_h == NULL || check_uptr(key_h, 4))
		return 0x8008273b;
	if (type != NULL && check_uptr(type, 4))
		return 0x8008273b;
	if (size != NULL && check_uptr(size, 4))
		return 0x8008273b;
	if (sub_6190(cat_h) == 0)
		return 0x8008272e;
	res = sub_08d0(cat_h, name, &raw_id, type, size);
	*key_h = raw_id ^ 0xa0f3d97b;
	return res;
}

int sceRegSetKeyValue(REGHANDLE cat_h, const char *name, const void *buf, u32 size)
{
	if (name == NULL || check_uptr(name, 0x1b))
		return 0x8008273b;
	if (buf == NULL || check_uptr(buf, size))
		return 0x8008273b;
	if (sub_6190(cat_h) == 0)
		return 0x8008272e;
	return sub_4ce4(cat_h, name, buf, size);
}

int sceRegGetKeyValue(REGHANDLE cat_h, REGHANDLE key_h, void *buf, u32 size)
{
	if (buf == NULL || check_uptr(buf, size))
		return 0x8008273b;
	if (sub_6190(cat_h) == 0)
		return 0x8008272e;
	return sub_0a64(cat_h, key_h ^ 0xa0f3d97b, buf, size);
}

int sceReg_driver_D323FC94(REGHANDLE cat_h, u32 *num)
{
	if (num == NULL || check_uptr(num, 4))
		return 0x8008273b;
	if (sub_6190(cat_h) == 0)
		return 0x8008272e;
	return sub_6280(cat_h, num);
}

int sceReg_driver_BAE3371A(REGHANDLE cat_h, char *buf, int num)
{
	if ((u32)num >= 0x20 || buf == NULL || check_uptr(buf, (u32)(num * 27)))
		return 0x8008273b;
	if (sub_6190(cat_h) == 0)
		return 0x8008272e;
	return sub_5d08(cat_h, buf, num);
}

int sceRegGetKeyInfoByName(REGHANDLE cat_h, const char *name, u32 *type, u32 *size)
{
	u32 dummy_id;

	if (name == NULL || check_uptr(name, 0x1b))
		return 0x8008273b;
	if (type != NULL && check_uptr(type, 4))
		return 0x8008273b;
	if (size != NULL && check_uptr(size, 4))
		return 0x8008273b;
	if (sub_6190(cat_h) == 0)
		return 0x8008272e;
	return sub_08d0(cat_h, name, &dummy_id, type, size);
}

int sceRegGetKeyValueByName(REGHANDLE cat_h, const char *name, void *buf, u32 size)
{
	if (name == NULL || check_uptr(name, 0x1b))
		return 0x8008273b;
	if (buf == NULL || check_uptr(buf, size))
		return 0x8008273b;
	if (sub_6190(cat_h) == 0)
		return 0x8008272e;
	return sub_62bc(cat_h, name, buf, size);
}

int sceRegCreateKey(REGHANDLE cat_h, const char *name, int type, u32 size)
{
	(void)cat_h;
	(void)type;
	(void)size;
	if (name == NULL)
		return 0x80082737;
	if (check_uptr(name, 0x1b))
		return 0x8008273b;
	return 0;
}

int sceRegRemoveCategory(REGHANDLE h, const char *name)
{
	(void)h;
	if (name == NULL)
		return 0x80082738;
	if (check_uptr(name, 0x10))
		return 0x8008273b;
	return 0;
}

int module_reboot_before(void)
{
	return 0;
}

int module_reboot_phase(int phase)
{
	if (phase == 2) {
		g_reg_open = 0;
		sub_6164();
	}
	return 0;
}

int sceReg_driver_30E567D5(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	sub_5da8();
	g_reg_open = 0;
	sub_6164();
	return 0;
}

int sceReg_driver_BDE681CF(void)
{
	g_reg_open = 0;
	sub_6164();
	return 0;
}

int sceRegCloseRegistry(REGHANDLE h)
{
	if (h == 0)
		g_reg_open = 0;
	if (sceReg_driver_var_5105E3FF != 0)
		sceKernelDelayThread(sceReg_driver_var_5105E3FF);
	return 0;
}

int sceRegCloseCategory(REGHANDLE cat_h)
{
	return sub_61c0(cat_h);
}

int sceReg_driver_C3DEEBC6(struct RegParam *reg)
{
	if (reg == NULL || check_uptr(reg, sizeof(*reg)))
		return 0x8008273b;
	return 0;
}

int sceReg_driver_405D36C3(REGHANDLE cat_h, const char *name)
{
	(void)cat_h;
	if (name == NULL || check_uptr(name, 0x1b))
		return 0x8008273b;
	return 0;
}

int sceRegFlushRegistry(REGHANDLE h)
{
	(void)h;
	return 0;
}

int sceRegFlushCategory(REGHANDLE cat_h)
{
	(void)cat_h;
	return 0;
}

int sceReg_driver_90AA3904(REGHANDLE h, u32 *num)
{
	(void)h;
	if (num == NULL || check_uptr(num, 4))
		return 0x8008273b;
	return 0;
}

int sceReg_driver_6328E4C6(REGHANDLE h, char *buf, int num)
{
	(void)h;
	if ((u32)num >= 0x40 || buf == NULL || check_uptr(buf, (u32)(num * 27)))
		return 0x8008273b;
	return 0;
}

static __attribute__((noinline)) int sub_08d0(u32 cat_id, const char *name, u32 *out_id, u32 *out_type, u32 *out_size)
{
	u32 i;
	struct RegKeyEntry *ke;

	if (cat_id >= 0x26)
		return 0x8008272e;

	if (cat_id >= 18 && cat_id <= 35) {
		ke = &g_keys[78];
		for (i = 0; i < 16; i++, ke++) {
			if (strncmp(name, ke->name, 0x1b) == 0) {
				if (out_id != NULL)
					*out_id = ke->id + (cat_id << 4) - 0x120;
				if (out_type != NULL)
					*out_type = ke->type;
				if (out_size != NULL)
					*out_size = ke->size;
				return 0;
			}
		}
		return 0x8008272e;
	}

	ke = g_cats[cat_id].keys;
	for (i = 0; i < g_cats[cat_id].num_keys; i++, ke++) {
		if (strncmp(name, ke->name, 0x1b) == 0) {
			if (out_id != NULL)
				*out_id = ke->id;
			if (out_type != NULL)
				*out_type = ke->type;
			if (out_size != NULL)
				*out_size = ke->size;
			return 0;
		}
	}
	return 0x8008272e;
}

static __attribute__((noinline)) int sub_0a64(int cat_id, u32 key_id, void *buf, u32 size)
{
	switch (key_id) {
	case 0x026:
		if (cat_id != 0x0 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x66;
		return 0;
	case 0x027:
		if (cat_id != 1 || (int)size < (int)g_keys[1].size)
			return 0x8008271d;
		strncpy(buf, g_home_uri, g_keys[1].size);
		return 0;
	case 0x028:
		if (cat_id != 0x1 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_cookie_mode;
		return 0;
	case 0x029:
		if (cat_id != 0x1 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_proxy_mode;
		return 0;
	case 0x02a:
		if (cat_id != 1 || (int)size < (int)g_keys[4].size)
			return 0x8008271d;
		strncpy(buf, g_proxy_address, g_keys[4].size);
		return 0;
	case 0x02b:
		if (cat_id != 0x1 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_proxy_port;
		return 0;
	case 0x02c:
		if (cat_id != 0x1 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_picture;
		return 0;
	case 0x02d:
		if (cat_id != 0x1 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_animation;
		return 0;
	case 0x02e:
		if (cat_id != 0x1 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_javascript;
		return 0;
	case 0x02f:
		if (cat_id != 0x1 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_cache_size;
		return 0;
	case 0x030:
		if (cat_id != 0x1 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_char_size;
		return 0;
	case 0x031:
		if (cat_id != 0x1 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_disp_mode;
		return 0;
	case 0x032:
		if (cat_id != 0x1 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_connect_mode;
		return 0;
	case 0x033:
		if (cat_id != 0x1 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x034:
		if (cat_id != 0x1 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x035:
		if (cat_id != 0x1 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_proxy_protect;
		return 0;
	case 0x036:
		if (cat_id != 0x1 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_proxy_autoauth;
		return 0;
	case 0x037:
		if (cat_id != 1 || (int)size < (int)g_keys[17].size)
			return 0x8008271d;
		strncpy(buf, g_proxy_user, g_keys[17].size);
		return 0;
	case 0x038:
		if (cat_id != 1 || (int)size < (int)g_keys[18].size)
			return 0x8008271d;
		strncpy(buf, g_proxy_password, g_keys[18].size);
		return 0;
	case 0x039:
		if (cat_id != 0x1 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_webpage_quality;
		return 0;
	case 0x03a:
		if (cat_id != 0x2 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x03b:
		if (cat_id != 0x2 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_tm_ec_ttl;
		return 0;
	case 0x03c:
		if (cat_id != 0x2 || size != 0x8)
			return 0x8008271d;
		memcpy(buf, g_tm_ec_ttl_update_time, 0x8);
		return 0;
	case 0x03d:
		if (cat_id != 0x2 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_tm_service_sub_status;
		return 0;
	case 0x03e:
		if (cat_id != 0x3 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x03f:
		if (cat_id != 0x4 || size != 0x9)
			return 0x8008271d;
		strncpy(buf, g_np_env, 0x9);
		return 0;
	case 0x040:
		if (cat_id != 0x4 || size != 0x10)
			return 0x8008271d;
		memcpy(buf, g_np_account_id, 0x10);
		return 0;
	case 0x041:
		if (cat_id != 0x4 || size != 0x41)
			return 0x8008271d;
		strncpy(buf, g_np_login_id, 0x41);
		return 0;
	case 0x042:
		if (cat_id != 0x4 || size != 0x1f)
			return 0x8008271d;
		strncpy(buf, g_np_password, 0x1f);
		return 0;
	case 0x043:
		if (cat_id != 0x4 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_np_auto_sign_in_enable;
		return 0;
	case 0x044:
		if (cat_id != 0x4 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_np_nav_only;
		return 0;
	case 0x045:
		if (cat_id != 0x4 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_np_ad_clock_diff;
		return 0;
	case 0x046:
		if (cat_id != 0x4 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_np_view_mode;
		return 0;
	case 0x047:
		if (cat_id != 0x4 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_np_geo_filtering;
		return 0;
	case 0x048:
		if (cat_id != 0x4 || size != 0x3)
			return 0x8008271d;
		strncpy(buf, g_np_guest_country, 0x3);
		return 0;
	case 0x049:
		if (cat_id != 0x4 || size != 0x3)
			return 0x8008271d;
		strncpy(buf, g_np_guest_lang, 0x3);
		return 0;
	case 0x04a:
		if (cat_id != 0x4 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_np_guest_yob;
		return 0;
	case 0x04b:
		if (cat_id != 0x4 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_np_guest_mob;
		return 0;
	case 0x04c:
		if (cat_id != 0x4 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_np_guest_dob;
		return 0;
	case 0x04d:
		if (cat_id != 0x4 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_np_check_drm;
		return 0;
	case 0x04e:
		if (cat_id != 5 || (int)size < (int)g_keys[40].size)
			return 0x8008271d;
		strncpy(buf, g_owner_name, g_keys[40].size);
		return 0;
	case 0x04f:
		if (cat_id != 0x5 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x050:
		if (cat_id != 0x5 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x051:
		if (cat_id != 0x5 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x052:
		if (cat_id == 6 && size == 4) {
			*(u32 *)buf = g_language;
			return 0;
		}
		return 0x8008271d;
	case 0x053:
		if (cat_id == 6 && size == 4) {
			*(u32 *)buf = g_button_assign;
			return 0;
		}
		return 0x8008271d;
	case 0x054:
		if (cat_id == 7 && size == 4) {
			memcpy(buf, g_lock_password, 4);
			return 0;
		}
		return 0x8008271d;
	case 0x055:
		if (cat_id != 0x7 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_parental_level;
		return 0;
	case 0x056:
		if (cat_id != 0x7 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_browser_start;
		return 0;
	case 0x057:
		if (cat_id != 0x9 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x14;
		return 0;
	case 0x058:
		if (cat_id != 0x9 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x059:
		if (cat_id != 0x9 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x05a:
		if (cat_id != 0x9 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x05b:
		if (cat_id != 0x8 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_char_oem;
		return 0;
	case 0x05c:
		if (cat_id != 0x8 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_char_ansi;
		return 0;
	case 0x05d:
		if (cat_id != 0x9 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x258;
		return 0;
	case 0x05e:
		if (cat_id != 0x9 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x12c;
		return 0;
	case 0x05f:
		if (cat_id != 0x9 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x060:
		if (cat_id != 0xb || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_time_format;
		return 0;
	case 0x061:
		if (cat_id != 0xb || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_date_format;
		return 0;
	case 0x062:
		if (cat_id != 0xb || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_summer_time;
		return 0;
	case 0x063:
		if (cat_id != 0xb || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_time_zone_offset;
		return 0;
	case 0x064:
		if (cat_id != 0xb || size != 0x30)
			return 0x8008271d;
		strncpy(buf, g_time_zone_area, 0x30);
		return 0;
	case 0x065:
		if (cat_id != 0xc || size != 4)
			return 0x8008271d;
		*(u32 *)buf = *(volatile u32 *)0xa83ff020;
		return 0;
	case 0x066:
		if (cat_id != 0xc || size != 0x4)
			return 0x8008271d;
		strncpy(buf, g_adhoc_ssid_prefix, 0x4);
		return 0;
	case 0x067:
		if (cat_id != 0xd || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_infra_latest_id;
		return 0;
	case 0x068:
		if (cat_id != 0xd || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_infra_eap_md5;
		return 0;
	case 0x069:
		if (cat_id != 0xd || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_infra_auto_setting;
		return 0;
	case 0x06a:
		if (cat_id != 0xd || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_infra_wifisvc_setting;
		return 0;
	case 0x06b:
		if (cat_id != 0xd || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_infra_btdun_warnings_check;
		return 0;
	case 0x06c:
		if (cat_id != 0xe || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "", 0x40);
		return 0;
	case 0x06d:
		if (cat_id != 0xe || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "", 0x40);
		return 0;
	case 0x06e:
		if (cat_id != 0xf || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_osk_version_id;
		return 0;
	case 0x06f:
		if (cat_id != 0xf || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_osk_disp_locale;
		return 0;
	case 0x070:
		if (cat_id != 0xf || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_osk_writing_locale;
		return 0;
	case 0x071:
		if (cat_id != 0xf || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_osk_input_char_mask;
		return 0;
	case 0x072:
		if (cat_id != 0xf || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_osk_keytop_index;
		return 0;
	case 0x073:
		if (cat_id != 0x10 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_count_boot;
		return 0;
	case 0x074:
		if (cat_id != 0x10 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_count_game_exec;
		return 0;
	case 0x075:
		if (cat_id != 0x10 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_count_slide;
		return 0;
	case 0x076:
		if (cat_id != 0x10 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_count_usb_connect;
		return 0;
	case 0x077:
		if (cat_id != 0x10 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_count_wifi_connect;
		return 0;
	case 0x078:
		if (cat_id != 0x10 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = g_count_psn_access;
		return 0;
	case 0x079:
		if (cat_id != 0x11 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "flash0:/font", 0x40);
		return 0;
	case 0x07a:
		if (cat_id != 0x11 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x12;
		return 0;
	case 0x07b:
		if (cat_id != 0x12 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x288;
		return 0;
	case 0x07c:
		if (cat_id != 0x12 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x288;
		return 0;
	case 0x07d:
		if (cat_id != 0x12 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x07e:
		if (cat_id != 0x12 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x07f:
		if (cat_id != 0x12 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x080:
		if (cat_id != 0x12 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x081:
		if (cat_id != 0x12 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x082:
		if (cat_id != 0x12 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x67;
		return 0;
	case 0x083:
		if (cat_id != 0x12 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x084:
		if (cat_id != 0x12 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x085:
		if (cat_id != 0x12 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x086:
		if (cat_id != 0x12 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x087:
		if (cat_id != 0x12 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "FTT-NewRodin Pro DB", 0x40);
		return 0;
	case 0x088:
		if (cat_id != 0x12 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "jpn0.pgf", 0x40);
		return 0;
	case 0x089:
		if (cat_id != 0x12 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x08a:
		if (cat_id != 0x12 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x08b:
		if (cat_id != 0x13 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x288;
		return 0;
	case 0x08c:
		if (cat_id != 0x13 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x288;
		return 0;
	case 0x08d:
		if (cat_id != 0x13 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x08e:
		if (cat_id != 0x13 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x08f:
		if (cat_id != 0x13 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x090:
		if (cat_id != 0x13 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x091:
		if (cat_id != 0x13 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x092:
		if (cat_id != 0x13 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x093:
		if (cat_id != 0x13 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x094:
		if (cat_id != 0x13 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x095:
		if (cat_id != 0x13 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x096:
		if (cat_id != 0x13 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x097:
		if (cat_id != 0x13 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "FTT-NewRodin Pro Latin", 0x40);
		return 0;
	case 0x098:
		if (cat_id != 0x13 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "ltn0.pgf", 0x40);
		return 0;
	case 0x099:
		if (cat_id != 0x13 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x09a:
		if (cat_id != 0x13 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x09b:
		if (cat_id != 0x14 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x288;
		return 0;
	case 0x09c:
		if (cat_id != 0x14 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x288;
		return 0;
	case 0x09d:
		if (cat_id != 0x14 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x09e:
		if (cat_id != 0x14 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x09f:
		if (cat_id != 0x14 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0a0:
		if (cat_id != 0x14 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0a1:
		if (cat_id != 0x14 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x0a2:
		if (cat_id != 0x14 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x0a3:
		if (cat_id != 0x14 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0a4:
		if (cat_id != 0x14 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x0a5:
		if (cat_id != 0x14 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0a6:
		if (cat_id != 0x14 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x0a7:
		if (cat_id != 0x14 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "FTT-Matisse Pro Latin", 0x40);
		return 0;
	case 0x0a8:
		if (cat_id != 0x14 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "ltn1.pgf", 0x40);
		return 0;
	case 0x0a9:
		if (cat_id != 0x14 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0aa:
		if (cat_id != 0x14 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0ab:
		if (cat_id != 0x15 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x288;
		return 0;
	case 0x0ac:
		if (cat_id != 0x15 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x288;
		return 0;
	case 0x0ad:
		if (cat_id != 0x15 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x0ae:
		if (cat_id != 0x15 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x0af:
		if (cat_id != 0x15 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0b0:
		if (cat_id != 0x15 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0b1:
		if (cat_id != 0x15 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x0b2:
		if (cat_id != 0x15 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x0b3:
		if (cat_id != 0x15 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0b4:
		if (cat_id != 0x15 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x0b5:
		if (cat_id != 0x15 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0b6:
		if (cat_id != 0x15 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x0b7:
		if (cat_id != 0x15 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "FTT-NewRodin Pro Latin", 0x40);
		return 0;
	case 0x0b8:
		if (cat_id != 0x15 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "ltn2.pgf", 0x40);
		return 0;
	case 0x0b9:
		if (cat_id != 0x15 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0ba:
		if (cat_id != 0x15 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0bb:
		if (cat_id != 0x16 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x288;
		return 0;
	case 0x0bc:
		if (cat_id != 0x16 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x288;
		return 0;
	case 0x0bd:
		if (cat_id != 0x16 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x0be:
		if (cat_id != 0x16 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x0bf:
		if (cat_id != 0x16 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0c0:
		if (cat_id != 0x16 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0c1:
		if (cat_id != 0x16 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x0c2:
		if (cat_id != 0x16 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x0c3:
		if (cat_id != 0x16 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0c4:
		if (cat_id != 0x16 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x0c5:
		if (cat_id != 0x16 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0c6:
		if (cat_id != 0x16 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x0c7:
		if (cat_id != 0x16 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "FTT-Matisse Pro Latin", 0x40);
		return 0;
	case 0x0c8:
		if (cat_id != 0x16 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "ltn3.pgf", 0x40);
		return 0;
	case 0x0c9:
		if (cat_id != 0x16 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0ca:
		if (cat_id != 0x16 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0cb:
		if (cat_id != 0x17 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x288;
		return 0;
	case 0x0cc:
		if (cat_id != 0x17 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x288;
		return 0;
	case 0x0cd:
		if (cat_id != 0x17 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x0ce:
		if (cat_id != 0x17 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x0cf:
		if (cat_id != 0x17 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0d0:
		if (cat_id != 0x17 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0d1:
		if (cat_id != 0x17 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x0d2:
		if (cat_id != 0x17 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x5;
		return 0;
	case 0x0d3:
		if (cat_id != 0x17 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0d4:
		if (cat_id != 0x17 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x0d5:
		if (cat_id != 0x17 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0d6:
		if (cat_id != 0x17 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x0d7:
		if (cat_id != 0x17 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "FTT-NewRodin Pro Latin", 0x40);
		return 0;
	case 0x0d8:
		if (cat_id != 0x17 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "ltn4.pgf", 0x40);
		return 0;
	case 0x0d9:
		if (cat_id != 0x17 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0da:
		if (cat_id != 0x17 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0db:
		if (cat_id != 0x18 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x288;
		return 0;
	case 0x0dc:
		if (cat_id != 0x18 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x288;
		return 0;
	case 0x0dd:
		if (cat_id != 0x18 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x0de:
		if (cat_id != 0x18 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x0df:
		if (cat_id != 0x18 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0e0:
		if (cat_id != 0x18 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0e1:
		if (cat_id != 0x18 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x0e2:
		if (cat_id != 0x18 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x5;
		return 0;
	case 0x0e3:
		if (cat_id != 0x18 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0e4:
		if (cat_id != 0x18 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x0e5:
		if (cat_id != 0x18 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0e6:
		if (cat_id != 0x18 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x0e7:
		if (cat_id != 0x18 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "FTT-Matisse Pro Latin", 0x40);
		return 0;
	case 0x0e8:
		if (cat_id != 0x18 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "ltn5.pgf", 0x40);
		return 0;
	case 0x0e9:
		if (cat_id != 0x18 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0ea:
		if (cat_id != 0x18 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0eb:
		if (cat_id != 0x19 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x288;
		return 0;
	case 0x0ec:
		if (cat_id != 0x19 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x288;
		return 0;
	case 0x0ed:
		if (cat_id != 0x19 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x0ee:
		if (cat_id != 0x19 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x0ef:
		if (cat_id != 0x19 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0f0:
		if (cat_id != 0x19 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0f1:
		if (cat_id != 0x19 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x0f2:
		if (cat_id != 0x19 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x6;
		return 0;
	case 0x0f3:
		if (cat_id != 0x19 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0f4:
		if (cat_id != 0x19 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x0f5:
		if (cat_id != 0x19 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0f6:
		if (cat_id != 0x19 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x0f7:
		if (cat_id != 0x19 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "FTT-NewRodin Pro Latin", 0x40);
		return 0;
	case 0x0f8:
		if (cat_id != 0x19 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "ltn6.pgf", 0x40);
		return 0;
	case 0x0f9:
		if (cat_id != 0x19 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0fa:
		if (cat_id != 0x19 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x0fb:
		if (cat_id != 0x1a || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x288;
		return 0;
	case 0x0fc:
		if (cat_id != 0x1a || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x288;
		return 0;
	case 0x0fd:
		if (cat_id != 0x1a || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x0fe:
		if (cat_id != 0x1a || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x0ff:
		if (cat_id != 0x1a || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x100:
		if (cat_id != 0x1a || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x101:
		if (cat_id != 0x1a || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x102:
		if (cat_id != 0x1a || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x6;
		return 0;
	case 0x103:
		if (cat_id != 0x1a || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x104:
		if (cat_id != 0x1a || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x105:
		if (cat_id != 0x1a || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x106:
		if (cat_id != 0x1a || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x107:
		if (cat_id != 0x1a || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "FTT-Matisse Pro Latin", 0x40);
		return 0;
	case 0x108:
		if (cat_id != 0x1a || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "ltn7.pgf", 0x40);
		return 0;
	case 0x109:
		if (cat_id != 0x1a || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x10a:
		if (cat_id != 0x1a || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x10b:
		if (cat_id != 0x1b || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1c0;
		return 0;
	case 0x10c:
		if (cat_id != 0x1b || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1c0;
		return 0;
	case 0x10d:
		if (cat_id != 0x1b || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x10e:
		if (cat_id != 0x1b || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x10f:
		if (cat_id != 0x1b || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x110:
		if (cat_id != 0x1b || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x111:
		if (cat_id != 0x1b || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x112:
		if (cat_id != 0x1b || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x113:
		if (cat_id != 0x1b || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x114:
		if (cat_id != 0x1b || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x115:
		if (cat_id != 0x1b || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x116:
		if (cat_id != 0x1b || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x117:
		if (cat_id != 0x1b || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "FTT-NewRodin Pro Latin", 0x40);
		return 0;
	case 0x118:
		if (cat_id != 0x1b || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "ltn8.pgf", 0x40);
		return 0;
	case 0x119:
		if (cat_id != 0x1b || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x11a:
		if (cat_id != 0x1b || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x11b:
		if (cat_id != 0x1c || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1c0;
		return 0;
	case 0x11c:
		if (cat_id != 0x1c || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1c0;
		return 0;
	case 0x11d:
		if (cat_id != 0x1c || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x11e:
		if (cat_id != 0x1c || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x11f:
		if (cat_id != 0x1c || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x120:
		if (cat_id != 0x1c || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x121:
		if (cat_id != 0x1c || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x122:
		if (cat_id != 0x1c || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x123:
		if (cat_id != 0x1c || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x124:
		if (cat_id != 0x1c || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x125:
		if (cat_id != 0x1c || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x126:
		if (cat_id != 0x1c || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x127:
		if (cat_id != 0x1c || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "FTT-Matisse Pro Latin", 0x40);
		return 0;
	case 0x128:
		if (cat_id != 0x1c || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "ltn9.pgf", 0x40);
		return 0;
	case 0x129:
		if (cat_id != 0x1c || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x12a:
		if (cat_id != 0x1c || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x12b:
		if (cat_id != 0x1d || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1c0;
		return 0;
	case 0x12c:
		if (cat_id != 0x1d || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1c0;
		return 0;
	case 0x12d:
		if (cat_id != 0x1d || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x12e:
		if (cat_id != 0x1d || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x12f:
		if (cat_id != 0x1d || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x130:
		if (cat_id != 0x1d || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x131:
		if (cat_id != 0x1d || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x132:
		if (cat_id != 0x1d || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x133:
		if (cat_id != 0x1d || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x134:
		if (cat_id != 0x1d || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x135:
		if (cat_id != 0x1d || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x136:
		if (cat_id != 0x1d || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x137:
		if (cat_id != 0x1d || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "FTT-NewRodin Pro Latin", 0x40);
		return 0;
	case 0x138:
		if (cat_id != 0x1d || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "ltn10.pgf", 0x40);
		return 0;
	case 0x139:
		if (cat_id != 0x1d || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x13a:
		if (cat_id != 0x1d || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x13b:
		if (cat_id != 0x1e || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1c0;
		return 0;
	case 0x13c:
		if (cat_id != 0x1e || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1c0;
		return 0;
	case 0x13d:
		if (cat_id != 0x1e || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x13e:
		if (cat_id != 0x1e || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x13f:
		if (cat_id != 0x1e || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x140:
		if (cat_id != 0x1e || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x141:
		if (cat_id != 0x1e || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x142:
		if (cat_id != 0x1e || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x143:
		if (cat_id != 0x1e || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x144:
		if (cat_id != 0x1e || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x145:
		if (cat_id != 0x1e || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x146:
		if (cat_id != 0x1e || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x147:
		if (cat_id != 0x1e || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "FTT-Matisse Pro Latin", 0x40);
		return 0;
	case 0x148:
		if (cat_id != 0x1e || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "ltn11.pgf", 0x40);
		return 0;
	case 0x149:
		if (cat_id != 0x1e || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x14a:
		if (cat_id != 0x1e || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x14b:
		if (cat_id != 0x1f || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1c0;
		return 0;
	case 0x14c:
		if (cat_id != 0x1f || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1c0;
		return 0;
	case 0x14d:
		if (cat_id != 0x1f || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x14e:
		if (cat_id != 0x1f || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x14f:
		if (cat_id != 0x1f || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x150:
		if (cat_id != 0x1f || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x151:
		if (cat_id != 0x1f || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x152:
		if (cat_id != 0x1f || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x5;
		return 0;
	case 0x153:
		if (cat_id != 0x1f || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x154:
		if (cat_id != 0x1f || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x155:
		if (cat_id != 0x1f || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x156:
		if (cat_id != 0x1f || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x157:
		if (cat_id != 0x1f || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "FTT-NewRodin Pro Latin", 0x40);
		return 0;
	case 0x158:
		if (cat_id != 0x1f || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "ltn12.pgf", 0x40);
		return 0;
	case 0x159:
		if (cat_id != 0x1f || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x15a:
		if (cat_id != 0x1f || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x15b:
		if (cat_id != 0x20 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1c0;
		return 0;
	case 0x15c:
		if (cat_id != 0x20 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1c0;
		return 0;
	case 0x15d:
		if (cat_id != 0x20 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x15e:
		if (cat_id != 0x20 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x15f:
		if (cat_id != 0x20 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x160:
		if (cat_id != 0x20 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x161:
		if (cat_id != 0x20 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x162:
		if (cat_id != 0x20 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x5;
		return 0;
	case 0x163:
		if (cat_id != 0x20 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x164:
		if (cat_id != 0x20 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x165:
		if (cat_id != 0x20 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x166:
		if (cat_id != 0x20 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x167:
		if (cat_id != 0x20 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "FTT-Matisse Pro Latin", 0x40);
		return 0;
	case 0x168:
		if (cat_id != 0x20 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "ltn13.pgf", 0x40);
		return 0;
	case 0x169:
		if (cat_id != 0x20 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x16a:
		if (cat_id != 0x20 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x16b:
		if (cat_id != 0x21 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1c0;
		return 0;
	case 0x16c:
		if (cat_id != 0x21 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1c0;
		return 0;
	case 0x16d:
		if (cat_id != 0x21 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x16e:
		if (cat_id != 0x21 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x16f:
		if (cat_id != 0x21 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x170:
		if (cat_id != 0x21 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x171:
		if (cat_id != 0x21 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x172:
		if (cat_id != 0x21 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x6;
		return 0;
	case 0x173:
		if (cat_id != 0x21 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x174:
		if (cat_id != 0x21 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x175:
		if (cat_id != 0x21 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x176:
		if (cat_id != 0x21 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x177:
		if (cat_id != 0x21 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "FTT-NewRodin Pro Latin", 0x40);
		return 0;
	case 0x178:
		if (cat_id != 0x21 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "ltn14.pgf", 0x40);
		return 0;
	case 0x179:
		if (cat_id != 0x21 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x17a:
		if (cat_id != 0x21 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x17b:
		if (cat_id != 0x22 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1c0;
		return 0;
	case 0x17c:
		if (cat_id != 0x22 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1c0;
		return 0;
	case 0x17d:
		if (cat_id != 0x22 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x17e:
		if (cat_id != 0x22 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x17f:
		if (cat_id != 0x22 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x180:
		if (cat_id != 0x22 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x181:
		if (cat_id != 0x22 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x182:
		if (cat_id != 0x22 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x6;
		return 0;
	case 0x183:
		if (cat_id != 0x22 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x184:
		if (cat_id != 0x22 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2;
		return 0;
	case 0x185:
		if (cat_id != 0x22 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x186:
		if (cat_id != 0x22 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x187:
		if (cat_id != 0x22 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "FTT-Matisse Pro Latin", 0x40);
		return 0;
	case 0x188:
		if (cat_id != 0x22 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "ltn15.pgf", 0x40);
		return 0;
	case 0x189:
		if (cat_id != 0x22 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x18a:
		if (cat_id != 0x22 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x18b:
		if (cat_id != 0x23 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x288;
		return 0;
	case 0x18c:
		if (cat_id != 0x23 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x288;
		return 0;
	case 0x18d:
		if (cat_id != 0x23 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x18e:
		if (cat_id != 0x23 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2000;
		return 0;
	case 0x18f:
		if (cat_id != 0x23 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x190:
		if (cat_id != 0x23 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x191:
		if (cat_id != 0x23 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x192:
		if (cat_id != 0x23 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x1;
		return 0;
	case 0x193:
		if (cat_id != 0x23 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x194:
		if (cat_id != 0x23 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x3;
		return 0;
	case 0x195:
		if (cat_id != 0x23 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x196:
		if (cat_id != 0x23 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x3;
		return 0;
	case 0x197:
		if (cat_id != 0x23 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "AsiaNHH(512Johab)", 0x40);
		return 0;
	case 0x198:
		if (cat_id != 0x23 || size != 0x40)
			return 0x8008271d;
		strncpy(buf, "kr0.pgf", 0x40);
		return 0;
	case 0x199:
		if (cat_id != 0x23 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x19a:
		if (cat_id != 0x23 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x19b:
		if (cat_id != 0x24 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0;
		return 0;
	case 0x19c:
		if (cat_id != 0x25 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2012;
		return 0;
	case 0x19d:
		if (cat_id != 0x25 || size != 0x4)
			return 0x8008271d;
		*(u32 *)buf = 0x2012;
		return 0;
	default:
		return 0x8008271d;
	}
}

static __attribute__((noinline)) int sub_4ce4(int cat_id, const char *name, const void *buf, u32 size)
{
	u32 key_id, type, ksize;
	int res;

	res = sub_08d0(cat_id, name, &key_id, &type, &ksize);
	if (res != 0)
		return res;
	switch (key_id) {
	case 0x026:
		if (cat_id == 0 && size == ksize)
			return 0;
		return 0x8008271d;
	case 0x027:
		return 0;
	case 0x028:
		if (cat_id != 0x1 || size != ksize)
			return 0x8008271d;
		g_cookie_mode = *(const u32 *)buf;
		return 0;
	case 0x029:
		if (cat_id != 0x1 || size != ksize)
			return 0x8008271d;
		g_proxy_mode = *(const u32 *)buf;
		return 0;
	case 0x02a:
		return 0;
	case 0x02b:
		if (cat_id != 0x1 || size != ksize)
			return 0x8008271d;
		g_proxy_port = *(const u32 *)buf;
		return 0;
	case 0x02c:
		if (cat_id != 0x1 || size != ksize)
			return 0x8008271d;
		g_picture = *(const u32 *)buf;
		return 0;
	case 0x02d:
		if (cat_id != 0x1 || size != ksize)
			return 0x8008271d;
		g_animation = *(const u32 *)buf;
		return 0;
	case 0x02e:
		if (cat_id != 0x1 || size != ksize)
			return 0x8008271d;
		g_javascript = *(const u32 *)buf;
		return 0;
	case 0x02f:
		if (cat_id != 0x1 || size != ksize)
			return 0x8008271d;
		g_cache_size = *(const u32 *)buf;
		return 0;
	case 0x030:
		if (cat_id != 0x1 || size != ksize)
			return 0x8008271d;
		g_char_size = *(const u32 *)buf;
		return 0;
	case 0x031:
		if (cat_id != 0x1 || size != ksize)
			return 0x8008271d;
		g_disp_mode = *(const u32 *)buf;
		return 0;
	case 0x032:
		if (cat_id != 0x1 || size != ksize)
			return 0x8008271d;
		g_connect_mode = *(const u32 *)buf;
		return 0;
	case 0x033:
		if (cat_id != 0x1 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x034:
		if (cat_id != 0x1 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x035:
		if (cat_id != 0x1 || size != ksize)
			return 0x8008271d;
		g_proxy_protect = *(const u32 *)buf;
		return 0;
	case 0x036:
		if (cat_id != 0x1 || size != ksize)
			return 0x8008271d;
		g_proxy_autoauth = *(const u32 *)buf;
		return 0;
	case 0x037:
		return 0;
	case 0x038:
		return 0;
	case 0x039:
		if (cat_id != 0x1 || size != ksize)
			return 0x8008271d;
		g_webpage_quality = *(const u32 *)buf;
		return 0;
	case 0x03a:
		if (cat_id != 0x2 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x03b:
		if (cat_id != 0x2 || size != ksize)
			return 0x8008271d;
		g_tm_ec_ttl = *(const u32 *)buf;
		return 0;
	case 0x03c:
		if (cat_id != 0x2 || size != ksize)
			return 0x8008271d;
		memcpy(g_tm_ec_ttl_update_time, buf, size);
		return 0;
	case 0x03d:
		if (cat_id != 0x2 || size != ksize)
			return 0x8008271d;
		g_tm_service_sub_status = *(const u32 *)buf;
		return 0;
	case 0x03e:
		if (cat_id != 0x3 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x03f:
		if (cat_id != 0x4 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x040:
		if (cat_id != 0x4 || size != ksize)
			return 0x8008271d;
		memcpy(g_np_account_id, buf, size);
		return 0;
	case 0x041:
		if (cat_id != 0x4 || size != ksize)
			return 0x8008271d;
		strncpy(g_np_login_id, buf, size);
		return 0;
	case 0x042:
		if (cat_id != 0x4 || size != ksize)
			return 0x8008271d;
		strncpy(g_np_password, buf, size);
		return 0;
	case 0x043:
		if (cat_id != 0x4 || size != ksize)
			return 0x8008271d;
		g_np_auto_sign_in_enable = *(const u32 *)buf;
		return 0;
	case 0x044:
		if (cat_id != 0x4 || size != ksize)
			return 0x8008271d;
		g_np_nav_only = *(const u32 *)buf;
		return 0;
	case 0x045:
		if (cat_id != 0x4 || size != ksize)
			return 0x8008271d;
		g_np_ad_clock_diff = *(const u32 *)buf;
		return 0;
	case 0x046:
		if (cat_id != 0x4 || size != ksize)
			return 0x8008271d;
		g_np_view_mode = *(const u32 *)buf;
		return 0;
	case 0x047:
		if (cat_id != 0x4 || size != ksize)
			return 0x8008271d;
		g_np_geo_filtering = *(const u32 *)buf;
		return 0;
	case 0x048:
		if (cat_id != 0x4 || size != ksize)
			return 0x8008271d;
		strncpy(g_np_guest_country, buf, size);
		return 0;
	case 0x049:
		if (cat_id != 0x4 || size != ksize)
			return 0x8008271d;
		strncpy(g_np_guest_lang, buf, size);
		return 0;
	case 0x04a:
		if (cat_id != 0x4 || size != ksize)
			return 0x8008271d;
		g_np_guest_yob = *(const u32 *)buf;
		return 0;
	case 0x04b:
		if (cat_id != 0x4 || size != ksize)
			return 0x8008271d;
		g_np_guest_mob = *(const u32 *)buf;
		return 0;
	case 0x04c:
		if (cat_id != 0x4 || size != ksize)
			return 0x8008271d;
		g_np_guest_dob = *(const u32 *)buf;
		return 0;
	case 0x04d:
		if (cat_id != 0x4 || size != ksize)
			return 0x8008271d;
		g_np_check_drm = *(const u32 *)buf;
		return 0;
	case 0x04e:
		if (cat_id != 0x5 || size != ksize)
			return 0x8008271d;
		strncpy(g_owner_name, buf, size);
		return 0;
	case 0x04f:
		if (cat_id != 0x5 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x050:
		if (cat_id != 0x5 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x051:
		if (cat_id != 0x5 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x052:
		if (cat_id == 6 && size == ksize) {
			g_language = *(const u32 *)buf;
			return 0;
		}
		return 0x8008271d;
	case 0x053:
		if (cat_id == 6 && size == ksize) {
			g_button_assign = *(const u32 *)buf;
			return 0;
		}
		return 0x8008271d;
	case 0x054:
		if (cat_id == 7 && size == ksize)
			return 0;
		return 0x8008271d;
	case 0x055:
		if (cat_id != 0x7 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x056:
		if (cat_id != 0x7 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x057:
		if (cat_id != 0x9 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x058:
		if (cat_id != 0x9 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x059:
		if (cat_id != 0x9 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x05a:
		if (cat_id != 0x9 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x05b:
		if (cat_id != 0x8 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x05c:
		if (cat_id != 0x8 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x05d:
		if (cat_id != 0x9 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x05e:
		if (cat_id != 0x9 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x05f:
		if (cat_id != 0x9 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x060:
		if (cat_id != 0xb || size != ksize)
			return 0x8008271d;
		g_time_format = *(const u32 *)buf;
		return 0;
	case 0x061:
		if (cat_id != 0xb || size != ksize)
			return 0x8008271d;
		g_date_format = *(const u32 *)buf;
		return 0;
	case 0x062:
		if (cat_id != 0xb || size != ksize)
			return 0x8008271d;
		g_summer_time = *(const u32 *)buf;
		return 0;
	case 0x063:
		if (cat_id != 0xb || size != ksize)
			return 0x8008271d;
		g_time_zone_offset = *(const u32 *)buf;
		return 0;
	case 0x064:
		if (cat_id != 0xb || size != ksize)
			return 0x8008271d;
		strncpy(g_time_zone_area, buf, size);
		return 0;
	case 0x065:
		if (cat_id != 0xc || size != ksize)
			return 0x8008271d;
		*(volatile u32 *)0xa83ff020 = *(const u32 *)buf;
		return 0;
	case 0x066:
		if (cat_id != 0xc || size != ksize)
			return 0x8008271d;
		strncpy(g_adhoc_ssid_prefix, buf, size);
		return 0;
	case 0x067:
		if (cat_id != 0xd || size != ksize)
			return 0x8008271d;
		g_infra_latest_id = *(const u32 *)buf;
		return 0;
	case 0x068:
		if (cat_id != 0xd || size != ksize)
			return 0x8008271d;
		g_infra_eap_md5 = *(const u32 *)buf;
		return 0;
	case 0x069:
		if (cat_id != 0xd || size != ksize)
			return 0x8008271d;
		g_infra_auto_setting = *(const u32 *)buf;
		return 0;
	case 0x06a:
		if (cat_id != 0xd || size != ksize)
			return 0x8008271d;
		g_infra_wifisvc_setting = *(const u32 *)buf;
		return 0;
	case 0x06b:
		if (cat_id != 0xd || size != ksize)
			return 0x8008271d;
		g_infra_btdun_warnings_check = *(const u32 *)buf;
		return 0;
	case 0x06c:
		if (cat_id != 0xe || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x06d:
		if (cat_id != 0xe || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x06e:
		if (cat_id != 0xf || size != ksize)
			return 0x8008271d;
		g_osk_version_id = *(const u32 *)buf;
		return 0;
	case 0x06f:
		if (cat_id != 0xf || size != ksize)
			return 0x8008271d;
		g_osk_disp_locale = *(const u32 *)buf;
		return 0;
	case 0x070:
		if (cat_id != 0xf || size != ksize)
			return 0x8008271d;
		g_osk_writing_locale = *(const u32 *)buf;
		return 0;
	case 0x071:
		if (cat_id != 0xf || size != ksize)
			return 0x8008271d;
		g_osk_input_char_mask = *(const u32 *)buf;
		return 0;
	case 0x072:
		if (cat_id != 0xf || size != ksize)
			return 0x8008271d;
		g_osk_keytop_index = *(const u32 *)buf;
		return 0;
	case 0x073:
		if (cat_id != 0x10 || size != ksize)
			return 0x8008271d;
		g_count_boot = *(const u32 *)buf;
		return 0;
	case 0x074:
		if (cat_id != 0x10 || size != ksize)
			return 0x8008271d;
		g_count_game_exec = *(const u32 *)buf;
		return 0;
	case 0x075:
		if (cat_id != 0x10 || size != ksize)
			return 0x8008271d;
		g_count_slide = *(const u32 *)buf;
		return 0;
	case 0x076:
		if (cat_id != 0x10 || size != ksize)
			return 0x8008271d;
		g_count_usb_connect = *(const u32 *)buf;
		return 0;
	case 0x077:
		if (cat_id != 0x10 || size != ksize)
			return 0x8008271d;
		g_count_wifi_connect = *(const u32 *)buf;
		return 0;
	case 0x078:
		if (cat_id != 0x10 || size != ksize)
			return 0x8008271d;
		g_count_psn_access = *(const u32 *)buf;
		return 0;
	case 0x079:
		if (cat_id != 0x11 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x07a:
		if (cat_id != 0x11 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x07b:
		if (cat_id != 0x12 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x07c:
		if (cat_id != 0x12 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x07d:
		if (cat_id != 0x12 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x07e:
		if (cat_id != 0x12 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x07f:
		if (cat_id != 0x12 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x080:
		if (cat_id != 0x12 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x081:
		if (cat_id != 0x12 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x082:
		if (cat_id != 0x12 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x083:
		if (cat_id != 0x12 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x084:
		if (cat_id != 0x12 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x085:
		if (cat_id != 0x12 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x086:
		if (cat_id != 0x12 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x087:
		if (cat_id != 0x12 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x088:
		if (cat_id != 0x12 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x089:
		if (cat_id != 0x12 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x08a:
		if (cat_id != 0x12 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x08b:
		if (cat_id != 0x13 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x08c:
		if (cat_id != 0x13 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x08d:
		if (cat_id != 0x13 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x08e:
		if (cat_id != 0x13 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x08f:
		if (cat_id != 0x13 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x090:
		if (cat_id != 0x13 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x091:
		if (cat_id != 0x13 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x092:
		if (cat_id != 0x13 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x093:
		if (cat_id != 0x13 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x094:
		if (cat_id != 0x13 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x095:
		if (cat_id != 0x13 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x096:
		if (cat_id != 0x13 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x097:
		if (cat_id != 0x13 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x098:
		if (cat_id != 0x13 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x099:
		if (cat_id != 0x13 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x09a:
		if (cat_id != 0x13 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x09b:
		if (cat_id != 0x14 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x09c:
		if (cat_id != 0x14 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x09d:
		if (cat_id != 0x14 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x09e:
		if (cat_id != 0x14 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x09f:
		if (cat_id != 0x14 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0a0:
		if (cat_id != 0x14 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0a1:
		if (cat_id != 0x14 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0a2:
		if (cat_id != 0x14 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0a3:
		if (cat_id != 0x14 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0a4:
		if (cat_id != 0x14 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0a5:
		if (cat_id != 0x14 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0a6:
		if (cat_id != 0x14 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0a7:
		if (cat_id != 0x14 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0a8:
		if (cat_id != 0x14 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0a9:
		if (cat_id != 0x14 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0aa:
		if (cat_id != 0x14 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0ab:
		if (cat_id != 0x15 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0ac:
		if (cat_id != 0x15 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0ad:
		if (cat_id != 0x15 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0ae:
		if (cat_id != 0x15 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0af:
		if (cat_id != 0x15 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0b0:
		if (cat_id != 0x15 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0b1:
		if (cat_id != 0x15 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0b2:
		if (cat_id != 0x15 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0b3:
		if (cat_id != 0x15 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0b4:
		if (cat_id != 0x15 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0b5:
		if (cat_id != 0x15 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0b6:
		if (cat_id != 0x15 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0b7:
		if (cat_id != 0x15 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0b8:
		if (cat_id != 0x15 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0b9:
		if (cat_id != 0x15 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0ba:
		if (cat_id != 0x15 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0bb:
		if (cat_id != 0x16 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0bc:
		if (cat_id != 0x16 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0bd:
		if (cat_id != 0x16 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0be:
		if (cat_id != 0x16 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0bf:
		if (cat_id != 0x16 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0c0:
		if (cat_id != 0x16 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0c1:
		if (cat_id != 0x16 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0c2:
		if (cat_id != 0x16 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0c3:
		if (cat_id != 0x16 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0c4:
		if (cat_id != 0x16 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0c5:
		if (cat_id != 0x16 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0c6:
		if (cat_id != 0x16 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0c7:
		if (cat_id != 0x16 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0c8:
		if (cat_id != 0x16 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0c9:
		if (cat_id != 0x16 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0ca:
		if (cat_id != 0x16 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0cb:
		if (cat_id != 0x17 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0cc:
		if (cat_id != 0x17 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0cd:
		if (cat_id != 0x17 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0ce:
		if (cat_id != 0x17 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0cf:
		if (cat_id != 0x17 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0d0:
		if (cat_id != 0x17 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0d1:
		if (cat_id != 0x17 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0d2:
		if (cat_id != 0x17 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0d3:
		if (cat_id != 0x17 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0d4:
		if (cat_id != 0x17 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0d5:
		if (cat_id != 0x17 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0d6:
		if (cat_id != 0x17 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0d7:
		if (cat_id != 0x17 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0d8:
		if (cat_id != 0x17 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0d9:
		if (cat_id != 0x17 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0da:
		if (cat_id != 0x17 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0db:
		if (cat_id != 0x18 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0dc:
		if (cat_id != 0x18 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0dd:
		if (cat_id != 0x18 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0de:
		if (cat_id != 0x18 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0df:
		if (cat_id != 0x18 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0e0:
		if (cat_id != 0x18 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0e1:
		if (cat_id != 0x18 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0e2:
		if (cat_id != 0x18 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0e3:
		if (cat_id != 0x18 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0e4:
		if (cat_id != 0x18 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0e5:
		if (cat_id != 0x18 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0e6:
		if (cat_id != 0x18 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0e7:
		if (cat_id != 0x18 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0e8:
		if (cat_id != 0x18 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0e9:
		if (cat_id != 0x18 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0ea:
		if (cat_id != 0x18 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0eb:
		if (cat_id != 0x19 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0ec:
		if (cat_id != 0x19 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0ed:
		if (cat_id != 0x19 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0ee:
		if (cat_id != 0x19 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0ef:
		if (cat_id != 0x19 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0f0:
		if (cat_id != 0x19 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0f1:
		if (cat_id != 0x19 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0f2:
		if (cat_id != 0x19 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0f3:
		if (cat_id != 0x19 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0f4:
		if (cat_id != 0x19 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0f5:
		if (cat_id != 0x19 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0f6:
		if (cat_id != 0x19 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0f7:
		if (cat_id != 0x19 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0f8:
		if (cat_id != 0x19 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0f9:
		if (cat_id != 0x19 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0fa:
		if (cat_id != 0x19 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0fb:
		if (cat_id != 0x1a || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0fc:
		if (cat_id != 0x1a || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0fd:
		if (cat_id != 0x1a || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0fe:
		if (cat_id != 0x1a || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x0ff:
		if (cat_id != 0x1a || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x100:
		if (cat_id != 0x1a || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x101:
		if (cat_id != 0x1a || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x102:
		if (cat_id != 0x1a || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x103:
		if (cat_id != 0x1a || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x104:
		if (cat_id != 0x1a || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x105:
		if (cat_id != 0x1a || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x106:
		if (cat_id != 0x1a || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x107:
		if (cat_id != 0x1a || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x108:
		if (cat_id != 0x1a || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x109:
		if (cat_id != 0x1a || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x10a:
		if (cat_id != 0x1a || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x10b:
		if (cat_id != 0x1b || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x10c:
		if (cat_id != 0x1b || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x10d:
		if (cat_id != 0x1b || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x10e:
		if (cat_id != 0x1b || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x10f:
		if (cat_id != 0x1b || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x110:
		if (cat_id != 0x1b || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x111:
		if (cat_id != 0x1b || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x112:
		if (cat_id != 0x1b || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x113:
		if (cat_id != 0x1b || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x114:
		if (cat_id != 0x1b || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x115:
		if (cat_id != 0x1b || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x116:
		if (cat_id != 0x1b || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x117:
		if (cat_id != 0x1b || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x118:
		if (cat_id != 0x1b || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x119:
		if (cat_id != 0x1b || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x11a:
		if (cat_id != 0x1b || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x11b:
		if (cat_id != 0x1c || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x11c:
		if (cat_id != 0x1c || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x11d:
		if (cat_id != 0x1c || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x11e:
		if (cat_id != 0x1c || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x11f:
		if (cat_id != 0x1c || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x120:
		if (cat_id != 0x1c || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x121:
		if (cat_id != 0x1c || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x122:
		if (cat_id != 0x1c || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x123:
		if (cat_id != 0x1c || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x124:
		if (cat_id != 0x1c || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x125:
		if (cat_id != 0x1c || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x126:
		if (cat_id != 0x1c || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x127:
		if (cat_id != 0x1c || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x128:
		if (cat_id != 0x1c || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x129:
		if (cat_id != 0x1c || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x12a:
		if (cat_id != 0x1c || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x12b:
		if (cat_id != 0x1d || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x12c:
		if (cat_id != 0x1d || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x12d:
		if (cat_id != 0x1d || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x12e:
		if (cat_id != 0x1d || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x12f:
		if (cat_id != 0x1d || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x130:
		if (cat_id != 0x1d || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x131:
		if (cat_id != 0x1d || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x132:
		if (cat_id != 0x1d || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x133:
		if (cat_id != 0x1d || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x134:
		if (cat_id != 0x1d || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x135:
		if (cat_id != 0x1d || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x136:
		if (cat_id != 0x1d || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x137:
		if (cat_id != 0x1d || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x138:
		if (cat_id != 0x1d || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x139:
		if (cat_id != 0x1d || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x13a:
		if (cat_id != 0x1d || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x13b:
		if (cat_id != 0x1e || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x13c:
		if (cat_id != 0x1e || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x13d:
		if (cat_id != 0x1e || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x13e:
		if (cat_id != 0x1e || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x13f:
		if (cat_id != 0x1e || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x140:
		if (cat_id != 0x1e || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x141:
		if (cat_id != 0x1e || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x142:
		if (cat_id != 0x1e || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x143:
		if (cat_id != 0x1e || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x144:
		if (cat_id != 0x1e || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x145:
		if (cat_id != 0x1e || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x146:
		if (cat_id != 0x1e || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x147:
		if (cat_id != 0x1e || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x148:
		if (cat_id != 0x1e || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x149:
		if (cat_id != 0x1e || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x14a:
		if (cat_id != 0x1e || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x14b:
		if (cat_id != 0x1f || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x14c:
		if (cat_id != 0x1f || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x14d:
		if (cat_id != 0x1f || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x14e:
		if (cat_id != 0x1f || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x14f:
		if (cat_id != 0x1f || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x150:
		if (cat_id != 0x1f || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x151:
		if (cat_id != 0x1f || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x152:
		if (cat_id != 0x1f || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x153:
		if (cat_id != 0x1f || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x154:
		if (cat_id != 0x1f || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x155:
		if (cat_id != 0x1f || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x156:
		if (cat_id != 0x1f || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x157:
		if (cat_id != 0x1f || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x158:
		if (cat_id != 0x1f || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x159:
		if (cat_id != 0x1f || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x15a:
		if (cat_id != 0x1f || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x15b:
		if (cat_id != 0x20 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x15c:
		if (cat_id != 0x20 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x15d:
		if (cat_id != 0x20 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x15e:
		if (cat_id != 0x20 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x15f:
		if (cat_id != 0x20 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x160:
		if (cat_id != 0x20 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x161:
		if (cat_id != 0x20 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x162:
		if (cat_id != 0x20 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x163:
		if (cat_id != 0x20 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x164:
		if (cat_id != 0x20 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x165:
		if (cat_id != 0x20 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x166:
		if (cat_id != 0x20 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x167:
		if (cat_id != 0x20 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x168:
		if (cat_id != 0x20 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x169:
		if (cat_id != 0x20 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x16a:
		if (cat_id != 0x20 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x16b:
		if (cat_id != 0x21 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x16c:
		if (cat_id != 0x21 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x16d:
		if (cat_id != 0x21 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x16e:
		if (cat_id != 0x21 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x16f:
		if (cat_id != 0x21 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x170:
		if (cat_id != 0x21 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x171:
		if (cat_id != 0x21 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x172:
		if (cat_id != 0x21 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x173:
		if (cat_id != 0x21 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x174:
		if (cat_id != 0x21 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x175:
		if (cat_id != 0x21 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x176:
		if (cat_id != 0x21 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x177:
		if (cat_id != 0x21 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x178:
		if (cat_id != 0x21 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x179:
		if (cat_id != 0x21 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x17a:
		if (cat_id != 0x21 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x17b:
		if (cat_id != 0x22 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x17c:
		if (cat_id != 0x22 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x17d:
		if (cat_id != 0x22 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x17e:
		if (cat_id != 0x22 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x17f:
		if (cat_id != 0x22 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x180:
		if (cat_id != 0x22 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x181:
		if (cat_id != 0x22 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x182:
		if (cat_id != 0x22 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x183:
		if (cat_id != 0x22 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x184:
		if (cat_id != 0x22 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x185:
		if (cat_id != 0x22 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x186:
		if (cat_id != 0x22 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x187:
		if (cat_id != 0x22 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x188:
		if (cat_id != 0x22 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x189:
		if (cat_id != 0x22 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x18a:
		if (cat_id != 0x22 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x18b:
		if (cat_id != 0x23 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x18c:
		if (cat_id != 0x23 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x18d:
		if (cat_id != 0x23 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x18e:
		if (cat_id != 0x23 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x18f:
		if (cat_id != 0x23 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x190:
		if (cat_id != 0x23 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x191:
		if (cat_id != 0x23 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x192:
		if (cat_id != 0x23 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x193:
		if (cat_id != 0x23 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x194:
		if (cat_id != 0x23 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x195:
		if (cat_id != 0x23 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x196:
		if (cat_id != 0x23 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x197:
		if (cat_id != 0x23 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x198:
		if (cat_id != 0x23 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x199:
		if (cat_id != 0x23 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x19a:
		if (cat_id != 0x23 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x19b:
		if (cat_id != 0x24 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x19c:
		if (cat_id != 0x25 || size != ksize)
			return 0x8008271d;
		return 0;
	case 0x19d:
		if (cat_id != 0x25 || size != ksize)
			return 0x8008271d;
		return 0;
	default:
		return 0x8008271d;
	}
}

static __attribute__((noinline)) int sub_5d08(u32 cat_id, char *buf, int count)
{
	int n, i;
	struct RegKeyEntry *ke;

	if (cat_id >= 0x24)
		return 0x8008272e;
	n = (int)g_cats[cat_id].num_keys;
	if (count < n)
		n = count;
	ke = g_cats[cat_id].keys;
	for (i = 0; i < n; i++) {
		memset(buf, 0, 0x1b);
		strncpy(buf, ke->name, 0x1a);
		buf += 0x1b;
	}
	return 0;
}

static __attribute__((noinline)) void sub_5da8(void)
{
	volatile u32 *cfg = (volatile u32 *)0xa83ff000;

	memset(g_home_uri, 0, 0x400);
	g_keys[1].size = 1;
	g_cookie_mode = 1;
	g_proxy_mode = 2;
	memset(g_proxy_address, 0, 0x80);
	g_keys[4].size = 1;
	g_proxy_port = 0;
	g_picture = 1;
	g_animation = 1;
	g_javascript = 1;
	g_cache_size = 0x200;
	g_char_size = 1;
	g_disp_mode = 0;
	g_connect_mode = 0;
	g_proxy_protect = 0;
	g_proxy_autoauth = 0;
	memset(g_proxy_user, 0, 0x80);
	g_keys[17].size = 1;
	memset(g_proxy_password, 0, 0x80);
	g_keys[18].size = 1;
	g_webpage_quality = 0;
	g_tm_ec_ttl = 0;
	memset(g_tm_ec_ttl_update_time, 0, 8);
	g_tm_service_sub_status = 0;

	memcpy(g_np_env, (const void *)0xa83ff110, 9);
	memcpy(g_np_account_id, (const void *)0xa83ff119, 0x10);
	memcpy(g_np_login_id, (const void *)0xa83ff129, 0x41);
	memcpy(g_np_password, (const void *)0xa83ff16a, 0x1f);

	g_np_auto_sign_in_enable = 0;
	g_np_nav_only = cfg[0x30 / 4];
	g_np_ad_clock_diff = cfg[0x34 / 4];
	g_np_view_mode = 0;
	g_np_geo_filtering = cfg[0x38 / 4];

	memcpy(g_np_guest_country, (const void *)0xa83ff189, 3);
	memcpy(g_np_guest_lang, (const void *)0xa83ff18c, 3);

	g_np_guest_yob = cfg[0x3c / 4];
	g_np_guest_mob = cfg[0x40 / 4];
	g_np_guest_dob = cfg[0x44 / 4];
	g_np_check_drm = 0;

	memcpy(g_owner_name, (const void *)0xa83ff050, 0x80);
	g_keys[40].size = strnlen(g_owner_name, 0x7f) + 1;

	g_language = cfg[0x00 / 4];
	g_button_assign = cfg[0x04 / 4];
	memcpy(g_lock_password, (const void *)0xa83ff18f, 4);
	g_parental_level = cfg[0x48 / 4];
	g_browser_start = cfg[0x4c / 4];
	g_char_oem = cfg[0x08 / 4];
	g_char_ansi = cfg[0x0c / 4];
	g_time_format = cfg[0x10 / 4];
	g_date_format = cfg[0x14 / 4];
	g_summer_time = cfg[0x18 / 4];
	g_time_zone_offset = cfg[0x1c / 4];
	memcpy(g_time_zone_area, (const void *)0xa83ff0d0, 0x30);
	memcpy(g_adhoc_ssid_prefix, (const void *)0xa83ff193, 4);

	g_infra_latest_id = 0;
	g_infra_eap_md5 = cfg[0x24 / 4];
	g_infra_auto_setting = cfg[0x28 / 4];
	g_infra_wifisvc_setting = cfg[0x2c / 4];
	g_infra_btdun_warnings_check = 0;
	g_osk_version_id = 0;
	g_osk_disp_locale = 0;
	g_osk_writing_locale = 0;
	g_osk_input_char_mask = 0;
	g_osk_keytop_index = 0;
	g_count_boot = 0;
	g_count_game_exec = 0;
	g_count_slide = 0;
	g_count_usb_connect = 0;
	g_count_wifi_connect = 0;
	g_count_psn_access = 0;
}

static __attribute__((noinline)) void sub_6164(void)
{
	u32 i;

	for (i = 0; i < 0x26; i++)
		g_cats[i].open_flag = 0;
}

static __attribute__((noinline)) int sub_6190(u32 cat_id)
{
	if (cat_id < 0x24)
		return g_cats[cat_id].open_flag;
	return 0;
}

static __attribute__((noinline)) int sub_61c0(u32 cat_id)
{
	if (cat_id < 0x24 && g_cats[cat_id].open_flag == 1)
		return 0;
	return 0x8008272e;
}

static __attribute__((noinline)) int sub_6204(const char *name)
{
	u32 i;

	for (i = 0; i < 0x26; i++) {
		if (strncmp(name, g_cats[i].name, 0x1e) == 0) {
			g_cats[i].open_flag = 1;
			return g_cats[i].id;
		}
	}
	return 0x80082716;
}

static __attribute__((noinline)) int sub_6280(u32 cat_id, u32 *out_num)
{
	if (cat_id < 0x24) {
		*out_num = g_cats[cat_id].num_keys;
		return 0;
	}
	return 0x8008272e;
}

static __attribute__((noinline)) int sub_62bc(int cat_id, const char *name, void *buf, u32 size)
{
	u32 key_id, type, ksize;
	int res;

	res = sub_08d0(cat_id, name, &key_id, &type, &ksize);
	if (res == 0)
		return sub_0a64(cat_id, key_id, buf, size);
	return res;
}
