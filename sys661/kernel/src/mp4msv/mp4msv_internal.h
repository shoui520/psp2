#ifndef MP4MSV_INTERNAL_H
#define MP4MSV_INTERNAL_H

#include <stdint.h>
#include <stddef.h>

#define MP4_NOINLINE __attribute__((noinline))

/* Standard ISO 14496-12 and Sony MSV box tags */
#define MP4_TAG_FTYP 0x66747970U /* 'ftyp' */
#define MP4_TAG_FREE 0x66726565U /* 'free' */
#define MP4_TAG_MDAT 0x6D646174U /* 'mdat' */
#define MP4_TAG_MOOV 0x6D6F6F76U /* 'moov' */
#define MP4_TAG_MVHD 0x6D766864U /* 'mvhd' */
#define MP4_TAG_IODS 0x696F6473U /* 'iods' */
#define MP4_TAG_TRAK 0x7472616BU /* 'trak' */
#define MP4_TAG_TKHD 0x746B6864U /* 'tkhd' */
#define MP4_TAG_TREF 0x74726566U /* 'tref' */
#define MP4_TAG_EDTS 0x65647473U /* 'edts' */
#define MP4_TAG_ELST 0x656C7374U /* 'elst' */
#define MP4_TAG_MDIA 0x6D646961U /* 'mdia' */
#define MP4_TAG_MDHD 0x6D646864U /* 'mdhd' */
#define MP4_TAG_HDLR 0x68646C72U /* 'hdlr' */
#define MP4_TAG_MINF 0x6D696E66U /* 'minf' */
#define MP4_TAG_VMHD 0x766D6864U /* 'vmhd' */
#define MP4_TAG_SMHD 0x736D6864U /* 'smhd' */
#define MP4_TAG_NMHD 0x6E6D6864U /* 'nmhd' */
#define MP4_TAG_DINF 0x64696E66U /* 'dinf' */
#define MP4_TAG_DREF 0x64726566U /* 'dref' */
#define MP4_TAG_URL  0x75726C20U /* 'url ' */
#define MP4_TAG_STBL 0x7374626CU /* 'stbl' */
#define MP4_TAG_STSD 0x73747364U /* 'stsd' */
#define MP4_TAG_STTS 0x73747473U /* 'stts' */
#define MP4_TAG_STSS 0x73747373U /* 'stss' */
#define MP4_TAG_STSC 0x73747363U /* 'stsc' */
#define MP4_TAG_STSZ 0x7374737AU /* 'stsz' */
#define MP4_TAG_STCO 0x7374636FU /* 'stco' */
#define MP4_TAG_CTTS 0x63747473U /* 'ctts' */
#define MP4_TAG_UDTA 0x75647461U /* 'udta' */
#define MP4_TAG_UUID 0x75756964U /* 'uuid' */
#define MP4_TAG_MVEX 0x6D766578U /* 'mvex' */
#define MP4_TAG_TREX 0x74726578U /* 'trex' */
#define MP4_TAG_MOOF 0x6D6F6F66U /* 'moof' */
#define MP4_TAG_MFHD 0x6D666864U /* 'mfhd' */
#define MP4_TAG_TRAF 0x74726166U /* 'traf' */
#define MP4_TAG_TFHD 0x74666864U /* 'tfhd' */
#define MP4_TAG_TRUN 0x7472756EU /* 'trun' */
#define MP4_TAG_MFRA 0x6D667261U /* 'mfra' */
#define MP4_TAG_TFRA 0x74667261U /* 'tfra' */
#define MP4_TAG_MFRO 0x6D66726FU /* 'mfro' */
#define MP4_TAG_ESDS 0x65736473U /* 'esds' */
#define MP4_TAG_AVCC 0x61766343U /* 'avcC' */
#define MP4_TAG_AVC1 0x61766331U /* 'avc1' */
#define MP4_TAG_SINF 0x73696E66U /* 'sinf' */
#define MP4_TAG_ENCA 0x656E6361U /* 'enca' */
#define MP4_TAG_ENCV 0x656E6376U /* 'encv' */
#define MP4_TAG_VIDE 0x76696465U /* 'vide' */
#define MP4_TAG_SOUN 0x736F756EU /* 'soun' */
#define MP4_TAG_ODSM 0x6F64736DU /* 'odsm' */
#define MP4_TAG_GRPH 0x47525048U /* 'GRPH' */
#define MP4_TAG_PNG  0x504E4720U /* 'PNG ' */
#define MP4_TAG_MPOD 0x6D706F64U /* 'mpod' */
#define MP4_TAG_SYNC 0x73796E63U /* 'sync' */
#define MP4_TAG_VTBL 0x7674626CU /* 'vtbl' */
#define MP4_TAG_SLDM 0x736D666DU /* 'smfm' (root container marker) */

/* Sony MSV proprietary tags and UUID prefixes */
#define MP4_TAG_PROF 0x50524F46U /* 'PROF' */
#define MP4_TAG_FPRF 0x46505246U /* 'FPRF' */
#define MP4_TAG_APRF 0x41505246U /* 'APRF' */
#define MP4_TAG_VPRF 0x56505246U /* 'VPRF' */
#define MP4_TAG_USMT 0x55534D54U /* 'USMT' */
#define MP4_TAG_MTDF 0x4D544446U /* 'MTDF' */
#define MP4_TAG_MTDT 0x4D544454U /* 'MTDT' */
#define MP4_TAG_MTSM 0x4D54534DU /* 'MTSM' */
#define MP4_TAG_MTHD 0x4D544844U /* 'MTHD' */
#define MP4_TAG_MCST 0x4D435354U /* 'MCST' */
#define MP4_TAG_MTSD 0x4D545344U /* 'MTSD' */
#define MP4_TAG_LICI 0x4C494349U /* 'LICI' */
#define MP4_TAG_STSD_UUID 0x33443353U /* '3D3S' */
#define MP4_TAG_SKIP_UUID 0x33443354U /* '3D3T' */

/* Sony UUID fixed tail words */
#define MP4_SONY_UUID_WORD_A 0x21D24FCEU
#define MP4_SONY_UUID_WORD_B 0xBB88695CU
#define MP4_SONY_UUID_WORD_C 0xFAC9C740U

/* Common mp4msv status / error codes */
#define MP4_OK                    0
#define MP4_ERR_NOMEM             1
#define MP4_ERR_IO                2
#define MP4_ERR_NOT_SUPPORTED     3
#define MP4_ERR_INVALID_HANDLE    4
#define MP4_ERR_INVALID_PARAM     6
#define MP4_ERR_NOT_READY         7
#define MP4_ERR_OVERFLOW          8
#define MP4_ERR_NOT_FOUND         9
#define MP4_ERR_INTERNAL         10
#define MP4_ERR_NO_PREV_FRAG     16
#define MP4_ERR_BAD_STATE        17
#define MP4_ERR_CHUNK_EMPTY    4109
#define MP4_ERR_OUT_OF_CHUNKS  4110
#define MP4_ERR_CODEC_MISMATCH 4112
#define MP4_ERR_FRAG_OVERFLOW  4120
#define MP4_ERR_DUR_OVERFLOW   4121
#define MP4_ERR_EVICT_FAIL     4122
#define MP4_ERR_COUNT_OVERFLOW 4123
#define MP4_ERR_BAD_FORMAT     8193
#define MP4_ERR_NO_ENTRY       8194
#define MP4_ERR_CORRUPT        8195
#define MP4_ERR_TIME_RANGE     8197
#define MP4_ERR_NO_SYNC        8200
#define MP4_ERR_SAMPLE_RANGE   8201
#define MP4_ERR_TRACK_RANGE    8203
#define MP4_ERR_NO_MOOV        8207
#define MP4_ERR_PARSE_FATAL   20481
#define MP4_ERR_PARSE_WARN    20485
#define MP4_ERR_INDEX_RANGE   20490
#define MP4_ERR_SUB_UNDERFLOW 20493
#define MP4_ERR_ADD_OVERFLOW  20494
#define MP4_ERR_NULL_OBJ      20495
#define MP4_ERR_EOF_FRAG      20496
#define MP4_ERR_NO_NEXT_FRAG  20497
#define MP4_ERR_TRAF_RANGE    20498
#define MP4_ERR_TRUN_RANGE    20499

/* Parse diagnostic flags (stored in Mp4IoStream.error_flags) */
#define MP4_PARSE_FLAG_SIZE_MISMATCH 0x00000001U
#define MP4_PARSE_FLAG_COUNT_CLAMPED 0x00000002U
#define MP4_PARSE_FLAG_UNREC_BOX     0x00000004U
#define MP4_PARSE_FLAG_DUPLICATE_BOX 0x00000008U
#define MP4_PARSE_FLAG_TRACK_LIMIT   0x00000010U
#define MP4_PARSE_FLAG_DREF_MISMATCH 0x00000020U
#define MP4_PARSE_FLAG_HAS_MOOF      0x00010000U
#define MP4_PARSE_FLAG_HAS_MVEX      0x00040000U
#define MP4_PARSE_FLAG_BAD_BOX_SIZE  0x20000000U
#define MP4_PARSE_FLAG_ALLOC_FAIL    0x40000000U
#define MP4_PARSE_FLAG_IO_FAIL       0x80000000U
#define MP4_PARSE_FLAG_FATAL_MASK    0xE0000000U

/* Forward declarations */
struct Mp4Handle;
struct Mp4IoStream;
struct Mp4Movie;
struct Mp4Track;
struct Mp4Stbl;
struct Mp4StsdBox;
struct Mp4SampleEntry;
struct Mp4CacheStbl;
struct Mp4MoofBox;
struct Mp4TrafBox;
struct Mp4TrunBox;
struct Mp4MvexBox;
struct Mp4MfraBox;
struct Mp4TfraBox;
struct Mp4UsmtBox;
struct Mp4MtsmBox;
struct Mp4DynArray;
struct Mp4ChunkArray;

/* Host-registered callback table (17 function pointers at 0x25DC0..0x25E00) */
typedef struct Mp4Callbacks {
    void *open_fn;
    void *close_fn;
    int64_t (*tell_fn)(void *fh);
    int32_t (*seek_fn)(void *fh, int64_t offset, int16_t origin);
    int64_t (*get_size_fn)(void *fh);
    int32_t (*read_fn)(void *dst, uint32_t size, void *fh);
    int32_t (*read_be_fn)(void *dst, uint32_t size, void *fh);
    void *(*malloc_fn)(uint32_t size);
    void *(*calloc_fn)(uint32_t size);
    void (*free_fn)(void *ptr);
    int32_t (*memcpy_fn)(void *dst, uint32_t dst_size, const void *src, uint32_t len);
    void *memmove_fn;
    int32_t (*strncpy_fn)(char *dst, const char *src, uint32_t len);
    void *strcmp_fn;
    void *strncmp_fn;
    void *memset_fn;
    uint32_t (*strlen_fn)(const char *str);
} Mp4Callbacks;

/* Global configuration parameters (0x25D40) */
typedef struct Mp4Config {
    uint32_t base_unit;
    uint32_t min_mul;
    uint32_t cache_mul;
    uint32_t max_mul;
    uint32_t extra_opt;
} Mp4Config;

/* State machine (8 bytes) */
typedef struct Mp4Fsm {
    int32_t cur_state;
    int32_t pending_state;
} Mp4Fsm;

typedef struct Mp4StateTrans {
    int32_t allowed;
    int32_t next_state;
} Mp4StateTrans;

/* Date/time output struct (12 bytes = 6 u16) */
typedef struct Mp4DateTime {
    uint16_t second;
    uint16_t minute;
    uint16_t hour;
    uint16_t day;
    uint16_t month;
    uint16_t year;
} Mp4DateTime;

/* IO stream context (88 bytes) */
typedef struct Mp4IoStream {
    void *file_handle;          /* +0x00 */
    uint32_t error_flags;       /* +0x04 */
    uint32_t reserved_a;        /* +0x08 */
    uint32_t max_frag_window;   /* +0x0C */
    uint32_t base_unit;         /* +0x10 */
    uint32_t min_read;          /* +0x14 */
    uint32_t cache_size;        /* +0x18 */
    uint32_t max_read;          /* +0x1C */
    uint32_t is_fragmented;     /* +0x20 */
    void *user_handle;          /* +0x24 */
    uint32_t is_attached;       /* +0x28 */
    uint32_t reserved_b;        /* +0x2C */
    uint32_t reserved_c;        /* +0x30 */
    uint32_t reserved_d;        /* +0x34 */
    uint32_t reserved_e;        /* +0x38 */
    uint32_t reserved_f;        /* +0x3C */
    uint32_t reserved_g;        /* +0x40 */
    uint32_t reserved_h;        /* +0x44 */
    uint32_t reserved_i;        /* +0x48 */
    uint32_t reserved_j;        /* +0x4C */
    uint32_t io_mode;           /* +0x50 */
    Mp4Fsm *fsm;                /* +0x54 */
} Mp4IoStream;

/* Top-level API handle (12 bytes) */
typedef struct Mp4Handle {
    struct Mp4Movie *movie;     /* +0x00 */
    Mp4IoStream *stream;        /* +0x04 */
    uint32_t options;           /* +0x08 */
} Mp4Handle;

/* Doubly-linked block array (28 bytes header + 16 bytes block node) */
typedef struct Mp4DynBlock {
    uint32_t alloc_bytes;         /* +0x00 */
    uint8_t *buf;               /* +0x04 */
    struct Mp4DynBlock *prev;   /* +0x08 */
    struct Mp4DynBlock *next;   /* +0x0C */
} Mp4DynBlock;

typedef struct Mp4DynArray {
    uint32_t elem_size;         /* +0x00 */
    uint32_t count;             /* +0x04 */
    uint32_t capacity;          /* +0x08 */
    uint32_t block_cap;         /* +0x0C */
    uint32_t block_count;       /* +0x10 */
    Mp4DynBlock *head;          /* +0x14 */
    Mp4DynBlock *tail;          /* +0x18 */
} Mp4DynArray;

/* Singly-linked chunked word array (28 bytes per chunk) */
typedef struct Mp4ChunkArray {
    uint32_t count;             /* +0x00 */
    uint32_t capacity;          /* +0x04 */
    uint32_t cols;              /* +0x08 */
    struct Mp4ChunkArray *next; /* +0x0C */
    uint32_t *words;            /* +0x10 */
    void *extra_a;              /* +0x14 */
    uint64_t *wide_words;       /* +0x18 */
} Mp4ChunkArray;

/* Unrecognized UUID node in movie list (32 bytes) */
typedef struct Mp4UuidNode {
    struct Mp4UuidNode *next;   /* +0x00 */
    uint32_t box_size;          /* +0x04 */
    uint64_t file_offset;       /* +0x08 */
    uint32_t uuid[4];           /* +0x10 */
} Mp4UuidNode;

/* MPEG-4 ES_Descriptor / ObjectDescriptor tree */
typedef struct Mp4OdsmEsItem {
    uint8_t tag;                /* +0x00 (11) */
    uint8_t es_index;           /* +0x01 */
    uint16_t stream_type;       /* +0x02 */
    uint8_t *payload;           /* +0x04 */
    uint32_t payload_size;      /* +0x08 */
} Mp4OdsmEsItem; /* 12 bytes */

typedef struct Mp4OdsmObject {
    uint8_t tag;                /* +0x00 (17) */
    uint8_t pad_head;           /* +0x01 */
    uint16_t object_id;         /* +0x02 */
    uint32_t url_flag;          /* +0x04 */
    uint8_t url_len;            /* +0x08 */
    uint8_t pad_url[3];         /* +0x09 */
    uint8_t *url_str;           /* +0x0C */
    uint32_t reserved_words[2]; /* +0x10..+0x17 */
    uint16_t *es_ids;           /* +0x18 */
    uint8_t es_id_count;        /* +0x1C */
    uint8_t pad_a[3];           /* +0x1D */
    uint8_t *es_indices;        /* +0x20 */
    uint8_t es_index_count;     /* +0x24 */
    uint8_t pad_b[3];           /* +0x25 */
    struct Mp4Track *od_track;  /* +0x28 */
} Mp4OdsmObject; /* 44 bytes */

typedef struct Mp4OdsmDescEntry {
    uint8_t tag;                /* +0x00 */
    uint8_t pad[3];             /* +0x01 */
    void *body;                 /* +0x04 */
} Mp4OdsmDescEntry; /* 8 bytes */

typedef struct Mp4OdsmCtx {
    uint8_t od_update_seen;     /* +0x00 */
    uint8_t es_update_seen;     /* +0x01 */
    uint8_t pad[10];            /* +0x02..+0x0B */
    uint32_t count;             /* +0x0C */
    Mp4OdsmDescEntry *entries;  /* +0x10 */
} Mp4OdsmCtx; /* 20 bytes */

typedef struct Mp4EsDecSpecific {
    uint8_t object_type_ind;    /* +0x00 */
    uint8_t stream_type;        /* +0x01 */
    uint16_t reserved;          /* +0x02 */
    uint32_t buffer_size_db;    /* +0x04 */
    uint32_t max_bitrate;       /* +0x08 */
    uint32_t avg_bitrate;       /* +0x0C */
    uint8_t *dec_specific_info; /* +0x10 */
    uint32_t dec_specific_size; /* +0x14 */
} Mp4EsDecSpecific;

typedef struct Mp4DescNode {
    uint8_t tag;                /* +0x00 */
    uint8_t pad[3];             /* +0x01 */
    void *body;                 /* +0x04 */
} Mp4DescNode; /* 8 bytes */

typedef struct Mp4EsDescBody {
    uint8_t tag;                /* +0x00 (3) */
    uint8_t pad_head;           /* +0x01 */
    uint16_t es_id;             /* +0x02 */
    uint32_t stream_dep_flag;   /* +0x04 */
    uint32_t url_flag;          /* +0x08 */
    uint32_t ocr_stream_flag;   /* +0x0C */
    uint16_t reserved_dep;      /* +0x10 */
    uint16_t depends_on_es_id;  /* +0x12 */
    uint8_t url_len;            /* +0x14 */
    uint8_t pad_url[3];         /* +0x15 */
    uint8_t *url_str;           /* +0x18 */
    uint16_t ocr_es_id;         /* +0x1C */
    uint16_t pad_ocr;           /* +0x1E */
    uint8_t has_dec_cfg;        /* +0x20 (tag 4) */
    uint8_t object_type_ind;    /* +0x21 */
    uint8_t stream_type_byte;   /* +0x22 */
    uint8_t pad_byte;           /* +0x23 */
    uint32_t up_stream;         /* +0x24 */
    uint32_t buffer_size_db;    /* +0x28 */
    uint32_t max_bitrate;       /* +0x2C */
    uint32_t avg_bitrate;       /* +0x30 */
    uint8_t *dec_specific_info; /* +0x34 */
    uint32_t dec_specific_size; /* +0x38 */
    uint8_t sl_cfg_tag;         /* +0x3C (tag 6) */
    uint8_t sl_predefined;      /* +0x3D */
    uint16_t pad_sl;            /* +0x3E */
    uint32_t sl_use_au_start;   /* +0x40 */
    uint32_t sl_use_au_end;     /* +0x44 */
    uint32_t sl_use_rap;        /* +0x48 */
    uint32_t sl_has_random_pad; /* +0x4C */
    uint32_t sl_use_padding;    /* +0x50 */
    uint32_t sl_use_timestamps; /* +0x54 */
    uint32_t sl_use_idle;       /* +0x58 */
    uint32_t sl_duration_flag;  /* +0x5C */
    uint32_t sl_ts_resolution;  /* +0x60 */
    uint32_t sl_ocr_resolution; /* +0x64 */
    uint8_t sl_ts_len;          /* +0x68 */
    uint8_t sl_ocr_len;         /* +0x69 */
    uint8_t sl_au_len;          /* +0x6A */
    uint8_t sl_inst_bitrate_len;/* +0x6B */
    uint8_t sl_degrad_prio_len; /* +0x6C */
    uint8_t sl_au_seq_len;      /* +0x6D */
    uint8_t sl_pkt_seq_len;     /* +0x6E */
    uint8_t pad_sl_tail;        /* +0x6F */
    uint32_t sl_time_scale;     /* +0x70 */
    uint16_t sl_au_duration;    /* +0x74 */
    uint16_t sl_cu_duration;    /* +0x76 */
    uint32_t sl_start_dts_lo;   /* +0x78 */
    uint32_t sl_start_dts_hi;   /* +0x7C */
    uint32_t sl_start_cts_lo;   /* +0x80 */
    uint32_t sl_start_cts_hi;   /* +0x84 */
} Mp4EsDescBody; /* 136 bytes */

typedef struct Mp4IodsBody {
    uint8_t tag;                /* +0x00 (16) */
    uint8_t pad_head;           /* +0x01 */
    uint16_t object_id;         /* +0x02 */
    uint32_t url_flag;          /* +0x04 */
    uint8_t url_len;            /* +0x08 */
    uint8_t pad_url[3];         /* +0x09 */
    uint8_t *url_str;           /* +0x0C */
    uint32_t include_inline;    /* +0x10 */
    uint8_t od_profile;         /* +0x14 */
    uint8_t scene_profile;      /* +0x15 */
    uint8_t audio_profile;      /* +0x16 */
    uint8_t visual_profile;     /* +0x17 */
    uint8_t graphics_profile;   /* +0x18 */
    uint8_t pad_prof[3];        /* +0x19 */
    uint32_t *es_track_ids;     /* +0x1C */
    uint8_t es_track_count;     /* +0x20 */
    uint8_t pad_tail[3];        /* +0x21 */
} Mp4IodsBody; /* 36 bytes */

typedef struct Mp4BitReader {
    const uint8_t *ptr;         /* +0x00 */
    uint8_t bit_pos;            /* +0x04 */
    uint8_t pad[3];             /* +0x05 */
} Mp4BitReader; /* 8 bytes */

/* Full-box headers and container boxes */
typedef struct Mp4MvhdBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t creation_time;     /* +0x0C */
    uint32_t modification_time; /* +0x10 */
    uint32_t timescale;         /* +0x14 */
    uint32_t duration;          /* +0x18 */
    int32_t rate;               /* +0x1C */
    int16_t volume;             /* +0x20 */
    int16_t reserved_short;     /* +0x22 */
    uint32_t reserved_pair[2];  /* +0x24 */
    int32_t matrix[9];          /* +0x2C */
    uint32_t predefined[6];     /* +0x50 */
    uint32_t next_track_id;     /* +0x68 */
} Mp4MvhdBox; /* 108 bytes */

typedef struct Mp4IodsBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint8_t *payload;           /* +0x0C */
    struct Mp4Track *od_track;  /* +0x10 */
} Mp4IodsBox; /* 20 bytes */

typedef struct Mp4FtypBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint32_t major_brand;       /* +0x08 */
    uint32_t minor_version;     /* +0x0C */
    uint16_t compat_count;      /* +0x10 */
    uint16_t compat_capacity;   /* +0x12 */
    uint32_t *compat_brands;    /* +0x14 */
} Mp4FtypBox; /* 24 bytes */

/* Sony PROF box and sub-profiles */
typedef struct Mp4FprfBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t global_attr;       /* +0x0C */
    uint32_t reserved_val;      /* +0x10 */
} Mp4FprfBox; /* 20 bytes */

typedef struct Mp4AprfBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t track_id;          /* +0x0C */
    uint32_t codec_tag;         /* +0x10 */
    uint32_t codec_attr;        /* +0x14 */
    uint32_t avg_bitrate;       /* +0x18 */
    uint32_t max_bitrate;       /* +0x1C */
    uint32_t sample_rate;       /* +0x20 */
    uint32_t channel_count;     /* +0x24 */
    uint32_t extra_attr;        /* +0x28 */
} Mp4AprfBox; /* 44 bytes */

typedef struct Mp4VprfBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t track_id;          /* +0x0C */
    uint32_t codec_tag;         /* +0x10 */
    uint32_t codec_attr;        /* +0x14 */
    uint32_t avg_bitrate;       /* +0x18 */
    uint32_t max_bitrate;       /* +0x1C */
    uint32_t avg_frame_rate;    /* +0x20 */
    uint32_t max_frame_rate;    /* +0x24 */
    uint32_t width;             /* +0x28 */
    uint32_t height;            /* +0x2C */
    uint32_t pixel_aspect;      /* +0x30 */
} Mp4VprfBox; /* 52 bytes */

typedef struct Mp4ProfBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint32_t uuid[4];           /* +0x08 */
    uint8_t version;            /* +0x18 */
    uint8_t flags[3];           /* +0x19 */
    uint32_t fprf_count;        /* +0x1C */
    uint32_t aprf_count;        /* +0x20 */
    uint32_t vprf_count;        /* +0x24 */
    Mp4FprfBox *fprf_list;      /* +0x28 */
    Mp4AprfBox *aprf_list;      /* +0x2C */
    Mp4VprfBox *vprf_list;      /* +0x30 */
} Mp4ProfBox; /* 52 bytes */

/* Sony LICI box (24 bytes) */
typedef struct Mp4LiciBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint32_t first_len;         /* +0x08 */
    uint8_t *first_buf;         /* +0x0C */
    uint32_t second_len;        /* +0x10 */
    uint8_t *second_buf;        /* +0x14 */
} Mp4LiciBox;

/* Sony USMT / MTDF / MTDT / MTSM metadata */
typedef struct Mp4MtdfEntry {
    uint16_t size;              /* +0x00 */
    uint16_t pad;               /* +0x02 */
    uint32_t tag;               /* +0x04 */
    uint16_t language;          /* +0x08 */
    uint16_t encoding;          /* +0x0A */
    uint8_t payload[32];        /* +0x0C */
} Mp4MtdfEntry; /* 44 bytes */

typedef struct Mp4MtdfBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint16_t entry_count;       /* +0x08 */
    uint16_t pad;               /* +0x0A */
    uint32_t capacity;          /* +0x0C */
    Mp4MtdfEntry *entries;      /* +0x10 */
} Mp4MtdfBox; /* 20 bytes */

typedef struct Mp4MtdtExtItem {
    uint32_t media_id;          /* +0x00 */
    uint32_t sample_index;      /* +0x04 */
    uint32_t item_flags;        /* +0x08 */
} Mp4MtdtExtItem; /* 12 bytes */

typedef struct Mp4MtdtExt {
    uint32_t head_magic;        /* +0x00 (entry+12) */
    uint32_t head_version;      /* +0x04 (entry+16) */
    uint32_t item_count;        /* +0x08 (entry+20) */
    Mp4MtdtExtItem *items;      /* +0x0C (entry+24) */
    struct Mp4MtsmBox *mtsm;    /* +0x10 (entry+28) */
    struct Mp4MtdtEntry *owner; /* +0x14 (entry+32) */
    uint8_t *raw_payload;       /* +0x18 (entry+36) */
} Mp4MtdtExt; /* 28 bytes */

typedef struct Mp4MtdtEntry {
    uint16_t size;              /* +0x00 */
    uint16_t pad;               /* +0x02 */
    uint32_t tag;               /* +0x04 */
    uint16_t language;          /* +0x08 */
    uint16_t encoding;          /* +0x0A */
    Mp4MtdtExt ext;             /* +0x0C..+0x27 */
} Mp4MtdtEntry; /* 40 bytes */

typedef struct Mp4MtdtBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint16_t entry_count;       /* +0x08 */
    uint16_t pad;               /* +0x0A */
    uint32_t capacity;          /* +0x0C */
    Mp4MtdtEntry *entries;      /* +0x10 */
} Mp4MtdtBox; /* 20 bytes */

/* DREF / DINF */
typedef struct Mp4DrefEntry {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    char *location;             /* +0x0C */
    uint16_t index;             /* +0x10 */
    uint16_t pad;               /* +0x12 */
    struct Mp4DrefEntry *next;  /* +0x14 */
} Mp4DrefEntry; /* 24 bytes */

typedef struct Mp4DrefBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t entry_count;       /* +0x0C */
    Mp4DrefEntry *head;         /* +0x10 */
    uint32_t reserved;          /* +0x14 */
} Mp4DrefBox; /* 24 bytes */

typedef struct Mp4DinfBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    Mp4DrefBox dref;            /* +0x08 */
} Mp4DinfBox; /* 32 bytes */

/* Sample entry codec descriptions */
typedef struct Mp4VisualDesc {
    uint16_t version;           /* +0x00 */
    uint16_t revision;          /* +0x02 */
    uint32_t vendor;            /* +0x04 */
    uint32_t temporal_quality;  /* +0x08 */
    uint32_t spatial_quality;   /* +0x0C */
    int16_t width;              /* +0x10 */
    int16_t height;             /* +0x12 */
    int32_t horiz_res;          /* +0x14 */
    int32_t vert_res;           /* +0x18 */
    uint32_t data_size;         /* +0x1C */
    uint16_t frame_count;       /* +0x20 */
    uint16_t pad;               /* +0x22 */
    uint8_t compressor[32];     /* +0x24 */
    int16_t depth;              /* +0x44 */
    int16_t color_table_id;     /* +0x46 */
} Mp4VisualDesc; /* 72 bytes */

typedef struct Mp4AudioDesc {
    uint16_t version;           /* +0x00 */
    uint16_t revision;          /* +0x02 */
    uint32_t vendor;            /* +0x04 */
    uint16_t channel_count;     /* +0x08 */
    uint16_t sample_size;       /* +0x0A */
    uint16_t compression_id;    /* +0x0C */
    uint16_t packet_size;       /* +0x0E */
    uint32_t sample_rate;       /* +0x10 */
    uint32_t samples_per_pkt;   /* +0x14 */
    uint32_t bytes_per_pkt;     /* +0x18 */
    uint32_t bytes_per_frame;   /* +0x1C */
    uint32_t bytes_per_sample;  /* +0x20 */
} Mp4AudioDesc; /* 36 bytes */

typedef struct Mp4HintDesc {
    uint16_t version;           /* +0x00 */
    uint16_t width;             /* +0x02 */
    uint16_t height;            /* +0x04 */
    uint16_t pad;               /* +0x06 */
    uint32_t horiz_res;         /* +0x08 */
    uint32_t vert_res;          /* +0x0C */
    uint16_t depth;             /* +0x10 */
    uint16_t color_id;          /* +0x12 */
    uint8_t bit_flags;          /* +0x14 */
    uint8_t tail_pad[3];        /* +0x15 */
} Mp4HintDesc; /* 24 bytes */

typedef struct Mp4EsdsBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint8_t *payload;           /* +0x0C */
} Mp4EsdsBox; /* 16 bytes */

typedef struct Mp4AvccNal {
    uint16_t length;            /* +0x00 */
    uint16_t pad;               /* +0x02 */
    uint8_t *nal_bytes;         /* +0x04 */
} Mp4AvccNal; /* 8 bytes */

typedef struct Mp4AvccBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t config_version;     /* +0x08 */
    uint8_t profile_idc;        /* +0x09 */
    uint8_t profile_compat;     /* +0x0A */
    uint8_t level_idc;          /* +0x0B */
    uint8_t reserved_len;       /* +0x0C */
    uint8_t length_size_minus;  /* +0x0D */
    uint8_t reserved_sps;       /* +0x0E */
    uint8_t sps_count;          /* +0x0F */
    Mp4DynArray *sps_array;     /* +0x10 */
    uint8_t pps_count;          /* +0x14 */
    uint8_t pad[3];             /* +0x15 */
    Mp4DynArray *pps_array;     /* +0x18 */
} Mp4AvccBox; /* 28 bytes */

typedef struct Mp4SinfBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint32_t payload_size;      /* +0x08 */
    uint8_t *payload;           /* +0x0C */
} Mp4SinfBox; /* 16 bytes */

typedef struct Mp4ExtraBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t *payload;           /* +0x08 */
} Mp4ExtraBox; /* 12 bytes */

typedef struct Mp4SampleEntry {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t reserved_bytes[6];  /* +0x08 */
    uint16_t data_ref_index;    /* +0x0E */
    uint32_t handler_type;      /* +0x10 */
    Mp4DrefEntry *data_ref;     /* +0x14 */
    uint32_t entry_index;       /* +0x18 */
    struct Mp4StsdBox *stsd;    /* +0x1C */
    struct Mp4SampleEntry *next;/* +0x20 */
    void *codec_desc;           /* +0x24 */
    Mp4EsdsBox *esds;           /* +0x28 */
    Mp4AvccBox *avcc;           /* +0x2C */
    Mp4SinfBox *sinf;           /* +0x30 */
    Mp4ExtraBox *extra[3];      /* +0x34 */
    uint32_t uuid_size;         /* +0x40 */
    uint8_t *uuid_payload;      /* +0x44 */
} Mp4SampleEntry; /* 72 bytes */

typedef struct Mp4StsdBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t entry_count;       /* +0x0C */
    Mp4SampleEntry *head;       /* +0x10 */
    struct Mp4Stbl *stbl;       /* +0x14 */
    uint32_t reserved;          /* +0x18 */
} Mp4StsdBox; /* 28 bytes */

/* MTSM sub-boxes */
typedef struct Mp4MthdBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t attr_a;            /* +0x0C */
    uint32_t attr_b;            /* +0x10 */
    uint32_t media_id;          /* +0x14 */
    uint32_t attr_c;            /* +0x18 */
    uint32_t attr_d;            /* +0x1C */
} Mp4MthdBox; /* 32 bytes */

typedef struct Mp4McstEntry {
    uint32_t sample_offset;     /* +0x00 */
    uint32_t sample_size;       /* +0x04 */
    uint32_t stsd_index;        /* +0x08 */
    uint32_t extra_a;           /* +0x0C */
    uint32_t extra_b;           /* +0x10 */
} Mp4McstEntry; /* 20 bytes */

typedef struct Mp4McstBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t entry_count;       /* +0x0C */
    Mp4McstEntry *entries;      /* +0x10 */
} Mp4McstBox; /* 20 bytes */

typedef struct Mp4MtsmBox {
    uint32_t size;              /* +0x00 (usmt+64) */
    uint32_t tag;               /* +0x04 (usmt+68) */
    Mp4MthdBox mthd;            /* +0x08 (usmt+72) */
    Mp4DinfBox dinf;            /* +0x28 (usmt+104) */
    Mp4StsdBox stsd;            /* +0x48 (usmt+136) */
    Mp4McstBox mcst;            /* +0x64 (usmt+164) */
} Mp4MtsmBox; /* 120 bytes */

typedef struct Mp4UsmtBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint32_t uuid[4];           /* +0x08 */
    Mp4MtdfBox mtdf;            /* +0x18 (24) */
    Mp4MtdtBox mtdt;            /* +0x2C (44) */
    Mp4MtsmBox mtsm;            /* +0x40 (64) */
} Mp4UsmtBox; /* 184 bytes */

typedef struct Mp4UdtaItem {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint32_t payload_size;      /* +0x08 */
    uint8_t *payload;           /* +0x0C */
} Mp4UdtaItem; /* 16 bytes */

typedef struct Mp4UdtaBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint32_t item_count;        /* +0x08 */
    Mp4UdtaItem *items;         /* +0x0C */
} Mp4UdtaBox; /* 16 bytes */

/* TREF / TREF sub-box */
typedef struct Mp4TrefTypeBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint16_t count;             /* +0x08 */
    uint16_t capacity;          /* +0x0A */
    uint32_t *track_ids;        /* +0x0C */
} Mp4TrefTypeBox; /* 16 bytes */

typedef struct Mp4TrefBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    Mp4TrefTypeBox *sync_ref;   /* +0x08 */
    Mp4TrefTypeBox *mpod_ref;   /* +0x0C */
} Mp4TrefBox; /* 16 bytes */

/* Track header and edit list */
typedef struct Mp4TkhdBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t creation_time;     /* +0x0C */
    uint32_t modification_time; /* +0x10 */
    uint32_t track_id;          /* +0x14 */
    uint32_t reserved_a;        /* +0x18 */
    uint32_t duration;          /* +0x1C */
    uint32_t reserved_b[2];     /* +0x20 */
    int16_t layer;              /* +0x28 */
    int16_t alternate_group;    /* +0x2A */
    int16_t volume;             /* +0x2C */
    uint16_t reserved_short;    /* +0x2E */
    int32_t matrix[9];          /* +0x30 */
    uint32_t width;             /* +0x54 */
    uint32_t height;            /* +0x58 */
} Mp4TkhdBox; /* 92 bytes */

typedef struct Mp4ElstBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t entry_count;       /* +0x0C */
    Mp4ChunkArray *entries;     /* +0x10 */
} Mp4ElstBox; /* 20 bytes */

typedef struct Mp4EdtsBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    Mp4ElstBox elst;            /* +0x08 */
} Mp4EdtsBox; /* 28 bytes */

/* Media header, handler, media info headers */
typedef struct Mp4MdhdBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t creation_time;     /* +0x0C */
    uint32_t modification_time; /* +0x10 */
    uint32_t timescale;         /* +0x14 */
    uint32_t duration;          /* +0x18 */
    uint16_t language;          /* +0x1C */
    uint16_t predefined;        /* +0x1E */
} Mp4MdhdBox; /* 32 bytes */

typedef struct Mp4HdlrBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t predefined;        /* +0x0C */
    uint32_t handler_type;      /* +0x10 */
    uint32_t reserved[3];       /* +0x14 */
    char name[64];              /* +0x20 */
    uint8_t is_c_string;        /* +0x60 */
    uint8_t pad[3];             /* +0x61 */
} Mp4HdlrBox; /* 100 bytes */

typedef struct Mp4VmhdBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    int16_t graphics_mode;      /* +0x0C */
    int16_t opcolor[3];         /* +0x0E */
} Mp4VmhdBox; /* 20 bytes */

typedef struct Mp4SmhdBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    int16_t balance;            /* +0x0C */
    uint16_t reserved;          /* +0x0E */
} Mp4SmhdBox; /* 16 bytes */

typedef struct Mp4NmhdBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
} Mp4NmhdBox; /* 12 bytes */

/* In-memory sample table sub-boxes (each 36 bytes) */
typedef struct Mp4SttsBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t reserved_a;        /* +0x0C */
    uint32_t entry_count;       /* +0x10 */
    Mp4ChunkArray *entries;     /* +0x14 */
    uint32_t cursor_entry;      /* +0x18 */
    uint32_t cursor_sample;     /* +0x1C */
    uint32_t cursor_time;       /* +0x20 */
} Mp4SttsBox; /* 36 bytes */

typedef struct Mp4StssBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t reserved_a;        /* +0x0C */
    uint32_t entry_count;       /* +0x10 */
    Mp4ChunkArray *entries;     /* +0x14 */
    uint32_t cursor_entry;      /* +0x18 */
    uint32_t cursor_sample;     /* +0x1C */
    uint32_t reserved_b;        /* +0x20 */
} Mp4StssBox; /* 36 bytes */

typedef struct Mp4StscBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t reserved_a;        /* +0x0C */
    uint32_t entry_count;       /* +0x10 */
    Mp4ChunkArray *entries;     /* +0x14 */
    uint32_t cursor_entry;      /* +0x18 */
    uint32_t cursor_sample;     /* +0x1C */
    uint32_t reserved_b;        /* +0x20 */
} Mp4StscBox; /* 36 bytes */

typedef struct Mp4StszBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t const_sample_size; /* +0x0C */
    uint32_t sample_count;      /* +0x10 */
    Mp4ChunkArray *entries;     /* +0x14 */
    uint32_t reserved_a;        /* +0x18 */
    uint32_t reserved_b;        /* +0x1C */
    uint32_t reserved_c;        /* +0x20 */
} Mp4StszBox; /* 36 bytes */

typedef struct Mp4StcoBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t reserved_a;        /* +0x0C */
    uint32_t entry_count;       /* +0x10 */
    Mp4ChunkArray *entries;     /* +0x14 */
    uint32_t reserved_b;        /* +0x18 */
    uint32_t reserved_c;        /* +0x1C */
    uint32_t reserved_d;        /* +0x20 */
} Mp4StcoBox; /* 36 bytes */

typedef struct Mp4CttsBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t reserved_a;        /* +0x0C */
    uint32_t entry_count;       /* +0x10 */
    Mp4ChunkArray *entries;     /* +0x14 */
    uint32_t cursor_entry;      /* +0x18 */
    uint32_t cursor_sample;     /* +0x1C */
    uint32_t reserved_b;        /* +0x20 */
} Mp4CttsBox; /* 36 bytes */

/* Fragment playback context attached to Mp4Stbl (72 bytes) */
typedef struct Mp4StblFragCtx {
    uint32_t base_sample;       /* +0x00 (v[0]) */
    uint32_t base_chunk;        /* +0x04 (v[1]) */
    uint32_t base_time;         /* +0x08 (v[2]) */
    uint32_t loaded_duration;   /* +0x0C (v[3]) */
    uint32_t frag_boundary;     /* +0x10 (v[4]) */
    uint32_t cur_read_sample;   /* +0x14 (v[5]) */
    uint64_t next_moof_offset;  /* +0x18 (v[6..7]) */
    Mp4ChunkArray *frag_marks;  /* +0x20 (v[8]) */
    uint32_t loaded_frags;      /* +0x24 (v[9]) */
    uint32_t is_complete;       /* +0x28 (v[10]) */
    uint32_t total_samples;     /* +0x2C (v[11]) */
    uint32_t total_duration;    /* +0x30 (v[12]) */
    Mp4IoStream *stream;        /* +0x34 (v[13]) */
    uint32_t min_sample;        /* +0x38 (v[14]) */
    uint32_t min_time;          /* +0x3C (v[15]) */
    uint32_t min_chunk;         /* +0x40 (v[16]) */
    uint32_t reserved;          /* +0x44 (v[17]) */
} Mp4StblFragCtx; /* 72 bytes */

/* In-memory sample table (288 bytes) */
typedef struct Mp4Stbl {
    uint32_t frag_seq_num;      /* +0x00 */
    uint32_t frag_base_time;    /* +0x04 */
    uint64_t frag_moof_offset;  /* +0x08 */
    uint32_t size;              /* +0x10 */
    uint32_t tag;               /* +0x14 */
    Mp4StsdBox stsd;            /* +0x18 */
    Mp4SttsBox stts;            /* +0x34 */
    Mp4StssBox stss;            /* +0x58 */
    Mp4StscBox stsc;            /* +0x7C */
    Mp4StszBox stsz;            /* +0xA0 */
    Mp4StcoBox stco;            /* +0xC4 */
    Mp4CttsBox ctts;            /* +0xE8 */
    uint8_t chunk_open;         /* +0x10C */
    uint8_t pad[3];             /* +0x10D */
    Mp4SampleEntry *first_desc; /* +0x110 */
    struct Mp4Track *track;     /* +0x114 */
    Mp4StblFragCtx *frag_ctx;   /* +0x118 */
    struct Mp4CacheStbl *cache; /* +0x11C */
} Mp4Stbl; /* 288 bytes */

/* Media information box (384 bytes: starts at track+304, ends at track+688) */
typedef struct Mp4MinfBox {
    uint32_t size;              /* +0x00 (track+304) */
    uint32_t tag;               /* +0x04 (track+308) */
    Mp4VmhdBox vmhd;            /* +0x08 (track+312) */
    Mp4SmhdBox smhd;            /* +0x1C (track+332) */
    Mp4NmhdBox nmhd;            /* +0x2C (track+348) */
    Mp4DinfBox dinf;            /* +0x38 (track+360) */
    Mp4Stbl stbl;               /* +0x58 (track+392) */
    Mp4Stbl *active_stbl;       /* +0x178 (track+680) */
    uint32_t reserved;          /* +0x17C (track+684) */
} Mp4MinfBox; /* 384 bytes */

/* Media box (536 bytes: starts at track+152, ends at track+688) */
typedef struct Mp4MdiaBox {
    uint32_t size;              /* +0x00 (track+152) */
    uint32_t tag;               /* +0x04 (track+156) */
    Mp4MdhdBox mdhd;            /* +0x08 (track+160) */
    uint32_t reserved_pad[2];   /* +0x28 (track+192) */
    Mp4HdlrBox hdlr;            /* +0x30 (track+200) */
    uint32_t reserved_tail;     /* +0x94 (track+300) */
    Mp4MinfBox minf;            /* +0x98 (track+304) */
} Mp4MdiaBox; /* 536 bytes */

/* Track box (728 bytes) */
typedef struct Mp4Track {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    struct Mp4Track *next;      /* +0x08 */
    Mp4TkhdBox tkhd;            /* +0x0C (12..103) */
    uint32_t movie_timescale;   /* +0x68 (104) */
    uint32_t reserved_a[4];     /* +0x6C (108..123) */
    Mp4EdtsBox edts;            /* +0x7C (124..151) */
    Mp4MdiaBox mdia;            /* +0x98 (152..687) */
    Mp4UsmtBox *usmt;           /* +0x2B0 (688) */
    Mp4UdtaBox *udta;           /* +0x2B4 (692) */
    Mp4TrefBox *tref;           /* +0x2B8 (696) */
    struct Mp4Movie *movie;     /* +0x2BC (700) */
    uint32_t reserved_b[4];     /* +0x2C0 (704..719) */
    void *odsm_ctx;             /* +0x2D0 (720) */
    uint32_t odsm_mode;         /* +0x2D4 (724) */
} Mp4Track; /* 728 bytes */

/* Fragment boxes: MVEX / TREX, MOOF / MFHD / TRAF / TFHD / TRUN, MFRA / TFRA / MFRO */
typedef struct Mp4TrexBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t track_id;          /* +0x0C */
    uint32_t def_sample_desc;   /* +0x10 */
    uint32_t def_sample_dur;    /* +0x14 */
    uint32_t def_sample_size;   /* +0x18 */
    uint32_t def_sample_flags;  /* +0x1C */
} Mp4TrexBox; /* 32 bytes */

typedef struct Mp4MehdBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t fragment_duration; /* +0x0C */
} Mp4MehdBox; /* 16 bytes */

typedef struct Mp4MvexBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    Mp4MehdBox mehd;            /* +0x08 */
    uint32_t trex_count;        /* +0x18 */
    Mp4TrexBox *trex_list[127]; /* +0x1C */
} Mp4MvexBox; /* 536 bytes */

typedef struct Mp4TrunBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t sample_count;      /* +0x0C */
    int32_t data_offset;        /* +0x10 */
    uint32_t first_sample_flags;/* +0x14 */
    uint32_t reserved_a;        /* +0x18 */
    Mp4ChunkArray *entries;     /* +0x1C */
    struct Mp4TrunBox *next;    /* +0x20 */
    uint32_t reserved_b;        /* +0x24 */
    uint64_t resolved_offset;   /* +0x28 */
} Mp4TrunBox; /* 48 bytes */

typedef struct Mp4TfhdBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t track_id;          /* +0x0C */
    uint32_t base_offset_hi;    /* +0x10 */
    uint32_t base_offset_lo;    /* +0x14 */
    uint32_t sample_desc_idx;   /* +0x18 */
    uint32_t def_sample_dur;    /* +0x1C */
    uint32_t def_sample_size;   /* +0x20 */
    uint32_t def_sample_flags;  /* +0x24 */
    uint32_t reserved;          /* +0x28 */
} Mp4TfhdBox; /* 44 bytes */

typedef struct Mp4TrafBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    Mp4TfhdBox tfhd;            /* +0x08 */
    uint32_t trun_count;        /* +0x34 */
    Mp4TrunBox *trun_head;      /* +0x38 */
    uint32_t total_samples;     /* +0x3C */
    uint32_t reserved_a;        /* +0x40 */
    Mp4TrunBox *cached_trun;    /* +0x44 */
    uint32_t cached_rel_idx;    /* +0x48 */
    uint32_t cached_sample_idx; /* +0x4C */
    uint64_t cached_offset;     /* +0x50 */
    uint32_t reserved_tail[2];  /* +0x58 */
} Mp4TrafBox; /* 96 bytes */

typedef struct Mp4MfhdBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t seq_number;        /* +0x0C */
} Mp4MfhdBox; /* 16 bytes */

typedef struct Mp4MoofBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    Mp4MfhdBox mfhd;            /* +0x08 */
    uint32_t traf_count;        /* +0x18 */
    Mp4TrafBox *traf_list[127]; /* +0x1C..+0x217 */
    struct Mp4MoofBox *next;    /* +0x218 (536) */
    struct Mp4Movie *movie;     /* +0x21C (540) */
    uint64_t file_offset;       /* +0x220 (544) */
    uint32_t evictable;         /* +0x228 (552) */
    uint32_t reserved;          /* +0x22C (556) */
    uint64_t next_moof_offset;  /* +0x230 (560) */
} Mp4MoofBox; /* 568 bytes */

typedef struct Mp4TfraEntry {
    uint32_t time_hi;           /* +0x00 */
    uint32_t time_lo;           /* +0x04 */
    uint64_t moof_offset;       /* +0x08 */
    uint32_t traf_number;       /* +0x10 */
    uint32_t trun_number;       /* +0x14 */
    uint32_t sample_number;     /* +0x18 */
    uint32_t reserved;          /* +0x1C */
} Mp4TfraEntry; /* 32 bytes */

typedef struct Mp4TfraBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t track_id;          /* +0x0C */
    uint8_t traf_len_size;      /* +0x10 */
    uint8_t trun_len_size;      /* +0x11 */
    uint8_t sample_len_size;    /* +0x12 */
    uint8_t pad;                /* +0x13 */
    uint32_t entry_count;       /* +0x14 */
    Mp4DynArray *entries;       /* +0x18 */
    uint32_t reserved_words[4]; /* +0x1C..+0x2B */
} Mp4TfraBox; /* 44 bytes */

typedef struct Mp4MfroBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t parent_size;       /* +0x0C */
} Mp4MfroBox; /* 16 bytes */

typedef struct Mp4MfraOther {
    uint32_t seq_id;            /* +0x00 */
    Mp4DynArray *entries;       /* +0x04 */
} Mp4MfraOther; /* 8 bytes */

typedef struct Mp4MfraBox {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    Mp4MfroBox *mfro;           /* +0x08 */
    Mp4DynArray *tfra_list;     /* +0x0C */
    Mp4DynArray *other_list;    /* +0x10 */
    uint32_t reserved_a;        /* +0x14 */
    uint32_t incomplete_flag;   /* +0x18 */
} Mp4MfraBox; /* 28 bytes */

/* Top-level movie structure (336 bytes) */
typedef struct Mp4Movie {
    uint32_t size;              /* +0x00 */
    uint32_t tag;               /* +0x04 */
    uint32_t track_count;       /* +0x08 */
    Mp4MvhdBox mvhd;            /* +0x0C */
    uint32_t reserved_pad[2];   /* +0x78 */
    Mp4Track *tracks_head;      /* +0x80 */
    Mp4IodsBox *iods;           /* +0x84 */
    Mp4MvexBox *mvex;           /* +0x88 */
    uint32_t reserved_align;    /* +0x8C */
    uint64_t first_moof_offset; /* +0x90 */
    struct Mp4Movie *self;      /* +0x98 */
    uint32_t reserved_align_b;  /* +0x9C */
    uint64_t moov_offset;       /* +0xA0 */
    Mp4UsmtBox *usmt;           /* +0xA8 */
    Mp4UdtaBox *udta;           /* +0xAC */
    uint32_t reserved_c[2];     /* +0xB0 */
    Mp4ProfBox prof;            /* +0xB8 */
    Mp4FtypBox ftyp;            /* +0xEC */
    uint32_t moof_count;        /* +0x104 */
    Mp4MoofBox *moof_head;      /* +0x108 */
    uint32_t reserved_d;        /* +0x10C */
    uint32_t parsed_flag;       /* +0x110 */
    Mp4Handle *handle;          /* +0x114 */
    Mp4MfraBox *mfra;           /* +0x118 */
    uint32_t options;           /* +0x11C */
    Mp4LiciBox lici;            /* +0x120 */
    uint64_t mtsd_offset;       /* +0x138 */
    uint64_t mtsd_size;         /* +0x140 */
    Mp4UuidNode *uuid_head;     /* +0x148 */
    uint32_t reserved_tail;     /* +0x14C */
} Mp4Movie; /* 336 bytes */

/* Paged streaming sample table cache structs */
typedef struct Mp4CacheBlock {
    uint32_t count;             /* +0x00 */
    uint32_t capacity;          /* +0x04 */
    uint32_t cols;              /* +0x08 */
    uint32_t *words;            /* +0x0C */
    uint32_t first_sample;      /* +0x10 */
    uint32_t first_time;        /* +0x14 */
    uint32_t first_chunk;       /* +0x18 */
    struct Mp4CacheBlock *next; /* +0x1C */
} Mp4CacheBlock; /* 32 bytes */

typedef struct Mp4CacheStts {
    uint32_t size;              /* +0x00 (w[0]) */
    uint32_t tag;               /* +0x04 (w[1]) */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t reserved_a;        /* +0x0C (w[3]) */
    uint32_t entry_count;       /* +0x10 (w[4]) */
    uint32_t reserved_align;    /* +0x14 (w[5]) */
    uint64_t file_offset;       /* +0x18 (w[6..7]) */
    Mp4CacheBlock *active;      /* +0x20 (w[8]) */
    Mp4CacheBlock *free_pool;   /* +0x24 (w[9]) */
    uint32_t win_entry_start;   /* +0x28 (w[10]) */
    uint32_t win_entry_count;   /* +0x2C (w[11]) */
    uint32_t win_sample_start;  /* +0x30 (w[12]) */
    uint32_t win_sample_count;  /* +0x34 (w[13]) */
    uint32_t win_time_start;    /* +0x38 (w[14]) */
    uint32_t win_time_count;    /* +0x3C (w[15]) */
    uint32_t cur_entry;         /* +0x40 (w[16]) */
    uint32_t cur_sample;        /* +0x44 (w[17]) */
    uint32_t cur_time;          /* +0x48 (w[18]) */
    uint32_t reserved_tail[3];  /* +0x4C (w[19..21]) */
} Mp4CacheStts; /* 88 bytes */

typedef struct Mp4CacheStsc {
    uint32_t size;              /* +0x00 (w[0]) */
    uint32_t tag;               /* +0x04 (w[1]) */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t reserved_a;        /* +0x0C (w[3]) */
    uint32_t entry_count;       /* +0x10 (w[4]) */
    uint32_t reserved_align;    /* +0x14 (w[5]) */
    uint64_t file_offset;       /* +0x18 (w[6..7]) */
    Mp4CacheBlock *active;      /* +0x20 (w[8]) */
    Mp4CacheBlock *free_pool;   /* +0x24 (w[9]) */
    uint32_t win_entry_start;   /* +0x28 (w[10]) */
    uint32_t win_entry_count;   /* +0x2C (w[11]) */
    uint32_t win_sample_start;  /* +0x30 (w[12]) */
    uint32_t win_sample_count;  /* +0x34 (w[13]) */
    uint32_t win_chunk_start;   /* +0x38 (w[14]) */
    uint32_t win_chunk_count;   /* +0x3C (w[15]) */
    uint32_t cur_entry;         /* +0x40 (w[16]) */
    uint32_t cur_sample;        /* +0x44 (w[17]) */
    uint32_t tail_resolved;     /* +0x48 (w[18]) */
    uint32_t reserved_tail[5];  /* +0x4C (w[19..23]) */
} Mp4CacheStsc; /* 96 bytes */

typedef struct Mp4CacheStsz {
    uint32_t size;              /* +0x00 (w[0]) */
    uint32_t tag;               /* +0x04 (w[1]) */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t const_sample_size; /* +0x0C (w[3]) */
    uint32_t sample_count;      /* +0x10 (w[4]) */
    uint32_t reserved_align;    /* +0x14 (w[5]) */
    uint64_t file_offset;       /* +0x18 (w[6..7]) */
    Mp4CacheBlock *active;      /* +0x20 (w[8]) */
    Mp4CacheBlock *free_pool;   /* +0x24 (w[9]) */
    uint32_t win_sample_start;  /* +0x28 (w[10]) */
    uint32_t win_sample_count;  /* +0x2C (w[11]) */
    uint32_t reserved_tail[2];  /* +0x30 (w[12..13]) */
} Mp4CacheStsz; /* 56 bytes */

typedef struct Mp4CacheStco {
    uint32_t size;              /* +0x00 (w[0]) */
    uint32_t tag;               /* +0x04 (w[1]) */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t reserved_a;        /* +0x0C (w[3]) */
    uint32_t entry_count;       /* +0x10 (w[4]) */
    uint32_t reserved_align;    /* +0x14 (w[5]) */
    uint64_t file_offset;       /* +0x18 (w[6..7]) */
    Mp4CacheBlock *active;      /* +0x20 (w[8]) */
    Mp4CacheBlock *free_pool;   /* +0x24 (w[9]) */
    uint32_t win_chunk_start;   /* +0x28 (w[10]) */
    uint32_t win_chunk_count;   /* +0x2C (w[11]) */
    uint32_t reserved_tail[2];  /* +0x30 (w[12..13]) */
} Mp4CacheStco; /* 56 bytes */

typedef struct Mp4CacheStss {
    uint32_t size;              /* +0x00 (w[0]) */
    uint32_t tag;               /* +0x04 (w[1]) */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t reserved_a;        /* +0x0C (w[3]) */
    uint32_t entry_count;       /* +0x10 (w[4]) */
    uint32_t reserved_align;    /* +0x14 (w[5]) */
    uint64_t file_offset;       /* +0x18 (w[6..7]) */
    Mp4CacheBlock *active;      /* +0x20 (w[8]) */
    Mp4CacheBlock *free_pool;   /* +0x24 (w[9]) */
    uint32_t win_entry_start;   /* +0x28 (w[10]) */
    uint32_t win_entry_count;   /* +0x2C (w[11]) */
    uint32_t win_sample_start;  /* +0x30 (w[12]) */
    uint32_t win_sample_count;  /* +0x34 (w[13]) */
    uint32_t cur_entry;         /* +0x38 (w[14]) */
    uint32_t cur_sample;        /* +0x3C (w[15]) */
    uint32_t tail_resolved;     /* +0x40 (w[16]) */
    uint32_t reserved_tail;     /* +0x44 (w[17]) */
} Mp4CacheStss; /* 72 bytes */

typedef struct Mp4CacheCtts {
    uint32_t size;              /* +0x00 (w[0]) */
    uint32_t tag;               /* +0x04 (w[1]) */
    uint8_t version;            /* +0x08 */
    uint8_t flags[3];           /* +0x09 */
    uint32_t reserved_a;        /* +0x0C (w[3]) */
    uint32_t entry_count;       /* +0x10 (w[4]) */
    uint32_t reserved_align;    /* +0x14 (w[5]) */
    uint64_t file_offset;       /* +0x18 (w[6..7]) */
    Mp4CacheBlock *active;      /* +0x20 (w[8]) */
    Mp4CacheBlock *free_pool;   /* +0x24 (w[9]) */
    uint32_t win_entry_start;   /* +0x28 (w[10]) */
    uint32_t win_entry_count;   /* +0x2C (w[11]) */
    uint32_t win_sample_start;  /* +0x30 (w[12]) */
    uint32_t win_sample_count;  /* +0x34 (w[13]) */
    uint32_t cur_entry;         /* +0x38 (w[14]) */
    uint32_t cur_sample;        /* +0x3C (w[15]) */
    uint32_t reserved_tail[2];  /* +0x40 (w[16..17]) */
} Mp4CacheCtts; /* 72 bytes */

typedef struct Mp4CacheCtrl {
    uint32_t win_sample_start;  /* +0x00 (w[0]) */
    uint32_t win_sample_count;  /* +0x04 (w[1]) */
    uint32_t win_chunk_start;   /* +0x08 (w[2]) */
    uint32_t win_chunk_count;   /* +0x0C (w[3]) */
    uint32_t win_time_start;    /* +0x10 (w[4]) */
    uint32_t win_time_count;    /* +0x14 (w[5]) */
    uint32_t chunk_prefix_bytes;/* +0x18 (w[6]) */
    uint32_t reserved_a;        /* +0x1C (w[7]) */
    uint32_t total_samples;     /* +0x20 (w[8]) */
    uint32_t total_duration;    /* +0x24 (w[9]) */
    uint32_t total_chunks;      /* +0x28 (w[10]) */
    Mp4IoStream *stream;        /* +0x2C (w[11]) */
} Mp4CacheCtrl; /* 48 bytes */

typedef struct Mp4CacheStbl {
    uint32_t size;              /* +0x00 (w[0]) */
    uint32_t tag;               /* +0x04 (w[1]) */
    Mp4CacheStts stts;          /* +0x08 (w[2..23]) */
    Mp4CacheStsc stsc;          /* +0x60 (w[24..47]) */
    Mp4CacheStsz stsz;          /* +0xC0 (w[48..61]) */
    Mp4CacheStco stco;          /* +0xF8 (w[62..75]) */
    Mp4CacheStss stss;          /* +0x130 (w[76..93]) */
    Mp4CacheCtts ctts;          /* +0x178 (w[94..111]) */
    Mp4Track *track;            /* +0x1C0 (w[112]) */
    uint8_t initialized;        /* +0x1C4 (w[113]) */
    uint8_t pad[3];             /* +0x1C5 */
    uint32_t reserved;          /* +0x1C8 (w[114]) */
    Mp4CacheCtrl *ctrl;         /* +0x1CC (w[115]) */
} Mp4CacheStbl; /* 464 bytes */

/* Sample query output structs */
typedef struct Mp4SampleInfo {
    uint32_t size;              /* +0x00 */
    uint32_t duration;          /* +0x04 */
    uint64_t offset;            /* +0x08 */
    uint32_t is_sync;           /* +0x10 */
    uint32_t ctts_offset;       /* +0x14 */
    uint32_t chunk_rem_bytes;   /* +0x18 */
    uint32_t chunk_rem_dur;     /* +0x1C */
    uint32_t chunk_rem_samples; /* +0x20 */
    uint32_t sample_desc;       /* +0x24 */
} Mp4SampleInfo; /* 40 bytes */

typedef struct Mp4TrackInfo {
    uint32_t track_id;          /* +0x00 */
    uint32_t duration;          /* +0x04 */
    uint32_t enabled;           /* +0x08 */
    uint32_t width;             /* +0x0C */
    uint32_t height;            /* +0x10 */
    int16_t layer;              /* +0x14 */
    uint16_t language;          /* +0x16 */
    int32_t alternate_group;    /* +0x18 */
} Mp4TrackInfo; /* 28 bytes */

typedef struct Mp4AudioSampleDescOut {
    uint32_t codec_tag;         /* +0x00 */
    uint32_t reserved;          /* +0x04 */
    Mp4DrefEntry *data_ref;     /* +0x08 */
    uint32_t vendor;            /* +0x0C */
    uint16_t channel_count;     /* +0x10 */
    uint16_t sample_size;       /* +0x12 */
    uint32_t sample_rate;       /* +0x14 */
    uint32_t samples_per_pkt;   /* +0x18 */
    uint32_t bytes_per_sample;  /* +0x1C */
} Mp4AudioSampleDescOut;

typedef struct Mp4AudioBriefOut {
    uint32_t codec_tag;         /* +0x00 */
    Mp4DrefEntry *data_ref;     /* +0x04 */
    uint16_t channel_count;     /* +0x08 */
    uint16_t pad;               /* +0x0A */
    uint32_t sample_rate;       /* +0x0C */
} Mp4AudioBriefOut;

typedef struct Mp4VisualSampleDescOut {
    uint32_t codec_tag;         /* +0x00 */
    uint32_t reserved;          /* +0x04 */
    Mp4DrefEntry *data_ref;     /* +0x08 */
    uint32_t vendor;            /* +0x0C */
    int32_t horiz_res;          /* +0x10 */
    int32_t vert_res;           /* +0x14 */
    int16_t width;              /* +0x18 */
    int16_t height;             /* +0x1A */
    uint8_t compressor[32];     /* +0x1C */
    int16_t depth;              /* +0x3C */
    uint16_t pad;               /* +0x3E */
} Mp4VisualSampleDescOut;

typedef struct Mp4VisualBriefOut {
    uint32_t codec_tag;         /* +0x00 */
    Mp4DrefEntry *data_ref;     /* +0x04 */
    int16_t width;              /* +0x08 */
    int16_t height;             /* +0x0A */
    int32_t horiz_res;          /* +0x0C */
    int32_t vert_res;           /* +0x10 */
} Mp4VisualBriefOut;

typedef struct Mp4HintSampleDescOut {
    uint32_t codec_tag;         /* +0x00 */
    Mp4DrefEntry *data_ref;     /* +0x04 */
    uint16_t version;           /* +0x08 */
    uint16_t width;             /* +0x0A */
    uint16_t height;            /* +0x0C */
    uint16_t pad;               /* +0x0E */
    uint32_t horiz_res;         /* +0x10 */
    uint32_t vert_res;          /* +0x14 */
    uint16_t depth;             /* +0x18 */
    uint16_t color_id;          /* +0x1A */
    uint8_t bit_flags;          /* +0x1C */
    uint8_t tail_pad[3];        /* +0x1D */
} Mp4HintSampleDescOut;

typedef struct Mp4MtsmImageInfo {
    uint32_t codec_tag;         /* +0x00 */
    Mp4DrefEntry *data_ref;     /* +0x04 */
    int32_t width;              /* +0x08 */
    int32_t height;             /* +0x0C */
    int32_t horiz_res;          /* +0x10 */
    int32_t vert_res;           /* +0x14 */
} Mp4MtsmImageInfo;

typedef struct Mp4EsConfigOut {
    uint8_t object_type_ind;    /* +0x00 */
    uint8_t stream_type_byte;   /* +0x01 */
    uint16_t pad;               /* +0x02 */
    uint32_t buffer_size_db;    /* +0x04 */
    uint32_t max_bitrate;       /* +0x08 */
    uint32_t avg_bitrate;       /* +0x0C */
    uint32_t dec_specific_size; /* +0x10 */
} Mp4EsConfigOut;

typedef struct Mp4OdsmTrackInfo {
    Mp4Track *track;            /* +0x00 */
    uint8_t es_index_count;     /* +0x04 */
    uint8_t pad[3];             /* +0x05 */
} Mp4OdsmTrackInfo;

/* Global state defined in mp4_api.c */
extern Mp4Callbacks g_mp4_callbacks;
extern Mp4Config g_mp4_config;
extern const Mp4StateTrans g_mp4_state_table[18][10];
extern const int32_t g_mp4_err_table[10];

/* ========================================================================= */
/* Section 1: mp4_api.c (0x000000..0x002a3c)                                 */
/* ========================================================================= */
int32_t mp4msv_9CA13D1A(int32_t count, const Mp4Callbacks *cbs);
int32_t mp4msv_2A2D949A(void);
Mp4Config *mp4msv_4A7AD789(void);
int32_t mp4msv_3C2183C7(int32_t count, const Mp4Config *cfg);

int64_t mp4CbTell(void *fh);
int32_t mp4CbSeek(void *fh, int64_t offset, int16_t origin);
int64_t mp4CbGetSize(void *fh);
int32_t mp4CbRead(void *dst, uint32_t size, void *fh);
int32_t mp4CbReadBe(void *dst, uint32_t size, void *fh);
void *mp4CbMalloc(uint32_t size);
void *mp4CbCalloc(uint32_t size);
void mp4CbFree(void *ptr);
int32_t mp4CbMemcpy(void *dst, uint32_t dst_size, const void *src, uint32_t len);
int32_t mp4CbStrncpy(char *dst, const char *src, uint32_t len);
uint32_t mp4CbStrlen(const char *str);

int32_t mp4CloseMovie(Mp4Handle *handle);
int32_t mp4FsmCreate(Mp4Fsm **out_fsm);
int32_t mp4FsmDestroy(Mp4Fsm *fsm);
int32_t mp4FsmBegin(Mp4Fsm *fsm, int32_t action);
int32_t mp4FsmCommit(Mp4Fsm *fsm);
int32_t mp4FsmSwitchToFrag(Mp4Fsm *fsm);
int32_t mp4FsmIsPendingCache(const Mp4Fsm *fsm);
int32_t mp4FsmIsFragReadOnly(const Mp4Fsm *fsm);
int32_t mp4FsmIsCacheMode(const Mp4Fsm *fsm);
int32_t mp4SecToDateTime(uint32_t seconds, Mp4DateTime *out_dt);
int32_t mp4SafeMemcpy(void *dst, uint32_t dst_size, const void *src, uint32_t len);
int32_t mp4SeekAbs(void *fh, uint64_t offset);
int32_t mp4HasCompatBrand(Mp4Handle *handle, uint32_t brand, uint32_t *out_found);
int32_t mp4FindSampleByTime(Mp4Track *track, uint32_t time_val, uint32_t *out_sample, uint32_t flags, uint32_t *out_sync, uint32_t *out_rem);
int32_t mp4FindSyncSample(Mp4Track *track, uint32_t sample_num, int32_t direction, uint32_t *out_sample, uint32_t *out_delta);
int32_t mp4GetSampleInfo(Mp4Track *track, uint32_t sample_num, int32_t want_chunk_rem, Mp4SampleInfo *out_info);
int32_t mp4GetSampleTime(Mp4Track *track, uint32_t sample_num, uint32_t *out_time, uint32_t *out_dur);

/* ========================================================================= */
/* Section 2: mp4_movie.c (0x002aa4..0x005e0c)                               */
/* ========================================================================= */
int32_t mp4FindFragSampleByTime(Mp4Track *track, uint32_t time_val, uint32_t *out_sample, uint32_t *out_sync, uint32_t *out_rem);
int32_t mp4PrefetchCacheWindow(Mp4Track *track, uint32_t start_sample, uint32_t count, uint32_t *out_start, uint32_t *out_count);
int32_t mp4CheckMoovValid(Mp4Handle *handle);
int32_t mp4TrackCheckValid(Mp4Track *track, uint32_t sample_num, int32_t mode, uint32_t *out_val);
int32_t mp4GetTrackFragBaseTime(Mp4Track *track, uint32_t *out_time);
int32_t mp4GetTrackCacheRange(Mp4Track *track, uint32_t *out_start, uint32_t *out_count);
int32_t mp4LoadNextTrackFrag(Mp4Track *track);
int32_t mp4SeekTrackFragByTime(Mp4Track *track, uint32_t target_time);
int32_t mp4LoadPrevTrackFrag(Mp4Track *track);
int32_t mp4OpenMovieFull(Mp4Handle *handle, uint32_t *out_flags, int32_t mode);
int32_t mp4OpenMovieBasic(Mp4Handle *handle, uint32_t *out_flags);
int32_t mp4OpenMoviePartial(Mp4Handle *handle, uint32_t mask, uint32_t *out_flags);
int32_t mp4GetTrackByIndex(Mp4Handle *handle, uint16_t index, Mp4Track **out_track);
int32_t mp4GetTrackHeaderInfo(Mp4Track *track, Mp4TrackInfo *out_info);
int32_t mp4GetMatrix(Mp4Handle *handle, Mp4Track *track, int32_t *out_matrix);
int32_t mp4GetTrefCount(Mp4Track *track, uint32_t ref_type, uint32_t *out_count);
int32_t mp4GetTrefTrack(Mp4Track *track, uint32_t ref_type, uint32_t index, Mp4Track **out_track);
int32_t mp4GetElstEntry(Mp4Track *track, uint32_t index, uint32_t *out_dur, uint32_t *out_time, uint32_t *out_rate);
int32_t mp4GetTrackDuration(Mp4Track *track, uint32_t *out_dur);
int32_t mp4GetSampleSize(Mp4Track *track, uint32_t sample_num, uint32_t *out_size);
int32_t mp4GetSampleDescByIndex(Mp4Track *track, uint32_t index, Mp4SampleEntry **out_entry);
int32_t mp4GetAudioSampleDesc(Mp4SampleEntry *entry, uint16_t *out_ver, Mp4AudioSampleDescOut *out_desc);
int32_t mp4GetAvccSpsLen(Mp4SampleEntry *entry, uint8_t index, uint16_t *out_len);
int32_t mp4GetAvccPpsLen(Mp4SampleEntry *entry, uint8_t index, uint16_t *out_len);
int32_t mp4GetAvccSpsData(Mp4SampleEntry *entry, uint8_t index, uint32_t buf_size, void *out_buf);
int32_t mp4GetAvccPpsData(Mp4SampleEntry *entry, uint8_t index, uint32_t buf_size, void *out_buf);
int32_t mp4GetTrackUsmtCustom(Mp4Track *track, uint32_t *out_hi, uint32_t *out_lo, int32_t mode);
int32_t mp4FindMtdtEntry(Mp4Handle *handle, Mp4Track *track, uint32_t tag, uint16_t lang, uint16_t enc, uint16_t *out_lang, Mp4MtdtEntry **out_entry, uint16_t *out_len);
int32_t mp4SerializeMtdtExt(Mp4MtdtEntry *entry, uint8_t *out_buf);
int32_t mp4CopyMtdtPayload(Mp4MtdtEntry *entry, uint32_t buf_size, uint8_t *out_buf);
int32_t mp4GetMtdtCount(Mp4Handle *handle, Mp4Track *track, uint32_t *out_count);
int32_t mp4GetMtdtByIndex(Mp4Handle *handle, Mp4Track *track, uint32_t index, Mp4MtdtEntry **out_entry, uint32_t *out_tag, uint16_t *out_lang, uint16_t *out_enc, uint16_t *out_len);
int32_t mp4FindMtdfEntry(Mp4Handle *handle, Mp4Track *track, uint32_t tag, uint16_t *out_lang, uint16_t *out_enc, void *out_buf);
int32_t mp4GetMtdfByIndex(Mp4Handle *handle, Mp4Track *track, uint32_t index, uint32_t *out_tag, uint16_t *out_lang, uint16_t *out_enc, void *out_buf);
int32_t mp4GetAprfEntry(Mp4Handle *handle, uint32_t index, uint32_t *out_words);
int32_t mp4GetVprfEntry(Mp4Handle *handle, uint32_t index, uint32_t *out_words);
int32_t mp4GetOdsmCount(Mp4Handle *handle, uint16_t *out_count);
int32_t mp4GetOdsmObject(Mp4Handle *handle, uint16_t index, Mp4OdsmObject **out_obj);
int32_t mp4GetOdsmTrackInfo(Mp4OdsmObject *obj, Mp4OdsmTrackInfo *out_info);
int32_t mp4GetOdsmEsHeader(Mp4OdsmObject *obj, uint8_t index, uint16_t *out_type, uint32_t *out_size);
int32_t mp4GetOdsmEsPayload(Mp4OdsmObject *obj, uint8_t index, uint32_t buf_size, void *out_buf);
int32_t mp4GetEsConfig(Mp4SampleEntry *entry, Mp4EsConfigOut *out_cfg);
int32_t mp4GetEsDecSpecific(Mp4SampleEntry *entry, int32_t check_size, uint32_t buf_size, void *out_buf);
int32_t mp4GetHintSampleDesc(Mp4SampleEntry *entry, Mp4HintSampleDescOut *out_desc);
int32_t mp4GetAvcUuidSize(Mp4SampleEntry *entry, uint32_t *out_size);
int32_t mp4GetFtypBrand(Mp4Handle *handle, uint32_t *out_major, uint32_t *out_minor);
int32_t mp4GetMovieTimes(Mp4Handle *handle, Mp4DateTime *out_ctime, Mp4DateTime *out_mtime);
int32_t mp4GetMovieTimescale(Mp4Handle *handle, uint32_t *out_scale);
int32_t mp4GetMovieDuration(Mp4Handle *handle, uint32_t *out_dur);
int32_t mp4GetTrackCount(Mp4Handle *handle, uint16_t *out_count);
int32_t mp4GetTrackById(Mp4Handle *handle, uint32_t track_id, Mp4Track **out_track);
int32_t mp4GetElstCount(Mp4Track *track, uint32_t *out_count);
int32_t mp4GetMediaTimescale(Mp4Track *track, uint32_t *out_scale);
int32_t mp4GetHandlerType(Mp4Track *track, uint32_t *out_type);
int32_t mp4GetTrackSampleCount(Mp4Track *track, uint32_t *out_count);
int32_t mp4GetSampleDescCount(Mp4Track *track, uint32_t *out_count);
int32_t mp4GetVisualSampleDesc(Mp4SampleEntry *entry, Mp4VisualSampleDescOut *out_desc);
int32_t mp4GetVisualBrief(Mp4SampleEntry *entry, Mp4VisualBriefOut *out_desc);
int32_t mp4GetAudioBrief(Mp4SampleEntry *entry, Mp4AudioBriefOut *out_desc);
int32_t mp4GetEsdsLen(Mp4SampleEntry *entry, uint32_t *out_len);
int32_t mp4GetEsdsData(Mp4SampleEntry *entry, uint32_t buf_size, void *out_buf);
int32_t mp4GetAvccHeader(Mp4SampleEntry *entry, uint8_t *out_hdr, uint8_t *out_sps_cnt, uint8_t *out_pps_cnt);
int32_t mp4GetDrefLocationLen(Mp4DrefEntry *entry, uint32_t *out_len);
int32_t mp4GetDrefLocation(Mp4DrefEntry *entry, uint32_t buf_size, char *out_buf);
int32_t mp4GetMoovOffset(Mp4Handle *handle, uint64_t *out_offset);
int32_t mp4GetFprfAttr(Mp4Handle *handle, uint32_t *out_attr);
int32_t mp4GetIodsProfiles(Mp4Handle *handle, uint8_t *out_profiles);
int32_t mp4GetAvcUuidData(Mp4SampleEntry *entry, uint32_t buf_size, void *out_buf);
int32_t mp4MatchCodecTag(uint32_t target_tag, Mp4SampleEntry *entry);

uint32_t mp4ReadU32(Mp4IoStream *stream);
int32_t mp4ReadS32(Mp4IoStream *stream);
uint16_t mp4ReadU16(Mp4IoStream *stream);
int16_t mp4ReadS16(Mp4IoStream *stream);
int32_t mp4ReadBytes(void *dst, uint32_t size, Mp4IoStream *stream);
int32_t mp4ReadU32Array(void *dst, uint32_t num_bytes, Mp4IoStream *stream);
uint32_t mp4ReadU32ToU64Array(uint8_t *dst, uint32_t num_bytes, Mp4IoStream *stream);
int32_t mp4AttachStream(Mp4IoStream *stream);

int32_t mp4MfraCreate(Mp4MfraBox **out_mfra);
int32_t mp4MfraDestroy(Mp4MfraBox *mfra);
int32_t mp4MfraLookupByTime(Mp4MfraBox *mfra, uint32_t track_id, uint32_t target_time, uint32_t *out_found, Mp4TfraEntry *out_entry);

/* ========================================================================= */
/* Section 3: mp4_frag_io.c (0x0060dc..0x00bae8)                             */
/* ========================================================================= */
int32_t mp4MfraFindPrevMoof(Mp4MfraBox *mfra, uint32_t track_id, uint32_t cur_time, uint64_t cur_offset, uint32_t *out_found, uint64_t *out_prev_offset);
int32_t mp4MfraLookupLast(Mp4MfraBox *mfra, uint32_t track_id, uint32_t *out_found, Mp4TfraEntry *out_entry);
int32_t mp4MfraRemoveOther(Mp4MfraBox *mfra, uint32_t seq_id);
int32_t mp4MfraParse(Mp4MfraBox *mfra, void *fh, uint32_t body_size, uint32_t *out_err);
int32_t mp4MfraParseTfra(Mp4MfraBox *mfra, void *fh, uint32_t body_size, uint32_t *out_err);
int32_t mp4MfraParseMfro(Mp4MfraBox *mfra, void *fh, uint32_t body_size, uint32_t *out_err);
int32_t mp4TfraCreate(Mp4TfraBox **out_tfra);
int32_t mp4TfraDestroy(Mp4TfraBox *tfra);
int32_t mp4TfraGetTrackId(Mp4TfraBox *tfra, uint32_t *out_id);
int32_t mp4TfraGetEntryCount(Mp4TfraBox *tfra, uint32_t *out_count);
int32_t mp4TfraGetEntry(Mp4TfraBox *tfra, uint32_t index_one_based, Mp4TfraEntry *out_entry);
int32_t mp4TfraMerge(Mp4TfraBox *dst, Mp4TfraBox *src);
int32_t mp4TfraValidate(Mp4TfraBox *tfra);
int32_t mp4TfraParseBody(Mp4TfraBox *tfra, void *fh, uint32_t body_size, uint32_t *out_err);
uint8_t *mp4ReadVarUint(Mp4TfraBox *tfra, uint8_t size_minus_one, uint8_t *src, uint32_t *out_val);
int32_t mp4MfroCreate(Mp4MfroBox **out_mfro);
int32_t mp4MfroDestroy(Mp4MfroBox *mfro);
int32_t mp4MfroGetSize(Mp4MfroBox *mfro, uint32_t *out_size);
int32_t mp4MfroParseBody(Mp4MfroBox *mfro, void *fh, uint32_t body_size, uint32_t *out_err);
int32_t mp4MfraOtherDestroy(Mp4MfraOther *item);
int32_t mp4MfraOtherGetId(Mp4MfraOther *item, uint32_t *out_id);
int32_t mp4MfraOtherCheckComplete(Mp4MfraOther *item, uint32_t *out_complete);
int32_t mp4InitFragmentedTracks(Mp4Movie *movie, uint32_t *sample_counts, Mp4IoStream *stream);
int32_t mp4BuildDefaultMvex(Mp4Movie *movie);
int32_t mp4CopyStblSamples(Mp4Stbl *src, Mp4Stbl *dst, uint32_t sample_count);
int32_t mp4MvexParse(Mp4MvexBox *mvex, uint32_t body_size, Mp4IoStream *stream);
int32_t mp4MoofReadNext(Mp4Movie *movie, Mp4IoStream *stream, uint64_t offset, Mp4MoofBox **out_moof, uint64_t *out_next_offset);
int32_t mp4MoofParse(Mp4MoofBox *moof, uint32_t body_size, Mp4IoStream *stream);
int32_t mp4MoofFindOrRead(Mp4Movie *movie, Mp4IoStream *stream, uint64_t offset, Mp4MoofBox **out_moof, uint64_t *out_next_offset);
int32_t mp4MoofGetTrackDuration(Mp4MoofBox *moof, uint32_t track_id, Mp4MvexBox *mvex, uint32_t *out_dur);
int32_t mp4MoofGetTimeBeforeSample(Mp4MoofBox *moof, uint32_t track_id, Mp4MvexBox *mvex, uint32_t traf_idx, uint32_t trun_idx, uint32_t sample_idx, uint32_t *out_dur);
int32_t mp4MoofGetTimeFromSample(Mp4MoofBox *moof, uint32_t track_id, Mp4MvexBox *mvex, uint32_t traf_idx, uint32_t trun_idx, uint32_t sample_idx, uint32_t *out_dur);
int32_t mp4TrafParse(Mp4TrafBox *traf, uint32_t body_size, Mp4IoStream *stream);
int32_t mp4MehdReadBody(Mp4MehdBox *mehd, Mp4IoStream *stream);
int32_t mp4MfhdReadBody(Mp4MfhdBox *mfhd, Mp4IoStream *stream);
void mp4MoofListFree(Mp4MoofBox *head);
void mp4TrafFree(Mp4TrafBox *traf);
void mp4TrunListFree(Mp4TrunBox *head);
void mp4MvexCalcSize(Mp4MvexBox *mvex);
int32_t mp4TrafAddTrun(Mp4TrafBox *traf, Mp4TrunBox *trun);
int32_t mp4TrunAddSample(Mp4TrunBox *trun, const uint32_t *quad);
int32_t mp4MovieAppendMoof(Mp4Movie *movie, Mp4MoofBox *moof);
Mp4TrunBox *mp4TrafGetTrun(Mp4TrafBox *traf, uint32_t idx);
int32_t mp4TrunGetSampleQuad(Mp4TrunBox *trun, uint32_t idx, uint32_t *out_quad);
Mp4MoofBox *mp4MovieGetMoofAt(Mp4Movie *movie, uint32_t idx);
int32_t mp4TrunSumSizes(Mp4TrexBox *trex, Mp4TrafBox *traf, Mp4TrunBox *trun, uint32_t start_idx, uint32_t end_idx, uint32_t *out_bytes);
int32_t mp4TrunGetSampleDur(Mp4TrunBox *trun, uint32_t idx, Mp4TfhdBox *tfhd, Mp4TrexBox *trex, uint32_t *out_dur);
int32_t mp4TrunGetSampleIsSync(Mp4TrunBox *trun, uint32_t idx, Mp4TfhdBox *tfhd, Mp4TrexBox *trex, uint32_t *out_is_sync);
int32_t mp4TrafResolveDataOffsets(Mp4TrafBox *prev_traf, Mp4MoofBox *moof, Mp4TrafBox *traf);
int32_t mp4TrunGetSampleSize(Mp4TrunBox *trun, uint32_t idx, Mp4TfhdBox *tfhd, Mp4TrexBox *trex, uint32_t *out_size);
int32_t mp4TfhdGetSampleDescIdx(Mp4TfhdBox *tfhd, Mp4TrexBox *trex, uint32_t *out_idx);
int32_t mp4TrafGetSampleDetail(Mp4TrafBox *traf, Mp4TrexBox *trex, uint32_t sample_idx, Mp4SampleInfo *out_info, uint32_t *out_desc_idx);
int32_t mp4MovieFindMoofByOffset(Mp4Movie *movie, Mp4IoStream *stream, uint64_t offset, Mp4MoofBox **out_moof, uint64_t *out_next_offset);
int32_t mp4MoofCountTrafForTrack(Mp4MoofBox *moof, uint32_t track_id, uint32_t *out_count);
int32_t mp4MoofGetTrafForTrack(Mp4MoofBox *moof, uint32_t track_id, uint32_t match_idx, Mp4TrafBox **out_traf);
int32_t mp4MvexGetDefSampleDur(Mp4MvexBox *mvex, uint32_t track_id, uint32_t *out_dur);
int32_t mp4TrafCalcDuration(Mp4TrafBox *traf, uint32_t def_dur, uint32_t *out_dur);
int32_t mp4TrafCalcTimeBefore(Mp4TrafBox *traf, uint32_t def_dur, uint32_t trun_idx, uint32_t sample_idx, uint32_t *out_dur);
int32_t mp4MvexFree(Mp4MvexBox *mvex);
int32_t mp4TrafGetTrunSampleCount(Mp4TrafBox *traf, uint32_t trun_idx, uint32_t *out_count);
int32_t mp4MoofGetOffset(Mp4MoofBox *moof, uint64_t *out_offset);
int32_t mp4MoofGetSeqNum(Mp4MoofBox *moof, uint32_t *out_seq);
int32_t mp4MoofGetMovie(Mp4MoofBox *moof, Mp4Movie **out_movie);
int32_t mp4BoxFree(void **box_ptr, uint32_t tag);
int32_t mp4MemFree(void *ptr);
int32_t mp4MovieRemoveTrackById(Mp4Movie *movie, uint32_t track_id);
int32_t mp4MovieRemoveTrackByIndex(Mp4Movie *movie, uint32_t index);
int32_t mp4MovieAppendTrack(Mp4Movie *movie, Mp4Track *track);
Mp4Track *mp4MovieGetTrackAt(Mp4Movie *movie, uint32_t index);
int32_t mp4TrackGetTrex(Mp4Track *track, Mp4TrexBox **out_trex);
int32_t mp4ElstFindSegment(Mp4ElstBox *elst, uint32_t time_val, uint32_t *out_seg_idx, uint32_t *out_offset_in_seg);
int32_t mp4DrefGetEntry(uint16_t index_one_based, Mp4DrefBox *dref, Mp4DrefEntry **out_entry);
int32_t mp4StsdGetEntry(Mp4StsdBox *stsd, uint32_t index_one_based, Mp4SampleEntry **out_entry);
int32_t mp4TrackMapMovieTime(Mp4Track *track, uint32_t movie_time, int32_t mode, uint32_t *out_media_time);
int32_t mp4ChunkArrayEraseU32(Mp4ChunkArray **head_ptr, uint32_t first_idx, uint32_t last_idx, uint32_t cols);
int32_t mp4ChunkArrayEraseU64(Mp4ChunkArray **head_ptr, uint32_t first_idx, uint32_t last_idx, uint32_t cols);
int32_t mp4MovieUpdateDuration(Mp4Movie *movie);
int32_t mp4TrackCalcDuration(Mp4Track *track, uint32_t *out_dur, int32_t allow_io);
int32_t mp4MovieCalcDuration(Mp4Movie *movie, uint32_t *out_dur, int32_t allow_io);
int32_t mp4MulU32ToU64(uint32_t lhs, uint32_t rhs, uint32_t *out_hi, uint32_t *out_lo);

/* ========================================================================= */
/* Section 4: mp4_parse.c (0x00bb84..0x0130d8)                               */
/* ========================================================================= */
int32_t mp4DivU64ByU32(uint32_t hi, uint32_t lo, uint32_t divisor, uint32_t *out_q_hi, uint32_t *out_q_lo, uint32_t *out_rem);
int32_t mp4ShiftLeftU32ToU64(uint32_t val, uint32_t shift, uint32_t *out_hi, uint32_t *out_lo);
int32_t mp4SubU64(uint32_t *hi_ptr, uint32_t *lo_ptr, uint32_t rhs_hi, uint32_t rhs_lo);
int32_t mp4AddU64(uint32_t *hi_ptr, uint32_t *lo_ptr, uint32_t add_hi, uint32_t add_lo);
int32_t mp4AdjustU64(uint32_t *hi_ptr, uint32_t *lo_ptr, int32_t delta);
int32_t mp4AddS32ToU64(uint64_t *val_ptr, int32_t delta);
int32_t mp4TrackResolveDrefs(Mp4Track *track);
int32_t mp4MtdfSetEntry(Mp4MtdfBox *mtdf, uint16_t size, uint32_t tag, uint16_t lang, uint16_t enc, uint32_t index, const void *payload);
int32_t mp4MtdtReserveEntry(Mp4MtdtBox *mtdt, uint16_t size, uint32_t tag, uint16_t lang, uint16_t enc, uint32_t index);
int32_t mp4MtdtSetEntry(Mp4MtdtBox *mtdt, uint16_t size, uint32_t tag, uint16_t lang, uint16_t enc, uint32_t index, const void *payload);
void *mp4MemCalloc(uint32_t size);
void *mp4MemMalloc(uint32_t size);
int32_t mp4MemRealloc(void **ptr, uint32_t old_size, uint32_t new_size);
void mp4MtdtExtFree(Mp4MtdtExt *ext);
int32_t mp4ParseFile(Mp4Movie *movie, uint64_t file_size, Mp4IoStream *stream, int32_t mode);
int32_t mp4ParseFilePartial(Mp4Movie *movie, uint32_t mask, Mp4IoStream *stream);
int32_t mp4TrackGetId(Mp4Track *track, uint32_t *out_id);
int32_t mp4MovieFindTrackById(Mp4Movie *movie, uint32_t track_id, Mp4Track **out_track);
int32_t mp4ElstReadCol(Mp4ElstBox *elst, uint32_t row, uint32_t col, uint32_t *out_val);
int32_t mp4TrackGetActiveStbl(Mp4Track *track, Mp4Stbl **out_stbl);
int32_t mp4GetBit(uint8_t octet_val, uint32_t bit_idx);
int32_t mp4CheckedAddU32(uint32_t *acc, uint32_t addend);
int32_t mp4CheckedAddU64(uint64_t *acc, uint32_t addend);
int32_t mp4MtdtSetExtEntry(Mp4MtdtBox *mtdt, uint16_t size, uint32_t tag, uint16_t lang, uint16_t enc, uint32_t index, uint32_t magic, Mp4MtdtExt **out_ext);
int32_t mp4ParseRootCheck(Mp4Movie *movie, uint64_t file_size, Mp4IoStream *stream, int32_t mode);
int32_t mp4ParseRootBoxes(Mp4Movie *movie, uint64_t file_size, Mp4IoStream *stream, int32_t mode);
int32_t mp4ParseMoov(Mp4Movie *movie, uint32_t body_size, Mp4IoStream *stream);
int32_t mp4ParseMoovPartial(Mp4Movie *movie, uint32_t body_size, uint32_t mask, Mp4IoStream *stream);
int32_t mp4ParseTrak(Mp4Track *track, uint32_t body_size, Mp4IoStream *stream);
int32_t mp4ParseTref(Mp4TrefBox *tref, uint32_t body_size, Mp4IoStream *stream);
int32_t mp4ParseEdts(Mp4EdtsBox *edts, uint32_t body_size, Mp4IoStream *stream);
int32_t mp4ParseMdia(Mp4MdiaBox *mdia, uint32_t body_size, Mp4IoStream *stream);
int32_t mp4ParseUsmt(Mp4UsmtBox *usmt, uint32_t body_size, Mp4IoStream *stream);
int32_t mp4ParseMtsm(Mp4MtsmBox *mtsm, uint32_t body_size, Mp4IoStream *stream);
int32_t mp4ParseMtdtExt(Mp4MtdtExt *ext, uint32_t body_size, Mp4IoStream *stream);
int32_t mp4ParseFtyp(Mp4FtypBox *ftyp, uint64_t *rem_file, uint32_t *box_size_ptr, Mp4IoStream *stream);
int32_t mp4ParseProf(Mp4ProfBox *prof, uint32_t entry_count, Mp4IoStream *stream);
uint32_t mp4ReadFprfBody(Mp4FprfBox *fprf, Mp4IoStream *stream);
uint32_t mp4ReadAprfBody(Mp4AprfBox *aprf, Mp4IoStream *stream);
uint32_t mp4ReadVprfBody(Mp4VprfBox *vprf, Mp4IoStream *stream);
int32_t mp4ReadHandlerName(char *dst, uint32_t len, Mp4IoStream *stream);
int32_t mp4ParseMinf(Mp4MinfBox *minf, uint32_t body_size, Mp4IoStream *stream, uint32_t handler_type);
int32_t mp4ParseDinf(Mp4DinfBox *dinf, uint32_t body_size, Mp4IoStream *stream);
int32_t mp4ParseDref(Mp4DrefBox *dref, uint32_t body_size, Mp4IoStream *stream);
int32_t mp4ParseStbl(Mp4Stbl *stbl, uint32_t body_size, Mp4IoStream *stream, uint32_t handler_type);
int32_t mp4ReadChunkArrayU32(Mp4ChunkArray **head_ptr, uint32_t entry_count, uint32_t cols, Mp4IoStream *stream);
int32_t mp4ReadChunkArrayU64(Mp4ChunkArray **head_ptr, uint32_t entry_count, uint32_t cols, Mp4IoStream *stream);
int32_t mp4ParseStsd(Mp4StsdBox *stsd, uint32_t body_size, Mp4IoStream *stream, uint32_t handler_type);
int32_t mp4ParseSampleExt(Mp4SampleEntry *entry, uint32_t body_size, int32_t unused_flag, Mp4IoStream *stream);
int32_t mp4ParseExtraBox(Mp4SampleEntry *entry, uint32_t tag, uint32_t box_size, uint64_t box_end, Mp4IoStream *stream);
int32_t mp4ParseAvccNalList(uint64_t box_end, uint8_t *count_ptr, uint32_t *out_rem, Mp4DynArray *arr, Mp4IoStream *stream);
int32_t mp4ParseRootPartial(Mp4Movie *movie, uint32_t mask, Mp4IoStream *stream);
uint16_t mp4ReadVisualDesc(Mp4VisualDesc *desc, Mp4IoStream *stream);
uint32_t mp4ReadAudioDesc(Mp4AudioDesc *desc, uint16_t ver, Mp4IoStream *stream);
uint32_t mp4ReadHintDesc(Mp4HintDesc *desc, Mp4IoStream *stream);
int16_t mp4ReadVmhd(Mp4VmhdBox *vmhd, Mp4IoStream *stream);
uint16_t mp4ReadSmhd(Mp4SmhdBox *smhd, Mp4IoStream *stream);
int32_t mp4ReadNmhd(Mp4NmhdBox *nmhd, Mp4IoStream *stream);
uint16_t mp4ReadMdhd(Mp4MdhdBox *mdhd, Mp4IoStream *stream);
uint32_t mp4ReadTkhd(Mp4TkhdBox *tkhd, Mp4IoStream *stream);
uint32_t mp4ReadMvhd(Mp4MvhdBox *mvhd, Mp4IoStream *stream);
int32_t mp4SetParseFlag(uint32_t tag, uint32_t flag, Mp4IoStream *stream);
int32_t mp4AllocUniqueBox(void **slot, uint32_t tag, uint32_t alloc_size, uint64_t box_end, Mp4IoStream *stream);
int32_t mp4RecordCustomUuid(Mp4Movie *movie, uint64_t offset, uint32_t box_size, const uint32_t *uuid);
int32_t mp4CacheStblCreate(Mp4CacheStbl **out_cache, Mp4IoStream *stream);

/* ========================================================================= */
/* Section 5: mp4_cache.c (0x013188..0x019db8)                               */
/* ========================================================================= */
int32_t mp4CacheParseBox(Mp4CacheStbl *cache, uint32_t box_size, uint32_t tag, uint64_t box_end, Mp4IoStream *stream);
int32_t mp4CacheAllocPool(Mp4CacheBlock **pool_ptr, uint32_t entry_count, uint32_t cols, Mp4IoStream *stream);
int32_t mp4CacheFreeBlocks(Mp4CacheBlock **head_ptr);
int32_t mp4CacheInitTableHeader(void *table, uint32_t box_size, uint32_t tag, uint32_t hdr_size, uint32_t entry_bytes, uint64_t box_end, Mp4IoStream *stream);
int32_t mp4CachePlanWindow(uint32_t req_start, uint32_t req_count, uint32_t cur_start, uint32_t cur_count, uint32_t max_idx, int32_t *out_plan, int32_t *out_evict);
int32_t mp4CacheLoadStts(Mp4CacheStts *stts, uint32_t req_start, uint32_t req_count, uint32_t total_samples, uint32_t total_dur, Mp4IoStream *stream);
int32_t mp4CacheLoadSttsFwd(Mp4CacheStts *stts, uint32_t req_start, uint32_t req_count, uint32_t entry_idx, uint32_t sample_idx, uint32_t time_val, int32_t has_existing, Mp4IoStream *stream);
int32_t mp4CacheLoadSttsBwd(Mp4CacheStts *stts, uint32_t req_start, uint32_t req_count, uint32_t entry_idx, uint32_t sample_idx, uint32_t time_val, int32_t has_existing, Mp4IoStream *stream);
int32_t mp4CacheLoadStsc(Mp4CacheStsc *stsc, uint32_t req_start, uint32_t req_count, uint32_t total_samples, uint32_t total_chunks, Mp4IoStream *stream);
int32_t mp4CacheLoadStscFwd(Mp4CacheStsc *stsc, uint32_t req_start, uint32_t req_count, uint32_t entry_idx, uint32_t sample_idx, uint32_t chunk_idx, int32_t has_existing, uint32_t total_chunks, Mp4IoStream *stream);
int32_t mp4CacheLoadStscBwd(Mp4CacheStsc *stsc, uint32_t req_start, uint32_t req_count, uint32_t entry_idx, uint32_t sample_idx, uint32_t chunk_idx, int32_t has_existing, uint32_t total_chunks, Mp4IoStream *stream);
int32_t mp4CacheLoadStsz(Mp4CacheStsz *stsz, uint32_t req_start, uint32_t req_count, uint32_t chunk_first_sample, uint32_t *prefix_bytes, Mp4IoStream *stream);
int32_t mp4CacheLoadStszFwd(Mp4CacheStsz *stsz, uint32_t start_idx, uint32_t count, int32_t has_existing, Mp4IoStream *stream);
int32_t mp4CacheLoadStszBwd(Mp4CacheStsz *stsz, uint32_t end_idx, uint32_t count, Mp4IoStream *stream);
int32_t mp4CacheLoadStco(Mp4CacheStco *stco, uint32_t req_start, uint32_t req_count, Mp4IoStream *stream);
int32_t mp4CacheLoadStcoFwd(Mp4CacheStco *stco, uint32_t start_idx, uint32_t count, int32_t has_existing, Mp4IoStream *stream);
int32_t mp4CacheLoadStcoBwd(Mp4CacheStco *stco, uint32_t end_idx, uint32_t count, Mp4IoStream *stream);
int32_t mp4CacheLoadStss(Mp4CacheStss *stss, uint32_t req_start, uint32_t req_count, uint32_t total_samples, Mp4IoStream *stream);
int32_t mp4CacheLoadStssFwd(Mp4CacheStss *stss, uint32_t req_start, uint32_t req_count, uint32_t entry_idx, uint32_t sample_idx, int32_t has_existing, uint32_t total_samples, Mp4IoStream *stream);
int32_t mp4CacheLoadStssBwd(Mp4CacheStss *stss, uint32_t req_start, uint32_t req_count, uint32_t entry_idx, uint32_t sample_idx, int32_t has_existing, uint32_t total_samples, Mp4IoStream *stream);
int32_t mp4CacheLoadCtts(Mp4CacheCtts *ctts, uint32_t req_start, uint32_t req_count, uint32_t total_samples, Mp4IoStream *stream);
int32_t mp4CacheLoadCttsFwd(Mp4CacheCtts *ctts, uint32_t req_start, uint32_t req_count, uint32_t entry_idx, uint32_t sample_idx, int32_t has_existing, Mp4IoStream *stream);
int32_t mp4CacheLoadCttsBwd(Mp4CacheCtts *ctts, uint32_t req_start, uint32_t req_count, uint32_t entry_idx, uint32_t sample_idx, int32_t has_existing, Mp4IoStream *stream);
int32_t mp4CacheReadSttsBlock(Mp4CacheBlock **slot, uint32_t req_entries, int32_t is_fwd, uint32_t sample_ref, uint32_t time_ref, uint32_t *out_samples, uint32_t *out_dur, uint32_t *out_entries, Mp4CacheBlock **pool, Mp4IoStream *stream);
int32_t mp4CacheReadStscBlock(Mp4CacheBlock **slot, uint32_t req_entries, int32_t is_fwd, uint32_t sample_ref, uint32_t chunk_ref, int32_t is_last, uint32_t total_chunks, uint32_t *out_resolved, uint32_t *out_samples, uint32_t *out_chunks, uint32_t *out_entries, Mp4CacheBlock **pool, Mp4IoStream *stream);
int32_t mp4CacheReadStszBlock(Mp4CacheBlock **slot, uint32_t first_sample, uint32_t req_entries, uint32_t *out_entries, Mp4CacheBlock **pool, Mp4IoStream *stream);
int32_t mp4CacheReadStcoBlock(Mp4CacheBlock **slot, uint32_t first_chunk, uint32_t req_entries, uint32_t *out_entries, Mp4CacheBlock **pool, Mp4IoStream *stream);
int32_t mp4CacheReadStssBlock(Mp4CacheBlock **slot, uint32_t req_entries, int32_t is_first, int32_t is_last, uint32_t total_samples, uint32_t *out_resolved, uint32_t *out_samples, uint32_t *out_entries, Mp4CacheBlock **pool, Mp4IoStream *stream);
int32_t mp4CacheReadCttsBlock(Mp4CacheBlock **slot, uint32_t req_entries, int32_t is_fwd, uint32_t sample_ref, uint32_t *out_samples, uint32_t *out_entries, Mp4CacheBlock **pool, Mp4IoStream *stream);
int32_t mp4CacheTrimBlocks(Mp4CacheBlock **active_ptr, uint32_t req_start, uint32_t req_count, uint32_t end_sample, uint32_t end_time, uint32_t end_chunk, int32_t is_fwd, uint32_t *out_samples, uint32_t *out_dur, uint32_t *out_chunks, uint32_t *out_entries, Mp4CacheBlock **pool);
int32_t mp4CacheTrimStcoBlocks(Mp4CacheBlock **active_ptr, uint32_t req_start, uint32_t req_count, int32_t is_fwd, uint32_t *out_chunks, Mp4CacheBlock **pool);
Mp4CacheBlock **mp4CacheFindTailSlot(Mp4CacheBlock **head_ptr);
Mp4CacheBlock **mp4CacheNextSlot(Mp4CacheBlock **slot);
int32_t mp4CacheSetNext(Mp4CacheBlock *blk, Mp4CacheBlock *next_blk);
int32_t mp4CacheGetSampleInfo(Mp4CacheStbl *cache, uint32_t sample_idx, int32_t want_chunk_rem, Mp4SampleInfo *out_info, int32_t allow_io);
int32_t mp4CacheEnsureSampleWindow(Mp4CacheStbl *cache, uint32_t sample_idx, int32_t allow_io);
int32_t mp4CacheCalcLoadRange(uint32_t target, uint32_t cur_start, uint32_t cur_count, uint32_t total, uint32_t *out_start, uint32_t *out_count, Mp4CacheCtrl *ctrl);
int32_t mp4CacheCheckSyncSample(Mp4CacheStss *stss, uint32_t sample_idx, uint32_t *out_is_sync);
int32_t mp4CacheFindSampleByTime(Mp4CacheStbl *cache, uint32_t time_val, uint32_t *out_sample, uint32_t *out_is_sync, uint32_t *out_rem, int32_t allow_io);
int32_t mp4CacheLookupTime(Mp4CacheStbl *cache, uint32_t time_val, uint32_t *out_sample, uint32_t *out_rem, int32_t allow_io);
int32_t mp4CacheEnsureTimeWindow(Mp4CacheStbl *cache, uint32_t time_val, int32_t allow_io);
int32_t mp4CacheSttsLookupByTime(Mp4CacheStts *stts, uint32_t time_val, uint32_t *out_sample, uint32_t *out_rem);
int32_t mp4CacheStscLookupBySample(Mp4CacheStsc *stsc, uint32_t sample_idx, uint32_t *out_chunk, uint32_t *out_chunk_first_sample, uint32_t *out_desc_idx);
int32_t mp4CacheUpdateWindow(Mp4CacheStbl *cache, uint32_t start_sample, uint32_t count, Mp4IoStream *stream);
int32_t mp4CacheHasStts(const Mp4CacheStts *stts);
int32_t mp4CacheHasStsc(const Mp4CacheStsc *stsc);
int32_t mp4CacheHasStsz(const Mp4CacheStsz *stsz);
int32_t mp4CacheHasStco(const Mp4CacheStco *stco);
int32_t mp4CacheGetSampleCount(Mp4CacheStbl *cache, uint32_t *out_count);
uint32_t mp4CacheStszCount(const Mp4CacheStsz *stsz);
uint32_t mp4CacheStcoCount(const Mp4CacheStco *stco);
void mp4CacheSttsGetWindow(const Mp4CacheStts *stts, uint32_t *out_s_start, uint32_t *out_s_cnt, uint32_t *out_t_start, uint32_t *out_t_cnt);
void mp4CacheStscGetWindow(const Mp4CacheStsc *stsc, uint32_t *out_s_start, uint32_t *out_s_cnt, uint32_t *out_c_start, uint32_t *out_c_cnt);
int32_t mp4CacheRecalcBounds(Mp4CacheStbl *cache);
int32_t mp4CacheIntersectRange(uint32_t *start_ptr, uint32_t *count_ptr, uint32_t other_start, uint32_t other_count);
void mp4CacheResetStts(Mp4CacheStts *stts);
void mp4CacheResetStsc(Mp4CacheStsc *stsc);
void mp4CacheResetStsz(Mp4CacheStsz *stsz);
void mp4CacheResetStco(Mp4CacheStco *stco);
void mp4CacheResetStss(Mp4CacheStss *stss);
void mp4CacheResetCtts(Mp4CacheCtts *ctts);
void mp4CacheRecycleList(Mp4CacheBlock **pool_ptr, Mp4CacheBlock **active_ptr);
int32_t mp4CacheFixupStscTail(Mp4CacheStsc *stsc, uint32_t last_chunk_idx);
int32_t mp4CacheSetStssSampleTotal(Mp4CacheStss *stss, uint32_t total_samples);
int32_t mp4CacheCalcStscTail(Mp4CacheBlock *blk, uint32_t last_chunk_idx, uint32_t *out_chunks, uint32_t *out_samples);
int32_t mp4CacheCalcPrefixBytes(Mp4CacheStsz *stsz, uint32_t chunk_first, uint32_t win_start, uint32_t prev_prefix, uint32_t *out_prefix);
int32_t mp4CacheReadAndSumStsz(Mp4CacheStsz *stsz, uint32_t start_idx, uint32_t end_idx, uint32_t *out_sum, Mp4IoStream *stream);
int32_t mp4CacheSumBlockWords(Mp4CacheBlock *blk, uint32_t *out_sum);
int32_t mp4CacheIsStscTailResolved(const Mp4CacheStsc *stsc);
int32_t mp4CacheIsStssTailResolved(const Mp4CacheStss *stss);
int32_t mp4CacheNeedsInitialStco(const Mp4CacheStco *stco);
int32_t mp4CacheGetSampleSize(Mp4CacheStbl *cache, uint32_t sample_idx, uint32_t *out_size, int32_t allow_io);
int32_t mp4CacheStszLookup(const Mp4CacheStsz *stsz, uint32_t sample_idx, uint32_t *out_size);
int32_t mp4CacheCalcBoxSizes(Mp4CacheStbl *cache, uint32_t *out_total);
int32_t mp4CacheCalcSttsSize(Mp4CacheStts *stts, uint32_t *out_size);

/* ========================================================================= */
/* Section 6: mp4_sample.c (0x019ddc..0x0203fc)                              */
/* ========================================================================= */
int32_t mp4CacheCalcStscSize(Mp4CacheStsc *stsc, uint32_t *out_size);
int32_t mp4CacheCalcStszSize(Mp4CacheStsz *stsz, uint32_t *out_size);
int32_t mp4CacheCalcStcoSize(Mp4CacheStco *stco, uint32_t *out_size);
int32_t mp4CacheCalcStssSize(Mp4CacheStss *stss, uint32_t *out_size);
int32_t mp4CacheCalcCttsSize(Mp4CacheCtts *ctts, uint32_t *out_size);
int32_t mp4CacheGetSampleOffset(Mp4CacheStbl *cache, uint32_t sample_idx, uint64_t *out_offset, uint32_t *out_chunk);
int32_t mp4CacheSumStszInWindow(Mp4CacheStsz *stsz, uint32_t start_idx, uint32_t end_idx, uint32_t *out_sum);
int32_t mp4CacheSumBlockRange(Mp4CacheBlock *head, uint32_t win_start, uint32_t start_idx, uint32_t end_idx, uint32_t col, uint32_t *out_sum);
int32_t mp4CacheFindSyncSample(Mp4CacheStbl *cache, uint32_t sample_idx, int32_t direction, uint32_t *out_sample, uint32_t *out_delta, int32_t allow_io);
int32_t mp4CacheGetSampleTime(Mp4CacheStbl *cache, uint32_t sample_idx, uint32_t *out_time, uint32_t *out_dur, int32_t allow_io);
int32_t mp4CacheLookupSampleTime(Mp4CacheStbl *cache, uint32_t sample_idx, uint32_t *out_time, uint32_t *out_dur, int32_t allow_io);
int32_t mp4CacheSttsLookupBySample(Mp4CacheStts *stts, uint32_t sample_idx, uint32_t *out_time, uint32_t *out_dur);
int32_t mp4CacheEnsureSttsSample(Mp4CacheStbl *cache, uint32_t sample_idx, int32_t allow_io);
int32_t mp4CacheEnsureStscSample(Mp4CacheStbl *cache, uint32_t sample_idx, int32_t allow_io);
int32_t mp4CacheEnsureStszSample(Mp4CacheStbl *cache, uint32_t sample_idx, int32_t allow_io);
int32_t mp4CacheEnsureStssSample(Mp4CacheStbl *cache, uint32_t sample_idx, int32_t allow_io);
int32_t mp4CacheGetCttsOffset(Mp4CacheCtts *ctts, uint32_t sample_idx, uint32_t *out_offset);
int32_t mp4CacheSumSampleSizesSpan(Mp4CacheStsz *stsz, uint32_t start_idx, uint32_t end_idx, uint32_t *out_sum, Mp4IoStream *stream);
int32_t mp4CacheSumDurationsSpan(Mp4CacheStts *stts, uint32_t start_idx, uint32_t end_idx, uint32_t *out_sum, Mp4IoStream *stream);
int32_t mp4CacheSumDurationsInWindow(Mp4CacheStts *stts, uint32_t start_idx, uint32_t end_idx, uint32_t *out_sum);
int32_t mp4CacheSumDurationsBeyond(Mp4CacheStts *stts, uint32_t end_idx, uint32_t *out_sum, Mp4IoStream *stream);
int32_t mp4CacheStscLookupByChunk(Mp4CacheStsc *stsc, uint32_t chunk_idx, uint32_t *out_first_sample, uint32_t *out_samples_per_chunk, uint32_t *out_desc_idx);
int32_t mp4CacheLoadStcoForSamples(Mp4CacheStbl *cache, uint32_t start_sample, uint32_t count, Mp4IoStream *stream);
int32_t mp4CacheSetTrack(Mp4CacheStbl *cache, Mp4Track *track);
int32_t mp4CacheSetTotals(Mp4CacheStbl *cache, uint32_t total_samples, uint32_t total_dur, uint32_t total_chunks);
int32_t mp4CacheGetChunkCount(Mp4CacheStbl *cache, uint32_t *out_count);
void mp4CacheDestroy(Mp4CacheStbl *cache);
int32_t mp4CacheGetTotalDuration(Mp4CacheStbl *cache, uint32_t *out_dur);
int32_t mp4CacheGetSampleWindow(Mp4CacheStbl *cache, uint32_t *out_start, uint32_t *out_count);

int32_t mp4StblInitDynamic(Mp4Stbl *stbl, Mp4IoStream *stream);
int32_t mp4StblAttachStream(Mp4Stbl *stbl, Mp4IoStream *stream);
int32_t mp4StblDetachFragCtx(Mp4Stbl *stbl);
int32_t mp4StblResetFrags(Mp4Stbl *stbl);
int32_t mp4StblReloadFromStart(Mp4Stbl *stbl);
int32_t mp4StblJumpToMoof(Mp4Stbl *stbl, Mp4MoofBox *moof, uint32_t base_time);
int32_t mp4StblAddRunAndCount(Mp4Stbl *stbl, uint32_t count, uint32_t dur, uint64_t offset, uint32_t sample_size, int32_t sync_flag, uint32_t ctts_off, uint32_t desc_idx);
int32_t mp4StblAddSampleRun(Mp4Stbl *stbl, uint32_t count, uint32_t dur, uint64_t offset, uint32_t sample_size, int32_t sync_flag, uint32_t ctts_off, uint32_t desc_idx);
int32_t mp4StblEndChunk(Mp4Stbl *stbl, int32_t mark_frag);
int32_t mp4StblAdvanceFrags(Mp4Stbl *stbl);
int32_t mp4StblLoadNextFrag(Mp4Stbl *stbl);
int32_t mp4StblEvictFrags(Mp4Stbl *stbl, uint32_t evict_count);
int32_t mp4StblGetChunkInfo(Mp4Stbl *stbl, uint32_t chunk_idx, uint32_t *out_first_sample, uint32_t *out_samples_per_chunk, uint32_t *out_desc_idx);
int32_t mp4StblFindChunkForSample(Mp4Stbl *stbl, uint32_t sample_idx, uint32_t *out_chunk, uint32_t *out_chunk_first_sample);
int32_t mp4StblGetSampleSize(Mp4Stbl *stbl, uint32_t sample_idx, uint32_t *out_size, int32_t allow_io);
uint32_t mp4StscGetWord(Mp4StscBox *stsc, uint32_t row, uint32_t col);
uint32_t mp4SttsGetWord(Mp4SttsBox *stts, uint32_t row, uint32_t col);
uint32_t mp4CttsGetWord(Mp4CttsBox *ctts, uint32_t row, uint32_t col);
int32_t mp4StszSetEntry(Mp4StszBox *stsz, uint32_t index, uint32_t size_val);
int32_t mp4StcoSetEntry(Mp4StcoBox *stco, uint32_t index, uint64_t offset_val);
int32_t mp4StscSetEntry(Mp4StscBox *stsc, uint32_t index, uint32_t col, uint32_t val);
int32_t mp4SttsSetEntry(Mp4SttsBox *stts, uint32_t index, uint32_t col, uint32_t val);
int32_t mp4StssSetEntry(Mp4StssBox *stss, uint32_t index, uint32_t sample_num);
int32_t mp4CttsSetEntry(Mp4CttsBox *ctts, uint32_t index, uint32_t col, uint32_t val);
int32_t mp4StblGetSampleDur(Mp4Stbl *stbl, uint32_t sample_idx, uint32_t *out_dur, int32_t allow_io);
int32_t mp4StblLookupSampleDur(Mp4Stbl *stbl, uint32_t sample_idx, uint32_t *out_dur);
int32_t mp4StblEnsureSample(Mp4Stbl *stbl, uint32_t sample_idx, int32_t allow_io);
int32_t mp4StblSumSampleSizes(Mp4Stbl *stbl, uint32_t start_idx, uint32_t end_idx, uint32_t *out_sum);
int32_t mp4StblGetSampleTime(Mp4Stbl *stbl, uint32_t sample_idx, uint32_t *out_time, uint32_t *out_dur, int32_t allow_io);
int32_t mp4StblLookupSampleTime(Mp4Stbl *stbl, uint32_t sample_idx, uint32_t *out_time, uint32_t *out_dur);
int32_t mp4StblFindSampleByTime(Mp4Stbl *stbl, uint32_t time_val, uint32_t *out_sample, uint32_t *out_is_sync, uint32_t *out_rem, int32_t allow_io);
int32_t mp4StblGetSampleInfo(Mp4Stbl *stbl, uint32_t sample_idx, int32_t want_chunk_rem, Mp4SampleInfo *out_info, int32_t allow_io);
int32_t mp4StblCheckSyncSample(Mp4Stbl *stbl, uint32_t sample_idx, uint32_t *out_is_sync, int32_t allow_io);
int32_t mp4StblLookupSyncSample(Mp4Stbl *stbl, uint32_t sample_idx, uint32_t *out_is_sync);
int32_t mp4StblFindSyncSample(Mp4Stbl *stbl, uint32_t sample_idx, int32_t direction, uint32_t *out_sample, uint32_t *out_delta, int32_t allow_io);
int32_t mp4StblGetSampleCount(Mp4Stbl *stbl, uint32_t *out_count, int32_t allow_io);
int32_t mp4StblGetTotalDuration(Mp4Stbl *stbl, uint32_t *out_dur, int32_t allow_io);
int32_t mp4StblGetChunkCount(Mp4Stbl *stbl, uint32_t *out_count);
int32_t mp4StblLookupSampleSize(Mp4Stbl *stbl, uint32_t sample_idx, uint32_t *out_size);
int32_t mp4StblInitStatic(Mp4Stbl *stbl, Mp4IoStream *stream);
int32_t mp4StblRecalcSizes(Mp4Stbl *stbl);
int32_t mp4StblLookupCtts(Mp4Stbl *stbl, uint32_t sample_idx, uint32_t *out_offset);
int32_t mp4StblUpdateMinBounds(Mp4Stbl *stbl);
int32_t mp4CttsHasNonZero(Mp4CttsBox *ctts);
int32_t mp4StblIsAtInitialFrag(Mp4Stbl *stbl, uint32_t *out_is_init);
int32_t mp4StblGetFragBaseTime(Mp4Stbl *stbl, uint32_t *out_time);

/* ========================================================================= */
/* Section 7: mp4_tables.c (0x02040c..0x023668)                              */
/* ========================================================================= */
int32_t mp4StblGetFragMoofOffset(Mp4Stbl *stbl, uint64_t *out_offset);
int32_t mp4StblAdvanceOneFrag(Mp4Stbl *stbl);
int32_t mp4StblGetLoadedRange(Mp4Stbl *stbl, uint32_t *out_start, uint32_t *out_count);
int32_t mp4StblGetFragRange(Mp4Stbl *stbl, uint32_t *out_has_stss, uint32_t *out_count);
int32_t mp4StblEnsureTime(Mp4Stbl *stbl, uint32_t time_val, int32_t allow_io);
int32_t mp4StblLookupTime(Mp4Stbl *stbl, uint32_t time_val, uint32_t *out_sample, uint32_t *out_rem);
int32_t mp4StblGetSampleOffset(Mp4Stbl *stbl, uint32_t sample_idx, uint64_t *out_offset, uint32_t *out_chunk);
int32_t mp4msv_32AB9DCB(Mp4Handle *handle, uint32_t *out_val);
int32_t mp4FreeOdsmContexts(Mp4Handle *handle);

int32_t mp4DynArrayPush(Mp4DynArray *arr, const void *elem);
int32_t mp4DynArraySet(Mp4DynArray *arr, uint32_t index, const void *elem);
int32_t mp4DynArrayErase(Mp4DynArray *arr, uint32_t index);
Mp4DynBlock *mp4DynArrayGetBlock(Mp4DynArray *arr, uint32_t block_idx);
int32_t mp4DynArrayGrow(Mp4DynArray *arr);
int32_t mp4DynArrayShrink(Mp4DynArray *arr);
int32_t mp4DynBlockCreate(uint32_t alloc_bytes, Mp4DynBlock **out_blk);
void mp4DynBlockFree(Mp4DynBlock *blk);
Mp4DynBlock *mp4DynBlockGetPrev(Mp4DynBlock *blk);
void mp4DynBlockSetPrev(Mp4DynBlock *blk, Mp4DynBlock *prev_blk);
Mp4DynBlock *mp4DynBlockGetNext(Mp4DynBlock *blk);
void mp4DynBlockSetNext(Mp4DynBlock *blk, Mp4DynBlock *next_blk);
int32_t mp4DynBlockGetPtr(Mp4DynBlock *blk, uint32_t elem_offset, void **out_ptr);
int32_t mp4DynBlockWrite(Mp4DynBlock *blk, uint32_t elem_offset, uint32_t len, const uint8_t *src);
int32_t mp4DynBlockInsertBytes(Mp4DynBlock *blk, uint32_t elem_offset, uint32_t len, const uint8_t *src);
int32_t mp4DynBlockEraseBytes(Mp4DynBlock *blk, uint32_t elem_offset, uint32_t len);
int32_t mp4DynArrayCreate(uint32_t elem_size, uint32_t block_cap, Mp4DynArray **out_arr);
void mp4DynArrayDestroy(Mp4DynArray *arr);
int32_t mp4DynArrayCount(Mp4DynArray *arr, uint32_t *out_count);
int32_t mp4DynArrayGet(Mp4DynArray *arr, uint32_t index, void *out_elem);

int32_t mp4OdsmGetObjectAt(void *odsm_ctx, uint16_t index, Mp4OdsmObject **out_obj);
int32_t mp4OdsmFindEsItem(void *odsm_ctx, uint8_t es_index, Mp4OdsmEsItem **out_item);
int32_t mp4OdsmFreeCtx(void *odsm_ctx);
int32_t mp4DescFree(Mp4DescNode *node);
int32_t mp4OdsmGetObjectCount(void *odsm_ctx, uint16_t *out_count);
int32_t mp4OdsmAppendEntry(Mp4OdsmCtx *ctx, uint8_t tag, void *body);
int32_t mp4DescBodyFree(void *body, uint8_t tag);
int32_t mp4DescParseIods(uint32_t size, const uint8_t *buf, Mp4DescNode **out_node);
int32_t mp4DescParseEsds(uint32_t size, const uint8_t *buf, Mp4DescNode **out_node);
int32_t mp4DescParseOdUpdate(Mp4OdsmCtx *ctx, uint32_t size, const uint8_t *buf);
int32_t mp4DescParseEsUpdate(Mp4OdsmCtx *ctx, uint32_t size, const uint8_t *buf);
int32_t mp4DescParseOdEntry(Mp4OdsmCtx *ctx, uint32_t size, const uint8_t *buf);
int32_t mp4DescParseEsIdRef(Mp4OdsmObject *obj, uint32_t size, const uint8_t *buf);
int32_t mp4DescParseEsIdInc(Mp4OdsmObject *obj, uint32_t size, const uint8_t *buf);
int32_t mp4DescParseEsItem(Mp4OdsmCtx *ctx, uint32_t size, const uint8_t *buf);
int32_t mp4DescParseDecConfig(Mp4EsDescBody *body, uint32_t size, const uint8_t *buf);
int32_t mp4DescParseDecSpecific(Mp4EsDescBody *body, uint32_t size, const uint8_t *buf);
int32_t mp4DescParseSlConfig(Mp4EsDescBody *body, uint32_t size, const uint8_t *buf);
int32_t mp4DescParseIodsEsTrack(Mp4IodsBody *iods, uint32_t size, const uint8_t *buf);
int32_t mp4DescReadTagAndLen(const uint8_t *buf, uint32_t rem, uint8_t *out_tag, uint32_t *out_len, uint8_t *out_hdr_bytes);
int32_t mp4BitReaderInit(Mp4BitReader *br, const uint8_t *buf);
uint32_t mp4BitReaderRead(Mp4BitReader *br, uint8_t nbits);
int32_t mp4OdsmParseCommand(uint32_t size, const uint8_t *buf, void **out_ctx);
int32_t mp4OdsmParseUpdateList(Mp4OdsmCtx *ctx, uint32_t size, const uint8_t *buf);

int32_t mp4ChunkArraySumU32(Mp4ChunkArray *head, uint32_t start_idx, uint32_t end_idx, uint32_t col, uint32_t *out_sum);
int32_t mp4ChunkArrayFindGe(Mp4ChunkArray *head, uint32_t target, uint32_t col, uint32_t *out_idx);
int32_t mp4ChunkArrayAppendU32(Mp4ChunkArray **head_ptr, uint32_t cols, const uint32_t *vals, uint32_t chunk_cap);
int32_t mp4ChunkArraySetU32(Mp4ChunkArray **head_ptr, uint32_t row, uint32_t cols, uint32_t col, uint32_t val);
int32_t mp4ChunkArrayGetU32(Mp4ChunkArray *head, uint32_t row, uint32_t col, uint32_t *out_val);
Mp4ChunkArray *mp4ChunkArrayLocate(Mp4ChunkArray *head, uint32_t *row_io);
Mp4ChunkArray *mp4ChunkArrayPruneEmpty(Mp4ChunkArray *head);
int32_t mp4ChunkArrayAppendU64(Mp4ChunkArray **head_ptr, uint32_t cols, const uint64_t *vals, uint32_t chunk_cap);
int32_t mp4ChunkArraySetU64(Mp4ChunkArray **head_ptr, uint32_t row, uint32_t cols, uint32_t col, uint64_t val);
int32_t mp4ChunkArrayGetU64(Mp4ChunkArray *head, uint32_t row, uint32_t col, uint64_t *out_val);
int32_t mp4ChunkArrayInitU32(Mp4ChunkArray *chunk, uint32_t cap, uint32_t cols);
int32_t mp4ChunkArrayInitU64(Mp4ChunkArray *chunk, uint32_t cap, uint32_t cols);

/* ========================================================================= */
/* Section 8: mp4_box_util.c (0x0236d8..0x025250)                            */
/* ========================================================================= */
void mp4FreeMovie(Mp4Movie *movie);
void mp4FreeMoovChildren(Mp4Movie *movie);
void mp4FreeIods(Mp4IodsBox *iods);
void mp4FreeChunkArray(Mp4ChunkArray *head);
void mp4FreeSampleEntry(Mp4SampleEntry *entry);
void mp4FreeExtraBox(Mp4ExtraBox *box);
void mp4FreeEsds(Mp4EsdsBox *esds);
void mp4FreeAvcc(Mp4AvccBox *avcc);
void mp4FreeSinf(Mp4SinfBox *sinf);
void mp4FreeUsmt(Mp4UsmtBox *usmt);
void mp4FreeUdta(Mp4UdtaBox *udta);
void mp4FreeUuidList(Mp4UuidNode *head);
int32_t mp4StszAppend(Mp4StszBox *stsz, uint32_t sample_size);
int32_t mp4StszExpandConst(Mp4StszBox *stsz);
void mp4CalcMoovSize(Mp4Movie *movie);
void mp4CalcTrakSize(Mp4Track *track);
void mp4CalcTrefSize(Mp4TrefBox *tref);
void mp4CalcEdtsSize(Mp4EdtsBox *edts);
void mp4CalcElstSize(Mp4ElstBox *elst);
void mp4CalcMdiaSize(Mp4MdiaBox *mdia);
void mp4CalcMinfSize(Mp4MinfBox *minf);
int32_t mp4CalcStblBoxSize(Mp4Stbl *stbl);
void mp4CalcStsdSizeWrap(Mp4StsdBox *stsd, uint32_t start_idx, uint32_t count, Mp4IoStream *stream);
void mp4CalcStsdSize(Mp4StsdBox *stsd, uint32_t start_idx, uint32_t count, uint32_t io_mode);
void mp4CalcAvccSize(Mp4AvccBox *avcc);
void mp4CalcUsmtSize(Mp4UsmtBox *usmt);
void mp4CalcMtdfSize(Mp4MtdfBox *mtdf);
void mp4CalcMtdtSize(Mp4MtdtBox *mtdt);
void mp4CalcDinfSize(Mp4DinfBox *dinf);
void mp4CalcDrefSize(Mp4DrefBox *dref);
void mp4CalcDrefEntrySize(Mp4DrefEntry *entry);
void mp4CalcProfSize(Mp4ProfBox *prof);
int32_t mp4ChunkArrayAccumFind(Mp4ChunkArray *head, uint32_t *rem_io, uint32_t col, uint32_t *out_idx);
void mp4FreeExtraBoxAlt(Mp4ExtraBox *box);
void mp4FreeTrack(Mp4Track *track);
void mp4FreeTref(Mp4TrefBox *tref);
void mp4FreeTrefType(Mp4TrefTypeBox *tref_type);
void mp4FreeStblData(Mp4Stbl *stbl);
int32_t mp4StcoAppend(Mp4StcoBox *stco, uint64_t offset_val);
int32_t mp4StscAppend(Mp4StscBox *stsc, const uint32_t *triple);
int32_t mp4SttsAppend(Mp4SttsBox *stts, const uint32_t *pair);
int32_t mp4CttsAppend(Mp4CttsBox *ctts, const uint32_t *pair);
int32_t mp4StssAppend(Mp4StssBox *stss, uint32_t sample_num);
Mp4Fsm *mp4StblGetFsm(Mp4Stbl *stbl);

#endif /* MP4MSV_INTERNAL_H */
