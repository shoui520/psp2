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
extern unsigned char updMode;
extern unsigned char updReset;
extern unsigned char updVersion[4];
extern char updBuild[12];
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

extern unsigned char resetFlags;
extern void rtcInit(void);
extern unsigned char configStorageInit(void);
extern unsigned char updaterInit(void);
extern void sub_969D(int);
extern void sub_7E89(void);
extern unsigned char sub_7A2B(void);
extern void sub_3B1AE(void);
extern void sub_3598A(void);
extern void sub_3B604(unsigned int);
extern int sub_3B718(void);
extern void sub_6B98(void);
extern void sub_6BD0(void);
extern void sub_AD21(void);
extern void sub_A026(void);
extern void sub_3AC35(void);
extern unsigned char sub_3B175(void);
extern void sub_9CA4(int);
extern void sub_8AE4(void);
extern void sub_9CBF(void);
extern unsigned char sub_4D63(void);
extern void sub_AC3C(int);
extern void sub_AC65(void);
extern void sub_3AF74(void);
extern void sub_A1E5(void);
extern void sub_A219(void);
extern void sub_A007(void);
extern void sub_6B2B(void);
extern void sub_6B42(void);
extern void sub_94A1(int);
extern void sub_A065(int);
extern void sub_723E(void);

#endif
