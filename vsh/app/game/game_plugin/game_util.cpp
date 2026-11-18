/*
 * The game plugin's helpers: a dialog handler calling a function, the power manager's callbacks
 * and applauncher's text of an error (0x81063A30..0x81063BD0). The file's name and the
 * functions' are ours (no record names them).
 */

#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>

#include "../../shell/plugins.h"
#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_util.h"

using namespace paf;

/* the icons the home screen takes, and the counts past which its text of the error changes */
#define GAME_UTIL_ICONS_FIRST		100
#define GAME_UTIL_ICONS_MAX		500

/*
 * applauncher's texts (RCO hashes; PLACEHOLDER: the labels are not known): of too many icons, at
 * the first count, between it and the most and at the most, and of an error the table does not
 * name
 */
#define GAME_MSG_TOO_MANY_ICONS_FIRST	0x4502AFC3
#define GAME_MSG_TOO_MANY_ICONS		0x9B75504E
#define GAME_MSG_TOO_MANY_ICONS_MAX	0xB7077687
#define GAME_MSG_ERROR			0xC51AA4A4

/* an error and applauncher's text of it */
struct GameErrorMessage {
	int error;
	SceUInt32 messageId;
};

/* the errors applauncher has a text of (the second 0x80800004 is never reached) */
static const GameErrorMessage s_errorMessages[] = {
	{ (int)0x8010112C, 0x338DFC50 },	/* GAME_ERROR_NO_UX0 */
	{ (int)0x80101138, 0x888126CA },	/* GAME_ERROR_INSTALL_TITLE */
	{ (int)0x80101135, 0x94C9A0BA },	/* GAME_ERROR_STORAGE_CONFLICT */
	{ (int)0x80101136, 0x4C148EAD },	/* GAME_ERROR_INSTALLED_ON_UX0 */
	{ (int)0x80101137, 0x43562173 },
	{ (int)0x80101114, 0xC29F0C5C },	/* GAME_ERROR_APP_RUNNING */
	{ (int)0x80800004, 0x33045A31 },	/* GAME_LICENSE_ERROR_NOT_FOUND */
	{ (int)0x80101112, 0xE2A2DDCC },	/* GAME_ERROR_NO_APPMETA_SPACE */
	{ (int)0x80101118, 0x26FCB046 },	/* GAME_ERROR_GC_PLAIN */
	{ (int)0x80870009, 0xC1223B2B },
	{ (int)0x80010002, 0xC8FAD0B6 },	/* SCE_ERROR_ERRNO_ENOENT */
	{ (int)0x8080000D, 0xC8FAD0B6 },
	{ (int)0x80800015, 0xC8FAD0B6 },	/* SCE_APPMGR_ERROR_INVALID_PARAM_SFO */
	{ (int)0x80870006, 0xC1223B2B },	/* GAME_NPDRM_ERROR_EXPIRED */
	{ (int)0x80870007, 0xC1223B2B },	/* GAME_NPDRM_ERROR_NOT_STARTED */
	{ (int)0x80800004, 0x1DF465B7 },
	{ (int)0x80800010, 0x1DF465B7 },
	{ (int)0x80870004, 0x1DF465B7 },
	{ (int)0x80870003, 0x1DF465B7 },
	{ (int)0x80870005, 0x1DF465B7 },
	{ (int)0x80870008, 0x3CD248DC },	/* GAME_LICENSE_ERROR_NO_ACCOUNT */
	{ (int)0x80101147, 0xBB293247 },	/* GAME_ERROR_GC_LICENSE_EXPIRED */
};

GameDialogFuncHandler::GameDialogFuncHandler(Func func, void *arg1, void *arg2)
	: m_func(func), m_arg1(arg1), m_arg2(arg2)
{
}

void GameDialogFuncHandler::operator()(SceInt32 value, SceInt32 button)
{
	if (m_func != NULL)
		m_func(value, button, m_arg1, m_arg2);
}

int gamePowerManageAddCallback(void (*func)(void *arg), void *arg)
{
	Plugin *plugin = Plugin::Find("power_manage_plugin");

	if (plugin == NULL)
		return 0;

	ShellPluginInterface *iface = (ShellPluginInterface *)plugin->GetInterface(1);

	if (iface == NULL)
		return 0;

	return ((int (*)(void (*)(void *), void *))
			iface->func[POWER_MANAGE_PLUGIN_FUNC_ADD_CALLBACK])(func, arg);
}

int gamePowerManageRemoveCallback(void (*func)(void *arg), void *arg)
{
	Plugin *plugin = Plugin::Find("power_manage_plugin");

	if (plugin == NULL)
		return 0;

	ShellPluginInterface *iface = (ShellPluginInterface *)plugin->GetInterface(1);

	if (iface == NULL)
		return 0;

	return ((int (*)(void (*)(void *), void *))
			iface->func[POWER_MANAGE_PLUGIN_FUNC_REMOVE_CALLBACK])(func, arg);
}

SceUInt32 gameErrorGetMessageId(int error)
{
	if (error == GAME_ERROR_TOO_MANY_ICONS) {
		AppInfoSClient client;

		SceLsdb_D322A870(&client);

		int icons = SceLsdb_4DB9BCB4(&client);
		SceUInt32 messageId = GAME_MSG_TOO_MANY_ICONS_FIRST;

		if (icons != GAME_UTIL_ICONS_FIRST) {
			if (icons > GAME_UTIL_ICONS_FIRST && icons < GAME_UTIL_ICONS_MAX)
				messageId = GAME_MSG_TOO_MANY_ICONS;
			else if (icons == GAME_UTIL_ICONS_MAX)
				messageId = GAME_MSG_TOO_MANY_ICONS_MAX;
		}

		SceLsdb_56D8DCF0(&client);

		return messageId;
	}

	for (unsigned int i = 0; i < sizeof(s_errorMessages) / sizeof(s_errorMessages[0]); i++) {
		if (error == s_errorMessages[i].error)
			return s_errorMessages[i].messageId;
	}

	return GAME_MSG_ERROR;
}
