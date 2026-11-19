/*
 * The LiveArea database: each title's template (tbl_livearea), its frames
 * (tbl_livearea_frame) and the image files they keep (tbl_livearea_file). Sony's daemon
 * (its mutex "SceLsdbLiDaemon"); the other names are ours.
 */
#include <string.h>
#include <std/string.h>
#include <std/stdio.h>
#include <std/ctype.h>
#include <sqlite3.h>
#include <kernel/iofilemgr.h>
#include <file/localfile.h>
#include <buffer/buffer.h>
#include <dir/dir.h>
#include <dbutil/statement.h>
#include <dbutil/function.h>
#include "livespace_db_internal.h"
#include "lsdb_statement.h"

using dbutil::RefPtr;
using dbutil::Statement;

/*
 * NPXS10998 and NPXS10999, the two titles the LiveArea keeps no row for, have their contents
 * found when they are shown. A retail system has neither.
 */
#if !defined(FW_TYPE_CEX)
#define LI_SYSTEM_TITLES	1
#else
#define LI_SYSTEM_TITLES	0
#endif

#if LI_SYSTEM_TITLES
/* the paths of the two titles, found when they are shown */
static paf::string s_npxs10999Path;
static paf::string s_npxs10998Path;
static paf::string s_npxs10998Dir;
#endif

/* whether a title ID is titleId (its first character tested first) */
static inline bool LiIsTitle(const paf::string *id, const char *titleId)
{
	return id->c_str()[0] == titleId[0] && sce_paf_strcmp(id->c_str(), titleId) == 0;
}

/* the daemon's SQL function, taking argCount arguments */
class LiDbFunction : public dbutil::Function {
public:
	LiDbFunction(const char *name, int argCount,
		void (*func)(sqlite3_context *context, int argc, sqlite3_value **argv))
	{
		m_name = name;
		m_textRep = SQLITE_UTF8;
		m_argCount = argCount;
		m_func = func;
	}

	virtual ~LiDbFunction()
	{
	}
};

/* the one daemon, made at its first use (its methods take it inline) */
inline LiDaemon *LiDaemon::Instance()
{
	static LiDaemon s_instance;

	return &s_instance;
}

__attribute__((noinline)) LiDaemon *LiDaemon::GetInstance()
{
	return Instance();
}

inline void LiDaemon::AddRmList(sqlite3_context *context, int argc, sqlite3_value **argv)
{
	if (argc == 1 && sqlite3_value_type(argv[0]) == SQLITE_TEXT) {
		const char *path = (const char *)sqlite3_value_text(argv[0]);
		int size = sqlite3_value_bytes(argv[0]);

		if (size > 0)
			Instance()->m_rmList.push_back(paf::string(path, size));
	}
}

/* creates the LiveArea's tables and the triggers that count and drop its files */
__attribute__((noinline)) int LiDaemon::Upgrade(ActivityDbConnection *connection, int version)
{
	int result = 0;

	if (version <= 0) {
		const char *sql[9];

		sql[0] = "CREATE TABLE IF NOT EXISTS tbl_livearea (titleId TEXT PRIMARY KEY,"
			"org_path TEXT NOT NULL,style TEXT NOT NULL,formatVer TEXT NOT NULL,"
			"contentRev INT,background_image TEXT,gate_startupImage TEXT,titleColor "
			"INT,modifiedDate TEXT,sfType0 INT,sfTarg0 TEXT,sfType1 INT,sfTarg1 TEXT,"
			"sfType2 INT,sfTarg2 TEXT,sfType3 INT,sfTarg3 TEXT,sfType4 INT,sfTarg4 "
			"TEXT,sfType5 INT,sfTarg5 TEXT,sfType6 INT,sfTarg6 TEXT,sfType7 INT,"
			"sfTarg7 TEXT)";
		sql[1] = "CREATE TABLE IF NOT EXISTS tbl_livearea_frame(titleId TEXT NOT NULL,"
			"frameId TEXT NOT NULL,type INT NOT NULL,frameMulti INT,frameAutoFlip INT,"
			"frameRev INT,item0_xml TEXT,item0_bg_path TEXT,item0_img_path TEXT,"
			"item1_xml TEXT,item1_bg_path TEXT,item1_img_path TEXT,item2_xml TEXT,"
			"item2_bg_path TEXT,item2_img_path TEXT,item3_xml TEXT,item3_bg_path TEXT,"
			"item3_img_path TEXT,item4_xml TEXT,item4_bg_path TEXT,item4_img_path TEXT,"
			"item5_xml TEXT,item5_bg_path TEXT,item5_img_path TEXT,item6_xml TEXT,"
			"item6_bg_path TEXT,item6_img_path TEXT,item7_xml TEXT,item7_bg_path TEXT,"
			"item7_img_path TEXT,item8_xml TEXT,item8_bg_path TEXT,item8_img_path TEXT,"
			"item9_xml TEXT,item9_bg_path TEXT,item9_img_path TEXT,userdata TEXT,"
			"PRIMARY KEY (titleId, frameId, type),FOREIGN KEY (titleId) REFERENCES "
			"tbl_livearea(titleId) ON DELETE CASCADE)";
		sql[2] = "CREATE TABLE IF NOT EXISTS tbl_livearea_file (rowid INTEGER PRIMARY KEY "
			"AUTOINCREMENT,refcnt INT  NOT NULL,ext TEXT NOT NULL,dir TEXT NOT NULL)";
		sql[3] = "CREATE TRIGGER IF NOT EXISTS tgr_livearea_file_update AFTER UPDATE OF "
			"refcnt ON tbl_livearea_file WHEN NEW.refcnt = 0 BEGIN DELETE FROM "
			"tbl_livearea_file WHERE rowid=NEW.rowid; END;";
		sql[4] = "CREATE TRIGGER IF NOT EXISTS tgr_livearea_file_delete BEFORE DELETE ON "
			"tbl_livearea_file BEGIN SELECT add_rm_list(dir||'/'||' '||rowid||ext) "
			"FROM tbl_livearea_file WHERE rowid=OLD.rowid;END;";
		sql[5] = "CREATE TRIGGER IF NOT EXISTS tgr_livearea_upd_bgimg AFTER UPDATE OF "
			"background_image ON tbl_livearea WHEN OLD.background_image LIKE ' %' "
			"BEGIN UPDATE tbl_livearea_file SET refcnt=refcnt-1 WHERE "
			"rowid=CAST(OLD.background_image AS INTEGER);END;";
		sql[6] = "CREATE TRIGGER IF NOT EXISTS tgr_livearea_upd_gtimg AFTER UPDATE OF "
			"gate_startupImage ON tbl_livearea WHEN OLD.gate_startupImage LIKE ' %' "
			"BEGIN UPDATE tbl_livearea_file SET refcnt=refcnt-1 WHERE "
			"rowid=CAST(OLD.gate_startupImage AS INTEGER);END;";
		sql[7] = "CREATE TRIGGER IF NOT EXISTS tgr_livearea_del_img AFTER DELETE ON "
			"tbl_livearea WHEN OLD.gate_startupImage LIKE ' %' OR OLD.background_image "
			"LIKE ' %' BEGIN UPDATE tbl_livearea_file SET refcnt=refcnt-1 WHERE "
			"rowid=CAST(OLD.gate_startupImage AS INTEGER);UPDATE tbl_livearea_file SET "
			"refcnt=refcnt-1 WHERE rowid=CAST(OLD.background_image AS INTEGER);END;";
		sql[8] = "CREATE TRIGGER IF NOT EXISTS tgr_livearea_frame_del_item BEFORE DELETE ON "
			"tbl_livearea_frame WHEN OLD.item0_bg_path LIKE ' %' OR OLD.item0_img_path "
			"LIKE ' %' OR OLD.item1_bg_path LIKE ' %' OR OLD.item1_img_path LIKE ' %' "
			"OR OLD.item2_bg_path LIKE ' %' OR OLD.item2_img_path LIKE ' %' OR "
			"OLD.item3_bg_path LIKE ' %' OR OLD.item3_img_path LIKE ' %' OR "
			"OLD.item4_bg_path LIKE ' %' OR OLD.item4_img_path LIKE ' %' OR "
			"OLD.item5_bg_path LIKE ' %' OR OLD.item5_img_path LIKE ' %' OR "
			"OLD.item6_bg_path LIKE ' %' OR OLD.item6_img_path LIKE ' %' OR "
			"OLD.item7_bg_path LIKE ' %' OR OLD.item7_img_path LIKE ' %' OR "
			"OLD.item8_bg_path LIKE ' %' OR OLD.item8_img_path LIKE ' %' OR "
			"OLD.item9_bg_path LIKE ' %' OR OLD.item9_img_path LIKE ' %' BEGIN UPDATE "
			"tbl_livearea_file SET refcnt=refcnt-1 WHERE rowid=CAST(OLD.item0_bg_path "
			"AS INTEGER);UPDATE tbl_livearea_file SET refcnt=refcnt-1 WHERE "
			"rowid=CAST(OLD.item0_img_path AS INTEGER);UPDATE tbl_livearea_file SET "
			"refcnt=refcnt-1 WHERE rowid=CAST(OLD.item1_bg_path AS INTEGER);UPDATE "
			"tbl_livearea_file SET refcnt=refcnt-1 WHERE rowid=CAST(OLD.item1_img_path "
			"AS INTEGER);UPDATE tbl_livearea_file SET refcnt=refcnt-1 WHERE "
			"rowid=CAST(OLD.item2_bg_path AS INTEGER);UPDATE tbl_livearea_file SET "
			"refcnt=refcnt-1 WHERE rowid=CAST(OLD.item2_img_path AS INTEGER);UPDATE "
			"tbl_livearea_file SET refcnt=refcnt-1 WHERE rowid=CAST(OLD.item3_bg_path "
			"AS INTEGER);UPDATE tbl_livearea_file SET refcnt=refcnt-1 WHERE "
			"rowid=CAST(OLD.item3_img_path AS INTEGER);UPDATE tbl_livearea_file SET "
			"refcnt=refcnt-1 WHERE rowid=CAST(OLD.item4_bg_path AS INTEGER);UPDATE "
			"tbl_livearea_file SET refcnt=refcnt-1 WHERE rowid=CAST(OLD.item4_img_path "
			"AS INTEGER);UPDATE tbl_livearea_file SET refcnt=refcnt-1 WHERE "
			"rowid=CAST(OLD.item5_bg_path AS INTEGER);UPDATE tbl_livearea_file SET "
			"refcnt=refcnt-1 WHERE rowid=CAST(OLD.item5_img_path AS INTEGER);UPDATE "
			"tbl_livearea_file SET refcnt=refcnt-1 WHERE rowid=CAST(OLD.item6_bg_path "
			"AS INTEGER);UPDATE tbl_livearea_file SET refcnt=refcnt-1 WHERE "
			"rowid=CAST(OLD.item6_img_path AS INTEGER);UPDATE tbl_livearea_file SET "
			"refcnt=refcnt-1 WHERE rowid=CAST(OLD.item7_bg_path AS INTEGER);UPDATE "
			"tbl_livearea_file SET refcnt=refcnt-1 WHERE rowid=CAST(OLD.item7_img_path "
			"AS INTEGER);UPDATE tbl_livearea_file SET refcnt=refcnt-1 WHERE "
			"rowid=CAST(OLD.item8_bg_path AS INTEGER);UPDATE tbl_livearea_file SET "
			"refcnt=refcnt-1 WHERE rowid=CAST(OLD.item8_img_path AS INTEGER);UPDATE "
			"tbl_livearea_file SET refcnt=refcnt-1 WHERE rowid=CAST(OLD.item9_bg_path "
			"AS INTEGER);UPDATE tbl_livearea_file SET refcnt=refcnt-1 WHERE "
			"rowid=CAST(OLD.item9_img_path AS INTEGER);END;";
		result = SceActivityDb_D3277649(connection, sql, 9);
		if (result != 0)
			return result;
	}
	if (version < 4) {
		const char *sql[3];

		sql[0] = "ALTER TABLE tbl_livearea ADD COLUMN pccUpdDate INT";
		sql[1] = "ALTER TABLE tbl_livearea ADD COLUMN pccResStatus INT";
		sql[2] = "ALTER TABLE tbl_livearea ADD COLUMN pccSignOff INT";
		result = SceActivityDb_D3277649(connection, sql, 3);
	}
	return result;
}

/* takes the database: add_rm_list goes on it, and the titles' paths are found again */
__attribute__((noinline)) int LiDaemon::Open(ActivityDbAccess *access)
{
	paf::thread::RMutexLock lock(&m_mutex);

	m_access = access;

	LiDbFunction addRmList("add_rm_list", 1, AddRmList);

	access->RegisterFunction(&addRmList);
#if LI_SYSTEM_TITLES
	s_npxs10999Path.clear();
	s_npxs10998Path.clear();
	s_npxs10998Dir.clear();
#endif
	return 0;
}

__attribute__((noinline)) int LiDaemon::Close()
{
	paf::thread::RMutexLock lock(&m_mutex);

	if (m_access != NULL) {
		LiDbFunction addRmList("add_rm_list", 1, AddRmList);

		m_access->UnregisterFunction(&addRmList);
	}
	m_access = NULL;
	return 0;
}

/*
 * The template.xml in dir read for the system's language and country of source (a system
 * application's when system): 0, LI_ERROR_*, or LSDB_ERROR_*
 */
static __attribute__((noinline)) int LiReadTemplate(paf::common::SharedPtr<LiTemplate> *out,
	bool system, const char *dir, int source, int language)
{
	int result;
	paf::string path = paf::string(dir) + "/template.xml";

	if (!paf::LocalFile::Exists(path.c_str()))
		return LI_ERROR_NO_TEMPLATE;

	LiParser parser(path.c_str(), system, false);
	paf::string country = SceLsdb_88FDC55C(source);
	int lang = SceLsdb_60B535FD(source, language);

	parser.SetFilter(&lang, country.c_str(), -1);
	result = parser.Parse();
	switch (result) {
	case 0:
		*out = parser.m_template;
		if (out->get() == NULL)
			result = LSDB_ERROR_NO_MEMORY;
		break;
	case -6:
		result = LI_ERROR_TEMPLATE_TOO_LARGE;
		break;
	case -5:
	case -3:
		result = LI_ERROR_INVALID_TEMPLATE;
		break;
	case -4:
		result = LI_ERROR_NO_TEMPLATE;
		break;
	case -2:
		result = LSDB_ERROR_NO_MEMORY;
		break;
	default:
		result = LSDB_ERROR_DATABASE;
		break;
	}
	return result;
}

#if LI_SYSTEM_TITLES
/* whether dir holds a template that can be read */
static __attribute__((noinline)) bool LiHasTemplate(const char *dir, int source)
{
	paf::common::SharedPtr<LiTemplate> tmpl;

	return LiReadTemplate(&tmpl, true, dir, source, -1) == 0;
}

/* the LiveArea contents of titleId, run as pid: the default contents for a title without */
static __attribute__((noinline)) paf::string LiContentsPath(const paf::string *titleId,
	SceUID pid)
{
	paf::string path;

	if (LiIsTitle(titleId, "NPXS10998")) {
		if (s_npxs10998Path.size() != 0)
			path = s_npxs10998Path;
		else if (LiHasTemplate(s_npxs10998Dir.c_str(), 0))
			path = s_npxs10998Dir;
	} else if (LiIsTitle(titleId, "NPXS10999")) {
		char app0[292];

		if (s_npxs10999Path.size() != 0) {
			path = s_npxs10999Path;
		} else if (sceAppMgrGetRawPathOfApp0ByPidForShell(pid, app0) == 0) {
			path = paf::string(app0) + paf::string("/sce_sys/livearea/contents");
			if (!LiHasTemplate(path.c_str(), 0))
				path.clear();
		}
	}
	if (path.size() == 0)
		path = "vs0:data/internal/livearea/default/sce_sys/livearea/contents";
	return path;
}

/* the parameters the LiveArea needs of a title it keeps no row for */
static __attribute__((noinline)) int LiSetSystemAppInfo(const paf::string *titleId,
	LsdbAppInfoPtr *info, SceUID pid)
{
	(**info)[LSDB_KEY_TITLE_ID].SetText(*titleId);
	(**info)[LSDB_KEY_LIVEAREA_PATH].SetText(LiContentsPath(titleId, pid));
	(**info)[LSDB_KEY_APPTYPE].SetInteger(1);
	(**info)[LSDB_KEY_CATEGORY].SetText("gd");
	(**info)[LSDB_KEY_ATTRIBUTE].SetInteger(0);
	return 0;
}

#endif

/* a title's parameters (a retail build inlines it) */
#if LI_SYSTEM_TITLES
static __attribute__((noinline)) int LiGetAppInfo(const paf::string *titleId,
	LsdbAppInfoPtr *info, SceUID pid)
#else
static inline int LiGetAppInfo(const paf::string *titleId, LsdbAppInfoPtr *info, SceUID pid)
#endif
{
#if LI_SYSTEM_TITLES
	if (LiIsTitle(titleId, "NPXS10998") ||
	    LiIsTitle(titleId, "NPXS10999"))
		return LiSetSystemAppInfo(titleId, info, pid);
#endif

	int result;
	AppInfoQClient client;

	SceLsdb_A6D8772C(&client);
	if (info->get() == NULL) {
		SceLsdb_E2101B03(&client);
		return LSDB_ERROR_NO_MEMORY;
	}
	result = SceLsdb_E90B81CD(&client);
	if (result == 0)
		result = SceLsdb_D239031A(&client, titleId, info);
	SceLsdb_E2101B03(&client);
	return result;
}

/* a number's text */
static inline paf::string LiNumber(SceInt64 number)
{
	char text[64];

	sce_paf_snprintf(text, sizeof(text), "%lld", number);
	return paf::string(text);
}

/*
 * The UPDATE of a title's tbl_livearea row: its style, format and revision unless kept, and
 * what the template has (the pcc columns of pccFlags)
 */
static __attribute__((noinline)) paf::string LiUpdateSql(
	paf::common::SharedPtr<LiTemplate> *tmpl, bool keepStyle, int pccFlags)
{
	paf::string sql("UPDATE tbl_livearea SET ");

	if (!keepStyle)
		sql += "style=?3,formatVer=?4,contentRev=?5,";

	LiBackground *background = (LiBackground *)(*tmpl)->GetPart(LI_PART_BACKGROUND);

	if (background != NULL && background->m_image.size() != 0)
		sql += "background_image=?6,";

	LiGate *gate = (LiGate *)(*tmpl)->GetPart(LI_PART_GATE);

	if (gate != NULL && gate->m_startupImage.size() != 0)
		sql += "gate_startupImage=?7,";
	if ((*tmpl)->GetPart(LI_PART_TITLE) != NULL)
		sql += "titleColor=?8,";
	if ((*tmpl)->m_modifiedDate != 0)
		sql += "modifiedDate=?9,";
	if ((*tmpl)->GetPart(LI_PART_FUNCTION_ZONE) != NULL) {
		for (unsigned int i = 0; i < 8; i++) {
			sql.append(paf::string("sfType") + LiNumber(i) + "=?" + LiNumber(i * 2 + 10) +
				",");
			sql.append(paf::string("sfTarg") + LiNumber(i) + "=?" + LiNumber(i * 2 + 11) +
				",");
		}
	}
	if (pccFlags & LI_PCC_UPD_DATE)
		sql += "pccUpdDate=?30,";
	if (pccFlags & LI_PCC_RES_STATUS)
		sql += "pccResStatus=?31,";
	if (pccFlags & LI_PCC_SIGN_OFF)
		sql += "pccSignOff=?32,";
	sql[sql.size() - 1] = ' ';
	sql += "WHERE titleId=?1";
	return sql;
}

/* binds a tbl_livearea row's parameters (the title and its org path when orgPath is set) */
static __attribute__((noinline)) int LiBindTemplate(RefPtr<Statement> *statement,
	const paf::string *titleId, paf::common::SharedPtr<LiTemplate> *tmpl, const char *orgPath,
	bool keepStyle, const LiPcc *pcc, int pccFlags)
{
	int result = AppDbBindString(statement, 1, titleId);

	if (result != 0)
		return result;
	if (orgPath != NULL) {
		result = AppDbBindText(statement, 2, orgPath, -1);
		if (result != 0)
			return result;
	}
	if (!keepStyle) {
		result = AppDbBindString(statement, 3, &(*tmpl)->m_style);
		if (result != 0)
			return result;
		result = AppDbBindString(statement, 4, &(*tmpl)->m_formatVer);
		if (result != 0)
			return result;
		result = AppDbBindInt(statement, 5, (*tmpl)->m_contentRev);
		if (result != 0)
			return result;
	}

	LiBackground *background = (LiBackground *)(*tmpl)->GetPart(LI_PART_BACKGROUND);

	if (background != NULL && background->m_image.size() != 0) {
		result = AppDbBindString(statement, 6, &background->m_image);
		if (result != 0)
			return result;
	}

	LiGate *gate = (LiGate *)(*tmpl)->GetPart(LI_PART_GATE);

	if (gate != NULL && gate->m_startupImage.size() != 0) {
		result = AppDbBindString(statement, 7, &gate->m_startupImage);
		if (result != 0)
			return result;
	}

	LiTitle *title = (LiTitle *)(*tmpl)->GetPart(LI_PART_TITLE);

	if (title != NULL) {
		result = AppDbBindInt(statement, 8,
			((unsigned int)(title->m_color[0] * 255.0f) << 24) +
			((unsigned int)(title->m_color[1] * 255.0f) << 16) +
			((unsigned int)(title->m_color[2] * 255.0f) << 8) +
			(unsigned int)(title->m_color[3] * 255.0f));
		if (result != 0)
			return result;
	}
	if ((*tmpl)->m_modifiedDate != 0) {
		char date[30] = "";

		SceActivityDb_9A8944FC(&(*tmpl)->m_modifiedDate, date, sizeof(date));
		result = AppDbBindText(statement, 9, date, -1);
		if (result != 0)
			return result;
	}

	LiFunctionZone *zone = (LiFunctionZone *)(*tmpl)->GetPart(LI_PART_FUNCTION_ZONE);

	if (zone != NULL) {
		for (unsigned int i = 0; i < zone->m_functions.size() && i < 8; i++) {
			result = AppDbBindInt(statement, i * 2 + 10, zone->m_functions[i].func);
			if (result != 0)
				return result;
			result = AppDbBindString(statement, i * 2 + 11,
				&zone->m_functions.at(i).target);
			if (result != 0)
				return result;
		}
	}
	if (pccFlags & LI_PCC_UPD_DATE) {
		result = AppDbBindInt(statement, 30, pcc->updDate);
		if (result != 0)
			return result;
	}
	if (pccFlags & LI_PCC_RES_STATUS) {
		result = AppDbBindInt(statement, 31, pcc->resStatus - 1);
		if (result != 0)
			return result;
	}
	if (pccFlags & LI_PCC_SIGN_OFF)
		result = AppDbBindInt(statement, 32, pcc->signOff);
	return result;
}

/* a title's tbl_livearea row: inserted with its org path, else updated */
static __attribute__((noinline)) int LiWriteTemplate(ActivityDbConnection *connection,
	const paf::string *titleId, paf::common::SharedPtr<LiTemplate> *tmpl, const char *orgPath,
	bool keepStyle, const LiPcc *pcc, int pccFlags)
{
	RefPtr<Statement> statement;
	paf::string sql = orgPath != NULL ?
		paf::string("INSERT INTO tbl_livearea VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,"
			"?,?,?,?,?,?30,?31,?32)") :
		LiUpdateSql(tmpl, keepStyle, pccFlags);

	int result = connection->Prepare(sql, &statement);

	if (result == 0) {
		result = LiBindTemplate(&statement, titleId, tmpl, orgPath, keepStyle, pcc,
			pccFlags);
		if (result == 0) {
			result = statement->Step(1);
			if (result == 1)
				result = 0;
			else if (result == (int)LSDB_DBUTIL_ERROR_CONSTRAINT)
				result = LSDB_ERROR_EXISTS;
		}
	}
	return result;
}

/*
 * Binds a frame's tbl_livearea_frame row: its type (0 for a system application's), its
 * liveitems' XML and image files, and its userdata
 */
static __attribute__((noinline)) int LiBindFrame(RefPtr<Statement> *statement,
	const paf::string *titleId, bool system, LiFrame *frame, bool systemApp)
{
	paf::string xml;
	int result = AppDbBindString(statement, 1, titleId);

	if (result != 0)
		goto end;
	result = AppDbBindString(statement, 2, &frame->m_id);
	if (result != 0)
		goto end;
	result = AppDbBindInt(statement, 3, !system);
	if (result != 0)
		goto end;
	result = AppDbBindInt(statement, 4, frame->m_multi);
	if (result != 0)
		goto end;
	result = AppDbBindInt(statement, 5, (unsigned int)frame->m_autoflip);
	if (result != 0)
		goto end;
	result = AppDbBindInt(statement, 6, frame->m_rev);
	if (result != 0)
		goto end;

	{
		int column = 7;
		int count = 0;

		for (paf::vector<LiLiveItem *>::iterator it = frame->m_items.begin();
		     it != frame->m_items.end() && count < LI_ROW_ITEM_COUNT;
		     ++it, column += 3, count++) {
			LiLiveItem *item = *it;

			xml.resize(0);
			if (LiWriteLiveitem(&xml, item, systemApp) == 0) {
				result = LSDB_ERROR_NO_MEMORY;
				goto end;
			}
			result = AppDbBindString(statement, column, &xml);
			if (result != 0)
				goto end;

			LiItemBackground *background =
				(LiItemBackground *)item->GetElement(LI_ITEM_BACKGROUND);

			if (background != NULL) {
				result = AppDbBindString(statement, column + 1,
							 &background->m_image);
				if (result != 0)
					goto end;
			}

			LiImage *image = (LiImage *)item->GetElement(LI_ITEM_IMAGE);

			if (image != NULL) {
				result = AppDbBindString(statement, column + 2, &image->m_image);
				if (result != 0)
					goto end;
			}
		}
	}
	result = AppDbBindString(statement, 37, &frame->m_userdata);
end:
	return result;
}

/* a title's template row (unless the template has no style) and its frames' rows */
static __attribute__((noinline)) int LiWriteFrames(ActivityDbConnection *connection,
	const paf::string *titleId, paf::common::SharedPtr<LiTemplate> *tmpl, const char *orgPath,
	bool system, bool systemApp)
{
	int result;

	if ((*tmpl)->m_style.size() != 0 && (*tmpl)->m_formatVer.size() != 0) {
		result = LiWriteTemplate(connection, titleId, tmpl, orgPath, false, NULL, 0);
		if (result != 0)
			return result;
	}

	RefPtr<Statement> statement;

	result = connection->Prepare(paf::string("REPLACE INTO tbl_livearea_frame VALUES(?,?,?,?,?,"
		"?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"), &statement);
	if (result != 0)
		goto end;
	for (paf::list<LiPart *>::iterator it = (*tmpl)->m_parts.begin();
	     it != (*tmpl)->m_parts.end(); ++it) {
		if ((*it)->m_type != LI_PART_FRAME)
			continue;

		int error = statement->ClearBindings();

		if (error == 0)
			error = statement->Reset();
		if (error == 0)
			error = LiBindFrame(&statement, titleId, system, (LiFrame *)*it, systemApp);
		if (error == 0) {
			error = statement->Step(1);
			if (error == 1)
				error = 0;
		}
		result = error;
		if (result != 0)
			break;
	}
end:
	return result;
}

/* a title's LiveArea from the template.xml in dir, read for language */
__attribute__((noinline)) int LiDaemon::Register(const paf::string *titleId, const char *dir,
	int language)
{
	LsdbAppInfoPtr info(new LsdbAppInfo);
	paf::common::SharedPtr<LiTemplate> tmpl;

	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;

	int result = LiGetAppInfo(titleId, &info, 0);

	if (result != 0)
		return result;

	bool system = SceLsdb_41019D89(&info);

	result = LiReadTemplate(&tmpl, system, dir, sceLsdbGetSystemVersion(&info), language);
	if (result != 0)
		return result;

	ActivityDbConnection *connection = m_access->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;
	result = LiWriteFrames(connection, titleId, &tmpl, dir, false, system);
	m_access->EndTransaction(result);
	return result;
}

/* deletes a title's row (its frames and files go with it); LSDB_ERROR_NOT_FOUND without one */
__attribute__((noinline)) int LiDaemon::DeleteTitle(ActivityDbConnection *connection,
	const paf::string *titleId)
{
	int result;

	{
		RefPtr<Statement> statement;

		result = connection->Prepare(paf::string("DELETE FROM tbl_livearea WHERE titleId=?"),
			&statement);
		if (result != 0)
			return result;
		result = AppDbBindString(&statement, 1, titleId);
		if (result != 0)
			return result;
		result = statement->Step(1);
		if (result != 1)
			return result;
		if (connection->GetChanges() == 0)
			return LSDB_ERROR_NOT_FOUND;
	}
	m_access->AddEndCallback(RemoveFiles);
	return 0;
}

/* deletes a title's LiveArea, and its activities */
__attribute__((noinline)) int LiDaemon::Delete(const paf::string *titleId)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;

	ActivityDbConnection *connection = m_access->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result = DeleteTitle(connection, titleId);

	if (result == 0 || result == (int)LSDB_ERROR_NOT_FOUND) {
		paf::vector<paf::string> titleIds(1, *titleId);

		SceActivityDb_FBB82B37()->DeleteTitles(0, &titleIds, NULL);
	}
	m_access->EndTransaction(result);
	return result;
}

/* a title in tbl_livearea: its title ID and the path its template was read from */
struct LiTitlePath {
	paf::string titleId;
	paf::string orgPath;
};

PAF_VECTOR_NOINLINE_PUSH_BACK(LiTitlePath, paf::allocator<LiTitlePath>)
PAF_VECTOR_NOINLINE_PUSH_BACK(LiFunction, paf::allocator<LiFunction>)

/* a column's text, "" for another type */
static inline paf::string LiValueText(const dbutil::Value *value)
{
	paf::string text;

	if (value->type == SQLITE_TEXT)
		text.assign(value->data.text, value->size);
	return text;
}

/* the titles in tbl_livearea, or titleId's (every title for an empty one) */
static __attribute__((noinline)) int LiSelectTitles(ActivityDbConnection *connection,
	const paf::string *titleId, paf::vector<LiTitlePath> *titles)
{
	RefPtr<Statement> statement;
	const char *text = "SELECT titleId, org_path FROM tbl_livearea WHERE titleId=?";

	if (titleId->size() == 0)
		text = "SELECT titleId, org_path FROM tbl_livearea";

	paf::string sql(text);
	int result = connection->Prepare(sql, &statement);

	if (result == 0 && (titleId->size() == 0 ||
			    (result = AppDbBindString(&statement, 1, titleId)) == 0)) {
		for (;;) {
			dbutil::Value id;
			dbutil::Value path;

			result = statement->Step(1);
			if (result != 0) {
				if (result == 1)
					result = 0;
				break;
			}
			result = statement->GetValue(0, &id);
			if (result != 0)
				break;
			result = statement->GetValue(1, &path);
			if (result != 0)
				break;
			titles->push_back(LiTitlePath());
			(*titles)[titles->size() - 1].titleId = LiValueText(&id);
			(*titles)[titles->size() - 1].orgPath = LiValueText(&path);
		}
	}
	return result;
}

/* the path a title's LiveArea is read from: kept for the two titles without a row */
static __attribute__((noinline)) int LiSetContentsPath(const paf::string *titleId,
	const paf::string *path)
{
#if LI_SYSTEM_TITLES
	if (LiIsTitle(titleId, "NPXS10998")) {
		s_npxs10998Path = *path;
		return 0;
	}
	if (LiIsTitle(titleId, "NPXS10999")) {
		s_npxs10999Path = *path;
		return 0;
	}
#endif

	LsdbValue value(*path);

	return LsdbUpdateAppInfoBits(titleId, LSDB_KEY_LIVEAREA_PATH, &value, -1);
}

/*
 * Reads the LiveAreas of the titles in tbl_livearea again (titleId's alone when it is not
 * empty) for language; path, when not NULL, gets titleId's path
 */
__attribute__((noinline)) int LiDaemon::UpdateTitles(const paf::string &titleId, int language,
	paf::string *path)
{
	paf::vector<LiTitlePath> titles;
	ActivityDbConnection *connection;

	if (m_access == NULL || (connection = m_access->BeginTransaction()) == NULL)
		return LSDB_ERROR_NOT_READY;

	int result = LiSelectTitles(connection, &titleId, &titles);

	if (result == 0) {
		for (paf::vector<LiTitlePath>::iterator it = titles.begin(); it != titles.end();
		     ++it) {
			result = Instance()->Delete(&it->titleId);
#if LI_SYSTEM_TITLES
			if (result == 0 && !LiIsTitle(&it->titleId, "NPXS10998") &&
			    !LiIsTitle(&it->titleId, "NPXS10999")) {
#else
			if (result == 0) {
#endif
				result = LiSetContentsPath(&it->titleId, &it->orgPath);
				if (result == 0)
					result = Instance()->Register(&it->titleId,
						it->orgPath.c_str(), language);
			}
			if (result != 0)
				result = 0;
			if (path != NULL && titleId.size() != 0)
				*path = it->orgPath;
		}
	}
	m_access->EndTransaction(result);
	return result;
}

/*
 * A title's tbl_livearea row read into tmpl from the row statement stepped to: its style,
 * format and revision unless kept, its parts and its functions
 */
static __attribute__((noinline)) int LiReadTemplateRow(LiTemplate *tmpl,
	RefPtr<Statement> *statement, bool keepStyle)
{
	LiFunctionZone *zone = NULL;
	int column = 1;
	int result;

	for (;;) {
		dbutil::Value value;

		result = (*statement)->GetValue(column, &value);
		if (result != 0)
			break;
		if (sce_paf_memcmp(value.name, "sfType", 6) == 0) {
			if (value.type == SQLITE_INTEGER) {
				if (zone == NULL)
					zone = new LiFunctionZone;
				if (zone != NULL && zone->m_functions.size() < 8) {
					LiFunction function;

					function.func = value.type == SQLITE_INTEGER ?
						value.data.integer : 0;
					result = (*statement)->GetValue(column + 1, &value);
					if (result == 0)
						function.target = LiValueText(&value);
					zone->m_functions.push_back(function);
				}
			}
			column += 2;
		} else {
			if (!keepStyle) {
				if (sce_paf_strcmp(value.name, "style") == 0) {
					tmpl->m_style = LiValueText(&value);
				} else if (sce_paf_strcmp(value.name, "formatVer") == 0) {
					tmpl->m_formatVer = LiValueText(&value);
					tmpl->m_formatVersion = SceLsdb_19BA5A0A(&tmpl->m_formatVer);
				} else if (sce_paf_strcmp(value.name, "contentRev") == 0) {
					tmpl->m_contentRev = value.type == SQLITE_INTEGER ?
						value.data.integer : 0;
				} else if (sce_paf_strcmp(value.name, "background_image") == 0) {
					LiBackground *background = new LiBackground;

					if (background != NULL) {
						background->m_image = LiValueText(&value);
						tmpl->m_parts.push_back(background);
					}
				} else if (sce_paf_strcmp(value.name, "gate_startupImage") == 0) {
					LiGate *gate = new LiGate;

					if (gate != NULL) {
						gate->m_startupImage = LiValueText(&value);
						tmpl->m_parts.push_back(gate);
					}
				} else if (sce_paf_strcmp(value.name, "titleColor") == 0) {
					if (value.type == SQLITE_INTEGER) {
						LiTitle *title = new LiTitle;

						if (title != NULL) {
							SceInt64 color = value.data.integer;

							title->m_color[0] =
								((color & 0xFF000000) >> 24) / 255.0f;
							title->m_color[1] =
								((color & 0xFF0000) >> 16) / 255.0f;
							title->m_color[2] = ((color & 0xFF00) >> 8) / 255.0f;
							title->m_color[3] = (color & 0xFF) / 255.0f;
							tmpl->m_parts.push_back(title);
						}
					}
				} else if (sce_paf_strcmp(value.name, "modifiedDate") == 0) {
					tmpl->m_modifiedDate = value.type == SQLITE_TEXT ?
						SceActivityDb_ADFC2C4C(value.data.text) : 0;
				}
			}
			column += 1;
		}
		if (column >= 25) {
			if (zone != NULL)
				tmpl->m_parts.push_back(zone);
			return result;
		}
	}
	if (zone != NULL)
		delete zone;
	return result;
}

/*
 * A tbl_livearea_frame row read into a new frame of tmpl, unless tmpl has the frame: its
 * liveitems that show (any model's, for age, now or on the network time) and its user data
 * when asked for. A frame of the default template is skipped (*skipped set) unless kept.
 */
static __attribute__((noinline)) int LiReadFrameRow(LiTemplate *tmpl,
	RefPtr<Statement> *statement, bool anyModel, bool keepDefault, bool readUserdata,
	bool network, bool *skipped, int age, int pokesute)
{
	paf::string id;
	dbutil::Value value;
	LiFrame *frame = NULL;
	bool isDefault = false;
	int column = 34;
	SceInt64 type;
	int result = (*statement)->GetValue(29, &value);

	if (result != 0)
		goto out;
	id = LiValueText(&value);
	if (tmpl->HasFrame(&id))
		goto out;
	result = (*statement)->GetValue(30, &value);
	if (result != 0)
		goto out;
	type = value.type == SQLITE_INTEGER ? value.data.integer : 0;
	if (type == 0) {
		isDefault = true;
		if (!keepDefault) {
			if (skipped != NULL)
				*skipped = true;
			goto out;
		}
	}
	frame = new LiFrame;
	if (frame == NULL) {
		result = LSDB_ERROR_NO_MEMORY;
		goto error;
	}
	frame->m_id = id;
	frame->m_isDefault = isDefault;
	result = (*statement)->GetValue(31, &value);
	if (result != 0)
		goto error;
	frame->m_multi = value.type == SQLITE_INTEGER ? value.data.integer : 0;
	result = (*statement)->GetValue(32, &value);
	if (result != 0)
		goto error;
	frame->m_autoflip = value.type == SQLITE_INTEGER ? value.data.integer : 0;
	result = (*statement)->GetValue(33, &value);
	if (result != 0)
		goto error;
	frame->m_rev = value.type == SQLITE_INTEGER ? value.data.integer : 0;
	for (int i = 0; i < LI_ROW_ITEM_COUNT; i++, column += 3) {
		LiLiveItem *item;
		LiItemBackground *background;
		LiImage *image;

		result = (*statement)->GetValue(column, &value);
		if (result != 0)
			goto error;
		if (value.type != SQLITE_TEXT)
			continue;
		item = new LiLiveItem;
		if (item == NULL) {
			result = LSDB_ERROR_NO_MEMORY;
			goto error;
		}
		if (!LiReadLiveitem(item, value.data.text, anyModel, age, tmpl->m_formatVersion,
				    pokesute) ||
		    !SceLsdb_628CFBE1(item, true, network)) {
			delete item;
			continue;
		}
		background = (LiItemBackground *)item->GetElement(LI_ITEM_BACKGROUND);
		if (background != NULL) {
			result = (*statement)->GetValue(column + 1, &value);
			if (result != 0)
				goto error;
			background->m_image = LiValueText(&value);
		}
		image = (LiImage *)item->GetElement(LI_ITEM_IMAGE);
		if (image != NULL) {
			result = (*statement)->GetValue(column + 2, &value);
			if (result != 0)
				goto error;
			image->m_image = LiValueText(&value);
		}
		frame->m_items.push_back(item);
	}
	if (readUserdata) {
		result = (*statement)->GetValue(64, &value);
		if (result != 0)
			goto error;
		frame->m_userdata = LiValueText(&value);
	}
	tmpl->m_parts.push_back(frame);
	goto out;
error:
	if (frame != NULL)
		delete frame;
out:
	return result;
}

/*
 * A title's template read from the database into *out: its tbl_livearea row and its frames,
 * or (keepStyle) its row only
 */
static __attribute__((noinline)) int LiSelectTemplate(ActivityDbConnection *connection,
	paf::common::SharedPtr<LiTemplate> *out, const paf::string *titleId, bool anyModel,
	bool keepStyle, bool keepDefault, bool readUserdata, bool network, bool *skipped, int age,
	int pokesute)
{
	RefPtr<Statement> statement;
	LiTemplate *tmpl = NULL;
	bool started = false;
	int result = connection->Prepare(paf::string("SELECT * FROM tbl_livearea "
		"LEFT OUTER JOIN tbl_livearea_frame "
		"ON tbl_livearea.titleId = tbl_livearea_frame.titleId "
		"WHERE tbl_livearea.titleId=? "
		"ORDER BY tbl_livearea_frame.frameId, tbl_livearea_frame.type"), &statement);

	if (result != 0 || (result = AppDbBindString(&statement, 1, titleId)) != 0)
		goto out;
	for (;;) {
		result = statement->Step(1);
		if (result != 0)
			break;
		if (!started) {
			tmpl = new LiTemplate;
			if (tmpl == NULL) {
				result = LSDB_ERROR_NO_MEMORY;
				goto error;
			}
			result = LiReadTemplateRow(tmpl, &statement, keepStyle);
			if (result != 0)
				goto error;
			if (keepStyle)
				goto done;
			started = true;
		}
		result = LiReadFrameRow(tmpl, &statement, anyModel, keepDefault, readUserdata,
					network, skipped, age, pokesute);
		if (result != 0)
			goto error;
	}
	if (result == 1) {
		if (!started) {
			result = LSDB_ERROR_NOT_FOUND;
			goto error;
		}
		result = 0;
	}
done:
	*out = paf::common::SharedPtr<LiTemplate>(tmpl);
	goto out;
error:
	if (tmpl != NULL)
		delete tmpl;
out:
	return result;
}

/* deletes a title's frames, or only those of the default template */
static __attribute__((noinline)) int LiDeleteFrames(ActivityDbConnection *connection,
	const paf::string *titleId, bool defaultOnly)
{
	paf::string sql("DELETE FROM tbl_livearea_frame WHERE titleId=?");
	RefPtr<Statement> statement;
	int result;

	if (defaultOnly)
		sql.append(" AND type=?");
	result = connection->Prepare(sql, &statement);
	if (result != 0)
		return result;
	result = AppDbBindString(&statement, 1, titleId);
	if (result != 0)
		return result;
	if (defaultOnly) {
		result = AppDbBindInt(&statement, 2, 0);
		if (result != 0)
			return result;
	}
	result = statement->Step(1);
	if (result == 1)
		result = 0;
	return result;
}

/*
 * A title's template from the database, with the liveitems that show for the user now; the
 * default template's frames are dropped, and deleted, unless kept
 */
__attribute__((noinline)) int LiDaemon::GetTemplate(paf::common::SharedPtr<LiTemplate> *tmpl,
	const paf::string *titleId, bool keepDefault)
{
	LsdbAppInfoPtr info(new LsdbAppInfo);
	bool skipped = false;
	int pokesute = 0;
	int age = SceLsdb_1673AD48();

	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;

	ActivityDbConnection *connection = m_access->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result;

#if LI_SYSTEM_TITLES
	if ((s_npxs10998Path.size() == 0 && LiIsTitle(titleId, "NPXS10998")) ||
	    (s_npxs10999Path.size() == 0 && LiIsTitle(titleId, "NPXS10999"))) {
		result = LSDB_ERROR_NOT_FOUND;
	} else
#endif
	{
		result = LiGetAppInfo(titleId, &info, 0);
		if (result == 0) {
			if (sceLsdbGetType(&info) == 3)
				pokesute = SceLsdb_BE72A404(titleId->c_str());
			result = LiSelectTemplate(connection, tmpl, titleId,
				SceLsdb_41019D89(&info), false, keepDefault, false,
				SceLsdb_69B32F6C(sceLsdbGetSystemVersion(&info)), &skipped, age,
				pokesute);
			if (result == 0 && skipped)
				result = LiDeleteFrames(connection, titleId, true);
		}
	}
	m_access->EndTransaction(result);
	return result;
}

/*
 * The first title installed in ur0: (by its last launch) that is not in excluded, into
 * *titleId
 */
static __attribute__((noinline)) int LiSelectUr0Title(ActivityDbConnection *connection,
	paf::vector<paf::string> *excluded, paf::string *titleId)
{
	RefPtr<Statement> statement;
	int result = connection->Prepare(paf::string("SELECT tbl_livearea.titleId "
		"FROM tbl_livearea LEFT OUTER JOIN tbl_appinfo "
		"ON tbl_livearea.titleId=tbl_appinfo.titleId AND tbl_appinfo.key=?2 "
		"WHERE org_path LIKE ?1 ORDER BY tbl_appinfo.val ASC"), &statement);

	if (result != 0)
		return result;
	result = AppDbBindText(&statement, 1, "ur0:%", -1);
	if (result != 0)
		return result;
	result = AppDbBindInt(&statement, 2, LSDB_KEY_LLTIME);
	if (result != 0)
		return result;
	for (;;) {
		result = statement->Step(1);
		if (result != 0)
			break;

		paf::string id = AppDbColumnString(&statement, 0);
		paf::vector<paf::string>::iterator it;

		for (it = excluded->begin(); it != excluded->end(); ++it) {
			if (LiIsTitle(&*it, id.c_str()))
				break;
		}
		if (it == excluded->end()) {
			titleId->swap(id);
			break;
		}
	}
	if (result == 1)
		result = 0;
	return result;
}


/*
 * Once ur0: holds its limit of titles, the first of them not in excluded into *titleId;
 * LSDB_ERROR_NOT_FOUND without one
 */
__attribute__((noinline)) int LiDaemon::GetUr0TitleToRemove(
	paf::vector<paf::string> *excluded, paf::string *titleId)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;

	ActivityDbConnection *connection = m_access->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int count;

	{
		RefPtr<Statement> statement;

		count = connection->Prepare(
			paf::string("SELECT count(*) FROM tbl_livearea WHERE org_path LIKE ?"),
			&statement);
		if (count != 0)
			goto counted;
		count = AppDbBindText(&statement, 1, "ur0:%", -1);
		if (count != 0)
			goto counted;
		count = statement->Step(1);
		if (count != 0)
			goto counted;
		count = AppDbColumnInt(&statement, 0);
	}
counted:
	if (count >= LI_UR0_TITLE_MAX)
		LiSelectUr0Title(connection, excluded, titleId);
	m_access->EndTransaction(-1);
	return titleId->size() == 0 ? LSDB_ERROR_NOT_FOUND : 0;
}

#if LI_SYSTEM_TITLES
/* a title's template replaced by the one in dir */
static __attribute__((noinline)) int LiReplaceTemplate(const paf::string *titleId,
	const paf::string *dir)
{
	LiDaemon::Instance()->Delete(titleId);

	int result = LiDaemon::Instance()->Register(titleId, dir->c_str(), -1);

	if (result == 0)
		result = LiSetContentsPath(titleId, dir);
	return result;
}

#endif

/*
 * A frame's userdata, its first 1023 bytes. A system title whose contents path is not known
 * has its template read again first.
 */
__attribute__((noinline)) int LiDaemon::GetUserdata(const paf::string *titleId,
	const paf::string *frameId, paf::string *userdata, SceUID pid)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
#if LI_SYSTEM_TITLES
	if ((s_npxs10998Path.size() == 0 && LiIsTitle(titleId, "NPXS10998")) ||
	    (s_npxs10999Path.size() == 0 && LiIsTitle(titleId, "NPXS10999"))) {
		paf::string path = LiContentsPath(titleId, pid);
		int result = LiReplaceTemplate(titleId, &path);

		if (result != 0)
			return result;
	}
#endif

	ActivityDbConnection *connection = m_access->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result;

	{
		RefPtr<Statement> statement;

		result = connection->Prepare(paf::string("SELECT userdata FROM tbl_livearea_frame "
			"WHERE titleId=? AND frameId=? ORDER BY type LIMIT 1"), &statement);
		dbutil::Value value;

		if (result != 0)
			goto end;
		result = AppDbBindString(&statement, 1, titleId);
		if (result != 0)
			goto end;
		result = AppDbBindString(&statement, 2, frameId);
		if (result != 0)
			goto end;
		result = statement->Step(1);
		if (result != 0) {
			if (result == 1)
				result = LI_ERROR_NO_FRAME;
			goto end;
		}
		result = statement->GetValue(0, &value);
		if (result != 0)
			goto end;
		*userdata = LiValueText(&value);
		if (userdata->size() > LI_USERDATA_MAX)
			userdata->erase(LI_USERDATA_MAX, paf::string::npos);
	}
end:
	m_access->EndTransaction(-1);
	return result;
}

/*
 * A file copied from src to dst, or moved from ur0:temp/livearea; its size added to *size
 * when size is not NULL
 */
static __attribute__((noinline)) int LiCopyFile(const char *src, const char *dst,
	SceUInt64 *size)
{
	int result = 0;

	if (sce_paf_memcmp(src, "ur0:temp/livearea", 17) == 0) {
		result = paf::LocalFile::RenameFile(src, dst);
		if (result != 0)
			return result;
		if (size != NULL) {
			SceInt64 fileSize = paf::LocalFile::GetFileSize(dst);

			if (fileSize >= 0)
				*size += fileSize;
		}
		return 0;
	}

	paf::common::SharedPtr<paf::File> in;
	paf::common::SharedPtr<paf::File> out;
	paf::common::SharedPtr<paf::MallocBuffer> buffer =
		paf::MallocBuffer::Allocate(LI_COPY_BUFFER_SIZE);

	if (buffer.get() == NULL)
		return LSDB_ERROR_NO_MEMORY;
	/* 0x10000: a flag of paf's own */
	in = paf::LocalFile::Open(src, SCE_O_RDONLY | 0x10000, 6, NULL);
	if (in.get() == NULL)
		return LSDB_ERROR_NOT_FOUND;
	out = paf::LocalFile::Open(dst, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC | 0x10000, 6,
				   NULL);
	if (out.get() == NULL)
		return LSDB_ERROR_DATABASE;
	while (!in->IsEof()) {
		int read = in->Read(buffer->GetAddr(), buffer->GetSize());

		if (read < 0)
			return LSDB_ERROR_DATABASE;

		int written = out->Write(buffer->GetAddr(), read);

		if (written < 0)
			return LSDB_ERROR_DATABASE;
		if (size != NULL)
			*size += written;
	}
	return result;
}

/* the files of a template stored, by their name in its folder: each one's tbl_livearea_file id */
typedef paf::map<paf::string, SceInt64> LiFileMap;

/*
 * A template's file name stored: the file copied from srcDir into dstDir under its
 * tbl_livearea_file id (a new row; a file named again only counts one more user of its row),
 * and *name then that stored name. A PNG or GIM image only, and a file that exists, when
 * strict; the size of the files copied is added to *size.
 */
static __attribute__((noinline)) int LiStoreFile(ActivityDbConnection *connection,
	RefPtr<Statement> *insert, RefPtr<Statement> *addUser, const char *dstDir,
	const char *srcDir, paf::string *name, bool strict, SceUInt64 *size, LiFileMap *files)
{
	if (name->size() == 0)
		return 0;

	SceInt64 id = 0;
	paf::string src = paf::string(srcDir) + "/" + *name;
	paf::string ext;
	paf::string::size_type dot = name->rfind('.');

	if (dot != paf::string::npos) {
		paf::string suffix = name->substr(dot, paf::string::npos);

		for (paf::string::size_type i = 0; i < suffix.size(); i++) {
			if (sce_paf_look_ctype_table((unsigned char)suffix.at(i)) & 1)
				suffix.at(i) = sce_paf_tolower((unsigned char)suffix.at(i));
		}
		ext = suffix;
	}

	const char *formats[2] = { ".png", ".gim" };
	int result = LI_ERROR_IMAGE_FORMAT;

	for (int i = 0; i < 2; i++) {
		if (sce_paf_strcasecmp(ext.c_str(), formats[i]) == 0) {
			result = 0;
			break;
		}
	}
	if (result != 0 && strict)
		goto end;

	{
		LiFileMap::iterator it = files->find(*name);
		bool copy;

		if (it == files->end()) {
			if (strict && !paf::LocalFile::Exists(src.c_str())) {
				result = LI_ERROR_NO_FILE;
				goto end;
			}
			{
				paf::string dir(dstDir);

				result = (*insert)->ClearBindings();
				if (result == 0 && (result = (*insert)->Reset()) == 0 &&
				    (result = AppDbBindString(insert, 1, &ext)) == 0 &&
				    (result = AppDbBindString(insert, 2, &dir)) == 0 &&
				    (result = (*insert)->Step(1)) == 1) {
					id = connection->GetLastInsertRowid();
					result = 0;
				}
			}
			if (result != 0)
				goto end;
			files->insert(LiFileMap::value_type(*name, id));
			copy = true;
		} else {
			id = it->second;
			result = (*addUser)->ClearBindings();
			if (result == 0 && (result = (*addUser)->Reset()) == 0 &&
			    (result = AppDbBindInt(addUser, 1, id)) == 0 &&
			    (result = (*addUser)->Step(1)) == 1)
				result = connection->GetChanges() == 1 ? 0 : LSDB_ERROR_DATABASE;
			if (result != 0)
				goto end;
			copy = false;
		}
		*name = paf::string(" ") + LiNumber(id) + ext;
		if (!copy)
			goto end;

		paf::string dst = paf::string(dstDir) + '/' + *name;

		result = LiCopyFile(src.c_str(), dst.c_str(), size);
		if (result != 0)
			goto end;
		if (size != NULL && *size > LI_FILES_SIZE_MAX)
			return LI_ERROR_FILES_TOO_LARGE;
		LiDaemon::Instance()->m_addList.push_back(dst);
	}
end:
	return result;
}

/*
 * The files a template names stored: its background, its gate's startup image and its
 * liveitems' backgrounds and images, copied from srcDir into dstDir
 */
static __attribute__((noinline)) int LiStoreFiles(ActivityDbConnection *connection,
	const paf::string *titleId, const char *dstDir, const char *srcDir,
	paf::common::SharedPtr<LiTemplate> *tmpl, bool strict)
{
	LiFileMap files;
	SceUInt64 size = 0;
	RefPtr<Statement> insert;
	RefPtr<Statement> addUser;
	int result = connection->Prepare(
		paf::string("INSERT INTO tbl_livearea_file VALUES (NULL,1,?,?)"), &insert);

	if (result != 0)
		goto end;
	result = connection->Prepare(
		paf::string("UPDATE tbl_livearea_file SET refcnt=refcnt+1 WHERE rowid=?"), &addUser);
	if (result != 0)
		goto end;

	{
		LiBackground *background = (LiBackground *)(*tmpl)->GetPart(LI_PART_BACKGROUND);

		if (background != NULL) {
			result = LiStoreFile(connection, &insert, &addUser, dstDir, srcDir,
					     &background->m_image, strict, &size, &files);
			if (result != 0)
				goto end;
		}

		LiGate *gate = (LiGate *)(*tmpl)->GetPart(LI_PART_GATE);

		if (gate != NULL) {
			result = LiStoreFile(connection, &insert, &addUser, dstDir, srcDir,
					     &gate->m_startupImage, strict, &size, &files);
			if (result != 0)
				goto end;
		}
	}
	for (paf::list<LiPart *>::iterator part = (*tmpl)->m_parts.begin();
	     part != (*tmpl)->m_parts.end(); ++part) {
		if ((*part)->m_type != LI_PART_FRAME)
			continue;

		LiFrame *frame = (LiFrame *)*part;

		for (paf::vector<LiLiveItem *>::iterator item = frame->m_items.begin();
		     item != frame->m_items.end(); ++item) {
			for (paf::vector<LiItemElement *>::iterator element =
				(*item)->m_elements.begin();
			     element != (*item)->m_elements.end(); ++element) {
				if ((*element)->m_type == LI_ITEM_BACKGROUND) {
					result = LiStoreFile(connection, &insert, &addUser, dstDir,
						srcDir, &((LiItemBackground *)*element)->m_image,
						strict, &size, &files);
					if (result != 0)
						goto end;
				} else if ((*element)->m_type == LI_ITEM_IMAGE) {
					result = LiStoreFile(connection, &insert, &addUser, dstDir,
						srcDir, &((LiImage *)*element)->m_image, strict,
						&size, &files);
					if (result != 0)
						goto end;
				}
			}
		}
	}
end:
	return result;
}

/* the folder a title's LiveArea contents are stored in */
static inline paf::string LiStoredContentsDir(const paf::string *titleId)
{
	return paf::string("ur0:appmeta/") + *titleId + "/livearea/contents";
}

/*
 * A title's template stored from the database to ur0:appmeta/<title>/livearea/contents with
 * the files in *path, and *path then that folder
 */
__attribute__((noinline)) int LiDaemon::StoreContents(const paf::string *titleId,
	paf::string *path, bool system, bool network)
{
	paf::common::SharedPtr<LiTemplate> tmpl;
	paf::string dir;
	ActivityDbConnection *connection = m_access->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result = LiSelectTemplate(connection, &tmpl, titleId, system, false, false, true,
				      network, NULL, -1, 0);

	if (result == 0) {
		dir = LiStoredContentsDir(titleId);
		if (!paf::Dir::IsDirectory(dir.c_str()))
			paf::Dir::CreateRecursive(dir.c_str(), 6);
		result = LiStoreFiles(connection, titleId, dir.c_str(), path->c_str(), &tmpl,
				      false);
		if (result == 0) {
			result = LiWriteFrames(connection, titleId, &tmpl, NULL, false, system);
			if (result == 0) {
				result = LiSetContentsPath(titleId, &dir);
				if (result == 0) {
					*path = dir;
					m_access->AddEndCallback(RemoveFiles);
				}
			}
		}
	}
	m_access->EndTransaction(result);
	return result;
}

/*
 * A title's template written from tmpl, read from dir: the frames the template has (its
 * style's frames are dropped first when it is a new style), and its files stored with them
 * unless *path is the folder they are in. The mask of its parts goes to *mask, the folder
 * they were stored to into *storedPath.
 */
__attribute__((noinline)) int LiDaemon::Update(const paf::string *titleId,
	paf::common::SharedPtr<LiTemplate> *tmpl, const char *dir, bool keepStyle,
	paf::string *path, bool system, bool network, unsigned int *mask,
	paf::string *storedPath)
{
	if (dir == NULL)
		return LSDB_ERROR_INVALID_ARGUMENT;

	paf::string style;

#if LI_SYSTEM_TITLES
	if ((s_npxs10998Path.size() == 0 && LiIsTitle(titleId, "NPXS10998")) ||
	    (s_npxs10999Path.size() == 0 && LiIsTitle(titleId, "NPXS10999"))) {
		int result = LiReplaceTemplate(titleId, path);

		if (result != 0)
			return result;
	}
#endif

	bool stored = sce_paf_memcmp(path->c_str(), "ur0:appmeta", 11) == 0;

	if (!stored) {
		bool same = false;

		{
			paf::common::SharedPtr<paf::MallocBuffer> rawPath =
				paf::MallocBuffer::Allocate(LI_RAW_PATH_SIZE);

			if (sceAppMgrGetRawPath(path->c_str(), (char *)rawPath->GetAddr(),
						rawPath->GetSize()) == 0)
				same = sce_paf_strcmp((const char *)rawPath->GetAddr(), dir) == 0;
		}
		if (!same) {
			int result = StoreContents(titleId, path, system, network);

			if (result != 0)
				return result;
			stored = true;
		}
	}

	unsigned int parts = 0;

	for (paf::list<LiPart *>::iterator part = (*tmpl)->m_parts.begin();
	     part != (*tmpl)->m_parts.end(); ++part)
		parts |= (*part)->GetMask();
	*mask = parts;

	ActivityDbConnection *connection = m_access->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result;

	{
		RefPtr<Statement> statement;

		result = connection->Prepare(
			paf::string("SELECT style FROM tbl_livearea WHERE titleId=?"), &statement);
		if (result == 0 && (result = AppDbBindString(&statement, 1, titleId)) == 0) {
			result = statement->Step(1);
			if (result != 0) {
				if (result == 1)
					result = LSDB_ERROR_NOT_FOUND;
			} else {
				dbutil::Value value;

				result = statement->GetValue(0, &value);
				if (result == 0)
					style = LiValueText(&value);
			}
		}
	}
	if (result == 0) {
		if ((*tmpl)->m_style.size() != 0 && (*tmpl)->m_formatVer.size() != 0 &&
		    !((*tmpl)->m_style == style)) {
			*mask |= LI_MASK_STYLE;
			result = LiDeleteFrames(connection, titleId, false);
			if (result != 0)
				goto end;
			style = (*tmpl)->m_style;
		}
		for (paf::list<LiPart *>::iterator part = (*tmpl)->m_parts.begin();
		     part != (*tmpl)->m_parts.end(); ++part) {
			if ((*part)->m_type == LI_PART_FRAME &&
			    !LiIsFrameAllowed(&style, &((LiFrame *)*part)->m_id)) {
				result = LI_ERROR_NO_FRAME;
				goto end;
			}
		}
		if (!stored || (result = LiStoreFiles(connection, titleId, path->c_str(), dir,
						      tmpl, true)) == 0) {
			result = LiWriteFrames(connection, titleId, tmpl, NULL, keepStyle, system);
			if (result == 0) {
				m_access->AddEndCallback(RemoveFiles);
				if (storedPath != NULL)
					*storedPath = *path;
			}
		}
	}
end:
	m_access->EndTransaction(result);
	return result;
}

/*
 * A title's template read from dir and written: an application's must have an "a" style
 * (a1 to a9); the date it was modified is kept with it
 */
__attribute__((noinline)) int LiDaemon::UpdateFromDir(const paf::string *titleId,
	const char *dir, unsigned int *mask, const SceUInt64 *modifiedDate,
	paf::string *storedPath, SceUID pid)
{
	paf::common::SharedPtr<LiTemplate> tmpl;
	LsdbAppInfoPtr info(new LsdbAppInfo);

	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;

	paf::string path;
	int result = LiGetAppInfo(titleId, &info, pid);

	if (result == 0) {
		bool system = SceLsdb_41019D89(&info);
		int version = sceLsdbGetSystemVersion(&info);

		path = SceLsdb_C070E42E(&info);
		result = LiReadTemplate(&tmpl, system, dir, version, -1);
		if (result != 0)
			return result;
		if (!system && (tmpl->m_style.size() != 2 || tmpl->m_style.c_str()[0] != 'a'))
			return LSDB_ERROR_DATABASE;
		tmpl->m_modifiedDate = *modifiedDate;
		result = Update(titleId, &tmpl, dir, false, &path, system,
				SceLsdb_69B32F6C(version), mask, storedPath);
	}
	return result;
}

/* a title's template written from tmpl, read from dir */
__attribute__((noinline)) int LiDaemon::UpdateTemplate(const paf::string *titleId,
	paf::common::SharedPtr<LiTemplate> *tmpl, const char *dir, bool keepStyle,
	unsigned int *mask, paf::string *storedPath, SceUID pid)
{
	LsdbAppInfoPtr info(new LsdbAppInfo);

	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;

	paf::string path;
	int result = LiGetAppInfo(titleId, &info, pid);

	if (result == 0) {
		path = SceLsdb_C070E42E(&info);
		result = Update(titleId, tmpl, dir, keepStyle, &path, SceLsdb_41019D89(&info),
				SceLsdb_69B32F6C(sceLsdbGetSystemVersion(&info)), mask,
				storedPath);
	}
	return result;
}

/*
 * A system title's contents path forgotten, NPXS10998's folder then the one path is in
 */
__attribute__((noinline)) void LiDaemon::ResetContentsPath(const paf::string *titleId,
	const char *path)
{
#if LI_SYSTEM_TITLES
	if (LiIsTitle(titleId, "NPXS10998")) {
		s_npxs10998Path.assign(NULL, 0);

		const char *slash = sce_paf_strrchr(path, '/');

		if (slash != NULL)
			s_npxs10998Dir.assign(path, slash - path);
		else
			s_npxs10998Dir.assign(NULL, 0);
	} else if (LiIsTitle(titleId, "NPXS10999")) {
		s_npxs10999Path.assign(NULL, 0);
	}
#endif
}

/* a title's tbl_livearea row only (its functions), into *tmpl */
static __attribute__((noinline)) int LiSelectTemplateRow(ActivityDbConnection *connection,
	const paf::string *titleId, paf::common::SharedPtr<LiTemplate> *tmpl)
{
	LsdbAppInfoPtr info(new LsdbAppInfo);
	int result = LiGetAppInfo(titleId, &info, 0);

	if (result == 0)
		result = LiSelectTemplate(connection, tmpl, titleId, SceLsdb_41019D89(&info), true,
			false, false, SceLsdb_69B32F6C(sceLsdbGetSystemVersion(&info)), NULL, -1,
			0);
	return result;
}

/* 1 when a title's system-function-zone has func */
__attribute__((noinline)) int LiDaemon::HasFunction(const paf::string *titleId, int func)
{
	int found = 0;

	if (m_access == NULL)
		return found;

	paf::common::SharedPtr<LiTemplate> tmpl;
	ActivityDbConnection *connection = m_access->BeginTransaction();

	if (connection == NULL)
		return found;
	if (LiSelectTemplateRow(connection, titleId, &tmpl) == 0) {
		LiFunctionZone *zone = (LiFunctionZone *)tmpl->GetPart(LI_PART_FUNCTION_ZONE);

		if (zone != NULL && zone->GetFunction(func) != NULL)
			found = 1;
	}
	m_access->EndTransaction(-1);
	return found;
}

/* a title's pcc columns: the update date, its resource status (from 1) and its sign-off */
__attribute__((noinline)) int LiDaemon::GetPcc(const paf::string *titleId, LiPcc *pcc)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;

	ActivityDbConnection *connection = m_access->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result;

	{
		RefPtr<Statement> statement;

		result = connection->Prepare(paf::string("SELECT pccUpdDate, pccResStatus, "
			"pccSignOff FROM tbl_livearea WHERE titleId=?"), &statement);
		if (result != 0)
			goto end;
		result = AppDbBindString(&statement, 1, titleId);
		if (result != 0)
			goto end;
		result = statement->Step(1);
		if (result != 0) {
			if (result == 1)
				result = LSDB_ERROR_NOT_FOUND;
			goto end;
		}
		pcc->updDate = AppDbColumnInt(&statement, 0);
		pcc->resStatus = AppDbColumnInt(&statement, 1) + 1;
		pcc->signOff = AppDbColumnInt(&statement, 2);
	}
end:
	m_access->EndTransaction(result);
	return result;
}

/*
 * A title's pcc columns written (those of pccFlags), with the system-function-zone's
 * function 0 added (function0) or removed
 */
__attribute__((noinline)) int LiDaemon::SetPcc(const paf::string *titleId, bool function0,
	const LiPcc *pcc, int pccFlags)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;

	paf::common::SharedPtr<LiTemplate> tmpl;
	ActivityDbConnection *connection = m_access->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result = LiSelectTemplateRow(connection, titleId, &tmpl);

	if (result == 0) {
		LiFunctionZone *zone = (LiFunctionZone *)tmpl->GetPart(LI_PART_FUNCTION_ZONE);

		if (function0) {
			if (zone == NULL) {
				zone = new LiFunctionZone;
				tmpl->m_parts.push_back(zone);
			}

			LiFunction *found = zone->GetFunction(0);
			LiFunction function;

			function.func = 0;
			if (found != NULL)
				*found = function;
			else
				zone->m_functions.push_back(function);
		} else if (zone != NULL) {
			paf::vector<LiFunction>::iterator it;

			for (it = zone->m_functions.begin(); it != zone->m_functions.end(); ++it) {
				if (it->func == 0)
					break;
			}
			if (it != zone->m_functions.end())
				zone->m_functions.erase(it);
		}
		result = LiWriteTemplate(connection, titleId, &tmpl, NULL, true, pcc, pccFlags);
	}
	m_access->EndTransaction(result);
	return result;
}

/* a function of a title's system-function-zone set, or added */
__attribute__((noinline)) int LiDaemon::SetFunction(const paf::string *titleId,
	const LiFunction *function)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;

	paf::common::SharedPtr<LiTemplate> tmpl;
	ActivityDbConnection *connection = m_access->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result = LiSelectTemplateRow(connection, titleId, &tmpl);

	if (result == 0) {
		LiFunctionZone *zone = (LiFunctionZone *)tmpl->GetPart(LI_PART_FUNCTION_ZONE);

		if (zone == NULL) {
			zone = new LiFunctionZone;
			tmpl->m_parts.push_back(zone);
		}

		LiFunction *found = zone->GetFunction(function->func);

		if (found != NULL) {
			*found = *function;
		} else {
			LiFunction added;

			added.func = function->func;
			added.target = function->target;
			zone->m_functions.push_back(added);
		}
		result = LiWriteTemplate(connection, titleId, &tmpl, NULL, true, NULL, 0);
	}
	m_access->EndTransaction(result);
	return result;
}

/* a function of a title's system-function-zone removed; LSDB_ERROR_NOT_FOUND without it */
__attribute__((noinline)) int LiDaemon::RemoveFunction(const paf::string *titleId, int func)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;

	paf::common::SharedPtr<LiTemplate> tmpl;
	ActivityDbConnection *connection = m_access->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result = LiSelectTemplateRow(connection, titleId, &tmpl);

	if (result == 0) {
		LiFunctionZone *zone = (LiFunctionZone *)tmpl->GetPart(LI_PART_FUNCTION_ZONE);

		if (zone == NULL) {
			result = LSDB_ERROR_NOT_FOUND;
		} else {
			result = LSDB_ERROR_NOT_FOUND;
			for (paf::vector<LiFunction>::iterator it = zone->m_functions.begin();
			     it != zone->m_functions.end(); ++it) {
				if (it->func == func) {
					zone->m_functions.erase(it);
					result = LiWriteTemplate(connection, titleId, &tmpl, NULL, true,
								 NULL, 0);
					break;
				}
			}
		}
	}
	m_access->EndTransaction(result);
	return result;
}

/* the date a title's template was modified (an rtc tick), 0 without one */
__attribute__((noinline)) int LiDaemon::GetModifiedDate(const paf::string *titleId,
	SceUInt64 *date)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;

	ActivityDbConnection *connection = m_access->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result;

	{
		RefPtr<Statement> statement;

		dbutil::Value value;

		result = connection->Prepare(
			paf::string("SELECT modifiedDate FROM tbl_livearea WHERE titleId=?"),
			&statement);
		if (result != 0)
			goto end;
		result = AppDbBindString(&statement, 1, titleId);
		if (result != 0)
			goto end;
		result = statement->Step(1);
		if (result != 0) {
			if (result == 1)
				result = LSDB_ERROR_NOT_FOUND;
			goto end;
		}
		result = statement->GetValue(0, &value);
		if (result != 0)
			goto end;
		*date = value.type == SQLITE_TEXT ? SceActivityDb_ADFC2C4C(value.data.text) : 0;
	}
end:
	m_access->EndTransaction(-1);
	return result;
}

/* the date a title's template was modified set; LSDB_ERROR_NOT_FOUND without its row */
__attribute__((noinline)) int LiDaemon::SetModifiedDate(const paf::string *titleId,
	const SceUInt64 *date)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;

	ActivityDbConnection *connection = m_access->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result;

	{
		RefPtr<Statement> statement;

		result = connection->Prepare(paf::string("UPDATE tbl_livearea SET "
			"modifiedDate=?2 WHERE titleId=?1"), &statement);
		if (result != 0)
			goto end;
		result = AppDbBindString(&statement, 1, titleId);
		if (result != 0)
			goto end;

		char text[30] = "";

		SceActivityDb_9A8944FC(date, text, sizeof(text));
		result = AppDbBindText(&statement, 2, text, -1);
		if (result != 0)
			goto end;
		result = statement->Step(1);
		if (result != 0) {
			if (result != 1) {
				result = LSDB_ERROR_UPDATE_FAILED;
				goto end;
			}
			result = 0;
		}
		if (connection->GetChanges() == 0)
			result = LSDB_ERROR_NOT_FOUND;
	}
end:
	m_access->EndTransaction(result);
	return result;
}

/*
 * A title's contentRev and org_path, or (frameId) the frameRev of its frame. A system title
 * whose contents path is not known has its template read again first.
 */
__attribute__((noinline)) int LiDaemon::GetRevision(const paf::string *titleId,
	const char *frameId, SceInt64 *rev, paf::string *orgPath, SceUID pid)
{
	if (m_access == NULL)
		return LSDB_ERROR_NOT_READY;
#if LI_SYSTEM_TITLES
	if ((s_npxs10998Path.size() == 0 && LiIsTitle(titleId, "NPXS10998")) ||
	    (s_npxs10999Path.size() == 0 && LiIsTitle(titleId, "NPXS10999"))) {
		paf::string path = LiContentsPath(titleId, pid);
		int result = LiReplaceTemplate(titleId, &path);

		if (result != 0)
			return result;
	}
#endif

	ActivityDbConnection *connection = m_access->BeginTransaction();

	if (connection == NULL)
		return LSDB_ERROR_NOT_READY;

	int result;

	{
		RefPtr<Statement> statement;
		const char *text = "SELECT contentRev, org_path FROM tbl_livearea WHERE titleId=?";

		if (frameId != NULL)
			text = "SELECT frameRev FROM tbl_livearea_frame WHERE titleId=? AND "
			       "frameId=? ORDER BY type LIMIT 1";
		dbutil::Value value;

		result = connection->Prepare(paf::string(text), &statement);
		if (result != 0)
			goto end;
		result = AppDbBindString(&statement, 1, titleId);
		if (result != 0)
			goto end;
		if (frameId != NULL) {
			result = AppDbBindText(&statement, 2, frameId, -1);
			if (result != 0)
				goto end;
		}
		result = statement->Step(1);
		if (result != 0) {
			if (result == 1) {
				result = LSDB_ERROR_NOT_FOUND;
				if (frameId != NULL)
					result = LI_ERROR_NO_FRAME;
			}
			goto end;
		}
		result = statement->GetValue(0, &value);
		if (result != 0)
			goto end;
		*rev = value.type == SQLITE_INTEGER ? value.data.integer : 0;
		if (frameId == NULL && orgPath != NULL) {
			result = statement->GetValue(1, &value);
			if (result != 0)
				goto end;
			*orgPath = LiValueText(&value);
		}
	}
end:
	m_access->EndTransaction(-1);
	return result;
}

/*
 * At the end of a transaction: the files the database dropped are removed, or on a rollback
 * the files written for it; both lists are then emptied
 */
void LiDaemon::RemoveFiles(int rolledBack)
{
	LiDaemon *daemon = Instance();
	paf::vector<paf::string> *files = rolledBack ? &daemon->m_addList : &daemon->m_rmList;

	for (paf::vector<paf::string>::iterator it = files->begin(); it != files->end(); ++it)
		paf::LocalFile::RemoveFile(it->c_str());
	daemon->m_addList.clear();
	daemon->m_rmList.clear();
}
