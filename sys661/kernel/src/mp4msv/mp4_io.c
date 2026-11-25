#include "mp4msv_internal.h"

void *mp4Alloc(u32 size)
{
    if (size == 0 || g_mp4_os_cbs.alloc == NULL)
        return NULL;
    return g_mp4_os_cbs.alloc(size);
}

void mp4Free(void *ptr)
{
    if (ptr != NULL && g_mp4_os_cbs.free != NULL)
        g_mp4_os_cbs.free(ptr);
}

void *mp4MemCopy(void *dst, const void *src, u32 len)
{
    if (g_mp4_os_cbs.memcpy_fn != NULL)
        return g_mp4_os_cbs.memcpy_fn(dst, src, len);
    u8 *out = (u8 *)dst;
    const u8 *in = (const u8 *)src;
    while (len-- > 0)
        *out++ = *in++;
    return dst;
}

void *mp4MemSet(void *dst, int val, u32 len)
{
    if (g_mp4_os_cbs.memset_fn != NULL)
        return g_mp4_os_cbs.memset_fn(dst, val, len);
    u8 *out = (u8 *)dst;
    while (len-- > 0)
        *out++ = (u8)val;
    return dst;
}

s64 mp4IoTell(Mp4IoStream *stream)
{
    if (stream == NULL || g_mp4_os_cbs.tell == NULL)
        return -1;
    return g_mp4_os_cbs.tell(stream->file_handle);
}

int mp4IoSeek(Mp4IoStream *stream, s64 offset, int whence)
{
    if (stream == NULL || g_mp4_os_cbs.seek == NULL)
        return MP4_ERR_IO;
    int ret = g_mp4_os_cbs.seek(stream->file_handle, offset, whence);
    if (ret == 0 && whence == 0)
        stream->file_pos = (u64)offset;
    return ret;
}

s64 mp4IoGetSize(Mp4IoStream *stream)
{
    if (stream == NULL || g_mp4_os_cbs.get_size == NULL)
        return -1;
    return g_mp4_os_cbs.get_size(stream->file_handle);
}

s32 mp4IoRead(Mp4IoStream *stream, void *buf, u32 size)
{
    if (stream == NULL || g_mp4_os_cbs.read == NULL)
        return -1;
    s32 n = g_mp4_os_cbs.read(stream->file_handle, buf, size);
    if (n > 0)
        stream->file_pos += (u32)n;
    else if (n < 0)
        stream->error_flags |= 0x80000000U;
    return n;
}

s32 mp4IoWrite(Mp4IoStream *stream, const void *buf, u32 size)
{
    if (stream == NULL || g_mp4_os_cbs.write == NULL)
        return -1;
    s32 n = g_mp4_os_cbs.write(stream->file_handle, buf, size);
    if (n > 0)
        stream->file_pos += (u32)n;
    else if (n < 0)
        stream->error_flags |= 0x80000000U;
    return n;
}

int mp4IoFlush(Mp4IoStream *stream)
{
    if (stream == NULL || g_mp4_os_cbs.flush == NULL)
        return MP4_OK;
    return g_mp4_os_cbs.flush(stream->file_handle);
}

void mp4IoRecordError(Mp4IoStream *stream, u32 box_type, u32 err_flag)
{
    if (stream != NULL) {
        stream->last_box_type = box_type;
        stream->error_flags |= err_flag;
    }
}

int mp4IoReadBytes(Mp4IoStream *stream, void *buf, u32 size)
{
    if (size == 0)
        return MP4_OK;
    s32 got = mp4IoRead(stream, buf, size);
    if (got < (s32)size) {
        mp4IoRecordError(stream, 0, 0x20000000U);
        return MP4_ERR_PARSE;
    }
    return MP4_OK;
}

u16 mp4IoReadU16(Mp4IoStream *stream)
{
    u8 raw[2] = { 0, 0 };
    if (mp4IoReadBytes(stream, raw, 2) != MP4_OK)
        return 0;
    return mp4ReadBe16(raw);
}

u32 mp4IoReadU32(Mp4IoStream *stream)
{
    u8 raw[4] = { 0, 0, 0, 0 };
    if (mp4IoReadBytes(stream, raw, 4) != MP4_OK)
        return 0;
    return mp4ReadBe32(raw);
}

u64 mp4IoReadU64(Mp4IoStream *stream)
{
    u8 raw[8] = { 0 };
    if (mp4IoReadBytes(stream, raw, 8) != MP4_OK)
        return 0;
    return mp4ReadBe64(raw);
}

int mp4IoWriteBytes(Mp4IoStream *stream, const void *buf, u32 size)
{
    if (size == 0)
        return MP4_OK;
    s32 put = mp4IoWrite(stream, buf, size);
    if (put < (s32)size)
        return MP4_ERR_IO;
    return MP4_OK;
}

int mp4IoWriteU16(Mp4IoStream *stream, u16 val)
{
    u8 raw[2];
    mp4WriteBe16(raw, val);
    return mp4IoWriteBytes(stream, raw, 2);
}

int mp4IoWriteU32(Mp4IoStream *stream, u32 val)
{
    u8 raw[4];
    mp4WriteBe32(raw, val);
    return mp4IoWriteBytes(stream, raw, 4);
}

int mp4IoWriteU64(Mp4IoStream *stream, u64 val)
{
    u8 raw[8];
    mp4WriteBe64(raw, val);
    return mp4IoWriteBytes(stream, raw, 8);
}
