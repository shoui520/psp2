/*
 * np_utility_profile.cpp: VSH client wrapper over SceNpUtility's lookup service
 * for fetching NP user profiles, avatar images, and profile panel images.
 * 3.65-CEX: 0x81388710 - 0x81388B23
 */
#include <stddef.h>
#include <scetypes.h>
#include <np/np_common.h>
#include <np/np_lookup.h>
#include <np/np_utility/np_utility.h>
#include <np/np_utility/np_utility_internal.h>

#define NP_UTIL_LOOKUP_SERVICE_PROFILE		0

#define NP_UTIL_ERR_ALREADY_INITIALIZED		(-2141911295)	/* 0x80550701 */
#define NP_UTIL_ERR_NOT_INITIALIZED		(-2141911294)	/* 0x80550702 */
#define NP_UTIL_ERR_INVALID_TITLE_CTX		(-2141911293)	/* 0x80550703 */
#define NP_UTIL_ERR_INVALID_ARGUMENT		(-2141911292)	/* 0x80550704 */
#define NP_UTIL_ERR_ABORTED			(-2141911289)	/* 0x80550707 */
#define NP_UTIL_ERR_INVALID_BUFFER		(-2141911284)	/* 0x8055070C */
#define NP_UTIL_ERR_INVALID_STATE		(-2141911279)	/* 0x80550711 */
#define NP_UTIL_ERR_CLIENT_EXISTS		(-2141911275)	/* 0x80550715 */

#define NP_UTIL_ALLOC_MODE_DEFAULT		0u
#define NP_UTIL_ALLOC_MODE_CALLBACKS		1u
#define NP_UTIL_ALLOC_MODE_MEMBLOCK		2u

#define NP_UTIL_AVATAR_SIZE_LARGE_MIN		0x32000u	/* 200 KiB */
#define NP_UTIL_AVATAR_SIZE_MEDIUM_MIN		0x19000u	/* 100 KiB */
#define NP_UTIL_AVATAR_SIZE_SMALL_MIN		0x02800u	/* 10 KiB */
#define NP_UTIL_PANEL_STATUS_AVAILABLE		2

/* 0x81388710 */
int npUtilityProfileInit(SceUInt32 allocMode, void *(*allocFunc)(SceSize),
			 void (*freeFunc)(void *), SceSize memblockSize, void *memblock)
{
	if (SceNpUtility_16954FFF() != 0)
		return NP_UTIL_ERR_ALREADY_INITIALIZED;

	if (allocMode == NP_UTIL_ALLOC_MODE_DEFAULT) {
		SceNpUtility_5FC4B7D5(NP_UTIL_LOOKUP_SERVICE_PROFILE);
	} else if (allocMode == NP_UTIL_ALLOC_MODE_CALLBACKS) {
		SceNpUtility_4A3FDD3B(NP_UTIL_LOOKUP_SERVICE_PROFILE, allocFunc, freeFunc);
	} else if (allocMode == NP_UTIL_ALLOC_MODE_MEMBLOCK) {
		SceNpUtility_B5711AF9(NP_UTIL_LOOKUP_SERVICE_PROFILE, memblockSize, memblock);
	} else {
		return NP_UTIL_ERR_INVALID_ARGUMENT;
	}

	int ret = SceNpUtility_E38548AB(NP_UTIL_LOOKUP_SERVICE_PROFILE);
	if (ret >= 0) {
		ret = SceNpUtility_9F142A92(NP_UTIL_LOOKUP_SERVICE_PROFILE);
		if (ret >= 0) {
			ret = SceNpUtility_17385829(NP_UTIL_LOOKUP_SERVICE_PROFILE);
			if (ret >= 0) {
				SceNpUtility_5D6FC99E(1);
				return 0;
			}
			SceNpUtility_6DF884DA(NP_UTIL_LOOKUP_SERVICE_PROFILE);
		}
		SceNpUtility_26B4AC00(NP_UTIL_LOOKUP_SERVICE_PROFILE);
	}
	return ret;
}

/* 0x813887AC */
int npUtilityProfileInitWithAllocator(void *(*allocFunc)(SceSize), void (*freeFunc)(void *))
{
	return npUtilityProfileInit(NP_UTIL_ALLOC_MODE_CALLBACKS, allocFunc, freeFunc, 0, NULL);
}

/* 0x813887C6 */
int npUtilityProfileCreateTitleCtx(const SceNpCommunicationId *commId, const SceNpId *selfNpId)
{
	if (SceNpUtility_16954FFF() != 1)
		return NP_UTIL_ERR_NOT_INITIALIZED;
	if (selfNpId == NULL)
		return NP_UTIL_ERR_INVALID_BUFFER;

	int clientId = SceNpUtility_F4B21646(&selfNpId->handle, selfNpId,
					     NP_UTIL_LOOKUP_SERVICE_PROFILE);
	if (clientId == NP_UTIL_ERR_CLIENT_EXISTS)
		clientId = 0;
	else if (clientId < 0)
		return clientId;

	int titleCtxId = SceNpUtility_44A2F444(commId, selfNpId, clientId,
					       NP_UTIL_LOOKUP_SERVICE_PROFILE);
	if (titleCtxId < 0)
		SceNpUtility_ED2B86ED(clientId, NP_UTIL_LOOKUP_SERVICE_PROFILE);
	return titleCtxId;
}

/* 0x8138882A */
int npUtilityProfileDestroyTitleCtx(int titleCtxId)
{
	if (SceNpUtility_16954FFF() != 1)
		return NP_UTIL_ERR_NOT_INITIALIZED;

	NpLookupTitle *title = NULL;
	int ret = SceNpUtility_B9FFB253(titleCtxId, &title, NP_UTIL_LOOKUP_SERVICE_PROFILE);
	if (ret < 0)
		return ret;

	SceNpUtility_CD3A3712(title->client);
	SceNpUtility_485F368A(title, NP_UTIL_LOOKUP_SERVICE_PROFILE);
	return SceNpUtility_4013244C(titleCtxId, NP_UTIL_LOOKUP_SERVICE_PROFILE);
}

/* 0x8138888E */
int npUtilityProfileCreateRequest(int titleCtxId)
{
	if (SceNpUtility_16954FFF() != 1)
		return NP_UTIL_ERR_NOT_INITIALIZED;

	NpLookupTitle *title = NULL;
	int ret = SceNpUtility_B9FFB253(titleCtxId, &title, NP_UTIL_LOOKUP_SERVICE_PROFILE);
	if (ret >= 0) {
		if (title != NULL) {
			ret = SceNpUtility_548625FC(title, 0, &title->npId, title->id);
			SceNpUtility_485F368A(title, NP_UTIL_LOOKUP_SERVICE_PROFILE);
			return ret;
		}
		ret = NP_UTIL_ERR_INVALID_TITLE_CTX;
	}
	if (title != NULL)
		SceNpUtility_485F368A(title, NP_UTIL_LOOKUP_SERVICE_PROFILE);
	return ret;
}

/* 0x81388910 */
int npUtilityProfileDestroyRequest(int reqId)
{
	if (SceNpUtility_16954FFF() != 1)
		return NP_UTIL_ERR_NOT_INITIALIZED;
	return SceNpUtility_4A479D1D(reqId);
}

/* 0x8138892E */
int npUtilityProfileGetUserProfile(int reqId, SceUInt32 avatarSizeType, SceUInt32 panelSizeType,
				   const SceNpId *targetNpId, NpLookupProfile *outProfile,
				   void *avatarBuffer, SceSize avatarBufferMax,
				   SceSize *outAvatarSize, void *panelBuffer,
				   SceSize panelBufferMax, SceSize *outPanelSize, void *option)
{
	if (SceNpUtility_16954FFF() != 1)
		return NP_UTIL_ERR_NOT_INITIALIZED;
	if (targetNpId == NULL || outProfile == NULL)
		return NP_UTIL_ERR_INVALID_BUFFER;
	if (option != NULL)
		return NP_UTIL_ERR_INVALID_ARGUMENT;

	if (avatarBuffer != NULL) {
		if (outAvatarSize == NULL)
			return NP_UTIL_ERR_INVALID_BUFFER;
		SceSize requiredAvatarMin = 0;
		if (avatarSizeType == 0)
			requiredAvatarMin = NP_UTIL_AVATAR_SIZE_LARGE_MIN;
		else if (avatarSizeType == 1)
			requiredAvatarMin = NP_UTIL_AVATAR_SIZE_MEDIUM_MIN;
		else if (avatarSizeType <= 3)
			requiredAvatarMin = NP_UTIL_AVATAR_SIZE_SMALL_MIN;
		else
			return NP_UTIL_ERR_INVALID_STATE;
		if (avatarBufferMax < requiredAvatarMin)
			return NP_UTIL_ERR_INVALID_BUFFER;
	}

	if (panelBuffer != NULL) {
		if (outPanelSize == NULL)
			return NP_UTIL_ERR_INVALID_BUFFER;
		if (panelSizeType > 2)
			return NP_UTIL_ERR_INVALID_STATE;
	}

	NpLookupTrans *trans = NULL;
	int ret = SceNpUtility_25D26B6F(reqId, &trans);
	if (ret >= 0) {
		if (trans->operation != 0) {
			ret = NP_UTIL_ERR_INVALID_STATE;
		} else {
			trans->operation = 1;
			ret = SceNpUtility_7BC7E7F7(reqId, targetNpId,
						    static_cast<SceInt32>(avatarSizeType),
						    static_cast<int>(panelSizeType), outProfile);
			if (ret >= 0 && avatarBuffer != NULL && trans->aborted != 1) {
				trans->operation = 1;
				trans->data = NULL;
				ret = SceNpUtility_08893FA1(reqId, &outProfile->icon,
							    outAvatarSize, avatarBufferMax,
							    avatarBuffer, option);
			}
			if (ret >= 0 && panelBuffer != NULL &&
			    outProfile->panel.color.panel.type == NP_UTIL_PANEL_STATUS_AVAILABLE &&
			    trans->aborted != 1) {
				trans->operation = 1;
				trans->data = NULL;
				ret = SceNpUtility_E64B4101(reqId, outProfile->panel.panelUrl,
							    outPanelSize, panelBufferMax,
							    panelBuffer, option);
			}
		}
	}

	if (trans != NULL) {
		if (trans->aborted == 1)
			ret = NP_UTIL_ERR_ABORTED;
		SceNpUtility_31D3542E(trans);
	}
	return ret;
}
