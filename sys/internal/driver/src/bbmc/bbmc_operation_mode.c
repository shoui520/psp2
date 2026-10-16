/* Operation-mode request family; Sony's source filename is unavailable. */
#include "bbmc_message.h"

void operationModeTimeoutHandler(void *arg)
{
	BbmcControl *control = arg;
	BbmcMessage *message = msgAllocMsgPacket(control, 187);

	if (message) {
		message->body.timedOutCommand = 117;
		if (msgSendMsgPacket(control, message) < 0)
			msgFreeMsgPacket(control, message);
	}
}
