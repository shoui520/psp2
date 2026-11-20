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
extern "C" int SceNpManagerSP_7C78A608(int *signedIn);

/* PLACEHOLDER: libmtp's count of the console's connections, in *count */
extern "C" int SceLibMtp_8E76FE4D(int *count);

/* syscon's log: an entry read (0x81417CBC, placed with the code the build shares, over
   _vshSysconLogReadData) */
int vshSysconLogReadData(int id, void *buf, SceSize size);

/* syscon's temperature log read (0x81417CD0, placed with the code the build shares, over
   _vshSysconGetTemperatureLog) */
int vshSysconGetTemperatureLog(SceUInt8 *log);

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
	if (prev->Read(&prevValue) == 0 && prevValue == value && !ci->m_changed &&
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
	if (prev->Read(&prevValue) == 0 && prevValue == value && !ci->m_changed &&
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

	if (ci->m_changed || m_key.m_force)
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
				goto error;
			break;
		}
		if (access.size() != 0)
			access.append(1, ',');
		access += titleId;
		ret = file->Read(&count, sizeof(count));
		if (ret != sizeof(count))
			goto error;
		if (count > CONSOLE_INFO_CI_ACCESS_MAX)
			count = CONSOLE_INFO_CI_ACCESS_MAX;
		ret = sce_paf_snprintf(number, sizeof(number), "%d", count);
		if (ret < 0)
			goto error;
		if (access.size() != 0)
			access.append(1, ',');
		access += number;
	}
	goto done;
error:
	access.clear();
done:
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
	if (SceNpManagerSP_7C78A608(&signedIn) == 0 && signedIn != 0)
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

/* TEMPORARY: keeps the readers until the table is written */
void *g_consoleInfoCiPending[] = {
	(void *)consoleInfoCiGetProductSubCode, (void *)consoleInfoCiGetTelephony,
	(void *)consoleInfoCiGetIduMode, (void *)consoleInfoCiGetFlag,
	(void *)consoleInfoCiGetBrightness, (void *)consoleInfoCiGetMcc,
	(void *)consoleInfoCiGetMnc, (void *)consoleInfoCiGetStorage,
	(void *)consoleInfoCiGetCompatTitles, (void *)consoleInfoCiGetHdmiMonitorName,
	(void *)consoleInfoCiGetHdmiEdidId, (void *)consoleInfoCiGetParentalDurations,
	(void *)consoleInfoCiGetLiveAreaJoin, (void *)consoleInfoCiGetLiveAreaAccess,
	(void *)consoleInfoCiGetSysconLog, (void *)consoleInfoCiGetTwitter,
	(void *)consoleInfoCiGetFacebook, (void *)consoleInfoCiGetNpSignedIn,
	(void *)consoleInfoCiGetMtp, (void *)consoleInfoCiCountFolders,
	(void *)consoleInfoCiCountBtDevices, (void *)consoleInfoCiGetBtDualShock,
	(void *)consoleInfoCiGetBtHeadset, (void *)consoleInfoCiGetBtSpeaker,
	(void *)consoleInfoCiGetBtPointer, (void *)consoleInfoCiGetBtKeyboard,
	(void *)consoleInfoCiGetBtBdRemote, (void *)consoleInfoCiGetBtDs3,
	(void *)consoleInfoCiGetBtDs4, (void *)consoleInfoCiGetTemperature,
	(void *)consoleInfoCiGetNatType, (void *)consoleInfoCiGetPs4Link,
	(void *)consoleInfoCiGetHomeScreen, (void *)consoleInfoCiGetTeleport,
	(void *)consoleInfoCiGetCecOneTouchPlay, (void *)consoleInfoCiGetCecUserControl, (void *)consoleInfoCiGetCecStandby,
	(void *)consoleInfoCiGetWireless, (void *)consoleInfoCiGetIcons,
	(void *)consoleInfoCiGetScreenLock, (void *)consoleInfoCiGetThemeType,
	(void *)consoleInfoCiGetThemeCount, (void *)consoleInfoCiGetFileInt64,
	(void *)consoleInfoCiGetPhoneTime, (void *)consoleInfoCiGetPhoneBytes,
	(void *)consoleInfoCiGetPhoneResetTick, (void *)consoleInfoCiGetOperator,
};
