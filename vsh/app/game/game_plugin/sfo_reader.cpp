/*
 * A game card's param.sfo, signed in an SCE header, read and checked into an application's
 * information (0x810329E0..0x81032F64). The file's name and the functions' are ours (no record
 * names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"

using namespace paf;

/* the SDK's glue over SceVshBridge's import (0x81417D84) */
extern "C" int vshSblAuthMgrVerifySpsfo(const char *path, void *sfo, SceSize size);

/* an SCE header's magic, "SCE\0" read big-endian, and a signed param.sfo's header type */
#define GAME_SCE_HEADER_MAGIC		0x53434500
#define GAME_SCE_HEADER_TYPE_SPSFO	6

/* the largest param.sfo a signed one may hold */
#define GAME_SPSFO_MAX_SIZE		0x2000000

/* the categories a game card's application has: "gc..." */
#define GAME_CATEGORY_GC		"gc"

/* the largest system version a param.sfo may ask for */
#define GAME_SYSTEM_VER_MAX		0x99999000

/* an SCE header (the fields read) */
struct GameSceHeader {
	SceUInt32 magic;
	SceUInt32 version;
	SceUInt16 keyRevision;
	SceUInt16 headerType;
	SceUInt32 metadataOffset;
	SceUInt64 headerLength;
	SceUInt64 dataLength;
};

/* what follows a signed param.sfo's SCE header: where the param.sfo is, and its size */
struct GameSpsfoHeader {
	SceUInt32 reserved;
	SceUInt32 offset;
	SceUInt32 size;
};

/* 0x810329E0 */
GameSfoReader::GameSfoReader(LsdbAppInfoPtr *info) : m_info(info)
{
}

/* 0x810329F4, deleting 0x81032A02 */
GameSfoReader::~GameSfoReader()
{
}

/*
 * Whether the information is a game card application's: its category "gc...", a title ID, and
 * the versions it gives well formed (0x81032A18).
 */
int GameSfoReader::Check(LsdbAppInfoPtr *info)
{
	LsdbAppInfo::iterator end = (*info)->end();
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_CATEGORY);

	if (it == end)
		return GAME_ERROR_BAD_DATA;

	const char *category = LsdbValueText(it->second);

	if (category == NULL || category[0] == '\0')
		return GAME_ERROR_BAD_DATA;

	if (sce_paf_strncmp(category, GAME_CATEGORY_GC, sizeof(GAME_CATEGORY_GC)) != 0)
		return GAME_ERROR_BAD_DATA;

	it = (*info)->find(LSDB_KEY_TITLE_ID_SFO);
	if (it == end)
		return GAME_ERROR_BAD_DATA;

	const char *titleId = LsdbValueText(it->second);

	if (titleId == NULL || titleId[0] == '\0')
		return GAME_ERROR_BAD_DATA;

	it = (*info)->find(LSDB_KEY_DISP_VER);
	if (it != end && gameSystemVersionCheck(LsdbValueText(it->second)) < 0)
		return GAME_ERROR_BAD_DATA;

	it = (*info)->find(LSDB_KEY_VERSION);
	if (it != end && gameVersionCheck(LsdbValueText(it->second)) < 0)
		return GAME_ERROR_BAD_DATA;

	it = (*info)->find(LSDB_KEY_SYSTEM_VER);
	if (it != end && (SceUInt32)gameLsdbValueInteger(it->second) > GAME_SYSTEM_VER_MAX)
		return GAME_ERROR_BAD_DATA;

	it = (*info)->find(LSDB_KEY_SYSTEM_ROOT_VER);
	if (it != end && (SceUInt32)gameLsdbValueInteger(it->second) > GAME_SYSTEM_VER_MAX)
		return GAME_ERROR_BAD_DATA;

	return 0;
}

/*
 * The signed param.sfo read from path checked: its SCE header, and its signature by the
 * authentication manager; size becomes the param.sfo's (0x81032CC2).
 */
int GameSfoReader::Verify(const char *path, void *sfo, SceSize *size)
{
	const GameSceHeader *header = (const GameSceHeader *)sfo;
	SceUInt32 magic = header->magic;

	magic = (((magic >> 16) & 0xFF) << 8) | ((magic >> 24) & 0xFF) |
		(((magic >> 8) & 0xFF) << 16) | ((magic & 0xFF) << 24);
	if (magic != GAME_SCE_HEADER_MAGIC)
		return GAME_ERROR_BAD_SIGNED_SFO;

	if (header->headerType != GAME_SCE_HEADER_TYPE_SPSFO)
		return GAME_ERROR_BAD_SIGNED_SFO;

	SceUInt32 headerLength = (SceUInt32)header->headerLength;
	const GameSpsfoHeader *spsfo = (const GameSpsfoHeader *)((const char *)sfo + headerLength);

	if ((const char *)sfo + *size < (const char *)spsfo)
		return GAME_ERROR_BAD_SIGNED_SFO;

	SceSize sfoSize = *size - (headerLength + spsfo->offset);

	if (sfoSize > GAME_SPSFO_MAX_SIZE || spsfo->size != sfoSize)
		return GAME_ERROR_BAD_SIGNED_SFO;

	int ret = vshSblAuthMgrVerifySpsfo(path, sfo, sfoSize);

	if (ret < 0)
		return ret;

	*size = sfoSize;

	return 0;
}

/* The signed param.sfo at path read, checked and loaded into info (0x81032D64). */
int GameSfoReader::Read(const string &path, LsdbAppInfoPtr *info)
{
	int ret = 0;
	SceSize size = 0;
	SceUInt8 *sfo;
	common::SharedPtr<File> file = LocalFile::Open(path.c_str(), SCE_O_RDONLY, 0, &ret);

	if (file.get() == NULL)
		return ret;

	if (ret < 0)
		return ret;

	int fileSize = file->GetFileSize();

	size = fileSize;
	if (fileSize >= 0) {
		sfo = new SceUInt8[size];
		if (sfo != NULL) {
			ret = file->Read(sfo, size);
			if (ret >= 0) {
				file->Close();
				file.reset();

				ret = Verify(path.c_str(), sfo, &size);
				if (ret >= 0) {
					ret = gameSfoLoadParams(sfo, size, gameSfoGetVersionParams(),
								gameSfoGetVersionParamCount(), info,
								true);
					if (ret >= 0)
						ret = 0;
				}
			}
		} else {
			ret = GAME_ERROR_NO_INFO_MEMORY;
		}
	} else {
		sfo = NULL;
		ret = fileSize;
	}

	if (file.get() != NULL)
		file->Close();

	delete[] sfo;

	return ret;
}

/* The signed param.sfo at path read into the information, and checked (0x81032F44). */
int GameSfoReader::Load(const string &path)
{
	int ret = Read(path, m_info);

	if (ret < 0)
		return ret;

	ret = Check(m_info);
	if (ret < 0)
		return ret;

	return 0;
}
