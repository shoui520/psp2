/*
 * A theme's theme.xml read for its registration: from its package mounted with its license, or,
 * for a theme already extracted that does not mount, an empty one (0x81033FA4..0x81034650). The
 * file's name and the class's are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <framework.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "../../shell/theme.h"
#include "game_plugin.h"
#include "game_appinfo.h"

using namespace paf;

/* where a theme's license is installed */
#define GAME_THEME_LICENSE_DIR	"ux0:license/theme/"

/* 0x81033FA4 */
GameThemeReader::GameThemeReader(SceNpDrmInstallInfo *info, const string *themeId,
				 const string *packagePath, const string *installPath,
				 bool extracted)
	: m_info(info), m_themeId(*themeId), m_packagePath(*packagePath),
	  m_installPath(*installPath), m_extracted(extracted)
{
}

/* 0x81034010, deleting 0x8103408C */
GameThemeReader::~GameThemeReader()
{
}

/*
 * The size the theme takes: its package's folder, its license and the cluster the license's
 * file takes (0x8103409C).
 */
int GameThemeReader::GetSize(SceSize *size, GameCancelFunc cancel, void *arg)
{
	*size = 0;

	SceUInt64 dirSize = 0;
	int ret = gameDirGetSize(m_packagePath.c_str(), cancel, arg, &dirSize);

	if (ret < 0)
		return ret;

	*size = (SceSize)dirSize;
	*size += m_info->licenseSize;

	string licensePath = string(GAME_THEME_LICENSE_DIR) + m_themeId;
	SceUInt64 clusterSize = 0;

	ret = gameDeviceGetClusterSize(licensePath.c_str(), &clusterSize);
	if (ret < 0)
		return ret;

	*size += (SceSize)clusterSize;

	return ret;
}

/* The mounted theme's theme.xml read into info (0x8103421A). */
int GameThemeReader::LoadInfo(common::SharedPtr<LsdbThemeInfo> *info)
{
	ShellThemeReader reader(m_mountPoint.c_str(), Framework::Instance()->GetLocale());

	reader.SetLoadOption(true);

	int ret = reader.Load();

	if (ret < 0)
		return ret;

	*info = reader.GetThemeInfo();
	if (info->get() == NULL)
		return GAME_ERROR_NOT_READY;

	return ret;
}

/* An extracted theme that does not mount registered with an empty theme.xml (0x81034316). */
int GameThemeReader::ReadEmpty(LsdbTheme *theme, common::SharedPtr<LsdbThemeInfo> *info,
			       GameCancelFunc cancel, void *arg)
{
	SceSize size = 0;
	int ret;

	*info = common::SharedPtr<LsdbThemeInfo>(new LsdbThemeInfo);

	ret = GAME_ERROR_NOT_READY;
	if (info->get() != NULL)
		ret = GetSize(&size, cancel, arg);

	theme->id = m_installPath;
	theme->size = size;
	theme->type = LSDB_THEME_TYPE_CUSTOM;

	return ret;
}

/*
 * The theme's package mounted, its theme.xml read into info and its entry into theme, and
 * unmounted (0x810344AE). What fails before the unmount is not returned.
 */
int GameThemeReader::Read(LsdbTheme *theme, common::SharedPtr<LsdbThemeInfo> *info,
			  GameCancelFunc cancel, void *arg)
{
	SceSize size = 0;

	if (gameThemeMount(m_themeId, m_packagePath, m_info->license, m_info->licenseSize,
			   &m_mountPoint) < 0) {
		if (m_extracted) {
			ReadEmpty(theme, info, cancel, arg);
			return 0;
		}
	} else if (LoadInfo(info) >= 0 && GetSize(&size, cancel, arg) >= 0) {
		theme->id = m_installPath;
		theme->type = LSDB_THEME_TYPE_CUSTOM;
		theme->packageImageFilePath = (*info)->m_infomationProperty.m_packageImageFilePath;
		theme->homePreviewFilePath = (*info)->m_infomationProperty.m_homePreviewFilePath;
		theme->startPreviewFilePath = (*info)->m_infomationProperty.m_startPreviewFilePath;
		theme->contentVer = (*info)->m_infomationProperty.m_contentVer;
		theme->titleDefault = (*info)->m_infomationProperty.m_title.m_default;
		theme->providerDefault = (*info)->m_infomationProperty.m_provider.m_default;
		theme->lastModifiedTime = (*info)->m_lastModifiedTime;
		theme->size = size;
	}

	int ret = 0;

	if (m_mountPoint.length() != 0) {
		ret = gameThemeUnmount(&m_mountPoint);
		m_mountPoint.assign(NULL, 0);
	}

	return ret;
}
