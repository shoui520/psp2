/*
 * SceLibHttp's loaded certificates: the CA certificates (a list) and the client certificate
 * with its private key, kept for the SSL glue under their own locks.
 */

#include <libssl/sslinternal.h>
#include "http_internal.h"

/* a CA certificate (SceHttp_F8885CFD) */
typedef struct HttpsLoadedCa {
	void *data;
	SceSize size;
	struct HttpsLoadedCa *next;
} HttpsLoadedCa;

/* the client certificate (SceHttp_031EBD2A) and its private key (SceHttp_802E3C30) */
typedef struct HttpsClientCert {
	void *cert;
	SceSize certSize;
	void *privKey;
	SceSize privKeySize;
} HttpsClientCert;

static HttpsLoadedCa *g_httpsCaList;
static HttpsClientCert *g_httpsClientCert;
static void *(*g_httpsCertMalloc)(SceSize size);
static void (*g_httpsCertFree)(void *ptr);
static SceBool g_httpsCertInitialized;
static SceKernelLwMutexWork g_httpsClientCertLock;	/* "SceLibHttpLoadedCert" */
static SceKernelLwMutexWork g_httpsCaListLock;		/* "SceLibHttpLoadedCert" */

int SceHttp_E0825488(void *(*mallocFunc)(SceSize size), void (*freeFunc)(void *ptr))
{
	int ret;

	ret = sceKernelCreateLwMutex(&g_httpsCaListLock, "SceLibHttpLoadedCert", 2, 0, NULL);
	if (ret < 0)
		return ret;
	ret = sceKernelCreateLwMutex(&g_httpsClientCertLock, "SceLibHttpLoadedCert", 2, 0, NULL);
	if (ret < 0)
		return ret;
	g_httpsCertMalloc = mallocFunc;
	g_httpsCertFree = freeFunc;
	g_httpsCertInitialized = 1;
	return ret;
}

/* drops the client certificate and its private key */
int SceHttp_42108FBE(void)
{
	sceKernelLockLwMutex(&g_httpsClientCertLock, 1, NULL);
	if (g_httpsClientCert != NULL) {
		g_httpsCertFree(g_httpsClientCert->cert);
		g_httpsCertFree(g_httpsClientCert->privKey);
		g_httpsCertFree(g_httpsClientCert);
		g_httpsClientCert = NULL;
	}
	return sceKernelUnlockLwMutex(&g_httpsClientCertLock, 1);
}

/* drops the CA certificates */
int SceHttp_0CC59EF6(void)
{
	HttpsLoadedCa *ca, *next;

	sceKernelLockLwMutex(&g_httpsCaListLock, 1, NULL);
	ca = g_httpsCaList;
	while (ca != NULL) {
		next = ca->next;
		g_httpsCertFree(ca->data);
		g_httpsCertFree(ca);
		ca = next;
	}
	g_httpsCaList = NULL;
	return sceKernelUnlockLwMutex(&g_httpsCaListLock, 1);
}

int SceHttp_11C9BCE8(void)
{
	int ret;

	g_httpsCertInitialized = 0;
	if (g_httpsCaList != NULL)
		SceHttp_0CC59EF6();
	if (g_httpsClientCert != NULL)
		SceHttp_42108FBE();
	ret = sceKernelDeleteLwMutex(&g_httpsCaListLock);
	if (ret < 0)
		return ret;
	return sceKernelDeleteLwMutex(&g_httpsClientCertLock);
}

SceBool _sceHttpsCertIsInitialized(void)
{
	return g_httpsCertInitialized;
}

/* appends a copy of a CA certificate to the list */
int SceHttp_F8885CFD(const void *data, SceSize size)
{
	int ret = 0;
	HttpsLoadedCa *last, *ca;

	sceKernelLockLwMutex(&g_httpsCaListLock, 1, NULL);
	last = g_httpsCaList;
	if (last != NULL) {
		for (ca = last->next; ca != NULL; ca = ca->next)
			last = ca;
	}
	ca = g_httpsCertMalloc(sizeof(*ca));
	if (ca != NULL) {
		ca->data = g_httpsCertMalloc(size);
		if (ca->data != NULL) {
			sceClibMemcpy_safe(ca->data, data, size);
			ca->size = size;
			ca->next = NULL;
			if (last != NULL)
				last->next = ca;
			else
				g_httpsCaList = ca;
		} else {
			g_httpsCertFree(ca);
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		}
	} else {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
	}
	sceKernelUnlockLwMutex(&g_httpsCaListLock, 1);
	return ret;
}

/* sets a copy of the client certificate (on failure the whole entry goes, its key leaked) */
int SceHttp_031EBD2A(const void *data, SceSize size)
{
	int ret = 0;

	sceKernelLockLwMutex(&g_httpsClientCertLock, 1, NULL);
	if (g_httpsClientCert == NULL) {
		g_httpsClientCert = g_httpsCertMalloc(sizeof(*g_httpsClientCert));
		if (g_httpsClientCert != NULL)
			sceClibMemset(g_httpsClientCert, 0, sizeof(*g_httpsClientCert));
	}
	if (g_httpsClientCert != NULL) {
		if (g_httpsClientCert->cert != NULL) {
			g_httpsCertFree(g_httpsClientCert->cert);
			g_httpsClientCert->cert = NULL;
		}
		g_httpsClientCert->cert = g_httpsCertMalloc(size);
		if (g_httpsClientCert->cert != NULL) {
			sceClibMemcpy_safe(g_httpsClientCert->cert, data, size);
			g_httpsClientCert->certSize = size;
		} else {
			g_httpsCertFree(g_httpsClientCert);
			g_httpsClientCert = NULL;
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		}
	} else {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
	}
	sceKernelUnlockLwMutex(&g_httpsClientCertLock, 1);
	return ret;
}

/* sets a copy of the client certificate's private key (the same way) */
int SceHttp_802E3C30(const void *data, SceSize size)
{
	int ret = 0;

	sceKernelLockLwMutex(&g_httpsClientCertLock, 1, NULL);
	if (g_httpsClientCert == NULL) {
		g_httpsClientCert = g_httpsCertMalloc(sizeof(*g_httpsClientCert));
		if (g_httpsClientCert != NULL)
			sceClibMemset(g_httpsClientCert, 0, sizeof(*g_httpsClientCert));
	}
	if (g_httpsClientCert != NULL) {
		if (g_httpsClientCert->privKey != NULL) {
			g_httpsCertFree(g_httpsClientCert->privKey);
			g_httpsClientCert->privKey = NULL;
		}
		g_httpsClientCert->privKey = g_httpsCertMalloc(size);
		if (g_httpsClientCert->privKey != NULL) {
			sceClibMemcpy_safe(g_httpsClientCert->privKey, data, size);
			g_httpsClientCert->privKeySize = size;
		} else {
			g_httpsCertFree(g_httpsClientCert);
			g_httpsClientCert = NULL;
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		}
	} else {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
	}
	sceKernelUnlockLwMutex(&g_httpsClientCertLock, 1);
	return ret;
}

/* the index'th CA certificate: copied when buf holds it, its size to *outSize */
int _sceHttpsCertGetCa(int unused, int index, void *buf, SceSize size, int unused2,
		       SceSize *outSize)
{
	int ret = 0;
	HttpsLoadedCa *ca;
	int i;

	sceKernelLockLwMutex(&g_httpsCaListLock, 1, NULL);
	for (ca = g_httpsCaList, i = 0; ca != NULL; ca = ca->next, i++) {
		if (i == index)
			break;
	}
	if (ca != NULL) {
		if (buf != NULL && size >= ca->size)
			sceClibMemcpy_safe(buf, ca->data, ca->size);
		if (outSize != NULL)
			*outSize = ca->size;
	} else {
		ret = HTTPS_SSL_ERROR_INVALID_ID;
	}
	sceKernelUnlockLwMutex(&g_httpsCaListLock, 1);
	return ret;
}

int _sceHttpsCertGetClientCert(void *buf, SceSize size, SceSize *outSize)
{
	int ret = 0;

	sceKernelLockLwMutex(&g_httpsClientCertLock, 1, NULL);
	if (g_httpsClientCert != NULL && g_httpsClientCert->cert != NULL) {
		if (buf != NULL && size >= g_httpsClientCert->certSize)
			sceClibMemcpy_safe(buf, g_httpsClientCert->cert,
					   g_httpsClientCert->certSize);
		if (outSize != NULL)
			*outSize = g_httpsClientCert->certSize;
	} else {
		ret = SCE_HTTP_ERROR_NOT_FOUND;
	}
	sceKernelUnlockLwMutex(&g_httpsClientCertLock, 1);
	return ret;
}

int _sceHttpsCertGetPrivateKey(void *buf, SceSize size, SceSize *outSize)
{
	int ret = 0;

	sceKernelLockLwMutex(&g_httpsClientCertLock, 1, NULL);
	if (g_httpsClientCert != NULL && g_httpsClientCert->privKey != NULL) {
		if (buf != NULL && size >= g_httpsClientCert->privKeySize)
			sceClibMemcpy_safe(buf, g_httpsClientCert->privKey,
					   g_httpsClientCert->privKeySize);
		if (outSize != NULL)
			*outSize = g_httpsClientCert->privKeySize;
	} else {
		ret = SCE_HTTP_ERROR_NOT_FOUND;
	}
	sceKernelUnlockLwMutex(&g_httpsClientCertLock, 1);
	return ret;
}

/* puts libhttp's callback on an SSL context */
int SceHttp_A9A39BA1(void *sslCtx, void *arg)
{
	int ret = 0;

	if (sslCtx == NULL || arg == NULL)
		return HTTPS_SSL_ERROR_INVALID_VALUE;
	SceSslInternal_260F5A85(sslCtx, 1);
	SceSslInternal_AF50B945(sslCtx, _sceHttpsSslCallback, arg);
	return ret;
}

int SceHttp_A4CF892E(void *sslCtx, void *arg)
{
	return 0;
}

int _sceHttpsCertGetCaList(SceSslCert ***caCerts, SceInt32 *caNum,
			   HttpsSslContext *context)
{
	int ret = HTTPS_SSL_ERROR_INVALID_VALUE;

	if (context == NULL)
		return ret;
	return _sceHttpsSslGetCaList(caCerts, caNum, context);
}

int _sceHttpsCertFreeCaList(SceSslCert **caCerts, SceInt32 caNum,
			    HttpsSslContext *context)
{
	int ret = HTTPS_SSL_ERROR_INVALID_VALUE;

	if (context == NULL)
		return ret;
	return _sceHttpsSslFreeCaList(caCerts, caNum, context);
}
