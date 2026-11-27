/*
 * The game plugin's source of a license's package: the license read from the package, then
 * written into ux0:license/app/<title ID> once installed (or, for a free trial's package without
 * one, its free trial license installed), and the package removed (0x81084D10..0x8108521C). The
 * file's name and the functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

GameLicenseInstallSource::GameLicenseInstallSource(const GameInstallParam *param, bool noLicense,
						   string *errorTitleId)
	: m_param(param), m_noLicense(noLicense), m_license(NULL), m_licenseSize(0),
	  m_errorTitleId(errorTitleId)
{
}

GameLicenseInstallSource::~GameLicenseInstallSource()
{
	delete m_license;
	gameRemoveDirectory("ur0:appmeta/new", NULL, NULL, NULL, true);
	gameRemoveDirectoryContents("ur0:temp/appmeta", NULL, NULL, NULL, true);
}

int GameLicenseInstallSource::Install(const string *path, const string *titleId, string *dir,
				      string *installPath, GameJobProgress *progress, int cancel,
				      int cancelArg, float scale)
{
	int ret;

	gameRemoveDirectory("ux0:temp/promote", NULL, NULL, NULL, true);

	if (!m_noLicense) {
		ret = gameRifRead(*path, &m_license, &m_licenseSize);
		if (ret < 0) {
			gamePathGetMountedDrive(*path, m_errorTitleId);
			return ret;
		}
	}

	dir->clear();
	*installPath = "ux0:" + string("license/app/") + *titleId;
	return 0;
}

int GameLicenseInstallSource::Complete(string *dir, string *installPath, int progress,
				       int cancel, int cancelArg, float scale)
{
	string titleId(m_param->titleId);
	int ret;

	if (m_noLicense) {
		{
			string path(m_param->path);

			ret = gameFttInstall(&path, titleId);
		}
		if (ret < 0)
			return ret;
	} else {
		string path;

		ret = gameLicenseWrite(GAME_LICENSE_CONTENT_APP, m_license, m_licenseSize,
				       installPath, &path, m_errorTitleId);
		if (ret < 0)
			return ret;

		gameFttRemove(titleId);
	}

	LocalFile::RemoveFile(m_param->path);
	gameRemoveDirectory("ux0:temp/promote", NULL, NULL, NULL, true);
	return 0;
}
