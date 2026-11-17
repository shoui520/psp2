/*
 * An application's parameters as SceLsdb exports them: each export reads one parameter of an
 * application's information (a text, "" when it is not one, or an integer, 0 when it is not
 * one), and the classifications made from them.
 */
#include "livespace_db_internal.h"
#include "lsdb_keys.h"
#include <plugin.h>

/* the game plugin's interface (Plugin::GetInterface(1) of "game_plugin"), as SceLsdb uses it */
struct LsdbGamePluginInterface {
	int (*getType)(const LsdbAppInfoPtr *app, int *type);
	int (*getSubType)(const LsdbAppInfoPtr *app, int *type);
	void *reserved[47];
	int (*getMode)(void);
};

/* ATTRIBUTE's low word, as the exports read its bits */
union LsdbAttribute {
	SceUInt32 value;
	struct {
		unsigned int : 13;
		unsigned int systemVersion : 2;
		unsigned int attributeFlag : 1;
	} bits;
};

/* an application's text parameter, NULL without the parameter */
static inline const char *LsdbAppInfoText(const LsdbAppInfoPtr *app, unsigned int key)
{
	LsdbAppInfo *info = app->get();
	LsdbAppInfo::iterator it = info->find(key);

	if (it == info->end())
		return NULL;
	return LsdbValueText(it->second);
}

/* an application's integer parameter, notFound without the parameter */
static inline SceInt64 LsdbAppInfoInteger(const LsdbAppInfoPtr *app, unsigned int key,
	SceInt64 notFound)
{
	LsdbAppInfo *info = app->get();
	LsdbAppInfo::iterator it = info->find(key);

	if (it == info->end())
		return notFound;
	return it->second.type == LSDB_VALUE_INTEGER ? it->second.integer : 0;
}

static inline LsdbGamePluginInterface *LsdbGamePlugin(void)
{
	paf::Plugin *plugin = paf::Plugin::Find("game_plugin");

	if (plugin == NULL)
		return NULL;
	return (LsdbGamePluginInterface *)plugin->GetInterface(1);
}

/* the TITLE_<nn> key of a language, TITLE without one */
static inline unsigned int LsdbTitleKey(int language)
{
	switch (language) {
	case 0:
		return 0xCF222A72u;	/* "TITLE_00" */
	case 1:
		return 0xD0222C05u;
	case 2:
		return 0xCD22274Cu;
	case 3:
		return 0xCE2228DFu;
	case 4:
		return 0xCB222426u;
	case 5:
		return 0xCC2225B9u;
	case 6:
		return 0xC9222100u;
	case 7:
		return 0xCA222293u;
	case 8:
		return 0xD722370Au;
	case 9:
		return 0xD822389Du;
	case 10:
		return 0x55253BFBu;	/* "TITLE_10" */
	case 11:
		return 0x54253A68u;
	case 12:
		return 0x57253F21u;
	case 13:
		return 0x56253D8Eu;
	case 14:
		return 0x59254247u;
	case 15:
		return 0x582540B4u;
	case 16:
		return 0x5B25456Du;
	case 17:
		return 0x5A2543DAu;
	case 18:
		return 0x4D252F63u;
	case 19:
		return 0x4C252DD0u;	/* "TITLE_19" */
	default:
		return LSDB_KEY_TITLE;
	}
}

extern "C" {

/* the STITLE_<nn> key of a language, STITLE without one */
unsigned int SceLsdb_63C29E70(int language)
{
	switch (language) {
	case 0:
		return 0x11F082E9u;	/* "STITLE_00" */
	case 1:
		return 0x10F08156u;
	case 2:
		return 0x0FF07FC3u;
	case 3:
		return 0x0EF07E30u;
	case 4:
		return 0x15F08935u;
	case 5:
		return 0x14F087A2u;
	case 6:
		return 0x13F0860Fu;
	case 7:
		return 0x12F0847Cu;
	case 8:
		return 0x19F08F81u;
	case 9:
		return 0x18F08DEEu;
	case 10:
		return 0x0BEE3AE0u;	/* "STITLE_10" */
	case 11:
		return 0x0CEE3C73u;
	case 12:
		return 0x0DEE3E06u;
	case 13:
		return 0x0EEE3F99u;
	case 14:
		return 0x0FEE412Cu;
	case 15:
		return 0x10EE42BFu;
	case 16:
		return 0x11EE4452u;
	case 17:
		return 0x12EE45E5u;
	case 18:
		return 0x13EE4778u;
	case 19:
		return 0x14EE490Bu;	/* "STITLE_19" */
	default:
		return LSDB_KEY_STITLE;
	}
}

/* the short title in the system's language, else STITLE */
const char *sceLsdbGetStitle(const LsdbAppInfoPtr *app)
{
	LsdbAppInfo *info = app->get();
	LsdbAppInfo::iterator it = info->find(SceLsdb_63C29E70(SceLsdb_68059E87()));

	if (it == app->get()->end())
		it = app->get()->find(LSDB_KEY_STITLE);
	if (it == app->get()->end())
		return NULL;
	return LsdbValueText(it->second);
}

/* the title in the system's language, else TITLE */
const char *sceLsdbGetTitle(const LsdbAppInfoPtr *app)
{
	LsdbAppInfo *info = app->get();
	LsdbAppInfo::iterator it = info->find(LsdbTitleKey(SceLsdb_68059E87()));

	if (it == app->get()->end())
		it = app->get()->find(LSDB_KEY_TITLE);
	if (it == app->get()->end())
		return NULL;
	return LsdbValueText(it->second);
}

const char *SceLsdb_C070E42E(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoText(app, LSDB_KEY_LIVEAREA_PATH);
}

const char *sceLsdbGetOriginalPath(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoText(app, LSDB_KEY_ORG_PATH);
}

SceInt64 sceLsdbGetAttribute(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoInteger(app, LSDB_KEY_ATTRIBUTE, 0);
}

SceInt64 sceLsdbGetMetaContentsPath(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoInteger(app, LSDB_KEY_LAREA_TYPE, 0);
}

SceInt64 SceLsdb_849E8DC6(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoInteger(app, LSDB_KEY_APP_MEMSIZE, 0);
}

const char *sceLsdbGetCategory(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoText(app, LSDB_KEY_CATEGORY);
}

/* whether the application is a "gda" (system) application */
int SceLsdb_41019D89(const LsdbAppInfoPtr *app)
{
	const char *category = sceLsdbGetCategory(app);

	if (category == NULL)
		return 0;
	return sce_paf_memcmp(category, "gda", 4) == 0;
}

SceInt64 SceLsdb_0B16523B(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoInteger(app, LSDB_KEY_PROMOTE_ATTR, 0);
}

const char *sceLsdbGetName(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoText(app, LSDB_KEY_TITLE_ID);
}

const char *SceLsdb_2D538EB9(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoText(app, LSDB_KEY_FG_TITLE_ID);
}

const char *SceLsdb_457F369E(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoText(app, LSDB_KEY_BG_TITLE_ID);
}

const char *sceLsdbGetDiscId(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoText(app, LSDB_KEY_TITLE_ID_SFO);
}

SceInt64 SceLsdb_9D27A87D(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoInteger(app, LSDB_KEY_APPTYPE, 0);
}

const char *sceLsdbGetSelfPath(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoText(app, LSDB_KEY_SELF_PATH);
}

SceInt64 SceLsdb_132FA3C6(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoInteger(app, LSDB_KEY_PHY_MEMSIZE, 0);
}

SceInt64 SceLsdb_45B38A03(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoInteger(app, LSDB_KEY_EBOOT_ATTR, 0);
}

const char *sceLsdbGetAppVer(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoText(app, LSDB_KEY_APP_VER);
}

const char *SceLsdb_8FDA59EA(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoText(app, LSDB_KEY_CONTENT_ID);
}

const char *SceLsdb_E26FC9E8(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoText(app, LSDB_KEY_NP_COMMUNICATION_ID);
}

SceInt64 SceLsdb_1518F869(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoInteger(app, LSDB_KEY_SYSTEM_VER, 0);
}

/* the application's type, as the game plugin tells it (0 without the plugin) */
int sceLsdbGetType(const LsdbAppInfoPtr *app)
{
	LsdbGamePluginInterface *game = LsdbGamePlugin();
	int type;

	if (game == NULL)
		return 0;
	type = 0;
	if (game->getType(app, &type) < 0)
		return 0;
	return type;
}

int SceLsdb_298134FE(const LsdbAppInfoPtr *app)
{
	LsdbGamePluginInterface *game = LsdbGamePlugin();
	int type;

	if (game == NULL)
		return 0;
	type = 0;
	if (game->getSubType(app, &type) < 0)
		return 0;
	return type;
}

const char *SceLsdb_039FD1AB(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoText(app, LSDB_KEY_DISC_ID);
}

const char *sceLsdbGetGameDataId(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoText(app, LSDB_KEY_GAMEDATA_ID);
}

SceInt64 sceLsdbGetParentalLevel(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoInteger(app, LSDB_KEY_PARENTAL_LEVEL, -1);
}

SceInt64 sceLsdbGetBootable(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoInteger(app, LSDB_KEY_BOOTABLE, -1);
}

/* ATTRIBUTE's bits 13 and 14 */
int sceLsdbGetSystemVersion(const LsdbAppInfoPtr *app)
{
	LsdbAppInfo *info = app->get();
	LsdbAppInfo::iterator it = info->find(LSDB_KEY_ATTRIBUTE);
	LsdbAttribute attribute;

	if (it == info->end())
		return 0;
	attribute.value = it->second.type == LSDB_VALUE_INTEGER ? it->second.integer : 0;
	return attribute.bits.systemVersion;
}

/* ATTRIBUTE's bit 15 */
bool SceLsdb_85B7A172(const LsdbAppInfoPtr *app)
{
	LsdbAppInfo *info = app->get();
	LsdbAppInfo::iterator it = info->find(LSDB_KEY_ATTRIBUTE);
	LsdbAttribute attribute;

	if (it == info->end())
		return false;
	attribute.value = it->second.type == LSDB_VALUE_INTEGER ? it->second.integer : 0;
	return attribute.bits.attributeFlag != 0;
}

const char *SceLsdb_D5747AD4(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoText(app, LSDB_KEY_BOOT_INSTALL_DIR);
}

/* whether the application is a game: a game category, or a system version of 1 or 2 */
int SceLsdb_DA0D7B0A(const LsdbAppInfoPtr *app)
{
	static const char *const categories[] = { "gd", "gp", "gdc", "gdd", "gpc", "gpd" };
	const char *category = sceLsdbGetCategory(app);
	int version;

	for (int i = 0; i < 6; i++) {
		if (sce_paf_strcmp(categories[i], category) == 0)
			return 1;
	}
	version = sceLsdbGetSystemVersion(app);
	if (version == 1 || version == 2)
		return 1;
	return 0;
}

/* whether the application is a game, or an "EG" one */
int SceLsdb_44CD0D07(const LsdbAppInfoPtr *app)
{
	static const char *const categories[] = { "gd", "gp", "gdc", "gdd", "gpc", "gpd" };
	static const char *const others[] = { "EG" };
	const char *category = sceLsdbGetCategory(app);

	for (int i = 0; i < 6; i++) {
		if (sce_paf_strcmp(categories[i], category) == 0)
			return 1;
	}
	for (int i = 0; i < 1; i++) {
		if (sce_paf_strcmp(others[i], category) == 0)
			return 1;
	}
	return 0;
}

/* whether the application is of the game plugin's type 1 without ATTRIBUTE's bit 15 */
bool SceLsdb_F4325AD0(const LsdbAppInfoPtr *app)
{
	bool attribute = SceLsdb_85B7A172(app);
	int type = sceLsdbGetType(app);

	return !attribute && type == 1;
}

const char *SceLsdb_B7F9E89F(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoText(app, LSDB_KEY_CONTENT_LOCATION);
}

const char *SceLsdb_1B9A8B45(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoText(app, LSDB_KEY_SPLASH_PATH);
}

/* ATTRIBUTE's bit 16 */
int SceLsdb_C80B07C3(const LsdbAppInfoPtr *app)
{
	LsdbAppInfo *info = app->get();
	LsdbAppInfo::iterator it = info->find(LSDB_KEY_ATTRIBUTE);
	SceInt64 attribute;

	if (it == info->end())
		return 0;
	attribute = it->second.type == LSDB_VALUE_INTEGER ? it->second.integer : 0;
	return (attribute >> 16) & 1;
}

const char *SceLsdb_F8874996(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoText(app, LSDB_KEY_LLTIME);
}

SceInt64 SceLsdb_8BC2DF72(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoInteger(app, LSDB_KEY_UP_STATUS, 0);
}

const char *SceLsdb_AE60C3A5(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoText(app, LSDB_KEY_SUPPORT_URI);
}

const char *SceLsdb_04D6A099(const LsdbAppInfoPtr *app)
{
	return LsdbAppInfoText(app, LSDB_KEY_INSTALL_DIR_SAVEDATA_ADD_1);
}

/* ATTRIBUTE's bits 34 to 36 */
int SceLsdb_43153457(const LsdbAppInfoPtr *app)
{
	return (sceLsdbGetAttribute(app) & 0x1C00000000LL) >> 34;
}

}
