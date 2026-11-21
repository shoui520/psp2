/*
 * impose_shell_event.h: impose_plugin's dialogs for the shell's events (impose_shell_event.cpp).
 */
#ifndef IMPOSE_SHELL_EVENT_H
#define IMPOSE_SHELL_EVENT_H

#include <scetypes.h>
#include <plugin.h>

struct CommonGuiDialogLayout;

/*
 * The shell's events impose_plugin answers with a dialog: one asking (yes or no) on event 4,
 * taken back on event 7, one telling on event 6, both of which go on with SceShellUtil_636544FB;
 * and the dialog shown for an application quit by its name. Events 12 to 16 are passed on to
 * SceShellUtil_BB54D049. The file's and the names are ours (PLACEHOLDER).
 */
class ImposeShellEvent {
public:
	ImposeShellEvent(paf::Plugin *plugin);
	~ImposeShellEvent();
	/* the event callbacks registered, and taken off */
	void Start();
	void Stop();
	/* the dialog of the text into the ID, unless one is open */
	void OpenDialog(SceInt32 *id, SceInt32 dimmer, CommonGuiDialogLayout layout,
			SceUInt32 textId);
	/* the application of the name quit, with its dialog */
	void QuitApp(const char *name);
	/* a dialog's button */
	void OnDialogButton(SceInt32 id, SceInt32 button);
	/* a shell event with its data */
	void OnEvent(int event, int *data);

	paf::Plugin *m_plugin;
	SceInt32 m_askDialog;
	SceInt32 m_noticeDialog;
	SceInt32 m_quitDialog;
};

#endif
