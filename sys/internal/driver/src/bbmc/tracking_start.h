/* Sony: qmi_solution/cmd_sequencer/tracking_start.h */
#ifndef BBMC_TRACKING_START_H
#define BBMC_TRACKING_START_H

#include <sysmem/debug.h>

#if FW_DEBUG
static const SceKernelDebugInfo dbg__constructTrackingStartQmi_L321
	__attribute__((section(".sce_dbginfo"), used)) = {
	.file = "qmi_solution/cmd_sequencer/tracking_start.h",
	.line = 321,
	.func = "_constructTrackingStartQmi",
};

static const SceKernelDebugInfoSource dbg__constructTrackingStartQmi_L321Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg__constructTrackingStartQmi_L321,
	"qmi_solution/cmd_sequencer/tracking_start.h", 321,
	"_constructTrackingStartQmi",
};
#endif

#define APPEND_TRACKING_MESSAGE(id, source, size) do { \
	client = *clientPointer; \
	if (client->service != 6) \
		return 0x80330002; \
	length = client->request.length; \
	if (length + sizeof(*message) + (size) > sizeof(client->request.data)) \
		return 0x80330009; \
	message = (QmiMessageHeader *)(client->request.data + length); \
	message->messageIdLow = (id); \
	message->messageIdHigh = 0; \
	message->lengthLow = (size); \
	message->lengthHigh = 0; \
	client->request.length = length + sizeof(*message); \
	if ((size) != 0) { \
		memmove(client->request.data + length + sizeof(*message), (source), (size)); \
		client->request.length += (size); \
	} \
} while (0)

int _constructTrackingStartQmi(BbcmdProtocolClient **clientPointer,
	BbcmdArgument *arguments, unsigned int command)
{
	QmiPdsReportOption options[10] = {
		{ 16, 1 }, { 18, 2 }, { 19, 4 }, { 20, 8 }, { 21, 16 },
		{ 22, 32 }, { 23, 64 }, { 24, 128 }, { 25, 256 }, { 26, 512 },
	};
	BbcmdProtocolClient *client = *clientPointer;
	QmiByteValueTlv *scalar;
	QmiMessageHeader *message;
	unsigned char *scratch;
	unsigned int length, size, index;

	scalar = (QmiByteValueTlv *)(client->request.data +
		sizeof(client->request.data) - 1 - sizeof(*scalar));
	scalar->header.type = 1;
	scalar->header.lengthLow = 1;
	scalar->value = 1;
	scalar->header.lengthHigh = 0;
	APPEND_TRACKING_MESSAGE(33, scalar, sizeof(*scalar));

	client = *clientPointer;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &dbg__constructTrackingStartQmi_L321,
		__builtin_return_address(0));
#endif
	scratch = client->request.data + sizeof(client->request.data) - 1 -
		10 * sizeof(QmiByteValueTlv);
	for (index = 0; index < 10; ++index) {
		QmiByteValueTlv *report = (QmiByteValueTlv *)scratch + index;

		report->header.type = options[index].type;
		report->header.lengthLow = 1;
		report->header.lengthHigh = 0;
		report->value = (options[index].reportMask & 0x22e) != 0;
	}
	APPEND_TRACKING_MESSAGE(1, scratch, 10 * sizeof(QmiByteValueTlv));
	if (command == 21) {
		QmiPdsTrackingSessionTlv *session;
		unsigned short interval;
		unsigned int accuracy;

		client = *clientPointer;
		session = (QmiPdsTrackingSessionTlv *)(client->request.data +
			sizeof(client->request.data) - 1 - sizeof(*session));
		session->header.type = 1;
		session->header.lengthHigh = 0;
		session->header.lengthLow = 10;
		session->sessionOperation = arguments[0].bytes[0];
		session->positionDataTimeout = arguments[0].bytes[1];
		__builtin_memcpy(&interval, &arguments[0].bytes[2], sizeof(interval));
		session->interval[0] = interval;
		session->interval[1] = interval >> 8;
		session->interval[2] = 0;
		session->interval[3] = 0;
		accuracy = arguments[1].value;
		session->accuracyThreshold[0] = accuracy;
		session->accuracyThreshold[1] = accuracy >> 8;
		session->accuracyThreshold[2] = accuracy >> 16;
		session->accuracyThreshold[3] = accuracy >> 24;
		APPEND_TRACKING_MESSAGE(42, session, sizeof(*session));
		client = *clientPointer;
		scalar = (QmiByteValueTlv *)(client->request.data +
			sizeof(client->request.data) - 1 - sizeof(*scalar));
		scalar->header.type = 1;
		scalar->header.lengthLow = 1;
		scalar->header.lengthHigh = 0;
		scalar->value = 1;
		APPEND_TRACKING_MESSAGE(49, scalar, sizeof(*scalar));
		return 0;
	}
	if (arguments[0].bytes[2] == 0) {
		APPEND_TRACKING_MESSAGE(53, NULL, 0);
		return 0;
	}
	client = *clientPointer;
	/* Original twelve-byte headroom can reach the adjacent response length. */
	scratch = (unsigned char *)client + offsetof(BbcmdProtocolClient, request) +
		offsetof(QmiPacketStorage, data) + sizeof(client->request.data) - 1 - 12;
	size = 0;
	if (arguments[0].bytes[2] & 1) {
		scalar = (QmiByteValueTlv *)(scratch + size);
		scalar->header.type = 16;
		scalar->header.lengthLow = 1;
		scalar->header.lengthHigh = 0;
		scalar->value = arguments[0].bytes[0];
		size += sizeof(*scalar);
	}
	if (arguments[0].bytes[2] & 2) {
		scalar = (QmiByteValueTlv *)(scratch + size);
		scalar->header.type = 17;
		scalar->header.lengthLow = 1;
		scalar->header.lengthHigh = 0;
		scalar->value = arguments[0].bytes[1];
		size += sizeof(*scalar);
	}
	if (arguments[0].bytes[2] & 4) {
		QmiWordValueTlv *value = (QmiWordValueTlv *)(scratch + size);
		unsigned int word;

		value->header.type = 18;
		value->header.lengthLow = 4;
		value->header.lengthHigh = 0;
		word = arguments[1].value;
		value->value[0] = word;
		value->value[1] = word >> 8;
		value->value[2] = word >> 16;
		value->value[3] = word >> 24;
		size += sizeof(*value);
	}
	APPEND_TRACKING_MESSAGE(53, scratch, size);
	return 0;
}

#undef APPEND_TRACKING_MESSAGE
#endif
