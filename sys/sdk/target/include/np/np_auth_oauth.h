/*
 * np/np_auth_oauth.h: OAuth authorization codes, under Sony's names and with Sony's
 * definitions (SDK Release 03.570.011, target/include/np/np_auth_oauth.h).
 */
#ifndef NP_AUTH_OAUTH_H_
#define NP_AUTH_OAUTH_H_

#include <np/np_common.h>

typedef struct SceNpAuthGetAuthorizationCodeParameter {
	SceSize size;
	const SceNpClientId *pClientId;
	const SceChar8 *pScope;
} SceNpAuthGetAuthorizationCodeParameter;

typedef int SceNpAuthOAuthRequestId;

#ifdef __cplusplus
extern "C" {
#endif

SceNpAuthOAuthRequestId sceNpAuthCreateOAuthRequest(void);
int sceNpAuthDeleteOAuthRequest(SceNpAuthOAuthRequestId reqId);
int sceNpAuthAbortOAuthRequest(SceNpAuthOAuthRequestId reqId);
int sceNpAuthGetAuthorizationCode(SceNpAuthOAuthRequestId reqId,
				  const SceNpAuthGetAuthorizationCodeParameter *param,
				  SceNpAuthorizationCode *authCode, int *issuerId);

#ifdef __cplusplus
}
#endif

#endif /* NP_AUTH_OAUTH_H_ */
