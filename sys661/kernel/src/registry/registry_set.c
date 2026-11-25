#include "registry_int.h"

__attribute__((noinline)) int regSetKeyValue(int cat_id, const char *name, const void *buf, u32 size)
{
	u32 key_id, type, ksize;
	int res;

	res = regFindKey(cat_id, name, &key_id, &type, &ksize);
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

