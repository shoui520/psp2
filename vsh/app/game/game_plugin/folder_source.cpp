/*
 * The game plugin's source of an application folder with its sce_sys: the application's
 * param.sfo (verified against a digest given) and those of its related applications
 * (param2.sfo), each related record completed from the application's (0x8107A8C0..0x8107BEF0).
 * The file's name and the functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

/* Load calls the related records' push_back out of line (0x8141030C) */
PAF_VECTOR_NOINLINE_PUSH_BACK(LsdbAppInfoPtr, paf::allocator<LsdbAppInfoPtr>)

using namespace paf;

int GameFolderSource::SetDigest(const void *digest, SceSize size)
{
	if (digest == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;
	if ((int)size <= 0)
		return GAME_ERROR_INVALID_ARGUMENT;

	m_digest = new char[size];
	if (m_digest == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	sce_paf_memcpy(m_digest, digest, size);
	m_digestSize = size;
	return 0;
}

GameFolderSource::GameFolderSource(const GameFolderSourceParam &param)
	: GamePromoteSource(param), m_sysDir(param.sysDir), m_metaDir(param.metaDir),
	  m_appPath(param.appPath), m_upStatus(0), m_oldInfo(param.info), m_installed(false),
	  m_digest(NULL), m_digestSize(0)
{
}

GameFolderSource::~GameFolderSource()
{
	if (m_digest != NULL) {
		delete[] (char *)m_digest;
		m_digest = NULL;
	}
}

/*
 * What a related application takes from the application's record when its own has none: the
 * install folders, IDs and limits, the additional content folders, and the application's title
 * ID as the title it runs over (0x8107AAD8).
 */
__attribute__((noinline)) int GameFolderSource::CopyRelated(unsigned int index,
							    const LsdbAppInfoPtr *info,
							    LsdbAppInfoPtr *related)
{
	/* Sony's computes the keys from their names, once */
	static const unsigned int keys[] = {
		LsdbKeyHasher<20>::Hash("INSTALL_DIR_SAVEDATA"),
		LsdbKeyHasher<19>::Hash("INSTALL_DIR_ADDCONT"),
		LsdbKeyHasher<17>::Hash("SAVEDATA_MAX_SIZE"),
		LsdbKeyHasher<10>::Hash("CONTENT_ID"),
		LsdbKeyHasher<19>::Hash("NP_COMMUNICATION_ID"),
		LsdbKeyHasher<11>::Hash("REGION_DENY"),
		LsdbKeyHasher<18>::Hash("INSTALL_DIR_CACHE0"),
	};
	LsdbAppInfo::iterator end = (*info)->end();
	LsdbAppInfo::iterator relatedEnd = (*related)->end();
	unsigned int i;

	for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
		if ((*related)->find(keys[i]) != relatedEnd)
			continue;

		LsdbAppInfo::iterator it = (*info)->find(keys[i]);

		if (it == end)
			continue;

		switch (it->second.type) {
		case LSDB_VALUE_INTEGER:
			gameLsdbValueSetInteger((**related)[keys[i]], gameLsdbValueInteger(it->second));
			break;
		case LSDB_VALUE_TEXT:
			gameLsdbValueSetText((**related)[keys[i]], LsdbValueText(it->second));
			break;
		}
	}

	const unsigned int *addcontKeys = gameGetAddcontDirKeys();
	unsigned int count = gameGetAddcontDirKeyCount();

	for (i = 0; i < count; i++, addcontKeys++) {
		if ((*related)->find(*addcontKeys) != relatedEnd)
			continue;

		LsdbAppInfo::iterator it = (*info)->find(*addcontKeys);

		if (it != end)
			gameLsdbValueSetText((**related)[*addcontKeys], LsdbValueText(it->second));
	}

	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_TITLE_ID);

	if (it == end)
		return GAME_ERROR_NO_VALUE;

	gameLsdbValueSetText((**related)[LSDB_KEY_FG_TITLE_ID], LsdbValueText(it->second));
	return 0;
}

/*
 * A param.sfo read into info, verified against the digest given; its category must be a
 * game's, then its values are read with that category's parameters (0x8107AF38).
 */
__attribute__((noinline)) int GameFolderSource::LoadSfo(const string &path, LsdbAppInfoPtr *info)
{
	const GameSfoParam *params = NULL;
	unsigned int count = 0;
	int ret;

	if (m_installed && m_digest != NULL && m_digestSize != 0)
		ret = gameSfoLoadAppVerVerified(path, info, &params, &count,
						(const char *)m_digest, m_digestSize);
	else
		ret = gameSfoLoadAppVer(path, info, &params, &count);

	if (ret < 0) {
		gamePathGetMountedDrive(path, m_detail);
		return ret;
	}

	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_CATEGORY);

	if (it == (*info)->end())
		return GAME_ERROR_BAD_DATA;

	m_category = LsdbValueText(it->second);
	if (!gameCategoryIsGameName(m_category))
		return GAME_ERROR_UNKNOWN_CATEGORY;

	if (m_installed && m_digest != NULL && m_digestSize != 0)
		ret = gameSfoLoadVerified(path, params, count, info, (const char *)m_digest,
					  m_digestSize);
	else
		ret = gameSfoLoad(path, params, count, info);

	if (ret < 0) {
		gamePathGetMountedDrive(path, m_detail);
		return ret;
	}

	return 0;
}

/* the application's param.sfo checked, its folders, paths and promotion state (0x8107B09E) */
__attribute__((noinline)) int GameFolderSource::LoadMain(GameAppRegistration *reg)
{
	string path = m_sysDir + string("/param.sfo");
	int ret;

	ret = LoadSfo(path, &reg->info);
	if (ret < 0)
		return ret;

	ret = gameAppInfoCheckSfo(&reg->info);
	if (ret < 0)
		return ret;

	SetDirs(reg);

	ret = SetPaths(reg);
	if (ret < 0)
		return ret;

	ret = gameAppInfoSetPromoteAttr(m_flags, &reg->info);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * A related application's param.sfo checked as a patch's, its attributes taken from its
 * installed record, then its paths (0x8107B24E).
 */
__attribute__((noinline)) int GameFolderSource::LoadRelated(const string &path, unsigned int index,
							    LsdbAppInfoPtr *related)
{
	int ret;

	ret = LoadSfo(path, related);
	if (ret < 0)
		return ret;

	ret = gameAppInfoCheckPatchSfo(related);
	if (ret < 0)
		return ret;

	LsdbAppInfo::iterator it = (*related)->find(LSDB_KEY_TITLE_ID_SFO);

	if (it == (*related)->end())
		return GAME_ERROR_NO_VALUE;

	string titleId(LsdbValueText(it->second));
	LsdbAppInfoPtr app(new LsdbAppInfo);

	if (app.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	ret = gameAppGetInfo(titleId.c_str(), &app);
	if (ret < 0 && ret != (int)LSDB_ERROR_NOT_FOUND)
		return ret;

	SetRelatedAttributes(index, &app, related);

	ret = SetRelatedPaths(index, related);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * The application's information: its related application's param2.sfo when there is one, then
 * its own param.sfo, each related record completed from it, and the additional contents its
 * old record had checked against them (0x8107B526).
 */
int GameFolderSource::Load(GameAppRegistration *reg)
{
	LocalFileStat stat;
	unsigned int i;
	int ret;

	for (i = 0; i < 1; i++) {
		char name[32];

		sce_paf_snprintf(name, sizeof(name), "/param%u.sfo", i + 2);

		string path = m_sysDir + string(name);

		ret = LocalFile::Getstat(path.c_str(), &stat);
		if (ret == 0) {
			LsdbAppInfoPtr related(new LsdbAppInfo);

			if (related.get() == NULL)
				return GAME_ERROR_NO_INFO_MEMORY;

			reg->related.push_back(related);

			ret = LoadRelated(path, i + 2, &reg->related[i]);
			if (ret < 0)
				return ret;
		} else if (ret != GAME_PAF_ERROR_NOT_FOUND) {
			gamePathGetMountedDrive(path, m_detail);
			return ret;
		}
	}

	ret = LoadMain(reg);
	if (ret < 0)
		return ret;

	for (i = 0; i < reg->related.size(); i++) {
		ret = CopyRelated(i, &reg->info, &reg->related[i]);
		if (ret < 0)
			return ret;
	}

	ret = gameAddcontCheck(&m_oldInfo, &reg->info, &reg->related);
	if (ret < 0)
		return ret;

	return 0;
}

int GameFolderSource::SetRelatedAttributes(unsigned int index, const LsdbAppInfoPtr *app,
					   LsdbAppInfoPtr *related)
{
	LsdbAppInfo::iterator it = (*related)->find(LSDB_KEY_TITLE_ID_SFO);

	if (it == (*related)->end())
		return GAME_ERROR_NO_VALUE;

	string titleId(LsdbValueText(it->second));
	int ret = gameAppInfoSetPatchAttribute(app, related);

	if (ret < 0)
		return ret;

	gameAppInfoClearImportSaveDataInfo(related);
	return 0;
}

int GameFolderSource::SetRelatedPaths(unsigned int index, LsdbAppInfoPtr *related)
{
	LsdbAppInfo::iterator it = (*related)->find(LSDB_KEY_TITLE_ID_SFO);

	if (it == (*related)->end())
		return GAME_ERROR_NO_VALUE;

	string titleId(LsdbValueText(it->second));

	gameLsdbValueSetText((**related)[LSDB_KEY_TITLE_ID], titleId.c_str());
	gameLsdbValueSetText((**related)[LSDB_KEY_ORG_PATH], m_path.c_str());

	char name[32];

	sce_paf_snprintf(name, sizeof(name), "/eboot%u.bin", index);

	string selfPath = m_appPath + string(name);

	gameLsdbValueSetText((**related)[LSDB_KEY_SELF_PATH], selfPath);

	int kind = 1;

	gameAppInfoGetGameKind(related, titleId, &kind);
	gameLsdbValueSetInteger((**related)[LSDB_KEY_APPTYPE], (SceUInt32)kind);

	bool onUx0 = false;
	int ret = gameAppIsOnUx0(related, &onUx0);

	if (ret < 0)
		return ret;

	ret = gameAppInfoSetPromoteAttr(onUx0 ? GAME_SOURCE_FLAG_UX0 : 0, related);
	if (ret < 0)
		return ret;

	return 0;
}
