/*
 * paf::get_time_internal: the process time since it was first called, in
 * microseconds. The time of the previous call is kept too, but only the first
 * call's is counted from.
 */
#include <stdint.h>
#include <scetypes.h>
#include <kernel/processmgr.h>

namespace paf {

namespace {

class ElapsedTime {
public:
	ElapsedTime()
	{
		m_elapsed = 0;
		m_from_start = true;
		m_start = sceKernelGetProcessTimeWide();
		m_last = m_start;
		m_now = m_start;
	}

	SceUInt64 Update()
	{
		SceUInt64 base;

		m_now = sceKernelGetProcessTimeWide();
		if (m_from_start)
			base = m_start;
		else
			base = m_last;
		m_last = m_now;
		m_elapsed = m_now - base;
		return m_elapsed;
	}

private:
	SceUInt64 m_elapsed;
	SceUInt64 m_now;
	SceUInt64 m_last;
	SceUInt64 m_start;
	bool m_from_start;
};

}

SceUInt64 get_time_internal()
{
	static ElapsedTime s_time;

	return s_time.Update();
}

}
