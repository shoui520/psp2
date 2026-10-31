/*
 * paf::Url: a URL split by sceHttpUriParse. The C++ view of src/url/url.c for
 * the C++ sources until Url is C++ itself.
 */
#ifndef PAF_URL_URL_H
#define PAF_URL_URL_H

#include <stddef.h>
#include <stdint.h>
#include <libhttp.h>
#include <std/string>
#include <common/string_util.h>

namespace paf {

class Url {
public:
	Url();
	Url(const string &url);
	Url(const wstring &url);
	Url(const Url &url);
	virtual ~Url();

	string GetSchemeString() const;
	string GetHostString() const;
	int32_t GetPort() const;
	string GetUserString() const;
	string GetPasswordString() const;
	string GetPathString() const;
	string GetQueryString() const;
	string GetAnchorString() const;
	string GetParamaterString() const;

	SceHttpUriElement m_url;
	void *m_pool;
	bool m_is_valid;
	bool m_is_file;
	common::String m_urlstring;
};

}

/* Whether the URL parsed (the export has no name). */
extern "C" bool ScePafMisc_0E846E5C(const paf::Url *url);

#endif
