/*
 * The base of every activity object: its constructor and destructor, and the class's new and
 * delete, which take the library's heap. No binary names this file; the name is ours.
 */
#include "activity_memory.h"
#include "activity_object.h"

ActivityObject::ActivityObject()
{
}

ActivityObject::~ActivityObject()
{
}

void ActivityObject::operator delete(void *ptr)
{
	activityFree(ptr);
}

void *ActivityObject::operator new(size_t size)
{
	return activityMalloc(size);
}
