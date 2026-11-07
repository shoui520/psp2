/*
 * paf::ui::Event: what a handler is sent, its type with how it is routed; paf::ui::EventQueue:
 * the events the environment passes on at the next frame. Names after ScePaf-RE (restored from
 * Sony's DWARF), as far as converted C++ uses them.
 */
#ifndef PAF_WIDGET_CORE_EVENT_H
#define PAF_WIDGET_CORE_EVENT_H

#include <stddef.h>
#include <stdint.h>
#include <thread/thread.h>
#include <std/vector>

namespace paf {
namespace ui {

class Handler;

/* 0x24 bytes */
class Event {
public:
	enum {
		ROUTE_FORWARD = 0x10000,
		ROUTE_BACK = 0x20000,
		ROUTE_TERM = 0x40000,
		MODE_DISPATCH = 0x100000,
		MODE_BROADCAST = 0x200000,
		MODE_DIRECT = 0x400000,
		MODE_DO = 0x800000,
		STATE_CONSUME = 0x1000000,
		STATE_IGNORE = 0x2000000,
		DETAIL_GRABBED = 0x10000000,
		DETAIL_SUPPLY = 0x20000000,
		DETAIL_HOOKED = 0x80000000
	};

	Event(uint32_t type, Handler *target, uint32_t route, uint32_t detail, int32_t d0 = 0,
	      int32_t d1 = 0, int32_t d2 = 0, int32_t d3 = 0);
	Event(uint32_t type, uint32_t mode, uint32_t route, uint32_t detail, int32_t d0 = 0,
	      int32_t d1 = 0, int32_t d2 = 0, int32_t d3 = 0);

	virtual ~Event()
	{
	}

	void Direct()
	{
		m_type = (m_type & 0xff0fffff) | MODE_DIRECT;
	}

	void Term()
	{
		m_type = (m_type & 0xfff0ffff) | ROUTE_TERM;
	}

	void SetTarget(Handler *w)
	{
		if (w != NULL) {
			Direct();
			m_target = w;
		}
	}

	/* marks it consumed (name after ScePaf-RE) */
	void Consume()
	{
		m_type = (m_type & 0xf0ffffff) | STATE_CONSUME;
	}

	void SetDetail(uint32_t detail)
	{
		m_detail = detail;
	}

	uint32_t GetType() const
	{
		return m_type & 0xffff;
	}

	uint32_t GetDetail() const
	{
		return m_detail;
	}

	/* the type in the low half, the route, mode, state and detail bits in the high half */
	uint32_t m_type;
	uint32_t m_detail;
	uint32_t m_dotype;
	int32_t m_data[4];
	Handler *m_target;
};

/* 0x38 bytes */
class EventQueue {
public:
	virtual ~EventQueue()
	{
	}

	int32_t PushBack(const Event &e);

	vector<Event> m_queue;
	thread::RMutex m_mutex;
};

}
}

#endif
