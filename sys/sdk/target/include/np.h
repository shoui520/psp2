/*
 * np.h: the NP library, under Sony's names and with Sony's definitions (SDK Release
 * 03.570.011, target/include/np.h), as user code needs them.
 */
#ifndef _SCE_NP_H
#define _SCE_NP_H

#include <np/np_auth.h>
#include <np/np_common.h>
#include <np/np_error.h>
#include <np/np_manager.h>
#include <np/np_auth_oauth.h>

typedef struct SceNpOptParam {
	SceSize optParamSize;
} SceNpOptParam;

typedef enum {
	SCE_NP_SERVICE_STATE_UNKNOWN = 0,
	SCE_NP_SERVICE_STATE_SIGNED_OUT,
	SCE_NP_SERVICE_STATE_SIGNED_IN,
	SCE_NP_SERVICE_STATE_ONLINE
} SceNpServiceState;

typedef void (*SceNpServiceStateCallback)(SceNpServiceState state, int retCode, void *userdata);

#ifdef __cplusplus
extern "C" {
#endif

int sceNpInit(const SceNpCommunicationConfig *commConf, SceNpOptParam *opt);
int sceNpTerm(void);
int sceNpCheckCallback(void);
int sceNpGetServiceState(SceNpServiceState *state);
int sceNpRegisterServiceStateCallback(SceNpServiceStateCallback callback, void *userdata);
int sceNpUnregisterServiceStateCallback(void);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_NP_H */
