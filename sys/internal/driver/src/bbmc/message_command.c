/* SceBbmc message packet allocation and ownership. */
#include <stddef.h>
#include <sysmem/debug.h>
#include "bbmc_message.h"
#include "bbmc_control.h"
#include "bbmc_mailbox.h"
#include "bbmc_memory.h"

_Static_assert(sizeof(BbmcStringPairList) == 108, "string pair list size");
_Static_assert(sizeof(BbmcMessage) == 124, "message packet size");
_Static_assert(offsetof(BbmcMessage, notificationValue1) == 116,
	       "message notification word offset");

/* name unknown: allocates the complete packet and sets only its command. */
BbmcMessage *msgAllocMsgPacket(BbmcControl *control, SceUInt32 command)
{
	BbmcMessage *message;

	if (control == NULL)
		return NULL;
	message = bbmcAlloc(sizeof(*message), 0);
	if (message != NULL)
		message->command = command;
	return message;
}

/* name unknown: command variants own different sets of allocated strings. */
void msgFreeMsgPacket(BbmcControl *control, BbmcMessage *message)
{
	int i;
	BbmcStringPairEntry *entry;

	if (message == NULL || control == NULL)
		return;
	switch (message->command) {
	case 119:
	case 122:
		if (message->body.stringPair.first != NULL)
			bbmcFree(message->body.stringPair.first);
		if (message->body.stringPair.second != NULL)
			bbmcFree(message->body.stringPair.second);
		break;
	case 120:
	case 125:
	case 126:
		if (message->body.stringSet.strings[0] != NULL)
			bbmcFree(message->body.stringSet.strings[0]);
		if (message->body.stringSet.strings[1] != NULL)
			bbmcFree(message->body.stringSet.strings[1]);
		if (message->body.stringSet.strings[2] != NULL)
			bbmcFree(message->body.stringSet.strings[2]);
		if (message->body.stringSet.strings[3] != NULL)
			bbmcFree(message->body.stringSet.strings[3]);
		if (message->body.stringSet.strings[4] != NULL)
			bbmcFree(message->body.stringSet.strings[4]);
		break;
	case 121:
		i = 0;
		entry = message->body.stringPairs.entries;
		do {
			BbmcStringPair *pair = &entry->strings;

			++i;
			++entry;
			if (pair->first != NULL)
				bbmcFree(pair->first);
			if (pair->second != NULL)
				bbmcFree(pair->second);
		} while (i != 5);
		break;
	}
	bbmcFree(message);
}

int msgSendMsgPacket(BbmcControl *control, BbmcMessage *message)
{
	int ret;

	if (message == NULL || control == NULL)
		SCE_PANIC(178);
	ret = bbmcSendMailbox(control->mailbox, &message->queue);
	if (ret < 0)
		SCE_PANIC(186);
	return ret;
}

/* a message with no body: allocates it with the command alone and posts it */
int bbmcSendSimpleMessage(BbmcControl *control, unsigned int command)
{
	BbmcMessage *message;
	int ret;

	if (control == NULL)
		return 0x80330002;
	message = bbmcAlloc(sizeof(*message), 0);
	if (message == NULL)
		return 0x80330005;
	message->command = command;
	ret = bbmcSendMailbox(control->mailbox, &message->queue);
	if (ret < 0)
		bbmcFree(message);
	return ret;
}

/* name unknown: waits without a timeout for the next message of the manager thread */
int msgReceiveMsgPacket(BbmcControl *control, BbmcMessage **message)
{
	BbmcQueueEntry *entry = NULL;
	int ret;

	if (message == NULL || control == NULL)
		return 0x80330002;
	*message = NULL;
	ret = bbmcReceiveMailbox(control->mailbox, &entry, NULL);
	if (ret >= 0)
		*message = (BbmcMessage *)entry;
	return ret;
}
