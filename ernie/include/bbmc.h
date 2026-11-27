/*
 * The 3G modem's link (app/bbmc.c), on UART2: packets of a start byte, a length, a sequence
 * number, a command, data and an XOR checksum, each answered with a status. The modem reads
 * and writes bbmcRegs, a block of 0x30 registers; the host reads and writes them too, through
 * request 0x0a. Task 6 (app/task6.c) runs the events of event flag 6.
 */
#ifndef ERNIE_BBMC_H
#define ERNIE_BBMC_H

#include "board.h"
#include "host.h"
#include "input.h"
#include "kernel.h"

#if !FW_SINCE(0x00080302)
#define BBMC_REGS		0x20	/* 0x00080003's registers */
#else
#define BBMC_REGS		0x30	/* the registers */
#endif

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
#if FW_SINCE(0x00090502)
#define BBMC_EVENTS		0x03ff
#else
#define BBMC_EVENTS		0x01ff	/* no BBMC_EVENT_RESUME on 0x00090206 */
#endif

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
#if SOURCE_0103
	/* USS-1001's keeps bbmc.c's counters here (one ES for all); the others' are its statics */
	unsigned int		savedMode;	/* the mode as NVS holds it */
	unsigned char		reserved;	/* nothing reads it */
	unsigned char		packets;	/* the packets answered since the start */
	unsigned long		statusSeen;	/* the status bits set since the start */
	unsigned long		eventsSeen;	/* and the events */
#endif
};

#if FW_SINCE(0x0100060B)
/* bbmcGetInfo's (host command 0x0a00, serial command 0x01d0) */
struct BbmcInfo {
	unsigned char	state;		/* Bbmc.state */
	union {
		unsigned char	value;
		struct {
			unsigned char	p126:1;	/* the modem's lines */
			unsigned char	p34:1;
			unsigned char	p32:1;
			unsigned char	p74:1;
			unsigned char	p36:1;
			unsigned char	p143:1;
			unsigned char	reserved:2;
		} bits;
	} lines;
	unsigned char	packets;	/* the packets answered since the start */
	unsigned char	lastSequence;
	unsigned int	modeActive;	/* the registers' */
	unsigned char	version;
	unsigned char	reg14;		/* register 0x14 */
	unsigned char	regs1c[8];	/* registers 0x1c-0x23 */
	unsigned long	statusSeen;	/* the status bits set since the start */
	unsigned long	eventsSeen;	/* and the events */
};
#endif

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
#if FW_SINCE(0x00090502)
extern unsigned char bbmcIsOn(void);
#endif
#if FW_SINCE(0x01000104)
extern unsigned char bbmcModeIsNone(void);
#endif
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
#if FW_SINCE(0x0100060B)
extern void bbmcGetInfo(unsigned char *out);
extern unsigned char hostCommand0a00(unsigned char *out, unsigned char *length);
#endif
extern void bbmcRegsInit(void);
extern unsigned char bbmcWriteRegs(unsigned char reg, unsigned char count, unsigned char *values,
				   unsigned char mode);
extern unsigned char bbmcReadRegs(unsigned char reg, unsigned char count, unsigned char *values,
				  unsigned char keep);
#if FW_SINCE(0x01000104)
extern void bbmcAddStatus(unsigned long bits);
#endif
extern void bbmcSetStatus(unsigned long bits);
extern void bbmcSetEvents(unsigned long bits);
extern void bbmcSetModeActive(unsigned int mode);
extern unsigned char bbmcGetPending(void);
#if (BOARD_USS1001 && SOURCE_0104) || defined(ERNIE_UPDATER)
/* the slim's 0x0105 keeps no version, and the updater has no modem: none */
#define bbmcGetVersion()	0
#else
extern unsigned char bbmcGetVersion(void);
#endif
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
