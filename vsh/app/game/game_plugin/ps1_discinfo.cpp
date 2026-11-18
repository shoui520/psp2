/*
 * A PlayStation game's multi-disc signature made from the PSP emulator's disc list, for a game
 * without its own __sce_discinfo (0x81034650..0x810349D4). The file's name and the class's are
 * ours (no record names them); the pool's name is Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <file/localfile.h>
#include <memory/pool_allocator.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"

using namespace paf;

/* the SDK's glue over SceVshBridge's imports (0x81417D50, 0x81417D64) */
extern "C" {
int vshNpDrmEbootSigGenMultiDisc(const char *ebootPath, const void *discInfo, void *signature,
				 int swVersion);
int vshNpDrmEbootSigVerify(const char *ebootPath, const void *signature);
}

/* the characters of an entry's product code compared with the title ID */
#define GAME_DISC_INFO_CODE_LEN	10

/* 0x81034650 */
GamePs1DiscInfoParser::GamePs1DiscInfoParser()
	: m_discInfo(NULL), m_size(0), m_error(0), m_count(0)
{
	int ret = gamePluginGetPool("SceGamePs1DiscInfoParser", &m_pool);

	if (ret < 0) {
		m_error = ret;
		return;
	}

	m_size = m_pool->GetFreeSize();
	m_discInfo = (GameDiscInfo *)m_pool->Alloc();
	if (m_discInfo == NULL)
		m_error = GAME_ERROR_NO_MEMORY;
}

/* 0x810346AE, deleting 0x81034722 */
GamePs1DiscInfoParser::~GamePs1DiscInfoParser()
{
	if (m_discInfo != NULL)
		m_pool->Free(m_discInfo);

	gamePluginReleasePool(&m_pool);
}

/* The disc list at path read, whole entries that fit the pool's block (0x81034732). */
int GamePs1DiscInfoParser::Load(const string &path)
{
	if (m_error < 0)
		return m_error;

	int ret = 0;
	common::SharedPtr<File> file = LocalFile::Open(path.c_str(), SCE_O_RDONLY, 0, &ret);

	if (ret < 0)
		return ret;

	SceInt64 size = file->GetFileSize();

	if (size < 0) {
		ret = (int)size;
	} else if ((SceUInt64)size > m_size || ((SceUInt32)size % sizeof(GameDiscInfo)) != 0) {
		ret = GAME_ERROR_NO_MEMORY;
	} else {
		m_count = (SceUInt32)size / sizeof(GameDiscInfo);
		ret = file->Read(m_discInfo, (SceSize)size);
		if (ret >= 0) {
			file->Close();
			return 0;
		}
	}

	file->Close();

	return ret;
}

/*
 * The signature of the EBOOT made from the first entry of the game's title ID it is made with,
 * and checked (0x810348E0).
 */
int GamePs1DiscInfoParser::Generate(const SceNpDrmInstallInfo *info, const string &ebootPath,
				    void *signature)
{
	if (m_discInfo == NULL)
		return GAME_ERROR_NO_MEMORY;

	for (SceUInt32 i = 0; i < m_count; i++) {
		if (sce_paf_strncmp((const char *)m_discInfo[i].data, info->info.titleId,
				    GAME_DISC_INFO_CODE_LEN) != 0)
			continue;

		if (vshNpDrmEbootSigGenMultiDisc(ebootPath.c_str(), &m_discInfo[i], signature,
						 info->swVersion) < 0)
			continue;

		int ret = vshNpDrmEbootSigVerify(ebootPath.c_str(), signature);

		if (ret >= 0)
			return 0;

		return ret;
	}

	return GAME_ERROR_NO_DISC_INFO;
}
