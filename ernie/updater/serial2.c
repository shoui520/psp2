/*
 * The serial link's mode 2, as the updater has it (an older revision of the application's):
 * the commands of serialCommands2 and the notification of serialNotifies2, answered from
 * serial2Data or with no data, and the test area they keep in the scratch pad (0xe0-0xff).
 * P7.1 is low while the link runs in mode 2.
 */
#pragma sfr

#include <string.h>

#include "ernie.h"
#include "charge.h"
#include "scratchpad.h"
#include "serial.h"
#include "serial2.h"
#include "serial4.h"
#include "state.h"
#include "updater.h"
#include "version.h"

#define SERIAL2_TEST_AREA	0xe0	/* the scratch pad's test area */
#define SERIAL2_TEST_SIZE	0x20

static unsigned char serial2Data[0x20];

void serial2Init(void)
{
	serial2SetP71(1);
	memset(serial2Data, 0, sizeof(serial2Data));
}

unsigned int serialCommands2Count(void)
{
	return 9;
}

unsigned char serialNotifies2Count(void)
{
	return 1;
}

/* an input's rising edge: transition 0x52, when input3Bit0High allows it */
void serial2InputRequest(void)
{
	struct StateEvent event;

	if ((unsigned char)input3Bit0High()) {
		event.transition = 0x52;
		event.wakeMask = stateGetWakeMask();
		updStateEvent(&event);
	}
}

/* an input's edges: nothing in the updater */
void serial2InputLink(void)
{
}

/* 0x0200: nothing */
static unsigned char serial2Command0200(struct SerialBuffer *request, struct SerialBuffer *response)
{
	response->length = 0;
	serial4SetAnswer(SERIAL_ANSWER);
	return 0;
}

/* 0x0201: the hardware info, the firmware's version and the updater's */
static unsigned char serial2Command0201(struct SerialBuffer *request, struct SerialBuffer *response)
{
	getHardwareInfo(&serial2Data[0]);
	getErnieVersion(&serial2Data[4]);
	getErnieDlVersion(&serial2Data[8]);
	memset(&serial2Data[12], 0, 4);
	response->data = serial2Data;
	response->length = 0x10;
	serial4SetAnswer(SERIAL_ANSWER);
	return 0;
}

/* 0x0202: data[0] 0 or 1 picks the mode, data[1] the value */
static unsigned char serial2Command0202(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char mode;
	unsigned char value;
	unsigned char result;

	response->length = 0;
	if (request->length != 4) {
		serial4SetAnswer(SERIAL_ANSWER);
		return 0x32;
	}
	mode = request->data[0];
	value = request->data[1];
	if (mode == 0)
		serial4SetNextMode(2);
	else if (mode == 1)
		serial4SetNextMode(0x82);
	else {
		serial4SetAnswer(SERIAL_ANSWER);
		return 0x33;
	}
	result = serial4SelectBaud(value);
	if (result != 0) {
		serial4SetAnswer(SERIAL_ANSWER);
		return 0x33;
	}
	serial4SetAnswer(SERIAL_ANSWER);
	return 0;
}

static unsigned char serial2Notify0202(void)
{
	unsigned char channel;

	channel = serial4GetNextMode();
	serial4Start(channel);
	return serial4Reopen(serial4GetBaud());
}

static unsigned char serial2Command0203(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;

	result = 0;
	response->length = 0;
	result = serial4Unlock(2);
	serial4SetAnswer(SERIAL_ANSWER);
	return result;
}

static unsigned char serial2Command0204(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;

	result = 0;
	response->length = 0;
	result = serial4Lock(2);
	serial4SetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0205: data[0] 0 runs transition 0x52 from state 0x20, 2 transition 0x0e from state 4 or
   8 */
static unsigned char serial2Command0205(struct SerialBuffer *request, struct SerialBuffer *response)
{
	struct StateEvent event;
	unsigned char mode;
	unsigned char result;
	unsigned int state;

	result = 0;
	state = stateGet();
	mode = request->data[0];
	response->length = 0;
	if (mode == 0) {
		if (state != 0x0020) {
			serial4SetAnswer(SERIAL_ANSWER);
			return 0x50;
		}
		event.transition = 0x52;
		event.wakeMask = stateGetWakeMask();
		result = updStateEvent(&event);
		serial4SetAnswer(SERIAL_ANSWER_LATER);
	} else if (mode == 2) {
		if (state != 0x0004 && state != 0x0008) {
			serial4SetAnswer(SERIAL_ANSWER);
			return 0x50;
		}
		if (!chargeMayStart()) {
			serial4SetAnswer(SERIAL_ANSWER);
			return 0x51;
		}
		event.transition = 0x0e;
		event.wakeMask = stateGetWakeMask();
		result = updStateEvent(&event);
		serial4SetAnswer(SERIAL_ANSWER_LATER);
	} else {
		serial4SetAnswer(SERIAL_ANSWER);
		return 0x33;
	}
	return result;
}

/* 0x0206: the system state, with bit 14 set (the updater's) */
static unsigned char serial2Command0206(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned int state;

	response->length = 2;
	state = stateGet();
	state |= 0x4000;
	memcpy(serial2Data, &state, response->length);
	response->data = serial2Data;
	serial4SetAnswer(SERIAL_ANSWER);
	return 0;
}

/* 0x0211: writes the test area */
static unsigned char serial2Command0211(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;

	result = 0;
	response->length = 0;
	if (serial4IsLocked()) {
		serial4SetAnswer(SERIAL_ANSWER);
		return 0x20;
	}
	result = scratchPadWrite(SERIAL2_TEST_AREA, SERIAL2_TEST_SIZE, request->data);
	serial4SetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0210: reads the test area */
static unsigned char serial2Command0210(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;

	result = 0;
	if (serial4IsLocked()) {
		serial4SetAnswer(SERIAL_ANSWER);
		return 0x20;
	}
	response->data = serial2Data;
	response->length = SERIAL2_TEST_SIZE;
	result = scratchPadRead(SERIAL2_TEST_AREA, SERIAL2_TEST_SIZE, serial2Data);
	serial4SetAnswer(SERIAL_ANSWER);
	return result;
}

/* drives P7.1 */
unsigned char serial2SetP71(unsigned char level)
{
	P7.1 = level;
	return level & 1;
}

/* PLACEHOLDER: takes a level and does nothing; called with 0 by serialOpen1, 1 by serialOpen2 */
unsigned char serialSetLevel(unsigned char level)
{
	return 0;
}

const struct SerialCommand __far serialCommands2[] = {
	{ 0x0200, serial2Command0200 },
	{ 0x0201, serial2Command0201 },
	{ 0x0202, serial2Command0202 },
	{ 0x0203, serial2Command0203 },
	{ 0x0204, serial2Command0204 },
	{ 0x0205, serial2Command0205 },
	{ 0x0206, serial2Command0206 },
	{ 0x0210, serial2Command0210 },
	{ 0x0211, serial2Command0211 },
};

const struct SerialNotify __far serialNotifies2[] = {
	{ 0x0202, serial2Notify0202 },
};
