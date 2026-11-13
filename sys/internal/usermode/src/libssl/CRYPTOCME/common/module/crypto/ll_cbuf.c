/* Filename and descriptive names are ours: CRYPTOCME's buffering of a
 * cipher object, which takes input of any length, and the saved state of
 * objects described by a schema.
 */
#include "ll_ciph.h"
#include "../../object.h"
#include "../dmem/dmem.h"
#include "../../../../memory.h"

/* The saved buffering state */
static const SceUInt32 sslCipherBufferStateSchema[] = {
	0x004010c2, 0x004020c2, 0x004070c2, 0x004080c2,
	0x01009001, 0x004060da, 0x004050da, 0x00004004,
	0x00000000
};

int sslFreeLowLevelCipher(SslCipherBuffer *buffer)
{
	SslDirectMemoryContext *dmem;

	if (buffer != NULL) {
		sslFreeCipherCore(buffer->core);
		dmem = buffer->dmem;
		if (buffer->buffer != NULL)
			dmem->methods->Free(buffer->buffer, dmem);
		if (buffer->flags & SSL_CIPHER_BUFFER_ALLOCATED)
			buffer->dmem->methods->Free(buffer, buffer->dmem);
	}
	return 0;
}

/* A failure to create the cipher is not reported */
int sslCreateLowLevelCipher(SslCipherBuffer **result,
			    const SslCipherDescriptor *descriptor,
			    SslDirectMemoryContext *dmem)
{
	SslCipherBuffer *allocated = NULL;
	SslCipherBuffer *buffer;
	int status;

	if (result == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	buffer = *result;
	if (dmem == NULL &&
	    (buffer == NULL || (dmem = buffer->dmem) == NULL))
		return SSL_CRYPTO_ERROR_NO_MEMORY_CONTEXT;
	if (buffer == NULL) {
		status = dmem->methods->Allocate((void **)&allocated,
						 sizeof(*allocated), dmem,
						 SSL_DMEM_ALLOCATE_ZERO);
		if (status != 0)
			return status;
		buffer = allocated;
		buffer->dmem = dmem;
		buffer->flags |= SSL_CIPHER_BUFFER_ALLOCATED;
	}
	status = sslCreateCipherCore(&buffer->core, descriptor, dmem);
	if (allocated != NULL) {
		if (status != 0)
			dmem->methods->Free(allocated, dmem);
		else
			*result = buffer;
	}
	return 0;
}

int sslControlLowLevelCipher(SslCipherBuffer *buffer, const void *key,
			     SceUInt32 keyLength, const unsigned char *iv,
			     int encrypt, int flags)
{
	if (buffer == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	buffer->encrypt = encrypt;
	buffer->count = 0;
	return sslSetCipherCoreKey(buffer->core, key, keyLength, iv,
				   encrypt | flags);
}

int sslInitLowLevelCipher(SslCipherBuffer *buffer, const void *key,
			  SceUInt32 keyLength, const unsigned char *iv,
			  int encrypt)
{
	int status = SSL_CRYPTO_ERROR_INVALID_STATE;

	if (buffer != NULL) {
		buffer->encrypt = encrypt;
		buffer->count = 0;
		status = sslSetCipherCoreKey(buffer->core, key, keyLength, iv,
					     encrypt | SSL_CIPHER_SAVE_KEY);
	}
	return status;
}

int sslProcessLowLevelCipher(SslCipherBuffer *buffer, unsigned char *output,
			     SceUInt32 *outputLength, SceUInt32 max,
			     const unsigned char *input, SceUInt32 length,
			     int options)
{
	SslCipherCore *core;
	SceUInt32 remaining = length;
	SceUInt32 done;
	SceUInt32 count, limit, excess, reserve, blockSize, fill, n;
	SceUInt32 blocks;
	SceUInt32 padded;
	int final;
	int status = 0;

	if (length > max)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	done = 0;
	if (buffer->buffer == NULL) {
		core = buffer->core;
		if ((buffer->flags & SSL_CIPHER_BUFFER_PADDING) &&
		    core->descriptor->blockSize > 1)
			return SSL_CRYPTO_ERROR_UNHANDLED;
		if (length % buffer->core->descriptor->blockSize != 0)
			return SSL_CRYPTO_ERROR_NOT_FOUND;
		if (length != 0) {
			status = sslProcessCipherCore(core, output, input,
						      length);
			if (status != 0)
				return status;
		}
		*outputLength = length;
		return 0;
	}
	count = buffer->count;
	if (count != 0) {
		if (length != 0) {
			limit = buffer->limit;
			if (count + length > limit) {
				excess = count + length - limit;
				reserve = buffer->reserve;
				blockSize = buffer->blockSize;
				if (excess < reserve) {
					/* Fill the first block */
					if (count < blockSize) {
						n = blockSize - count;
						sslCopyMemory(buffer->buffer +
								      count,
							      input, n);
						blockSize = buffer->blockSize;
						count = buffer->count + n;
						input += n;
						remaining = length - n;
						reserve = buffer->reserve;
						buffer->count = count;
					}
					if (count - blockSize + remaining >=
					    reserve) {
						status = sslProcessCipherCore(
							buffer->core, output,
							buffer->buffer,
							blockSize);
						if (status != 0)
							return status;
						blockSize = buffer->blockSize;
						count = buffer->count -
							blockSize;
						output += blockSize;
						buffer->count = count;
						done = blockSize;
						if (count != 0)
							sslCopyMemory(
								buffer->buffer,
								buffer->buffer +
									blockSize,
								count);
					}
				} else {
					/* Complete the buffered blocks */
					fill = (count + blockSize - 1) /
					       blockSize * blockSize;
					n = fill - count;
					sslCopyMemory(buffer->buffer + count,
						      input, n);
					status = sslProcessCipherCore(
						buffer->core, output,
						buffer->buffer, fill);
					core = buffer->core;
					if (status != 0)
						return status;
					remaining = length - n;
					input += n;
					output += fill;
					excess -= buffer->reserve;
					buffer->count = done;
					blockSize = buffer->blockSize;
					if (buffer->reserve == 0)
						excess--;
					done = fill;
					blocks = excess / blockSize;
					if (remaining > blockSize &&
					    blocks != 0) {
						n = blocks * blockSize;
						status = sslProcessCipherCore(
							core, output, input,
							n);
						if (status != 0)
							return status;
						output += n;
						done += n;
						input += n;
						remaining -= n;
					}
				}
			}
		}
	} else {
		if (buffer->flags & SSL_CIPHER_BUFFER_PADDING) {
			blocks = done;
			if (length > buffer->reserve)
				blocks = (length - buffer->reserve) /
					 buffer->blockSize;
		} else {
			blocks = length / buffer->blockSize;
		}
		if (blocks != 0) {
			n = blocks * buffer->blockSize;
			status = sslProcessCipherCore(buffer->core, output,
						      input, n);
			if (status != 0)
				return status;
			input += n;
			output += n;
			done = n;
			remaining = length - n;
		}
	}
	if (remaining != 0) {
		sslCopyMemory(buffer->buffer + buffer->count, input,
			      remaining);
		buffer->count += remaining;
	}
	final = options & SSL_CIPHER_BUFFER_FINAL;
	if (final) {
		if (!(buffer->flags & SSL_CIPHER_BUFFER_PADDING)) {
			count = buffer->count;
			if (count != 0 && count % buffer->blockSize == 0) {
				status = sslProcessCipherCore(buffer->core,
							      output,
							      buffer->buffer,
							      buffer->count);
				if (status != 0)
					return status;
				done += count;
				buffer->count = 0;
			}
		} else {
			padded = 0;
			status = buffer->Pad(buffer->core, output, &padded,
					     max, buffer->buffer,
					     buffer->count,
					     buffer->padParameter, 0);
			if (status != 0)
				return status;
			done += padded;
			buffer->count = 0;
		}
		if (buffer->count != 0)
			return SSL_CRYPTO_ERROR_NOT_FOUND;
	}
	if (outputLength != NULL)
		*outputLength = done;
	return status;
}

int sslSetLowLevelCipherInfo(SslCipherBuffer *buffer, int selector,
			     SceUInt32 value, void *pointer)
{
	SceUInt32 blockSize;
	SceUInt32 size;
	int status;

	if (buffer == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	switch (selector) {
	case SSL_CIPHER_BUFFER_INFO_BUFFERING:
		if (buffer->count != 0)
			return SSL_CRYPTO_ERROR_FAILED;
		if (value == 0) {
			blockSize = 0;
		} else {
			status = sslQueryCipherDescriptorInfo(
				NULL, buffer->core, SSL_CIPHER_INFO_BLOCK_SIZE,
				&blockSize, NULL);
			if (status != 0)
				return status;
			if (blockSize <= 1)
				return 0;
		}
		buffer->blockSize = blockSize;
		if (buffer->buffer != NULL) {
			buffer->dmem->methods->Free(buffer->buffer,
						    buffer->dmem);
			buffer->buffer = NULL;
		}
		break;
	case SSL_CIPHER_BUFFER_INFO_PAD_FUNCTION:
		buffer->Pad = (SslCipherPadFunction)pointer;
		break;
	case SSL_CIPHER_BUFFER_INFO_RESERVE:
		status = sslQueryCipherDescriptorInfo(
			NULL, buffer->core, SSL_CIPHER_INFO_BLOCK_SIZE,
			&blockSize, NULL);
		if (status != 0)
			return status;
		if (blockSize <= 1)
			return 0;
		buffer->blockSize = blockSize;
		buffer->reserve = value;
		buffer->limit = value + blockSize - 1;
		if (buffer->count != 0)
			return SSL_CRYPTO_ERROR_FAILED;
		size = (buffer->limit + blockSize - 1) / blockSize * blockSize;
		if (buffer->buffer != NULL && buffer->limit <= size)
			return 0;
		if (buffer->buffer != NULL) {
			buffer->dmem->methods->Free(buffer->buffer,
						    buffer->dmem);
			buffer->buffer = NULL;
		}
		status = buffer->dmem->methods->Allocate(
			(void **)&buffer->buffer, size, buffer->dmem, 0);
		if (status != 0)
			return status;
		break;
	case SSL_CIPHER_BUFFER_INFO_PADDING:
		if (value != 0) {
			buffer->flags |= SSL_CIPHER_BUFFER_PADDING;
		} else {
			if (buffer->count != 0)
				return SSL_CRYPTO_ERROR_FAILED;
			buffer->reserve = 0;
			buffer->flags &= ~SSL_CIPHER_BUFFER_PADDING;
		}
		break;
	case SSL_CIPHER_BUFFER_INFO_PAD_PARAMETER:
		buffer->padParameter = pointer;
		break;
	default:
		return sslSetCipherCoreInfo(buffer->core, selector, value,
					    pointer);
	}
	return 0;
}

int sslGetLowLevelCipherInfo(SslCipherBuffer *buffer, int selector,
			     SceUInt32 *value, void **pointer)
{
	int status = 0;

	if (buffer == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	switch (selector) {
	case SSL_CIPHER_BUFFER_INFO_BUFFERING:
		*value = buffer->blockSize != 0;
		break;
	case SSL_CIPHER_BUFFER_INFO_RESERVE:
		*value = buffer->reserve;
		break;
	case SSL_CIPHER_BUFFER_INFO_PADDING:
		*value = (buffer->flags & SSL_CIPHER_BUFFER_PADDING) != 0;
		break;
	case SSL_CIPHER_BUFFER_INFO_PAD_PARAMETER:
		*pointer = buffer->padParameter;
		break;
	default:
		return sslQueryCipherCoreInfo(buffer->core, selector, value,
					      pointer);
	}
	return status;
}

/* Each integer as its saved width, big-endian */
#define SSL_STATE_PUT(p, width, value)                    \
	do {                                              \
		switch (width) {                          \
		case 4:                                   \
			*(p)++ = (value) >> 24;           \
			*(p)++ = (value) >> 16;           \
			/* fall through */                \
		case 2:                                   \
			*(p)++ = (value) >> 8;            \
			/* fall through */                \
		case 1:                                   \
			*(p)++ = (value);                 \
		}                                         \
	} while (0)

int sslEncodeObjectState(const void *object, const SceUInt32 *schema,
			 unsigned char *buffer, SceUInt32 *length,
			 SceUInt32 max)
{
	SceUInt32 value = 0;
	SceUInt32 total = 0;
	const unsigned char *base = object;
	const unsigned char *p;
	SceUInt32 entry = *schema;
	SceUInt32 kind = SSL_STATE_KIND(entry);
	SceUInt32 width, stored, size, count, n, bytes, i;
	int shift;
	int status = 0;

	do {
		p = (entry & SSL_STATE_FROM_BASE ? base :
						   (const unsigned char *)
							   object) +
		    SSL_STATE_OFFSET(entry);
		width = SSL_STATE_WIDTH(entry);
		stored = width + 1;
		size = SSL_STATE_SIZE(entry) + 1;
		count = value;
		if (entry & SSL_STATE_COUNT_MASK)
			count = SSL_STATE_COUNT(entry);
		n = count;
		shift = 0;
		switch (kind) {
		case SSL_STATE_POINTER:
		case SSL_STATE_ALLOCATED:
			p = *(const unsigned char *const *)p;
			if (p == NULL)
				n = 0;
			/* fall through */
		case SSL_STATE_BYTES:
			bytes = n + n * width;
			total += bytes + 2;
			if (buffer == NULL)
				break;
			if (max < total) {
				status = SSL_CRYPTO_ERROR_NO_OPERATION;
				buffer = NULL;
			}
			if (buffer == NULL)
				break;
			*buffer++ = bytes >> 8;
			*buffer++ = bytes;
			if (stored == size) {
				if (bytes != 0 && p != NULL) {
					sslCopyMemory(buffer, p, bytes);
					buffer += bytes;
				}
				break;
			}
			for (i = 0; i < n; i++) {
				switch (stored) {
				case 4:
					*buffer++ = *p++;
					*buffer++ = *p++;
					/* fall through */
				case 2:
					*buffer++ = *p++;
					/* fall through */
				case 1:
					*buffer++ = *p++;
				}
				p += size - stored;
			}
			break;
		case SSL_STATE_BASE:
			base = *(const unsigned char *const *)p;
			break;
		case SSL_STATE_INTEGERS_SHIFT2:
			shift = 2;
			goto integers;
		case SSL_STATE_INTEGERS_SHIFT3:
			shift = 3;
			/* fall through */
		case SSL_STATE_INTEGERS:
		integers:
			bytes = count + count * width;
			total += bytes + 2;
			if (buffer != NULL) {
				if (max < total) {
					status = SSL_CRYPTO_ERROR_NO_OPERATION;
					buffer = NULL;
				}
				if (buffer != NULL) {
					*buffer++ = bytes >> 8;
					*buffer++ = bytes;
					value = 0;
					for (i = 0; i < n; i++) {
						switch (size) {
						case 1:
							value = *p;
							break;
						case 2:
							value = *(const unsigned short *)p;
							break;
						case 4:
						case 8:
							value = *(const SceUInt32 *)p;
							break;
						}
						p += size;
						if (shift != 0)
							value >>= shift;
						SSL_STATE_PUT(buffer, stored,
							      value);
					}
					break;
				}
			}
			/* The count of the next member */
			if (!(schema[1] & SSL_STATE_COUNT_MASK)) {
				switch (size) {
				case 1:
					value = *p;
					break;
				case 2:
					value = *(const unsigned short *)p;
					break;
				case 4:
				case 8:
					value = *(const SceUInt32 *)p;
					break;
				}
			}
			break;
		}
		entry = *++schema;
		kind = SSL_STATE_KIND(entry);
	} while (kind != SSL_STATE_END);
	if (length != NULL)
		*length = total;
	return status;
}

int sslSaveLowLevelCipherState(SslCipherBuffer *buffer, unsigned char *data,
			       SceUInt32 *length, SceUInt32 max)
{
	SceUInt32 coreLength = 0;
	int status;

	status = sslEncodeObjectState(buffer, sslCipherBufferStateSchema,
				      data, length, max);
	if (status != 0)
		return status;
	if (buffer->core != NULL) {
		if (data != NULL) {
			data += *length;
			max -= *length;
		}
		status = sslSaveCipherCoreState(buffer->core, data,
						&coreLength, max);
		if (status != SSL_CRYPTO_ERROR_NO_IMPLEMENTATION &&
		    status != 0)
			return status;
		*length += coreLength;
	}
	return 0;
}

int sslDecodeObjectState(void *object, const SceUInt32 *schema,
			 const unsigned char *buffer, SceUInt32 length,
			 SceUInt32 *used, SslDirectMemoryContext *dmem)
{
	SceUInt32 value = 0;
	SceUInt32 total = 0;
	unsigned char *base = object;
	unsigned char *member;
	unsigned char *p;
	SceUInt32 entry = *schema;
	SceUInt32 kind = SSL_STATE_KIND(entry);
	SceUInt32 offset, width, stored, size, count, bytes, pad, rest, i;
	SceUInt32 v;
	unsigned long long v64;
	int shift;
	int status = 0;

	for (;;) {
		member = entry & SSL_STATE_FROM_BASE ? base :
						       (unsigned char *)object;
		offset = SSL_STATE_OFFSET(entry);
		width = SSL_STATE_WIDTH(entry);
		stored = width + 1;
		size = SSL_STATE_SIZE(entry) + 1;
		p = member + offset;
		if (kind == SSL_STATE_BASE) {
			base = *(unsigned char **)(member + offset);
			goto next;
		}
		if (kind == SSL_STATE_END)
			break;
		if (total + 2 > length)
			return SSL_CRYPTO_ERROR_NO_OPERATION;
		bytes = (buffer[0] << 8) | buffer[1];
		buffer += 2;
		if (bytes + total + 2 > length)
			return SSL_CRYPTO_ERROR_NO_OPERATION;
		total += bytes + 2;
		count = value;
		if (entry & SSL_STATE_COUNT_MASK)
			count = SSL_STATE_COUNT(entry);
		if (bytes != 0 && count + count * width != bytes)
			return SSL_CRYPTO_ERROR_NOT_FOUND;
		shift = 0;
		switch (kind) {
		case SSL_STATE_ALLOCATED:
			if (*(void **)(member + offset) != NULL) {
				dmem->methods->Free(
					*(void **)(member + offset), dmem);
				*(void **)(member + offset) = NULL;
			}
			if (count != 0) {
				status = dmem->methods->Allocate(
					(void **)p,
					count + count * SSL_STATE_SIZE(entry),
					dmem, 0);
				if (status != 0)
					return status;
			}
			/* fall through */
		case SSL_STATE_POINTER:
			p = *(unsigned char **)(member + offset);
			/* fall through */
		case SSL_STATE_BYTES:
			if (stored == size) {
				if (bytes != 0)
					sslCopyMemory(p, buffer, bytes);
				buffer += bytes;
				break;
			}
			for (i = 0; i < count; i++) {
				switch (stored) {
				case 4:
					*p++ = *buffer++;
					*p++ = *buffer++;
					/* fall through */
				case 2:
					*p++ = *buffer++;
					/* fall through */
				case 1:
					*p++ = *buffer++;
				}
				p += size - stored;
			}
			break;
		case SSL_STATE_INTEGERS_SHIFT2:
			shift = 2;
			goto integers;
		case SSL_STATE_INTEGERS_SHIFT3:
			shift = 3;
			/* fall through */
		case SSL_STATE_INTEGERS:
		integers:
			pad = size - stored;
			for (i = 0; i < count; i++) {
				v = value;
				switch (stored) {
				case 1:
					v = *buffer++;
					if (shift != 0) {
						/* A shifted byte takes two */
						v <<= shift;
						*(unsigned short *)p = v;
						p += 2;
						pad--;
					} else {
						*p++ = v;
					}
					break;
				case 2:
					v = buffer[1] | (buffer[0] << 8);
					buffer += 2;
					if (shift != 0)
						v = (unsigned short)(v << shift);
					*(unsigned short *)p = v;
					p += 2;
					break;
				case 4:
					v = ((SceUInt32)buffer[0] << 24) |
					    (buffer[1] << 16) |
					    (buffer[2] << 8) | buffer[3];
					buffer += 4;
					if (shift != 0)
						v <<= shift;
					*(SceUInt32 *)p = v;
					p += 4;
					break;
				case 8:
					v64 = ((unsigned long long)
						       (((SceUInt32)buffer[0] << 24) |
							(buffer[1] << 16) |
							(buffer[2] << 8) |
							buffer[3])
					       << 32) |
					      (((SceUInt32)buffer[4] << 24) |
					       (buffer[5] << 16) |
					       (buffer[6] << 8) | buffer[7]);
					buffer += 8;
					if (shift != 0)
						v64 <<= shift;
					*(unsigned long long *)p = v64;
					p += 8;
					v = (SceUInt32)v64;
					break;
				}
				value = v;
				for (rest = pad; rest != 0; rest--)
					*p++ = 0;
			}
			break;
		}
next:
		entry = *++schema;
		kind = SSL_STATE_KIND(entry);
		if (kind == SSL_STATE_END)
			break;
	}
	if (used != NULL)
		*used = total;
	return status;
}

int sslRestoreLowLevelCipherState(SslCipherBuffer *buffer,
				  const unsigned char *data,
				  SceUInt32 length)
{
	SceUInt32 used;
	int status;

	status = sslDecodeObjectState(buffer, sslCipherBufferStateSchema,
				      data, length, &used, buffer->dmem);
	if (status == 0 && buffer->core != NULL && length != used)
		status = sslRestoreCipherCoreState(buffer->core, data + used,
						   length - used);
	return status;
}
