/*
 * game_plugin.h: the game plugin, a module of its own in Sony's early builds
 * (vs0:vsh/game/game_plugin) and linked into SceShell since. The names are ours (no record
 * names them).
 */
#ifndef _GAME_PLUGIN_H
#define _GAME_PLUGIN_H

#include <plugin.h>

/*
 * PLACEHOLDER: the plugin's GetInterface(1) table, 65 functions (0x815431FC), each
 * written out with the plugin's code.
 */
struct GamePluginInterface {
	void (*func[65])(void);
};
extern GamePluginInterface gamePluginInterface;

/* the plugin's main object, made at start and deleted at stop (0x8102B296, 0x8102B2BA) */
void gamePluginCreate(paf::Plugin *plugin);
void gamePluginDestroy(void);

#endif
