#ifndef PAF_TTY_H
#define PAF_TTY_H

#include "../compiler.h"
#include <scetypes.h>

typedef struct TTY {
	SceUID fd_stdout;
	SceUID fd_stdin;
	SceUID fd_extratty;
} TTY;

typedef char TTYSizeCheck[sizeof(TTY) == 12 ? 1 : -1];

/* Sony: paf::TTY constructors and destructors. */
TTY *ScePafStdc_112FF129(TTY *self);
TTY *ScePafStdc_DF1F6AB5(TTY *self);
TTY *ScePafStdc_CAFCD794(TTY *self);
TTY *ScePafStdc_A6AA1C1B(TTY *self);
/* Sony: write_to_stdout and read_char_from_stdin. */
int ScePafStdc_913B81F5(const void *buf, SceSize nbyte);
int ScePafStdc_7FA92616(void);
int TTYInitialize(void);

#endif
