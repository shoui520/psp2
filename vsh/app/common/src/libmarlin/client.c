/*
 * The remote client: the engine runs in another process, and each step is a message sent
 * through the kind's transport and the reply decoded. The file's name is ours.
 */
#include "mln_internal.h"

/* the result the client keeps for the last step, fetched with its own request */
__attribute__((noinline)) int MlnImpl_8101D5D4(MlnClient *client, MlnApiValue **out)
{
	MlnServiceSession *session = client->kind->remote;
	MlnMessageResult *result;
	MlnApiValue *value;
	MlnBuffer *request = NULL, *reply = NULL;
	MlnMessage message;
	int ret;

	MlnMemset(&message, 0, sizeof(message));
	ret = MlnImpl_81022A1C(&request);
	if (ret != MLN_OK) {
		value = NULL;
		goto out;
	}
	if (session->transact(session->cookie, request, &reply) != 0 || reply == NULL) {
		value = NULL;
		ret = 83;
		goto out;
	}
	ret = MlnImpl_8102371C(reply, &message);
	if (ret != MLN_OK) {
		value = NULL;
		goto out;
	}
	if (message.type != MLN_MESSAGE_RESULT) {
		value = NULL;
		ret = 84;
		goto out;
	}
	result = message.payload;
	if (result->status == MLN_RESULT_VALUE) {
		value = MlnAllocate(MLN_HEAP_SECURE, sizeof(*value));
		if (value == NULL)
			goto nomemory;
		MlnMemset(value, 0, sizeof(*value));
		value->type = 16;
		if (client->account != NULL) {
			value->data[0] = MlnBufferCreate(MLN_HEAP_SECURE, client->account->size,
							 client->account->data);
			if (value->data[0] == NULL)
				goto nomemory;
		}
		if (client->secret != NULL) {
			value->data[1] = MlnBufferCreate(MLN_HEAP_SECURE, client->secret->size,
							 client->secret->data);
			if (value->data[1] == NULL)
				goto nomemory;
		}
		value->data[2] = result->data;
		result->data = NULL;
	} else if (result->status == MLN_RESULT_NONE) {
		if (result->error != MLN_OK) {
			value = NULL;
			ret = result->error;
			goto out;
		}
		value = NULL;
	} else {
		value = NULL;
		ret = result->status == MLN_RESULT_BUSY ? 84 : 86;
		goto out;
	}
	if (out != NULL) {
		*out = value;
		value = NULL;
	}
	goto out;
nomemory:
	ret = MLN_ERROR_NO_MEMORY;
out:
	MlnApiValueFree(value);
	MlnImpl_810233C4(&message);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	if (session != NULL && session->release != NULL && reply != NULL)
		session->release(reply);
	return ret;
}

/*
 * The request a message asks for, as a step's result: a GET of its url, a POST with its body
 * and header lines, or a body alone.
 */
__attribute__((noinline)) int MlnImpl_8101D7C0(MlnMessage *message, MlnApiResult **out)
{
	MlnApiResult *result;
	MlnHttpRequest *request;
	MlnHttpHeader **headers;
	MlnMessagePost *post;
	MlnMessageGet *get;
	MlnBuffer *text;
	char *url, *p, *end, *line;
	unsigned int count, i;
	int expect, ret;

	if (message == NULL) {
		result = NULL;
		ret = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	if ((unsigned int)(message->type - MLN_MESSAGE_HTTP_GET) > 2) {
		result = NULL;
		ret = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	result = MlnAllocate(MLN_HEAP_SECURE, sizeof(*result));
	if (result == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	MlnMemset(result, 0, sizeof(*result));
	switch (message->type) {
	case MLN_MESSAGE_HTTP_POST:
		post = message->payload;
		if (post->method != 0) {
			ret = 6;
			goto out;
		}
		url = NULL;
		request = MlnAllocate(MLN_HEAP_SECURE, sizeof(*request));
		if (request == NULL) {
			ret = MLN_ERROR_NO_MEMORY;
			goto free;
		}
		MlnMemset(request, 0, sizeof(*request));
		text = post->headers;
		if (text == NULL) {
			ret = MLN_CORE_ERROR_INVALID;
			goto free;
		}
		if (text->size != 0) {
			if (text->data == NULL) {
				ret = MLN_CORE_ERROR_INVALID;
				goto free;
			}
			/* each line's name and value end at the ':' and the CR, folded lines kept */
			line = (char *)text->data;
			end = line + text->size;
			count = 0;
			if (line != end) {
				expect = ':';
				p = line;
				do {
					if (*p == '\r') {
						if (expect != '\r' || p + 1 == end || p[1] != '\n')
							goto malformed;
						if (p + 2 >= end || (p[2] != ' ' && p[2] != '\t')) {
							*p = '\0';
							count++;
							expect = p + 2 == end ? 0 : ':';
						}
					} else {
						if (*p == ':') {
							if (expect == ':') {
								expect = '\r';
								*p = '\0';
							}
						} else if (*p == '\0') {
							goto malformed;
						}
						if (p + 1 == end)
							goto malformed;
					}
					p++;
				} while (expect != 0);
			}
			headers = MlnAllocate(MLN_HEAP_SECURE, count * sizeof(*headers));
			if (headers == NULL) {
				ret = MLN_ERROR_NO_MEMORY;
				goto free;
			}
			MlnMemset(headers, 0, count * sizeof(*headers));
			p = line;
			for (i = 0; i != count; i++) {
				headers[i] = MlnAllocate(MLN_HEAP_SECURE, sizeof(**headers));
				if (headers[i] == NULL) {
					ret = MLN_ERROR_NO_MEMORY;
					goto drop;
				}
				MlnMemset(headers[i], 0, sizeof(**headers));
				headers[i]->name = p;
				while (*p++ != '\0')
					;
				while (*p == ' ')
					p++;
				headers[i]->value = p;
				while (*p++ != '\0')
					;
				p++;
			}
			ret = MlnHttpHeadersCreate(count, headers, &request->headers);
drop:
			for (i = 0; i != count; i++)
				MlnFree(MLN_HEAP_SECURE, headers[i]);
			MlnFree(MLN_HEAP_SECURE, headers);
			if (ret != MLN_OK)
				goto free;
		}
		ret = MlnBufferToString(MLN_HEAP_SECURE, post->url, &url);
		if (ret != MLN_OK)
			goto free;
		request->method = MLN_HTTP_POST;
		request->body = post->body;
		request->url = url;
		url = NULL;
		post->body = NULL;
		result->request = request;
		request = NULL;
		MlnFree(MLN_HEAP_SECURE, url);
		MlnHttpRequestFree(request);
		break;
malformed:
		ret = 84;
free:
		MlnFree(MLN_HEAP_SECURE, url);
		MlnHttpRequestFree(request);
		goto out;
	case MLN_MESSAGE_HTTP_BODY:
		result->request = MlnAllocate(MLN_HEAP_SECURE, sizeof(*result->request));
		if (result->request == NULL) {
			ret = MLN_ERROR_NO_MEMORY;
			goto out;
		}
		MlnMemset(result->request, 0, sizeof(*result->request));
		result->request->body = *(MlnBuffer **)message->payload;
		*(MlnBuffer **)message->payload = NULL;
		break;
	case MLN_MESSAGE_HTTP_GET:
		get = message->payload;
		url = NULL;
		if (get == NULL) {
			request = NULL;
			ret = MLN_CORE_ERROR_INVALID;
			goto get;
		}
		request = MlnAllocate(MLN_HEAP_SECURE, sizeof(*request));
		if (request == NULL) {
			ret = MLN_ERROR_NO_MEMORY;
			goto get;
		}
		MlnMemset(request, 0, sizeof(*request));
		ret = MlnBufferToString(MLN_HEAP_SECURE, get->url, &url);
		if (ret != MLN_OK)
			goto get;
		request->method = MLN_HTTP_GET;
		request->url = url;
		url = NULL;
		result->request = request;
		request = NULL;
		MlnFree(MLN_HEAP_SECURE, url);
		MlnHttpRequestFree(request);
		break;
get:
		MlnFree(MLN_HEAP_SECURE, url);
		MlnHttpRequestFree(request);
		goto out;
	default:
		ret = 7;
		goto out;
	}
	*out = result;
	result = NULL;
	ret = MLN_OK;
out:
	MlnApiResultFree(result);
	return ret;
}

/* a status message: the result it announces, as a step's value */
int MlnImpl_8101DC14(MlnClient *client, int failed, int result,
					       MlnApiResult **out)
{
	MlnApiValue *value = NULL;
	MlnApiResult *step;
	int ret;

	if (result == 1) {
		ret = MlnImpl_8101D5D4(client, &value);
		if (ret != MLN_OK)
			goto out;
	} else if (failed) {
		ret = 86;
		goto out;
	}
	ret = MLN_OK;
	if (out != NULL) {
		step = MlnAllocate(MLN_HEAP_SECURE, sizeof(*step));
		if (step == NULL) {
			ret = MLN_ERROR_NO_MEMORY;
			goto out;
		}
		MlnMemset(step, 0, sizeof(*step));
		step->value = value;
		*out = step;
		value = NULL;
	}
out:
	MlnApiResultFree(NULL);
	MlnApiValueFree(value);
	return ret;
}

/* a status message where a reply was due: the status's error, else unexpected */
__attribute__((noinline)) int MlnImpl_8101DCB4(MlnClient *client,
					       const MlnMessageStatus *status)
{
	int ret;

	ret = MlnImpl_8101DC14(client, status->failed, status->result, NULL);
	if (ret == MLN_OK)
		ret = 86;
	return ret;
}

/* opens a remote client on the kind's transport, with its version */
__attribute__((noinline)) int MlnImpl_8101DD24(MlnApiKind *kind, MlnBuffer *address,
					       MlnClient **out, MlnApiVersion *version)
{
	MlnServiceSession *session;
	MlnApiVersion *reported;
	MlnBuffer *request, *reply;
	MlnMessage message;
	MlnClient *client;
	int ret;

	if (kind == NULL || kind->type != 1 || kind->remote == NULL ||
	    kind->remote->transact == NULL || kind->remote->release == NULL || out == NULL)
		return MLN_CORE_ERROR_INVALID;
	client = MlnAllocate(MLN_HEAP_SECURE, sizeof(*client));
	if (client == NULL)
		return MLN_ERROR_NO_MEMORY;
	MlnMemset(client, 0, sizeof(*client));
	client->kind = kind;
	client->busy = 0;
	client->pending.type = MLN_MESSAGE_NONE;
	request = NULL;
	reply = NULL;
	MlnMemset(&message, 0, sizeof(message));
	message.type = MLN_MESSAGE_NONE;
	ret = MlnImpl_810222A0(address, &request);
	if (ret != MLN_OK) {
		MlnImpl_810233C4(&message);
		MlnBufferDestroy(MLN_HEAP_SECURE, request);
		goto fail;
	}
	session = client->kind->remote;
	if (session->transact(session->cookie, request, &reply) != 0 || reply == NULL) {
		ret = 83;
		goto out;
	}
	ret = MlnImpl_8102371C(reply, &message);
	if (ret != MLN_OK)
		goto out;
	if (message.type == MLN_MESSAGE_STATUS) {
		/* MlnImpl_8101DCB4's check, written out */
		ret = MlnImpl_8101DC14(client, ((MlnMessageStatus *)message.payload)->failed,
				       ((MlnMessageStatus *)message.payload)->result, NULL);
		if (ret == MLN_OK)
			ret = 86;
	} else if (message.type == MLN_MESSAGE_VERSION) {
		reported = message.payload;
		client->build = reported->build;
		if (version != NULL) {
			version->major = reported->major;
			version->minor = reported->minor;
			version->build = reported->build;
			version->name = reported->name;
			version->id = reported->id;
			reported->name = NULL;
			reported->id = NULL;
		}
	} else {
		ret = 84;
	}
out:
	MlnImpl_810233C4(&message);
	if (reply != NULL)
		session->release(reply);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	if (ret == MLN_OK) {
		*out = client;
		return ret;
	}
fail:
	if (client->pending.type != MLN_MESSAGE_NONE)
		MlnImpl_810233C4(&client->pending);
	MlnFree(MLN_HEAP_SECURE, client);
	return ret;
}

__attribute__((noinline)) int MlnImpl_8101DF70(MlnClient *client)
{
	if (client == NULL)
		return MLN_OK;
	if (client->pending.type != MLN_MESSAGE_NONE)
		MlnImpl_810233C4(&client->pending);
	MlnFree(MLN_HEAP_SECURE, client);
	return MLN_OK;
}

/* a request of the client's that the reply answers with two values */
__attribute__((noinline)) int MlnImpl_8101DFA0(MlnClient *client, void *a1, void *a2, int *first,
					       int *second)
{
	MlnServiceSession *session;
	MlnBuffer *request = NULL, *reply = NULL;
	MlnMessage message;
	int *pair;
	int ret;

	MlnMemset(&message, 0, sizeof(message));
	if (client == NULL || client->kind == NULL || client->kind->type != 1 ||
	    client->kind->remote == NULL) {
		ret = 3;
		goto free;
	}
	ret = MlnImpl_81022478(a1, a2, &request);
	if (ret != MLN_OK)
		goto free;
	session = client->kind->remote;
	if (session->transact(session->cookie, request, &reply) != 0 || reply == NULL) {
		ret = 83;
		goto out;
	}
	ret = MlnImpl_8102371C(reply, &message);
	if (ret != MLN_OK)
		goto out;
	if (message.type == MLN_MESSAGE_STATUS) {
		ret = MlnImpl_8101DCB4(client, message.payload);
	} else if (message.type == MLN_MESSAGE_PAIR) {
		pair = message.payload;
		if (pair == NULL || first == NULL || second == NULL) {
			ret = MLN_CORE_ERROR_INVALID;
		} else {
			*first = pair[0];
			*second = pair[1];
		}
	} else {
		ret = 84;
	}
out:
	MlnImpl_810233C4(&message);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	if (session->release != NULL)
		session->release(reply);
	return ret;
free:
	MlnImpl_810233C4(&message);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	return ret;
}

/* a request of the client's that the reply answers with pairs of buffers */
__attribute__((noinline)) int MlnImpl_8101E118(MlnClient *client, MlnBuffer *a1, MlnBuffer *a2,
					       unsigned int *count, MlnBuffer ***items)
{
	MlnServiceSession *session;
	MlnBuffer *request = NULL, *reply = NULL;
	MlnMessagePairs *pairs;
	MlnBuffer **buffers;
	MlnMessage message;
	unsigned int i;
	int ret;

	MlnMemset(&message, 0, sizeof(message));
	if (client == NULL || client->kind == NULL || client->kind->type != 1 ||
	    client->kind->remote == NULL) {
		ret = 3;
		MlnImpl_810233C4(&message);
		goto free;
	}
	ret = MlnImpl_81022570(a1, a2, &request);
	if (ret != MLN_OK) {
		MlnImpl_810233C4(&message);
		goto free;
	}
	session = client->kind->remote;
	if (session->transact(session->cookie, request, &reply) != 0 || reply == NULL) {
		ret = 83;
		goto out;
	}
	ret = MlnImpl_8102371C(reply, &message);
	if (ret != MLN_OK)
		goto out;
	if (message.type == MLN_MESSAGE_STATUS) {
		ret = MlnImpl_8101DCB4(client, message.payload);
		goto out;
	}
	if (message.type != MLN_MESSAGE_PAIRS) {
		ret = 84;
		goto out;
	}
	pairs = message.payload;
	if (pairs == NULL || count == NULL || items == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	if (pairs->count * 2 != 0) {
		if (pairs->items == NULL) {
			ret = 7;
			goto out;
		}
		buffers = MlnAllocate(MLN_HEAP_SECURE, pairs->count * 2 * sizeof(*buffers));
		if (buffers == NULL) {
			ret = MLN_ERROR_NO_MEMORY;
			goto out;
		}
		MlnMemset(buffers, 0, pairs->count * 2 * sizeof(*buffers));
		for (i = 0; i < pairs->count; i++) {
			if (pairs->items[i] == NULL) {
				/* frees as many buffers as there are pairs, from the first */
				for (i = 0; i < pairs->count; i++)
					MlnBufferDestroy(MLN_HEAP_SECURE, buffers[i]);
				MlnFree(MLN_HEAP_SECURE, buffers);
				ret = 7;
				goto out;
			}
			buffers[2 * i] = pairs->items[i]->pair->first;
			pairs->items[i]->pair->first = NULL;
			buffers[2 * i + 1] = pairs->items[i]->pair->second;
			pairs->items[i]->pair->second = NULL;
		}
	} else {
		buffers = NULL;
	}
	*count = pairs->count * 2;
	*items = buffers;
out:
	MlnImpl_810233C4(&message);
	if (session->release != NULL)
		session->release(reply);
free:
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	return ret;
}

/*
 * Starts an action token on the client (start set; else it goes on with the token it runs):
 * the reply is the first request the client asks for.
 */
__attribute__((noinline)) int MlnImpl_8101E380(MlnClient *client, const MlnBuffer *token,
					       MlnBuffer *account, MlnBuffer *secret, int start)
{
	MlnServiceSession *session;
	MlnBuffer *request = NULL, *reply = NULL;
	MlnMessage message;
	int failed = 1;
	int ret;

	MlnMemset(&message, 0, sizeof(message));
	if (client == NULL || (token != NULL && (token->size == 0 || token->data == NULL)) ||
	    (secret != NULL && secret->size != 0 && secret->data == NULL)) {
		ret = MLN_CORE_ERROR_INVALID;
		MlnImpl_810233C4(&message);
		goto free;
	}
	if (client->kind == NULL || client->kind->type != 1 ||
	    (session = client->kind->remote) == NULL) {
		ret = 3;
		MlnImpl_810233C4(&message);
		goto free;
	}
	if (session->transact == NULL || session->release == NULL) {
		ret = 3;
		goto out;
	}
	client->busy = 1;
	client->account = account;
	client->secret = secret;
	client->pending.type = MLN_MESSAGE_NONE;
	if (start)
		ret = MlnImpl_81021734(token == NULL, token, account, secret, &request);
	else
		ret = MlnImpl_810217E4(2, &request);
	if (ret != MLN_OK)
		goto out;
	if (session->transact(session->cookie, request, &reply) != 0 || reply == NULL) {
		ret = 83;
		goto out;
	}
	ret = MlnImpl_8102371C(reply, &message);
	if (ret != MLN_OK)
		goto out;
	switch (message.type) {
	case MLN_MESSAGE_HTTP_GET:
	case MLN_MESSAGE_HTTP_POST:
		break;
	case MLN_MESSAGE_HTTP_BODY:
		if (start) {
			ret = 84;
			goto out;
		}
		break;
	case MLN_MESSAGE_STATUS:
		ret = MlnImpl_8101DCB4(client, message.payload);
		if (ret != MLN_OK)
			goto out;
		client->busy = 0;
		failed = 0;
		goto out;
	default:
		ret = 84;
		goto out;
	}
	client->pending.type = message.type;
	client->pending.payload = message.payload;
	message.payload = NULL;
	failed = 0;
out:
	MlnImpl_810233C4(&message);
	if (reply != NULL)
		session->release(reply);
free:
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	if (client != NULL && failed)
		client->busy = 0;
	return ret;
}

/* stops what the client runs */
__attribute__((noinline)) int MlnImpl_8101E628(MlnClient *client)
{
	if (client != NULL) {
		client->busy = 0;
		MlnImpl_810233C4(&client->pending);
		client->pending.type = MLN_MESSAGE_NONE;
		client->pending.payload = NULL;
	}
	return MLN_OK;
}
