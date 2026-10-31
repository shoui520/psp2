/*
 * paf::inputdevice::pad: the buttons and sticks, read through SceCtrl.
 *
 * PROVISIONAL: what inputdevice.cpp calls, until pad.c (0x810440a4..) is converted.
 */
#ifndef PAF_INPUTDEVICE_PAD_PAD_H
#define PAF_INPUTDEVICE_PAD_PAD_H

#include <stdint.h>

namespace paf {
namespace inputdevice {
namespace pad {

class Data;

class InitParam {
public:
	int32_t m_mode;
};

int32_t Init(InitParam &init_param);
int32_t End();
int32_t Update(float diff_time, const Data **data, const Data **data_pre);
void Lock();
void Unlock();
bool IsOwnershipReleasable();

}
}
}

/* The pad locked for a time; no Sony name found for the NID. */
extern "C" void ScePafMisc_FA86D1FA(float time);

#endif /* PAF_INPUTDEVICE_PAD_PAD_H */
