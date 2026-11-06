/*
 * The progress view the setup reports to (cui_view.c): a console line for each step, and,
 * where the system has them, SceSblSsUpdateMgr's update status calls, which show it on the
 * display. psp2swu links the same code. No build names it: the names are ours.
 */
#ifndef CUI_SETUPPER_CUI_VIEW_H
#define CUI_SETUPPER_CUI_VIEW_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

void CuiInformUpdateStarted(SceUInt32 phase, SceInt32 reserved, SceInt32 estimate,
			    const char *message, SceInt32 length);
void CuiInformUpdateOngoing(SceUInt32 phase, SceInt32 reserved, SceInt32 progress,
			    const char *message, SceInt32 length);
void CuiInformUpdateFinished(SceInt32 result, const char *message, SceInt32 length);

int CuiSetSwInfoInt(const char *name, SceSize nameLength, SceUInt32 value);
int CuiSetSwInfoStr(const char *name, SceSize nameLength, const char *value, SceSize valueLength);
int CuiSetSwInfoBin(const char *name, SceSize nameLength, const void *value, SceSize valueLength);

/* what the program shows of each step besides the console line */
typedef int (*CuiStartedCallback)(SceUInt32 phase, SceInt32 reserved, const char *message);
typedef int (*CuiOngoingCallback)(SceUInt32 phase, SceInt32 reserved, SceInt32 progress);
typedef int (*CuiFinishedCallback)(SceInt32 result, const char *message);

void CuiSetCallbacks(CuiStartedCallback started, CuiOngoingCallback ongoing,
		     CuiFinishedCallback finished);
int CuiDefaultStartedCallback(SceUInt32 phase, SceInt32 reserved, const char *message);
int CuiDefaultOngoingCallback(SceUInt32 phase, SceInt32 reserved, SceInt32 progress);
int CuiDefaultFinishedCallback(SceInt32 result, const char *message);

#ifdef __cplusplus
}
#endif

#endif
