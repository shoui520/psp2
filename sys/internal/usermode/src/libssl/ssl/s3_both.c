/* Identified SSLeay handshake buffer code, adapted in s3_both_sce.patch. */
#include "ssl_locl.h"
#include "../memory.h"
#include "../err/err.h"

#define Malloc sslAllocate
#define memset sslFillMemory
#define SSL_F_SSL3_SETUP_BUFFERS 157
#define ERR_R_MALLOC_FAILURE 33
#define SSLerr(function, reason) sslPushError(20, function, reason, NULL, 0)

/* SSL-C extension; this descriptive function name is ours, not Sony's. */
int ssl3ResizeReadBuffer(SSL *ssl, int length)
{
	unsigned char *buffer = ssl->s3->rbuf.buf;
	unsigned char *replacement;
	SceUInt32 packetOffset = 0;
	int restorePacket = 0;

	if (length <= 0)
		length = SSL3_RT_MAX_PACKET_SIZE;
	if (ssl->read_buffer_size >= length && buffer != NULL)
		return 1;
	if (ssl->packet != NULL && buffer != NULL) {
		restorePacket = 1;
		packetOffset = (SceUIntPtr)ssl->packet - (SceUIntPtr)buffer;
	}
	if (buffer != NULL)
		replacement = sslReallocate(buffer, length, ssl->s3->rbuf.capacity);
	else
		replacement = sslAllocate(length);
	if (replacement == NULL)
		return 0;
	ssl->s3->rbuf.capacity = length;
	ssl->s3->rbuf.buf = replacement;
	ssl->read_buffer_size = length;
	if (restorePacket)
		ssl->packet = (unsigned char *)((SceUIntPtr)replacement + packetOffset);
	return 1;
}

#include "s3_both_sce.inc"
