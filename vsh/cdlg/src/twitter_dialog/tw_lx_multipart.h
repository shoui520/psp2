/*
 * SceTwitterDialogPlugin: multipart/form-data builder, streaming uploader, and HTTP downloader.
 */
#ifndef TWITTER_DIALOG_TW_LX_MULTIPART_H
#define TWITTER_DIALOG_TW_LX_MULTIPART_H

#include <stdint.h>
#include <scetypes.h>
#include <std/list>
#include <common/shared_ptr.h>
#include "../tw_login_dialog/tw_lx.h"
#include "../tw_login_dialog/tw_lx_http.h"

/* Single part in a multipart/form-data payload (either inline text or file path) */
struct LxMultipartPart {
	enum Kind {
		KIND_MEMORY = 0,
		KIND_FILE = 1
	};

	SceInt32 kind;
	LxString dataOrPath;
	paf::list<LxString> headers;
};

/* Builds and streams RFC 2388 multipart/form-data requests over LxHttpRequest */
class LxMultipartForm {
public:
	LxMultipartForm();
	~LxMultipartForm();

	void SetBoundary(const LxString &boundary);
	void AddPart(SceInt32 kind, const LxString &dataOrPath,
		     const paf::list<LxString> &headers);
	void AddTextField(const LxString &name, const LxString &value);
	void AddFileField(const LxString &name, const LxString &filePath,
			  const LxString &mimeType);

	SceInt32 GetContentType(LxString &outContentType) const;
	SceInt32 ComputePartLength(LxMultipartPart &part, SceUInt64 *outLen);
	SceInt32 ComputeContentLength(SceUInt64 *outLen);
	SceInt32 SetContentTypeHeader(LxHttpRequest &req) const;
	SceInt32 SendPart(LxMultipartPart &part, LxHttpRequest &req);
	SceInt32 SendAll(LxHttpRequest &req);

	paf::list<LxMultipartPart> m_parts;
	LxString m_boundary;
};

/* MIME type lookup for image file extensions */
const char *LxGetMimeTypeForExtension(const char *ext);

/* Format string helper producing LxString */
LxString LxFormatString(const char *fmt, ...);

/* Extract basename after last '/' or ':' */
const char *LxGetPathBasename(const char *path);

#endif /* TWITTER_DIALOG_TW_LX_MULTIPART_H */
