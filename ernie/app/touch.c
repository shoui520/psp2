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
#include "board.h"
#include "charge.h"
#include "config.h"
#include "csi10.h"
#include "host.h"
#if SOURCE_0103
#include "iica1.h"
#endif
#include "inputreq.h"
#include "input.h"
#include "kernel.h"
#include "touch.h"


extern unsigned char input1Bit5Active(void);
extern unsigned char input1Bit0Active(void);
extern unsigned char input1Bit3Active(void);
extern unsigned char input1Bit7Active(void);

/* the link is taken by a frame: USS-1001's frame runs from touchStartFrame to its last
   transfer's interrupt */
#if SOURCE_0103
#define TOUCH_LINK_BUSY()	(touch.flags & TOUCH_RUNNING)
#else
#define TOUCH_LINK_BUSY()	(csi10GetState() != CSI10_OPEN)
#endif

/* the frame after i */
#define TOUCH_NEXT_FRAME(i)	((i) + 1 < TOUCH_FRAMES ? (i) + 1 : 0)

/* an object's address in the controller */
#define TOUCH_OBJECT_ADDRESS(o)	((unsigned int)(o).addressHigh << 8 | (o).addressLow)

static unsigned char touchBufferData[TOUCH_BUFFER_SIZE];
#if SOURCE_0103
unsigned char touchRearStatus;
#endif
struct TouchFrame touchFrames[TOUCH_FRAMES];
struct TouchPacket touchCommand;
struct TouchPacket touchReply;
struct Touch touch;
struct TouchBuffer touchBuffer;
#if FW_SINCE(0x01000306)
struct TouchDelays touchDelays;
#endif
#if TOUCH_0105
/* the 8 bytes a memory write carries after its data, under its CRC */
const unsigned char touchRearWriteKey[8] = { 0xa5, 0x01, 0x02, 0x03, 0xff, 0xfe, 0xfd, 0x5a };
const struct TouchRearController touchRearControllers[4] = {
	{ 0, 1, 0x96, 0 },
	{ 1, 0, 0x48, 0 },
	{ 2, 1, 0xc8, 0 },
	{ 3, 0, 0xc8, 0 },
};
#endif

#if SOURCE_0104
/* forgets both controllers */
void touchClearPanels(void)
{
	TOUCH_PANEL_ID(0) = 0;
	touch.panel[0].version = 0;
	touch.panel[0].build = 0;
	touch.panel[0].configVersion = 0;
	touch.panel[0].gripEdges[0] = 0;
	touch.panel[0].gripEdges[1] = 0;
	touch.panel[0].objectsUnread = 0;
	touch.panel[0].modesUnread = 0;
	touch.panel[0].features.value = 0;
	memset(&touch.panel[0].objects, 0, sizeof(touch.panel[0].objects));
	memset(touch.panel[0].modes, 0, sizeof(touch.panel[0].modes));
	TOUCH_PANEL_ID(1) = 0;
	touch.panel[1].version = 0;
	touch.panel[1].build = 0;
	touch.panel[1].configVersion = 0;
	touch.panel[1].gripEdges[0] = 0;
	touch.panel[1].gripEdges[1] = 0;
	touch.panel[1].objectsUnread = 0;
	touch.panel[1].modesUnread = 0;
	touch.panel[1].features.value = 0;
	memset(&touch.panel[1].objects, 0, sizeof(touch.panel[1].objects));
	memset(touch.panel[1].modes, 0, sizeof(touch.panel[1].modes));
	touch.state.value = 0;
}

/* opens a link (0: CSI10), with no panel enabled */
void touchOpen(unsigned char link)
{
	if (link == 0) {
		csi10Open();
#if TOUCH_0105
		iica1Init();
#endif
	} else if (link == 1)
		;
	touchClearFrames(0);
	touchClearFrames(1);
	touch.flags = 0;
	touch.frame = 0;
	touch.setting[0] = 8;
	touch.setting[1] = 8;
	touch.rate = 0;
#if TOUCH_0105
	touch.rearShort = 0;
	touch.panel[0].controller = 0xff;
	touch.panel[1].controller = 0xff;
	touch.panel[0].controllerStatus = 0;
	touch.panel[1].controllerStatus = 0;
	touch.panel[0].controllerActive = 0;
	touch.panel[0].controllerWasActive = 0;
	touch.panel[1].controllerActive = 0;
	touch.panel[1].controllerWasActive = 0;
#endif
	touchClearMode();
	touchBuffer.frame = 0;
	touchBuffer.end = 0;
	touchBuffer.data = touchBufferData;
	memset(&touchCommand, 0, sizeof(touchCommand));
	touchDelays.select = configGet()->touchDelaySelect;
	touchDelays.byte = configGet()->touchDelayReadByte;
	touchDelays.writeByte = configGet()->touchDelayWriteByte;
	touchDelays.deselect = configGet()->touchDelayDeselect;
	touchDelays.release = configGet()->touchDelayRelease;
}

void touchClose(unsigned char link)
{
	touchDisable(0);
	touchDisable(1);
#if TOUCH_0105
	TOUCH_MODE_PIN = 0;
	if (link == 0) {
		csi10Close();
		iica1Close();
	} else if (link == 1)
		;
#else
	if (link == 0)
		csi10Close();
	else if (link == 1)
		;
#endif
}

/* a panel's parts of both frames: no touch */
void touchClearFrames(char panel)
{
	switch (panel) {
	case 0:
		memset(touchFrames[0].panel[0], 0x80, TOUCH_REPORT_SIZE);
		memset(&touchFrames[0].panel[0][TOUCH_REPORT_SIZE], 0,
		       TOUCH_PANEL_SIZE - TOUCH_REPORT_SIZE);
		memset(touchFrames[1].panel[0], 0x80, TOUCH_REPORT_SIZE);
		memset(&touchFrames[1].panel[0][TOUCH_REPORT_SIZE], 0,
		       TOUCH_PANEL_SIZE - TOUCH_REPORT_SIZE);
		touchFrames[0].time0 = 0;
		touchFrames[1].time0 = 0;
		break;
	case 1:
		memset(touchFrames[0].panel[1], 0x80, TOUCH_REPORT_SIZE);
		memset(&touchFrames[0].panel[1][TOUCH_REPORT_SIZE], 0,
		       TOUCH_PANEL_SIZE - TOUCH_REPORT_SIZE);
		memset(touchFrames[1].panel[1], 0x80, TOUCH_REPORT_SIZE);
		memset(&touchFrames[1].panel[1][TOUCH_REPORT_SIZE], 0,
		       TOUCH_PANEL_SIZE - TOUCH_REPORT_SIZE);
		touchFrames[0].time1 = 0;
		touchFrames[1].time1 = 0;
		break;
	}
}
#elif FW_SINCE(0x00090009)	/* 0x00080504 has none */
/* forgets both controllers */
void touchClearPanels(void)
{
	touch.panel[0].family = 0;
	touch.panel[0].variant = 0;
	touch.panel[0].version = 0;
	touch.panel[0].build = 0;
#if FW_SINCE(0x01000104)
	touch.panel[0].configVersion = 0;
#endif
#if FW_SINCE(0x0100060B)
	touch.panel[0].gripEdges[0] = 0;
	touch.panel[0].gripEdges[1] = 0;
#endif
#if SOURCE_0103
	touch.panel[0].objectsUnread = 0;
	touch.panel[0].modesUnread = 0;
#endif
#if FW_SINCE(0x00090804)
	memset(&touch.panel[0].objects, 0, sizeof(touch.panel[0].objects));
	memset(touch.panel[0].modes, 0, sizeof(touch.panel[0].modes));
#else
	/* 0x00090206 clears its five objects one by one */
	touch.panel[0].objects.report = 0;
	touch.panel[0].objects.numTouch = 0;
	touch.panel[0].objects.reset = 0;
	touch.panel[0].objects.backup = 0;
	touch.panel[0].objects.power = 0;
#endif
	touch.panel[1].family = 0;
	touch.panel[1].variant = 0;
	touch.panel[1].version = 0;
	touch.panel[1].build = 0;
#if FW_SINCE(0x01000104)
	touch.panel[1].configVersion = 0;
#endif
#if FW_SINCE(0x0100060B)
	touch.panel[1].gripEdges[0] = 0;
	touch.panel[1].gripEdges[1] = 0;
#endif
#if SOURCE_0103
	touch.panel[1].objectsUnread = 0;
	touch.panel[1].modesUnread = 0;
#endif
#if FW_SINCE(0x00090804)
	memset(&touch.panel[1].objects, 0, sizeof(touch.panel[1].objects));
	memset(touch.panel[1].modes, 0, sizeof(touch.panel[1].modes));
#else
	/* 0x00090206 clears its five objects one by one */
	touch.panel[1].objects.report = 0;
	touch.panel[1].objects.numTouch = 0;
	touch.panel[1].objects.reset = 0;
	touch.panel[1].objects.backup = 0;
	touch.panel[1].objects.power = 0;
#endif
#if FW_SINCE(0x00090804)
	touch.state.value = 0;
#endif
}
#endif

#if !SOURCE_0104
/* opens the link, with no panel enabled */
void touchOpen(void)
{
	csi10Open();
#if TOUCH_PANEL1_IICA1
	iica1Init();
#endif
	touchClearFrames0();
	touchClearFrames1();
	touch.flags = 0;
	touch.frame = 0;
	touch.setting[0] = 8;
	touch.setting[1] = 8;
	touch.rate = 0;
#if SOURCE_0103
	touchClearMode();
#else
	memset(&touch.panel[0], 0, 4);
	memset(&touch.panel[1], 0, 4);
#if FW_SINCE(0x00090804)
	touch.state.value = 0;
#endif
#endif
	touchBuffer.frame = 0;
	touchBuffer.end = 0;
	touchBuffer.data = touchBufferData;
#if !FW_SINCE(0x00090009)
	/* 0x00080504: the registers of firmware 0x20-0x21 until touchIdentify0 reads the version */
	touch.panel[0].objects.report = 0x031e;
	touch.panel[1].objects.report = 0x031e;
	touch.panel[0].objects.numTouch = 0x011c;
	touch.panel[1].objects.numTouch = 0x011c;
	touch.panel[0].objects.reset = 0x00f5;
	touch.panel[1].objects.reset = 0x00f5;
#endif
	memset(&touchCommand, 0, sizeof(touchCommand));
#if FW_SINCE(0x01000306)
	touchDelays.select = configGet()->touchDelaySelect;
#if SOURCE_0103
	touchDelays.byte = configGet()->touchDelayReadByte;
	touchDelays.writeByte = configGet()->touchDelayWriteByte;
	touchDelays.deselect = configGet()->touchDelayDeselect;
	touchDelays.release = configGet()->touchDelayRelease;
#else
	touchDelays.byte = configGet()->touchDelayByte;
	touchDelays.deselect = configGet()->touchDelayDeselect;
#endif
#endif
}

void touchClose(void)
{
	touchDisable0();
	touchDisable1();
#if TOUCH_PANEL1_IICA1
	TOUCH_MODE_PIN = 0;
	csi10Close();
	iica1Close();
#else
	csi10Close();
#endif
}

/* panel 0's parts of both frames: no touch */
void touchClearFrames0(void)
{
#if FW_SINCE(0x00090903)
	memset(touchFrames[0].panel[0], 0x80, TOUCH_REPORT_SIZE);
	memset(&touchFrames[0].panel[0][TOUCH_REPORT_SIZE], 0,
	       TOUCH_PANEL_SIZE - TOUCH_REPORT_SIZE);
	memset(touchFrames[1].panel[0], 0x80, TOUCH_REPORT_SIZE);
	memset(&touchFrames[1].panel[0][TOUCH_REPORT_SIZE], 0,
	       TOUCH_PANEL_SIZE - TOUCH_REPORT_SIZE);
	touchFrames[0].time0 = 0;
	touchFrames[1].time0 = 0;
#else
	memset(touchFrames[0].panel[0], 0x80, TOUCH_PANEL_SIZE);
	memset(touchFrames[1].panel[0], 0x80, TOUCH_PANEL_SIZE);
#endif
}

void touchClearFrames1(void)
{
#if FW_SINCE(0x00090903)
	memset(touchFrames[0].panel[1], 0x80, TOUCH_REPORT_SIZE);
	memset(&touchFrames[0].panel[1][TOUCH_REPORT_SIZE], 0,
	       TOUCH_PANEL_SIZE - TOUCH_REPORT_SIZE);
	memset(touchFrames[1].panel[1], 0x80, TOUCH_REPORT_SIZE);
	memset(&touchFrames[1].panel[1][TOUCH_REPORT_SIZE], 0,
	       TOUCH_PANEL_SIZE - TOUCH_REPORT_SIZE);
	touchFrames[0].time1 = 0;
	touchFrames[1].time1 = 0;
#else
	memset(touchFrames[0].panel[1], 0x80, TOUCH_PANEL_SIZE);
	memset(touchFrames[1].panel[1], 0x80, TOUCH_PANEL_SIZE);
#endif
}
#endif

#if SOURCE_0103
/* starts a frame, into a frame no stream is sending: the rear panel's report first, on IICA1,
   when it is enabled (INTIICA1 goes on from there), else panel 0's on CSI10 */
void touchStartFrame(void)
{
	unsigned char next;
	struct Csi10Transfer transfer;
#if TOUCH_PANEL1_IICA1
	struct Iica1Request request;
#endif

	if (!(touch.flags & TOUCH_RUNNING) && !(touch.flags & TOUCH_BUSY)) {
		touch.flags |= TOUCH_RUNNING;
		if (TOUCH_REAR_READ) {
#if TOUCH_PANEL1_IICA1
			touchCommand.address = touch.panel[1].objects.report;
#else
			TOUCH_SELECT1 = 0;
			transfer.state = CSI10_COMMAND1;
			touchCommand.address = touch.panel[1].objects.report | TOUCH_READ;
#endif
		} else if (touch.flags & TOUCH_PANEL0) {
			TOUCH_SELECT0 = 0;
			transfer.state = CSI10_COMMAND0;
			touchCommand.address = touch.panel[0].objects.report | TOUCH_READ;
		}
		next = TOUCH_NEXT_FRAME(hostStream0300.first);
		if (next == hostStream0300.block || next == hostStream0301.block ||
		    next == hostStream0302.block)
			next = TOUCH_NEXT_FRAME(next);
		touch.frame = next;
		if (TOUCH_REAR_READ) {
			touchFrames[touch.frame].time1 = TCR02;
			TOUCH_FRAME_STATE(touch.frame, 1) = touch.state.value;
		} else if (touch.flags & TOUCH_PANEL0) {
			touchFrames[touch.frame].time0 = TCR02;
			TOUCH_FRAME_STATE(touch.frame, 0) = touch.state.value;
		}
#if TOUCH_PANEL1_IICA1
#if TOUCH_0105
		if (TOUCH_REAR_READ) {
			/* by the controller found: 0's report after its address, 2's alone */
			if (touch.panel[1].controller == 0) {
				request.rxCount = 0x1d;
				request.txCount = 2;
				request.address = touchRearControllers[0].address;
			} else if (touch.panel[1].controller == 2) {
				if (touch.rearShort == 0)
					request.rxCount = 0x20;
				else
					request.rxCount = 0x18;
				request.txCount = 0;
				request.address = touchRearControllers[2].address;
			}
			request.rxData = touchFrames[touch.frame].panel[1];
			request.txData = (unsigned char *)&touchCommand;
			request.status = &touchRearStatus;
			iica1Start(&request);
		} else {
#else
		if (touch.flags & TOUCH_PANEL1) {
			if (touch.panel[1].version == 9 && touch.panel[1].build < 0x12)
				request.rxCount = TOUCH_REAR_LENGTH_OLD;
			else
				request.rxCount = TOUCH_REAR_LENGTH;
			request.rxData = touchFrames[touch.frame].panel[1];
			request.txData = (unsigned char *)&touchCommand;
			request.txCount = 2;
			request.address = IICA1_TOUCH;
			request.status = &touchRearStatus;
			iica1Start(&request);
		} else {
#endif
#endif
#if SOURCE_0104
		touchDelay(TOUCH_DELAY_SELECT);
#else
		touchDelaySelect();
#endif
		transfer.receive = (unsigned char *)&touchReply;
		transfer.receiveCount = 0;
		transfer.send = (unsigned char *)&touchCommand;
		transfer.sendCount = 2;
		transfer.delay = touchDelays.byte;
		csi10Transfer(&transfer);
#if TOUCH_PANEL1_IICA1
		}
#endif
	}
}
#else
/* starts a frame, into a frame no stream is sending: panel 1's report first when it is
   enabled; INTCSI10 goes on from there */
void touchStartFrame(void)
{
	unsigned char next;
	struct Csi10Transfer transfer;

#if FW_SINCE(0x01000306)
	if (csi10GetState() == CSI10_OPEN && !(touch.flags & TOUCH_BUSY)) {
#else
	if (csi10GetState() == CSI10_OPEN) {
#endif
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
#if FW_SINCE(0x00090804)
			TOUCH_FRAME_STATE(touch.frame, 1) = touch.state.value;
#endif
		} else if (touch.flags & TOUCH_PANEL0) {
			touchFrames[touch.frame].time0 = TCR02;
#if FW_SINCE(0x00090804)
			TOUCH_FRAME_STATE(touch.frame, 0) = touch.state.value;
#endif
		}
		touchDelaySelect();
		transfer.receive = (unsigned char *)&touchReply;
		transfer.receiveCount = 0;
		transfer.send = (unsigned char *)&touchCommand;
		transfer.sendCount = 2;
		csi10Transfer(&transfer);
	}
}
#endif

#if FW_SINCE(0x00090804)	/* 0x00090206 has no NoLock forms */
/* touchStartFrame, for a caller that has disabled the interrupts */
#if SOURCE_0103
void touchStartFrameNoLock(void)
{
	unsigned char next;
	struct Csi10Transfer transfer;
#if TOUCH_PANEL1_IICA1
	struct Iica1Request request;
#endif

	if (!(touch.flags & TOUCH_RUNNING) && !(touch.flags & TOUCH_BUSY)) {
		touch.flags |= TOUCH_RUNNING;
		if (TOUCH_REAR_READ) {
#if TOUCH_PANEL1_IICA1
			touchCommand.address = touch.panel[1].objects.report;
#else
			TOUCH_SELECT1 = 0;
			transfer.state = CSI10_COMMAND1;
			touchCommand.address = touch.panel[1].objects.report | TOUCH_READ;
#endif
		} else if (touch.flags & TOUCH_PANEL0) {
			TOUCH_SELECT0 = 0;
			transfer.state = CSI10_COMMAND0;
			touchCommand.address = touch.panel[0].objects.report | TOUCH_READ;
		}
		next = TOUCH_NEXT_FRAME(hostStream0300.first);
		if (next == hostStream0300.block || next == hostStream0301.block ||
		    next == hostStream0302.block)
			next = TOUCH_NEXT_FRAME(next);
		touch.frame = next;
		if (TOUCH_REAR_READ) {
			touchFrames[touch.frame].time1 = TCR02;
			TOUCH_FRAME_STATE(touch.frame, 1) = touch.state.value;
		} else if (touch.flags & TOUCH_PANEL0) {
			touchFrames[touch.frame].time0 = TCR02;
			TOUCH_FRAME_STATE(touch.frame, 0) = touch.state.value;
		}
#if TOUCH_PANEL1_IICA1
#if TOUCH_0105
		if (TOUCH_REAR_READ) {
			/* by the controller found: 0's report after its address, 2's alone */
			if (touch.panel[1].controller == 0) {
				request.rxCount = 0x1d;
				request.txCount = 2;
				request.address = touchRearControllers[0].address;
			} else if (touch.panel[1].controller == 2) {
				if (touch.rearShort == 0)
					request.rxCount = 0x20;
				else
					request.rxCount = 0x18;
				request.txCount = 0;
				request.address = touchRearControllers[2].address;
			}
			request.rxData = touchFrames[touch.frame].panel[1];
			request.txData = (unsigned char *)&touchCommand;
			request.status = &touchRearStatus;
			iica1Start(&request);
		} else {
#else
		if (touch.flags & TOUCH_PANEL1) {
			if (touch.panel[1].version == 9 && touch.panel[1].build < 0x12)
				request.rxCount = TOUCH_REAR_LENGTH_OLD;
			else
				request.rxCount = TOUCH_REAR_LENGTH;
			request.rxData = touchFrames[touch.frame].panel[1];
			request.txData = (unsigned char *)&touchCommand;
			request.txCount = 2;
			request.address = IICA1_TOUCH;
			request.status = &touchRearStatus;
			iica1Start(&request);
		} else {
#endif
#endif
#if SOURCE_0104
		touchDelay(TOUCH_DELAY_SELECT);
#else
		touchDelaySelect();
#endif
		transfer.receive = (unsigned char *)&touchReply;
		transfer.receiveCount = 0;
		transfer.send = (unsigned char *)&touchCommand;
		transfer.sendCount = 2;
		transfer.delay = touchDelays.byte;
		csi10TransferNoLock(&transfer);
#if TOUCH_PANEL1_IICA1
		}
#endif
	}
}
#else
void touchStartFrameNoLock(void)
{
	unsigned char next;
	struct Csi10Transfer transfer;

#if FW_SINCE(0x01000306)
	if (csi10GetState() == CSI10_OPEN && !(touch.flags & TOUCH_BUSY)) {
#else
	if (csi10GetState() == CSI10_OPEN) {
#endif
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
#if FW_SINCE(0x00090804)
			TOUCH_FRAME_STATE(touch.frame, 1) = touch.state.value;
#endif
		} else if (touch.flags & TOUCH_PANEL0) {
			touchFrames[touch.frame].time0 = TCR02;
#if FW_SINCE(0x00090804)
			TOUCH_FRAME_STATE(touch.frame, 0) = touch.state.value;
#endif
		}
		touchDelaySelect();
		transfer.receive = (unsigned char *)&touchReply;
		transfer.receiveCount = 0;
		transfer.send = (unsigned char *)&touchCommand;
		transfer.sendCount = 2;
		csi10TransferNoLock(&transfer);
	}
}
#endif
#endif

#if SOURCE_0104
/* enables a panel when it is present (TOUCH_PRESENT0 or TOUCH_PRESENT1; inputKeys.state's bit
   0x0004 or 0x0008) */
void touchEnable(char panel)
{
	switch (panel) {
	case 0:
		if (TOUCH_PRESENT0 && !(inputKeys.state & 0x0004)) {
			DI();
			inputKeys.state |= 0x0004;
			EI();
			touchSetFlags(TOUCH_PANEL0);
		}
		break;
	case 1:
		if (TOUCH_PRESENT1 && !(inputKeys.state & 0x0008)) {
			DI();
			inputKeys.state |= 0x0008;
			EI();
			touchSetFlags(TOUCH_PANEL1);
		}
		break;
	}
}

/* clears inputKeys.state's high byte too */
void touchDisable(char panel)
{
	switch (panel) {
	case 0:
		DI();
		inputKeys.state &= 0x00fb;
		EI();
		touchClearFlags(TOUCH_PANEL0);
		break;
	case 1:
		DI();
		inputKeys.state &= 0x00f7;
		EI();
		touchClearFlags(TOUCH_PANEL1);
		break;
	}
	touchClearFrames(panel);
}

#if TOUCH_0105
/* the slim's 0x0105: panel 0's select line; the link's result is the panel's own number */
void touchSelectLink(char panel)
{
	if (panel == 0)
		TOUCH_SELECT0 = 0;
}

void touchReleaseLink(char panel)
{
	if (panel == 0)
		TOUCH_SELECT0 = 1;
}

unsigned char touchLinkTransfer(char panel)
{
	unsigned char result;

	result = 0xff;
	switch (panel) {
	case 0:
		result = 0;
		break;
	case 1:
		result = 1;
		break;
	default:
		result = 0xff;
		break;
	}
	return result;
}
#else
/* DOL-1002 has no touch panels: the link's select, release and transfer do nothing, and a
   transfer fails (0xff) */
void touchSelectLink(char panel)
{
}

void touchReleaseLink(char panel)
{
}

unsigned char touchLinkTransfer(char panel)
{
	unsigned char result;

	result = 0xff;
	switch (panel) {
	case 0:
		break;
	case 1:
		break;
	default:
		result = 0xff;
		break;
	}
	return result;
}
#endif

#if TOUCH_0105
/* takes the link and writes: 0, 8 if the link stays busy, 0x3c for a length the buffer cannot
   hold or a panel without a link; panel 0 over CSI10, the rear's controller over the IICA
   (its bootloader at its own address, the data without a register address), three tries more,
   its failures 0x11 and 0x12 */
unsigned char touchWrite(unsigned int address, unsigned char *data, unsigned int length,
			 char panel)
{
	unsigned char result;
	unsigned char wait;
	struct Iica1Request request;
	unsigned char tries;

	result = 0;
	tries = 3;
	if (length >= TOUCH_BUFFER_SIZE + 1)
		return 0x3c;
	touchSetFlags(TOUCH_BUSY);
	wait = 10;
	while (touch.flags & TOUCH_RUNNING) {
		if (wait-- == 0) {
			result = 8;
			goto unlock;
		}
		dly_tsk(1);
	}
	switch (touchLinkTransfer(panel)) {
	case 0:
		touchCommand.address = address * 2;
		memcpy(touchCommand.data, data, length);
		touchSelectLink(panel);
		touchDelay(touchDelays.select);
		result = csi10TransferWait((unsigned char *)&touchReply, 0,
					   (unsigned char *)&touchCommand, length + 2,
					   touchDelays.writeByte);
		touchDelay(touchDelays.deselect);
		touchReleaseLink(panel);
		touchDelay(touchDelays.release);
		break;
	case 1:
		memcpy(touchCommand.data, data, length);
		do {
			switch (panel) {
			case 1:
				if (address == TOUCH_BOOTLOADER) {
					request.address = 0x48;
					request.txData = touchCommand.data;
					request.txCount = length;
				} else {
					request.address = IICA1_TOUCH;
					touchCommand.address = address;
					request.txData = (unsigned char *)&touchCommand;
					request.txCount = length + 2;
				}
				break;
			case 0:
			default:
				return 0x3c;
			}
			request.rxData = NULL;
			request.rxCount = 0;
			request.status = &result;
			iica1Transfer(&request);
			if (result == 0)
				break;
		} while (tries-- != 0);
		if (result != 0)
			result += 0x10;
		break;
	default:
		result = 0x3c;
		break;
	}
unlock:
	touchClearFlags(TOUCH_BUSY);
	return result;
}

/* takes the link and reads: as touchWrite; a read over CSI10 that fails clears data; the
   rear's tries after the first read its bootloader */
unsigned char touchRead(unsigned int address, unsigned char *data, unsigned int length,
			char panel)
{
	unsigned char result;
	unsigned char wait;
	struct Iica1Request request;
	unsigned char tries;

	result = 0;
	tries = 3;
	if (length >= TOUCH_BUFFER_SIZE + 1)
		return 0x3c;
	touchSetFlags(TOUCH_BUSY);
	wait = 10;
	while (touch.flags & TOUCH_RUNNING) {
		if (wait-- == 0) {
			result = 8;
			goto unlock;
		}
		dly_tsk(1);
	}
	switch (touchLinkTransfer(panel)) {
	case 0:
		touchCommand.address = address * 2 | 1;
		touchSelectLink(panel);
		touchDelay(touchDelays.select);
		result = csi10TransferWait((unsigned char *)&touchReply, length + 2,
					   (unsigned char *)&touchCommand, 2, touchDelays.byte);
		touchDelay(touchDelays.deselect);
		touchReleaseLink(panel);
		if (result != 0)
			memset(data, 0, length);
		else
			memcpy(data, touchReply.data, length);
		touchDelay(touchDelays.release);
		break;
	case 1:
		do {
			switch (panel) {
			case 1:
				if (address == TOUCH_BOOTLOADER) {
					request.address = 0x48;
					request.txData = NULL;
					request.txCount = 0;
				} else {
					touchCommand.address = address;
					request.address = IICA1_TOUCH;
					request.txData = (unsigned char *)&touchCommand;
					request.txCount = 2;
				}
				break;
			case 0:
			default:
				return 0x3c;
			}
			request.rxData = data;
			request.rxCount = length;
			request.status = &result;
			iica1Transfer(&request);
			if (result == 0)
				break;
			address = TOUCH_BOOTLOADER;
		} while (tries-- != 0);
		if (result != 0)
			result += 0x10;
		break;
	default:
		result = 0x3c;
		break;
	}
unlock:
	touchClearFlags(TOUCH_BUSY);
	return result;
}

/* finds a panel's controller: panel 0's is 1 when it answers in its bootloader, 0 when not,
   0xff when it does not answer; the rear's is the first of touchRearControllers that answers
   at its address, in three tries each */
void touchProbeController(char panel)
{
	struct Iica1Request request;
	unsigned char data[2];
	unsigned char status;
	unsigned char tries;
	unsigned char i;

	i = 0;
	touchSetFlags(TOUCH_BUSY);
	if (panel == 0) {
		status = touchRead(0, data, 2, 0);
		if (status != 0)
			touch.panel[0].controller = 0xff;
		else if ((data[0] & 0xc0) == 0xc0)
			touch.panel[0].controller = 1;
		else
			touch.panel[0].controller = 0;
	} else {
		request.rxData = data;
		request.txData = NULL;
		request.txCount = 0;
		request.rxCount = 2;
		request.status = &status;
		request.address = touchRearControllers[i].address;
		while (i < 4) {
			for (tries = 3; tries != 0; tries--) {
				iica1Transfer(&request);
				if (status == 0)
					break;
			}
			if (status == 0) {
				touch.panel[panel].controller = i;
				break;
			}
			i++;
			request.address = touchRearControllers[i].address;
		}
	}
	touchClearFlags(TOUCH_BUSY);
}
#else
/* takes the link and writes: 0, 8 if the link stays busy, 0x3c for a length the buffer cannot
   hold or a transfer that fails */
unsigned char touchWrite(unsigned int address, unsigned char *data, unsigned int length,
			 char panel)
{
	unsigned char result;
	unsigned char wait;

	result = 0;
	if (length >= TOUCH_BUFFER_SIZE + 1)
		return 0x3c;
	touchSetFlags(TOUCH_BUSY);
	wait = 10;
	while (touch.flags & TOUCH_RUNNING) {
		if (wait-- == 0) {
			result = 8;
			goto unlock;
		}
		dly_tsk(1);
	}
	touchLinkTransfer(panel);
	result = 0x3c;
unlock:
	touchClearFlags(TOUCH_BUSY);
	return result;
}

unsigned char touchRead(unsigned int address, unsigned char *data, unsigned int length,
			char panel)
{
	unsigned char result;
	unsigned char wait;

	result = 0;
	if (length >= TOUCH_BUFFER_SIZE + 1)
		return 0x3c;
	touchSetFlags(TOUCH_BUSY);
	wait = 10;
	while (touch.flags & TOUCH_RUNNING) {
		if (wait-- == 0) {
			result = 8;
			goto unlock;
		}
		dly_tsk(1);
	}
	touchLinkTransfer(panel);
	result = 0x3c;
unlock:
	touchClearFlags(TOUCH_BUSY);
	return result;
}

#endif

#if TOUCH_0105
/* the rear's controller (panel 1): writes txCount bytes of tx, waits, then reads rxCount bytes
   into rx, three tries; between them it sleeps delay / 4 ticks (rounded up), or, when a read
   follows and delay is 0, waits while P6.7 is high, up to 0x32 ticks for controller 3 and
   0x226 for the others */
unsigned char touchWriteRead(unsigned char *tx, unsigned int txCount, unsigned char *rx,
			     unsigned int rxCount, char panel, unsigned int delay)
{
	unsigned char result;
	unsigned int wait;
	struct Iica1Request request;
	unsigned char tries;

	result = 0;
	tries = 2;
	if (txCount >= TOUCH_BUFFER_SIZE + 1 || rxCount >= TOUCH_BUFFER_SIZE + 1)
		return 0x3c;
	touchSetFlags(TOUCH_BUSY);
	wait = 10;
	while (touch.flags & TOUCH_RUNNING) {
		if (wait-- == 0) {
			result = 8;
			goto unlock;
		}
		dly_tsk(1);
	}
	switch (touchLinkTransfer(panel)) {
	case 1:
		do {
			switch (panel) {
			case 1:
				request.address =
					touchRearControllers[touch.panel[1].controller].address;
				break;
			case 0:
			default:
				return 0x3c;
			}
			request.txData = tx;
			request.txCount = txCount;
			request.rxData = NULL;
			request.rxCount = 0;
			request.status = &result;
			iica1Transfer(&request);
			if (rxCount != 0) {
				if (delay != 0)
					wait = delay / 4 + (delay % 4 ? 1 : 0);
				else if (touch.panel[panel].controller == 3)
					wait = 0x32;
				else
					wait = 0x226;
				while (P6.7) {
					if (wait == 0)
						break;
					dly_tsk(0);
					wait--;
				}
			} else
				dly_tsk(delay / 4 + (delay % 4 ? 1 : 0));
			request.txData = NULL;
			request.txCount = 0;
			request.rxData = rx;
			request.rxCount = rxCount;
			request.status = &result;
			iica1Transfer(&request);
			if (result == 0)
				break;
		} while (tries-- != 0);
		if (result != 0)
			result += 0x10;
		break;
	default:
		result = 0x3c;
		break;
	}
unlock:
	touchClearFlags(TOUCH_BUSY);
	return result;
}
#endif

/* writes, and reads back to compare, as the settings block says: 0, 8 if the link stays busy,
   9 if no try compared */
unsigned char touchWriteVerified(unsigned int address, unsigned char *data,
				 unsigned int length, char panel)
{
	unsigned char count;
	unsigned char result;
	unsigned char matches;
	unsigned char reads;

#if TOUCH_0105
	if (panel == 1 && touch.panel[1].controller >= 2)
		return 0;
#endif
	touchSetFlags(TOUCH_BUSY);
	count = 10;
	while (touch.flags & TOUCH_RUNNING) {
		if (count-- == 0) {
			result = 8;
			goto unlock;
		}
		dly_tsk(1);
	}
	count = configGet()->touchWriteTries;
	while (count-- != 0) {
		if (touchWrite(address, data, length, panel) != 0)
			continue;
		reads = configGet()->touchWriteReads;
		matches = 0;
		for (; reads != 0; reads--) {
			if (touchRead(address, (unsigned char *)&touchReply, length, panel) == 0 &&
			    memcmp(data, &touchReply, length) == 0)
				matches++;
		}
		if (matches == configGet()->touchWriteReads) {
			result = 0;
			goto unlock;
		}
	}
	result = 9;
unlock:
	touchClearFlags(TOUCH_BUSY);
	return result;
}

/* reads a panel's information block, and its object table when its firmware changed or the
   last read failed: 0, the read's error, or 0x40 for a table too long; a controller in its
   bootloader has its status as its information */
#if TOUCH_0105
unsigned char touchIdentify(char panel)
{
	unsigned char result;
	unsigned char info[TOUCH_INFO_SIZE + 2];

	switch (panel) {
	case 0:
		if (!TOUCH_PRESENT0) {
			memset(&touch.panel[panel], 0xff, 4);
			return 0;
		}
		break;
	case 1:
		if (!TOUCH_PRESENT1) {
			memset(&touch.panel[panel], 0xff, 4);
			return 0;
		}
		break;
	}
	touchProbeController(panel);
	if (touch.panel[panel].controller == 2) {
		result = touchRearReadInfo(touchBufferData, panel);
		if (result == 0 && touchBufferData[0] == 0xff)
			touch.panel[panel].controller = 3;
	}
	if (touch.panel[panel].controller == 0) {
		result = touchRead(0x0000, info, TOUCH_INFO_SIZE, panel);
		if (result != 0) {
			memset(&touch.panel[panel], 0xff, 4);
		} else if (touch.panel[panel].version != info[TOUCH_INFO_VERSION] ||
			   touch.panel[panel].build != info[TOUCH_INFO_BUILD] ||
			   touch.panel[panel].objectsUnread) {
			memcpy(&touch.panel[panel], info, 4);
			if (info[TOUCH_INFO_OBJECTS] < TOUCH_OBJECTS_MAX) {
				touch.panel[panel].objectsUnread = 0;
				result = touchReadObjects(&touch.panel[panel].objects,
							  info[TOUCH_INFO_OBJECTS], panel);
				if (result != 0)
					touch.panel[panel].objectsUnread = 1;
			} else {
				touch.panel[panel].objectsUnread = 1;
				result = 0x40;
			}
		}
	} else if (touch.panel[panel].controller == 1) {
		memset(&touch.panel[panel], info[0], 4);
		touch.panel[panel].objectsUnread = 0;
		result = 0;
	} else if (touch.panel[panel].controller == 2 || touch.panel[panel].controller == 3)
		result = touchRearIdentify(info, TOUCH_REAR_INFO, panel);
	else
		result = 0x3e;
	touchSetFeatures(panel);
	return result;
}
#else
unsigned char touchIdentify(char panel)
{
	unsigned char result;
	unsigned char info[TOUCH_INFO_SIZE];

	switch (panel) {
	case 0:
		if (!TOUCH_PRESENT0) {
			memset(&touch.panel[panel], 0xff, 4);
			return 0;
		}
		break;
	case 1:
		if (!TOUCH_PRESENT1) {
			memset(&touch.panel[panel], 0xff, 4);
			return 0;
		}
		break;
	}
	result = touchRead(0x0000, info, sizeof(info), panel);
	if (result != 0) {
		memset(&touch.panel[panel], 0xff, 4);
	} else if (info[TOUCH_INFO_VERSION] != touch.panel[panel].version ||
		   info[TOUCH_INFO_BUILD] != touch.panel[panel].build ||
		   touch.panel[panel].objectsUnread) {
		if ((info[0] & 0xc0) == 0xc0) {
			memset(&touch.panel[panel], info[0], 4);
			touch.panel[panel].objectsUnread = 0;
			result = 0;
		} else {
			memcpy(&touch.panel[panel], info, 4);
			if (info[TOUCH_INFO_OBJECTS] < TOUCH_OBJECTS_MAX) {
				touch.panel[panel].objectsUnread = 0;
				result = touchReadObjects(&touch.panel[panel].objects,
							  info[TOUCH_INFO_OBJECTS], panel);
				if (result != 0)
					touch.panel[panel].objectsUnread = 1;
			} else {
				touch.panel[panel].objectsUnread = 1;
				result = 0x40;
			}
		}
		touchSetFeatures(panel);
	}
	return result;
}
#endif

/* what the controller has, by its family and variant (and version) */
void touchSetFeatures(char panel)
{
	switch (TOUCH_PANEL_ID(panel)) {
	case TOUCH_ID(0x80, 0x0a):
		if (touch.panel[panel].version < 0x31) {
			touch.panel[panel].features.bits.noModes = 0;
			touch.panel[panel].features.bits.family82 = 0;
			touch.panel[panel].features.bits.t51Settings = 0;
			touch.panel[panel].features.bits.gripInUserData = 0;
		} else {
			touch.panel[panel].features.bits.noModes = 0;
			touch.panel[panel].features.bits.family82 = 0;
			touch.panel[panel].features.bits.t51Settings = 1;
			touch.panel[panel].features.bits.gripInUserData = 1;
		}
		break;
	case TOUCH_ID(0x82, 0x27):
	case TOUCH_ID(0x82, 0x2b):
	case TOUCH_ID(0x82, 0x34):
	case TOUCH_ID(0x82, 0x3a):
		touch.panel[panel].features.bits.noModes = 1;
		touch.panel[panel].features.bits.family82 = 1;
		touch.panel[panel].features.bits.t51Settings = 1;
		touch.panel[panel].features.bits.gripInUserData = 1;
		break;
#if TOUCH_0105
	case TOUCH_ID(0x44, 0x28):
		touch.panel[panel].features.bits.noModes = 0;
		touch.panel[panel].features.bits.family82 = 1;
		touch.panel[panel].features.bits.t51Settings = 0;
		touch.panel[panel].features.bits.gripInUserData = 0;
		break;
#endif
	default:
		touch.panel[panel].features.bits.noModes = 0;
		touch.panel[panel].features.bits.family82 = 0;
		touch.panel[panel].features.bits.t51Settings = 0;
		touch.panel[panel].features.bits.gripInUserData = 0;
		break;
	}
}

/* the bootloader's status byte, its two state bits alone when one is set */
unsigned char touchBootloaderStatus(unsigned char *status, char panel)
{
	unsigned char result;

#if TOUCH_0105
	if (panel == 1 && touch.panel[1].controller >= 2)
		return 0x11;
#endif
	result = touchRead(TOUCH_BOOTLOADER, status, 1, panel);
	if (result == 0 && (*status & 0xc0) != 0)
		*status &= 0xc0;
	return result;
}

/* resets the controller into its bootloader */
unsigned char touchEnterBootloader(char panel)
{
	unsigned char command;

	command = 0xa5;
	return touchWrite(touch.panel[panel].objects.reset, &command, 1, panel);
}

unsigned char touchBootloaderUnlock(char panel)
{
	unsigned char command[2];

	command[0] = 0xdc;
	command[1] = 0xaa;
	return touchWrite(TOUCH_BOOTLOADER, command, 2, panel);
}

/* saves the controller's objects to its non-volatile memory (T6's BACKUPNV) */
unsigned char touchBackup(char panel)
{
	unsigned char command;

	command = 0x55;
	return touchWrite(touch.panel[panel].objects.reset + 1, &command, 1, panel);
}

unsigned char touchReset(char panel)
{
	unsigned char command;

	command = 0xff;
	return touchWrite(touch.panel[panel].objects.reset, &command, 1, panel);
}

/* the acquisition interval, idle and active */
unsigned char touchSetPower(unsigned char interval, char panel)
{
	unsigned char data[2];

	data[0] = interval;
	data[1] = interval;
#if TOUCH_0105
	if (panel == 1 && touch.panel[1].controller >= 2)
		return 0;
#endif
	return touchWriteVerified(touch.panel[panel].objects.power, data, 2, panel);
}
#else
#if SOURCE_0103
/* enables panel 0 when it is present (TOUCH_PRESENT0, the slim's P4.1; inputKeys.state's bit 0x0004) */
void touchEnable0(void)
{
	if (TOUCH_PRESENT0 && !(inputKeys.state & 0x0004)) {
		DI();
		inputKeys.state |= 0x0004;
		EI();
		touchSetFlags(TOUCH_PANEL0);
	}
}

/* clears inputKeys.state's high byte too */
void touchDisable0(void)
{
	DI();
	inputKeys.state &= 0x00fb;
	EI();
	touchClearFlags(TOUCH_PANEL0);
	touchClearFrames0();
}

/* the rear panel's presence is TOUCH_PRESENT1 (the slim's P4.2) */
void touchEnable1(void)
{
	if (TOUCH_PRESENT1 && !(inputKeys.state & 0x0008)) {
		DI();
		inputKeys.state |= 0x0008;
		EI();
		touchSetFlags(TOUCH_PANEL1);
	}
}

void touchDisable1(void)
{
	DI();
	inputKeys.state &= 0x00f7;
	EI();
	touchClearFlags(TOUCH_PANEL1);
	touchClearFrames1();
}
#else
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
#endif

#if FW_SINCE(0x01000306)
/* writes length bytes at a controller's address, on the link the caller has taken: 0, 1 for
   no such panel, 2 if the transfer does not end */
unsigned char touchSpiWrite(unsigned int address, unsigned char *data, unsigned int length,
			    unsigned char panel)
#if SOURCE_0103
{
	struct Csi10Transfer transfer;
	unsigned char retry;
	unsigned char i;
#if TOUCH_PANEL1_IICA1
	struct Iica1Request request;
	unsigned char status;
	unsigned char tries;
#endif

	if (length > sizeof(touchCommand.data))
		return 1;
#if TOUCH_PANEL1_IICA1
	if (panel == 1) {
		/* the rear panel, on IICA1: three tries */
		tries = 3;
		touchCommand.address = address;
		memcpy(touchCommand.data, data, length);
		do {
			request.rxData = NULL;
			request.rxCount = 0;
			request.txData = (unsigned char *)&touchCommand;
			request.txCount = length + 2;
			request.address = IICA1_TOUCH;
			request.status = &status;
			if (iica1Transfer(&request) == 0)
				break;
		} while (tries-- != 0);
		if (status != 0)
			status += 0x10;
		return status;
	}
#endif
	touchCommand.address = address * 2;
	memcpy(touchCommand.data, data, length);
	if (panel == 0)
		TOUCH_SELECT0 = 0;
#if TOUCH_PANEL1_IICA1
	else if (panel != 1)
		return 1;
#else
	else if (panel == 1)
		TOUCH_SELECT1 = 0;
	else
		return 1;
#endif
	touchDelaySelect();
	transfer.receive = (unsigned char *)&touchReply;
	transfer.receiveCount = 0;
	transfer.send = (unsigned char *)&touchCommand;
	transfer.sendCount = length + 2;
	transfer.state = CSI10_TRANSFER;
	transfer.delay = touchDelays.writeByte;
	csi10Transfer(&transfer);
	retry = 0xff;
	do {
		if (csi10GetState() == CSI10_DONE) {
			for (i = 0; i < touchDelays.deselect; i++)
				NOP();
			if (panel == 0)
				TOUCH_SELECT0 = 1;
			else if (panel == 1)
#if TOUCH_PANEL1_IICA1
				;
#else
				TOUCH_SELECT1 = 1;
#endif
			for (i = 0; i < touchDelays.release; i++)
				NOP();
			return 0;
		}
	} while (retry-- != 0);
	return 2;
}
#else
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
#endif

/* reads length bytes at a controller's address, as touchSpiWrite */
unsigned char touchSpiRead(unsigned int address, unsigned char *data, unsigned int length,
			   unsigned char panel)
#if SOURCE_0103
{
	struct Csi10Transfer transfer;
	unsigned char retry;
	unsigned char i;
#if TOUCH_PANEL1_IICA1
	struct Iica1Request request;
	unsigned char status;
	unsigned char tries;
#endif

	if (length > sizeof(touchCommand.data))
		return 1;
#if TOUCH_PANEL1_IICA1
	if (panel == 1) {
		tries = 3;
		touchCommand.address = address;
		do {
			request.rxData = data;
			request.rxCount = length;
			request.txData = (unsigned char *)&touchCommand;
			request.txCount = 2;
			request.address = IICA1_TOUCH;
			request.status = &status;
			if (iica1Transfer(&request) == 0)
				break;
		} while (tries-- != 0);
		if (status != 0)
			status += 0x10;
		return status;
	}
#endif
	touchCommand.address = address * 2 | TOUCH_READ;
	if (panel == 0)
		TOUCH_SELECT0 = 0;
#if TOUCH_PANEL1_IICA1
	else if (panel != 1)
		return 1;
#else
	else if (panel == 1)
		TOUCH_SELECT1 = 0;
	else
		return 1;
#endif
	touchDelaySelect();
	transfer.receive = (unsigned char *)&touchReply;
	transfer.receiveCount = length + 2;
	transfer.send = (unsigned char *)&touchCommand;
	transfer.sendCount = 2;
	transfer.state = CSI10_TRANSFER;
	transfer.delay = touchDelays.byte;
	csi10Transfer(&transfer);
	retry = 0xff;
	do {
		if (csi10GetState() == CSI10_DONE) {
			for (i = 0; i < touchDelays.deselect; i++)
				NOP();
			if (panel == 0)
				TOUCH_SELECT0 = 1;
			else if (panel == 1)
#if TOUCH_PANEL1_IICA1
				;
#else
				TOUCH_SELECT1 = 1;
#endif
			memcpy(data, touchReply.data, length);
			for (i = 0; i < touchDelays.release; i++)
				NOP();
			return 0;
		}
	} while (retry-- != 0);
	return 2;
}
#else
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
#endif

#if TOUCH_PANEL1_IICA1
/* writes length bytes to the rear panel's bootloader, on IICA1 (panel 1 only): 0, 1 for no
   such panel or too long, 2 if the link stays busy, else 0x10 + the transfer's status */
unsigned char touchBootloaderWrite(unsigned char *data, unsigned int length, unsigned char panel)
{
	struct Iica1Request request;
	unsigned char status;
	unsigned char tries = 3;
	unsigned char wait;

	if (length > sizeof(touchCommand.data))
		return 1;
	if (panel != 1)
		return 1;
	touchSetFlags(TOUCH_BUSY);
	wait = 10;
	while (TOUCH_LINK_BUSY()) {
		if (wait-- == 0) {
			status = 2;
			goto unlock;
		}
		dly_tsk(1);
	}
	memcpy(touchCommand.data, data, length);
	do {
		request.rxData = NULL;
		request.rxCount = 0;
		request.txData = touchCommand.data;
		request.txCount = length + 2;
		request.address = IICA1_TOUCH_BOOTLOADER;
		request.status = &status;
		if (iica1Transfer(&request) == 0)
			break;
	} while (tries-- != 0);
	if (status != 0)
		status += 0x10;
unlock:
	touchClearFlags(TOUCH_BUSY);
	return status;
}

/* reads length bytes from the rear panel's bootloader, as touchBootloaderWrite */
unsigned char touchBootloaderRead(unsigned char *data, unsigned int length, unsigned char panel)
{
	struct Iica1Request request;
	unsigned char status;
	unsigned char tries = 3;
	unsigned char wait;

	if (length > sizeof(touchCommand.data))
		return 1;
	if (panel != 1)
		return 1;
	touchSetFlags(TOUCH_BUSY);
	wait = 10;
	while (TOUCH_LINK_BUSY()) {
		if (wait-- == 0) {
			status = 2;
			goto unlock;
		}
		dly_tsk(1);
	}
	do {
		request.rxData = data;
		request.rxCount = length;
		request.txData = NULL;
		request.txCount = 0;
		request.address = IICA1_TOUCH_BOOTLOADER;
		request.status = &status;
		if (iica1Transfer(&request) == 0)
			break;
	} while (tries-- != 0);
	if (status != 0)
		status += 0x10;
unlock:
	touchClearFlags(TOUCH_BUSY);
	return status;
}

#endif
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
	while (TOUCH_LINK_BUSY()) {
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
	while (TOUCH_LINK_BUSY()) {
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
	while (TOUCH_LINK_BUSY()) {
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
#else
/* takes the link and writes: 0, 1 for no such panel, 2 if the link stays busy or the transfer
   does not end */
unsigned char touchWrite(unsigned int address, unsigned char *data, unsigned int length,
			 unsigned char panel)
{
	struct Csi10Transfer transfer;
	unsigned char retry;

	retry = 10;
	while (TOUCH_LINK_BUSY()) {
		if (retry-- == 0)
			goto busy;
		dly_tsk(1);
	}
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
#if FW_SINCE(0x00090009)
	retry = 0xff;
	do {
		if (csi10GetState() == CSI10_DONE) {
			if (panel == 0)
				P0.1 = 1;
			else if (panel == 1)
				P0.0 = 1;
			csi10Release();
			return 0;
		}
	} while (retry-- != 0);
#else
	/* 0x00080504 waits a tick between its 30 looks */
	retry = 30;
	for (;;) {
		if (csi10GetState() == CSI10_DONE) {
			if (panel == 0)
				P0.1 = 1;
			else if (panel == 1)
				P0.0 = 1;
			csi10Release();
			return 0;
		}
		if (retry-- == 0)
			break;
		dly_tsk(1);
	}
#endif
busy:
	return 2;
}

unsigned char touchRead(unsigned int address, unsigned char *data, unsigned int length,
			unsigned char panel)
{
	struct Csi10Transfer transfer;
	unsigned char retry;

	retry = 10;
	while (TOUCH_LINK_BUSY()) {
		if (retry-- == 0)
			goto busy;
		dly_tsk(1);
	}
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
#if FW_SINCE(0x00090009)
	retry = 0xff;
	do {
		if (csi10GetState() == CSI10_DONE) {
			if (panel == 0)
				P0.1 = 1;
			else if (panel == 1)
				P0.0 = 1;
			memcpy(data, touchReply.data, length);
			csi10Release();
			return 0;
		}
	} while (retry-- != 0);
#else
	/* 0x00080504 waits a tick between its 30 looks */
	retry = 30;
	for (;;) {
		if (csi10GetState() == CSI10_DONE) {
			if (panel == 0)
				P0.1 = 1;
			else if (panel == 1)
				P0.0 = 1;
			memcpy(data, touchReply.data, length);
			csi10Release();
			return 0;
		}
		if (retry-- == 0)
			break;
		dly_tsk(1);
	}
#endif
busy:
	return 2;
}
#endif

#if SOURCE_0103
/* reads panel 0's information block, and its object table when its firmware changed or the
   last read failed: 0, the read's error, or 4 for a table too long */
unsigned char touchIdentify0(void)
{
	unsigned char result;
	unsigned char info[TOUCH_INFO_SIZE];

	if (TOUCH_PRESENT0) {
		result = touchRead(0x0000, info, sizeof(info), 0);
		if (result != 0) {
			memset(&touch.panel[0], 0xff, 4);
		} else if (info[TOUCH_INFO_VERSION] != touch.panel[0].version ||
			   info[TOUCH_INFO_BUILD] != touch.panel[0].build ||
			   touch.panel[0].objectsUnread) {
#if !BOARD_DOL1001	/* DOL-1001 keeps the version as read */
			/* version 0x10 is 9 unless its build is 0xaa */
			if (info[TOUCH_INFO_VERSION] == 0x10 && info[TOUCH_INFO_BUILD] != 0xaa)
				info[TOUCH_INFO_VERSION] = 9;
#endif
			memcpy(&touch.panel[0], info, 4);
			if ((info[0] & 0xc0) == 0xc0) {
				touch.panel[0].objectsUnread = 0;
				result = 0;
			} else if (info[TOUCH_INFO_OBJECTS] < TOUCH_OBJECTS_MAX) {
				touch.panel[0].objectsUnread = 0;
				result = touchReadObjects(&touch.panel[0].objects,
							  info[TOUCH_INFO_OBJECTS], 0);
				if (result != 0)
					touch.panel[0].objectsUnread = 1;
			} else {
				touch.panel[0].objectsUnread = 1;
				result = 4;
			}
		}
	} else {
		result = 0;
		memset(&touch.panel[0], 0xff, 4);
	}
	return result;
}

/* reads panel 1's information block, and its object table when its firmware changed or the
   last read failed: 0, the read's error, or 4 for a table too long;
   a controller in its bootloader has its status as its information */
unsigned char touchIdentify1(void)
{
	unsigned char result;
	unsigned char info[TOUCH_INFO_SIZE];

	if (TOUCH_PRESENT1) {
		result = touchRead(0x0000, info, sizeof(info), 1);
		if (result != 0) {
#if TOUCH_PANEL1_IICA1
			/* 0x11: no answer at the controller's address; its bootloader's status
			   fills the panel's information */
			if (result == 0x11) {
				result = touchBootloaderStatus1(info);
				if (result != 0)
					memset(&touch.panel[1], 0xff, 4);
				else
					memset(&touch.panel[1], info[0], 4);
			} else
				memset(&touch.panel[1], 0xff, 4);
#else
			memset(&touch.panel[1], 0xff, 4);
#endif
		} else if (info[TOUCH_INFO_VERSION] != touch.panel[1].version ||
			   info[TOUCH_INFO_BUILD] != touch.panel[1].build ||
			   touch.panel[1].objectsUnread) {
#if !BOARD_DOL1001	/* DOL-1001 keeps the version as read */
			/* version 0x10 is 9 unless its build is 0xaa */
			if (info[TOUCH_INFO_VERSION] == 0x10 && info[TOUCH_INFO_BUILD] != 0xaa)
				info[TOUCH_INFO_VERSION] = 9;
#endif
			memcpy(&touch.panel[1], info, 4);
			if ((info[0] & 0xc0) == 0xc0) {
				touch.panel[1].objectsUnread = 0;
				result = 0;
			} else if (info[TOUCH_INFO_OBJECTS] < TOUCH_OBJECTS_MAX) {
				touch.panel[1].objectsUnread = 0;
				result = touchReadObjects(&touch.panel[1].objects,
							  info[TOUCH_INFO_OBJECTS], 1);
				if (result != 0)
					touch.panel[1].objectsUnread = 1;
			} else {
				touch.panel[1].objectsUnread = 1;
				result = 4;
			}
		}
	} else {
		result = 0;
		memset(&touch.panel[1], 0xff, 4);
	}
	return result;
}

/* the bootloader's status byte, its two state bits alone when one is set */
unsigned char touchBootloaderStatus0(unsigned char *status)
{
	unsigned char result;

	result = touchRead(TOUCH_BOOTLOADER, status, 1, 0);
	if (result == 0 && (*status & 0xc0) != 0)
		*status &= 0xc0;
	return result;
}

/* the rear panel's bootloader is on IICA1 (DOL-1001's panel 1 on the SPI link) */
unsigned char touchBootloaderStatus1(unsigned char *status)
{
	unsigned char result;

#if TOUCH_PANEL1_IICA1
	result = touchBootloaderRead(status, 1, 1);
#else
	result = touchRead(TOUCH_BOOTLOADER, status, 1, 1);
#endif
	if (result == 0 && (*status & 0xc0) != 0)
		*status &= 0xc0;
	return result;
}
#elif FW_SINCE(0x00090009)
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
#else
/* 0x00080504: the information block's first four bytes read into the panel, and the registers
   of the controller's firmware version, for each version the module knows */
void touchIdentify0(void)
{
	int result;

	if (P0.6) {
		result = touchRead(0x0000, (unsigned char *)&touch.panel[0], 4, 0);
		if (result != 0)
			memset(&touch.panel[0], 0xff, 4);
		else if (touch.panel[0].version < 0x20) {
			touch.panel[0].objects.report = 0x0358;
			touch.panel[0].objects.numTouch = 0x011a;
			touch.panel[0].objects.reset = 0x00fb;
		} else if (touch.panel[0].version < 0x22) {
			touch.panel[0].objects.report = 0x031e;
			touch.panel[0].objects.numTouch = 0x011c;
			touch.panel[0].objects.reset = 0x00f5;
		} else if (touch.panel[0].version < 0x23) {
			touch.panel[0].objects.report = 0x0288;
			touch.panel[0].objects.numTouch = 0x0106;
			touch.panel[0].objects.reset = 0x00dd;
		} else if (touch.panel[0].version >= 0xf0) {
			touch.panel[0].objects.report = 0x031e;
			touch.panel[0].objects.numTouch = 0x011c;
			touch.panel[0].objects.reset = 0x00f5;
		} else {
			touch.panel[0].objects.report = 0x00c0;
			touch.panel[0].objects.numTouch = 0x01a0;
			touch.panel[0].objects.reset = 0x0177;
		}
	} else
		memset(&touch.panel[0], 0xff, 4);
}

void touchIdentify1(void)
{
	int result;

	if (P0.5) {
		result = touchRead(0x0000, (unsigned char *)&touch.panel[1], 4, 1);
		if (result != 0)
			memset(&touch.panel[1], 0xff, 4);
		else if (touch.panel[1].version < 0x20) {
			touch.panel[1].objects.report = 0x0358;
			touch.panel[1].objects.numTouch = 0x011a;
			touch.panel[1].objects.reset = 0x00fb;
		} else if (touch.panel[1].version < 0x22) {
			touch.panel[1].objects.report = 0x031e;
			touch.panel[1].objects.numTouch = 0x011c;
			touch.panel[1].objects.reset = 0x00f5;
		} else if (touch.panel[1].version < 0x23) {
			touch.panel[1].objects.report = 0x0288;
			touch.panel[1].objects.numTouch = 0x0106;
			touch.panel[1].objects.reset = 0x00dd;
		} else if (touch.panel[1].version >= 0xf0) {
			touch.panel[1].objects.report = 0x031e;
			touch.panel[1].objects.numTouch = 0x011c;
			touch.panel[1].objects.reset = 0x00f5;
		} else {
			touch.panel[1].objects.report = 0x00c0;
			touch.panel[1].objects.numTouch = 0x01a0;
			touch.panel[1].objects.reset = 0x0177;
		}
	} else
		memset(&touch.panel[1], 0xff, 4);
}
#endif

#if !SOURCE_0103
unsigned char touchBootloaderStatus0(unsigned char *status)
{
	return touchRead(TOUCH_BOOTLOADER, status, 1, 0);
}

unsigned char touchBootloaderStatus1(unsigned char *status)
{
	return touchRead(TOUCH_BOOTLOADER, status, 1, 1);
}
#endif

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
#if TOUCH_PANEL1_IICA1
	return touchBootloaderWrite(command, 2, 1);
#else
	return touchWrite(TOUCH_BOOTLOADER, command, 2, 1);
#endif
}

#if FW_SINCE(0x00090009)	/* 0x00080504: no backup, reset or power commands */
/* saves the controller's objects to its non-volatile memory */
unsigned char touchBackup0(void)
{
	unsigned char command;

	command = 0x55;
#if FW_SINCE(0x00090206)
	return touchWrite(touch.panel[0].objects.backup, &command, 1, 0);
#else	/* 0x00090009 passes the byte itself as the pointer */
	return touchWrite(touch.panel[0].objects.backup, (unsigned char *)command, 1, 0);
#endif
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
#if FW_SINCE(0x00090206)
	return touchWrite(touch.panel[1].objects.backup, &command, 1, 1);
#else	/* 0x00090009 passes the byte itself as the pointer */
	return touchWrite(touch.panel[1].objects.backup, (unsigned char *)command, 1, 1);
#endif
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
#endif
#endif

#if FW_SINCE(0x00090804)	/* 0x00090206 has no modes, user data or self tests */
/* reads the user data's configuration version into data (2 bytes), and the modes when it
   changed (before 0x01000109 task3 reads them after touchIdentify) */
/* PDEL's 0x01000104 and 0x01000107 read the modes of a changed configuration before it keeps the version */
#define TOUCH_MODES_BEFORE_VERSION	(BOARD_IS(0x00416000) && !FW_SINCE(0x01000109))

#if SOURCE_0104
/* reads the user data's configuration version into data (2 bytes), and the modes when it
   changed or their last read failed; a controller in its bootloader gives its version as
   bytes 4-5 of its 6-byte answer, and has no modes to read */
unsigned char touchReadConfigVersion(unsigned char *data, char panel)
{
	unsigned char result;
	unsigned char status;
	unsigned int version;

#if TOUCH_0105
	/* the rear's controllers 2 and 3 give it little-endian */
	if (panel == 1 &&
	    (touch.panel[1].controller == 2 || touch.panel[1].controller == 3)) {
		result = touchRearIdentify(data, TOUCH_REAR_VERSION, panel);
		version = data[0];
		data[0] = data[1];
		data[1] = version;
		return 0;
	}
#endif
	if ((touch.panel[panel].family & 0xc0) == 0xc0) {
		result = touchRead(TOUCH_BOOTLOADER, touchBufferData, 6, panel);
		if (result == 0) {
			data[0] = touchBufferData[4];
			data[1] = touchBufferData[5];
		}
	} else
		result = touchRead(touch.panel[panel].objects.userData, data, 2, panel);
	if (result == 0) {
		version = (unsigned int)data[0] << 8 | data[1];
		if (touch.panel[panel].configVersion != version ||
		    touch.panel[panel].modesUnread) {
			touch.panel[panel].configVersion = version;
			if ((touch.panel[panel].family & 0xc0) != 0xc0)
				result = touchReadModes(panel);
		}
	}
	return result;
}

/* the user data's bytes 12-19, T60's where the controller has one */
unsigned char touchReadUserData(unsigned char *data, char panel)
{
	unsigned int address;

#if TOUCH_0105
	if (panel == 1 &&
	    (touch.panel[1].controller == 2 || touch.panel[1].controller == 3))
		return touchRearIdentify(data, TOUCH_REAR_EXTRA, panel);
#endif
	if (touch.panel[panel].objects.t60 != 0)
		address = touch.panel[panel].objects.t60;
	else
		address = touch.panel[panel].objects.userData + 0x0c;
	return touchRead(address, data, 8, panel);
}

/* runs a self test (T25); *pass 0 if it passed, else 0xff; the result's address in T34
   by the controller's family and variant */
unsigned char touchSelfTest(unsigned char test, unsigned char *pass, char panel)
{
	unsigned int address;
	unsigned char command[2];
	unsigned char value;
	unsigned char result;

	command[0] = 3;
	command[1] = test;
	result = touchWrite(touch.panel[panel].objects.selfTest, command, 2, panel);
	if (result != 0) {
		*pass = 0xff;
	} else {
		dly_tsk(8);
		if (TOUCH_PANEL_ID(panel) == TOUCH_ID(0x82, 0x2b))
			address = touch.panel[panel].objects.t34 + 0x5b;
		else if (TOUCH_PANEL_ID(panel) == TOUCH_ID(0x82, 0x27))
			address = touch.panel[panel].objects.t34 + 0x31;
		else if (TOUCH_PANEL_ID(panel) == TOUCH_ID(0x82, 0x34))
			address = touch.panel[panel].objects.t34 + 0x54;
		else if (TOUCH_PANEL_ID(panel) == TOUCH_ID(0x82, 0x3a))
			address = touch.panel[panel].objects.t34 + 0x5b;
		else
			address = touch.panel[panel].objects.t34 + 0x62;
		result = touchRead(address, &value, 1, panel);
		if (result == 0) {
			if (value == 0xfe)
				*pass = 0;
			else
				*pass = 0xff;
		} else
			*pass = 0xff;
	}
	return result;
}

/* T51's byte 4; 2 for a controller without T51's settings */
unsigned char touchSetT51Byte4(unsigned char value, char panel)
{
	unsigned char byte;
	unsigned char result;

	if (!touch.panel[panel].features.bits.t51Settings)
		result = 2;
	else {
		byte = value;
		result = touchWrite(touch.panel[panel].objects.t51 + 4, &byte, 1, panel);
	}
	return result;
}

/* T34's 7 bytes from 0x70 (0x46 for family 0x82 variant 0x27, 0x62 for 0x82 0x34) */
unsigned char touchReadT34Bytes(unsigned char *data, char panel)
{
	unsigned int address;
	unsigned char result;

	if (TOUCH_PANEL_ID(panel) == TOUCH_ID(0x82, 0x2b))
		address = touch.panel[panel].objects.t34 + 0x70;
	else if (TOUCH_PANEL_ID(panel) == TOUCH_ID(0x82, 0x27))
		address = touch.panel[panel].objects.t34 + 0x46;
	else if (TOUCH_PANEL_ID(panel) == TOUCH_ID(0x82, 0x34))
		address = touch.panel[panel].objects.t34 + 0x62;
	else if (TOUCH_PANEL_ID(panel) == TOUCH_ID(0x82, 0x3a))
		address = touch.panel[panel].objects.t34 + 0x70;
	else
		address = touch.panel[panel].objects.t34 + 0x70;
	result = touchRead(address, data, 7, panel);
	return result;
}

/* T51's bytes 5-6, and 9-10, as touchSetT51Byte4 */
unsigned char touchSetT51Bytes5(unsigned char *data, char panel)
{
	unsigned char result;

	if (!touch.panel[panel].features.bits.t51Settings)
		result = 2;
	else
		result = touchWrite(touch.panel[panel].objects.t51 + 5, data, 2, panel);
	return result;
}

unsigned char touchSetT51Bytes9(unsigned char *data, char panel)
{
	unsigned char result;

	if (!touch.panel[panel].features.bits.t51Settings)
		result = 2;
	else
		result = touchWrite(touch.panel[panel].objects.t51 + 9, data, 2, panel);
	return result;
}

/* T18's byte 1; 2 for a controller without T18 */
unsigned char touchSetComms(unsigned char value, char panel)
{
	unsigned char result = 0;

	if (touch.panel[panel].objects.comms != 0)
		result = touchWrite(touch.panel[panel].objects.comms + 1, &value, 1, panel);
	else
		result = 2;
	return result;
}

/* *set 1 when T34's byte 7 has bit 3 or the controller is in its bootloader, else 0 */
unsigned char touchReadT34Bit3(unsigned char *set, char panel)
{
	unsigned char result = 0;
	unsigned char value;
#if TOUCH_0105
	unsigned char values[6];

	if (panel == 1 && touch.panel[1].controller == 2) {
		result = touchRearRead(0, values, 1);
		if (result == 0)
			*set = values[0];
		return result;
	}
	if (touch.panel[1].controller == 3) {
		*set = 1;
		return 0;
	}
#endif

	*set = 1;
	if ((touch.panel[panel].family & 0xc0) != 0xc0) {
		result = touchRead(touch.panel[panel].objects.t34 + 7, &value, 1, panel);
		if (result == 0)
			*set = (value & 0x08) ? 1 : 0;
	}
	return result;
}

/* T34's bytes 8-10 */
#if TOUCH_0105
unsigned char touchReadT34Bytes8(unsigned char *data, char panel)
{
	unsigned char result = 0;
	unsigned char values[7];

	if (touch.panel[panel].controller == 0)
		result = touchRead(touch.panel[panel].objects.t34 + 8, data, 3, panel);
	else if (touch.panel[panel].controller == 2) {
		result = touchRearRead(0, values, panel);
		if (values[0] == 0) {
			memcpy(data, &values[3], 2);
			data[2] = 0;
		} else
			result = 0x0b;
	} else
		result = 0x3e;
	return result;
}
#else
unsigned char touchReadT34Bytes8(unsigned char *data, char panel)
{
	unsigned char result = 0;

	result = touchRead(touch.panel[panel].objects.t34 + 8, data, 3, panel);
	return result;
}
#endif

/* T6's byte 4 */
unsigned char touchSetT6Byte4(unsigned char *data, char panel)
{
	unsigned char result = 0;

	result = touchWrite(touch.panel[panel].objects.reset + 4, data, 1, panel);
	return result;
}
#elif SOURCE_0103
/* reads the user data's configuration version into data (2 bytes), and the modes when it
   changed or their last read failed; a controller in its bootloader gives its version as
   bytes 4-5 of its 6-byte answer */
unsigned char touchReadConfigVersion0(unsigned char *data)
{
	unsigned char result;
	unsigned char status;
	unsigned int version;

	if ((touch.panel[0].family & 0xc0) == 0xc0) {
		result = touchRead(TOUCH_BOOTLOADER, touchBufferData, 6, 0);
		if (result == 0) {
			data[0] = touchBufferData[4];
			data[1] = touchBufferData[5];
		}
	} else
		result = touchRead(touch.panel[0].objects.userData, data, 2, 0);
	if (result == 0) {
		version = (unsigned int)data[0] << 8 | data[1];
		if (touch.panel[0].configVersion != version || touch.panel[0].modesUnread) {
			touch.panel[0].configVersion = version;
			result = touchReadModes0();
		}
	}
	return result;
}

/* the rear controller does not answer (0x11) in its bootloader, which is asked instead */
unsigned char touchReadConfigVersion1(unsigned char *data)
{
	unsigned char result;
	unsigned char status;
	unsigned int version;

	result = touchRead(touch.panel[1].objects.userData, data, 2, 1);
	if (result == 0) {
		version = (unsigned int)data[0] << 8 | data[1];
		if (touch.panel[1].configVersion != version || touch.panel[1].modesUnread) {
			touch.panel[1].configVersion = version;
			result = touchReadModes1();
		}
	}
#if TOUCH_PANEL1_IICA1
	else if (result == 0x11) {
		result = touchBootloaderStatus1(&status);
		if (result == 0) {
			result = touchBootloaderRead(touchBufferData, 6, 1);
			if (result == 0) {
				data[0] = touchBufferData[4];
				data[1] = touchBufferData[5];
				version = (unsigned int)data[0] << 8 | data[1];
				if (touch.panel[1].configVersion != version)
					touch.panel[1].configVersion = version;
			}
		}
	}
#endif
	return result;
}
#else
unsigned char touchReadConfigVersion0(unsigned char *data)
{
#if FW_SINCE(0x01000104)
	unsigned char result;
	unsigned int version;
#endif

	if (touch.panel[0].version < 0x24) {
		memset(data, 0, 2);
		return 0;
	}
#if FW_SINCE(0x01000104)
	result = touchRead(touch.panel[0].objects.userData, data, 2, 0);
	version = (unsigned int)data[0] << 8 | data[1];
	if (touch.panel[0].configVersion != version) {
#if TOUCH_MODES_BEFORE_VERSION
		touchReadModes0();
#endif
		touch.panel[0].configVersion = version;
#if FW_SINCE(0x01000109)
		touchReadModes0();
#endif
	}
	return result;
#else
	/* before 0x01000104 the version is not kept */
	return touchRead(touch.panel[0].objects.userData, data, 2, 0);
#endif
}

unsigned char touchReadConfigVersion1(unsigned char *data)
{
#if FW_SINCE(0x01000104)
	unsigned char result;
	unsigned int version;
#endif

	if (touch.panel[1].version < 0x24) {
		memset(data, 0, 2);
		return 0;
	}
#if FW_SINCE(0x01000104)
	result = touchRead(touch.panel[1].objects.userData, data, 2, 1);
	version = (unsigned int)data[0] << 8 | data[1];
	if (touch.panel[1].configVersion != version) {
#if TOUCH_MODES_BEFORE_VERSION
		touchReadModes1();
#endif
		touch.panel[1].configVersion = version;
#if FW_SINCE(0x01000109)
		touchReadModes1();
#endif
	}
	return result;
#else
	/* before 0x01000104 the version is not kept */
	return touchRead(touch.panel[1].objects.userData, data, 2, 1);
#endif
}
#endif

#if SOURCE_0103 && !SOURCE_0104
/* the user data's bytes 12-19 */
unsigned char touchReadUserData0(unsigned char *data)
{
	return touchRead(touch.panel[0].objects.userData + 0x0c, data, 8, 0);
}

unsigned char touchReadUserData1(unsigned char *data)
{
	return touchRead(touch.panel[1].objects.userData + 0x0c, data, 8, 1);
}

/* runs a self test (T25); *pass 0 if it passed, else 0xff; the result's address in T34
   by the controller's family and variant */
unsigned char touchSelfTest0(unsigned char test, unsigned char *pass)
{
	unsigned int address;
	unsigned char command[2];
	unsigned char value;
	unsigned char result;

	command[0] = 3;
	command[1] = test;
	touchWrite(touch.panel[0].objects.selfTest, command, 2, 0);
	dly_tsk(8);
	if (touch.panel[0].family == 0x82 && touch.panel[0].variant == 0x2b)
		address = touch.panel[0].objects.t34 + 0x5b;
	else if (touch.panel[0].family == 0x82 && touch.panel[0].variant == 0x1a)
		address = touch.panel[0].objects.t34 + 0x54;
	else
		address = touch.panel[0].objects.t34 + 0x62;
	result = touchRead(address, &value, 1, 0);
	if (result == 0) {
		if (value == 0xfe)
			*pass = 0;
		else
			*pass = 0xff;
	} else
		*pass = 0xff;
	return result;
}

unsigned char touchSelfTest1(unsigned char test, unsigned char *pass)
{
	unsigned int address;
	unsigned char command[2];
	unsigned char value;
	unsigned char result;

	command[0] = 3;
	command[1] = test;
	touchWrite(touch.panel[1].objects.selfTest, command, 2, 1);
	dly_tsk(8);
	if (touch.panel[1].family == 0x82 && touch.panel[1].variant == 0x27)
		address = touch.panel[1].objects.t34 + 0x31;
	else
		address = touch.panel[1].objects.t34 + 0x62;
	result = touchRead(address, &value, 1, 1);
	if (result == 0) {
		if (value == 0xfe)
			*pass = 0;
		else
			*pass = 0xff;
	} else
		*pass = 0xff;
	return result;
}
/* T51's byte 4; 2 for family 0x80 variant 0x0a before version 0x31, which have none */
unsigned char touchSetT51Byte4_0(unsigned char value)
{
	unsigned char byte;
	unsigned char result;

	if (touch.panel[0].family == 0x80 && touch.panel[0].variant == 0x0a &&
	    touch.panel[0].version < 0x31)
		result = 2;
	else {
		byte = value;
		result = touchWrite(touch.panel[0].objects.t51 + 4, &byte, 1, 0);
	}
	return result;
}

unsigned char touchSetT51Byte4_1(unsigned char value)
{
	unsigned char byte;
	unsigned char result;

	if (touch.panel[1].family == 0x80 && touch.panel[1].variant == 0x0a &&
	    touch.panel[1].version < 0x31)
		result = 2;
	else {
		byte = value;
		result = touchWrite(touch.panel[1].objects.t51 + 4, &byte, 1, 1);
	}
	return result;
}

/* T34's 7 bytes from 0x70 (0x69 for family 0x82 variant 0x1a; the rear's 0x82 0x27 from 0x46) */
unsigned char touchReadT34Bytes0(unsigned char *data)
{
	unsigned int address;
	unsigned char result;

	if (touch.panel[0].family == 0x82 && touch.panel[0].variant == 0x2b)
		address = touch.panel[0].objects.t34 + 0x70;
	else if (touch.panel[0].family == 0x82 && touch.panel[0].variant == 0x1a)
		address = touch.panel[0].objects.t34 + 0x69;
	else
		address = touch.panel[0].objects.t34 + 0x70;
	result = touchRead(address, data, 7, 0);
	return result;
}

unsigned char touchReadT34Bytes1(unsigned char *data)
{
	unsigned int address;
	unsigned char result;

	if (touch.panel[1].family == 0x82 && touch.panel[1].variant == 0x27)
		address = touch.panel[1].objects.t34 + 0x46;
	else
		address = touch.panel[1].objects.t34 + 0x70;
	result = touchRead(address, data, 7, 1);
	return result;
}

/* T51's bytes 5-6, and 9-10, as touchSetT51Byte4_0 */
unsigned char touchSetT51Bytes5_0(unsigned char *data)
{
	unsigned char result;

	if (touch.panel[0].family == 0x80 && touch.panel[0].variant == 0x0a &&
	    touch.panel[0].version < 0x31)
		result = 2;
	else
		result = touchWrite(touch.panel[0].objects.t51 + 5, data, 2, 0);
	return result;
}

unsigned char touchSetT51Bytes5_1(unsigned char *data)
{
	unsigned char result;

	if (touch.panel[1].family == 0x80 && touch.panel[1].variant == 0x0a &&
	    touch.panel[1].version < 0x31)
		result = 2;
	else
		result = touchWrite(touch.panel[1].objects.t51 + 5, data, 2, 1);
	return result;
}

unsigned char touchSetT51Bytes9_0(unsigned char *data)
{
	unsigned char result;

	if (touch.panel[0].family == 0x80 && touch.panel[0].variant == 0x0a &&
	    touch.panel[0].version < 0x31)
		result = 2;
	else
		result = touchWrite(touch.panel[0].objects.t51 + 9, data, 2, 0);
	return result;
}

unsigned char touchSetT51Bytes9_1(unsigned char *data)
{
	unsigned char result;

	if (touch.panel[1].family == 0x80 && touch.panel[1].variant == 0x0a &&
	    touch.panel[1].version < 0x31)
		result = 2;
	else
		result = touchWrite(touch.panel[1].objects.t51 + 9, data, 2, 1);
	return result;
}

/* T18's byte 1 */
unsigned char touchSetComms(unsigned char value, unsigned char panel)
{
	unsigned char result = 0;
	unsigned int address;

	if (panel == 0)
		address = touch.panel[0].objects.comms + 1;
	else
		address = touch.panel[1].objects.comms + 1;
	result = touchWrite(address, &value, 1, panel);
	return result;
}

/* *set 1 when T34's byte 7 has bit 3 (or the rear controller is in its bootloader), else 0 */
unsigned char touchReadT34Bit3(unsigned char *set, unsigned char panel)
{
	unsigned char result;
	unsigned char value;
	unsigned int address;

	if (panel == 0)
		address = touch.panel[0].objects.t34;
	else
		address = touch.panel[1].objects.t34;
	address += 7;
	result = touchRead(address, &value, 1, panel);
	if (result == 0)
		*set = (value & 0x08) ? 1 : 0;
	else if (result == 0x11) {
		result = touchBootloaderStatus1(&value);
		if (result == 0)
			*set = 1;
	} else
		*set = 0;
	return result;
}
#elif !SOURCE_0103
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
#if FW_SINCE(0x00090903)
	unsigned int address;
#endif

#if FW_SINCE(0x00090903)
	if (touch.panel[0].version < 0x21) {
#else
	if (touch.panel[0].version < 0x24) {
#endif
		*pass = 0xfe;
		return 0;
	}
	command[0] = 3;
	command[1] = test;
	touchWrite(touch.panel[0].objects.selfTest, command, 2, 0);
	dly_tsk(8);
#if FW_SINCE(0x00090903)
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
#else
	result = touchRead(touch.panel[0].objects.t34 + 0x62, &value, 1, 0);
#endif
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
#if FW_SINCE(0x00090903)
	unsigned int address;
#endif

#if FW_SINCE(0x00090903)
	if (touch.panel[1].version < 0x21) {
#else
	if (touch.panel[0].version < 0x24) {	/* (0x00090804's: panel 0's version) */
#endif
		*pass = 0xfe;
		return 0;
	}
	command[0] = 3;
	command[1] = test;
	touchWrite(touch.panel[1].objects.selfTest, command, 2, 1);
	dly_tsk(8);
#if FW_SINCE(0x00090903)
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
#else
	result = touchRead(touch.panel[1].objects.t34 + 0x62, &value, 1, 1);
#endif
	if (value == 0xfe)
		*pass = 0;
	else
		*pass = 0xff;
	return result;
}
#endif
#endif

#if SOURCE_0104
/* count NOPs (the link's delays) */
void touchDelay(unsigned char count)
{
	unsigned char i;

	for (i = 0; i < count; i++)
		NOP();
}
#else
/* after a chip select */
void touchDelaySelect(void)
{
	unsigned char i;

	for (i = 0; i < TOUCH_DELAY_SELECT; i++)
		NOP();
}
#endif

#if SOURCE_0104
/* reads a controller's object table into the panel's objects: the addresses of the objects
   the module uses; 0x40 when the table lacks one the controller's family and variant (and
   version) should have */
unsigned char touchReadObjects(struct TouchObjects *objects, unsigned char count,
			       char panel)
{
	unsigned char result;
	unsigned char i;
	unsigned int address;
	unsigned int object;
	unsigned int found;
	struct TouchObject *table;

	found = 0;
	address = TOUCH_INFO_SIZE;
	table = (struct TouchObject *)touchBufferData;
	result = touchRead(address, (unsigned char *)table, (unsigned char)(count * 6), panel);
	if (result == 0) {
		memset(&touch.panel[panel].objects, 0, sizeof(touch.panel[panel].objects));
		for (i = 0; i < count; i++) {
			object = TOUCH_OBJECT_ADDRESS(table[i]);
			if (object >= TOUCH_INFO_SIZE && object < 0x801) {
				switch (table[i].type) {
				case 6:
					found |= TOUCH_FOUND_RESET;
					objects->reset = object;
					break;
				case 7:
					found |= TOUCH_FOUND_POWER;
					objects->power = object;
					break;
				case 9:
					found |= TOUCH_FOUND_TOUCH;
					objects->numTouch = object + 0x0e;
					objects->touchScreen = object;
					objects->threshold = object + 7;
					break;
				case 18:
					found |= TOUCH_FOUND_COMMS;
					objects->comms = object;
					break;
				case 20:
					found |= TOUCH_FOUND_GRIP;
					objects->grip = object;
					break;
				case 22:
					found |= TOUCH_FOUND_NOISE;
					objects->noise = object;
					break;
				case 25:
					found |= TOUCH_FOUND_SELF_TEST;
					objects->selfTest = object;
					break;
				case 28:
					found |= TOUCH_FOUND_CTE;
					objects->cte = object;
					break;
				case 34:
					found |= TOUCH_FOUND_REPORT;
					objects->report = object + 0x0e;
					objects->t34 = object;
					break;
				case 38:
					found |= TOUCH_FOUND_USER_DATA;
					objects->userData = object;
					break;
				case 51:
					found |= TOUCH_FOUND_T51;
					objects->t51 = object;
					break;
				case 60:
					found |= TOUCH_FOUND_T60;
					objects->t60 = object;
					break;
				case 100:
					found |= TOUCH_FOUND_T100;
					objects->numTouch = object + 6;
					objects->touchScreen = object;
					objects->threshold = object + 0x1e;
					break;
				}
			}
		}
	}
	/* the report's address doubles but for 0x8227 */
	switch (TOUCH_PANEL_ID(panel)) {
	case TOUCH_ID(0x80, 0x0a):
		objects->report *= 2;
		if (found != TOUCH_FOUND_ALL)
			result = 0x40;
		break;
	case TOUCH_ID(0x82, 0x27):
		if (found != TOUCH_FOUND_REAR)
			result = 0x40;
		break;
	case TOUCH_ID(0x82, 0x34):
		objects->report *= 2;
		if (touch.panel[panel].version < 0x11) {
			if (found != TOUCH_FOUND_T100_OLD)
				result = 0x40;
		} else if (found != TOUCH_FOUND_T100_ALL)
			result = 0x40;
		break;
	case TOUCH_ID(0x82, 0x3a):
		objects->report *= 2;
		if (touch.panel[panel].version == 9 && touch.panel[panel].build == 0) {
			if (found != TOUCH_FOUND_REAR)
				result = 0x40;
		} else if (found != TOUCH_FOUND_REAR_T60)
			result = 0x40;
		break;
	case TOUCH_ID(0x82, 0x2b):
	default:
		objects->report *= 2;
		if (found != TOUCH_FOUND_REAR)
			result = 0x40;
		break;
	}
	return result;
}
#elif SOURCE_0103
/* reads a controller's object table: the addresses of the objects the module uses; 4 when the
   table lacks one the controller's family and variant should have */
unsigned char touchReadObjects(struct TouchObjects *objects, unsigned char count,
			       unsigned char panel)
{
	unsigned char result;
	unsigned char i;
	unsigned int address;
	unsigned int object;
	unsigned int found;
	struct TouchObject *table;
	unsigned int kind;

	found = 0;
	address = TOUCH_INFO_SIZE;
	table = (struct TouchObject *)touchBufferData;
	result = touchRead(address, (unsigned char *)table, (unsigned char)(count * 6), panel);
	if (result == 0) {
		for (i = 0; i < count; i++) {
			object = TOUCH_OBJECT_ADDRESS(table[i]);
			if (object >= TOUCH_INFO_SIZE && object < 0x801) {
				switch (table[i].type) {
				case 6:
					found |= TOUCH_FOUND_RESET;
					objects->reset = object;
					objects->backup = object + 1;
					break;
				case 7:
					found |= TOUCH_FOUND_POWER;
					objects->power = object;
					break;
				case 9:
					found |= TOUCH_FOUND_TOUCH;
					objects->numTouch = object + 0x0e;
					objects->touchScreen = object;
					break;
				case 18:
					found |= TOUCH_FOUND_COMMS;
					objects->comms = object;
					break;
				case 20:
					found |= TOUCH_FOUND_GRIP;
					objects->grip = object;
					break;
				case 22:
					found |= TOUCH_FOUND_NOISE;
					objects->noise = object;
					break;
				case 25:
					found |= TOUCH_FOUND_SELF_TEST;
					objects->selfTest = object;
					break;
				case 28:
					found |= TOUCH_FOUND_CTE;
					objects->cte = object;
					break;
				case 34:
					found |= TOUCH_FOUND_REPORT;
					objects->report = object + 0x0e;
					objects->t34 = object;
					break;
				case 38:
					found |= TOUCH_FOUND_USER_DATA;
					objects->userData = object;
					break;
				case 51:
					found |= TOUCH_FOUND_T51;
					objects->t51 = object;
					break;
				}
			}
		}
	}
	if (panel == 0)
		kind = (unsigned int)touch.panel[0].family << 8 | touch.panel[0].variant;
	else
		kind = (unsigned int)touch.panel[1].family << 8 | touch.panel[1].variant;
	/* the report's address doubles but for 0x8227 */
	switch (kind) {
	case 0x800a:
		objects->report *= 2;
		if (found != TOUCH_FOUND_ALL)
			result = 4;
		break;
	case 0x8227:
		if (found != TOUCH_FOUND_REAR)
			result = 4;
		break;
	case 0x821a:
		objects->report *= 2;
		if (found != TOUCH_FOUND_REAR)
			result = 4;
		break;
	case 0x822b:
	default:
		objects->report *= 2;
		if (found != TOUCH_FOUND_REAR)
			result = 4;
		break;
	}
	return result;
}
#elif FW_SINCE(0x00090009)	/* 0x00080504 reads no object table */
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
#if FW_SINCE(0x00090804)
				objects->touchScreen = TOUCH_OBJECT_ADDRESS(table[i]);
#endif
				break;
#if FW_SINCE(0x00090903)
			case 20:
				objects->grip = TOUCH_OBJECT_ADDRESS(table[i]);
				break;
#endif
#if FW_SINCE(0x00090804)
			case 22:
				objects->noise = TOUCH_OBJECT_ADDRESS(table[i]);
				break;
			case 25:
				objects->selfTest = TOUCH_OBJECT_ADDRESS(table[i]);
				break;
#endif
#if FW_SINCE(0x01000109)
			case 28:
				objects->cte = TOUCH_OBJECT_ADDRESS(table[i]);
				break;
#endif
			case 34:
				objects->report = TOUCH_OBJECT_ADDRESS(table[i]) + 0x0e;
#if FW_SINCE(0x00090903)
				objects->status = TOUCH_OBJECT_ADDRESS(table[i]) + 0x54;
				objects->report *= 2;
				objects->status *= 2;
#else
				objects->report *= 2;
#endif
#if FW_SINCE(0x00090804)
				objects->t34 = TOUCH_OBJECT_ADDRESS(table[i]);
#endif
				break;
#if FW_SINCE(0x00090804)
			case 38:
				objects->userData = TOUCH_OBJECT_ADDRESS(table[i]);
				break;
			case 51:
				objects->t51 = TOUCH_OBJECT_ADDRESS(table[i]);
				break;
#endif
			}
		}
	}
}
#endif

#if FW_SINCE(0x00090804)	/* 0x00090206 has no modes, user data or self tests */
#if !SOURCE_0103 || BOARD_DOL1001	/* USS-1001's modes follow touchSetModePin */
/* writes mode's settings to both controllers (DOL-1001: to each of family 0x80 variant 0x0a) */
void touchSetMode(unsigned char mode)
{
	unsigned int address;

#if SOURCE_0104
	if (!touch.panel[0].features.bits.noModes) {
#elif BOARD_DOL1001
	if (touch.panel[0].family == 0x80 && touch.panel[0].variant == 0x0a) {
#else
	if (touch.panel[0].version >= 0x24) {
#endif
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
#if FW_SINCE(0x01000109)
		if ((touch.panel[0].configVersion & 0x7fff) >= 0x0d00) {
			address = touch.panel[0].objects.noise + 0x0c;
			touchWriteVerified(address, touch.panel[0].modes[mode].frequency, 4, 0);
		}
		if ((touch.panel[0].configVersion & 0x7fff) >= 0x0d01) {
			address = touch.panel[0].objects.cte + 4;
			touchWriteVerified(address, &touch.panel[0].modes[mode].gcafDepth, 1, 0);
		}
#endif
	}
#if SOURCE_0104
	if (!touch.panel[1].features.bits.noModes) {
		address = touch.panel[1].objects.t51 + 3;
#elif BOARD_DOL1001
	if (touch.panel[1].family == 0x80 && touch.panel[1].variant == 0x0a) {
		address = touch.panel[1].objects.t51 + 3;
#elif FW_SINCE(0x01000109)
	if (touch.panel[1].version >= 0x24) {
		address = touch.panel[1].objects.t51 + 3;
#else
	/* panel 0's version decides for panel 1 too, and panel 0's T51 is where panel 1's goes */
	if (touch.panel[0].version >= 0x24) {
		address = touch.panel[0].objects.t51 + 3;
#endif
		touchWriteVerified(address, &touch.panel[1].modes[mode].t51, 1, 1);
		address = touch.panel[1].objects.touchScreen + 7;
		touchWriteVerified(address, &touch.panel[1].modes[mode].threshold, 1, 1);
		address = touch.panel[1].objects.touchScreen + 0x1f;
		touchWriteVerified(address, &touch.panel[1].modes[mode].hysteresis, 1, 1);
		address = touch.panel[1].objects.touchScreen + 0x10;
		touchWriteVerified(address, &touch.panel[1].modes[mode].mergeThreshold, 1, 1);
		address = touch.panel[1].objects.noise + 8;
		touchWriteVerified(address, &touch.panel[1].modes[mode].noiseThreshold, 1, 1);
#if FW_SINCE(0x01000109)
		if ((touch.panel[1].configVersion & 0x7fff) >= 0x0d00) {
			address = touch.panel[1].objects.noise + 0x0c;
			touchWriteVerified(address, touch.panel[1].modes[mode].frequency, 4, 1);
		}
		if ((touch.panel[1].configVersion & 0x7fff) >= 0x0d01) {
			address = touch.panel[1].objects.cte + 4;
			touchWriteVerified(address, &touch.panel[1].modes[mode].gcafDepth, 1, 1);
		}
#endif
	}
}
#endif

#if SOURCE_0103 && !BOARD_DOL1001
/* the mode pin (the slim's P3.2) low in modes 1 and 2, high in mode 0 */
void touchSetModePin(unsigned char mode)
{
	switch (mode) {
	case 1:
	case 2:
		TOUCH_MODE_PIN = 0;
		break;
	case 0:
	default:
		TOUCH_MODE_PIN = 1;
		break;
	}
}

#endif
/* modes 1 and 2 from panel 0's user data, mode 0 from its objects */
#if TOUCH_0105
/* the slim's 0x0105 reads no modes */
unsigned char touchReadModes(char panel)
{
	return 0;
}
#elif SOURCE_0104
/* reads a controller's modes, but for one without modes: the last error */
unsigned char touchReadModes(char panel)
{
	unsigned char result;
	unsigned char failed;
	unsigned int address;
	unsigned char data[TOUCH_MODE_SIZE];

	failed = 0;
	touch.panel[panel].modesUnread = 0;
	if (!touch.panel[panel].features.bits.noModes) {
		address = touch.panel[panel].objects.userData + 2;
		result = touchRead(address, data, sizeof(data), panel);
		if (result == 0) {
			memcpy(&touch.panel[panel].modes[2], data, sizeof(data));
			memcpy(&touch.panel[panel].modes[1], data, sizeof(data) - 1);
		} else
			failed = result;
		address = touch.panel[panel].objects.t51 + 3;
		result = touchRead(address, data, 1, panel);
		if (result == 0)
			touch.panel[panel].modes[0].t51 = data[0];
		else
			failed = result;
		address = touch.panel[panel].objects.touchScreen + 7;
		result = touchRead(address, data, 1, panel);
		if (result == 0)
			touch.panel[panel].modes[0].threshold = data[0];
		else
			failed = result;
		address = touch.panel[panel].objects.touchScreen + 0x1f;
		result = touchRead(address, data, 1, panel);
		if (result == 0)
			touch.panel[panel].modes[0].hysteresis = data[0];
		else
			failed = result;
		address = touch.panel[panel].objects.touchScreen + 0x10;
		result = touchRead(address, data, 1, panel);
		if (result == 0)
			touch.panel[panel].modes[0].mergeThreshold = data[0];
		else
			failed = result;
		address = touch.panel[panel].objects.noise + 8;
		result = touchRead(address, data, 1, panel);
		if (result == 0)
			touch.panel[panel].modes[0].noiseThreshold = data[0];
		else
			failed = result;
		address = touch.panel[panel].objects.noise + 0x0c;
		result = touchRead(address, data, 4, panel);
		if (result == 0)
			memcpy(touch.panel[panel].modes[0].frequency, data, 4);
		else
			failed = result;
		address = touch.panel[panel].objects.cte + 4;
		result = touchRead(address, data, 1, panel);
		if (result == 0) {
			touch.panel[panel].modes[0].gcafDepth = data[0];
			touch.panel[panel].modes[1].gcafDepth = data[0];
		} else
			failed = result;
		if (failed != 0)
			touch.panel[panel].modesUnread = 1;
	}
	return failed;
}
#elif SOURCE_0103
/* reads the controller's modes, family 0x80 variant 0x0a only: the last error */
unsigned char touchReadModes0(void)
{
	unsigned char result;
	unsigned char failed;
	unsigned int address;
	unsigned char data[TOUCH_MODE_SIZE];

	failed = 0;
	touch.panel[0].modesUnread = 0;
	if (touch.panel[0].family == 0x80 && touch.panel[0].variant == 0x0a) {
		address = touch.panel[0].objects.userData + 2;
		result = touchRead(address, data, sizeof(data), 0);
		if (result == 0) {
			memcpy(&touch.panel[0].modes[2], data, sizeof(data));
			memcpy(&touch.panel[0].modes[1], data, sizeof(data) - 1);
		} else
			failed = result;
		address = touch.panel[0].objects.t51 + 3;
		result = touchRead(address, data, 1, 0);
		if (result == 0)
			touch.panel[0].modes[0].t51 = data[0];
		else
			failed = result;
		address = touch.panel[0].objects.touchScreen + 7;
		result = touchRead(address, data, 1, 0);
		if (result == 0)
			touch.panel[0].modes[0].threshold = data[0];
		else
			failed = result;
		address = touch.panel[0].objects.touchScreen + 0x1f;
		result = touchRead(address, data, 1, 0);
		if (result == 0)
			touch.panel[0].modes[0].hysteresis = data[0];
		else
			failed = result;
		address = touch.panel[0].objects.touchScreen + 0x10;
		result = touchRead(address, data, 1, 0);
		if (result == 0)
			touch.panel[0].modes[0].mergeThreshold = data[0];
		else
			failed = result;
		address = touch.panel[0].objects.noise + 8;
		result = touchRead(address, data, 1, 0);
		if (result == 0)
			touch.panel[0].modes[0].noiseThreshold = data[0];
		else
			failed = result;
		address = touch.panel[0].objects.noise + 0x0c;
		result = touchRead(address, data, 4, 0);
		if (result == 0)
			memcpy(touch.panel[0].modes[0].frequency, data, 4);
		else
			failed = result;
		address = touch.panel[0].objects.cte + 4;
		result = touchRead(address, data, 1, 0);
		if (result == 0) {
			touch.panel[0].modes[0].gcafDepth = data[0];
			touch.panel[0].modes[1].gcafDepth = data[0];
		} else
			failed = result;
		if (failed != 0)
			touch.panel[0].modesUnread = 1;
	}
	return failed;
}

unsigned char touchReadModes1(void)
{
	unsigned char result;
	unsigned char failed;
	unsigned int address;
	unsigned char data[TOUCH_MODE_SIZE];

	failed = 0;
	touch.panel[1].modesUnread = 0;
	if (touch.panel[1].family == 0x80 && touch.panel[1].variant == 0x0a) {
		address = touch.panel[1].objects.userData + 2;
		result = touchRead(address, data, sizeof(data), 1);
		if (result == 0) {
			memcpy(&touch.panel[1].modes[2], data, sizeof(data));
			memcpy(&touch.panel[1].modes[1], data, sizeof(data) - 1);
		} else
			failed = result;
		address = touch.panel[1].objects.t51 + 3;
		result = touchRead(address, data, 1, 1);
		if (result == 0)
			touch.panel[1].modes[0].t51 = data[0];
		else
			failed = result;
		address = touch.panel[1].objects.touchScreen + 7;
		result = touchRead(address, data, 1, 1);
		if (result == 0)
			touch.panel[1].modes[0].threshold = data[0];
		else
			failed = result;
		address = touch.panel[1].objects.touchScreen + 0x1f;
		result = touchRead(address, data, 1, 1);
		if (result == 0)
			touch.panel[1].modes[0].hysteresis = data[0];
		else
			failed = result;
		address = touch.panel[1].objects.touchScreen + 0x10;
		result = touchRead(address, data, 1, 1);
		if (result == 0)
			touch.panel[1].modes[0].mergeThreshold = data[0];
		else
			failed = result;
		address = touch.panel[1].objects.noise + 8;
		result = touchRead(address, data, 1, 1);
		if (result == 0)
			touch.panel[1].modes[0].noiseThreshold = data[0];
		else
			failed = result;
		address = touch.panel[1].objects.noise + 0x0c;
		result = touchRead(address, data, 4, 1);
		if (result == 0)
			memcpy(touch.panel[1].modes[0].frequency, data, 4);
		else
			failed = result;
		address = touch.panel[1].objects.cte + 4;
		result = touchRead(address, data, 1, 1);
		if (result == 0) {
			touch.panel[1].modes[0].gcafDepth = data[0];
			touch.panel[1].modes[1].gcafDepth = data[0];
		} else
			failed = result;
		if (failed != 0)
			touch.panel[1].modesUnread = 1;
	}
	return failed;
}
#else
void touchReadModes0(void)
{
	int result;
	unsigned int address;
	unsigned char data[TOUCH_MODE_SIZE];

	if (touch.panel[0].version >= 0x24) {
		address = touch.panel[0].objects.userData + 2;
		result = touchRead(address, data, sizeof(data), 0);
		if (result == 0) {
#if FW_SINCE(0x01000109)
			memcpy(&touch.panel[0].modes[2], data, sizeof(data));
			memcpy(&touch.panel[0].modes[1], data, sizeof(data) - 1);
#else
			memcpy(&touch.panel[0].modes[1], data, sizeof(data));
#endif
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
#if FW_SINCE(0x01000109)
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
#endif
	}
}

void touchReadModes1(void)
{
	int result;
	unsigned int address;
	unsigned char data[TOUCH_MODE_SIZE];

	if (touch.panel[1].version >= 0x24) {
		address = touch.panel[1].objects.userData + 2;
		result = touchRead(address, data, sizeof(data), 1);
		if (result == 0) {
#if FW_SINCE(0x01000109)
			memcpy(&touch.panel[1].modes[2], data, sizeof(data));
			memcpy(&touch.panel[1].modes[1], data, sizeof(data) - 1);
#else
			memcpy(&touch.panel[1].modes[1], data, sizeof(data));
#endif
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
#if FW_SINCE(0x01000109)
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
#endif
	}
}
#endif

/* the state the frames carry; mode 1 from now on */
#if SOURCE_0103
void touchClearMode(void)
{
	touch.state.bits.mode = 0;
#if BOARD_DOL1001
	touchSetMode(0);
#else
	touchSetModePin(0);
#endif
}

#endif
void touchUpdateState(void)
{
#if BOARD_TOUCH_CHARGE || SOURCE_0103 || FW_SINCE(0x01000109)
	unsigned char mode;
#endif

	if (touch.flags & (TOUCH_PANEL0 | TOUCH_PANEL1)) {
#if BOARD_TOUCH_CHARGE || SOURCE_0103
		touch.state.bits.power = chargeGetAc() | chargeGetCharging();
#elif FW_SINCE(0x01000104)
		touch.state.bits.power = 1;
#else
		touch.state.bits.power = input1Bit7Active();	/* before 0x01000104 input 1.7 */
#endif
		touch.state.bits.input3 = input1Bit3Active();
		touch.state.bits.input5 = input1Bit5Active();
		touch.state.bits.input0 = input1Bit0Active();
#if BOARD_TOUCH_CHARGE || SOURCE_0103
		if (touch.state.value & 0xf0) {
			if (chargeUsbIs1() || chargeUsbIs2())
				mode = 2;
			else
				mode = 1;
		} else
			mode = 0;
#elif FW_SINCE(0x01000109)
		mode = 1;
#endif
#if FW_SINCE(0x01000109)
		if (touch.state.bits.mode != mode) {
#if SOURCE_0103 && !BOARD_DOL1001
			touchSetModePin(mode);	/* the controllers' modes follow the pin */
#else
			touchSetMode(mode);
#endif
			touch.state.bits.mode = mode;
		}
#else
		/* mode 1 while any of the bits is set (power always is), else mode 0 */
		if (touch.state.value & 0xf0) {
			if (!touch.state.bits.mode) {
				touchSetMode(1);
				touch.state.bits.mode = 1;
			}
		} else if (touch.state.bits.mode) {
			touchSetMode(0);
			touch.state.bits.mode = 0;
		}
#endif
	}
}
#endif

#if FW_SINCE(0x00090903) && !SOURCE_0103	/* USS-1001 reads no grip status */
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
#endif

#if SOURCE_0104
/* the grip edges (4 bytes), kept in the panel's gripEdges: the user data's bytes 20-23 where
   the controller keeps them there, else T20's bytes 1-4; a controller in its bootloader gives
   them as bytes 6-9 of its 10-byte answer */
unsigned char touchReadGripEdges(unsigned char *data, char panel)
{
	unsigned char result;
	unsigned char status;
	unsigned int address;

#if TOUCH_0105
	if (panel == 1 &&
	    (touch.panel[1].controller == 2 || touch.panel[1].controller == 3)) {
		result = touchRearIdentify(data, TOUCH_REAR_GRIP, panel);
		return 0;
	}
#endif
	if ((touch.panel[panel].family & 0xc0) == 0xc0) {
		result = touchRead(TOUCH_BOOTLOADER, touchBufferData, 10, panel);
		if (result == 0) {
			memcpy(data, &touchBufferData[6], 4);
			memcpy(touch.panel[panel].gripEdges, &touchBufferData[6], 4);
		}
	} else {
		if (touch.panel[panel].features.bits.gripInUserData)
			address = touch.panel[panel].objects.userData + 0x14;
		else
			address = touch.panel[panel].objects.grip + 1;
		result = touchRead(address, data, 4, panel);
		if (result == 0)
			memcpy(touch.panel[panel].gripEdges, data, 4);
	}
	return result;
}
#elif SOURCE_0103
/* the grip edges (4 bytes), kept in the panel's gripEdges: T20's bytes 1-4 for family 0x80
   variant 0x0a before version 0x30, else the user data's bytes 20-23; a controller in its
   bootloader gives them as bytes 6-9 of its 10-byte answer */
unsigned char touchReadGripEdges0(unsigned char *data)
{
	unsigned char result;
	unsigned char status;
	unsigned int address;

	if ((touch.panel[0].family & 0xc0) == 0xc0) {
		result = touchRead(TOUCH_BOOTLOADER, touchBufferData, 10, 0);
		if (result == 0) {
			memcpy(data, &touchBufferData[6], 4);
			memcpy(touch.panel[0].gripEdges, &touchBufferData[6], 4);
		}
	} else {
		if (touch.panel[0].family == 0x80 && touch.panel[0].variant == 0x0a) {
			if (touch.panel[0].version < 0x30)
				address = touch.panel[0].objects.grip + 1;
			else
				address = touch.panel[0].objects.userData + 0x14;
		} else
			address = touch.panel[0].objects.userData + 0x14;
		result = touchRead(address, data, 4, 0);
		if (result == 0)
			memcpy(touch.panel[0].gripEdges, data, 4);
	}
	return result;
}

/* the rear controller does not answer (0x11) in its bootloader, which is asked instead */
unsigned char touchReadGripEdges1(unsigned char *data)
{
	unsigned char result;
	unsigned char status;
	unsigned int address;

	if (touch.panel[1].family == 0x80 && touch.panel[1].variant == 0x0a) {
		if (touch.panel[1].version < 0x30)
			address = touch.panel[1].objects.grip + 1;
		else
			address = touch.panel[1].objects.userData + 0x14;
	} else
		address = touch.panel[1].objects.userData + 0x14;
	result = touchRead(address, data, 4, 1);
	if (result == 0)
		memcpy(touch.panel[1].gripEdges, data, 4);
#if TOUCH_PANEL1_IICA1
	else if (result == 0x11) {
		result = touchBootloaderStatus1(&status);
		if (result == 0) {
			result = touchBootloaderRead(touchBufferData, 10, 1);
			if (result == 0) {
				memcpy(data, &touchBufferData[6], 4);
				memcpy(touch.panel[1].gripEdges, &touchBufferData[6], 4);
			}
		}
	}
#endif
	return result;
}
#elif FW_SINCE(0x0100060B)
/* T20's bytes 1-4, kept in the panel's gripEdges */
unsigned char touchReadGripEdges0(unsigned char *data)
{
	unsigned char result;

	result = touchRead(touch.panel[0].objects.grip + 1, data, 4, 0);
	if (result == 0)
		memcpy(touch.panel[0].gripEdges, data, 4);
	return result;
}

unsigned char touchReadGripEdges1(unsigned char *data)
{
	unsigned char result;

	result = touchRead(touch.panel[1].objects.grip + 1, data, 4, 1);
	if (result == 0)
		memcpy(touch.panel[1].gripEdges, data, 4);
	return result;
}
#endif

#if FW_SINCE(0x01000306)
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
#endif

#if FW_SINCE(0x0100060B)
/* serial command 0x01a1: the first byte (the report ID) of each touch of the frame being
   filled, panel 0's touches then panel 1's */
void touchGetReportIds(unsigned char *ids)
{
#if FW_SINCE(0x0100060B)
	unsigned char i;
	unsigned char offset;
#else
	unsigned char offset;
	unsigned char i;
#endif

#if FW_SINCE(0x0100060B)
	for (offset = 0, i = 0; i < touch.setting[0]; offset += 6, i++) {
		*ids = touchFrames[touch.frame].panel[0][offset];
		ids++;
	}
	for (offset = 0, i = 0; i < touch.setting[1]; offset += 6, i++) {
		*ids = touchFrames[touch.frame].panel[1][offset];
		ids++;
	}
#else
	for (offset = 0, i = 0; i < touch.setting[0]; i++, offset += 6)
		*ids++ = touchFrames[touch.frame].panel[0][offset];
	for (offset = 0, i = 0; i < touch.setting[1]; i++, offset += 6)
		*ids++ = touchFrames[touch.frame].panel[1][offset];
#endif
}
#endif

#if SOURCE_0103
/*
 * Request 0x038a: whether the mode pin (P3.2) switches each panel's touch threshold (T9's byte 7):
 * with the pin high it is not T38's byte 4, with it low it is, and high again it is back. *out
 * 0, or the first that fails: 0xfd panel 0 high, 0xfc panel 1 high, 0xff panel 0 low, 0xfe panel
 * 1 low. The pin is left as it was.
 */
unsigned char touchTestModePin(unsigned char *out)
{
#if BOARD_DOL1001	/* no mode pin */
	*out = 0;
	return 0;
#else
	unsigned int address;
	unsigned char high0;
	unsigned char low0;
	unsigned char high1;
	unsigned char low1;
	unsigned char value;
	unsigned char pin;
	unsigned char result = 0;

#if TOUCH_0105
	/* only the panels whose features say so (the rear's controllers 2 and 3 have none) */
	*out = 0;
	pin = TOUCH_MODE_PIN;
	TOUCH_MODE_PIN = 1;
	dly_tsk(8);
	if (touch.panel[0].features.bits.noModes) {
		address = touch.panel[0].objects.threshold;
		result = touchRead(address, &high0, 1, 0);
		if (result != 0)
			goto done;
		address = touch.panel[0].objects.userData + 4;
		result = touchRead(address, &low0, 1, 0);
		if (result != 0)
			goto done;
		if (high0 == low0) {
			*out = 0xfd;
			goto done;
		}
	}
	if (touch.panel[1].features.bits.noModes) {
		address = touch.panel[1].objects.threshold;
		result = touchRead(address, &high1, 1, 1);
		if (result != 0)
			goto done;
		address = touch.panel[1].objects.userData + 4;
		result = touchRead(address, &low1, 1, 1);
		if (result != 0)
			goto done;
		if (high1 == low1) {
			*out = 0xfc;
			goto done;
		}
	}

	TOUCH_MODE_PIN = 0;
	dly_tsk(8);
	if (touch.panel[0].features.bits.noModes) {
		address = touch.panel[0].objects.threshold;
		result = touchRead(address, &value, 1, 0);
		if (result != 0)
			goto done;
		if (value != low0) {
			*out = 0xff;
			goto done;
		}
	}
	if (touch.panel[1].features.bits.noModes) {
		address = touch.panel[1].objects.threshold;
		result = touchRead(address, &value, 1, 1);
		if (result != 0)
			goto done;
		if (value != low1) {
			*out = 0xfe;
			goto done;
		}
	}

	TOUCH_MODE_PIN = 1;
	dly_tsk(8);
	if (touch.panel[0].features.bits.noModes) {
		address = touch.panel[0].objects.threshold;
		result = touchRead(address, &value, 1, 0);
		if (result != 0)
			goto done;
		if (value != high0) {
			*out = 0xfd;
			goto done;
		}
	}
	if (touch.panel[1].features.bits.noModes) {
		address = touch.panel[1].objects.threshold;
		result = touchRead(address, &value, 1, 1);
		if (result != 0)
			goto done;
		if (value != high1)
			*out = 0xfc;
	}
#else
	*out = 0;
	pin = TOUCH_MODE_PIN;
	TOUCH_MODE_PIN = 1;
	dly_tsk(8);
	address = touch.panel[0].objects.touchScreen + 7;
	result = touchRead(address, &high0, 1, 0);
	if (result != 0)
		goto done;
	address = touch.panel[0].objects.userData + 4;
	result = touchRead(address, &low0, 1, 0);
	if (result != 0)
		goto done;
	address = touch.panel[1].objects.touchScreen + 7;
	result = touchRead(address, &high1, 1, 1);
	if (result != 0)
		goto done;
	address = touch.panel[1].objects.userData + 4;
	result = touchRead(address, &low1, 1, 1);
	if (result != 0)
		goto done;
	if (high0 == low0) {
		*out = 0xfd;
		goto done;
	}
	if (high1 == low1) {
		*out = 0xfc;
		goto done;
	}

	TOUCH_MODE_PIN = 0;
	dly_tsk(8);
	address = touch.panel[0].objects.touchScreen + 7;
	result = touchRead(address, &value, 1, 0);
	if (result != 0)
		goto done;
	if (value != low0) {
		*out = 0xff;
		goto done;
	}
	address = touch.panel[1].objects.touchScreen + 7;
	result = touchRead(address, &value, 1, 1);
	if (result != 0)
		goto done;
	if (value != low1) {
		*out = 0xfe;
		goto done;
	}

	TOUCH_MODE_PIN = 1;
	dly_tsk(8);
	address = touch.panel[0].objects.touchScreen + 7;
	result = touchRead(address, &value, 1, 0);
	if (result != 0)
		goto done;
	if (value != high0) {
		*out = 0xfd;
		goto done;
	}
	address = touch.panel[1].objects.touchScreen + 7;
	result = touchRead(address, &value, 1, 1);
	if (result != 0)
		goto done;
	if (value != high1)
		*out = 0xfc;
#endif
done:
	TOUCH_MODE_PIN = pin;
	return result;
#endif
}
#endif

#if TOUCH_0105
/* the rear's controllers 2 and 3: writes register 1 and reads 0x20 bytes into data, moving the
   0x1e after the first two to its start */
unsigned char touchRearReadInfo(unsigned char *data, char panel)
{
	unsigned int command;
	unsigned char (*reply)[0x20];
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	reply = (unsigned char (*)[0x20])data;
	command = 1;
	result = touchWriteRead((unsigned char *)&command, 2, *reply, 0x20, panel, 0);
	if (result == 0)
		memmove(data, &(*reply)[2], 0x1e);
	return result;
}

/* the rear's controller 2: writes register 5 with mode (its low two bits) and reads its
   status back into out and the panel's controllerStatus (3 when the read fails); mode 0
   waits 8 ticks */
unsigned char touchRearSetStatus(unsigned char mode, unsigned char *out, char panel)
{
	struct TouchRearShortCommand command;
	unsigned char reply[6];
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	command.address = 5;
	command.data[0] = mode & 3;
	command.data[1] = 8;
	result = touchWriteRead((unsigned char *)&command, 4, reply, 5, panel, 0);
	if (mode == 0)
		dly_tsk(8);
	if (result == 0) {
		*out = reply[3] & 3;
		touch.panel[panel].controllerStatus = *out;
	} else
		touch.panel[panel].controllerStatus = 3;
	return result;
}

/* probes the panel's controller and answers its touchRearControllers value, 0xff for none */
unsigned char touchGetController(unsigned char *out, char panel)
{
	unsigned char info[0x20];
	unsigned char result;
	unsigned char controller;

	result = 0;
	touchProbeController(panel);
	if (touch.panel[panel].controller == 2) {
		result = touchRearReadInfo(info, panel);
		if (result == 0 && info[0] == 0xff)
			touch.panel[panel].controller = 3;
	}
	controller = touch.panel[panel].controller;
	if (controller < 5)
		*out = touchRearControllers[controller].value;
	else
		*out = 0xff;
	return result;
}

/* the rear's controllers 2 and 3: reads the identity (controller 2, when mode needs it or
   none is known yet; controller 3 has 0xc0c0 for none), then copies the part mode names to
   out: TOUCH_REAR_ALL the identity and 8 more bytes, TOUCH_REAR_INFO the information block,
   TOUCH_REAR_VERSION the configuration version, TOUCH_REAR_GRIP the grip edges,
   TOUCH_REAR_EXTRA the 8 bytes; 0x3e for another controller, or a mode controller 3 cannot
   read */
unsigned char touchRearIdentify(unsigned char *out, unsigned char mode, char panel)
{
	struct TouchRearCommand command;
	unsigned char reply[0x18];
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	if (touch.panel[panel].controller == 2) {
		if (mode == TOUCH_REAR_ALL || mode == TOUCH_REAR_EXTRA || mode == TOUCH_REAR_VERSION ||
		    TOUCH_PANEL_ID(panel) == 0)
			goto read;
		goto copy;
	} else if (touch.panel[panel].controller == 3) {
		if (TOUCH_PANEL_ID(panel) == 0) {
			TOUCH_PANEL_ID(panel) = 0xc0c0;
			goto copy;
		} else if (mode == TOUCH_REAR_ALL || mode == TOUCH_REAR_EXTRA) {
			result = 0x3e;
			goto out;
		}
		goto copy;
	} else {
		result = 0x3e;
		goto out;
	}
read:
	command.address = 4;
	command.length = 6;
	command.data[0] = 0x2f;
	command.data[1] = 0;
	command.data[2] = 0x70;
	command.data[3] = 1;
	result = touchWriteRead((unsigned char *)&command, 8, reply, 0x17, panel, 0);
	if (result == 0) {
		touch.panel[1].family = reply[5];
		touch.panel[1].variant = reply[6];
		touch.panel[1].version = reply[7];
		touch.panel[1].build = reply[8];
		touch.panel[1].configVersion = (unsigned int)reply[9] | (unsigned int)reply[10] << 8;
		memcpy(touch.panel[1].gripEdges, &reply[11], 4);
	} else
		goto out;
copy:
	if (result != 0)
		goto out;
	if (out == NULL)
		goto out;
	switch (mode) {
	case TOUCH_REAR_ALL:
		memcpy(out, &touch.panel[1], 10);
		memcpy(out + 10, &reply[15], 8);
		break;
	case TOUCH_REAR_INFO:
		memcpy(out, &touch.panel[1], 4);
		break;
	case TOUCH_REAR_VERSION:
		memcpy(out, &touch.panel[1].configVersion, 2);
		break;
	case TOUCH_REAR_GRIP:
		memcpy(out, touch.panel[1].gripEdges, 4);
		break;
	case TOUCH_REAR_EXTRA:
		memcpy(out, &reply[15], 8);
		break;
	}
out:
	return result;
}
/* the rear's controller 2: command 1, after which it reads as controller 3 */
unsigned char touchRearEnterBootloader(char panel)
{
	struct TouchRearCommand command;
	unsigned char reply[6];
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	command.address = 4;
	command.length = 5;
	command.data[0] = 0x2f;
	command.data[1] = 0;
	command.data[2] = 1;
	result = touchWriteRead((unsigned char *)&command, 7, reply, 2, panel, 0);
	if (result == 0)
		touch.panel[panel].controller = 3;
	return result;
}

/* the rear's controller 2: command 3, active */
unsigned char touchRearSetActive(char panel)
{
	struct TouchRearCommand command;
	unsigned char reply[6];
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	command.address = 4;
	command.length = 5;
	command.data[0] = 0x2f;
	command.data[1] = 0;
	command.data[2] = 3;
	result = touchWriteRead((unsigned char *)&command, 7, reply, 5, panel, 0);
	if (result == 0)
		touch.panel[panel].controllerActive = 1;
	else
		touch.panel[panel].controllerActive = 0xff;
	return result;
}

/* the rear's controller 2: command 4, idle */
unsigned char touchRearSetIdle(char panel)
{
	struct TouchRearCommand command;
	unsigned char reply[6];
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	command.address = 4;
	command.length = 5;
	command.data[0] = 0x2f;
	command.data[1] = 0;
	command.data[2] = 4;
	result = touchWriteRead((unsigned char *)&command, 7, reply, 5, panel, 0);
	if (result == 0)
		touch.panel[panel].controllerActive = 0;
	else
		touch.panel[panel].controllerActive = 0xff;
	return result;
}

/* the rear's controller 2: command 5 with index, its 8 bytes into out */
unsigned char touchRearReadBlock(unsigned char index, unsigned char *out, char panel)
{
	struct TouchRearCommand command;
	unsigned char reply[0xc];
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	command.address = 4;
	command.length = 6;
	command.data[0] = 0x2f;
	command.data[1] = 0;
	command.data[2] = 5;
	command.data[3] = index;
	result = touchWriteRead((unsigned char *)&command, 8, reply, 0xb, panel, 0);
	if (result == 0)
		memcpy(out, &reply[5], 8);
	return result;
}

/* the rear's controller 2: command 6 with index, writes length (< 7) bytes of data and its
   two bytes back into out; 0x3c for a longer length */
unsigned char touchRearWriteBlock(unsigned char index, unsigned char length, unsigned char *data,
				  unsigned char *out, char panel)
{
	struct TouchRearLongCommand command;
	unsigned char reply[8];
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	command.address = 4;
	command.length = 7 + length;
	command.data[0] = 0x2f;
	command.data[1] = 0;
	command.data[2] = 6;
	command.data[3] = index;
	command.data[4] = length;
	if (length < 7) {
		memcpy(&command.data[5], data, length);
		result = touchWriteRead((unsigned char *)&command, 7 + length + 2, reply, 7,
					panel, 0);
		if (result == 0)
			memcpy(out, &reply[5], 2);
	} else
		result = 0x3c;
	return result;
}

/* the rear's controller 2: command 0x20 with index, active around it, its 5 bytes into out */
unsigned char touchRearRead(unsigned char index, unsigned char *out, char panel)
{
	struct TouchRearCommand command;
	unsigned char reply[10];
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	touchRearBegin(panel);
	command.address = 4;
	command.length = 6;
	command.data[0] = 0x2f;
	command.data[1] = 0;
	command.data[2] = 0x20;
	command.data[3] = index;
	result = touchWriteRead((unsigned char *)&command, 8, reply, 10, panel, 0);
	if (result == 0)
		memcpy(out, &reply[5], 5);
	touchRearEnd(panel);
	return result;
}

/* the rear's controller 2: command 0x22, reads length bytes of its memory at address (page
   address >> 7, offset address & 0x7f, the range within one page; 0x3c otherwise) into data
   from its byte 2, with the length read in data[0] and 0 in data[1]; 0x0a when the controller
   answers an error */
unsigned char touchRearReadMemory(unsigned char index, unsigned int address, unsigned char length,
				  unsigned char *data, char panel)
{
	struct TouchRearMemoryCommand command;
	unsigned char (*reply)[0x80];
	unsigned int page;
	unsigned char offset;
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	page = address >> 7;
	offset = address & 0x7f;
	if ((unsigned char)(offset + length) > 0x80)
		return 0x3c;
	touchRearBegin(panel);
	reply = (unsigned char (*)[0x80])data;
	command.address = 4;
	command.length = 10;
	command.data[0] = 0x2f;
	command.data[1] = 0;
	command.data[2] = 0x22;
	command.data[3] = page;
	command.data[4] = ((unsigned char *)&page)[1];
	command.data[5] = offset + length;
	command.data[6] = 0;
	command.data[7] = index;
	result = touchWriteRead((unsigned char *)&command, 0x0c, *reply, 10 + offset + length + 2,
				panel, 0);
	if (result == 0) {
		if ((*reply)[5] == 0) {
			(*reply)[0] = (*reply)[7] - offset;
			(*reply)[1] = 0;
			memmove(&(*reply)[2], &(*reply)[10 + offset], length);
		} else
			result = 0x0a;
	}
	touchRearEnd(panel);
	return result;
}

/* a command's bytes in a buffer */
#define TOUCH_REAR_BYTES(command)	(*(unsigned char (*)[0x80])(command))

/* the rear's controller 2: command 0x23, writes length bytes of data to its memory at address
   (in one page, 0x3c otherwise): reads the page's bytes before the offset first (0x0a when
   that fails), then writes them with data, touchRearWriteKey and their CRC; out[0] the
   length written, out[1] 0 */
unsigned char touchRearWriteMemory(unsigned char index, unsigned int address, unsigned char length,
				   unsigned char *data, unsigned char *out, char panel)
{
	struct TouchRearMemoryCommand read;
	unsigned char reply[10];
	struct TouchRearWriteCommand *command;
	unsigned int page;
	unsigned char offset;
	unsigned char result;
	unsigned int crc;
	unsigned char total;

	result = 0;
	if (panel == 0)
		return result;
	page = address >> 7;
	offset = address & 0x7f;
	if ((unsigned char)(offset + length) > 0x80)
		return 0x3c;
	touchRearBegin(panel);
	read.address = 4;
	read.length = 10;
	read.data[0] = 0x2f;
	read.data[1] = 0;
	read.data[2] = 0x22;
	read.data[3] = ((unsigned char *)&page)[0];
	read.data[4] = ((unsigned char *)&page)[1];
	read.data[5] = offset;
	read.data[6] = 0;
	read.data[7] = index;
	result = touchWriteRead((unsigned char *)&read, 0x0c, touchBufferData,
				10 + offset + (offset != 0 ? 2 : 0), panel, 0);
	if (result != 0) {
		touchRearEnd(panel);
		return 0x0a;
	}
	memmove(&touchBufferData[12], &touchBufferData[10], offset);
	memcpy(&touchBufferData[12 + offset], data, length);
	total = offset + length;
	command = (struct TouchRearWriteCommand *)touchBufferData;
	command->address = 4;
	command->length = 10 + total + 8 + (total != 0 ? 2 : 0);
	command->prefix = 0x2f;
	command->zero = 0;
	command->command = 0x23;
	TOUCH_REAR_BYTES(command)[7] = ((unsigned char *)&page)[0];
	TOUCH_REAR_BYTES(command)[8] = ((unsigned char *)&page)[1];
	TOUCH_REAR_BYTES(command)[9] = total;
	TOUCH_REAR_BYTES(command)[10] = 0;
	TOUCH_REAR_BYTES(command)[11] = index;
	memcpy(&TOUCH_REAR_BYTES(command)[12 + total], touchRearWriteKey, 8);
	crc = touchRearCrc(total, &TOUCH_REAR_BYTES(command)[12]);
	TOUCH_REAR_BYTES(command)[12 + total + 8] = ((unsigned char *)&crc)[0];
	TOUCH_REAR_BYTES(command)[12 + total + 8 + 1] = ((unsigned char *)&crc)[1];
	result = touchWriteRead(TOUCH_REAR_BYTES(command), 10 + total + 8 + (total != 0 ? 2 : 0) + 2,
				reply, 9, panel, 0);
	if (result == 0) {
		if (reply[5] == 0) {
			out[0] = reply[7] - offset;
			out[1] = 0;
		} else
			result = 0x0a;
	}
	touchRearEnd(panel);
	return result;
}

/* the rear's controller 2: command 0x24, reads length bytes at address into data: its two
   bytes from byte 7 first, then the bytes */
unsigned char touchRearReadRange(unsigned char index, unsigned int address, unsigned int length,
				 unsigned char *data, char panel)
{
	struct TouchRearMemoryCommand command;
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	touchRearBegin(panel);
	command.address = 4;
	command.length = 10;
	command.data[0] = 0x2f;
	command.data[1] = 0;
	command.data[2] = 0x24;
	command.data[3] = address;
	command.data[4] = address >> 8;
	command.data[5] = length;
	command.data[6] = length >> 8;
	command.data[7] = index;
	result = touchWriteRead((unsigned char *)&command, 0x0c, data, length + 10, panel, 0);
	if (result == 0 && data[5] == 0) {
		memmove(data, &data[7], 2);
		memmove(&data[2], &data[10], length);
	}
	touchRearEnd(panel);
	return result;
}

/* the rear's controller 2: command 0x26 with index, *out its state: 0x10 when it answers
   0xff, 0 when 0, 0xfe when the command fails with 0xff, 0xff otherwise */
unsigned char touchRearGetState(unsigned char index, unsigned char *out, char panel)
{
	struct TouchRearCommand command;
	unsigned char reply[8];
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	touchRearBegin(panel);
	command.address = 4;
	command.length = 6;
	command.data[0] = 0x2f;
	command.data[1] = 0;
	command.data[2] = 0x26;
	command.data[3] = index;
	result = touchWriteRead((unsigned char *)&command, 8, reply, 7, panel, 0);
	if (result == 0) {
		if (reply[5] == 0) {
			switch (reply[6]) {
			case 0:
				*out = 0;
				break;
			case 0xff:
				*out = 0x10;
				break;
			default:
				*out = 0xff;
				break;
			}
		} else if (reply[5] == 0xff)
			*out = 0xfe;
		else
			*out = 0xff;
	}
	touchRearEnd(panel);
	return result;
}

/* the rear's controller 2: command 0x27, touchRearReadRange's read of another range */
unsigned char touchRearReadRange2(unsigned char index, unsigned int address, unsigned int length,
				  unsigned char *data, char panel)
{
	struct TouchRearMemoryCommand command;
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	touchRearBegin(panel);
	command.address = 4;
	command.length = 10;
	command.data[0] = 0x2f;
	command.data[1] = 0;
	command.data[2] = 0x27;
	command.data[3] = address;
	command.data[4] = address >> 8;
	command.data[5] = length;
	command.data[6] = length >> 8;
	command.data[7] = index;
	result = touchWriteRead((unsigned char *)&command, 0x0c, data, length + 10, panel, 0);
	if (result == 0 && data[5] == 0) {
		memmove(data, &data[7], 2);
		memmove(&data[2], &data[10], length);
	}
	touchRearEnd(panel);
	return result;
}

/* the rear's controller 2: command 0x28 with index; 0x0a when it answers an error */
unsigned char touchRearCommand28(unsigned char index, char panel)
{
	struct TouchRearCommand command;
	unsigned char reply[6];
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	touchRearBegin(panel);
	command.address = 4;
	command.length = 6;
	command.data[0] = 0x2f;
	command.data[1] = 0;
	command.data[2] = 0x28;
	command.data[3] = index;
	result = touchWriteRead((unsigned char *)&command, 8, reply, 6, panel, 0);
	if (result == 0 && reply[5] != 0)
		result = 0x0a;
	touchRearEnd(panel);
	return result;
}

/* the rear's controller 2: command 0x29 with index; 0x0a when it answers an error */
unsigned char touchRearCommand29(unsigned char index, char panel)
{
	struct TouchRearCommand command;
	unsigned char reply[6];
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	touchRearBegin(panel);
	command.address = 4;
	command.length = 6;
	command.data[0] = 0x2f;
	command.data[1] = 0;
	command.data[2] = 0x29;
	command.data[3] = index;
	result = touchWriteRead((unsigned char *)&command, 8, reply, 6, panel, 0);
	if (result == 0 && reply[5] != 0)
		result = 0x0a;
	touchRearEnd(panel);
	return result;
}

/* the rear's controller 2: command 0x2a; 0x0a when it answers an error */
unsigned char touchRearCommand2a(char panel)
{
	struct TouchRearCommand command;
	unsigned char reply[6];
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	touchRearBegin(panel);
	command.address = 4;
	command.length = 5;
	command.data[0] = 0x2f;
	command.data[1] = 0;
	command.data[2] = 0x2a;
	result = touchWriteRead((unsigned char *)&command, 7, reply, 6, panel, 0);
	if (result == 0 && reply[5] != 0)
		result = 0x0a;
	touchRearEnd(panel);
	return result;
}

/* the rear's controller 2: command 0x2b, reads count words at address into data: its two
   bytes from byte 7 first, then the words */
unsigned char touchRearReadWords(unsigned char index, unsigned int address, unsigned int count,
				 unsigned char *data, char panel)
{
	struct TouchRearMemoryCommand command;
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	touchRearBegin(panel);
	command.address = 4;
	command.length = 10;
	command.data[0] = 0x2f;
	command.data[1] = 0;
	command.data[2] = 0x2b;
	command.data[3] = address;
	command.data[4] = address >> 8;
	command.data[5] = count;
	command.data[6] = count >> 8;
	command.data[7] = index;
	result = touchWriteRead((unsigned char *)&command, 0x0c, data, count * 2 + 10, panel, 0);
	if (result == 0 && data[5] == 0) {
		memmove(data, &data[7], 2);
		memmove(&data[2], &data[10], count * 2);
	}
	touchRearEnd(panel);
	return result;
}

/* the rear's controller 2: command 0x71, its report format: 1 the short report, 0 the full;
   kept in touch.rearShort when the controller takes it */
unsigned char touchRearSetReportShort(unsigned char rearShort, char panel)
{
	struct TouchRearCommand command;
	unsigned char reply[6];
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	command.address = 4;
	command.length = 6;
	command.data[0] = 0x2f;
	command.data[1] = 0;
	command.data[2] = 0x71;
	command.data[3] = rearShort;
	result = touchWriteRead((unsigned char *)&command, 8, reply, 6, panel, 0);
	if (result == 0 && (rearShort == 0 || rearShort == 1))
		touch.rearShort = rearShort;
	return result;
}

/* the rear's controller 2: command 0x72 with the four bytes of data */
unsigned char touchRearCommand72(unsigned char *data, char panel)
{
	struct TouchRearMemoryCommand command;
	unsigned char reply[6];
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	command.address = 4;
	command.length = 9;
	command.data[0] = 0x2f;
	command.data[1] = 0;
	command.data[2] = 0x72;
	command.data[3] = data[0];
	command.data[4] = data[1];
	command.data[5] = data[2];
	command.data[6] = data[3];
	result = touchWriteRead((unsigned char *)&command, 0x0b, reply, 5, panel, 0);
	return result;
}

/* the rear's controller 2: command 0x31 in the checked form; *out 0 when it answers 1, 1
   otherwise */
unsigned char touchRearCommand31(unsigned char *out, char panel)
{
	struct TouchRearCheckedCommand command;
	unsigned int crc;
	unsigned char reply[12];
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	command.address = 4;
	command.length = 0x0b;
	command.prefix = 0x40;
	command.zero = 0;
	command.mode = 1;
	command.code = 0x31;
	command.value = 0;
	crc = touchRearCrc(4, &command.mode);
	command.crc[0] = ((unsigned char *)&crc)[0];
	command.crc[1] = ((unsigned char *)&crc)[1];
	command.end = 0x17;
	result = touchWriteRead((unsigned char *)&command, 0x0d, reply, 0x0c, panel, 0);
	if (result == 0) {
		if (reply[5] == 0)
			*out = reply[8] == 1 ? 0 : 1;
		else
			result = 0x0a;
	}
	return result;
}

/* the rear's controller 2: command 0x38 in the checked form, its 8 bytes into out */
unsigned char touchRearCommand38(unsigned char *out, char panel)
{
	struct TouchRearCheckedCommand command;
	unsigned int crc;
	unsigned char reply[20];
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	command.address = 4;
	command.length = 0x0b;
	command.prefix = 0x40;
	command.zero = 0;
	command.mode = 1;
	command.code = 0x38;
	command.value = 0;
	crc = touchRearCrc(4, &command.mode);
	command.crc[0] = ((unsigned char *)&crc)[0];
	command.crc[1] = ((unsigned char *)&crc)[1];
	command.end = 0x17;
	result = touchWriteRead((unsigned char *)&command, 0x0d, reply, 0x13, panel, 0);
	if (result == 0) {
		if (reply[5] == 0)
			memcpy(out, &reply[8], 8);
		else
			result = 0x0a;
	}
	return result;
}

/* the rear's controller 2: command 0x39 in the checked form, with the 0x83 bytes the host
   gathered in touchBuffer; out its bytes 0x0b and 0x0c back and the answer's status */
unsigned char touchRearCommand39(unsigned char *out, char panel)
{
	struct TouchRearCheckedCommand *command;
	unsigned int crc;
	unsigned char reply[12];
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	command = (struct TouchRearCheckedCommand *)touchBuffer.data;
	command->address = 4;
	command->length = 0x8e;
	command->prefix = 0x40;
	command->zero = 0;
	command->mode = 1;
	command->code = 0x39;
	command->value = 0x83;
	crc = touchRearCrc(0x87, &TOUCH_REAR_BYTES(command)[6]);
	TOUCH_REAR_BYTES(command)[0x8d] = ((unsigned char *)&crc)[0];
	TOUCH_REAR_BYTES(command)[0x8e] = ((unsigned char *)&crc)[1];
	TOUCH_REAR_BYTES(command)[0x8f] = 0x17;
	result = touchWriteRead(TOUCH_REAR_BYTES(command), 0x90, reply, 0x0b, panel, 0);
	if (result == 0) {
		out[0] = TOUCH_REAR_BYTES(command)[0x0b];
		out[1] = TOUCH_REAR_BYTES(command)[0x0c];
		out[2] = reply[5];
	}
	return result;
}

/* the rear's controller 2: command 0x3b in the checked form, then its information block into
   out: when it starts with 0xf7 the panel is controller 2 again, active state cleared, and
   identified (out[0] 0); out[0] 1 otherwise */
unsigned char touchRearCommand3b(unsigned char *out, char panel)
{
	struct TouchRearCheckedCommand command;
	unsigned int crc;
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	command.address = 4;
	command.length = 0x0b;
	command.prefix = 0x40;
	command.zero = 0;
	command.mode = 1;
	command.code = 0x3b;
	command.value = 0;
	crc = touchRearCrc(4, &command.mode);
	command.crc[0] = ((unsigned char *)&crc)[0];
	command.crc[1] = ((unsigned char *)&crc)[1];
	command.end = 0x17;
	result = touchWriteRead((unsigned char *)&command, 0x0d, 0, 0, panel, 0x82);
	if (result != 0)
		goto done;
	result = touchRearReadInfo(out, panel);
	if (result != 0)
		goto done;
	if (out[0] == 0xf7) {
		touch.panel[panel].controller = 2;
		touch.panel[panel].controllerStatus = 0;
		touch.panel[panel].controllerActive = 0;
		touch.panel[panel].controllerWasActive = 0;
		result = touchRearIdentify(0, TOUCH_REAR_ALL, panel);
		out[0] = 0;
	} else
		out[0] = 1;
done:
	return result;
}

/* CRC-16/CCITT (polynomial 0x1021, 0xffff first, most significant bit first) of length bytes */
unsigned int touchRearCrc(unsigned char length, unsigned char *data)
{
	unsigned char i;
	unsigned char shift;
	unsigned int crc;

	crc = 0xffff;
	for (i = 0; i < length; i++) {
		crc ^= (unsigned int)data[i] << 8;
		for (shift = 0; shift < 8; shift++) {
			if (crc & 0x8000)
				crc = crc << 1 ^ 0x1021;
			else
				crc <<= 1;
		}
	}
	return crc;
}

/* the rear's controller 2: command 0x48 in the checked form, address 0x1ff alone (0x3c
   otherwise): touchRearWriteKey and the 0x80 bytes the host gathered in touchBuffer after it;
   out the address back and the answer's status */
unsigned char touchRearCommand48(unsigned int address, unsigned char *out, char panel)
{
	struct TouchRearCheckedCommand *command;
	unsigned int crc;
	unsigned char reply[12];
	unsigned char result;

	result = 0;
	if (panel == 0)
		return result;
	if (address != 0x1ff)
		return 0x3c;
	command = (struct TouchRearCheckedCommand *)touchBuffer.data;
	command->address = 4;
	command->length = 0x93;
	command->prefix = 0x40;
	command->zero = 0;
	command->mode = 1;
	command->code = 0x48;
	command->value = 0x88;
	memcpy(&TOUCH_REAR_BYTES(command)[0x0a], touchRearWriteKey, 8);
	crc = touchRearCrc(0x8c, &TOUCH_REAR_BYTES(command)[6]);
	TOUCH_REAR_BYTES(command)[0x92] = ((unsigned char *)&crc)[0];
	TOUCH_REAR_BYTES(command)[0x93] = ((unsigned char *)&crc)[1];
	TOUCH_REAR_BYTES(command)[0x94] = 0x17;
	result = touchWriteRead(TOUCH_REAR_BYTES(command), 0x95, reply, 0x0b, panel, 0x2ee0);
	if (result == 0) {
		out[0] = ((unsigned char *)&address)[0];
		out[1] = ((unsigned char *)&address)[1];
		out[2] = reply[5];
	}
	return result;
}

/* makes the rear's controller 2 active for a command, keeping its state for touchRearEnd */
unsigned char touchRearBegin(char panel)
{
	unsigned char result;
	unsigned char p;

	result = 0;
	p = panel;
	touch.panel[p].controllerWasActive = touch.panel[p].controllerActive;
	if (touch.panel[p].controllerActive != 1)
		result = touchRearSetActive(p);
	return result;
}

/* puts the rear's controller 2 back in the state touchRearBegin kept: idle again when it was
   idle */
unsigned char touchRearEnd(char panel)
{
	unsigned char result;
	unsigned char p;

	result = 0;
	p = panel;
	if (touch.panel[p].controllerWasActive != 1) {
		if (touch.panel[p].controllerWasActive == 0)
			result = touchRearSetIdle(p);
		else
			touch.panel[p].controllerActive = touch.panel[p].controllerWasActive;
	}
	return result;
}
#endif
