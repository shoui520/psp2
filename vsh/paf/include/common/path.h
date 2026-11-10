/*
 * paf::common::path: paths cut into their folder and file, file and extension, joined, and
 * normalized ("." and empty parts dropped, ".." taking the part before it away).
 */
#ifndef PAF_COMMON_PATH_H
#define PAF_COMMON_PATH_H

#include <std/string>
#include <std/vector>

namespace paf {
namespace common {
namespace path {

/* the folder (without its trailing slashes, "/" for the root) and the file */
void Split(const string &path, string &base, string &file);
/* the path without its extension, and the extension with its dot */
void SplitExt(const string &path, string &filepath, string &ext);
/* right appended to left with one slash between them; an absolute right replaces left */
string Join(const string &left, const string &right);

/* a part of a path: where it starts and its length (name ours) */
struct PathSegment {
	size_t start;
	size_t len;
};

}
}
}

extern "C" {
/* the path normalized, its parts joined by single slashes (name ours: the export has no Sony
   name) */
paf::string ScePafCommon_19056E57(const paf::string &path);
}

#endif
