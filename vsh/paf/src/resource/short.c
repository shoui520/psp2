/* Small standalone resource-object lifetime and readiness operations. */
#include <stdint.h>
#include "../compiler.h"
#include "../std/new.h"

int ScePafResource_CEED7338(const void *const *self)
{
	void *object = *(void *const *)self;
	if (object)
		return ((int (**)(void *))*(void **)object)[5](object);
	return 0;
}
