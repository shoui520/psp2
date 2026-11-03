/*
 * SceSslInternal: libssl's library for the system's own users (libhttp), the exports this
 * tree calls so far. libssl is RSA BSAFE SSL-C, whose interface follows SSLeay's: a library
 * handle, a context made from a method, a connection made from the context, and the
 * certificate, key and BIO objects around them.
 */

#ifndef _LIBSSL_SSLINTERNAL_H
#define _LIBSSL_SSLINTERNAL_H

#include <scetypes.h>
#include <libssl/name.h>
#include <libssl/object.h>

/* what SceSslInternal_2F0FC3CD makes of an I/O call's result */
#define SSL_INTERNAL_ERROR_NONE		0
#define SSL_INTERNAL_ERROR_SSL		1
#define SSL_INTERNAL_ERROR_WANT_READ	2
#define SSL_INTERNAL_ERROR_WANT_WRITE	3
#define SSL_INTERNAL_ERROR_SYSCALL	5
#define SSL_INTERNAL_ERROR_ZERO_RETURN	6

/* what the calls report when the object has no such item */
#define SSL_INTERNAL_ERROR_NOT_FOUND	10008

/* SceSslInternal_24C6257F's values of a connection */
#define SSL_INTERNAL_VALUE_LIBRARY	1
#define SSL_INTERNAL_VALUE_CERT_LIBRARY	5
#define SSL_INTERNAL_VALUE_KEY_LIBRARY	6

/* the libraries SceSslInternal_461C5990 reports for */
#define SSL_INTERNAL_LIBRARY_SSL	0
#define SSL_INTERNAL_LIBRARY_HTTP	1

/* the server certificate check libhttp installs on an SSL context: its chain, certNum long */
typedef int (*SceSslInternalCallback)(void *ssl, void **certs, int certNum, void *arg,
				      int *verifyResult);
/* a connection's client certificate and key, on the server's request */
typedef int (*SceSslInternalClientCertCallback)(void *ssl, void **cert, void **key);
/* the handshake's progress */
typedef void (*SceSslInternalInfoCallback)(void *ssl, int where, int ret);
typedef void (*SceSslInternalMessageCallback)(void);

int sceSslInternalIsInitalized(void);
int sceSslInternalGetCertificateAuthority(int store, int index, const char ***pem, void *buf,
					  SceSize size, SceSize *outSize);
struct ssl_st;
int sceSslInternalWrite(struct ssl_st *ssl, const void *buf, int len);

/* the library, its client method, contexts and their settings */
int SceSslInternal_2185BB05(void **lib);
void *SceSslInternal_A374CD7B(void);
void *SceSslInternal_AC1DBE40(void);
void *SceSslInternal_E746465E(void *lib, void *method);
void SceSslInternal_EF9EBAC7(void *ctx);
int SceSslInternal_67FB1CD3(void *ctx, const void *seed, SceSize size);
void SceSslInternal_FBEB1FD7(void *ctx, SceUInt32 options);
void SceSslInternal_9E38815D(void *ctx, SceSslInternalClientCertCallback callback);
int SceSslInternal_CC7F4211(void *ctx, int cmd, int larg, void *parg);
int SceSslInternal_260F5A85(void *ctx, int value);
int SceSslInternal_AF50B945(void *ctx, SceSslInternalCallback callback, void *arg);
int SceSslInternal_461C5990(int library, int error);

/* connections */
int SceSslInternal_3EF42454(struct ssl_st *ssl);
int SceSslInternal_6F3B9809(struct ssl_st *ssl);
int SceSslInternal_B1745DAD(struct ssl_st *ssl);
int SceSslInternal_D6D32A2F(struct ssl_st *ssl);
void *SceSslInternal_D28834E6(void *ctx);
void SceSslInternal_19B2BB58(void *ssl);
int SceSslInternal_EC9FAF42(struct ssl_st *ssl);
int SceSslInternal_D776E2F8(struct ssl_st *ssl);
void SceSslInternal_0DB6C206(void *ssl, SceUInt32 mode);
void SceSslInternal_4A1557A7(void *ssl, void *rbio, void *wbio);
void SceSslInternal_F2C9B744(void *ssl, SceSslInternalInfoCallback callback);
void SceSslInternal_F71DB06F(void *ssl, SceSslInternalMessageCallback callback, void *arg);
void SceSslInternal_CEE46B33(void *ssl);
int SceSslInternal_E5EDD91B(struct ssl_st *ssl);
int SceSslInternal_2F0FC3CD(struct ssl_st *ssl, int ret);
int SceSslInternal_4F781BB6(struct ssl_st *ssl, void *buf, int len);
int SceSslInternal_24C6257F(void *ssl, int what, void **value);
int SceSslInternal_A4EA9DE0(void *ssl);
int SceSslInternal_243DE290(void *ssl, int index, void **name);

/* BIOs, certificates, keys and names */
void *SceSslInternal_FFE2ED51(int sock, int closeFlag);
void *SceSslInternal_B8E6E249(void);
int SceSslInternal_33AC7A16(void *bio, const void *buf, int len);
void SceSslInternal_C4D984C2(void *bio);
int SceSslInternal_29AAC768(void *lib, int arg1, int format, SceSize size, const void *data,
			    void **end, void **cert);
int SceSslInternal_7D30F0A6(void *bio, void *lib, void **key, int type, int format);
int SceSslInternal_D68D8421(void *cert, void *key);
int SceSslInternal_81FA5DBE(void *cert, int which, void **name);
int SceSslInternal_568F0E80(void *a, void *b);
int SceSslInternal_08179EBD(void *cert, int which, void **name);
int SceSslInternal_91C2531C(void *name, int what, void *value);
int SceSslInternal_B5A099D2(void *ava, int what, void *part);
int SceSslInternal_17BF2367(void *ava);
int SceSslInternal_4D80D3F7(void *oid, int arg1, const void *data, SceSize len);
int SceSslInternal_3DE4F3DB(void *oid, int arg1, SceSize size, char *buf);

/* certificate extensions and the system's CA store */
int SceSslInternal_BF99D664(void *lib, int arg1, void **ext);
int SceSslInternal_64BDF1BB(void *cert, int item, void *ext);
int SceSslInternal_93EAE08A(void *ext, int item, void *data);
void SceSslInternal_F030A0DE(void *ext);
int SceSslInternal_3746501F(void *store, const char ***pem, void *buf, SceSize size,
			    SceSize *outSize, SceBool systemOnly);

#endif /* _LIBSSL_SSLINTERNAL_H */
