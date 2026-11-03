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
