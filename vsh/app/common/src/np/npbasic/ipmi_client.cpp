/*
 * SceNpBasic: the requests the library makes to the NP service over IPMI. Each sends one command
 * with its input buffers and takes the output buffers back; the command's own result is the
 * request's, unless the call itself failed.
 */

#include "npbasic_internal.h"


int npBasicRegisterHandler(NpBasicClient *client, const void *a2)
{
	int ret;
	NpBasicIpmiIn in[1];
	int result;

	ret = 0;
	in[0].data = a2;
	in[0].size = 14;
	result = client->vtbl->invoke(client, 0x90000, in, 1, &ret, NULL, 0);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicUnregisterHandler(NpBasicClient *client)
{
	int ret;
	int result;

	ret = 0;
	result = client->vtbl->invoke(client, 0x90001, NULL, 0, &ret, NULL, 0);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicGetFriendListEntryCount(NpBasicClient *client, int *a2, unsigned char *a3)
{
	int ret;
	NpBasicIpmiOut out[2];
	int result;

	ret = 0;
	out[0].data = a2;
	out[0].size = 4;
	out[1].data = a3;
	out[1].size = 1;
	result = client->vtbl->invoke(client, 0x90007, NULL, 0, &ret, out, 2);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicGetFriendListEntries(NpBasicClient *client, int a2, void *a3, int a4, int *a5, unsigned char *a6)
{
	int ret;
	NpBasicIpmiIn in[1];
	NpBasicIpmiOut out[3];
	int arg[2];
	int result;

	ret = 0;
	arg[0] = a2;
	arg[1] = a4;
	in[0].data = arg;
	in[0].size = 8;
	out[0].data = a3;
	out[0].size = 36 * a4;
	out[1].data = a5;
	out[1].size = 4;
	out[2].data = a6;
	out[2].size = 1;
	result = client->vtbl->invoke(client, 0x90008, in, 1, &ret, out, 3);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicGetFriendPresence(NpBasicClient *client, const void *a2, void *a3)
{
	int ret;
	NpBasicIpmiIn in[1];
	NpBasicIpmiOut out[1];
	int result;

	ret = 0;
	in[0].data = a2;
	in[0].size = 36;
	out[0].data = a3;
	out[0].size = 1016;
	result = client->vtbl->invoke(client, 0x90009, in, 1, &ret, out, 1);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicGetBlockListEntryCount(NpBasicClient *client, int *a2, unsigned char *a3)
{
	int ret;
	NpBasicIpmiOut out[2];
	int result;

	ret = 0;
	out[0].data = a2;
	out[0].size = 4;
	out[1].data = a3;
	out[1].size = 1;
	result = client->vtbl->invoke(client, 0x9000A, NULL, 0, &ret, out, 2);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicGetBlockListEntries(NpBasicClient *client, int a2, void *a3, int a4, int *a5, unsigned char *a6)
{
	int ret;
	NpBasicIpmiIn in[1];
	NpBasicIpmiOut out[3];
	int arg[2];
	int result;

	ret = 0;
	arg[0] = a2;
	arg[1] = a4;
	in[0].data = arg;
	in[0].size = 8;
	out[0].data = a3;
	out[0].size = 36 * a4;
	out[1].data = a5;
	out[1].size = 4;
	out[2].data = a6;
	out[2].size = 1;
	result = client->vtbl->invoke(client, 0x9000B, in, 1, &ret, out, 3);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicCheckIfPlayerIsBlocked(NpBasicClient *client, const void *a2, unsigned char *a3, unsigned char *a4)
{
	int ret;
	NpBasicIpmiIn in[1];
	NpBasicIpmiOut out[2];
	int result;

	ret = 0;
	in[0].data = a2;
	in[0].size = 36;
	out[0].data = a3;
	out[0].size = 1;
	out[1].data = a4;
	out[1].size = 1;
	result = client->vtbl->invoke(client, 0x9000C, in, 1, &ret, out, 2);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicRequest_9000D(NpBasicClient *client, const void *a2)
{
	int ret;
	NpBasicIpmiIn in[1];
	int result;

	ret = 0;
	in[0].data = a2;
	in[0].size = 460;
	result = client->vtbl->invoke(client, 0x9000D, in, 1, &ret, NULL, 0);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicRequest_9000E(NpBasicClient *client)
{
	int ret;
	int result;

	ret = 0;
	result = client->vtbl->invoke(client, 0x9000E, NULL, 0, &ret, NULL, 0);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicRequest_9000F(NpBasicClient *client, const void *a2)
{
	int ret;
	NpBasicIpmiIn in[1];
	int result;

	ret = 0;
	in[0].data = a2;
	in[0].size = 12;
	result = client->vtbl->invoke(client, 0x9000F, in, 1, &ret, NULL, 0);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicRequest_90010(NpBasicClient *client)
{
	int ret;
	int result;

	ret = 0;
	result = client->vtbl->invoke(client, 0x90010, NULL, 0, &ret, NULL, 0);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicRequest_90011(NpBasicClient *client, const void *a2)
{
	int ret;
	NpBasicIpmiIn in[1];
	int result;

	ret = 0;
	in[0].data = a2;
	in[0].size = 332;
	result = client->vtbl->invoke(client, 0x90011, in, 1, &ret, NULL, 0);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicRequest_90012(NpBasicClient *client)
{
	int ret;
	int result;

	ret = 0;
	result = client->vtbl->invoke(client, 0x90012, NULL, 0, &ret, NULL, 0);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicRequest_90013(NpBasicClient *client, const void *a2, const void *a3, const void *a4, int a5)
{
	int ret;
	NpBasicIpmiIn in[4];
	int arg;
	int result;

	ret = 0;
	arg = a5;
	in[0].data = &arg;
	in[0].size = 4;
	in[1].data = a2;
	in[1].size = 36;
	in[2].data = a3;
	in[2].size = a5;
	in[3].data = a4;
	in[3].size = 12;
	result = client->vtbl->invoke(client, 0x90013, in, 4, &ret, NULL, 0);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicRequest_90014(NpBasicClient *client, const void *a2, const void *a3, const void *a4, const void *a5)
{
	int ret;
	NpBasicIpmiIn in[4];
	int result;

	ret = 0;
	in[0].data = a2;
	in[0].size = 8;
	in[1].data = a3;
	in[1].size = 36;
	in[2].data = a4;
	in[2].size = 12;
	in[3].data = a5;
	in[3].size = 512;
	result = client->vtbl->invoke(client, 0x90014, in, 4, &ret, NULL, 0);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicRequest_90015(NpBasicClient *client, int a2, void *a3, int *a4)
{
	int ret;
	NpBasicIpmiIn in[2];
	NpBasicIpmiOut out[1];
	int arg;
	int result;

	ret = 0;
	arg = a2;
	in[0].data = &arg;
	in[0].size = 4;
	in[1].data = a3;
	in[1].size = 12;
	out[0].data = a4;
	out[0].size = 4;
	result = client->vtbl->invoke(client, 0x90015, in, 2, &ret, out, 1);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicRequest_90016(NpBasicClient *client, int a2, void *a3, int a4, void *a5, int a6, int *a7)
{
	int ret;
	NpBasicIpmiIn in[2];
	NpBasicIpmiOut out[2];
	int arg[3];
	int result;

	ret = 0;
	arg[0] = a2;
	arg[1] = a4;
	arg[2] = a6;
	in[0].data = arg;
	in[0].size = 12;
	in[1].data = a3;
	in[1].size = 12;
	out[0].data = a5;
	out[0].size = 696 * a6;
	out[1].data = a7;
	out[1].size = 4;
	result = client->vtbl->invoke(client, 0x90016, in, 2, &ret, out, 2);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicRequest_9002B(NpBasicClient *client)
{
	int ret;
	int result;

	ret = 0;
	result = client->vtbl->invoke(client, 0x9002B, NULL, 0, &ret, NULL, 0);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicGetFriendOnlineStatus(NpBasicClient *client, const void *a2, int *a3)
{
	int ret;
	NpBasicIpmiIn in[1];
	NpBasicIpmiOut out[1];
	int result;

	ret = 0;
	in[0].data = a2;
	in[0].size = 36;
	out[0].data = a3;
	out[0].size = 4;
	result = client->vtbl->invoke(client, 0x9002C, in, 1, &ret, out, 1);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicGetGameJoiningPresence(NpBasicClient *client, const void *a2, void *a3)
{
	int ret;
	NpBasicIpmiIn in[1];
	NpBasicIpmiOut out[1];
	int result;

	ret = 0;
	in[0].data = a2;
	in[0].size = 36;
	out[0].data = a3;
	out[0].size = 464;
	result = client->vtbl->invoke(client, 0x9002D, in, 1, &ret, out, 1);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicGetFriendRequestEntryCount(NpBasicClient *client, int *a2, unsigned char *a3)
{
	int ret;
	NpBasicIpmiOut out[2];
	int result;

	ret = 0;
	out[0].data = a2;
	out[0].size = 4;
	out[1].data = a3;
	out[1].size = 1;
	result = client->vtbl->invoke(client, 0x90031, NULL, 0, &ret, out, 2);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicGetFriendRequestEntries(NpBasicClient *client, int a2, void *a3, int a4, int *a5, unsigned char *a6)
{
	int ret;
	NpBasicIpmiIn in[1];
	NpBasicIpmiOut out[3];
	int arg[2];
	int result;

	ret = 0;
	arg[0] = a2;
	arg[1] = a4;
	in[0].data = arg;
	in[0].size = 8;
	out[0].data = a3;
	out[0].size = 36 * a4;
	out[1].data = a5;
	out[1].size = 4;
	out[2].data = a6;
	out[2].size = 1;
	result = client->vtbl->invoke(client, 0x90032, in, 1, &ret, out, 3);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicGetRequestedFriendRequestEntryCount(NpBasicClient *client, int *a2, unsigned char *a3)
{
	int ret;
	NpBasicIpmiOut out[2];
	int result;

	ret = 0;
	out[0].data = a2;
	out[0].size = 4;
	out[1].data = a3;
	out[1].size = 1;
	result = client->vtbl->invoke(client, 0x90033, NULL, 0, &ret, out, 2);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicGetRequestedFriendRequestEntries(NpBasicClient *client, int a2, void *a3, int a4, int *a5, unsigned char *a6)
{
	int ret;
	NpBasicIpmiIn in[1];
	NpBasicIpmiOut out[3];
	int arg[2];
	int result;

	ret = 0;
	arg[0] = a2;
	arg[1] = a4;
	in[0].data = arg;
	in[0].size = 8;
	out[0].data = a3;
	out[0].size = 36 * a4;
	out[1].data = a5;
	out[1].size = 4;
	out[2].data = a6;
	out[2].size = 1;
	result = client->vtbl->invoke(client, 0x90034, in, 1, &ret, out, 3);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicRequest_90035(NpBasicClient *client, int a2)
{
	int ret;
	NpBasicIpmiIn in[1];
	int arg;
	int result;

	ret = 0;
	arg = a2;
	in[0].data = &arg;
	in[0].size = 4;
	result = client->vtbl->invoke(client, 0x90035, in, 1, &ret, NULL, 0);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicRequest_90036(NpBasicClient *client, int a2)
{
	int ret;
	NpBasicIpmiIn in[1];
	int arg;
	int result;

	ret = 0;
	arg = a2;
	in[0].data = &arg;
	in[0].size = 4;
	result = client->vtbl->invoke(client, 0x90036, in, 1, &ret, NULL, 0);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicRequest_90037(NpBasicClient *client, int a2, const void *a3, int a4)
{
	int ret;
	NpBasicIpmiIn in[2];
	int arg[2];
	int result;

	ret = 0;
	arg[0] = a2;
	arg[1] = a4;
	in[0].data = arg;
	in[0].size = 8;
	in[1].data = a3;
	in[1].size = a4;
	result = client->vtbl->invoke(client, 0x90037, in, 2, &ret, NULL, 0);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicGetOwnInfo(NpBasicClient *client, const void *a2, void *a3)
{
	int ret;
	NpBasicIpmiIn in[1];
	NpBasicIpmiOut out[1];
	int result;

	ret = 0;
	in[0].data = a2;
	in[0].size = 36;
	out[0].data = a3;
	out[0].size = 1016;
	result = client->vtbl->invoke(client, 0x9003A, in, 1, &ret, out, 1);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicRequest_9003B(NpBasicClient *client, int a2)
{
	int ret;
	NpBasicIpmiIn in[1];
	int arg;
	int result;

	ret = 0;
	arg = a2;
	in[0].data = &arg;
	in[0].size = 4;
	result = client->vtbl->invoke(client, 0x9003B, in, 1, &ret, NULL, 0);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicRequest_9003C(NpBasicClient *client, int *a2, void *a3)
{
	int ret;
	NpBasicIpmiOut out[2];
	int result;

	ret = 0;
	out[0].data = a2;
	out[0].size = 4;
	out[1].data = a3;
	out[1].size = 1016;
	result = client->vtbl->invoke(client, 0x9003C, NULL, 0, &ret, out, 2);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicRequest_9003D(NpBasicClient *client, const void *a2, int *a3)
{
	int ret;
	NpBasicIpmiIn in[1];
	NpBasicIpmiOut out[1];
	int result;

	ret = 0;
	in[0].data = a2;
	in[0].size = 36;
	out[0].data = a3;
	out[0].size = 4;
	result = client->vtbl->invoke(client, 0x9003D, in, 1, &ret, out, 1);
	if (result == 0)
		result = ret;
	return result;
}

int npBasicRequest_9003E(NpBasicClient *client, const void *a2, int a3, void *a4)
{
	int ret;
	NpBasicIpmiIn in[2];
	NpBasicIpmiOut out[1];
	int arg;
	int result;

	ret = 0;
	arg = a3;
	in[0].data = &arg;
	in[0].size = 4;
	in[1].data = a2;
	in[1].size = 36;
	out[0].data = a4;
	out[0].size = 712;
	result = client->vtbl->invoke(client, 0x9003E, in, 2, &ret, out, 1);
	if (result == 0)
		result = ret;
	return result;
}
