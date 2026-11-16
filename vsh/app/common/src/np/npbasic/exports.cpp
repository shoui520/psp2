/*
 * SceNpBasic: the exports. Each checks that the library is initialized and the NP service
 * reachable, checks its arguments, makes its request and gives the service's error as ours.
 */

#include "npbasic_internal.h"

extern "C" {

int sceNpBasicGetFriendOnlineStatus(const void *a1, int *a2)
{
	NpBasicClient *client;
	int result;

	if (g_initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_ipmiClient);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	if (a1 == NULL || a2 == NULL)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	result = npBasicGetFriendOnlineStatus(client, a1, a2);
	if (result < 0)
		return npBasicConvertError(result);
	return result;
}

int sceNpBasicGetFriendContextState(const void *a1, int *a2)
{
	NpBasicClient *client;
	NpBasicPresenceRecord record;
	unsigned char none[12];
	unsigned char own[12];
	int result;

	if (g_initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_ipmiClient);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	if (a1 == NULL || a2 == NULL)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	result = npBasicGetFriendPresence(client, a1, &record);
	if (result < 0)
		return npBasicConvertError(result);
	sceClibMemset(none, 0, sizeof(none));
	if (sceClibMemcmp(none, &record.commId, sizeof(none)) != 0) {
		sceClibMemset(own, 0, sizeof(own));
		if (sceClibMemcmp(&g_commId, &record.commId, sizeof(g_commId)) != 0
		    || sceClibMemcmp(own, &g_commId, sizeof(g_commId)) == 0)
			*a2 = 1;
		else
			*a2 = 2;
	} else {
		*a2 = 0;
	}
	return result;
}

int sceNpBasicGetGamePresenceOfFriend(const void *a1, SceNpBasicGamePresence *a2)
{
	NpBasicClient *client;
	NpBasicPresenceRecord record;
	int result;

	if (g_initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_ipmiClient);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	if (a1 == NULL || a2 == NULL)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	result = npBasicGetFriendPresence(client, a1, &record);
	if (result < 0)
		return npBasicConvertError(result);
	a2->size = sizeof(SceNpBasicGamePresence);
	sceClibStrncpy(a2->title, record.title, SCE_NP_BASIC_TITLE_SIZE_MAX);
	sceClibMemcpy_safe(&a2->inGamePresence, &record.inGamePresence, sizeof(record.inGamePresence));
	return result;
}

int sceNpBasicGetFriendListEntryCount(int *a1)
{
	NpBasicClient *client;
	unsigned char found;
	int result;

	if (g_initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_ipmiClient);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	if (a1 == NULL)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	found = 0;
	result = npBasicGetFriendListEntryCount(client, a1, &found);
	if (result < 0)
		result = npBasicConvertError(result);
	if (result != 0)
		return result;
	if (found != 0)
		return result;
	return NP_BASIC_ERROR_NO_RESULT;
}

int sceNpBasicGetFriendListEntries(unsigned int a1, void *a2, int a3, int *a4)
{
	NpBasicClient *client;
	unsigned char found;
	int result;

	if (g_initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_ipmiClient);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	result = 0;
	found = 0;
	if (a2 == NULL || a3 == 0 || a4 == NULL || a1 >= 100)
		result = NP_BASIC_ERROR_INVALID_ARGUMENT;
	if (result >= 0) {
		result = npBasicGetFriendListEntries(client, a1, a2, a3, a4, &found);
		if (result < 0)
			result = npBasicConvertError(result);
		if (result == 0) {
			if (found == 0)
				result = -2141905651;
		}
	}
	return result;
}

int sceNpBasicGetFriendRequestEntryCount(int *a1)
{
	NpBasicClient *client;
	unsigned char found;
	int result;

	if (g_initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_ipmiClient);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	if (a1 == NULL)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	found = 0;
	result = npBasicGetFriendRequestEntryCount(client, a1, &found);
	if (result < 0)
		result = npBasicConvertError(result);
	if (result != 0)
		return result;
	if (found != 0)
		return result;
	return NP_BASIC_ERROR_NO_RESULT;
}

int sceNpBasicGetFriendRequestEntries(unsigned int a1, void *a2, int a3, int *a4)
{
	NpBasicClient *client;
	unsigned char found;
	int result;

	if (g_initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_ipmiClient);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	result = 0;
	found = 0;
	if (a2 == NULL || a3 == 0 || a4 == NULL || a1 >= 100)
		result = NP_BASIC_ERROR_INVALID_ARGUMENT;
	if (result >= 0) {
		result = npBasicGetFriendRequestEntries(client, a1, a2, a3, a4, &found);
		if (result < 0)
			result = npBasicConvertError(result);
		if (result == 0) {
			if (found == 0)
				result = -2141905651;
		}
	}
	return result;
}

int sceNpBasicGetRequestedFriendRequestEntryCount(int *a1)
{
	NpBasicClient *client;
	unsigned char found;
	int result;

	if (g_initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_ipmiClient);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	if (a1 == NULL)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	found = 0;
	result = npBasicGetRequestedFriendRequestEntryCount(client, a1, &found);
	if (result < 0)
		result = npBasicConvertError(result);
	if (result != 0)
		return result;
	if (found != 0)
		return result;
	return NP_BASIC_ERROR_NO_RESULT;
}

int sceNpBasicGetRequestedFriendRequestEntries(unsigned int a1, void *a2, int a3, int *a4)
{
	NpBasicClient *client;
	unsigned char found;
	int result;

	if (g_initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_ipmiClient);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	result = 0;
	found = 0;
	if (a2 == NULL || a3 == 0 || a4 == NULL || a1 >= 100)
		result = NP_BASIC_ERROR_INVALID_ARGUMENT;
	if (result >= 0) {
		result = npBasicGetRequestedFriendRequestEntries(client, a1, a2, a3, a4, &found);
		if (result < 0)
			result = npBasicConvertError(result);
		if (result == 0) {
			if (found == 0)
				result = -2141905651;
		}
	}
	return result;
}

int sceNpBasicGetBlockListEntryCount(int *a1)
{
	NpBasicClient *client;
	unsigned char found;
	int result;

	if (g_initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_ipmiClient);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	if (a1 == NULL)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	found = 0;
	result = npBasicGetBlockListEntryCount(client, a1, &found);
	if (result < 0)
		result = npBasicConvertError(result);
	if (result != 0)
		return result;
	if (found != 0)
		return result;
	return NP_BASIC_ERROR_NO_RESULT;
}

int sceNpBasicGetBlockListEntries(unsigned int a1, void *a2, int a3, int *a4)
{
	NpBasicClient *client;
	unsigned char found;
	int result;

	if (g_initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_ipmiClient);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	if (a2 == NULL || a3 == 0 || a4 == NULL || a1 >= 100)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	found = 0;
	result = npBasicGetBlockListEntries(client, a1, a2, a3, a4, &found);
	if (result < 0)
		result = npBasicConvertError(result);
	if (result != 0)
		return result;
	if (found != 0)
		return result;
	return NP_BASIC_ERROR_NO_RESULT;
}

int SceNpBasic_0C9BAF57(const void *a1, NpBasicPresenceTail *a2)
{
	NpBasicClient *client;
	NpBasicPresenceRecord record;
	int result;

	if (g_initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_ipmiClient);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	if (a1 == NULL || a2 == NULL)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	result = npBasicGetOwnInfo(client, a1, &record);
	if (result < 0)
		return npBasicConvertError(result);
	*a2 = record.tail;
	return result;
}

int SceNpBasic_B93ADCDA(const void *a1, NpBasicFriendInfo *a2)
{
	NpBasicClient *client;
	NpBasicPresenceRecord record;
	int result;

	if (g_initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_ipmiClient);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	if (a1 == NULL || a2 == NULL)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	result = npBasicGetOwnInfo(client, a1, &record);
	if (result < 0)
		return npBasicConvertError(result);
	a2->size = sizeof(NpBasicFriendInfo);
	a2->friendCommId = record.friendCommId;
	sceClibMemcpy_safe(a2->block, record.block, sizeof(record.block));
	a2->commId = record.commId;
	a2->gamePresence.size = sizeof(SceNpBasicGamePresence);
	sceClibStrncpy(a2->gamePresence.title, record.title, SCE_NP_BASIC_TITLE_SIZE_MAX);
	a2->gamePresence.inGamePresence = record.inGamePresence;
	return result;
}

int sceNpBasicCheckIfPlayerIsBlocked(const void *a1, unsigned char *a2)
{
	NpBasicClient *client;
	unsigned char found;
	int result;

	if (g_initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_ipmiClient);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	if (a1 == NULL || a2 == NULL)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	found = 0;
	result = npBasicCheckIfPlayerIsBlocked(client, a1, a2, &found);
	if (result < 0)
		result = npBasicConvertError(result);
	if (result != 0)
		return result;
	if (found != 0)
		return result;
	return NP_BASIC_ERROR_NO_RESULT;
}

int sceNpBasicGetGameJoiningPresence(const void *a1, void *a2)
{
	NpBasicClient *client;
	int result;

	if (g_initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_ipmiClient);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	if (a1 == NULL || a2 == NULL)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	result = npBasicGetGameJoiningPresence(client, a1, a2);
	if (result < 0)
		return npBasicConvertError(result);
	return result;
}

int SceNpBasic_375EABF2(int *a1, NpBasicFriendInfo *a2, NpBasicPresenceTail *a3)
{
	NpBasicClient *client;
	NpBasicPresenceRecord record;
	int state;
	int result;

	if (g_initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_ipmiClient);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	sceClibMemset(&record, 0, sizeof(record));
	if (a1 == NULL || a2 == NULL || a3 == NULL)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	state = 0;
	result = npBasicRequest_9003C(client, &state, &record);
	if (result < 0)
		return npBasicConvertError(result);
	if (state > 1) {
		if (state <= 2)
			*a1 = 2;
		else if (state <= 3)
			*a1 = 3;
		else
			*a1 = 0;
	} else if (state > 0) {
		*a1 = 1;
	} else {
		*a1 = 0;
	}
	sceClibMemset(a2, 0, sizeof(NpBasicFriendInfo));
	a2->size = sizeof(NpBasicFriendInfo);
	a2->friendCommId = record.friendCommId;
	a2->commId = record.commId;
	a2->extraCommId = record.extraCommId;
	sceClibMemcpy_safe(a2->block, record.block, sizeof(record.block));
	a2->gamePresence.size = sizeof(SceNpBasicGamePresence);
	sceClibStrncpy(a2->gamePresence.title, record.title, SCE_NP_BASIC_TITLE_SIZE_MAX);
	a2->gamePresence.title[SCE_NP_BASIC_TITLE_SIZE_MAX - 1] = 0;
	a2->gamePresence.inGamePresence = record.inGamePresence;
	sceClibMemset(a3, 0, sizeof(NpBasicPresenceTail));
	*a3 = record.tail;
	return result;
}

} /* extern "C" */
