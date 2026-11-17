/*
 * The network time the liveitems are shown by, and the filter of a liveitem by its lang,
 * exclude-lang, cntry, exclude-cntry, age-limit and uage-limit tags.
 */
#include <std/string.h>
#include <std/stdlib.h>
#include <rtc.h>
#include "livearea.h"

/* the time exports' object: nothing in it */
class LsdbTime {
};

static LsdbTime *s_lsdbTime;

void LsdbTimeInit(void)
{
	s_lsdbTime = new LsdbTime();
}

void LsdbTimeTerm(void)
{
	if (s_lsdbTime != NULL)
		delete s_lsdbTime;
	s_lsdbTime = NULL;
}

extern "C" {

#if defined(FW_TYPE_CEX)
/* paf's network time (a retail system has no debug network time) */
int SceLsdb_76C1997A(paf::rtc::Tick *tick)
{
	return paf::rtc::GetCurrentNetworkTick(tick);
}
#else
/* the debug network time when one is set, else paf's network time */
int SceLsdb_76C1997A(paf::rtc::Tick *tick)
{
	SceRtcTick now;
	int result = sceRtcGetCurrentDebugNetworkTick(&now);

	if (result != 0)
		result = paf::rtc::GetCurrentNetworkTick(tick);
	else
		*tick = now.tick;
	return result;
}
#endif

int SceLsdb_D8DF91EE(paf::rtc::Tick *tick)
{
	SceRtcTick now;
	int result = sceRtcGetCurrentAdNetworkTick(&now);

	if (result == 0)
		*tick = now.tick;
	return result;
}

}

/* an exclude-cntry: the system's country, for the system language when it names a lang */
int LiIsExcludedCountry(const paf::string *country, const char *systemCountry,
			XmlElement::AttrMap *attrs, const int *language)
{
	for (XmlElement::AttrMap::iterator it = attrs->begin(); it != attrs->end(); ++it) {
		if (it->first == g_liAttrCntryLang && language != NULL) {
			bool match = false;

			for (int i = 0; i <= SCE_SYSTEM_PARAM_LANG_TURKISH; i++) {
				if (g_liLanguages[i] == it->second) {
					match = *language == i;
					break;
				}
			}
			if (!match)
				return 0;
		}
	}
	return systemCountry != NULL && country->size() == 2 &&
	       country->c_str()[0] == systemCountry[0] && country->c_str()[1] == systemCountry[1];
}

/*
 * A cntry: any country, or the system's for the system language when it names a lang; its
 * age-limit and uage-limit then in limits
 */
int LiCheckCountry(const paf::string *country, const char *systemCountry,
		   XmlElement::AttrMap *attrs, const int *language, LiAgeLimits *limits)
{
	int ageLimit = 0;
	int uageLimit = 255;
	bool hasAgeLimit = false;
	bool hasUageLimit = false;

	for (XmlElement::AttrMap::iterator it = attrs->begin(); it != attrs->end(); ++it) {
		if (it->first == g_liAttrCntryLang) {
			if (language != NULL) {
				bool match = false;

				for (int i = 0; i <= SCE_SYSTEM_PARAM_LANG_TURKISH; i++) {
					if (g_liLanguages[i] == it->second) {
						match = *language == i;
						break;
					}
				}
				if (!match)
					return 0;
			}
		} else if (it->first == g_liAttrCntryAgeLimit) {
			char *end;
			int value = sce_paf_strtol(it->second.c_str(), &end, 10);

			if (*end == '\0') {
				ageLimit = value;
				hasAgeLimit = true;
			}
		} else if (it->first == g_liAttrCntryUageLimit) {
			char *end;
			int value = sce_paf_strtol(it->second.c_str(), &end, 10);

			if (*end == '\0') {
				uageLimit = value;
				hasUageLimit = true;
			}
		}
	}
	if (systemCountry == NULL)
		return 1;
	if (g_liAll == *country)
		return 1;
	if (country->size() != 2 || country->c_str()[0] != systemCountry[0] ||
	    country->c_str()[1] != systemCountry[1])
		return 0;
	if (hasAgeLimit) {
		limits->ageLimit = ageLimit;
		limits->hasAgeLimit = true;
	}
	if (hasUageLimit) {
		limits->uageLimit = uageLimit;
		limits->hasUageLimit = true;
	}
	return 1;
}

/*
 * A liveitem's filter tags against the system's language, country and age (NULL, NULL and
 * below 0: any); its age-limit and uage-limit in limits when limits is not NULL
 */
int LiFilterLiveitem(LiParser *parser, paf::list<XmlElement> *children, const int *language,
		     const char *country, int age, int *limits)
{
	bool hasLang = false;
	bool langMatch = false;
	bool hasCntry = false;
	bool cntryMatch = false;
	LiAgeLimits ages = { 0, 255, false, false };

	for (paf::list<XmlElement>::iterator child = children->begin(); child != children->end();
	     ++child) {
		if (g_liTagLang == child->name) {
			hasLang = true;
			if (langMatch)
				continue;
			if (language == NULL) {
				langMatch = true;
				continue;
			}
			for (int i = 0; i <= SCE_SYSTEM_PARAM_LANG_TURKISH; i++) {
				if (g_liLanguages[i] == child->text) {
					if (*language == i)
						langMatch = true;
					break;
				}
			}
		} else if (g_liTagExcludeLang == child->name) {
			if (language == NULL)
				continue;
			for (int i = 0; i <= SCE_SYSTEM_PARAM_LANG_TURKISH; i++) {
				if (g_liLanguages[i] == child->text) {
					if (*language == i)
						return 0;
					break;
				}
			}
		} else if (g_liTagCntry == child->name) {
			hasCntry = true;
			if (!cntryMatch && LiCheckCountry(&child->text, country, &child->attrs,
							  language, &ages))
				cntryMatch = true;
		} else if (g_liTagExcludeCntry == child->name) {
			if (LiIsExcludedCountry(&child->text, country, &child->attrs, language))
				return 0;
		} else if (g_liTagAgeLimit == child->name) {
			if (!ages.hasAgeLimit) {
				char *end;
				int value = sce_paf_strtol(child->text.c_str(), &end, 10);

				if (*end != '\0')
					LiParserError(parser, "tag[%s] invalid string:%s\n",
						      child->name.c_str(), child->text.c_str());
				else
					ages.ageLimit = value;
			}
		} else if (g_liTagUageLimit == child->name && !ages.hasUageLimit) {
			char *end;
			int value = sce_paf_strtol(child->text.c_str(), &end, 10);

			if (*end != '\0')
				LiParserError(parser, "tag[%s] invalid string:%s\n",
					      child->name.c_str(), child->text.c_str());
			else
				ages.uageLimit = value;
		}
	}
	if (limits != NULL) {
		limits[0] = ages.ageLimit;
		limits[1] = ages.uageLimit;
	}
	if (!hasLang)
		langMatch = true;
	if (!hasCntry)
		cntryMatch = true;
	if (!langMatch || !cntryMatch)
		return 0;
	if (age < 0)
		return 1;
	return ages.ageLimit <= age && ages.uageLimit >= age;
}

int LsdbLanguageFromCode(const paf::string *code)
{
	for (int i = 0; i <= SCE_SYSTEM_PARAM_LANG_TURKISH; i++) {
		if (g_liLanguages[i] == *code)
			return i;
	}
	return -1;
}

paf::string g_liAll("all");

/* the LiveArea's language codes, in the order of the system languages */
paf::string g_liLanguages[LI_LANGUAGE_COUNT] = {
	"ja", "en", "fr", "es", "de", "it", "nl", "pt", "ru", "ko",
	"ch", "zh", "fi", "sv", "da", "no", "pl", "pt-br", "en-gb", "tr",
};
