/* Private descriptive names and this header filename are ours; component path
 * follows MES/common/verify/r_verify.c.
 */
#ifndef LIBSSL_R_VERIFY_H
#define LIBSSL_R_VERIFY_H

#include <stddef.h>
#include "stack.h"
#include "../cert/src/ri_cert.h"
#include "../module/crt_stor/src/small_stor.h"

typedef struct SslVerifyChainEntry SslVerifyChainEntry;
typedef struct SslVerifyModule SslVerifyModule;
typedef struct SslVerifyContext SslVerifyContext;
typedef struct SslVerifyState SslVerifyState;

typedef int (*SslVerifyCallback)(SslVerifyChainEntry *entry, int depth,
				 void *arg);

typedef struct SslVerifyCallbackInfo {
	void *callback;
	void *arg;
} SslVerifyCallbackInfo;

typedef struct SslVerifyTimeRange {
	SceUInt32 flags;
	void *startTime;
	void *endTime;
} SslVerifyTimeRange;

typedef struct SslVerifyIndexedChainEntry {
	int index;
	SslVerifyChainEntry *entry;
	SceUInt32 reserved[2];
} SslVerifyIndexedChainEntry;

struct SslVerifyChainEntry {
	SslCertificate *certificate;
	SslObjectHeader *crl;
	void *publicKey;
	int error;
	SceUInt32 flags;
	SceUInt32 reserved;
};

struct SslVerifyModule {
	SceUInt32 type;
	SceUInt32 flags;
	SceUInt32 reserved;
	void *context;
	int (*VerifyCertificate)(void *context, SslVerifyState *state,
				 SslVerifyChainEntry *entry,
				 SslVerifyChainEntry *issuerEntry, int depth);
	int (*VerifyChain)(void *context, SslVerifyState *state, STACK *chain);
};

struct SslVerifyContext {
	int references;
	SslLibrary *library;
	SceUInt32 flags;
	int maxDepth;
	void *crlCallback;
	void *crlArg;
	SslVerifyCallback verifyCallback;
	void *verifyArg;
	void *applicationData;
	STACK *modules;
	SceUInt32 moduleFlags;
	SceUInt32 supportedFlags;
	STACK *policies;
	SceUInt32 policyFlags;
};

struct SslVerifyState {
	SslVerifyContext *context;
	SceUInt32 timeFlags;
	void *startTime;
	void *endTime;
	int error;
	int errorDepth;
	SceUInt32 flags;
	STACK *chain;
	void *applicationData;
};

_Static_assert(sizeof(SslVerifyChainEntry) == 24, "SslVerifyChainEntry");
_Static_assert(sizeof(SslVerifyContext) == 56, "SslVerifyContext");
_Static_assert(sizeof(SslVerifyState) == 36, "SslVerifyState");

int sslSetVerifyChainEntryError(SslVerifyChainEntry *entry, int error);
int sslSetVerifyChainEntryFlags(SslVerifyChainEntry *entry, SceUInt32 flags);
int sslGetVerifyChainEntryInfo(SslVerifyChainEntry *entry, int selector,
			       void *result);
int sslGetVerifyChainEntryFlags(SslVerifyChainEntry *entry, SceUInt32 *result);
int sslGetVerifyContextInfo(SslVerifyContext *context, int selector,
			    void *result);
int sslGetVerifyContextFlags(SslVerifyContext *context, SceUInt32 *result);
int sslGetVerifyStateInfo(SslVerifyState *state, int selector, void *result);
int sslGetVerifyStateContext(SslVerifyState *state, SslVerifyContext **result);
int sslValidateVerifyChainEntry(SslVerifyState *state,
				SslVerifyChainEntry *entry);
int sslPushVerifyStateChainEntry(SslVerifyState *state, int selector,
				 SslVerifyChainEntry *entry);
int sslFreeVerifyChainEntryStruct(SslVerifyChainEntry *entry);
int sslSetVerifyChainEntryInfo(SslVerifyChainEntry *entry, int selector,
			       void *value);
int sslNewVerifyChainEntry(SslVerifyState *state, SceUInt32 flags,
			   SslVerifyChainEntry **result);
int sslSetVerifyStateInfo(SslVerifyState *state, int selector, void *value);
int sslSetVerifyStateError(SslVerifyState *state, int error, int depth);
int sslAppendVerifyChainCertificate(SslVerifyState *state,
				    SslCertificate *certificate, int unused,
				    SceUInt32 flags,
				    SslVerifyChainEntry **result);
int sslBuildVerifyCertificateChain(SslVerifyState *state,
				   SslSmallCertificateStore *store,
				   STACK *certificates, int takeOwnership);
int sslBuildVerifyCertificateChainArray(SslVerifyState *state,
					SslSmallCertificateStore *store,
					SslCertificate **certificates,
					int count, int takeOwnership);
int sslRegisterVerifyModule(SslVerifyContext *context, SslVerifyModule *module,
			    int replace);
int sslFreeVerifyContext(SslVerifyContext *context);
int sslNewVerifyContext(SslLibrary *library, int unused,
			SslVerifyContext **result);
int sslRetainVerifyContext(SslVerifyContext *context);
int sslSetVerifyContextInfo(SslVerifyContext *context, int selector,
			    void *value);
int sslSetVerifyContextFlags(SslVerifyContext *context, SceUInt32 flags);
int sslGetVerifyChainEntryCertificate(SslVerifyChainEntry *entry, int selector,
				      void **result);
int sslGetVerifyChainEntryError(SslVerifyChainEntry *entry, int *result);
int sslGetVerifyStateError(SslVerifyState *state, int *result);
int sslGetVerifyStateChainLength(SslVerifyState *state, int *result);
int sslGetVerifyStateChainEntry(SslVerifyState *state, int index,
				SslVerifyChainEntry **result);
int sslNewVerifyState(SslVerifyContext *context, void *arg,
		      SslVerifyState **result);
int sslSetVerifyStateTimeRange(SslVerifyState *state, SceUInt32 flags,
			       void *startTime, void *endTime);
int sslFreeVerifyState(SslVerifyState *state);
int sslSetVerifyStateChain(SslVerifyState *state, STACK *chain);
int sslUpdateVerifyChainEntryError(SslVerifyContext *context,
				   SslVerifyState *state,
				   SslVerifyChainEntry *entry, int depth,
				   int *result);
int sslVerifyCertificateChain(SslVerifyState *state, int *result);
int sslFreeVerifyChainEntry(SslVerifyChainEntry *entry);

#endif
