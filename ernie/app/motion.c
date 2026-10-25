/*
 * The motion sensor: on CSI01 (csi01.c), selected by P4.2 and held in reset by P6.3 low. The
 * two talk in packets of MOTION_PACKET_SIZE bytes each way, built in motionSend. While the
 * frames run, every period of TM01 a packet asks for a frame, which comes into the block of
 * motionFrames stream 0x0400 does not send; once after a start the sensor's information
 * words are read.
 */
#pragma sfr
#pragma NOP
#pragma inline

#include <stddef.h>
#include <string.h>

#include "csi01.h"
#include "host.h"
#include "input.h"
#include "inputreq.h"
#include "motion.h"
#include "tau.h"

/* the block of motionFrames after n */
#define MOTION_NEXT_FRAME(n)	((n) + 1 < MOTION_FRAMES ? (n) + 1 : 0)

static struct MotionEntry motionEntry;
struct Motion motion;
unsigned char motionFrames[MOTION_FRAMES][MOTION_FRAME_SIZE];
unsigned char motionSend[MOTION_FRAME_SIZE];
unsigned char motionReceive[MOTION_FRAME_SIZE];
unsigned char motionPeriod;			/* TM01's, in 0.1 ms */
static unsigned char motionInfoPhase;
unsigned char motionInfoResult;			/* 0: read, 2: the link stayed busy */

/* opens the link, unless the sensor is absent */
void motionOpen(void)
{
	if (motion.status != MOTION_ABSENT) {
		csi01Open();
		motionClearFrames();
		motion.status = 0;
		motion.info[0].value = 0;
		motion.info[1].value = 0;
		motionPeriod = 80;
		memset(motionSend, 0, MOTION_FRAME_SIZE);
		motionInfoPhase = 0;
	}
}

void motionClose(void)
{
	motionStop();
	csi01Close();
}

void motionClearFrames(void)
{
	memset(motionFrames, 0, sizeof(motionFrames));
}

/* starts the frames, TM01 every motionPeriod, if the sensor is out of reset */
void motionStart(void)
{
	if (P6.3 && !(inputKeys.state & 0x0010)) {
		motion.block = 0;
		inputKeys.state |= 0x0010;
		tau0PowerOn();
		TMR01 = 0x8000;
		TDR01 = (unsigned int)motionPeriod * 100 / 32 - 1;
		TMMK01 = 0;
		TS0L = 0x02;
	}
}

void motionStop(void)
{
	TT0L = 0x02;
	TMMK01 = 1;
	inputKeys.state &= ~0x0010;
	motionClearFrames();
}

/* TM01's tick: asks for a frame, into the block after the one stream 0x0400 starts from that
   it does not send */
unsigned char motionReadFrame(void)
{
	unsigned char result;
	unsigned char block;
	struct Csi01Transfer transfer;

	memset(motionSend, 0, MOTION_FRAME_SIZE);
	motionPacketAdd(MOTION_COMMAND_FRAME, NULL, 0);
	motionPacketFinish();
	block = MOTION_NEXT_FRAME(hostStream0400.first);
	if (block == hostStream0400.block)
		block = MOTION_NEXT_FRAME(block);
	motion.block = block;
	P4.2 = 0;
	transfer.receive = motionFrames[motion.block];
	transfer.receiveCount = MOTION_PACKET_SIZE;
	transfer.send = motionSend;
	transfer.sendCount = MOTION_PACKET_SIZE;
	transfer.state = CSI01_FRAME;
	result = csi01Transfer(&transfer);
	return result;
}

/* the frame came in: lets the sensor go; 1 if its sum is wrong (never called) */
unsigned char motionCheckFrame(void)
{
	P4.2 = 1;
	if (motionPacketCheck(motionFrames[motion.block]) == 1)
		return 0;
	return 1;
}

/* 1 if the packet's sum is right */
unsigned char motionPacketCheck(unsigned char *packet)
{
	unsigned char last;

	last = packet[0] - 1;
	if (packet[last] == motionPacketSum(packet))
		return 1;
	return 0;
}

/* walks the frame's entries (never called) */
void motionParseFrame(void)
{
	motionEntry.offset = 0;
	motionEntry.packet = motionFrames[motion.block] + 1;
	while (motionPacketNext(&motionEntry)) {
		if (motionEntry.type == 1)
			NOP();
		else if (motionEntry.type == 2)
			hostStream0400.first = motion.block;
	}
}

/* adds an entry to the packet in motionSend; 1 if it does not fit */
unsigned char motionPacketAdd(unsigned char command, unsigned char *data, unsigned char length)
{
	unsigned char end;

	end = motionPacketLength();
	if (end + length + 2 < MOTION_PACKET_SIZE - 1) {
		(motionSend + 1)[end] = command;
		end++;
		(motionSend + 1)[end] = length + 2;
		end++;
		if (length != 0)
			memcpy(&(motionSend + 1)[end], data, length);
	} else
		return 1;
	return 0;
}

/* the length of the entries in motionSend */
unsigned char motionPacketLength(void)
{
	unsigned char length;
	unsigned char i;

	length = 0;
	i = 1;
	while ((motionSend + 1)[i] != 0) {
		length += (motionSend + 1)[i];
		i += (motionSend + 1)[i];
		if (i >= MOTION_PACKET_SIZE - 2)
			break;
	}
	return length;
}

/* the sum of the packet's bytes before its last */
unsigned char motionPacketSum(unsigned char *packet)
{
	unsigned char i;
	unsigned char length;
	unsigned char sum;

	sum = 0;
	length = packet[0] - 1;
	for (i = 0; i < length; i++)
		sum += packet[i];
	return sum;
}

/* closes the packet in motionSend: its length first, its sum last */
void motionPacketFinish(void)
{
	unsigned char length;

	length = motionPacketLength() + 3;
	motionSend[0] = length;
	motionSend[length - 1] = motionPacketSum(motionSend);
}

/* the entry at entry->offset, and the offset of the next; 0 at the packet's end */
unsigned char motionPacketNext(struct MotionEntry *entry)
{
	unsigned char *p;

	p = entry->packet + entry->offset;
	if (*p != 0) {
		entry->type = *p;
		p++;
		entry->length = *p;
		entry->offset += *p;
		p++;
		entry->data = p;
		return 1;
	}
	return 0;
}

/* reads the information words: a packet asks for them, an empty one brings the answer;
   motionInfoResult 2 if the link stays busy */
void motionInfoPoll(void)
{
	struct Csi01Transfer transfer;
	unsigned char tries;

	switch (motionInfoPhase) {
	case 0:
		memset(motionSend, 0, MOTION_FRAME_SIZE);
		motionPacketAdd(MOTION_COMMAND_INFO, NULL, 0);
		motionPacketFinish();
		P4.2 = 0;
		transfer.receive = motionReceive;
		transfer.receiveCount = MOTION_PACKET_SIZE;
		transfer.send = motionSend;
		transfer.sendCount = MOTION_PACKET_SIZE;
		transfer.state = CSI01_TRANSFER;
		csi01Transfer(&transfer);
		motionInfoPhase++;
		break;
	case 1:
		tries = 0;
		if (csi01GetState() == CSI01_OPEN) {
			memset(motionSend, 0, MOTION_FRAME_SIZE);
			motionPacketFinish();
			P4.2 = 0;
			transfer.receive = motionReceive;
			transfer.receiveCount = MOTION_PACKET_SIZE;
			transfer.send = motionSend;
			transfer.sendCount = MOTION_PACKET_SIZE;
			transfer.state = CSI01_TRANSFER;
			csi01Transfer(&transfer);
			motionInfoPhase++;
		} else if (tries++ >= 3)
			goto fail;
		break;
	case 2:
		tries = 0;
		if (csi01GetState() == CSI01_OPEN) {
			motionEntry.offset = 0;
			motionEntry.packet = &motionReceive[1];
			while (motionPacketNext(&motionEntry)) {
				if (motionEntry.type == 1)
					;	/* Sony's tests entry 1 and does nothing */
				if (motionEntry.type == MOTION_ENTRY_INFO)
					memcpy(motion.info, motionEntry.data, sizeof(motion.info));
			}
			motionInfoResult = 0;
			inputKeys.state &= ~0x0400;
			return;
		} else if (tries++ >= 3)
			goto fail;
		return;
	default:
		break;
	}
	return;
fail:
	inputKeys.state &= ~0x0400;
	motionInfoResult = 2;
}

/* starts reading the information words (motionInfoPoll) */
void motionInfoStart(void)
{
	inputKeys.state |= 0x0400;
	motionInfoPhase = 0;
	motion.info[0].value = 0;
	motion.info[1].value = 0;
}

void motionSetAbsent(void)
{
	motion.status = MOTION_ABSENT;
	motion.info[0].value = 0xffff;
	motion.info[1].value = 0xffff;
}

unsigned char motionGetStatus(void)
{
	return motion.status;
}

/* sends a packet and receives one, MOTION_PACKET_SIZE bytes each way */
int motionTransfer(unsigned char *send, unsigned char *receive)
{
	struct Csi01Transfer transfer;

	P4.2 = 0;
	transfer.receive = receive;
	transfer.receiveCount = MOTION_PACKET_SIZE;
	transfer.send = send;
	transfer.sendCount = MOTION_PACKET_SIZE;
	transfer.state = CSI01_TRANSFER;
	csi01Transfer(&transfer);
	return 0;
}
