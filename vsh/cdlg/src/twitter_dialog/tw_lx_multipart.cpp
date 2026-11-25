/*
 * SceTwitterDialogPlugin: multipart/form-data streaming upload and MIME helpers.
 */
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <std/stdio.h>
#include <std/string.h>
#include <thread/thread.h>
#include <file/localfile.h>
#include "tw_lx_multipart.h"

#define LX_ERROR_ABORTED	((SceInt32)0xA0000704)
#define LX_ERROR_NO_MEM		((SceInt32)0xA0000701)
#define LX_ERROR_IO_SHORT_READ	((SceInt32)0xA000070E)
#define SCE_ERROR_ERRNO_ENODEV	((SceInt32)0x80010013)

extern "C" {
SceInt32 sceIoOpen(const char *path, int flags, SceMode mode);
SceInt32 sceIoClose(SceUID fd);
SceInt32 sceIoPread(SceUID fd, void *data, SceSize size, SceOff offset);
}

struct MimeTypeEntry {
	const char *ext;
	const char *mime;
};

static const MimeTypeEntry s_mimeTypes[5] = {
	{ "jpg",  "image/jpeg" },
	{ "jpeg", "image/jpeg" },
	{ "png",  "image/png" },
	{ "gif",  "image/gif" },
	{ "bmp",  "image/bmp" },
};

const char *LxGetMimeTypeForExtension(const char *ext)
{
	if (ext == NULL)
		return NULL;
	for (unsigned int i = 0; i < 5; ++i) {
		if (sce_paf_strcmp(s_mimeTypes[i].ext, ext) == 0)
			return s_mimeTypes[i].mime;
	}
	return NULL;
}

LxString LxFormatString(const char *fmt, ...)
{
	char buf[512];
	va_list ap;

	va_start(ap, fmt);
	int n = sce_paf_vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n <= 0)
		return LxString();
	if ((SceSize)n >= sizeof(buf))
		n = (int)(sizeof(buf) - 1);
	return LxString(buf, (SceSize)n);
}

const char *LxGetPathBasename(const char *path)
{
	if (path == NULL)
		return "";
	const char *slash = sce_paf_strrchr(path, '/');

	if (slash != NULL)
		return slash + 1;
	const char *colon = sce_paf_strrchr(path, ':');

	if (colon != NULL)
		return colon + 1;
	return path;
}

LxMultipartForm::LxMultipartForm()
	: m_parts(), m_boundary()
{
	LxString empty;

	SetBoundary(empty);
}

LxMultipartForm::~LxMultipartForm()
{
}

void LxMultipartForm::SetBoundary(const LxString &boundary)
{
	if (boundary.Empty()) {
		m_boundary.Assign("----------VitaTwitterBoundary", 29);
	} else {
		m_boundary = boundary;
	}
}

void LxMultipartForm::AddPart(SceInt32 kind, const LxString &dataOrPath,
			      const paf::list<LxString> &headers)
{
	LxMultipartPart part;

	part.kind = kind;
	part.dataOrPath = dataOrPath;
	part.headers = headers;
	m_parts.push_back(part);
}

void LxMultipartForm::AddTextField(const LxString &name, const LxString &value)
{
	paf::list<LxString> headers;

	headers.push_back(LxFormatString("Content-Disposition: form-data; name=\"%s\"",
					 name.CStr()));
	AddPart(LxMultipartPart::KIND_MEMORY, value, headers);
}

void LxMultipartForm::AddFileField(const LxString &name, const LxString &filePath,
				   const LxString &mimeType)
{
	paf::list<LxString> headers;

	headers.push_back(LxFormatString(
		"Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"",
		name.CStr(), LxGetPathBasename(filePath.CStr())));
	if (!mimeType.Empty()) {
		headers.push_back(LxFormatString("Content-Type: %s", mimeType.CStr()));
	}
	headers.push_back(LxString("Content-Transfer-Encoding: binary"));
	AddPart(LxMultipartPart::KIND_FILE, filePath, headers);
}

SceInt32 LxMultipartForm::GetContentType(LxString &outContentType) const
{
	LxString result("multipart/form-data, boundary=");

	result.Append(m_boundary);
	outContentType = result;
	return 0;
}

SceInt32 LxMultipartForm::ComputePartLength(LxMultipartPart &part,
					    SceUInt64 *outLen)
{
	SceUInt64 total = 0;

	for (paf::list<LxString>::iterator it = part.headers.begin();
	     it != part.headers.end(); ++it) {
		total += (SceUInt64)(it->Length() + 2);
	}
	total += 2; /* blank line after headers */

	if (part.kind == LxMultipartPart::KIND_FILE) {
		SceInt64 fileSize = paf::LocalFile::GetFileSize(part.dataOrPath.CStr());

		if (fileSize < 0)
			return (SceInt32)fileSize;
		total += (SceUInt64)fileSize + 2;
	} else {
		total += (SceUInt64)part.dataOrPath.Length() + 2;
	}
	*outLen = total;
	return 0;
}

SceInt32 LxMultipartForm::ComputeContentLength(SceUInt64 *outLen)
{
	*outLen = 0;
	SceSize count = m_parts.size();

	if (count == 0)
		return 0;

	SceUInt64 bodySum = 0;

	for (paf::list<LxMultipartPart>::iterator it = m_parts.begin();
	     it != m_parts.end(); ++it) {
		SceUInt64 partLen = 0;
		SceInt32 ret = ComputePartLength(*it, &partLen);

		if (ret < 0)
			return ret;
		bodySum += partLen;
	}
	/* Each part has "--<boundary>\r\n" (4 + boundaryLen), plus closing "--<boundary>--\r\n" (6 + boundaryLen) */
	*outLen = bodySum + (SceUInt64)(4 * count + m_boundary.Length() * count +
					m_boundary.Length() + 6);
	return 0;
}

SceInt32 LxMultipartForm::SetContentTypeHeader(LxHttpRequest &req) const
{
	LxString contentType;
	SceInt32 ret = GetContentType(contentType);

	if (ret < 0)
		return ret;
	ret = req.AddHeader("Content-type", contentType.CStr());
	return (ret < 0) ? ret : 0;
}

SceInt32 LxMultipartForm::SendPart(LxMultipartPart &part, LxHttpRequest &req)
{
	for (paf::list<LxString>::iterator it = part.headers.begin();
	     it != part.headers.end(); ++it) {
		SceInt32 ret = req.Send(it->CStr(), it->Length());

		if (ret < 0)
			return ret;
		ret = req.Send("\r\n", 2);
		if (ret < 0)
			return ret;
	}

	SceInt32 ret = req.Send("\r\n", 2);

	if (ret < 0)
		return ret;

	if (part.kind == LxMultipartPart::KIND_FILE) {
		static const SceSize chunkSizes[3] = { 0x2000, 0x1000, 0x800 };
		LxBuffer chunkBuf;
		bool allocated = false;

		for (int i = 0; i < 3; ++i) {
			if (chunkBuf.Resize(chunkSizes[i])) {
				allocated = true;
				break;
			}
		}
		if (!allocated) {
			chunkBuf.Resize(0);
			return LX_ERROR_NO_MEM;
		}

		const char *path = part.dataOrPath.CStr();
		SceInt64 fileSize = paf::LocalFile::GetFileSize(path);

		if (fileSize < 0) {
			chunkBuf.Resize(0);
			return (SceInt32)fileSize;
		}

		SceUID fd = -1;
		SceUInt32 offset = 0;
		SceUInt32 totalSize = (SceUInt32)fileSize;
		SceSize cap = chunkBuf.Capacity();

		while (offset < totalSize) {
			SceSize toRead = totalSize - offset;

			if (toRead > cap)
				toRead = cap;
			if (fd < 0) {
				fd = sceIoOpen(path, 1, 0);
				if (fd < 0) {
					chunkBuf.Resize(0);
					return fd;
				}
			}

			SceInt32 bytesRead = 0;
			int retries = 0;

			for (;;) {
				bytesRead = sceIoPread(fd, chunkBuf.begin, toRead,
						       (SceOff)offset);
				if (bytesRead != SCE_ERROR_ERRNO_ENODEV)
					break;
				paf::thread::Sleep(1000);
				if (fd >= 0) {
					sceIoClose(fd);
					fd = -1;
				}
				fd = sceIoOpen(path, 1, 0);
				if (fd < 0) {
					chunkBuf.Resize(0);
					return fd;
				}
				retries++;
				if (retries >= 60) {
					bytesRead = 0;
					break;
				}
			}
			if (bytesRead < 0) {
				if (fd >= 0)
					sceIoClose(fd);
				chunkBuf.Resize(0);
				return bytesRead;
			}
			if ((SceSize)bytesRead != toRead) {
				if (fd >= 0)
					sceIoClose(fd);
				chunkBuf.Resize(0);
				return LX_ERROR_IO_SHORT_READ;
			}

			ret = req.Send(chunkBuf.begin, toRead);
			if (ret < 0) {
				if (fd >= 0)
					sceIoClose(fd);
				chunkBuf.Resize(0);
				return ret;
			}
			offset += toRead;
		}
		if (fd >= 0)
			sceIoClose(fd);
		chunkBuf.Resize(0);
	} else {
		ret = req.Send(part.dataOrPath.CStr(), part.dataOrPath.Length());
		if (ret < 0)
			return ret;
	}

	return req.Send("\r\n", 2);
}

SceInt32 LxMultipartForm::SendAll(LxHttpRequest &req)
{
	if (m_parts.size() == 0)
		return 0;

	LxString boundaryLine("--");

	boundaryLine.Append(m_boundary);
	boundaryLine.Append("\r\n", 2);

	for (paf::list<LxMultipartPart>::iterator it = m_parts.begin();
	     it != m_parts.end(); ++it) {
		SceInt32 ret = req.Send(boundaryLine.CStr(), boundaryLine.Length());

		if (ret < 0)
			return ret;
		ret = SendPart(*it, req);
		if (ret < 0)
			return ret;
	}

	SceInt32 ret = req.Send(boundaryLine.CStr(), boundaryLine.Length() - 2);

	if (ret < 0)
		return ret;
	return req.Send("--\r\n", 4);
}
