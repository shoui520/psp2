/*
 * SceSblGcAuthMgr's system calls (gcauthmgr_syscall.c) as user code reaches them: each takes
 * its sizes in a request block GcAuthMgr copies whole (the reserved words stay unwritten).
 * libkernel exports wrappers of them, and the stub library programs link has its own.
 * The contexts are opaque records GcAuthMgr reads and writes.
 */

#pragma once

#include <scetypes.h>

struct AdhocBB160Context;
struct AdhocBB224Context;
struct MsSaveBBMacContext;
struct MsSaveBBCipherContext;

struct PackageVerifyRequest {
	SceSize firstSize;
	SceSize secondSize;
	SceUInt reserved[2];
};

struct SecureClockRequest {
	SceSize size;
	SceUInt reserved;
};

struct PcActivationChallengeRequest {
	SceSize inputSize;
	SceSize outputSize;
	SceUInt reserved[2];
};

struct PsmActivationRequest {
	SceSize responseSize;
	SceUInt reserved;
};

struct AdhocAuthRequest {
	SceSize dataSize;
	SceUInt reserved;
};

struct AdhocAuthExchangeRequest {
	SceSize inputSize;
	SceSize outputSize;
	SceUInt reserved[2];
};

struct AdhocKeysRequest {
	void *thirdKey;
	SceSize firstKeySize;
	SceSize secondKeySize;
	SceSize thirdKeySize;
	SceUInt reserved[2];
};

struct AdhocUnicastEncryptRequest {
	void *authentication;
	SceUInt unknownArgument;
	SceSize dataSize;
	SceSize authenticationSize;
	SceUInt reserved[2];
};

struct AdhocDecryptRequest {
	const void *authentication;
	SceUInt unknownFirstArgument;
	SceUInt unknownSecondArgument;
	SceSize dataSize;
	SceSize authenticationSize;
	SceUInt reserved;
};

struct AdhocBroadcastEncryptRequest {
	SceUInt unknownArgument;
	SceSize dataSize;
	SceSize authenticationSize;
	SceUInt reserved;
};

struct MsSaveBBMacRequest {
	SceSize macSize;
	SceSize keySize;
	SceUInt reserved[2];
};

struct MsSaveBBCipherRequest {
	void *data;
	const void *key;
	SceSize dataSize;
	SceSize keySize;
	SceUInt reserved[2];
};

extern int _sceSblGcAuthMgrAdhocBB160UniCastEncrypt(
		struct AdhocBB160Context *context, void *data, SceSize size,
		const struct AdhocUnicastEncryptRequest *request);
extern int _sceSblGcAuthMgrAdhocBB160UniCastDecrypt(
		struct AdhocBB160Context *context, void *data, SceSize size,
		const struct AdhocDecryptRequest *request);
extern int _sceSblGcAuthMgrAdhocBB160BroadCastEncrypt(void *data, SceSize size,
		void *authentication,
		const struct AdhocBroadcastEncryptRequest *request);
extern int _sceSblGcAuthMgrAdhocBB160BroadCastDecrypt(
		struct AdhocBB160Context *context, void *data, SceSize size,
		const struct AdhocDecryptRequest *request);
extern int _sceSblGcAuthMgrPkgVry(const void *first, const void *second,
		const struct PackageVerifyRequest *request);
extern int _sceSblGcAuthMgrSclkGetData1(void *data,
		const struct SecureClockRequest *request);
extern int _sceSblGcAuthMgrSclkSetData2(const void *data,
		const struct SecureClockRequest *request);
extern int _sceSblGcAuthMgrAdhocBB224Init(void);
extern int _sceSblGcAuthMgrAdhocBB160Init(void);
extern int _sceSblGcAuthMgrPcactGetChallenge(SceUInt mode, const void *input,
		void *output, const struct PcActivationChallengeRequest *request);
extern int _sceSblGcAuthMgrPcactActivation(const void *activation,
		SceUInt unknownArgument);
extern int _sceSblGcAuthMgrPsmactCreateC1(SceUInt mode, void *activation,
		void *response, const struct PsmActivationRequest *request);
extern int _sceSblGcAuthMgrPsmactVerifyR1(const void *activation,
		const void *response, void *output,
		const struct PsmActivationRequest *request);
extern int _sceSblGcAuthMgrAdhocBB224Auth1(struct AdhocBB224Context *context,
		void *output, const struct AdhocAuthRequest *request);
extern int _sceSblGcAuthMgrAdhocBB160Auth1(struct AdhocBB160Context *context,
		void *output, const struct AdhocAuthRequest *request);
extern int _sceSblGcAuthMgrAdhocBB160Auth5(struct AdhocBB160Context *context,
		const void *input, const struct AdhocAuthRequest *request);
extern int _sceSblGcAuthMgrAdhocBB224Auth2(struct AdhocBB224Context *context,
		const void *input, void *output,
		const struct AdhocAuthExchangeRequest *request);
extern int _sceSblGcAuthMgrAdhocBB224Auth3(struct AdhocBB224Context *context,
		const void *input, void *output,
		const struct AdhocAuthExchangeRequest *request);
extern int _sceSblGcAuthMgrAdhocBB224Auth4(struct AdhocBB224Context *context,
		const void *input, void *output,
		const struct AdhocAuthExchangeRequest *request);
extern int _sceSblGcAuthMgrAdhocBB224Auth5(struct AdhocBB224Context *context,
		const void *input, const struct AdhocAuthRequest *request);
extern int _sceSblGcAuthMgrAdhocBB160Auth2(struct AdhocBB160Context *context,
		const void *input, void *output,
		const struct AdhocAuthExchangeRequest *request);
extern int _sceSblGcAuthMgrAdhocBB160Auth3(struct AdhocBB160Context *context,
		const void *input, void *output,
		const struct AdhocAuthExchangeRequest *request);
extern int _sceSblGcAuthMgrAdhocBB160Auth4(struct AdhocBB160Context *context,
		const void *input, void *output,
		const struct AdhocAuthExchangeRequest *request);
extern int _sceSblGcAuthMgrAdhocBB224GetKeys(
		const struct AdhocBB224Context *context, void *firstKey,
		void *secondKey, const struct AdhocKeysRequest *request);
extern int _sceSblGcAuthMgrAdhocBB160GetKeys(
		const struct AdhocBB160Context *context, void *firstKey,
		void *secondKey, const struct AdhocKeysRequest *request);
extern int _sceSblGcAuthMgrGetMediaIdType01(void *mediaId,
		const struct AdhocAuthRequest *request);
extern int _sceSblGcAuthMgrMsSaveBBMacInit(struct MsSaveBBMacContext *context,
		SceUInt mode);
extern int _sceSblGcAuthMgrMsSaveBBMacUpdate(struct MsSaveBBMacContext *context,
		const void *data, SceSize size);
extern int _sceSblGcAuthMgrMsSaveBBCipherUpdate(
		struct MsSaveBBCipherContext *context, void *data, SceSize size);
extern int _sceSblGcAuthMgrMsSaveBBCipherFinal(
		struct MsSaveBBCipherContext *context);
extern int _sceSblGcAuthMgrMsSaveBBMacFinal(struct MsSaveBBMacContext *context,
		void *mac, const void *key, const struct MsSaveBBMacRequest *request);
extern int _sceSblGcAuthMgrMsSaveBBCipherInit(
		struct MsSaveBBCipherContext *context, SceUInt mode,
		SceUInt generationMode, const struct MsSaveBBCipherRequest *request);
extern int _sceSblGcAuthMgrAdhocBB160Shutdown(void);
extern int _sceSblGcAuthMgrAdhocBB224Shutdown(void);
