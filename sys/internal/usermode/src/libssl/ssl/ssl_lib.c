/* Identified SSLeay connection operations, adapted in ssl_lib_sce.patch. */
#include <libssl/name.h>
#include "ssl_locl.h"
#include "ssl_context.h"
#include "../err/err.h"
#include "../format.h"
#include "../memory.h"
#include "../runtime.h"
#include "../time.h"
#include "../CRYPTOCME/common/key/key_api.h"
#include "../CRYPTOCME/common/key/pkey_ctx.h"
#include "../CRYPTOCME/common/module/crypto/cry_api.h"
#include "../CRYPTOCME/common/module/crypto/ctx.h"
#include "../MES/common/cert/src/ri_cert.h"
#include "../MES/common/module/crt_stor/src/small_stor.h"

#undef Malloc
#undef Free
#undef memset
#undef strncpy
#undef strcmp
#define Malloc(size) sslAllocate(size)
#define Free(ptr) sslFree(ptr)
#define memset(dst, value, size) sslFillMemory(dst, value, size)
#define strncpy(dst, src, size) sslCopyString(dst, src, size)
#define strcmp(a, b) sslCompareCString(a, b)

#define TLS1_VERSION 0x0301
#define TLS1_1_VERSION 0x0302
#define SSL_SENT_SHUTDOWN 1
#define SSL_RECEIVED_SHUTDOWN 2
#define SSL_ST_READ_HEADER 0xF0
#define SSL_VERIFY_NONE 0x00
#define SSL_SESS_CACHE_SERVER 0x0002
#define SSL_DEFAULT_CIPHER_LIST \
	"EDH-DSS-AES256-SHA:EDH-RSA-AES256-SHA:AES256-SHA:" \
	"EDH-DSS-AES128-SHA:EDH-RSA-AES128-SHA:AES128-SHA:" \
	"EDH-DSS-DES-CBC3-SHA:EDH-RSA-DES-CBC3-SHA:DES-CBC3-SHA:" \
	"EDH-DSS-RC4-SHA:RC4-SHA:RC4-MD5"

#define SSL_F_SSL_BAD_METHOD 161
#define SSL_F_SSL_BYTES_TO_CIPHER_LIST 162
#define SSL_F_SSL_CERT_NEW 163
#define SSL_F_SSL_CLEAR 165
#define SSL_F_SSL_CREATE_CIPHER_LIST 166
#define SSL_F_SSL_CTX_CHECK_PRIVATE_KEY 167
#define SSL_F_SSL_CTX_NEW 168
#define SSL_F_SSL_DO_HANDSHAKE 180
#define SSL_F_SSL_GET_SERVER_SEND_CERT 182
#define SSL_F_SSL_GET_SIGN_PKEY 183
#define SSL_F_SSL_INIT_WBIO_BUFFER 184
#define SSL_F_SSL_NEW 186
#define SSL_F_SSL_UNDEFINED_FUNCTION 198
#define SSL_F_SSL_WRITE 209

#define ERR_R_MALLOC_FAILURE 33
#define ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED 34
#define SSL_R_CONNECTION_TYPE_NOT_SET 144
#define SSL_R_ERROR_IN_RECEIVED_CIPHER_LIST 151
#define SSL_R_INTERNAL_ERROR 157
#define SSL_R_LIBRARY_HAS_NO_CIPHERS 161
#define SSL_R_NO_CERTIFICATE_ASSIGNED 177
#define SSL_R_NO_METHOD_SPECIFIED 188
#define SSL_R_NO_PRIVATE_KEY_ASSIGNED 190
#define SSL_R_NULL_SSL_CTX 195
#define SSL_R_NULL_SSL_METHOD_PASSED 196
#define SSL_R_PROTOCOL_IS_SHUTDOWN 207
#define SSL_R_SSL_CTX_HAS_NO_DEFAULT_SSL_VERSION 228
#define SSL_R_NO_CIPHER_MATCH 1108

#define SSLerr(function, reason) sslPushError(20, function, reason, NULL, 0)
#define ERR_peek_error sslPeekError

/* Retain SSL-C's out-of-line BIO queries at these call sites. */
#undef BIO_should_read
#undef BIO_should_write
#undef BIO_should_io_special

/* Sony's export link names; the API names are upstream's. */
#define SSL_version SceSslInternal_3EF42454
#define SSL_get_shutdown SceSslInternal_B1745DAD
#define SSL_state SceSslInternal_6F3B9809
#define SSL_pending SceSslInternal_D776E2F8
#define SSL_read SceSslInternal_4F781BB6
#define SSL_write sceSslInternalWrite
#define SSL_shutdown SceSslInternal_EC9FAF42
#define SSL_do_handshake SceSslInternal_E5EDD91B
#define SSL_get_error SceSslInternal_2F0FC3CD
#define SSL_connect SceSslInternal_92560B8D
#define SSL_set_connect_state SceSslInternal_CEE46B33
#define SSL_CTX_free SceSslInternal_EF9EBAC7
#define SSL_free SceSslInternal_19B2BB58
#define SSL_new SceSslInternal_D28834E6
#define SSL_get_value SceSslInternal_24C6257F
#define SSL_set_bio SceSslInternal_4A1557A7
#define SSL_set_options SceSslInternal_0DB6C206
#define SSL_CTX_ctrl SceSslInternal_CC7F4211
#define SSL_CTX_set_options SceSslInternal_FBEB1FD7
#define SSL_CTX_set_cipher_list SceSslInternal_6046C1F3
#define SSL_CTX_new SceSslInternal_E746465E
#define SSL_CTX_set_value SceSslInternal_CBEC7D13
#define SSL_set_verify_result SceSslInternal_B0E4CB2F
#define SSL_get_current_cipher SceSslInternal_0C693D36
#define SSL_set_info_callback SceSslInternal_F2C9B744
#define sslSetInfoCallbackEx SceSslInternal_F71DB06F
#define X509_check_private_key SceSslInternal_D68D8421
#define X509_NAME_free SceSslInternal_CE37B69E
#define X509_free SceSslInternal_144FA354

#include "ssl_lib_sce.inc"
