#ifndef PAF_STD_TTY_H
#define PAF_STD_TTY_H

#include "../compiler.h"
#include <scetypes.h>

namespace paf {

class TTY {
public:
	TTY();
	~TTY();

	SceUID m_stdout;
	SceUID m_stdin;
	SceUID m_extra_tty;
};

}

extern "C" {
int write_to_stdout(const void *buf, SceSize nbyte);
int read_char_from_stdin(void);
}

#endif
