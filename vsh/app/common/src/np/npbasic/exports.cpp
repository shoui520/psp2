/*
 * SceNpBasic: the exports. Each checks that the library is initialized and the NP service
 * reachable, checks its arguments, makes its request and gives the service's error as ours.
 */

#include "npbasic_internal.h"

extern "C" {

int sceNpBasicGetFriendOnlineStatus(int a1, int a2)
{
	int result;

	if (g_initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	if (SceIpmi_9EE9496B(g_ipmiClient) == 0)
		return NP_BASIC_ERROR_UNKNOWN;
	if (a1 == 0 || a2 == 0)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	result = npBasicGetFriendOnlineStatus(a1, a2, 0);
	if (result < 0)
		return npBasicConvertError(result);
	return result;
}

int sceNpBasicGetFriendListEntryCount(int a1)
{
	NpBasicClient *client;
	unsigned char found;
	int result;

	if (g_initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_ipmiClient);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	if (a1 == 0)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	found = 0;
	result = npBasicGetFriendListEntryCount(client, a1, &found);
	if (result < 0)
		result = npBasicConvertError(result);
	if (result != 0)
		return result;
	if (found != 0)
		return result;
	return -2141905651;
}

int sceNpBasicGetFriendRequestEntryCount(int a1)
{
	NpBasicClient *client;
	unsigned char found;
	int result;

	if (g_initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_ipmiClient);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	if (a1 == 0)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	found = 0;
	result = npBasicGetFriendRequestEntryCount(client, a1, &found);
	if (result < 0)
		result = npBasicConvertError(result);
	if (result != 0)
		return result;
	if (found != 0)
		return result;
	return -2141905651;
}

int sceNpBasicGetRequestedFriendRequestEntryCount(int a1)
{
	NpBasicClient *client;
	unsigned char found;
	int result;

	if (g_initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_ipmiClient);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	if (a1 == 0)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	found = 0;
	result = npBasicGetRequestedFriendRequestEntryCount(client, a1, &found);
	if (result < 0)
		result = npBasicConvertError(result);
	if (result != 0)
		return result;
	if (found != 0)
		return result;
	return -2141905651;
}

int sceNpBasicGetBlockListEntryCount(int a1)
{
	NpBasicClient *client;
	unsigned char found;
	int result;

	if (g_initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_ipmiClient);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	if (a1 == 0)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	found = 0;
	result = npBasicGetBlockListEntryCount(client, a1, &found);
	if (result < 0)
		result = npBasicConvertError(result);
	if (result != 0)
		return result;
	if (found != 0)
		return result;
	return -2141905651;
}

int sceNpBasicGetFriendListEntries(unsigned int a1, int a2, int a3, int a4)
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
	if (a2 == 0 || a3 == 0 || a4 == 0 || a1 >= 100)
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

int sceNpBasicGetFriendRequestEntries(unsigned int a1, int a2, int a3, int a4)
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
	if (a2 == 0 || a3 == 0 || a4 == 0 || a1 >= 100)
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

int sceNpBasicGetRequestedFriendRequestEntries(unsigned int a1, int a2, int a3, int a4)
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
	if (a2 == 0 || a3 == 0 || a4 == 0 || a1 >= 100)
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

int sceNpBasicGetBlockListEntries(unsigned int a1, int a2, int a3, int a4)
{
	NpBasicClient *client;
	unsigned char found;
	int result;

	if (g_initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_ipmiClient);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	if (a2 == 0 || a3 == 0 || a4 == 0 || a1 >= 100)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	found = 0;
	result = npBasicGetBlockListEntries(client, a1, a2, a3, a4, &found);
	if (result < 0)
		result = npBasicConvertError(result);
	if (result != 0)
		return result;
	if (found != 0)
		return result;
	return -2141905651;
}

} /* extern "C" */
