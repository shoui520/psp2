/*
 * The settings' requests: request 0x13 takes the host's commands 0x1380-0x1386 on the
 * settings (app/config.c), and command 0x1300 answers their versions. The commands run in
 * task 2, at event flag 3's bit 0x4000.
 */
#include "config.h"
#include "configreq.h"
#include "handshake.h"
#include "host.h"
#include "kernel.h"
#include "task.h"

/* a command's offset, sent low byte first */
union ConfigRequestOffset {
	unsigned int	value;
	struct {
		unsigned char	low;
		unsigned char	high;
	} bytes;
};

unsigned char configRequestAnswer[0x20];
unsigned char configRequestData[3 + 0x40];	/* the offset, the size, the data or records */
struct HostRequest configRequest;

void configRequestInit(void)
{
	configRequest.id = 0x13;
	configRequest.data = configRequestData;
	configRequest.answer = configRequestAnswer;
	configRequest.size = sizeof(configRequestData);
	configRequest.notify = configRequestNotify;
	configRequest.state = HOST_REQUEST_RECEIVE;
}

/* 0x1300: the settings' versions (struct ConfigVersions) */
unsigned char hostCommandConfigVersions(unsigned char *out, unsigned char *length)
{
	configGetVersions((struct ConfigVersions *)out);
	*length = 8;
	return 0;
}

/* event flag 3's 0x4000: the host's command came in */
void configRequestEvent(FLGPTN ptn)
{
	unsigned char result;
	union ConfigRequestOffset offset;
	unsigned char size;
	unsigned char length;

	if (ptn & 0x4000) {
		if (!handshakeUnlocked11()) {
			hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
			handshakeRefuse();
		} else {
			switch (configRequest.command) {
			case 0x1380:		/* read */
				if (configRequest.length == 3) {
					offset.bytes.low = configRequestData[0];
					offset.bytes.high = configRequestData[1];
					size = configRequestData[2];
					result = configRead(offset.value, size, configRequestAnswer);
					hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, result,
							  result == 0 ? size : 0);
				} else {
					hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
				}
				break;
			case 0x1381:		/* write */
				if (configRequest.length >= 4) {
					offset.bytes.low = configRequestData[0];
					offset.bytes.high = configRequestData[1];
					size = configRequestData[2];
					result = configWrite(offset.value, size, &configRequestData[3]);
					hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, result, 0);
				} else {
					hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
				}
				break;
			case 0x1382:
				result = configOpen();
				hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, result, 0);
				break;
			case 0x1383:
				result = configClose();
				hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, result, 0);
				break;
			case 0x1384:
				result = configCommit();
				hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, result, 0);
				break;
			case 0x1385:		/* records */
				/* a short command is answered, and still run */
				if (configRequest.length < 4)
					hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
				offset.bytes.low = configRequestData[0];
				offset.bytes.high = configRequestData[1];
				length = configRequestData[2];
				if (configRequest.length - 3 == length)
					result = configWriteRecords(offset.value, length,
								    &configRequestData[3], 0);
				else
					result = 0x3c;
				hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, result, 0);
				break;
			case 0x1386:		/* records, checked */
				if (configRequest.length < 4)
					hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
				offset.bytes.low = configRequestData[0];
				offset.bytes.high = configRequestData[1];
				length = configRequestData[2];
				if (configRequest.length - 3 == length)
					result = configWriteRecords(offset.value, length,
								    &configRequestData[3], 1);
				else
					result = 0x3c;
				hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, result, 0);
				break;
			default:
				hostRequestAnswer(&configRequest, HOST_REQUEST_UNKNOWN, 0, 0);
				break;
			}
		}
	}
}

/* request 0x13's: the command runs in task 2 */
void configRequestNotify(void)
{
	switch (configRequest.event) {
	case HOST_EVENT_COMMAND:
		hostRequestSetState(&configRequest, HOST_REQUEST_BUSY);
		task2SetFlag(3, 0x4000);
		break;
	case HOST_EVENT_ANSWERED:
		hostRequestSetState(&configRequest, HOST_REQUEST_RECEIVE);
		break;
	}
}
