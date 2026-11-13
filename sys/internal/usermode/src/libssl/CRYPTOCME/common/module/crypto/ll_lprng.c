/* Filename and descriptive names are ours: CRYPTOCME's generator that
 * calls the next one under a lock.
 */
#include "ll_rand.h"
#include "../../../../memory.h"

#define SSL_PRNG_LOCK_ACQUIRE 1
#define SSL_PRNG_LOCK_RELEASE 0

static int sslNullPrngLock(SslPrngLock *lock, int acquire, int arg,
			   void *value)
{
	return 0;
}

int sslInitLockedPrng(SslPrng *prng)
{
	SslPrngLock *lock = prng->state;

	lock->context = prng->dmem;
	lock->Lock = sslNullPrngLock;
	lock->reserved[0] = 0;
	lock->reserved[1] = 0;
	return 0;
}

int sslSeedLockedPrng(SslPrng *prng, const unsigned char *seed,
		      SceUInt32 length)
{
	SslPrngLock *lock = prng->state;
	int status;

	if (prng->next == NULL)
		return SSL_CRYPTO_ERROR_NO_IMPLEMENTATION;
	lock->Lock(lock, SSL_PRNG_LOCK_ACQUIRE, 0, NULL);
	status = sslSeedLowLevelPrngInline(prng->next, seed, length);
	lock->Lock(lock, SSL_PRNG_LOCK_RELEASE, 0, NULL);
	return status;
}

int sslGenerateLockedPrng(SslPrng *prng, unsigned char *output,
			  SceUInt32 *length, SceUInt32 max)
{
	SslPrngLock *lock = prng->state;
	int status = SSL_CRYPTO_ERROR_FAILED;

	if (prng->next != NULL) {
		lock->Lock(lock, SSL_PRNG_LOCK_ACQUIRE, 0, NULL);
		status = sslGenerateLowLevelPrngInline(prng->next, output,
						       length, max);
		lock->Lock(lock, SSL_PRNG_LOCK_RELEASE, 0, NULL);
	}
	return status;
}

int sslSetLockedPrngInfo(SslPrng *prng, int id, int arg, const void *value)
{
	SslPrngLock *lock = prng->state;
	int status = SSL_CRYPTO_ERROR_FAILED;

	if (prng->next != NULL) {
		lock->Lock(lock, SSL_PRNG_LOCK_ACQUIRE, 0, NULL);
		status = sslSetLowLevelPrngInfoInline(prng->next, id, arg,
						      value);
		lock->Lock(lock, SSL_PRNG_LOCK_RELEASE, 0, NULL);
	}
	return status;
}

int sslGetLockedPrngInfo(SslPrng *prng, int id, int arg, void *value)
{
	SslPrngLock *lock = prng->state;
	int status;

	lock->Lock(lock, SSL_PRNG_LOCK_ACQUIRE, 0, NULL);
	status = sslGetLowLevelPrngInfoInline(prng->next, id, arg, value);
	lock->Lock(lock, SSL_PRNG_LOCK_RELEASE, 0, NULL);
	return status;
}

int sslControlLockedPrng(SslPrng *prng, int id, int arg, const void *value)
{
	SslPrngLock *lock = prng->state;
	int status = SSL_CRYPTO_ERROR_FAILED;

	if (id == SSL_PRNG_CONTROL_LOCK) {
		if (arg == 1) {
			sslCopyMemory(prng->state, value, sizeof(*lock));
			return 0;
		}
	} else if (prng->next != NULL) {
		lock->Lock(lock, SSL_PRNG_LOCK_ACQUIRE, 0, NULL);
		status = sslControlLowLevelPrngInline(prng->next, id, arg,
						      value);
		lock->Lock(lock, SSL_PRNG_LOCK_RELEASE, 0, NULL);
	}
	return status;
}
