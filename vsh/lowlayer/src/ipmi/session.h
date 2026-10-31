#ifndef IPMI_SESSION_H
#define IPMI_SESSION_H

#include <stddef.h>
#include <kernel.h>

namespace IPMI {

/* The server's end of one client's connection (Sony's IPMI::Session, session_impl.cpp). */
class Session {
public:
	/* What the server allocates for one client: the client's Config, as the server sees it. */
	struct Config {
		SceUInt32 sessionParam[2];
		SceUInt32 requestCount;
		SceSize maxRequestSize;
		SceSize maxResponseSize;
		SceUInt32 syncCallCount;
		SceSize maxSyncCallInSize;
		SceSize maxSyncCallOutSize;
		SceUInt32 asyncCallCount;
		SceUInt32 msgQueueCount;
		SceSize msgQueueBufferSize[32];
		int budget;
	};

	static SceSize getBufferSize(const Config *pConfig);
};

}

#endif
