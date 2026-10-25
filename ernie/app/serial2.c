/*
 * The serial link's mode 2: the commands of serialCommands2 and the notifications of
 * serialNotifies2, answered from serial2Data or with no data, and the test area they keep in
 * the scratch pad (0xe0-0xff), which arms notification 0x0211 while it is empty. P7.1 is low
 * while the link runs in mode 2.
 */
#pragma sfr

#include <string.h>

#include "ernie.h"
#include "charge.h"
#include "config.h"
#include "kernel.h"
#include "rtc.h"
#include "scratchpad.h"
#include "serial.h"
#include "serial2.h"
#include "serial4.h"
#include "version.h"

#define SERIAL2_TEST_AREA	0xe0	/* the scratch pad's test area */
#define SERIAL2_TEST_SIZE	0x20

extern ER sub_2C2ED(unsigned char flgid, FLGPTN setptn);
extern unsigned int sub_2AB46(void);
extern unsigned char sub_2AB49(int transition);
extern unsigned char sub_11D4B(void);

static unsigned char serial2Data[0x20];
static unsigned char serial2Armed;	/* the test area was empty: notification 0x0211 may run */

/* level less trim, or 0 when that is 0x40 or more */
unsigned char levelTrim(unsigned char level, unsigned char trim)
{
	unsigned char result;

	result = level - trim;
	if (result >= 0x40)
		result = 0;
	return result;
}

void serial2Init(void)
{
	serial2SetP71(1);
	memset(serial2Data, 0, sizeof(serial2Data));
	serial2Disarm();
	return;
}

unsigned int serialCommands2Count(void)
{
	return 9;
}

unsigned char serialNotifies2Count(void)
{
	return 2;
}

/* an input's rising edge: transition 0x57 */
void serial2InputRequest(void)
{
	sub_2AB49(0x57);
	return;
}

/* an input's edges: event 0x0100 of flag 3, and P7.1 high unless sub_11D4B holds */
void serial2InputLink(void)
{
	sub_2C2ED(3, 0x0100);
	if (!sub_11D4B())
		serial2SetP71(1);
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

/* 0x0205: data[0] 0 runs transition 0x52 from state 0x20 or 8, 2 transition 0x0e from state
   4 or 8 */
static unsigned char serial2Command0205(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char mode;
	unsigned char result;
	unsigned int state;

	result = 0;
	state = sub_2AB46();
	mode = request->data[0];
	response->length = 0;
	if (mode == 0) {
		serial2Disarm();
		if (state != 0x0020 && state != 0x0008) {
			serial4SetAnswer(SERIAL_ANSWER);
			return 0x50;
		}
		result = sub_2AB49(0x52);
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
		if (chargeAlarm0On()) {
			serial4SetAnswer(SERIAL_ANSWER);
			return 0x53;
		}
		if (serial2TestAreaUsed() == 0 && !sub_2C658()->startCheckOff) {
			serial4SetAnswer(SERIAL_ANSWER);
			return 0x52;
		}
		result = sub_2AB49(0x0e);
		serial4SetAnswer(SERIAL_ANSWER_LATER);
	} else {
		serial4SetAnswer(SERIAL_ANSWER);
		return 0x33;
	}
	return result;
}

/* 0x0206: the system state */
static unsigned char serial2Command0206(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned int state;

	response->length = 2;
	state = sub_2AB46();
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

/* runs transition 0 once when armed and the config allows it */
static unsigned char serial2Notify0211(void)
{
	unsigned char result;

	result = 0;
	if (serial2GetArmed() != 0 && sub_2C658()->testDisarm) {
		serial2Disarm();
		result = sub_2AB49(0);
	}
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

/* 1 when a byte of the test area is not 0 */
int serial2TestAreaUsed(void)
{
	unsigned char area[SERIAL2_TEST_SIZE];
	unsigned char i;

	if (scratchPadRead(SERIAL2_TEST_AREA, SERIAL2_TEST_SIZE, area))
		return 0;
	for (i = 0; i < SERIAL2_TEST_SIZE; i++) {
		if (area[i] != 0)
			return 1;
	}
	return 0;
}

int serial2GetArmed(void)
{
	return serial2Armed;
}

unsigned char serial2Disarm(void)
{
	dis_dsp();
	serial2Armed = 0;
	ena_dsp();
	return 0;
}

/* arms notification 0x0211 when the test area is empty; 1 when it is not */
unsigned char serial2Arm(void)
{
	unsigned char result;

	dis_dsp();
	if (serial2TestAreaUsed() == 0) {
		serial2Armed = 1;
		result = 0;
	} else
		result = 1;
	ena_dsp();
	return result;
}

/* 1 while the clock's low byte has not passed the config's limit */
int serial2InTime(void)
{
	return getClock().byte[0] <= sub_2C658()->startClockLimit;
}

const struct SerialCommand serialCommands2[] = {
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

const struct SerialNotify serialNotifies2[] = {
	{ 0x0202, serial2Notify0202 },
	{ 0x0211, serial2Notify0211 },
};
