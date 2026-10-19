/* AVDTP channels; original source filename unavailable. */
#include "bt_internal.h"

void BtAvdtpClosed(BtConnection *connection, BtChannel *channel)
{
	BtAvdtpState *avdtp = &connection->avdtp;
	int state;

	if (avdtp->signalingChannel == channel) {
		connection->flags &= ~0x20;
		avdtp->signalingChannel = NULL;
		BtDisconnectConnection(connection->stack, connection);
		return;
	}
	if (avdtp->streams[0].channel == channel) {
		state = avdtp->streams[0].state;
		avdtp->streams[0].channel = NULL;
		if (state == 13) {
			BtClearConnectedProfiles(connection, NULL, 4);
			avdtp->streams[0].state = 3;
		} else if (state == 1) {
			BtClearConnectedProfiles(connection, NULL, 4);
		} else {
			avdtp->streams[0].state = 1;
		}
	} else if (avdtp->streams[1].channel == channel) {
		state = avdtp->streams[1].state;
		avdtp->streams[1].channel = NULL;
		if (state == 13) {
			BtClearConnectedProfiles(connection, NULL, 8);
			avdtp->streams[1].state = 3;
		} else if (state == 1) {
			BtClearConnectedProfiles(connection, NULL, 8);
		} else {
			avdtp->streams[1].state = 1;
		}
	}
	BtDisconnectConnection(connection->stack, connection);
}

uintptr_t BtAvdtpSignalingOpened(BtConnection *connection, BtChannel *channel)
{
	BtAvdtpState *avdtp = &connection->avdtp;
	unsigned int label;

	if (channel->flags & 2)
		return (uintptr_t)connection;
	label = (avdtp->transactionLabel + 16) & 0xf0;
	while (label == avdtp->streams[0].transactionLabel ||
	       label == avdtp->streams[1].transactionLabel)
		label = (label + 16) & 0xf0;
	avdtp->transactionLabel = label;
	return BtSendL2capPacket(connection, channel->remoteCid, "11", label, 1);
}

uintptr_t BtAvdtpTransmitOpened(BtConnection *connection, BtChannel *channel)
{
	(void)channel;
	if (connection->avdtp.streams[1].state == 7) {
		connection->avdtp.streams[1].state = 8;
		return BtSetConnectedProfiles(connection, NULL, 8);
	}
	return (uintptr_t)connection;
}

uintptr_t BtAvdtpReceiveOpened(BtConnection *connection, BtChannel *channel)
{
	(void)channel;
	if (connection->avdtp.streams[0].state == 7) {
		connection->avdtp.streams[0].state = 8;
		return BtSetConnectedProfiles(connection, NULL, 4);
	}
	return (uintptr_t)connection;
}

uintptr_t BtRequestAvdtpTransmitDisconnect(BtConnection *connection,
					 BtChannel *channel)
{
	BtAvdtpState *avdtp = &connection->avdtp;
	BtAvdtpStream *stream = &avdtp->streams[1];
	BtChannel *signalingChannel;
	unsigned int label;
	int command;

	(void)channel;
	if ((stream->state & ~2) != 8)
		return 0;
	signalingChannel = avdtp->signalingChannel;
	if (!signalingChannel)
		return 0;
	if (connection->flags & 0x1000) {
		stream->state = 14;
		label = (avdtp->transactionLabel + 16) & 0xf0;
		while (label == avdtp->streams[0].transactionLabel ||
		       label == avdtp->streams[1].transactionLabel)
			label = (label + 16) & 0xf0;
		avdtp->transactionLabel = label;
		command = 10;
	} else {
		stream->state = 12;
		label = (avdtp->transactionLabel + 16) & 0xf0;
		while (label == avdtp->streams[0].transactionLabel ||
		       label == avdtp->streams[1].transactionLabel)
			label = (label + 16) & 0xf0;
		avdtp->transactionLabel = label;
		command = 8;
	}
	stream->transactionLabel = label;
	BtSendL2capPacket(connection, signalingChannel->remoteCid,
			 "11 1", label, command, stream->remoteSeid & 0xfc);
	return 1;
}

uintptr_t BtRequestAvdtpReceiveDisconnect(BtConnection *connection,
					BtChannel *channel)
{
	BtAvdtpState *avdtp = &connection->avdtp;
	BtAvdtpStream *stream = &avdtp->streams[0];
	BtChannel *signalingChannel;
	unsigned int label;
	int command;

	(void)channel;
	if ((stream->state & ~2) != 8)
		return 0;
	signalingChannel = avdtp->signalingChannel;
	if (!signalingChannel)
		return 0;
	if (connection->flags & 0x1000) {
		stream->state = 14;
		label = (avdtp->transactionLabel + 16) & 0xf0;
		while (label == avdtp->streams[0].transactionLabel ||
		       label == avdtp->streams[1].transactionLabel)
			label = (label + 16) & 0xf0;
		avdtp->transactionLabel = label;
		command = 10;
	} else {
		stream->state = 12;
		label = (avdtp->transactionLabel + 16) & 0xf0;
		while (label == avdtp->streams[0].transactionLabel ||
		       label == avdtp->streams[1].transactionLabel)
			label = (label + 16) & 0xf0;
		avdtp->transactionLabel = label;
		command = 8;
	}
	stream->transactionLabel = label;
	BtSendL2capPacket(connection, signalingChannel->remoteCid,
			 "11 1", label, command, stream->remoteSeid & 0xfc);
	return 1;
}

int BtInitializeAvdtp(BtConnection *connection, BtChannel *channel,
		      unsigned int profile)
{
	BtAvdtpState *avdtp = &connection->avdtp;
	BtAvdtpStream *stream;
	BtChannel *signalingChannel;
	unsigned int flags;
	unsigned int label;

	if (!channel) {
		channel = BtFindChannel(connection, 20, 25);
		if (!channel)
			return (int)0x802f0501;
	}
	flags = connection->flags;
	if (!(flags & 0x20)) {
		connection->flags = flags | 0x20;
		avdtp->signalingChannel = channel;
		channel->ops = &gBtAvdtpSignalingOps;
		avdtp->streams[0].state = 1;
		avdtp->streams[0].localSeid = 8;
		avdtp->streams[0].transactionLabel = 15;
		avdtp->streams[1].state = 1;
		avdtp->streams[1].transactionLabel = 15;
		avdtp->streams[1].localSeid = 4;
		avdtp->requestedProfile = profile;
		if (!(channel->flags & 2))
			return BtSendL2capConnectionRequest(connection, channel);
		return 0;
	}
	if (channel->flags & 2) {
		if (avdtp->pendingLocalEndpoint == 8) {
			if (avdtp->streams[0].state == 6) {
				avdtp->streams[0].channel = channel;
				channel->ops = &gBtAvdtpReceiveOps;
				avdtp->pendingLocalEndpoint = 0;
				avdtp->streams[0].state = 7;
			}
		} else if (avdtp->pendingLocalEndpoint == 4 &&
			   avdtp->streams[1].state == 6) {
			avdtp->streams[1].channel = channel;
			channel->ops = &gBtAvdtpTransmitOps;
			avdtp->pendingLocalEndpoint = 0;
			avdtp->streams[1].state = 7;
		}
		return 0;
	}
	stream = &avdtp->streams[0];
	if (stream->state != 3) {
		stream = &avdtp->streams[1];
		if (stream->state != 3)
			return 0;
	}
	stream->state = 4;
	label = (avdtp->transactionLabel + 16) & 0xf0;
	while (label == avdtp->streams[0].transactionLabel ||
	       label == avdtp->streams[1].transactionLabel)
		label = (label + 16) & 0xf0;
	avdtp->transactionLabel = label;
	stream->transactionLabel = label;
	BtSelectAudioCapabilities(connection, profile, stream->capabilities,
				  stream->capabilityLength);
	signalingChannel = avdtp->signalingChannel;
	if (signalingChannel)
		BtSendL2capPacket(connection, signalingChannel->remoteCid,
			"11 11#", stream->transactionLabel, 3,
			stream->remoteSeid, stream->localSeid,
			stream->capabilities, stream->capabilityLength);
	return 0;
}

int BtStartAvdtpStream(BtConnection *connection, unsigned int profile)
{
	BtAvdtpState *avdtp = &connection->avdtp;
	BtAvdtpStream *stream;
	BtChannel *channel;
	unsigned int label;

	if (profile == 4)
		stream = &avdtp->streams[0];
	else if (profile == 8)
		stream = &avdtp->streams[1];
	else
		return (int)0x802f0504;
	if (stream->state == 10) {
		BtNotifyAudioStarted(connection, 0, stream->localSeid == 8 ? 4 : 8);
		return 0;
	}
	if (stream->state != 8)
		return (int)0x802f0505;
	stream->state = 9;
	label = (avdtp->transactionLabel + 16) & 0xf0;
	while (label == avdtp->streams[0].transactionLabel ||
	       label == avdtp->streams[1].transactionLabel)
		label = (label + 16) & 0xf0;
	avdtp->transactionLabel = label;
	stream->transactionLabel = label;
	stream->startDeferred = 0;
	channel = avdtp->signalingChannel;
	if (channel)
		return BtSendL2capPacket(connection, channel->remoteCid,
					"11 1", label, 7, stream->remoteSeid & 0xfc);
	return 0;
}

int BtStopAvdtpStream(BtConnection *connection, unsigned int profile)
{
	BtAvdtpState *avdtp = &connection->avdtp;
	BtAvdtpStream *stream;
	BtChannel *channel;
	unsigned int label;

	if (profile == 4)
		stream = &avdtp->streams[0];
	else if (profile == 8)
		stream = &avdtp->streams[1];
	else
		return (int)0x802f0506;
	if (stream->state != 10) {
		BtNotifyAudioStopped(connection, 0, stream->localSeid == 8 ? 4 : 8);
		return 0;
	}
	stream->state = 11;
	label = (avdtp->transactionLabel + 16) & 0xf0;
	while (label == avdtp->streams[0].transactionLabel ||
	       label == avdtp->streams[1].transactionLabel)
		label = (label + 16) & 0xf0;
	avdtp->transactionLabel = label;
	stream->transactionLabel = label;
	stream->startDeferred = 0;
	channel = avdtp->signalingChannel;
	if (channel)
		return BtSendL2capPacket(connection, channel->remoteCid,
					"11 1", label, 9, stream->remoteSeid & 0xfc);
	return 0;
}
