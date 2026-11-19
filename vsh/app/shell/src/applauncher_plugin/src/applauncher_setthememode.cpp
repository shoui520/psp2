/*
 * applauncher_setthememode.cpp: AppTabSetThemeMode, the mode of a page's theme chooser
 * (0x810C94DC-0x810C976C). The class is Sony's; the file's name and the functions' and
 * variables' names are ours (PLACEHOLDER).
 */
#include "applauncher.h"
#include "applauncher_home.h"
#include "applauncher_mode.h"
#include "../../../plugins.h"
#include "../../../../game/game_plugin/game_plugin.h"

/* PLACEHOLDER: the chooser's picks under way (0x815442D4) */
static int s_busyCount = 0;

/* (0x815442D8) */
const char *AppTabSetThemeMode::s_className = "AppTabSetThemeMode";

/* (0x810C94DC) */
const char *AppTabSetThemeMode::GetName()
{
	return s_className;
}

static void onSetThemePowerEvent(void *arg);

/* the chooser made with the mode, the game plugin's dialogs and topmenu held, the overlay
   faded out and the power events listened to (0x810C94E8) */
AppTabSetThemeMode::AppTabSetThemeMode()
{
	applauncherThemeSelectorCreate();

	GamePluginInterface *game =
		(GamePluginInterface *)applauncherPluginGetInterface("game_plugin", 1);

	if (game != NULL)
		((void (*)(bool))game->func[GAME_PLUGIN_FUNC_SET_DIALOG_HOLD])(true);
	applauncherHomeHideOverlay();
	((void (*)(void))g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_HOLD_BEGIN])();
	((void (*)(void))g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_GAME_HOLD])();

	ShellPluginInterface *power =
		(ShellPluginInterface *)applauncherPluginGetInterface("power_manage_plugin", 1);

	if (power != NULL)
		((int (*)(void (*)(void *), void *))power->func[POWER_MANAGE_PLUGIN_FUNC_ADD_CALLBACK])(
			onSetThemePowerEvent, NULL);
	s_busyCount = 0;
}

/* all of it undone with the mode, the chosen page set and saved, and the overlay faded in
   (0x810C9568, 0x810C9690) */
AppTabSetThemeMode::~AppTabSetThemeMode()
{
	ShellPluginInterface *power =
		(ShellPluginInterface *)applauncherPluginGetInterface("power_manage_plugin", 1);

	if (power != NULL)
		((int (*)(void (*)(void *), void *))
			power->func[POWER_MANAGE_PLUGIN_FUNC_REMOVE_CALLBACK])(onSetThemePowerEvent, NULL);
	((void (*)(void))g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_GAME_RELEASE])();
	((void (*)(void))g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_HOLD_END])();

	GamePluginInterface *game =
		(GamePluginInterface *)applauncherPluginGetInterface("game_plugin", 1);

	if (game != NULL)
		((void (*)(bool))game->func[GAME_PLUGIN_FUNC_SET_DIALOG_HOLD])(false);
	applauncherThemeSelectorDestroy();

	ApplauncherThemePage chosen;

	if (applauncherThemeSelectorGetPage(&chosen)) {
		LsdbPage param;

		param.themeFile = chosen.themeFile;
		param.bgColor = chosen.bgColor;
		param.texWidth = chosen.texWidth;
		param.texHeight = chosen.texHeight;
		param.imageWidth = chosen.imageWidth;
		param.imageHeight = chosen.imageHeight;
		param.reserved01 = chosen.reserved01;
		applauncherHomeSetPageParam(&param);
		applauncherPagesCleanTabpaper();
	}
	applauncherHomeShowOverlay();
}

/* (0x810C96A0) */
void applauncherBusyBegin(void)
{
	s_busyCount++;
}

/* (0x810C96B0) */
void applauncherBusyEnd(void)
{
	s_busyCount--;
}

/* a power event: back to the edit, or while a pick is under way the impose's zoom refused
   (0x810C96C0) */
static void onSetThemePowerEvent(void *arg)
{
	if (s_busyCount == 0) {
		g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_EDIT);
	} else {
		ImposePluginInterface *impose =
			(ImposePluginInterface *)applauncherPluginGetInterface("impose_plugin", 1);

		if (impose != NULL)
			impose->zoomRefuse();
	}
}
