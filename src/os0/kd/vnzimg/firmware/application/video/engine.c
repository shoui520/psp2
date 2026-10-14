/*
 * The video engine (0x824c96-): the codecs' service on the other cores,
 * answering the decoders' and the encoder's requests (avcdec.c, m4vdec.c,
 * avcenc.c).
 */
#include "video.h"

#define VIDEO_ENGINE_VERSION		0x01070a97

uint32_t videoEngineVersion(void)
{
	return VIDEO_ENGINE_VERSION;
}
