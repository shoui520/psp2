/*
 * applauncher_ctxmode.cpp: AppTabContextMenuMode, the mode of an icon's context menu
 * (0x810C8A84-0x810C8D96). The class is Sony's; the file's name and the functions' and
 * variables' names are ours (PLACEHOLDER).
 */
#include "applauncher.h"
#include "applauncher_home.h"
#include "applauncher_mode.h"
#include "../../../../game/game_plugin/game_plugin.h"

/* (0x815442B8) */
const char *AppTabContextMenuMode::s_className = "AppTabContextMenuMode";

/* PLACEHOLDER: the icon whose context menu the mode shows (0x815559F0), and whether the game
   plugin's dialog hold stays on when the mode ends (0x815559F8) */
static paf::common::SharedPtr<ApplauncherIcon> s_contextMenuIcon;
static bool s_keepHold;

/* (0x810C8A84) */
const char *AppTabContextMenuMode::GetName()
{
	return s_className;
}

/* the icon's context menu opened with the mode, the game plugin's dialogs held (0x810C8A90) */
AppTabContextMenuMode::AppTabContextMenuMode()
{
	applauncherContextMenuOpen(s_contextMenuIcon);
	s_keepHold = false;

	GamePluginInterface *game =
		(GamePluginInterface *)applauncherPluginGetInterface("game_plugin", 1);

	if (game != NULL)
		((void (*)(bool))game->func[GAME_PLUGIN_FUNC_SET_DIALOG_HOLD])(true);
}

/* closed with it, the icon let go of and the hold ended unless asked to stay (0x810C8B6A,
   0x810C8C2E) */
AppTabContextMenuMode::~AppTabContextMenuMode()
{
	applauncherContextMenuClose();
	s_contextMenuIcon.reset();

	GamePluginInterface *game =
		(GamePluginInterface *)applauncherPluginGetInterface("game_plugin", 1);

	if (game != NULL && !s_keepHold)
		((void (*)(bool))game->func[GAME_PLUGIN_FUNC_SET_DIALOG_HOLD])(false);
}

/* (0x810C8C3E) */
void applauncherContextMenuEnter(const paf::common::SharedPtr<ApplauncherIcon> &icon)
{
	s_contextMenuIcon = icon;
	g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_CONTEXT_MENU);
}

/* (0x810C8CF8) */
void applauncherContextMenuKeepHold(bool keep)
{
	s_keepHold = keep;
}
