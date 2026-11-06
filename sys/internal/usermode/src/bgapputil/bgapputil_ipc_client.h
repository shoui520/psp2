/* the shell's background application methods (bgapputil_ipc_client.cpp; the names are ours) */
#ifndef BGAPPUTIL_BGAPPUTIL_IPC_CLIENT_H
#define BGAPPUTIL_BGAPPUTIL_IPC_CLIENT_H

#include <scetypes.h>
#include "../../../../../vsh/lowlayer/src/ipmi/client.h"

/* asks the shell to start the background application; the result comes with the event flag */
int bgAppUtilRequestStart(IPMI::Client *client, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag);
/* the request's result, once its event flag is set */
int bgAppUtilGetStartResult(IPMI::Client *client, int requestId);

#endif
