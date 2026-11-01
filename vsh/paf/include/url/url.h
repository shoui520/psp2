/*
 * paf::Url: a URL split by sceHttpUriParse into the pool it owns. The getters
 * build each part from the parsed elements or from the URL string itself.
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
	Url(const Url &url);
	Url(const string &url);
	Url(const wstring &url);
	virtual ~Url();

	Url &operator=(const string &url);
	Url &operator=(const wstring &url);
	Url &operator=(const Url &url);

	int32_t Finalize();
	bool IsParsed();

	string GetUrlString() const;
	wstring GetUrlWString() const;
	string GetBaseUrlString() const;
	wstring GetBaseUrlWString() const;
	string GetHostString() const;
	wstring GetHostWString() const;
	string GetParamaterString() const;
	wstring GetParamaterWString() const;
	string GetAnchorString() const;
	wstring GetAnchorWString() const;
	string GetPasswordString() const;
	wstring GetPasswordWString() const;
	string GetPathString() const;
	wstring GetPathWString() const;
	int32_t GetPort() const;
	string GetQueryString() const;
	wstring GetQueryWString() const;
	string GetRelativePathString() const;
	wstring GetRelativePathWString() const;
	string GetUserString() const;
	wstring GetUserWString() const;
	string GetSchemeString() const;
	wstring GetSchemeWString() const;

	SceHttpUriElement m_url;
	char *m_pool;
	bool m_is_valid;
	bool m_is_file;
	common::String m_urlstring;

private:
	/* the parts GetComponent builds from the parsed elements */
	enum Component {
		COMPONENT_BASE_URL = 1,
		COMPONENT_HOST = 2,
		COMPONENT_PASSWORD = 5,
		COMPONENT_PATH = 6,
		COMPONENT_RELATIVE_PATH = 9,
		COMPONENT_USER = 10,
		COMPONENT_SCHEME = 12
	};

	/* the parts GetSuffix cuts out of a string, by the delimiters they start with */
	enum Suffix {
		SUFFIX_PARAMATER = 0,
		SUFFIX_ANCHOR = 1,
		SUFFIX_QUERY = 2,
		SUFFIX_PATH = 3
	};

	int32_t Initialize(const string &url, bool init_pool);
	int32_t ParseUrl();
	string GetComponent(int32_t component) const;
	string GetSuffix(const common::String &value, int8_t suffix) const;
};

}

/* Copies of GetUrlString and GetUrlWString; no name found for their NIDs. */
extern "C" {
paf::string ScePafMisc_14C9C3D8(const paf::Url *url);
paf::wstring ScePafMisc_2E04CDAE(const paf::Url *url);
}

#endif
