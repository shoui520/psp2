/*
 * The engine: runs the actions of a Marlin Broadband action token with the service, one
 * step at a time. Each step either gives the caller a request to send or a value; the
 * response to a request feeds the next step. The file's name is ours.
 */
#include "mln_internal.h"

/* the extension every engine sends: the client's certification by Sony */
#define MLN_CLIENT_CERTIFICATION	"urn:marlin:organization:sony:connect:client-certification"

static inline __attribute__((always_inline)) void MlnBufferPairListFree(MlnBufferPairList *list)
{
	int i;

	for (i = 0; i < list->count; i++)
		MlnBufferPairFree(list->items[i]);
	MlnFree(MLN_HEAP_SECURE, list->items);
	MlnFree(MLN_HEAP_SECURE, list);
}

__attribute__((noinline)) void MlnEngineFree(MlnEngine *engine)
{
	unsigned int i;

	if (engine == NULL)
		return;
#ifndef MLN_PLAYBACK
	if (engine->kind == 1)
		MlnImpl_81018220(engine->context);
	else
#endif
	if (engine->kind == 0 || engine->kind == 2)
		MlnImpl_81015168(engine->context);
	MlnBufferDestroy(MLN_HEAP_SECURE, engine->account);
	MlnBufferDestroy(MLN_HEAP_SECURE, engine->secret);
	MlnActionTokenFree(engine->token);
	MlnServiceConfigFree(engine->config);
	if (engine->extensions != NULL)
		MlnBufferPairListFree(engine->extensions);
	if (engine->resources != NULL) {
		for (i = 0; i < engine->resourceCount; i++)
			MlnFree(MLN_HEAP_SECURE, engine->resources[i]);
		MlnFree(MLN_HEAP_SECURE, engine->resources);
	}
	MlnFree(MLN_HEAP_SECURE, engine);
}

/* an engine without a token: its resources are the files in the library's area */
__attribute__((noinline)) int MlnImpl_810199E4(MlnEngine *engine)
{
	MlnFileList *files = NULL, *file;
	unsigned int size = MLN_PATH_MAX, count, i;
	char path[MLN_PATH_MAX];
	char **paths;
	int ret;

	if (MlnImpl_810262B4(MLN_FILE_AREA, &size, path) != MLN_OK) {
		ret = MLN_ERROR_INVALID;
		goto out;
	}
	ret = MlnImpl_810265E8(MLN_HEAP_SECURE, path, &files);
	if (ret != MLN_OK) {
		if (ret != MLN_ERROR_NO_MEMORY)
			ret = MLN_ERROR_INVALID;
		goto out;
	}
	if (files == NULL) {
		ret = 88;
		goto out;
	}
	count = 0;
	for (file = files; file != NULL; file = file->next) {
		count++;
		if (file->path == NULL) {
			ret = 7;
			goto out;
		}
	}
	if (count != 0) {
		paths = MlnAllocate(MLN_HEAP_SECURE, count * sizeof(*paths));
		if (paths == NULL) {
			ret = MLN_ERROR_NO_MEMORY;
			goto out;
		}
		MlnMemset(paths, 0, count * sizeof(*paths));
		file = files;
		for (i = 0; i != count; i++) {
			paths[i] = MlnStrdup(MLN_HEAP_SECURE, file->path);
			if (paths[i] == NULL) {
				for (i = 0; i != count; i++)
					MlnFree(MLN_HEAP_SECURE, paths[i]);
				MlnFree(MLN_HEAP_SECURE, paths);
				ret = MLN_ERROR_NO_MEMORY;
				goto out;
			}
			file = file->next;
		}
	} else {
		paths = NULL;
	}
	engine->resourceCount = count;
	engine->resources = paths;
out:
	MlnImpl_810257DC(MLN_HEAP_SECURE, files);
	return ret;
}

/* a new engine with copies of the account and its secret, on the caller's context or its own */
static inline __attribute__((always_inline)) int MlnEngineInit(int kind, const MlnBuffer *account,
							       const MlnBuffer *secret,
							       MlnServiceContext *context,
							       MlnEngine **out)
{
	MlnEngine *engine;
	MlnBuffer *accountCopy = NULL, *secretCopy = NULL;
	int ret = MLN_ERROR_NO_MEMORY;

	engine = MlnAllocate(MLN_HEAP_SECURE, sizeof(*engine));
	if (engine == NULL)
		goto out;
	MlnMemset(engine, 0, sizeof(*engine));
	/* Sony's clears it twice */
	MlnMemset(engine, 0, sizeof(*engine));
	if (context == NULL) {
		context = &engine->localContext;
		engine->localContext.id = -1;
		engine->localContext.session = &engine->session;
	}
	if (account != NULL) {
		accountCopy = MlnBufferCreate(MLN_HEAP_SECURE, account->size, account->data);
		if (accountCopy == NULL)
			goto out;
	}
	if (secret != NULL) {
		secretCopy = MlnBufferCreate(MLN_HEAP_SECURE, secret->size, secret->data);
		if (secretCopy == NULL)
			goto out;
	}
	engine->secret = secretCopy;
	secretCopy = NULL;
	engine->account = accountCopy;
	accountCopy = NULL;
	engine->context = context;
	engine->kind = kind;
	engine->state = 0;
	engine->progress.done = 0;
	*out = engine;
	engine = NULL;
	ret = MLN_OK;
out:
	MlnBufferDestroy(MLN_HEAP_SECURE, accountCopy);
	MlnBufferDestroy(MLN_HEAP_SECURE, secretCopy);
	MlnEngineFree(engine);
	return ret;
}

/*
 * Creates an engine for an action token. A token for a service whose configuration was kept
 * resumes with it, unless the service or its configuration's version changed.
 */
__attribute__((noinline)) int MlnEngineCreate(int kind, const MlnBuffer *token,
					      const MlnBuffer *account, const MlnBuffer *secret,
					      MlnServiceContext *context, MlnEngine **out)
{
	MlnEngine *engine = NULL;
	MlnActionToken *parsed = NULL;
	MlnServiceConfig *config;
	MlnBufferPairList *extensions;
	MlnBuffer *stored, *serviceId;
	unsigned int i;
	int exists;
	int ret;

	if (out == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	*out = NULL;
	ret = MlnEngineInit(kind, account, secret, context, &engine);
	if (ret != MLN_OK) {
		engine = NULL;
		goto out;
	}
	extensions = MlnAllocate(MLN_HEAP_SECURE, sizeof(*extensions));
	if (extensions == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	MlnMemset(extensions, 0, sizeof(*extensions));
	extensions->count = 0;
	extensions->items = MlnAllocate(MLN_HEAP_SECURE, sizeof(*extensions->items));
	if (extensions->items == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto fail;
	}
	MlnMemset(extensions->items, 0, sizeof(*extensions->items));
	ret = MlnBufferPairCreate(MlnStrlen(MLN_CLIENT_CERTIFICATION), MLN_CLIENT_CERTIFICATION, 0,
				  NULL, 1, extensions->items);
	if (ret != MLN_OK)
		goto fail;
	extensions->count = 1;
	engine->extensions = extensions;

	if (token == NULL) {
		if (kind != 1) {
			ret = MlnImpl_810199E4(engine);
			if (ret != MLN_OK)
				goto out;
		}
		engine->state = 2;
		goto done;
	}
	ret = MlnActionTokenParse(token->size, token->data, &parsed);
	if (ret != MLN_OK)
		goto out;
	if (parsed->actions == NULL) {
		ret = 7;
		goto out;
	}
	config = NULL;
	stored = NULL;
	engine->resourceCount = parsed->resourceLocations.count;
	serviceId = parsed->broadbandServiceId;
	if (parsed->actions[0]->type == MLN_ACTION_GENERIC) {
		engine->kind = 1;
	} else {
		ret = MlnImpl_8101D468(serviceId, &exists);
		if (ret != MLN_OK)
			goto free;
		if (exists != 0 && MlnImpl_8101D408(serviceId, &stored) == MLN_OK) {
			ret = MlnServiceConfigParse(stored->size, stored->data, &config);
			if (ret == MLN_ENGINE_ERROR_FORMAT) {
				MlnImpl_8101D520(serviceId);
				goto free;
			}
			if (ret != MLN_OK)
				goto free;
			if (MlnBufferCompare(MLN_HEAP_SECURE, config->broadbandServiceId,
					     parsed->broadbandServiceId) != 0 ||
			    MlnBufferCompare(MLN_HEAP_SECURE, config->configVersion,
					     parsed->configVersion) != 0) {
				MlnImpl_8101D520(serviceId);
			} else {
				engine->config = config;
				config = NULL;
			}
		}
	}
	MlnBufferDestroy(MLN_HEAP_SECURE, stored);
	MlnServiceConfigFree(config);
	if (engine->config == NULL) {
		engine->state = 2;
	} else {
		ret = MlnImpl_8101ACD0(engine, parsed, NULL);
		if (ret != MLN_OK)
			goto out;
	}
	/* a link acquisition and a deregistration take two steps */
	engine->progress.total = parsed->actionCount;
	for (i = 0; i < (unsigned int)parsed->actionCount; i++) {
		if ((unsigned int)(parsed->actions[i]->type - MLN_ACTION_LINK_ACQUISITION) <= 1)
			engine->progress.total++;
	}
done:
	if (engine->kind != 0 || secret == NULL || secret->size == 0 || secret->data == NULL ||
	    (account != NULL && account->size != 0 && account->data != NULL)) {
		engine->token = parsed;
		parsed = NULL;
		*out = engine;
		engine = NULL;
	} else {
		ret = MLN_CORE_ERROR_INVALID;
	}
out:
	MlnEngineFree(engine);
	MlnActionTokenFree(parsed);
	return ret;
free:
	MlnBufferDestroy(MLN_HEAP_SECURE, stored);
	MlnServiceConfigFree(config);
	goto out;
fail:
	MlnBufferPairListFree(extensions);
	goto out;
}

__attribute__((noinline)) int MlnEngineDestroy(MlnEngine *engine)
{
	MlnEngineFree(engine);
	return MLN_OK;
}

/* a new, empty step */
static inline __attribute__((always_inline)) int MlnApiResultCreate(MlnApiResult **out)
{
	MlnApiResult *result = MlnAllocate(MLN_HEAP_SECURE, sizeof(*result));
	int ret = MLN_ERROR_NO_MEMORY;

	if (result != NULL) {
		MlnMemset(result, 0, sizeof(*result));
		*out = result;
		result = NULL;
		ret = MLN_OK;
	}
	MlnApiResultFree(result);
	return ret;
}

/*
 * Runs the engine until it gives a request to send or a value the caller must see; the
 * message is the response to the last request. The engine's state names its next step.
 */
__attribute__((noinline)) int MlnEngineStep(MlnEngine *engine, MlnBuffer **message,
					    MlnApiResult **out)
{
	MlnApiResult *result = NULL;
	int hasValue;
	int ret;

	if (engine == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
		if (out != NULL)
			*out = NULL;
		return ret;
	}
	if (out == NULL)
		return MLN_CORE_ERROR_INVALID;
	ret = MlnApiResultCreate(&result);
	if (ret != MLN_OK)
		goto out;
	while (result->request == NULL) {
		if (engine->state == 1)
			break;
		hasValue = 0;
		if (result->value != NULL) {
			if (result->value->type == 4 || result->value->type == 16)
				break;
			hasValue = 1;
		}
		switch (engine->state) {
		case 2:
			ret = MlnImpl_8101AD74(engine, result, message);
			break;
		case 3:
			ret = MlnImpl_8101AF30(engine, message, result, hasValue);
			break;
		case 4:
			ret = MlnImpl_8101B158(engine, result);
			break;
		case 5:
			ret = MlnImpl_8101B244(engine, message, result, hasValue);
			break;
		case 10:
			ret = MlnImpl_8101CA44(engine, result);
			break;
		case 11:
			ret = MlnImpl_8101B324(engine, message, result, hasValue);
			break;
		case 12:
			ret = MlnImpl_8101C880(engine, result);
			break;
		case 13:
			ret = MlnImpl_8101B444(engine, message, result, hasValue);
			break;
		case 14:
			ret = MlnImpl_8101B7D8(engine, result);
			break;
		case 15:
			ret = MlnImpl_8101C6B4(engine, result);
			break;
		case 16:
			ret = MlnImpl_8101B850(engine, message, result, hasValue);
			break;
		case 17:
			ret = MlnImpl_8101BBE4(engine, result);
			break;
		case 18:
			ret = MlnImpl_8101C500(engine, result);
			break;
		case 19:
			ret = MlnImpl_8101BC60(engine, message, result, hasValue);
			break;
#ifndef MLN_PLAYBACK
		case 20:
			ret = MlnImpl_8101BD9C(engine, message, result, hasValue);
			break;
		case 21:
			ret = MlnImpl_8101CC94(engine, result);
			break;
		case 22:
			ret = MlnImpl_8101BE94(engine, message, result, hasValue);
			break;
		case 23:
			ret = MlnImpl_8101BFEC(engine, message, result, hasValue);
			break;
		case 24:
			ret = MlnImpl_8101C0F4(engine, message, result, hasValue);
			break;
#endif
		default:
			ret = 7;
			break;
		}
		if (ret != MLN_OK)
			break;
	}
out:
	*out = result;
	if (ret == 41)
		return MLN_OK;
	return ret;
}

static inline __attribute__((always_inline)) void MlnBufferArrayFree(MlnBuffer **items,
								     unsigned int count)
{
	unsigned int i;

	if (items == NULL)
		return;
	for (i = 0; i < count; i++)
		MlnBufferDestroy(MLN_HEAP_SECURE, items[i]);
	MlnFree(MLN_HEAP_SECURE, items);
}

/*
 * The first step of a remote engine: the request that opens it on the service, its body
 * the message the service's open reply asks for.
 */
#ifndef MLN_PLAYBACK
__attribute__((noinline)) int MlnImpl_8101A360(MlnEngine *engine, MlnApiResult **out)
{
	MlnApiResult *result = NULL;
	MlnBuffer **first = NULL, **second = NULL;
	MlnBuffer *third = NULL, *fourth = NULL;
	unsigned int firstCount = 0, secondCount = 0;
	MlnHttpRequest *request;
	MlnBuffer *body;
	char *text;
	int ret;

	if (engine == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
		goto free;
	}
	if (out == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto free;
	}
	ret = MlnApiResultCreate(&result);
	if (ret != MLN_OK)
		goto out;
	ret = MlnImpl_8101CBF0(NULL, 2, NULL, &result->request);
	if (ret != MLN_OK)
		goto out;
	ret = MlnImpl_81017880(engine->context, engine->extensions, &firstCount, &first,
			       &secondCount, &second, &third, &fourth);
	if (ret != MLN_OK)
		goto out;
	text = NULL;
	engine->state = 20;
	request = result->request;
	ret = MlnImpl_81035018(firstCount, first, secondCount, second, third, fourth, &text);
	if (ret == MLN_OK) {
		body = MlnBufferCreate(MLN_HEAP_SECURE, MlnStrlen(text), text);
		if (body != NULL) {
			request->body = body;
			MlnFree(MLN_HEAP_SECURE, text);
			engine->state = 20;
			*out = result;
			result = NULL;
			goto out;
		}
		ret = MLN_ERROR_NO_MEMORY;
	}
	MlnFree(MLN_HEAP_SECURE, text);
out:
	MlnBufferArrayFree(first, firstCount);
free:
	MlnBufferArrayFree(second, secondCount);
	MlnBufferDestroy(MLN_HEAP_SECURE, third);
	MlnBufferDestroy(MLN_HEAP_SECURE, fourth);
	MlnApiResultFree(result);
	return ret;
}
#endif

/* the action token of a license transfer (LTP) */
#ifndef MLN_PLAYBACK
__attribute__((noinline)) int MlnImpl_8101A59C(MlnBuffer **out)
{
	MlnBuffer *buffer = NULL;
	char *text = NULL;
	int ret;

	if (out == NULL) {
		ret = 7;
		goto out;
	}
	ret = MlnImpl_810353A8(&text);
	if (ret == MLN_OK) {
		buffer = MlnBufferCreate(MLN_HEAP_SECURE, MlnStrlen(text), text);
		if (buffer == NULL) {
			ret = MLN_ERROR_NO_MEMORY;
		} else {
			*out = buffer;
			buffer = NULL;
		}
	}
out:
	MlnFree(MLN_HEAP_SECURE, text);
	MlnBufferDestroy(MLN_HEAP_SECURE, buffer);
	return ret;
}
#endif

/* the data command 0x405 asks the engine for: none, and the engine moves on to state 4 */
__attribute__((noinline)) int MlnImpl_8101A620(void *cookie, void *arg, void **data,
					       unsigned int *size)
{
	MlnEngine *engine = cookie;

	if (engine != NULL)
		engine->state = 4;
	return 1;
}

__attribute__((noinline)) int MlnImpl_8101A634(void *cookie, void *data)
{
	return MLN_OK;
}

#define MLN_SOAP_ENVELOPE	"http://schemas.xmlsoap.org/soap/envelope/"
#define MLN_WS_ADDRESSING	"http://www.w3.org/2005/08/addressing"

/* a SOAP message's action (its WS-Addressing Action header), quoted */
static inline __attribute__((always_inline)) int MlnSoapAction(const MlnBuffer *body, char **out)
{
	MlnXmlNode *document = NULL;
	MlnXmlNodeList *header = NULL, *action = NULL;
	MlnBuffer *text = NULL;
	char *quoted = NULL;
	int ret;

	if (MlnXmlParse(MLN_HEAP_SECURE, body->size, body->data, &document) != MLN_OK ||
	    MlnXmlFindNodes(MLN_HEAP_SECURE, document, "Envelope/Header", MLN_SOAP_ENVELOPE,
			    &header) != MLN_OK ||
	    MlnXmlFindNodes(MLN_HEAP_SECURE, header->nodes[0], "Action", MLN_WS_ADDRESSING,
			    &action) != MLN_OK ||
	    (ret = MlnXmlGetText(MLN_HEAP_SECURE, action->nodes[0], &text)) != MLN_OK) {
		ret = 7;
	} else {
		quoted = MlnAllocate(MLN_HEAP_SECURE, text->size + 3);
		if (quoted != NULL) {
			quoted[0] = '"';
			quoted[text->size + 1] = '"';
			quoted[text->size + 2] = '\0';
			MlnMemcpy(quoted + 1, text->data, text->size);
			*out = quoted;
			quoted = NULL;
		} else {
			ret = MLN_ERROR_NO_MEMORY;
		}
	}
	MlnFree(MLN_HEAP_SECURE, quoted);
	MlnBufferDestroy(MLN_HEAP_SECURE, text);
	MlnXmlListFree(MLN_HEAP_SECURE, action);
	MlnXmlListFree(MLN_HEAP_SECURE, header);
	MlnXmlNodeFree(MLN_HEAP_SECURE, document);
	return ret;
}

/*
 * The headers of a request with a SOAP body: its SOAPAction and Content-Type. A body whose
 * action cannot be read still gets the headers, with an empty action, and the error.
 */
__attribute__((noinline)) int MlnImpl_8101A63C(const MlnBuffer *body, MlnHttpHeaders *headers)
{
	MlnHttpHeader **items;
	MlnHttpHeader *entry;
	char *soapAction = NULL;
	char *name, *value;
	int ret;

	if (headers == NULL)
		return 7;
	if (body == NULL)
		return MLN_OK;
	items = MlnAllocate(MLN_HEAP_SECURE, 2 * sizeof(*items));
	if (items == NULL)
		return MLN_ERROR_NO_MEMORY;
	MlnMemset(items, 0, 2 * sizeof(*items));
	entry = MlnAllocate(MLN_HEAP_SECURE, sizeof(*entry));
	if (entry == NULL)
		goto fail;
	entry->name = NULL;
	entry->value = NULL;
	ret = MlnSoapAction(body, &soapAction);
	if (ret == MLN_ERROR_NO_MEMORY)
		goto free;
	if (soapAction != NULL) {
		entry->name = MlnStrdup(MLN_HEAP_SECURE, "SOAPAction");
		entry->value = soapAction;
		if (entry->name == NULL)
			goto free;
	} else {
		value = MlnStrdup(MLN_HEAP_SECURE, "");
		name = MlnStrdup(MLN_HEAP_SECURE, "SOAPAction");
		entry->name = name;
		entry->value = value;
		if (name == NULL || value == NULL)
			goto free;
	}
	items[0] = entry;
	entry = MlnAllocate(MLN_HEAP_SECURE, sizeof(*entry));
	if (entry == NULL)
		goto fail;
	entry->name = MlnStrdup(MLN_HEAP_SECURE, "Content-Type");
	entry->value = MlnStrdup(MLN_HEAP_SECURE, "text/xml; charset=\"utf-8\"");
	if (entry->name == NULL || entry->value == NULL)
		goto free;
	items[1] = entry;
	headers->count = 2;
	headers->items = items;
	return ret;
free:
	MlnFree(MLN_HEAP_SECURE, entry->name);
	MlnFree(MLN_HEAP_SECURE, entry->value);
	MlnFree(MLN_HEAP_SECURE, entry);
fail:
	MlnHttpHeaderArrayFree(items, 2);
	return MLN_ERROR_NO_MEMORY;
}

/* a value for the caller, with copies of the engine's account and secret */
__attribute__((noinline)) int MlnImpl_8101A960(MlnEngine *engine, int type, void *first,
					       void *second, MlnApiValue **out)
{
	MlnBuffer *account = NULL, *secret = NULL;
	MlnApiValue *value;
	int ret;

	if (engine == NULL || out == NULL) {
		ret = 7;
		goto out;
	}
	if (engine->account != NULL) {
		account = MlnBufferCreate(MLN_HEAP_SECURE, engine->account->size,
					  engine->account->data);
		if (account == NULL) {
			ret = MLN_ERROR_NO_MEMORY;
			goto out;
		}
	}
	if (engine->secret != NULL) {
		secret = MlnBufferCreate(MLN_HEAP_SECURE, engine->secret->size,
					 engine->secret->data);
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
	switch (type) {
	case 1:
	case 4:
		value->data[2] = first;
		break;
	case 2:
	case 3:
		value->data[2] = first;
		value->data[3] = second;
		break;
	}
	value->data[1] = secret;
	secret = NULL;
	value->data[0] = account;
	account = NULL;
	value->type = type;
	*out = value;
	ret = MLN_OK;
out:
	MlnBufferDestroy(MLN_HEAP_SECURE, account);
	MlnBufferDestroy(MLN_HEAP_SECURE, secret);
	return ret;
}

/* the value that ends a token: the service's buffers (command 0x426), the account, the secret */
__attribute__((noinline)) int MlnImpl_8101AA80(MlnEngine *engine, MlnApiValue **out)
{
	MlnBufferGroup *group = NULL;
	MlnApiValue *value = NULL;
	int ret = MLN_OK;

	if (out == NULL || engine == NULL)
		goto out;
	ret = MlnImpl_81015CD8(engine->context, &group);
	if (ret != MLN_OK)
		goto out;
	value = MlnAllocate(MLN_HEAP_SECURE, sizeof(*value));
	if (value == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	MlnMemset(value, 0, sizeof(*value));
	value->type = 16;
	if ((engine->account != NULL &&
	     (value->data[0] = MlnBufferCreate(MLN_HEAP_SECURE, engine->account->size,
					       engine->account->data)) == NULL) ||
	    (engine->secret != NULL &&
	     (value->data[1] = MlnBufferCreate(MLN_HEAP_SECURE, engine->secret->size,
					       engine->secret->data)) == NULL)) {
		MlnImpl_81006D14(MLN_HEAP_SECURE, group);
		ret = MLN_ERROR_NO_MEMORY;
		MlnBufferDestroy(MLN_HEAP_SECURE, value->data[0]);
		MlnBufferDestroy(MLN_HEAP_SECURE, value->data[1]);
		/* Sony's frees the copies a second time with the value */
		goto free;
	}
	value->data[2] = group;
	group = NULL;
	*out = value;
	value = NULL;
out:
	MlnImpl_81006D14(MLN_HEAP_SECURE, group);
free:
	MlnApiValueFree(value);
	return ret;
}

/* the data a command takes from a buffer */
__attribute__((noinline)) int MlnImpl_8101ABA8(void *cookie, void *arg, void **data,
					       unsigned int *size)
{
	MlnBuffer *buffer = cookie;

	if (size == NULL || buffer == NULL || data == NULL)
		return 1;
	*size = buffer->size;
	*data = buffer->data;
	return MLN_OK;
}

/* the state that starts the token's next action; none left ends the engine */
__attribute__((noinline)) int MlnImpl_8101ABE0(MlnEngine *engine, MlnActionToken *token)
{
	MlnAction *action;

	if (token->actions == NULL)
		return 7;
	if (engine->action >= (unsigned int)token->actionCount) {
		engine->state = 1;
		engine->progress = (MlnEngineProgress){ 0, 0 };
		return MLN_OK;
	}
	action = token->actions[engine->action];
	if (action == NULL)
		return MLN_ENGINE_ERROR_FORMAT;
	switch (engine->state) {
	case 0:
	case 1:
	case 3:
	case 5:
	case 11:
	case 14:
	case 17:
	case 19:
		break;
	default:
		return 7;
	}
	switch (action->type) {
	case MLN_ACTION_LICENSE_ACQUISITION:
		engine->state = 18;
		return MLN_OK;
	case MLN_ACTION_NODE_ACQUISITION:
		engine->state = 10;
		return MLN_OK;
	case MLN_ACTION_LINK_ACQUISITION:
		engine->state = 12;
		return MLN_OK;
	case MLN_ACTION_DEREGISTRATION:
		engine->state = 15;
		return MLN_OK;
	case MLN_ACTION_GENERIC:
		engine->state = 21;
		return MLN_OK;
	default:
		return MLN_ENGINE_ERROR_FORMAT;
	}
}

/*
 * Starts the token's actions once the service is configured: asks the service whether it
 * wants data first (command 0x405; the callback moves the engine to state 4), and reports
 * the progress on the first step.
 */
static inline __attribute__((always_inline)) int MlnEngineStartActions(MlnEngine *engine,
								       MlnActionToken *token,
								       MlnApiResult *result)
{
	if (engine->state != 5) {
		MlnImpl_81014A10(engine->context, MlnImpl_8101A620, MlnImpl_8101A634, NULL, engine);
		if (engine->state == 4)
			return MLN_OK;
	}
	if (engine->progress.done == 0 && result != NULL) {
		result->progress = 1;
		result->progressDone = engine->progress.done;
		result->progressTotal = engine->progress.total;
	}
	if (token == NULL)
		return 7;
	return MlnImpl_8101ABE0(engine, token);
}

__attribute__((noinline)) int MlnImpl_8101ACD0(MlnEngine *engine, MlnActionToken *token,
					       MlnApiResult *result)
{
	if (engine == NULL)
		return 7;
	return MlnEngineStartActions(engine, token, result);
}

/* state 2: the request for the service's configuration, from the next resource */
__attribute__((noinline)) int MlnImpl_8101AD74(MlnEngine *engine, MlnApiResult *result,
					       MlnBuffer **message)
{
	MlnHttpRequest *request = NULL;
	MlnBuffer **locations;
	char *url = NULL;
	int ret;

	if (engine == NULL || engine->context == NULL || result == NULL) {
		ret = 7;
		goto out;
	}
	if (engine->token == NULL) {
		if (message == NULL || *message != NULL ||
		    engine->resourceIndex >= engine->resourceCount) {
			ret = 7;
			goto out;
		}
		*message = MlnBufferCreate(MLN_HEAP_SECURE,
					   MlnStrlen(engine->resources[engine->resourceIndex]),
					   engine->resources[engine->resourceIndex]);
		if (*message == NULL) {
			ret = MLN_ERROR_NO_MEMORY;
			goto out;
		}
		goto next;
	}
	locations = engine->token->resourceLocations.items;
	if (locations == NULL) {
		ret = 7;
		goto out;
	}
	ret = MlnBufferToString(MLN_HEAP_SECURE, locations[engine->resourceIndex], &url);
	if (ret != MLN_OK) {
		if (ret != MLN_ERROR_NO_MEMORY)
			ret = MLN_ENGINE_ERROR_FORMAT;
		goto out;
	}
	ret = MlnImpl_8101CBF0(url, 1, NULL, &request);
	if (ret != MLN_OK)
		goto out;
	url = NULL;
	result->request = request;
	request = NULL;
next:
	ret = MLN_OK;
	engine->state = 3;
out:
	MlnFree(MLN_HEAP_SECURE, url);
	MlnHttpRequestFree(request);
	return ret;
}

/*
 * State 3: the service's configuration, as the response gives it: checked against the token
 * and kept, or, for an engine without a token, read from the address the response names.
 * No response tries the next resource.
 */
__attribute__((noinline)) int MlnImpl_8101AF30(MlnEngine *engine, MlnBuffer **message,
					       MlnApiResult *result, int hasValue)
{
	MlnServiceConfig *config = NULL;
	MlnBuffer *response, *decoded = NULL;
	char *text = NULL;
	int ret;

	if (message == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	if (engine == NULL || engine->context == NULL || result == NULL) {
		ret = 7;
		goto out;
	}
	response = *message;
	if (response == NULL) {
		if (++engine->resourceIndex < engine->resourceCount) {
			engine->state = 2;
			ret = MLN_OK;
		} else {
			ret = 85;
		}
		goto out;
	}
	if (engine->token != NULL) {
		ret = MlnServiceConfigParse(response->size, response->data, &config);
		if (ret != MLN_OK)
			goto out;
		if (MlnBufferCompare(MLN_HEAP_SECURE, config->broadbandServiceId,
				     engine->token->broadbandServiceId) != 0 ||
		    MlnBufferCompare(MLN_HEAP_SECURE, config->configVersion,
				     engine->token->configVersion) != 0) {
			ret = MLN_ENGINE_ERROR_FORMAT;
			goto out;
		}
		if (engine->kind == 1) {
			if (engine->token == NULL) {
				ret = 7;
				goto out;
			}
			ret = MlnImpl_8101ABE0(engine, engine->token);
			if (ret != MLN_OK)
				goto out;
		} else {
			ret = MlnImpl_8101D29C(engine->token->broadbandServiceId, response);
			if (ret != MLN_OK)
				goto out;
			ret = MlnImpl_8101ACD0(engine, engine->token, result);
			if (ret != MLN_OK)
				goto out;
		}
	} else {
		if (MlnBufferToString(MLN_HEAP_SECURE, response, &text) != MLN_OK) {
			ret = MLN_ERROR_NO_MEMORY;
			goto out;
		}
		ret = MlnImpl_8101D5B4(text, &decoded);
		if (ret != MLN_OK)
			goto out;
		ret = MlnServiceConfigParse(decoded->size, decoded->data, &config);
		if (ret != MLN_OK)
			goto out;
		MlnBufferDestroy(MLN_HEAP_SECURE, *message);
		*message = NULL;
		engine->state = 1;
	}
	engine->config = config;
	config = NULL;
	ret = MLN_OK;
out:
	MlnServiceConfigFree(config);
	MlnFree(MLN_HEAP_SECURE, text);
	MlnBufferDestroy(MLN_HEAP_SECURE, decoded);
	return ret;
}

#define MLN_CRL_URL	"http://crl.marlin-tmo.com/crl/mtmocrls.crl"

/* state 4: the request for the Marlin trust authority's revocation lists */
__attribute__((noinline)) int MlnImpl_8101B158(MlnEngine *engine, MlnApiResult *result)
{
	MlnHttpRequest *request;
	char *url = NULL;
	int ret;

	if (engine == NULL || result == NULL) {
		ret = 7;
		goto out;
	}
	url = MlnStrdup(MLN_HEAP_SECURE, MLN_CRL_URL);
	if (url == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	ret = MlnImpl_8101CBF0(url, 1, NULL, &request);
	if (ret == MLN_OK) {
		url = NULL;
		result->request = request;
		engine->state = 5;
	}
out:
	MlnFree(MLN_HEAP_SECURE, url);
	return ret;
}

/* state 5: the revocation lists, handed to the service (command 0x405), then the actions */
__attribute__((noinline)) int MlnImpl_8101B244(MlnEngine *engine, MlnBuffer **message,
					       MlnApiResult *result, int hasValue)
{
	if (engine == NULL || engine->context == NULL || result == NULL)
		return 7;
	if (message != NULL && *message != NULL)
		MlnImpl_81014A10(engine->context, MlnImpl_8101ABA8, MlnImpl_8101A634, NULL,
				 *message);
	return MlnEngineStartActions(engine, engine->token, result);
}

/* state 11: the node the service acquired (command 0x41d on the response) */
__attribute__((noinline)) int MlnImpl_8101B324(MlnEngine *engine, MlnBuffer **message,
					       MlnApiResult *result, int hasValue)
{
	MlnBuffer *node = NULL;
	int ret;

	if (engine == NULL) {
		ret = 7;
		goto out;
	}
	if (engine->context == NULL || result == NULL) {
		ret = 7;
	} else if (message == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else if (*message == NULL) {
		ret = 85;
	} else {
		ret = MlnImpl_81015374(engine->context, *message, &node);
		if (ret != MLN_OK) {
			if (ret == 41)
				ret = MlnImpl_8101AA80(engine, &result->value);
		} else {
			ret = MlnImpl_8101A960(engine, 1, node, NULL, &result->value);
			if (ret == MLN_OK) {
				node = NULL;
				engine->progress.done++;
				result->progress = 6;
				result->progressDone = engine->progress.done;
				result->progressTotal = engine->progress.total;
				engine->action++;
				if (engine->token != NULL)
					ret = MlnImpl_8101ABE0(engine, engine->token);
				else
					ret = 7;
			}
		}
	}
	MlnImpl_81015168(engine->context);
out:
	MlnBufferDestroy(MLN_HEAP_SECURE, node);
	return ret;
}

/*
 * State 13: the link the service made (command 0x41f on the response), sent to the
 * registration service; the value gives the link's two ends.
 */
__attribute__((noinline)) int MlnImpl_8101B444(MlnEngine *engine, MlnBuffer **message,
					       MlnApiResult *result, int hasValue)
{
	MlnBuffer *link = NULL, *body = NULL, *from = NULL, *to = NULL;
	MlnServiceEndpoint *endpoint;
	MlnHttpRequest *request;
	MlnServiceConfig *config;
	MlnAction *action;
	char *url = NULL;
	int ret;

	if (engine == NULL) {
		ret = 7;
		goto free;
	}
	if (engine->token == NULL || engine->token->actions == NULL || engine->context == NULL ||
	    result == NULL) {
		ret = 7;
		goto close;
	}
	if (message == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto close;
	}
	if (*message == NULL) {
		ret = 85;
		goto close;
	}
	action = engine->token->actions[engine->action];
	if (action == NULL || action->businessToken == NULL || action->uid[1] == NULL) {
		ret = 7;
		goto close;
	}
	ret = MlnImpl_81015620(engine->context, *message, &link, &body);
	if (ret != 14 && ret != MLN_OK) {
		if (ret == 41) {
			ret = MlnImpl_8101AA80(engine, &result->value);
			if (ret == MLN_OK)
				goto free;
		}
		goto close;
	}
	config = engine->config;
	if (config == NULL || config->registrationService.items == NULL ||
	    (endpoint = *(MlnServiceEndpoint **)config->registrationService.items) == NULL ||
	    endpoint->location == NULL) {
		ret = MLN_ENGINE_ERROR_FORMAT;
		goto close;
	}
	url = MlnStrdup(MLN_HEAP_SECURE, endpoint->location);
	if (url == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto close;
	}
	ret = MlnImpl_8101CBF0(url, 2, body, &request);
	if (ret != MLN_OK)
		goto close;
	result->request = request;
	url = NULL;
	body = NULL;
	if (action->uid[0] != NULL) {
		from = MlnBufferCreate(MLN_HEAP_SECURE, action->uid[0]->size, action->uid[0]->data);
		if (from == NULL) {
			ret = MLN_ERROR_NO_MEMORY;
			goto close;
		}
	}
	to = MlnBufferCreate(MLN_HEAP_SECURE, action->uid[1]->size, action->uid[1]->data);
	if (to == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto close;
	}
	ret = MlnImpl_8101A960(engine, 2, from, to, &result->value);
	if (ret != MLN_OK)
		goto close;
	from = NULL;
	to = NULL;
	engine->progress.done++;
	result->progress = 7;
	result->progressDone = engine->progress.done;
	result->progressTotal = engine->progress.total;
	engine->state = 14;
	goto free;
close:
	MlnImpl_81015168(engine->context);
free:
	MlnFree(MLN_HEAP_SECURE, url);
	MlnBufferDestroy(MLN_HEAP_SECURE, body);
	MlnBufferDestroy(MLN_HEAP_SECURE, link);
	MlnBufferDestroy(MLN_HEAP_SECURE, from);
	MlnBufferDestroy(MLN_HEAP_SECURE, to);
	return ret;
}

/* state 14: the link is made; on to the next action */
__attribute__((noinline)) int MlnImpl_8101B7D8(MlnEngine *engine, MlnApiResult *result)
{
	int ret;

	if (engine == NULL)
		return 7;
	if (result == NULL) {
		ret = 7;
	} else {
		engine->progress.done++;
		result->progress = 7;
		result->progressDone = engine->progress.done;
		result->progressTotal = engine->progress.total;
		engine->action++;
		if (engine->token != NULL)
			ret = MlnImpl_8101ABE0(engine, engine->token);
		else
			ret = 7;
	}
	MlnImpl_81015168(engine->context);
	return ret;
}

/*
 * State 16: the service's deregistration (command 0x423 on the response), sent to the
 * registration service; a service with nothing to send (18) goes on to the next action.
 * The value gives the link's two ends.
 */
__attribute__((noinline)) int MlnImpl_8101B850(MlnEngine *engine, MlnBuffer **message,
					       MlnApiResult *result, int hasValue)
{
	MlnBuffer *body = NULL, *from = NULL, *to = NULL;
	MlnServiceEndpoint *endpoint;
	MlnHttpRequest *request;
	MlnServiceConfig *config;
	MlnAction *action;
	char *url = NULL;
	int ret;

	if (engine == NULL) {
		ret = 7;
		goto free;
	}
	if (engine->token == NULL || engine->token->actions == NULL || engine->context == NULL ||
	    result == NULL) {
		ret = 7;
		goto close;
	}
	if (message == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto close;
	}
	if (*message == NULL) {
		ret = 85;
		goto close;
	}
	action = engine->token->actions[engine->action];
	if (action == NULL || action->businessToken == NULL || action->uid[1] == NULL) {
		ret = 7;
		goto close;
	}
	ret = MlnImpl_81015BB0(engine->context, *message, &body);
	if (ret == 41) {
		ret = MlnImpl_8101AA80(engine, &result->value);
		if (ret == MLN_OK)
			goto free;
		goto close;
	}
	if (ret != MLN_OK) {
		if (ret != 18)
			goto close;
		MlnImpl_81015168(engine->context);
		engine->state = 17;
		engine->action++;
		if (engine->token == NULL) {
			ret = 7;
			goto close;
		}
		ret = MlnImpl_8101ABE0(engine, engine->token);
		if (ret != MLN_OK)
			goto close;
		goto value;
	}
	config = engine->config;
	if (config == NULL || config->registrationService.items == NULL ||
	    (endpoint = *(MlnServiceEndpoint **)config->registrationService.items) == NULL ||
	    endpoint->location == NULL) {
		ret = MLN_ENGINE_ERROR_FORMAT;
		goto close;
	}
	url = MlnStrdup(MLN_HEAP_SECURE, endpoint->location);
	if (url == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto close;
	}
	ret = MlnImpl_8101CBF0(url, 2, body, &request);
	if (ret != MLN_OK)
		goto close;
	result->request = request;
	url = NULL;
	body = NULL;
	engine->state = 17;
value:
	if (action->uid[0] != NULL) {
		from = MlnBufferCreate(MLN_HEAP_SECURE, action->uid[0]->size, action->uid[0]->data);
		if (from == NULL) {
			ret = MLN_ERROR_NO_MEMORY;
			goto close;
		}
	}
	to = MlnBufferCreate(MLN_HEAP_SECURE, action->uid[1]->size, action->uid[1]->data);
	if (to == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto close;
	}
	ret = MlnImpl_8101A960(engine, 3, from, to, &result->value);
	if (ret != MLN_OK)
		goto close;
	from = NULL;
	to = NULL;
	engine->progress.done++;
	result->progress = 8;
	result->progressDone = engine->progress.done;
	result->progressTotal = engine->progress.total;
	goto free;
close:
	MlnImpl_81015168(engine->context);
free:
	MlnFree(MLN_HEAP_SECURE, url);
	MlnBufferDestroy(MLN_HEAP_SECURE, body);
	MlnBufferDestroy(MLN_HEAP_SECURE, from);
	MlnBufferDestroy(MLN_HEAP_SECURE, to);
	return ret;
}

/* state 17: the link is removed; on to the next action */
__attribute__((noinline)) int MlnImpl_8101BBE4(MlnEngine *engine, MlnApiResult *result)
{
	int ret;

	if (engine == NULL)
		return 7;
	if (result == NULL) {
		ret = 7;
	} else {
		engine->progress.done++;
		result->progress = 8;
		result->progressDone = engine->progress.done;
		result->progressTotal = engine->progress.total;
		engine->action++;
		if (engine->token != NULL)
			ret = MlnImpl_8101ABE0(engine, engine->token);
		else
			ret = 7;
	}
	MlnImpl_81015168(engine->context);
	return ret;
}

/* state 19: the license the service acquired (command 0x421 on the response) */
__attribute__((noinline)) int MlnImpl_8101BC60(MlnEngine *engine, MlnBuffer **message,
					       MlnApiResult *result, int hasValue)
{
	MlnBuffer *license = NULL;
	int ret;

	if (engine == NULL) {
		MlnBufferDestroy(MLN_HEAP_SECURE, NULL);
		return 7;
	}
	if (engine->context == NULL || result == NULL) {
		ret = 7;
	} else if (message == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else if (*message == NULL) {
		ret = 85;
	} else {
		ret = MlnImpl_810158FC(engine->context, *message, &license);
		if (ret != MLN_OK) {
			if (ret == 41)
				ret = MlnImpl_8101AA80(engine, &result->value);
		} else {
			ret = MlnImpl_8101A960(engine, 4, license, NULL, &result->value);
			if (ret == MLN_OK) {
				license = NULL;
				engine->progress.done++;
				result->progress = 9;
				result->progressDone = engine->progress.done;
				result->progressTotal = engine->progress.total;
				engine->action++;
				if (engine->token != NULL)
					ret = MlnImpl_8101ABE0(engine, engine->token);
				else
					ret = 7;
			}
		}
	}
	MlnBufferDestroy(MLN_HEAP_SECURE, license);
	MlnImpl_81015168(engine->context);
	return ret;
}

/* state 20: the remote engine's open response (command 0x445), sent on to the service */
#ifndef MLN_PLAYBACK
__attribute__((noinline)) int MlnImpl_8101BD9C(MlnEngine *engine, MlnBuffer **message,
					       MlnApiResult *result, int hasValue)
{
	MlnHttpRequest *request = NULL;
	MlnBuffer *body = NULL;
	int ret;

	if (engine == NULL) {
		ret = 7;
		goto out;
	}
	if (message == NULL || result == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	ret = MlnImpl_81018400(engine->context, *message, &body);
	if (ret != MLN_OK)
		goto out;
	ret = MlnImpl_8101CBF0(NULL, 2, body, &request);
	if (ret == MLN_OK) {
		result->request = request;
		request = NULL;
		body = NULL;
		engine->state = 23;
	}
out:
	MlnHttpRequestFree(request);
	MlnBufferDestroy(MLN_HEAP_SECURE, body);
	return ret;
}
#endif

/* state 22: the remote engine's request (command 0x446), sent to the license transfer service */
#ifndef MLN_PLAYBACK
__attribute__((noinline)) int MlnImpl_8101BE94(MlnEngine *engine, MlnBuffer **message,
					       MlnApiResult *result, int hasValue)
{
	MlnHttpRequest *request = NULL;
	MlnServiceEndpoint *endpoint;
	MlnServiceConfig *config;
	MlnBuffer *body = NULL;
	const char *location;
	char *url = NULL;
	int ret;

	if (engine == NULL) {
		ret = 7;
		goto out;
	}
	if (message == NULL || result == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	ret = MlnImpl_81018528(engine->context, *message, &body);
	if (ret != MLN_OK)
		goto out;
	location = NULL;
	config = engine->config;
	if (config != NULL && config->licenseTransferService.items != NULL &&
	    (endpoint = *(MlnServiceEndpoint **)config->licenseTransferService.items) != NULL)
		location = endpoint->location;
	url = MlnStrdup(MLN_HEAP_SECURE, location);
	if (url == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	ret = MlnImpl_8101CBF0(url, 2, body, &request);
	if (ret == MLN_OK) {
		url = NULL;
		result->request = request;
		request = NULL;
		body = NULL;
		engine->state = 24;
	}
out:
	MlnFree(MLN_HEAP_SECURE, url);
	MlnHttpRequestFree(request);
	MlnBufferDestroy(MLN_HEAP_SECURE, body);
	return ret;
}
#endif

/* state 23: the remote engine's reply (command 0x447), sent back; the engine is closed */
#ifndef MLN_PLAYBACK
__attribute__((noinline)) int MlnImpl_8101BFEC(MlnEngine *engine, MlnBuffer **message,
					       MlnApiResult *result, int hasValue)
{
	MlnHttpRequest *request = NULL;
	MlnBuffer *body = NULL;
	int ret;

	if (engine == NULL) {
		ret = 7;
		goto out;
	}
	if (message == NULL || result == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	ret = MlnImpl_81018650(engine->context, *message, &body);
	if (ret != MLN_OK)
		goto out;
	ret = MlnImpl_81018220(engine->context);
	if (ret != MLN_OK)
		goto out;
	engine->state = 0;
	ret = MlnImpl_8101CBF0(NULL, 2, body, &request);
	if (ret == MLN_OK) {
		result->request = request;
		request = NULL;
		body = NULL;
		engine->state = 0;
	}
out:
	MlnHttpRequestFree(request);
	MlnBufferDestroy(MLN_HEAP_SECURE, body);
	return ret;
}
#endif

/*
 * State 24: the license the transfer service sent (command 0x448 with the engine's secret);
 * the engine is closed and goes on to the next action.
 */
#ifndef MLN_PLAYBACK
__attribute__((noinline)) int MlnImpl_8101C0F4(MlnEngine *engine, MlnBuffer **message,
					       MlnApiResult *result, int hasValue)
{
	MlnBuffer *license = NULL;
	int ret;

	if (engine == NULL) {
		ret = 7;
		goto out;
	}
	if (message == NULL || result == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	ret = MlnImpl_81018784(engine->context, engine->secret, *message, &license);
	if (ret != MLN_OK)
		goto out;
	MlnBufferDestroy(MLN_HEAP_SECURE, engine->secret);
	engine->secret = NULL;
	ret = MlnImpl_81018220(engine->context);
	if (ret != MLN_OK)
		goto out;
	engine->state = 0;
	ret = MlnImpl_8101A960(engine, 4, license, NULL, &result->value);
	if (ret != MLN_OK)
		goto out;
	license = NULL;
	engine->state = 0;
	engine->action++;
	if (engine->token != NULL)
		ret = MlnImpl_8101ABE0(engine, engine->token);
	else
		ret = 7;
out:
	MlnBufferDestroy(MLN_HEAP_SECURE, license);
	return ret;
}
#endif

/* the first endpoint of the license service or of the registration service */
static inline __attribute__((always_inline)) MlnServiceEndpoint *
MlnEngineEndpoint(MlnEngine *engine, int license)
{
	MlnServiceConfig *config = engine->config;
	MlnConfigList *list;

	if (config == NULL)
		return NULL;
	list = license ? &config->licenseService : &config->registrationService;
	if (list->items == NULL)
		return NULL;
	return *(MlnServiceEndpoint **)list->items;
}

/*
 * Opens the engine's context on a service (2 node, 3 link, 4 license, 5 deregistration):
 * its keys and first role from the service's endpoint, the engine's account and secret for a
 * link. Gives the endpoint's node and location.
 */
__attribute__((noinline)) int MlnImpl_8101C1E8(MlnEngine *engine, int service, char **nodeId,
					       char **location)
{
	MlnBuffer *account, *secret, *encryptionKey, *signingKey, *role;
	MlnServiceEndpoint *endpoint;
	MlnBufferPairList *extensions;
	char *node = NULL, *url = NULL, *where;
	int license, ret;

	if (engine == NULL || engine->context == NULL || nodeId == NULL) {
		ret = 7;
		goto out;
	}
	switch (service) {
	case 2:
	case 5:
		account = NULL;
		secret = NULL;
		license = 0;
		break;
	case 3:
		secret = engine->secret;
		account = engine->account;
		license = 0;
		break;
	case 4:
		account = NULL;
		secret = NULL;
		license = 1;
		break;
	default:
		ret = 7;
		goto out;
	}
	endpoint = MlnEngineEndpoint(engine, license);
	if (endpoint != NULL) {
		encryptionKey = endpoint->requestEncryptionKey;
		signingKey = endpoint->responseSigningKey;
		role = endpoint->roles.items != NULL ? *(MlnBuffer **)endpoint->roles.items : NULL;
	} else {
		encryptionKey = NULL;
		signingKey = NULL;
		role = NULL;
	}
	extensions = engine->extensions;
	ret = MlnBufferToString(MLN_HEAP_SECURE, endpoint != NULL ? endpoint->nodeId : NULL, &node);
	if (ret != MLN_OK) {
		if (ret != MLN_ERROR_NO_MEMORY)
			ret = MLN_ENGINE_ERROR_FORMAT;
		goto out;
	}
	endpoint = MlnEngineEndpoint(engine, license);
	where = endpoint != NULL ? endpoint->location : NULL;
	ret = MlnImpl_8101503C(engine->context, service, extensions);
	if (ret != MLN_OK)
		goto out;
	if (encryptionKey == NULL || signingKey == NULL || role == NULL) {
		ret = MLN_ENGINE_ERROR_FORMAT;
		goto out;
	}
	if (secret != NULL) {
		if (account == NULL) {
			ret = MLN_CORE_ERROR_INVALID;
			goto out;
		}
		ret = MlnImpl_81015EA4(engine->context, account, secret);
		if (ret != MLN_OK)
			goto out;
	}
	ret = MlnImpl_81015DDC(engine->context, encryptionKey->size, encryptionKey->data,
			       signingKey->size, signingKey->data, role->size, role->data);
	if (ret != MLN_OK)
		goto out;
	if (where == NULL) {
		ret = MLN_ENGINE_ERROR_FORMAT;
		goto out;
	}
	url = MlnStrdup(MLN_HEAP_SECURE, where);
	if (url == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	*nodeId = node;
	node = NULL;
	*location = url;
	url = NULL;
out:
	MlnFree(MLN_HEAP_SECURE, url);
	MlnFree(MLN_HEAP_SECURE, node);
	return ret;
}

/* state 18: the license acquisition (a business token for the action's content) */
__attribute__((noinline)) int MlnImpl_8101C500(MlnEngine *engine, MlnApiResult *result)
{
	MlnHttpRequest *request = NULL;
	MlnBuffer *body = NULL;
	MlnAction *action;
	char *node = NULL, *url = NULL;
	int ret;

	if (engine == NULL) {
		ret = 7;
		goto free;
	}
	if (engine->token == NULL || engine->token->actions == NULL || engine->context == NULL ||
	    result == NULL) {
		ret = 7;
		goto close;
	}
	action = engine->token->actions[engine->action];
	if (action == NULL || action->businessToken == NULL) {
		ret = 7;
		goto close;
	}
	ret = MlnImpl_8101C1E8(engine, 4, &node, &url);
	if (ret != MLN_OK)
		goto close;
	ret = MlnImpl_81015798(engine->context, action->businessToken->size,
			       action->businessToken->data, node, action->uid[1], &body);
	if (ret != MLN_OK)
		goto close;
	ret = MlnImpl_8101CBF0(url, MLN_HTTP_POST, body, &request);
	if (ret != MLN_OK)
		goto close;
	result->request = request;
	request = NULL;
	body = NULL;
	url = NULL;
	engine->state = 19;
	goto free;
close:
	MlnImpl_81015168(engine->context);
free:
	MlnFree(MLN_HEAP_SECURE, url);
	MlnFree(MLN_HEAP_SECURE, node);
	MlnBufferDestroy(MLN_HEAP_SECURE, body);
	MlnHttpRequestFree(request);
	return ret;
}

/* state 15: the deregistration of the action's link (its two ends) */
__attribute__((noinline)) int MlnImpl_8101C6B4(MlnEngine *engine, MlnApiResult *result)
{
	MlnHttpRequest *request = NULL;
	MlnBuffer *body = NULL;
	MlnAction *action;
	char *node = NULL, *url = NULL;
	int ret;

	if (engine == NULL) {
		ret = 7;
		goto free;
	}
	if (engine->token == NULL || engine->token->actions == NULL || engine->context == NULL ||
	    result == NULL) {
		ret = 7;
		goto close;
	}
	action = engine->token->actions[engine->action];
	if (action == NULL || action->businessToken == NULL || action->uid[1] == NULL) {
		ret = 7;
		goto close;
	}
	ret = MlnImpl_8101C1E8(engine, 5, &node, &url);
	if (ret != MLN_OK)
		goto close;
	ret = MlnImpl_81015A24(engine->context, action->businessToken->size,
			       action->businessToken->data, node, NULL, action->uid[0],
			       action->uid[1], &body);
	if (ret != MLN_OK)
		goto close;
	ret = MlnImpl_8101CBF0(url, MLN_HTTP_POST, body, &request);
	if (ret != MLN_OK)
		goto close;
	result->request = request;
	request = NULL;
	body = NULL;
	url = NULL;
	engine->state = 16;
	goto free;
close:
	MlnImpl_81015168(engine->context);
free:
	MlnFree(MLN_HEAP_SECURE, url);
	MlnFree(MLN_HEAP_SECURE, node);
	MlnBufferDestroy(MLN_HEAP_SECURE, body);
	MlnHttpRequestFree(request);
	return ret;
}

/* state 12: the link acquisition, between the action's two ends */
__attribute__((noinline)) int MlnImpl_8101C880(MlnEngine *engine, MlnApiResult *result)
{
	MlnHttpRequest *request = NULL;
	MlnBuffer *body = NULL;
	MlnAction *action;
	char *node = NULL, *url = NULL;
	int ret;

	if (engine == NULL) {
		ret = 7;
		goto free;
	}
	if (engine->token == NULL || engine->token->actions == NULL || engine->context == NULL ||
	    result == NULL) {
		ret = 7;
		goto close;
	}
	action = engine->token->actions[engine->action];
	if (action == NULL || action->businessToken == NULL || action->uid[1] == NULL) {
		ret = 7;
		goto close;
	}
	ret = MlnImpl_8101C1E8(engine, 3, &node, &url);
	if (ret != MLN_OK)
		goto close;
	ret = MlnImpl_8101549C(engine->context, action->businessToken->size,
			       action->businessToken->data, node, action->uid[0], action->uid[1],
			       &body);
	if (ret != MLN_OK)
		goto close;
	ret = MlnImpl_8101CBF0(url, MLN_HTTP_POST, body, &request);
	if (ret != MLN_OK)
		goto close;
	result->request = request;
	request = NULL;
	body = NULL;
	url = NULL;
	engine->state = 13;
	goto free;
close:
	MlnImpl_81015168(engine->context);
free:
	MlnFree(MLN_HEAP_SECURE, url);
	MlnFree(MLN_HEAP_SECURE, node);
	MlnBufferDestroy(MLN_HEAP_SECURE, body);
	MlnHttpRequestFree(request);
	return ret;
}

/* state 10: the node acquisition */
__attribute__((noinline)) int MlnImpl_8101CA44(MlnEngine *engine, MlnApiResult *result)
{
	MlnHttpRequest *request = NULL;
	MlnBuffer *body = NULL;
	MlnAction *action;
	char *node = NULL, *url = NULL;
	int ret;

	if (engine == NULL) {
		ret = 7;
		goto free;
	}
	if (engine->token == NULL || engine->token->actions == NULL || engine->context == NULL ||
	    result == NULL) {
		ret = 7;
		goto close;
	}
	action = engine->token->actions[engine->action];
	if (action == NULL || action->businessToken == NULL) {
		ret = 7;
		goto close;
	}
	ret = MlnImpl_8101C1E8(engine, 2, &node, &url);
	if (ret != MLN_OK)
		goto close;
	ret = MlnImpl_8101521C(engine->context, action->businessToken->size,
			       action->businessToken->data, node, &body);
	if (ret != MLN_OK)
		goto close;
	ret = MlnImpl_8101CBF0(url, MLN_HTTP_POST, body, &request);
	if (ret != MLN_OK)
		goto close;
	result->request = request;
	request = NULL;
	body = NULL;
	url = NULL;
	engine->state = 11;
	goto free;
close:
	MlnImpl_81015168(engine->context);
free:
	MlnFree(MLN_HEAP_SECURE, url);
	MlnFree(MLN_HEAP_SECURE, node);
	MlnBufferDestroy(MLN_HEAP_SECURE, body);
	MlnHttpRequestFree(request);
	return ret;
}

/* a request to send: the method, the url and body it takes, the body's SOAP headers */
int MlnImpl_8101CBF0(char *url, int method, MlnBuffer *body, MlnHttpRequest **out)
{
	MlnHttpHeaders headers = { 0, NULL };
	MlnHttpRequest *request;
	int ret;

	if (out == NULL) {
		request = NULL;
		ret = 7;
		goto out;
	}
	request = MlnAllocate(MLN_HEAP_SECURE, sizeof(*request));
	if (request == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	MlnMemset(request, 0, sizeof(*request));
	ret = MlnImpl_8101A63C(body, &headers);
	if (ret != MLN_OK)
		goto out;
	request->method = method;
	request->url = url;
	request->body = body;
	request->headers = headers;
	*out = request;
	request = NULL;
out:
	MlnFree(MLN_HEAP_SECURE, request);
	return ret;
}

/*
 * State 21: the license transfer, to the transfer service's endpoint with its keys and the
 * engine's secret.
 */
#ifndef MLN_PLAYBACK
__attribute__((noinline)) int MlnImpl_8101CC94(MlnEngine *engine, MlnApiResult *result)
{
	MlnBuffer *body = NULL, *encryptionKey = NULL, *signingKey = NULL;
	const MlnBuffer *nodeId, *role;
	MlnHttpRequest *request = NULL;
	MlnServiceEndpoint *endpoint;
	MlnServiceConfig *config;
	char *url = NULL;
	int ret;

	config = engine->config;
	if (config == NULL || config->licenseTransferService.count == 0 || engine->state != 21) {
		ret = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	endpoint = config->licenseTransferService.items != NULL ?
		   *(MlnServiceEndpoint **)config->licenseTransferService.items : NULL;
	if (endpoint != NULL) {
		nodeId = endpoint->nodeId;
		role = endpoint->roles.items != NULL ? *(MlnBuffer **)endpoint->roles.items : NULL;
	} else {
		nodeId = NULL;
		role = NULL;
	}
	url = MlnStrdup(MLN_HEAP_SECURE, endpoint != NULL ? endpoint->location : NULL);
	if (url == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	config = engine->config;
	if (config == NULL || config->licenseTransferService.items == NULL ||
	    (endpoint = *(MlnServiceEndpoint **)config->licenseTransferService.items) == NULL ||
	    endpoint->requestEncryptionKey == NULL) {
		ret = 7;
		goto out;
	}
	encryptionKey = MlnBufferCreate(MLN_HEAP_SECURE, endpoint->requestEncryptionKey->size,
					endpoint->requestEncryptionKey->data);
	if (encryptionKey == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	config = engine->config;
	if (config == NULL || config->licenseTransferService.items == NULL ||
	    (endpoint = *(MlnServiceEndpoint **)config->licenseTransferService.items) == NULL ||
	    endpoint->responseSigningKey == NULL) {
		ret = 7;
		goto out;
	}
	signingKey = MlnBufferCreate(MLN_HEAP_SECURE, endpoint->responseSigningKey->size,
				     endpoint->responseSigningKey->data);
	if (signingKey == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	ret = MlnImpl_81017D1C(engine->context, (void **)&engine->extensions, 1, &encryptionKey, 1,
			       &signingKey, role, nodeId);
	if (ret != MLN_OK)
		goto out;
	ret = MlnImpl_810182D0(engine->context, engine->secret, &body);
	if (ret != MLN_OK)
		goto out;
	ret = MlnImpl_8101CBF0(url, MLN_HTTP_POST, body, &request);
	if (ret != MLN_OK)
		goto out;
	body = NULL;
	url = NULL;
	result->request = request;
	request = NULL;
	engine->state = 22;
out:
	MlnFree(MLN_HEAP_SECURE, encryptionKey);
	MlnFree(MLN_HEAP_SECURE, signingKey);
	MlnFree(MLN_HEAP_SECURE, url);
	MlnBufferDestroy(MLN_HEAP_SECURE, body);
	return ret;
}
#endif

/* a file layer's error as the engine gives it */
static inline __attribute__((always_inline)) int MlnFileError(unsigned int ret)
{
	if (ret <= MLN_ERROR_NO_MEMORY)
		return ret != MLN_OK ? MLN_ERROR_NO_MEMORY : MLN_OK;
	if (ret - MLN_FILE_ERROR_PLATFORM <= 7)
		return MLN_FILE_ERROR_INVALID;
	return 7;
}

/* a path in the file area */
#define MLN_FILE_PATH_MAX	1024

static const char s_hexDigits[16] = "0123456789ABCDEF";

/*
 * The file that keeps a service's configuration: the SHA-1 of its id in hex, ".xml", in the
 * file area (made first when create is set).
 */
__attribute__((noinline)) int MlnImpl_8101CECC(int create, const MlnBuffer *serviceId,
					       char *path)
{
	unsigned char digest[20];
	MlnSha1Context sha;
	char name[45];
	unsigned int size = MLN_FILE_PATH_MAX;
	unsigned int i;
	int ret;

	if (serviceId == NULL || serviceId->size == 0 || serviceId->data == NULL || path == NULL)
		return 7;
	MlnMemset(name, 0, sizeof(name));
	ret = MlnImpl_810262B4(MLN_FILE_AREA, &size, path);
	if (ret != MLN_OK)
		goto error;
	if (create) {
		ret = MlnImpl_8102619C(MLN_FILE_AREA, path);
		if (ret != MLN_OK)
			goto error;
	}
	MlnImpl_81033774(&sha);
	MlnImpl_810337C4(&sha, serviceId->data, serviceId->size);
	MlnImpl_81033864(&sha, digest);
	for (i = 0; i < sizeof(digest); i++) {
		name[2 * i] = s_hexDigits[digest[i] >> 4];
		name[2 * i + 1] = s_hexDigits[digest[i] & 0xf];
	}
	MlnStrncat(name, ".xml", 5);
	size = MLN_FILE_PATH_MAX;
	ret = MlnImpl_810256EC(&size, path, name);
	if (ret == MLN_OK)
		return MLN_OK;
error:
	return MlnFileError(ret);
}

/* reads a configuration file: its SHA-1, then the configuration it must match */
__attribute__((noinline)) int MlnImpl_8101D020(const char *path, MlnBuffer **config)
{
	unsigned char stored[20], digest[20];
	unsigned int length, end, size, read, i;
	MlnBuffer *buffer = NULL;
	MlnSha1Context sha;
	void *file = NULL;
	int ret;

	if (config == NULL) {
		ret = 7;
		goto out;
	}
	if (path == NULL)
		goto invalid;
	length = MlnStrlen(path);
	if (length <= 44)
		goto invalid;
	end = length - 4;
	if (MlnMemcmp(&path[end], ".xml", 4) != 0 || MlnImpl_81026430() != (signed char)path[length - 45])
		goto invalid;
	for (i = length - 44; i < end; i++) {
		if ((unsigned int)((unsigned char)path[i] - 'A') > 5 &&
		    (unsigned int)((unsigned char)path[i] - '0') > 9)
			goto invalid;
	}
	ret = MlnImpl_810259F8(MLN_FILE_AREA, path, 1, &file);
	if (ret == MLN_OK)
		ret = MlnImpl_810260E4(MLN_FILE_AREA, file, &size);
	if (ret != MLN_OK) {
		ret = MlnFileError(ret);
		goto out;
	}
	if (size <= sizeof(stored)) {
		ret = 7;
		goto out;
	}
	size -= sizeof(stored);
	buffer = MlnBufferCreate(MLN_HEAP_SECURE, size, NULL);
	if (buffer == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	ret = MlnImpl_81025C1C(MLN_FILE_AREA, file, sizeof(stored), &read, stored);
	if (ret != MLN_OK) {
		ret = MlnFileError(ret);
		goto out;
	}
	if (read != sizeof(stored)) {
		ret = 7;
		goto out;
	}
	ret = MlnImpl_81025C1C(MLN_FILE_AREA, file, size, &read, buffer->data);
	if (ret != MLN_OK) {
		ret = MlnFileError(ret);
		goto out;
	}
	if (size != read) {
		ret = 7;
		goto out;
	}
	MlnImpl_81033774(&sha);
	MlnImpl_810337C4(&sha, buffer->data, buffer->size);
	MlnImpl_81033864(&sha, digest);
	if (MlnMemcmp(stored, digest, sizeof(digest)) != 0) {
		MlnImpl_8102590C(MLN_FILE_AREA, path);
		ret = MLN_FILE_ERROR_INVALID;
		goto out;
	}
	*config = buffer;
	buffer = NULL;
	goto out;
invalid:
	ret = MLN_FILE_ERROR_INVALID;
out:
	MlnBufferDestroy(MLN_HEAP_SECURE, buffer);
	if (file != NULL)
		MlnImpl_81025BE8(MLN_FILE_AREA, file);
	return ret;
}

/* keeps a service's configuration, after its SHA-1; a file left unfinished is removed */
__attribute__((noinline)) int MlnImpl_8101D29C(const MlnBuffer *serviceId, const MlnBuffer *config)
{
	unsigned char digest[20];
	char path[MLN_FILE_PATH_MAX];
	MlnSha1Context sha;
	void *file = NULL;
	int ret;

	if (serviceId == NULL || config == NULL || config->size == 0 || config->data == NULL)
		return 7;
	MlnMemset(path, 0, sizeof(path));
	ret = MlnImpl_8101CECC(1, serviceId, path);
	if (ret == MLN_OK) {
		MlnImpl_81033774(&sha);
		MlnImpl_810337C4(&sha, config->data, config->size);
		MlnImpl_81033864(&sha, digest);
		ret = MlnImpl_810259F8(MLN_FILE_AREA, path, 2, &file);
		if (ret == MLN_OK)
			ret = MlnImpl_81025F5C(MLN_FILE_AREA, file, sizeof(digest), digest);
		if (ret == MLN_OK)
			ret = MlnImpl_81025F5C(MLN_FILE_AREA, file, config->size, config->data);
		ret = MlnFileError(ret);
	}
	if (file != NULL) {
		MlnImpl_81025BE8(MLN_FILE_AREA, file);
		if (ret != MLN_OK)
			MlnImpl_8102590C(MLN_FILE_AREA, path);
	}
	return ret;
}

/* a service's kept configuration */
__attribute__((noinline)) int MlnImpl_8101D408(const MlnBuffer *serviceId, MlnBuffer **config)
{
	char path[MLN_FILE_PATH_MAX];
	int ret;

	if (config == NULL)
		return 7;
	MlnMemset(path, 0, sizeof(path));
	ret = MlnImpl_8101CECC(0, serviceId, path);
	if (ret != MLN_OK)
		return ret;
	return MlnImpl_8101D020(path, config);
}

/* whether a service's configuration is kept */
__attribute__((noinline)) int MlnImpl_8101D468(const MlnBuffer *serviceId, int *exists)
{
	char path[MLN_FILE_PATH_MAX];
	MlnFileInfo info;
	int ret, status;

	if (exists == NULL)
		return 7;
	MlnMemset(path, 0, sizeof(path));
	ret = MlnImpl_8101CECC(0, serviceId, path);
	if (ret != MLN_OK)
		return ret;
	status = MlnImpl_81026324(MLN_FILE_AREA, path, &info);
	if (status == MLN_OK)
		*exists = 1;
	else if (status == MLN_FILE_ERROR_NOT_FOUND)
		*exists = 0;
	else
		ret = MlnFileError(status);
	return ret;
}

/* removes a service's kept configuration */
__attribute__((noinline)) int MlnImpl_8101D520(const MlnBuffer *serviceId)
{
	char path[MLN_FILE_PATH_MAX];
	int ret, status;

	MlnMemset(path, 0, sizeof(path));
	ret = MlnImpl_8101CECC(0, serviceId, path);
	if (ret != MLN_OK)
		return ret;
	status = MlnImpl_8102590C(MLN_FILE_AREA, path);
	if (status != MLN_OK && status != MLN_FILE_ERROR_NOT_FOUND)
		ret = MlnFileError(status);
	return ret;
}

/* a configuration file read by its path */
__attribute__((noinline)) int MlnImpl_8101D5B4(const char *path, MlnBuffer **config)
{
	if (path == NULL || config == NULL)
		return MLN_CORE_ERROR_INVALID;
	return MlnImpl_8101D020(path, config);
}
