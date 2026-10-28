#include "path.h"

extern size_t sce_paf_strlen(const char *str);
extern char *sce_paf_strchr(const char *str, int ch);
extern int sce_paf_strncmp(const char *left, const char *right, size_t len);
extern void *ScePafStdc_D8CC1AFE(size_t size);
extern void ScePafStdc_5C468294(void *ptr);
#define PafPlacementNew _ZnwjPv
extern void *PafPlacementNew(size_t size, void *where);

static const char pathEmpty[] = "";

void PathSplit(const PafString *path, PafString *base, PafString *file)
{
	char slash = '/', colon = ':';
	size_t split = PafStringRfind(path, &slash, (size_t)-1, 1);
	PafString part;
	size_t count;

	if (split == (size_t)-1) {
		split = PafStringRfind(path, &colon, (size_t)-1, 1);
		if (split == (size_t)-1) {
			PafStringAssign(base, pathEmpty, sce_paf_strlen(pathEmpty));
			PafStringCopy(file, path);
			return;
		}
		++split;
		PafStringSubstring(&part, path, 0, split);
		PafStringCopy(base, &part);
		PafStringDestroy(&part);
		PafStringSubstring(&part, path, split, (size_t)-1);
		PafStringCopy(file, &part);
		PafStringDestroy(&part);
		return;
	}
	PafStringSubstring(&part, path, 0, split);
	PafStringCopy(base, &part);
	PafStringDestroy(&part);
	PafStringSubstring(&part, path, split + 1, (size_t)-1);
	PafStringCopy(file, &part);
	PafStringDestroy(&part);
	count = base->count;
	while (count && base->ptr[count - 1] == '/')
		--count;
	PafStringSubstring(&part, base, 0, count);
	PafStringCopy(base, &part);
	PafStringDestroy(&part);
	if (!count)
		PafStringAssignChar(base, '/');
}

void PathSplitExt(const PafString *path, PafString *filepath, PafString *ext)
{
	char dot = '.', slash = '/';
	size_t extension = PafStringRfind(path, &dot, (size_t)-1, 1);
	size_t separator = PafStringRfind(path, &slash, (size_t)-1, 1);
	PafString part;

	if (extension != (size_t)-1 &&
	    (separator == (size_t)-1 || separator <= extension)) {
		PafStringSubstring(&part, path, 0, extension);
		PafStringCopy(filepath, &part);
		PafStringDestroy(&part);
		PafStringSubstring(&part, path, extension, (size_t)-1);
		PafStringCopy(ext, &part);
		PafStringDestroy(&part);
	} else {
		PafStringCopy(filepath, path);
		PafStringAssign(ext, pathEmpty, sce_paf_strlen(pathEmpty));
	}
}

void PathJoin(PafString *result, const PafString *left, const PafString *right)
{
	size_t count = left->count;
	PafString prefix, joined;

	if (!count) {
		PafStringInitialize(result);
		PafStringCopy(result, right);
	} else if (!right->count) {
		PafStringInitialize(result);
		PafStringCopy(result, left);
	} else if (right->ptr[0] == '/') {
		PafStringInitialize(result);
		PafStringCopy(result, right);
	} else if (left->ptr[count - 1] == '/' || right->ptr[0] == '.') {
		size_t index = count - 1, end;
		const char *current = left->ptr + index;

		/* The original scans before the last byte even for a dot suffix. */
		do {
			end = index;
			if (!index)
				break;
			--index;
		} while (*--current == '/');
		PafStringSubstring(&prefix, left, 0, end + 1);
		PafStringInitialize(&joined);
		PafStringCopy(&joined, &prefix);
		PafStringAppendBytes(&joined, right->ptr, right->count);
		PafStringInitialize(result);
		PafStringCopy(result, &joined);
		PafStringDestroy(&joined);
		PafStringDestroy(&prefix);
	} else {
		PafString temporary;
		PafString *appended;

		PafStringInitialize(&temporary);
		PafStringCopy(&temporary, left);
		appended = PafStringAppend(&temporary, 1, '/');
		PafStringInitialize(&prefix);
		PafStringCopy(&prefix, appended);
		PafStringDestroy(&temporary);
		PafStringInitialize(&joined);
		PafStringCopy(&joined, &prefix);
		PafStringAppendBytes(&joined, right->ptr, right->count);
		PafStringInitialize(result);
		PafStringCopy(result, &joined);
		PafStringDestroy(&joined);
		PafStringDestroy(&prefix);
	}
}

static inline __attribute__((always_inline))
void PathSegmentVectorAppend(PathSegmentVector *self, size_t start, size_t len)
{
	if ((int32_t)(self->_size + 1) <= (int32_t)self->_capacity ||
	    PathSegmentVectorReallocate(self, self->_capacity + 10)) {
		PathSegment *dest = PafPlacementNew(sizeof(*dest),
						  self->_data + self->_size);

		if (dest) {
			dest->start = start;
			dest->len = len;
		}
		++self->_size;
	}
}

void PathNormalize(PafString *result, const PafString *path)
{
	PathSegmentVector parts, normalized;
	size_t start = 0, count = path->count;
	PathSegment *part, *end;
	char *text, *cursor;

	parts._data = NULL;
	parts._size = parts._capacity = 0;
	for (;;) {
		char slash = '/';
		size_t remaining = count - start;
		size_t separator = (size_t)-1;

		if (start < count && remaining) {
			const char *current = path->ptr + start;
			char *found;

			do {
				found = sce_paf_strchr(current, slash);
				if (!found)
					break;
				if (!sce_paf_strncmp(found, &slash, 1)) {
					separator = found - path->ptr;
					break;
				}
				remaining -= found - current + 1;
				current = found + 1;
			} while (remaining);
		}
		if (separator == (size_t)-1) {
			PathSegmentVectorAppend(&parts, start, path->count - start);
			break;
		}
		PathSegmentVectorAppend(&parts, start, separator - start);
		start = separator + 1;
		count = path->count;
		if (start >= count)
			break;
	}
	normalized._data = NULL;
	normalized._size = normalized._capacity = 0;
	part = parts._data;
	end = parts._data + parts._size;
	while (part != end) {
		if (part->len &&
		    (part->len != 1 || path->ptr[part->start] != '.')) {
			PathSegment *previous = normalized._size ?
				normalized._data + normalized._size - 1 : NULL;

			if (part->len == 2 && path->ptr[part->start] == '.' &&
			    path->ptr[part->start + 1] == '.' && normalized._size &&
			    (previous->len != 2 ||
			     (path->ptr[previous->start] != '.' &&
			      path->ptr[previous->start + 1] != '.'))) {
				--normalized._size;
				if ((int32_t)normalized._size <
				    (int32_t)(normalized._capacity - 20))
					PathSegmentVectorReallocate(&normalized,
							    normalized._capacity - 10);
			} else {
				if ((int32_t)(normalized._size + 1) <=
				    (int32_t)normalized._capacity ||
				    PathSegmentVectorReallocate(&normalized,
							 normalized._capacity + 10)) {
					PathSegment *dest = PafPlacementNew(sizeof(*dest),
						normalized._data + normalized._size);

					if (dest) {
						dest->start = part->start;
						dest->len = part->len;
					}
					++normalized._size;
				}
			}
		}
		if (part)
			++part;
		end = parts._data + parts._size;
	}
	text = ScePafStdc_D8CC1AFE(path->count + 1);
	cursor = text;
	part = normalized._data;
	while (part != normalized._data + normalized._size) {
		sce_paf_memcpy(cursor, path->ptr + part->start, part->len);
		cursor[part->len] = '/';
		cursor += part->len + 1;
		if (part)
			++part;
	}
	if (cursor != text)
		--cursor;
	*cursor = 0;
	PafStringInitialize(result);
	PafStringAssign(result, text, text ? sce_paf_strlen(text) : 0);
	ScePafStdc_5C468294(text);
	if (normalized._data)
		ScePafStdc_1B77082E(normalized._data);
	if (parts._data) {
		normalized._data = (PathSegment *)-1;
		normalized._size = normalized._capacity = (size_t)-1;
		ScePafStdc_1B77082E(parts._data);
	}
}

int PathSegmentVectorReallocate(PathSegmentVector *self, size_t capacity)
{
	PathSegment *data;
	size_t index;

	if (!self->_data) {
		data = ScePafStdc_FC5CD359(sizeof(*data) * capacity);
		if (!data)
			ScePafStdc_D608246A();
		self->_data = data;
		if (!data)
			return 0;
		self->_capacity = capacity;
		return 1;
	}
	data = ScePafStdc_FC5CD359(sizeof(*data) * capacity);
	if (!data) {
		ScePafStdc_D608246A();
		return 0;
	}
	for (index = 0; index < self->_size; ++index) {
		PathSegment *source = self->_data + index;
		PathSegment *dest = PafPlacementNew(sizeof(*dest), data + index);

		if (dest) {
			dest->start = source->start;
			dest->len = source->len;
		}
	}
	ScePafStdc_1B77082E(self->_data);
	self->_data = data;
	self->_capacity = capacity;
	return 1;
}
