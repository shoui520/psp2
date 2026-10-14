/* Authorized transition family; Sony's source filename is not recorded. */
#include <sce/syscon.h>
#include "../bbmc_message.h"

SceUInt32 authorizedTransitionStep;
int authorizedSysconPending;

int authorizedSysconReply(SceSysconPacket *packet, void *argp)
{
	BbmcControl *control = argp;
	BbmcMessage *message;
	SceUInt32 step = authorizedTransitionStep;
	int result = 0;

	if (packet->request.command[0] != 0x80)
		goto done;
	if (packet->request.command[1] != 0x0a)
		goto done;
	switch (step) {
	case 2:
	case 3:
		break;
	default:
		goto done;
	}
	message = msgAllocMsgPacket(control, 23);
	message->body.sysconReplyHalfword = packet->reply.data[0] |
		(packet->reply.data[1] << 8);
	result = msgSendMsgPacket(control, message);
done:
	authorizedSysconPending = 0;
	return result;
}

void authorizedTimeoutHandler(void *arg)
{
	BbmcControl *control = arg;
	BbmcMessage *message;

	if (authorizedTransitionStep == 2) {
		message = msgAllocMsgPacket(control, 187);
		if (message) {
			message->body.timedOutCommand = 67;
			msgSendMsgPacket(control, message);
		}
	}
}
