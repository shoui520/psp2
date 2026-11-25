/*
 * SceFriendList2DialogPlugin: SceFriendSelect embedded plugin
 * ("friend_select_plugin", "vs0:data/external/cdlg/friend_select_plugin.rco").
 */
#ifndef FRIENDLIST2_DIALOG_FRIEND_SELECT_PLUGIN_H
#define FRIENDLIST2_DIALOG_FRIEND_SELECT_PLUGIN_H

#include <stdint.h>
#include <scetypes.h>
#include <plugin.h>

/* Interface table published by friend_select_plugin via Plugin::SetInterface(1, ...) */
struct FriendSelectInterface {
	void (*abort)(void);
};

/* PAF plugin lifecycle hooks passed in Plugin::InitParam */
void FriendSelectSetParam(paf::Plugin::InitParam2 *param);
void FriendSelectInit(paf::Plugin *plugin);
void FriendSelectStart(paf::Plugin *plugin);
void FriendSelectStop(paf::Plugin *plugin);
void FriendSelectExit(paf::Plugin *plugin);

#endif /* FRIENDLIST2_DIALOG_FRIEND_SELECT_PLUGIN_H */
