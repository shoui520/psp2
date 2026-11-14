/*
 * The NVS's requests (app/nvsreq.c): the host's commands 0x1080-0x1083 (request 0x10), and
 * the jobs other modules queue, which task 2 runs at event flag 3's bit 0x0080 and answers
 * by their done handler.
 */
#ifndef ERNIE_NVSREQ_H
#define ERNIE_NVSREQ_H

#include "host.h"
#include "kernel.h"

/* the NVS's areas: the host's, and the syscon's own 0x40 bytes after it */
#define NVS_AREA_HOST		0
#define NVS_AREA_SYSCON		1

#if FW_SINCE(0x00090903)
#define NVS_JOBS		3
#else
#define NVS_JOBS		2
#endif

/* a job's state */
#define NVS_JOB_FREE		0
#if FW_SINCE(0x0100060B)
#define NVS_JOB_TAKEN		1	/* nvsJobAlloc's, until the job is filled in */
#define NVS_JOB_READ		2
#define NVS_JOB_WRITE		3
#define NVS_JOB_SERVICE		4
#else
#define NVS_JOB_READ		1
#define NVS_JOB_WRITE		2
#if FW_SINCE(0x00090903)
#define NVS_JOB_SERVICE		3	/* the NVS's upkeep (0x00090804: event flag 3's 0x0040) */
#endif
#endif

/* nvsJobRead's, nvsJobWrite's and nvsJobService's errors: no done, no free job */
#if FW_SINCE(0x0100060B)
#define NVS_JOB_ERROR_DONE	0x30
#define NVS_JOB_ERROR_FULL	0x31
#else
#define NVS_JOB_ERROR_DONE	1
#define NVS_JOB_ERROR_FULL	2
#endif

struct NvsJob {
	unsigned int	offset;
	unsigned int	size;
	unsigned char	*data;
	unsigned char	area;		/* NVS_AREA_* */
	unsigned char	command;	/* passed to done */
#if FW_SINCE(0x0100060B)
	signed char	state;		/* NVS_JOB_*: ready from NVS_JOB_READ */
#else
	unsigned char	state;		/* NVS_JOB_* */
#endif
	void		(*done)(unsigned char command, unsigned char result);
};

extern struct HostRequest nvsRequest;

extern void nvsRequestInit(void);
extern void nvsRequestEvent(FLGPTN ptn);
extern int nvsJobRead(unsigned int offset, unsigned int size, unsigned char *data,
		      unsigned char area, unsigned char command,
		      void (*done)(unsigned char command, unsigned char result));
extern int nvsJobWrite(unsigned int offset, unsigned int size, unsigned char *data,
		       unsigned char area, unsigned char command,
		       void (*done)(unsigned char command, unsigned char result));
#if FW_SINCE(0x00090903)
extern int nvsJobService(void);
#endif
extern void nvsRequestNotify(void);

#endif
