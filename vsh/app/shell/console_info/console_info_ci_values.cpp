/*
 * console_info_ci_values.cpp: the "ci" record's values files of fields (0x810F4294-0x810F4B78):
 * those of the fields a lookup gives, their keys and line formats, and the registry keys' fields.
 * The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/string.h>
#include <std/stdio.h>
#include <regmgr/regmgr.h>
#include "console_info.h"

using namespace paf;

/* the registry keys' fields */
#define CONSOLE_INFO_CI_REG_FIELDS	58

/* the size of a key's format piece */
#define CONSOLE_INFO_CI_FORMAT_SIZE	64

/* the size of a registry key's text */
#define CONSOLE_INFO_CI_REG_TEXT_SIZE	512

/* (0x81559110) the registry keys' fields */
static ConsoleInfoCiRegField s_regFields[CONSOLE_INFO_CI_REG_FIELDS] = {
	ConsoleInfoCiRegField(ConsoleInfoCiKey(54, 'D', 1, false, 1, 3),
		"/CONFIG/SYSTEM", "flight_mode", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(55, 'D', 4, false, 1, 3),
		"/CONFIG/SYSTEM", "language", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(56, 'D', 1, false, 1, 3),
		"/CONFIG/SYSTEM", "initialize", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(57, 'D', 1, false, 1, 3),
		"/CONFIG/SHELL", "bgm", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(58, 'D', 1, false, 1, 3),
		"/CONFIG/SHELL", "notify_ps_button", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(59, 'D', 1, false, 1, 3),
		"/CONFIG/SHELL", "notify_sound", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(60, 'D', 1, false, 1, 3),
		"/CONFIG/SHELL", "mic_mute", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(61, 'D', 1, false, 1, 3),
		"/CONFIG/SHELL", "voice_priority", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(62, 'D', 2, false, 1, 3),
		"/CONFIG/SECURITY/PARENTAL", "content_start_control", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(63, 'D', 1, false, 1, 3),
		"/CONFIG/SECURITY/PARENTAL", "browser_start_control", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(64, 'D', 3, false, 1, 3),
		"/CONFIG/DATE", "time_zone", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(65, 'D', 1, false, 1, 3),
		"/CONFIG/DATE", "set_automatically", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(66, 'D', 3, false, 1, 1),
		"/CONFIG/POWER_SAVING", "suspend_interval", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(0, 'D', 2, false, 1, 3),
		"/CONFIG/DISPLAY", "brightness", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(68, 'D', 2, false, 1, 3),
		"/CONFIG/SOUND", "main_volume", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(69, 'D', 1, false, 1, 3),
		"/CONFIG/SOUND", "avls", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(70, 'D', 1, false, 1, 3),
		"/CONFIG/BT", "bt_enable", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(71, 'D', 1, false, 1, 3),
		"/CONFIG/BT", "bt_mic", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(72, 'D', 1, false, 1, 3),
		"/CONFIG/NET", "wifi_flag", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(73, 'D', 2, false, 1, 3),
		"/CONFIG/NET", "adhoc_channel", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(74, 'D', 1, false, 1, 3),
		"/CONFIG/NET", "ap_auto_config", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(75, 'D', 1, false, 1, 3),
		"/CONFIG/NET", "enable_impose_scanlist", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(84, 'D', 1, false, 1, 3),
		"/CONFIG/NP2", "privacy_confirmation", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(85, 'D', 1, false, 1, 3),
		"/CONFIG/LOCATION", "enable_service", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(86, 'D', 1, false, 1, 3),
		"/CONFIG/LOCATION", "enable_parental", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(0, 'D', 1, false, 1, 3),
		"/CONFIG/MAIL", "enable_network", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(88, 'D', 1, false, 1, 3),
		"/CONFIG/TEL", "enable_wwan", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(89, 'H', 8, false, 1, 3),
		"/CONFIG/BROWSER/ADDIN/TRENDMICRO", "tm_service", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(90, 'D', 1, false, 1, 3),
		"/CONFIG/THEME", "wallpaper", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(91, 'D', 1, false, 1, 3),
		"/CONFIG/USB", "usb_charge_enable", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(67, 'D', 1, false, 1, 3),
		"/CONFIG/SYSTEM", "key_pad", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(38, 'D', 1, true, 1, 3),
		"/CONFIG/NP2/AUTOUPDATE", "enable", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(39, 'D', 1, true, 1, 3),
		"/CONFIG/NP2/AUTOUPDATE", "application", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(40, 'D', 1, true, 1, 3),
		"/CONFIG/NP2/AUTOUPDATE", "system", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(41, 'D', 1, true, 1, 3),
		"/CONFIG/NP2/AUTOUPDATE", "trophy", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(42, 'D', 1, true, 1, 3),
		"/CONFIG/NP2/AUTOUPDATE", "save_data", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(400, 'D', 1, true, 1, 2),
		"/CONFIG/NP2/TELEPORT", "enable_media_transfer", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(410, 'D', 1, true, 1, 2),
		"/CONFIG/DISPLAY", "hdmi_resolution_mode", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(411, 'D', 1, true, 1, 2),
		"/CONFIG/DISPLAY", "hdmi_out_scaling_ratio", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(420, 'D', 1, true, 1, 2),
		"/CONFIG/SYSTEM", "hdmi_use_cec", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(447, 'D', 1, true, 1, 2),
		"/CONFIG/NET/ETH/IP", "ip_config", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(430, 'D', 1, true, 1, 2),
		"/CONFIG/DISPLAY", "dimming_interval", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(431, 'D', 1, true, 1, 2),
		"/CONFIG/POWER_SAVING", "suspend_interval", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(432, 'D', 1, true, 1, 2),
		"/CONFIG/POWER_SAVING", "controller_off_interval", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(470, 'D', 1, true, 1, 3),
		"/CONFIG/SECURITY/PARENTAL", "restrict_mode", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(471, 'D', 1, true, 1, 3),
		"/CONFIG/SECURITY/PARENTAL", "restrict_duration_one", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(491, 'D', 1, true, 1, 3),
		"/CONFIG/PS4LINK", "counter", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(474, 'D', 1, true, 1, 3),
		"/CONFIG/SECURITY/PARENTAL/", "store_start_control", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(475, 'D', 2, true, 1, 3),
		"/CONFIG/SECURITY/PARENTAL/", "content_start_control", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(542, 'D', 1, false, 1, 3),
		"/CONFIG/ACCESSIBILITY/", "zoom", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(543, 'D', 1, false, 1, 3),
		"/CONFIG/ACCESSIBILITY/", "invert_color", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(544, 'D', 1, false, 1, 3),
		"/CONFIG/ACCESSIBILITY/", "large_text", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(545, 'D', 1, false, 1, 3),
		"/CONFIG/ACCESSIBILITY/", "bold_text", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(546, 'D', 1, false, 1, 3),
		"/CONFIG/ACCESSIBILITY/", "contrast", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(547, 'D', 1, false, 1, 3),
		"/CONFIG/ACCESSIBILITY/CC/", "enable_cc", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(548, 'D', 1, false, 1, 3),
		"/CONFIG/ACCESSIBILITY/CC/", "content_specific", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(549, 'D', 1, false, 1, 3),
		"/CONFIG/ACCESSIBILITY/", "keyremap_enable", "int"),
	ConsoleInfoCiRegField(ConsoleInfoCiKey(550, 'D', 1, false, 1, 3),
		"/CONFIG/ACCESSIBILITY/", "shortcut", "int")
};

/* (0x810F4294) */
ConsoleInfoCiFieldValueFile::ConsoleInfoCiFieldValueFile(int size, GetField getField)
	: ConsoleInfoCiValueFile(size), m_getField(getField), m_begin(NULL), m_end(NULL)
{
}

/* (0x810F42B6) */
ConsoleInfoCiFieldValueFile::ConsoleInfoCiFieldValueFile(int size, GetField getField,
							 Callback begin, Callback end)
	: ConsoleInfoCiValueFile(size), m_getField(getField), m_begin(begin), m_end(end)
{
}

/* (0x810F42EA, 0x810F4306) */
ConsoleInfoCiFieldValueFile::~ConsoleInfoCiFieldValueFile()
{
}

/* (0x810F4326) each field's value now */
int ConsoleInfoCiFieldValueFile::Store()
{
	ConsoleInfoCiField *field;
	int ret = 0;

	unsigned int i = 0;

	if (m_begin != NULL)
		m_begin();
	for (field = m_getField(i++); field != NULL; field = m_getField(i++)) {
		if (field->m_key.m_id != 0) {
			ret = field->Read(this);
			if (ret != 0)
				break;
		}
	}
	if (m_end != NULL)
		m_end();
	return ret;
}

/* (0x810F4368) the lines of the fields whose values changed since the previous values */
int ConsoleInfoCiFieldValueFile::Compare(ConsoleInfoCi *ci)
{
	ConsoleInfoCiFieldValueFile prev(m_size);
	ConsoleInfoCiField *field;
	unsigned int i = 0;
	int ret = prev.OpenPrev();

	if (ret == 0) {
		for (field = m_getField(i++); field != NULL; field = m_getField(i++)) {
			if (field->m_key.m_id != 0) {
				ret = field->Write(&prev, this, ci);
				if (ret != 0)
					break;
			}
		}
	}
	return ret;
}

/* (0x810F4416) */
int ConsoleInfoCiValueFile::Write(const void *value)
{
	int ret = m_file->Write(value, m_size);

	if (ret < 0)
		return ret;
	return 0;
}

/* (0x810F442C) */
int ConsoleInfoCiValueFile::Read(void *value)
{
	int ret = m_file->Read(value, m_size);

	if (ret < 0)
		return ret;
	if (ret == 0)
		return CONSOLE_INFO_ERROR_END_OF_FILE;
	return 0;
}

/* (0x810F444C) */
ConsoleInfoCiKey::ConsoleInfoCiKey(SceUInt16 id, char type, SceUInt16 size, bool force,
				   SceUInt8 lineMode, SceUInt8 platforms)
	: m_id(id), m_size(size), m_type(type), m_force(force), m_lineMode(lineMode)
{
	switch (consoleInfoGetPlatform()) {
	case CONSOLE_INFO_PLATFORM_VITA:
		if ((platforms & 1) == 0)
			m_id = 0;
		break;
	case CONSOLE_INFO_PLATFORM_DOLCE:
		if ((platforms & 2) == 0)
			m_id = 0;
		break;
	}
}

/* (0x810F4490) "d<id>=" and the value's conversion */
string ConsoleInfoCiKey::GetFormat(const char *decimal, const char *hex) const
{
	char buf[CONSOLE_INFO_CI_FORMAT_SIZE];
	int id = m_id;
	string format;

	sce_paf_snprintf(buf, sizeof(buf), "d%d", id);
	format += buf;
	format += "=";
	switch (m_type) {
	case 'D':
		format += "%";
		format += decimal;
		break;
	case 'H':
		format += "%0";
		sce_paf_snprintf(buf, sizeof(buf), "%d", m_size);
		format += buf;
		format += hex;
		break;
	case 'S':
		format += "%s";
		break;
	case CONSOLE_INFO_CI_TYPE_TIME:
		format += "%04d%02d%02d%02d%02d%02d";
		break;
	}
	return format;
}

/* (0x810F4898) */
ConsoleInfoCiRegValueFile::ConsoleInfoCiRegValueFile()
	: ConsoleInfoCiValueFile(0)
{
}

/* (0x810F48B2, 0x810F48CE) */
ConsoleInfoCiRegValueFile::~ConsoleInfoCiRegValueFile()
{
}

/* (0x810F48EE) the key's value now, or the error reading it */
__attribute__((noinline)) int ConsoleInfoCiRegValueFile::StoreKey(const ConsoleInfoCiRegField *field)
{
	if (field->m_key.m_id == 0)
		return 0;

	int ret = CONSOLE_INFO_ERROR_INVALID_ARGUMENT;

	if (field->m_type[0] == 's') {
		/* text keys are not kept */
	} else if (field->m_type[0] == 'i') {
		int value = 0;
		int error = sceRegMgrGetKeyInt(field->m_category, field->m_name, &value);

		if (error != 0)
			value = error;
		ret = m_file->Write(&value, sizeof(value));
		if (ret >= 0)
			return 0;
	}
	return ret;
}

/* (0x810F4952) */
int ConsoleInfoCiRegValueFile::Store()
{
	int ret;

	for (unsigned int i = 0; i < CONSOLE_INFO_CI_REG_FIELDS; i++) {
		ret = StoreKey(&s_regFields[i]);
		if (ret != 0)
			break;
	}
	return ret;
}

/* (0x810F4976) */
__attribute__((noinline)) int ConsoleInfoCiRegValueFile::ReadKey(const ConsoleInfoCiRegField *field,
								  int *value)
{
	int ret = CONSOLE_INFO_ERROR_INVALID_ARGUMENT;

	if (field->m_type[0] == 's') {
		/* text keys are not kept */
	} else if (field->m_type[0] == 'i') {
		ret = m_file->Read(value, sizeof(*value));
		if (ret >= 0) {
			if (ret == 0)
				return CONSOLE_INFO_ERROR_END_OF_FILE;
			return 0;
		}
	}
	return ret;
}

/* (0x810F49AA) the key's line, when its value changed */
int ConsoleInfoCiRegValueFile::CompareKey(ConsoleInfoCiRegValueFile *prev, ConsoleInfoCi *ci,
					  const ConsoleInfoCiRegField *field)
{
	if (field->m_key.m_id == 0)
		return 0;

	int ret = CONSOLE_INFO_ERROR_INVALID_ARGUMENT;

	if (field->m_type[0] == 's') {
		/* text keys are not kept */
	} else if (field->m_type[0] == 'i') {
		int prevValue = 0;
		int value = 0;

		ret = ReadKey(field, &value);
		if (ret != 0)
			return ret;
		if (prev->ReadKey(field, &prevValue) == 0 && prevValue == value && !ci->m_updated &&
		    !field->m_key.m_force)
			return 0;

		string format = field->m_key.GetFormat("d", "x");

		sce_paf_snprintf(ci->m_line, CONSOLE_INFO_BUFFER_SIZE, format.c_str(), value);
		ret = ci->AppendLine(sce_paf_strlen(ci->m_line), 1);
		if (ret != 0)
			return ret;
		return 0;
	}
	return ret;
}

/* (0x810F4A8C) */
int ConsoleInfoCiRegValueFile::Compare(ConsoleInfoCi *ci)
{
	ConsoleInfoCiRegValueFile prev;
	int ret = prev.OpenPrev();

	if (ret != 0)
		return ret;
	for (unsigned int i = 0; i < CONSOLE_INFO_CI_REG_FIELDS; i++) {
		ret = CompareKey(&prev, ci, &s_regFields[i]);
		if (ret != 0)
			break;
	}
	prev.Close();
	return ret;
}

/* (0x810F4B26) */
bool consoleInfoRegKeyIsSet(const char *category, const char *name)
{
	char text[CONSOLE_INFO_CI_REG_TEXT_SIZE];

	if (name == NULL)
		return false;
	text[0] = '\0';
	return sceRegMgrGetKeyStr(category, name, text, sizeof(text)) == 0 && text[0] != '\0';
}
