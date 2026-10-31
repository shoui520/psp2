/*
 * paf::inputdevice::btrm: the Bluetooth remote, read through SceAppMgr's Btrm.
 *
 * PROVISIONAL: what inputdevice.cpp calls, until btrm (0x81046b3c..) is converted.
 */
#ifndef PAF_INPUTDEVICE_BTRM_BTRM_H
#define PAF_INPUTDEVICE_BTRM_BTRM_H

#include <stdint.h>

namespace paf {
namespace inputdevice {
namespace btrm {

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
int32_t Activate();
int32_t Deactivate();
bool IsOwnershipReleasable();

}
}
}

/* The Bluetooth remote locked for a time; no Sony name found for the NID. */
extern "C" void ScePafMisc_F9C3A155(float time);

#endif /* PAF_INPUTDEVICE_BTRM_BTRM_H */
