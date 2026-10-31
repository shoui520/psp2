/*
 * paf::common::SharedPtr: a reference-counted pointer. The counter block holds
 * the strong and weak counts and the deleter; the last strong reference calls
 * the deleter, the last weak one frees the block.
 */
#ifndef PAF_COMMON_SHARED_PTR_H
#define PAF_COMMON_SHARED_PTR_H

#include <stddef.h>
#include <stdint.h>
#include <sce_atomic.h>
#include <std/new>

namespace paf {
namespace common {

template<class T>
class SharedPtr {
public:
	struct RefCounter {
		T *m_ptr;
		int32_t m_counter;
		int32_t m_weak_counter;
		void (*m_deleter)(T *ptr);
	};

	SharedPtr() : m_ptr(NULL), m_ref_counter(NULL)
	{
	}

	SharedPtr(const SharedPtr &src) : m_ptr(src.m_ptr), m_ref_counter(src.m_ref_counter)
	{
		if (m_ptr != NULL)
			sceAtomicIncrement32AcqRel(&m_ref_counter->m_counter);
	}

	~SharedPtr()
	{
		Release();
	}

	SharedPtr &operator=(const SharedPtr &src)
	{
		if (m_ptr != src.m_ptr) {
			T *ptr = src.m_ptr;
			RefCounter *counter = src.m_ref_counter;

			Release();
			m_ptr = ptr;
			m_ref_counter = counter;
			if (m_ptr != NULL)
				sceAtomicIncrement32AcqRel(&m_ref_counter->m_counter);
		}
		return *this;
	}

	void reset()
	{
		Release();
		m_ptr = NULL;
		m_ref_counter = NULL;
	}

	T *get() const
	{
		return m_ptr;
	}

	T *operator->() const
	{
		return m_ptr;
	}

	T &operator*() const
	{
		return *m_ptr;
	}

	T *m_ptr;
	RefCounter *m_ref_counter;

private:
	void Release()
	{
		if (m_ptr != NULL &&
		    sceAtomicDecrement32AcqRel(&m_ref_counter->m_counter) == 1) {
			m_ref_counter->m_deleter(m_ptr);
			if (sceAtomicDecrement32AcqRel(&m_ref_counter->m_weak_counter) == 1)
				delete m_ref_counter;
		}
	}
};

}
}

#endif
