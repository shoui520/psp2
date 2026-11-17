/*
 * SceNpCommonPs4, NpAsm: the callbacks the system's own client is given, and the transport that keeps the title
 * tokens the ASM server issued (at most 20, the one used longest ago makes room for a new one).
 */

#include "np_asm.h"

extern "C" {

extern const int g_npAsmCallbackTableExtra;		/* 0x8102b168 */

extern const void *g_npAsmCallbackTableVtable[];
extern const void *g_npAsmTransportVtable[];

struct NpAsmCallbackTable g_npAsmCallbackTable;		/* 0x8102b990 */

/* the transport's destructor and its deleting variant carry the same body */
#define NP_ASM_TRANSPORT_RELEASE(transport) \
	do { \
		struct NpAsmClient *client_ = (transport)->client; \
		struct NpAsmTokenEntry *entry_; \
		struct NpAsmTokenEntry *next_; \
		(transport)->vtable = NP_COMMON_PS4_VTABLE(g_npAsmTransportVtable); \
		npAsmClientLock(client_); \
		entry_ = (transport)->head; \
		if (entry_ != NULL) { \
			do { \
				next_ = entry_->next; \
				SceNpCommonPs4_6B428829(npAsmClientGetAllocator(client_), entry_); \
				entry_ = next_; \
			} while (next_ != NULL); \
		} \
		(transport)->count = 0; \
		npAsmClientUnlock(client_); \
		(transport)->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable); \
	} while (0)

/*SORT-START*/
__attribute__((noinline)) struct NpAsmCallbackTable *npAsmCallbackTableInit(struct NpAsmCallbackTable *table)
{
	table->vtable = NP_COMMON_PS4_VTABLE(g_npAsmCallbackTableVtable);
	table->callbacks.update = NULL;
	table->callbacks.check = NULL;
	return table;
}

struct NpAsmCallbackTable *npAsmCallbackTableTerm(struct NpAsmCallbackTable *table)
{
	table->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return table;
}

int npAsmCallbackTableDelete(struct NpAsmCallbackTable *table)
{
	table->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(table);
}

void npAsmCallbackTableNullsub(void)
{
}

struct NpAsmCallbackTable *npAsmGetCallbackTable(void)
{
	return &g_npAsmCallbackTable;
}

int npAsmCallbackTableSet(struct NpAsmCallbackTable *table, const struct NpAsmCallbacks *pCallbacks)
{
	int (*check)(void *, void *, void *, void *, int);

	check = pCallbacks->check;
	table->callbacks.update = pCallbacks->update;
	table->callbacks.check = check;
	return 0;
}

int npAsmCallbackTableClear(struct NpAsmCallbackTable *table)
{
	table->callbacks.update = NULL;
	table->callbacks.check = NULL;
	return 0;
}

int npAsmCallbackTableCheck(struct NpAsmCallbackTable *table, void *pTitleId, void *serviceName, void *pValid,
	void *buffer, int size)
{
	int result;
	int (*check)(void *, void *, void *, void *, int);

	result = 0;
	if (npAsmIsSystemApp() != 0) {
		check = table->callbacks.check;
		if (check != NULL)
			return check(pTitleId, serviceName, pValid, buffer, size);
		*(SceUInt8 *)pValid = 0;
	} else {
		return npAsmIpcCheck((void *)SceNpCommonPs4_A0599F24(), pTitleId, serviceName, pValid,
			buffer, size);
	}
	return result;
}

int npAsmCallbackTableUpdate(struct NpAsmCallbackTable *table, void *pTitleId, void *serviceName, void *buffer)
{
	int result;
	int (*update)(void *, void *, void *);

	result = 0;
	if (npAsmIsSystemApp() != 0) {
		update = table->callbacks.update;
		if (update != NULL)
			return update(pTitleId, serviceName, buffer);
	} else {
		return npAsmIpcUpdate((void *)SceNpCommonPs4_A0599F24(), pTitleId, serviceName, buffer);
	}
	return result;
}

int npAsmCallbackTableRegister(void)
{
	npAsmCallbackTableInit(&g_npAsmCallbackTable);
	return npAsmRegistryAdd(&g_npAsmCallbackTable, (void *)npAsmCallbackTableTerm, &g_npAsmCallbackTableExtra);
}

int npAsmTransportAddToken(struct NpAsmTransport *transport, struct NpAsmTokenEntry *entry)
{
	struct NpAsmClient *client;
	struct NpAsmClient *lockClient;
	struct NpAsmTokenEntry *oldest;
	struct NpAsmTokenEntry *previous;
	struct NpAsmTokenEntry *scan;
	struct NpAsmTokenEntry *next;
	SceUInt32 count;

	client = transport->client;
	oldest = NULL;
	previous = NULL;
	npAsmClientLock(client);
	count = transport->count;
	npAsmClientUnlock(client);
	if (count >= 20) {
		npAsmClientLock(client);
		for (scan = transport->head; scan != NULL; scan = scan->next) {
			if (oldest == NULL || scan->lastUsed < oldest->lastUsed)
				oldest = scan;
		}
		npAsmClientUnlock(client);
		lockClient = transport->client;
		npAsmClientLock(lockClient);
		scan = transport->head;
		if (scan != NULL) {
			do {
				if (scan == oldest)
					break;
				previous = scan;
				scan = scan->next;
			} while (scan != NULL);
			if (scan != NULL) {
				next = scan->next;
				if (previous != NULL) {
					previous->next = next;
					count = transport->count;
				} else {
					count = transport->count;
					transport->head = next;
				}
				transport->count = count - 1;
				npAsmClientUnlock(lockClient);
			}
		}
		SceNpCommonPs4_6B428829(npAsmClientGetAllocator(client), oldest);
	}
	lockClient = transport->client;
	npAsmClientLock(lockClient);
	next = transport->head;
	count = transport->count;
	if (next != NULL)
		entry->next = next;
	transport->head = entry;
	transport->count = count + 1;
	return npAsmClientUnlock(lockClient);
}

int npAsmClientUpdateTokenLimitation(struct NpAsmTransport *transport, void *connection, const void *pTitleId)
{
	struct NpAsmClient *client;
	int result;
	char *header;
	unsigned int length;
	unsigned int skip;
	const char *scan;
	int comma;
	SceRtcTick expires;
	unsigned int size;

	client = npAsmClientGet();
	size = 0;
	if (pTitleId != NULL) {
		result = npAsmConnectionGetHeaderSize(connection, "X-SCE-Title-Access-Limitation-Expires", &size);
		if (result >= 0) {
			header = (char *)SceNpCommonPs4_AA980113(npAsmClientGetAllocator(client), size + 1);
			if (header != NULL) {
				sceClibMemset(header, 0, size + 1);
				result = npAsmConnectionGetHeader(connection, "X-SCE-Title-Access-Limitation-Expires", header, size);
				if (result >= 0) {
					skip = 0;
					if (size != 0) {
						skip = 0;
						scan = header;
						comma = 0;
						do {
							int c = *scan;

							if (comma) {
								if (c != 32)
									break;
							} else {
								comma = c == 44;
							}
							++skip;
							++scan;
						} while (skip < size);
					}
					sceClibMemset(&expires, 0, 8);
					result = sceRtcParseDateTime(&expires, &header[skip]);
					if (result >= 0 && npAsmClientIsSystemTitle(client) == 0)
						result = npAsmLimitationSet(pTitleId, &expires.tick);
				}
			} else {
				result = NP_ASM_ERROR_OUT_OF_MEMORY;
			}
			if (header != NULL)
				SceNpCommonPs4_6B428829(npAsmClientGetAllocator(client), header);
		}
	} else {
		result = NP_ASM_ERROR_INVALID_ARGUMENT;
	}
	return result;
}

struct NpAsmTransport *npAsmTransportInit(struct NpAsmTransport *transport, struct NpAsmClient *client)
{
	transport->client = client;
	transport->vtable = NP_COMMON_PS4_VTABLE(g_npAsmTransportVtable);
	transport->head = NULL;
	transport->count = 0;
	return transport;
}

struct NpAsmTransport *npAsmTransportTerm(struct NpAsmTransport *transport)
{
	NP_ASM_TRANSPORT_RELEASE(transport);
	return transport;
}

int npAsmTransportDelete(struct NpAsmTransport *transport)
{
	NP_ASM_TRANSPORT_RELEASE(transport);
	return SceNpCommonPs4_BB5282C8(transport);
}

int npAsmTransportGetTitleToken(struct NpAsmTransport *transport, struct NpAsmRequest *request, const void *pOnlineId,
	void *tokenOut, SceUInt8 force)
{
	struct NpAsmClient *client;
	struct NpAsmTokenEntry *entry;
	int result;
	int loginId;
	void *connection;
	char limited;
	SceRtcTick tick;
	SceRtcTick halfLife;
	SceUInt8 titleId[16];
	SceUInt8 titleSecret[128];
	SceUInt8 titleId2[16];
	SceUInt8 titleSecret2[128];
	SceUInt8 accessToken[272];
	SceUInt8 token[1040];
	int fresh;
	int ret;
	struct NpAsmTransport *self;

	loginId = 0;
	connection = NULL;
	limited = 0;
	client = transport->client;
	sceClibMemset(titleId, 0, 16);
	sceClibMemset(titleSecret, 0, 128);
	result = npAsmClientGetTitleId(client, titleId, titleSecret, 1);
	if (result >= 0 && (npAsmClientIsSystemTitle(client) != 0 || (result = npAsmLimitationCheck((const char *)titleId, 2, (SceUInt8 *)&limited, NULL)) >= 0)
		&& limited != 0)
		result = NP_ASM_ERROR_80552A09;
	ret = result;
	if (result >= 0) {
		ret = sceRtcGetCurrentRetainedNetworkTick(&tick);
		if (ret >= 0) {
			self = transport;
			npAsmClientLock(transport->client);
			for (entry = transport->head; entry != NULL; entry = entry->next) {
				if (SceNpCommon_8C21F489(entry, pOnlineId) == 0)
					break;
			}
			npAsmClientUnlock(transport->client);
			if (entry != NULL) {
				SceUInt64 life;

				life = entry->token.expires - entry->fetched;
				result = sceRtcTickAddTicks(&halfLife, (const SceRtcTick *)&entry->fetched, life >> 1);
				fresh = force;
				{
					int expired = 0;

					if (result >= 0)
						expired = tick.tick >= halfLife.tick;
					ret = result;
					if (result < 0)
						goto done;
					if (expired)
						fresh = 1;
				}
				if (fresh == 0) {
update:
					if (entry != NULL) {
						sceClibMemcpy_safe(&entry->lastUsed, &tick, 8);
						if (tokenOut != NULL) {
							npAsmClientLock(client);
							sceClibMemcpy_safe(tokenOut, &entry->token, 0x410);
							npAsmClientUnlock(client);
						}
					}
					goto done;
				}
			}
			loginId = 0;
			ret = SceNpManager_4110B787(&loginId);
			if (ret < 0)
				goto done;
			npAsmClientLock(client);
			npAsmRequestSetNpManagerId(request, loginId);
			if (npAsmRequestIsAborted(request) != 0) {
				ret = NP_ASM_ERROR_ABORTED;
				npAsmClientUnlock(client);
				goto done;
			}
			npAsmClientUnlock(client);
			ret = SceNpManager_FDE78C74(npAsmRequestGetNpManagerId(request), accessToken);
			if (ret >= 0) {
				npAsmClientLock(client);
				SceNpManager_3907F96A(npAsmRequestGetNpManagerId(request));
				npAsmRequestSetNpManagerId(request, 0);
				npAsmClientUnlock(client);
				ret = npAsmConnectionCreate(
					npAsmRequestGetParam(request)->httpContextId <= 0 ? npAsmClientGetHttpContextId(client)
						: npAsmRequestGetParam(request)->httpContextId,
					npAsmRequestGetParam(request)->allocator != NULL ? npAsmRequestGetParam(request)->allocator
						: npAsmClientGetRequestAllocator(client),
					&connection);
				if (ret >= 0) {
					sceClibMemset(titleId2, 0, 16);
					sceClibMemset(titleSecret2, 0, 128);
					ret = npAsmClientGetTitleId(self->client, titleId2, titleSecret2, 1);
					if (ret >= 0) {
						npAsmClientLock(client);
						npAsmRequestSetConnection(request, connection);
						if (npAsmRequestIsAborted(request) != 0) {
							ret = NP_ASM_ERROR_ABORTED;
							npAsmClientUnlock(client);
							goto done;
						}
						npAsmClientUnlock(client);
						ret = npAsmConnectionGetTitleToken(npAsmRequestGetConnection(request), titleId2, titleSecret2,
							accessToken, token);
						if (ret < 0) {
							if (ret == -2111831545) {
								if (npAsmIsSystemApp() == 0)
									npAsmIpcTerminate(SceNpCommonPs4_A0599F24());
								if (npAsmClientUpdateTokenLimitation(self, npAsmRequestGetConnection(request), titleId2) >= 0)
									ret = NP_ASM_ERROR_80552A09;
							}
							goto done;
						}
						npAsmClientLock(client);
						npAsmConnectionDestroy(npAsmRequestGetConnection(request));
						npAsmRequestSetConnection(request, NULL);
						{
							struct NpAsmClient *owner = self->client;

							npAsmClientLock(owner);
							for (entry = self->head; entry != NULL; entry = entry->next) {
								if (SceNpCommon_8C21F489(entry, pOnlineId) == 0)
									break;
							}
							npAsmClientUnlock(owner);
							if (entry != NULL) {
								npAsmClientLock(self->client);
								sceClibMemcpy_safe(&entry->token, token, 0x410);
								npAsmClientUnlock(self->client);
							} else {
								entry = (struct NpAsmTokenEntry *)SceNpCommonPs4_AA980113(
									npAsmClientGetAllocator(self->client), sizeof(struct NpAsmTokenEntry));
								ret = NP_ASM_ERROR_OUT_OF_MEMORY;
								if (entry != NULL) {
									sceClibMemset(entry, 0, sizeof(struct NpAsmTokenEntry));
									sceClibMemcpy_safe(entry, pOnlineId, 0x14);
									sceClibMemcpy_safe(&entry->token, token, 0x410);
									ret = 0;
								}
								if (ret < 0) {
									npAsmClientUnlock(client);
									goto done;
								}
								npAsmTransportAddToken(self, entry);
							}
							sceClibMemcpy_safe(&entry->fetched, &tick, 8);
							npAsmClientUnlock(client);
							goto update;
						}
					}
				}
			}
		}
	}
done:
	npAsmClientLock(client);
	if (npAsmRequestIsAborted(request) != 0)
		ret = NP_ASM_ERROR_ABORTED;
	if (npAsmRequestGetConnection(request) != NULL) {
		npAsmConnectionDestroy(npAsmRequestGetConnection(request));
		npAsmRequestSetConnection(request, NULL);
	}
	if (npAsmRequestGetNpManagerId(request) != 0) {
		SceNpManager_3907F96A(npAsmRequestGetNpManagerId(request));
		npAsmRequestSetNpManagerId(request, 0);
	}
	npAsmClientUnlock(client);
	return ret;
}

int npAsmTransportGetTokenLimitation(struct NpAsmTransport *transport, void *pExpired, void *pTick)
{
	struct NpAsmClient *client;
	int result;
	SceUInt8 titleId[16];
	SceUInt8 titleSecret[128];

	client = transport->client;
	sceClibMemset(titleId, 0, 16);
	sceClibMemset(titleSecret, 0, 128);
	result = npAsmClientGetTitleId(client, titleId, titleSecret, 1);
	if (result >= 0)
		result = npAsmLimitationCheck((const char *)titleId, 1, (SceUInt8 *)pExpired, (SceUInt64 *)pTick);
	return result;
}

int npAsmTransportIsTokenValid(struct NpAsmTransport *transport, const void *pOnlineId, SceUInt8 *pValid,
	SceUInt64 *pExpire)
{
	struct NpAsmClient *client;
	struct NpAsmClient *lockClient;
	struct NpAsmTokenEntry *entry;
	int result;
	int valid;
	SceRtcTick tick;
	SceRtcTick halfLife;
	SceUInt8 titleId[16];
	SceUInt8 titleSecret[128];
	SceUInt8 limited;
	int i;
	const SceRtcTick *src;
	SceUInt64 *dst;

	limited = 0;
	client = transport->client;
	sceClibMemset(titleId, 0, 16);
	sceClibMemset(titleSecret, 0, 128);
	result = npAsmClientGetTitleId(client, titleId, titleSecret, 1);
	if (result >= 0 && (npAsmClientIsSystemTitle(client) != 0 || (result = npAsmLimitationCheck((const char *)titleId, 2, &limited, NULL)) >= 0)
		&& limited != 0)
		result = NP_ASM_ERROR_80552A09;
	if (result >= 0) {
		result = sceRtcGetCurrentRetainedNetworkTick(&tick);
		if (result >= 0) {
			lockClient = transport->client;
			npAsmClientLock(lockClient);
			for (entry = transport->head; entry != NULL; entry = entry->next) {
				if (SceNpCommon_8C21F489(entry, pOnlineId) == 0)
					break;
			}
			npAsmClientUnlock(lockClient);
			if (entry != NULL) {
				valid = sceRtcTickAddTicks(&halfLife, (const SceRtcTick *)&entry->fetched,
					(entry->token.expires - entry->fetched) >> 1);
				i = 0;
				if (valid >= 0)
					i = tick.tick >= halfLife.tick;
				result = valid;
				if (valid >= 0) {
					if (i) {
						*pValid = 0;
					} else {
						dst = pExpire;
						src = (const SceRtcTick *)&entry->token;
						*pValid = 1;
						i = 65;
						do {
							SceUInt64 first = src[0].tick;
							SceUInt64 second = src[1].tick;

							--i;
							src += 2;
							dst[0] = first;
							dst[1] = second;
							dst += 2;
						} while (i != 0);
					}
				}
			} else {
				*pValid = 0;
			}
		}
	}
	return result;
}
/*SORT-END*/

const void *g_npAsmCallbackTableVtable[] = {	/* 0x8102ab78 */
	NULL,
	NULL,
	(const void *)npAsmCallbackTableTerm,
	(const void *)npAsmCallbackTableDelete,
};

const void *g_npAsmTransportVtable[] = {		/* 0x8102ab88 */
	NULL,
	NULL,
	(const void *)npAsmTransportTerm,
	(const void *)npAsmTransportDelete,
};

}
