/* Source path recovered from the firmware; descriptive private names are ours. */
#include <libssl/name.h>
#include "small_stor.h"
#include "../../../../../memory.h"
#include "../../../../../runtime.h"

static const char smallCertificateStoreSource[] =
	"S:\\work\\MES\\common\\module/crt_stor/src/small_stor.c";

int sslCreateCertificateStore(SslLibrary *library, int parameter,
			      SslObjectHeader **result)
{
	return sslConstructLibraryObject(library, 26, 0, 0, parameter, result);
}

int sslRetainCertificateStore(void *store)
{
	SslSmallCertificateStore *object = store;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL) {
		status = 10011;
		if (object->methods->RetainStore != NULL)
			status = object->methods->RetainStore(object);
	}
	return status;
}

int sslDestroyCertificateStore(void *store)
{
	SslSmallCertificateStore *object = store;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL) {
		status = 10011;
		if (object->methods->DestroyStore != NULL)
			status = object->methods->DestroyStore(object);
	}
	return status;
}

int sslOpenCertificateStore(void *store, int mode)
{
	SslSmallCertificateStore *object = store;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL) {
		status = 10011;
		if (object->methods->OpenStore != NULL)
			status = object->methods->OpenStore(object, mode);
	}
	return status;
}

int sslCloseCertificateStore(void *store, int mode)
{
	SslSmallCertificateStore *object = store;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL) {
		status = 10011;
		if (object->methods->CloseStore != NULL)
			status = object->methods->CloseStore(object, mode);
	}
	return status;
}

int sslCreateCertificateStoreCursor(void *store, void **result)
{
	SslSmallCertificateStore *object = store;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL && result != NULL) {
		status = 10011;
		if (object->methods->CreateCursor != NULL)
			status = object->methods->CreateCursor(
				object, (SslSmallCertificateStoreCursor **)result);
	}
	return status;
}

int sslDestroyCertificateStoreCursor(void *cursor)
{
	SslSmallCertificateStoreCursor *object = cursor;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL) {
		status = 10011;
		if (object->methods->DestroyCursor != NULL)
			status = object->methods->DestroyCursor(object);
	}
	return status;
}

int sslResetCertificateStoreCursor(void *cursor)
{
	SslSmallCertificateStoreCursor *object = cursor;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL) {
		status = 10011;
		if (object->methods->ResetCursor != NULL)
			status = object->methods->ResetCursor(object);
	}
	return status;
}

int sslAddCertificateStoreEntry(void *cursor)
{
	SslSmallCertificateStoreCursor *object = cursor;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL) {
		status = 10011;
		if (object->methods->AddEntry != NULL)
			status = object->methods->AddEntry(object);
	}
	return status;
}

static const SslCertificateStoreMethods sslSmallCertificateStoreMethods = {
	.CreateStore = sslNewSmallCertificateStore,
	.RetainStore = sslRetainSmallCertificateStore,
	.DestroyStore = sslFreeSmallCertificateStore,
	.GetStoreInfo = sslGetSmallCertificateStoreInfo,
	.SetStoreInfo = sslSetSmallCertificateStoreInfo,
	.OpenStore = sslOpenSmallCertificateStore,
	.CloseStore = sslCloseSmallCertificateStore,
	.CreateCursor = sslCreateSmallCertificateStoreCursor,
	.DestroyCursor = sslFreeSmallCertificateStoreCursor,
	.ResetCursor = sslResetSmallCertificateStoreCursor,
	.AddEntry = sslInsertSmallCertificateStoreEntry,
	.UpdateEntry = sslInsertSmallCertificateStoreEntry,
	.DeleteEntry = sslDeleteSmallCertificateStoreEntry,
	.GetCursorInfo = sslGetSmallCertificateStoreCursorInfo,
	.SetCursorInfo = sslSetSmallCertificateStoreCursorInfo,
	.FindEntry = sslFindSmallCertificateStoreEntry
};

const SslCertificateStoreMethods *sslGetSmallCertificateStoreMethods(void)
{
	return &sslSmallCertificateStoreMethods;
}

int sslNewSmallCertificateStore(SslLibrary *library, int unused,
				SslSmallCertificateStore **result)
{
	SslSmallCertificateStore *store;
	int status = 0;

	(void)unused;
	store = sslAllocate(sizeof(*store));
	*result = store;
	if (store == NULL) {
		status = 10005;
	} else {
		sslFillMemory(store, 0, sizeof(*store));
		sslLibraryAddReference(library);
		(*result)->library = library;
		(*result)->references = 1;
	}
	return status;
}

int sslRetainSmallCertificateStore(SslSmallCertificateStore *store)
{
	sslAddReference(&store->references, 1, 11,
			smallCertificateStoreSource, 50);
	return 0;
}

int sslFreeSmallCertificateStore(SslSmallCertificateStore *store)
{
	SslCertificate *certificate;
	int i;

	if (store != NULL &&
	    sslAddReference(&store->references, -1, 11,
			    smallCertificateStoreSource, 63) == 0) {
		for (i = 0; i < store->count; i++) {
			certificate = store->entries[i].certificate;
			if (certificate != NULL &&
			    certificate->methods->DestroyCertificate != NULL)
				certificate->methods->DestroyCertificate(
					certificate);
		}
		sslFree(store->entries);
		SceSslInternal_4725F9E7(store->library);
		sslFree(store);
	}
	return 0;
}

int sslGetSmallCertificateStoreInfo(SslSmallCertificateStore *store,
				    int selector, void *result)
{
	int status = 0;

	if (selector == 1)
		*(SslLibrary **)result = store->library;
	else
		status = 10022;
	return status;
}

int sslSetSmallCertificateStoreInfo(SslSmallCertificateStore *store,
				    int selector, void *value)
{
	(void)store;
	(void)selector;
	(void)value;
	return 10018;
}

int sslOpenSmallCertificateStore(SslSmallCertificateStore *store, int mode)
{
	(void)store;
	(void)mode;
	return 0;
}

int sslCloseSmallCertificateStore(SslSmallCertificateStore *store, int mode)
{
	(void)store;
	(void)mode;
	return 0;
}

int sslCreateSmallCertificateStoreCursor(SslSmallCertificateStore *store,
					 SslSmallCertificateStoreCursor **result)
{
	SslSmallCertificateStoreCursor *cursor;
	int status = 0;

	cursor = sslAllocate(sizeof(*cursor));
	*result = cursor;
	if (cursor == NULL) {
		status = 10005;
	} else {
		sslFillMemory(cursor, 0, sizeof(*cursor));
		(*result)->methods = store->methods;
		(*result)->store = store;
		sslRetainCertificateStore(store);
	}
	return status;
}

int sslFreeSmallCertificateStoreCursor(SslSmallCertificateStoreCursor *cursor)
{
	SslCertificate *certificate;

	if (cursor != NULL) {
		if (cursor->store != NULL)
			sslFreeSmallCertificateStore(cursor->store);
		certificate = cursor->entry.certificate;
		if (certificate != NULL &&
		    certificate->methods->DestroyCertificate != NULL)
			certificate->methods->DestroyCertificate(certificate);
		sslFree(cursor);
	}
	return 0;
}

int sslResetSmallCertificateStoreCursor(SslSmallCertificateStoreCursor *cursor)
{
	SslCertificate *certificate = cursor->entry.certificate;

	if (certificate != NULL &&
	    certificate->methods->DestroyCertificate != NULL)
		certificate->methods->DestroyCertificate(certificate);
	cursor->entry.certificate = NULL;
	cursor->entry.keyId.length = 0;
	cursor->entry.keyId.data = NULL;
	cursor->entry.flags = 0;
	cursor->entry.status = 0;
	return 0;
}

int sslDeleteSmallCertificateStoreEntry(SslSmallCertificateStoreCursor *cursor)
{
	SslSmallCertificateStore *store = cursor->store;
	SslCertificate *certificate;
	int status = 10008;
	int i;

	for (i = 0; i < store->count; i++) {
		if ((cursor->entry.keyId.data != NULL &&
		     store->entries[i].keyId.data != NULL &&
		     cursor->entry.keyId.length == store->entries[i].keyId.length &&
		     sslCompareMemory(cursor->entry.keyId.data,
				      store->entries[i].keyId.data,
				      cursor->entry.keyId.length) == 0) ||
		    (cursor->entry.keyId.data == NULL &&
		     cursor->entry.certificate == store->entries[i].certificate)) {
			store->count--;
			certificate = store->entries[i].certificate;
			if (certificate != NULL &&
			    certificate->methods->DestroyCertificate != NULL)
				certificate->methods->DestroyCertificate(
					certificate);
			sslMoveMemory(&store->entries[i],
				      &store->entries[i + 1],
				      sizeof(*store->entries) *
					      (SceUInt32)(i - store->count));
			status = 0;
			break;
		}
	}
	return status;
}

int sslInsertSmallCertificateStoreEntry(SslSmallCertificateStoreCursor *cursor)
{
	SslSmallCertificateStore *store = cursor->store;
	SslSmallCertificateStoreEntry *entries;
	SslCertificate *certificate;
	int status;

	status = sslDeleteSmallCertificateStoreEntry(cursor);
	if (status == 0 || status == 10008) {
		status = 0;
		if (store->count == store->capacity) {
			entries = store->entries;
			store->entries = sslReallocate(
				entries,
				(SceUInt32)(store->capacity + 5) *
					sizeof(*store->entries),
				(SceUInt32)store->capacity *
					sizeof(*store->entries));
			if (store->entries == NULL) {
				status = 10005;
				store->entries = entries;
				return status;
			}
			store->capacity += 5;
		}
		sslCopyMemory(&store->entries[store->count], &cursor->entry,
			      sizeof(cursor->entry));
		store->count++;
		certificate = cursor->entry.certificate;
		if (certificate != NULL &&
		    certificate->methods->RetainCertificate != NULL)
			certificate->methods->RetainCertificate(certificate, 1,
								0);
	}
	return status;
}

int sslFindSmallCertificateStoreEntry(SslSmallCertificateStoreCursor *cursor,
				      int selector,
				      SslCertificateStoreQuery *query)
{
	SslSmallCertificateStore *store;
	void *subjectObject;
	SslCertificateName *subject;
	SslCertificate *certificate;
	int matched;
	int status = 10008;
	int i;

	if (selector == 0xa101) {
		store = cursor->store;
		for (i = 0; i < store->count; i++) {
			subjectObject = NULL;
			status = SceSslInternal_08179EBD(
				store->entries[i].certificate, 1,
				&subjectObject);
			if (status != 0)
				break;
			subject = subjectObject;
			matched = SceSslInternal_568F0E80(subject, query->value);
			if (subject != NULL && subject->methods->Destroy != NULL)
				subject->methods->Destroy(subject);
			if (matched) {
				sslCopyMemory(&cursor->entry,
					      &store->entries[i],
					      sizeof(cursor->entry));
				certificate = cursor->entry.certificate;
				if (certificate != NULL &&
				    certificate->methods->RetainCertificate != NULL)
					certificate->methods->RetainCertificate(
						certificate, 1, 0);
				status = 0;
				break;
			}
		}
	} else {
		status = 10011;
	}
	return status;
}

int sslGetSmallCertificateStoreCursorInfo(SslSmallCertificateStoreCursor *cursor,
					  int selector, void *result)
{
	SslCertificate *certificate;
	int status = 0;

	switch (selector) {
	case 4:
		certificate = cursor->entry.certificate;
		*(SslCertificate **)result = certificate;
		if (certificate != NULL &&
		    certificate->methods->RetainCertificate != NULL)
			certificate->methods->RetainCertificate(certificate, 1,
								0);
		break;
	case 5:
		*(SslCertificateExtensionData *)result = cursor->entry.keyId;
		break;
	case 6:
		*(SceUInt32 *)result = cursor->entry.flags;
		break;
	case 7:
		*(SceUInt32 *)result = cursor->entry.status;
		break;
	case 10:
		status = 10008;
		break;
	default:
		status = 10011;
		break;
	}
	return status;
}

int sslSetSmallCertificateStoreCursorInfo(SslSmallCertificateStoreCursor *cursor,
					  int selector, void *value)
{
	int status = 0;

	switch (selector) {
	case 4:
		cursor->entry.certificate = value;
		break;
	case 5:
		cursor->entry.keyId = *(SslCertificateExtensionData *)value;
		break;
	case 6:
		cursor->entry.flags = *(SceUInt32 *)value;
		break;
	case 7:
		cursor->entry.status = *(SceUInt32 *)value;
		break;
	default:
		status = 10011;
		break;
	}
	return status;
}

int sslSetCertificateStoreCursorCertificate(void *cursor, void *certificate,
					    void *keyId)
{
	SslSmallCertificateStoreCursor *object = cursor;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL && certificate != NULL) {
		status = object->methods->SetCursorInfo(object, 4, certificate);
		if (status == 0 && keyId != NULL)
			status = object->methods->SetCursorInfo(object, 5, keyId);
	}
	return status;
}

int sslGetCertificateStoreCursorCertificate(void *cursor, void **certificate,
					    SceUInt32 *keyId)
{
	SslSmallCertificateStoreCursor *object = cursor;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL && certificate != NULL) {
		status = object->methods->GetCursorInfo(object, 4, certificate);
		if (status == 0 && keyId != NULL && *keyId != 0)
			status = object->methods->GetCursorInfo(object, 5, keyId);
	}
	return status;
}

int sslSetCertificateStoreCursorFlags(void *cursor, int flags)
{
	SslSmallCertificateStoreCursor *object = cursor;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL)
		status = object->methods->SetCursorInfo(object, 6, &flags);
	return status;
}

int sslGetCertificateStoreCursorFlags(void *cursor, int *flags)
{
	SslSmallCertificateStoreCursor *object = cursor;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL && flags != NULL)
		status = object->methods->GetCursorInfo(object, 6, flags);
	return status;
}

int sslFindCertificateStoreEntryByName(void *cursor, void *name, int mask)
{
	SslSmallCertificateStoreCursor *object = cursor;
	SslCertificateStoreQuery query = { 0 };
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL) {
		query.mask = (SceUInt32)mask;
		query.value = name;
		status = object->methods->FindEntry(object, 0xa101, &query);
	}
	return status;
}

int sslFindCertificateStoreEntryByKeyId(void *cursor, void *keyId, int mask)
{
	SslSmallCertificateStoreCursor *object = cursor;
	SslCertificateStoreQuery query = { 0 };
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL) {
		query.mask = (SceUInt32)mask;
		query.value = keyId;
		status = object->methods->FindEntry(object, 4, &query);
	}
	return status;
}
