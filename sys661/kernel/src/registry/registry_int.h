#ifndef REGISTRY_INT_H
#define REGISTRY_INT_H

/*
 * ePSP 6.61 sceRegistry_Service (registry.prx)
 */

#include <common_imp.h>

typedef u32 REGHANDLE;

struct RegParam {
	u32 regtype;
	char name[256];
	u32 namelen;
	u32 reserved[2];
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
extern int strncmp(const char *lhs, const char *rhs, u32 n);
extern u32 strnlen(const char *s, u32 maxlen);
extern void *memcpy(void *dst, const void *src, u32 n);
extern char *strncpy(char *dst, const char *src, u32 n);


extern struct RegKeyEntry g_keys[104];
extern char g_home_uri[0x400];
extern u32 g_cookie_mode;
extern u32 g_proxy_mode;
extern char g_proxy_address[0x80];
extern u32 g_proxy_port;
extern u32 g_picture;
extern u32 g_animation;
extern u32 g_javascript;
extern u32 g_cache_size;
extern u32 g_char_size;
extern u32 g_disp_mode;
extern u32 g_connect_mode;
extern u32 g_proxy_protect;
extern u32 g_proxy_autoauth;
extern char g_proxy_user[0x80];
extern char g_proxy_password[0x80];
extern u32 g_webpage_quality;
extern u32 g_tm_ec_ttl;
extern u8 g_tm_ec_ttl_update_time[8];
extern u32 g_tm_service_sub_status;
extern char g_np_env[12];
extern u8 g_np_account_id[0x10];
extern char g_np_login_id[0x44];
extern char g_np_password[0x20];
extern u32 g_np_auto_sign_in_enable;
extern u32 g_np_nav_only;
extern u32 g_np_ad_clock_diff;
extern u32 g_np_view_mode;
extern u32 g_np_geo_filtering;
extern char g_np_guest_country[4];
extern char g_np_guest_lang[4];
extern u32 g_np_guest_yob;
extern u32 g_np_guest_mob;
extern u32 g_np_guest_dob;
extern u32 g_np_check_drm;
extern char g_owner_name[0x80];
extern u32 g_language;
extern u32 g_button_assign;
extern u8 g_lock_password[4];
extern u32 g_parental_level;
extern u32 g_browser_start;
extern u32 g_char_oem;
extern u32 g_char_ansi;
extern u32 g_time_format;
extern u32 g_date_format;
extern u32 g_summer_time;
extern u32 g_time_zone_offset;
extern char g_time_zone_area[0x30];
extern char g_adhoc_ssid_prefix[4];
extern u32 g_infra_latest_id;
extern u32 g_infra_eap_md5;
extern u32 g_infra_auto_setting;
extern u32 g_infra_wifisvc_setting;
extern u32 g_infra_btdun_warnings_check;
extern u32 g_osk_version_id;
extern u32 g_osk_disp_locale;
extern u32 g_osk_writing_locale;
extern u32 g_osk_input_char_mask;
extern u32 g_osk_keytop_index;
extern u32 g_count_boot;
extern u32 g_count_game_exec;
extern u32 g_count_slide;
extern u32 g_count_usb_connect;
extern u32 g_count_wifi_connect;
extern u32 g_count_psn_access;

int regFindKey(u32 cat_id, const char *name, u32 *out_id, u32 *out_type, u32 *out_size);
int regGetKeyValueById(int cat_id, u32 key_id, void *buf, u32 size);
int regSetKeyValue(int cat_id, const char *name, const void *buf, u32 size);

#endif /* REGISTRY_INT_H */
