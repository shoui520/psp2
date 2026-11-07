/* Adapted from SSLeay 0.9.1b crypto/pem/pem.h; CRYPTOCME PEM wrapper names are ours. */
#ifndef LIBSSL_PEM_H
#define LIBSSL_PEM_H

#include "../bio/bio.h"
#include "../CRYPTOCME/common/object.h"
#include "../CRYPTOCME/common/library/lib_ctx.h"

#define PEM_STRING_X509_OLD	"X509 CERTIFICATE"
#define PEM_STRING_X509		"CERTIFICATE"
#define PEM_STRING_X509_REQ_OLD	"NEW CERTIFICATE REQUEST"
#define PEM_STRING_X509_REQ	"CERTIFICATE REQUEST"
#define PEM_STRING_X509_CRL	"X509 CRL"
#define PEM_STRING_EVP_PKEY	"PRIVATE KEY"
#define PEM_STRING_RSA		"RSA PRIVATE KEY"
#define PEM_STRING_RSA_PUBLIC	"RSA PUBLIC KEY"
#define PEM_STRING_DSA		"DSA PRIVATE KEY"
#define PEM_STRING_PKCS7	"PKCS7"
#define PEM_STRING_DHPARAMS	"DH PARAMETERS"
#define PEM_STRING_SSL_SESSION	"SSL SESSION PARAMETERS"
#define PEM_STRING_DSAPARAMS	"DSA PARAMETERS"

#define SSL_PEM_MODULE_TYPE 17
#define SSL_PEM_CONTEXT_INFO_LIBRARY 1
#define SSL_PEM_STRING_ENCRYPTED_PKEY "ENCRYPTED PRIVATE KEY"

typedef struct SslPemMethods {
	int (*CreateContext)(SslLibrary *library, int parameter, void **result);
	int (*DestroyContext)(void *context);
	int (*GetContextInfo)(void *context, int selector, void *result);
	int (*SetContextInfo)(void *context, int selector, void *value);
	int (*ReadObject)(void *context, BIO *bio, const char *name,
			  void **data, SceUInt32 *length);
	int (*WriteObject)(void *context, BIO *bio, const char *name,
			   const unsigned char *data, int length,
			   void *encryption);
} SslPemMethods;

typedef struct SslPemContext {
	const SslPemMethods *methods;
	SslLibrary *library;
} SslPemContext;

_Static_assert(sizeof(SslPemMethods) == 24, "SslPemMethods");
_Static_assert(sizeof(SslPemContext) == 8, "SslPemContext");

const SslPemMethods *sslGetPemModuleMethods(void);
int sslCreatePemContext(SslLibrary *library, int parameter,
			SslObjectHeader **result);
int sslDestroyPemContext(void *context);
int sslReadPemObject(void *context, BIO *bio, const char *name,
		     void **data, SceUInt32 *length);
int sslNewPemContext(SslLibrary *library, int parameter, void **result);
int sslFreePemContext(void *context);
int sslGetPemContextInfo(void *context, int selector, void *result);
int sslSetPemContextInfo(void *context, int selector, void *value);
int sslReadPemBio(BIO *bp, char **name, char **header,
		  unsigned char **data, int *len);
int sslReadPemTypedObject(void *context, BIO *bp, const char *name,
			  void **dataOut, SceUInt32 *lenOut);
int sslWritePemTypedObject(void *context, BIO *bp, const char *name,
			   const unsigned char *data, int len,
			   void *encryption);

#endif
