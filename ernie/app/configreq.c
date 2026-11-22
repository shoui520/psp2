/*
 * The settings' requests: request 0x13 takes the host's commands 0x1380-0x1386 on the
 * settings (app/config.c), and command 0x1300 answers their versions. The commands run in
 * task 2, at event flag 3's bit 0x4000. The slim's also take 0x1390-0x1394 on the backup
 * store's regions (app/bstore.c), and command 0x1310 answers their number.
 */
#include <string.h>

#include "board.h"
#include "bstore.h"
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

#if SOURCE_0103
unsigned char configRequestAnswer[0x40];
/* the region, the offset, the size, the data */
unsigned char configRequestData[4 + 0x40];
#else
unsigned char configRequestAnswer[0x20];
#endif
#if SOURCE_0103
#elif FW_SINCE(0x01000104)
unsigned char configRequestData[3 + 0x40];	/* the offset, the size, the data or records */
#elif FW_SINCE(0x00090502)
unsigned char configRequestData[0x40];
#if !FW_SINCE(0x00090804)
static struct ConfigRecords configRecords;	/* 0x00090502's, given over 64 bytes at a time */
#endif
#else
unsigned char configRequestData[0x20];
#endif
struct HostRequest configRequest;

void configRequestInit(void)
{
	configRequest.id = 0x13;
	configRequest.data = configRequestData;
	configRequest.answer = configRequestAnswer;
	configRequest.size = sizeof(configRequestData);
	configRequest.notify = configRequestNotify;
	configRequest.state = HOST_REQUEST_RECEIVE;
#if FW_SINCE(0x00090502) && !FW_SINCE(0x00090804)
	memset(&configRecords, 0, sizeof(configRecords));
#endif
}

/* 0x1300: the settings' versions (struct ConfigVersions) */
unsigned char hostCommandConfigVersions(unsigned char *out, unsigned char *length)
{
	configGetVersions((struct ConfigVersions *)out);
	*length = 8;
	return 0;
}

#if SOURCE_0103
/* 0x1310: the block store's regions */
unsigned char hostCommand1310(unsigned char *out, unsigned char *length)
{
#if BOARD_BSTORE
	*out = bstoreRegionCount();
#else
	*out = 0;
#endif
	*length = 1;
	return 0;
}
#endif

/* event flag 3's 0x4000: the host's command came in */
void configRequestEvent(FLGPTN ptn)
{
	unsigned char result;
	union ConfigRequestOffset offset;
	unsigned char size;
#if FW_SINCE(0x00090502)
	unsigned char length;
#endif

	if (ptn & 0x4000) {
		if (!handshakeUnlocked11()) {
			hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
			handshakeRefuse();
		} else {
			switch (configRequest.command) {
			case 0x1380:		/* read */
				if (configRequest.length == 3) {
#if FW_SINCE(0x00090804)
					offset.bytes.low = configRequestData[0];
					offset.bytes.high = configRequestData[1];
#else
					offset.value = configRequestData[0] |
						       (unsigned int)configRequestData[1] << 8;
#endif
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
#if FW_SINCE(0x00090804)
					offset.bytes.low = configRequestData[0];
					offset.bytes.high = configRequestData[1];
#else
					offset.value = configRequestData[0] |
						       (unsigned int)configRequestData[1] << 8;
#endif
					size = configRequestData[2];
					result = configWrite(offset.value, size, &configRequestData[3]);
					hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, result, 0);
				} else {
					hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
				}
				break;
			case 0x1382:
#if FW_SINCE(0x00090502) && !FW_SINCE(0x00090804)
				configRecords.count = 0;
#endif
				result = configOpen();
				hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, result, 0);
				break;
			case 0x1383:
				result = configClose();
				hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, result, 0);
				break;
			case 0x1384:
#if FW_SINCE(0x00090502) && !FW_SINCE(0x00090804)
				/* what is left of the records first */
				result = 0;
				if (configRecords.count != 0) {
					result = configApplyRecords(configRecords.buffer,
								    configRecords.count);
					configRecords.count = 0;
				}
				if (result == 0)
					result = configCommit();
#else
				result = configCommit();
#endif
				hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, result, 0);
				break;
#if FW_SINCE(0x00090804)
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
#elif FW_SINCE(0x00090502)
			case 0x1385:		/* records for the open copy */
				if (configRequest.length < 4)
					hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
				result = 0;
				length = configRequestData[2];
				if (configRequest.length - 3 != length)
					result = 0x3c;
				/* each chunk from the command's first byte of records */
				while (length != 0 && result == 0) {
					size = SHA1BlockSize - configRecords.count > length ?
						length : SHA1BlockSize - configRecords.count;
					memcpy(&configRecords.buffer[configRecords.count],
					       &configRequestData[3], size);
					configRecords.count += size;
					if (configRecords.count == SHA1BlockSize) {
						result = configApplyRecords(configRecords.buffer,
									    configRecords.count);
						configRecords.count = 0;
					}
					length -= size;
				}
				hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, result, 0);
				break;
			case 0x1386:		/* records compared with the bank used; no short check */
				result = 0;
				length = configRequestData[2];
				if (configRequest.length - 3 != length)
					result = 0x3c;
				while (length != 0 && result == 0) {
					size = SHA1BlockSize - configRecords.count > length ?
						length : SHA1BlockSize - configRecords.count;
					memcpy(&configRecords.buffer[configRecords.count],
					       &configRequestData[3], size);
					configRecords.count += size;
					if (configRecords.count == SHA1BlockSize) {
						result = configCompareRecords(configRecords.buffer,
									      configRecords.count);
						configRecords.count = 0;
					}
					length -= size;
				}
				hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, result, 0);
				break;
#else
			case 0x1385:	/* 0x00090206: the records' number, answered as unknown */
#endif
#if BOARD_BSTORE
			case 0x1390:		/* a block store region's read */
				if (configRequest.length == 4) {
					offset.bytes.low = configRequestData[1];
					offset.bytes.high = configRequestData[2];
					size = configRequestData[3];
					result = bstoreRead(configRequestData[0], offset.value, size,
							    configRequestAnswer);
				} else
					result = 0x3b;
				hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, result,
						  result == 0 ? size : 0);
				break;
			case 0x1391:		/* write */
				if (configRequest.length >= 5) {
					offset.bytes.low = configRequestData[1];
					offset.bytes.high = configRequestData[2];
					size = configRequestData[3];
					result = bstoreWrite(configRequestData[0], offset.value, size,
							     &configRequestData[4]);
				} else
					result = 0x3b;
				hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, result, 0);
				break;
			case 0x1392:		/* open */
				if (configRequest.length == 2)
					result = bstoreOpen(configRequestData[0], configRequestData[1]);
				else
					result = 0x3b;
				hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, result, 0);
				break;
			case 0x1393:		/* close */
				if (configRequest.length == 1)
					result = bstoreClose(configRequestData[0]);
				else
					result = 0x3b;
				hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, result, 0);
				break;
			case 0x1394:		/* commit */
				if (configRequest.length == 1)
					result = bstoreCommit(configRequestData[0]);
				else
					result = 0x3b;
				hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, result, 0);
				break;
#elif BOARD_DOL1001
			case 0x1390:		/* the block store's commands: no regions, status 3 */
			case 0x1391:
			case 0x1392:
			case 0x1393:
			case 0x1394:
				hostRequestAnswer(&configRequest, HOST_REQUEST_ANSWER, 3, 0);
				break;
#endif
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
#if SOURCE_0103
	case HOST_EVENT_OPEN:
		hostRequestSetState(&configRequest, HOST_REQUEST_RECEIVE);
		break;
	case HOST_EVENT_CLOSE:
		hostRequestSetState(&configRequest, HOST_REQUEST_IDLE);
		break;
#endif
	}
}
