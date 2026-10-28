#include "uri.h"
#include "../std/string.h"

extern size_t sceClibStrnlen(const char *str, size_t max_len);
extern int sce_paf_look_ctype_table(int ch);
extern int sce_paf_tolower(int ch);
extern int sce_paf_strncasecmp(const char *left, const char *right, size_t len);
extern char *sce_paf_strchr(const char *str, int ch);
extern char *sce_paf_strrchr(const char *str, int ch);

int UrlUriParse(SceHttpUriElement *out, const char *srcUrl, void *pool,
		SceSize *require, SceSize prepare)
{
	char scheme[128];
	char *buffer = pool;
	const char *src;
	size_t bound, count, consumed, used;
	size_t user_len = 0, password_start = 0, password_size = 0;
	size_t host_size, path_size, query_size;
	int write = 1, has_scheme = 0, has_password = 0;
	int has_port = 0, invalid_port = 0;
	unsigned char ch;
	char *path, *query, *fragment;

	if (!srcUrl)
		return 0;
	/* Without both outputs, only the required pool size is computed. */
	if (buffer && out)
		sce_paf_memset(out, 0, sizeof(*out));
	else if (!require)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	else
		write = 0;

	/* The scheme scan is bounded, but its first character is checked separately. */
	src = srcUrl;
	count = 0;
	bound = sceClibStrnlen(srcUrl, 32);
	ch = *srcUrl;
	while (((sce_paf_look_ctype_table(ch) & 7) ||
		ch == '+' || ch == '-' || ch == '.') && count != bound) {
		ch = *++src;
		++count;
	}
	if ((sce_paf_look_ctype_table((signed char)*srcUrl) & 4) ||
	    srcUrl[count] != ':') {
		used = 1;
		consumed = 0;
		if (write) {
			if (prepare <= 1)
				return SCE_HTTP_ERROR_OUT_OF_MEMORY;
			buffer[0] = 0;
		}
	} else {
		used = count + 1;
		if (write) {
			if (used > prepare)
				return SCE_HTTP_ERROR_OUT_OF_MEMORY;
			sce_paf_memcpy(buffer, srcUrl, count);
			buffer[count] = 0;
		}
		consumed = count + 1;
	}
	if ((int32_t)consumed < 0)
		return (int32_t)consumed;
	if (consumed)
		has_scheme = 1;
	if (write)
		out->scheme = buffer;
	src = srcUrl + consumed;
	if (*src == '/') {
		size_t slashes = 0;
		const char *next = src;

		do {
			++next;
			++slashes;
		} while (*next == '/');
		if (slashes >= 2 && has_scheme) {
			consumed += 2;
			src = srcUrl + consumed;
		} else if (write) {
			out->opaque = 1;
		}
	} else if (write) {
		out->opaque = 1;
	}

	/* Credentials are consumed only when a permitted sequence reaches '@'. */
	count = 0;
	ch = *src;
	if (ch) {
		while (ch != '@') {
			if (!has_password && ch == ':') {
				user_len = count;
				has_password = 1;
				password_start = count + 1;
			} else if (!(sce_paf_look_ctype_table(ch) & 7) &&
				   ch != '-' && ch != '_' && ch != '.' && ch != '!' &&
				   ch != '~' && ch != '*' && ch != '\'' && ch != '(' &&
				   ch != ')' && ch != '%' && ch != ';' && ch != ':' &&
				   ch != '&' && ch != '=' && ch != '+' && ch != '$' &&
				   ch != ',') {
				break;
			}
			ch = src[++count];
			if (!ch)
				break;
		}
	}
	if (ch == '@') {
		size_t password_len;

		if (has_password) {
			password_size = count - user_len;
			password_len = count - user_len - 1;
		} else {
			user_len = count;
			password_start = count + 1;
			password_size = 1;
			password_len = 0;
		}
		if (write) {
			if (prepare - used < password_start + password_size)
				return SCE_HTTP_ERROR_OUT_OF_MEMORY;
			sce_paf_memcpy(buffer + used, src, user_len);
			buffer[used + user_len] = 0;
			sce_paf_memcpy(buffer + used + password_start,
				       src + password_start, password_len);
			buffer[used + password_start + password_len] = 0;
		}
		++count;
	} else {
		password_start = 1;
		password_size = 1;
		if (write) {
			if (prepare - used < 2)
				return SCE_HTTP_ERROR_OUT_OF_MEMORY;
			buffer[used] = 0;
			buffer[used + 1] = 0;
		}
		count = 0;
	}
	if ((int32_t)count < 0)
		return (int32_t)count;
	if (write) {
		out->username = buffer + used;
		out->password = buffer + used + password_start;
	}
	consumed += count;
	used += password_start + password_size;
	src = srcUrl + consumed;
	count = 0;
	ch = *src;
	if (ch != '.') {
		while ((sce_paf_look_ctype_table(ch) & 7) || ch == '.' || ch == '-')
			ch = src[++count];
	}
	if (write) {
		if (count + 1 > prepare - used)
			return SCE_HTTP_ERROR_OUT_OF_MEMORY;
		sce_paf_memcpy(buffer + used, src, count);
		buffer[used + count] = 0;
	}
	host_size = count + 1;
	if ((int32_t)count < 0)
		return (int32_t)count;
	if (write)
		out->hostname = buffer + used;
	consumed += count;
	used += host_size;
	if (srcUrl[consumed] == ':') {
		++consumed;
		has_port = 1;
	}
	src = srcUrl + consumed;
	if (has_port) {
		uint32_t port = 0;
		uint16_t value = 0;
		int digit;

		count = 0;
		digit = (signed char)*src;
		while (sce_paf_look_ctype_table(digit) & 4) {
			if (count >= 5) {
				count = 0;
				break;
			}
			port = (signed char)src[count] - '0' + 10 * port;
			digit = (signed char)src[++count];
		}
		/* Native parsing accepts 65536 and narrows it to a zero port. */
		if (port <= 0x10000 && (src[count] == '/' || !src[count]))
			value = port;
		else
			count = 0;
		if ((int32_t)count < 0)
			return (int32_t)count;
		if (count) {
			if (write)
				out->port = value;
			consumed += count;
			src = srcUrl + consumed;
		} else {
			invalid_port = 1;
		}
	}
	if ((!has_port || invalid_port) && write) {
		const char *name = out->scheme;
		uint16_t port = 0;

		if (name) {
			int32_t size = sceClibStrnlen(name, 32);

			if (size <= 32) {
				size_t i = 0;

				while (size-- > 0) {
					scheme[i++] = sce_paf_tolower((signed char)*name++);
				}
				scheme[i] = 0;
				if (!sce_paf_strncasecmp(scheme, "HTTPS",
						 sceClibStrnlen("HTTPS", 32)))
					port = 443;
				else if (!sce_paf_strncasecmp(scheme, "HTTP",
						      sceClibStrnlen("HTTP", 32)))
					port = 80;
				else if (!sce_paf_strncasecmp(scheme, "TTP",
						      sceClibStrnlen("TTP", 32)))
					port = 80;
			}
		}
		out->port = port;
	}

	/* Pool accounting keeps the original path length after normalization. */
	path = buffer + used;
	count = 0;
	while (src[count] && src[count] != '?' && src[count] != '#')
		++count;
	if (write) {
		if (count + 1 > prepare - used)
			return SCE_HTTP_ERROR_OUT_OF_MEMORY;
		sce_paf_memcpy(path, src, count);
		path[count] = 0;
		if (count != (size_t)-1) {
			if (*src == '/') {
				char *end = path;
				size_t pos = 1;

				path[0] = '/';
				path[1] = 0;
				while (pos < count) {
					if (src[pos] == '.' && src[pos + 1] == '.' &&
					    src[pos + 2] == '/') {
						if (end != path) {
							*end = 0;
							end = sce_paf_strrchr(path, '/');
							if (end)
								end[1] = 0;
						}
						pos += 3;
					} else if (src[pos] == '.' && src[pos + 1] == '/') {
						pos += 2;
					} else {
						const char *begin = src + pos;
						char *slash = sce_paf_strchr(begin, '/');
						size_t length = slash ? (size_t)(slash - begin + 1) :
							       count - pos;

						if (slash && length > count - pos)
							length = count - pos;
						sce_paf_memcpy(end + 1, begin, length);
						end += length;
						pos += length;
						end[1] = 0;
					}
				}
			} else {
				sce_paf_memcpy(path, src, count);
				path[count] = 0;
			}
		}
	}
	path_size = count + 1;
	if ((int32_t)count < 0)
		return (int32_t)count;
	if (write)
		out->path = path;
	consumed += count;
	used += path_size;
	query = buffer + used;
	src = srcUrl + consumed;
	count = 0;
	while (src[count] && src[count] != '#')
		++count;
	if (write) {
		if (count + 1 > prepare - used)
			return SCE_HTTP_ERROR_OUT_OF_MEMORY;
		sce_paf_memcpy(query, src, count);
		query[count] = 0;
	}
	query_size = count + 1;
	if ((int32_t)count < 0)
		return (int32_t)count;
	if (write)
		out->query = query;
	consumed += count;
	used += query_size;
	fragment = buffer + used;
	src = srcUrl + consumed;
	count = 0;
	while (src[count])
		++count;
	if (write) {
		if (count + 1 > prepare - used)
			return SCE_HTTP_ERROR_OUT_OF_MEMORY;
		sce_paf_memcpy(fragment, src, count);
		fragment[count] = 0;
	}
	if ((int32_t)count < 0)
		return (int32_t)count;
	if (write)
		out->fragment = fragment;
	if (require)
		*require = used + count + 1;
	return 0;
}
