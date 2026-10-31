#ifndef IPMI_PSP2_EVENT_FLAG_H
#define IPMI_PSP2_EVENT_FLAG_H

#include <stddef.h>
#include <kernel.h>

namespace IPMI {
namespace impl {
namespace psp2 {

/* An event flag libipmi created (and deletes) or opened by name (and closes). */
class EventFlag {
public:
	EventFlag();

	int create(const char *name);
	int open(const char *name);
	int close();
	int set(SceUInt32 bits);
	int poll(SceUInt32 bits, SceUInt32 waitMode, SceUInt32 *resultBits);
	int wait(SceUInt32 bits, SceUInt32 waitMode, SceUInt32 *resultBits,
		 SceUInt32 *timeout);
	int destroy();

private:
	SceUID m_id;
	bool m_owner;
};

}
}
}

#endif
