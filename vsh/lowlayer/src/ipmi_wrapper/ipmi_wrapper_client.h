#ifndef IPMI_WRAPPER_CLIENT_H
#define IPMI_WRAPPER_CLIENT_H

#include <stddef.h>
#include <kernel.h>
#include "../ipmi/client.h"

/*
 * IpmiWrapper: an IPMI client whose server signals callbacks through the client's event
 * flags. The owner registers a callback for a bit and polls it with checkCallback, which
 * runs the callback once the server has set the bit. Sony's file:
 * ipmi_wrapper/ipmi_wrapper_client.cpp (0.990's records, with the method names); its log
 * prefix names the library SceIpmiWrapper. The class and field names are ours.
 */

/* Our names for SceIpmiWrapper's error codes (facility 0x0010, 0x801016xx). */
#define IPMI_WRAPPER_ERROR_ALREADY_CONNECTED	-2146429439	/* 0x80101601 */
#define IPMI_WRAPPER_ERROR_NOT_CONNECTED	-2146429438	/* 0x80101602 */
#define IPMI_WRAPPER_ERROR_NO_CLIENT		-2146429437	/* 0x80101603 */
#define IPMI_WRAPPER_ERROR_INVALID_INDEX	-2146429436	/* 0x80101604 */
#define IPMI_WRAPPER_ERROR_TOO_MANY_SESSIONS	-2146429433	/* 0x80101607 */
#define IPMI_WRAPPER_ERROR_NO_CALLBACK		-2146429431	/* 0x80101609 */
#define IPMI_WRAPPER_ERROR_NOT_SIGNALED		-2146429430	/* 0x8010160A */

namespace IpmiWrapper {

class Client {
public:
	struct Config {
		char name[16];
		SceUInt32 reserved;
		SceUInt32 syncCallCount;
		SceSize maxSyncCallInSize;
		SceSize maxSyncCallOutSize;
		SceUInt32 asyncCallCount;
		SceSize maxAsyncCallInSize;
		SceSize maxAsyncCallOutSize;
		SceUInt32 callbackCount;
		SceUInt32 msgQueueCount;
		struct {
			SceSize msgSize;
			SceUInt32 msgCount;
		} msgQueue[32];
		int budget;
	};

	/* What checkCallback runs once the server has set the callback's bit. */
	typedef int (*Callback)(SceUInt32 index, void *pArg);

	static int create(Client **ppClient, const Config *pConfig, void *pMemory);
	static SceSize getBufferSize(const Config *pConfig);

	int initialize(const Config *pConfig);
	int destroy();
	int connect();
	int disconnect();
	void cancel_callback_nolock(SceUInt32 index);
	int registerCallback(SceUInt32 index, Callback callback, void *pArg);
	int unregisterCallback(SceUInt32 index);
	int checkCallback(SceUInt32 index);
	int cancelCallback(SceUInt32 index);
	IPMI::Client *getClient();

private:
	struct CallbackEntry {
		Callback callback;
		void *pArg;
	};

	SceKernelLwMutexWork m_mutex;
	IPMI::Client *m_pClient;
	int m_connected;
	CallbackEntry *m_pCallbacks;
	SceUInt32 m_callbackCount;
};

}

#endif
