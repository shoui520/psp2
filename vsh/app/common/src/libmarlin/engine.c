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
	if (engine->kind == 1)
		MlnImpl_81018220(engine->context);
	else if (engine->kind == 0 || engine->kind == 2)
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
	engine->step = 0;
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
			if (ret == 0x1f0001) {
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
		ret = MlnImpl_8101ACD0(engine, parsed, 0);
		if (ret != MLN_OK)
			goto out;
	}
	/* a link acquisition and a deregistration take two steps */
	engine->stepCount = parsed->actionCount;
	for (i = 0; i < (unsigned int)parsed->actionCount; i++) {
		if ((unsigned int)(parsed->actions[i]->type - MLN_ACTION_LINK_ACQUISITION) <= 1)
			engine->stepCount++;
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

/* the action token of a license transfer (LTP) */
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
