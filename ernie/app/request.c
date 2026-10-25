/*
 * The host's requests (host.h): the table of every request a task answers, which the host
 * link searches by the request's number, and their reset at start.
 */
#include <string.h>

#include "bbmc.h"
#include "command.h"
#include "configreq.h"
#include "host.h"
#include "inputreq.h"
#include "nvsreq.h"
#include "powerreq.h"
#include "serialreq.h"
#include "updatereq.h"

struct HostRequest * const hostRequests[] = {
	&commandRequest,
	&bbmcRequest,
	&serialRequest,
	&touchRequest,
	&motionRequest,
	&controlsRequest,
	&nvsRequest,
	&updaterRequest,
	&powerRequest,
	&batteryRequest,
	&configRequest,
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
