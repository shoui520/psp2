/*
 * SceCommonDialogMain's exports: what the dialog plugins call on the common
 * dialog's work area (a slot by its dialog type), and the paths and dimmer
 * they share. No Sony name is known for any of them: the link names are
 * <Library>_<NID>, the descriptions ours.
 */
#ifndef CDLG_MAIN_COMMON_DIALOG_MAIN_H
#define CDLG_MAIN_COMMON_DIALOG_MAIN_H

#include <scetypes.h>
#include "../cdlg_mgr/common_dialog_mgr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* closes the dialog: unloads its plugin once it runs */
int SceCommonDialogMain_05EFC635(int dialogType);
/* the dialog's parameter, as libcdlg keeps it */
void *SceCommonDialogMain_DD2CAC30(int dialogType);
/* whether the dialog's parameter is ready */
bool SceCommonDialogMain_75F10F83(int dialogType);
/* the dialog's plugin */
CommonDialogPlugin *SceCommonDialogMain_2E153774(int dialogType);
/* locks, tries to lock and unlocks the dialog's mutex */
int SceCommonDialogMain_B9D0E5A8(int dialogType);
int SceCommonDialogMain_36DF1E1B(int dialogType);
int SceCommonDialogMain_5739BC42(int dialogType);
/* sets and gets the dialog's user data */
int SceCommonDialogMain_68330971(int dialogType, int userData);
int SceCommonDialogMain_E33EFFA5(int dialogType);
/* the path of a dialog's resource and of its module (the plugins pass 1 as the option, which
   neither reads) */
int SceCommonDialogMain_1DE9E1A7(char *path, SceSize size, const char *name, int option);
int SceCommonDialogMain_C224F846(char *path, SceSize size, const char *name, int option);
/* the system's language as the paf Locale */
int SceCommonDialogMain_60E5D4E7(void);
/* shows and hides the dimmer behind the dialogs, on the main thread */
void SceCommonDialogMain_877BB562(void);
void SceCommonDialogMain_FDED47DB(void);
/* gives the info bar the application's state */
void SceCommonDialogMain_C62B845D(void);

#ifdef __cplusplus
}
#endif

#endif
