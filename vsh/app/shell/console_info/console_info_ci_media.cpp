/*
 * console_info_ci_media.cpp: the "ci" record's Int fields the live area's database and the media
 * databases give (0x810F1924-0x810F2174): the titles of some categories installed, and the
 * videos, the music and the photos the media databases hold. The file's and the names are ours
 * (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/string.h>
#include <std/stdio.h>
#include <std/vector>
#include <dbutil/connection.h>
#include <dbutil/statement.h>
#include "console_info.h"
#include "../src/livespace_db/livespace_db_internal.h"

PAF_VECTOR_NOINLINE_PUSH_BACK(LsdbValue, paf::allocator<LsdbValue>)

using namespace paf;

/* the Int fields giving the titles of some categories (their indices) */
#define CONSOLE_INFO_CI_FIELD_GAMES		14
#define CONSOLE_INFO_CI_FIELD_EG		15
#define CONSOLE_INFO_CI_FIELD_NONE_16		16
#define CONSOLE_INFO_CI_FIELD_NONE_18		18
#define CONSOLE_INFO_CI_FIELD_NONE_19		19
#define CONSOLE_INFO_CI_FIELD_NONE_20		20
#define CONSOLE_INFO_CI_FIELD_ME		17

/* the media databases' mode (SQLite's read-write and create flags) */
#define CONSOLE_INFO_CI_MEDIA_OPEN		6

/* the content types the video and the photo databases count */
#define CONSOLE_INFO_CI_VIDEO_TYPE		5
#define CONSOLE_INFO_CI_PHOTO_TYPE		4LL

/* (0x81544588, 0x815445A0, 0x815445A4) the categories each field counts */
static const char *s_gameCategories[] = { "gd", "gp", "gdc", "gdd", "gpc", "gpd" };
static const char *s_egCategories[] = { "EG" };
static const char *s_meCategories[] = { "ME" };

/* (0x810F1924) the titles installed of the field's categories */
int consoleInfoCiGetLsdbCount(ConsoleInfoCiIntField *field, int *value)
{
	const char **categories;
	int count;

	switch (field->m_index) {
	case CONSOLE_INFO_CI_FIELD_GAMES:
		categories = s_gameCategories;
		count = sizeof(s_gameCategories) / sizeof(s_gameCategories[0]);
		break;
	case CONSOLE_INFO_CI_FIELD_EG:
		categories = s_egCategories;
		count = sizeof(s_egCategories) / sizeof(s_egCategories[0]);
		break;
	case CONSOLE_INFO_CI_FIELD_ME:
		categories = s_meCategories;
		count = sizeof(s_meCategories) / sizeof(s_meCategories[0]);
		break;
	case CONSOLE_INFO_CI_FIELD_NONE_16:
		return 0;
	case CONSOLE_INFO_CI_FIELD_NONE_18:
	case CONSOLE_INFO_CI_FIELD_NONE_19:
	case CONSOLE_INFO_CI_FIELD_NONE_20:
	default:
		return 0;
	}

	LsdbQueryCondition condition;

	condition.op = LSDB_QUERY_OP_EQUAL;
	condition.next = NULL;
	condition.key = LSDB_KEY_CATEGORY;
	for (int i = 0; i < count; i++, categories++)
		condition.values.push_back(LsdbValue(*categories));

	AppInfoQClient client;
	vector<string> titleIds;

	SceLsdb_A6D8772C(&client);
	SceLsdb_E90B81CD(&client);
	SceLsdb_76B75713(&client, &condition, &titleIds, 0);
	*value = titleIds.size();
	SceLsdb_E2101B03(&client);
	return 0;
}

/* (0x810F1AC6) the count a query of a media database gives */
__attribute__((noinline)) static int consoleInfoCiQueryCount(const char *path, const char *sql,
							      int *count)
{
	dbutil::Connection db;

	*count = 0;
	if (db.Open(string(path), CONSOLE_INFO_CI_MEDIA_OPEN) != 0)
		return 0;

	dbutil::RefPtr<dbutil::Statement> statement;

	if (db.Prepare(string(sql), &statement) == 0) {
		dbutil::Value result;

		if (statement->Step(1) == 0 && statement->GetValue(0, &result) == 0 &&
		    result.type == SQLITE_INTEGER)
			*count = (int)result.data.integer;
	}
	db.Close();
	return 0;
}

/* (0x810F1BDC) the videos */
int consoleInfoCiGetVideoCount(ConsoleInfoCiIntField *field, int *value)
{
	char type[64];
	string query;

	sce_paf_snprintf(type, sizeof(type), "%i", CONSOLE_INFO_CI_VIDEO_TYPE);
	query.append("SELECT count(mrid) FROM tbl_vpcontent WHERE content_type='" + string(type) +
		     "'");
	consoleInfoCiQueryCount("ux0:/mms/video/AVContent.db", query.c_str(), value);
	return 0;
}

/* (0x810F1E28) the music */
int consoleInfoCiGetMusicCount(ConsoleInfoCiIntField *field, int *value)
{
	consoleInfoCiQueryCount("ux0:/mms/music/AVContent.db", "SELECT count(mrid) from tbl_Music",
				value);
	return 0;
}

/* (0x810F1E44) the photos */
int consoleInfoCiGetPhotoCount(ConsoleInfoCiIntField *field, int *value)
{
	char type[64];
	string query;

	query += "SELECT count(*) FROM tbl_vpcontent";

	sce_paf_snprintf(type, sizeof(type), "%lld", CONSOLE_INFO_CI_PHOTO_TYPE);
	query.append(" WHERE content_type='" + string(type) + "'");
	consoleInfoCiQueryCount("ux0:/mms/photo/AVContent.db", query.c_str(), value);
	return 0;
}
