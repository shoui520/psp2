/* Identified SSLeay SSLv23 client handshake, adapted in s23_clnt_sce.patch. */
#include "ssl_locl.h"
#include "tls1_sce.h"
#include "../memory.h"
#include "../err/err.h"

#define memcpy sslCopyMemory
#define ERR_clear_error sslClearThreadErrors
#define clear_sys_error() (*sceNetErrnoLoc() = 0)
#define BIO_write SceSslInternal_33AC7A16
#define SSL_state SceSslInternal_6F3B9809
#define SSL_connect SceSslInternal_92560B8D

#define SSL2_MT_CLIENT_HELLO 1
#define SSL2_MT_SERVER_HELLO 4
#define SSL2_CHALLENGE_LENGTH 16
#define SSL2_MAX_CHALLENGE_LENGTH 32
#define SSL_OP_NETSCAPE_CHALLENGE_BUG 0x00000002L
#define SSL_OP_NO_SSLv3 0x02000000L
#define SSL_OP_NO_TLSv1 0x04000000L
#define SSL_OP_NO_TLSv1_1 0x40000000UL
#define SSL_AD_REASON_OFFSET 1000

#define SSL_F_SSL23_CLIENT_HELLO 116
#define SSL_F_SSL23_CONNECT 117
#define SSL_F_SSL23_GET_SERVER_HELLO 119
#define SSL_R_NO_CIPHERS_AVAILABLE 181
#define SSL_R_NO_PROTOCOLS_AVAILABLE 191
#define SSL_R_SESSION_ID_CONTEXT_UNINITIALIZED 221
#define SSL_R_UNKNOWN_PROTOCOL 252
#define SSL_R_UNKNOWN_STATE 255
#define SSL_R_UNSUPPORTED_PROTOCOL 258
#define SSL_R_RANDOM_NUMBER_GENERATION_FAILED 1116
#define SSLerr(function, reason) sslPushError(20, function, reason, NULL, 0)

extern SSL_METHOD TLSv1_client_data;
extern SSL_METHOD SSLv3_client_data;

int *sceNetErrnoLoc(void);
int SceSslInternal_6F3B9809(SSL *s);
int ssl3_client_hello(SSL *s);
SSL_METHOD *ssl3_get_client_method(int ver);

#include "s23_clnt_sce.inc"
