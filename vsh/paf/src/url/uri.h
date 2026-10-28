#ifndef PAF_URL_URI_H
#define PAF_URL_URI_H

#include <libhttp.h>

int UrlUriParse(SceHttpUriElement *out, const char *srcUrl, void *pool,
		SceSize *require, SceSize prepare);

#endif
