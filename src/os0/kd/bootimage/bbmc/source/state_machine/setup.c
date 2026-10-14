/* Modem setup family; Sony's source filename is unavailable. */
#include "../bbmc_message.h"

SceUInt32 modemSetupStep;

void modemSetupTimeoutHandler(void *arg)
{
	BbmcControl *control = arg;
	BbmcMessage *message;

	if (modemSetupStep == 1) {
		message = msgAllocMsgPacket(control, 187);
		if (message) {
			message->body.timedOutCommand = 181;
			msgSendMsgPacket(control, message);
		}
	} else if (modemSetupStep == 3) {
		message = msgAllocMsgPacket(control, 187);
		if (message) {
			message->body.timedOutCommand = 69;
			msgSendMsgPacket(control, message);
		}
	}
}
