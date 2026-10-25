/*
 * The host's requests (host.h): the table of every request a task answers, which the host
 * link searches by the request's number, and their reset at start.
 */
#include <string.h>

#include "command.h"
#include "host.h"

extern struct HostRequest hostRequestFF57E;
extern struct HostRequest hostRequestFFC3A;
extern struct HostRequest hostRequestFEA82;
extern struct HostRequest hostRequestFEA98;
extern struct HostRequest hostRequestFEA6C;
extern struct HostRequest hostRequestFEDDC;
extern struct HostRequest hostRequestFEE86;
extern struct HostRequest hostRequestFF15E;
extern struct HostRequest hostRequestFF174;
extern struct HostRequest hostRequestFFBE0;

struct HostRequest * const hostRequests[] = {
	&commandRequest,
	&hostRequestFF57E,
	&hostRequestFFC3A,
	&hostRequestFEA82,
	&hostRequestFEA98,
	&hostRequestFEA6C,
	&hostRequestFEDDC,
	&hostRequestFEE86,
	&hostRequestFF15E,
	&hostRequestFF174,
	&hostRequestFFBE0,
};

#define HOST_REQUESTS		(sizeof(hostRequests) / sizeof(hostRequests[0]))

unsigned int hostRequestCount(void)
{
	return HOST_REQUESTS;
}

void hostRequestInit(void)
{
	unsigned char i;
	struct HostRequest * const *p;

	p = hostRequests;
	for (i = 0; i < HOST_REQUESTS; i++) {
		memset(*p, 0, sizeof(struct HostRequest));
		p++;
	}
}
