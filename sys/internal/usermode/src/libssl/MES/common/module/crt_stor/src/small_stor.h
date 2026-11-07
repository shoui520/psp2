/* Private descriptive names and this header filename are ours. */
#ifndef LIBSSL_SMALL_STOR_H
#define LIBSSL_SMALL_STOR_H

#include "../../../cert/src/ri_cert.h"
#include "../../../cert/src/extension.h"

typedef struct SslSmallCertificateStore SslSmallCertificateStore;
typedef struct SslSmallCertificateStoreCursor SslSmallCertificateStoreCursor;

typedef struct SslCertificateStoreQuery {
	SceUInt32 mask;
	void *value;
	SceUInt32 reserved[3];
} SslCertificateStoreQuery;

typedef struct SslCertificateStoreMethods {
	int (*CreateStore)(SslLibrary *library, int unused,
			   SslSmallCertificateStore **result);
	int (*RetainStore)(SslSmallCertificateStore *store);
	int (*DestroyStore)(SslSmallCertificateStore *store);
	int (*GetStoreInfo)(SslSmallCertificateStore *store, int selector,
			    void *result);
	int (*SetStoreInfo)(SslSmallCertificateStore *store, int selector,
			    void *value);
	int (*OpenStore)(SslSmallCertificateStore *store, int mode);
	int (*CloseStore)(SslSmallCertificateStore *store, int mode);
	SceUIntPtr opaqueOperation;
	int (*CreateCursor)(SslSmallCertificateStore *store,
			    SslSmallCertificateStoreCursor **result);
	int (*DestroyCursor)(SslSmallCertificateStoreCursor *cursor);
	int (*ResetCursor)(SslSmallCertificateStoreCursor *cursor);
	int (*AddEntry)(SslSmallCertificateStoreCursor *cursor);
	int (*UpdateEntry)(SslSmallCertificateStoreCursor *cursor);
	int (*DeleteEntry)(SslSmallCertificateStoreCursor *cursor);
	int (*GetCursorInfo)(SslSmallCertificateStoreCursor *cursor,
			     int selector, void *result);
	int (*SetCursorInfo)(SslSmallCertificateStoreCursor *cursor,
			     int selector, void *value);
	int (*FindEntry)(SslSmallCertificateStoreCursor *cursor, int selector,
			 SslCertificateStoreQuery *query);
} SslCertificateStoreMethods;

typedef struct SslSmallCertificateStoreEntry {
	SslCertificate *certificate;
	SslCertificateExtensionData keyId;
	SceUInt32 flags;
	SceUInt32 status;
} SslSmallCertificateStoreEntry;

struct SslSmallCertificateStore {
	const SslCertificateStoreMethods *methods;
	SslLibrary *library;
	int count;
	int capacity;
	SslSmallCertificateStoreEntry *entries;
	int references;
};

struct SslSmallCertificateStoreCursor {
	const SslCertificateStoreMethods *methods;
	SslSmallCertificateStore *store;
	SslSmallCertificateStoreEntry entry;
};

_Static_assert(sizeof(SslCertificateStoreMethods) == 68,
	       "SslCertificateStoreMethods");
_Static_assert(sizeof(SslSmallCertificateStoreEntry) == 20,
	       "SslSmallCertificateStoreEntry");
_Static_assert(sizeof(SslSmallCertificateStore) == 24,
	       "SslSmallCertificateStore");
_Static_assert(sizeof(SslSmallCertificateStoreCursor) == 28,
	       "SslSmallCertificateStoreCursor");

int sslCreateCertificateStore(SslLibrary *library, int parameter,
			      SslObjectHeader **result);
int sslRetainCertificateStore(void *store);
int sslDestroyCertificateStore(void *store);
int sslOpenCertificateStore(void *store, int mode);
int sslCloseCertificateStore(void *store, int mode);
int sslCreateCertificateStoreCursor(void *store, void **result);
int sslDestroyCertificateStoreCursor(void *cursor);
int sslResetCertificateStoreCursor(void *cursor);
int sslAddCertificateStoreEntry(void *cursor);
const SslCertificateStoreMethods *sslGetSmallCertificateStoreMethods(void);
int sslNewSmallCertificateStore(SslLibrary *library, int unused,
				SslSmallCertificateStore **result);
int sslRetainSmallCertificateStore(SslSmallCertificateStore *store);
int sslFreeSmallCertificateStore(SslSmallCertificateStore *store);
int sslGetSmallCertificateStoreInfo(SslSmallCertificateStore *store,
				    int selector, void *result);
int sslSetSmallCertificateStoreInfo(SslSmallCertificateStore *store,
				    int selector, void *value);
int sslOpenSmallCertificateStore(SslSmallCertificateStore *store, int mode);
int sslCloseSmallCertificateStore(SslSmallCertificateStore *store, int mode);
int sslCreateSmallCertificateStoreCursor(SslSmallCertificateStore *store,
					 SslSmallCertificateStoreCursor **result);
int sslFreeSmallCertificateStoreCursor(SslSmallCertificateStoreCursor *cursor);
int sslResetSmallCertificateStoreCursor(SslSmallCertificateStoreCursor *cursor);
int sslDeleteSmallCertificateStoreEntry(SslSmallCertificateStoreCursor *cursor);
int sslInsertSmallCertificateStoreEntry(SslSmallCertificateStoreCursor *cursor);
int sslFindSmallCertificateStoreEntry(SslSmallCertificateStoreCursor *cursor,
				      int selector,
				      SslCertificateStoreQuery *query);
int sslGetSmallCertificateStoreCursorInfo(SslSmallCertificateStoreCursor *cursor,
					  int selector, void *result);
int sslSetSmallCertificateStoreCursorInfo(SslSmallCertificateStoreCursor *cursor,
					  int selector, void *value);
int sslSetCertificateStoreCursorCertificate(void *cursor, void *certificate,
					    void *keyId);
int sslGetCertificateStoreCursorCertificate(void *cursor, void **certificate,
					    SceUInt32 *keyId);
int sslSetCertificateStoreCursorFlags(void *cursor, int flags);
int sslGetCertificateStoreCursorFlags(void *cursor, int *flags);
int sslFindCertificateStoreEntryByName(void *cursor, void *name, int mask);
int sslFindCertificateStoreEntryByKeyId(void *cursor, void *keyId, int mask);

#endif
