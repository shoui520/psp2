/*
 * A theme's parameters: what theme.xml gives each system application's icon and each page's
 * background, and the names of theme.xml's classes and members as Sony's serializer writes
 * them (SceLsdb only constructs them; the shell and the settings share the code). The file name
 * and the names of the statics are ours.
 */
#include <std/string.h>
#include "livespace_db_internal.h"

/* the font colour (RGB), its shadow in bit 24 */
unsigned int BackgroundParam::GetFontColor() const
{
	unsigned int color = m_fontColor & 0xFFFFFF;

	if (m_fontShadow)
		return color | 0x1000000;
	return color;
}

/* the system applications, in the order of HomeProperty's icons */
static const char *const s_themeSystemTitleIds[] = {
	"NPXS10003", "NPXS10010", "NPXS10009", "NPXS10012", "NPXS10001",
	"NPXS10008", "NPXS10005", "NPXS10000", "NPXS10026", "NPXS10007",
	"NPXS10013", "NPXS10098", "NPXS10006", "NPXS10072", "NPXS10091",
	"NPXS10002", "NPXS10014", "NPXS10094", "NPXS10004", "NPXS10015",
};

const char *const *LsdbThemeGetSystemTitleIds(unsigned int *count)
{
	*count = sizeof(s_themeSystemTitleIds) / sizeof(s_themeSystemTitleIds[0]);
	return s_themeSystemTitleIds;
}

/* NPXS10013 and NPXS10098 (the PS4 Link applications) share an icon */
const IconParam *HomeProperty::GetIconParam(const char *titleId, int type) const
{
	if (titleId == NULL)
		return NULL;
	if (sce_paf_strcmp("NPXS10003", titleId) == 0)
		return &m_browser;
	if (sce_paf_strcmp("NPXS10010", titleId) == 0)
		return &m_video;
	if (sce_paf_strcmp("NPXS10009", titleId) == 0)
		return &m_music;
	if (sce_paf_strcmp("NPXS10012", titleId) == 0)
		return &m_ps3Link;
	if (sce_paf_strcmp("NPXS10001", titleId) == 0)
		return &m_party;
	if (sce_paf_strcmp("NPXS10008", titleId) == 0)
		return &m_trophy;
	if (sce_paf_strcmp("NPXS10005", titleId) == 0)
		return &m_map;
	if (sce_paf_strcmp("NPXS10000", titleId) == 0)
		return &m_near;
	if (sce_paf_strcmp("NPXS10026", titleId) == 0)
		return &m_hostCollabo;
	if (sce_paf_strcmp("NPXS10007", titleId) == 0)
		return &m_welcomePark;
	if (sce_paf_strcmp("NPXS10013", titleId) == 0)
		return &m_ps4Link;
	if (sce_paf_strcmp("NPXS10098", titleId) == 0)
		return &m_ps4Link;
	if (sce_paf_strcmp("NPXS10006", titleId) == 0)
		return &m_friend;
	if (sce_paf_strcmp("NPXS10072", titleId) == 0)
		return &m_email;
	if (sce_paf_strcmp("NPXS10091", titleId) == 0)
		return &m_calendar;
	if (sce_paf_strcmp("NPXS10002", titleId) == 0)
		return &m_store;
	if (sce_paf_strcmp("NPXS10014", titleId) == 0)
		return &m_message;
	if (sce_paf_strcmp("NPXS10094", titleId) == 0)
		return &m_parental;
	if (sce_paf_strcmp("NPXS10004", titleId) == 0)
		return &m_camera;
	if (sce_paf_strcmp("NPXS10015", titleId) == 0)
		return &m_settings;
	if (type == 6)
		return &m_power;
	return NULL;
}

static paf::string s_themeNameDefault("m_default");
static paf::string s_themeNameParam("m_param");

/* LsdbThemeText's members, a language each */
static paf::string s_themeNameLanguages[LSDB_THEME_LANGUAGES] = {
	"m_ja", "m_en", "m_fr", "m_es", "m_de", "m_it", "m_nl", "m_pt", "m_ru", "m_ko",
	"m_ch", "m_zh", "m_fi", "m_sv", "m_da", "m_no", "m_pl", "m_pt_br", "m_en_gb", "m_tr",
};

static paf::string s_themeNameStartScreenProperty("StartScreenProperty");
static paf::string s_themeNameFilePath("m_filePath");
static paf::string s_themeNameDateColor("m_dateColor");
static paf::string s_themeNameDateLayout("m_dateLayout");
static paf::string s_themeNameNotifyBgColor("m_notifyBgColor");
static paf::string s_themeNameNotifyBorderColor("m_notifyBorderColor");
static paf::string s_themeNameNotifyFontColor("m_notifyFontColor");
static paf::string s_themeNameInfomationBarProperty("InfomationBarProperty");
static paf::string s_themeNameBarColor("m_barColor");
static paf::string s_themeNameIndicatorColor("m_indicatorColor");
static paf::string s_themeNameNoticeFontColor("m_noticeFontColor");
static paf::string s_themeNameNoticeGlowColor("m_noticeGlowColor");
static paf::string s_themeNameNoNoticeFilePath("m_noNoticeFilePath");
static paf::string s_themeNameNewNoticeFilePath("m_newNoticeFilePath");
static paf::string s_themeNameBackgroundParam("BackgroundParam");
static paf::string s_themeNameImageFilePath("m_imageFilePath");
static paf::string s_themeNameThumbnailFilePath("m_thumbnailFilePath");
static paf::string s_themeNameWaveType("m_waveType");
static paf::string s_themeNameFontColor("m_fontColor");
static paf::string s_themeNameFontShadow("m_fontShadow");
static paf::string s_themeNameIconParam("IconParam");
static paf::string s_themeNameIconFilePath("m_iconFilePath");
static paf::string s_themeNamePagePos("m_pagePos");
static paf::string s_themeNameIconPos("m_iconPos");
static paf::string s_themeNameBrowser("m_browser");
static paf::string s_themeNameVideo("m_video");
static paf::string s_themeNameMusic("m_music");
static paf::string s_themeNamePs3Link("m_ps3Link");
static paf::string s_themeNameParty("m_party");
static paf::string s_themeNameTrophy("m_trophy");
static paf::string s_themeNameMap("m_map");
static paf::string s_themeNameNear("m_near");
static paf::string s_themeNameHostCollabo("m_hostCollabo");
static paf::string s_themeNameWelcomePark("m_welcomePark");
static paf::string s_themeNamePs4Link("m_ps4Link");
static paf::string s_themeNameFriend("m_friend");
static paf::string s_themeNameEmail("m_email");
static paf::string s_themeNameCalendar("m_calendar");
static paf::string s_themeNameStore("m_store");
static paf::string s_themeNameMessage("m_message");
static paf::string s_themeNameParental("m_parental");
static paf::string s_themeNameCamera("m_camera");
static paf::string s_themeNameSettings("m_settings");
static paf::string s_themeNamePower("m_power");
static paf::string s_themeNameHomeProperty("HomeProperty");
static paf::string s_themeNameBgParam("m_bgParam");
static paf::string s_themeNameBgmFilePath("m_bgmFilePath");
static paf::string s_themeNameBasePageFilePath("m_basePageFilePath");
static paf::string s_themeNameCurPageFilePath("m_curPageFilePath");
static paf::string s_themeNameInfomationProperty("InfomationProperty");
static paf::string s_themeNameContentVer("m_contentVer");
static paf::string s_themeNameTitle("m_title");
static paf::string s_themeNameProvider("m_provider");
static paf::string s_themeNamePackageImageFilePath("m_packageImageFilePath");
static paf::string s_themeNameStartPreviewFilePath("m_startPreviewFilePath");
static paf::string s_themeNameHomePreviewFilePath("m_homePreviewFilePath");
static paf::string s_themeNameTheme("theme");
static paf::string s_themeNameFormatVer("format-ver");
static paf::string s_themeNamePackage("package");
