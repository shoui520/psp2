/*
 * The NP community helper (np_community.h): SceNpUtility's community, started in three steps
 * and undone in reverse when a step fails.
 */
#include <scetypes.h>
#include <np/np_error.h>
#include "../np_utility/np_utility.h"
#include "np_community.h"

int npCommunityInit(SceUInt32 type, void *(*alloc)(SceSize size), void (*free)(void *ptr),
		    int arg1, int arg2)
{
	int ret;

	if (SceNpUtility_16954FFF() != 0)
		return SCE_NP_COMMUNITY_ERROR_ALREADY_INITIALIZED;
	switch (type) {
	case NP_COMMUNITY_INIT_DEFAULT:
		SceNpUtility_5FC4B7D5(0);
		break;
	case NP_COMMUNITY_INIT_ALLOCATOR:
		SceNpUtility_4A3FDD3B(0, alloc, free);
		break;
	case NP_COMMUNITY_INIT_PARAM:
		SceNpUtility_B5711AF9(0, arg1, arg2);
		break;
	default:
		return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
	}
	ret = SceNpUtility_E38548AB(0);
	if (ret < 0)
		return ret;
	ret = SceNpUtility_9F142A92(0);
	if (ret < 0)
		goto undo_first;
	ret = SceNpUtility_17385829(0);
	if (ret < 0)
		goto undo_second;
	SceNpUtility_5D6FC99E(1);
	return 0;

undo_second:
	SceNpUtility_6DF884DA(0);
undo_first:
	SceNpUtility_26B4AC00(0);
	return ret;
}

int npCommunityInitWithAllocator(void *(*alloc)(SceSize size), void (*free)(void *ptr))
{
	return npCommunityInit(NP_COMMUNITY_INIT_ALLOCATOR, alloc, free, 0, 0);
}

int npCommunityTerm(void)
{
	int ret = 0;

	if (SceNpUtility_16954FFF() != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	SceNpUtility_6DF884DA(0);
	SceNpUtility_26B4AC00(0);
	SceNpUtility_7A86C37E(0);
	SceNpUtility_418E1324(0);
	SceNpUtility_5D6FC99E(0);
	return ret;
}
