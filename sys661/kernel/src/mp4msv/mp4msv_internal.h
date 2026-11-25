#ifndef MP4MSV_INTERNAL_H
#define MP4MSV_INTERNAL_H

#include <stddef.h>

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;
typedef unsigned long long u64;
typedef signed long long s64;

/* MP4 MSV error codes */
enum Mp4Error {
    MP4_OK               = 0,
    MP4_ERR_NOMEM        = 1,
    MP4_ERR_IO           = 2,
    MP4_ERR_BAD_HANDLE   = 4,
    MP4_ERR_INVALID_ARG  = 6,
    MP4_ERR_NOT_OPEN     = 7,
    MP4_ERR_NOT_FOUND    = 9,
    MP4_ERR_INTERNAL     = 10,
    MP4_ERR_NO_BRAND     = 8194,
    MP4_ERR_BAD_STATE    = 8195,
    MP4_ERR_OUT_OF_RANGE = 8201,
    MP4_ERR_PARSE        = 20481,
};

/* ISO BMFF FourCC constants */
#define MP4_FOURCC(a, b, c, d) \
    (((u32)(u8)(a) << 24) | ((u32)(u8)(b) << 16) | ((u32)(u8)(c) << 8) | (u32)(u8)(d))

#define MP4_BOX_FTYP MP4_FOURCC('f', 't', 'y', 'p')
#define MP4_BOX_MOOV MP4_FOURCC('m', 'o', 'o', 'v')
#define MP4_BOX_MVHD MP4_FOURCC('m', 'v', 'h', 'd')
#define MP4_BOX_TRAK MP4_FOURCC('t', 'r', 'a', 'k')
#define MP4_BOX_TKHD MP4_FOURCC('t', 'k', 'h', 'd')
#define MP4_BOX_MDIA MP4_FOURCC('m', 'd', 'i', 'a')
#define MP4_BOX_MDHD MP4_FOURCC('m', 'd', 'h', 'd')
#define MP4_BOX_HDLR MP4_FOURCC('h', 'd', 'l', 'r')
#define MP4_BOX_MINF MP4_FOURCC('m', 'i', 'n', 'f')
#define MP4_BOX_STBL MP4_FOURCC('s', 't', 'b', 'l')
#define MP4_BOX_STSD MP4_FOURCC('s', 't', 's', 'd')
#define MP4_BOX_STTS MP4_FOURCC('s', 't', 't', 's')
#define MP4_BOX_CTTS MP4_FOURCC('c', 't', 't', 's')
#define MP4_BOX_STSC MP4_FOURCC('s', 't', 's', 'c')
#define MP4_BOX_STSZ MP4_FOURCC('s', 't', 's', 'z')
#define MP4_BOX_STCO MP4_FOURCC('s', 't', 'c', 'o')
#define MP4_BOX_CO64 MP4_FOURCC('c', 'o', '6', '4')
#define MP4_BOX_STSS MP4_FOURCC('s', 't', 's', 's')
#define MP4_BOX_AVCC MP4_FOURCC('a', 'v', 'c', 'C')
#define MP4_BOX_ESDS MP4_FOURCC('e', 's', 'd', 's')
#define MP4_BOX_MDAT MP4_FOURCC('m', 'd', 'a', 't')
#define MP4_BOX_MOOF MP4_FOURCC('m', 'o', 'o', 'f')
#define MP4_BOX_UUID MP4_FOURCC('u', 'u', 'i', 'd')
#define MP4_BOX_UDTA MP4_FOURCC('u', 'd', 't', 'a')
#define MP4_BOX_FREE MP4_FOURCC('f', 'r', 'e', 'e')
#define MP4_BOX_SKIP MP4_FOURCC('s', 'k', 'i', 'p')
#define MP4_BOX_PRFT MP4_FOURCC('P', 'R', 'F', 'T')

static inline s32 mp4MinS32(s32 lhs, s32 rhs)
{
    return lhs < rhs ? lhs : rhs;
}

static inline u16 mp4ReadBe16(const u8 *buf)
{
    return (u16)(((u16)buf[0] << 8) | buf[1]);
}

static inline u32 mp4ReadBe32(const u8 *buf)
{
    return ((u32)buf[0] << 24) | ((u32)buf[1] << 16) | ((u32)buf[2] << 8) | buf[3];
}

static inline u64 mp4ReadBe64(const u8 *buf)
{
    return ((u64)mp4ReadBe32(buf) << 32) | mp4ReadBe32(buf + 4);
}

static inline void mp4WriteBe16(u8 *buf, u16 val)
{
    buf[0] = (u8)(val >> 8);
    buf[1] = (u8)val;
}

static inline void mp4WriteBe32(u8 *buf, u32 val)
{
    buf[0] = (u8)(val >> 24);
    buf[1] = (u8)(val >> 16);
    buf[2] = (u8)(val >> 8);
    buf[3] = (u8)val;
}

static inline void mp4WriteBe64(u8 *buf, u64 val)
{
    mp4WriteBe32(buf, (u32)(val >> 32));
    mp4WriteBe32(buf + 4, (u32)val);
}

/* Host OS / I/O / allocator callback table */
typedef struct Mp4OsCallbacks {
    void *(*open)(const char *path, u32 mode);
    int (*close)(void *fh);
    s64 (*tell)(void *fh);
    int (*seek)(void *fh, s64 offset, int whence);
    s64 (*get_size)(void *fh);
    s32 (*read)(void *fh, void *buf, u32 size);
    s32 (*write)(void *fh, const void *buf, u32 size);
    void *(*alloc)(u32 size);
    void (*free)(void *ptr);
    u32 (*get_time)(void);
    void *(*memcpy_fn)(void *dst, const void *src, u32 len);
    void *(*memmove_fn)(void *dst, const void *src, u32 len);
    void *(*memset_fn)(void *dst, int val, u32 len);
    int (*memcmp_fn)(const void *lhs, const void *rhs, u32 len);
    int (*lock_fn)(void *ctx);
    int (*unlock_fn)(void *ctx);
    int (*flush)(void *fh);
} Mp4OsCallbacks;

/* Global cache configuration */
typedef struct Mp4Config {
    u32 base_unit;
    u32 min_mult;
    u32 cache_mult;
    u32 max_mult;
    u32 flags;
} Mp4Config;

/* Parser/muxer state machine */
typedef struct Mp4StateMachine {
    u32 cur_state;
    u32 pending_state;
} Mp4StateMachine;

/* Buffered stream context */
typedef struct Mp4IoStream {
    void *file_handle;
    u32 error_flags;
    u32 last_box_type;
    u32 mode;
    u32 base_unit;
    u32 min_read;
    u32 cache_size;
    u32 max_read;
    u32 reserved0;
    void *user_handle;
    u32 is_attached;
    u32 reserved1;
    u64 file_pos;
    u64 file_size;
    u8 *cache_buf;
    u32 cache_pos;
    u32 cache_valid;
    u32 io_mode;
    Mp4StateMachine *fsm;
} Mp4IoStream;

/* Chunked dynamic array for MP4 sample tables */
typedef struct Mp4TableChunk {
    u32 count;
    u32 capacity;
    u32 stride_words;
    u32 *entries;
    u32 reserved[3];
    struct Mp4TableChunk *next;
} Mp4TableChunk;

/* Sample-to-chunk (stsc) lookup table */
typedef struct Mp4StscTable {
    u32 valid;
    u32 reserved[7];
    Mp4TableChunk *chunks;
    u32 chunk_count;
    u32 first_entry;
    u32 entry_count;
    u32 first_sample;
    u32 sample_count;
    u32 first_chunk;
    u32 total_chunks;
    u32 cached_entry;
    u32 cached_sample;
} Mp4StscTable;

/* Sample-to-time (stts) and composition-time (ctts) entry */
typedef struct Mp4TimeToSampleEntry {
    u32 sample_count;
    u32 sample_delta;
} Mp4TimeToSampleEntry;

/* Sample description / codec config */
typedef struct Mp4CodecConfig {
    u32 format;
    u32 width;
    u32 height;
    u32 sample_rate;
    u32 channels;
    u32 sample_size;
    u8 *sps_data;
    u32 sps_size;
    u8 *pps_data;
    u32 pps_size;
    u8 *esds_data;
    u32 esds_size;
    u8 avc_profile;
    u8 avc_compat;
    u8 avc_level;
    u8 nal_length_size;
} Mp4CodecConfig;

/* UUID box entry */
typedef struct Mp4UuidBox {
    struct Mp4UuidBox *next;
    u32 size;
    u64 file_offset;
    u32 uuid[4];
} Mp4UuidBox;

/* Track reference / profile entry */
typedef struct Mp4ProfileItem {
    u32 id;
    u32 type;
    u16 ref_index;
    u16 reserved;
    u32 flags;
    u32 param0;
    u32 param1;
    u32 param2;
    struct Mp4ProfileItem *next;
} Mp4ProfileItem;

/* Metadata / user data entry */
typedef struct Mp4MetaItem {
    u32 type;
    u32 size;
    u16 lang;
    u16 reserved;
    void *data;
    struct Mp4MetaItem *next;
} Mp4MetaItem;

/* Media sample index & table container */
typedef struct Mp4SampleTable {
    Mp4StscTable stsc;
    Mp4TableChunk *stts_chunks;
    u32 stts_count;
    Mp4TableChunk *ctts_chunks;
    u32 ctts_count;
    Mp4TableChunk *stsz_chunks;
    u32 stsz_fixed_size;
    u32 stsz_count;
    Mp4TableChunk *stco_chunks;
    u32 stco_count;
    u32 stco_is_64bit;
    Mp4TableChunk *stss_chunks;
    u32 stss_count;
    u32 max_sample_size;
    u32 cur_sample_idx;
} Mp4SampleTable;

/* Forward declaration */
struct Mp4Movie;
struct Mp4Handle;

/* Track descriptor */
typedef struct Mp4Track {
    struct Mp4Track *next;
    struct Mp4Movie *movie;
    u32 track_id;
    u32 handler_type;
    u32 timescale;
    u64 duration;
    u32 creation_time;
    u32 modification_time;
    u16 language;
    u16 volume;
    u32 width_fixed;
    u32 height_fixed;
    u32 flags;
    Mp4CodecConfig codec;
    Mp4SampleTable samples;
    u32 edit_count;
    u32 *edit_list;
    u32 ref_count;
    u32 *ref_tracks;
    void *extra_data;
} Mp4Track;

/* Movie fragment descriptor */
typedef struct Mp4Fragment {
    struct Mp4Fragment *next;
    struct Mp4Movie *movie;
    u32 seq_number;
    u64 base_offset;
    u32 sample_count;
    u32 flags;
} Mp4Fragment;

/* File-type (ftyp) box */
typedef struct Mp4FtypBox {
    u32 major_brand;
    u32 minor_version;
    u32 compat_count;
    u32 compat_brands[16];
} Mp4FtypBox;

/* Top-level MP4 movie container */
typedef struct Mp4Movie {
    struct Mp4Handle *handle;
    u32 flags;
    u32 track_count;
    Mp4Track *tracks;
    u32 timescale;
    u64 duration;
    u32 creation_time;
    u32 modification_time;
    u32 next_track_id;
    u32 rate_fixed;
    u16 volume_fixed;
    u16 reserved0;
    Mp4FtypBox *ftyp;
    u64 mdat_offset;
    u64 mdat_size;
    Mp4UuidBox *uuid_list;
    u32 fragment_count;
    Mp4Fragment *fragments;
    u32 profile_count;
    Mp4ProfileItem *profiles;
    Mp4MetaItem *meta_list;
    void *fragment_index;
} Mp4Movie;

/* Public opaque MP4 handle */
typedef struct Mp4Handle {
    Mp4Movie *movie;
    Mp4IoStream *stream;
    u32 options;
} Mp4Handle;

/* Globals */
extern Mp4Config g_mp4_config;
extern Mp4OsCallbacks g_mp4_os_cbs;
extern const u32 g_mp4_state_table[360];
extern const u32 g_mp4_err_table[28];

/* Memory & OS helpers (mp4_io.c) */
void *mp4Alloc(u32 size);
void mp4Free(void *ptr);
void *mp4MemCopy(void *dst, const void *src, u32 len);
void *mp4MemSet(void *dst, int val, u32 len);
s64 mp4IoTell(Mp4IoStream *stream);
int mp4IoSeek(Mp4IoStream *stream, s64 offset, int whence);
s64 mp4IoGetSize(Mp4IoStream *stream);
s32 mp4IoRead(Mp4IoStream *stream, void *buf, u32 size);
s32 mp4IoWrite(Mp4IoStream *stream, const void *buf, u32 size);
int mp4IoFlush(Mp4IoStream *stream);
u16 mp4IoReadU16(Mp4IoStream *stream);
u32 mp4IoReadU32(Mp4IoStream *stream);
u64 mp4IoReadU64(Mp4IoStream *stream);
int mp4IoReadBytes(Mp4IoStream *stream, void *buf, u32 size);
int mp4IoWriteU16(Mp4IoStream *stream, u16 val);
int mp4IoWriteU32(Mp4IoStream *stream, u32 val);
int mp4IoWriteU64(Mp4IoStream *stream, u64 val);
int mp4IoWriteBytes(Mp4IoStream *stream, const void *buf, u32 size);
void mp4IoRecordError(Mp4IoStream *stream, u32 box_type, u32 err_flag);

/* State machine helpers (mp4_box.c) */
int mp4StateCreate(Mp4StateMachine **out_fsm);
void mp4StateDestroy(Mp4StateMachine *fsm);
int mp4StateCheck(Mp4StateMachine *fsm, u32 action);
void mp4StateCommit(Mp4StateMachine *fsm);
int mp4ParseTopLevel(Mp4Movie *movie, u32 parse_mask, Mp4IoStream *stream);

/* Track helpers (mp4_track.c) */
Mp4Track *mp4TrackCreate(Mp4Movie *movie, u32 handler_type);
void mp4TrackDestroy(Mp4Track *track);
Mp4Track *mp4TrackGetByIndex(Mp4Movie *movie, u32 index);
Mp4Track *mp4TrackGetById(Mp4Movie *movie, u32 track_id);
int mp4MovieCreate(Mp4Handle *handle);
int mp4MovieDestroy(Mp4Handle *handle);
int mp4ParseTrakBox(Mp4Movie *movie, Mp4IoStream *stream, u64 end_pos);

/* Sample table helpers (mp4_sample.c) */
Mp4TableChunk *mp4ChunkAlloc(u32 capacity, u32 stride_words);
void mp4ChunkListFree(Mp4TableChunk *head);
int mp4ChunkGetWord(const Mp4TableChunk *head, u32 row, u32 col, u32 *out_val);
int mp4ChunkAppendRow(Mp4TableChunk **head_ptr, u32 stride_words, const u32 *row_words);
int mp4StscLookup(Mp4StscTable *stsc, u32 sample_num, u32 *out_chunk,
                  u32 *out_first_sample, u32 *out_desc_idx);
int mp4SampleGetInfo(Mp4Track *track, u32 sample_idx, u64 *out_offset,
                     u32 *out_size, u64 *out_dts, s32 *out_cts, u32 *out_sync);
int mp4SampleTimeToIndex(Mp4Track *track, u64 target_time, u32 *out_idx);
int mp4SampleFindSync(Mp4Track *track, u32 sample_idx, int forward, u32 *out_idx);
int mp4SampleAppend(Mp4Track *track, u64 offset, u32 size, u32 duration, s32 cts_offset, int is_sync);

/* Codec config helpers (mp4_codec.c) */
int mp4ParseAvcC(Mp4CodecConfig *codec, Mp4IoStream *stream, u64 end_pos);
int mp4ParseEsds(Mp4CodecConfig *codec, Mp4IoStream *stream, u64 end_pos);
void mp4CodecConfigFree(Mp4CodecConfig *codec);
int mp4WriteAvcC(const Mp4CodecConfig *codec, Mp4IoStream *stream);

/* Public APIs referenced internally */
int mp4msv_9CA13D1A(int enable, const Mp4OsCallbacks *cbs);
int mp4msv_2A2D949A(void);
Mp4Config *mp4msv_4A7AD789(void);
int mp4msv_3C2183C7(int enable, const Mp4Config *cfg);
int mp4msv_2A296DDA(Mp4Movie *movie, u32 index, Mp4MetaItem **out_item);

#endif /* MP4MSV_INTERNAL_H */
