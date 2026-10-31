#ifndef IPMI_CLIENT_H
#define IPMI_CLIENT_H

#include <stddef.h>
#include <kernel.h>
#include "psp2/msg_pipe.h"

namespace IPMI {

/*
 * The client end of an IPMI connection. Sony's class: IPMI::Client, its implementation
 * IPMI::impl::ClientImpl (client_impl.cpp); the method names are 0.990's.
 */
class Client {
public:
	/* What the client allocates: queue depths and the largest message of each kind. */
	struct Config {
		char serviceName[16];
		SceUInt32 reserved;
		SceUInt32 syncCallCount;
		SceSize maxSyncCallInSize;
		SceSize maxSyncCallOutSize;
		SceUInt32 asyncCallCount;
		SceSize maxAsyncCallInSize;
		SceSize maxAsyncCallOutSize;
		SceUInt32 eventFlagCount;
		SceUInt32 msgQueueCount;
		SceSize msgQueueBufferSize[32];
		int budget;
		int openLimit;

		Config()
		{
			syncCallCount = 1;
			maxSyncCallInSize = 0xF00;
			maxSyncCallOutSize = 0xF00;
			asyncCallCount = 1;
			maxAsyncCallInSize = 0xF00;
			maxAsyncCallOutSize = 0xF00;
			eventFlagCount = 1;
			msgQueueCount = 0;
			for (int i = 0; i < 32; i++)
				msgQueueBufferSize[i] = 0;
			budget = DEFAULT_BUDGET;
			openLimit = -1;
			serviceName[0] = '\0';
		}
	};

	Client();

	static int create(Client **ppClient, const Config *pConfig, void *pUserData,
			  void *pMemory);
	static SceSize getBufferSize(const Config *pConfig);

	virtual int destroy() = 0;
	virtual int connect(const void *pParam, SceSize paramSize, int *pResult) = 0;
	virtual int disconnect() = 0;
	virtual int terminateConnection() = 0;
	virtual int invokeSyncMethod(SceUInt32 methodId, const void *pIn, SceSize inSize,
				     int *pResult, void *pOut, SceSize *pOutSize,
				     SceSize outSize) = 0;
	virtual int invokeSyncMethod(SceUInt32 methodId, const DataInfo *pInList,
				     SceUInt32 inCount, int *pResult, BufferInfo *pOutList,
				     SceUInt32 outCount) = 0;
	virtual int invokeAsyncMethod(SceUInt32 methodId, const void *pIn, SceSize inSize,
				      int *pRequestId) = 0;
	virtual int invokeAsyncMethod(SceUInt32 methodId, const DataInfo *pInList,
				      SceUInt32 inCount, int *pRequestId) = 0;
	virtual int tryGetResult(SceUInt32 methodId, int requestId, int *pResult,
				 void *pOut, SceSize outSize, SceSize *pOutSize) = 0;
	virtual int tryGetResult(SceUInt32 methodId, int requestId, int *pResult,
				 BufferInfo *pOutList, SceUInt32 outCount) = 0;
	virtual int pollEventFlag(SceUInt32 index, SceUInt32 bitPattern, SceUInt32 waitMode,
				  SceUInt32 *pResultPattern) = 0;
	virtual int waitEventFlag(SceUInt32 index, SceUInt32 bitPattern, SceUInt32 waitMode,
				  SceUInt32 *pResultPattern, SceUInt32 *pTimeout) = 0;
	virtual void *getUserData() = 0;
	virtual int getMsg(SceUInt32 index, void *pBuffer, SceSize *pSize, SceSize bufferSize,
			   SceUInt32 *pTimeout) = 0;
	virtual int tryGetMsg(SceUInt32 index, void *pBuffer, SceSize *pSize,
			      SceSize bufferSize) = 0;

	virtual ~Client();
};

}

#endif
