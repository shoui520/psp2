/*
 * The game plugin's source of an application's information for a promotion: the folders and the
 * name it is read from, and the leftovers of an earlier install cleared from the temporary
 * folders of the drives it can come from (0x8107A3B4..0x8107A8C0). The file's name and the
 * functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

/*
 * Without a name, the title ID the path's last part; then the temporary folders of the drives
 * an application on m_path's drive can be installed through emptied, all but the one read from
 * (0x8107A3B4).
 */
__attribute__((noinline)) void GamePromoteSource::Init(const GameAppSourceParam &param)
{
	if (m_titleId.length() == 0) {
		unsigned int pos = param.path.rfind("/", string::npos, sce_paf_strlen("/"));

		m_titleId = param.path.substr(pos + 1, param.path.length() - pos);
		m_folderName = m_titleId;
	}

	if (m_path.rfind("gro0:", string::npos, sce_paf_strlen("gro0:")) != string::npos ||
	    m_path.rfind("grw0:", string::npos, sce_paf_strlen("grw0:")) != string::npos) {
		if (m_dir.find("grw0:temp/game", 0, sce_paf_strlen("grw0:temp/game")) == string::npos)
			gameRemoveDirectory("grw0:temp/game", 0, 0, 0, true);
		if (m_dir.find("ur0:temp/game", 0, sce_paf_strlen("ur0:temp/game")) == string::npos)
			gameRemoveDirectory("ur0:temp/game", 0, 0, 0, true);
		if (m_dir.find("ux0:temp/game", 0, sce_paf_strlen("ux0:temp/game")) == string::npos)
			gameRemoveDirectoryContents("ux0:temp/game", 0, 0, 0, true);
	} else if (m_path.rfind("vs0:", string::npos, sce_paf_strlen("vs0:")) != string::npos ||
		   m_path.rfind("ux0:", string::npos, sce_paf_strlen("ux0:")) != string::npos) {
		if (m_dir.find("ur0:temp/game", 0, sce_paf_strlen("ur0:temp/game")) == string::npos)
			gameRemoveDirectory("ur0:temp/game", 0, 0, 0, true);
		if (m_dir.find("ux0:temp/game", 0, sce_paf_strlen("ux0:temp/game")) == string::npos)
			gameRemoveDirectoryContents("ux0:temp/game", 0, 0, 0, true);
	}
}

GamePromoteSource::GamePromoteSource(const GameAppSourceParam &param)
	: m_ready(false), m_dir(param.dir), m_path(param.path), m_name(param.name),
	  m_option(param.options[2]), m_titleId(param.name), m_folderName(param.name), m_flags(0),
	  m_detail(NULL)
{
	m_options[0] = param.options[0];
	m_options[1] = param.options[1];
	Init(param);
}

GamePromoteSource::~GamePromoteSource()
{
}

int GamePromoteSource::SetInfo(GameAppRegistration *reg)
{
	int ret = Load(reg);

	if (ret < 0)
		return ret;

	return 0;
}
