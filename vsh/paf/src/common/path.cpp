/*
 * paf::common::path: cutting, joining and normalizing paths held in paf strings.
 */
#include <common/path.h>
#include <std/new>

namespace paf {
namespace common {
namespace path {

static const char pathEmpty[] = "";

void Split(const string &path, string &base, string &file)
{
	size_t split = path.rfind('/');

	if (split == string::npos) {
		split = path.rfind(':');
		if (split == string::npos) {
			base = pathEmpty;
			file = path;
			return;
		}
		++split;
		base = path.substr(0, split);
		file = path.substr(split, string::npos);
		return;
	}
	base = path.substr(0, split);
	file = path.substr(split + 1, string::npos);

	size_t count = base.size();

	while (count != 0 && base.c_str()[count - 1] == '/')
		--count;
	base = base.substr(0, count);
	if (count == 0)
		base.assign('/');
}

void SplitExt(const string &path, string &filepath, string &ext)
{
	size_t extension = path.rfind('.');
	size_t separator = path.rfind('/');

	if (extension != string::npos &&
	    (separator == string::npos || separator <= extension)) {
		filepath = path.substr(0, extension);
		ext = path.substr(extension, string::npos);
	} else {
		filepath = path;
		ext = pathEmpty;
	}
}

string Join(const string &left, const string &right)
{
	size_t count = left.size();

	if (count == 0)
		return right;
	if (right.size() == 0)
		return left;
	if (right.c_str()[0] == '/')
		return right;
	if (left.c_str()[count - 1] == '/' || right.c_str()[0] == '.') {
		/* the slashes at the end of left dropped (Sony scans back from the byte before the
		   last one also when right starts with a dot) */
		size_t index = count - 1, end;
		const char *current = left.c_str() + index;

		do {
			end = index;
			if (index == 0)
				break;
			--index;
		} while (*--current == '/');
		return left.substr(0, end + 1) + right;
	}

	string prefix = string(left).append(1, '/');

	return prefix + right;
}

}
}
}

using namespace paf;
using paf::common::path::PathSegment;

string ScePafCommon_19056E57(const string &path)
{
	vector<PathSegment> parts;
	size_t start = 0;

	for (;;) {
		size_t separator = path.find('/', start);
		PathSegment part;

		part.start = start;
		if (separator == string::npos) {
			part.len = path.size() - start;
			parts.push_back(part);
			break;
		}
		part.len = separator - start;
		parts.push_back(part);
		start = separator + 1;
		if (start >= path.size())
			break;
	}

	vector<PathSegment> normalized;
	const char *text = path.c_str();

	for (vector<PathSegment>::iterator it = parts.begin(); it != parts.end(); ++it) {
		const PathSegment &part = *it.m_ptr;

		if (part.len == 0 || (part.len == 1 && text[part.start] == '.'))
			continue;
		if (part.len == 2 && text[part.start] == '.' && text[part.start + 1] == '.' &&
		    normalized.size() != 0 &&
		    (normalized.back().len != 2 ||
		     (text[normalized.back().start] != '.' &&
		      text[normalized.back().start + 1] != '.')))
			normalized.pop_back();
		else
			normalized.push_back(part);
	}

	char *buf = new char[path.size() + 1];
	char *cursor = buf;

	for (vector<PathSegment>::iterator it = normalized.begin(); it != normalized.end(); ++it) {
		const PathSegment &part = *it.m_ptr;

		sce_paf_memcpy(cursor, path.c_str() + part.start, part.len);
		cursor[part.len] = '/';
		cursor += part.len + 1;
	}
	if (cursor != buf)
		--cursor;
	*cursor = 0;

	string result(buf);

	delete[] buf;
	return result;
}
