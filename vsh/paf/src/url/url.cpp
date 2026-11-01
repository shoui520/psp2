/*
 * paf::Url: the URL string is kept as given and parsed by sceHttpUriParse
 * (UrlUriParse, uri.c) into a pool sized by a first, measuring call.
 */
#include <url/url.h>
#include <std/string.h>

extern "C" {
#include "uri.h"
}

namespace paf {

/* Each suffix starts at its first delimiter and ends before either of the others. */
static const char s_suffix_delimiters[4][3] = {
	{ ';', '#', '?' },
	{ '#', ';', '?' },
	{ '?', ';', '#' },
	{ '/', ';', '\0' }
};

int32_t Url::ParseUrl()
{
	SceSize require, used;

	m_is_valid = false;
	if (m_urlstring.GetString().size() == 0)
		return 0;
	if (UrlUriParse(NULL, m_urlstring.GetString().c_str(), NULL, &require, 0) != 0)
		return 0;
	if (m_pool != NULL) {
		delete[] m_pool;
		m_pool = NULL;
	}
	m_pool = new char[4 * require];
	if (UrlUriParse(&m_url, m_urlstring.GetString().c_str(), m_pool, &used, require) != 0) {
		if (m_pool != NULL) {
			delete[] m_pool;
			m_pool = NULL;
		}
		return 0;
	}
	if (string(m_url.scheme).find("file") != string::npos)
		m_is_file = true;
	m_is_valid = true;
	return 1;
}

int32_t Url::Initialize(const string &url, bool init_pool)
{
	m_is_file = false;
	m_is_valid = false;
	m_urlstring = common::String("");
	sce_paf_memset(&m_url, 0, sizeof(m_url));
	if (init_pool)
		m_pool = NULL;
	if (url.size() != 0) {
		m_urlstring = common::String(url);
		ParseUrl();
	}
	return 1;
}

Url::Url()
{
	Initialize(string(""), true);
}

Url::Url(const string &url)
{
	Initialize(url, true);
}

Url::Url(const wstring &url)
{
	common::String value(url);

	Initialize(value.GetString(), true);
}

string Url::GetUrlString() const
{
	common::String url;

	url = m_urlstring;
	return url.GetString();
}

Url::Url(const Url &url)
{
	Initialize(url.GetUrlString(), true);
}

int32_t Url::Finalize()
{
	if (m_pool != NULL) {
		delete[] m_pool;
		m_pool = NULL;
	}
	return 0;
}

Url::~Url()
{
	if (m_pool != NULL) {
		delete[] m_pool;
		m_pool = NULL;
	}
}

Url &Url::operator=(const string &url)
{
	Initialize(url, false);
	return *this;
}

Url &Url::operator=(const wstring &url)
{
	common::String value(url);

	Initialize(value.GetString(), false);
	return *this;
}

Url &Url::operator=(const Url &url)
{
	Initialize(url.GetUrlString(), false);
	return *this;
}

wstring Url::GetUrlWString() const
{
	common::String url;

	url = m_urlstring;
	return url.GetWString();
}

string Url::GetComponent(int32_t component) const
{
	string result("");

	if (m_is_valid) {
		string host;

		switch (component) {
		case COMPONENT_BASE_URL:
			host = string(m_url.hostname).size() != 0 ? string(m_url.hostname) : string("/");
			result = string(m_url.scheme) + "://" + host.c_str();
			break;
		case COMPONENT_HOST:
			result = string(m_url.hostname);
			break;
		case COMPONENT_PASSWORD:
			result = string(m_url.password);
			break;
		case COMPONENT_PATH:
			result = string(m_url.path);
			break;
		case COMPONENT_RELATIVE_PATH:
			result = string(m_url.path);
			break;
		case COMPONENT_USER:
			result = string(m_url.username);
			break;
		case COMPONENT_SCHEME:
			result = string(m_url.scheme);
			break;
		default:
			result = "";
			break;
		}
	}
	return result;
}

string Url::GetBaseUrlString() const
{
	common::String base;
	string url(m_urlstring.GetString().c_str());

	base = common::String(GetComponent(COMPONENT_BASE_URL));
	return base.GetString();
}

wstring Url::GetBaseUrlWString() const
{
	common::String value(GetBaseUrlString());

	return value.GetWString();
}

string Url::GetHostString() const
{
	common::String host;
	string url(m_urlstring.GetString().c_str());

	host = common::String(GetComponent(COMPONENT_HOST));
	return host.GetString();
}

wstring Url::GetHostWString() const
{
	common::String value(GetHostString());

	return value.GetWString();
}

string Url::GetSuffix(const common::String &value, int8_t suffix) const
{
	const char *delimiters = s_suffix_delimiters[suffix];
	string result(value.GetString().c_str());
	string::size_type pos = result.find(delimiters[0]);

	if (pos == string::npos) {
		result = "";
		return result;
	}
	result = result.substr(pos, string::npos);
	pos = result.find(delimiters[1]);
	if (pos != string::npos)
		result = result.substr(0, pos);
	pos = result.find(delimiters[2]);
	if (pos != string::npos)
		result = result.substr(0, pos);
	return result;
}

string Url::GetParamaterString() const
{
	common::String value(GetSuffix(m_urlstring, SUFFIX_PARAMATER));

	return value.GetString();
}

wstring Url::GetParamaterWString() const
{
	common::String value(GetParamaterString());

	return value.GetWString();
}

string Url::GetAnchorString() const
{
	common::String value(GetSuffix(m_urlstring, SUFFIX_ANCHOR));

	return value.GetString();
}

wstring Url::GetAnchorWString() const
{
	common::String value(GetAnchorString());

	return value.GetWString();
}

string Url::GetPasswordString() const
{
	common::String password;

	password = common::String(GetComponent(COMPONENT_PASSWORD));
	return password.GetString();
}

wstring Url::GetPasswordWString() const
{
	common::String value(GetPasswordString());

	return value.GetWString();
}

string Url::GetPathString() const
{
	common::String path;

	path = common::String(GetComponent(COMPONENT_PATH));
	path = common::String(GetSuffix(path, SUFFIX_PATH));
	return path.GetString();
}

wstring Url::GetPathWString() const
{
	common::String value(GetPathString());

	return value.GetWString();
}

int32_t Url::GetPort() const
{
	return m_url.port;
}

string Url::GetQueryString() const
{
	common::String value(GetSuffix(m_urlstring, SUFFIX_QUERY));

	return value.GetString();
}

wstring Url::GetQueryWString() const
{
	common::String value(GetQueryString());

	return value.GetWString();
}

string Url::GetRelativePathString() const
{
	common::String value(GetPathString());

	return value.GetString();
}

wstring Url::GetRelativePathWString() const
{
	common::String value(GetPathString());

	return value.GetWString();
}

string Url::GetUserString() const
{
	common::String user;

	user = common::String(GetComponent(COMPONENT_USER));
	return user.GetString();
}

wstring Url::GetUserWString() const
{
	common::String value(GetUserString());

	return value.GetWString();
}

}

paf::string ScePafMisc_14C9C3D8(const paf::Url *url)
{
	paf::common::String copy;

	copy = url->m_urlstring;
	return copy.GetString();
}

paf::wstring ScePafMisc_2E04CDAE(const paf::Url *url)
{
	paf::common::String copy;

	copy = url->m_urlstring;
	return copy.GetWString();
}

namespace paf {

string Url::GetSchemeString() const
{
	common::String scheme;

	scheme = common::String(GetComponent(COMPONENT_SCHEME));
	return scheme.GetString();
}

wstring Url::GetSchemeWString() const
{
	common::String value(GetSchemeString());

	return value.GetWString();
}

bool Url::IsParsed()
{
	return m_is_valid;
}

}
