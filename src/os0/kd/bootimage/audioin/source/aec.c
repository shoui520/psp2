/* AEC capture adapters; original source filename unavailable. */
#include "audioin_internal.h"

extern int initializeAudioInAecMutex(void);
extern int createAudioInAecContext(AudioInAecContext *context);
extern int processAudioInAecContext(AudioInAecContext *context, int gain,
		unsigned int flags);
extern int configureAudioInAecContext(AudioInAecContext *context,
		const void *control);

int initializeAudioInAecLibrary(void)
{
	return initializeAudioInAecMutex();
}

int startAudioInAec(AudioInAecContext *context)
{
	context->size = sizeof(*context);
	return createAudioInAecContext(context);
}

int processAudioInAecBuffers(AudioInAecContext *context, int16_t *first,
		int16_t *second, int gain, unsigned int flags)
{
	context->sampleBuffers[0] = first;
	context->sampleBuffers[1] = second;
	return processAudioInAecContext(context, gain, flags);
}

int configureAudioInAec(AudioInAecContext *context, const void *control)
{
	return configureAudioInAecContext(context, control);
}
