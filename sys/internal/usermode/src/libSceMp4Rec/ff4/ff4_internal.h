/*
 * Internal definitions and box structures for Sony FF4 MP4 muxer library.
 */
#ifndef FF4_INTERNAL_H
#define FF4_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include "../ff4_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* FF4 status codes */
#define FF4_OK                      0
#define FF4_ERR_INVALID_ARG         1
#define FF4_ERR_INVALID_STATE       2
#define FF4_ERR_NO_MEM              256
#define FF4_ERR_IO                  257
#define FF4_ERR_NOT_FOUND           512
#define FF4_ERR_OVERFLOW            513
#define FF4_ERR_UNSUPPORTED         514
#define FF4_ERR_FORMAT              4096
#define FF4_ERR_INTERNAL            0x10000000

/* Pool tags */
#define FF4_POOL_META               1
#define FF4_POOL_CACHE              2

/* FourCC constants */
#define FF4_FCC(a, b, c, d) \
	(((uint32_t)(uint8_t)(a) << 24) | ((uint32_t)(uint8_t)(b) << 16) | \
	 ((uint32_t)(uint8_t)(c) << 8)  | ((uint32_t)(uint8_t)(d)))

#define FF4_BOX_FTYP FF4_FCC('f','t','y','p')
#define FF4_BOX_MDAT FF4_FCC('m','d','a','t')
#define FF4_BOX_FREE FF4_FCC('f','r','e','e')
#define FF4_BOX_SKIP FF4_FCC('s','k','i','p')
#define FF4_BOX_UUID FF4_FCC('u','u','i','d')
#define FF4_BOX_MOOV FF4_FCC('m','o','o','v')
#define FF4_BOX_MVHD FF4_FCC('m','v','h','d')
#define FF4_BOX_IODS FF4_FCC('i','o','d','s')
#define FF4_BOX_MVEX FF4_FCC('m','v','e','x')
#define FF4_BOX_MEHD FF4_FCC('m','e','h','d')
#define FF4_BOX_TREX FF4_FCC('t','r','e','x')
#define FF4_BOX_TRAK FF4_FCC('t','r','a','k')
#define FF4_BOX_TKHD FF4_FCC('t','k','h','d')
#define FF4_BOX_EDTS FF4_FCC('e','d','t','s')
#define FF4_BOX_ELST FF4_FCC('e','l','s','t')
#define FF4_BOX_TREF FF4_FCC('t','r','e','f')
#define FF4_BOX_MDIA FF4_FCC('m','d','i','a')
#define FF4_BOX_MDHD FF4_FCC('m','d','h','d')
#define FF4_BOX_HDLR FF4_FCC('h','d','l','r')
#define FF4_BOX_MINF FF4_FCC('m','i','n','f')
#define FF4_BOX_VMHD FF4_FCC('v','m','h','d')
#define FF4_BOX_SMHD FF4_FCC('s','m','h','d')
#define FF4_BOX_NMHD FF4_FCC('n','m','h','d')
#define FF4_BOX_DINF FF4_FCC('d','i','n','f')
#define FF4_BOX_DREF FF4_FCC('d','r','e','f')
#define FF4_BOX_URL  FF4_FCC('u','r','l',' ')
#define FF4_BOX_URN  FF4_FCC('u','r','n',' ')
#define FF4_BOX_STBL FF4_FCC('s','t','b','l')
#define FF4_BOX_STSD FF4_FCC('s','t','s','d')
#define FF4_BOX_STTS FF4_FCC('s','t','t','s')
#define FF4_BOX_CTTS FF4_FCC('c','t','t','s')
#define FF4_BOX_STSC FF4_FCC('s','t','s','c')
#define FF4_BOX_STSZ FF4_FCC('s','t','s','z')
#define FF4_BOX_STCO FF4_FCC('s','t','c','o')
#define FF4_BOX_CO64 FF4_FCC('c','o','6','4')
#define FF4_BOX_STSS FF4_FCC('s','t','s','s')
#define FF4_BOX_MOOF FF4_FCC('m','o','o','f')
#define FF4_BOX_MFHD FF4_FCC('m','f','h','d')
#define FF4_BOX_TRAF FF4_FCC('t','r','a','f')
#define FF4_BOX_TFHD FF4_FCC('t','f','h','d')
#define FF4_BOX_TRUN FF4_FCC('t','r','u','n')
#define FF4_BOX_MFRA FF4_FCC('m','f','r','a')
#define FF4_BOX_TFRA FF4_FCC('t','f','r','a')
#define FF4_BOX_MFRO FF4_FCC('m','f','r','o')
#define FF4_BOX_AVC1 FF4_FCC('a','v','c','1')
#define FF4_BOX_AVCC FF4_FCC('a','v','c','C')
#define FF4_BOX_MP4V FF4_FCC('m','p','4','v')
#define FF4_BOX_MP4A FF4_FCC('m','p','4','a')
#define FF4_BOX_MP4S FF4_FCC('m','p','4','s')
#define FF4_BOX_ESDS FF4_FCC('e','s','d','s')
#define FF4_BOX_SINF FF4_FCC('s','i','n','f')
#define FF4_BOX_PROF FF4_FCC('P','R','O','F')
#define FF4_BOX_FPRF FF4_FCC('F','P','R','F')
#define FF4_BOX_VPRF FF4_FCC('V','P','R','F')
#define FF4_BOX_APRF FF4_FCC('A','P','R','F')
#define FF4_BOX_BPRF FF4_FCC('B','P','R','F')
#define FF4_BOX_PPRF FF4_FCC('P','P','R','F')
#define FF4_BOX_USMT FF4_FCC('U','S','M','T')
#define FF4_BOX_MTDT FF4_FCC('M','T','D','T')
#define FF4_BOX_MTDF FF4_FCC('M','T','D','F')
#define FF4_BOX_MTSD FF4_FCC('M','T','S','D')
#define FF4_BOX_MTHD FF4_FCC('M','T','H','D')
#define FF4_BOX_MTSM FF4_FCC('M','T','S','M')
#define FF4_BOX_MDST FF4_FCC('M','D','S','T')
#define FF4_BOX_ZPCK FF4_FCC('Z','P','C','K')
#define FF4_BOX_LBIN FF4_FCC('L','B','I','N')

static inline uint16_t ff4_bswap16(uint16_t v)
{
	return (uint16_t)((v >> 8) | (v << 8));
}

static inline uint32_t ff4_bswap32(uint32_t v)
{
	return ((v & 0x000000FFu) << 24) |
	       ((v & 0x0000FF00u) << 8)  |
	       ((v & 0x00FF0000u) >> 8)  |
	       ((v & 0xFF000000u) >> 24);
}

static inline uint64_t ff4_bswap64(uint64_t v)
{
	return ((uint64_t)ff4_bswap32((uint32_t)(v & 0xFFFFFFFFu)) << 32) |
	       (uint64_t)ff4_bswap32((uint32_t)(v >> 32));
}

static inline uint16_t ff4_get_be16(const uint8_t *p)
{
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static inline uint32_t ff4_get_be32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

static inline uint64_t ff4_get_be64(const uint8_t *p)
{
	return ((uint64_t)ff4_get_be32(p) << 32) | (uint64_t)ff4_get_be32(p + 4);
}

static inline void ff4_put_be16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)(v >> 8);
	p[1] = (uint8_t)(v & 0xFFu);
}

static inline void ff4_put_be32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)((v >> 16) & 0xFFu);
	p[2] = (uint8_t)((v >> 8) & 0xFFu);
	p[3] = (uint8_t)(v & 0xFFu);
}

static inline void ff4_put_be64(uint8_t *p, uint64_t v)
{
	ff4_put_be32(p, (uint32_t)(v >> 32));
	ff4_put_be32(p + 4, (uint32_t)(v & 0xFFFFFFFFu));
}

/* Session implementation (96 bytes) */
typedef struct Ff4SessionImpl {
	Ff4Callbacks cb;       /* +0x00 (56 bytes) */
	uint32_t limitEnabled; /* +0x38 */
	uint32_t pad;        /* +0x3C */
	uint64_t pool1Limit;   /* +0x40 */
	uint64_t pool2Limit;   /* +0x48 */
	uint64_t pool1Used;    /* +0x50 */
	uint64_t pool2Used;    /* +0x58 */
} Ff4SessionImpl;

typedef struct Ff4DateTime {
	uint16_t year;
	uint16_t month;
	uint16_t day;
	uint16_t hour;
	uint16_t minute;
	uint16_t second;
} Ff4DateTime;

typedef struct Ff4MemStats {
	uint64_t pool1Limit;
	uint64_t pool1Used;
	uint64_t pool1Remain;
	uint64_t pool2Limit;
	uint64_t pool2Used;
	uint64_t pool2Remain;
} Ff4MemStats;

typedef struct Ff4AllocHeader {
	char magic[3];
	uint8_t poolType;
	uint32_t allocSize;
} Ff4AllocHeader;

typedef struct Ff4Buffer {
	uint64_t size; /* +0x00 */
	uint8_t *data; /* +0x08 */
	uint32_t pad;
} Ff4Buffer;

typedef struct Ff4BufStream {
	void *fileCtx;     /* +0x00 */
	uint32_t pad;    /* +0x04 */
	uint64_t filePos;  /* +0x08 */
	uint64_t bufCap;   /* +0x10 */
	uint64_t bufValid; /* +0x18 */
	uint8_t *bufBase;  /* +0x20 */
	uint8_t *bufCur;   /* +0x24 */
} Ff4BufStream;

/* Common list node layouts used by ff4_util */
typedef struct Ff4NodeU32 {
	uint32_t value;
	struct Ff4NodeU32 *next;
} Ff4NodeU32;

typedef struct Ff4NodePair {
	uint32_t a;
	uint32_t b;
	struct Ff4NodePair *next;
} Ff4NodePair;

typedef struct Ff4NodeTriple {
	uint32_t a;
	uint32_t b;
	uint32_t c;
	struct Ff4NodeTriple *next;
} Ff4NodeTriple;

typedef struct Ff4NodeGroup {
	uint32_t tag;
	Ff4NodeU32 *children;
	Ff4NodeU32 *childrenTail;
	struct Ff4NodeGroup *next;
} Ff4NodeGroup;

typedef struct Ff4NodeBuf {
	uint8_t *data;
	uint32_t pad;
	uint64_t size;
	struct Ff4NodeBuf *next;
} Ff4NodeBuf;

typedef struct Ff4EditEntryNode {
	uint32_t f[5];
	struct Ff4EditEntryNode *next;
} Ff4EditEntryNode;

typedef struct Ff4MetaItemNode {
	uint8_t tag;
	uint8_t padA[3];
	uint32_t flags;
	uint8_t *data;
	uint32_t padB;
	uint64_t size;
	struct Ff4MetaItemNode *next;
} Ff4MetaItemNode;

typedef struct Ff4DrefEntry {
	uint32_t type;       /* +0x00 ('url ' or 'urn ') */
	uint32_t flags;      /* +0x04 */
	char *location;      /* +0x08 */
	uint32_t padA;      /* +0x0C */
	uint64_t locationSz; /* +0x10 */
	char *name;          /* +0x18 */
	uint32_t padB;      /* +0x1C */
	uint64_t nameSz;     /* +0x20 */
	struct Ff4DrefEntry *next; /* +0x28 */
	uint32_t padC;
} Ff4DrefEntry;

typedef struct Ff4TrefEntry {
	uint32_t type;
	Ff4NodeU32 *trackIds;
	Ff4NodeU32 *trackIdsTail;
	Ff4NodeU32 *extraIds;
	Ff4NodeU32 *extraIdsTail;
	struct Ff4TrefEntry *next;
} Ff4TrefEntry;

typedef struct Ff4ZpckHeader {
	uint32_t fourcc;
	uint8_t version;
	uint8_t pad;
	uint16_t flags;
} Ff4ZpckHeader;

typedef struct Ff4ZpckTableHdr {
	uint32_t version;
	uint32_t entryCount;
	uint64_t dataOffset;
	uint64_t dataSize;
} Ff4ZpckTableHdr;

typedef struct Ff4ZpckTable {
	uint32_t version;
	Ff4NodePair *head;
	Ff4NodePair *tail;
} Ff4ZpckTable;

/* ---- Core FF4 writer/format options & box model/plan structures ---- */

typedef struct Ff4FormatOpts {
	uint32_t boxSizeMode;     /* +0x00: 0=32-bit, 1=auto 32/64, 2=64-bit */
	uint32_t boxVerMode;      /* +0x04: 0=v0 only, 1=auto, 2=v0 strict, 3=v1 */
	uint32_t chunkOffMode;    /* +0x08: 0=stco, 1=auto, 2=co64 */
	uint32_t mehdMode;        /* +0x0C: 0=omit, 1=auto, 2=v0, 3=v1 */
	uint32_t tfraMode;        /* +0x10 */
} Ff4FormatOpts;

typedef struct Ff4BoxDataBuf {
	uint8_t *data;            /* +0x00 */
	uint32_t pad;           /* +0x04 */
	uint64_t size;            /* +0x08 */
} Ff4BoxDataBuf; /* 16 bytes */

typedef struct Ff4BoxChunkNode {
	uint8_t *data;            /* +0x00 */
	uint32_t padA;           /* +0x04 */
	uint64_t size;            /* +0x08 */
	struct Ff4BoxChunkNode *next; /* +0x10 */
	uint32_t padB;           /* +0x14 */
} Ff4BoxChunkNode; /* 20/24 bytes */

typedef struct Ff4ProfEntry {
	uint32_t fourcc;          /* +0x00 (FPRF/VPRF/APRF/BPRF/PPRF) */
	uint32_t fields[10];      /* +0x04..+0x28 */
	struct Ff4ProfEntry *next; /* +0x2C (44) */
} Ff4ProfEntry; /* 48 bytes */

typedef struct Ff4ProfModel {
	uint32_t baseField;       /* +0x00 */
	Ff4ProfEntry *head;       /* +0x04 */
	Ff4ProfEntry *tail;       /* +0x08 */
} Ff4ProfModel; /* 12 bytes */

typedef struct Ff4FtypModel {
	uint32_t majorBrand;      /* +0x00 */
	uint32_t minorVersion;    /* +0x04 */
	Ff4NodeU32 *compatHead;   /* +0x08 */
	Ff4NodeU32 *compatTail;   /* +0x0C */
} Ff4FtypModel; /* 16 bytes */

typedef struct Ff4FtypPlan {
	uint64_t boxSize;         /* +0x00 */
	Ff4FtypModel *model;      /* +0x08 */
	uint32_t pad;           /* +0x0C */
} Ff4FtypPlan; /* 16 bytes */

typedef struct Ff4IodsModel {
	uint16_t objectDescrId;   /* +0x00 */
	uint8_t urlFlag;          /* +0x02 */
	uint8_t includeInline;    /* +0x03 */
	uint8_t odProfile;        /* +0x04 */
	uint8_t sceneProfile;     /* +0x05 */
	uint8_t audioProfile;     /* +0x06 */
	uint8_t visualProfile;    /* +0x07 */
	uint32_t esId;            /* +0x08 */
} Ff4IodsModel; /* 12 bytes */

typedef struct Ff4EsdsModel {
	uint32_t kind;            /* +0x00: 0=structured, 1=raw */
	uint32_t esId;            /* +0x04 */
	uint8_t *rawData;         /* +0x08 (kind==1) or streamPriority/objType */
	uint32_t streamType;      /* +0x0C */
	uint32_t bufferSizeDb;    /* +0x10 */
	uint32_t maxBitrate;      /* +0x14 */
	uint8_t *decSpecific;     /* +0x18 (kind==0) */
	uint32_t avgBitrate;      /* +0x1C */
	uint64_t decSpecificSize; /* +0x20 */
	uint8_t flags;            /* +0x28 */
	uint8_t pad[7];
} Ff4EsdsModel;

typedef struct Ff4AvccModel {
	uint8_t configVersion;    /* +0x00 */
	uint8_t profileInd;       /* +0x01 */
	uint8_t profileCompat;    /* +0x02 */
	uint8_t levelInd;         /* +0x03 */
	uint8_t lenSizeMinusOne;  /* +0x04 */
	uint8_t pad[3];
	Ff4NodeBuf *spsHead;      /* +0x08 */
	Ff4NodeBuf *spsTail;      /* +0x0C */
	Ff4NodeBuf *ppsHead;      /* +0x10 */
	Ff4NodeBuf *ppsTail;      /* +0x14 */
} Ff4AvccModel; /* 24 bytes */

typedef struct Ff4StsdEntry {
	uint32_t kind;            /* +0x00: 1=mp4v,2=avc1,3=mp4a,4=mp4s,5=s263/etc,6=raw,7=plain,8/9=other */
	uint16_t dataRefIndex;    /* +0x04 */
	uint16_t padA;           /* +0x06 */
	union {
		struct {
			uint16_t width;        /* +0x08 */
			uint16_t height;       /* +0x0A */
			uint32_t horizRes;     /* +0x0C */
			uint32_t vertRes;      /* +0x10 */
			uint16_t frameCount;   /* +0x14 */
			uint8_t compressorName[32]; /* +0x16 */
			uint16_t depth;        /* +0x36 */
			uint32_t padB;        /* +0x38 */
			void *codecCfg;        /* +0x3C (60): Ff4EsdsModel* or Ff4AvccModel* */
			void *paspOrBtrt;      /* +0x40 (64) */
			Ff4BoxChunkNode *extraHead; /* +0x44 (68) */
			Ff4BoxChunkNode *extraTail; /* +0x48 (72) */
		} video;
		struct {
			uint16_t channelCount; /* +0x08 */
			uint16_t sampleSize;   /* +0x0A */
			uint32_t sampleRate;   /* +0x0C */
			uint32_t padC[2];     /* +0x10 */
			Ff4EsdsModel *esds;    /* +0x18 (24) */
			Ff4BoxChunkNode *extraHead; /* +0x1C (28) */
			Ff4BoxChunkNode *extraTail; /* +0x20 (32) */
			uint32_t padD[10];
		} audio;
		struct {
			uint32_t padE;        /* +0x08 */
			void *payload;         /* +0x0C (12): Ff4EsdsModel* or Ff4BoxChunkNode* */
			void *payloadTail;     /* +0x10 (16) */
			uint32_t padF[4];     /* +0x14..+0x20 */
			void *subBox;          /* +0x24 (36) */
			uint32_t padG[9];
		} generic;
	} u;
	struct Ff4StsdEntry *next; /* +0x4C (76) */
} Ff4StsdEntry; /* 80 bytes */

typedef struct Ff4MtdtEntry {
	uint32_t fourcc;          /* +0x00 */
	uint8_t isUtf16;          /* +0x04 */
	uint8_t padA;            /* +0x05 */
	uint16_t langCode;        /* +0x06 */
	uint16_t dataType;        /* +0x08 */
	uint16_t padB;           /* +0x0A */
	uint32_t padC;           /* +0x0C */
	uint8_t *data;            /* +0x10 (16) */
	uint32_t padD;           /* +0x14 */
	uint64_t size;            /* +0x18 (24) */
	struct Ff4MtdtEntry *next; /* +0x20 (32) */
	uint32_t padE;           /* +0x24 */
} Ff4MtdtEntry; /* 40 bytes */

typedef struct Ff4MtdfEntry {
	uint32_t fourcc;          /* +0x00 */
	uint16_t langCode;        /* +0x04 */
	uint16_t dataType;        /* +0x06 */
	uint8_t *data;            /* +0x08 */
	uint32_t padA;           /* +0x0C */
	uint64_t size;            /* +0x10 (16) */
	struct Ff4MtdfEntry *next; /* +0x18 (24) */
	uint32_t padB;           /* +0x1C */
} Ff4MtdfEntry; /* 32 bytes */

typedef struct Ff4MtsmEntry {
	uint64_t creationTime;    /* +0x00 */
	uint64_t modTime;         /* +0x08 */
	uint32_t streamId;        /* +0x10 (16) */
	uint32_t padA;           /* +0x14 */
	Ff4DrefEntry *drefHead;   /* +0x18 (24) */
	Ff4DrefEntry *drefTail;   /* +0x1C (28) */
	Ff4StsdEntry *stsdHead;   /* +0x20 (32) */
	Ff4StsdEntry *stsdTail;   /* +0x24 (36) */
	Ff4NodeTriple *mdstHead;  /* +0x28 (40) */
	Ff4NodeTriple *mdstTail;  /* +0x2C (44) */
	struct Ff4MtsmEntry *next; /* +0x30 (48) */
	uint32_t padB;           /* +0x34 (52) */
} Ff4MtsmEntry; /* 56 bytes */

typedef struct Ff4EditListModel {
	Ff4EditEntryNode *head;     /* +0x00 */
	Ff4EditEntryNode *tail;     /* +0x04 */
} Ff4EditListModel; /* 8 bytes */

typedef struct Ff4MetaDataList {
	Ff4TrefEntry *trefHead;   /* +0x00 */
	Ff4TrefEntry *trefTail;   /* +0x04 */
	Ff4MetaItemNode *dataHead;  /* +0x08 */
	Ff4MetaItemNode *dataTail;  /* +0x0C */
} Ff4MetaDataList; /* 16 bytes */

typedef struct Ff4StblRunBlock {
	uint32_t count;           /* +0x00 */
	void *entries;            /* +0x04 */
	struct Ff4StblRunBlock *next; /* +0x08 */
} Ff4StblRunBlock; /* 12 bytes */

typedef struct Ff4SampleTableModel {
	uint64_t fragBaseOff;         /* +0x00 */
	uint64_t baseDecodeTime;      /* +0x08 */
	uint64_t minPresTime;         /* +0x10 */
	uint64_t totalDuration;       /* +0x18 */
	uint32_t tfhdFlags;           /* +0x20 (32) */
	uint32_t padA;               /* +0x24 */
	uint64_t baseDataOffset;      /* +0x28 (40) */
	uint32_t sampleDescIdx;       /* +0x30 (48) */
	uint32_t defSampleDur;        /* +0x34 (52) */
	uint32_t defSampleSize;       /* +0x38 (56) */
	uint32_t defSampleFlags;      /* +0x3C (60) */
	uint32_t sttsBlockCap;        /* +0x40 (64) */
	Ff4StblRunBlock *sttsHead;    /* +0x44 (68) */
	Ff4StblRunBlock *sttsTail;    /* +0x48 (72) */
	uint32_t *sttsLastEntry;      /* +0x4C (76) */
	uint32_t cttsVersion;         /* +0x50 (80) */
	uint32_t cttsBlockCap;        /* +0x54 (84) */
	Ff4StblRunBlock *cttsHead;    /* +0x58 (88) */
	Ff4StblRunBlock *cttsTail;    /* +0x5C (92) */
	uint32_t *cttsLastEntry;      /* +0x60 (96) */
	uint32_t stscFirstChunk;      /* +0x64 (100) */
	uint32_t stscChunkCount;      /* +0x68 (104: pending samples in cur chunk) */
	uint32_t stscCurDesc;         /* +0x6C (108) */
	uint32_t stscCommittedChunks; /* +0x70 (112) */
	uint32_t stscBlockCap;        /* +0x74 (116) */
	Ff4StblRunBlock *stscHead;    /* +0x78 (120) */
	Ff4StblRunBlock *stscTail;    /* +0x7C (124) */
	uint32_t *stscLastEntry;      /* +0x80 (128) */
	uint32_t fixedSampleSize;     /* +0x84 (132) */
	uint32_t totalSamples;        /* +0x88 (136) */
	uint32_t stszBlockCap;        /* +0x8C (140) */
	Ff4StblRunBlock *stszHead;    /* +0x90 (144) */
	Ff4StblRunBlock *stszTail;    /* +0x94 (148) */
	uint32_t stcoFirstSample;     /* +0x98 (152) */
	uint32_t stcoBlockCap;        /* +0x9C (156) */
	Ff4StblRunBlock *stcoHead;    /* +0xA0 (160) */
	Ff4StblRunBlock *stcoTail;    /* +0xA4 (164) */
	uint32_t hasSyncSample;       /* +0xA8 (168: sync sample count) */
	uint32_t stssBlockCap;        /* +0xAC (172) */
	Ff4StblRunBlock *stssHead;    /* +0xB0 (176) */
	Ff4StblRunBlock *stssTail;    /* +0xB4 (180) */
	struct Ff4SampleTableModel *next; /* +0xB8 (184) */
	uint32_t padB;               /* +0xBC (188) */
} Ff4SampleTableModel; /* 192 bytes */

typedef struct Ff4TrackModel {
	uint32_t tkhdMask;        /* +0x00 */
	uint32_t tkhdFlags;       /* +0x04 */
	uint64_t creationTime;    /* +0x08 */
	uint64_t modTime;         /* +0x10 */
	uint32_t trackId;         /* +0x18 (24) */
	uint32_t padA;           /* +0x1C */
	uint64_t duration;        /* +0x20 (32) */
	int16_t layer;            /* +0x28 (40) */
	int16_t altGroup;         /* +0x2A (42) */
	int16_t volume;           /* +0x2C (44) */
	uint16_t padB;           /* +0x2E */
	int32_t matrix[9];        /* +0x30..+0x50 */
	uint32_t width;           /* +0x54 (84) */
	uint32_t height;          /* +0x58 (88) */
	uint32_t padC;           /* +0x5C */
	Ff4NodeGroup *trefHead;   /* +0x60 (96) */
	Ff4NodeGroup *trefTail;   /* +0x64 (100) */
	Ff4EditListModel *edts;   /* +0x68 (104) */
	uint32_t padD;           /* +0x6C */
	/* Media header (mdhd, 48 bytes at +0x70..+0x9F) */
	uint32_t mdhdMask;        /* +0x70 (112) */
	uint32_t padE;           /* +0x74 */
	uint64_t mdhdCreation;    /* +0x78 (120) */
	uint64_t mdhdMod;         /* +0x80 (128) */
	uint32_t mdhdTimescale;   /* +0x88 (136) */
	uint32_t padF;           /* +0x8C */
	uint64_t mdhdDuration;    /* +0x90 (144) */
	uint16_t mdhdLanguage;    /* +0x98 (152) */
	uint16_t padG[3];        /* +0x9A..+0x9F */
	/* Handler (hdlr, 24 bytes at +0xA0..+0xB7) */
	uint32_t handlerType;     /* +0xA0 (160) */
	uint32_t padH;           /* +0xA4 */
	Ff4BoxDataBuf handlerName; /* +0xA8 (168) */
	/* Media info header (minf, 12 bytes at +0xB8..+0xC3) */
	uint32_t minfType;        /* +0xB8 (184) */
	uint8_t minfHdrData[8];   /* +0xBC (188) */
	Ff4DrefEntry *drefHead;   /* +0xC4 (196) */
	Ff4DrefEntry *drefTail;   /* +0xC8 (200) */
	Ff4StsdEntry *stsdHead;   /* +0xCC (204) */
	Ff4StsdEntry *stsdTail;   /* +0xD0 (208) */
	/* Trex defaults & fragment/sample tables (40 bytes at +0xD4..+0xFB) */
	uint32_t trexMask;        /* +0xD4 (212) */
	uint32_t trexDescIdx;     /* +0xD8 (216) */
	uint32_t trexSampleDur;   /* +0xDC (220) */
	uint32_t trexSampleSize;  /* +0xE0 (224) */
	uint32_t trexSampleFlags; /* +0xE4 (228) */
	uint32_t tfraBlockCap;    /* +0xE8 (232) */
	Ff4StblRunBlock *tfraHead; /* +0xEC (236) */
	Ff4StblRunBlock *tfraTail; /* +0xF0 (240) */
	Ff4SampleTableModel *fragHead; /* +0xF4 (244) */
	Ff4SampleTableModel *fragTail; /* +0xF8 (248) */
	/* Track UUID metadata (16 bytes at +0xFC..+0x10B) */
	Ff4MtdtEntry *mtdtHead;   /* +0xFC (252) */
	Ff4MtdtEntry *mtdtTail;   /* +0x100 (256) */
	Ff4MtdfEntry *mtdfHead;   /* +0x104 (260) */
	Ff4MtdfEntry *mtdfTail;   /* +0x108 (264) */
	uint32_t padI;          /* +0x10C (268) */
	uint32_t padJ;          /* +0x110 (272) */
	struct Ff4TrackModel *next; /* +0x114 (276) */
} Ff4TrackModel; /* 280 bytes */

typedef struct Ff4MovieModel {
	uint32_t mvhdMask;        /* +0x00 */
	uint32_t padA;           /* +0x04 */
	uint64_t creationTime;    /* +0x08 */
	uint64_t modTime;         /* +0x10 */
	uint32_t timescale;       /* +0x18 (24) */
	uint32_t padB;           /* +0x1C */
	uint64_t duration;        /* +0x20 (32) */
	uint32_t rate;            /* +0x28 (40) */
	uint16_t volume;          /* +0x2C (44) */
	uint16_t padC;           /* +0x2E */
	int32_t matrix[9];        /* +0x30..+0x50 */
	uint32_t nextTrackId;     /* +0x54 (84) */
	Ff4TrackModel *trackHead; /* +0x58 (88) */
	Ff4TrackModel *trackTail; /* +0x5C (92) */
	Ff4IodsModel *iods;       /* +0x60 (96) */
	Ff4MtdtEntry *mtdtHead;   /* +0x64 (100) */
	Ff4MtdtEntry *mtdtTail;   /* +0x68 (104) */
	Ff4MtdfEntry *mtdfHead;   /* +0x6C (108) */
	Ff4MtdfEntry *mtdfTail;   /* +0x70 (112) */
	Ff4MtsmEntry *mtsmHead;   /* +0x74 (116) */
	Ff4MtsmEntry *mtsmTail;   /* +0x78 (120) */
	uint32_t fragCount;       /* +0x7C (124) */
	uint32_t padD;           /* +0x80 (128) */
} Ff4MovieModel; /* 132 bytes */

typedef struct Ff4WriterModels {
	Ff4FtypModel *ftyp;       /* +0x00 */
	Ff4ProfModel *prof;       /* +0x04 */
	Ff4MovieModel *moov;      /* +0x08 */
	Ff4MetaDataList meta;     /* +0x0C (16 bytes) */
} Ff4WriterModels; /* 28 bytes */

typedef struct Ff4WriterImpl {
	Ff4SessionImpl *sess;     /* +0x00 */
	Ff4FormatOpts opts;       /* +0x04 (20 bytes) */
	Ff4WriterModels models;   /* +0x18 (28 bytes) */
	uint32_t pad;           /* +0x34 */
	Ff4Buffer scratchBuf;     /* +0x38 (16 bytes) */
} Ff4WriterImpl; /* 72 bytes */

/* ---- Box serialization plan structures ---- */

typedef struct Ff4MdhdModel {
	uint32_t mask;            /* +0x00 */
	uint32_t padA;           /* +0x04 */
	uint64_t creationTime;    /* +0x08 */
	uint64_t modTime;         /* +0x10 */
	uint32_t timescale;       /* +0x18 */
	uint32_t padB;           /* +0x1C */
	uint64_t duration;        /* +0x20 */
	uint16_t language;        /* +0x28 */
	uint16_t padC[3];        /* +0x2A */
} Ff4MdhdModel; /* 48 bytes */

typedef struct Ff4MdhdPlan {
	uint8_t version;          /* +0x00 */
	uint8_t pad[7];         /* +0x01 */
	Ff4MdhdModel model;       /* +0x08 (48 bytes) */
} Ff4MdhdPlan; /* 56 bytes */

typedef struct Ff4HdlrModel {
	uint32_t handlerType;     /* +0x00 */
	uint32_t pad;           /* +0x04 */
	Ff4BoxDataBuf name;       /* +0x08 (16 bytes) */
} Ff4HdlrModel; /* 24 bytes */

typedef struct Ff4HdlrPlan {
	uint64_t boxSize;         /* +0x00 */
	Ff4HdlrModel model;       /* +0x08 (24 bytes) */
} Ff4HdlrPlan; /* 32 bytes */

typedef struct Ff4StsdExtraPlan {
	uint64_t boxSize;         /* +0x00 */
	uint8_t *data;            /* +0x08 */
	uint32_t padA;           /* +0x0C */
	uint64_t size;            /* +0x10 */
	struct Ff4StsdExtraPlan *next; /* +0x18 (24) */
	uint32_t padB;           /* +0x1C */
} Ff4StsdExtraPlan; /* 32 bytes */

typedef struct Ff4EsdsPlan {
	uint64_t boxSize;         /* +0x00 */
	uint32_t kind;            /* +0x08 */
	uint32_t padA;           /* +0x0C */
	uint8_t *rawData;         /* +0x10 */
	uint32_t padB[8];        /* +0x14..+0x30 */
	uint8_t *decBufA;         /* +0x38 (56, a2[14]) */
	uint32_t padC[3];
	uint8_t *decBufB;         /* +0x48 (72, a2[18]) */
	uint32_t padD;
} Ff4EsdsPlan;

typedef struct Ff4StsdPlan {
	uint32_t kind;            /* +0x00 */
	uint32_t padA;           /* +0x04 */
	uint64_t boxSize;         /* +0x08 */
	uint32_t bodyWords[5];    /* +0x10..+0x20 */
	union {
		struct {
			uint32_t padB[10];
			Ff4EsdsPlan *codecPlan; /* +0x4C (76, v3[19]) */
			void *paspPlan;         /* +0x50 (80, v3[20]) */
			Ff4StsdExtraPlan *extraHead; /* +0x54 (84, v3[21]) */
			Ff4StsdExtraPlan *extraTail; /* +0x58 (88, v3[22]) */
			uint32_t padC;         /* +0x5C (92) */
		} video;
		struct {
			uint32_t padD;         /* +0x24 */
			Ff4EsdsPlan *esdsPlan;  /* +0x28 (40, v3[10]) */
			Ff4StsdExtraPlan *extraHead; /* +0x2C (44, v3[11]) */
			Ff4StsdExtraPlan *extraTail; /* +0x30 (48, v3[12]) */
			void *subBoxPlan;       /* +0x34 (52, v3[13]) */
			uint32_t padE[10];
		} audio;
		struct {
			void *esdsOrExtra;      /* +0x1C (28, v3[7]) -> aliased via bodyWords[3] */
		} small;
	} u;
	struct Ff4StsdPlan *next; /* +0x60 (96, v3[24]) */
} Ff4StsdPlan; /* 100 bytes */

typedef struct Ff4TrefTypePlan {
	uint64_t boxSize;         /* +0x00 */
	Ff4NodeGroup *group;      /* +0x08 */
	struct Ff4TrefTypePlan *next; /* +0x0C (12) */
} Ff4TrefTypePlan; /* 16 bytes */

typedef struct Ff4TrefPlan {
	uint64_t boxSize;         /* +0x00 */
	Ff4TrefTypePlan *head;    /* +0x08 */
	Ff4TrefTypePlan *tail;    /* +0x0C */
} Ff4TrefPlan; /* 16 bytes */

typedef struct Ff4MtsmPlan {
	uint64_t boxSize;         /* +0x00 */
	uint8_t mthdPlan[32];     /* +0x08..+0x27 */
	uint8_t dinfPlan[24];     /* +0x28..+0x3F */
	uint64_t stsdBoxSize;     /* +0x40 (64) */
	Ff4StsdPlan *stsdHead;    /* +0x48 (72) */
	Ff4StsdPlan *stsdTail;    /* +0x4C (76) */
	uint8_t mdstPlan[16];     /* +0x50..+0x5F */
	struct Ff4MtsmPlan *next; /* +0x60 (96) */
	uint32_t pad;           /* +0x64 */
} Ff4MtsmPlan; /* 104 bytes */

typedef struct Ff4UuidPlan {
	uint64_t boxSize;         /* +0x00 */
	void *mtdtPlan;           /* +0x08 */
	void *mtdfPlan;           /* +0x0C */
	Ff4MtsmPlan *mtsmHead;    /* +0x10 */
	Ff4MtsmPlan *mtsmTail;    /* +0x14 */
} Ff4UuidPlan; /* 24 bytes */

typedef struct Ff4RunEntryNode {
	uint32_t val0;            /* +0x00 */
	uint32_t val1;            /* +0x04 */
	uint32_t val2;            /* +0x08 */
	struct Ff4RunEntryNode *next; /* +0x0C (12) */
} Ff4RunEntryNode; /* 16 bytes */

typedef struct Ff4RunGroupNode {
	uint32_t f0;              /* +0x00 */
	uint32_t f1;              /* +0x04 */
	uint32_t f2;              /* +0x08 */
	Ff4RunEntryNode *listAHead;  /* +0x0C (v3[3]) */
	Ff4RunEntryNode *listATail;  /* +0x10 (v3[4]) */
	Ff4RunEntryNode *listBHead;  /* +0x14 (v3[5]) */
	Ff4RunEntryNode *listBTail;  /* +0x18 (v3[6]) */
	struct Ff4RunGroupNode *next; /* +0x1C (v3[7]) */
} Ff4RunGroupNode; /* 32 bytes */

typedef struct Ff4RunTable {
	uint32_t header[4];       /* +0x00..+0x0F */
	Ff4RunGroupNode *groupHead; /* +0x10 (16) */
	Ff4RunGroupNode *groupTail; /* +0x14 (20) */
	uint32_t pad[2];        /* +0x18..+0x1F */
	Ff4RunEntryNode *extraHead;  /* +0x20 (32) */
	Ff4RunEntryNode *extraTail;  /* +0x24 (36) */
} Ff4RunTable; /* 40 bytes */

typedef struct Ff4SttsPlan {
	uint64_t boxSize;         /* +0x00 */
	Ff4StblRunBlock *head;    /* +0x08 */
	uint32_t pad;           /* +0x0C */
} Ff4SttsPlan; /* 16 bytes */

typedef struct Ff4CttsPlan {
	uint64_t boxSize;         /* +0x00 */
	uint8_t version;          /* +0x08 */
	uint8_t pad[3];         /* +0x09 */
	Ff4StblRunBlock *head;    /* +0x0C */
} Ff4CttsPlan; /* 16 bytes */

typedef struct Ff4StszPlan {
	uint64_t boxSize;         /* +0x00 */
	uint32_t fixedSize;       /* +0x08 */
	uint32_t sampleCount;     /* +0x0C */
	Ff4StblRunBlock *head;    /* +0x10 */
	uint32_t pad;           /* +0x14 */
} Ff4StszPlan; /* 24 bytes */

typedef struct Ff4StcoPlan {
	uint64_t boxSize;         /* +0x00 */
	uint32_t fourcc;          /* +0x08 (stco or co64) */
	Ff4StblRunBlock *head;    /* +0x0C */
} Ff4StcoPlan; /* 16 bytes */

typedef struct Ff4StblPlan {
	uint64_t boxSize;         /* +0x00 */
	uint64_t stsdBoxSize;     /* +0x08 */
	Ff4StsdPlan *stsdHead;    /* +0x10 */
	Ff4StsdPlan *stsdTail;    /* +0x14 */
	Ff4SttsPlan stts;         /* +0x18 (16 bytes) */
	Ff4CttsPlan *cttsPlan;    /* +0x28 (40) */
	uint32_t padA;           /* +0x2C */
	Ff4SttsPlan stsc;         /* +0x30 (16 bytes) */
	Ff4StszPlan stsz;         /* +0x40 (24 bytes) */
	Ff4StcoPlan stco;         /* +0x58 (16 bytes) */
	Ff4SttsPlan *stssPlan;    /* +0x68 (104) */
	uint32_t padB;           /* +0x6C */
} Ff4StblPlan; /* 112 bytes */

typedef struct Ff4MinfHdrPlan {
	uint64_t boxSize;         /* +0x00 */
	uint32_t fourcc;          /* +0x08 */
	uint8_t hdrData[8];       /* +0x0C */
	uint32_t pad;           /* +0x14 */
} Ff4MinfHdrPlan; /* 24 bytes */

typedef struct Ff4DrefPlan {
	uint64_t boxSize;         /* +0x00 */
	Ff4DrefEntry *head;       /* +0x08 */
	Ff4DrefEntry *tail;       /* +0x0C */
} Ff4DrefPlan; /* 16 bytes */

typedef struct Ff4DinfPlan {
	uint64_t boxSize;         /* +0x00 */
	Ff4DrefPlan dref;         /* +0x08 (16 bytes) */
} Ff4DinfPlan; /* 24 bytes */

typedef struct Ff4MinfPlan {
	uint64_t boxSize;         /* +0x00 */
	Ff4MinfHdrPlan hdr;       /* +0x08 (24 bytes: a4+2) */
	Ff4DinfPlan dinf;         /* +0x20 (24 bytes: a4+8) */
	Ff4StblPlan stbl;         /* +0x38 (112 bytes: a4+14) */
	uint32_t pad;           /* +0xA8 */
} Ff4MinfPlan; /* 172 bytes */

typedef struct Ff4SttsCursor {
	uint32_t sampleInRun;     /* +0x00 */
	uint32_t entryInBlk;      /* +0x04 */
	Ff4StblRunBlock *blk;     /* +0x08 */
	uint32_t *curEntry;       /* +0x0C */
} Ff4SttsCursor; /* 16 bytes */

typedef struct Ff4StszCursor {
	uint32_t fixedSize;       /* +0x00 */
	uint32_t entryInBlk;      /* +0x04 */
	Ff4StblRunBlock *blk;     /* +0x08 */
	uint32_t *curEntry;       /* +0x0C */
} Ff4StszCursor; /* 16 bytes */

typedef struct Ff4StssCursor {
	uint32_t hasSync;         /* +0x00 */
	uint32_t totalSync;       /* +0x04 */
	uint32_t entryInBlk;      /* +0x08 */
	Ff4StblRunBlock *blk;     /* +0x0C */
	uint32_t *curEntry;       /* +0x10 */
} Ff4StssCursor; /* 20 bytes */

typedef struct Ff4StcoCursor {
	uint32_t entryInBlk;      /* +0x00 */
	Ff4StblRunBlock *blk;     /* +0x04 */
	uint64_t *curEntry;       /* +0x08 */
} Ff4StcoCursor; /* 12 bytes */

typedef struct Ff4StscCursor {
	uint32_t totalChunks;     /* +0x00 */
	uint32_t curChunk;        /* +0x04 */
	uint32_t nextFirstChunk;  /* +0x08 */
	uint32_t entryInBlk;      /* +0x0C */
	Ff4StblRunBlock *blk;     /* +0x10 */
	uint32_t *curEntry;       /* +0x14 */
} Ff4StscCursor; /* 24 bytes */

typedef struct Ff4FragCursor {
	uint32_t sampleIdx;       /* +0x00 (a5[0]) */
	uint32_t chunkCount;      /* +0x04 (a5[1]) */
	Ff4SttsCursor stts;       /* +0x08 (a5+2) */
	Ff4SttsCursor ctts;       /* +0x18 (a5+6) */
	Ff4StszCursor stsz;       /* +0x28 (a5+10) */
	Ff4StssCursor stss;       /* +0x38 (a5+14) */
	Ff4StcoCursor stco;       /* +0x4C (a5+19) */
	Ff4StscCursor stsc;       /* +0x58 (a5+22) */
} Ff4FragCursor; /* 112 bytes */

typedef struct Ff4TrunData {
	uint32_t flags;           /* +0x00 */
	uint32_t sampleCount;     /* +0x04 */
	int32_t dataOffset;       /* +0x08 */
	uint32_t firstSampleFlags; /* +0x0C */
	uint8_t *tablePtr;        /* +0x10 */
	uint8_t *tempAlloc;       /* +0x14 */
} Ff4TrunData; /* 24 bytes */

typedef struct Ff4TrunPlan {
	uint64_t boxSize;         /* +0x00 */
	struct Ff4TrunPlan *next; /* +0x08 */
	uint32_t pad;           /* +0x0C */
} Ff4TrunPlan; /* 16 bytes */

typedef struct Ff4TfhdPlan {
	uint64_t boxSize;         /* +0x00 */
	uint32_t flags;           /* +0x08 */
	uint32_t trackId;         /* +0x0C */
	uint64_t baseDataOffset;  /* +0x10 */
	uint32_t sampleDescIdx;   /* +0x18 */
	uint32_t defSampleDur;    /* +0x1C */
	uint32_t defSampleSize;   /* +0x20 */
	uint32_t defSampleFlags;  /* +0x24 */
} Ff4TfhdPlan; /* 40 bytes */

typedef struct Ff4TrafPlan {
	uint64_t boxSize;         /* +0x00 */
	Ff4TfhdPlan tfhd;         /* +0x08 (40 bytes) */
	Ff4TrunPlan *trunHead;    /* +0x30 (48) */
	Ff4TrunPlan *trunTail;    /* +0x34 (52) */
	Ff4SampleTableModel *frag; /* +0x38 (56) */
	struct Ff4TrafPlan *next; /* +0x3C (60) */
} Ff4TrafPlan; /* 64 bytes */

typedef struct Ff4MoofPlan {
	uint64_t boxSize;         /* +0x00 */
	uint32_t seqNumber;       /* +0x08 */
	Ff4TrafPlan *trafHead;    /* +0x0C */
	Ff4TrafPlan *trafTail;    /* +0x10 */
	uint32_t pad;           /* +0x14 */
} Ff4MoofPlan; /* 24 bytes */

typedef struct Ff4MdiaPlan {
	uint64_t boxSize;         /* +0x00 */
	Ff4MdhdPlan mdhd;         /* +0x08 (56 bytes) */
	Ff4HdlrPlan hdlr;         /* +0x40 (32 bytes) */
	Ff4MinfPlan minf;         /* +0x60 (172 bytes) */
} Ff4MdiaPlan; /* 268 bytes */

typedef struct Ff4TrakPlan {
	uint64_t boxSize;         /* +0x00 */
	uint8_t tkhdVersion;      /* +0x08 */
	uint8_t tkhdPad[7];       /* +0x09 */
	uint8_t tkhdModel[96];    /* +0x10..+0x6F */
	Ff4TrefPlan *trefPlan;    /* +0x70 (112, v3[28]) */
	void *edtsPlan;           /* +0x74 (116, v3[29]) */
	Ff4MdiaPlan mdia;         /* +0x78 (120..387) */
	Ff4UuidPlan *uuidPlan;    /* +0x180 (384, v3[96]) */
	struct Ff4TrakPlan *next; /* +0x184 (388, v3[97]) */
} Ff4TrakPlan; /* 392 bytes */

typedef struct Ff4MoovPlan {
	uint64_t boxSize;         /* +0x00 */
	uint8_t mvhdVersion;      /* +0x08 */
	uint8_t mvhdPad[7];       /* +0x09 */
	uint8_t mvhdModel[88];    /* +0x10..+0x67 */
	void *iodsPlan;           /* +0x68 (104, a2[26]) */
	Ff4TrakPlan *trakHead;    /* +0x6C (108, a2[27]) */
	Ff4TrakPlan *trakTail;    /* +0x70 (112, a2[28]) */
	void *mvexPlan;           /* +0x74 (116, a2[29]) */
	Ff4UuidPlan *uuidPlan;    /* +0x78 (120, a2[30]) */
	uint32_t pad;           /* +0x7C */
} Ff4MoovPlan; /* 128 bytes */

typedef struct Ff4TfraEntry {
	uint64_t time;            /* +0x00 */
	uint32_t fragIndex;       /* +0x08 */
	uint32_t trackIndex;      /* +0x0C */
	uint32_t trunNum;         /* +0x10 */
	uint32_t sampleNum;       /* +0x14 */
} Ff4TfraEntry; /* 24 bytes */

typedef struct Ff4TfraPlan {
	uint64_t boxSize;         /* +0x00 */
	Ff4MovieModel *moov;      /* +0x08 */
	Ff4TrackModel *track;     /* +0x0C */
	uint8_t version;          /* +0x10 */
	uint8_t padA[3];         /* +0x11 */
	uint32_t flags;           /* +0x14 */
	uint32_t trackId;         /* +0x18 */
	uint32_t lengthSizeField; /* +0x1C */
	uint32_t entryCount;      /* +0x20 */
	uint32_t entryStride;     /* +0x24 */
	struct Ff4TfraPlan *next; /* +0x28 (40) */
	uint32_t padB;           /* +0x2C */
} Ff4TfraPlan; /* 48 bytes */

typedef struct Ff4MfraPlan {
	uint64_t boxSize;         /* +0x00 */
	Ff4TfraPlan *tfraHead;    /* +0x08 */
	Ff4TfraPlan *tfraTail;    /* +0x0C */
	uint32_t mfroSize;        /* +0x10 */
	uint32_t pad;           /* +0x14 */
} Ff4MfraPlan; /* 24 bytes */

/* ---- Forward declarations of internal FF4 functions across TUs ---- */

/* ff4_mdia.c */
int32_t ff4_set_mdhd_params(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4MdhdModel *mdhd,
			    uint32_t mask, const uint32_t *params);
int32_t ff4_set_mdhd_timestamps(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4MdhdModel *mdhd,
				uint8_t flags, uint64_t creationTime, uint64_t modTime);
int32_t ff4_set_hdlr_params(Ff4SessionImpl *sess, Ff4HdlrModel *hdlr, uint8_t mask,
			    const void *const *params);
int32_t ff4_write_hdlr_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4HdlrPlan *plan, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_hdlr_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4HdlrPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_mdhd_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4MdhdPlan *plan, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_mdhd_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4MdhdPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_mdia_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const Ff4MdiaPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_plan_hdlr(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, const Ff4HdlrModel *model,
		      Ff4HdlrPlan *outPlan);
int32_t ff4_calc_media_duration(Ff4SessionImpl *sess, const Ff4TrackModel *track, uint64_t *outDur);
int32_t ff4_plan_mdhd(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, const Ff4TrackModel *track,
		      Ff4MdhdPlan *outPlan);
int32_t ff4_plan_mdia(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4TrackModel *track,
		      Ff4MdiaPlan *outPlan);
int32_t ff4_write_mdia_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4MdiaPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);

/* ff4_mfra.c */
int32_t ff4_calc_traf_number(const Ff4MovieModel *moov, uint32_t fragIdx, uint32_t trackIdx,
			     uint32_t *outTrafNum);
int32_t ff4_serialize_tfra_entries(Ff4SessionImpl *sess, const Ff4TfraPlan *plan, uint8_t *dst);
int32_t ff4_write_tfra_payload(Ff4SessionImpl *sess, Ff4Buffer *scratch, void *fileCtx,
			       uint64_t pos, const Ff4TfraPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_plan_tfra(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
		      Ff4MovieModel *moov, Ff4TrackModel *track, Ff4TfraPlan *outPlan);
int32_t ff4_write_tfra_box(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
			   void *fileCtx, uint64_t pos, const Ff4TfraPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_plan_mfro(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, const Ff4MfraPlan *mfra,
		      uint32_t *outSize);
int32_t ff4_write_mfro_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const uint32_t *mfroSize, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_mfro_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const uint32_t *mfroSize,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_mfra_payload(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
			       void *fileCtx, uint64_t pos, const Ff4MfraPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_mfra_box(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
			   void *fileCtx, uint64_t pos, const Ff4MfraPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_free_mfra_plan(Ff4SessionImpl *sess, Ff4MfraPlan *plan);
int32_t ff4_plan_mfra(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
		      Ff4MovieModel *moov, Ff4MfraPlan *outPlan);

/* Cross-TU hooks used by ff4_box/ff4_mdia/ff4_mfra */
int32_t ff4_free_mvex_plan(Ff4SessionImpl *sess, void *plan);
int32_t ff4_free_track_sample_tables(Ff4SessionImpl *sess, void *trexAndFrags);
int32_t ff4_set_mvhd_timestamps(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4MovieModel *moov,
				uint8_t flags, uint64_t creationTime, uint64_t modTime);
int32_t ff4_set_tkhd_timestamps(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4TrackModel *track,
				uint8_t flags, uint64_t creationTime, uint64_t modTime);
int32_t ff4_set_mtsm_timestamps(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4MtsmEntry *mtsm,
				uint8_t flags, uint64_t creationTime, uint64_t modTime);
int32_t ff4_plan_minf(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4TrackModel *track,
		      Ff4MinfPlan *outPlan);
int32_t ff4_write_minf_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4MinfPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_sum_stts_duration(const Ff4TrackModel *track, uint64_t *outDur);
int32_t ff4_count_run_block_entries(const Ff4StblRunBlock *head, uint32_t *outCount);
int32_t ff4_get_fragment_at(Ff4SampleTableModel *head, uint32_t index1, Ff4SampleTableModel **outFrag);

/* ff4_minf.c */
int32_t ff4_set_vmhd_params(Ff4SessionImpl *sess, uint8_t hdrData[8], uint8_t mask,
			    const uint16_t *params);
int32_t ff4_set_smhd_params(Ff4SessionImpl *sess, uint8_t hdrData[2], uint8_t mask,
			    const uint8_t *params);
int32_t ff4_copy_url_entry(Ff4SessionImpl *sess, const void *const *params, Ff4BoxDataBuf *outLoc);
int32_t ff4_copy_urn_entry(Ff4SessionImpl *sess, const void *const *params,
			   Ff4BoxDataBuf *outNameAndLoc);
int32_t ff4_add_dref_entry(Ff4SessionImpl *sess, Ff4DrefEntry **listHeadTail,
			   const uint32_t *entryIn, uint32_t *outIndex);
int32_t ff4_write_url_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			      const Ff4BoxDataBuf *loc, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_urn_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			      const Ff4BoxDataBuf *nameAndLoc, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_dref_entry_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
				 uint64_t pos, const Ff4DrefEntry *entry,
				 uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_dref_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const Ff4DrefPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_dref_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4DrefPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_dinf_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const Ff4DinfPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_dinf_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4DinfPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_nmhd_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_nmhd_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4MinfHdrPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_smhd_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const uint16_t *balance, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_smhd_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4MinfHdrPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_vmhd_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const uint16_t *vmhdWords, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_vmhd_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4MinfHdrPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_minf_hdr_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const Ff4MinfHdrPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_minf_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const Ff4MinfPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_plan_dref(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4DrefEntry *const *listHeadTail,
		      Ff4DrefPlan *outPlan);
int32_t ff4_plan_dinf(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4DrefEntry *const *listHeadTail,
		      Ff4DinfPlan *outPlan);
int32_t ff4_plan_minf_hdr(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, const void *minfHdrModel,
			  Ff4MinfHdrPlan *outPlan);

/* ff4_moof.c */
int32_t ff4_stsc_cursor_peek_next(const Ff4StscCursor *cur, uint32_t *outFirstChunk);
int32_t ff4_frag_cursor_init(Ff4SessionImpl *sess, const Ff4SampleTableModel *frag, Ff4FragCursor *outCur);
int32_t ff4_stts_cursor_next(int32_t advance, Ff4SttsCursor *cur, uint32_t *outVal);
int32_t ff4_ctts_cursor_next(int32_t advance, Ff4SttsCursor *cur, uint32_t *outVal);
int32_t ff4_stco_cursor_next(int32_t advance, Ff4StcoCursor *cur, uint64_t *outOff);
int32_t ff4_stsc_cursor_next(int32_t advance, Ff4StscCursor *cur, uint32_t *outSamples, uint32_t *outDesc);
int32_t ff4_stsz_cursor_next(int32_t advance, Ff4StszCursor *cur, uint32_t *outSize);
int32_t ff4_stss_cursor_read(int32_t advance, Ff4StssCursor *cur, uint32_t *outSample);
int32_t ff4_stss_cursor_flags(uint32_t sampleNum1, Ff4StssCursor *cur, uint32_t *outFlags);
int32_t ff4_build_trun_data(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
			    const Ff4TfhdPlan *tfhd, Ff4FragCursor *cursor, Ff4TrunData *outData);
int32_t ff4_write_trun_payload(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
			       void *fileCtx, uint64_t pos, const Ff4TfhdPlan *tfhd,
			       Ff4FragCursor *cursor, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_plan_trun(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
		      const Ff4TfhdPlan *tfhd, Ff4FragCursor *cursor, Ff4TrunPlan *outPlan);
int32_t ff4_free_traf_plan_list(Ff4SessionImpl *sess, Ff4TrafPlan *head);
int32_t ff4_free_moof_plan(Ff4SessionImpl *sess, Ff4MoofPlan *plan);
int32_t ff4_get_fragment_count(const Ff4MovieModel *moov, uint32_t *outCount);
int32_t ff4_write_trun_box(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
			   void *fileCtx, uint64_t pos, const Ff4TfhdPlan *tfhd,
			   const Ff4TrunPlan *plan, Ff4FragCursor *cursor,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_tfhd_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4TfhdPlan *plan, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_tfhd_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4TfhdPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_traf_payload(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
			       void *fileCtx, uint64_t pos, const Ff4TrafPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_traf_box(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
			   void *fileCtx, uint64_t pos, const Ff4TrafPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_mfhd_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const uint32_t *seqNum, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_mfhd_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const uint32_t *seqNum,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_moof_payload(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
			       void *fileCtx, uint64_t pos, const Ff4MoofPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_plan_tfhd(Ff4SessionImpl *sess, uint32_t fragIdx, const Ff4FormatOpts *opts,
		      Ff4TrackModel *track, Ff4TfhdPlan *outPlan);
int32_t ff4_plan_traf(Ff4SessionImpl *sess, Ff4Buffer *scratch, uint32_t fragIdx,
		      const Ff4FormatOpts *opts, Ff4TrackModel *track,
		      Ff4SampleTableModel *frag, Ff4TrafPlan *outPlan);
int32_t ff4_set_mfhd_seq(Ff4SessionImpl *sess, uint32_t seqNum, uint32_t *outSeq);
int32_t ff4_plan_moof(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
		      uint32_t fragIdx, Ff4MovieModel *moov, Ff4MoofPlan *outPlan);
int32_t ff4_write_moof_box(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
			   void *fileCtx, uint64_t pos, const Ff4MoofPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_begin_movie_fragment(Ff4SessionImpl *sess, Ff4WriterModels *models);
int32_t ff4_query_fragment_state(Ff4SessionImpl *sess, Ff4WriterModels *models, uint32_t fragIdx,
				 int32_t *outAllHaveChunks, int32_t *outNoPendingStsc,
				 int32_t *outNotLastFrag, int32_t *outHasAnySamples);
int32_t ff4_clear_fragment_samples(Ff4SessionImpl *sess, Ff4WriterModels *models, uint32_t fragIdx);
int32_t ff4_write_fragment_cache(Ff4SessionImpl *sess, Ff4WriterModels *models, uint32_t fragIdx,
				 void *fileCtx, uint64_t pos, uint64_t *outWritten);
int32_t ff4_read_fragment_cache(Ff4SessionImpl *sess, Ff4Buffer *scratch, Ff4WriterModels *models,
				uint32_t fragIdx, void *fileCtx, uint64_t pos, uint64_t availSize);
int32_t ff4_set_fragment_base_offset(Ff4WriterModels *models, uint32_t fragIdx, uint64_t baseOff);
int32_t ff4_set_tfhd_override(Ff4WriterModels *models, uint32_t fragIdx, uint32_t trackIdx,
			      uint8_t mask, const void *params);

/* Additional stbl hooks used by ff4_minf / ff4_moof */
int32_t ff4_plan_stbl(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4TrackModel *track,
		      Ff4StblPlan *outPlan);
int32_t ff4_write_stbl_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4StblPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_count_stss_entries(const Ff4StblRunBlock *head, uint32_t *outCount);
int32_t ff4_count_stco_entries(const Ff4StblRunBlock *head, uint32_t *outCount);
int32_t ff4_clear_fragment_node(Ff4SessionImpl *sess, Ff4SampleTableModel *frag);
int32_t ff4_append_fragment_node(Ff4SessionImpl *sess, void *trexAndFrags);
int32_t ff4_write_cache_header(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos, uint64_t totalSize,
			       uint32_t fragIdx, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_cache_fragment(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				 const Ff4SampleTableModel *frag,
				 uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_read_cache_header(Ff4SessionImpl *sess, Ff4BufStream *stream, uint64_t pos,
			      uint64_t availSize, uint32_t fragIdx,
			      uint64_t *outRead, uint64_t *accumRead);
int32_t ff4_read_cache_fragment(Ff4SessionImpl *sess, Ff4BufStream *stream, uint64_t pos,
				uint64_t availSize, Ff4SampleTableModel *frag,
				uint64_t *outRead, uint64_t *accumRead);

/* ff4_moov.c */
typedef struct Ff4IodsPlan {
	uint64_t boxSize;         /* +0x00 */
	uint64_t odSize;          /* +0x08 */
	Ff4IodsModel model;       /* +0x10 (12 bytes) */
	uint32_t padA;           /* +0x1C */
	uint64_t esIdSize;        /* +0x20 */
	uint32_t esId;            /* +0x28 */
	uint32_t padB;           /* +0x2C */
} Ff4IodsPlan; /* 48 bytes */

typedef struct Ff4MehdPlan {
	uint8_t version;          /* +0x00 */
	uint8_t pad[7];         /* +0x01 */
	uint64_t fragDuration;    /* +0x08 */
} Ff4MehdPlan; /* 16 bytes */

typedef struct Ff4TrexPlan {
	uint32_t trackId;         /* +0x00 */
	uint32_t descIdx;         /* +0x04 */
	uint32_t sampleDur;       /* +0x08 */
	uint32_t sampleSize;      /* +0x0C */
	uint32_t sampleFlags;     /* +0x10 */
	struct Ff4TrexPlan *next; /* +0x14 */
} Ff4TrexPlan; /* 24 bytes */

typedef struct Ff4MvexPlan {
	uint64_t boxSize;         /* +0x00 */
	Ff4MehdPlan *mehd;        /* +0x08 */
	Ff4TrexPlan *trexHead;    /* +0x0C */
	Ff4TrexPlan *trexTail;    /* +0x10 */
	uint32_t pad;           /* +0x14 */
} Ff4MvexPlan; /* 24 bytes */

typedef struct Ff4MetaItemPlan {
	uint64_t boxSize;         /* +0x00 */
	Ff4MetaItemNode *item;      /* +0x08 */
	struct Ff4MetaItemPlan *next; /* +0x0C */
} Ff4MetaItemPlan; /* 16 bytes */

typedef struct Ff4MetaBoxPlan {
	uint64_t boxSize;         /* +0x00 */
	Ff4MetaItemPlan *head;    /* +0x08 */
	Ff4MetaItemPlan *tail;    /* +0x0C */
} Ff4MetaBoxPlan; /* 16 bytes */

int32_t ff4_init_iods(Ff4SessionImpl *sess, Ff4MovieModel *moov, const uint8_t *profileBytes,
		      uint32_t esId);
int32_t ff4_set_mvhd_params(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4MovieModel *moov,
			    uint32_t mask, const uint32_t *params);
int32_t ff4_set_iods_params(Ff4SessionImpl *sess, Ff4IodsModel *iods, uint8_t mask,
			    const uint16_t *params);
int32_t ff4_write_es_id_inc_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				    const uint32_t *esIdPtr, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_es_id_inc_descr(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				  const uint64_t *sizeAndEsId, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_iod_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			      const Ff4IodsPlan *plan, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_iod_descr(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			    const Ff4IodsPlan *plan, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_iods_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4IodsPlan *plan, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_iods_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4IodsPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_mvhd_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4MoovPlan *moovMvhd, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_mvhd_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4MoovPlan *moovMvhd,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_moov_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const Ff4MoovPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_plan_es_id_inc(Ff4SessionImpl *sess, const Ff4IodsModel *model, void *outEsIdPlan);
int32_t ff4_plan_iod_descr(Ff4SessionImpl *sess, const Ff4IodsModel *model, void *outOdPlan);
int32_t ff4_plan_iods(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, const Ff4IodsModel *model,
		      Ff4IodsPlan *outPlan);
int32_t ff4_calc_next_track_id(Ff4SessionImpl *sess, const Ff4TrackModel *head, uint32_t *outNextId);
int32_t ff4_calc_movie_duration(Ff4SessionImpl *sess, uint32_t movieTimescale,
				const Ff4TrackModel *head, uint64_t *outDur);
int32_t ff4_plan_mvhd(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, const Ff4MovieModel *moov,
		      void *outMvhdPlan);
int32_t ff4_plan_moov(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4MovieModel *moov,
		      Ff4MoovPlan *outPlan);
int32_t ff4_write_moov_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4MoovPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_plan_mehd(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, const Ff4MovieModel *moov,
		      Ff4MehdPlan *outPlan);
int32_t ff4_plan_trex(Ff4SessionImpl *sess, const Ff4TrackModel *track, Ff4TrexPlan *outPlan);
int32_t ff4_write_trex_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4TrexPlan *plan, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_trex_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4TrexPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_mehd_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4MehdPlan *plan, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_mehd_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4MehdPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_mvex_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const Ff4MvexPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_plan_mvex(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, const Ff4MovieModel *moov,
		      Ff4MvexPlan *outPlan);
int32_t ff4_write_mvex_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4MvexPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_set_trex_defaults(Ff4WriterModels *models, uint32_t trackIdx, uint8_t mask,
			      const uint32_t *params);
int32_t ff4_finalize_trex_defaults(Ff4WriterModels *models);
int32_t ff4_is_valid_tref_id_hi(uint32_t id, int32_t *outValid);
int32_t ff4_is_valid_tref_id_lo(uint32_t id, int32_t *outValid);
int32_t ff4_add_tref_tag(Ff4SessionImpl *sess, Ff4TrefEntry *entry, const void *tagParam);
int32_t ff4_set_tref_value(Ff4SessionImpl *sess, Ff4TrefEntry *entry, const void *valParam);
int32_t ff4_add_meta_tref(Ff4SessionImpl *sess, Ff4TrefEntry **headTail, const uint16_t *param);
int32_t ff4_add_meta_item(Ff4SessionImpl *sess, Ff4MetaDataList *meta, const void *param);
int32_t ff4_write_meta_item_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				    const Ff4MetaItemPlan *plan, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_meta_item_descr(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				  const Ff4MetaItemPlan *plan, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_meta_list_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				    const Ff4MetaBoxPlan *plan, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_meta_list_descr(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				  const Ff4MetaBoxPlan *plan, uint64_t *outWritten, uint64_t *accumWritten);

/* ff4_trak.c */
int32_t ff4_create_pdat_track(Ff4SessionImpl *sess, const uint32_t *params, Ff4TrackModel **outTrack);
int32_t ff4_create_book_track(Ff4SessionImpl *sess, const uint32_t *params, Ff4TrackModel **outTrack);
int32_t ff4_create_odsm_track(Ff4SessionImpl *sess, const uint32_t *params, uint32_t movieTimescale,
			      Ff4TrackModel **outTrack);
int32_t ff4_create_grph_track(Ff4SessionImpl *sess, const void *params, Ff4TrackModel **outTrack);
int32_t ff4_create_soun_track(Ff4SessionImpl *sess, const void *params, Ff4TrackModel **outTrack);
int32_t ff4_create_vide_track(Ff4SessionImpl *sess, const void *params, Ff4TrackModel **outTrack);
int32_t ff4_add_track(Ff4SessionImpl *sess, Ff4MovieModel *moov, const uint32_t *params,
		      uint32_t *outTrackIndex);
int32_t ff4_set_tkhd_params(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4TrackModel *track,
			    uint32_t mask, const void *params);
int32_t ff4_is_valid_track_id(uint32_t trackId, int32_t *outValid);
int32_t ff4_alloc_next_track_id(Ff4SessionImpl *sess, const Ff4TrackModel *head, uint32_t *outId);
int32_t ff4_is_track_id_available(uint32_t trackId, const Ff4TrackModel *head, int32_t *outAvail);
int32_t ff4_classify_id_box_type(uint32_t id, uint32_t *outFourcc);
int32_t ff4_remap_track_id_refs(Ff4SessionImpl *sess, Ff4MovieModel *moov,
				uint32_t oldId, uint32_t newId);
int32_t ff4_add_tref_target_id(Ff4SessionImpl *sess, Ff4NodeGroup *group, const uint32_t *param,
			       uint32_t *outIndex);
int32_t ff4_add_track_reference(Ff4SessionImpl *sess, Ff4TrackModel *track, const uint32_t *param,
				uint32_t *outIndex);
int32_t ff4_append_edit_entry(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
			      Ff4EditListModel *edts, const uint32_t *entryIn);
int32_t ff4_add_track_edit(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
			   Ff4TrackModel *track, const uint32_t *entryIn);
int32_t ff4_write_elst_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const void *plan, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_elst_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const void *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_edts_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const void *plan,
			       uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_edts_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const void *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_tref_type_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				    const Ff4TrefTypePlan *plan,
				    uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_tref_type_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
				uint64_t pos, const Ff4TrefTypePlan *plan,
				uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_tref_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const Ff4TrefPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_tref_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4TrefPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_tkhd_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const void *plan, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_tkhd_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const void *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_trak_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const Ff4TrakPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_sum_edit_duration(Ff4SessionImpl *sess, const Ff4EditListModel *edts, uint64_t *outDur);
int32_t ff4_calc_track_duration(Ff4SessionImpl *sess, uint32_t movieTimescale,
				const Ff4TrackModel *track, uint64_t *outDur);
int32_t ff4_plan_tkhd(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, uint32_t movieTimescale,
		      const Ff4TrackModel *track, void *outTkhdPlan);
int32_t ff4_choose_elst_version(const Ff4FormatOpts *opts, const Ff4EditListModel *edts,
				uint8_t *outVersion);
int32_t ff4_plan_elst(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
		      Ff4EditListModel *edts, void *outElstPlan);
int32_t ff4_plan_edts(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
		      Ff4EditListModel *edts, void *outEdtsPlan);
int32_t ff4_plan_tref_type(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
			   Ff4NodeGroup *group, Ff4TrefTypePlan *outPlan);
int32_t ff4_plan_tref(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
		      Ff4NodeGroup *groupHead, Ff4TrefPlan *outPlan);
int32_t ff4_plan_trak(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, uint32_t movieTimescale,
		      Ff4TrackModel *track, Ff4TrakPlan *outPlan);
int32_t ff4_write_trak_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4TrakPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_plan_raw_uuid(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, const uint32_t uuid[4],
			  uint64_t extraSize, void *outPlan);

/* ff4_uuid.c hooks */
int32_t ff4_plan_usmt(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *usmtModel,
		      Ff4UuidPlan *outPlan);
int32_t ff4_write_usmt_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4UuidPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_add_mtdt_zpck_header(Ff4SessionImpl *sess, Ff4MtdtEntry **headTail,
				 uint8_t isUtf16, const uint32_t *hdrVals);
int32_t ff4_write_zpck_table_to_buf(Ff4SessionImpl *sess, Ff4BoxDataBuf *dstBuf,
				    const Ff4ZpckTable *table);
int32_t ff4_is_valid_mtsm_id(uint32_t id, int32_t *outValid);
int32_t ff4_track_type_to_tkhd_flags(uint32_t kind, uint32_t *outFlags);
int32_t ff4_plan_stsd_list(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
			   uint32_t trackFlag, Ff4StsdEntry *const *stsdHeadTail, void *outStsdPlan);
int32_t ff4_write_stsd_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const void *stsdPlan,
			   uint64_t *outWritten, uint64_t *accumWritten);

/* ff4_box.c */
int32_t ff4_box_seek(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos, int32_t whence);
int32_t ff4_box_write(Ff4SessionImpl *sess, void *fileCtx, const void *buf, uint64_t size,
		      uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_box_header(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			     uint64_t pos, uint64_t boxSize, uint32_t fourcc,
			     uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_calc_box_size(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, uint64_t payloadSize,
			  uint64_t *outBoxSize);
int32_t ff4_write_short_box_header(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				   uint64_t boxSize, uint32_t fourcc,
				   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_calc_short_box_size(Ff4SessionImpl *sess, uint64_t payloadSize, uint64_t *outBoxSize);
int32_t ff4_write_uuid_bytes(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			     const uint32_t uuid[4], uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_write_descr_header(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       uint64_t payloadSize, uint8_t tag,
			       uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_copy_data_buf(Ff4SessionImpl *sess, int32_t isString, Ff4BoxDataBuf *dst,
			  const void *src, uint64_t size);
int32_t ff4_choose_box_version(const Ff4FormatOpts *opts, uint64_t v1, uint64_t v2, uint64_t v3,
			       uint8_t *outVersion);
int32_t ff4_set_timestamps(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4MovieModel *moov,
			   uint32_t trackIndex, uint8_t flags, uint64_t creationTime, uint64_t modTime);
int32_t ff4_write_ftyp_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4FtypPlan *plan, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_plan_ftyp(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4FtypModel *model,
		      Ff4FtypPlan *outPlan);
int32_t ff4_write_ftyp_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4FtypPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_free_meta_list_all(Ff4SessionImpl *sess, Ff4MetaDataList *meta);
int32_t ff4_free_chunk_node_list(Ff4SessionImpl *sess, Ff4BoxChunkNode *head);
int32_t ff4_free_esds_model(Ff4SessionImpl *sess, Ff4EsdsModel *esds);
int32_t ff4_free_avcc_model(Ff4SessionImpl *sess, Ff4AvccModel *avcc);
int32_t ff4_free_stsd_entry_list(Ff4SessionImpl *sess, Ff4StsdEntry *head);
int32_t ff4_free_mtsm_entry_list(Ff4SessionImpl *sess, Ff4MtsmEntry *head);
int32_t ff4_free_mtdt_entry_list(Ff4SessionImpl *sess, Ff4MtdtEntry *head);
int32_t ff4_free_edit_list(Ff4SessionImpl *sess, Ff4EditListModel *edts);
int32_t ff4_free_track_list(Ff4SessionImpl *sess, Ff4TrackModel *head);
int32_t ff4_free_movie_model(Ff4SessionImpl *sess, Ff4MovieModel *moov);
int32_t ff4_free_prof_entry_list(Ff4SessionImpl *sess, Ff4ProfEntry *head);
int32_t ff4_free_prof_model(Ff4SessionImpl *sess, Ff4ProfModel *prof);
int32_t ff4_free_compat_list(Ff4SessionImpl *sess, Ff4NodeU32 *head);
int32_t ff4_free_ftyp_model(Ff4SessionImpl *sess, Ff4FtypModel *ftyp);
int32_t ff4_free_writer_models(Ff4SessionImpl *sess, Ff4WriterModels *models);
int32_t ff4_free_mtdt_input(Ff4SessionImpl *sess, void *ptr);
int32_t ff4_free_extra_plan_list(Ff4SessionImpl *sess, void *head);
int32_t ff4_free_esds_plan(Ff4SessionImpl *sess, void *plan);
int32_t ff4_free_plan_node_a(Ff4SessionImpl *sess, void *ptr);
int32_t ff4_free_plan_node_b(Ff4SessionImpl *sess, void *ptr);
int32_t ff4_free_plan_node_c(Ff4SessionImpl *sess, void *ptr);
int32_t ff4_free_stsd_plan_list(Ff4SessionImpl *sess, void *head);
int32_t ff4_free_mtsm_plan_list(Ff4SessionImpl *sess, void *head);
int32_t ff4_free_uuid_plan(Ff4SessionImpl *sess, void *plan);
int32_t ff4_free_tref_plan_list(Ff4SessionImpl *sess, void *head);
int32_t ff4_free_tref_plan(Ff4SessionImpl *sess, void *plan);
int32_t ff4_free_trak_plan_list(Ff4SessionImpl *sess, void *head);
int32_t ff4_free_moov_plan(Ff4SessionImpl *sess, void *plan);
int32_t ff4_free_run_block_list_a(Ff4SessionImpl *sess, void *head);
int32_t ff4_free_run_block_list_b(Ff4SessionImpl *sess, void *head);
int32_t ff4_free_run_block_list_c(Ff4SessionImpl *sess, void *head);
int32_t ff4_free_run_group_list(Ff4SessionImpl *sess, void *head);
int32_t ff4_free_run_table(Ff4SessionImpl *sess, void *table);
int32_t ff4_count_prof_entries(const Ff4ProfModel *prof, uint32_t *outCount);
int32_t ff4_count_tracks(const Ff4TrackModel *head, uint32_t *outCount);
int32_t ff4_count_mtdt_entries(const Ff4MtdtEntry *head, uint32_t *outCount);
int32_t ff4_count_stsd_entries(const Ff4StsdEntry *head, uint32_t *outCount);
int32_t ff4_count_stsd_plans(const void *head, uint32_t *outCount);
int32_t ff4_count_mtsm_entries(const Ff4MtsmEntry *head, uint32_t *outCount);
int32_t ff4_find_u32_node(Ff4NodeU32 *head, uint32_t value, Ff4NodeU32 **outNode);
int32_t ff4_find_track_by_id(Ff4TrackModel *head, uint32_t trackId,
			     Ff4TrackModel **outTrack, uint32_t *outIndex1);
int32_t ff4_get_track_at(Ff4TrackModel *head, uint32_t index1, Ff4TrackModel **outTrack);
int32_t ff4_get_mtdt_at(Ff4MtdtEntry *head, uint32_t index1, Ff4MtdtEntry **outEntry);
int32_t ff4_get_mtsm_at(Ff4MtsmEntry *head, uint32_t index1, Ff4MtsmEntry **outEntry);
int32_t ff4_find_mtsm_by_id(Ff4MtsmEntry *head, uint32_t streamId, Ff4MtsmEntry **outEntry);
int32_t ff4_find_group_by_tag(Ff4NodeGroup *head, uint32_t tag, Ff4NodeGroup **outGroup);
int32_t ff4_find_u32_with_index(Ff4NodeU32 *head, uint32_t value,
				Ff4NodeU32 **outNode, uint32_t *outIndex1);
int32_t ff4_get_stsd_at(Ff4StsdEntry *head, uint32_t index1, Ff4StsdEntry **outEntry);
int32_t ff4_find_tref_by_id(Ff4TrefEntry *head, uint16_t id, Ff4TrefEntry **outEntry);
int32_t ff4_find_u8_node(Ff4NodeU32 *head, uint8_t val, Ff4NodeU32 **outNode);

/* ff4_session.c */
int32_t ff4_convert_mp4time_to_date(Ff4SessionImpl *sess, uint64_t mp4Time, Ff4DateTime *outDate);
int32_t ff4_rescale_u64(uint64_t value, uint32_t denom, uint32_t numer, uint64_t *outVal);
int32_t ff4_get_mem_stats(Ff4SessionImpl *sess, Ff4MemStats *outStats);
int32_t ff4_u128_add(int32_t *outOverflow, uint64_t *hi, uint64_t *lo, uint64_t addHi, uint64_t addLo);
int32_t ff4_u128_sub(int32_t *outBorrow, uint64_t *hi, uint64_t *lo, uint64_t subHi, uint64_t subLo);
int32_t ff4_u64_mul128(uint64_t a, uint64_t b, uint64_t *outHi, uint64_t *outLo);
int32_t ff4_u64_shl128(uint64_t val, uint32_t shift, uint64_t *outHi, uint64_t *outLo);
int32_t ff4_u128_div64(uint64_t numHi, uint64_t numLo, uint64_t denom,
		       uint64_t *outQuotHi, uint64_t *outQuotLo, uint64_t *outRem);
int32_t ff4_io_read(Ff4SessionImpl *sess, void *fileCtx, void *buf, uint64_t size, uint64_t *outRead);
int32_t ff4_io_read_be16(Ff4SessionImpl *sess, void *fileCtx, uint16_t *outVal);
int32_t ff4_io_read_be32(Ff4SessionImpl *sess, void *fileCtx, uint32_t *outVal);
int32_t ff4_io_read_be64(Ff4SessionImpl *sess, void *fileCtx, uint64_t *outVal);
int32_t ff4_io_write(Ff4SessionImpl *sess, void *fileCtx, const void *buf, uint64_t size, uint64_t *outWritten);
int32_t ff4_io_seek(Ff4SessionImpl *sess, void *fileCtx, int64_t offset, int32_t whence);
int32_t ff4_io_tell(Ff4SessionImpl *sess, void *fileCtx, uint64_t *outPos);
int32_t ff4_io_getsize(Ff4SessionImpl *sess, void *fileCtx, uint64_t *outSize);
void *ff4_malloc(Ff4SessionImpl *sess, uint64_t size);
void *ff4_memset(Ff4SessionImpl *sess, void *dst, int32_t val, uint64_t size);
void *ff4_calloc(Ff4SessionImpl *sess, uint64_t size);
void ff4_free(Ff4SessionImpl *sess, void *ptr);
uint64_t ff4_strnlen(const char *s, uint64_t maxLen);
int32_t ff4_strncmp(Ff4SessionImpl *sess, const char *s1, const char *s2, uint64_t n);
char *ff4_strncpy(Ff4SessionImpl *sess, char *dst, uint64_t dstSize, const char *src, uint64_t n);
int32_t ff4_memcmp(Ff4SessionImpl *sess, const void *p1, const void *p2, uint64_t n);
void *ff4_memcpy(Ff4SessionImpl *sess, void *dst, uint64_t dstSize, const void *src, uint64_t n);
void *ff4_memmove(Ff4SessionImpl *sess, void *dst, uint64_t dstSize, const void *src, uint64_t n);

/* ff4_util.c */
int32_t ff4_buf_init(Ff4SessionImpl *sess, uint32_t poolType, uint64_t size, Ff4Buffer *outBuf);
int32_t ff4_buf_free(Ff4SessionImpl *sess, Ff4Buffer *buf);
int32_t ff4_stream_reset(Ff4SessionImpl *sess, Ff4BufStream *stream);
int32_t ff4_stream_seek(Ff4SessionImpl *sess, Ff4BufStream *stream, int64_t offset, uint32_t whence);
int32_t ff4_stream_open(Ff4SessionImpl *sess, void *fileCtx, uint64_t startPos,
			uint8_t *bufBase, uint64_t bufCap, Ff4BufStream *outStream);
int32_t ff4_stream_read(Ff4SessionImpl *sess, Ff4BufStream *stream, uint8_t *dst,
			uint64_t size, uint64_t *outRead);
int32_t ff4_stream_read_be16(Ff4SessionImpl *sess, Ff4BufStream *stream, uint16_t *outVal);
int32_t ff4_stream_read_be32(Ff4SessionImpl *sess, Ff4BufStream *stream, uint32_t *outVal);
int32_t ff4_stream_read_be64(Ff4SessionImpl *sess, Ff4BufStream *stream, uint64_t *outVal);
int32_t ff4_free_meta_item_list(Ff4SessionImpl *sess, Ff4MetaItemNode *head);
int32_t ff4_free_pair_list(Ff4SessionImpl *sess, Ff4NodePair *head);
int32_t ff4_free_zpck_table(Ff4SessionImpl *sess, Ff4ZpckTable *table);
int32_t ff4_free_triple_list(Ff4SessionImpl *sess, Ff4NodeTriple *head);
int32_t ff4_free_u32_list(Ff4SessionImpl *sess, Ff4NodeU32 *head);
int32_t ff4_free_group_list(Ff4SessionImpl *sess, Ff4NodeGroup *head);
int32_t ff4_free_edit_entry_list(Ff4SessionImpl *sess, Ff4EditEntryNode *head);
int32_t ff4_free_dref_list(Ff4SessionImpl *sess, Ff4DrefEntry *head);
int32_t ff4_free_buf_list(Ff4SessionImpl *sess, Ff4NodeBuf *head);
int32_t ff4_free_buf_node(Ff4SessionImpl *sess, Ff4NodeBuf *node);
int32_t ff4_free_single(Ff4SessionImpl *sess, void *ptr);
int32_t ff4_free_brand_list(Ff4SessionImpl *sess, Ff4NodeU32 *head);
int32_t ff4_free_id_list(Ff4SessionImpl *sess, Ff4NodeU32 *head);
int32_t ff4_free_tref_list(Ff4SessionImpl *sess, Ff4TrefEntry *head);
int32_t ff4_free_meta_list(Ff4SessionImpl *sess, Ff4MetaItemNode *head);
int32_t ff4_count_meta_item_list(const Ff4MetaItemNode *head, uint32_t *outCount);
int32_t ff4_count_pair_list(const Ff4NodePair *head, uint32_t *outCount);
int32_t ff4_count_triple_list(const Ff4NodeTriple *head, uint32_t *outCount);
int32_t ff4_count_u32_list(const Ff4NodeU32 *head, uint32_t *outCount);
int32_t ff4_count_edit_entry_list(const Ff4EditEntryNode *head, uint32_t *outCount);
int32_t ff4_count_dref_list(const Ff4DrefEntry *head, uint32_t *outCount);
int32_t ff4_count_buf_list(const Ff4NodeBuf *head, uint32_t *outCount);
int32_t ff4_get_triple_at(Ff4NodeTriple *head, uint32_t index1, Ff4NodeTriple **outNode);
int32_t ff4_get_u32_at(Ff4NodeU32 *head, uint32_t index1, Ff4NodeU32 **outNode);
int32_t ff4_get_dref_at(Ff4DrefEntry *head, uint32_t index1, Ff4DrefEntry **outNode);
int32_t ff4_find_meta_item_by_tag(Ff4MetaItemNode *head, uint8_t tag,
			       Ff4MetaItemNode **outNode, uint32_t *outIndex1);
void *ff4_pool_alloc(Ff4SessionImpl *sess, uint32_t poolType, uint64_t size);
int32_t ff4_pool_parse_header(Ff4SessionImpl *sess, const void *ptr, Ff4AllocHeader *outHdr);
void ff4_pool_free(Ff4SessionImpl *sess, void *ptr);
int32_t ff4_pool_init(Ff4SessionImpl *sess, uint32_t useDefaultCache, uint64_t totalMem,
		      uint32_t cachePercent, uint64_t minMetaMem);
int32_t ff4_parse_zpck_header(Ff4SessionImpl *sess, const uint8_t *buf, uint64_t offset,
			      uint64_t size, Ff4ZpckHeader *outHdr);
int32_t ff4_parse_zpck_entries(Ff4SessionImpl *sess, const uint8_t *buf, uint64_t offset,
			       uint64_t size, uint32_t entryCount, Ff4ZpckTable *table);
int32_t ff4_parse_zpck_table_hdr(Ff4SessionImpl *sess, const uint8_t *buf, uint64_t offset,
				 uint64_t size, Ff4ZpckTableHdr *outHdr);
int32_t ff4_parse_zpck_table(Ff4SessionImpl *sess, const uint8_t *buf, uint64_t offset,
			     uint64_t size, Ff4ZpckTable *outTable);

/* ff4_stbl.c cross-TU declarations */
int32_t ff4_write_od_sample_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				    const void *planPtr, uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_plan_od_sample(Ff4SessionImpl *sess, Ff4MetaDataList *meta, void *outPlanPtr);
int32_t ff4_shift_chunk_offsets(Ff4SessionImpl *sess, int32_t useFragMode, uint32_t trackIdx,
				uint32_t fragIdx, uint32_t chunkIdx1, int64_t delta,
				Ff4TrackModel *trackListHead);
int32_t ff4_get_track_duration_in_timescale(uint32_t movieTimescale, const Ff4TrackModel *track,
					    uint64_t *outDur);
int32_t ff4_get_track_min_pres_in_timescale(uint32_t movieTimescale, const Ff4TrackModel *track,
					    uint64_t *outMinPres);
int32_t ff4_add_sample(Ff4SessionImpl *sess, Ff4WriterModels *models, uint32_t trackIdx,
		       const void *samplePtr, int32_t recordTfra);
int32_t ff4_commit_chunk(Ff4SessionImpl *sess, Ff4WriterModels *models, uint32_t trackIdx);

/* ff4_stsd.c cross-TU declarations */
int32_t ff4_add_stsd_entry(Ff4SessionImpl *sess, Ff4StsdEntry **headTail,
			   const void *inputPtr, uint32_t *outIndex1);
int32_t ff4_set_stsd_entry_params(Ff4SessionImpl *sess, Ff4StsdEntry *entryPtr, uint16_t mask,
				  const void *paramsPtr);
int32_t ff4_set_stsd_esds(Ff4SessionImpl *sess, Ff4StsdEntry *entryPtr, const void *esdsInputPtr);
int32_t ff4_set_stsd_avcc_hdr(Ff4SessionImpl *sess, Ff4StsdEntry *entryPtr, const uint8_t hdr[5]);
int32_t ff4_add_avcc_nalu(Ff4SessionImpl *sess, Ff4NodeBuf **headTail, const void *naluInputPtr);
int32_t ff4_set_stsd_text_subbox(Ff4SessionImpl *sess, Ff4StsdEntry *entryPtr, const void *subBoxPtr);

/* ff4_uuid.c cross-TU declarations */
int32_t ff4_write_raw_uuid_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const void *rawUuidPlan,
			       uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_plan_prof(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4ProfModel *prof,
		      void *outProfPlan);
int32_t ff4_write_prof_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const void *profPlan,
			   uint64_t *outWritten, uint64_t *accumWritten);
int32_t ff4_add_mtdt_zpck_table(Ff4SessionImpl *sess, Ff4MtdtEntry **headTail,
				const void *inputPtr, uint32_t *outIndex1);
int32_t ff4_append_zpck_entry_to_mtdt(Ff4SessionImpl *sess, Ff4MtdtEntry *mtdt,
				      const uint32_t pair[2]);
int32_t ff4_set_mtdt_item(Ff4SessionImpl *sess, Ff4MovieModel *moov, uint32_t trackIdx,
			  const void *inputPtr);
int32_t ff4_get_mtdt_zpck_header(Ff4SessionImpl *sess, Ff4MtdtEntry *const *headTail,
				 Ff4ZpckHeader *outHdr, Ff4MtdtEntry **outEntry);
int32_t ff4_set_mtdf_item(Ff4SessionImpl *sess, Ff4MovieModel *moov, uint32_t trackIdx,
			  const void *inputPtr);
int32_t ff4_is_mtsm_id_available(uint32_t id, Ff4MtsmEntry *head, int32_t *outAvail);
int32_t ff4_set_mtsh_params(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
			    Ff4MtsmEntry *mtsm, uint32_t mask, const void *paramsPtr);
int32_t ff4_add_mtct_entry(Ff4SessionImpl *sess, Ff4NodeTriple **headTail,
			   const uint32_t vals[3], uint32_t *outCount);

#ifdef __cplusplus
}
#endif

#endif /* FF4_INTERNAL_H */
