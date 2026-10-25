/*
 * The 3G modem's link (app/bbmc.c), on UART2: packets of a start byte, a length, a sequence
 * number, a command, data and an XOR checksum, each answered with a status. The modem reads
 * and writes bbmcRegs, a block of 0x30 registers; the host reads and writes them too, through
 * request 0x0a. Task 6 (app/task6.c) runs the events of event flag 6.
 */
#ifndef ERNIE_BBMC_H
#define ERNIE_BBMC_H

#include "host.h"
#include "input.h"
#include "kernel.h"

#define BBMC_REGS		0x30	/* the registers */

/* bbmcWriteRegs' modes: who writes, which decides what each register lets it do
   (bbmcRegModes) */
#define BBMC_WRITE_HOST		0	/* request 0x0a's command 0x0a81 */
#define BBMC_WRITE_MODEM	1	/* the modem's command 0x10 */
#define BBMC_WRITE_SET		2	/* the firmware's own changes */
#define BBMC_WRITE_RESET	3	/* the firmware's resets */

/* event flag 6's events (task 6) */
#define BBMC_EVENT_RECEIVE	0x0001	/* UART2 received bytes */
#define BBMC_EVENT_WAKE		0x0002	/* the modem's wake input changed */
#define BBMC_EVENT_POWER	0x0004	/* the modem's power input changed */
#define BBMC_EVENT_RX_TIMEOUT	0x0008	/* timer 2: a packet did not complete */
#define BBMC_EVENT_LINK_TIMEOUT	0x0010	/* timer 0x13: no packet for a while */
#define BBMC_EVENT_START	0x0020	/* timer 0x15 */
#define BBMC_EVENT_REQUEST	0x0040	/* request 0x0a has a command */
#define BBMC_EVENT_STATE	0x0080	/* the system state changed */
#define BBMC_EVENT_MODE		0x0100	/* timer 0x19: the mode set takes effect */
#define BBMC_EVENT_RESUME	0x0200	/* UART2 opens again */
#define BBMC_EVENTS		0x03ff

/* the registers */
union BbmcRegs {
	unsigned char	byte[BBMC_REGS];
	struct {
		unsigned char	id;		/* 5 */
		unsigned char	reserved1;
		unsigned char	size;		/* BBMC_REGS */
		unsigned char	version;	/* the modem's */
		unsigned int	mode;		/* the mode set (NVS area 1, 0x2e) */
		unsigned int	modeActive;	/* the mode in effect; bit 15 when the firmware set it */
		unsigned long	status;		/* read clears */
		unsigned long	events;		/* read clears */
	} r;
};

/* a packet from the modem */
struct BbmcPacket {
	unsigned char	start;		/* 0x5a */
	unsigned char	length;		/* from sequence to the checksum */
	unsigned char	sequence;
	unsigned char	command;
	unsigned char	data[0x1c];	/* then the checksum */
};

/* an answer to the modem */
struct BbmcAnswer {
	unsigned char	start;		/* 0xa5 */
	unsigned char	length;
	unsigned char	sequence;	/* the packet's */
	unsigned char	command;	/* the packet's */
	unsigned char	status;
	unsigned char	data[0x1b];	/* then the checksum */
};

/* the link's state */
struct Bbmc {
	unsigned char		lastSequence;	/* the last packet's sequence number */
	unsigned char		repeats;	/* how often in a row it came again */
	unsigned char		rxLength;	/* the bytes of rx received */
	unsigned char		pending;	/* the status or the events are not 0 */
	unsigned char		transition;	/* transition 8 ran for the events */
	unsigned char		state;		/* 1 off, 2 on, 3 on in mode 1, 4 on in mode 6 or 7 */
	struct BbmcPacket	rx;
	struct BbmcAnswer	tx;
};

extern struct Bbmc bbmc;
extern union BbmcRegs bbmcRegs;
extern struct HostRequest bbmcRequest;
extern struct InputHandler bbmcInputHandlerPower;
extern struct InputHandler bbmcInputHandlerWake;

extern void bbmcInit(void);
extern unsigned char bbmcPowerOn(void);
extern unsigned char bbmcPowerOff(void);
extern unsigned char bbmcStart(void);
extern unsigned char bbmcSetOff(void);
extern void bbmcStop(void);
extern void bbmcWake(void);
extern void bbmcSignal4(void);
extern void bbmcSignal8(void);
extern unsigned char bbmcGetMode(void);
extern unsigned char bbmcIsOn(void);
extern unsigned char bbmcModeIsNone(void);
extern unsigned char bbmcEvent(FLGPTN ptn);
extern void bbmcTrace(const char *name, unsigned char *packet);
extern void bbmcReceive(void);
extern unsigned char bbmcChecksum(unsigned char *packet);
extern unsigned char bbmcCommandVersion(struct BbmcPacket *packet, struct BbmcAnswer *answer);
extern unsigned char bbmcCommandHardwareInfo(struct BbmcPacket *packet, struct BbmcAnswer *answer);
extern unsigned char bbmcCommandNone(struct BbmcPacket *packet, struct BbmcAnswer *answer);
extern unsigned char bbmcCommandWrite(struct BbmcPacket *packet, struct BbmcAnswer *answer);
extern unsigned char bbmcCommandRead(struct BbmcPacket *packet, struct BbmcAnswer *answer);
extern unsigned char bbmcCommandEcho20(struct BbmcPacket *packet, struct BbmcAnswer *answer);
extern unsigned char bbmcCommandEcho21(struct BbmcPacket *packet, struct BbmcAnswer *answer);
extern void bbmcNvsDone(unsigned char command, unsigned char result);
extern void bbmcRegsInit(void);
extern unsigned char bbmcWriteRegs(unsigned char reg, unsigned char count, unsigned char *values,
				   unsigned char mode);
extern unsigned char bbmcReadRegs(unsigned char reg, unsigned char count, unsigned char *values,
				  unsigned char keep);
extern void bbmcAddStatus(unsigned long bits);
extern void bbmcSetStatus(unsigned long bits);
extern void bbmcSetEvents(unsigned long bits);
extern void bbmcSetModeActive(unsigned int mode);
extern unsigned char bbmcGetPending(void);
extern unsigned char bbmcGetVersion(void);
extern void bbmcDelay(void);
extern void bbmcRequestNotify(void);
extern void bbmcRequestRun(void);
extern void bbmcPostRxTimeout(void);
extern void bbmcPostLinkTimeout(void);
extern void bbmcPostStart(void);
extern void bbmcPostMode(void);
extern void bbmcPostPower(void);
extern void bbmcPostWake(void);

#endif
