/*
 * The root of updater_core's classes: a virtual destructor and nothing else (its out-of-line
 * copy is at 0x81015970 in cui_setupper). The name is ours.
 */
#ifndef UPDATER_CORE_UPDATER_OBJECT_H
#define UPDATER_CORE_UPDATER_OBJECT_H

#include <stddef.h>

class UpdaterObject {
public:
	virtual ~UpdaterObject() {}

	static void *operator new(size_t size);
	static void *operator new[](size_t size);
	static void operator delete(void *ptr);
	static void operator delete[](void *ptr);
};

#endif
