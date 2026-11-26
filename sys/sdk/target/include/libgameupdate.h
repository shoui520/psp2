#ifndef _SCE_LIBGAMEUPDATE_H
#define _SCE_LIBGAMEUPDATE_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCE_GAME_UPDATE_ERROR_NOT_INITIALIZED		(-2143213567)	/* 0x80412801 */
#define SCE_GAME_UPDATE_ERROR_ALREADY_INITIALIZED	(-2143213566)	/* 0x80412802 */
#define SCE_GAME_UPDATE_ERROR_INVALID_ARG		(-2143213565)	/* 0x80412803 */
#define SCE_GAME_UPDATE_ERROR_HTTP_OUT_OF_MEMORY	(-2143213564)	/* 0x80412804 */
#define SCE_GAME_UPDATE_ERROR_HTTP_LOCAL_ERROR		(-2143213563)	/* 0x80412805 */
#define SCE_GAME_UPDATE_ERROR_HTTP_CONNECTION_ERROR	(-2143213562)	/* 0x80412806 */
#define SCE_GAME_UPDATE_ERROR_HTTP_PROXY_ERROR		(-2143213561)	/* 0x80412807 */
#define SCE_GAME_UPDATE_ERROR_HTTP_BAD_RESPONSE		(-2143213560)	/* 0x80412808 */
#define SCE_GAME_UPDATE_ERROR_PARSER_FAILED		(-2143213559)	/* 0x80412809 */
#define SCE_GAME_UPDATE_ERROR_BAD_RESPONSE		(-2143213558)	/* 0x8041280A */
#define SCE_GAME_UPDATE_ERROR_BUSY			(-2143213557)	/* 0x8041280B */
#define SCE_GAME_UPDATE_ERROR_ABORTED			(-2143213556)	/* 0x8041280C */
#define SCE_GAME_UPDATE_ERROR_SERVER_MAINTENANCE	(-2143213555)	/* 0x8041280D */

#define SCE_GAME_UPDATE_APP_VER_SIZE			6

typedef void SceGameUpdateOptParam;

typedef struct SceGameUpdateResult {
	unsigned int size;
	SceBool patchExist;
	char appVer[SCE_GAME_UPDATE_APP_VER_SIZE];
	char padding[2];
} SceGameUpdateResult;

int sceGameUpdateInit(SceGameUpdateOptParam *optParam);
int sceGameUpdateRun(SceGameUpdateResult *result);
int sceGameUpdateTerm(void);
int sceGameUpdateAbort(void);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_LIBGAMEUPDATE_H */
