/*
 * paf::ui::Event and EventQueue (TOOL 0x81141090..0x811416a8): what a handler is sent, the bit of a
 * key code, and the queue of the events the environment passes on at the next frame. No record or
 * string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <widget/core/event.h>
#include <widget/core/widget.h>

namespace paf {

/* Sony's build calls the queue's push_back rather than inlining it. */
template<>
inline __attribute__((noinline)) int vector<ui::Event>::push_back(const ui::Event &val)
{
	int result;

	if (count + 1 <= limit || (result = reserve(limit + 10)) != 0) {
		new (&ptr[count]) ui::Event(val);
		result = ++count;
	}
	return result;
}

namespace ui {

Event::Event() : m_type(ROUTE_FORWARD | MODE_DISPATCH), m_detail(0), m_target(NULL)
{
}

Event::Event(uint32_t type, Handler *target, uint32_t route, uint32_t detail, int32_t d0, int32_t d1,
	     int32_t d2, int32_t d3)
	: m_detail(detail), m_target(target)
{
	m_type = (type & 0xFFFF) | route | MODE_DIRECT;
	m_data[0] = d0;
	m_data[1] = d1;
	m_data[2] = d2;
	m_data[3] = d3;
}

Event::Event(uint32_t type, uint32_t mode, uint32_t route, uint32_t detail, int32_t d0, int32_t d1,
	     int32_t d2, int32_t d3)
	: m_detail(detail), m_target(NULL)
{
	m_type = (type & 0xFFFF) | ((mode | route) & 0xFFFF0000);
	m_data[0] = d0;
	m_data[1] = d1;
	m_data[2] = d2;
	m_data[3] = d3;
}

bool Event::Comp(const Event &e, uint32_t mode, uint32_t mask) const
{
	if ((mode & 7) == 0)
		return false;
	if ((mode & 1) && (uint16_t)m_type != (uint16_t)e.m_type)
		return false;
	if ((mode & 2) && ((m_type ^ e.m_type) & 0xFFFF0000) != 0)
		return false;
	return !(mode & 4) || ((m_detail ^ e.m_detail) & mask) == 0;
}

uint32_t KeycodeToMask(int32_t keycode)
{
	switch (keycode) {
	case 1:
		return 1;
	case 2:
		return 2;
	case 3:
		return 4;
	case 4:
		return 8;
	case 27:
		return 16;
	case 28:
		return 32;
	case 29:
		return 64;
	case 30:
		return 128;
	case 31:
		return 1024;
	case 32:
		return 2048;
	case 33:
		return 0x10000;
	case 34:
		return 0x20000;
	case 35:
		return 0x40000;
	case 36:
		return 0x80000;
	case 37:
		return 512;
	case 38:
		return 256;
	case 39:
		return 4096;
	default:
		return 0;
	}
}

EventQueue::EventQueue(int32_t size) : m_queue(), m_mutex("ScePafWidgetEventQueue", 0)
{
	if (m_queue.capacity() < (uint32_t)size)
		m_queue.reserve(size);
}

int32_t EventQueue::Add(const Event &e, int32_t index)
{
	m_mutex.Lock();
	if (index >= 0 && index < (int32_t)m_queue.size()) {
		m_queue.insert(m_queue.begin() + index, e);
		m_mutex.Unlock();
		return 1;
	}
	m_mutex.Unlock();
	return 0;
}

int32_t EventQueue::Get(Event &e, int32_t index)
{
	m_mutex.Lock();
	if (index < (int32_t)m_queue.size() && index >= 0) {
		e = m_queue[index];
		m_mutex.Unlock();
		return 0;
	}
	m_mutex.Unlock();
	return -1;
}

int32_t EventQueue::PushBack(const Event &e)
{
	m_mutex.Lock();
	m_queue.push_back(e);
	m_mutex.Unlock();
	return 0;
}

int32_t EventQueue::PushFront(const Event &e)
{
	m_mutex.Lock();
	m_queue.insert(m_queue.begin(), e);
	m_mutex.Unlock();
	return 0;
}

int32_t EventQueue::PopFront(Event &e)
{
	m_mutex.Lock();
	m_mutex.Lock();
	if ((int32_t)m_queue.size() > 0) {
		e = m_queue[0];
		m_mutex.Unlock();
		m_queue.erase(m_queue.begin());
		m_mutex.Unlock();
		return 0;
	}
	m_mutex.Unlock();
	m_mutex.Unlock();
	return -1;
}

int32_t EventQueue::Remove(int32_t index)
{
	m_mutex.Lock();
	if (index < (int32_t)m_queue.size() && index >= 0) {
		m_queue.erase(m_queue.begin() + index);
		m_mutex.Unlock();
		return 0;
	}
	m_mutex.Unlock();
	return -1;
}

int32_t EventQueue::Cancel(const Event &e, uint32_t mode)
{
	int32_t result = -1;

	m_mutex.Lock();
	for (vector<Event>::iterator it = m_queue.begin(); it != m_queue.end();) {
		if ((mode & 7) != 0 && !((mode & 1) && (uint16_t)it->m_type != (uint16_t)e.m_type) &&
		    !((mode & 2) && ((it->m_type ^ e.m_type) & 0xFFFF0000) != 0) &&
		    !((mode & 4) && it->m_detail != e.m_detail)) {
			it = m_queue.erase(it);
			result = 0;
		} else {
			++it;
		}
	}
	m_mutex.Unlock();
	return result;
}

int32_t EventQueue::Clear()
{
	m_mutex.Lock();
	m_queue.clear();
	m_mutex.Unlock();
	return 0;
}

}
}
