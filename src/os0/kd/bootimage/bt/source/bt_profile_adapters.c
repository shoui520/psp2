/* RFCOMM profile adapters; original source filename unavailable. */
#include "bt_internal.h"

int BtInitializeRfcomm(BtConnection *connection, BtChannel *channel,
		      unsigned int profile, unsigned int dlci);

int BtInitializeImagingSession(BtConnection *connection, BtChannel *channel,
			       unsigned int profile)
{
	unsigned int dlci = connection->device->imagingChannel;

	connection->imagingChannel = dlci;
	return BtInitializeRfcomm(connection, channel, profile, dlci);
}

int BtInitializeImagingProfile(BtConnection *connection, BtChannel *channel,
			       unsigned int profile)
{
	unsigned int dlci = connection->device->imagingChannel;

	connection->imagingChannel = dlci;
	return BtInitializeRfcomm(connection, channel, profile, dlci);
}

int BtInitializeHsp(BtConnection *connection, BtChannel *channel)
{
	unsigned int dlci;

	if (connection->flags & 1)
		connection->hsp.buttonCount++;
	dlci = connection->device->headsetChannel;
	connection->hspHeadsetChannel = dlci;
	return BtInitializeRfcomm(connection, channel, 0x10000, dlci);
}

int BtInitializeSerialPort(BtConnection *connection, BtChannel *channel)
{
	unsigned int dlci = connection->device->serialPortChannel;

	connection->serialPortChannel = dlci;
	return BtInitializeRfcomm(connection, channel, 0x80000, dlci);
}
