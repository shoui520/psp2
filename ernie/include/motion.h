/*
 * The motion sensor (its module at 0x1aad3, not written yet): its link on CSI01, the frames
 * stream 0x0400 sends and the buffers of the commands request 4 passes it.
 */
#ifndef ERNIE_MOTION_H
#define ERNIE_MOTION_H

#define MOTION_FRAME_SIZE	0x3c

/* a word of the sensor's, least significant byte first */
union MotionWord {
	unsigned int	value;
	unsigned char	byte[2];
};

/* a record the module keeps (its INTSR0 writes it), reset when the inputs wake; request 4's
   0x0480 answers its two words */
struct RecordFEBBC {
	unsigned char	flag;
	union MotionWord value[2];
};

extern struct RecordFEBBC recordFEBBC;
extern unsigned char motionFrames[2][MOTION_FRAME_SIZE];
extern unsigned char bufferFEC3A[MOTION_FRAME_SIZE];
extern unsigned char bufferFEC76[MOTION_FRAME_SIZE];
extern unsigned char byteFECB2;
extern unsigned char byteFECB5;

extern void sub_1AAD3(void);
extern void sub_1AB0E(void);
extern void sub_1AB25(void);
extern void sub_1AB70(void);
extern void sub_1AB89(void);
extern void sub_1ACCB(unsigned char command, unsigned char *data, unsigned char length);
extern void sub_1ADA7(void);
extern void sub_1AFD8(void);
extern void sub_1B013(unsigned char *send, unsigned char *receive);

#endif
