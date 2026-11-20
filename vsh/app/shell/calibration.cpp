/*
 * The motion sensors' calibration, updated by a thread of its own while the shell runs
 * (0x810E181C-0x810E1944): it starts the calibration library linked into the shell, reads its
 * state every 125 vertical blanks, every 2 while the sensors measure in its fast mode, and ends
 * it when the thread stops. The thread's name is Sony's ("SceCalibrationUpdate"); the file's, the
 * class's and the library functions' names are ours (PLACEHOLDER): no record names them.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <display.h>
#include "shell.h"

/* PLACEHOLDER: the calibration library's state (0x8144CF70 fills it) */
struct ShellCalibrationState {
	int count;			/* the samples taken */
	int reserved;			/* 0 */
	int version;			/* 1 */
	int updated;			/* the calibration changed since the last read */
	SceUInt8 measMode;		/* sceMotionDevGetMeasMode's mode; bit 2 the fast one */
};

#define SHELL_CALIBRATION_MEAS_FAST	0x04

extern "C" {
/* PLACEHOLDER names: the calibration library's start (0x8144AB70), state (0x8144CF70) and end
   (0x8144AE78) */
int shellMotionCalibInit(void);
int shellMotionCalibGetState(ShellCalibrationState *state);
int shellMotionCalibTerm(void);
}

/* PLACEHOLDER: the thread updating the calibration (8 bytes) */
class ShellCalibrationUpdater {
public:
	ShellCalibrationUpdater() : thread(-1), running(false)
	{
	}

	~ShellCalibrationUpdater()
	{
		Reset();
	}

	void Reset()
	{
		running = false;
		thread = -1;
	}

	SceUID thread;
	bool running;
};

static ShellCalibrationUpdater *s_calibrationUpdater;

/* (0x810E18E6) */
static int shellCalibrationUpdateThread(SceSize args, void *argp)
{
	if (shellMotionCalibInit() == 0) {
		ShellCalibrationUpdater *updater = *(ShellCalibrationUpdater **)argp;

		while (updater->running) {
			ShellCalibrationState state;

			shellMotionCalibGetState(&state);
			if ((state.measMode & SHELL_CALIBRATION_MEAS_FAST) != 0)
				sceDisplayWaitVblankStartMulti(2);
			else
				sceDisplayWaitVblankStartMulti(125);
		}
	}
	shellMotionCalibTerm();
	return 0;
}

/* (0x810E181C) */
void shellCalibrationUpdateThreadCreate(void)
{
	if (s_calibrationUpdater != NULL)
		return;

	ShellCalibrationUpdater *updater = new ShellCalibrationUpdater();

	s_calibrationUpdater = updater;
	if (updater == NULL)
		return;

	int ret = sceKernelCreateThread("SceCalibrationUpdate", shellCalibrationUpdateThread,
					0x1000010A, 0x1000, 0, 0, NULL);

	/* the thread's argument, the updater; Sony passes 8 bytes from the 4-byte pointer */
	ShellCalibrationUpdater *arg = updater;

	if (ret >= 0) {
		updater->thread = ret;
		updater->running = true;
		if (sceKernelStartThread(ret, 8, &arg) >= 0)
			return;
		ret = sceKernelDeleteThread(updater->thread);
		updater->Reset();
	}
	if (ret < 0) {
		delete s_calibrationUpdater;
		s_calibrationUpdater = NULL;
	}
}
