/*
 * swagner's request dispatcher for the manager's commands (DTCPIP-MW/Src/Mw/
 * SwMwManagerDispatch.cpp, from its assert and error strings): the session commands the
 * library's IPC sends (0x2101 connect, 0x2102 disconnect, 0x2113..0x2118), the 0x1101
 * device connect and the key manager's commands. Each handler fills the channel's request
 * context, calls the manager (SwMwManager.cpp) and answers through the server's reply.
 */

#include <stddef.h>
#include <scetypes.h>
#include "../Comm/vita/SwMwCommServer.h"

#define SWMW_MANAGER_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-MW\\Src\\Mw\\SwMwManagerDispatch.cpp"

extern "C" {
int swMwTrace(const char *format, ...);

/* the manager object's session methods (SwMwManager.cpp) */
int SwMwManagerSessionConnect(int *device, const int *values, void *payload, int size);
int SwMwManagerSessionDisconnect(int device);
int SwMwManagerSessionStatus(int device);
int SwMwManagerSessionResult(int device, int *out);
int SwMwManagerSessionClose(int device);
int SwMwManagerSessionOpen(int device);
int SwMwManagerSessionWaitReady(int device, int *out);
int SwMwManagerSessionDestroy(int device);
int SwMwManagerConnectDevice(int device);

/* the key manager's commands: PLACEHOLDER: cryptographic routine, deferred (the key
 * manager's object model is the deferred DRM/authentication batch) */
int SwMwManagerKeystoreCommand(struct SwMwCommChannel *channel, struct SwMwIpcMessage *request,
			       void *argument, int command);
}



/* 0x2101: connect the manager to the library's session */
extern "C" int SwMwManagerConnect(struct SwMwCommChannel *channel, struct SwMwIpcMessage *request,
			      void *argument)
{
	struct SwMwManagerContextBlock context;
	int values[4];
	int *device;
	int result;
	int reply;

	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 162);
	}
	if (*device != 0) {
		SwMwCommServerReplyStatus(channel, request, 12004);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MANAGER_SOURCE, 168);
	}
	values[0] = request->value0;
	values[1] = request->value1;
	values[2] = request->value2;
	values[3] = request->value3;
	result = SwMwManagerSessionConnect(device, values, request->payload, request->size);
	if (result != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_MANAGER_SOURCE, 184);
	else
		SwMwCommServerSetRequestValue((struct SwMwCommRequest *)channel, context.head);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 192);
	return 0;
}

/* 0x2102: disconnect the manager */
extern "C" int SwMwManagerDisconnect(struct SwMwCommChannel *channel,
				 struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int result;
	int reply;

	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 206);
	}
	if (*device == 0) {
		SwMwCommServerReplyStatus(channel, request, 12004);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MANAGER_SOURCE, 212);
	}
	result = SwMwManagerSessionDisconnect(*device);
	if (result != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_MANAGER_SOURCE, 217);
	*device = 0;
	SwMwCommServerSetRequestValue((struct SwMwCommRequest *)channel, context.head);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 226);
	return 0;
}

/* 0x2113: the session's status, or 12003 when the request names no stream */
extern "C" int SwMwManagerGetSessionStatus(struct SwMwCommChannel *channel,
				       struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int result;
	int reply;

	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 850);
	}
	result = SwMwManagerSessionStatus(*device);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 861);
	return 0;
}

/* 0x2114: the session status's value in out0 */
extern "C" int SwMwManagerGetSessionResult(struct SwMwCommChannel *channel,
				       struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int value;
	int result;
	int reply;

	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 875);
	}
	result = SwMwManagerSessionResult(*device, &value);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	request->out0 = value;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 887);
	return 0;
}

/* 0x2115: close the session */
extern "C" int SwMwManagerCloseSession(struct SwMwCommChannel *channel,
				   struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int result;
	int reply;

	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 899);
	}
	result = SwMwManagerSessionClose(*device);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 910);
	return 0;
}

/* 0x2116: open the session */
extern "C" int SwMwManagerOpenSession(struct SwMwCommChannel *channel,
				  struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int result;
	int reply;

	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 922);
	}
	result = SwMwManagerSessionOpen(*device);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 933);
	return 0;
}

/* 0x2117: wait for the session to be ready */
extern "C" int SwMwManagerWaitReady(struct SwMwCommChannel *channel,
				struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int value;
	int result;
	int reply;

	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 241);
	}
	result = SwMwManagerSessionWaitReady(*device, &value);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	request->out0 = value;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 253);
	return 0;
}

/* 0x2118: destroy the session */
extern "C" int SwMwManagerDestroy(struct SwMwCommChannel *channel,
			      struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int result;
	int reply;

	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwCommServerReplyStatus(channel, request, 12003);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MANAGER_SOURCE, 266);
	}
	result = SwMwManagerSessionDestroy(*device);
	request->result = result;
	request->outSize = 0;
	request->outPayload = NULL;
	reply = SwMwCommServerReplyError(channel, request);
	if (reply != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", reply, SWMW_MANAGER_SOURCE, 277);
	return 0;
}

/* 0x1101: connect the channel's device */
extern "C" int SwMwManagerDeviceConnect(struct SwMwCommChannel *channel,
				    struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *device;
	int result;

	SwMwManagerBuildContext(&context, channel);
	device = SwMwManagerContextValues(&context);
	if (request->value0 == 0) {
		SwMwCommServerReplyStatus(channel, request, 12004);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MANAGER_SOURCE, 321);
	}
	result = SwMwManagerConnectDevice(*device);
	if (result != 0)
		return swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_MANAGER_SOURCE, 326);
	return result;
}

/* the manager's command dispatcher (0x81005812) */
extern "C" int SwMwManagerCommandDispatch(struct SwMwCommChannel *channel,
					  struct SwMwIpcMessage *request, void *argument)
{
	if (channel == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MANAGER_SOURCE, 57);
	if (request == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MANAGER_SOURCE, 58);
	switch (request->command) {
	case 0x2101:
		return SwMwManagerConnect(channel, request, argument);
	case 0x2102:
		return SwMwManagerDisconnect(channel, request, argument);
	case 0x2113:
		return SwMwManagerGetSessionStatus(channel, request, argument);
	case 0x2114:
		return SwMwManagerGetSessionResult(channel, request, argument);
	case 0x2115:
		return SwMwManagerCloseSession(channel, request, argument);
	case 0x2116:
		return SwMwManagerOpenSession(channel, request, argument);
	case 0x2117:
		return SwMwManagerWaitReady(channel, request, argument);
	case 0x2118:
		return SwMwManagerDestroy(channel, request, argument);
	case 0x1101:
		return SwMwManagerDeviceConnect(channel, request, argument);
	default:
		return SwMwManagerKeystoreCommand(channel, request, argument, request->command);
	}
}

/*
 * PLACEHOLDER: cryptographic routine, deferred. The key manager's commands
 * (0x2103..0x2112 and 0x2119) drive the SWKeyManager object (libkm/libactmgr); that
 * object model and its key/certificate operations are the deferred DRM/authentication
 * batch, so the commands are answered with 12003 until it is written.
 */
extern "C" int SwMwManagerKeystoreCommand(struct SwMwCommChannel *channel,
					  struct SwMwIpcMessage *request, void *argument,
					  int command)
{
	(void)argument;
	(void)command;
	SwMwCommServerReplyStatus(channel, request, 12003);
	return 0;
}
