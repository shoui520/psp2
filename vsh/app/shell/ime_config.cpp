/*
 * ime_config.cpp: the IME settings (0x81140118-0x81140C48), one table for no language and one per
 * language bit, read from the registry ("/CONFIG/IME/<language>") when the service starts and
 * written back by a job when a client changes one. The registry's names are Sony's; the file's
 * and the other names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <job/job.h>
#include <common/shared_ptr.h>
#include <regmgr/regmgr.h>
#include "ime_service.h"

using namespace paf;

/* the input type setting: the type in the low 16 bits, the predictive bit at the top */
#define IME_CONFIG_INPUT_TYPE_MASK	0xFFFF
#define IME_CONFIG_PREDICTIVE_BIT	0x80000000

/* where the last unit keeps its table's index */
#define IME_CONFIG_LAST_UNIT_SHIFT	26

static ImeConfig *s_instance;			/* (0x815448CC) */

/* (0x81140118) a key's name in the registry */
const char *ImeConfig::GetKeyName(SceUInt32 key)
{
	switch (key) {
	case IME_CONFIG_KEY_INPUT_TYPE:
	case IME_CONFIG_KEY_PREDICTIVE:
		return "input_type";
	case IME_CONFIG_KEY_AUTO_CAPITALIZATION:
		return "auto_capitalization";
	case IME_CONFIG_KEY_PREDICTIVE_TEXT:
		return "predictive_text";
	case IME_CONFIG_KEY_KB_TYPE:
		return "kb_type";
	case IME_CONFIG_KEY_KB_ZH_T_INPUT_METHOD:
		return "kb_zh_t_input_method";
	case IME_CONFIG_KEY_KB_JA_INPUT_METHOD:
		return "kb_ja_input_method";
	case IME_CONFIG_KEY_KB_KEY_REPEAT_START:
		return "kb_key_repeat_start";
	case IME_CONFIG_KEY_KB_KEY_REPEAT_INTERVAL:
		return "kb_key_repeat_interval";
	case IME_CONFIG_KEY_LAST_UNIT:
		return "last_unit";
	case IME_CONFIG_KEY_UI_HINT:
		return "ui_hint";
	default:
		return NULL;
	}
}

/* (0x811401AE) a language's registry folder (SCE_IME_LANGUAGE_* bits) */
const char *ImeConfig::GetLanguagePath(SceUInt64 language)
{
	switch (language) {
	case 0:
		return "/CONFIG/IME";
	case 0x1ULL:
		return "/CONFIG/IME/DANISH";
	case 0x2ULL:
		return "/CONFIG/IME/GERMAN";
	case 0x4ULL:
		return "/CONFIG/IME/US_ENGLISH";
	case 0x8ULL:
		return "/CONFIG/IME/SPANISH";
	case 0x10ULL:
		return "/CONFIG/IME/FRENCH";
	case 0x20ULL:
		return "/CONFIG/IME/ITALIAN";
	case 0x40ULL:
		return "/CONFIG/IME/DUTCH";
	case 0x80ULL:
		return "/CONFIG/IME/NORWEGIAN";
	case 0x100ULL:
		return "/CONFIG/IME/POLISH";
	case 0x200ULL:
		return "/CONFIG/IME/PORTUGUESE";
	case 0x400ULL:
		return "/CONFIG/IME/RUSSIAN";
	case 0x800ULL:
		return "/CONFIG/IME/FINNISH";
	case 0x1000ULL:
		return "/CONFIG/IME/SWEDISH";
	case 0x2000ULL:
		return "/CONFIG/IME/JAPANESE";
	case 0x4000ULL:
		return "/CONFIG/IME/KOREAN";
	case 0x8000ULL:
		return "/CONFIG/IME/SIMPLIFIED_CHINESE";
	case 0x20000ULL:
		return "/CONFIG/IME/BRAZILIAN_PORTUGUESE";
	case 0x40000ULL:
		return "/CONFIG/IME/UK_ENGLISH";
	case 0x80000ULL:
		return "/CONFIG/IME/TURKISH";
	case 0x10000000000ULL:
		return "/CONFIG/IME/TRADITIONAL_CHINESE_TW";
	case 0x20000000000ULL:
		return "/CONFIG/IME/TRADITIONAL_CHINESE_HK";
	default:
		return NULL;
	}
}

/* a language's lowest bit, -1 when it has none */
static inline int imeConfigGetLowestBit(SceUInt64 language)
{
	int bit = 0;

	while ((language & 1) == 0) {
		bit++;
		language >>= 1;
		if (bit >= 64)
			return -1;
	}
	return bit;
}

/* a language's table: 0 for none, else its lowest bit's number plus one */
static inline int imeConfigGetTableIndex(SceUInt64 language)
{
	return imeConfigGetLowestBit(language) + 1;
}

/* (0x811403FE) a setting read from the registry */
__attribute__((noinline)) void ImeConfig::Load(SceUInt64 language, SceUInt32 key)
{
	const char *path = GetLanguagePath(language);

	if (path == NULL)
		return;

	int value = 0;

	if (sceRegMgrGetKeyInt(path, GetKeyName(key), &value) >= 0)
		m_tables[imeConfigGetTableIndex(language)][key] = value;
}

/* (0x811404C2) */
ImeConfig::ImeConfig()
{
	m_reserved = 0;
	for (int i = 0; i < IME_CONFIG_TABLE_COUNT; i++)
		m_tables[i][IME_CONFIG_KEY_INPUT_TYPE] = 0;
}

/* (0x81140562) the settings read: each language's input type, and the common ones */
void ImeConfig::Create()
{
	ImeConfig *config = new ImeConfig();

	s_instance = config;
	for (int i = 0; i < IME_CONFIG_TABLE_COUNT; i++) {
		SceUInt64 language = i != 0 ? 1ULL << (i - 1) : 0;

		if (language != 0) {
			config->Load(language, IME_CONFIG_KEY_INPUT_TYPE);
		} else {
			config->Load(language, IME_CONFIG_KEY_AUTO_CAPITALIZATION);
			config->Load(language, IME_CONFIG_KEY_PREDICTIVE_TEXT);
			config->Load(language, IME_CONFIG_KEY_LAST_UNIT);
			config->Load(language, IME_CONFIG_KEY_UI_HINT);
			config->Load(language, IME_CONFIG_KEY_KB_TYPE);
			config->Load(language, IME_CONFIG_KEY_KB_ZH_T_INPUT_METHOD);
			config->Load(language, IME_CONFIG_KEY_KB_JA_INPUT_METHOD);
			config->Load(language, IME_CONFIG_KEY_KB_KEY_REPEAT_START);
			config->Load(language, IME_CONFIG_KEY_KB_KEY_REPEAT_INTERVAL);
		}
	}
}

/* (0x81140648) */
ImeConfig *ImeConfig::GetInstance()
{
	return s_instance;
}

/* (0x81140654) a setting changed, and written back to the registry by a job */
int ImeConfig::SetValue(SceUInt64 language, SceUInt32 key, int value)
{
	if (GetLanguagePath(language) == NULL)
		return IME_CONFIG_ERROR_LANGUAGE;
	if (GetKeyName(key) == NULL)
		return IME_CONFIG_ERROR_KEY;

	int index = imeConfigGetTableIndex(language);

	if (key == IME_CONFIG_KEY_INPUT_TYPE) {
		if ((value & ~IME_CONFIG_INPUT_TYPE_MASK) != 0)
			return IME_CONFIG_ERROR_INPUT_TYPE;
		m_tables[index][IME_CONFIG_KEY_INPUT_TYPE] =
			(m_tables[index][IME_CONFIG_KEY_INPUT_TYPE] & ~IME_CONFIG_INPUT_TYPE_MASK) | value;
		value = m_tables[index][IME_CONFIG_KEY_INPUT_TYPE];
	} else if (key == IME_CONFIG_KEY_PREDICTIVE) {
		if ((language & IME_CONFIG_LANGUAGE_PREDICTIVE) == 0)
			return IME_SERVICE_ERROR_INVALID_PARAM;
		m_tables[index][IME_CONFIG_KEY_INPUT_TYPE] =
			(m_tables[index][IME_CONFIG_KEY_INPUT_TYPE] & ~IME_CONFIG_PREDICTIVE_BIT) |
			(value != 0 ? IME_CONFIG_PREDICTIVE_BIT : 0);
		value = m_tables[index][IME_CONFIG_KEY_INPUT_TYPE];
	} else {
		m_tables[index][key] = value;
	}

	common::SharedPtr<job::JobItem> job(new ImeConfigJob(language, key, value));

	job::JobQueue::default_queue->Enqueue(job);
	return 0;
}

/* (0x811408D6) */
int ImeConfig::GetValue(SceUInt64 language, SceUInt32 key, int *value)
{
	int index = imeConfigGetTableIndex(language);
	int ret = 0;

	if (key == 0 || key > IME_CONFIG_KEY_UI_HINT) {
		ret = IME_SERVICE_ERROR_INVALID_PARAM;
		*value = 0;
	} else if (key == IME_CONFIG_KEY_INPUT_TYPE) {
		*value = m_tables[index].find(IME_CONFIG_KEY_INPUT_TYPE)->second & IME_CONFIG_INPUT_TYPE_MASK;
	} else if (key == IME_CONFIG_KEY_PREDICTIVE) {
		if ((language & IME_CONFIG_LANGUAGE_PREDICTIVE) == 0) {
			*value = 0;
			return IME_SERVICE_ERROR_INVALID_PARAM;
		}
		*value = m_tables[index].find(IME_CONFIG_KEY_INPUT_TYPE)->second < 0;
	} else {
		*value = m_tables[index].find(key)->second;
	}
	return ret;
}

/* (0x81140A3A) */
int ImeConfig::GetLastUnit()
{
	int unit;
	int ret = GetValue(0, IME_CONFIG_KEY_LAST_UNIT, &unit);

	return ret >= 0 ? unit : ret;
}

/* (0x81140A78) */
int ImeConfig::SetLastUnit(int unit)
{
	return SetValue(0, IME_CONFIG_KEY_LAST_UNIT, unit);
}

/* (0x81140A92) the job's work: a setting written, and the language its input type was set for
   kept as the last unit */
int ImeConfig::Save(SceUInt64 language, SceUInt32 key, int value)
{
	int ret = sceRegMgrSetKeyInt(GetLanguagePath(language), GetKeyName(key), value);

	if (ret >= 0) {
		m_tables[imeConfigGetTableIndex(language)][key] = value;
		if (key == IME_CONFIG_KEY_INPUT_TYPE)
			SetLastUnit(imeConfigGetTableIndex(language) << IME_CONFIG_LAST_UNIT_SHIFT);
	}
	return ret;
}

/* (0x81140BB4, 0x81140BFC) */
ImeConfig::~ImeConfig()
{
}
