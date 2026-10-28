#include "httpfile.h"
#include "../paf_types.h"
#include "../std/new.h"

HttpFileTimeout httpFileDefaultTimeout = { -1, -1, -1, -1 };

HttpFileOpenArg *HttpFileOpenArgConstruct(HttpFileOpenArg *self)
{
	PafStringInitialize(&self->scheme);
	self->file_type = Type_Http;
	PafStringAssign(&self->scheme, "HTTP", sce_paf_strlen("HTTP"));
	PafStringInitialize(&self->host);
	self->port = 80;
	PafStringInitialize(&self->method);
	PafStringAssign(&self->method, "GET", sce_paf_strlen("GET"));
	PafStringInitialize(&self->uri);
	PafStringInitialize(&self->version);
	PafStringAssign(&self->version, "HTTP/1.1", sce_paf_strlen("HTTP/1.1"));
	self->response_header_max_size = 0;
	self->template_option = 0;
	self->open_flag = 0;
	self->accept_encoding_gzip = 2;
	self->msg_body_len = 0;
	self->https_flag = HTTPS_FLAG_DEFAULT & ~HTTPS_FLAG_CLIENT_VERIFY;
	self->header._size = 0;
	self->header._capacity = 0;
	self->passwd.count = 0;
	self->user.count = 0;
	self->referer.count = 0;
	self->response_code = NULL;
	self->user_agent.count = 0;
	self->msg_body = NULL;
	self->timeout.resolve_timeout = httpFileDefaultTimeout.resolve_timeout;
	self->header._data = NULL;
	self->passwd.ptr = &PafStringEmpty;
	self->timeout.connect_timeout = httpFileDefaultTimeout.connect_timeout;
	self->user.ptr = &PafStringEmpty;
	self->referer.ptr = &PafStringEmpty;
	self->user_agent.ptr = &PafStringEmpty;
	self->timeout.send_timeout = httpFileDefaultTimeout.send_timeout;
	self->timeout.recv_timeout = httpFileDefaultTimeout.recv_timeout;
	return self;
}

int32_t HttpFileSetDefaultTimeout(int32_t value, HttpFileOptionType type)
{
	if (value < 0)
		return SCE_PAF_ERROR_INVALID_ARGUMENT;
	if (type & OptionType_ResolveTimeOut)
		httpFileDefaultTimeout.resolve_timeout = value / 31;
	if (type & OptionType_ConnectTimeOut)
		httpFileDefaultTimeout.connect_timeout = value;
	if (type & OptionType_SendTimeOut)
		httpFileDefaultTimeout.send_timeout = value;
	if (type & OptionType_RecvTimeOut)
		httpFileDefaultTimeout.recv_timeout = value;
	return SCE_PAF_OK;
}

HttpFileOpenArg *HttpFileOpenArgConstructBase(HttpFileOpenArg *self)
{
	return HttpFileOpenArgConstruct(self);
}

int32_t HttpFileOpenArgAddRequestHeader(HttpFileOpenArg *self, const char *name,
				       const char *value)
{
	PafString empty;
	int allocated = 1;

	if (!name)
		return SCE_PAF_ERROR_INVALID_ARGUMENT;
	PafStringInitialize(&empty);
	PafStringAssign(&empty, "", sce_paf_strlen(""));
	if ((int32_t)(self->header._size + 1) > (int32_t)self->header._capacity)
		allocated = PafStringVectorReallocate(&self->header,
						    self->header._capacity + 10);
	if (allocated) {
		PafString *entry = PafPlacementNew(sizeof(*entry),
						 &self->header._data[self->header._size]);

		if (entry) {
			PafStringInitialize(entry);
			PafStringCopy(entry, &empty);
		}
		++self->header._size;
	}
	PafStringDestroy(&empty);
	PafStringAppendBytes(&self->header._data[self->header._size - 1],
			     name, sce_paf_strlen(name));
	PafStringAppendBytes(&self->header._data[self->header._size - 1],
			     ": ", sce_paf_strlen(": "));
	PafStringAppendBytes(&self->header._data[self->header._size - 1],
			     value, sce_paf_strlen(value));
	return SCE_PAF_OK;
}

int32_t HttpFileOpenArgParseUrl(HttpFileOpenArg *self, Url *url)
{
	PafString value;

	if (!url || !UrlIsValid(url))
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	UrlGetSchemeString(&value, url);
	PafStringCopy(&self->scheme, &value);
	PafStringDestroy(&value);
	UrlGetHostString(&value, url);
	PafStringCopy(&self->host, &value);
	PafStringDestroy(&value);
	self->port = (uint16_t)UrlGetPort(url);
	UrlGetUserString(&value, url);
	PafStringCopy(&self->user, &value);
	PafStringDestroy(&value);
	UrlGetPasswordString(&value, url);
	PafStringCopy(&self->passwd, &value);
	PafStringDestroy(&value);
	UrlGetPathString(&value, url);
	PafStringCopy(&self->uri, &value);
	PafStringDestroy(&value);
	UrlGetQueryString(&value, url);
	PafStringAppendBytes(&self->uri, value.ptr, value.count);
	PafStringDestroy(&value);
	UrlGetAnchorString(&value, url);
	PafStringAppendBytes(&self->uri, value.ptr, value.count);
	PafStringDestroy(&value);
	UrlGetParamaterString(&value, url);
	PafStringAppendBytes(&self->uri, value.ptr, value.count);
	PafStringDestroy(&value);
	return 0;
}

int32_t HttpFileOpenArgSetOption(HttpFileOpenArg *self, int32_t value,
				HttpFileOptionType type)
{
	if (value < 0)
		return SCE_PAF_ERROR_INVALID_ARGUMENT;
	if (type & OptionType_ResolveTimeOut)
		self->timeout.resolve_timeout = value / 31;
	if (type & OptionType_ConnectTimeOut)
		self->timeout.connect_timeout = value;
	if (type & OptionType_SendTimeOut)
		self->timeout.send_timeout = value;
	if (type & OptionType_RecvTimeOut)
		self->timeout.recv_timeout = value;
	return SCE_PAF_OK;
}
