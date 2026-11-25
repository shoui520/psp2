/*
 * swagner's DTCP-IP middleware main (DTCPIP-MW/Src/Mw/SwMwMain.cpp, from the error
 * strings' path and lines): the activation's and the download's locks, the communication
 * server, its dispatch loop (the manager's dispatcher), then the shutdown in reverse.
 * Each step remembers its own failure and prints its error with its source line; a failed
 * step skips the rest, and the program returns 1 (0 when the loop ran to its end).
 */

#include <stddef.h>
#include <scetypes.h>
#include "../Comm/vita/SwMwCommServer.h"

extern "C" {
/* the middleware's trace (SwMwTrace.c's [SWMW] print) */
int swMwTrace(const char *format, ...);

/* the activation's lock (DTCPIP-MW/Src/Activate/dtcpip_activate.c) */
int _sceDtcpIpActivateInit(void);
int _sceDtcpIpActivateTerm(void);

/* the download's lock (DTCPIP-MW/Src/Mw/dtcpip_download.c) */
int _sceDtcpIpDownloadInit(void);
int _sceDtcpIpDownloadTerm(void);

/* the communication server (DTCPIP-MW/Src/Comm/vita/SwMwCommServer.cpp) */
struct SwMwCommServer;
int SwMwCommServerCreate(struct SwMwCommServer **server);
int SwMwCommServerRun(struct SwMwCommServer *server, void (*dispatch)(void *, void *, void *),
		      void *argument, int flags);
int SwMwCommServerTerm(struct SwMwCommServer *server);

/* the server's side of the SW core (DTCPIP-MW/Src/Comm/vita/SwMwCommServer.cpp) */
struct SwMwCommChannel;
int SwMwCommServerGetRequest(const struct SwMwCommRequest *request, int *out);
int SwMwCommServerReplyError(struct SwMwCommChannel *channel,
			     struct SwMwIpcMessage *request);

/* the manager's dispatcher (DTCPIP-MW/Src/Mw/SwMwManagerDispatch.cpp) */
void SwMwManagerDispatch(void *, void *, void *);
int SwMwManagerCommandDispatch(struct SwMwCommChannel *, struct SwMwIpcMessage *, void *);
int SwMwSinkDispatch(struct SwMwCommChannel *, struct SwMwIpcMessage *, void *);
int SwMwMoveSinkDispatch(struct SwMwCommChannel *, struct SwMwIpcMessage *, void *);
void SwMwDeviceKeepalive(void *device);
}

#define SWMW_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-MW\\Src\\Mw\\SwMwMain.cpp"

/* one step's failure: its error printed with its source line */
#define SWMW_TRACE_ERROR(result, line)						\
	swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_SOURCE, line)

/* fill the dispatch context from the channel's request view (0x8107b0d8) */
extern "C" __attribute__((noinline)) void SwMwManagerBuildContext(void *context, void *channelArgument)
{
	struct SwMwManagerContextBlock *block = (struct SwMwManagerContextBlock *)context;

	block->channel = (struct SwMwCommChannel *)channelArgument;
	SwMwCommServerGetRequest((const struct SwMwCommRequest *)block->channel, block->head);
}

/* the context's device words, past its channel word (0x8107b0fc) */
extern "C" __attribute__((noinline)) int *SwMwManagerContextValues(void *context)
{
	struct SwMwManagerContextBlock *block = (struct SwMwManagerContextBlock *)context;

	return (int *)block->body;
}

/*
 * The request dispatcher (0x81002b80): the command's subsystem bits pick the manager's
 * own dispatch (0x100), the sink's (0x200, SwMwSinkDispatch.cpp), the move sink's (0x800,
 * SwMwMoveSinkDispatch.cpp) or the 0x400 error reply; a connect (command 1) also
 * re-dispatches the three device words the channel's context holds, and a live sink
 * device gets its keepalive.
 */
extern "C" void SwMwManagerDispatch(void *channelArgument, void *requestArgument, void *argument)
{
	struct SwMwCommChannel *channel = (struct SwMwCommChannel *)channelArgument;
	struct SwMwIpcMessage *request = (struct SwMwIpcMessage *)requestArgument;
	struct SwMwManagerContextBlock context;
	int command = request->command;
	int *values;

	if ((unsigned int)command > 0x1000) {
		if ((command & 0x400) != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_SOURCE, 65);
			request->result = 12003;
			request->outSize = 0;
			request->outPayload = NULL;
			SwMwCommServerReplyError(channel, request);
		} else if ((command & 0x200) != 0) {
			SwMwSinkDispatch(channel, request, argument);
		} else if ((command & 0x100) != 0) {
			SwMwManagerCommandDispatch(channel, request, argument);
		} else if ((command & 0x800) != 0) {
			SwMwMoveSinkDispatch(channel, request, argument);
		}
	}
	SwMwManagerBuildContext(&context, channel);
	values = SwMwManagerContextValues(&context);
	if (command == 1 && values != NULL) {
		if (values[0] != 0) {
			request->command = 0x1101;
			request->value0 = values[0];
			SwMwManagerCommandDispatch(channel, request, argument);
		}
		if (values[2] != 0) {
			request->command = 0x1201;
			request->value0 = values[2];
			SwMwSinkDispatch(channel, request, argument);
		}
		if (values[4] != 0) {
			request->command = 0x1801;
			request->value0 = values[4];
			SwMwMoveSinkDispatch(channel, request, argument);
		}
	}
	if (values != NULL && values[0] != 0)
		SwMwDeviceKeepalive((void *)values[0]);
}

extern "C" int swagnerMain(void)
{
	int result;
	struct SwMwCommServer *server;

	int activationFailed = 0;
	result = _sceDtcpIpActivateInit();
	if (result != 0) {
		SWMW_TRACE_ERROR(result, 26);
		activationFailed = 1;
	}
	if (activationFailed == 0) {
		int downloadFailed = 0;

		result = _sceDtcpIpDownloadInit();
		if (result != 0) {
			SWMW_TRACE_ERROR(result, 29);
			downloadFailed = 1;
		}
		if (downloadFailed == 0) {
			int createFailed = 0;

			result = SwMwCommServerCreate(&server);
			if (result != 0) {
				SWMW_TRACE_ERROR(result, 32);
				createFailed = 1;
			}
			if (createFailed == 0) {
				int runFailed = 0;

				result = SwMwCommServerRun(server, SwMwManagerDispatch,
							   NULL, 0);
				if (result != 0) {
					SWMW_TRACE_ERROR(result, 35);
					runFailed = 1;
				}
				if (runFailed == 0) {
					int termFailed = 0;

					result = SwMwCommServerTerm(server);
					if (result != 0) {
						SWMW_TRACE_ERROR(result, 38);
						termFailed = 1;
					}
					if (termFailed == 0) {
						int downloadTermFailed = 0;

						result = _sceDtcpIpDownloadTerm();
						if (result != 0) {
							SWMW_TRACE_ERROR(result, 41);
							downloadTermFailed = 1;
						}
						if (downloadTermFailed == 0) {
							int activationTermFailed = 0;

							result = _sceDtcpIpActivateTerm();
							if (result != 0) {
								SWMW_TRACE_ERROR(result, 44);
								activationTermFailed = 1;
							}
							if (activationTermFailed == 0)
								return 0;
						}
					}
				}
			}
		}
	}
	return 1;
}
