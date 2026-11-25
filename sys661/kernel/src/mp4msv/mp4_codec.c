#include "mp4msv_internal.h"

int mp4ParseAvcC(Mp4CodecConfig *codec, Mp4IoStream *stream, u64 end_pos)
{
    if (codec == NULL || stream == NULL)
        return MP4_ERR_INVALID_ARG;
    s64 pos = mp4IoTell(stream);
    if (pos < 0 || (u64)pos + 6ULL > end_pos) {
        mp4IoRecordError(stream, MP4_BOX_AVCC, 0x20000000U);
        return MP4_ERR_PARSE;
    }

    u8 hdr[6];
    if (mp4IoReadBytes(stream, hdr, 6) != MP4_OK)
        return MP4_ERR_PARSE;

    codec->avc_profile = hdr[1];
    codec->avc_compat = hdr[2];
    codec->avc_level = hdr[3];
    codec->nal_length_size = (u8)((hdr[4] & 0x03U) + 1U);

    u8 num_sps = (u8)(hdr[5] & 0x1FU);
    if (num_sps > 0 && (u64)mp4IoTell(stream) + 2ULL <= end_pos) {
        u16 sps_len = mp4IoReadU16(stream);
        if (sps_len > 0 && (u64)mp4IoTell(stream) + sps_len <= end_pos) {
            codec->sps_data = (u8 *)mp4Alloc(sps_len);
            if (codec->sps_data == NULL)
                return MP4_ERR_NOMEM;
            if (mp4IoReadBytes(stream, codec->sps_data, sps_len) != MP4_OK)
                return MP4_ERR_PARSE;
            codec->sps_size = sps_len;
        }
    }

    if ((u64)mp4IoTell(stream) + 1ULL <= end_pos) {
        u8 num_pps = 0;
        if (mp4IoReadBytes(stream, &num_pps, 1) == MP4_OK && num_pps > 0 &&
            (u64)mp4IoTell(stream) + 2ULL <= end_pos) {
            u16 pps_len = mp4IoReadU16(stream);
            if (pps_len > 0 && (u64)mp4IoTell(stream) + pps_len <= end_pos) {
                codec->pps_data = (u8 *)mp4Alloc(pps_len);
                if (codec->pps_data == NULL)
                    return MP4_ERR_NOMEM;
                if (mp4IoReadBytes(stream, codec->pps_data, pps_len) != MP4_OK)
                    return MP4_ERR_PARSE;
                codec->pps_size = pps_len;
            }
        }
    }
    return MP4_OK;
}

int mp4ParseEsds(Mp4CodecConfig *codec, Mp4IoStream *stream, u64 end_pos)
{
    if (codec == NULL || stream == NULL)
        return MP4_ERR_INVALID_ARG;
    s64 pos = mp4IoTell(stream);
    if (pos < 0 || (u64)pos + 4ULL > end_pos)
        return MP4_ERR_PARSE;

    mp4IoReadU32(stream);
    u32 rem = (u32)(end_pos - (u64)mp4IoTell(stream));
    if (rem > 0) {
        codec->esds_data = (u8 *)mp4Alloc(rem);
        if (codec->esds_data == NULL)
            return MP4_ERR_NOMEM;
        if (mp4IoReadBytes(stream, codec->esds_data, rem) != MP4_OK)
            return MP4_ERR_PARSE;
        codec->esds_size = rem;
    }
    return MP4_OK;
}

void mp4CodecConfigFree(Mp4CodecConfig *codec)
{
    if (codec == NULL)
        return;
    mp4Free(codec->sps_data);
    mp4Free(codec->pps_data);
    mp4Free(codec->esds_data);
    mp4MemSet(codec, 0, sizeof(*codec));
}

int mp4WriteAvcC(const Mp4CodecConfig *codec, Mp4IoStream *stream)
{
    if (codec == NULL || stream == NULL)
        return MP4_ERR_INVALID_ARG;
    u32 box_size = 8U + 7U + (codec->sps_size ? (2U + codec->sps_size) : 0U) +
                   (codec->pps_size ? (2U + codec->pps_size) : 0U);
    mp4IoWriteU32(stream, box_size);
    mp4IoWriteU32(stream, MP4_BOX_AVCC);

    u8 hdr[6];
    hdr[0] = 1;
    hdr[1] = codec->avc_profile;
    hdr[2] = codec->avc_compat;
    hdr[3] = codec->avc_level;
    hdr[4] = (u8)(0xFCU | ((codec->nal_length_size > 0 ? (codec->nal_length_size - 1U) : 3U) & 0x03U));
    hdr[5] = (u8)(0xE0U | (codec->sps_size ? 1U : 0U));
    mp4IoWriteBytes(stream, hdr, 6);

    if (codec->sps_size > 0) {
        mp4IoWriteU16(stream, (u16)codec->sps_size);
        mp4IoWriteBytes(stream, codec->sps_data, codec->sps_size);
    }

    u8 num_pps = codec->pps_size ? 1U : 0U;
    mp4IoWriteBytes(stream, &num_pps, 1);
    if (codec->pps_size > 0) {
        mp4IoWriteU16(stream, (u16)codec->pps_size);
        mp4IoWriteBytes(stream, codec->pps_data, codec->pps_size);
    }
    return MP4_OK;
}
