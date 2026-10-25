/*
 * The NVS's requests: request 0x10 takes the host's commands 0x1080-0x1083 (the NVS's read
 * and write, sceSysconNvsReadData and sceSysconNvsWriteData), and NVS_JOBS slots hold the
 * reads and writes other modules ask for. Both run in task 2, at event flag 3's bits 0x0020
 * and 0x0080.
 */
#include <string.h>

#include "host.h"
#include "kernel.h"
#include "nvsreq.h"

extern unsigned char sub_2BB99(void);
extern unsigned char sub_2BBE9(void);
extern void sub_2C2ED(int, int);
extern unsigned char sub_22542(unsigned int offset, unsigned int size, unsigned char *data,
			       unsigned char area);
extern unsigned char sub_225E1(unsigned int offset, unsigned int size, unsigned char *data,
			       unsigned char area);
extern unsigned char sub_22960(void);
extern unsigned char sub_2296F(void);
extern unsigned char sub_22A9A(void);
extern unsigned char sub_22C98(int);

static struct NvsJob *nvsJobAlloc(void);
static struct NvsJob *nvsJobNext(void);

/* the lowest offset the host may read or write */
const unsigned int nvsHostOffsetMin = 0;

unsigned char nvsRequestAnswer[0x20];
unsigned char nvsRequestData[3 + 0x20];	/* the offset, the size, the data written */
struct HostRequest nvsRequest;
struct NvsJob nvsJobs[NVS_JOBS];

void nvsRequestInit(void)
{
	nvsRequest.id = 0x10;
	nvsRequest.data = nvsRequestData;
	nvsRequest.answer = nvsRequestAnswer;
	nvsRequest.size = sizeof(nvsRequestData);
	nvsRequest.notify = nvsRequestNotify;
	nvsRequest.state = HOST_REQUEST_RECEIVE;
	memset(nvsJobs, 0, sizeof(nvsJobs));
}

/* event flag 3's 0x0020 (the host's command came in) and 0x0080 (a job was queued) */
void nvsRequestEvent(FLGPTN ptn)
{
	unsigned char result;

	if (ptn & 0x0020) {
		unsigned int offset;
		unsigned char size;

		if (!sub_2BB99()) {
			hostRequestAnswer(&nvsRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
			sub_2BBE9();
		} else {
			switch (nvsRequest.command) {
			case 0x1080:
				hostRequestAnswer(&nvsRequest, HOST_REQUEST_ANSWER, 0, 0);
				break;
			case 0x1081:
				if (nvsRequestData[0] == 0x55) {
					result = sub_2296F();
					hostRequestAnswer(&nvsRequest, HOST_REQUEST_ANSWER,
							  result == 0 ? 0 : result, 0);
				} else {
					hostRequestAnswer(&nvsRequest, HOST_REQUEST_ANSWER, 0x3c, 0);
				}
				break;
			case 0x1082:		/* sceSysconNvsReadData */
				if (nvsRequest.length == 3) {
					offset = nvsRequestData[0] | (unsigned int)nvsRequestData[1] << 8;
					size = nvsRequestData[2];
					if (offset >= nvsHostOffsetMin)
						result = sub_22542(offset, size, nvsRequestAnswer,
								   NVS_AREA_HOST);
					else
						result = 0x3c;
					if (result == 0)
						hostRequestAnswer(&nvsRequest, HOST_REQUEST_ANSWER, 0,
								  size);
					else
						hostRequestAnswer(&nvsRequest, HOST_REQUEST_ANSWER,
								  result, 0);
				} else {
					hostRequestAnswer(&nvsRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
				}
				break;
			case 0x1083:		/* sceSysconNvsWriteData */
				if (nvsRequest.length >= 4) {
					offset = nvsRequestData[0] | (unsigned int)nvsRequestData[1] << 8;
					size = nvsRequestData[2];
					if (offset >= nvsHostOffsetMin)
						result = sub_225E1(offset, size, &nvsRequestData[3],
								   NVS_AREA_HOST);
					else
						result = 0x3c;
					hostRequestAnswer(&nvsRequest, HOST_REQUEST_ANSWER, result, 0);
				} else {
					hostRequestAnswer(&nvsRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
				}
				break;
			default:
				hostRequestAnswer(&nvsRequest, HOST_REQUEST_UNKNOWN, 0, 0);
				break;
			}
		}
	}
	if (ptn & 0x0080) {
		struct NvsJob *job;
		unsigned char status;

		while ((job = nvsJobNext()) != NULL) {
			switch (job->state) {
			case NVS_JOB_READ:
				result = sub_22542(job->offset, job->size, job->data, job->area);
				job->done(job->command, result);
				break;
			case NVS_JOB_WRITE:
				result = sub_225E1(job->offset, job->size, job->data, job->area);
				job->done(job->command, result);
				break;
			case NVS_JOB_SERVICE:
				status = sub_22960();
				switch (status) {
				case 0xf0:
					result = sub_2296F();
					break;
				case 0x03:
					result = sub_22A9A();
					if (result == 0x0a)
						;
					break;
				case 0x12:
					result = sub_22C98(2);
					if (result == 0x20)
						;
					break;
				default:
					result = sub_22C98(2);
					if (result == 0x20)
						;
					break;
				}
				break;
			}
			job->state = NVS_JOB_FREE;
		}
	}
}

/* queues a read of size bytes at offset into data; done gets command and the result */
int nvsJobRead(unsigned int offset, unsigned int size, unsigned char *data, unsigned char area,
	       unsigned char command, void (*done)(unsigned char command, unsigned char result))
{
	struct NvsJob *job;

	if (done == NULL)
		return 1;
	job = nvsJobAlloc();
	if (job == NULL)
		return 2;
	job->state = NVS_JOB_READ;
	job->done = done;
	job->command = command;
	job->offset = offset;
	job->size = size;
	job->data = data;
	job->area = area;
	sub_2C2ED(3, 0x0080);
	return 0;
}

int nvsJobWrite(unsigned int offset, unsigned int size, unsigned char *data, unsigned char area,
		unsigned char command, void (*done)(unsigned char command, unsigned char result))
{
	struct NvsJob *job;

	if (done == NULL)
		return 1;
	job = nvsJobAlloc();
	if (job == NULL)
		return 2;
	job->state = NVS_JOB_WRITE;
	job->done = done;
	job->command = command;
	job->offset = offset;
	job->size = size;
	job->data = data;
	job->area = area;
	sub_2C2ED(3, 0x0080);
	return 0;
}

int nvsJobService(void)
{
	struct NvsJob *job;

	job = nvsJobAlloc();
	if (job == NULL)
		return 2;
	job->state = NVS_JOB_SERVICE;
	job->done = NULL;
	job->command = 0;
	sub_2C2ED(3, 0x0080);
	return 0;
}

/* request 0x10's: the command runs in task 2 */
void nvsRequestNotify(void)
{
	switch (nvsRequest.event) {
	case HOST_EVENT_COMMAND:
		hostRequestSetState(&nvsRequest, HOST_REQUEST_BUSY);
		sub_2C2ED(3, 0x0020);
		break;
	case HOST_EVENT_ANSWERED:
		hostRequestSetState(&nvsRequest, HOST_REQUEST_RECEIVE);
		break;
	}
}

static struct NvsJob *nvsJobAlloc(void)
{
	struct NvsJob *job;
	unsigned char i;

	job = NULL;
	dis_dsp();
	for (i = 0; i < NVS_JOBS; i++) {
		if (nvsJobs[i].state == NVS_JOB_FREE) {
			job = &nvsJobs[i];
			break;
		}
	}
	ena_dsp();
	return job;
}

static struct NvsJob *nvsJobNext(void)
{
	struct NvsJob *job;
	unsigned char i;

	job = NULL;
	dis_dsp();
	for (i = 0; i < NVS_JOBS; i++) {
		if (nvsJobs[i].state != NVS_JOB_FREE) {
			job = &nvsJobs[i];
			break;
		}
	}
	ena_dsp();
	return job;
}
