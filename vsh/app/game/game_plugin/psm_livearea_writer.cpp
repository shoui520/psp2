/*
 * The game plugin's LiveArea of a PSM application: its template.xml written from its app.info,
 * with its title in every language, its author, its developer link and its intellectual property
 * notices (0x810520C4..0x81053840). The file's name and the functions' are ours (no record names
 * them); the strings are Sony's.
 */

#include <stddef.h>
#include <kernel/iofilemgr.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

/* the share of each step in a writer's progress */
#define GAME_PSM_LIVEAREA_WEIGHT_CHECK	0.05f
#define GAME_PSM_LIVEAREA_WEIGHT_WRITE	0.9f
#define GAME_PSM_LIVEAREA_WEIGHT_COMMIT	0.05f

/* the largest template.xml written */
#define GAME_PSM_TEMPLATE_MAX_SIZE	0x8000

/* the languages a PSM application's titles and notices are written in */
#define GAME_PSM_LANGUAGE_NUM		20

/* PLACEHOLDER name: the shell's own XML escaping (0x8138AA5C, the code of livespace_db's) */
void XmlAppendEscaped(string *out, const char *text);

/* "Intellectual Property Notices" in each language of s_languageCodes */
static const char *s_noticeTitles[GAME_PSM_LANGUAGE_NUM] = {
	"知的財産の表記",
	"Intellectual Property Notices",
	"Avis de propriété intellectuelle",
	"Avisos sobre propiedad intelectual",
	"Geistiges Eigentum",
	"Avvisi sulla proprietà intellettuale",
	"Intellectuele eigendomsmeldingen",
	"Avisos de propriedade intelectual",
	"Уведомления об интеллектуальной собственности",
	"지적 재산권 표기",
	"知識產權資訊",
	"知识产权信息",
	"Immateriaalioikeudelliset huomautukset",
	"Meddelande om upphovsrätt",
	"Meddelelser om immateriel ejendomsret",
	"Opphavsrettvarsel",
	"Informacje dotyczące własności intelektualnej",
	"Avisos de propriedade intelectual",
	"Intellectual Property Notices",
	"Fikri Mülkiyet Bildirimleri",
};

/* the LiveArea's language codes, in the order of the system's languages */
static const char *s_languageCodes[GAME_PSM_LANGUAGE_NUM] = {
	"ja", "en", "fr", "es", "de", "it", "nl", "pt", "ru", "ko",
	"ch", "zh", "fi", "sv", "da", "no", "pl", "pt-br", "en-gb", "tr",
};

GamePsmLiveAreaWriter::GamePsmLiveAreaWriter(GameCancelFunc cancel, void *arg,
					     const GameJobProgress *progress, string *detail,
					     float scale)
	: m_cancel(cancel), m_arg(arg), m_progress(*progress, scale), m_detail(detail),
	  m_appInfo(NULL)
{
	m_weights[0] = GAME_PSM_LIVEAREA_WEIGHT_CHECK;
	m_weights[1] = GAME_PSM_LIVEAREA_WEIGHT_WRITE;
	m_weights[2] = GAME_PSM_LIVEAREA_WEIGHT_COMMIT;
}

/* the commit's share of the progress added, when the registry has room for the application */
int GamePsmLiveAreaWriter::Run(GameAppRegistration *reg, const string *dir)
{
	bool hasRoom = false;
	int ret = gameLsdbHasRoom(&hasRoom);

	if (ret < 0)
		return ret;

	if (hasRoom)
		m_progress.Add(m_weights[2] * 100.0f);

	return 0;
}

/* text with XML's five characters as entities */
static string gameXmlEscape(const char *text)
{
	string escaped("");

	XmlAppendEscaped(&escaped, text);
	return escaped;
}

/*
 * The template at path written for the record info: frame1 the title in each language the record
 * has, frame2 the author, frame3 the developer link and frame4 the notices in each language
 * (0x810521BA).
 */
int GamePsmLiveAreaWriter::Write(const string &path, const LsdbAppInfoPtr *info)
{
	int ret = 0;
	LsdbAppInfo::iterator end = (*info)->end();
	unsigned int language = m_appInfo->m_defaultLanguage;
	string xml("<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n");
	unsigned int key;

	xml.append("<livearea style=\"psmobile\" format-ver=\"01.03\" content-rev=\"1\">\r\n");
	xml.append("<livearea-background>\r\n<image>LA_bg.png</image>\r\n"
	           "</livearea-background>\r\n");
	xml.append("<gate><startup-image>startup.dds</startup-image></gate>");

	xml.append("<frame id=\"frame1\">\r\n");

	const char **code = s_languageCodes;

	for (unsigned int i = 0; i < GAME_PSM_LANGUAGE_NUM; i++, code++) {
		ret = gameTitleKey(i, &key);
		if (ret < 0)
			return ret;

		LsdbAppInfo::iterator it = (*info)->find(key);

		if (it != end) {
			xml.append("<liveitem>\r\n");
			xml.append("<lang>" + string(*code) + "</lang>\r\n");
			xml.append("<text valign=\"bottom\" align=\"left\" text-align=\"left\" "
			           "text-valign=\"bottom\" line-space=\"3\" ellipsis=\"on\">\r\n"
			           "<str size=\"30\"  bold=\"on\" shadow=\"on\">" +
			           gameXmlEscape(LsdbValueText(it->second)) +
			           "</str>\r\n</text>\r\n");
			xml.append("</liveitem>\r\n");
		}
	}

	ret = gameTitleKey(language, &key);
	if (ret < 0)
		return ret;

	LsdbAppInfo::iterator it = (*info)->find(key);

	if (it == end)
		return GAME_ERROR_BAD_DATA;

	xml.append("<liveitem default=\"on\">\r\n");
	xml.append("<text valign=\"bottom\" align=\"left\" text-align=\"left\" "
	           "text-valign=\"bottom\" line-space=\"3\" ellipsis=\"on\">\r\n"
	           "<str size=\"30\"  bold=\"on\" shadow=\"on\">" +
	           gameXmlEscape(LsdbValueText(it->second)) + "</str>\r\n</text>\r\n");
	xml.append("</liveitem>\r\n");
	xml.append("</frame>\r\n");

	xml.append("<frame id=\"frame2\">\r\n");
	xml.append("<liveitem>\r\n");
	xml.append("<text valign=\"top\" align=\"left\" text-align=\"left\" text-valign=\"top\" "
	           "line-space=\"2\" ellipsis=\"on\">\r\n<str size=\"22\" shadow=\"on\">" +
	           gameXmlEscape(m_appInfo->m_author.c_str()) + "</str>\r\n</text>\r\n");
	xml.append("</liveitem>\r\n");
	xml.append("</frame>\r\n");

	xml.append("<frame id=\"frame3\">\r\n");
	xml.append("<liveitem>\r\n");
	xml.append("<target>" + gameXmlEscape(m_appInfo->m_website.c_str()) +
	           "</target>\r\n<image align=\"left\">LA_DeveloperLink.png</image>\r\n");
	xml.append("</liveitem>\r\n");
	xml.append("</frame>\r\n");

	xml.append("<frame id=\"frame4\">\r\n");

	it = (*info)->find(LsdbKeyHash("_org_path"));
	if (it == end)
		return GAME_ERROR_NO_VALUE;

	{
		string orgPath(LsdbValueText(it->second));
		const char **code = s_languageCodes;
		const char **title = s_noticeTitles;

		for (unsigned int i = 0; i < GAME_PSM_LANGUAGE_NUM; i++, code++, title++) {
			xml.append("<liveitem>\r\n");
			xml.append("<lang>" + string(*code) + "</lang>\r\n");
			xml.append("<target>psm_manual:" + orgPath +
			           "</target>\r\n<text align=\"center\" text-align=\"center\"  "
			           "word-wrap=\"off\" ellipsis=\"on\">\r\n"
			           "<str size=\"18\" color=\"#63FDFF\" shadow=\"on\">| " +
			           string(*title) + " |</str>\r\n</text>\r\n");
			xml.append("</liveitem>\r\n");
		}

		xml.append("<liveitem default=\"on\">\r\n");
		xml.append("<target>psm_manual:" + orgPath +
		           "</target>\r\n<text align=\"center\" text-align=\"center\"  "
		           "word-wrap=\"off\" ellipsis=\"on\">\r\n"
		           "<str size=\"18\" color=\"#63FDFF\" shadow=\"on\">| " +
		           string(s_noticeTitles[language]) + " |</str>\r\n</text>\r\n");
		xml.append("</liveitem>\r\n");
	}

	xml.append("</frame>\r\n");
	xml.append("\r\n</livearea>\r\n");

	if (xml.length() > GAME_PSM_TEMPLATE_MAX_SIZE)
		return GAME_ERROR_NO_MEMORY;

	common::SharedPtr<File> file =
		LocalFile::Open(path.c_str(), SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC, 6, &ret);

	if (ret < 0)
		return ret;

	ret = file->Write(xml.c_str(), xml.length());
	if (ret < 0) {
		file->Close();
		return ret;
	}

	file->Close();

	return 0;
}
