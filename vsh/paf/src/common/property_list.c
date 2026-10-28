#include "property_list.h"
#include "../std/stdc++/atexit.h"

PafString StringElementEmptyString;
PafWString WStringElementEmptyString;

void PropertyListInitialize(void)
{
	extern void *__dso_handle;
	unsigned char allocator[4];

	PafStringConstruct(&StringElementEmptyString, &allocator[0]);
	ScePafStdc_C95732BB(&StringElementEmptyString,
			  (void (*)(void *))PafStringDestruct, &__dso_handle);
	PafWStringConstruct(&WStringElementEmptyString, &allocator[1]);
	ScePafStdc_C95732BB(&WStringElementEmptyString,
			  (void (*)(void *))PafWStringDestruct, &__dso_handle);
}
