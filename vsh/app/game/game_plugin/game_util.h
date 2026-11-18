/*
 * The game plugin's helpers of game_util.cpp: a dialog handler calling a function, the power
 * manager's callbacks and applauncher's text of an error. The names are ours (no record names
 * them).
 */
#ifndef _GAME_PLUGIN_GAME_UTIL_H
#define _GAME_PLUGIN_GAME_UTIL_H

#include <scetypes.h>
#include <common_gui_dialog/common_gui_dialog.h>

/*
 * A dialog handler that calls a function with the dialog's value and button and its two
 * arguments (constructor 0x81063A30).
 */
class GameDialogFuncHandler : public CommonGuiDialogHandler {
public:
	typedef void (*Func)(SceInt32 value, SceInt32 button, void *arg1, void *arg2);

	GameDialogFuncHandler(Func func, void *arg1, void *arg2);

	virtual void operator()(SceInt32 value, SceInt32 button);

	Func m_func;
	void *m_arg1;
	void *m_arg2;
};

/* func called with arg on the power events, and no longer (0x81063A60, 0x81063A8C) */
int gamePowerManageAddCallback(void (*func)(void *arg), void *arg);
int gamePowerManageRemoveCallback(void (*func)(void *arg), void *arg);

/* applauncher's text of an error the game plugin gives (0x81063AB8) */
SceUInt32 gameErrorGetMessageId(int error);

#endif /* _GAME_PLUGIN_GAME_UTIL_H */
