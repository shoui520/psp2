/* Filename ours. SSL-C out-of-line forms of SSLeay's BIO flag operations. */
#include "bio.h"

BIO *BIO_set_flags(BIO *bio, int flags)
{
	if (bio != NULL)
		bio->flags |= flags;
	return bio;
}

int BIO_get_retry_flags(BIO *bio)
{
	if (bio == NULL)
		return 0;
	return bio->flags & (BIO_FLAGS_RWS | BIO_FLAGS_SHOULD_RETRY |
			     SCE_BIO_RETRY_EXTENSION);
}

BIO *BIO_clear_retry_flags(BIO *bio)
{
	if (bio != NULL)
		bio->flags &= ~(BIO_FLAGS_RWS | BIO_FLAGS_SHOULD_RETRY |
				SCE_BIO_RETRY_EXTENSION);
	return bio;
}

BIO *BIO_set_retry_read(BIO *bio)
{
	if (bio != NULL)
		bio->flags |= BIO_FLAGS_READ | BIO_FLAGS_SHOULD_RETRY;
	return bio;
}

BIO *BIO_set_retry_write(BIO *bio)
{
	if (bio != NULL)
		bio->flags |= BIO_FLAGS_WRITE | BIO_FLAGS_SHOULD_RETRY;
	return bio;
}

BIO *BIO_set_retry_special(BIO *bio)
{
	if (bio != NULL)
		bio->flags |= BIO_FLAGS_IO_SPECIAL | BIO_FLAGS_SHOULD_RETRY;
	return bio;
}

/* SSL-C also emits these upstream macros as null-safe query functions. */
int (BIO_should_read)(BIO *bio)
{
	if (bio == NULL)
		return -1;
	return BIO_should_read(bio);
}

int (BIO_should_write)(BIO *bio)
{
	if (bio == NULL)
		return -1;
	return BIO_should_write(bio);
}

int (BIO_should_io_special)(BIO *bio)
{
	if (bio == NULL)
		return -1;
	return BIO_should_io_special(bio);
}

BIO *BIO_set_callback(BIO *bio, long (*callback)(BIO *, int, char *, int, long, long))
{
	if (bio != NULL)
		bio->callback = callback;
	return bio;
}

BIO *BIO_set_callback_arg(BIO *bio, char *arg)
{
	if (bio != NULL)
		bio->cb_arg = arg;
	return bio;
}

char *BIO_get_callback_arg(BIO *bio)
{
	if (bio == NULL)
		return NULL;
	return bio->cb_arg;
}

long (*BIO_get_callback(BIO *bio))(BIO *, int, char *, int, long, long)
{
	if (bio == NULL)
		return NULL;
	return bio->callback;
}
