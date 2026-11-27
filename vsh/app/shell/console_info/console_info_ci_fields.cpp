/*
 * console_info_ci_fields.cpp: the "ci" record's fields (0x810E8440-0x810F1924): the field kinds,
 * reading a value into the values file and writing its "d<n>=" line when it changed, the readers
 * of the console's state each field gives, and the table of fields. The file's and the names are
 * ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <std/string>
#include <std/vector>
#include <std/string.h>
#include <std/stdio.h>
#include <plugin.h>
#include <rtc/rtc.h>
#include <datetime/datetime.h>
#include <regmgr/regmgr.h>
#include <ss_mgr/ss_mgr.h>
#include <vshbridge/vshbridge.h>
#include <msif/msif.h>
#include <av_config/av_config.h>
#include <libhttp/libhttp_url.h>
#include <kernel/iofilemgr.h>
#include <appmgr/appmgr.h>
#include <power/power.h>
#include <bt/bt.h>
#include <bt/bt_registered.h>
#include "shell.h"
#include "plugins.h"
#include "../theme.h"
#include "console_info.h"
#include "../src/livespace_db/livespace_db_internal.h"

using namespace paf;

/* SceCompat's list of the PSP titles played (sceCompatGetTitleList's user call) */
extern "C" int SceCompatForVsh_4915DEE7(void *buf, SceSize length);

/* PLACEHOLDER: an NP account signed in on the console, in *signedIn */
extern "C" int SceNpManager_BF8921C8(int *signedIn);

/* PLACEHOLDER: libmtp's count of the console's connections, in *count */
extern "C" int SceLibMtp_8E76FE4D(int *count);

/* syscon's log: an entry read (0x81417CBC, placed with the code the build shares, over
   _vshSysconLogReadData) */
extern "C" int vshSysconLogReadData(int id, void *buf, SceSize size);

/* syscon's temperature log read (0x81417CD0, placed with the code the build shares, over
   _vshSysconGetTemperatureLog) */
extern "C" int vshSysconGetTemperatureLog(SceUInt8 *log);

/* PLACEHOLDER: libnetctl's NAT information in its 0x28-byte form (netctl.c) */
extern "C" int SceNetCtl_F30365D6(SceSize *info);


/* telephony_plugin not there (PLACEHOLDER name) */
#define CONSOLE_INFO_ERROR_NO_TELEPHONY		-42

/* the platform whose telephony is not asked (consoleInfoGetPlatform) */
#define CONSOLE_INFO_PLATFORM_NO_TELEPHONY	2

/* the flag of ur0:ci/file the field 5 gives */
#define CONSOLE_INFO_CI_FLAG_INDEX		0

/* the memory card's state in the "ms" field */
#define CONSOLE_INFO_CI_MS_NONE			0
#define CONSOLE_INFO_CI_MS_PRESENT		1
#define CONSOLE_INFO_CI_MS_ERROR		2

/* the parental restriction's registry folder, and the size of its durations' text */
#define CONSOLE_INFO_CI_PARENTAL		"/CONFIG/SECURITY/PARENTAL"
#define CONSOLE_INFO_CI_DURATIONS_SIZE		35

/* the user folder (sceAppMgrGetUserDirPath) holding the live area's records, and the mode
   their files are opened with */
#define CONSOLE_INFO_CI_USER_DIR		2
#define CONSOLE_INFO_CI_FILE_MODE		6

/* the titles the live area's access record keeps, and the most times it counts */
#define CONSOLE_INFO_CI_ACCESS_TITLES		10
#define CONSOLE_INFO_CI_ACCESS_MAX		9999

/* the NVS area whose first bit says the memory card is the internal one, and sdstor's
   request for its size */
#define CONSOLE_INFO_CI_NVS_AREA_STORAGE	5
#define CONSOLE_INFO_CI_SDSTOR_GET_SIZE		1

/* the Int fields syscon's log and the battery give (their indices), and the log's entries */
#define CONSOLE_INFO_CI_FIELD_LOG_11		21
#define CONSOLE_INFO_CI_FIELD_LOG_23		22
#define CONSOLE_INFO_CI_FIELD_LOG_9		23
#define CONSOLE_INFO_CI_FIELD_LOG_10		24
#define CONSOLE_INFO_CI_FIELD_LOG_8		25
#define CONSOLE_INFO_CI_FIELD_LOG_5		26
#define CONSOLE_INFO_CI_FIELD_BATTERY_CYCLES	27
#define CONSOLE_INFO_CI_FIELD_BATTERY_SOH	28

/* the Bluetooth devices registered the record counts: Sony's controllers by their IDs, the
   others by their class of device's major and minor classes */
#define CONSOLE_INFO_CI_BT_MAX			32
#define CONSOLE_INFO_CI_BT_VID_SONY		0x054C
#define CONSOLE_INFO_CI_BT_PID_DS3		0x0268
#define CONSOLE_INFO_CI_BT_PID_DS4		0x05C4
#define CONSOLE_INFO_CI_BT_PID_BD_REMOTE	0x0306
#define CONSOLE_INFO_CI_BT_MAJOR_MASK		0x1F00
#define CONSOLE_INFO_CI_BT_MAJOR_AUDIO		0x0400
#define CONSOLE_INFO_CI_BT_MAJOR_PERIPHERAL	0x0500
#define CONSOLE_INFO_CI_BT_MAJOR_IMAGING	0x0600
#define CONSOLE_INFO_CI_BT_MINOR_MASK		0x00FC
#define CONSOLE_INFO_CI_BT_MINOR_HEADSET	0x0004
#define CONSOLE_INFO_CI_BT_MINOR_HANDSFREE	0x0008
#define CONSOLE_INFO_CI_BT_MINOR_LOUDSPEAKER	0x0014
#define CONSOLE_INFO_CI_BT_KEYBOARD		0x0040
#define CONSOLE_INFO_CI_BT_POINTER		0x0080

/* the temperature fields (their indices), and the value given while the log was not read */
#define CONSOLE_INFO_CI_FIELD_TEMPERATURE_1	158
#define CONSOLE_INFO_CI_FIELD_TEMPERATURE_5	159
#define CONSOLE_INFO_CI_FIELD_TEMPERATURE_6	160
#define CONSOLE_INFO_CI_FIELD_TEMPERATURE_7	161
#define CONSOLE_INFO_CI_TEMPERATURE_NONE	0x80

/* syscon's temperature log the record reads and clears, and its size */
#define CONSOLE_INFO_CI_TEMPERATURE_LOG		3
#define CONSOLE_INFO_CI_TEMPERATURE_LOG_SIZE	16

/* the home screen's fields (their indices) */
#define CONSOLE_INFO_CI_FIELD_FOLDERS		127
#define CONSOLE_INFO_CI_FIELD_FOLDER_TITLES	128
#define CONSOLE_INFO_CI_FIELD_PAGES		129

/* the most of the home screen's icons the record gives */
#define CONSOLE_INFO_CI_ICONS_MAX		1000

/* the theme kinds (shellThemeGetType) and what the record gives for each */
#define CONSOLE_INFO_CI_THEME_UX0		100
#define CONSOLE_INFO_CI_THEME_PD0		200
#define CONSOLE_INFO_CI_THEME_VS0		300

/* the fields of each kind the record gives */
#define CONSOLE_INFO_CI_INT_FIELDS		59
#define CONSOLE_INFO_CI_FILE_INT_FIELDS		164
#define CONSOLE_INFO_CI_INT64_FIELDS		4
#define CONSOLE_INFO_CI_FILE_INT64_FIELDS	3
#define CONSOLE_INFO_CI_NET_FIELDS		7
#define CONSOLE_INFO_CI_STRING_FIELDS		10

/* the network settings (their numbers 1 to 30) and their registry folders */
#define CONSOLE_INFO_CI_NET_SETTINGS		30
#define CONSOLE_INFO_CI_NET_COMMON		"/CONFIG/NET/%02d/COMMON"
#define CONSOLE_INFO_CI_NET_IP			"/CONFIG/NET/%02d/IP"
#define CONSOLE_INFO_CI_NET_APP			"/CONFIG/NET/%02d/APP"
#define CONSOLE_INFO_CI_NET_FOLDER_SIZE		23

/* the bytes of a Bluetooth device's name the record gives */
#define CONSOLE_INFO_CI_BT_NAME_SIZE		120

/* libnetctl's NAT information: its size, and the word giving the NAT type */
#define CONSOLE_INFO_CI_NAT_INFO_SIZE		40
#define CONSOLE_INFO_CI_NAT_TYPE		6


/* the social networks' accounts in the registry */
#define CONSOLE_INFO_CI_TWITTER			"/CONFIG/NP2/TWITTER"
#define CONSOLE_INFO_CI_FACEBOOK		"/CONFIG/NP2/FACEBOOK"

/* (0x81508AEC) the NVS areas the record reads: their offsets and sizes */
static const struct {
	SceUInt32 offset;
	SceSize size;
} s_nvsAreas[] = {
	{ 0x4A4, 4 },
	{ 0x500, 1 },
	{ 0x482, 1 },
	{ 0x4E0, 32 },
	{ 0x483, 1 },
	{ 0x486, 1 },
};

/* (0x81544530, 0x81544538, 0x81544540) ux0:'s state and space when last asked */
static int s_ux0State = CONSOLE_INFO_CI_MS_ERROR;
static SceUInt64 s_ux0Free = (SceUInt64)-1;
static SceUInt64 s_ux0Max = (SceUInt64)-1;

/* (0x81556398, 0x815563A4) the HDMI monitor's EDID bytes in hexadecimal, and its name */
static string s_hdmiEdidId;
static char s_hdmiMonitorName[16];

/* a temperature log entry the record gives, when last read: CONSOLE_INFO_CI_TEMPERATURE_NONE
   until it is */
struct ConsoleInfoCiTemperature {
	__attribute__((always_inline)) ConsoleInfoCiTemperature()
		: value(CONSOLE_INFO_CI_TEMPERATURE_NONE)
	{
	}

	SceUInt8 value;
};

/* (0x815563B4..0x815563B7) syscon's temperature log's entries (PLACEHOLDER names: the log's
   bytes they are) */
static ConsoleInfoCiTemperature s_temperature1;
static ConsoleInfoCiTemperature s_temperature5;
static ConsoleInfoCiTemperature s_temperature6;
static ConsoleInfoCiTemperature s_temperature7;

/* (0x81558454, 0x81558852) a text field's previous and current values */
static ConsoleInfoCiString s_prevString;
static ConsoleInfoCiString s_curString;

/* (0x81414890, 0x81414954: placed with the code the build shares) */
ConsoleInfoCiField::~ConsoleInfoCiField()
{
}

/* (0x810E8440) */
int ConsoleInfoCiIntField::Read(ConsoleInfoCiValueFile *file)
{
	int value = 0;

	if (m_getter(this, &value) != 0)
		return 0;

	int ret = file->Write(&value);

	if (ret != 0)
		return ret;
	return 0;
}

/* (0x810E8482) */
int ConsoleInfoCiIntField::Write(ConsoleInfoCiValueFile *prev, ConsoleInfoCiValueFile *cur,
				 ConsoleInfoCi *ci)
{
	int prevValue = 0;
	int value = 0;
	int ret = cur->Read(&value);

	if (ret != 0)
		return ret;
	if (prev->Read(&prevValue) == 0 && prevValue == value && !ci->m_updated &&
	    !m_key.m_force)
		return 0;

	string format = m_key.GetFormat("d", "x");

	sce_paf_snprintf(ci->m_line, CONSOLE_INFO_BUFFER_SIZE, format.c_str(), value);
	ret = ci->AppendLine(sce_paf_strlen(ci->m_line), 1);
	if (ret != 0)
		return ret;
	return 0;
}

/* (0x810E8544) */
int ConsoleInfoCiInt64Field::Read(ConsoleInfoCiValueFile *file)
{
	SceInt64 value = 0;
	int ret = m_getter(this, &value);

	if (ret == 0) {
		ret = file->Write(&value);
		if (ret == 0)
			return 0;
	}
	return ret;
}

/* (0x810E858A) a time is printed as its date */
int ConsoleInfoCiInt64Field::Write(ConsoleInfoCiValueFile *prev, ConsoleInfoCiValueFile *cur,
				   ConsoleInfoCi *ci)
{
	SceInt64 prevValue = 0;
	SceUInt64 value = 0;
	int ret = cur->Read(&value);

	if (ret != 0)
		return ret;
	if (prev->Read(&prevValue) == 0 && prevValue == value && !ci->m_updated &&
	    !m_key.m_force)
		return 0;

	string format;
	char *line = ci->m_line;

	if (m_key.m_type == CONSOLE_INFO_CI_TYPE_TIME) {
		format = m_key.GetFormat(NULL, NULL);

		datetime::DateTime date(0, 0, 0, 0, 0, 0, 0);

		if (value != 0)
			date = datetime::DateTime::ConvertRtcTickToDateTime((rtc::Tick *)&value);
		sce_paf_snprintf(line, CONSOLE_INFO_BUFFER_SIZE, format.c_str(), date.m_year,
				 date.m_month, date.m_day, date.m_hour, date.m_minute, date.m_second);
	} else {
		format = m_key.GetFormat("lld", "llx");
		sce_paf_snprintf(line, CONSOLE_INFO_BUFFER_SIZE, format.c_str(), value);
	}
	ret = ci->AppendLine(sce_paf_strlen(line), 1);
	if (ret != 0)
		return ret;
	return 0;
}

/* (0x810E8762) */
int ConsoleInfoCiStringField::Read(ConsoleInfoCiValueFile *file)
{
	ConsoleInfoCiString value;

	sce_paf_memset(&value, 0, sizeof(value));

	int ret = m_getter(this, &value);

	if (ret == 0) {
		ret = file->Write(&value);
		if (ret == 0)
			return 0;
	}
	return ret;
}

/* (0x810E87BC) the field CONSOLE_INFO_CI_ID_EMPTY_SKIPPED is not written empty */
int ConsoleInfoCiStringField::Write(ConsoleInfoCiValueFile *prev, ConsoleInfoCiValueFile *cur,
				    ConsoleInfoCi *ci)
{
	int ret = cur->Read(&s_curString);

	if (ret != 0)
		return ret;

	int force = 0;
	int prevRet = prev->Read(&s_prevString);

	if (ci->m_updated || m_key.m_force)
		force = 1;
	if ((prevRet | force) == 0 &&
	    sce_paf_memcmp(&s_prevString, &s_curString, sizeof(s_curString)) == 0)
		return 0;
	if (force == 0 && m_key.m_id == CONSOLE_INFO_CI_ID_EMPTY_SKIPPED && prevRet == 0 &&
	    sce_paf_strcmp(s_curString.text, "") == 0)
		return ret;

	string format = m_key.GetFormat(NULL, NULL);

	sce_paf_snprintf(ci->m_line, CONSOLE_INFO_BUFFER_SIZE, format.c_str(), s_curString.text);
	ret = ci->AppendLine(sce_paf_strlen(ci->m_line), m_key.m_lineMode);
	if (ret != 0)
		return ret;
	return 0;
}

/* (0x810E88CC) the PS code's product sub code */
static int consoleInfoCiGetProductSubCode(ConsoleInfoCiIntField *field, int *value)
{
	ScePsCode code;

	sce_paf_memset(&code, 0, sizeof(code));

	int ret = vshSblAimgrGetPscode(&code);

	if (ret != 0)
		return ret;
	*value = code.product_sub_code;
	return 0;
}

/* (0x810E8910) whether the telephony service is restricted */
static int consoleInfoCiGetTelephony(ConsoleInfoCiIntField *field, int *value)
{
	int restriction;

	if (consoleInfoGetPlatform() == CONSOLE_INFO_PLATFORM_NO_TELEPHONY) {
		*value = 2;
		return 0;
	}
	restriction = 0;

	Plugin *plugin = Plugin::Find("telephony_plugin");

	if (plugin == NULL)
		return CONSOLE_INFO_ERROR_NO_TELEPHONY;

	TelephonyPluginInterface *iface = (TelephonyPluginInterface *)plugin->GetInterface(1);

	if (iface == NULL)
		return CONSOLE_INFO_ERROR_NO_TELEPHONY;

	TelephonyService *service = iface->getService();

	if (service == NULL)
		return CONSOLE_INFO_ERROR_NO_TELEPHONY;

	int ret;

	{
		TelephonyResult result = service->vtable->getRestriction(service, &restriction);

		ret = result.GetCode();
	}
	if (ret < 0)
		return ret;
	*value = restriction != 0;
	return 0;
}

/* (0x810E89AC) */
static int consoleInfoCiGetIduMode(ConsoleInfoCiIntField *field, int *value)
{
	int idu = vshSysconIsIduMode();

	if (idu < 0)
		*value = 0;
	else if (idu != 0)
		*value = 1;
	else
		*value = 0;
	return 0;
}

/* (0x810E89C8) */
static int consoleInfoCiGetFlag(ConsoleInfoCiIntField *field, int *value)
{
	SceUInt8 flag = 0;
	int ret = consoleInfoFileReadFlag(CONSOLE_INFO_CI_FLAG_INDEX, &flag, 0);

	*value = flag;
	return ret;
}

/* (0x810E8A02) */
static int consoleInfoCiGetBrightness(ConsoleInfoCiIntField *field, int *value)
{
	int brightness = 0;
	int ret = sceRegMgrGetKeyInt("/CONFIG/DISPLAY", "brightness", &brightness);

	*value = brightness;
	return ret;
}

/* (0x810E8A44) the network's country code */
static int consoleInfoCiGetMcc(ConsoleInfoCiIntField *field, int *value)
{
	Plugin *plugin = Plugin::Find("telephony_plugin");

	if (plugin == NULL)
		return CONSOLE_INFO_ERROR_NO_TELEPHONY;

	TelephonyPluginInterface *iface = (TelephonyPluginInterface *)plugin->GetInterface(1);

	if (iface == NULL)
		return CONSOLE_INFO_ERROR_NO_TELEPHONY;

	TelephonyService *service = iface->getService();

	if (service == NULL)
		return CONSOLE_INFO_ERROR_NO_TELEPHONY;

	TelephonyPlmn plmn;

	if (service->vtable->getPlmn(service, &plmn).GetCode() != 0)
		*value = 0;
	else
		*value = plmn.GetMcc();
	return 0;
}

/* (0x810E8AE4) the network's operator code */
static int consoleInfoCiGetMnc(ConsoleInfoCiIntField *field, int *value)
{
	Plugin *plugin = Plugin::Find("telephony_plugin");

	if (plugin == NULL)
		return CONSOLE_INFO_ERROR_NO_TELEPHONY;

	TelephonyPluginInterface *iface = (TelephonyPluginInterface *)plugin->GetInterface(1);

	if (iface == NULL)
		return CONSOLE_INFO_ERROR_NO_TELEPHONY;

	TelephonyService *service = iface->getService();

	if (service == NULL)
		return CONSOLE_INFO_ERROR_NO_TELEPHONY;

	TelephonyPlmn plmn;

	if (service->vtable->getPlmn(service, &plmn).GetCode() != 0)
		*value = 0;
	else
		*value = plmn.GetMnc();
	return 0;
}

/* (0x810E8B84) the memory card's ID: its assembly date's bytes and its serial number */
static __attribute__((noinline)) void consoleInfoCiMsId(const SceMsInfo *info, string *id)
{
	char hex[3];

	id->clear();
	for (int i = 0; i < 8; i++) {
		if (sce_paf_snprintf(hex, sizeof(hex), "%02x", info->assemblyDate[i]) < 0) {
			id->clear();
			break;
		}
		id->append(hex);
	}
	id->append(1, ',');

	char serial[9];

	if (sce_paf_snprintf(serial, sizeof(serial), "%08x", info->serialNumber) < 0)
		id->clear();
	id->append(serial);
}

/* the activation's MID, or zeros */
static inline void consoleInfoCiGetActivationMid(string *mid)
{
	ShellActivationInfo info;

	*mid = "00000000000000000000000000000000";
	if (shellActivationRead(&info, 0) != 0)
		return;
	*mid = info.mid;
}

/* (0x810E8CCE) ux0:'s space, the activation's MID and the memory card's size and ID */
static int consoleInfoCiGetStorage(ConsoleInfoCiStringField *field, ConsoleInfoCiString *value)
{
	string mid;
	int ret = gameIoGetDeviceSpace("ux0:", &s_ux0Max, &s_ux0Free);

	if (ret == 0) {
		s_ux0State = CONSOLE_INFO_CI_MS_PRESENT;
	} else if (ret == SCE_ERROR_ERRNO_ENODEV) {
		s_ux0Free = (SceUInt64)-1;
		s_ux0Max = (SceUInt64)-1;
		s_ux0State = CONSOLE_INFO_CI_MS_NONE;
	} else {
		s_ux0Free = (SceUInt64)-1;
		s_ux0Max = (SceUInt64)-1;
		s_ux0State = CONSOLE_INFO_CI_MS_ERROR;
	}

	consoleInfoCiGetActivationMid(&mid);

	int maxKb = 0;
	int freeKb = 0;

	if (s_ux0Max != (SceUInt64)-1) {
		maxKb = s_ux0Max >> 10;
		freeKb = s_ux0Free >> 10;
	}

	SceUInt8 storage;
	int internal;

	if (vshSblSsGetNvsData(CONSOLE_INFO_CI_NVS_AREA_STORAGE, &storage,
			       s_nvsAreas[CONSOLE_INFO_CI_NVS_AREA_STORAGE].size) != 0)
		internal = 0;
	else
		internal = (storage & 1) == 0;
	string msId("0000000000000000,00000000");
	SceUInt64 msSize = (SceUInt64)-1;

	if (internal) {
		SceUInt32 out[4];

		if (sceIoDevctl("sdstor0:", CONSOLE_INFO_CI_SDSTOR_GET_SIZE, (void *)"int-lp-ign-userext",
				sce_paf_strlen("int-lp-ign-userext"), out, sizeof(out)) == 0)
			msSize = (out[1] >> 1) & 0x3FFFFF;
	} else {
		SceMsInfo info;

		if (vshMsifGetMsInfo(&info) == 0) {
			consoleInfoCiMsId(&info, &msId);
			msSize = info.nbytes2 >> 10;
		}
	}
	sce_paf_snprintf(value->text, sizeof(value->text),
			 "%d&d51=%08x&d52=%08x&d351=%s&d371=%d&d532=%llx&d535=%s", s_ux0State,
			 maxKb, freeKb, mid.c_str(), internal, msSize, msId.c_str());
	value->length = sce_paf_strlen(value->text);
	return 0;
}

/* (0x810E8FF2) the PSP titles played */
static int consoleInfoCiGetCompatTitles(ConsoleInfoCiStringField *field,
					ConsoleInfoCiString *value)
{
	char *text = value->text;

	if (SceCompatForVsh_4915DEE7(text, 100) != 0)
		value->text[0] = '\0';
	value->length = sce_paf_strlen(text);
	return 0;
}

/* (0x810E9014) the HDMI monitor's EDID bytes and name read again */
void consoleInfoCiUpdateHdmiMonitor(void)
{
	s_hdmiEdidId.clear();
	sce_paf_memset(s_hdmiMonitorName, 0, sizeof(s_hdmiMonitorName));

	AvConfigMonitorInfo info;

	info.size = sizeof(info);
	if (sceAVConfigHdmiGetMonitorInfo(&info) != 0)
		return;
	for (int i = 0; i < 16; i++) {
		if ((SceUInt8)info.monitorName[i] == 0) {
			sce_paf_strncpy(s_hdmiMonitorName, (const char *)info.monitorName,
					sizeof(s_hdmiMonitorName));
			break;
		}
	}
	for (int i = 0; i < 10; i++) {
		char hex[3];

		if (sce_paf_snprintf(hex, sizeof(hex), "%02x", info.productId[i]) < 0)
			break;
		s_hdmiEdidId.append(hex);
	}
}

/* (0x810E9134) */
static int consoleInfoCiGetHdmiMonitorName(ConsoleInfoCiStringField *field,
					   ConsoleInfoCiString *value)
{
	char *text = value->text;
	SceSize require;

	if (sceHttpUriEscape(text, &require, sizeof(value->text), s_hdmiMonitorName) != 0)
		value->text[0] = '\0';
	value->length = sce_paf_strlen(text);
	return 0;
}

/* (0x810E9180) */
static int consoleInfoCiGetHdmiEdidId(ConsoleInfoCiStringField *field, ConsoleInfoCiString *value)
{
	sce_paf_strncpy(value->text, s_hdmiEdidId.c_str(), s_hdmiEdidId.size());
	value->length = s_hdmiEdidId.size();
	return 0;
}

/* (0x810E919E) the parental restriction's play time of each day of the week */
static int consoleInfoCiGetParentalDurations(ConsoleInfoCiStringField *field,
					     ConsoleInfoCiString *value)
{
	char *text = value->text;

	sce_paf_memset(text, 0, CONSOLE_INFO_CI_DURATIONS_SIZE);

	int sun = 0;
	int mon = 0;
	int tue = 0;
	int wed = 0;
	int thu = 0;
	int fri = 0;
	int sat = 0;

	if (sceRegMgrGetKeyInt(CONSOLE_INFO_CI_PARENTAL, "restrict_duration_sun", &sun) != 0)
		sun = 0;
	if (sceRegMgrGetKeyInt(CONSOLE_INFO_CI_PARENTAL, "restrict_duration_mon", &mon) != 0)
		mon = 0;
	if (sceRegMgrGetKeyInt(CONSOLE_INFO_CI_PARENTAL, "restrict_duration_tue", &tue) != 0)
		tue = 0;
	if (sceRegMgrGetKeyInt(CONSOLE_INFO_CI_PARENTAL, "restrict_duration_wed", &wed) != 0)
		wed = 0;
	if (sceRegMgrGetKeyInt(CONSOLE_INFO_CI_PARENTAL, "restrict_duration_thu", &thu) != 0)
		thu = 0;
	if (sceRegMgrGetKeyInt(CONSOLE_INFO_CI_PARENTAL, "restrict_duration_fri", &fri) != 0)
		fri = 0;
	if (sceRegMgrGetKeyInt(CONSOLE_INFO_CI_PARENTAL, "restrict_duration_sat", &sat) != 0)
		sat = 0;
	if (sce_paf_snprintf(text, CONSOLE_INFO_CI_DURATIONS_SIZE, "%d,%d,%d,%d,%d,%d,%d", sun, mon,
			     tue, wed, thu, fri, sat) < 0)
		sce_paf_memset(text, 0, CONSOLE_INFO_CI_DURATIONS_SIZE);
	return 0;
}

/* (0x810E92F4) the live area's join record, taken: the file is renamed, read and removed */
static int consoleInfoCiGetLiveAreaJoin(ConsoleInfoCiStringField *field,
					ConsoleInfoCiString *value)
{
	string join;
	char dir[32];

	if (sceAppMgrGetUserDirPath(CONSOLE_INFO_CI_USER_DIR, dir, sizeof(dir)) != 0)
		return 0;

	string path(dir);

	path += "/savedata_plus/cinfo/la_join.dat";

	string taken(dir);

	taken += "/savedata_plus/cinfo/_la_join.dat";
	LocalFile::RemoveFile(taken.c_str());

	int ret = LocalFile::RenameFile(path.c_str(), taken.c_str());

	if (ret != 0) {
		sce_paf_memset(&value->text[join.size()], 0, 1);
		value->length = join.size();
		return 0;
	}

	common::SharedPtr<File> file = LocalFile::Open(taken.c_str(), SCE_O_RDONLY,
						       CONSOLE_INFO_CI_FILE_MODE, &ret);

	if (ret != 0) {
		ret = LocalFile::RemoveFile(taken.c_str());
		return 0;
	}

	char buf[100];

	sce_paf_memset(buf, 0, sizeof(buf));
	ret = file->Read(buf, sizeof(buf) - 1);
	if (ret >= 0)
		join = buf;
	sce_paf_strncpy(value->text, join.c_str(), join.size());
	sce_paf_memset(&value->text[join.size()], 0, 1);
	value->length = join.size();
	file->Close();
	ret = LocalFile::RemoveFile(taken.c_str());
	return 0;
}

/* (0x810E9678) the live area's access record, taken like the join record: up to ten titles,
   each with the times it was opened */
static int consoleInfoCiGetLiveAreaAccess(ConsoleInfoCiStringField *field,
					  ConsoleInfoCiString *value)
{
	string access;
	char dir[32];

	if (sceAppMgrGetUserDirPath(CONSOLE_INFO_CI_USER_DIR, dir, sizeof(dir)) != 0)
		return 0;

	string path(dir);

	path += "/savedata_plus/cinfo/la_access.dat";

	string taken(dir);

	taken += "/savedata_plus/cinfo/_la_access.dat";
	LocalFile::RemoveFile(taken.c_str());

	int ret = LocalFile::RenameFile(path.c_str(), taken.c_str());

	if (ret != 0) {
		sce_paf_memset(&value->text[access.size()], 0, 1);
		value->length = access.size();
		return 0;
	}

	common::SharedPtr<File> file = LocalFile::Open(taken.c_str(), SCE_O_RDONLY,
						       CONSOLE_INFO_CI_FILE_MODE, &ret);

	if (ret != 0) {
		ret = LocalFile::RemoveFile(taken.c_str());
		return 0;
	}

	int count = 0;
	bool ok = true;

	for (int i = 0; i < CONSOLE_INFO_CI_ACCESS_TITLES; i++) {
		char titleId[10];
		char number[5];

		sce_paf_memset(titleId, 0, sizeof(titleId));
		count = 0;
		ret = file->Read(titleId, sizeof(titleId));
		if (titleId[9] != '\0') {
			char copy[11];

			sce_paf_memset(copy, 0, sizeof(copy));
			sce_paf_memcpy(copy, titleId, sizeof(titleId));
		}
		if (ret != sizeof(titleId)) {
			if (ret != 0)
				ok = false;
			break;
		}
		if (access.size() != 0)
			access.append(1, ',');
		access += titleId;
		ret = file->Read(&count, sizeof(count));
		if (ret != sizeof(count)) {
			ok = false;
			break;
		}
		if (count > CONSOLE_INFO_CI_ACCESS_MAX)
			count = CONSOLE_INFO_CI_ACCESS_MAX;
		ret = sce_paf_snprintf(number, sizeof(number), "%d", count);
		if (ret < 0) {
			ok = false;
			break;
		}
		if (access.size() != 0)
			access.append(1, ',');
		access += number;
	}
	if (!ok)
		access.clear();
	sce_paf_strncpy(value->text, access.c_str(), access.size());
	sce_paf_memset(&value->text[access.size()], 0, 1);
	value->length = access.size();
	file->Close();
	ret = LocalFile::RemoveFile(taken.c_str());
	return 0;
}

/* (0x810E9AFA) */
int ConsoleInfoCiFileIntField::Get(ConsoleInfoCiIntField *field, int *value)
{
	ConsoleInfoCiFileIntField *self = (ConsoleInfoCiFileIntField *)field;
	int ret = 0;

	switch (self->m_value.kind) {
	case CONSOLE_INFO_CI_FILE_FLAG: {
		SceUInt8 flag = 0;

		ret = consoleInfoFileReadFlag(self->m_value.index, &flag, 0);
		*value = flag;
		break;
	}
	case CONSOLE_INFO_CI_FILE_INT: {
		SceUInt32 v = 0;

		ret = consoleInfoFileReadInt(self->m_value.index, &v, 0);
		*value = v;
		break;
	}
	case CONSOLE_INFO_CI_FILE_INT64: {
		SceUInt64 v = 0;

		ret = consoleInfoFileReadInt64(self->m_value.index, &v, 0);
		if (self->m_value.unit == CONSOLE_INFO_CI_UNIT_USEC)
			v /= 1000000;
		*value = v;
		break;
	}
	}
	if (self->m_value.max != 0 && (SceUInt32)*value > self->m_value.max)
		*value = self->m_value.max;
	return ret;
}

/* (0x810E9BA6) */
int ConsoleInfoCiFileInt64Field::Get(ConsoleInfoCiInt64Field *field, SceInt64 *value)
{
	ConsoleInfoCiFileInt64Field *self = (ConsoleInfoCiFileInt64Field *)field;
	int ret = 0;
	SceUInt32 max;

	switch (self->m_value.kind) {
	case CONSOLE_INFO_CI_FILE_FLAG:
	case CONSOLE_INFO_CI_FILE_INT:
		max = self->m_value.max;
		break;
	case CONSOLE_INFO_CI_FILE_INT64: {
		SceUInt64 v = 0;

		ret = consoleInfoFileReadInt64(self->m_value.index, &v, 0);

		int unit = self->m_value.unit;

		max = self->m_value.max;
		if (unit == CONSOLE_INFO_CI_UNIT_USEC)
			*value = v / 1000000;
		else
			*value = v;
		break;
	}
	default:
		max = self->m_value.max;
		break;
	}
	if (max != 0 && (SceUInt64)*value > max)
		*value = max;
	return ret;
}

/* (0x810E9C3C) syscon's log entries, the battery's cycles and health */
static int consoleInfoCiGetSysconLog(ConsoleInfoCiIntField *field, int *value)
{
	SceUInt8 byte = 0;
	SceUInt16 word = 0;
	int ret;

	switch (field->m_index) {
	case CONSOLE_INFO_CI_FIELD_LOG_11:
		vshSysconLogReadData(11, &word, sizeof(word));
		break;
	case CONSOLE_INFO_CI_FIELD_LOG_23:
		vshSysconLogReadData(23, &word, sizeof(word));
		break;
	case CONSOLE_INFO_CI_FIELD_LOG_9:
		vshSysconLogReadData(9, &word, sizeof(word));
		break;
	case CONSOLE_INFO_CI_FIELD_LOG_10:
		vshSysconLogReadData(10, &word, sizeof(word));
		break;
	case CONSOLE_INFO_CI_FIELD_LOG_8:
		vshSysconLogReadData(8, &byte, sizeof(byte));
		*value = byte;
		return 0;
	case CONSOLE_INFO_CI_FIELD_LOG_5:
		vshSysconLogReadData(5, &word, sizeof(word));
		break;
	case CONSOLE_INFO_CI_FIELD_BATTERY_CYCLES:
		ret = scePowerGetBatteryCycleCount();
		if (ret >= 0)
			*value = ret;
		return 0;
	case CONSOLE_INFO_CI_FIELD_BATTERY_SOH:
		ret = scePowerGetBatterySOH();
		if (ret >= 0)
			*value = ret;
		return 0;
	}
	*value = word;
	return 0;
}

/* (0x810E9CF0) a Twitter account set */
static int consoleInfoCiGetTwitter(ConsoleInfoCiIntField *field, int *value)
{
	*value = 0;
	if (consoleInfoRegKeyIsSet(CONSOLE_INFO_CI_TWITTER, "access_token") &&
	    consoleInfoRegKeyIsSet(CONSOLE_INFO_CI_TWITTER, "access_token_secret"))
		*value = 1;
	return 0;
}

/* (0x810E9D30) a Facebook account set */
static int consoleInfoCiGetFacebook(ConsoleInfoCiIntField *field, int *value)
{
	*value = 0;
	if (consoleInfoRegKeyIsSet(CONSOLE_INFO_CI_FACEBOOK, "account"))
		*value = 1;
	return 0;
}

/* (0x810E9D58) an NP account signed in */
static int consoleInfoCiGetNpSignedIn(ConsoleInfoCiIntField *field, int *value)
{
	int signedIn;

	*value = 0;
	signedIn = 0;
	if (SceNpManager_BF8921C8(&signedIn) == 0 && signedIn != 0)
		*value = 1;
	return 0;
}

/* (0x810E9D96) libmtp's connections */
static int consoleInfoCiGetMtp(ConsoleInfoCiIntField *field, int *value)
{
	int count;

	*value = 0;
	count = 0;
	if (SceLibMtp_8E76FE4D(&count) == 0)
		*value = count;
	return 0;
}

/* (0x8154454C, 0x81544550, 0x81544554) the home screen's folders, the titles in them and its
   pages, when last counted */
static int s_folderCount;
static int s_folderTitleCount;
static unsigned int s_pageCount;

/* (0x810E9DD0) the home screen's folders and their titles counted, page by page */
__attribute__((noinline)) static void consoleInfoCiCountFolders(void)
{
	AppInfoSClient client;

	s_folderCount = 0;
	s_folderTitleCount = 0;
	s_pageCount = 0;
	SceLsdb_D322A870(&client);

	int count = SceLsdb_94128795(&client);

	if (count < 0) {
		SceLsdb_56D8DCF0(&client);
		return;
	}
	s_pageCount = count;
	{
		LsdbPageIconLists lists(1, LsdbIconListPtr(new LsdbIconList));

		for (unsigned int i = 0; i < s_pageCount; i++) {
			if (SceLsdb_D74AC354(&client, i, 1, &lists) != 0)
				break;

			LsdbIconList *icons = lists[0].get();

			for (LsdbIconList::iterator it = icons->begin(); it != icons->end(); ++it) {
				if ((*it)->type == LSDB_ICON_TYPE_FOLDER) {
					s_folderCount++;
					if ((*it)->folder.get() != NULL)
						s_folderTitleCount += (*it)->folder->titleIds.size();
				}
			}
		}
	}
	SceLsdb_56D8DCF0(&client);
}

/* (0x81544558, 0x8154455C, 0x81544560) the HDMI CEC events counted, when last read */
static int s_cecOneTouchPlay;
static int s_cecUserControl;
static int s_cecStandby;

/* (0x81544568..0x81544580) the Bluetooth devices registered, when last counted */
static int s_btDs3Count;
static int s_btDs4Count;
static int s_btHeadsetCount;
static int s_btSpeakerCount;
static int s_btPointerCount;
static int s_btKeyboardCount;
static int s_btBdRemoteCount;

/* (0x810E9F66) the Bluetooth devices registered counted, by kind */
__attribute__((noinline)) static void consoleInfoCiCountBtDevices(void)
{
	SceBtRegisteredInfo info[CONSOLE_INFO_CI_BT_MAX];

	s_btDs3Count = 0;
	s_btDs4Count = 0;
	s_btHeadsetCount = 0;
	s_btSpeakerCount = 0;
	s_btPointerCount = 0;
	s_btKeyboardCount = 0;
	s_btBdRemoteCount = 0;

	int count = sceBtGetRegisteredInfo(0, info, CONSOLE_INFO_CI_BT_MAX);

	if (count < 0)
		return;
	for (int i = 0; i < count; i++) {
		SceBtRegisteredInfo *device = &info[i];
		int vid = device->vid;

		if (vid == CONSOLE_INFO_CI_BT_VID_SONY && device->pid == CONSOLE_INFO_CI_BT_PID_DS3) {
			s_btDs3Count++;
		} else if (vid == CONSOLE_INFO_CI_BT_VID_SONY &&
			   device->pid == CONSOLE_INFO_CI_BT_PID_DS4) {
			s_btDs4Count++;
		} else if (vid == CONSOLE_INFO_CI_BT_VID_SONY &&
			   device->pid == CONSOLE_INFO_CI_BT_PID_BD_REMOTE) {
			s_btBdRemoteCount++;
		} else {
			SceUInt32 major = device->bt_class & CONSOLE_INFO_CI_BT_MAJOR_MASK;

			if (major == CONSOLE_INFO_CI_BT_MAJOR_IMAGING) {
			} else if (major == CONSOLE_INFO_CI_BT_MAJOR_PERIPHERAL) {
				if (device->bt_class & CONSOLE_INFO_CI_BT_POINTER)
					s_btPointerCount++;
				if (device->bt_class & CONSOLE_INFO_CI_BT_KEYBOARD)
					s_btKeyboardCount++;
			} else if (major == CONSOLE_INFO_CI_BT_MAJOR_AUDIO) {
				SceUInt32 minor = device->bt_class & CONSOLE_INFO_CI_BT_MINOR_MASK;

				if (minor == CONSOLE_INFO_CI_BT_MINOR_LOUDSPEAKER)
					s_btSpeakerCount++;
				else if (minor == CONSOLE_INFO_CI_BT_MINOR_HANDSFREE ||
					 minor == CONSOLE_INFO_CI_BT_MINOR_HEADSET)
					s_btHeadsetCount++;
			}
		}
	}
}

/* (0x810EA15E) the DualShock controllers registered */
static int consoleInfoCiGetBtDualShock(ConsoleInfoCiIntField *field, int *value)
{
	*value = s_btDs3Count + s_btDs4Count;
	return 0;
}

/* (0x810EA17A) */
static int consoleInfoCiGetBtHeadset(ConsoleInfoCiIntField *field, int *value)
{
	*value = s_btHeadsetCount;
	return 0;
}

/* (0x810EA18A) */
static int consoleInfoCiGetBtSpeaker(ConsoleInfoCiIntField *field, int *value)
{
	*value = s_btSpeakerCount;
	return 0;
}

/* (0x810EA19A) */
static int consoleInfoCiGetBtPointer(ConsoleInfoCiIntField *field, int *value)
{
	*value = s_btPointerCount;
	return 0;
}

/* (0x810EA1AA) */
static int consoleInfoCiGetBtKeyboard(ConsoleInfoCiIntField *field, int *value)
{
	*value = s_btKeyboardCount;
	return 0;
}

/* (0x810EA1BA) */
static int consoleInfoCiGetBtBdRemote(ConsoleInfoCiIntField *field, int *value)
{
	*value = s_btBdRemoteCount;
	return 0;
}

/* (0x810EA1CA) */
static int consoleInfoCiGetBtDs3(ConsoleInfoCiIntField *field, int *value)
{
	*value = s_btDs3Count;
	return 0;
}

/* (0x810EA1DA) */
static int consoleInfoCiGetBtDs4(ConsoleInfoCiIntField *field, int *value)
{
	*value = s_btDs4Count;
	return 0;
}

/* (0x810EA1EA) syscon's temperature log's entries */
static int consoleInfoCiGetTemperature(ConsoleInfoCiIntField *field, int *value)
{
	*value = 0;
	switch (field->m_index) {
	case CONSOLE_INFO_CI_FIELD_TEMPERATURE_1:
		*value = s_temperature1.value;
		break;
	case CONSOLE_INFO_CI_FIELD_TEMPERATURE_5:
		*value = s_temperature5.value;
		break;
	case CONSOLE_INFO_CI_FIELD_TEMPERATURE_6:
		*value = s_temperature6.value;
		break;
	case CONSOLE_INFO_CI_FIELD_TEMPERATURE_7:
		*value = s_temperature7.value;
		break;
	default:
		*value = CONSOLE_INFO_CI_TEMPERATURE_NONE;
		break;
	}
	return 0;
}

/* (0x810EA248) the NAT type */
static int consoleInfoCiGetNatType(ConsoleInfoCiIntField *field, int *value)
{
	SceSize info[CONSOLE_INFO_CI_NAT_INFO_SIZE / sizeof(SceSize)];

	info[0] = CONSOLE_INFO_CI_NAT_INFO_SIZE;
	if (SceNetCtl_F30365D6(info) != 0) {
		*value = 0;
		return 0;
	}
	*value = info[CONSOLE_INFO_CI_NAT_TYPE];
	return 0;
}

/* (0x810EA286) a PS4 linked */
static int consoleInfoCiGetPs4Link(ConsoleInfoCiIntField *field, int *value)
{
	int counter;

	*value = 0;
	counter = 0;
	if (sceRegMgrGetKeyInt("/CONFIG/PS4LINK", "counter", &counter) == 0 && counter != 0)
		*value = 1;
	return 0;
}

/* (0x810EA2D4) the home screen's folders, their titles and its pages */
static int consoleInfoCiGetHomeScreen(ConsoleInfoCiIntField *field, int *value)
{
	switch (field->m_index) {
	case CONSOLE_INFO_CI_FIELD_FOLDERS:
		*value = s_folderCount;
		break;
	case CONSOLE_INFO_CI_FIELD_FOLDER_TITLES:
		*value = s_folderTitleCount;
		break;
	case CONSOLE_INFO_CI_FIELD_PAGES:
		*value = s_pageCount;
		break;
	default:
		*value = 0;
		break;
	}
	return 0;
}

/* (0x810EA31C) a passcode server set for teleport */
static int consoleInfoCiGetTeleport(ConsoleInfoCiIntField *field, int *value)
{
	char server[32];

	*value = 0;
	sce_paf_memset(server, 0, sizeof(server));
	if (sceRegMgrGetKeyStr("/CONFIG/NP2/TELEPORT", "passcode_server", server,
			       sizeof(server)) == 0 && server[0] != '\0')
		*value = 1;
	return 0;
}

/* (0x810EA378) */
static int consoleInfoCiGetCecOneTouchPlay(ConsoleInfoCiIntField *field, int *value)
{
	*value = s_cecOneTouchPlay;
	return 0;
}

/* (0x810EA388) */
static int consoleInfoCiGetCecUserControl(ConsoleInfoCiIntField *field, int *value)
{
	*value = s_cecUserControl;
	return 0;
}

/* (0x810EA398) */
static int consoleInfoCiGetCecStandby(ConsoleInfoCiIntField *field, int *value)
{
	*value = s_cecStandby;
	return 0;
}

/* (0x810EA3A8) the network device a wireless one */
static int consoleInfoCiGetWireless(ConsoleInfoCiIntField *field, int *value)
{
	SceNetCtlInfo info;

	*value = 0;
	if (shellNetCtlInetGetInfo(SCE_NET_CTL_INFO_DEVICE, &info) >= 0 &&
	    info.device == SCE_NET_CTL_DEVICE_WIRELESS)
		*value = 1;
	return 0;
}

/* (0x810EA3E6) the home screen's icons */
static int consoleInfoCiGetIcons(ConsoleInfoCiIntField *field, int *value)
{
	AppInfoSClient client;

	*value = 0;
	SceLsdb_D322A870(&client);

	int count = SceLsdb_4DB9BCB4(&client);

	if (count < 0) {
		SceLsdb_56D8DCF0(&client);
		return 0;
	}
	if (count > CONSOLE_INFO_CI_ICONS_MAX)
		count = CONSOLE_INFO_CI_ICONS_MAX;
	*value = count;
	SceLsdb_56D8DCF0(&client);
	return 0;
}

/* (0x810EA43E) a screen lock passcode set */
static int consoleInfoCiGetScreenLock(ConsoleInfoCiIntField *field, int *value)
{
	char passcode[32];

	sce_paf_memset(passcode, 0, sizeof(passcode));
	if (sceRegMgrGetKeyStr("/CONFIG/SECURITY/SCREEN_LOCK", "passcode", passcode,
			       sizeof(passcode)) != 0)
		sce_paf_memset(passcode, 0, sizeof(passcode));
	*value = 0;
	if (passcode[0] != '\0')
		*value = 1;
	return 0;
}

/* (0x810EA4A4) where the theme is from */
static int consoleInfoCiGetThemeType(ConsoleInfoCiIntField *field, int *value)
{
	int type;

	switch (shellThemeGetType()) {
	case CONSOLE_INFO_CI_THEME_VS0:
		type = 0;
		break;
	case CONSOLE_INFO_CI_THEME_PD0:
		type = 1;
		break;
	case CONSOLE_INFO_CI_THEME_UX0:
		type = 2;
		break;
	default:
		type = 0;
		break;
	}
	*value = type;
	return 0;
}

/* (0x810EA4DA) the themes installed */
static int consoleInfoCiGetThemeCount(ConsoleInfoCiIntField *field, int *value)
{
	*value = 0;

	int count = shellThemeGetCount();

	if (count >= 0)
		*value = count;
	return 0;
}

/* (0x810EA4F0) the counts the fields give taken before the record is written: the home
   screen's, the Bluetooth devices', and on a PS TV the HDMI CEC counts and the temperature log,
   both then cleared */
void consoleInfoCiUpdateCounts(void)
{
	consoleInfoCiCountFolders();
	consoleInfoCiCountBtDevices();
	if (consoleInfoGetPlatform() != CONSOLE_INFO_PLATFORM_NO_TELEPHONY)
		return;

	s_cecOneTouchPlay = 0;
	s_cecUserControl = 0;
	s_cecStandby = 0;

	CecStatistics info = { sizeof(info) };

	if (sceAVConfigHdmiGetCecInfo(&info) >= 0) {
		s_cecOneTouchPlay = info.oneTouchPlay;
		s_cecUserControl = info.userControl;
		s_cecStandby = info.standby;
		sceAVConfigHdmiClearCecInfo();
	}
	s_temperature1.value = s_temperature5.value = s_temperature6.value = s_temperature7.value =
		CONSOLE_INFO_CI_TEMPERATURE_NONE;

	SceUInt8 log[CONSOLE_INFO_CI_TEMPERATURE_LOG_SIZE];

	if (vshSysconGetTemperatureLog(log) == 0) {
		s_temperature1.value = log[1];
		s_temperature5.value = log[5];
		s_temperature6.value = log[6];
		s_temperature7.value = log[7];
		vshSysconClearTemperatureLog(CONSOLE_INFO_CI_TEMPERATURE_LOG);
	}
}

/* (0x8141038C: Sony's build folds it into the code the build shares) a field the console does
   not give: 0 */
static int consoleInfoCiGetNone(ConsoleInfoCiIntField *field, int *value)
{
	*value = 0;
	return 0;
}

/* (0x815563B8, 0x81556854) the Int fields, and those ur0:ci/file gives */
static ConsoleInfoCiIntField s_intFields[CONSOLE_INFO_CI_INT_FIELDS] = {
	ConsoleInfoCiIntField(0, consoleInfoCiGetProductSubCode,
		ConsoleInfoCiKey(1, 'H', 4, true, 1, 3)),
	ConsoleInfoCiIntField(1, consoleInfoCiGetTelephony,
		ConsoleInfoCiKey(3, 'D', 1, true, 1, 3)),
	ConsoleInfoCiIntField(2, consoleInfoCiGetFlag,
		ConsoleInfoCiKey(5, 'D', 1, false, 1, 3)),
	ConsoleInfoCiIntField(3, consoleInfoCiGetBrightness,
		ConsoleInfoCiKey(26, 'D', 2, true, 1, 1)),
	ConsoleInfoCiIntField(4, consoleInfoCiGetMcc,
		ConsoleInfoCiKey(29, 'H', 4, true, 1, 3)),
	ConsoleInfoCiIntField(5, consoleInfoCiGetMnc,
		ConsoleInfoCiKey(30, 'H', 4, true, 1, 3)),
	ConsoleInfoCiIntField(6, NULL,
		ConsoleInfoCiKey(0, 'D', 1, false, 1, 3)),
	ConsoleInfoCiIntField(7, NULL,
		ConsoleInfoCiKey(0, 'D', 8, false, 1, 3)),
	ConsoleInfoCiIntField(8, NULL,
		ConsoleInfoCiKey(0, 'D', 8, false, 1, 3)),
	ConsoleInfoCiIntField(9, consoleInfoCiGetIduMode,
		ConsoleInfoCiKey(2, 'D', 1, true, 1, 3)),
	ConsoleInfoCiIntField(10, consoleInfoCiGetVideoCount,
		ConsoleInfoCiKey(101, 'D', 6, false, 1, 3)),
	ConsoleInfoCiIntField(11, consoleInfoCiGetMusicCount,
		ConsoleInfoCiKey(102, 'D', 6, false, 1, 3)),
	ConsoleInfoCiIntField(12, consoleInfoCiGetNone,
		ConsoleInfoCiKey(103, 'D', 6, false, 1, 3)),
	ConsoleInfoCiIntField(13, consoleInfoCiGetPhotoCount,
		ConsoleInfoCiKey(104, 'D', 6, false, 1, 3)),
	ConsoleInfoCiIntField(14, consoleInfoCiGetLsdbCount,
		ConsoleInfoCiKey(200, 'D', 6, false, 1, 3)),
	ConsoleInfoCiIntField(15, consoleInfoCiGetLsdbCount,
		ConsoleInfoCiKey(201, 'D', 6, false, 1, 3)),
	ConsoleInfoCiIntField(16, consoleInfoCiGetNone,
		ConsoleInfoCiKey(202, 'D', 6, false, 1, 3)),
	ConsoleInfoCiIntField(17, consoleInfoCiGetLsdbCount,
		ConsoleInfoCiKey(203, 'D', 6, false, 1, 3)),
	ConsoleInfoCiIntField(18, consoleInfoCiGetNone,
		ConsoleInfoCiKey(204, 'D', 6, false, 1, 3)),
	ConsoleInfoCiIntField(19, consoleInfoCiGetNone,
		ConsoleInfoCiKey(205, 'D', 6, false, 1, 3)),
	ConsoleInfoCiIntField(20, consoleInfoCiGetNone,
		ConsoleInfoCiKey(206, 'D', 6, false, 1, 3)),
	ConsoleInfoCiIntField(21, consoleInfoCiGetSysconLog,
		ConsoleInfoCiKey(18, 'D', 3, false, 1, 3)),
	ConsoleInfoCiIntField(22, consoleInfoCiGetSysconLog,
		ConsoleInfoCiKey(19, 'D', 3, false, 1, 3)),
	ConsoleInfoCiIntField(23, consoleInfoCiGetSysconLog,
		ConsoleInfoCiKey(20, 'D', 3, false, 1, 3)),
	ConsoleInfoCiIntField(24, consoleInfoCiGetSysconLog,
		ConsoleInfoCiKey(21, 'D', 3, false, 1, 3)),
	ConsoleInfoCiIntField(25, consoleInfoCiGetSysconLog,
		ConsoleInfoCiKey(22, 'D', 3, false, 1, 3)),
	ConsoleInfoCiIntField(26, consoleInfoCiGetSysconLog,
		ConsoleInfoCiKey(23, 'D', 3, false, 1, 3)),
	ConsoleInfoCiIntField(27, consoleInfoCiGetSysconLog,
		ConsoleInfoCiKey(24, 'D', 3, false, 1, 1)),
	ConsoleInfoCiIntField(28, consoleInfoCiGetSysconLog,
		ConsoleInfoCiKey(25, 'D', 3, false, 1, 1)),
	ConsoleInfoCiIntField(101, consoleInfoCiGetTwitter,
		ConsoleInfoCiKey(35, 'D', 1, false, 1, 3)),
	ConsoleInfoCiIntField(102, consoleInfoCiGetFacebook,
		ConsoleInfoCiKey(36, 'D', 1, false, 1, 3)),
	ConsoleInfoCiIntField(103, consoleInfoCiGetNpSignedIn,
		ConsoleInfoCiKey(37, 'D', 1, true, 1, 3)),
	ConsoleInfoCiIntField(104, consoleInfoCiGetMtp,
		ConsoleInfoCiKey(210, 'D', 1, false, 1, 3)),
	ConsoleInfoCiIntField(127, consoleInfoCiGetHomeScreen,
		ConsoleInfoCiKey(207, 'D', 5, false, 1, 3)),
	ConsoleInfoCiIntField(128, consoleInfoCiGetHomeScreen,
		ConsoleInfoCiKey(208, 'D', 6, false, 1, 3)),
	ConsoleInfoCiIntField(129, consoleInfoCiGetHomeScreen,
		ConsoleInfoCiKey(209, 'D', 4, false, 1, 3)),
	ConsoleInfoCiIntField(141, consoleInfoCiGetTeleport,
		ConsoleInfoCiKey(401, 'D', 1, true, 1, 2)),
	ConsoleInfoCiIntField(144, consoleInfoCiGetCecOneTouchPlay,
		ConsoleInfoCiKey(421, 'D', 3, true, 1, 2)),
	ConsoleInfoCiIntField(145, consoleInfoCiGetCecUserControl,
		ConsoleInfoCiKey(422, 'D', 3, true, 1, 2)),
	ConsoleInfoCiIntField(146, consoleInfoCiGetCecStandby,
		ConsoleInfoCiKey(423, 'D', 3, true, 1, 2)),
	ConsoleInfoCiIntField(151, consoleInfoCiGetWireless,
		ConsoleInfoCiKey(445, 'D', 1, true, 1, 2)),
	ConsoleInfoCiIntField(152, consoleInfoCiGetBtDualShock,
		ConsoleInfoCiKey(380, 'D', 2, true, 1, 3)),
	ConsoleInfoCiIntField(153, consoleInfoCiGetBtHeadset,
		ConsoleInfoCiKey(382, 'D', 2, true, 1, 3)),
	ConsoleInfoCiIntField(154, consoleInfoCiGetBtSpeaker,
		ConsoleInfoCiKey(384, 'D', 2, true, 1, 3)),
	ConsoleInfoCiIntField(155, consoleInfoCiGetBtPointer,
		ConsoleInfoCiKey(386, 'D', 2, true, 1, 3)),
	ConsoleInfoCiIntField(156, consoleInfoCiGetBtKeyboard,
		ConsoleInfoCiKey(388, 'D', 2, true, 1, 3)),
	ConsoleInfoCiIntField(157, consoleInfoCiGetBtBdRemote,
		ConsoleInfoCiKey(390, 'D', 2, true, 1, 3)),
	ConsoleInfoCiIntField(158, consoleInfoCiGetTemperature,
		ConsoleInfoCiKey(450, 'H', 2, true, 1, 2)),
	ConsoleInfoCiIntField(159, consoleInfoCiGetTemperature,
		ConsoleInfoCiKey(451, 'H', 2, true, 1, 2)),
	ConsoleInfoCiIntField(160, consoleInfoCiGetTemperature,
		ConsoleInfoCiKey(452, 'H', 2, true, 1, 2)),
	ConsoleInfoCiIntField(161, consoleInfoCiGetTemperature,
		ConsoleInfoCiKey(453, 'H', 2, true, 1, 2)),
	ConsoleInfoCiIntField(162, consoleInfoCiGetNatType,
		ConsoleInfoCiKey(446, 'D', 1, true, 1, 2)),
	ConsoleInfoCiIntField(179, consoleInfoCiGetPs4Link,
		ConsoleInfoCiKey(490, 'D', 1, true, 1, 3)),
	ConsoleInfoCiIntField(198, consoleInfoCiGetIcons,
		ConsoleInfoCiKey(515, 'D', 4, true, 1, 3)),
	ConsoleInfoCiIntField(199, consoleInfoCiGetBtDs3,
		ConsoleInfoCiKey(392, 'D', 2, true, 1, 3)),
	ConsoleInfoCiIntField(200, consoleInfoCiGetBtDs4,
		ConsoleInfoCiKey(393, 'D', 2, true, 1, 3)),
	ConsoleInfoCiIntField(209, consoleInfoCiGetScreenLock,
		ConsoleInfoCiKey(530, 'D', 1, true, 1, 1)),
	ConsoleInfoCiIntField(212, consoleInfoCiGetThemeType,
		ConsoleInfoCiKey(536, 'D', 4, false, 1, 3)),
	ConsoleInfoCiIntField(213, consoleInfoCiGetThemeCount,
		ConsoleInfoCiKey(537, 'D', 4, false, 1, 3))
};
static ConsoleInfoCiFileIntField s_fileIntFields[CONSOLE_INFO_CI_FILE_INT_FIELDS] = {
	ConsoleInfoCiFileIntField(29, ConsoleInfoCiKey(110, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 32, 999990, 0)),
	ConsoleInfoCiFileIntField(30, ConsoleInfoCiKey(111, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 39, 999990, 0)),
	ConsoleInfoCiFileIntField(31, ConsoleInfoCiKey(112, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 40, 999990, 0)),
	ConsoleInfoCiFileIntField(32, ConsoleInfoCiKey(113, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 43, 999990, 0)),
	ConsoleInfoCiFileIntField(33, ConsoleInfoCiKey(114, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 41, 999990, 0)),
	ConsoleInfoCiFileIntField(34, ConsoleInfoCiKey(115, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 48, 999990, 0)),
	ConsoleInfoCiFileIntField(35, ConsoleInfoCiKey(116, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 55, 999990, 0)),
	ConsoleInfoCiFileIntField(36, ConsoleInfoCiKey(117, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 56, 999990, 0)),
	ConsoleInfoCiFileIntField(37, ConsoleInfoCiKey(118, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 59, 999990, 0)),
	ConsoleInfoCiFileIntField(38, ConsoleInfoCiKey(119, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 57, 999990, 0)),
	ConsoleInfoCiFileIntField(39, ConsoleInfoCiKey(120, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 60, 999990, 0)),
	ConsoleInfoCiFileIntField(40, ConsoleInfoCiKey(121, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 67, 999990, 0)),
	ConsoleInfoCiFileIntField(41, ConsoleInfoCiKey(122, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 68, 999990, 0)),
	ConsoleInfoCiFileIntField(42, ConsoleInfoCiKey(123, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 71, 999990, 0)),
	ConsoleInfoCiFileIntField(43, ConsoleInfoCiKey(124, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 69, 999990, 0)),
	ConsoleInfoCiFileIntField(44, ConsoleInfoCiKey(125, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 84, 999990, 0)),
	ConsoleInfoCiFileIntField(45, ConsoleInfoCiKey(126, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 91, 999990, 0)),
	ConsoleInfoCiFileIntField(46, ConsoleInfoCiKey(127, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 92, 999990, 0)),
	ConsoleInfoCiFileIntField(47, ConsoleInfoCiKey(128, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 95, 999990, 0)),
	ConsoleInfoCiFileIntField(48, ConsoleInfoCiKey(129, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 93, 999990, 0)),
	ConsoleInfoCiFileIntField(49, ConsoleInfoCiKey(130, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 96, 999990, 0)),
	ConsoleInfoCiFileIntField(50, ConsoleInfoCiKey(131, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 103, 999990, 0)),
	ConsoleInfoCiFileIntField(51, ConsoleInfoCiKey(132, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 104, 999990, 0)),
	ConsoleInfoCiFileIntField(52, ConsoleInfoCiKey(133, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 107, 999990, 0)),
	ConsoleInfoCiFileIntField(53, ConsoleInfoCiKey(134, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 105, 999990, 0)),
	ConsoleInfoCiFileIntField(54, ConsoleInfoCiKey(135, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 108, 999990, 0)),
	ConsoleInfoCiFileIntField(55, ConsoleInfoCiKey(136, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 115, 999990, 0)),
	ConsoleInfoCiFileIntField(56, ConsoleInfoCiKey(137, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 116, 999990, 0)),
	ConsoleInfoCiFileIntField(57, ConsoleInfoCiKey(138, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 119, 999990, 0)),
	ConsoleInfoCiFileIntField(58, ConsoleInfoCiKey(139, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 117, 999990, 0)),
	ConsoleInfoCiFileIntField(59, ConsoleInfoCiKey(140, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 132, 999990, 0)),
	ConsoleInfoCiFileIntField(60, ConsoleInfoCiKey(141, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 139, 999990, 0)),
	ConsoleInfoCiFileIntField(61, ConsoleInfoCiKey(142, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 140, 999990, 0)),
	ConsoleInfoCiFileIntField(62, ConsoleInfoCiKey(143, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 143, 999990, 0)),
	ConsoleInfoCiFileIntField(63, ConsoleInfoCiKey(144, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 141, 999990, 0)),
	ConsoleInfoCiFileIntField(64, ConsoleInfoCiKey(301, 'D', 6, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 19, 999990, 0)),
	ConsoleInfoCiFileIntField(65, ConsoleInfoCiKey(302, 'D', 6, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 18, 999990, 0)),
	ConsoleInfoCiFileIntField(66, ConsoleInfoCiKey(303, 'D', 6, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 20, 999990, 0)),
	ConsoleInfoCiFileIntField(67, ConsoleInfoCiKey(304, 'D', 6, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 21, 999990, 0)),
	ConsoleInfoCiFileIntField(68, ConsoleInfoCiKey(250, 'D', 5, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 144, 65520, 1)),
	ConsoleInfoCiFileIntField(69, ConsoleInfoCiKey(251, 'D', 3, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 145, 240, 0)),
	ConsoleInfoCiFileIntField(70, ConsoleInfoCiKey(252, 'D', 3, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 146, 65520, 1)),
	ConsoleInfoCiFileIntField(71, ConsoleInfoCiKey(253, 'H', 8, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 147, 0xFFFFFFF0, 1)),
	ConsoleInfoCiFileIntField(72, ConsoleInfoCiKey(254, 'D', 6, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 148, 65520, 1)),
	ConsoleInfoCiFileIntField(73, ConsoleInfoCiKey(255, 'H', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 149, 65520, 1)),
	ConsoleInfoCiFileIntField(74, ConsoleInfoCiKey(256, 'H', 2, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 150, 65520, 1)),
	ConsoleInfoCiFileIntField(75, ConsoleInfoCiKey(257, 'D', 3, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 151, 65520, 1)),
	ConsoleInfoCiFileIntField(76, ConsoleInfoCiKey(92, 'D', 6, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 152, 9999, 1)),
	ConsoleInfoCiFileIntField(77, ConsoleInfoCiKey(93, 'D', 6, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 153, 9999, 1)),
	ConsoleInfoCiFileIntField(78, ConsoleInfoCiKey(94, 'D', 6, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 154, 9999, 1)),
	ConsoleInfoCiFileIntField(79, ConsoleInfoCiKey(95, 'D', 6, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 155, 9999, 1)),
	ConsoleInfoCiFileIntField(80, ConsoleInfoCiKey(96, 'D', 6, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 156, 9999, 1)),
	ConsoleInfoCiFileIntField(81, ConsoleInfoCiKey(258, 'H', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 157, 65520, 1)),
	ConsoleInfoCiFileIntField(82, ConsoleInfoCiKey(259, 'H', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 158, 65520, 1)),
	ConsoleInfoCiFileIntField(83, ConsoleInfoCiKey(260, 'H', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 159, 65520, 1)),
	ConsoleInfoCiFileIntField(84, ConsoleInfoCiKey(261, 'H', 2, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 160, 240, 1)),
	ConsoleInfoCiFileIntField(85, ConsoleInfoCiKey(262, 'H', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 161, 65520, 1)),
	ConsoleInfoCiFileIntField(86, ConsoleInfoCiKey(263, 'H', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 162, 65520, 1)),
	ConsoleInfoCiFileIntField(87, ConsoleInfoCiKey(264, 'H', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 163, 65520, 1)),
	ConsoleInfoCiFileIntField(88, ConsoleInfoCiKey(265, 'H', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 164, 65520, 1)),
	ConsoleInfoCiFileIntField(89, ConsoleInfoCiKey(266, 'H', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 165, 65520, 1)),
	ConsoleInfoCiFileIntField(90, ConsoleInfoCiKey(267, 'H', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 166, 65520, 1)),
	ConsoleInfoCiFileIntField(91, ConsoleInfoCiKey(268, 'H', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 167, 65520, 1)),
	ConsoleInfoCiFileIntField(92, ConsoleInfoCiKey(269, 'H', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 168, 65520, 1)),
	ConsoleInfoCiFileIntField(93, ConsoleInfoCiKey(320, 'D', 8, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 169, 99999990, 1)),
	ConsoleInfoCiFileIntField(94, ConsoleInfoCiKey(321, 'D', 8, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 170, 99999990, 1)),
	ConsoleInfoCiFileIntField(95, ConsoleInfoCiKey(322, 'D', 8, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 171, 9999, 1)),
	ConsoleInfoCiFileIntField(96, ConsoleInfoCiKey(323, 'D', 8, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 172, 0xFFFFFFFF, 1)),
	ConsoleInfoCiFileIntField(97, ConsoleInfoCiKey(324, 'D', 8, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 173, 9999, 1)),
	ConsoleInfoCiFileIntField(98, ConsoleInfoCiKey(325, 'D', 8, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 174, 9999, 1)),
	ConsoleInfoCiFileIntField(99, ConsoleInfoCiKey(326, 'D', 8, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 175, 9999, 1)),
	ConsoleInfoCiFileIntField(100, ConsoleInfoCiKey(327, 'D', 8, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 176, 9999, 1)),
	ConsoleInfoCiFileIntField(105, ConsoleInfoCiKey(211, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 177, 999990, 0)),
	ConsoleInfoCiFileIntField(106, ConsoleInfoCiKey(340, 'D', 6, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 179, 999990, 1)),
	ConsoleInfoCiFileIntField(107, ConsoleInfoCiKey(341, 'D', 6, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 180, 999990, 1)),
	ConsoleInfoCiFileIntField(108, ConsoleInfoCiKey(342, 'D', 6, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 181, 999990, 1)),
	ConsoleInfoCiFileIntField(109, ConsoleInfoCiKey(343, 'D', 6, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 182, 999990, 1)),
	ConsoleInfoCiFileIntField(110, ConsoleInfoCiKey(344, 'D', 4, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 183, 5000, 0)),
	ConsoleInfoCiFileIntField(111, ConsoleInfoCiKey(345, 'D', 1, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 184, 5, 0)),
	ConsoleInfoCiFileIntField(112, ConsoleInfoCiKey(346, 'D', 1, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 185, 5, 0)),
	ConsoleInfoCiFileIntField(113, ConsoleInfoCiKey(347, 'D', 1, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 186, 5, 0)),
	ConsoleInfoCiFileIntField(114, ConsoleInfoCiKey(348, 'D', 1, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 187, 5, 0)),
	ConsoleInfoCiFileIntField(115, ConsoleInfoCiKey(349, 'D', 2, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 188, 60, 0)),
	ConsoleInfoCiFileIntField(116, ConsoleInfoCiKey(145, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 189, 999990, 0)),
	ConsoleInfoCiFileIntField(117, ConsoleInfoCiKey(146, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 196, 999990, 0)),
	ConsoleInfoCiFileIntField(118, ConsoleInfoCiKey(147, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 197, 999990, 0)),
	ConsoleInfoCiFileIntField(119, ConsoleInfoCiKey(148, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 200, 999990, 0)),
	ConsoleInfoCiFileIntField(120, ConsoleInfoCiKey(149, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 198, 999990, 0)),
	ConsoleInfoCiFileIntField(121, ConsoleInfoCiKey(150, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 201, 999990, 0)),
	ConsoleInfoCiFileIntField(122, ConsoleInfoCiKey(151, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 208, 999990, 0)),
	ConsoleInfoCiFileIntField(123, ConsoleInfoCiKey(152, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 209, 999990, 0)),
	ConsoleInfoCiFileIntField(124, ConsoleInfoCiKey(153, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 212, 999990, 0)),
	ConsoleInfoCiFileIntField(125, ConsoleInfoCiKey(154, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 210, 999990, 0)),
	ConsoleInfoCiFileIntField(126, ConsoleInfoCiKey(350, 'D', 6, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 213, 999990, 1)),
	ConsoleInfoCiFileIntField(130, ConsoleInfoCiKey(352, 'D', 3, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 214, 999, 1)),
	ConsoleInfoCiFileIntField(131, ConsoleInfoCiKey(353, 'D', 3, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 215, 999, 1)),
	ConsoleInfoCiFileIntField(132, ConsoleInfoCiKey(354, 'D', 3, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 216, 999, 1)),
	ConsoleInfoCiFileIntField(133, ConsoleInfoCiKey(355, 'D', 3, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 217, 999, 1)),
	ConsoleInfoCiFileIntField(134, ConsoleInfoCiKey(356, 'D', 3, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 218, 999, 1)),
	ConsoleInfoCiFileIntField(135, ConsoleInfoCiKey(357, 'D', 3, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 219, 999, 1)),
	ConsoleInfoCiFileIntField(136, ConsoleInfoCiKey(358, 'D', 3, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 220, 999, 1)),
	ConsoleInfoCiFileIntField(137, ConsoleInfoCiKey(359, 'D', 3, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 221, 999, 1)),
	ConsoleInfoCiFileIntField(138, ConsoleInfoCiKey(360, 'D', 3, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 222, 999, 1)),
	ConsoleInfoCiFileIntField(139, ConsoleInfoCiKey(361, 'D', 3, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 223, 999, 1)),
	ConsoleInfoCiFileIntField(140, ConsoleInfoCiKey(370, 'D', 2, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 224, 99, 1)),
	ConsoleInfoCiFileIntField(142, ConsoleInfoCiKey(402, 'D', 3, true, 1, 2),
		ConsoleInfoCiFileValue(1, 0, 225, 999, 1)),
	ConsoleInfoCiFileIntField(143, ConsoleInfoCiKey(403, 'D', 3, true, 1, 2),
		ConsoleInfoCiFileValue(1, 0, 226, 999, 1)),
	ConsoleInfoCiFileIntField(147, ConsoleInfoCiKey(440, 'D', 2, true, 1, 2),
		ConsoleInfoCiFileValue(1, 0, 227, 99, 1)),
	ConsoleInfoCiFileIntField(148, ConsoleInfoCiKey(441, 'D', 2, true, 1, 2),
		ConsoleInfoCiFileValue(1, 0, 228, 99, 1)),
	ConsoleInfoCiFileIntField(149, ConsoleInfoCiKey(442, 'D', 2, true, 1, 2),
		ConsoleInfoCiFileValue(1, 0, 229, 99, 1)),
	ConsoleInfoCiFileIntField(150, ConsoleInfoCiKey(435, 'D', 2, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 230, 99, 1)),
	ConsoleInfoCiFileIntField(163, ConsoleInfoCiKey(455, 'D', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 231, 9999, 1)),
	ConsoleInfoCiFileIntField(164, ConsoleInfoCiKey(460, 'D', 1, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 232, 3, 0)),
	ConsoleInfoCiFileIntField(165, ConsoleInfoCiKey(461, 'D', 3, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 233, 100, 0)),
	ConsoleInfoCiFileIntField(166, ConsoleInfoCiKey(462, 'D', 3, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 234, 100, 0)),
	ConsoleInfoCiFileIntField(167, ConsoleInfoCiKey(463, 'D', 6, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 235, 999999, 0)),
	ConsoleInfoCiFileIntField(168, ConsoleInfoCiKey(464, 'D', 6, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 236, 999999, 0)),
	ConsoleInfoCiFileIntField(169, ConsoleInfoCiKey(465, 'D', 6, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 237, 999999, 0)),
	ConsoleInfoCiFileIntField(170, ConsoleInfoCiKey(466, 'D', 6, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 238, 999999, 0)),
	ConsoleInfoCiFileIntField(171, ConsoleInfoCiKey(467, 'D', 6, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 239, 999999, 0)),
	ConsoleInfoCiFileIntField(172, ConsoleInfoCiKey(468, 'D', 6, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 240, 999999, 0)),
	ConsoleInfoCiFileIntField(173, ConsoleInfoCiKey(473, 'D', 2, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 241, 99, 1)),
	ConsoleInfoCiFileIntField(174, ConsoleInfoCiKey(480, 'D', 3, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 242, 100, 1)),
	ConsoleInfoCiFileIntField(175, ConsoleInfoCiKey(481, 'D', 2, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 243, 99, 1)),
	ConsoleInfoCiFileIntField(176, ConsoleInfoCiKey(482, 'D', 2, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 244, 99, 1)),
	ConsoleInfoCiFileIntField(177, ConsoleInfoCiKey(485, 'D', 2, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 245, 99, 1)),
	ConsoleInfoCiFileIntField(178, ConsoleInfoCiKey(488, 'D', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 246, 9999, 1)),
	ConsoleInfoCiFileIntField(180, ConsoleInfoCiKey(492, 'D', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 247, 9999, 1)),
	ConsoleInfoCiFileIntField(181, ConsoleInfoCiKey(493, 'D', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 248, 9999, 1)),
	ConsoleInfoCiFileIntField(182, ConsoleInfoCiKey(494, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 249, 999999, 0)),
	ConsoleInfoCiFileIntField(183, ConsoleInfoCiKey(495, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 250, 999999, 0)),
	ConsoleInfoCiFileIntField(184, ConsoleInfoCiKey(496, 'D', 3, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 251, 100, 0)),
	ConsoleInfoCiFileIntField(185, ConsoleInfoCiKey(497, 'D', 8, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 252, 99999999, 0)),
	ConsoleInfoCiFileIntField(186, ConsoleInfoCiKey(498, 'D', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 253, 9999, 1)),
	ConsoleInfoCiFileIntField(187, ConsoleInfoCiKey(499, 'D', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 254, 9999, 1)),
	ConsoleInfoCiFileIntField(188, ConsoleInfoCiKey(500, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 255, 999999, 0)),
	ConsoleInfoCiFileIntField(189, ConsoleInfoCiKey(501, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 256, 999999, 0)),
	ConsoleInfoCiFileIntField(190, ConsoleInfoCiKey(502, 'D', 3, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 257, 100, 0)),
	ConsoleInfoCiFileIntField(191, ConsoleInfoCiKey(503, 'D', 8, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 258, 99999999, 0)),
	ConsoleInfoCiFileIntField(192, ConsoleInfoCiKey(504, 'D', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 259, 9999, 1)),
	ConsoleInfoCiFileIntField(193, ConsoleInfoCiKey(505, 'D', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 260, 9999, 1)),
	ConsoleInfoCiFileIntField(194, ConsoleInfoCiKey(506, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 261, 999999, 0)),
	ConsoleInfoCiFileIntField(195, ConsoleInfoCiKey(507, 'D', 6, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 262, 999999, 0)),
	ConsoleInfoCiFileIntField(196, ConsoleInfoCiKey(508, 'D', 3, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 263, 100, 0)),
	ConsoleInfoCiFileIntField(197, ConsoleInfoCiKey(509, 'D', 8, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 264, 99999999, 0)),
	ConsoleInfoCiFileIntField(201, ConsoleInfoCiKey(516, 'D', 4, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 265, 1000, 0)),
	ConsoleInfoCiFileIntField(202, ConsoleInfoCiKey(517, 'D', 4, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 266, 1000, 0)),
	ConsoleInfoCiFileIntField(203, ConsoleInfoCiKey(518, 'D', 4, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 267, 1000, 0)),
	ConsoleInfoCiFileIntField(204, ConsoleInfoCiKey(519, 'D', 2, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 268, 99, 0)),
	ConsoleInfoCiFileIntField(205, ConsoleInfoCiKey(520, 'D', 2, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 269, 99, 0)),
	ConsoleInfoCiFileIntField(206, ConsoleInfoCiKey(521, 'D', 2, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 270, 99, 0)),
	ConsoleInfoCiFileIntField(207, ConsoleInfoCiKey(522, 'D', 2, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 271, 99, 0)),
	ConsoleInfoCiFileIntField(208, ConsoleInfoCiKey(523, 'D', 2, false, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 272, 99, 0)),
	ConsoleInfoCiFileIntField(210, ConsoleInfoCiKey(539, 'D', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 273, 9999, 1)),
	ConsoleInfoCiFileIntField(211, ConsoleInfoCiKey(540, 'D', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 274, 9999, 1)),
	ConsoleInfoCiFileIntField(214, ConsoleInfoCiKey(538, 'D', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 275, 9999, 1)),
	ConsoleInfoCiFileIntField(215, ConsoleInfoCiKey(541, 'D', 6, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 276, 999990, 1)),
	ConsoleInfoCiFileIntField(216, ConsoleInfoCiKey(551, 'D', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 277, 9999, 1)),
	ConsoleInfoCiFileIntField(217, ConsoleInfoCiKey(552, 'D', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 278, 9999, 1)),
	ConsoleInfoCiFileIntField(218, ConsoleInfoCiKey(553, 'D', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 279, 9999, 1)),
	ConsoleInfoCiFileIntField(219, ConsoleInfoCiKey(554, 'D', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 280, 9999, 1)),
	ConsoleInfoCiFileIntField(220, ConsoleInfoCiKey(555, 'D', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 281, 9999, 1)),
	ConsoleInfoCiFileIntField(221, ConsoleInfoCiKey(556, 'D', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 282, 9999, 1)),
	ConsoleInfoCiFileIntField(222, ConsoleInfoCiKey(557, 'D', 4, true, 1, 3),
		ConsoleInfoCiFileValue(1, 0, 283, 9999, 1))
};

/* (0x810EA5FA) an Int field by its index */
ConsoleInfoCiField *consoleInfoCiGetIntField(unsigned int index)
{
	if (index >= CONSOLE_INFO_CI_INT_FIELDS + CONSOLE_INFO_CI_FILE_INT_FIELDS)
		return NULL;
	if (index >= CONSOLE_INFO_CI_INT_FIELDS)
		return &s_fileIntFields[index - CONSOLE_INFO_CI_INT_FIELDS];
	return &s_intFields[index];
}

/* (0x810EA62C) */
static int consoleInfoCiGetFileInt64(ConsoleInfoCiInt64Field *field, SceInt64 *value)
{
	SceUInt64 v = 0;
	int ret = consoleInfoFileReadInt64(0, &v, 0);

	*value = v;
	return ret;
}

/* (0x815581F8, 0x81544584) the phone network's interface statistics, and the result of reading
   them */
static SceNetCtlIfStat s_phoneIfStat;
static int s_phoneIfStatResult = SCE_ERROR_ERRNO_EINVAL;

/* (0x810EA66A) the phone network's statistics read: the seconds it was up */
static int consoleInfoCiGetPhoneTime(ConsoleInfoCiInt64Field *field, SceInt64 *value)
{
	s_phoneIfStat.size = sizeof(s_phoneIfStat);
	s_phoneIfStatResult = shellNetGetIfStat(SCE_NET_CTL_DEVICE_PHONE, &s_phoneIfStat);
	*value = s_phoneIfStat.totalSec;
	return s_phoneIfStatResult;
}

/* (0x810EA698) the bytes the phone network moved */
static int consoleInfoCiGetPhoneBytes(ConsoleInfoCiInt64Field *field, SceInt64 *value)
{
	if (s_phoneIfStatResult == 0)
		*value = s_phoneIfStat.txBytes + s_phoneIfStat.rxBytes;
	return s_phoneIfStatResult;
}

/* (0x810EA6C2) when the phone network's statistics were reset */
static int consoleInfoCiGetPhoneResetTick(ConsoleInfoCiInt64Field *field, SceInt64 *value)
{
	if (s_phoneIfStatResult == 0)
		*value = s_phoneIfStat.resetTick.tick;
	return s_phoneIfStatResult;
}

/* (0x81558238, 0x81558288) the Int64 fields, and those ur0:ci/file gives */
static ConsoleInfoCiInt64Field s_int64Fields[CONSOLE_INFO_CI_INT64_FIELDS] = {
	ConsoleInfoCiInt64Field(0, consoleInfoCiGetFileInt64,
		ConsoleInfoCiKey(6, 'T', 14, true, 1, 3)),
	ConsoleInfoCiInt64Field(1, consoleInfoCiGetPhoneTime,
		ConsoleInfoCiKey(32, 'D', 4, false, 1, 3)),
	ConsoleInfoCiInt64Field(2, consoleInfoCiGetPhoneBytes,
		ConsoleInfoCiKey(33, 'D', 4, false, 1, 3)),
	ConsoleInfoCiInt64Field(3, consoleInfoCiGetPhoneResetTick,
		ConsoleInfoCiKey(34, 'T', 14, false, 1, 3))
};
static ConsoleInfoCiFileInt64Field s_fileInt64Fields[CONSOLE_INFO_CI_FILE_INT64_FIELDS] = {
	ConsoleInfoCiFileInt64Field(4, ConsoleInfoCiKey(27, 'D', 10, true, 1, 1),
		ConsoleInfoCiFileValue(2, 2, 1, 0, 1)),
	ConsoleInfoCiFileInt64Field(5, ConsoleInfoCiKey(300, 'T', 14, true, 1, 3),
		ConsoleInfoCiFileValue(2, 1, 2, 0, 1)),
	ConsoleInfoCiFileInt64Field(6, ConsoleInfoCiKey(305, 'D', 10, true, 1, 3),
		ConsoleInfoCiFileValue(2, 2, 3, 99999990, 1))
};

/* (0x810EA6E0) an Int64 field by its index */
ConsoleInfoCiField *consoleInfoCiGetInt64Field(unsigned int index)
{
	if (index >= CONSOLE_INFO_CI_INT64_FIELDS + CONSOLE_INFO_CI_FILE_INT64_FIELDS)
		return NULL;
	if (index >= CONSOLE_INFO_CI_INT64_FIELDS)
		return &s_fileInt64Fields[index - CONSOLE_INFO_CI_INT64_FIELDS];
	return &s_int64Fields[index];
}

/* (0x810EA712) the network operator's name, escaped for a URI */
static int consoleInfoCiGetOperator(ConsoleInfoCiStringField *field, ConsoleInfoCiString *value)
{
	Plugin *plugin = Plugin::Find("telephony_plugin");

	if (plugin == NULL)
		return CONSOLE_INFO_ERROR_NO_TELEPHONY;

	TelephonyPluginInterface *iface = (TelephonyPluginInterface *)plugin->GetInterface(1);

	if (iface == NULL)
		return CONSOLE_INFO_ERROR_NO_TELEPHONY;

	TelephonyService *service = iface->getService();

	if (service == NULL)
		return CONSOLE_INFO_ERROR_NO_TELEPHONY;

	TelephonyOperator op;

	if (service->vtable->getOperator(service, &op).GetCode() != 0) {
		value->text[0] = '\0';
		value->length = 0;
		return 0;
	}

	unsigned int require;
	string name(op.GetName());

	require = 0;
	if (sceHttpUriEscape(value->text, &require, sizeof(value->text), name.c_str()) != 0)
		value->text[0] = '\0';
	value->length = sce_paf_strlen(value->text);
	return 0;
}

/* (0x810EA804) the names of the Bluetooth devices registered, each escaped for a URI */
static int consoleInfoCiGetBtNames(ConsoleInfoCiStringField *field, ConsoleInfoCiString *value)
{
	SceBtRegisteredInfo info[CONSOLE_INFO_CI_BT_MAX];
	char name[sizeof(info[0].name)];
	char escaped[1024];

	value->text[0] = '\0';
	value->length = 0;

	int count = sceBtGetRegisteredInfo(0, info, CONSOLE_INFO_CI_BT_MAX);
	string names;

	for (int i = 0; i < count; i++) {
		sce_paf_memset(name, 0, sizeof(name));
		sce_paf_memcpy(name, info[i].name, CONSOLE_INFO_CI_BT_NAME_SIZE);
		sce_paf_memset(escaped, 0, sizeof(escaped));
		unsigned int require = 0;

		if (sceHttpUriEscape(escaped, &require, sizeof(name), name) != 0)
			return 0;
		if (require >= sizeof(escaped))
			return 0;
		if (i != 0)
			names += ",";
		names += "BT-";
		names += escaped;
	}
	if (names.size() != 0 && names.size() < sizeof(value->text)) {
		sce_paf_strncpy(value->text, names.c_str(), names.size() + 1);
		value->length = sce_paf_strlen(value->text);
	}
	return 0;
}

/* (0x81558300) the network settings' fields */
static ConsoleInfoCiRegField s_netFields[CONSOLE_INFO_CI_NET_FIELDS] = {
	ConsoleInfoCiRegField(ConsoleInfoCiKey(77, 'D', 1, false, 1, 3), CONSOLE_INFO_CI_NET_COMMON,
		"enable_auto_connect", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(78, 'D', 4, false, 1, 3), CONSOLE_INFO_CI_NET_COMMON,
		"mtu", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(79, 'D', 1, false, 1, 3), CONSOLE_INFO_CI_NET_COMMON,
		"device", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(80, 'D', 1, false, 1, 3), CONSOLE_INFO_CI_NET_COMMON,
		"ether_mode", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(81, 'D', 1, false, 1, 3), CONSOLE_INFO_CI_NET_IP,
		"ip_config", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(82, 'D', 1, false, 1, 3), CONSOLE_INFO_CI_NET_IP,
		"dns_flag", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(83, 'D', 4, false, 1, 3), CONSOLE_INFO_CI_NET_APP,
		"http_proxy_flag", "int")
};

/* (0x810EAA72) the network settings used: their number, then each network field's values, one
   per setting ("<n>&d<id>=<v>,<v>...") */
static int consoleInfoCiGetNetSettings(ConsoleInfoCiStringField *field, ConsoleInfoCiString *value)
{
	int flag;
	vector<SceUInt8> settings;
	char folder[CONSOLE_INFO_CI_NET_FOLDER_SIZE];

	for (SceUInt8 setting = 1; setting < CONSOLE_INFO_CI_NET_SETTINGS + 1; setting++) {
		flag = 0;
		sce_paf_snprintf(folder, sizeof(folder), CONSOLE_INFO_CI_NET_COMMON, setting);
		if (sceRegMgrGetKeyInt(folder, "conf_flag", &flag) != 0)
			return 0;
		if (flag != 0)
			settings.push_back(setting);
	}

	char *text = value->text;

	sce_paf_snprintf(text, sizeof(value->text) - 1, "%d", settings.size());

	SceSize length = sce_paf_strlen(text);
	SceSize left = sizeof(value->text) - 1 - length;

	text += length;
	for (unsigned int i = 0; i < CONSOLE_INFO_CI_NET_FIELDS; i++) {
		ConsoleInfoCiRegField *net = &s_netFields[i];

		sce_paf_snprintf(text, left, "&d%d=", net->m_key.m_id);
		length = sce_paf_strlen(text);
		left -= length;
		text += length;
		for (vector<SceUInt8>::iterator it = settings.begin(); it != settings.end(); ++it) {
			sce_paf_snprintf(folder, sizeof(folder), net->m_category, *it);

			const char *name = net->m_name;

			flag = 0;
			if (sceRegMgrGetKeyInt(folder, name, &flag) != 0)
				return 0;
			if (it == settings.begin())
				sce_paf_snprintf(text, left, "%d", flag);
			else
				sce_paf_snprintf(text, left, ",%d", flag);
			length = sce_paf_strlen(text);
			left -= length;
			text += length;
		}
	}
	return 0;
}

/* (0x8155838C) the String fields */
static ConsoleInfoCiStringField s_stringFields[CONSOLE_INFO_CI_STRING_FIELDS] = {
	ConsoleInfoCiStringField(0, consoleInfoCiGetOperator,
		ConsoleInfoCiKey(31, 'S', 255, true, 1, 3)),
	ConsoleInfoCiStringField(1, consoleInfoCiGetBtNames,
		ConsoleInfoCiKey(4, 'S', 255, false, 1, 3)),
	ConsoleInfoCiStringField(2, consoleInfoCiGetNetSettings,
		ConsoleInfoCiKey(76, 'S', 640, false, 8, 3)),
	ConsoleInfoCiStringField(3, consoleInfoCiGetStorage,
		ConsoleInfoCiKey(50, 'S', 14, false, 6, 3)),
	ConsoleInfoCiStringField(4, consoleInfoCiGetCompatTitles,
		ConsoleInfoCiKey(436, 'S', 99, false, 1, 3)),
	ConsoleInfoCiStringField(5, consoleInfoCiGetHdmiMonitorName,
		ConsoleInfoCiKey(412, 'S', 255, true, 1, 2)),
	ConsoleInfoCiStringField(6, consoleInfoCiGetHdmiEdidId,
		ConsoleInfoCiKey(413, 'S', 255, true, 1, 2)),
	ConsoleInfoCiStringField(7, consoleInfoCiGetParentalDurations,
		ConsoleInfoCiKey(472, 'S', 35, true, 1, 3)),
	ConsoleInfoCiStringField(8, consoleInfoCiGetLiveAreaJoin,
		ConsoleInfoCiKey(486, 'S', 99, true, 1, 3)),
	ConsoleInfoCiStringField(9, consoleInfoCiGetLiveAreaAccess,
		ConsoleInfoCiKey(487, 'S', 139, true, 1, 3))
};

/* (0x810EAC1E) the String fields' values taken before the record is written: the HDMI monitor's
   on a PS TV */
void consoleInfoCiUpdateStrings(void)
{
	if (consoleInfoGetPlatform() == CONSOLE_INFO_PLATFORM_NO_TELEPHONY)
		consoleInfoCiUpdateHdmiMonitor();
}

/* (0x810EAC30) a String field by its index */
ConsoleInfoCiField *consoleInfoCiGetStringField(unsigned int index)
{
	if (index < CONSOLE_INFO_CI_STRING_FIELDS)
		return &s_stringFields[index];
	return NULL;
}
