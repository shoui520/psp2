/*
 * SceNpCommonPs4: the system's language and parameters the NP requests carry (npext_npm/np_system_param.cpp).
 */

#include "../np_common_ps4_internal.h"

extern "C" {

/* the language codes, by the system's language numbers */
const char *g_languageCodes[22] = {		/* 0x8102b01c */
	"ja", "en", "fr", "es", "de", "it", "nl", "pt", "ru", "ko", "zh-TW", "zh-CN", "fi", "sv", "da", "no",
	"pl", "pt-BR", "en-GB", "tr", "es-MX", "ar"
};

int SceNpCommonPs4_C08B6E1C(int language, char *code)
{
	if (language < 0 || language >= 22)
		return NP_COMMON_PS4_ERROR_JSON_INVALID_ARGUMENT;
	if (code == NULL)
		return NP_COMMON_PS4_ERROR_JSON_INVALID_ARGUMENT;
	sceClibStrlcpy(code, g_languageCodes[language], 6);
	return 0;
}

const char *SceNpCommonPs4_D1AB9B23(int language)
{
	if (language >= 0 && language < 22)
		return g_languageCodes[language];
	return NULL;
}

int SceNpCommonPs4_FA1249E8(void)
{
	int value;

	value = 100;
	if (sceRegMgrSystemParamGetInt(5266382, &value) >= 0)
		return value;
	return 100;
}

int SceNpCommonPs4_64C85FD5(void)
{
	int value;

	value = 0;
	return sceRegMgrSystemParamGetInt(5409805, &value) >= 0 && value != 0;
}

}
