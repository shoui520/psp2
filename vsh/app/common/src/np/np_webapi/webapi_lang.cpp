/*
 * SceNpWebApi: the language of the system as the Accept-Language header of a request (Sony's
 * np_webapi/webapi_lang.cpp).
 */

#include "np_webapi_internal.h"

/* the system's language first, then English, then all the others, as "tag,tag,...": at most `size` characters */
int npWebApiGetLanguageSetting(char *buffer, SceSize size)
{
	int language;
	int result;
	int length;
	char *end;
	int i;
	char text[128];

	language = g_systemParam;
	if (language < 0) {
		SceNpCommonPs4_F50BD96A(sceClibPrintf("Assertion in %s(%s:%d): %s(%s)\n", "getLanguageSetting",
			"W:\\Build\\" NP_WEBAPI_BUILD "\\vsh\\app\\common\\src\\np\\np_webapi\\webapi_lang.cpp", 69, "ASSERT", "0"));
		language = g_systemParam;
	}
	result = 0;
	if (language >= 0 && language <= 19) {
		sceClibMemset(text, 0, 128);
		length = sceClibSnprintf(text, 127, "%s,", SceNpCommonPs4_D1AB9B23(language));
		end = text + length;
		if (language != 1) {
			length += sceClibSnprintf(end, 127 - length, "%s,", SceNpCommonPs4_D1AB9B23(1));
			end = text + length;
		}
		for (i = 0; i <= 19; i++) {
			if (i != language && i != 1) {
				length += sceClibSnprintf(end, 127 - length, "%s,", SceNpCommonPs4_D1AB9B23(i));
				end = text + length;
			}
		}
		if (buffer != NULL) {
			end[-1] = 0;
			sceClibStrncpy(buffer, text, size);
		}
	} else {
		result = -2141871101;
	}
	return result;
}
