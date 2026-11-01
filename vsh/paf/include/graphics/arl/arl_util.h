/*
 * paf::graph::arl's file loading, which the texture loader and the shader programs share: a
 * file read whole into a buffer. The functions are named after the messages they report; the
 * header's and the option's names ours.
 */
#ifndef PAF_GRAPHICS_ARL_ARL_UTIL_H
#define PAF_GRAPHICS_ARL_ARL_UTIL_H

#include <stdint.h>
#include <kernel/iofilemgr.h>
#include <std/stdio.h>
#include <std/string>
#include <std/vector>

namespace paf {
namespace graph {
namespace arl {

/* reports a failure of the arl layer: where and what, which release builds drop */
inline void ReportError(const string &where, const string &what)
{
}

/* how a file is loaded: both set by every caller, neither read */
class LoadOption {
public:
	bool m_reserved0;
	bool m_reserved1;
};

/* Sony's build calls both loaders rather than inlining them. */
inline __attribute__((noinline)) bool loadFromDisk(vector<uint8_t> &buffer, const string &filename,
						    const LoadOption &option)
{
	sce_paf_FILE *fp = sce_paf_fopen(filename.c_str(), "rb");

	if (fp == NULL) {
		ReportError("load from disk", "fopen() failed");
		return false;
	}

	sce_paf_fseek(fp, 0, SCE_SEEK_END);

	long size = sce_paf_ftell(fp);

	sce_paf_fseek(fp, 0, SCE_SEEK_SET);
	if (size == 0) {
		ReportError("load from disk", "file has been opened but it's empty");
		sce_paf_fclose(fp);
		return false;
	}
	buffer.resize(size, 0);
	sce_paf_fread(&buffer[0], buffer.size(), 1, fp);
	sce_paf_fclose(fp);
	return true;
}

inline __attribute__((noinline)) bool load(vector<uint8_t> &buffer, const string &filename,
					    bool reserved0, bool reserved1)
{
	if (filename == "") {
		ReportError("load", "filename string is the empty string");
		return false;
	}

	LoadOption option;

	option.m_reserved0 = reserved0;
	option.m_reserved1 = reserved1;
	if (loadFromDisk(buffer, filename, option))
		return true;
	return false;
}

}
}
}

#endif
