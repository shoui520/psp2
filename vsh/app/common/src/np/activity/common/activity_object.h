/*
 * The base of every activity object: allocated from the library's heap (activity_memory.cpp).
 * No binary names the class; the name is ours.
 */
#ifndef NP_ACTIVITY_COMMON_ACTIVITY_OBJECT_H
#define NP_ACTIVITY_COMMON_ACTIVITY_OBJECT_H

#include <stddef.h>

class ActivityObject {
public:
	ActivityObject();
	virtual ~ActivityObject();

	static void *operator new(size_t size);
	static void operator delete(void *ptr);
};

#endif
