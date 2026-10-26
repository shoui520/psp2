/*
 * The touch panels: two maXTouch controllers on the CSI10 link, panel 0 (present when P0.6 is
 * high) and panel 1 (P0.5). A frame reads each enabled panel's report, and its status when
 * asked, into one of two frames that the host reads through streams 0x0300-0x0302
 * (app/csi10.c runs it). Commands read and write a controller's objects in between: they take
 * the link (TOUCH_BUSY) so that no frame starts meanwhile.
 */
#pragma sfr
#pragma DI
#pragma EI
#pragma NOP

#include <string.h>
#include "config.h"
#include "csi10.h"
#include "host.h"
#include "inputreq.h"
#include "input.h"
#include "kernel.h"
#include "touch.h"


extern unsigned char input1Bit5Active(void);
extern unsigned char input1Bit0Active(void);
extern unsigned char input1Bit3Active(void);

/* the frame after i */
#define TOUCH_NEXT_FRAME(i)	((i) + 1 < TOUCH_FRAMES ? (i) + 1 : 0)

/* an object's address in the controller */
#define TOUCH_OBJECT_ADDRESS(o)	((unsigned int)(o).addressHigh << 8 | (o).addressLow)

static unsigned char touchBufferData[TOUCH_BUFFER_SIZE];
struct TouchFrame touchFrames[TOUCH_FRAMES];
struct TouchPacket touchCommand;
struct TouchPacket touchReply;
struct Touch touch;
struct TouchBuffer touchBuffer;
struct TouchDelays touchDelays;

/* forgets both controllers */
void touchClearPanels(void)
{
	touch.panel[0].family = 0;
	touch.panel[0].variant = 0;
	touch.panel[0].version = 0;
	touch.panel[0].build = 0;
	touch.panel[0].configVersion = 0;
	memset(&touch.panel[0].objects, 0, sizeof(touch.panel[0].objects));
	memset(touch.panel[0].modes, 0, sizeof(touch.panel[0].modes));
	touch.panel[1].family = 0;
	touch.panel[1].variant = 0;
	touch.panel[1].version = 0;
	touch.panel[1].build = 0;
	touch.panel[1].configVersion = 0;
	memset(&touch.panel[1].objects, 0, sizeof(touch.panel[1].objects));
	memset(touch.panel[1].modes, 0, sizeof(touch.panel[1].modes));
	touch.state.value = 0;
}

/* opens the link, with no panel enabled */
void touchOpen(void)
{
	csi10Open();
	touchClearFrames0();
	touchClearFrames1();
	touch.flags = 0;
	touch.frame = 0;
	touch.setting[0] = 8;
	touch.setting[1] = 8;
	touch.rate = 0;
	memset(&touch.panel[0], 0, 4);
	memset(&touch.panel[1], 0, 4);
	touch.state.value = 0;
	touchBuffer.frame = 0;
	touchBuffer.end = 0;
	touchBuffer.data = touchBufferData;
	memset(&touchCommand, 0, sizeof(touchCommand));
	touchDelays.select = configGet()->touchDelaySelect;
	touchDelays.byte = configGet()->touchDelayByte;
	touchDelays.deselect = configGet()->touchDelayDeselect;
}

void touchClose(void)
{
	touchDisable0();
	touchDisable1();
	csi10Close();
}

/* panel 0's parts of both frames: no touch */
void touchClearFrames0(void)
{
	memset(touchFrames[0].panel[0], 0x80, TOUCH_REPORT_SIZE);
	memset(&touchFrames[0].panel[0][TOUCH_REPORT_SIZE], 0,
	       TOUCH_PANEL_SIZE - TOUCH_REPORT_SIZE);
	memset(touchFrames[1].panel[0], 0x80, TOUCH_REPORT_SIZE);
	memset(&touchFrames[1].panel[0][TOUCH_REPORT_SIZE], 0,
	       TOUCH_PANEL_SIZE - TOUCH_REPORT_SIZE);
	touchFrames[0].time0 = 0;
	touchFrames[1].time0 = 0;
}

void touchClearFrames1(void)
{
	memset(touchFrames[0].panel[1], 0x80, TOUCH_REPORT_SIZE);
	memset(&touchFrames[0].panel[1][TOUCH_REPORT_SIZE], 0,
	       TOUCH_PANEL_SIZE - TOUCH_REPORT_SIZE);
	memset(touchFrames[1].panel[1], 0x80, TOUCH_REPORT_SIZE);
	memset(&touchFrames[1].panel[1][TOUCH_REPORT_SIZE], 0,
	       TOUCH_PANEL_SIZE - TOUCH_REPORT_SIZE);
	touchFrames[0].time1 = 0;
	touchFrames[1].time1 = 0;
}

/* starts a frame, into a frame no stream is sending: panel 1's report first when it is
   enabled; INTCSI10 goes on from there */
void touchStartFrame(void)
{
	unsigned char next;
	struct Csi10Transfer transfer;

	if (csi10GetState() == CSI10_OPEN && !(touch.flags & TOUCH_BUSY)) {
		if (touch.flags & TOUCH_PANEL1) {
			P0.0 = 0;
			transfer.state = CSI10_COMMAND1;
			touchCommand.address = touch.panel[1].objects.report | TOUCH_READ;
		} else if (touch.flags & TOUCH_PANEL0) {
			P0.1 = 0;
			transfer.state = CSI10_COMMAND0;
			touchCommand.address = touch.panel[0].objects.report | TOUCH_READ;
		}
		next = TOUCH_NEXT_FRAME(hostStream0300.first);
		if (next == hostStream0300.block || next == hostStream0301.block ||
		    next == hostStream0302.block)
			next = TOUCH_NEXT_FRAME(next);
		touch.frame = next;
		if (touch.flags & TOUCH_PANEL1) {
			touchFrames[touch.frame].time1 = TCR02;
			touchFrames[touch.frame].panel[1][TOUCH_PANEL_STATE] = touch.state.value;
		} else if (touch.flags & TOUCH_PANEL0) {
			touchFrames[touch.frame].time0 = TCR02;
			touchFrames[touch.frame].panel[0][TOUCH_PANEL_STATE] = touch.state.value;
		}
		touchDelaySelect();
		transfer.receive = (unsigned char *)&touchReply;
		transfer.receiveCount = 0;
		transfer.send = (unsigned char *)&touchCommand;
		transfer.sendCount = 2;
		csi10Transfer(&transfer);
	}
}

/* touchStartFrame, for a caller that has disabled the interrupts */
void touchStartFrameNoLock(void)
{
	unsigned char next;
	struct Csi10Transfer transfer;

	if (csi10GetState() == CSI10_OPEN && !(touch.flags & TOUCH_BUSY)) {
		if (touch.flags & TOUCH_PANEL1) {
			P0.0 = 0;
			transfer.state = CSI10_COMMAND1;
			touchCommand.address = touch.panel[1].objects.report | TOUCH_READ;
		} else if (touch.flags & TOUCH_PANEL0) {
			P0.1 = 0;
			transfer.state = CSI10_COMMAND0;
			touchCommand.address = touch.panel[0].objects.report | TOUCH_READ;
		}
		next = TOUCH_NEXT_FRAME(hostStream0300.first);
		if (next == hostStream0300.block || next == hostStream0301.block ||
		    next == hostStream0302.block)
			next = TOUCH_NEXT_FRAME(next);
		touch.frame = next;
		if (touch.flags & TOUCH_PANEL1) {
			touchFrames[touch.frame].time1 = TCR02;
			touchFrames[touch.frame].panel[1][TOUCH_PANEL_STATE] = touch.state.value;
		} else if (touch.flags & TOUCH_PANEL0) {
			touchFrames[touch.frame].time0 = TCR02;
			touchFrames[touch.frame].panel[0][TOUCH_PANEL_STATE] = touch.state.value;
		}
		touchDelaySelect();
		transfer.receive = (unsigned char *)&touchReply;
		transfer.receiveCount = 0;
		transfer.send = (unsigned char *)&touchCommand;
		transfer.sendCount = 2;
		csi10TransferNoLock(&transfer);
	}
}

/* enables panel 0 when it is present (inputKeys.state's bit 0x0004) */
void touchEnable0(void)
{
	if (P0.6 && !(inputKeys.state & 0x0004)) {
		inputKeys.state |= 0x0004;
		touchSetFlags(TOUCH_PANEL0);
	}
}

/* clears inputKeys.state's high byte too */
void touchDisable0(void)
{
	inputKeys.state &= 0x00fb;
	touchClearFlags(TOUCH_PANEL0);
	touchClearFrames0();
}

void touchEnable1(void)
{
	if (P0.5 && !(inputKeys.state & 0x0008)) {
		inputKeys.state |= 0x0008;
		touchSetFlags(TOUCH_PANEL1);
	}
}

void touchDisable1(void)
{
	inputKeys.state &= 0x00f7;
	touchClearFlags(TOUCH_PANEL1);
	touchClearFrames1();
}

/* writes length bytes at a controller's address, on the link the caller has taken: 0, 1 for
   no such panel, 2 if the transfer does not end */
unsigned char touchSpiWrite(unsigned int address, unsigned char *data, unsigned int length,
			    unsigned char panel)
{
	struct Csi10Transfer transfer;
	unsigned char retry;
	unsigned char i;

	touchCommand.address = address * 2;
	memcpy(touchCommand.data, data, length);
	if (panel == 0)
		P0.1 = 0;
	else if (panel == 1)
		P0.0 = 0;
	else
		return 1;
	touchDelaySelect();
	transfer.receive = (unsigned char *)&touchReply;
	transfer.receiveCount = 0;
	transfer.send = (unsigned char *)&touchCommand;
	transfer.sendCount = length + 2;
	transfer.state = CSI10_TRANSFER;
	csi10Transfer(&transfer);
	retry = 0xff;
	do {
		if (csi10GetState() == CSI10_DONE) {
			for (i = 0; i < touchDelays.deselect; i++)
				NOP();
			if (panel == 0)
				P0.1 = 1;
			else if (panel == 1)
				P0.0 = 1;
			return 0;
		}
	} while (retry-- != 0);
	return 2;
}

/* reads length bytes at a controller's address, as touchSpiWrite */
unsigned char touchSpiRead(unsigned int address, unsigned char *data, unsigned int length,
			   unsigned char panel)
{
	struct Csi10Transfer transfer;
	unsigned char retry;
	unsigned char i;

	touchCommand.address = address * 2 | TOUCH_READ;
	if (panel == 0)
		P0.1 = 0;
	else if (panel == 1)
		P0.0 = 0;
	else
		return 1;
	touchDelaySelect();
	transfer.receive = (unsigned char *)&touchReply;
	transfer.receiveCount = length + 2;
	transfer.send = (unsigned char *)&touchCommand;
	transfer.sendCount = 2;
	transfer.state = CSI10_TRANSFER;
	csi10Transfer(&transfer);
	retry = 0xff;
	do {
		if (csi10GetState() == CSI10_DONE) {
			for (i = 0; i < touchDelays.deselect; i++)
				NOP();
			if (panel == 0)
				P0.1 = 1;
			else if (panel == 1)
				P0.0 = 1;
			memcpy(data, touchReply.data, length);
			return 0;
		}
	} while (retry-- != 0);
	return 2;
}

/* writes, and reads back to compare, as the settings block says: 0, 2 if the link stays busy,
   3 if no try compared */
unsigned char touchWriteVerified(unsigned int address, unsigned char *data,
				 unsigned int length, unsigned char panel)
{
	unsigned char wait;
	unsigned char result;
	unsigned char tries;
	unsigned char matches;
	unsigned char reads;

	touchSetFlags(TOUCH_BUSY);
	wait = 10;
	while (csi10GetState() != CSI10_OPEN) {
		if (wait-- == 0) {
			result = 2;
			goto unlock;
		}
		dly_tsk(1);
	}
	for (tries = configGet()->touchWriteTries; tries != 0; tries--) {
		if (touchSpiWrite(address, data, length, panel) != 0)
			continue;
		reads = configGet()->touchWriteReads;
		matches = 0;
		for (; reads != 0; reads--) {
			if (touchSpiRead(address, (unsigned char *)&touchReply, length,
					 panel) == 0 &&
			    memcmp(data, &touchReply, length) == 0)
				matches++;
		}
		if (matches == configGet()->touchWriteReads) {
			result = 0;
			goto release;
		}
	}
	result = 3;
release:
	csi10Release();
unlock:
	touchClearFlags(TOUCH_BUSY);
	return result;
}

/* takes the link and writes: 0, 2 if the link stays busy */
unsigned char touchWrite(unsigned int address, unsigned char *data, unsigned int length,
			 unsigned char panel)
{
	unsigned char wait;
	unsigned char result;

	touchSetFlags(TOUCH_BUSY);
	wait = 10;
	while (csi10GetState() != CSI10_OPEN) {
		if (wait-- == 0) {
			result = 2;
			goto unlock;
		}
		dly_tsk(1);
	}
	result = touchSpiWrite(address, data, length, panel);
	csi10Release();
unlock:
	touchClearFlags(TOUCH_BUSY);
	return result;
}

unsigned char touchRead(unsigned int address, unsigned char *data, unsigned int length,
			unsigned char panel)
{
	unsigned char wait;
	unsigned char result;

	touchSetFlags(TOUCH_BUSY);
	wait = 10;
	while (csi10GetState() != CSI10_OPEN) {
		if (wait-- == 0) {
			result = 2;
			goto unlock;
		}
		dly_tsk(1);
	}
	result = touchSpiRead(address, data, length, panel);
	csi10Release();
unlock:
	touchClearFlags(TOUCH_BUSY);
	return result;
}

/* reads panel 0's information block, and its object table when its firmware changed */
void touchIdentify0(void)
{
	int result;
	unsigned char info[TOUCH_INFO_SIZE];

	if (P0.6) {
		result = touchRead(0x0000, info, sizeof(info), 0);
		if (result != 0)
			memset(&touch.panel[0], 0xff, 4);
		else if (info[TOUCH_INFO_VERSION] != touch.panel[0].version ||
			 info[TOUCH_INFO_BUILD] != touch.panel[0].build) {
			touchReadObjects(&touch.panel[0].objects, info[TOUCH_INFO_OBJECTS], 0);
			memcpy(&touch.panel[0], info, 4);
		}
	} else
		memset(&touch.panel[0], 0xff, 4);
}

void touchIdentify1(void)
{
	int result;
	unsigned char info[TOUCH_INFO_SIZE];

	if (P0.5) {
		result = touchRead(0x0000, info, sizeof(info), 1);
		if (result != 0)
			memset(&touch.panel[1], 0xff, 4);
		else if (info[TOUCH_INFO_VERSION] != touch.panel[1].version ||
			 info[TOUCH_INFO_BUILD] != touch.panel[1].build) {
			touchReadObjects(&touch.panel[1].objects, info[TOUCH_INFO_OBJECTS], 1);
			memcpy(&touch.panel[1], info, 4);
		}
	} else
		memset(&touch.panel[1], 0xff, 4);
}

unsigned char touchBootloaderStatus0(unsigned char *status)
{
	return touchRead(TOUCH_BOOTLOADER, status, 1, 0);
}

unsigned char touchBootloaderStatus1(unsigned char *status)
{
	return touchRead(TOUCH_BOOTLOADER, status, 1, 1);
}

/* resets the controller into its bootloader */
unsigned char touchEnterBootloader0(void)
{
	unsigned char command;

	command = 0xa5;
	return touchWrite(touch.panel[0].objects.reset, &command, 1, 0);
}

unsigned char touchEnterBootloader1(void)
{
	unsigned char command;

	command = 0xa5;
	return touchWrite(touch.panel[1].objects.reset, &command, 1, 1);
}

unsigned char touchBootloaderUnlock0(void)
{
	unsigned char command[2];

	command[0] = 0xdc;
	command[1] = 0xaa;
	return touchWrite(TOUCH_BOOTLOADER, command, 2, 0);
}

unsigned char touchBootloaderUnlock1(void)
{
	unsigned char command[2];

	command[0] = 0xdc;
	command[1] = 0xaa;
	return touchWrite(TOUCH_BOOTLOADER, command, 2, 1);
}

/* saves the controller's objects to its non-volatile memory */
unsigned char touchBackup0(void)
{
	unsigned char command;

	command = 0x55;
	return touchWrite(touch.panel[0].objects.backup, &command, 1, 0);
}

/* BACKUPNV as read (0xff: no answer) */
unsigned char touchBackupState0(void)
{
	unsigned char value;
	unsigned char result;

	result = touchRead(touch.panel[0].objects.backup, &value, 1, 0);
	if (result != 0)
		value = 0xff;
	return value;
}

unsigned char touchBackup1(void)
{
	unsigned char command;

	command = 0x55;
	return touchWrite(touch.panel[1].objects.backup, &command, 1, 1);
}

unsigned char touchBackupState1(void)
{
	unsigned char value;
	unsigned char result;

	result = touchRead(touch.panel[1].objects.backup, &value, 1, 1);
	if (result != 0)
		value = 0xff;
	return value;
}

unsigned char touchReset0(void)
{
	unsigned char command;

	command = 0xff;
	return touchWrite(touch.panel[0].objects.reset, &command, 1, 0);
}

unsigned char touchReset1(void)
{
	unsigned char command;

	command = 0xff;
	return touchWrite(touch.panel[1].objects.reset, &command, 1, 1);
}

/* the acquisition interval, idle and active */
unsigned char touchSetPower0(unsigned char interval)
{
	unsigned char data[2];

	data[0] = interval;
	data[1] = interval;
	return touchWriteVerified(touch.panel[0].objects.power, data, 2, 0);
}

unsigned char touchSetPower1(unsigned char interval)
{
	unsigned char data[2];

	data[0] = interval;
	data[1] = interval;
	return touchWriteVerified(touch.panel[1].objects.power, data, 2, 1);
}

/* reads the user data's configuration version into data (2 bytes), and the modes when it
   changed */
unsigned char touchReadConfigVersion0(unsigned char *data)
{
	unsigned char result;
	unsigned int version;

	if (touch.panel[0].version < 0x24) {
		memset(data, 0, 2);
		return 0;
	}
	result = touchRead(touch.panel[0].objects.userData, data, 2, 0);
	version = (unsigned int)data[0] << 8 | data[1];
	if (touch.panel[0].configVersion != version) {
		touch.panel[0].configVersion = version;
		touchReadModes0();
	}
	return result;
}

unsigned char touchReadConfigVersion1(unsigned char *data)
{
	unsigned char result;
	unsigned int version;

	if (touch.panel[1].version < 0x24) {
		memset(data, 0, 2);
		return 0;
	}
	result = touchRead(touch.panel[1].objects.userData, data, 2, 1);
	version = (unsigned int)data[0] << 8 | data[1];
	if (touch.panel[1].configVersion != version) {
		touch.panel[1].configVersion = version;
		touchReadModes1();
	}
	return result;
}

/* the user data's bytes 12-19 */
unsigned char touchReadUserData0(unsigned char *data)
{
	if (touch.panel[0].version < 0x24) {
		memset(data, 0xff, 8);
		return 0;
	}
	return touchRead(touch.panel[0].objects.userData + 0x0c, data, 8, 0);
}

unsigned char touchReadUserData1(unsigned char *data)
{
	if (touch.panel[1].version < 0x24) {
		memset(data, 0xff, 8);
		return 0;
	}
	return touchRead(touch.panel[1].objects.userData + 0x0c, data, 8, 1);
}

/* runs a self test (T25); *pass 0 if it passed, 0xff if not, 0xfe if the firmware has none */
unsigned char touchSelfTest0(unsigned char test, unsigned char *pass)
{
	unsigned char command[2];
	unsigned char value;
	unsigned char result;
	unsigned int address;

	if (touch.panel[0].version < 0x21) {
		*pass = 0xfe;
		return 0;
	}
	command[0] = 3;
	command[1] = test;
	touchWrite(touch.panel[0].objects.selfTest, command, 2, 0);
	dly_tsk(8);
	address = touch.panel[0].objects.t34;
	if (touch.panel[0].version >= 0x24)
		address += 0x62;
	else if (touch.panel[0].version == 0x23)
		address += 0x5b;
	else if (touch.panel[0].version == 0x22)
		address += 0x5b;
	else
		address += 0x85;
	result = touchRead(address, &value, 1, 0);
	if (value == 0xfe)
		*pass = 0;
	else
		*pass = 0xff;
	return result;
}

unsigned char touchSelfTest1(unsigned char test, unsigned char *pass)
{
	unsigned char command[2];
	unsigned char value;
	unsigned char result;
	unsigned int address;

	if (touch.panel[1].version < 0x21) {
		*pass = 0xfe;
		return 0;
	}
	command[0] = 3;
	command[1] = test;
	touchWrite(touch.panel[1].objects.selfTest, command, 2, 1);
	dly_tsk(8);
	address = touch.panel[1].objects.t34;
	if (touch.panel[1].version >= 0x24)
		address += 0x62;
	else if (touch.panel[1].version == 0x23)
		address += 0x5b;
	else if (touch.panel[1].version == 0x22)
		address += 0x5b;
	else
		address += 0x85;
	result = touchRead(address, &value, 1, 1);
	if (value == 0xfe)
		*pass = 0;
	else
		*pass = 0xff;
	return result;
}

/* after a chip select */
void touchDelaySelect(void)
{
	unsigned char i;

	for (i = 0; i < touchDelays.select; i++)
		NOP();
}

/* reads a controller's object table: the addresses of the objects the module uses */
void touchReadObjects(struct TouchObjects *objects, unsigned char count, unsigned char panel)
{
	int result;
	unsigned int address;
	unsigned char i;
	struct TouchObject *table;

	address = TOUCH_INFO_SIZE;
	table = (struct TouchObject *)touchBufferData;
	result = touchRead(address, (unsigned char *)table, count * 6, panel);
	if (result == 0) {
		for (i = 0; i < count; i++) {
			switch (table[i].type) {
			case 6:
				objects->reset = TOUCH_OBJECT_ADDRESS(table[i]);
				objects->backup = TOUCH_OBJECT_ADDRESS(table[i]) + 1;
				break;
			case 7:
				objects->power = TOUCH_OBJECT_ADDRESS(table[i]);
				break;
			case 9:
				objects->numTouch = TOUCH_OBJECT_ADDRESS(table[i]) + 0x0e;
				objects->touchScreen = TOUCH_OBJECT_ADDRESS(table[i]);
				break;
			case 20:
				objects->grip = TOUCH_OBJECT_ADDRESS(table[i]);
				break;
			case 22:
				objects->noise = TOUCH_OBJECT_ADDRESS(table[i]);
				break;
			case 25:
				objects->selfTest = TOUCH_OBJECT_ADDRESS(table[i]);
				break;
			case 28:
				objects->cte = TOUCH_OBJECT_ADDRESS(table[i]);
				break;
			case 34:
				objects->report = TOUCH_OBJECT_ADDRESS(table[i]) + 0x0e;
				objects->status = TOUCH_OBJECT_ADDRESS(table[i]) + 0x54;
				objects->report *= 2;
				objects->status *= 2;
				objects->t34 = TOUCH_OBJECT_ADDRESS(table[i]);
				break;
			case 38:
				objects->userData = TOUCH_OBJECT_ADDRESS(table[i]);
				break;
			case 51:
				objects->t51 = TOUCH_OBJECT_ADDRESS(table[i]);
				break;
			}
		}
	}
}

/* writes mode's settings to both controllers */
void touchSetMode(unsigned char mode)
{
	unsigned int address;

	if (touch.panel[0].version >= 0x24) {
		address = touch.panel[0].objects.t51 + 3;
		touchWriteVerified(address, &touch.panel[0].modes[mode].t51, 1, 0);
		address = touch.panel[0].objects.touchScreen + 7;
		touchWriteVerified(address, &touch.panel[0].modes[mode].threshold, 1, 0);
		address = touch.panel[0].objects.touchScreen + 0x1f;
		touchWriteVerified(address, &touch.panel[0].modes[mode].hysteresis, 1, 0);
		address = touch.panel[0].objects.touchScreen + 0x10;
		touchWriteVerified(address, &touch.panel[0].modes[mode].mergeThreshold, 1, 0);
		address = touch.panel[0].objects.noise + 8;
		touchWriteVerified(address, &touch.panel[0].modes[mode].noiseThreshold, 1, 0);
		if ((touch.panel[0].configVersion & 0x7fff) >= 0x0d00) {
			address = touch.panel[0].objects.noise + 0x0c;
			touchWriteVerified(address, touch.panel[0].modes[mode].frequency, 4, 0);
		}
		if ((touch.panel[0].configVersion & 0x7fff) >= 0x0d01) {
			address = touch.panel[0].objects.cte + 4;
			touchWriteVerified(address, &touch.panel[0].modes[mode].gcafDepth, 1, 0);
		}
	}
	if (touch.panel[1].version >= 0x24) {
		address = touch.panel[1].objects.t51 + 3;
		touchWriteVerified(address, &touch.panel[1].modes[mode].t51, 1, 1);
		address = touch.panel[1].objects.touchScreen + 7;
		touchWriteVerified(address, &touch.panel[1].modes[mode].threshold, 1, 1);
		address = touch.panel[1].objects.touchScreen + 0x1f;
		touchWriteVerified(address, &touch.panel[1].modes[mode].hysteresis, 1, 1);
		address = touch.panel[1].objects.touchScreen + 0x10;
		touchWriteVerified(address, &touch.panel[1].modes[mode].mergeThreshold, 1, 1);
		address = touch.panel[1].objects.noise + 8;
		touchWriteVerified(address, &touch.panel[1].modes[mode].noiseThreshold, 1, 1);
		if ((touch.panel[1].configVersion & 0x7fff) >= 0x0d00) {
			address = touch.panel[1].objects.noise + 0x0c;
			touchWriteVerified(address, touch.panel[1].modes[mode].frequency, 4, 1);
		}
		if ((touch.panel[1].configVersion & 0x7fff) >= 0x0d01) {
			address = touch.panel[1].objects.cte + 4;
			touchWriteVerified(address, &touch.panel[1].modes[mode].gcafDepth, 1, 1);
		}
	}
}

/* modes 1 and 2 from panel 0's user data, mode 0 from its objects */
void touchReadModes0(void)
{
	int result;
	unsigned int address;
	unsigned char data[sizeof(struct TouchMode)];

	if (touch.panel[0].version >= 0x24) {
		address = touch.panel[0].objects.userData + 2;
		result = touchRead(address, data, sizeof(data), 0);
		if (result == 0) {
			memcpy(&touch.panel[0].modes[2], data, sizeof(data));
			memcpy(&touch.panel[0].modes[1], data, sizeof(data) - 1);
		}
		address = touch.panel[0].objects.t51 + 3;
		result = touchRead(address, data, 1, 0);
		if (result == 0)
			touch.panel[0].modes[0].t51 = data[0];
		address = touch.panel[0].objects.touchScreen + 7;
		result = touchRead(address, data, 1, 0);
		if (result == 0)
			touch.panel[0].modes[0].threshold = data[0];
		address = touch.panel[0].objects.touchScreen + 0x1f;
		result = touchRead(address, data, 1, 0);
		if (result == 0)
			touch.panel[0].modes[0].hysteresis = data[0];
		address = touch.panel[0].objects.touchScreen + 0x10;
		result = touchRead(address, data, 1, 0);
		if (result == 0)
			touch.panel[0].modes[0].mergeThreshold = data[0];
		address = touch.panel[0].objects.noise + 8;
		result = touchRead(address, data, 1, 0);
		if (result == 0)
			touch.panel[0].modes[0].noiseThreshold = data[0];
		address = touch.panel[0].objects.noise + 0x0c;
		result = touchRead(address, data, 4, 0);
		if (result == 0)
			memcpy(touch.panel[0].modes[0].frequency, data, 4);
		address = touch.panel[0].objects.cte + 4;
		result = touchRead(address, data, 1, 0);
		if (result == 0) {
			touch.panel[0].modes[0].gcafDepth = data[0];
			touch.panel[0].modes[1].gcafDepth = data[0];
		}
	}
}

void touchReadModes1(void)
{
	int result;
	unsigned int address;
	unsigned char data[sizeof(struct TouchMode)];

	if (touch.panel[1].version >= 0x24) {
		address = touch.panel[1].objects.userData + 2;
		result = touchRead(address, data, sizeof(data), 1);
		if (result == 0) {
			memcpy(&touch.panel[1].modes[2], data, sizeof(data));
			memcpy(&touch.panel[1].modes[1], data, sizeof(data) - 1);
		}
		address = touch.panel[1].objects.t51 + 3;
		result = touchRead(address, data, 1, 1);
		if (result == 0)
			touch.panel[1].modes[0].t51 = data[0];
		address = touch.panel[1].objects.touchScreen + 7;
		result = touchRead(address, data, 1, 1);
		if (result == 0)
			touch.panel[1].modes[0].threshold = data[0];
		address = touch.panel[1].objects.touchScreen + 0x1f;
		result = touchRead(address, data, 1, 1);
		if (result == 0)
			touch.panel[1].modes[0].hysteresis = data[0];
		address = touch.panel[1].objects.touchScreen + 0x10;
		result = touchRead(address, data, 1, 1);
		if (result == 0)
			touch.panel[1].modes[0].mergeThreshold = data[0];
		address = touch.panel[1].objects.noise + 8;
		result = touchRead(address, data, 1, 1);
		if (result == 0)
			touch.panel[1].modes[0].noiseThreshold = data[0];
		address = touch.panel[1].objects.noise + 0x0c;
		result = touchRead(address, data, 4, 1);
		if (result == 0)
			memcpy(touch.panel[1].modes[0].frequency, data, 4);
		address = touch.panel[1].objects.cte + 4;
		result = touchRead(address, data, 1, 1);
		if (result == 0) {
			touch.panel[1].modes[0].gcafDepth = data[0];
			touch.panel[1].modes[1].gcafDepth = data[0];
		}
	}
}

/* the state the frames carry; mode 1 from now on */
void touchUpdateState(void)
{
	unsigned char mode;

	if (touch.flags & (TOUCH_PANEL0 | TOUCH_PANEL1)) {
		touch.state.bits.valid = 1;
		touch.state.bits.input3 = input1Bit3Active();
		touch.state.bits.input5 = input1Bit5Active();
		touch.state.bits.input0 = input1Bit0Active();
		mode = 1;
		if (touch.state.bits.mode != mode) {
			touchSetMode(mode);
			touch.state.bits.mode = mode;
		}
	}
}

/* enables panel 0's grip status (T20), which each frame then reads */
unsigned char touchStatusOn0(void)
{
	unsigned char enable;
	unsigned char result;

	if (touch.panel[0].version < 0x24)
		return 0xfe;
	if (!(touch.flags & TOUCH_STATUS0)) {
		enable = 7;
		result = touchWriteVerified(touch.panel[0].objects.grip, &enable, 1, 0);
		touchSetFlags(TOUCH_STATUS0);
	}
	return result;
}

unsigned char touchStatusOff0(void)
{
	unsigned char enable;
	unsigned char result;

	touchClearFlags(TOUCH_STATUS0);
	if (touch.panel[0].version < 0x24)
		return 0xfe;
	enable = 0;
	result = touchWriteVerified(touch.panel[0].objects.grip, &enable, 1, 0);
	touchFrames[0].panel[0][TOUCH_PANEL_STATUS] = 0;
	touchFrames[1].panel[0][TOUCH_PANEL_STATUS] = 0;
	return result;
}

unsigned char touchStatusOn1(void)
{
	unsigned char enable;
	unsigned char result;

	if (touch.panel[1].version < 0x24)
		return 0xfe;
	if (!(touch.flags & TOUCH_STATUS1)) {
		enable = 7;
		result = touchWriteVerified(touch.panel[1].objects.grip, &enable, 1, 1);
		touchSetFlags(TOUCH_STATUS1);
	}
	return result;
}

unsigned char touchStatusOff1(void)
{
	unsigned char enable;
	unsigned char result;

	touchClearFlags(TOUCH_STATUS1);
	if (touch.panel[1].version < 0x24)
		return 0xfe;
	enable = 0;
	result = touchWriteVerified(touch.panel[1].objects.grip, &enable, 1, 1);
	touchFrames[0].panel[1][TOUCH_PANEL_STATUS] = 0;
	touchFrames[1].panel[1][TOUCH_PANEL_STATUS] = 0;
	return result;
}

/* T20's bytes 7-11 */
unsigned char touchSetGrip0(unsigned char *data)
{
	unsigned char result;

	if (touch.panel[0].version < 0x24)
		return 0xfe;
	result = touchWriteVerified(touch.panel[0].objects.grip + 7, data, 5, 0);
	return result;
}

unsigned char touchSetGrip1(unsigned char *data)
{
	unsigned char result;

	if (touch.panel[1].version < 0x24)
		return 0xfe;
	result = touchWriteVerified(touch.panel[1].objects.grip + 7, data, 5, 1);
	return result;
}

void touchSetFlags(unsigned char flags)
{
	DI();
	touch.flags |= flags;
	EI();
}

void touchClearFlags(unsigned char flags)
{
	DI();
	touch.flags &= ~flags;
	EI();
}
