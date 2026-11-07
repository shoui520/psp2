/* Source path recovered from the firmware; descriptive private names are ours. */
#include <libssl/object.h>
#include <libssl/name.h>
#include "r_verify.h"
#include "../cert/src/extension.h"
#include "../../../memory.h"
#include "../../../runtime.h"
#include "../../../objects/objects.h"

char *sslStackSetValue(STACK *st, int loc, char *data);

static const char verifySource[] = "S:\\work\\MES\\common\\verify/r_verify.c";

int sslSetVerifyChainEntryError(SslVerifyChainEntry *entry, int error)
{
	int status = 0;

	if (entry == NULL) {
		status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	} else {
		entry->error = error;
	}
	return status;
}

int sslSetVerifyChainEntryFlags(SslVerifyChainEntry *entry, SceUInt32 flags)
{
	int status = 0;

	if (entry == NULL) {
		status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	} else {
		entry->flags = flags;
	}
	return status;
}

int sslGetVerifyChainEntryInfo(SslVerifyChainEntry *entry, int selector,
			       void *result)
{
	int status = 0;

	if (entry == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	switch (selector) {
	case 1:
		*(SslCertificate **)result = entry->certificate;
		break;
	case 2:
		*(SslObjectHeader **)result = entry->crl;
		break;
	case 3:
		*(void **)result = entry->publicKey;
		break;
	case 4:
		*(SceUInt32 *)result = entry->flags;
		break;
	case 5:
		*(int *)result = entry->error;
		break;
	default:
		status = 10018;
		break;
	}
	return status;
}

int sslGetVerifyChainEntryFlags(SslVerifyChainEntry *entry, SceUInt32 *result)
{
	return sslGetVerifyChainEntryInfo(entry, 4, result);
}

int sslGetVerifyContextInfo(SslVerifyContext *context, int selector,
			    void *result)
{
	int status = 0;

	if (context == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	switch (selector) {
	case 1:
		*(SslLibrary **)result = context->library;
		break;
	case 2:
		*(int *)result = context->maxDepth;
		break;
	case 3:
		*(SceUInt32 *)result = context->flags;
		break;
	case 4:
		*(SslVerifyCallbackInfo **)result =
			(SslVerifyCallbackInfo *)&context->crlCallback;
		break;
	case 5:
		*(void **)result = context->crlCallback;
		break;
	case 6:
		*(void **)result = context->crlArg;
		break;
	case 7:
		*(void **)result = context->applicationData;
		break;
	case 11:
		*(SslVerifyCallbackInfo **)result =
			(SslVerifyCallbackInfo *)&context->verifyCallback;
		break;
	case 14:
		*(SceUInt32 *)result = context->policyFlags;
		break;
	case 15:
		*(SceUInt32 *)result = context->moduleFlags | 0x141u;
		break;
	default:
		status = 10018;
		break;
	}
	return status;
}

int sslGetVerifyContextFlags(SslVerifyContext *context, SceUInt32 *result)
{
	return sslGetVerifyContextInfo(context, 3, result);
}

int sslGetVerifyStateInfo(SslVerifyState *state, int selector, void *result)
{
	SslVerifyTimeRange *timeRange;
	SslVerifyIndexedChainEntry *indexedEntry;
	STACK *chain;
	int index;
	int status = 0;

	if (state == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	switch (selector) {
	case 0:
		*(SslVerifyContext **)result = state->context;
		break;
	case 1:
		timeRange = result;
		timeRange->flags = state->timeFlags;
		timeRange->startTime = state->startTime;
		timeRange->endTime = state->endTime;
		break;
	case 2:
		*(SceUInt32 *)result = state->timeFlags;
		break;
	case 3:
		*(void **)result = state->startTime;
		break;
	case 4:
		*(void **)result = state->endTime;
		break;
	case 5:
		*(int *)result = state->errorDepth;
		break;
	case 6:
		*(int *)result = state->error;
		break;
	case 7:
		chain = state->chain;
		if (chain != NULL)
			*(int *)result = sk_num(chain);
		else
			*(int *)result = 0;
		break;
	case 8:
		indexedEntry = result;
		chain = state->chain;
		if (chain != NULL) {
			index = indexedEntry->index;
			if (index >= 0 && index <= sk_num(chain))
				indexedEntry->entry =
					(SslVerifyChainEntry *)sk_value(chain,
									index);
			else
				status = 10018;
		} else {
			indexedEntry->entry = NULL;
		}
		break;
	case 9:
		*(SceUInt32 *)result = state->flags & 1u;
		break;
	default:
		status = 10018;
		break;
	}
	return status;
}

int sslGetVerifyStateContext(SslVerifyState *state, SslVerifyContext **result)
{
	return sslGetVerifyStateInfo(state, 0, result);
}

int sslValidateVerifyChainEntry(SslVerifyState *state,
				SslVerifyChainEntry *entry)
{
	SslVerifyContext *context;
	SslCertificate *certificate;
	SceUInt32 entryFlags;
	SceUInt32 contextFlags;
	int entryError = 0;
	int status;

	status = sslGetVerifyStateContext(state, &context);
	if (status == 0) {
		status = sslGetVerifyContextFlags(context, &contextFlags);
		if (status == 0) {
			status = sslGetVerifyChainEntryFlags(entry,
							     &entryFlags);
			if (status == 0)
				status = sslGetVerifyChainEntryInfo(
					entry, 1, &certificate);
		}
	}
	if (status != 0) {
		entryError = 11;
	} else {
		status = sslValidateCertificateOptions(
			certificate, (int)(contextFlags & 0x3000u));
		if (status != 0) {
			entryError = 12;
			status = 0;
		} else {
			sslSetVerifyChainEntryFlags(entry, entryFlags | 0x40u);
		}
	}
	sslSetVerifyChainEntryError(entry, entryError);
	return status;
}

int sslPushVerifyStateChainEntry(SslVerifyState *state, int selector,
				 SslVerifyChainEntry *entry)
{
	STACK *chain;
	int status = 0;

	if (state == NULL || entry == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (selector != 1)
		return 10021;
	chain = state->chain;
	if (chain == NULL) {
		chain = sk_new(NULL);
		state->chain = chain;
		if (chain == NULL)
			return 10005;
	}
	if (sk_push(chain, (char *)entry) == 0)
		return 10005;
	return status;
}

int sslFreeVerifyChainEntryStruct(SslVerifyChainEntry *entry)
{
	int status = 0;

	if (entry == NULL) {
		status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	} else {
		sslFree(entry);
	}
	return status;
}

int sslSetVerifyChainEntryInfo(SslVerifyChainEntry *entry, int selector,
			       void *value)
{
	SslCertificate *certificate;
	SslObjectHeader *crl;
	void *publicKey;
	int status = 0;

	if (entry == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	switch (selector) {
	case 1:
		certificate = entry->certificate;
		if (certificate != NULL && (entry->flags & 0x20u) != 0)
			certificate->methods->DestroyCertificate(certificate);
		entry->certificate = value;
		break;
	case 2:
		crl = entry->crl;
		if (crl != NULL && (entry->flags & 0x20u) != 0)
			crl->methods->Destroy(crl);
		entry->crl = value;
		break;
	case 3:
		publicKey = entry->publicKey;
		if (publicKey != NULL && (entry->flags & 0x20u) != 0)
			SceSslInternal_46358909(publicKey);
		entry->publicKey = value;
		break;
	case 4:
		entry->flags = *(SceUInt32 *)value;
		break;
	case 5:
		entry->error = *(int *)value;
		break;
	default:
		status = 10018;
		break;
	}
	return status;
}

int sslNewVerifyChainEntry(SslVerifyState *state, SceUInt32 flags,
			   SslVerifyChainEntry **result)
{
	SslVerifyChainEntry *entry;

	if (state == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	entry = sslAllocate(sizeof(*entry));
	if (entry == NULL)
		return 10005;
	sslFillMemory(entry, 0, sizeof(*entry));
	*result = entry;
	entry->flags = flags;
	return 0;
}

int sslSetVerifyStateInfo(SslVerifyState *state, int selector, void *value)
{
	SslVerifyTimeRange *timeRange;
	int status = 0;

	if (state == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	switch (selector) {
	case 1:
		timeRange = value;
		state->timeFlags = timeRange->flags;
		state->startTime = timeRange->startTime;
		state->endTime = timeRange->endTime;
		break;
	case 2:
		state->timeFlags = *(SceUInt32 *)value;
		break;
	case 3:
		state->startTime = value;
		break;
	case 4:
		state->endTime = value;
		break;
	case 5:
		state->errorDepth = *(int *)value;
		break;
	case 6:
		state->error = *(int *)value;
		break;
	default:
		status = 10018;
		break;
	}
	return status;
}

int sslSetVerifyStateError(SslVerifyState *state, int error, int depth)
{
	int status;

	status = sslSetVerifyStateInfo(state, 5, &depth);
	if (status == 0)
		status = sslSetVerifyStateInfo(state, 6, &error);
	return status;
}

int sslAppendVerifyChainCertificate(SslVerifyState *state,
				    SslCertificate *certificate, int unused,
				    SceUInt32 flags,
				    SslVerifyChainEntry **result)
{
	SslVerifyChainEntry *entry = NULL;
	SslCertificateName *subjectName = NULL;
	SslCertificateName *issuerName = NULL;
	SslVerifyContext *context;
	int status;

	(void)unused;
	status = sslGetVerifyStateInfo(state, 0, &context);
	if (status == 0) {
		status = sslNewVerifyChainEntry((SslVerifyState *)context, 0,
						&entry);
		if (status == 0) {
			status = SceSslInternal_08179EBD(
				certificate, 1, (void **)&subjectName);
			if (status == 0) {
				status = SceSslInternal_81FA5DBE(
					certificate, 1, (void **)&issuerName);
				if (status == 0) {
					if (SceSslInternal_568F0E80(
						    subjectName, issuerName))
						flags |= 1u;
					status = sslSetVerifyChainEntryFlags(
						entry, flags);
					if (status == 0) {
						status =
							sslSetVerifyChainEntryInfo(
								entry, 1,
								certificate);
						if (status == 0) {
							status =
								sslPushVerifyStateChainEntry(
									state,
									1,
									entry);
							*result = entry;
						}
					}
				}
			}
		}
	}
	if (status != 0 && entry != NULL)
		sslFreeVerifyChainEntryStruct(entry);
	if (subjectName != NULL)
		subjectName->methods->Destroy(subjectName);
	if (issuerName != NULL)
		issuerName->methods->Destroy(issuerName);
	return status;
}

static __inline int sslVerifyCursorFindByName(
	SslSmallCertificateStoreCursor *cursor, SslCertificateName *name,
	SceUInt32 mask)
{
	SslCertificateStoreQuery query = { 0 };
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (cursor != NULL) {
		query.mask = mask;
		query.value = name;
		status = cursor->methods->FindEntry(cursor, 0xa101, &query);
	}
	return status;
}

static __inline int sslVerifyCursorGetFlags(
	SslSmallCertificateStoreCursor *cursor, int *flags)
{
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (cursor != NULL && flags != NULL)
		status = cursor->methods->GetCursorInfo(cursor, 6, flags);
	return status;
}

static __inline int sslVerifyCursorGetCertificate(
	SslSmallCertificateStoreCursor *cursor, SslCertificate **certificate)
{
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (cursor != NULL && certificate != NULL)
		status = cursor->methods->GetCursorInfo(cursor, 4, certificate);
	return status;
}

int sslBuildVerifyCertificateChain(SslVerifyState *state,
				   SslSmallCertificateStore *store,
				   STACK *certificates, int takeOwnership)
{
	int chainError = 0;
	SceUInt32 contextFlags;
	int cursorFlags;
	SceUInt32 entryFlags = 0;
	SslCertificate *storeCert = NULL;
	SslCertificate *issuerCert = NULL;
	SslCertificateName *issuerName = NULL;
	SslCertificateName *subjectName = NULL;
	SslVerifyContext *context;
	SslSmallCertificateStoreCursor *cursor = NULL;
	SslVerifyChainEntry *entry;
	int maxDepth;
	const SslObjectMethods *extensionMethods = NULL;
	SslCertificateExtension *subjectKeyIdExt = NULL;
	SslCertificateExtension *authorityKeyIdExt = NULL;
	SslCertificateExtensionData authorityKeyId;
	SslCertificateExtensionData subjectKeyId;
	SslCertificate *prevCert;
	int appendedCount = 0;
	int storeChainCount;
	int chainCount;
	int foundTrusted = 0;
	int errorDepth = 0;
	int i, j, status;

	status = sslGetVerifyStateContext(state, &context);
	if (status != 0)
		goto cleanup;
	status = sslGetVerifyContextFlags(context, &contextFlags);
	if (status != 0)
		goto cleanup;
	if ((contextFlags & 1u) != 0 || (contextFlags & 0x40u) != 0) {
		if (store == NULL) {
			status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;
			goto cleanup;
		}
		status = store->methods->CreateCursor(store, &cursor);
		if (status != 0)
			goto cleanup;
	}
	if (takeOwnership == 1)
		entryFlags |= 0x20u;
	issuerCert = NULL;
	storeChainCount = 0;
	chainCount = 0;
	prevCert = NULL;
	if ((contextFlags & 0x100u) != 0) {
		int status1, status2;

		status = sslGetVerifyContextInfo(context, 1, &extensionMethods);
		status1 = SceSslInternal_BF99D664(extensionMethods, 0,
						  &authorityKeyIdExt);
		status2 = SceSslInternal_BF99D664(extensionMethods, 0,
						  &subjectKeyIdExt);
		if ((status | status1 | status2) != 0)
			goto cleanup;
		for (i = 0; i + 1 < sk_num(certificates); i++) {
			SslCertificate *cert =
				(SslCertificate *)sk_value(certificates, i);

			if (cert->methods->GetCertificateInfo(
				    cert, 0x4010, authorityKeyIdExt) != 0 ||
			    SceSslInternal_93EAE08A(authorityKeyIdExt, 1,
						    &authorityKeyId) != 0) {
				authorityKeyId.data = NULL;
				status = SceSslInternal_81FA5DBE(
					cert, 1, (void **)&issuerName);
				if (status != 0)
					goto cleanup;
			}
			for (j = i + 1; j < sk_num(certificates); j++) {
				SslCertificate *candidate =
					(SslCertificate *)sk_value(certificates,
								   j);
				int matched = 0;

				if (authorityKeyId.data != NULL) {
					if (candidate->methods
						    ->GetCertificateInfo(
							    candidate, 0x4000,
							    subjectKeyIdExt) ==
						    0 &&
					    SceSslInternal_93EAE08A(
						    subjectKeyIdExt, 0x8009,
						    &subjectKeyId) == 0 &&
					    subjectKeyId.length ==
						    authorityKeyId.length &&
					    sslCompareMemory(
						    subjectKeyId.data,
						    authorityKeyId.data,
						    authorityKeyId.length) ==
						    0) {
						matched = 1;
					}
				} else {
					status = SceSslInternal_08179EBD(
						candidate, 1,
						(void **)&subjectName);
					if (status != 0)
						goto cleanup;
					if (SceSslInternal_568F0E80(issuerName,
								    subjectName))
						matched = 1;
					subjectName->methods->Destroy(
						subjectName);
					subjectName = NULL;
				}
				if (matched != 0)
					break;
			}
			if (issuerName != NULL) {
				issuerName->methods->Destroy(issuerName);
				issuerName = NULL;
			}
			if (j != i + 1 && j < sk_num(certificates)) {
				char *temp = sk_value(certificates, i + 1);

				sslStackSetValue(certificates, i + 1,
						 sk_value(certificates, j));
				sslStackSetValue(certificates, j, temp);
			}
		}
	}
	for (i = 0; i < sk_num(certificates); i++) {
		SslCertificate *cert =
			(SslCertificate *)sk_value(certificates, i);

		if (prevCert != NULL) {
			status = SceSslInternal_81FA5DBE(prevCert, 1,
							 (void **)&issuerName);
			if (status != 0)
				goto cleanup;
			status = SceSslInternal_08179EBD(cert, 1,
							 (void **)&subjectName);
			if (status != 0)
				goto cleanup;
			if (!SceSslInternal_568F0E80(issuerName, subjectName)) {
				chainError = 1;
				break;
			}
			subjectName->methods->Destroy(subjectName);
			subjectName = NULL;
			issuerName->methods->Destroy(issuerName);
			issuerName = NULL;
		} else {
			int findStatus;

			status = SceSslInternal_08179EBD(cert, 1,
							 (void **)&subjectName);
			if (status != 0)
				goto cleanup;
			findStatus = sslVerifyCursorFindByName(
				cursor, subjectName, 0xffff);
			subjectName->methods->Destroy(subjectName);
			subjectName = NULL;
			if (findStatus == 0 &&
			    sslVerifyCursorGetFlags(cursor, &cursorFlags) ==
				    0 &&
			    (cursorFlags & 1) != 0 &&
			    sslVerifyCursorGetCertificate(cursor, &storeCert) ==
				    0) {
				if (sslAreCertificatesEqual(storeCert, cert)) {
					foundTrusted = 1;
					entryFlags |= 2u;
				}
				storeCert->methods->DestroyCertificate(
					storeCert);
				storeCert = NULL;
			}
		}
		if ((contextFlags & 1u) != 0 || (contextFlags & 0x40u) != 0) {
			status = SceSslInternal_81FA5DBE(cert, 1,
							 (void **)&issuerName);
			if (status != 0)
				goto cleanup;
			status = sslVerifyCursorFindByName(cursor, issuerName,
							   0xffff);
			issuerName->methods->Destroy(issuerName);
			issuerName = NULL;
			if (status == 0) {
				status = sslVerifyCursorGetCertificate(
					cursor, &issuerCert);
				if (status == 0) {
					status = sslVerifyCursorGetFlags(
						cursor, &cursorFlags);
					if (status != 0 ||
					    (cursorFlags & 1) == 0) {
						goto release_issuer_cert;
					}
					if ((cursorFlags & 8) != 0)
						entryFlags |= 0x8000u;
				}
			}
			if (status == 10008) {
release_issuer_cert:
				if (issuerCert != NULL)
					issuerCert->methods->DestroyCertificate(
						issuerCert);
				issuerCert = NULL;
			} else if (status != 0) {
				goto cleanup;
			}
		}
		status = sslAppendVerifyChainCertificate(
			state, cert, 0, entryFlags, &entry);
		if (status != 0)
			goto cleanup;
		appendedCount++;
		if ((entryFlags & 2u) == 0) {
			chainCount++;
			status = sslGetVerifyChainEntryInfo(entry, 4,
							    &entryFlags);
			if (status != 0)
				goto cleanup;
			if ((entryFlags & 1u) != 0 &&
			    i != sk_num(certificates) - 1) {
				chainError = 6;
				break;
			}
		}
		if (issuerCert != NULL) {
			status = sslAppendVerifyChainCertificate(
				state, issuerCert, 0, 0x22u, &entry);
			if (status != 0)
				goto cleanup;
			storeChainCount = 1;
			if ((contextFlags & 0x40u) == 0) {
				issuerCert = NULL;
				goto check_depth;
			}
			status = sslGetVerifyChainEntryInfo(entry, 4,
							    &entryFlags);
			if (status != 0)
				goto cleanup;
			while ((entryFlags & 1u) == 0) {
				SslCertificate *currentCert = issuerCert;

				issuerCert = NULL;
				status = SceSslInternal_81FA5DBE(
					currentCert, 1, (void **)&issuerName);
				if (status != 0)
					goto cleanup;
				status = sslVerifyCursorFindByName(
					cursor, issuerName, 0xffff);
				issuerName->methods->Destroy(issuerName);
				issuerName = NULL;
				if (status == 0) {
					status = sslVerifyCursorGetCertificate(
						cursor, &issuerCert);
					if (status == 0) {
						status =
							sslVerifyCursorGetFlags(
								cursor,
								&cursorFlags);
						if (status != 0 ||
						    (cursorFlags & 1) == 0)
							status = 10008;
					}
				}
				if (status != 0) {
					if (issuerCert != NULL)
						issuerCert->methods
							->DestroyCertificate(
								issuerCert);
					issuerCert = NULL;
					break;
				}
				status = sslAppendVerifyChainCertificate(
					state, issuerCert, 0, entryFlags,
					&entry);
				if (status != 0)
					goto cleanup;
				storeChainCount++;
				status = sslGetVerifyChainEntryInfo(
					entry, 4, &entryFlags);
				if (status != 0)
					goto cleanup;
			}
			if (status == 0) {
				issuerCert = NULL;
				goto check_depth;
			}
			break;
		}
		prevCert = cert;
	}
	if (foundTrusted == 0 &&
	    ((contextFlags & 1u) != 0 || (contextFlags & 0x40u) != 0)) {
		chainError = 1;
		goto set_error;
	}
check_depth:
	status = sslGetVerifyContextInfo(context, 2, &maxDepth);
	if (status == 0) {
		if (storeChainCount + chainCount > maxDepth)
			chainError = 7;
set_error:
		if (chainError == 0)
			i = errorDepth;
		status = sslSetVerifyStateError(state, chainError, i);
	}
cleanup:
	if (authorityKeyIdExt != NULL)
		SceSslInternal_F030A0DE(authorityKeyIdExt);
	if (subjectKeyIdExt != NULL)
		SceSslInternal_F030A0DE(subjectKeyIdExt);
	if (appendedCount < sk_num(certificates) && takeOwnership == 1) {
		while (appendedCount < sk_num(certificates)) {
			SslCertificate *removed =
				(SslCertificate *)sk_delete(certificates,
							    appendedCount);
			removed->methods->DestroyCertificate(removed);
		}
	}
	if (issuerCert != NULL)
		issuerCert->methods->DestroyCertificate(issuerCert);
	if (subjectName != NULL)
		subjectName->methods->Destroy(subjectName);
	if (issuerName != NULL)
		issuerName->methods->Destroy(issuerName);
	if (cursor != NULL)
		cursor->methods->DestroyCursor(cursor);
	return status;
}

int sslBuildVerifyCertificateChainArray(SslVerifyState *state,
					SslSmallCertificateStore *store,
					SslCertificate **certificates,
					int count, int takeOwnership)
{
	STACK *stack = NULL;
	int i;
	int status;

	if (state != NULL && certificates != NULL) {
		stack = sk_new(NULL);
		if (stack != NULL) {
			for (i = 0; i < count; i++) {
				if (sk_push(stack, (char *)certificates[i]) == 0) {
					status = 10005;
					goto free_owned;
				}
			}
			status = sslBuildVerifyCertificateChain(
				state, store, stack, takeOwnership);
			goto cleanup;
		}
		status = 10005;
	} else {
		status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	}
free_owned:
	if (takeOwnership == 1) {
		for (i = 0; i < count; i++)
			certificates[i]->methods->DestroyCertificate(
				certificates[i]);
	}
cleanup:
	if (stack != NULL)
		sk_free(stack);
	return status;
}

int sslRegisterVerifyModule(SslVerifyContext *context, SslVerifyModule *module,
			    int replace)
{
	STACK *modules = context->modules;
	int i;

	for (i = 0; i < sk_num(modules); i++) {
		if (((SslVerifyModule *)sk_value(modules, i))->flags ==
		    module->flags) {
			if (replace != 0)
				sk_value(modules, i) = (char *)module;
			return 0;
		}
	}
	if (sk_push(modules, (char *)module) <= 0)
		return 10005;
	context->moduleFlags |= module->flags;
	return 0;
}

int sslFreeVerifyContext(SslVerifyContext *context)
{
	if (context != NULL &&
	    sslAddReference(&context->references, -1, 28, verifySource, 407) ==
		    0) {
		if (context->modules != NULL)
			sk_free(context->modules);
		if (context->policies != NULL)
			sk_pop_free(context->policies,
				    (void (*)())sslFreeObjectIdentifier);
		if (context->library != NULL)
			SceSslInternal_4725F9E7(context->library);
		sslFree(context);
	}
	return 0;
}

int sslNewVerifyContext(SslLibrary *library, int unused,
			SslVerifyContext **result)
{
	SslVerifyContext *context = NULL;
	SslLibraryModule *table;
	SslLibraryModule *next = NULL;
	SslLibraryModule *module = NULL;
	int status;

	(void)unused;
	if (library != NULL && result != NULL) {
		context = sslAllocate(sizeof(*context));
		if (context == NULL) {
			status = 10005;
			goto cleanup;
		}
	}
	sslFillMemory(context, 0, sizeof(*context));
	context->library = library;
	context->references = 1;
	sslLibraryAddReference(library);
	context->flags = 0x718fu;
	context->maxDepth = 10;
	context->supportedFlags = 0x9eu;
	context->modules = sk_new(NULL);
	if (context->modules == NULL) {
		status = 10005;
		goto cleanup;
	}
	context->policies = sk_new(NULL);
	if (context->policies == NULL) {
		status = 10005;
		goto cleanup;
	}
	context->policyFlags = 0x7fu;
	table = sslGetLibraryModuleTable(library);
	if (table == NULL) {
		status = 10001;
		goto cleanup;
	}
	for (;;) {
		SslVerifyModule *impl;

		status = sslFindLibraryModule(table, 22, 24, 0, 0, &module,
					      &next);
		if (status == 10008) {
			*result = context;
			return 0;
		}
		if (status != 0)
			goto cleanup;
		impl = module->GetImplementation(NULL);
		if (impl != NULL) {
			status = sslRegisterVerifyModule(context, impl, 0);
			if (status != 0)
				goto cleanup;
		}
		table = next;
	}
cleanup:
	sslFreeVerifyContext(context);
	return status;
}

int sslRetainVerifyContext(SslVerifyContext *context)
{
	int status = 0;

	if (context == NULL) {
		status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	} else {
		sslAddReference(&context->references, 1, 28, verifySource, 380);
	}
	return status;
}

int sslSetVerifyContextInfo(SslVerifyContext *context, int selector,
			    void *value)
{
	SslVerifyCallbackInfo *callbackInfo;
	int status = 0;

	if (context == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	switch (selector) {
	case 1:
		status = 10011;
		break;
	case 2:
		context->maxDepth = *(int *)value;
		break;
	case 3:
		context->flags = *(SceUInt32 *)value;
		break;
	case 4:
		callbackInfo = value;
		context->crlCallback = callbackInfo->callback;
		context->crlArg = callbackInfo->arg;
		break;
	case 5:
		context->crlCallback = value;
		break;
	case 6:
		context->crlArg = value;
		break;
	case 7:
		context->applicationData = value;
		break;
	case 11:
		callbackInfo = value;
		context->verifyCallback =
			(SslVerifyCallback)callbackInfo->callback;
		context->verifyArg = callbackInfo->arg;
		break;
	case 12:
		if (sk_push(context->policies, value) == 0)
			status = 10005;
		break;
	case 13:
		sslRegisterVerifyModule(context, value, 1);
		break;
	case 14:
		context->policyFlags = *(SceUInt32 *)value;
		break;
	default:
		status = 10018;
		break;
	}
	return status;
}

int sslSetVerifyContextFlags(SslVerifyContext *context, SceUInt32 flags)
{
	return sslSetVerifyContextInfo(context, 3, &flags);
}

int sslGetVerifyChainEntryCertificate(SslVerifyChainEntry *entry, int selector,
				      void **result)
{
	SslCertificate *certificate;
	int status;

	status = sslGetVerifyChainEntryInfo(entry, 1, &certificate);
	if (status == 0) {
		if ((selector & 0x2000) != 0 && (selector & 0x1000) == 0) {
			*result = certificate;
		} else {
			status = 10018;
			if ((selector & 0x2000) == 0 &&
			    (selector & 0x1000) != 0)
				status = ((int (*)(SslCertificate *, int,
						   void **))certificate->methods
						  ->opaqueInterface[1])(
					certificate, 0, result);
		}
	}
	return status;
}

int sslGetVerifyChainEntryError(SslVerifyChainEntry *entry, int *result)
{
	return sslGetVerifyChainEntryInfo(entry, 5, result);
}

int sslGetVerifyStateError(SslVerifyState *state, int *result)
{
	return sslGetVerifyStateInfo(state, 6, result);
}

int sslGetVerifyStateChainLength(SslVerifyState *state, int *result)
{
	return sslGetVerifyStateInfo(state, 7, result);
}

int sslGetVerifyStateChainEntry(SslVerifyState *state, int index,
				SslVerifyChainEntry **result)
{
	SslVerifyIndexedChainEntry indexedEntry;
	int status;

	indexedEntry.index = index;
	status = sslGetVerifyStateInfo(state, 8, &indexedEntry);
	if (status == 0)
		*result = indexedEntry.entry;
	return status;
}

int sslNewVerifyState(SslVerifyContext *context, void *arg,
		      SslVerifyState **result)
{
	SslVerifyState *state;

	if (context == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	state = sslAllocate(sizeof(*state));
	if (state == NULL)
		return 10005;
	sslFillMemory(state, 0, sizeof(*state));
	state->applicationData = arg;
	state->context = context;
	sslRetainVerifyContext(context);
	*result = state;
	return 0;
}

int sslSetVerifyStateTimeRange(SslVerifyState *state, SceUInt32 flags,
			       void *startTime, void *endTime)
{
	int status = 0;

	if (state == NULL) {
		status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	} else {
		state->timeFlags = flags;
		state->startTime = startTime;
		state->endTime = endTime;
	}
	return status;
}

int sslFreeVerifyState(SslVerifyState *state)
{
	if (state == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	sslFreeVerifyContext(state->context);
	if (state->chain != NULL)
		sk_pop_free(state->chain,
			    (void (*)())sslFreeVerifyChainEntry);
	sslFree(state);
	return 0;
}

int sslSetVerifyStateChain(SslVerifyState *state, STACK *chain)
{
	int status = 0;

	if (state == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (state->chain != NULL)
		sk_pop_free(state->chain,
			    (void (*)())sslFreeVerifyChainEntry);
	state->chain = chain;
	return status;
}

int sslUpdateVerifyChainEntryError(SslVerifyContext *context,
				   SslVerifyState *state,
				   SslVerifyChainEntry *entry, int depth,
				   int *result)
{
	int stateError;
	int status;

	status = sslGetVerifyChainEntryError(entry, result);
	if (status != 0)
		return status;
	if (*result != 0 && context->verifyCallback != NULL) {
		status = context->verifyCallback(entry, depth,
						 context->verifyArg);
		if (status != 0)
			return status;
		status = sslGetVerifyChainEntryError(entry, result);
		if (status != 0)
			return status;
	}
	if (*result != 0) {
		status = sslGetVerifyStateError(state, &stateError);
		if (status == 0 && stateError == 0)
			sslSetVerifyStateError(state, *result, depth);
	}
	return status;
}

int sslVerifyCertificateChain(SslVerifyState *state, int *result)
{
	SslVerifyChainEntry *entry = NULL;
	SslVerifyChainEntry *issuerEntry = NULL;
	int stateError;
	int maxDepth;
	int chainLength;
	SceUInt32 entryFlags;
	SceUInt32 contextFlags;
	SslVerifyContext *context;
	SslLibrary *library;
	SceUIntPtr factoryAddr;
	void *moduleData = NULL;
	STACK *verifiedChain;
	int (*postVerifyCallback)(SslVerifyState *state,
				  SslVerifyChainEntry *entry, int depth) = NULL;
	int i, j;
	int status;

	status = sslGetVerifyStateError(state, &stateError);
	if ((stateError | status) == 0) {
		status = sslGetVerifyStateContext(state, &context);
		if (status == 0) {
			status = sslGetVerifyContextFlags(context,
							  &contextFlags);
			if (status == 0 && contextFlags != 0) {
				status = sslGetVerifyContextInfo(context, 1,
								 &library);
				if (status == 0) {
					status = sslLookupLibraryMethod(
						library, 22, 8, 0, 0, 1,
						&factoryAddr);
					if (status == 0) {
						int (**(*factory)(void *))(
							SslVerifyState *,
							SslVerifyChainEntry *,
							int) =
							(void *)factoryAddr;

						status =
							sslGetLibraryModuleData(
								library, 22,
								&moduleData);
						if (status != 0)
							goto done;
						postVerifyCallback =
							(void *)factory(
								moduleData);
					} else if (status != 10008) {
						goto done;
					}
					status = sslGetVerifyStateChainLength(
						state, &chainLength);
					if (status == 0) {
						status = sslGetVerifyContextInfo(
							context, 2, &maxDepth);
						if (status == 0) {
							if (maxDepth <
							    chainLength)
								chainLength =
									maxDepth;
							verifiedChain =
								sk_new(NULL);
							if (verifiedChain ==
							    NULL) {
								status = 10005;
							} else {
								for (i = 0;
								     i <
								     chainLength;
								     i++) {
									status = sslGetVerifyStateChainEntry(
										state,
										i,
										&entry);
									if (status !=
									    0)
										break;
									sk_push(verifiedChain,
										(char *)entry);
									status = sslGetVerifyChainEntryFlags(
										entry,
										&entryFlags);
									if (status !=
									    0)
										break;
									if ((entryFlags &
									     2u) !=
										    0 &&
									    (contextFlags &
									     0x40u) ==
										    0)
										goto run_chain_modules;
									status = sslGetVerifyStateChainEntry(
										state,
										i + 1,
										&issuerEntry);
									if (status !=
									    0)
										break;
									if ((contextFlags &
									     0x3000u) !=
									    0) {
										status = sslValidateVerifyChainEntry(
											state,
											entry);
										if (status !=
										    0)
											break;
										status = sslUpdateVerifyChainEntryError(
											context,
											state,
											entry,
											i,
											&stateError);
										if (status !=
										    0)
											break;
										if (stateError !=
										    0) {
											if ((contextFlags &
											     0x40u) ==
											    0)
												goto run_chain_modules;
											continue;
										}
									}
									for (j = 0;
									     j <
									     sk_num(context->modules);
									     j++) {
										SslVerifyModule *module =
											(SslVerifyModule *)sk_value(
												context->modules,
												j);

										if (module !=
											    NULL &&
										    (module->flags &
										     contextFlags) !=
											    0 &&
										    module->VerifyCertificate !=
											    NULL) {
											status = module->VerifyCertificate(
												module->context,
												state,
												entry,
												issuerEntry,
												i);
											if (status !=
											    0)
												goto free_chain;
											status = sslUpdateVerifyChainEntryError(
												context,
												state,
												entry,
												i,
												&stateError);
											if (status !=
											    0)
												goto free_chain;
											if (stateError !=
												    0 &&
											    (contextFlags &
											     0x40u) ==
												    0)
												break;
										}
									}
									if (postVerifyCallback !=
									    NULL) {
										status = postVerifyCallback(
											state,
											entry,
											i);
										if (status !=
										    0)
											break;
									}
									if (context->verifyCallback !=
									    NULL) {
										status = context->verifyCallback(
											entry,
											i,
											context->verifyArg);
										if (status !=
										    0)
											break;
										sslGetVerifyChainEntryError(
											entry,
											&stateError);
										if (stateError !=
										    0)
											sslSetVerifyStateError(
												state,
												stateError,
												i);
									}
								}
								if (i >=
								    chainLength) {
run_chain_modules:
									for (j = 0;
									     j <
									     sk_num(context->modules);
									     j++) {
										SslVerifyModule *module =
											(SslVerifyModule *)sk_value(
												context->modules,
												j);

										if (module !=
											    NULL &&
										    (module->flags &
										     contextFlags) !=
											    0 &&
										    module->VerifyChain !=
											    NULL) {
											status = module->VerifyChain(
												module->context,
												state,
												verifiedChain);
											if (status !=
											    0)
												break;
										}
									}
								}
							}
free_chain:
							if (verifiedChain !=
							    NULL)
								sk_free(verifiedChain);
						}
					}
				}
			}
		}
	}
done:
	sslGetVerifyStateError(state, &stateError);
	*result = stateError == 0 ? 1 : 0;
	return status;
}

int sslFreeVerifyChainEntry(SslVerifyChainEntry *entry)
{
	if (entry != NULL) {
		if ((entry->flags & 0x20u) != 0) {
			if (entry->certificate != NULL)
				entry->certificate->methods->DestroyCertificate(
					entry->certificate);
			if (entry->crl != NULL)
				entry->crl->methods->Destroy(entry->crl);
			if (entry->publicKey != NULL)
				SceSslInternal_46358909(entry->publicKey);
		}
		sslFree(entry);
	}
	return 0;
}
