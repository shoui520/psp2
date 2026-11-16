/*
 * SceNpBasic: the join game acknowledgment requests (Sony's npbasic/jga_response.cpp). A request
 * of another player arrives through the raw message queue; it is kept in one of 8 entries until
 * the title answers it (sceNpBasicJoinGameAckResponseSend), the player cancels it or it times out.
 */
#include "npbasic_internal.h"

#define NP_BASIC_JGA_SOURCE_FILE \
	"W:\\Build\\J01509658\\vsh\\app\\common\\src\\np\\npbasic\\jga_response.cpp"
#define NP_BASIC_JGA_ASSERT_FAILED(line, func)						\
	do {										\
		sceClibPrintf("## ASSERTION FAILURE in %s(%d): %s() \n",		\
			      NP_BASIC_JGA_SOURCE_FILE, line, func);			\
		*(volatile int *)0 = 0;							\
	} while (0)

extern "C" int sceNpBasicCheckCallback(void)
{
	if (g_state.initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	npImpsUtilityIpcTryGetMessages(g_state.client, &g_state.callbackTable, &g_state.commId);
	return npBasicJgaCheckTimeouts();
}

int npBasicJgaRequestReceived(int requestId, SceNpId npId, SceUInt64 deadline)
{
	SceNpId self;
	NpBasicJoinGameEntry *entry;
	SceNpId arg;
	int result;
	int i;
	int found;

	result = 0;
	if (g_state.joinGameAckMode == 4) {
		for (i = 0; i < NP_BASIC_JOIN_GAME_ENTRY_MAX; i++) {
			if (sceNpCmpNpId(&g_state.joinGameEntries[i].npId, &npId) == 0) {
				g_state.joinGameEntries[i].requestId = requestId;
				g_state.joinGameEntries[i].deadline = deadline;
				g_state.joinGameEntries[i].timedOut = 0;
				g_state.joinGameEntries[i].canceled = 0;
				g_state.joinGameEntries[i].used = 1;
				return result;
			}
		}
		for (i = 0; i < NP_BASIC_JOIN_GAME_ENTRY_MAX; i++) {
			entry = &g_state.joinGameEntries[i];
			if (entry->used == 0) {
				found = i;
				sceClibMemcpy_safe(&entry->npId, &npId, sizeof(SceNpId));
				entry->deadline = deadline;
				entry->requestId = requestId;
				entry->canceled = 0;
				entry->used = 1;
				if (g_state.joinGameAckHandler != NULL) {
					arg = g_state.joinGameEntries[found].npId;
					g_state.joinGameAckHandler(arg, g_state.joinGameAckArg);
				}
				return result;
			}
		}
		sceClibMemset(&self, 0, sizeof(SceNpId));
		result = sceNpManagerGetNpId(&self);
		if (result >= 0) {
			NpBasicJgaSendMessage dummy;

			sceClibMemset(&dummy, 0, 0x84);
			((int *)&dummy)[0] = 2;
			result = npBasicJgaSendMessage(requestId, &self, &npId, &dummy, 0x84);
		}
	} else {
		sceClibMemset(&self, 0, sizeof(SceNpId));
		result = sceNpManagerGetNpId(&self);
		if (result >= 0) {
			NpBasicJgaSendMessage dummy;

			sceClibMemset(&dummy, 0, 0x84);
			((int *)&dummy)[0] = 2;
			npBasicJgaSendMessage(requestId, &self, &npId, &dummy, 0x84);
			result = 0;
		}
	}
	return result;
}

int npBasicJgaCancelAll(void)
{
	int message[33];
	SceNpId self;
	int i;

	if (g_state.joinGameAckHandler != NULL) {
		sceClibMemset(&self, 0, sizeof(SceNpId));
		if (sceNpManagerGetNpId(&self) >= 0) {
			sceClibMemset(message, 0, 0x84);
			message[0] = 2;
			for (i = 0; i < NP_BASIC_JOIN_GAME_ENTRY_MAX; i++) {
				if (g_state.joinGameEntries[i].used != 0 && g_state.joinGameEntries[i].canceled == 0
				    && g_state.joinGameEntries[i].timedOut == 0)
					npBasicJgaSendMessage(g_state.joinGameEntries[i].requestId, &self,
							      &g_state.joinGameEntries[i].npId, message, 0x84);
			}
		}
		sceClibMemset(g_state.joinGameEntries, 0, sizeof(g_state.joinGameEntries));
	}
	return 0;
}

int npBasicJgaCheckTimeouts(void)
{
	SceRtcTick tick;
	int message[33];
	SceNpId self;
	int result;
	int i;

	if (g_state.joinGameAckHandler == NULL)
		return 0;
	result = sceRtcGetCurrentNetworkTick(&tick);
	if (result >= 0) {
		for (i = 0; i < NP_BASIC_JOIN_GAME_ENTRY_MAX; i++) {
			if (g_state.joinGameEntries[i].used == 1 && g_state.joinGameEntries[i].timedOut == 0
			    && g_state.joinGameEntries[i].canceled == 0
			    && (SceInt64)(g_state.joinGameEntries[i].deadline - tick.tick) < 0) {
				g_state.joinGameEntries[i].timedOut = 1;
				sceClibMemset(&self, 0, sizeof(SceNpId));
				result = sceNpManagerGetNpId(&self);
				if (result < 0)
					break;
				sceClibMemset(message, 0, 0x84);
				message[0] = 5;
				result = npBasicJgaSendMessage(g_state.joinGameEntries[i].requestId, &self,
							       &g_state.joinGameEntries[i].npId, message, 0x84);
				if (result < 0)
					break;
			}
		}
		if (i >= NP_BASIC_JOIN_GAME_ENTRY_MAX)
			result = npBasicJgaPoll();
	}
	return result;
}

extern "C" {

int sceNpBasicRegisterJoinGameAckHandler(SceNpBasicJoinGameAckRequestEventHandler handler, void *userdata)
{
	int result;

	if (g_state.initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	if (g_state.joinGameAckHandler != NULL)
		return NP_BASIC_ERROR_ALREADY_INITIALIZED;
	if (handler == NULL)
		return NP_BASIC_JGA_ERROR_INVALID_HANDLER;
	result = npBasicJgaStart(npBasicJgaEventCallback);
	if (result >= 0) {
		g_state.joinGameAckHandler = handler;
		g_state.joinGameAckArg = userdata;
		return 0;
	}
	return result;
}

int sceNpBasicUnregisterJoinGameAckHandler(void)
{
	if (g_state.initialized == 0 || g_state.joinGameAckHandler == NULL)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	npBasicJgaCancelAll();
	npBasicJgaStop();
	g_state.joinGameAckHandler = NULL;
	g_state.joinGameAckArg = NULL;
	return 0;
}


static void npBasicJgaRemoveEntry(int index)
{
	if (index == NP_BASIC_JOIN_GAME_ENTRY_MAX - 1) {
		sceClibMemset(&g_state.joinGameEntries[index], 0, sizeof(NpBasicJoinGameEntry));
	} else {
		sceClibMemmove(&g_state.joinGameEntries[index], &g_state.joinGameEntries[index + 1],
			       sizeof(NpBasicJoinGameEntry) * (unsigned char)(NP_BASIC_JOIN_GAME_ENTRY_MAX - 1 - index));
		sceClibMemset(&g_state.joinGameEntries[NP_BASIC_JOIN_GAME_ENTRY_MAX - 1], 0, sizeof(NpBasicJoinGameEntry));
	}
}

int sceNpBasicJoinGameAckResponseSend(const SceNpBasicJoinGameAckResponse *response)
{
	int message[33];
	SceNpId self;
	int result;
	int i;

	result = 0;
	if (g_state.initialized == 0 || g_state.joinGameAckHandler == NULL)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	if (response == NULL || (int)response->type >= 5 || (int)response->type == 0)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	for (i = 0; i < NP_BASIC_JOIN_GAME_ENTRY_MAX; i++) {
		if (sceNpCmpNpId(&g_state.joinGameEntries[i].npId, &response->npId) == 0)
			break;
	}
	if (i < NP_BASIC_JOIN_GAME_ENTRY_MAX) {
		if (g_state.joinGameEntries[i].used != 1 || g_state.joinGameEntries[i].timedOut != 0
		    || g_state.joinGameEntries[i].canceled != 0) {
			if (g_state.joinGameEntries[i].timedOut == 1)
				result = NP_BASIC_ERROR_JOIN_GAME_ACK_REQUEST_TIMEDOUT;
			if (g_state.joinGameEntries[i].canceled == 1)
				result = NP_BASIC_ERROR_JOIN_GAME_ACK_REQUEST_CANCELED;
			npBasicJgaRemoveEntry(i);
		} else {
			sceClibMemset(message, 0, 0x84);
			message[0] = response->type;
			if (message[0] == 4)
				sceClibStrncpy((char *)&message[1], response->denyReason, 0x7f);
			sceClibMemset(&self, 0, sizeof(SceNpId));
			result = sceNpManagerGetNpId(&self);
			if (result < 0)
				return result;
			result = npBasicJgaSendMessage(g_state.joinGameEntries[i].requestId, &self,
						       &g_state.joinGameEntries[i].npId, message, 0x84);
			if (result < 0)
				return result;
			npBasicJgaRemoveEntry(i);
		}
	}
	if (i == NP_BASIC_JOIN_GAME_ENTRY_MAX)
		return NP_BASIC_ERROR_JOIN_GAME_ACK_REQUEST_INVALID;
	return result;
}

} /* extern "C" */

void npBasicJgaEventCallback(const NpBasicJgaEvent *event)
{
	NpBasicJgaRequestHeader header;
	SceNpId npId;
	SceNpId self;
	int requestId;
	int i;

	if (event == NULL || event->type != 1)
		return;
	if (event->dataSize != sizeof(NpBasicJgaRequestHeader))
		NP_BASIC_JGA_ASSERT_FAILED(35, "SceNpJgaIpcCallbackFn");
	sceClibMemcpy_safe(&header, event->data, sizeof(NpBasicJgaRequestHeader));
	if (header.kind > 0) {
		if (header.kind <= 1) {
			npId = *(const SceNpId *)event->peer;
			requestId = event->requestId;
			for (i = 0; i < NP_BASIC_JOIN_GAME_ENTRY_MAX; i++) {
				if (sceNpCmpNpId(&g_state.joinGameEntries[i].npId, &npId) == 0) {
					g_state.joinGameEntries[i].canceled = 1;
					break;
				}
			}
			sceClibMemset(&self, 0, sizeof(SceNpId));
			if (sceNpManagerGetNpId(&self) >= 0)
				npBasicJgaSendMessage(requestId, &self, &npId, NULL, 0);
			return;
		}
	} else if (header.kind >= 0) {
		npBasicJgaRequestReceived(event->requestId, *(const SceNpId *)event->peer, header.deadline);
		return;
	}
	NP_BASIC_JGA_ASSERT_FAILED(58, "SceNpJgaIpcCallbackFn");
}
