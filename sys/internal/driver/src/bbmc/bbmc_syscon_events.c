#include <syscon/syscon.h>
#include "bbmc_message.h"

/* Reviewed source grouping: the original filename is unavailable. */
SceUInt32 sysconEventStep;
int sysconEventPending;
int sysconStatusPending;

int bbmcSysconEventReply(SceSysconPacket *packet, void *argp)
{
	BbmcControl *control = argp;
	BbmcMessage *message;
	SceUInt32 step = sysconEventStep;
	int result = 0;

	if (packet->request.command[0] != 0x80)
		goto out;
	if (packet->request.command[1] != 0x0a)
		goto out;
	if (step == 1) {
		message = msgAllocMsgPacket(control, 2);
		message->body.sysconReplyWords.words[0] =
			((SceUInt32)packet->reply.data[2] << 16) |
			((SceUInt32)packet->reply.data[3] << 24) |
			packet->reply.data[0] |
			((SceUInt32)packet->reply.data[1] << 8);
		message->body.sysconReplyWords.words[1] =
			((SceUInt32)packet->reply.data[6] << 16) |
			((SceUInt32)packet->reply.data[7] << 24) |
			packet->reply.data[4] |
			((SceUInt32)packet->reply.data[5] << 8);
	} else if (step == 2) {
		message = msgAllocMsgPacket(control, 23);
		message->body.sysconReplyHalfword = packet->reply.data[0] |
			((SceUInt16)packet->reply.data[1] << 8);
	} else {
		goto out;
	}
	result = msgSendMsgPacket(control, message);
	if (result < 0)
		msgFreeMsgPacket(control, message);
out:
	sysconEventPending = 0;
	return result;
}

int bbmcSysconStatusReply(SceSysconPacket *packet, void *argp)
{
	BbmcControl *control = argp;
	BbmcMessage *message = msgAllocMsgPacket(control, 5);

	if (message && (packet->request.command[0] |
		       ((SceUInt16)packet->request.command[1] << 8)) == 0x0a80) {
		message->body.sysconReplyFlags =
			((SceUInt32)packet->reply.data[2] << 16) |
			((SceUInt32)packet->reply.data[3] << 24) |
			packet->reply.data[0] |
			((SceUInt32)packet->reply.data[1] << 8);
		if (msgSendMsgPacket(control, message) < 0)
			msgFreeMsgPacket(control, message);
	}
	sysconStatusPending = 0;
	return 0;
}
