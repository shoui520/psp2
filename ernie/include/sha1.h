/*
 * SHA-1 (app/sha1.c): RFC 3174's sample code, changed to keep the 64-byte message block and a
 * 16-word W in the caller's RAM (the context points to them), to read the message from
 * anywhere, and without the Corrupted state.
 */
#ifndef ERNIE_SHA1_H
#define ERNIE_SHA1_H

/* the RFC's codes, but Input after Result and a message too long have each other's number */
enum
{
	shaSuccess = 0,
	shaNull,		/* Null pointer parameter */
	shaStateError,		/* called Input after Result */
	shaInputTooLong		/* input data too long */
};

#define SHA1HashSize		20
#define SHA1BlockSize		64
#define SHA1WordsSize		16

typedef struct SHA1Context
{
	unsigned long		Intermediate_Hash[SHA1HashSize / 4];	/* Message Digest */
	unsigned long		Length_Low;		/* Message length in bits */
	unsigned long		Length_High;		/* Message length in bits */
	unsigned char __near	*Message_Block;		/* SHA1BlockSize bytes, the caller's */
	unsigned char		Message_Block_Index;	/* Index into message block array */
	unsigned char		Computed;		/* Is the digest computed? */
	unsigned long __near	*W;			/* SHA1WordsSize words, the caller's */
} SHA1Context;

extern int SHA1Reset(SHA1Context __near *context, unsigned char __near *block,
		     unsigned long __near *w);
extern int SHA1Result(SHA1Context __near *context, unsigned char __near *Message_Digest);
extern int SHA1Input(SHA1Context __near *context, const unsigned char *message_array,
		     unsigned long length);
extern int SHA1InputBlocks(SHA1Context __near *context, const unsigned char *data,
			   unsigned long blocks);

#endif
