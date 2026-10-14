/* Audio profiles and packet accounting; original source filename unavailable. */
#include "bt_internal.h"

int BtStartAvdtpStream(BtConnection *connection, unsigned int profile);
int BtStartScoConnection(BtConnection *connection);
int BtStopScoConnection(BtConnection *connection);
int BtStopAvdtpStream(BtConnection *connection, unsigned int profile);
BtConnection *BtFindConnection(BtStack *stack, unsigned char selector,
			       uint64_t deviceAddress);
BtChannel *BtFindChannel(BtConnection *connection, unsigned char selector,
			 int identifier);
int BtSendHciCommand(BtStack *stack, unsigned int opcode,
		     const char *format, ...);
uintptr_t BtNotifyAudioStarted(BtConnection *connection, int status,
			     unsigned int profiles);
uintptr_t BtNotifyAudioStopped(BtConnection *connection, int status,
			     unsigned int profiles);
int BtDisconnectConnection(BtStack *stack, BtConnection *connection);

int gBtAudioUnderruns;
int gBtAudioOverruns;
int gBtPendingAudioPackets;
int gBtAudioLastSequence;
uint64_t gBtAudioContiguousPackets;
uint64_t gBtAudioIntervalStartUs;
uint64_t gBtAudioIntervalEndUs;

int BtNotifyPendingProfilesConnected(BtConnection *connection, BtChannel *channel,
				     unsigned int profiles)
{
	unsigned int pending;

	if (channel)
		return 0;
	pending = connection->pendingStartProfiles;
	if (pending & profiles & 4) {
		connection->pendingStartProfiles = pending & ~4;
		connection->startingProfiles |= 4;
		BtStartAvdtpStream(connection, 4);
		pending = connection->pendingStartProfiles;
	}
	if (pending & profiles & 8) {
		connection->pendingStartProfiles = pending & ~8;
		connection->startingProfiles |= 8;
		BtStartAvdtpStream(connection, 8);
	}
	if (!(profiles & 0x20000000))
		return 0;
	connection->connectedProfiles |= 0x20000000;
	pending = connection->pendingStartProfiles;
	if (!(pending & 0x20000000))
		return 0;
	connection->pendingStartProfiles = pending & ~0x20000000;
	connection->startingProfiles |= 0x20000000;
	BtStartScoConnection(connection);
	return 0;
}

int BtStartAudio(BtStack *stack, uint64_t deviceAddress, unsigned int profile)
{
	BtConnection *connection;
	int result;

	if (profile != 4 && profile != 8 && profile != 0x20000000)
		return (int)0x802f0d0b;
	connection = BtFindConnection(stack, 1, deviceAddress);
	if (!connection)
		return (int)0x802f0d01;
	if (!(profile & connection->device->profiles))
		return (int)0x802f0d02;
	if (profile & connection->excludedProfiles)
		return (int)0x802f0d0d;
	if (!(profile & connection->connectedProfiles)) {
		connection->pendingStartProfiles |= profile;
		return 0;
	}
	if (profile & connection->activeProfiles)
		return 0;
	if (profile & connection->startingProfiles)
		return 0;
	connection->startingProfiles |= profile;
	if (connection->linkMode == 2) {
		BtSendHciCommand(connection->stack, 0x804, "2", connection->aclHandle);
		connection->interval = 0;
	}
	if (profile & 4) {
		result = BtStartAvdtpStream(connection, 4);
		if (result < 0)
			return result;
		gBtAudioLastSequence = -1;
		gBtAudioContiguousPackets = 0;
		gBtAudioIntervalStartUs = 0;
		gBtAudioIntervalEndUs = 0;
	}
	if (profile & 8) {
		result = BtStartAvdtpStream(connection, 8);
		if (result < 0)
			return result;
	}
	if (profile & 0x20000000) {
		result = BtStartScoConnection(connection);
		return result < 0 ? result : 0;
	}
	return 0;
}

int BtStopAudio(BtStack *stack, uint64_t deviceAddress, unsigned int profile)
{
	BtConnection *connection;
	int result;

	if (profile != 4 && profile != 8 && profile != 0x20000000)
		return (int)0x802f0d0c;
	connection = BtFindConnection(stack, 1, deviceAddress);
	if (!connection)
		return (int)0x802f0d03;
	if (!(profile & connection->device->profiles))
		return (int)0x802f0d04;
	if (profile & connection->excludedProfiles)
		return (int)0x802f0d0e;
	if (!(profile & connection->connectedProfiles))
		connection->pendingStartProfiles &= ~profile;
	result = connection->activeProfiles & profile;
	if (!result)
		return result;
	connection->stoppingProfiles |= profile;
	if (connection->linkMode == 2) {
		BtSendHciCommand(connection->stack, 0x804, "2", connection->aclHandle);
		connection->interval = 0;
	}
	result = profile & 4;
	if (result) {
		result = BtStopAvdtpStream(connection, 4);
		if (result < 0)
			return result;
	}
	if (profile & 8) {
		result = BtStopAvdtpStream(connection, 8);
		if (result < 0)
			return result;
	}
	if (profile & 0x20000000)
		return BtStopScoConnection(connection);
	return result;
}

uintptr_t BtCompleteAudioPacket(BtConnection *connection)
{
	if (gBtPendingAudioPackets > 0)
		gBtPendingAudioPackets--;
	return (uintptr_t)connection;
}

int BtNotifyScoConnection(BtConnection *connection, int status, int handle)
{
	unsigned int profiles = connection->connectedProfiles;
	BtStack *stack = connection->stack;
	int identifier;
	BtChannel *channel;

	if (profiles & 1)
		identifier = connection->hfpHandsfreeChannel;
	else if (profiles & 2)
		identifier = connection->hfpAudioGatewayChannel;
	else if (profiles & 0x10000)
		identifier = connection->hspHeadsetChannel;
	else if (profiles & 0x20000)
		identifier = connection->hspAudioGatewayChannel;
	else
		return -1;
	channel = BtFindChannel(connection, 7, identifier);
	if (!channel)
		return -1;
	if (handle) {
		if (status) {
			channel->flags &= ~0x30000000;
			BtNotifyAudioStarted(connection, status, 0x20000000);
		} else {
			channel->flags = (channel->flags & ~0x30000000) | 0x20000000;
			connection->scoHandle = handle;
			BtNotifyAudioStarted(connection, 0, 0x20000000);
		}
	} else {
		channel->flags &= ~0x30000000;
		BtNotifyAudioStopped(connection, 0, 0x20000000);
		BtDisconnectConnection(stack, connection);
	}
	return 0;
}

int BtStartScoConnection(BtConnection *connection)
{
	unsigned int profiles = connection->connectedProfiles;
	unsigned int flags;
	int identifier;
	BtChannel *channel;

	if (profiles & 1)
		identifier = connection->hfpHandsfreeChannel;
	else if (profiles & 2)
		identifier = connection->hfpAudioGatewayChannel;
	else if (profiles & 0x10000)
		identifier = connection->hspHeadsetChannel;
	else if (profiles & 0x20000)
		identifier = connection->hspAudioGatewayChannel;
	else
		return (int)0x802f0801;
	channel = BtFindChannel(connection, 7, identifier);
	if (!channel)
		return (int)0x802f0801;
	flags = channel->flags;
	if (flags & 0x30000000)
		return (int)0x802f0802;
	channel->flags = (flags & ~0x30000000) | 0x10000000;
	BtSendHciCommand(connection->stack, 0x428, "2 44 2212",
			 connection->aclHandle, 8000, 8000, 5, 96, 255, 964);
	return 0;
}

int BtStopScoConnection(BtConnection *connection)
{
	unsigned int profiles = connection->connectedProfiles;
	unsigned int flags;
	BtStack *stack = connection->stack;
	int identifier;
	BtChannel *channel;

	if (profiles & 1)
		identifier = connection->hfpHandsfreeChannel;
	else if (profiles & 2)
		identifier = connection->hfpAudioGatewayChannel;
	else if (profiles & 0x10000)
		identifier = connection->hspHeadsetChannel;
	else if (profiles & 0x20000)
		identifier = connection->hspAudioGatewayChannel;
	else
		return (int)0x802f0805;
	channel = BtFindChannel(connection, 7, identifier);
	if (!channel)
		return (int)0x802f0805;
	flags = channel->flags;
	if ((flags & 0x30000000) != 0x20000000)
		return (int)0x802f0806;
	channel->flags = (flags & ~0x70000000) | 0x30000000;
	BtSendHciCommand(stack, 0x406, "21", connection->scoHandle, 19);
	return 0;
}
