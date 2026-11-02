/*
 * resolver.c: the DNS stub resolver and its cache (sceNetResolver*).
 *
 * A resolver is a libnet descriptor (sceNetDescriptorCreate) with a 1328-byte object of its
 * own; the application can supply the object's allocator and free function. An operation builds
 * a DNS query in the object's 768-byte buffer, sends it to each of the configured servers with
 * the timeout doubling per retry, and parses the answer (A for a name, PTR for an address).
 * The response is kept in a cache keyed by the server and the query, its TTL deciding when it
 * expires. In asynchronous mode the operation registers its socket with the descriptor, and the
 * application's epoll wait calls the handler (libnet_resolver_event) to continue it.
 *
 * Nothing names this file's functions; the names follow libnet_resolver_* and the messages
 * agent namecheck lists as reviewed.
 */
#include <libkernel/libkernel.h>
#include <libsha1.h>
#include "libnet_internal.h"

/* the stack's epoll notice: an automatic connection ended or a timeout expired */
#define LIBNET_EPOLL_NOTICE		0x20000

/* every helper below is a real call in Sony's code: never inline or clone one */
#define LIBNET_NOINLINE			__attribute__((noinline, noclone))

/* what a resolver is doing */
#define LIBNET_RESOLVER_NTOA		0
#define LIBNET_RESOLVER_ATON		1

/* the resolver's flags */
#define LIBNET_RESOLVER_ACTIVE		0x00000001	/* an operation runs */
#define LIBNET_RESOLVER_ASYNC		0x00000002	/* waits for epoll events */
#define LIBNET_RESOLVER_REGISTERED	0x00000004	/* its socket is in the descriptor */
#define LIBNET_RESOLVER_SENT		0x00000008	/* the query is sent, a reply is due */
#define LIBNET_RESOLVER_DONE		0x00000010	/* the last operation ended */
#define LIBNET_RESOLVER_CALLBACK		0x00000020	/* runs from the epoll handler */
#define LIBNET_RESOLVER_BUSY		0x00000040	/* an operation is in progress */
#define LIBNET_RESOLVER_ABORTED		0x00000080	/* aborted: EINTR */
#define LIBNET_RESOLVER_KEEP_NTOA	0x00000100	/* the last NTOA result is kept */
#define LIBNET_RESOLVER_KEEP_ATON	0x00000200	/* the last ATON result is kept */
#define LIBNET_RESOLVER_NO_IPADDRESS	0x00000800	/* NTOA of an address is an error */
#define LIBNET_RESOLVER_EDNS		0x00001000	/* the query carries an OPT record */
#define LIBNET_RESOLVER_STREAM		0x00002000	/* TCP: the reply has a length prefix */
#define LIBNET_RESOLVER_CONNECTED	0x00004000	/* the socket is connected */
#define LIBNET_RESOLVER_TCP_RETRY	0x00008000	/* the query was retried over TCP */
#define LIBNET_RESOLVER_CONNECTING	0x00010000	/* connect() is in progress */
#define LIBNET_RESOLVER_DESTROYED	0x00080000	/* the object is being destroyed */

/* what a resolver keeps while it is idle (everything the operation clears and refills) */
typedef struct LibnetResolver {
	struct LibnetResolver	*next;
	struct LibnetResolver	*prev;
	LibnetSync		sync;
	char			name[32];
	int			operation;
	int			serverCount;
	unsigned short		id;
	unsigned short		padding;
	int			timeout;
	int			retry;
	int			socket;
	void			*eventHandler;
	void			*eventArgument;
	int			eventSocket;
	int			waitTimeout;
	int			result;
	int			attempt;
	int			serverIndex;
	union {
		SceNetInAddr	address;	/* the address ATON asks about */
		SceNetInAddr	*addressPointer;/* where NTOA writes the answer */
	} target;
	union {
		char		hostname[256];	/* NTOA's name */
		struct {
			char	*name;		/* ATON's answer area */
			int	length;
		} reverse;
	} names;
	int			descriptor;
	unsigned char		*buffer;
	int			bufferSize;
	int			received;
	int			flags;
	SceNetResolverParam	param;
	SceSha1Context		sha1;
	unsigned char		digest[SCE_SHA1_DIGEST_SIZE];
	unsigned short		nameLength;
	unsigned char		padding2[2];
	unsigned char		packet[768];
} LibnetResolver;

/* a cached DNS response: the server and query it answers, and the response itself */
typedef struct LibnetDnsCacheEntry {
	struct LibnetDnsCacheEntry	*next;		/* the cache's list, most recent first */
	struct LibnetDnsCacheEntry	*prev;
	struct LibnetDnsCacheEntry	*bucketNext;	/* the hash bucket's list */
	struct LibnetDnsCacheEntry	*bucketPrev;
	unsigned char			key[16];	/* the server's sockaddr_in */
	unsigned char			*response;	/* it follows the entry */
	int				responseLength;
	unsigned char			digest[SCE_SHA1_DIGEST_SIZE];
	int				padding;
	SceUInt64			time;		/* when the response arrived */
	struct LibnetDnsCacheEntry	**bucket;	/* the bucket's head slot */
} LibnetDnsCacheEntry;

/* the cache: entry 0 is the list's head and 1 its tail, 2 * n and 2 * n + 1 a bucket's */
static LibnetDnsCacheEntry *libnet_dns_cache[2 * 65];
static int libnet_dns_cache_memory;

/* the resolvers, and the last transaction id handed out */
static LibnetResolver *libnet_resolver_head;
static LibnetResolver *libnet_resolver_tail;
static unsigned short libnet_dns_transaction_id;

static LIBNET_NOINLINE int libnet_resolver_query(LibnetResolver *resolver, unsigned char *packet, int packetSize,
				 const char *hostname, const char *hostnameEnd, void *address,
				 char *name, int nameSize);
int libnet_resolver_event(SceNetEpollEvent *event);
int libnet_resolver_start(LibnetResolver *resolver);

/* the label that ends a name, and the compression pointer that continues it */
#define LIBNET_DNS_LABEL_MASK	0xC0
#define LIBNET_DNS_LABEL_POINTER	0xC0

/* a name's compression pointer: 14 bits of offset from the packet start */
#define LIBNET_DNS_POINTER(p)	(((*p & 0x3F) << 8) | (p)[1])

/* skip the name at *POINTER, which ends before END; NULL when it runs past */
static LIBNET_NOINLINE unsigned char *libnet_dns_name_skip(unsigned char *pointer, unsigned int end)
{
	unsigned char *p;
	unsigned int length;

	if (pointer == NULL)
		return NULL;
	if ((unsigned int)pointer >= end)
		return NULL;
	length = *pointer;
	p = pointer + 1;
	if (length == 0)
		return p;
	while ((length & LIBNET_DNS_LABEL_MASK) != LIBNET_DNS_LABEL_POINTER) {
		pointer = &p[length];
		if (end <= (unsigned int)pointer)
			return NULL;
		length = *pointer;
		p = pointer + 1;
		if (length == 0)
			return p;
	}
	if (end <= (unsigned int)p)
		return NULL;
	return pointer + 2;
}

/* unlink ENTRY from the cache and free it */
static LIBNET_NOINLINE int libnet_dns_cache_remove(LibnetDnsCacheEntry *entry)
{
	LibnetDnsCacheEntry *next, *prev, *bucketNext, *bucketPrev;
	LibnetDnsCacheEntry **bucket;

	if (entry == NULL)
		return 0;
	next = entry->next;
	prev = entry->prev;
	if (next != NULL) {
		next->prev = prev;
		prev = entry->prev;
		if (prev != NULL)
			prev->next = next;
		else
			libnet_dns_cache[0] = next;
	} else {
		libnet_dns_cache[1] = prev;
		prev = entry->prev;
		if (prev != NULL)
			prev->next = next;
		else
			libnet_dns_cache[0] = next;
	}
	bucketNext = entry->bucketNext;
	bucket = entry->bucket;
	bucketPrev = entry->bucketPrev;
	if (bucketNext != NULL) {
		bucketNext->bucketPrev = bucketPrev;
		bucketPrev = entry->bucketPrev;
		if (bucketPrev != NULL)
			bucketPrev->bucketNext = bucketNext;
		else
			bucket[0] = bucketNext;
	} else {
		bucket[1] = bucketPrev;
		bucketPrev = entry->bucketPrev;
		if (bucketPrev != NULL)
			bucketPrev->bucketNext = bucketNext;
		else
			bucket[0] = bucketNext;
	}
	libnet_dns_cache_memory -= entry->responseLength;
	libnet_dns_cache_memory -= 80;
	return libnet_free(entry);
}

/*
 * Find the entry KEY and RESOLVER's digest name, moving it to the front of the cache and of its
 * bucket; *BUCKET gets the bucket's head slot. A NAME (a query) makes the digest first.
 */
static LIBNET_NOINLINE LibnetDnsCacheEntry *libnet_dns_cache_find(LibnetResolver *resolver,
						  LibnetDnsCacheEntry ***bucket,
						  const void *key, void *name, int nameLength)
{
	LibnetDnsCacheEntry *entry, *next, *prev, *bucketNext, *bucketPrev;
	LibnetDnsCacheEntry **slot;
	unsigned int index;

	if (name != NULL) {
		resolver->nameLength = *(unsigned short *)name;
		sceSha1BlockInit(&resolver->sha1);
		sceSha1BlockUpdate(&resolver->sha1, key, 4);
		sceSha1BlockUpdate(&resolver->sha1, (unsigned char *)name + 2, nameLength - 2);
		sceSha1BlockResult(&resolver->sha1, resolver->digest);
	}
	index = (resolver->digest[0] & 0x3F) + 1;
	slot = &libnet_dns_cache[2 * index];
	entry = *slot;
	if (entry != NULL) {
		while (sceClibMemcmp(entry->key, key, 0x10) != 0 ||
		       sceClibMemcmp(resolver->digest, entry->digest, 0x14) != 0) {
			entry = entry->bucketNext;
			if (entry == NULL)
				break;
		}
	}
	if (entry == NULL) {
		*bucket = slot;
		return NULL;
	}
	*bucket = slot;
	if (libnet_dns_cache[0] != entry) {
		next = entry->next;
		if (next != NULL) {
			prev = entry->prev;
			next->prev = prev;
		} else {
			prev = entry->prev;
			libnet_dns_cache[1] = prev;
		}
		prev = entry->prev;
		if (prev != NULL) {
			prev->next = next;
			next = libnet_dns_cache[0];
		} else {
			libnet_dns_cache[0] = next;
		}
		entry->next = next;
		if (next != NULL)
			next->prev = entry;
		else
			libnet_dns_cache[1] = entry;
		libnet_dns_cache[0] = entry;
		entry->prev = NULL;
	}
	if (*slot != entry) {
		bucketNext = entry->bucketNext;
		if (bucketNext != NULL) {
			bucketPrev = entry->bucketPrev;
			bucketNext->bucketPrev = bucketPrev;
		} else {
			bucketPrev = entry->bucketPrev;
			slot[1] = bucketPrev;
		}
		bucketPrev = entry->bucketPrev;
		if (bucketPrev != NULL) {
			bucketPrev->bucketNext = bucketNext;
			bucketNext = *slot;
		} else {
			slot[0] = bucketNext;
		}
		entry->bucketNext = bucketNext;
		if (bucketNext != NULL)
			bucketNext->bucketPrev = entry;
		else
			slot[1] = entry;
		entry->bucketPrev = NULL;
		*slot = entry;
	}
	return entry;
}

/*
 * The cached response to RESOLVER's query (built from SERVER's address, NAME and NAME_LENGTH)
 * if it is still valid: copy it over RESPONSE, whose capacity is *RESPONSE_LENGTH and which
 * gets the response's length. 1 when it was found and every answer still lives, else 0.
 */
static LIBNET_NOINLINE int libnet_dns_cache_lookup(LibnetResolver *resolver, int *server, unsigned short *name,
				   int nameLength, unsigned short *response, int *responseLength)
{
	LibnetDnsCacheEntry **slot;
	LibnetDnsCacheEntry *entry;
	unsigned char key[16];
	unsigned char *data, *end, *p, *record;
	SceUInt64 now;
	unsigned int elapsed;
	int count, i;

	sceClibMemset(key, 0, 0x10);
	key[0] = 16;
	key[1] = 2;
	*(int *)(key + 4) = *server;
	entry = libnet_dns_cache_find(resolver, &slot, key, name, nameLength);
	if (entry == NULL || *responseLength < (unsigned int)entry->responseLength)
		return 0;
	libnet_get_time(&now);
	if (entry->time >= now) {
		now = 0;
		elapsed = 0;
	} else {
		SceUInt64 age = now - entry->time;

		now -= entry->time;
		elapsed = (age + 999999) / 1000000;
	}
	data = entry->response;
	end = data + entry->responseLength + 1;
	p = data + 12;
	for (i = __builtin_bswap16(*((SceUShort16 *)data + 2)); i != 0; i--) {
		p = libnet_dns_name_skip(p, (unsigned int)end);
		if (p != NULL) {
			p += 4;
			if ((unsigned int)end <= (unsigned int)p)
				p = NULL;
		}
	}
	for (count = __builtin_bswap16(*((SceUShort16 *)data + 3)); count != 0; count--) {
		record = libnet_dns_name_skip(p, (unsigned int)end);
		p = record;
		if (record == NULL)
			continue;
		if ((unsigned int)end > (unsigned int)(record + 8) &&
		    __builtin_bswap32(((SceUInt32 *)(record + 4))[0]) > elapsed) {
			if ((unsigned int)end > (unsigned int)(record + 10)) {
				p = record + (((record[8] << 8) | record[9]) + 10);
				if ((unsigned int)end <= (unsigned int)p)
					p = NULL;
				continue;
			}
			p = NULL;
			continue;
		}
		p = NULL;
	}
	for (count = __builtin_bswap16(*((SceUShort16 *)data + 4)); count != 0; count--) {
		record = libnet_dns_name_skip(p, (unsigned int)end);
		p = record;
		if (record == NULL)
			continue;
		if ((unsigned int)end > (unsigned int)(record + 8) &&
		    __builtin_bswap32(((SceUInt32 *)(record + 4))[0]) > elapsed) {
			if ((unsigned int)end > (unsigned int)(record + 10)) {
				p = record + (((record[8] << 8) | record[9]) + 10);
				if ((unsigned int)end <= (unsigned int)p)
					p = NULL;
				continue;
			}
			p = NULL;
			continue;
		}
		p = NULL;
	}
	for (count = __builtin_bswap16(*((SceUShort16 *)data + 5)); count != 0; count--) {
		record = libnet_dns_name_skip(p, (unsigned int)end);
		p = record;
		if (record == NULL)
			continue;
		if ((unsigned int)end > (unsigned int)(record + 8) &&
		    elapsed < __builtin_bswap32(((SceUInt32 *)(record + 4))[0])) {
			if ((unsigned int)end > (unsigned int)(record + 10)) {
				p = record + (((record[8] << 8) | record[9]) + 10);
				if ((unsigned int)end <= (unsigned int)p)
					p = NULL;
				continue;
			}
			p = NULL;
			continue;
		}
		p = NULL;
	}
	if (p == NULL) {
		libnet_dns_cache_remove(entry);
		return 0;
	}
	sceClibMemcpy_safe(response, entry->response, entry->responseLength);
	*response = resolver->nameLength;
	if (entry->responseLength == 0)
		return 0;
	*responseLength = entry->responseLength;
	return 1;
}

/* keep RESPONSE (LENGTH bytes) as the answer to RESOLVER's query to SERVER */
static LIBNET_NOINLINE int libnet_dns_cache_store(LibnetResolver *resolver, int *server, void *response, int length)
{
	LibnetDnsCacheEntry **slot;
	LibnetDnsCacheEntry *entry;
	unsigned char key[16];
	int size;

	sceClibMemset(key, 0, 0x10);
	key[0] = 16;
	key[1] = 2;
	*(int *)(key + 4) = *server;
	if (libnet_dns_cache_find(resolver, &slot, key, NULL, 0) != NULL)
		return 0;
	((unsigned char *)response)[2] &= ~2;
	size = length + 80;
	while (libnet_dns_cache_memory + size >= 4096) {
		if (libnet_dns_cache[1] == NULL)
			return 0;
		libnet_dns_cache_remove(libnet_dns_cache[1]);
	}
	entry = libnet_malloc(size, 2);
	if (entry == NULL)
		return 0;
	sceClibMemcpy_safe(entry->key, key, 0x10);
	entry->response = (unsigned char *)entry + 80;
	entry->responseLength = length;
	libnet_get_time(&entry->time);
	entry->bucket = slot;
	sceClibMemcpy_safe(entry->digest, resolver->digest, 0x14);
	sceClibMemcpy_safe(entry->response, response, length);
	entry->next = libnet_dns_cache[0];
	if (entry->next != NULL)
		entry->next->prev = entry;
	else
		libnet_dns_cache[1] = entry;
	libnet_dns_cache[0] = entry;
	entry->prev = NULL;
	entry->bucketNext = *slot;
	if (entry->bucketNext != NULL)
		entry->bucketNext->bucketPrev = entry;
	else
		slot[1] = entry;
	entry->bucketPrev = NULL;
	*slot = entry;
	libnet_dns_cache_memory += size;
	return 0;
}

/* drop every cached response */
static LIBNET_NOINLINE int libnet_dns_cache_clear(void)
{
	int i;

	for (i = 0; i != 128; i += 2) {
		while (libnet_dns_cache[i + 2] != NULL)
			libnet_dns_cache_remove(libnet_dns_cache[i + 2]);
	}
	sceClibMemset(&libnet_dns_cache[2], 0, 0x200);
	return 0;
}

/* the cache needs nothing of its own */
static LIBNET_NOINLINE int libnet_dns_cache_init(void)
{
	__asm__ volatile ("" ::: "memory");
	return 0;
}

/* the cache goes away with the library */
static LIBNET_NOINLINE __attribute__((externally_visible)) int libnet_dns_cache_term(void)
{
	libnet_dns_cache_clear();
	return 0;
}

/* the resolvers' list is empty */
static LIBNET_NOINLINE __attribute__((externally_visible)) int libnet_resolver_list_init(void)
{
	libnet_resolver_head = NULL;
	libnet_resolver_tail = NULL;
	return 0;
}

/* and stays empty */
static LIBNET_NOINLINE int libnet_resolver_list_term(void)
{
	__asm__ volatile ("" ::: "memory");
	return 0;
}

/* the first transaction id is random */
static LIBNET_NOINLINE __attribute__((externally_visible)) int libnet_resolver_id_init(void)
{
	libnet_dns_transaction_id = libnet_random();
	return 0;
}

static LIBNET_NOINLINE int libnet_resolver_id_term(void)
{
	__asm__ volatile ("" ::: "memory");
	return 0;
}

int libnet_resolver_init(void)
{
	int ret, result;

	ret = libnet_resolver_id_init();
	if (ret < 0)
		return ret;
	result = libnet_resolver_list_init();
	if (result >= 0) {
		ret = libnet_dns_cache_init();
		if (ret >= 0)
			return 0;
		result = libnet_resolver_list_term();
	}
	libnet_resolver_id_term();
	return ret;
}

int libnet_resolver_term(void)
{
	libnet_dns_cache_term();
	libnet_resolver_list_term();
	libnet_resolver_id_term();
	return 0;
}

/* the resolver whose id is RID, or NULL */
static LIBNET_NOINLINE LibnetResolver *libnet_resolver_find(int rid)
{
	LibnetResolver *resolver;

	resolver = libnet_resolver_head;
	while (resolver != NULL && resolver->descriptor != rid)
		resolver = resolver->next;
	return resolver;
}

/* whether RESOLVER is still in the list */
static LIBNET_NOINLINE int libnet_resolver_exists(LibnetResolver *resolver)
{
	LibnetResolver *entry;

	entry = libnet_resolver_head;
	if (libnet_resolver_head == NULL)
		return (int)entry;
	if (libnet_resolver_head != resolver) {
		for (;;) {
			entry = entry->next;
			if (entry == NULL)
				break;
			if (resolver == entry)
				return 1;
		}
		return (int)entry;
	}
	return 1;
}

/* create the object of the resolver NAME from PARAM, and return its descriptor id */
static LIBNET_NOINLINE int libnet_resolver_create_entry(const char *name, SceNetResolverParam *param)
{
	LibnetResolver *resolver;
	SceSize length;
	int descriptor, ret;
	char *object;

	length = libnet_strnlen(name, 31);
	if (length > 0x1F)
		return SCE_NET_ERROR_EINVAL;
	descriptor = libnet_descriptor_create(name, 0);
	if (descriptor < 0)
		return descriptor;
	if (param != NULL && param->allocate != NULL) {
		object = param->allocate(sizeof(*resolver), descriptor, name, param->user);
		resolver = (LibnetResolver *)object;
		if (object == NULL) {
			ret = SCE_NET_ERROR_ENOLIBMEM;
			goto release;
		}
		if (((unsigned int)object & 3) != 0) {
			ret = SCE_NET_ERROR_RESOLVER_EALIGNMENT;
			goto fail;
		}
		sceClibMemset(object, 0, sizeof(*resolver));
		resolver->socket = -1;
		sceClibMemcpy_safe(resolver->name, name, length);
		ret = libnet_sync_create(&resolver->sync, 0, resolver->name);
		if (ret >= 0)
			goto link;
		goto fail;
	}
	object = libnet_malloc(sizeof(*resolver), 2);
	resolver = (LibnetResolver *)object;
	if (object == NULL) {
		ret = SCE_NET_ERROR_ENOLIBMEM;
		goto release;
	}
	resolver->socket = -1;
	sceClibMemcpy_safe(resolver->name, name, length);
	ret = libnet_sync_create(&resolver->sync, 0, resolver->name);
	if (ret < 0)
		goto fail;
link:
	resolver->descriptor = descriptor;
	resolver->buffer = resolver->packet;
	resolver->bufferSize = 768;
	resolver->flags = 0x400;
	if (param != NULL) {
		resolver->param.allocate = param->allocate;
		resolver->param.free = param->free;
		resolver->param.user = param->user;
	}
	resolver->next = NULL;
	resolver->prev = libnet_resolver_tail;
	if (libnet_resolver_tail != NULL)
		libnet_resolver_tail->next = resolver;
	else
		libnet_resolver_head = resolver;
	libnet_resolver_tail = resolver;
	return descriptor;
fail:
	if (param != NULL && param->free != NULL)
		param->free(object, descriptor, name, param->user);
	else
		libnet_free(object);
release:
	libnet_descriptor_destroy(descriptor);
	return ret;
}

/* destroy RESOLVER: wait for a running operation, then free it and its descriptor */
static LIBNET_NOINLINE int libnet_resolver_destroy_entry(LibnetResolver *resolver)
{
	LibnetResolver *next, *prev;
	int ret;

	if ((resolver->flags & LIBNET_RESOLVER_DESTROYED) != 0)
		return SCE_NET_ERROR_EBADF;
	if ((resolver->flags & LIBNET_RESOLVER_BUSY) != 0) {
		resolver->flags |= LIBNET_RESOLVER_DESTROYED;
		libnet_unlock();
		libnet_sync_wait(&resolver->sync, NULL);
		libnet_lock();
	}
	next = resolver->next;
	prev = resolver->prev;
	if (next != NULL)
		next->prev = prev;
	else
		libnet_resolver_tail = prev;
	prev = resolver->prev;
	if (prev != NULL)
		prev->next = next;
	else
		libnet_resolver_head = next;
	libnet_sync_destroy(&resolver->sync);
	libnet_descriptor_destroy(resolver->descriptor);
	if (resolver->socket >= 0) {
		libnet_socket_close(resolver->socket);
		resolver->socket = -1;
	}
	if (resolver->param.free != NULL)
		resolver->param.free(resolver, resolver->descriptor, resolver->name,
				     resolver->param.user);
	else
		libnet_free(resolver);
	return 0;
}

/* stop RESOLVER's operation: 0, or ENOTBLK when none runs */
static LIBNET_NOINLINE int libnet_resolver_cancel(LibnetResolver *resolver)
{
	int flags, ret;

	flags = resolver->flags;
	if ((flags & LIBNET_RESOLVER_DONE) != 0)
		return 0;
	if (resolver->socket >= 0) {
		ret = libnet_socket_abort(resolver->socket, 3);
		flags = resolver->flags;
	} else {
		ret = flags & LIBNET_RESOLVER_DONE;
	}
	resolver->flags = flags | LIBNET_RESOLVER_ABORTED;
	if ((flags & LIBNET_RESOLVER_BUSY) == 0) {
		if ((flags & LIBNET_RESOLVER_ACTIVE) != 0) {
			int updated = flags & 0xFFFFFFFE;

			__asm__ volatile ("" : "+r" (updated));
			resolver->flags = updated | 0x90;
			resolver->result = SCE_NET_ERROR_EINTR;
		} else {
			return SCE_NET_ERROR_ENOTBLK;
		}
	}
	return ret;
}

/* register RESOLVER's socket with its descriptor, or its timeouts for TIMEOUT microseconds */
static LIBNET_NOINLINE int libnet_resolver_register(LibnetResolver *resolver, int timeout)
{
	int flags, op, ret, value;

	if ((resolver->flags & LIBNET_RESOLVER_ASYNC) != 0) {
		flags = resolver->flags;
		resolver->waitTimeout = timeout;
		op = (flags & LIBNET_RESOLVER_REGISTERED) != 0 ? 2 : 11;
		resolver->eventHandler = libnet_resolver_event;
		resolver->eventArgument = resolver;
		resolver->eventSocket = resolver->socket;
		ret = libnet_descriptor_control(resolver->descriptor, op, resolver->socket,
						&resolver->eventHandler);
		if (ret >= 0)
			resolver->flags |= LIBNET_RESOLVER_REGISTERED;
		return ret;
	}
	value = timeout;
	ret = libnet_setsockopt(resolver->socket, SCE_NET_SOL_SOCKET, SCE_NET_SO_SNDTIMEO,
				&value, 4);
	if (ret < 0)
		return ret;
	ret = libnet_setsockopt(resolver->socket, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO,
				&value, 4);
	if (ret < 0)
		return ret;
	flags = resolver->flags;
	if ((flags & LIBNET_RESOLVER_REGISTERED) == 0) {
		ret = libnet_descriptor_control(resolver->descriptor, 11, resolver->socket, NULL);
		if (ret < 0)
			return ret;
		flags = resolver->flags;
	}
	resolver->flags = flags | LIBNET_RESOLVER_REGISTERED;
	return ret;
}

/* give RESOLVER a socket for its operation */
static LIBNET_NOINLINE int libnet_resolver_socket(LibnetResolver *resolver)
{
	int type, ret, value;

	if (resolver->socket >= 0) {
		ret = 0;
	} else {
		type = (resolver->flags & LIBNET_RESOLVER_STREAM) != 0 ?
		       SCE_NET_SOCK_STREAM : SCE_NET_SOCK_DGRAM;
		ret = libnet_socket(resolver->name, SCE_NET_AF_INET, type, 0);
		if (ret < 0)
			return ret;
		resolver->socket = ret;
	}
	if ((resolver->flags & LIBNET_RESOLVER_ASYNC) != 0) {
		value = 1;
		ret = libnet_setsockopt(resolver->socket, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO,
					&value, 4);
		return ret & (ret >> 31);
	}
	return ret;
}

/* finish RESOLVER's operation: clear its active bit and, for epoll, keep its socket waiting */
static LIBNET_NOINLINE int libnet_resolver_complete(LibnetResolver *resolver)
{
	int flags, ret;

	flags = resolver->flags;
	{
		int updated = flags & 0xFFFFFFFE;

		__asm__ volatile ("" : "+r" (updated));
		resolver->flags = updated | LIBNET_RESOLVER_DONE;
	}
	if ((flags & LIBNET_RESOLVER_ASYNC) == 0)
		return 0;
	if ((flags & LIBNET_RESOLVER_CALLBACK) != 0) {
		if (resolver->socket >= 0) {
			libnet_socket_close(resolver->socket);
			resolver->socket = -1;
		}
		return 0;
	}
	ret = libnet_resolver_socket(resolver);
	if (ret >= 0) {
		ret = libnet_resolver_register(resolver, 0);
		return ret & (ret >> 31);
	}
	return ret;
}

/*
 * Compare the DNS name at *POINTER with NAME: case-insensitively, label by label, following the
 * compression pointers inside PACKET. *MATCHED is cleared when the name differs. The pointer's
 * position after the name is stored back.
 */
static LIBNET_NOINLINE int libnet_dns_name_compare(unsigned char **pointer, unsigned int end,
				   unsigned char *packet, unsigned int packetEnd,
				   const char *name, unsigned int nameEnd, int *matched)
{
	unsigned char *p, *next, *fallback;
	unsigned char label[4];
	const char *compare;
	unsigned int consumed, length, i;
	unsigned char a, b;

	p = *pointer;
	fallback = NULL;
	consumed = 0;
	length = packetEnd - (unsigned int)packet;
	*matched = 1;
again:
	next = p + 1;
	if ((unsigned int)next > end) {
		*pointer = fallback != NULL ? fallback : next;
		return SCE_NET_ERROR_RESOLVER_ENOSPACE;
	}
	for (;;) {
		sceClibMemcpy_safe(label, p, 1);
		switch (label[0] & LIBNET_DNS_LABEL_MASK) {
		case 0x01:
		case 0x10:
		case LIBNET_DNS_LABEL_POINTER:
			goto invalid;
		default:
			break;
		}
		if (label[0] == 0)
			goto root;
		p = &next[label[0]];
		consumed++;
		if (end < (unsigned int)p) {
			*pointer = fallback != NULL ? fallback : p;
			return SCE_NET_ERROR_RESOLVER_ENOSPACE;
		}
		if (*matched == 1) {
			if ((unsigned int)name + label[0] + 1 <= nameEnd) {
				compare = name;
				for (i = 0;; i++) {
					a = compare[i];
					b = next[i];
					if (a >= 'A' && a <= 'Z')
						a += 32;
					if (b >= 'A' && b <= 'Z')
						b += 32;
					if (a != b)
						goto differ;
					if (compare[i] == 0 || label[0] <= i + 1)
						break;
				}
				if (name[label[0]] != '.' && name[label[0]] != 0)
					goto differ;
				name += label[0] + 1;
				goto advanced;
			}
		differ:
			*matched = 0;
		}
	advanced:
		consumed += p - next;
		next = p + 1;
		if ((unsigned int)next <= end)
			goto again;
		*pointer = fallback != NULL ? fallback : next;
		return SCE_NET_ERROR_RESOLVER_ENOSPACE;
	}
root:
	if (name[-1] != 0)
		*matched = 0;
	if (fallback != NULL) {
		*pointer = fallback;
		return 0;
	}
	*pointer = next;
	return 0;
invalid:
	*pointer = fallback != NULL ? fallback : next;
	return SCE_NET_ERROR_RESOLVER_EPACKET;
}

/*
 * Expand the DNS name at SOURCE into OUT, dot separated, following the compression pointers
 * inside PACKET whose end is PACKET_END; *END is the output's end and gets its new position.
 */
static LIBNET_NOINLINE int libnet_dns_name_expand(unsigned char *source, unsigned int sourceEnd, char *out,
				  char **end, unsigned char *packet, unsigned int packetEnd)
{
	unsigned char *p, *next;
	unsigned char label[4];
	char *last;
	unsigned int consumed, length;

	p = source;
	last = NULL;
	consumed = 0;
	length = packetEnd - (unsigned int)packet;
again:
	next = p + 1;
	if ((unsigned int)next > sourceEnd)
		return SCE_NET_ERROR_RESOLVER_ENOSPACE;
	for (;;) {
		sceClibMemcpy_safe(label, p, 1);
		switch (label[0] & LIBNET_DNS_LABEL_MASK) {
		case 0x01:
		case 0x10:
			return SCE_NET_ERROR_RESOLVER_EPACKET;
		case LIBNET_DNS_LABEL_POINTER:
			goto compress;
		default:
			break;
		}
		if (label[0] == 0) {
			out[-1] = 0;
			*end = out;
			return 0;
		}
		p = &next[label[0]];
		consumed++;
		if (sourceEnd < (unsigned int)p)
			return SCE_NET_ERROR_RESOLVER_ENOSPACE;
		last = out + label[0] + 1;
		if ((unsigned int)last > (unsigned int)*end)
			return SCE_NET_ERROR_RESOLVER_ENOSPACE;
		sceClibMemcpy_safe(out, next, label[0]);
		consumed += p - next;
		next = p + 1;
		out[label[0]] = '.';
		out = last;
		if ((unsigned int)next <= sourceEnd)
			continue;
		return SCE_NET_ERROR_RESOLVER_ENOSPACE;
	}
compress:
	if (sourceEnd < (unsigned int)(p + 2))
		return SCE_NET_ERROR_RESOLVER_ENOSPACE;
	consumed += 2;
	next = (unsigned char *)((((*p & 0x3F) << 8) | p[1]) + (unsigned int)packet);
	if ((int)consumed >= (int)length)
		return SCE_NET_ERROR_RESOLVER_EPACKET;
	p = next;
	goto again;
}

/*
 * Build RESOLVER's query for HOSTNAME (which ends at HOSTNAME_END) at PACKET, whose room is
 * PACKET_SIZE, exchange it with each server until an answer arrives, and store the address at
 * ADDRESS (NTOA) or the name at NAME (ATON).
 */
static LIBNET_NOINLINE int libnet_resolver_query(LibnetResolver *resolver, unsigned char *packet, int packetSize,
				 const char *hostname, const char *hostnameEnd, void *address,
				 char *name, int nameSize)
{
	SceNetDnsInfo dnsInfo;
	SceUInt32 server[4];
	SceNetMsghdr message;
	SceNetIovec iovec[2];
	unsigned char buffer[32];
	unsigned char key[16];
	unsigned char *build, *buildEnd, *question, *p, *record, *rdata, *next;
	unsigned char *responseEnd;
	const char *label, *labelEnd;
	char *nameOutEnd;
	SceUInt64 start, now;
	unsigned short id, answerCount, answerIndex, qtype, tcpLength;
	unsigned int queryLength, received, remaining, length;
	int ret, flags, count, attempt, maxAttempt, serverIndex, serverCount;
	int state, totalWait, cacheUsed, asyncNow, responseLength, cacheResult;
	int rcode, type, resourceClass, rdlength, i;

	ret = libnet_resolver_socket(resolver);
	if (ret < 0)
		goto exit;
	ret = libnet_resolver_register(resolver, -1);
	if (ret < 0)
		goto exit;
	count = libnet_get_dns_info(&dnsInfo, 0);
	ret = count;
	if (count < 0)
		goto exit;
	if (count == 0) {
		flags = resolver->flags;
		if ((flags & LIBNET_RESOLVER_CALLBACK) != 0) {
			ret = SCE_NET_ERROR_RESOLVER_ENODNS;
			goto failed;
		}
		libnet_unlock();
		ret = libnet_icm_connect(resolver->socket,
					 (flags & LIBNET_RESOLVER_ASYNC) != 0 ? 128 : 0);
		libnet_lock();
		if (ret < 0) {
			flags = resolver->flags;
			goto failed;
		}
		flags = resolver->flags;
		if ((flags & LIBNET_RESOLVER_ASYNC) != 0)
			goto checkdone;
		count = libnet_get_dns_info(&dnsInfo, ret);
		ret = count;
		if (count < 0)
			goto exit;
		if (count == 0) {
			ret = SCE_NET_ERROR_RESOLVER_ENODNS;
			flags = resolver->flags;
			goto failed;
		}
	}
	flags = resolver->flags;
	resolver->serverCount = count;
	if ((flags & LIBNET_RESOLVER_ASYNC) != 0)
		maxAttempt = resolver->retry + 1;
	else
		maxAttempt = resolver->retry;
	attempt = resolver->attempt;
	if (maxAttempt <= attempt) {
		ret = SCE_NET_ERROR_RESOLVER_ETIMEDOUT;
		goto exit;
	}
	serverIndex = resolver->serverIndex;
	serverCount = count;
	state = 0;
	totalWait = 0;
	cacheUsed = 0;
	build = packet;
	ret = 0;

serverLoop:
	if (serverIndex >= serverCount)
		goto nextAttempt;
	question = build + 12;
	buildEnd = (unsigned char *)(packetSize + (unsigned int)build);
	resolver->attempt = attempt;
	if ((unsigned int)buildEnd < (unsigned int)(build + 12)) {
		ret = SCE_NET_ERROR_EINVAL;
		goto exit;
	}
	sceClibMemset(build, 0, 0xC);
	if (resolver->id != 0) {
		*(unsigned short *)build = resolver->id;
	} else {
		unsigned short nextId;

		do {
			*(unsigned short *)build = libnet_dns_transaction_id;
			nextId = libnet_random();
			id = *(unsigned short *)build;
			libnet_dns_transaction_id = nextId;
			if (nextId == id) {
				id = nextId;
				libnet_dns_transaction_id = nextId + 1;
			}
		} while (id == 0);
		resolver->id = id;
	}
	build[2] = 1;
	build[3] &= 0x7F;
	*((SceUShort16 *)build + 2) = 0x0100;
	label = hostname;
	p = question;
	for (;;) {
		length = 0;
		labelEnd = sceClibStrchr(label, '.');
		if (labelEnd == NULL) {
			labelEnd = sceClibStrchr(label, 0);
			length = 1;
		}
		i = (unsigned int)labelEnd - (unsigned int)label;
		if ((unsigned int)buildEnd < (unsigned int)&p[i + 1])
			goto tooSmall;
		buffer[0] = (unsigned char)i;
		sceClibMemcpy_safe(p, buffer, 1);
		sceClibMemcpy_safe(p + 1, label, (unsigned int)i);
		label += i;
		p += 1 + i;
		if (length != 0)
			break;
		label++;
	}
	if ((unsigned int)(p + 1) > (unsigned int)buildEnd)
		goto tooSmall;
	*p = 0;
	next = p + 5;
	if ((unsigned int)(p + 5) > (unsigned int)buildEnd)
		goto tooSmall;
	switch (resolver->operation) {
	case LIBNET_RESOLVER_ATON:
		qtype = 0x0C00;
		break;
	case LIBNET_RESOLVER_NTOA:
		qtype = 0x0100;
		break;
	default:
		qtype = 0;
		break;
	}
	sceClibMemcpy_safe(p + 1, &qtype, 2);
	qtype = 0x0100;
	sceClibMemcpy_safe(p + 3, &qtype, 2);
	flags = resolver->flags;
	if ((flags & LIBNET_RESOLVER_EDNS) != 0) {
		next = p + 16;
		if ((unsigned int)(p + 16) > (unsigned int)buildEnd)
			goto badSize;
		p[5] = 0;
		qtype = 0x2900;
		sceClibMemcpy_safe(p + 6, &qtype, 2);
		qtype = __builtin_bswap16(resolver->bufferSize);
		sceClibMemcpy_safe(p + 8, &qtype, 2);
		p[10] = 0;
		p[11] = 0;
		qtype = 0;
		sceClibMemcpy_safe(p + 12, &qtype, 2);
		qtype = 0;
		sceClibMemcpy_safe(p + 14, &qtype, 2);
	}
	queryLength = (unsigned int)(next - build);
	libnet_get_time(&start);

exchange:
	length = resolver->attempt;
	remaining = resolver->timeout << length;
	if ((int)length > 0)
		remaining = remaining / resolver->serverCount;
	libnet_get_time(&now);
	flags = resolver->flags;
	remaining -= (int)(now - start);
	now -= start;
	if ((flags & LIBNET_RESOLVER_STREAM) != 0) {
		totalWait += remaining;
		if (totalWait <= 5999999) {
			state = 2;
			goto nextServer;
		}
		remaining = 6000000;
	} else if ((int)remaining < 0) {
		resolver->flags = flags & 0xFFFFFFF7;
	}
	ret = libnet_resolver_register(resolver, remaining);
	flags = resolver->flags;
	if (ret < 0)
		goto exit;
	responseLength = 0;
	id = resolver->id;
	if ((flags & LIBNET_RESOLVER_ABORTED) != 0) {
		ret = SCE_NET_ERROR_EINTR;
		goto done;
	}
	if ((flags & LIBNET_RESOLVER_DESTROYED) != 0) {
		ret = SCE_NET_ERROR_EBADF;
		goto done;
	}
	if ((unsigned int)resolver->bufferSize - ((unsigned int)build -
	    (unsigned int)resolver->buffer) > 0)
		responseLength = resolver->bufferSize - (build - resolver->buffer);
	asyncNow = ((flags & (LIBNET_RESOLVER_ASYNC | LIBNET_RESOLVER_CALLBACK)) ==
		    LIBNET_RESOLVER_ASYNC);
	cacheResult = libnet_dns_cache_lookup(resolver, (int *)&dnsInfo.dns_addr[serverIndex],
					      (unsigned short *)build,
					      queryLength - ((flags & LIBNET_RESOLVER_EDNS) ?
							     11 : 0),
					      (unsigned short *)build, &responseLength);
	if (cacheResult == 1) {
		if (!asyncNow) {
			state = 0;
			cacheUsed = 1;
			received = responseLength;
			goto received;
		}
		libnet_resolver_register(resolver, 0);
		ret = 0;
		goto done;
	}
	flags = resolver->flags;
	if ((flags & LIBNET_RESOLVER_SENT) == 0) {
		sceClibMemset(server, 0, 0x10);
		server[1] = dnsInfo.dns_addr[serverIndex].s_addr;
		server[0] = 0x35000210;
		if ((flags & LIBNET_RESOLVER_CONNECTED) == 0) {
			libnet_unlock();
			ret = libnet_connect(resolver->socket, (SceNetSockaddr *)server, 16);
			libnet_lock();
			if (ret < 0)
				goto connectFailed;
		} else {
			goto send;
		}
	connected:
		flags = resolver->flags;
		if ((flags & LIBNET_RESOLVER_STREAM) != 0)
			resolver->flags = flags | LIBNET_RESOLVER_CONNECTED;
	send:
		libnet_unlock();
		if ((resolver->flags & LIBNET_RESOLVER_STREAM) != 0) {
			while (libnet_recv(resolver->socket, buffer, 32,
					   SCE_NET_MSG_DONTWAIT) > 0)
				;
			resolver->received = 0;
			iovec[1].iov_base = build;
			iovec[1].iov_len = queryLength;
			tcpLength = __builtin_bswap16(queryLength);
			iovec[0].iov_base = &tcpLength;
			iovec[0].iov_len = 2;
			sceClibMemset(&message, 0, 0x1C);
			message.msg_iov = iovec;
			message.msg_iovlen = 2;
			ret = libnet_sendmsg(resolver->socket, &message, 0);
		} else {
			ret = libnet_send(resolver->socket, build, queryLength, 0);
		}
		libnet_lock();
		if (ret < 0) {
			if (ret != SCE_NET_ERROR_EAGAIN)
				goto done;
			flags = resolver->flags;
			if ((flags & LIBNET_RESOLVER_ASYNC) == 0)
				goto done;
		}
	}
noSend:
	if (asyncNow) {
		ret = 0;
		goto done;
	}
	flags = resolver->flags;
	resolver->flags = flags & 0xFFFFFFF7;
	if ((flags & LIBNET_RESOLVER_ASYNC) == 0) {
		libnet_unlock();
		flags = resolver->flags;
	}
	if ((flags & LIBNET_RESOLVER_STREAM) == 0) {
		ret = libnet_recv(resolver->socket, build, responseLength, 0);
		if (ret == 0) {
			ret = SCE_NET_ERROR_RESOLVER_ETIMEDOUT;
			state = 2;
		}
		goto checked;
	}
	length = resolver->received;
	if (length == 0) {
		ret = libnet_recv(resolver->socket, buffer, 2, SCE_NET_MSG_WAITALL);
		if (ret < 0) {
			state = 0;
			goto checked;
		}
		if ((unsigned int)ret <= 1) {
			ret = SCE_NET_ERROR_RESOLVER_ETIMEDOUT;
			state = 12;
			goto checked;
		}
		length = (buffer[0] << 8) | buffer[1];
		resolver->received = length;
	}
	if ((unsigned int)responseLength >= length) {
		ret = libnet_recv(resolver->socket, build, length, SCE_NET_MSG_WAITALL);
		if (ret < 0 || (unsigned int)ret >= (unsigned int)resolver->received) {
			state = 0;
			goto checked;
		}
		ret = SCE_NET_ERROR_RESOLVER_ETIMEDOUT;
		state = 12;
		goto checked;
	}
	ret = SCE_NET_ERROR_EINVAL;
	state = 12;
checked:
	if ((resolver->flags & LIBNET_RESOLVER_ASYNC) == 0)
		libnet_lock();
	if (ret >= 0) {
		responseLength = ret;
		if (ret == 0)
			goto finish;
		received = ret;
		cacheUsed = 0;
		goto received;
	}
	goto connectFailed;

received:
	responseEnd = build + received;
	if (responseEnd < (unsigned char *)(build + 12)) {
		ret = SCE_NET_ERROR_EINVAL;
		state = 2;
		goto resend;
	}
	flags = resolver->flags;
	if ((flags & LIBNET_RESOLVER_STREAM) == 0 && *(unsigned short *)build != id) {
		ret = SCE_NET_ERROR_RESOLVER_ETIMEDOUT;
		state = 2;
		goto resend;
	}
	if ((build[2] & 0x80) == 0)
		goto badPacket;
	if ((build[2] & 0x78) != 0)
		goto badPacket;
	if (__builtin_bswap16(*((SceUShort16 *)build + 2)) != 1)
		goto badPacket;
	rcode = build[3] & 0xF;
	switch (rcode) {
	case 0:
		if ((build[2] & 2) == 0 || (flags & LIBNET_RESOLVER_TCP_RETRY) != 0)
			break;
		ret = SCE_NET_ERROR_RESOLVER_EPACKET;
		state = 11;
		flags = resolver->flags;
		if ((flags & LIBNET_RESOLVER_TCP_RETRY) == 0) {
			flags |= 0x3000;
			goto moved;
		}
		goto done;
	case 1:
		ret = SCE_NET_ERROR_RESOLVER_EFORMAT;
		goto finish;
	case 2:
		ret = SCE_NET_ERROR_RESOLVER_ESERVERFAILURE;
		state = 1;
		goto nextServer;
	case 3:
		ret = SCE_NET_ERROR_RESOLVER_ENOHOST;
		goto finish;
	case 4:
		ret = SCE_NET_ERROR_RESOLVER_ENOTIMPLEMENTED;
		goto finish;
	case 5:
		ret = SCE_NET_ERROR_RESOLVER_ESERVERREFUSED;
		goto finish;
	default:
		goto badPacket;
	}
	p = build + 12;
	length = 0;
	ret = libnet_dns_name_compare(&p, (unsigned int)responseEnd, build,
				      (unsigned int)responseEnd, hostname,
				      (unsigned int)hostnameEnd, &length);
	if (ret < 0)
		goto packetError;
	p += 4;
	if ((unsigned int)responseEnd < (unsigned int)p) {
		ret = SCE_NET_ERROR_EINVAL;
		goto packetError;
	}
	if (length == 0) {
		ret = SCE_NET_ERROR_RESOLVER_EPACKET;
		goto packetError;
	}
	question = p;
	length = 0;
	answerCount = __builtin_bswap16(*((SceUShort16 *)build + 3));
	if (answerCount == 0) {
		ret = SCE_NET_ERROR_RESOLVER_ENORECORD;
		flags = resolver->flags;
		goto resend;
	}
	label = hostname;
	labelEnd = hostnameEnd;
	answerIndex = 0;
answerLoop:
	ret = libnet_dns_name_compare(&question, (unsigned int)responseEnd, build,
				      (unsigned int)responseEnd, label,
				      (unsigned int)labelEnd, &length);
	if (ret < 0)
		goto answerError;
	record = question + 10;
	if ((unsigned int)responseEnd < (unsigned int)record)
		goto recordError;
	type = (question[0] << 8) | question[1];
	resourceClass = (question[2] << 8) | question[3];
	rdlength = (question[8] << 8) | question[9];
	rdata = question + 10;
	question = record;
	next = rdata + rdlength;
	if ((unsigned int)responseEnd < (unsigned int)next)
		goto recordError;
	if (length == 0 || resourceClass != 1)
		goto nextRecord;
	if (type == 1) {
		if (rdlength != 4) {
			ret = SCE_NET_ERROR_RESOLVER_EPACKET;
			flags = resolver->flags;
			goto resend;
		}
		sceClibMemcpy_safe(address, rdata, 4);
		goto found;
	}
	if (type == 5) {
		nameOutEnd = (char *)((unsigned int)(resolver->buffer +
						     resolver->bufferSize));
		ret = libnet_dns_name_expand(rdata, (unsigned int)responseEnd,
					     (char *)responseEnd, &nameOutEnd, build,
					     (unsigned int)responseEnd);
		if (ret < 0)
			goto answerError;
		question = next;
		label = (const char *)responseEnd;
		labelEnd = nameOutEnd;
		goto nextRecord;
	}
	if (type == 12) {
		if (name == NULL) {
			ret = SCE_NET_ERROR_RESOLVER_EPACKET;
			flags = resolver->flags;
			goto resend;
		}
		nameOutEnd = name + nameSize;
		ret = libnet_dns_name_expand(rdata, (unsigned int)responseEnd, name,
					     &nameOutEnd, build,
					     (unsigned int)responseEnd);
		if (ret < 0)
			goto answerError;
		goto found;
	}
nextRecord:
	if (++answerIndex == answerCount) {
		ret = SCE_NET_ERROR_RESOLVER_ENORECORD;
		flags = resolver->flags;
		goto resend;
	}
	goto answerLoop;
found:
	resolver->flags |= LIBNET_RESOLVER_DONE;
	if (cacheUsed == 0)
		libnet_dns_cache_store(resolver, (int *)&dnsInfo.dns_addr[serverIndex],
				       build, responseLength);
finish:
	if (state == 12) {
		flags = resolver->flags;
		goto movedRetry;
	}
	if (state != 2)
		goto retryCheck;
	flags = resolver->flags;
resend:
	resolver->flags = flags | LIBNET_RESOLVER_SENT;
	goto exchange;
packetError:
	flags = resolver->flags;
	state = 2;
	goto resend;
recordError:
	flags = resolver->flags;
	if ((flags & LIBNET_RESOLVER_STREAM) != 0) {
		state = 12;
		goto movedRetry;
	}
	ret = SCE_NET_ERROR_EINVAL;
	state = 2;
	goto resend;
answerError:
	if (ret != SCE_NET_ERROR_EINVAL) {
		flags = resolver->flags;
		state = 2;
		goto resend;
	}
	flags = resolver->flags;
	if ((flags & LIBNET_RESOLVER_STREAM) != 0) {
		state = 12;
		goto movedRetry;
	}
	ret = SCE_NET_ERROR_EINVAL;
	state = 2;
	goto resend;
badPacket:
	ret = SCE_NET_ERROR_RESOLVER_EPACKET;
	flags = resolver->flags;
	state = 2;
	goto resend;
retryCheck:
	if (state > 10) {
		flags = resolver->flags;
		goto moved;
	}
	if (state == 0)
		goto done;
nextServer:
	if (resolver->serverCount <= ++serverIndex) {
		resolver->serverIndex = 0;
		if (++attempt == maxAttempt) {
			if (ret < 0)
				goto exit;
			ret = SCE_NET_ERROR_RESOLVER_ETIMEDOUT;
			goto exit;
		}
		serverIndex = 0;
		serverCount = resolver->serverCount;
		goto serverLoop;
	}
	resolver->attempt = attempt;
	resolver->serverIndex = serverIndex;
	goto serverLoop;
nextAttempt:
	resolver->serverIndex = 0;
	if (++attempt == maxAttempt) {
		if (ret < 0)
			goto exit;
		ret = SCE_NET_ERROR_RESOLVER_ETIMEDOUT;
		goto exit;
	}
	serverIndex = 0;
	serverCount = resolver->serverCount;
	goto serverLoop;
movedRetry:
	flags = (flags & 0xFFFF4FFF) | LIBNET_RESOLVER_TCP_RETRY;
moved:
	resolver->flags = flags & 0xFFFEBFF3;
	if (resolver->socket >= 0) {
		libnet_socket_close(resolver->socket);
		resolver->socket = -1;
	}
	ret = libnet_resolver_socket(resolver);
	if (ret >= 0) {
		serverIndex--;
		goto nextServer;
	}
	goto exit;
connectFailed:
	if (ret != SCE_NET_ERROR_EINPROGRESS) {
		if (ret != SCE_NET_ERROR_EISCONN) {
			flags = resolver->flags;
			if ((flags & LIBNET_RESOLVER_STREAM) != 0) {
				state = 12;
				goto movedRetry;
			}
			goto done;
		}
		goto connected;
	}
	flags = resolver->flags;
	if ((flags & LIBNET_RESOLVER_ASYNC) == 0)
		goto done;
	if ((flags & LIBNET_RESOLVER_CONNECTING) == 0) {
		resolver->flags = flags | LIBNET_RESOLVER_CONNECTING;
		goto noSend;
	}
	if ((flags & LIBNET_RESOLVER_STREAM) == 0)
		goto done;
	state = 12;
	goto movedRetry;
done:
	flags = resolver->flags;
	resolver->serverIndex = serverIndex + 1;
	if (ret < 0)
		goto exit;
checkdone:
	if ((flags & LIBNET_RESOLVER_DONE) != 0)
		goto exit;
	return ret;
tooSmall:
	ret = SCE_NET_ERROR_EINVAL;
	goto exit;
badSize:
	ret = SCE_NET_ERROR_EINVAL;
	goto exit;
failed:
	resolver->flags = (flags & 0xFFFFFFFE) | LIBNET_RESOLVER_DONE;
	if (resolver->socket >= 0) {
		libnet_socket_close(resolver->socket);
		resolver->socket = -1;
	}
	return ret;
exit:
	flags = resolver->flags;
	resolver->flags = (flags & 0xFFFFFFFE) | LIBNET_RESOLVER_DONE;
	if (resolver->socket >= 0) {
		libnet_socket_close(resolver->socket);
		resolver->socket = -1;
	}
	return ret;
}

/*
 * Start RESOLVER's operation: NTOA builds and sends the query for its hostname, ATON for its
 * address's reverse name, then waits for the answer. A trailing dot is dropped, an address
 * given as a number is returned as is, and "localhost" is answered without a query.
 */
LIBNET_NOINLINE int libnet_resolver_start(LibnetResolver *resolver)
{
	char *hostname;
	SceSize length, nameLength, aligned;
	int flags, ret, result;

	length = resolver->bufferSize;
	flags = resolver->flags;
	if (length <= 4) {
		result = SCE_NET_ERROR_EINVAL;
		flags = (flags & 0xFFFFFFEE) | LIBNET_RESOLVER_DONE;
		resolver->flags = flags;
		goto done;
	}
	flags = resolver->operation;
	resolver->flags = flags | LIBNET_RESOLVER_BUSY;
	if (flags != LIBNET_RESOLVER_NTOA) {
		if (flags != LIBNET_RESOLVER_ATON) {
			result = SCE_NET_ERROR_RESOLVER_EINTERNAL;
			flags = (resolver->flags & 0xFFFFFFAE) | 0x50;
			resolver->flags = flags;
			goto done;
		}
		if (resolver->target.address.s_addr != 0x0100007F) {
			unsigned char *bytes = (unsigned char *)&resolver->target.address;

			result = sceClibSnprintf((char *)resolver->buffer, length,
						 "%u.%u.%u.%u.in-addr.arpa",
						 bytes[3], bytes[2], bytes[1], bytes[0]);
			aligned = (result + 4) & 0xFFFFFFFC;
			ret = libnet_resolver_query(resolver, resolver->buffer + aligned,
						    length - aligned,
						    (const char *)resolver->buffer,
						    (const char *)resolver->buffer +
						    result + 1,
						    &resolver->target.address,
						    resolver->names.reverse.name,
						    resolver->names.reverse.length);
			flags = resolver->flags;
			result = ret;
			goto done;
		}
		if (resolver->names.reverse.length <= 9) {
			result = SCE_NET_ERROR_EINVAL;
			flags = (resolver->flags & 0xFFFFFFAE) | 0x50;
			resolver->flags = flags;
			goto done;
		}
		sceClibStrncpy(resolver->names.reverse.name, "localhost", 10);
		goto complete;
	}
	hostname = resolver->names.hostname;
	nameLength = sceClibStrnlen(hostname, 0x100);
	length = nameLength;
	if (hostname[nameLength - 1] == '.') {
		sceClibStrncpy((char *)resolver->buffer, hostname, nameLength - 1);
		((char *)resolver->buffer)[nameLength - 1] = 0;
		aligned = (nameLength + 3) & 0xFFFFFFFC;
		length = resolver->bufferSize - aligned;
		hostname = (char *)resolver->buffer;
		if (nameLength == 10 && sceClibStrncmp(hostname, "localhost", 9) == 0) {
			resolver->target.address.s_addr = 0x0100007F;
			goto complete;
		}
		ret = libnet_resolver_query(resolver, resolver->buffer + aligned, length,
					    hostname, hostname + nameLength + 1,
					    resolver->target.addressPointer, NULL, 0);
	} else {
		length = nameLength + 1;
		if (length == 10 && sceClibStrncmp(hostname, "localhost", 9) == 0) {
			resolver->target.address.s_addr = 0x0100007F;
			goto complete;
		}
		if ((resolver->flags & LIBNET_RESOLVER_NO_IPADDRESS) != 0 ||
		    sceNetInetPton(SCE_NET_AF_INET, hostname,
				   resolver->target.addressPointer) == 0) {
			ret = libnet_resolver_query(resolver, resolver->buffer,
						    resolver->bufferSize, hostname,
						    hostname + length,
						    resolver->target.addressPointer, NULL, 0);
		} else {
			goto complete;
		}
	}
	flags = resolver->flags;
	result = ret;
	goto done;
complete:
	ret = libnet_resolver_complete(resolver);
	flags = resolver->flags;
	result = ret;
done:
	resolver->flags = flags & 0xFFFFFFBF;
	if ((flags & LIBNET_RESOLVER_ABORTED) != 0) {
		result = SCE_NET_ERROR_EINTR;
		if ((flags & LIBNET_RESOLVER_DESTROYED) != 0)
			goto destroyed;
	} else if ((flags & LIBNET_RESOLVER_DESTROYED) != 0) {
		result = SCE_NET_ERROR_EBADF;
		goto destroyed;
	}
	resolver->result = result;
	return result;
destroyed:
	libnet_sync_signal(&resolver->sync, 1);
	resolver->result = result;
	return result;
}

/* the epoll handler of RESOLVER: an event on its socket continues the operation */
LIBNET_NOINLINE int libnet_resolver_event(SceNetEpollEvent *event)
{
	LibnetResolver *resolver;
	int flags, ret, result;

	libnet_lock();
	resolver = (LibnetResolver *)event->system.system[1];
	if (libnet_resolver_exists(resolver) == 0) {
		libnet_unlock();
		return 0;
	}
	flags = resolver->flags;
	if ((flags & (LIBNET_RESOLVER_BUSY | LIBNET_RESOLVER_DONE)) != 0) {
		result = 0;
		resolver->result = 0;
	} else {
		if ((event->events & LIBNET_EPOLL_NOTICE) != 0)
			flags &= 0xFFFFFFF7;
		else
			flags |= LIBNET_RESOLVER_SENT;
		resolver->flags = flags | LIBNET_RESOLVER_CALLBACK;
		ret = libnet_resolver_start(resolver);
		flags = resolver->flags;
		if (ret < 0) {
			resolver->result = ret;
			result = 0;
		} else if ((flags & LIBNET_RESOLVER_DONE) != 0) {
			result = 0;
			resolver->result = 0;
		} else {
			result = 1;
		}
		resolver->flags = flags & 0xFFFFFFDF;
	}
	libnet_unlock();
	return result;
}

int libnet_resolver_create(const char *name, SceNetResolverParam *param, int flags)
{
	if (param != NULL) {
		if (param->allocate != NULL) {
			if (param->free == NULL)
				return SCE_NET_ERROR_EINVAL;
		} else if (param->free != NULL) {
			return SCE_NET_ERROR_EINVAL;
		}
	}
	if (flags != 0)
		return SCE_NET_ERROR_EINVAL;
	return libnet_resolver_create_entry(name, param);
}

int libnet_resolver_start_ntoa(int rid, const char *hostname, SceNetInAddr *addr, int timeout,
			       int retry, int flags)
{
	LibnetResolver *resolver;
	int kept;

	if ((flags & 0xFFFEFFFE) != 0 || (timeout | retry) < 0)
		return SCE_NET_ERROR_EINVAL;
	if (retry == 0)
		retry = 5;
	if (timeout == 0)
		timeout = 1000000;
	if (timeout <= 999999)
		return SCE_NET_ERROR_EINVAL;
	if (timeout <= 1999999) {
		if (--retry == 0)
			retry = 1;
		timeout = 2000000;
	}
	resolver = libnet_resolver_find(rid);
	if (resolver == NULL)
		return SCE_NET_ERROR_EBADF;
	if (addr == NULL || hostname == NULL ||
	    (unsigned int)(libnet_strnlen(hostname, 255) - 1) > 0xFE)
		return SCE_NET_ERROR_EINVAL;
	if ((resolver->flags & LIBNET_RESOLVER_KEEP_NTOA) != 0) {
		resolver->flags &= 0xFFFFFE7F;
		return SCE_NET_ERROR_EINTR;
	}
	if ((resolver->flags & LIBNET_RESOLVER_DESTROYED) != 0)
		return SCE_NET_ERROR_EBADF;
	if ((resolver->flags & LIBNET_RESOLVER_ACTIVE) != 0)
		return SCE_NET_ERROR_RESOLVER_EBUSY;
	kept = resolver->flags & LIBNET_RESOLVER_KEEP_ATON;
	if (resolver->socket >= 0) {
		libnet_socket_close(resolver->socket);
		resolver->socket = -1;
	}
	sceClibMemset(&resolver->operation, 0, 0x138);
	resolver->timeout = timeout;
	resolver->retry = retry;
	if ((flags & SCE_NET_RESOLVER_ASYNC) != 0)
		kept |= 3;
	else
		kept |= 1;
	resolver->flags = kept;
	resolver->target.addressPointer = addr;
	if ((flags & SCE_NET_RESOLVER_START_NTOA_DISABLE_IPADDRESS) != 0) {
		__asm__ volatile ("" ::: "memory");
		kept = resolver->flags;
		resolver->flags = kept | 0x800;
	}
	resolver->operation = LIBNET_RESOLVER_NTOA;
	sceClibStrncpy(resolver->names.hostname, hostname, 0xFF);
	resolver->socket = -1;
	return libnet_resolver_start(resolver);
}

int libnet_resolver_start_aton(int rid, const SceNetInAddr *addr, char *hostname, int len,
			       int timeout, int retry, int flags)
{
	LibnetResolver *resolver;
	int value;

	if ((flags & 0xFFFFFFFE) != 0 || (timeout | retry) < 0)
		return SCE_NET_ERROR_EINVAL;
	if (retry == 0)
		retry = 5;
	if (timeout == 0)
		timeout = 1000000;
	if (timeout <= 999999)
		return SCE_NET_ERROR_EINVAL;
	if (timeout <= 1999999) {
		if (--retry == 0)
			retry = 1;
		timeout = 2000000;
	}
	resolver = libnet_resolver_find(rid);
	if (resolver == NULL)
		return SCE_NET_ERROR_EBADF;
	if (addr == NULL || hostname == NULL || len <= 0)
		return SCE_NET_ERROR_EINVAL;
	if ((resolver->flags & LIBNET_RESOLVER_KEEP_ATON) != 0) {
		resolver->flags &= 0xFFFFFD7F;
		return SCE_NET_ERROR_EINTR;
	}
	if ((resolver->flags & LIBNET_RESOLVER_DESTROYED) != 0)
		return SCE_NET_ERROR_EBADF;
	if ((resolver->flags & LIBNET_RESOLVER_ACTIVE) != 0)
		return SCE_NET_ERROR_RESOLVER_EBUSY;
	value = resolver->flags & LIBNET_RESOLVER_KEEP_NTOA;
	if (resolver->socket >= 0) {
		libnet_socket_close(resolver->socket);
		resolver->socket = -1;
	}
	sceClibMemset(&resolver->operation, 0, 0x138);
	if ((flags & SCE_NET_RESOLVER_ASYNC) != 0)
		resolver->flags = value | 3;
	else
		resolver->flags = value | 1;
	resolver->timeout = timeout;
	resolver->retry = retry;
	resolver->operation = LIBNET_RESOLVER_ATON;
	sceClibMemcpy_safe(&resolver->target.address, addr, 4);
	resolver->names.reverse.name = hostname;
	resolver->names.reverse.length = len;
	resolver->socket = -1;
	return libnet_resolver_start(resolver);
}

int libnet_resolver_get_error(int rid, int *result)
{
	LibnetResolver *resolver;
	int error;

	if (result == NULL)
		return SCE_NET_ERROR_EINVAL;
	resolver = libnet_resolver_find(rid);
	if (resolver == NULL)
		return SCE_NET_ERROR_EBADF;
	error = resolver->result;
	if ((resolver->flags & LIBNET_RESOLVER_DESTROYED) == 0) {
		*result = error;
		return 0;
	}
	if (error == 0 || error == SCE_NET_ERROR_EBADF)
		return SCE_NET_ERROR_EBADF;
	*result = error;
	return 0;
}

int libnet_resolver_destroy(int rid)
{
	LibnetResolver *resolver;

	resolver = libnet_resolver_find(rid);
	if (resolver == NULL)
		return SCE_NET_ERROR_EBADF;
	return libnet_resolver_destroy_entry(resolver);
}

int libnet_resolver_abort(int rid, int flags)
{
	LibnetResolver *resolver;
	int result;

	if ((flags & 0xFFFFFFFC) != 0)
		return SCE_NET_ERROR_EINVAL;
	resolver = libnet_resolver_find(rid);
	if (resolver == NULL)
		return SCE_NET_ERROR_EBADF;
	result = libnet_resolver_cancel(resolver);
	if (result < 0 || (resolver->flags & LIBNET_RESOLVER_DONE) != 0) {
		if ((flags & 1) != 0) {
			result = 0;
			resolver->flags |= LIBNET_RESOLVER_KEEP_NTOA;
		}
		if ((flags & 2) != 0) {
			resolver->flags |= LIBNET_RESOLVER_KEEP_ATON;
			return 0;
		}
	}
	return result;
}

int libnet_clear_dns_cache(int flags)
{
	return libnet_dns_cache_clear();
}
