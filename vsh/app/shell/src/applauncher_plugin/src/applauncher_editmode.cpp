/*
 * applauncher_editmode.cpp: AppTabEditMode, the mode of the home's edit (0x810C8D98-0x810C8FD2).
 * The class is Sony's; the file's name and the functions' and variables' names are ours
 * (PLACEHOLDER).
 */
#include "applauncher.h"
#include "applauncher_home.h"
#include "applauncher_mode.h"
#include "../../../shell.h"

/* PLACEHOLDER: the deletions under way (0x815442BC) */
static int s_holdCount = 0;

/* (0x815442C0) */
const char *AppTabEditMode::s_className = "AppTabEditMode";

/* (0x810C8D98) */
const char *AppTabEditMode::GetName()
{
	return s_className;
}

static void onEditPowerEvent(void *arg);

/* the home's edit entered with the mode, the mode given to topmenu, and the power events and
   shell event 10 listened to (0x810C8DA4) */
AppTabEditMode::AppTabEditMode()
{
	g_applauncherHome->EnterEditMode();
	((void (*)(ModeBase *, int, int, int, int))
		g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_MODE_ENTER])(
		this, g_applauncherPlugin->m_draw_priority, 0, 0, 0);

	ShellPluginInterface *power =
		(ShellPluginInterface *)applauncherPluginGetInterface("power_manage_plugin", 1);

	if (power != NULL)
		((int (*)(void (*)(void *), void *))power->func[POWER_MANAGE_PLUGIN_FUNC_ADD_CALLBACK])(
			onEditPowerEvent, NULL);
	SceShell_7FE4E732(10, (ShellEventCallback)applauncherInterfaceClearRequest, NULL);
}

/* and all of it undone in the other order (0x810C8E20, 0x810C8E88) */
AppTabEditMode::~AppTabEditMode()
{
	SceShell_D4C7D2B7(10, (ShellEventCallback)applauncherInterfaceClearRequest, NULL);

	ShellPluginInterface *power =
		(ShellPluginInterface *)applauncherPluginGetInterface("power_manage_plugin", 1);

	if (power != NULL)
		((int (*)(void (*)(void *), void *))
			power->func[POWER_MANAGE_PLUGIN_FUNC_REMOVE_CALLBACK])(onEditPowerEvent, NULL);
	((void (*)(ModeBase *, int))
		g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_MODE_LEAVE])(this, 230);
	g_applauncherHome->ExitEditMode();
}

/* (0x810C8EF4) */
void applauncherEditModeHold(void)
{
	s_holdCount++;
}

/* (0x810C8F04) */
void applauncherEditModeRelease(void)
{
	s_holdCount--;
}

/* a power event: the edit left (an icon's information back to the edit), or while a deletion
   is under way the impose's zoom refused (0x810C8F14) */
static void onEditPowerEvent(void *arg)
{
	if (s_holdCount == 0) {
		if (g_applauncherModeManager->m_currentMode == APPLAUNCHER_MODE_ICON_INFO)
			g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_EDIT);
		else
			g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_LAUNCHER);
	} else {
		ImposePluginInterface *impose =
			(ImposePluginInterface *)applauncherPluginGetInterface("impose_plugin", 1);

		if (impose != NULL)
			impose->zoomRefuse();
	}
}
