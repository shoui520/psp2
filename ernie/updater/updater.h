/*
 * The updater's own functions and data, which the application has in other forms or not at
 * all; its stubs (stub.c) take the names of the application's functions they stand for.
 */
#ifndef ERNIE_UPDATER_H
#define ERNIE_UPDATER_H

/* PCH-1000's 0x0100060D forms (and the later updaters'): PDEL 0x00416000's 0x0100060D, the same
   version, keeps the earlier ones where its board differs (stub.c's pins, mainLoop's start) */
#define UPD_060D_FORMS	(FW_SINCE(0x0100060D) && !HW_IS(0x00416000))

/*
 * the version blocks as the updater reads them: each with the other's layout, the
 * application's at 0xf00 as the updater's (its version at 8, its time stamp at 0xc) and the
 * updater's at 0x1f00 as the application's (4 and 8)
 */
#define APP_VERSION_INFO	((const struct UpdaterVersionInfo __far *)0xf00)

/* the image's version block, in boot cluster 1 until the clusters are swapped: read as the
   application's, its time stamp right after it */
#define IMAGE_VERSION_INFO	((const struct AppVersionInfo __far *)0x1f00)

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

/* the events (updEventSet) */
union UpdEvents {
	unsigned long	value;
	struct {
		unsigned int	event01:1;
		unsigned int	event02:1;
		unsigned int	event03:1;
		unsigned int	event04:1;
		unsigned int	event05:1;
		unsigned int	event0a:1;
		unsigned int	event14:1;
		unsigned int	event15:1;
		unsigned int	event32:1;
		unsigned int	event33:1;
		unsigned int	event34:1;
		unsigned int	event64:1;
		unsigned int	event67:1;
		unsigned int	event69:1;
		unsigned int	event66:1;
		unsigned int	event78:1;
		unsigned int	event79:1;
		unsigned int	event7a:1;
		unsigned int	event16:1;
		unsigned int	event1e:1;
		unsigned int	event1f:1;
		unsigned int	event20:1;
		unsigned int	event35:1;
		unsigned int	event82:1;
#if SOURCE_0103
#if BOARD_USS1001
		/* the slim's: 0x83, 0x96, 0x97, 0x17 and 0x18 */
		unsigned int	event83:1;
		unsigned int	reserved1:1;
		unsigned int	event96:1;
#else
		/* the PS TV's: 0x8c, 0x97, 0x17 and 0x18 */
		unsigned int	reserved1:1;
		unsigned int	event8c:1;
		unsigned int	reserved2:1;
#endif
		unsigned int	event97:1;
		unsigned int	event17:1;
		unsigned int	event18:1;
		unsigned int	reserved3:2;
#else
		unsigned int	reserved:8;
#endif
	} bits;
};

extern unsigned char updMode;
extern unsigned char updReset;
#if SOURCE_0103
extern unsigned char updImageChecked;
extern void updaterGetStatus(unsigned char *out);
#endif
extern unsigned char updVersion[4];
extern char updBuild[12];
extern union UpdInput updInput;
extern unsigned long updLoops;
extern union UpdEvents updEvents;
extern unsigned int buttonPowerHeld;
extern unsigned int buttonPsHeld;
extern unsigned long buttonPolls;
extern unsigned int buttonShortTime;
extern unsigned int buttonHoldTime1;
extern unsigned int buttonHoldTime2;
extern char logBuffer[0x80];
extern int updaterInitErnieUpdMode(void);
extern unsigned char updModeGet(void);
extern unsigned char updModeChange(unsigned char mode);
extern unsigned char updEventTake(unsigned char event);
extern unsigned char updEventSet(unsigned char event);
extern void buttonInit(void);
extern unsigned char buttonPoll(void);
extern int inputPoll(void);
#if FW_SINCE(0x0100060D)
extern unsigned char updMultiCn;	/* what is on the multi connector (analogReadMultiCn's) */
extern int multiCnCheck(void);
#endif
extern int multiCnChanged(void);
extern int p77Changed(void);

/* the host link (host.c) */
#if FW_SINCE(0x0100060D)
/* since 0x0100060D the application's HostLinkConfig */
struct UpdHostLinkConfig {
	unsigned int	size:4;
	unsigned int	level:12;
};
#endif

struct UpdHostLink {
	unsigned char	statusBit1;	/* the answers' status bit 1 (0x02): the last command's
					   checksum was wrong */
	unsigned char	statusBit5;	/* and bit 5 (0x20): slow commands queued */
#if FW_SINCE(0x0100060D)
	struct UpdHostLinkConfig config;
#else
	unsigned int	config;		/* HostLinkConfig's */
#endif
#if SOURCE_0103
	unsigned int	frameSize;	/* the PS TV's: a word, as the application's */
#else
	unsigned char	frameSize;
	unsigned char	reserved;
#endif
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

/* hostSlowCommand's states */
#define HOST_SLOW_QUEUED	1	/* runs from the main loop */
#define HOST_SLOW_DONE		2	/* its answer is ready */

#define HOST_SLOW_COMMANDS	3

/* the image check's progress (0x1184's handler) */
struct HostSlowHash {
#if SOURCE_0103
	unsigned int	segment;	/* the next segment to hash (the PS TV's a word) */
#else
	unsigned char	segment;	/* the next segment to hash */
#endif
	unsigned char	segmentDone;	/* the segments hashed */
	unsigned char	segmentCount;	/* the segments received */
};

/* a queued slow command */
struct HostSlowCommand {
	unsigned int	command;	/* 0: the entry is free */
	struct HostAnswerFrame answer;	/* the handler's, sent from its length on */
	struct HostCommandFrame frame;	/* the command, for the handler */
	struct HostSlowHash hash;
#if SOURCE_0103
	unsigned char	reserved[0x0b];
#else
	unsigned char	reserved[0x0c];
#endif
	void		(*handler)(struct HostSlowCommand *slow);	/* run from the main loop */
	signed char	state;		/* HOST_SLOW_* */
	unsigned char	reserved2;
};

extern struct UpdHostLink hostLink;
extern struct HostCommandFrame hostRx;
extern struct HostAnswerFrame hostTx;
extern struct HostSlowCommand hostSlowCommands[HOST_SLOW_COMMANDS];
#if FW_SINCE(0x0100060D)
extern int hostInit(void);
#endif
extern int hostOpen(void);
extern int hostClose(void);
extern unsigned char hostPoll(void);
/* the serial links' timers (uart4.c and serialevent.c's, uart.c and serial.c's): the PS TV's
   two below the others' */
#if SOURCE_0103
#define SERIAL4_TIMER		0x0d
#define SERIAL_TIMER		0x0e
#else
#define SERIAL4_TIMER		TIMER(0x11)
#define SERIAL_TIMER		TIMER(0x12)
#endif

extern int csiTransferPoll(unsigned char *rx, unsigned char *tx, unsigned char count);
extern int uart3WritePoll(unsigned char *data, unsigned char count);
extern void logPrintf(const char *format, ...);
/* a log line, as UPD_LOG((format, ...)); 0x0100060D's updater logs nothing */
#if FW_SINCE(0x0100060D)
#define UPD_LOG(args)
#else
#define UPD_LOG(args)	logPrintf args
#endif
extern unsigned char hostChecksum(unsigned char *data, unsigned char length);
#if SOURCE_0103
/* the PS TV's: in version.c, as the application's */
extern int copyTimeStamp(char *out, const char *stamp);
#else
extern int timeStampDigits(char *digits, const char *stamp);
#endif
extern struct HostSlowCommand *hostSlowFind(unsigned int command);
extern struct HostSlowCommand *hostSlowAlloc(unsigned int command);
extern int hostSlowFree(struct HostSlowCommand *slow);
extern unsigned char hostSlowPending(void);
extern void hostSlowRun(void);

/* stub.c */
#if FW_SINCE(0x0100060D)
/* since 0x0100060D the updater's events take the transition alone, as the slim's stateTransition */
extern int updStateEvent(unsigned char transition);
#define UPD_STATE_EVENT_LOCAL
#define UPD_STATE_EVENT(t)	updStateEvent(t)
#else
struct StateEvent;
extern int updStateEvent(struct StateEvent *event);
#define UPD_STATE_EVENT_LOCAL	struct StateEvent event;
#define UPD_STATE_EVENT(t)	(event.transition = (t), event.wakeMask = stateGetWakeMask(), \
				 updStateEvent(&event))
#endif
extern int input2Bit0Get(void);
extern int input2Bit1Get(void);
extern int input3Bit0High(void);
#if FW_SINCE(0x0100060D)
extern unsigned char input1Bit5Active(void);
#else
extern int input1Bit5Active(void);
#endif
#if UPD_060D_FORMS
extern int intp11Low(void);
#else
extern unsigned char intp11Low(void);
#endif
extern int chargeVbusCharging(void);
extern void powerWaitLong(unsigned int ms);
extern int bbmcStart(void);
extern int bbmcSetOff(void);
extern void bbmcStop(void);
extern void bbmcWake(void);
extern int bbmcGetMode(void);

/* power.c */
extern unsigned char powerCleared[2];
extern unsigned char levelTrim(unsigned char level, unsigned char trim);
extern void powerInit(void);
extern void powerOn(void);
#if SOURCE_0103
/* the PS TV's: the SPI device's key exchange on the way from mode 9 to 3 (our name) */
extern void powerCsiOpen(void);
extern void powerResume(unsigned char mode);
#else
extern void powerResume(void);
#endif
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

#if FW_SINCE(0x0100060D)
extern __near unsigned char resetFlags;
extern __near unsigned long idRecord1;
extern __near unsigned int idRecordsLength;

/* analog.c: the multi connector's level, from the A/D converter */
extern unsigned char analogReadMultiCn(void);
#else
extern unsigned char resetFlags;
#endif
extern void rtcInit(void);
extern unsigned char configStorageInit(void);

/* handshake.c: the host's handshake, mode 0x0b only */
#define HANDSHAKE_LOCKED	0x5a5a
#define HANDSHAKE_UNLOCKED	0xa5a5
#if !FW_SINCE(0x0100060D)
extern int handshakeInit(void);
#endif
extern unsigned char handshakeSetMode(unsigned char state, unsigned char mode);
extern unsigned char handshakeCommand(unsigned char *data);
extern unsigned char handshakeState(void);
extern unsigned char handshakeUnlocked11(void);
extern unsigned char handshakeRefuse(void);
extern void handshakeSetUnlocked(unsigned int unlocked);

/* command.c: the host's commands, and the update's */
/* the segment programmed ("block"); the PS TV's a word */
#if SOURCE_0103
extern unsigned int updSegment;
#else
extern unsigned char updSegment;
#endif
extern unsigned char updCipher[0x400];	/* its data as it comes in */
#if BOARD_USS1001
#define UPD_SEGMENTS	0x180	/* the slim's 384 KiB of flash, in segments of 1 KiB */
#else
#define UPD_SEGMENTS	0x100
#endif
extern unsigned char updReceived[UPD_SEGMENTS];	/* the segments programmed */
extern unsigned char updProgress;	/* the image check's, in percent */
extern unsigned char updaterInit(void);
extern int updaterDecryptInit(void);
#if FW_SINCE(0x0100060D)
extern unsigned int hostCommandFast(unsigned int command);
extern unsigned int hostCommandSlow(unsigned int command, struct HostSlowCommand *slow);
#else
extern unsigned char hostCommandFast(unsigned int command);
extern unsigned char hostCommandSlow(unsigned int command, struct HostSlowCommand *slow);
#endif
extern unsigned char serialSetLevel(unsigned char level);

#endif
