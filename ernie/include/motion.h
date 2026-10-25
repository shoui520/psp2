/*
 * The motion sensor (app/motion.c): its packets on CSI01, the frames stream 0x0400 sends and
 * the sensor's information words.
 */
#ifndef ERNIE_MOTION_H
#define ERNIE_MOTION_H

#define MOTION_FRAME_SIZE	0x3c
#define MOTION_PACKET_SIZE	0x3b	/* the bytes a transfer moves each way */
#define MOTION_FRAMES		2

/* a packet: its length, its entries (a command or type, the entry's length with these two
   bytes, its data), a 0 and the sum of every byte before the sum */
#define MOTION_COMMAND_FRAME	0x02	/* asks for a frame */
#define MOTION_COMMAND_INFO	0x03	/* asks for the information words */
#define MOTION_ENTRY_INFO	0x03	/* the answer's entry: the two words */

/* where motionPacketNext is in a packet, and the entry it found */
struct MotionEntry {
	unsigned char	type;
	unsigned char	length;		/* the entry's, with its type and length */
	unsigned char	*data;
	unsigned char	*packet;	/* the packet's entries */
	unsigned char	offset;		/* the next entry's, from packet */
};

/* a word of the sensor's, least significant byte first */
union MotionWord {
	unsigned int	value;
	unsigned char	byte[2];
};

struct Motion {
	unsigned char	status;		/* MOTION_ABSENT, else MOTION_FRAME once a frame came in */
	unsigned char	block;		/* the frame being received, stream 0x0400's next */
	union MotionWord info[2];	/* the information words, request 4's 0x0480 answers */
};

/* motion.status */
#define MOTION_ABSENT		0xff
#define MOTION_FRAME		0x80

extern struct Motion motion;
extern unsigned char motionFrames[MOTION_FRAMES][MOTION_FRAME_SIZE];
extern unsigned char motionSend[MOTION_FRAME_SIZE];
extern unsigned char motionReceive[MOTION_FRAME_SIZE];
extern unsigned char motionPeriod;
extern unsigned char motionInfoResult;

extern void motionOpen(void);
extern void motionClose(void);
extern void motionClearFrames(void);
extern void motionStart(void);
extern void motionStop(void);
extern unsigned char motionReadFrame(void);
extern unsigned char motionCheckFrame(void);
extern unsigned char motionPacketCheck(unsigned char *packet);
extern void motionParseFrame(void);
extern unsigned char motionPacketAdd(unsigned char command, unsigned char *data,
				     unsigned char length);
extern unsigned char motionPacketLength(void);
extern unsigned char motionPacketSum(unsigned char *packet);
extern void motionPacketFinish(void);
extern unsigned char motionPacketNext(struct MotionEntry *entry);
extern void motionInfoPoll(void);
extern void motionInfoStart(void);
extern void motionSetAbsent(void);
extern unsigned char motionGetStatus(void);
extern int motionTransfer(unsigned char *send, unsigned char *receive);

#endif
