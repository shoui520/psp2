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

__attribute__((noinline)) void MlnImpl_8101DF70(MlnClient *client)
{
	if (client == NULL)
		return;
	if (client->pending.type != MLN_MESSAGE_NONE)
		MlnImpl_810233C4(&client->pending);
	MlnFree(MLN_HEAP_SECURE, client);
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

/* a value message: the value of an action, with the client's account and secret */
static inline __attribute__((always_inline)) int MlnClientValue(MlnClient *client,
								MlnMessageValue *payload,
								MlnApiResult **out)
{
	MlnBuffer *account = client->account, *secret = client->secret;
	MlnApiResult *step = NULL;
	MlnApiValue *value = NULL;
	int ret;

	if (payload == NULL) {
		account = NULL;
		secret = NULL;
		ret = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	if (payload->kind != 1) {
		account = NULL;
		secret = NULL;
		ret = 6;
		goto out;
	}
	if (account != NULL) {
		account = MlnBufferCreate(MLN_HEAP_SECURE, account->size, account->data);
		if (account == NULL) {
			secret = NULL;
			ret = MLN_ERROR_NO_MEMORY;
			goto out;
		}
	}
	if (secret != NULL) {
		secret = MlnBufferCreate(MLN_HEAP_SECURE, secret->size, secret->data);
		if (secret == NULL) {
			ret = MLN_ERROR_NO_MEMORY;
			goto out;
		}
	}
	value = MlnAllocate(MLN_HEAP_SECURE, sizeof(*value));
	if (value == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	MlnMemset(value, 0, sizeof(*value));
	value->type = 5;
	value->data[0] = account;
	value->data[1] = secret;
	value->data[2] = (void *)1;
	value->data[3] = payload->data;
	payload->data = NULL;
	account = NULL;
	secret = NULL;
	step = MlnAllocate(MLN_HEAP_SECURE, sizeof(*step));
	if (step == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	MlnMemset(step, 0, sizeof(*step));
	step->value = value;
	value = NULL;
	*out = step;
	step = NULL;
	ret = MLN_OK;
out:
	MlnApiValueFree(value);
	MlnBufferDestroy(MLN_HEAP_SECURE, account);
	MlnBufferDestroy(MLN_HEAP_SECURE, secret);
	MlnApiResultFree(step);
	return ret;
}

/*
 * A step of the action token the client runs: the request it asked for before, or the
 * client's answer to the response to the last one.
 */
__attribute__((noinline)) int MlnImpl_8101E658(MlnClient *client, MlnBuffer **message,
					       MlnApiResult **out)
{
	MlnServiceSession *session;
	MlnBuffer *request = NULL, *reply = NULL;
	MlnApiResult *result = NULL;
	MlnMessageStatus *status;
	MlnMessage decoded;
	int ret;

	MlnMemset(&decoded, 0, sizeof(decoded));
	if (client == NULL || out == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto free;
	}
	if (client->kind == NULL || client->kind->type != 1 ||
	    (session = client->kind->remote) == NULL || session->transact == NULL ||
	    session->release == NULL || client->busy != 1) {
		ret = 3;
		goto free;
	}
	if (client->pending.type != MLN_MESSAGE_NONE) {
		ret = MlnImpl_8101D7C0(&client->pending, &result);
		MlnImpl_810233C4(&client->pending);
		client->pending.payload = NULL;
		client->pending.type = MLN_MESSAGE_NONE;
		if (ret != MLN_OK)
			goto out;
		goto done;
	}
	ret = MlnImpl_81021BAC(message != NULL ? *message : NULL, &request);
	if (ret != MLN_OK)
		goto out;
	if (session->transact(session->cookie, request, &reply) != 0 || reply == NULL) {
		ret = 83;
		goto out;
	}
	ret = MlnImpl_8102371C(reply, &decoded);
	if (ret != MLN_OK)
		goto out;
	switch (decoded.type) {
	case MLN_MESSAGE_HTTP_GET:
	case MLN_MESSAGE_HTTP_POST:
		ret = MlnImpl_8101D7C0(&decoded, &result);
		break;
	case MLN_MESSAGE_VALUE:
		ret = MlnClientValue(client, decoded.payload, &result);
		break;
	case MLN_MESSAGE_STATUS:
		status = decoded.payload;
		if (status == NULL) {
			ret = 7;
			goto out;
		}
		ret = MlnImpl_8101DC14(client, status->failed, status->result, &result);
		break;
	default:
		ret = 84;
		goto out;
	}
	if (ret != MLN_OK)
		goto out;
done:
	*out = result;
	result = NULL;
	ret = MLN_OK;
out:
	MlnApiResultFree(result);
	MlnImpl_810233C4(&decoded);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	if (session->release != NULL)
		session->release(reply);
	return ret;
free:
	MlnApiResultFree(result);
	MlnImpl_810233C4(&decoded);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	return ret;
}

/* hands the client a step's request; the reply is the body to send with it */
__attribute__((noinline)) int MlnImpl_8101E9EC(MlnClient *client, MlnApiResult *step,
					       MlnBuffer **message)
{
	MlnServiceSession *session;
	MlnBuffer *request = NULL, *reply = NULL, *body = NULL;
	MlnHttpRequest *http;
	MlnMessage decoded;
	int ret;

	if (client == NULL) {
		ret = 3;
		goto free;
	}
	if (step == NULL || step->request == NULL || message == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto free;
	}
	if (client->kind == NULL || client->kind->type != 1 ||
	    (session = client->kind->remote) == NULL || session->transact == NULL ||
	    session->release == NULL || client->busy != 1) {
		ret = 3;
		goto free;
	}
	MlnMemset(&decoded, 0, sizeof(decoded));
	http = step->request;
	session = client->kind->remote;
	if (http->method == MLN_HTTP_GET)
		ret = MlnImpl_81021864(http->url, &request);
	else
		ret = MlnImpl_8102190C(0, http->url, http->body, &http->headers, &request);
	if (ret != MLN_OK)
		goto out;
	if (session->transact(session->cookie, request, &reply) != 0 || reply == NULL) {
		ret = 83;
		goto release;
	}
	ret = MlnImpl_8102371C(reply, &decoded);
	if (ret != MLN_OK)
		goto release;
	if (decoded.type == MLN_MESSAGE_HTTP_BODY) {
		body = *(MlnBuffer **)decoded.payload;
		*(MlnBuffer **)decoded.payload = NULL;
	} else if (decoded.type != MLN_MESSAGE_STATUS) {
		ret = 84;
		goto release;
	} else if (decoded.payload == NULL) {
		ret = 7;
		goto release;
	} else {
		ret = MlnImpl_8101DCB4(client, decoded.payload);
		if (ret != MLN_OK)
			goto release;
	}
	*message = body;
	body = NULL;
release:
	MlnBufferDestroy(MLN_HEAP_SECURE, body);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	MlnImpl_810233C4(&decoded);
	if (session->release != NULL)
		session->release(reply);
	return ret;
out:
	MlnBufferDestroy(MLN_HEAP_SECURE, body);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	MlnImpl_810233C4(&decoded);
	if (session != NULL && session->release != NULL)
		session->release(reply);
	return ret;
free:
	MlnBufferDestroy(MLN_HEAP_SECURE, NULL);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	MlnImpl_810233C4(&decoded);
	return ret;
}

/* the client's list: pairs of buffers, first and second of each in turn */
__attribute__((noinline)) int MlnImpl_8101EC08(MlnClient *client, unsigned int *count,
					       MlnBuffer ***items)
{
	MlnServiceSession *session = NULL;
	MlnBuffer *request = NULL, *reply = NULL;
	MlnBuffer **buffers = NULL;
	MlnMessagePairs *pairs;
	MlnMessage decoded;
	unsigned int total = 0, i;
	int ret;

	MlnMemset(&decoded, 0, sizeof(decoded));
	if (client == NULL || client->kind == NULL || client->kind->type != 1 ||
	    client->kind->remote == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_81022180(&request);
	if (ret != MLN_OK)
		goto out;
	session = client->kind->remote;
	if (session->transact(session->cookie, request, &reply) != 0 || reply == NULL) {
		ret = 83;
		goto out;
	}
	ret = MlnImpl_8102371C(reply, &decoded);
	if (ret != MLN_OK)
		goto out;
	if (decoded.type == MLN_MESSAGE_STATUS) {
		ret = MlnImpl_8101DCB4(client, decoded.payload);
		if (ret != MLN_OK)
			goto out;
	} else if (decoded.type != MLN_MESSAGE_LIST) {
		ret = 84;
		goto out;
	} else {
		pairs = decoded.payload;
		total = pairs->count * 2;
		if (total != 0) {
			if (pairs->items == NULL) {
				ret = 7;
				total = 0;
				goto out;
			}
			buffers = MlnAllocate(MLN_HEAP_SECURE, total * sizeof(*buffers));
			if (buffers == NULL) {
				ret = MLN_ERROR_NO_MEMORY;
				total = 0;
				goto out;
			}
			MlnMemset(buffers, 0, total * sizeof(*buffers));
			for (i = 0; i < pairs->count; i++) {
				if (pairs->items[i] == NULL) {
					ret = 7;
					goto out;
				}
				buffers[2 * i] = pairs->items[i]->pair->first;
				pairs->items[i]->pair->first = NULL;
				buffers[2 * i + 1] = pairs->items[i]->pair->second;
				pairs->items[i]->pair->second = NULL;
			}
		}
	}
	*count = total;
	*items = buffers;
	buffers = NULL;
out:
	MlnImpl_810233C4(&decoded);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	if (buffers != NULL) {
		for (i = 0; i != total; i++)
			MlnBufferDestroy(MLN_HEAP_SECURE, buffers[i]);
		MlnFree(MLN_HEAP_SECURE, buffers);
	}
	if (session != NULL && session->release != NULL)
		session->release(reply);
	return ret;
}

/* a license's rights, summarized by the client; none when it gives nothing */
__attribute__((noinline)) int MlnImpl_8101EE90(MlnClient *client, MlnBuffer *license,
					       MlnRightsSummary **out)
{
	MlnServiceSession *session;
	MlnBuffer *request = NULL, *reply = NULL;
	MlnRightsSummary *rights, *summary;
	MlnMessage decoded;
	unsigned int count, i;
	int ret;

	MlnMemset(&decoded, 0, sizeof(decoded));
	if (client == NULL || client->kind == NULL || client->kind->type != 1 ||
	    client->kind->remote == NULL) {
		ret = 3;
		goto free;
	}
	ret = MlnImpl_81021D70(license, &request);
	if (ret != MLN_OK)
		goto free;
	session = client->kind->remote;
	if (session->transact(session->cookie, request, &reply) != 0 || reply == NULL) {
		ret = 83;
		goto out;
	}
	ret = MlnImpl_8102371C(reply, &decoded);
	if (ret != MLN_OK)
		goto out;
	if (decoded.type == MLN_MESSAGE_STATUS) {
		ret = MlnImpl_8101DCB4(client, decoded.payload);
		if (ret != MLN_OK)
			goto out;
		summary = NULL;
	} else if (decoded.type != MLN_MESSAGE_RIGHTS) {
		ret = 84;
		goto out;
	} else {
		rights = decoded.payload;
		if (rights->notBefore == 0 && rights->notAfter == 0 && rights->notMoreThan == 0 &&
		    rights->notLongerThan == 0 && rights->idCount == 0) {
			summary = NULL;
		} else {
			summary = MlnAllocate(MLN_HEAP_SECURE, sizeof(*summary));
			if (summary == NULL) {
				ret = MLN_ERROR_NO_MEMORY;
				goto out;
			}
			summary->state = rights->state;
			summary->notBefore = rights->notBefore;
			summary->notAfter = rights->notAfter;
			summary->notMoreThan = rights->notMoreThan;
			summary->notLongerThan = rights->notLongerThan;
			count = rights->idCount;
			summary->idCount = count;
			if (count != 0) {
				summary->ids = MlnAllocate(MLN_HEAP_SECURE, count * sizeof(*summary->ids));
				if (summary->ids == NULL) {
					ret = MLN_ERROR_NO_MEMORY;
					goto out;
				}
				for (i = 0; i < summary->idCount; i++) {
					summary->ids[i] = rights->ids[i];
					rights->ids[i] = NULL;
				}
			}
		}
	}
	*out = summary;
out:
	MlnImpl_810233C4(&decoded);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	if (session->release != NULL)
		session->release(reply);
	return ret;
free:
	MlnImpl_810233C4(&decoded);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	return ret;
}

/* a request the client answers with its status alone */
__attribute__((noinline)) int MlnImpl_8101F0B8(MlnClient *client, void *a1, void *a2)
{
	MlnServiceSession *session;
	MlnBuffer *request = NULL, *reply = NULL;
	MlnMessageStatus *status;
	MlnMessage decoded;
	int ret;

	MlnMemset(&decoded, 0, sizeof(decoded));
	if (client == NULL || client->kind == NULL || client->kind->type != 1 ||
	    client->kind->remote == NULL) {
		ret = 3;
		goto free;
	}
	ret = MlnImpl_81022C10(a1, a2, &request);
	if (ret != MLN_OK)
		goto free;
	session = client->kind->remote;
	if (session->transact(session->cookie, request, &reply) != 0 || reply == NULL) {
		ret = 83;
	} else {
		ret = MlnImpl_8102371C(reply, &decoded);
		if (ret == MLN_OK) {
			if (decoded.type == MLN_MESSAGE_STATUS) {
				status = decoded.payload;
				ret = MlnImpl_8101DC14(client, status->failed, status->result, NULL);
			} else {
				ret = 84;
			}
		}
	}
	MlnImpl_810233C4(&decoded);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	if (session->release != NULL)
		session->release(reply);
	return ret;
free:
	MlnImpl_810233C4(&decoded);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	return ret;
}

/* a request the client answers with a buffer it found */
__attribute__((noinline)) int MlnImpl_8101F1F0(MlnClient *client, void *a1, MlnBuffer **found)
{
	MlnServiceSession *session;
	MlnBuffer *request = NULL, *reply = NULL;
	MlnMessage decoded;
	int ret;

	MlnMemset(&decoded, 0, sizeof(decoded));
	if (client == NULL || client->kind == NULL || client->kind->type != 1 ||
	    client->kind->remote == NULL) {
		ret = 3;
		goto free;
	}
	ret = MlnImpl_81022D54(a1, &request);
	if (ret != MLN_OK)
		goto free;
	session = client->kind->remote;
	if (session->transact(session->cookie, request, &reply) != 0 || reply == NULL) {
		ret = 83;
	} else {
		ret = MlnImpl_8102371C(reply, &decoded);
		if (ret == MLN_OK) {
			if (decoded.type == MLN_MESSAGE_STATUS) {
				ret = MlnImpl_8101DCB4(client, decoded.payload);
			} else if (decoded.type == MLN_MESSAGE_FOUND) {
				*found = *(MlnBuffer **)decoded.payload;
				*(MlnBuffer **)decoded.payload = NULL;
			} else {
				ret = 84;
			}
		}
	}
	MlnImpl_810233C4(&decoded);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	if (session->release != NULL)
		session->release(reply);
	return ret;
free:
	MlnImpl_810233C4(&decoded);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	return ret;
}

/* the client's state; one that announces a result has it fetched first */
__attribute__((noinline)) int MlnImpl_8101F33C(MlnClient *client, int *out)
{
	MlnServiceSession *session;
	MlnBuffer *request = NULL, *reply = NULL;
	MlnMessage decoded;
	int state, ret;

	MlnMemset(&decoded, 0, sizeof(decoded));
	if (client == NULL || client->kind == NULL || client->kind->type != 1 ||
	    client->kind->remote == NULL) {
		ret = 3;
		goto free;
	}
	ret = MlnImpl_810228FC(&request);
	if (ret != MLN_OK)
		goto free;
	session = client->kind->remote;
	if (session->transact(session->cookie, request, &reply) != 0 || reply == NULL) {
		ret = 83;
		goto out;
	}
	ret = MlnImpl_8102371C(reply, &decoded);
	if (ret != MLN_OK)
		goto out;
	if (decoded.type == MLN_MESSAGE_STATUS) {
		ret = MlnImpl_8101DCB4(client, decoded.payload);
		if (ret != MLN_OK)
			goto out;
		state = 0;
	} else if (decoded.type != MLN_MESSAGE_STATE) {
		ret = 84;
		goto out;
	} else {
		state = *(int *)decoded.payload;
		if ((state & 0x100100) != 0) {
			ret = MlnImpl_8101D5D4(client, NULL);
			if (ret != MLN_OK)
				goto out;
		}
	}
	*out = state;
out:
	MlnImpl_810233C4(&decoded);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	if (session->release != NULL)
		session->release(reply);
	return ret;
free:
	MlnImpl_810233C4(&decoded);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	return ret;
}

/* a request the client answers with a handle */
__attribute__((noinline)) int MlnImpl_8101F4B0(MlnClient *client, void **handle)
{
	MlnServiceSession *session;
	MlnBuffer *request = NULL, *reply = NULL;
	MlnMessage decoded;
	int ret;

	MlnMemset(&decoded, 0, sizeof(decoded));
	if (client == NULL || client->kind == NULL || client->kind->type != 1 ||
	    client->kind->remote == NULL) {
		ret = 3;
		goto free;
	}
	ret = MlnImpl_81022EFC(&request);
	if (ret != MLN_OK)
		goto free;
	session = client->kind->remote;
	if (session->transact(session->cookie, request, &reply) != 0 || reply == NULL) {
		ret = 83;
	} else {
		ret = MlnImpl_8102371C(reply, &decoded);
		if (ret == MLN_OK) {
			if (decoded.type == MLN_MESSAGE_STATUS) {
				ret = MlnImpl_8101DCB4(client, decoded.payload);
			} else if (decoded.type == MLN_MESSAGE_HANDLE) {
				*handle = *(void **)decoded.payload;
				*(void **)decoded.payload = NULL;
			} else {
				ret = 84;
			}
		}
	}
	MlnImpl_810233C4(&decoded);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	if (session->release != NULL)
		session->release(reply);
	return ret;
free:
	MlnImpl_810233C4(&decoded);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	return ret;
}

/* a buffer handed to the client, answered with its status alone */
__attribute__((noinline)) int MlnImpl_8101F5F4(MlnClient *client, MlnBuffer *a1)
{
	MlnServiceSession *session;
	MlnBuffer *request = NULL, *reply = NULL;
	MlnMessageStatus *status;
	MlnMessage decoded;
	int ret;

	MlnMemset(&decoded, 0, sizeof(decoded));
	if (client == NULL || client->kind == NULL || client->kind->type != 1 ||
	    client->kind->remote == NULL) {
		ret = 3;
		goto free;
	}
	ret = MlnImpl_810230AC(a1, &request);
	if (ret != MLN_OK)
		goto free;
	session = client->kind->remote;
	if (session->transact(session->cookie, request, &reply) != 0 || reply == NULL) {
		ret = 83;
	} else {
		ret = MlnImpl_8102371C(reply, &decoded);
		if (ret == MLN_OK) {
			if (decoded.type == MLN_MESSAGE_STATUS) {
				status = decoded.payload;
				ret = MlnImpl_8101DC14(client, status->failed, status->result, NULL);
			} else {
				ret = 84;
			}
		}
	}
	MlnImpl_810233C4(&decoded);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	if (session->release != NULL)
		session->release(reply);
	return ret;
free:
	MlnImpl_810233C4(&decoded);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	return ret;
}

/* an action token handed to the client, answered with a buffer it found in it */
__attribute__((noinline)) int MlnImpl_8101F724(MlnClient *client, MlnBuffer *token,
					       MlnBuffer **found)
{
	MlnServiceSession *session;
	MlnBuffer *request = NULL, *reply = NULL;
	MlnMessage decoded;
	int ret;

	MlnMemset(&decoded, 0, sizeof(decoded));
	if (client == NULL || client->kind == NULL || client->kind->type != 1 ||
	    client->kind->remote == NULL) {
		ret = 3;
		goto free;
	}
	ret = MlnImpl_810231D4(token, &request);
	if (ret != MLN_OK)
		goto free;
	session = client->kind->remote;
	if (session->transact(session->cookie, request, &reply) != 0 || reply == NULL) {
		ret = 83;
	} else {
		ret = MlnImpl_8102371C(reply, &decoded);
		if (ret == MLN_OK) {
			if (decoded.type == MLN_MESSAGE_STATUS) {
				ret = MlnImpl_8101DCB4(client, decoded.payload);
			} else if (decoded.type == MLN_MESSAGE_TOKEN) {
				*found = *(MlnBuffer **)decoded.payload;
				*(MlnBuffer **)decoded.payload = NULL;
			} else {
				ret = 84;
			}
		}
	}
	MlnImpl_810233C4(&decoded);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	if (session->release != NULL)
		session->release(reply);
	return ret;
free:
	MlnImpl_810233C4(&decoded);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	return ret;
}

/* opens the session that serves a remote client */
__attribute__((noinline)) int MlnImpl_8101F870(MlnApiKind *kind, MlnBuffer *address,
					       MlnSession **out)
{
	MlnSession *session;

	if (kind == NULL || kind->type != 1 || kind->remote != NULL || out == NULL)
		return MLN_CORE_ERROR_INVALID;
	session = MlnAllocate(MLN_HEAP_SECURE, sizeof(*session));
	if (session == NULL)
		return MLN_ERROR_NO_MEMORY;
	MlnMemset(session, 0, sizeof(*session));
	session->kind = kind;
	session->mode = 1;
	session->allowed = 1;
	if (address != NULL) {
		session->address = MlnBufferCreate(MLN_HEAP_SECURE, address->size, address->data);
		if (session->address == NULL) {
			MlnBufferDestroy(MLN_HEAP_SECURE, NULL);
			MlnFree(MLN_HEAP_SECURE, session);
			return MLN_ERROR_NO_MEMORY;
		}
	}
	*out = session;
	return MLN_OK;
}

__attribute__((noinline)) void MlnImpl_8101F92C(MlnSession *session)
{
	if (session == NULL)
		return;
	MlnBufferDestroy(MLN_HEAP_SECURE, session->address);
	MlnFree(MLN_HEAP_SECURE, session);
}

/*
 * A client's message as a request for the engine. The client may send a request or a body
 * only while the session allows it; a message refused gives a status reply to send back.
 */
__attribute__((noinline)) int MlnImpl_8101F954(MlnSession *session, MlnBuffer *message, int status,
					       void *data, MlnApiRequest *request)
{
	MlnBuffer *reply = NULL, *answer;
	MlnMessage decoded;
	unsigned int mask;
	void **payload;
	int code, ret;

	MlnMemset(&decoded, 0, sizeof(decoded));
	decoded.type = MLN_MESSAGE_NONE;
	if (message == NULL || request == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		MlnImpl_810233C4(&decoded);
		if (request == NULL)
			goto out;
		code = 128;
		goto refuse;
	}
	ret = MlnImpl_8102371C(message, &decoded);
	if (ret != MLN_OK)
		goto decoded;
	if (session == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto decoded;
	}
	switch (decoded.type) {
	case MLN_MESSAGE_HTTP_GET:
		mask = 2;
		goto gated;
	case MLN_MESSAGE_HTTP_POST:
		mask = 4;
		goto gated;
	case MLN_MESSAGE_HTTP_BODY:
		mask = 8;
		goto gated;
	case 0x603:
		mask = 32;
gated:
		if (session->allowed == 1 || (session->allowed & mask) != mask) {
			ret = 84;
			goto decoded;
		}
		break;
	case 0x400:
	case 0x401:
	case 0x500:
	case 0x502:
	case 0x504:
	case 0x506:
	case 0x601:
	case 0x10a0:
	case 0x10a2:
	case 0x10a4:
	case 0x10a6:
	case 0x10ae:
	case 0x10b0:
		break;
	default:
		ret = 84;
		goto decoded;
	}
	payload = decoded.payload;
	switch (decoded.type) {
	case 0x400:
		if (payload == NULL) {
			ret = 7;
			break;
		}
		if (*(unsigned short *)payload <= 1) {
			request->type = 0;
			request->arg[0] = payload[1];
			request->arg[1] = payload[2];
			request->arg[2] = payload[3];
			payload[1] = NULL;
			payload[2] = NULL;
			payload[3] = NULL;
			break;
		}
		if (*(unsigned short *)payload != 2) {
			ret = 84;
			break;
		}
		goto busy;
	case 0x401:
		if (payload == NULL) {
			ret = 7;
			break;
		}
		if (*(unsigned short *)payload <= 1)
			goto busy;
		if (*(unsigned short *)payload == 2)
			request->type = 1;
		else
			ret = 84;
		break;
busy:
		ret = MlnImpl_81022C98(17, &reply);
		if (ret != MLN_OK)
			break;
		request->type = 5;
		request->arg[0] = reply;
		reply = NULL;
		break;
	case MLN_MESSAGE_HTTP_GET:
		if (payload == NULL) {
			ret = 7;
			break;
		}
		request->type = 2;
		request->arg[0] = payload[0];
		request->arg[1] = NULL;
		request->arg[2] = NULL;
		payload[0] = NULL;
		break;
	case MLN_MESSAGE_HTTP_POST:
		if (payload != NULL) {
			request->type = 2;
			request->arg[0] = payload[1];
			request->arg[1] = payload[2];
			request->arg[2] = payload[3];
			payload[1] = NULL;
			payload[2] = NULL;
			payload[3] = NULL;
		}
		break;
	case MLN_MESSAGE_HTTP_BODY:
		if (payload == NULL) {
			ret = 7;
			break;
		}
		request->type = 2;
		request->arg[0] = NULL;
		request->arg[2] = NULL;
		request->arg[1] = payload[0];
		payload[0] = NULL;
		break;
	case 0x500:
		if (payload == NULL) {
			ret = 7;
			break;
		}
		request->type = 4;
		request->arg[0] = payload[0];
		payload[0] = NULL;
		break;
	case 0x502:
		request->type = 8;
		break;
	case 0x504:
		if (payload == NULL) {
			ret = 7;
			break;
		}
		request->type = 7;
		request->arg[0] = payload[0];
		request->arg[1] = payload[1];
		payload[0] = NULL;
		payload[1] = NULL;
		break;
	case 0x506:
		if (payload == NULL) {
			ret = 7;
			break;
		}
		request->type = 11;
		request->arg[0] = payload[0];
		request->arg[1] = payload[1];
		payload[1] = NULL;
		payload[0] = NULL;
		break;
	case 0x601:
		request->type = 15;
		request->arg[0] = payload[0];
		payload[0] = NULL;
		break;
	case 0x603:
		/* the result the client fetches: a value, the status alone, or nothing yet */
		if (payload != NULL) {
			ret = 7;
			break;
		}
		if (data != NULL)
			code = MLN_RESULT_VALUE;
		else if (status != MLN_OK)
			code = MLN_RESULT_NONE;
		else
			code = MLN_RESULT_BUSY;
		ret = MlnImpl_81022B3C(code, status, data, &reply);
		if (ret != MLN_OK)
			break;
		request->type = 5;
		request->arg[0] = reply;
		reply = NULL;
		MlnBufferDestroy(MLN_HEAP_SECURE, NULL);
		session->allowed = 1;
		MlnImpl_810233C4(&decoded);
		goto out;
	case 0x10a0:
		if (payload == NULL) {
			ret = 7;
			break;
		}
		request->type = 12;
		/* two halves of the first argument */
		((unsigned short *)request->arg)[0] = ((unsigned short *)payload)[0];
		((unsigned short *)request->arg)[1] = ((unsigned short *)payload)[1];
		break;
	case 0x10a2:
		if (payload == NULL) {
			ret = 7;
			break;
		}
		request->type = 13;
		request->arg[0] = payload[0];
		payload[0] = NULL;
		break;
	case 0x10a4:
		if (payload == NULL) {
			ret = 7;
			break;
		}
		request->type = 6;
		request->arg[0] = payload[0];
		request->arg[1] = payload[1];
		payload[0] = NULL;
		payload[1] = NULL;
		break;
	case 0x10a6:
		request->type = 9;
		break;
	case 0x10ae:
		if (payload == NULL) {
			ret = 7;
			break;
		}
		request->type = 10;
		request->arg[0] = payload[0];
		break;
	case 0x10b0:
		if (payload == NULL) {
			ret = 7;
			break;
		}
		request->type = 14;
		request->arg[0] = payload[0];
		payload[0] = NULL;
		break;
	}
	MlnBufferDestroy(MLN_HEAP_SECURE, reply);
	reply = NULL;
decoded:
	MlnImpl_810233C4(&decoded);
	if (ret == MLN_OK || ret == MLN_ERROR_NO_MEMORY || ret == 84)
		goto out;
	code = ret == 6 ? 18 : 128;
refuse:
	if (session != NULL) {
		session->allowed = 32;
		ret = MlnImpl_81022C98(code, &reply);
		if (ret != MLN_OK) {
			answer = NULL;
		} else {
			answer = reply;
			reply = NULL;
		}
	} else {
		ret = MLN_CORE_ERROR_INVALID;
		answer = NULL;
	}
	MlnBufferDestroy(MLN_HEAP_SECURE, reply);
	request->type = 5;
	request->arg[0] = answer;
	reply = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_SECURE, reply);
	return ret;
}

/* the name a session gives itself */
#define MLN_SESSION_NAME	"sony.net:1.0;"

/* a status reply */
static inline __attribute__((always_inline)) int MlnSessionStatus(int code, MlnBuffer **message)
{
	MlnBuffer *reply = NULL;
	int ret;

	ret = MlnImpl_81022C98(code, &reply);
	if (ret == MLN_OK) {
		*message = reply;
		reply = NULL;
	}
	MlnBufferDestroy(MLN_HEAP_SECURE, reply);
	return ret;
}

/*
 * The engine's answer to a request, as the message to send the client: the request to send
 * for it, a value, a report, or the status alone.
 */
__attribute__((noinline)) int MlnImpl_81020094(MlnSession *session, int status, MlnApiValue *value,
					       MlnHttpRequest *request, MlnApiReport *report,
					       MlnBuffer **response)
{
	MlnBuffer name = { sizeof(MLN_SESSION_NAME) - 1, (unsigned char *)MLN_SESSION_NAME };
	MlnBuffer *message = NULL;
	int ret;

	if (session == NULL) {
		ret = 3;
		goto out;
	}
	if (response == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	session->allowed = 1;
	if (status != MLN_OK) {
		session->allowed = 32;
		ret = MlnSessionStatus(128, &message);
		if (ret != MLN_OK)
			goto out;
	} else if (request != NULL) {
		if (request->method == MLN_HTTP_GET)
			ret = MlnImpl_81021864(request->url, &message);
		else
			ret = MlnImpl_8102190C(status, request->url, request->body, &request->headers,
					       &message);
		if (ret != MLN_OK)
			goto out;
		session->allowed = 8;
	} else if (value != NULL) {
		switch (value->type) {
		case 4:
			if (session->mode == 1) {
				ret = MlnImpl_81021CD4(MLN_HEAP_SECURE, value->data[2], &message,
						       value->type - 1);
				if (ret != MLN_OK)
					goto out;
				session->allowed = 8;
				break;
			}
			/* fall through */
		case 1:
		case 2:
		case 3:
			ret = MlnSessionStatus(0, &message);
			if (ret != MLN_OK)
				goto out;
			break;
		case 6:
			ret = MlnImpl_810223C8(MLN_HEAP_SECURE, 0, 0xe000, session->address, &name,
					       &message);
			if (ret != MLN_OK)
				goto out;
			break;
		case 16:
			session->allowed = 32;
			ret = MlnSessionStatus(0, &message);
			if (ret != MLN_OK)
				goto out;
			break;
		default:
			ret = 7;
			goto out;
		}
	} else if (report != NULL) {
		switch (report->type) {
		case 0:
			ret = MlnImpl_81021BAC(report->data.buffer, &message);
			if (ret != MLN_OK)
				goto out;
			session->allowed = 6;
			break;
		case 1:
			ret = MlnImpl_81022500(&report->data, &message);
			break;
		case 2:
			ret = MlnImpl_81021F74(&report->data, &message);
			break;
		case 3:
			ret = MlnImpl_81021DE8(&report->data, &message);
			break;
		case 4:
			ret = MlnImpl_8102285C(&report->data, &message);
			break;
		case 5:
			ret = MlnImpl_81022E7C(&report->data, &message);
			break;
		case 6:
			ret = MlnImpl_81022608(&report->data, &message);
			break;
		case 7:
			ret = MlnImpl_8102302C(report->data.buffer, &message);
			break;
		case 9:
			ret = MlnImpl_810232FC(&report->data, &message);
			break;
		default:
			ret = 7;
			goto out;
		}
		if (ret != MLN_OK)
			goto out;
	} else {
		ret = MlnSessionStatus(0, &message);
		if (ret != MLN_OK)
			goto out;
	}
	*response = message;
	message = NULL;
	ret = MLN_OK;
out:
	MlnBufferDestroy(MLN_HEAP_SECURE, message);
	MlnBufferDestroy(MLN_HEAP_SECURE, NULL);
	return ret;
}

__attribute__((noinline)) int MlnImpl_8102045C(MlnSession *session, int mode)
{
	if (session == NULL)
		return 3;
	session->mode = mode;
	return MLN_OK;
}
