/*
 * The GPU's clock: three levels, from the fastest, and the frequency saved to
 * go back to.
 */
#include <graphics/surface/surface_display.h>

/* ScePower's GPU clock, in MHz (no SDK header declares them) */
extern "C" {
int scePowerGetGpuClockFrequency(void);
int scePowerSetGpuClockFrequency(int freq);
}

enum {
	GPU_CLOCK_LEVEL_HIGH = 0,
	GPU_CLOCK_LEVEL_MIDDLE = 1,
	GPU_CLOCK_LEVEL_LOW = 2,
	/* back to the frequency saved */
	GPU_CLOCK_LEVEL_SAVED = 255
};

/* the frequency set last, -1 once back to the one saved */
static int32_t s_gpu_clock = -1;
static int32_t s_saved_gpu_clock;

int32_t paf::GetGpuClockLevel()
{
	int32_t freq = scePowerGetGpuClockFrequency();

	if (freq <= 41)
		return GPU_CLOCK_LEVEL_LOW;
	if (freq <= 111)
		return GPU_CLOCK_LEVEL_MIDDLE;
	return GPU_CLOCK_LEVEL_HIGH;
}

/* sets the level's frequency when it is not the one set last */
static int32_t SetGpuClock(int32_t freq)
{
	if (s_gpu_clock == freq)
		return 0;
	s_gpu_clock = freq;
	return scePowerSetGpuClockFrequency(freq);
}

int32_t paf::SetGpuClockLevel(int32_t level)
{
	if (level == GPU_CLOCK_LEVEL_SAVED) {
		if (s_gpu_clock == -1)
			return 0;
		s_gpu_clock = -1;
		if (s_saved_gpu_clock == 0)
			return 0;
		return scePowerSetGpuClockFrequency(s_saved_gpu_clock);
	}
	if (level == GPU_CLOCK_LEVEL_HIGH)
		return SetGpuClock(166);
	if (level == GPU_CLOCK_LEVEL_LOW)
		return SetGpuClock(1);
	return SetGpuClock(111);
}

int32_t paf::SaveGpuClock()
{
	s_saved_gpu_clock = scePowerGetGpuClockFrequency();
	return s_saved_gpu_clock;
}
