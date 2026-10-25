/*
 * The updater's own functions and data, which the application has in other forms or not at
 * all. PLACEHOLDER: the sub_ names are functions not written yet, named when they are.
 */
#ifndef ERNIE_UPDATER_H
#define ERNIE_UPDATER_H

/* main.c */
extern void startHook(void);
extern int csiOpen(void);
extern void csiClose(void);
extern int uartsOpen(void);

/* updater.c */
/* the inputs inputPoll follows: each level, its last, and for P2.0 the one reported */
union UpdInput {
	unsigned char	value;
	struct {
		unsigned char	multiCn:1;	/* P2.0 low: something on the multi connector */
		unsigned char	multiCnLast:1;
		unsigned char	multiCnReported:1;
		unsigned char	p77:1;		/* P7.7 */
		unsigned char	p77Last:1;
		unsigned char	p73:1;		/* P7.3 */
		unsigned char	p73Last:1;
		unsigned char	reserved:1;
	} bits;
};

extern unsigned char updMode;
extern unsigned char updReset;
extern unsigned char updVersion[4];
extern char updBuild[12];
extern union UpdInput updInput;
extern int updaterInitErnieUpdMode(void);
extern unsigned char updModeGet(void);
extern unsigned char updModeChange(unsigned char mode);
extern unsigned char updEventTake(unsigned char event);
extern unsigned char updEventSet(unsigned char event);
extern void buttonInit(void);
extern unsigned char buttonPoll(void);
extern int inputPoll(void);

/* the host link (host.c) */
struct UpdHostLink {
	unsigned char	statusBit1;	/* the answers' status bit 1 (0x02): the last command's
					   checksum was wrong */
	unsigned char	statusBit5;	/* and bit 5 (0x20): slow commands queued */
	unsigned int	config;		/* HostLinkConfig's */
	unsigned char	frameSize;
	unsigned char	reserved;
	unsigned int	resetWait;	/* the rounds a reset waits for the slow commands */
};

/* a frame's command, as the host sends it */
struct HostCommandFrame {
	unsigned int	command;
	unsigned char	length;		/* of data, the checksum included */
	unsigned char	data[0x7d];
};

/* and the answer */
struct HostAnswerFrame {
	unsigned char	status;
	unsigned char	reserved;
	unsigned char	length;		/* of result and data, the checksum included */
	unsigned char	result;
	unsigned char	data[0x7c];
};

/* PLACEHOLDER: a queued slow command; only the fields read so far */
struct HostSlowCommand {
	unsigned int	command;	/* 0: the entry is free */
	struct HostAnswerFrame answer;	/* the handler's, sent from its length on */
	unsigned char	reserved[0x90];
	void		(*handler)(struct HostSlowCommand *slow);	/* run from the main loop */
	signed char	state;		/* HOST_SLOW_* */
	unsigned char	reserved2;
};

extern struct UpdHostLink hostLink;
extern unsigned char logMode;		/* 3: the log goes out */
extern int hostOpen(void);
extern int hostClose(void);
extern unsigned char hostPoll(void);
extern int csiTransferPoll(unsigned char *rx, unsigned char *tx, unsigned char count);
extern int uart3WritePoll(unsigned char *data, unsigned char count);
extern void logPrintf(const char *format, ...);
extern unsigned char hostChecksum(unsigned char *data, unsigned char length);
extern int timeStampDigits(char *digits, const char *stamp);
extern struct HostSlowCommand *hostSlowFind(unsigned int command);
extern struct HostSlowCommand *hostSlowAlloc(unsigned int command);
extern int hostSlowFree(struct HostSlowCommand *slow);
extern unsigned char hostSlowPending(void);
extern void hostSlowRun(void);
extern unsigned char hostCommandFast(unsigned int command);
extern unsigned char hostCommandSlow(unsigned int command, struct HostSlowCommand *slow);
extern void sub_3B721(void);

/* stub.c */
struct StateEvent;
extern int updStateEvent(struct StateEvent *event);
extern int sub_4D3F(void);
extern int sub_4D47(void);
extern int sub_4D4F(void);
extern int sub_4D57(void);
extern unsigned char sub_4D63(void);
extern int sub_4D6B(void);
extern void sub_4DC5(unsigned int ms);
extern int sub_4E1F(void);
extern int sub_4E21(void);
extern void sub_4E23(void);
extern void sub_4E24(void);
extern int sub_4E25(void);

/* power.c */
extern unsigned char powerCleared[2];
extern unsigned char levelTrim(unsigned char level, unsigned char trim);
extern void powerInit(void);
extern void powerOn(void);
extern void powerResume(void);
extern void powerOff(void);
extern void powerSuspend(void);
extern void powerWaitStop(void);
extern void powerDelayMs(unsigned int ms);
extern void keyPortsEnable(void);
extern void keyPortsDisable(void);

/* powerwait.c: the waits polled */
extern unsigned int powerTimers;
extern void powerWaitInit(void);
extern void powerWaitMs(unsigned int ms);
extern void powerSleep(unsigned int ms);
extern void powerSleep2(unsigned int ms);
extern void powerWaitInput(unsigned char edges, unsigned int timeout);
extern void powerWaitTimers(unsigned int bits);
extern void powerStartTimer(unsigned int bits, unsigned char id, unsigned int ticks);

extern unsigned char resetFlags;
extern void rtcInit(void);
extern unsigned char configStorageInit(void);
extern unsigned char updaterInit(void);
extern void sub_3B1AE(void);
extern void sub_3598A(void);
extern void sub_3B604(unsigned int);
extern int sub_3B718(void);
extern void sub_AD21(void);
extern void sub_3AC35(void);
extern unsigned char sub_3B175(void);
extern void sub_AC3C(int);
extern void sub_AC65(void);
extern void sub_3AF74(void);
extern void sub_A1E5(void);
extern void sub_A219(void);
extern void sub_A065(int);
extern unsigned char sub_A5F9(unsigned char level);

#endif
