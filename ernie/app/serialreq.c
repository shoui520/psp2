/*
 * Request 0x20: the host's commands on the serial link. 0x2080 and 0x2081 unlock and lock
 * it, 0x2082 restarts it in mode 1 (text or binary) at one of two speeds, 0x2083 reads mode
 * 1's buffer, 0x2084 writes it and sends the bytes written down the link, and 0x2085 does
 * either. All but the first two need the link unlocked and open in mode 1.
 */
#include "handshake.h"
#include "host.h"
#include "kernel.h"
#include "serial.h"
#include "serial1.h"
#include "serialreq.h"
#include "sleepfactor.h"
#include "task.h"

unsigned char serialRequestAnswer[0x28];
unsigned char serialRequestData[0x2b];		/* the offset, the size, the data */
struct HostRequest serialRequest;

void serialRequestInit(void)
{
	serialRequest.id = 0x20;
	serialRequest.state = HOST_REQUEST_RECEIVE;
	serialRequest.data = serialRequestData;
	serialRequest.answer = serialRequestAnswer;
	serialRequest.size = sizeof(serialRequestData);
	serialRequest.notify = serialRequestNotify;
}

void serialRequestReset(void)
{
	serialRequest.state = HOST_REQUEST_RECEIVE;
	sleepFactorSet25(0);
	serial1BufferClear();
	return;
}

void serialRequestNotify(void)
{
	unsigned char result;
	unsigned char offset;
	unsigned char size;

	if (!handshakeUnlocked11()) {
		hostRequestAnswer(&serialRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		handshakeRefuse();
		return;
	}
	switch (serialRequest.event) {
	case HOST_EVENT_COMMAND:
		if (serialRequest.command >= 0x2082 && (serialIsLocked() || serialGetLink() != 1)) {
			hostRequestAnswer(&serialRequest, HOST_REQUEST_ANSWER, 0x10, 0);
			return;
		}
		switch (serialRequest.command) {
		case 0x2080:		/* unlock */
			result = serialUnlock(1);
			hostRequestAnswer(&serialRequest, HOST_REQUEST_ANSWER, result, 0);
			break;
		case 0x2081:		/* lock */
			result = serialLock(1);
			hostRequestAnswer(&serialRequest, HOST_REQUEST_ANSWER, result, 0);
			break;
		case 0x2082:		/* restart: text (0) or binary (1), speed 2 or 3 */
			if (serialRequest.length == 4) {
				result = serialGetState();
				if (result != 0 && result != 1) {
					hostRequestAnswer(&serialRequest, HOST_REQUEST_ANSWER, 1, 0);
					break;
				}
				if (serialRequestData[0] == 0)
					serialStart(1);
				else if (serialRequestData[0] == 1)
					serialStart(SERIAL_BINARY | 1);
				else {
					hostRequestAnswer(&serialRequest, HOST_REQUEST_ANSWER, 3, 0);
					break;
				}
				if (serialRequestData[1] == 0 || serialRequestData[1] == 1) {
					result = serialReopen(serialRequestData[1] += 2);
					hostRequestAnswer(&serialRequest, HOST_REQUEST_ANSWER, 0, 0);
				} else
					hostRequestAnswer(&serialRequest, HOST_REQUEST_ANSWER, 3, 0);
			} else
				hostRequestAnswer(&serialRequest, HOST_REQUEST_ANSWER, 2, 0);
			break;
		case 0x2083:		/* read the buffer */
			if (serialRequest.length == 2) {
				offset = serialRequestData[0];
				size = serialRequestData[1];
				result = serial1BufferRead(offset, size, serialRequestAnswer);
				sleepFactorSet25(0);
				if (result != 0)
					hostRequestAnswer(&serialRequest, HOST_REQUEST_ANSWER, result, 0);
				else
					hostRequestAnswer(&serialRequest, HOST_REQUEST_ANSWER, 0, size);
			} else
				hostRequestAnswer(&serialRequest, HOST_REQUEST_ANSWER, 2, 0);
			break;
		case 0x2084:		/* write the buffer, and send what was written */
			if (serialRequest.length >= 3) {
				offset = serialRequestData[0];
				size = serialRequestData[1];
				result = serial1BufferWrite(offset, size, &serialRequestData[2]);
				if (result == 0) {
					dis_dsp();
					if (serialGetState() == SERIAL_RECEIVE) {
						result = serial1BufferRead(offset, size, serial.buffer);
						serial.command = SERIAL_REQUEST_FRAME;
						serial.status = 0;
						serial.length = size;
						task2SetFlag(3, 0x0010);
						hostRequestAnswer(&serialRequest, HOST_REQUEST_ANSWER, result, 0);
					} else
						hostRequestAnswer(&serialRequest, HOST_REQUEST_ANSWER, 5, 0);
					ena_dsp();
				}
			} else
				hostRequestAnswer(&serialRequest, HOST_REQUEST_ANSWER, 2, 0);
			break;
		case 0x2085:		/* 0: write the buffer, 1: send its bytes */
			if (serialRequest.length >= 3) {
				if (serialRequestData[0] == 0) {
					offset = serialRequestData[1];
					size = serialRequestData[2];
					result = serial1BufferWrite(offset, size, &serialRequestData[3]);
					hostRequestAnswer(&serialRequest, HOST_REQUEST_ANSWER, result, 0);
				} else if (serialRequestData[0] == 1) {
					offset = serialRequestData[1];
					size = serialRequestData[2];
					dis_dsp();
					if (serialGetState() == SERIAL_RECEIVE) {
						result = serial1BufferRead(offset, size, serial.buffer);
						serial.command = SERIAL_REQUEST_FRAME;
						serial.status = 0;
						serial.length = size;
						task2SetFlag(3, 0x0010);
						hostRequestAnswer(&serialRequest, HOST_REQUEST_ANSWER, 0, 0);
					} else
						hostRequestAnswer(&serialRequest, HOST_REQUEST_ANSWER, 5, 0);
					ena_dsp();
				} else
					hostRequestAnswer(&serialRequest, HOST_REQUEST_ANSWER, 3, 0);
			} else
				hostRequestAnswer(&serialRequest, HOST_REQUEST_ANSWER, 2, 0);
			break;
		default:
			hostRequestAnswer(&serialRequest, HOST_REQUEST_UNKNOWN, 0x3f, 0);
			break;
		}
		break;
	case HOST_EVENT_ANSWERED:
		hostRequestSetState(&serialRequest, HOST_REQUEST_RECEIVE);
		break;
	}
}
