/* SceSblGcAuthMgr's stub library (gc_auth_mgr_stub.c). */
#pragma once

#include <scetypes.h>
#include <gcauthmgr/gcauthmgr_user.h>

#ifdef __cplusplus
extern "C" {
#endif

int sceSblGcAuthMgrPcactGetChallenge(SceUInt mode, const void *input, void *output);
int sceSblGcAuthMgrPcactActivation(const void *activation, SceUInt unknownArgument);
int sceSblGcAuthMgrPkgVry(const void *first, const void *second);
int sceSblGcAuthMgrSclkGetData1(void *data);
int sceSblGcAuthMgrSclkSetData2(const void *data);
int sceSblGcAuthMgrAdhocBB224Init(void);
int sceSblGcAuthMgrAdhocBB224Shutdown(void);
int sceSblGcAuthMgrAdhocBB224Auth1(struct AdhocBB224Context *context, void *output);
int sceSblGcAuthMgrAdhocBB224Auth2(struct AdhocBB224Context *context, const void *input,
	void *output);
int sceSblGcAuthMgrAdhocBB224Auth3(struct AdhocBB224Context *context, const void *input,
	void *output);
int sceSblGcAuthMgrAdhocBB224Auth4(struct AdhocBB224Context *context, const void *input,
	void *output);
int sceSblGcAuthMgrAdhocBB224Auth5(struct AdhocBB224Context *context, const void *input);
int sceSblGcAuthMgrAdhocBB224GetKeys(const struct AdhocBB224Context *context, void *firstKey,
	void *secondKey, void *thirdKey);
int sceSblGcAuthMgrGetMediaIdType01(void *mediaId);
int sceSblGcAuthMgrAdhocBB160Init(void);
int sceSblGcAuthMgrAdhocBB160Shutdown(void);
int sceSblGcAuthMgrAdhocBB160Auth1(struct AdhocBB160Context *context, void *output);
int sceSblGcAuthMgrAdhocBB160Auth2(struct AdhocBB160Context *context, const void *input,
	void *output);
int sceSblGcAuthMgrAdhocBB160Auth3(struct AdhocBB160Context *context, const void *input,
	void *output);
int sceSblGcAuthMgrAdhocBB160Auth4(struct AdhocBB160Context *context, const void *input,
	void *output);
int sceSblGcAuthMgrAdhocBB160Auth5(struct AdhocBB160Context *context, const void *input);
int sceSblGcAuthMgrAdhocBB160GetKeys(const struct AdhocBB160Context *context, void *firstKey,
	void *secondKey, void *thirdKey);
int sceSblGcAuthMgrAdhocBB160UniCastEncrypt(struct AdhocBB160Context *context, void *data,
	SceSize size, void *authentication, SceUInt unknownArgument);
int sceSblGcAuthMgrAdhocBB160UniCastDecrypt(struct AdhocBB160Context *context, void *data,
	SceSize size, const void *authentication, SceUInt unknownFirstArgument,
	SceUInt unknownSecondArgument);
int sceSblGcAuthMgrAdhocBB160BroadCastEncrypt(void *data, SceSize size, void *authentication,
	SceUInt unknownArgument);
int sceSblGcAuthMgrAdhocBB160BroadCastDecrypt(struct AdhocBB160Context *context, void *data,
	SceSize size, const void *authentication, SceUInt unknownFirstArgument,
	SceUInt unknownSecondArgument);
int sceSblGcAuthMgrMsSaveBBMacInit(struct MsSaveBBMacContext *context, SceUInt mode);
int sceSblGcAuthMgrMsSaveBBMacUpdate(struct MsSaveBBMacContext *context, const void *data,
	SceSize size);
int sceSblGcAuthMgrMsSaveBBMacFinal(struct MsSaveBBMacContext *context, void *mac,
	const void *key);
int sceSblGcAuthMgrMsSaveBBCipherInit(struct MsSaveBBCipherContext *context, SceUInt mode,
	SceUInt generationMode, void *data, const void *key);
int sceSblGcAuthMgrMsSaveBBCipherUpdate(struct MsSaveBBCipherContext *context, void *data,
	SceSize size);
int sceSblGcAuthMgrMsSaveBBCipherFinal(struct MsSaveBBCipherContext *context);
int sceSblGcAuthMgrPsmactCreateC1(SceUInt mode, void *activation, void *response);
int sceSblGcAuthMgrPsmactVerifyR1(const void *activation, const void *response, void *output);

#ifdef __cplusplus
}
#endif
