/* Identified SSLeay SSL3 operations, adapted in s3_lib_sce.patch. */
#include "ssl_locl.h"
#include "../memory.h"
#include "../err/err.h"
#include "../CRYPTOCME/common/module/crypto/cry_api.h"
#include <libssl/name.h>
#include <libssl/object.h>

#undef Malloc
#undef Free
#define Malloc sslAllocate
#define memset sslFillMemory
#define memcpy sslCopyMemory
#define Free sslFree
#define qsort sslSort
/* SSLv3_data puts ciphers with ssl_lib.c's function, like TLSv1_data */
#define ssl3_put_cipher_by_char ssl_put_cipher_by_char
#define ERR_get_error sslGetError
#define clear_sys_error() (*sceNetErrnoLoc() = 0)
#define BIO_flush(b) (int)BIO_ctrl(b, BIO_CTRL_FLUSH, 0, NULL)
#define SSL_in_init(a) (SSL_state(a) & SSL_ST_INIT)
#define SSL_state(a) ((a)->state)
#define SSL_SENT_SHUTDOWN 1
#define SSL_RECEIVED_SHUTDOWN 2
#define SSL_AD_CLOSE_NOTIFY SSL3_AD_CLOSE_NOTIFY
#define MD5_DIGEST_LENGTH 16
#define SHA_DIGEST_LENGTH 20
#define SSL_F_SSL3_NEW 147
#define SSL_F_SSL3_CTX_CTRL 217
#define SSL_F_SSL3_CTRL 226
#define ERR_R_MALLOC_FAILURE 33
#define ERR_R_EVP_LIB 35
#define SSLerr(function, reason) sslPushError(20, function, reason, NULL, 0)

int *sceNetErrnoLoc(void);
void sslSort(void *base, unsigned int count, unsigned int size,
	     int (*compare)(const void *, const void *));
void sslRetainCertificateObject(void *cert);
int sslGetPublicKeyInfo(void *key, int selector, void *result);
int sslClonePublicKey(void *key, int flags, void **out);
int ssl_undefined_function(SSL *s);
SSL_METHOD *ssl_bad_method(int ver);
int ssl_cipher_ptr_id_cmp(SSL_CIPHER **ap, SSL_CIPHER **bp);
int ssl3_send_alert(SSL *s, int level, int desc);
int ssl3_dispatch_alert(SSL *s);
int ssl3_read_bytes(SSL *s, int type, char *buf, int len);
int ssl3_write_bytes(SSL *s, int type, char *buf, int len);

static char *OBJ_bsearch(char *key, char *base, int num, int size,
			 int (*cmp)())
{
	int l, h, i, c;
	char *p;

	if (num == 0)
		return(NULL);
	l = 0;
	h = num;
	while (l < h) {
		i = (l + h) / 2;
		p = &(base[i * size]);
		c = (*cmp)(key, p);
		if (c < 0)
			h = i;
		else if (c > 0)
			l = i + 1;
		else
			return(p);
	}
	return(NULL);
}

#include "s3_lib_sce.inc"
