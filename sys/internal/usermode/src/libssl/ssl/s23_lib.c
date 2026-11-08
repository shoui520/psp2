/* Identified SSLeay SSLv23 operations, adapted in s23_lib_sce.patch. */
#include "ssl_locl.h"
#include "tls1_sce.h"
#include "../err/err.h"

#define clear_sys_error() (*sceNetErrnoLoc() = 0)
#define SSL_in_init(a) (SSL_state(a) & SSL_ST_INIT)
#define SSL_state SceSslInternal_6F3B9809
#define SSL_read SceSslInternal_4F781BB6
#define SSL_write sceSslInternalWrite

#define SSL_F_SSL23_READ 120
#define SSL_F_SSL23_WRITE 121
#define SSL_R_SSL_HANDSHAKE_FAILURE 229
#define SSL_R_UNINITIALIZED 1101
#define SSLerr(function, reason) sslPushError(20, function, reason, NULL, 0)

int *sceNetErrnoLoc(void);
int SceSslInternal_6F3B9809(SSL *s);
int SceSslInternal_4F781BB6(SSL *s, void *buf, int len);
int sceSslInternalWrite(SSL *s, const void *buf, int len);

#include "s23_lib_sce.inc"
