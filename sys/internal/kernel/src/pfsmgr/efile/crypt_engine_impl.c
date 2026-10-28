/*
 * The crypt engine: one thread that takes block requests off a "todo" list,
 * runs them and puts them on a "done" list, where the submitting thread
 * waits for them.
 */

#include <threadmgr/threadmgr.h>
#include "pfsmgr_internal.h"

#define SCE_PFS_ERROR_NO_JOB	0x80140f09	/* name unknown */

#define PFS_CRYPT_ENGINE_STOP	0x1	/* name unknown */

static SceUInt32 g_pfsCryptEngineFlags;		/* names unknown */
static SceUID g_pfsCryptEngineThread;
static SceUID g_pfsCryptEngineTodoMtx;
static SceUID g_pfsCryptEngineTodoCnd;
static SceUID g_pfsCryptEngineDoneMtx;
static SceUID g_pfsCryptEngineDoneCnd;
static ScePfsListNode g_pfsCryptEngineTodo;
static ScePfsListNode g_pfsCryptEngineDone;

#define PFS_CRYPT_VERIFY	0
#define PFS_CRYPT_DECRYPT	1
#define PFS_CRYPT_ENCRYPT	2
#define PFS_CRYPT_SIGN		3

/* one run of sectors of a request (name unknown) */
typedef struct PfsCryptPrimitive {
	const void *dataKey;			/* the file's */
	const void *tweakKey;
	const ScePfsHash *secret;		/* its icv key */
	const SceUInt8 *src;
	SceUInt8 *dst;
	SceUInt32 reserved;			/* name unknown: the file's index */
	SceUInt16 mode;
	SceUInt16 flags;
	SceUInt16 keyId;
	SceUInt16 attr;
	ScePfsHash *icv;			/* the sectors' icvs */
	SceSize size;				/* bytes of the run */
	SceUInt64 offset;			/* of src[0], in the file */
	SceSize sectorSize;
	SceUInt32 count;			/* sectors */
} PfsCryptPrimitive;


/*
 * The work of one request: the icv check, the decryption, the encryption and
 * the icv generation over its run of sectors. Sony's 4.00 builds the read and
 * the write paths and the primitive they share into this one function
 * (pfsCryptEngineProcess); 3.200.010-CEXTOOL's records name them
 * _crypt_for_read, _crypt_for_write, _icv_gd_verify, _icv_sd_verify and
 * scePfsCryptEngineRequestProcess, which this file follows.
 */

/* the drives whose data cipher is AES-CBC with an IV from the byte offset;
 * the others are AES-XTS (the mask of the mode table) */
static inline int pfsCryptModeIsCbc(SceUInt16 mode)	/* name unknown */
{
	return (SceUInt32)(mode - 2) <= 0x1f &&
	       ((1u << (mode - 2) & 0xC0000B03) != 0);
}


static inline void pfsCryptPrimitiveInit(PfsCryptPrimitive *p, ScePfsBlockRequest *req)	/* name unknown */
{
	ScePfsFileKey *key = req->key;

	p->dataKey = &key->dataKey;
	p->tweakKey = &key->tweakKey;
	p->secret = &key->icvKey;
	p->mode = key->mode;
	p->flags = key->flags;
	p->keyId = key->a6;
	p->attr = key->attr;
	p->sectorSize = key->blockSize;
	p->src = NULL;
	p->dst = NULL;
	p->icv = NULL;
	p->size = 0;
	p->offset = 0;
	p->count = 0;
}


/* the sectors' icvs are checked before anything is decrypted, and built after
 * everything is encrypted (the request's result is 0x80140e02 when one does
 * not hold, unless the file's flags say not to care) */
static inline int pfsCryptIcvsHold(PfsCryptPrimitive *p, int build)	/* name unknown */
{
	SceSize ss = p->sectorSize;
	ScePfsHash key, icv;
	SceUInt32 index, k;

	if ((p->attr & 0x2000) != 0 || (p->attr & 0x8000) != 0 || (p->flags & 0x20) != 0)
		return 0;
	if ((p->flags & 0x41) == 0x41)
		return 0;
	if (pfsCryptModeIsCbc(p->mode)) {
		/* every sector's icv is keyed by its index */
		index = (SceUInt32)((SceInt64)p->offset / (SceInt64)ss);
		for (k = 0; k < p->count; k++) {
			SceSize rest = p->size - ss * k;
			SceSize len = rest < ss ? rest : ss;

			icv_set_hmac_sw(&key, p->secret, &index, sizeof(index));
			if (build)
				icv_set_hmac((ScePfsHash *)p->icv + k, &key, p->src + ss * k, len);
			else {
				icv_set_hmac(&icv, &key, p->src + ss * k, len);
				if (!icv_equal((const SceUInt8 *)p->icv + PFS_HASH_SIZE * k, &icv) &&
				    (p->flags & 9) != 1)
					return SCE_PFS_ERROR_ICV_HASH;
			}
			index++;
		}
		return 0;
	}
	for (k = 0; k < p->count; k++) {
		if (build) {
			icv_set_hmac((ScePfsHash *)p->icv + k, p->secret, p->src + ss * k, ss);
		} else {
			icv_set_hmac(&icv, p->secret, p->src + ss * k, ss);
			if (!icv_equal((const SceUInt8 *)p->icv + PFS_HASH_SIZE * k, &icv) &&
			    (p->flags & 9) != 1)
				return SCE_PFS_ERROR_ICV_HASH;
		}
	}
	return 0;
}


/* the run's sectors through the data cipher; the icvs decide whether it runs
 * at all, and a run without one copies the buffers over */
static inline int pfsCryptCipherRun(PfsCryptPrimitive *p, int encrypt)	/* name unknown */
{
	SceSize ss = p->sectorSize;
	SceUInt32 k;

	if ((p->attr & 0x4000) != 0 || (p->attr & 0x8000) != 0 || (p->flags & 0x41) == 0x41) {
		if (p->src != p->dst)
			memcpy(p->dst, p->src, p->size);
		return 0;
	}
	for (k = 0; k < p->count; k++) {
		SceSize rest = p->size - ss * k;
		SceSize len = rest < ss ? rest : ss;
		SceUInt64 sector = p->offset + ss * k;
		int ret;

		if (pfsCryptModeIsCbc(p->mode))
			ret = encrypt ?
			      CBC_Encrypt_ext(p->dataKey, p->tweakKey, sector, len, ss,
					      p->src + ss * k, p->dst + ss * k, p->flags, p->keyId) :
			      CBC_Decrypt_ext(p->dataKey, p->tweakKey, sector, len, ss,
					      p->src + ss * k, p->dst + ss * k, p->flags, p->keyId);
		else
			ret = encrypt ?
			      XTS_Encrypt_ext(p->dataKey, p->tweakKey, 128, sector, ss, ss,
					      p->src + ss * k, p->dst + ss * k, p->flags) :
			      XTS_Decrypt_ext(p->dataKey, p->tweakKey, 128, sector, ss, ss,
					      p->src + ss * k, p->dst + ss * k, p->flags);
		if (ret != 0)
			return ret;
	}
	return 0;
}


static inline int pfsCryptForRead(ScePfsBlockRequest *req)	/* _crypt_for_read */
{
	ScePfsFileKey *key = req->key;
	SceSize ss = key->blockSize;
	SceUInt8 *buf = pfsCryptModeIsCbc(key->mode) ? req->disk : req->plain;
	PfsCryptPrimitive p;
	int ret;

	pfsCryptPrimitiveInit(&p, req);
	p.icv = req->icvs;
	p.count = req->count;
	p.size = ss * (req->count - 1) + req->tailLen;
	p.offset = (SceUInt64)req->first * ss;
	p.src = buf;
	p.dst = buf;
	ret = pfsCryptIcvsHold(&p, 0);			/* PRIM_VERIFY */
	if (ret != 0)
		return ret;
	if (req->fullBlocks == 0)
		return pfsCryptCipherRun(&p, 0);	/* PRIM_DECRYPT, all into the buffer */
	if (req->headPartial != 0) {
		p.count = 1;
		p.offset = (SceUInt64)req->first * ss;
		p.src = buf;
		p.dst = buf;
		p.size = ss;
		pfsCryptCipherRun(&p, 0);
	}
	if (req->fullBlocks + req->headPartial < req->count) {
		p.count = 1;
		p.size = req->tailLen;
		p.src = buf + ss * (req->count - 1);
		p.dst = buf + ss * (req->count - 1);
		p.offset = (SceUInt64)ss * (req->count + req->first - 1);
		pfsCryptCipherRun(&p, 0);
	}
	p.count = req->fullBlocks;
	p.size = ss * req->fullBlocks;
	p.src = buf + ss * req->headPartial;
	p.dst = (SceUInt8 *)req->dst + (ss * req->headPartial - req->offsetInBlock);
	p.offset = (SceUInt64)ss * (req->headPartial + req->first);
	return pfsCryptCipherRun(&p, 0);		/* PRIM_DECRYPT, into the caller's buffer */
}


static inline int pfsCryptForWrite(ScePfsBlockRequest *req)	/* _crypt_for_write */
{
	ScePfsFileKey *key = req->key;
	SceSize ss = key->blockSize;
	SceUInt8 *buf = pfsCryptModeIsCbc(key->mode) ? req->disk : req->plain;
	PfsCryptPrimitive p;
	int ret;

	pfsCryptPrimitiveInit(&p, req);
	if (req->headLen != 0) {
		p.src = buf;
		p.dst = buf;
		p.size = req->headLen;
		p.icv = req->icvs;
		p.offset = (SceUInt64)req->first * ss;
		p.count = 1;
		ret = pfsCryptIcvsHold(&p, 0);		/* PRIM_VERIFY */
		if (ret != 0)
			return ret;
		pfsCryptCipherRun(&p, 0);		/* PRIM_DECRYPT */
	}
	if (req->tailLen != 0 && (req->count != 1 || req->headLen == 0)) {
		p.size = req->tailLen;
		p.src = buf + ss * (req->count - 1);
		p.dst = buf + ss * (req->count - 1);
		p.offset = (SceUInt64)ss * (req->count + req->first - 1);
		p.count = 1;
		p.icv = (ScePfsHash *)req->icvs + (req->count - 1);
		ret = pfsCryptIcvsHold(&p, 0);		/* PRIM_VERIFY */
		if (ret != 0)
			return ret;
		pfsCryptCipherRun(&p, 0);		/* PRIM_DECRYPT */
	}
	if (req->op == PFS_BLOCK_RESIZE) {
		if (req->length != 0)
			memset(buf + req->offsetInBlock, 0, req->length);
	} else if (req->length != 0) {
		memcpy(buf + req->offsetInBlock, req->src, req->length);
	}
	if (req->padLen != 0)
		memset(buf + req->length + req->offsetInBlock, 0, req->padLen);
	p.src = buf;
	p.dst = buf;
	p.icv = req->icvs;
	p.size = ss * (req->count - 1) + req->lastLen;
	p.count = req->count;
	p.offset = (SceUInt64)req->first * ss;
	pfsCryptCipherRun(&p, 1);			/* PRIM_ENCRYPT */
	return pfsCryptIcvsHold(&p, 1);			/* PRIM_SIGN */
}


void pfsCryptEngineProcess(ScePfsCryptJob *job)	/* scePfsCryptEngineRequestProcess */
{
	ScePfsBlockRequest *req = job->req;

	switch (req->op) {
	case PFS_BLOCK_WRITE:
	case PFS_BLOCK_RESIZE:
		job->result = pfsCryptForWrite(req);
		break;
	case PFS_BLOCK_READ:
		job->result = pfsCryptForRead(req);
		break;
	default:
		break;
	}
}
