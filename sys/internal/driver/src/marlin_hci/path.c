/* Descriptive source name: the original library has no debug records. */
#include "buffer.h"
#include "path.h"
#include "search.h"

int MarlinAppendPath(unsigned int *size, char *path, const char *component)
{
	int result;
	unsigned int pathLength;
	unsigned int componentLength;

	if (size == 0 || path == 0 || component == 0) {
		result = 3;
		goto out;
	}
	pathLength = MarlinStringLength(path);
	componentLength = MarlinStringLength(component);
	if (pathLength != 0 &&
	    (signed char)path[pathLength - 1] != MarlinGetPathSeparator())
		pathLength++;
	if (*size < pathLength + componentLength + 1) {
		*size = pathLength + componentLength + 1;
		result = 1;
		goto out;
	}
	if (pathLength != 0)
		path[pathLength - 1] = MarlinGetPathSeparator();
	MarlinCopyMemory(&path[pathLength], component, componentLength + 1);
	*size = pathLength + componentLength + 1;
	result = 0;
out:
	return result;
}

int MarlinStringListDestroy(void *allocator, MarlinStringList *list)
{
	int result = 0;
	MarlinStringList *node = list;
	MarlinStringList *next = 0;

	while (node != 0) {
		next = node->next;
		MarlinFreeMemory(allocator, node->string);
		MarlinFreeMemory(allocator, node);
		node = next;
	}
	return result;
}
