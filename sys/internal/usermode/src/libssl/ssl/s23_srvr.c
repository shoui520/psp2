/* Identified SSLeay SSLv23 server handshake, adapted in s23_srvr_sce.patch. */
#include "ssl_locl.h"
#include "tls1_sce.h"
#include "../memory.h"
#include "../err/err.h"

#define memcpy sslCopyMemory
#define memset sslFillMemory
#define strncmp sslCompareStrings
#define ERR_clear_error sslClearThreadErrors
#define clear_sys_error() (*sceNetErrnoLoc() = 0)

#define SSL2_VERSION 0x0002
#define SSL2_MT_CLIENT_HELLO 1
#define SSL23_ST_SR_CLNT_HELLO_C (0x212 | SSL_ST_ACCEPT)
#define SSL_OP_NO_SSLv3 0x02000000L
#define SSL_OP_NO_TLSv1 0x04000000L

#define SSL_F_SSL23_ACCEPT 115
#define SSL_F_SSL23_GET_CLIENT_HELLO 118
#define SSL_R_HTTPS_PROXY_REQUEST 155
#define SSL_R_HTTP_REQUEST 156
#define SSL_R_RECORD_LENGTH_MISMATCH 213
#define SSL_R_RECORD_TOO_LARGE 214
#define SSL_R_UNKNOWN_PROTOCOL 252
#define SSL_R_UNKNOWN_STATE 255
#define SSLerr(function, reason) sslPushError(20, function, reason, NULL, 0)

extern SSL_METHOD TLSv1_server_data;

int *sceNetErrnoLoc(void);
int sslCompareStrings(const char *left, const char *right, SceUInt32 size);

#include "s23_srvr_sce.inc"
