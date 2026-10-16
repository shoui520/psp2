/*
 * SceNetCtl UPnP client: minimal HTTP/1.1 GET and POST.  The response
 * headers are read line by line (only Content-Length is used) and the body
 * is streamed to an XML parser callback.
 */

#include "netctl_internal.h"
#include "upnp.h"

__attribute__((noinline))
int netCtlUpnpHttpReceive(NetCtlUpnp *upnp, int sock, char *buf, SceSize size,
			  NetCtlUpnpBodyCallback callback)
{
	SceSize used = 0;
	SceSize contentLength = 0;
	SceSize total, received, n;
	int skipLine = 0;
	int headerDone = 0;
	char *end, *p, *next;
	int ret, len, value;

	size--;
	buf[size] = '\0';
	p = buf;
	end = buf + size;
	for (;;) {
		if (p >= end) {
			/* line longer than the buffer: drop it */
			used = 0;
			p = buf;
			skipLine = 1;
		}
		ret = sceNetRecv(sock, p, end - p, 0);
		if (ret < 0)
			return ret;
		if (ret == 0)
			return UPNP_ERROR_BAD_HTTP;
		used += ret;
		while ((len = netCtlUpnpReadLine(buf, used)) != UPNP_ERROR_NO_LINE) {
			if (len < 0)
				return UPNP_ERROR_BAD_HTTP;
			next = buf + len;
			if (skipLine == 1) {
				skipLine = 0;
			} else if (buf[0] == '\0') {
				headerDone = 1;
			} else {
				value = netCtlUpnpSplitHeader(buf);
				if (value != UPNP_ERROR_BAD_URL) {
					if (value < 0)
						return UPNP_ERROR_BAD_HTTP;
					if (sceClibStrncasecmp(buf, "Content-Length", 15) == 0 &&
					    sceClibStrnlen(buf + value, 5) <= 4)
						contentLength = sceClibStrtoll(buf + value, NULL, 10);
				}
			}
			used -= next - buf;
			sceClibMemmove(buf, next, used);
			if (headerDone == 1)
				goto body;
		}
		p = buf + used;
	}

body:
	total = 0;
	if (contentLength != 0)
		total = contentLength;
	received = 0;
	if (used != 0) {
		n = used;
		if (total != 0 && n > total)
			n = total;
		ret = callback(upnp, buf, n, total != 0 && n >= total);
		if (ret < 0)
			goto out;
		received = n;
	}
	while (total == 0 || total > received) {
		ret = sceNetRecv(sock, buf, size, 0);
		if (ret < 0)
			goto out;
		if (ret == 0)
			break;
		if (total != 0 && total < received + ret)
			ret = total - received;
		len = callback(upnp, buf, ret, total != 0 && total <= received + ret);
		if (len < 0) {
			ret = len;
			goto out;
		}
		received += ret;
	}
	ret = received;
out:
	if (ret < 0)
		return ret;
	if (contentLength == 0)
		return ret;
	if (total == ret)
		return ret;
	return UPNP_ERROR_LENGTH_MISMATCH;
}

static int netCtlUpnpDescBody(NetCtlUpnp *upnp, const char *data, SceSize size, int last)
{
	return netCtlUpnpDescParse(upnp, data, size, last);
}

static int netCtlUpnpSoapBody(NetCtlUpnp *upnp, const char *data, SceSize size, int last)
{
	return netCtlUpnpSoapParse(upnp, data, size, last);
}

static inline __attribute__((always_inline))
int netCtlUpnpBuildGet(char *buf, int size, const SceNetSockaddrIn *addr, const char *path)
{
	char host[16];
	char *p = buf;
	int n;

	n = sceClibSnprintf(p, size, "GET %s HTTP/1.1\r\n", path);
	if (n < 0 || n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, "HOST: %s:%d\r\n",
			    sceNetInetNtop(SCE_NET_AF_INET, &addr->sin_addr, host, sizeof(host)),
			    sceNetNtohs(addr->sin_port));
	if (n < 0 || n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, "\r\n");
	if (n < 0 || n >= size)
		goto fail;
	return p + n - buf;
fail:
	return UPNP_ERROR_BUFFER_TOO_SMALL;
}

/* Downloads the device description. */
__attribute__((noinline))
int netCtlUpnpHttpGet(NetCtlUpnp *upnp, int sock, char *buf, int size,
		      const SceNetSockaddrIn *addr, const char *path)
{
	int ret, len;

	ret = sceNetConnect(sock, (const SceNetSockaddr *)addr, sizeof(*addr));
	if (ret < 0)
		return ret;
	len = netCtlUpnpBuildGet(buf, size, addr, path);
	if (len < 0)
		return len;
	ret = sceNetSend(sock, buf, len, 0);
	if (ret < 0)
		return ret;
	if (ret > 0 && ret < len)
		return SCE_NET_ERROR_EINTR;
	return netCtlUpnpHttpReceive(upnp, sock, buf, size, netCtlUpnpDescBody);
}

static inline __attribute__((always_inline))
int netCtlUpnpBuildPost(char *buf, int size, const SceNetSockaddrIn *addr, const char *path,
			int action, int service, SceSize bodyLen)
{
	char host[16];
	char *p = buf;
	int n;

	n = sceClibSnprintf(p, size, "POST %s HTTP/1.1\r\n", path);
	if (n < 0 || n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, "HOST: %s:%d\r\n",
			    sceNetInetNtop(SCE_NET_AF_INET, &addr->sin_addr, host, sizeof(host)),
			    sceNetNtohs(addr->sin_port));
	if (n < 0 || n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, "Content-Length: %lu\r\n", (unsigned long)bodyLen);
	if (n < 0 || n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, "Content-Type: %s\r\n", "text/xml; charset=\"utf-8\"");
	if (n < 0 || n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, "SOAPACTION: \"%s#%s\"\r\n", g_upnpServiceTypes[service],
			    g_upnpActionNames[action]);
	if (n < 0 || n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, "\r\n");
	if (n < 0 || n >= size)
		goto fail;
	return p + n - buf;
fail:
	return UPNP_ERROR_BUFFER_TOO_SMALL;
}

/* Connects (with a timeout when given) and sends a SOAP action. */
__attribute__((noinline))
int netCtlUpnpHttpPost(NetCtlUpnp *upnp, int sock, char *buf, SceSize size,
		       const SceNetSockaddrIn *addr, const char *path, int action, int service,
		       const char *body, SceSize bodyLen, SceUInt32 timeout)
{
	int nbio;
	int error;
	unsigned int optlen;
	SceNetEpollEvent event;
	SceNetEpollEvent result;
	SceUInt32 waited;
	SceSize total;
	int epoll = -1;
	int ret, len, n;

	if (timeout == 0) {
		ret = sceNetConnect(sock, (const SceNetSockaddr *)addr, sizeof(*addr));
	} else {
		nbio = 1;
		ret = sceNetSetsockopt(sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &nbio,
				       sizeof(nbio));
		if (ret < 0)
			goto connected;
		ret = sceNetEpollCreate("SceNetUpnpAction", 0);
		if (ret < 0)
			goto connected;
		epoll = ret;
		sceClibMemset(&event, 0, sizeof(event));
		event.events = SCE_NET_EPOLLIN | SCE_NET_EPOLLOUT;
		ret = sceNetEpollControl(epoll, SCE_NET_EPOLL_CTL_ADD, sock, &event);
		if (ret < 0)
			goto destroy;
		ret = sceNetConnect(sock, (const SceNetSockaddr *)addr, sizeof(*addr));
		if (ret < 0 && ret != SCE_NET_ERROR_EINPROGRESS)
			goto destroy;
		for (waited = 0; waited < timeout; waited += 100000) {
			n = sceNetEpollWait(epoll, &result, 1, 100000);
			if (n < 0)
				goto destroy;
			if (n != 0) {
				optlen = sizeof(error);
				ret = sceNetGetsockopt(sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_ERROR,
						       &error, &optlen);
				if (ret < 0)
					goto destroy;
				if (error != 0) {
					ret = error | 0x80410100;
					goto destroy;
				}
				break;
			}
		}
		if (waited >= timeout) {
			ret = UPNP_ERROR_CONNECT_TIMEOUT;
		} else {
			nbio = 0;
			ret = sceNetSetsockopt(sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &nbio,
					       sizeof(nbio));
		}
destroy:
		if (epoll >= 0)
			sceNetEpollDestroy(epoll);
	}
connected:
	if (ret < 0)
		return ret;

	len = netCtlUpnpBuildPost(buf, size, addr, path, action, service, bodyLen);
	if (len < 0)
		return len;
	total = len + bodyLen;
	if (size < total) {
		ret = sceNetSend(sock, buf, len, 0);
		if (ret < 0)
			return ret;
		if (ret > 0 && ret < len)
			return SCE_NET_ERROR_EINTR;
		ret = sceNetSend(sock, body, bodyLen, 0);
		if (ret < 0)
			return ret;
		if (ret > 0 && (SceSize)ret < bodyLen)
			return SCE_NET_ERROR_EINTR;
	} else {
		sceClibMemcpy(buf + len, body, bodyLen);
		ret = sceNetSend(sock, buf, total, 0);
		if (ret < 0)
			return ret;
		if (ret > 0 && (SceSize)ret < total)
			return SCE_NET_ERROR_EINTR;
	}
	return netCtlUpnpHttpReceive(upnp, sock, buf, size, netCtlUpnpSoapBody);
}
