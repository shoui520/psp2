#include "../../../../../naac_internal.h"

int32_t __f98CreateFloatFifoBuffer(NaacFloatFifo *fifo, float *backing, int32_t capacity)
{
	if (fifo == NULL || backing == NULL || capacity <= 0)
		return NAAC_ERR_INVALID_PARAM;

	fifo->samples = backing;
	fifo->capacity = capacity;
	fifo->readIndex = 0;
	fifo->writeIndex = 0;
	fifo->count = 0;
	sceClibMemset(backing, 0, (size_t)capacity * sizeof(float));
	return NAAC_OK;
}

int32_t CreateFloatModuloBuffer(NaacFloatModulo *mod, float *backing, int32_t length)
{
	if (mod == NULL || backing == NULL || length <= 0)
		return NAAC_ERR_INVALID_PARAM;

	mod->samples = backing;
	mod->length = length;
	mod->cursor = 0;
	sceClibMemset(backing, 0, (size_t)length * sizeof(float));
	return NAAC_OK;
}

void naacFifoPush(NaacFloatFifo *fifo, const float *src, int32_t count)
{
	if (fifo == NULL || src == NULL || count <= 0)
		return;
	for (int32_t i = 0; i < count; i++) {
		fifo->samples[fifo->writeIndex] = src[i];
		fifo->writeIndex++;
		if (fifo->writeIndex >= fifo->capacity)
			fifo->writeIndex = 0;
		if (fifo->count < fifo->capacity) {
			fifo->count++;
		} else {
			fifo->readIndex++;
			if (fifo->readIndex >= fifo->capacity)
				fifo->readIndex = 0;
		}
	}
}

void naacFifoPop(NaacFloatFifo *fifo, float *dst, int32_t count)
{
	if (fifo == NULL || dst == NULL || count <= 0)
		return;
	for (int32_t i = 0; i < count; i++) {
		if (fifo->count > 0) {
			dst[i] = fifo->samples[fifo->readIndex];
			fifo->readIndex++;
			if (fifo->readIndex >= fifo->capacity)
				fifo->readIndex = 0;
			fifo->count--;
		} else {
			dst[i] = 0.0f;
		}
	}
}
