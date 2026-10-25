/*
 * The host link (app/host.c): the frames the host (the main processor) exchanges with the
 * syscon over SPI, the host's commands, and the requests that tasks answer later.
 */
#ifndef ERNIE_HOST_H
#define ERNIE_HOST_H

/* the link's settings, set by the host */
struct HostLinkConfig {
	unsigned int	size:4;		/* the frame's size: 0 32 bytes, 1 64, 2 128 */
	unsigned int	level:12;	/* 0 or 1: at 1 some answers carry more */
};

/* a request's states */
#define HOST_REQUEST_IDLE	0	/* no command expected */
#define HOST_REQUEST_RECEIVE	1	/* the next command is taken in */
#define HOST_REQUEST_BUSY	2	/* the command is handled */
#define HOST_REQUEST_ANSWER	6	/* the answer is ready */
#define HOST_REQUEST_ERROR	8	/* the command failed */
#define HOST_REQUEST_UNKNOWN	0x10	/* hostRequestAnswer: the command is not known (status
					   0x3f) */

/* a request's events, for its notify handler */
#define HOST_EVENT_NONE		0
#define HOST_EVENT_COMMAND	1	/* a command came in */
#define HOST_EVENT_ANSWERED	2	/* the answer went out */

/*
 * A request: a command the host sends with its number (the frame's second byte, with bit 7 of
 * the first set), handled by a task, then polled for its answer. hostRequests lists them.
 */
struct HostRequest {
	unsigned char	id;		/* the request's number */
	unsigned char	event;		/* HOST_EVENT_* */
	unsigned char	state;		/* HOST_REQUEST_* */
	unsigned char	length;		/* the command's data length */
	unsigned int	command;	/* the command */
	unsigned char	status;		/* the answer's status */
	unsigned char	answerLength;	/* the answer's length */
	unsigned char	offset;		/* how much of the answer went out */
	unsigned char	size;		/* data's size */
	unsigned char	*data;		/* the command's data */
	unsigned char	*answer;	/* the answer's data */
	void		(*notify)(void);	/* called on an event */
};

/*
 * A stream: a command's answer longer than a frame, sent in pieces from one of its blocks
 * (the command table points to it, type 1).
 */
struct HostStream {
	unsigned char	first;		/* the block a new answer starts from */
	unsigned char	block;		/* the block being sent (0xff: none) */
	unsigned char	state;		/* 1: not ready, 3: ready */
	unsigned char	length;		/* the answer's length */
	unsigned char	offset;		/* how much of it went out */
	unsigned char	*blocks[2];
	void		(*prepare)(void);	/* called before a new answer */
};

/* the link's flags, sent in the answer's first byte */
union HostFlags {
	unsigned char	value;
	struct {
		unsigned char	overflow:1;	/* a frame did not fit, or polled no request */
		unsigned char	checksum:1;	/* a command's checksum was wrong */
		unsigned char	reserved1:3;
		unsigned char	answer:1;	/* a request's answer is ready */
		unsigned char	reserved2:2;
	} bits;
};

/* the link */
struct HostLink {
	unsigned int	frameMax;	/* the frames' largest size */
	struct HostLinkConfig config;
	unsigned char	frameSize;	/* the frame's size from config */
	unsigned char	state;		/* the frame's state (host.c) */
	unsigned char	*tx;		/* the host's command */
	unsigned char	*rx;		/* the answer */
	unsigned char	position;	/* the bytes of the frame transferred */
	union HostFlags	flags;
	unsigned int	frameCount;
	struct HostRequest *request;	/* the request the frame polls */
	unsigned char	requestActive;
	unsigned char	requestMismatch;	/* the command is not the request's */
	unsigned char	txBuffer[0x80];
	unsigned char	rxBuffer[0x80];
};

extern struct HostLink hostLink;

/* the requests (request.c) */
extern struct HostRequest * const hostRequests[];

extern unsigned int hostRequestCount(void);
extern void hostRequestInit(void);

extern void hostIntInit(void);
extern void hostIntEnable(void);
extern void hostIntDisable(void);
extern void hostIntCancel(void);
extern int hostInit(void);
extern unsigned char hostSetLinkConfig(struct HostLinkConfig config);
extern int hostGetLinkConfig(struct HostLinkConfig *config);
extern int hostGetLinkSize(unsigned char *size);
extern int hostGetLinkLevel(unsigned int *level);
extern void hostRequestSetState(struct HostRequest *request, unsigned char state);
extern void hostRequestAnswer(struct HostRequest *request, unsigned char state,
			      unsigned char status, unsigned char length);
extern void hostRequestAnswerReady(unsigned char *state);

#endif
