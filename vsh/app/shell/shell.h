/*
 * shell.h: SceShell's exports (link names SceShell_<NID>: no Sony name is known for them) and
 * what its files share. Names ours.
 */
#ifndef _SHELL_SHELL_H
#define _SHELL_SHELL_H

#include <scetypes.h>

/* the shell events boot.cpp listens to (names ours, from what the listeners do) */
enum {
	SHELL_EVENT_POWER	= 0,	/* the system resumed */
	SHELL_EVENT_MODEL	= 3,
};

/* what a shell event listener is called with: the event's code and its data */
typedef void (*ShellEventCallback)(int code, const void *data, void *arg);

#ifdef __cplusplus
extern "C" {
#endif

/* add and remove a listener of a shell event */
int SceShell_7FE4E732(int event, ShellEventCallback callback, void *arg);
int SceShell_E3D3A162(int event, ShellEventCallback callback, void *arg);
int SceShell_D4C7D2B7(int event, ShellEventCallback callback, void *arg);

#ifdef __cplusplus
}
#endif

#endif
