#include <syscon/syscon.h>
#include "../bbmc_message.h"

int testModeSysconPending;

int testModeSysconReply(SceSysconPacket *packet, void *argp)
{
	BbmcControl *control = argp;
	BbmcMessage *message;
	if (packet->request.command[0] == 0x8d &&
	    packet->request.command[1] == 8) {
		message = msgAllocMsgPacket(control, 39);
		if (message)
			msgSendMsgPacket(control, message);
	}
	testModeSysconPending = 0;
	return 0;
}
