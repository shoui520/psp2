/*
 * The icon layout: where each application's icon is on the home screen, a line per title in
 * ux0:iconlayout.ini ("page<TAB>position", and for an icon in a folder the folder's place and
 * title after them), read and written through SceIniFileProcessor. The file stays open while a
 * caller uses it; the names are ours.
 */
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/algorithm>
#include <file/localfile.h>
#include <ini_file_processor/ini_file_processor.h>
#include "livespace_db_internal.h"

PAF_VECTOR_INLINE_DTOR(int)

#define LSDB_ICON_LAYOUT_PATH		"ux0:iconlayout.ini"
#define LSDB_ICON_LAYOUT_OPEN_FLAGS	0x300006	/* as Sony's build passes them */
#define LSDB_ICON_LAYOUT_LINE_SIZE	256
#define LSDB_ICON_LAYOUT_KEY_SIZE	128

/* paf's result for a file that is not there */
#define LSDB_PAF_ERROR_NO_FILE		-2135994334	/* 0x80AF5022 */

static IniFileProcessor s_iconLayout;
static int s_iconLayoutUsers = 0;		/* the callers that have the file open */
static bool s_iconLayoutReadOnly = false;	/* set: nothing is written */

static void *IconLayoutAlloc(SceSize size, void *context);
static void IconLayoutFree(void *ptr, void *context);

extern "C" {

/* one caller less; the last one closes the file */
void SceLsdb_E05F71CB(void)
{
	if (s_iconLayoutUsers > 1) {
		s_iconLayoutUsers--;
	} else if (s_iconLayoutUsers == 1) {
		s_iconLayoutUsers = 0;
		s_iconLayout.close();
		s_iconLayout.terminate();
	}
}

/* one caller more: the first opens the file (made when it is not there), the others rewind it */
int SceLsdb_30900530(void)
{
	int result = 0;

	s_iconLayoutUsers++;
	if (s_iconLayoutUsers == 1) {
		IniAllocatorOps ops = { IconLayoutAlloc, IconLayoutFree };
		IniConfig config;

		config.poolSize = 0x2000;
		config.reserved = 0x2000;
		config.ops = &ops;
		result = s_iconLayout.initialize(&config);
		if (result != 0)
			goto fail;
		result = s_iconLayout.open(LSDB_ICON_LAYOUT_PATH, "rw", LSDB_ICON_LAYOUT_OPEN_FLAGS);
		if (result == SCE_INI_FILE_PROCESSOR_ERROR_NO_FILE_NAME)
			result = s_iconLayout.create(LSDB_ICON_LAYOUT_PATH, "rw",
						     LSDB_ICON_LAYOUT_OPEN_FLAGS);
	} else if (s_iconLayoutUsers > 1) {
		result = s_iconLayout.rewind();
	} else {
		return result;
	}
	if (result == 0)
		return result;
fail:
	SceLsdb_E05F71CB();
	return result;
}

/* the file removed (none is not an error) */
int SceLsdb_06CD70C5(void)
{
	int result = paf::LocalFile::RemoveFile(LSDB_ICON_LAYOUT_PATH);

	if (result == LSDB_PAF_ERROR_NO_FILE)
		return 0;
	return result;
}

bool SceLsdb_8DEDCA55(void)
{
	return paf::LocalFile::Exists(LSDB_ICON_LAYOUT_PATH);
}

void SceLsdb_DEC47CBD(bool readOnly)
{
	s_iconLayoutReadOnly = readOnly;
}

}

/* a location read from nothing */
static inline void IconLayoutClear(LsdbIconLocation *location)
{
	location->pageNo = 0;
	location->pos = 0;
	location->folderPageNo = 0;
	location->folderPos = 0;
}

/* a line of the file: the page (or folder ID) and position, then the folder's place and title */
static int IconLayoutParse(const char *text, LsdbIconLocation *location)
{
	char *end = NULL;
	int value = sce_paf_strtol(text, &end, 16);

	if (end == NULL || *end != '\t' ||
	    !((value >= 0 && value < 10) ||
	      (value >= LSDB_FOLDER_ID_MIN && value < LSDB_FOLDER_ID_END)))
		return -1;
	location->pageNo = value;
	value = sce_paf_strtol(end + 1, &end, 16);
	if (end == NULL || value < 0 || value >= 10)
		return -1;
	location->pos = value;
	if (*end != '\t') {
		if (*end == '\0')
			return 0;
		return -1;
	}
	value = sce_paf_strtol(end + 1, &end, 16);
	if (end == NULL || *end != '\t' || value < 0 || value >= 10)
		return -1;
	location->folderPageNo = value;
	value = sce_paf_strtol(end + 1, &end, 16);
	if (end == NULL || *end != '\t' || value < 0 || value >= 10)
		return -1;

	const char *title = end + 1;

	location->folderPos = value;
	location->folderTitle = title;
	return 0;
}

/* a line written for location: the long form for a folder */
static void IconLayoutFormat(char *line, const LsdbIconLocation *location)
{
	if (location->pageNo < LSDB_FOLDER_ID_MIN || location->pageNo >= LSDB_FOLDER_ID_END)
		sce_paf_snprintf(line, LSDB_ICON_LAYOUT_LINE_SIZE, "%08x\t%08x",
				 location->pageNo, location->pos);
	else
		sce_paf_snprintf(line, LSDB_ICON_LAYOUT_LINE_SIZE, "%08x\t%08x\t%08x\t%08x\t%s",
				 location->pageNo, location->pos, location->folderPageNo,
				 location->folderPos, location->folderTitle.c_str());
}

extern "C" {

/* a title's place; whether it has one */
bool SceLsdb_0746F1E9(const char *titleId, LsdbIconLocation *location)
{
	int result = SceLsdb_30900530();

	if (result == 0) {
		char line[LSDB_ICON_LAYOUT_LINE_SIZE] = "";

		result = s_iconLayout.get(titleId, line, sizeof(line), 0);
		if (result == 0)
			result = IconLayoutParse(line, location);
	}
	SceLsdb_E05F71CB();
	return result == 0;
}

int SceLsdb_2AB5A403(const char *titleId, const LsdbIconLocation *location)
{
	char line[LSDB_ICON_LAYOUT_LINE_SIZE];

	if (s_iconLayoutReadOnly)
		return 0;

	int result = SceLsdb_30900530();

	if (result == 0) {
		IconLayoutFormat(line, location);
		result = s_iconLayout.set(titleId, line);
	}
	SceLsdb_E05F71CB();
	return result;
}

int SceLsdb_2DF6EA05(const char *titleId)
{
	if (s_iconLayoutReadOnly)
		return 0;

	int result = SceLsdb_30900530();

	if (result == 0)
		result = s_iconLayout.remove(titleId, 0);
	SceLsdb_E05F71CB();
	return result;
}

/* the folder's place and title (each when not NULL) in the line of the folder folderId */
int SceLsdb_6E4B6EBF(int folderId, int *pageNo, int *pos, const char *title)
{
	char key[LSDB_ICON_LAYOUT_KEY_SIZE];
	char line[LSDB_ICON_LAYOUT_LINE_SIZE];

	if (s_iconLayoutReadOnly)
		return 0;

	int result = SceLsdb_30900530();

	if (result == 0) {
		for (;;) {
			result = s_iconLayout.next(key, line, sizeof(line));
			if (result == INI_RESULT_STREAM_END ||
			    result == SCE_INI_FILE_PROCESSOR_ERROR_EMPTY) {
				result = 0;
				break;
			}
			if (result != 0)
				break;

			LsdbIconLocation location;

			IconLayoutClear(&location);

			result = IconLayoutParse(line, &location);
			if (result != 0)
				break;
			if (folderId != location.pageNo)
				continue;
			if (pageNo != NULL)
				location.folderPageNo = *pageNo;
			if (pos != NULL)
				location.folderPos = *pos;
			if (title != NULL)
				location.folderTitle = title;
			IconLayoutFormat(line, &location);
			result = s_iconLayout.set(key, line);
			if (result != 0)
				break;
			result = s_iconLayout.next(key, line, sizeof(line));
			if (result != 0)
				break;
		}
	}
	SceLsdb_E05F71CB();
	return result;
}

/* the first icon from page pageNo on moved a page on (added) or back */
int SceLsdb_A92420DA(bool added, int pageNo)
{
	char key[LSDB_ICON_LAYOUT_KEY_SIZE];
	char line[LSDB_ICON_LAYOUT_LINE_SIZE];

	if (s_iconLayoutReadOnly)
		return 0;

	int result = SceLsdb_30900530();

	if (result == 0) {
		for (;;) {
			result = s_iconLayout.next(key, line, sizeof(line));
			if (result == INI_RESULT_STREAM_END ||
			    result == SCE_INI_FILE_PROCESSOR_ERROR_EMPTY) {
				result = 0;
				break;
			}
			if (result != 0)
				break;

			LsdbIconLocation location;

			IconLayoutClear(&location);

			result = IconLayoutParse(line, &location);
			if (result != 0)
				break;
			if (location.pageNo < pageNo)
				continue;
			if (added)
				location.pageNo = location.pageNo + 1;
			else
				location.pageNo = location.pageNo - 1;
			IconLayoutFormat(line, &location);
			result = s_iconLayout.set(key, line);
			if (result != 0)
				break;
			result = s_iconLayout.next(key, line, sizeof(line));
			if (result != 0)
				break;
		}
	}
	SceLsdb_E05F71CB();
	return result;
}

/* whether an icon is at position pos of page pageNo */
bool SceLsdb_74DDAE58(int pageNo, int pos)
{
	char key[LSDB_ICON_LAYOUT_KEY_SIZE];
	char line[LSDB_ICON_LAYOUT_LINE_SIZE];
	bool found = false;

	if (SceLsdb_30900530() == 0) {
		for (;;) {
			int result = s_iconLayout.next(key, line, sizeof(line));

			if (result == INI_RESULT_STREAM_END ||
			    result == SCE_INI_FILE_PROCESSOR_ERROR_EMPTY || result != 0)
				break;

			LsdbIconLocation location;

			IconLayoutClear(&location);

			if (IconLayoutParse(line, &location) != 0)
				break;
			if (location.pageNo == pageNo && location.pos == pos) {
				found = true;
				break;
			}
		}
	}
	SceLsdb_E05F71CB();
	return found;
}

/* the number of folders the file has lines for */
int SceLsdb_81759902(void)
{
	char key[LSDB_ICON_LAYOUT_KEY_SIZE];
	char line[LSDB_ICON_LAYOUT_LINE_SIZE];
	paf::vector<int> folderIds;

	if (SceLsdb_30900530() == 0) {
		for (;;) {
			int result = s_iconLayout.next(key, line, sizeof(line));

			if (result == INI_RESULT_STREAM_END ||
			    result == SCE_INI_FILE_PROCESSOR_ERROR_EMPTY || result != 0)
				break;

			LsdbIconLocation location;

			IconLayoutClear(&location);

			if (IconLayoutParse(line, &location) != 0)
				break;
			if (location.pageNo < LSDB_FOLDER_ID_MIN ||
			    location.pageNo >= LSDB_FOLDER_ID_END)
				continue;
			if (paf::find(folderIds.begin(), folderIds.end(), location.pageNo) !=
			    folderIds.end())
				folderIds.push_back(location.pageNo);
		}
	}
	SceLsdb_E05F71CB();
	return folderIds.size();
}

}

static void *IconLayoutAlloc(SceSize size, void *context)
{
	return sce_paf_malloc(size);
}

static void IconLayoutFree(void *ptr, void *context)
{
	sce_paf_free(ptr);
}
