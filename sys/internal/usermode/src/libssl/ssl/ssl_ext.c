/* Native extension ownership and TLS extension processing.
 * Filename and descriptive helper names are ours, not recovered Sony names.
 */
#include <skeleton.h>
#include "ssl_locl.h"
#include "ssl_ext.h"
#include "../err/err.h"
#include "../memory.h"

static int sslHandleRenegotiationInfoExtension(SslExtension *ext, SSL *s);

int sslDestroyExtension(SslExtension *extension)
{
	if (extension != NULL) {
		if (extension->length != 0) {
			if (extension->data != NULL) {
				sslFree(extension->data);
				extension->data = NULL;
			}
			extension->length = 0;
		}
		sslFree(extension);
	}
	return 0;
}

int sslExtensionNew(SceUInt32 type, SslExtension **result)
{
	int status = 0;
	SslExtension *ext;

	if (result == NULL) {
		sslPushError(0x2c, 0x64, 0x23, NULL, 0);
		status = 0x2721;
	} else {
		ext = (SslExtension *)sslAllocate(sizeof(SslExtension));
		if (ext == NULL) {
			status = 0x2715;
			sslPushError(0x2c, 0x64, 0x21, NULL, 0);
		} else {
			sslFillMemory(ext, 0, sizeof(SslExtension));
			ext->type = type;
			*result = ext;
		}
	}
	return status;
}

int sslExtensionCopy(const SslExtension *src, SslExtension **result)
{
	int status;
	SslExtension *ext = NULL;

	if ((src == NULL) || (result == NULL)) {
		sslPushError(0x2c, 0x65, 0x23, NULL, 0);
		return 0x2721;
	}
	status = sslExtensionNew(src->type, &ext);
	if (status == 0) {
		if (src->length != 0) {
			ext->length = src->length;
			if (src->data != NULL) {
				ext->data = sslAllocate(ext->length);
				if (ext->data == NULL) {
					status = 0x2715;
					sslPushError(0x2c, 0x65, 0x21, NULL, 0);
					goto err;
				}
				sslCopyMemory(ext->data, src->data, src->length);
			}
		}
		ext->boolean_option = src->boolean_option;
		ext->state = src->state;
		ext->alert = src->alert;
		if (src->handlers[0].callback != NULL) {
			ext->handlers[0].callback = src->handlers[0].callback;
			ext->handlers[0].parameters[0] = src->handlers[0].parameters[0];
			ext->handlers[0].parameters[1] = src->handlers[0].parameters[1];
		}
		if (src->handlers[1].callback != NULL) {
			ext->handlers[1].callback = src->handlers[1].callback;
			ext->handlers[1].parameters[0] = src->handlers[1].parameters[0];
			ext->handlers[1].parameters[1] = src->handlers[1].parameters[1];
		}
		*result = ext;
	}
	if (status != 0) {
err:
		sslDestroyExtension(ext);
	}
	return status;
}

int sslExtensionListAdd(SslExtensionList *list, const SslExtension *ext)
{
	int status;
	SslExtension *copy = NULL;

	if ((list == NULL) || (ext == NULL)) {
		sslPushError(0x2c, 0x74, 0x23, NULL, 0);
		status = 0x2721;
		goto err;
	}
	if (ext->handlers[0].callback == NULL) {
		sslPushError(0x2c, 0x74, 0x6f, NULL, 0);
		status = 0x2726;
		goto err;
	}
	if (sslStackFindUnsorted(list->entries, (char *)ext) != -1) {
		sslPushError(0x2c, 0x74, 0x70, NULL, 0);
		status = 0x2726;
		goto err;
	}
	status = sslExtensionCopy(ext, &copy);
	if (status == 0) {
		if (sk_insert(list->entries, (char *)copy, -1) == 0) {
			sslPushError(0x2c, 0x74, 0x6e, NULL, 0);
			status = 0x2711;
			goto err;
		}
	}
	if (status != 0) {
err:
		sslDestroyExtension(copy);
	}
	return status;
}

int sslExtensionSetProperty(SslExtension *ext, int selector, const void *value)
{
	int status = 0;

	if ((ext == NULL) || (value == NULL)) {
		sslPushError(0x2c, 0x66, 0x23, NULL, 0);
		return 0x2721;
	}
	switch (selector) {
	case 1:
		if (ext->data != NULL) {
			sslFree(ext->data);
			ext->length = 0;
			ext->data = NULL;
		}
		ext->length = ((const SslExtensionData *)value)->length;
		if (ext->length != 0) {
			ext->data = sslAllocate(ext->length);
			if (ext->data == NULL) {
				sslPushError(0x2c, 0x66, 0x21, NULL, 0);
				status = 0x2715;
			} else {
				sslCopyMemory(
					ext->data,
					((const SslExtensionData *)value)->data,
					ext->length);
			}
		}
		break;
	case 2:
		if ((*(const SceUInt32 *)value != 1) && (*(const SceUInt32 *)value != 0)) {
			sslPushError(0x2c, 0x66, 0x6b, NULL, 0);
			status = 0x2722;
		} else {
			ext->boolean_option = *(const SceUInt32 *)value;
		}
		break;
	case 3:
		ext->state = *(const SceUInt32 *)value;
		break;
	case 4:
		ext->alert = *(const int *)value;
		break;
	case 5:
		ext->handlers[0].callback = ((const SslExtensionHandler *)value)->callback;
		ext->handlers[0].parameters[0] = ((const SslExtensionHandler *)value)->parameters[0];
		ext->handlers[0].parameters[1] = ((const SslExtensionHandler *)value)->parameters[1];
		break;
	case 6:
		ext->handlers[1].callback = ((const SslExtensionHandler *)value)->callback;
		ext->handlers[1].parameters[0] = ((const SslExtensionHandler *)value)->parameters[0];
		ext->handlers[1].parameters[1] = ((const SslExtensionHandler *)value)->parameters[1];
		break;
	default:
		sslPushError(0x2c, 0x66, 0x0e, NULL, 0);
		status = 0x271b;
		break;
	}
	return status;
}

int sslCreateRenegotiationInfoExtension(
	void *library,
	const SslExtensionData *verify_data,
	SslExtension **result)
{
	int status;
	SslExtension *ext = NULL;
	SslExtensionData encoded = { 0, NULL };
	SslExtensionHandler handler = { NULL, { 0, 0 } };

	(void)library;
	if ((result == NULL) || (verify_data == NULL)) {
		status = 0x2721;
		goto end;
	}
	status = sslExtensionNew(0xff01, &ext);
	if (status != 0) {
		goto end;
	}
	encoded.length = verify_data->length + 1;
	encoded.data = (unsigned char *)sslAllocate(encoded.length);
	if (encoded.data == NULL) {
		status = 0x2715;
		goto end;
	}
	encoded.data[0] = (unsigned char)verify_data->length;
	sslCopyMemory(encoded.data + 1, verify_data->data, verify_data->length);
	status = sslExtensionSetProperty(ext, 1, &encoded);
	if (status != 0) {
		goto end;
	}
	handler.callback = sslHandleRenegotiationInfoExtension;
	handler.parameters[0] = 0;
	status = sslExtensionSetProperty(ext, 5, &handler);
	if (status != 0) {
		goto end;
	}
	*result = ext;
	ext = NULL;
end:
	if (encoded.data != NULL) {
		sslFree(encoded.data);
	}
	sslDestroyExtension(ext);
	return status;
}

int sslExtensionListRemove(SslExtensionList *list, SceUInt32 type)
{
	int status = 0;
	int i;
	SslExtension *entry;

	if (list == NULL) {
		sslPushError(0x2c, 0x75, 0x23, NULL, 0);
		return 0x2721;
	}
	for (i = 0; i < list->entries->num; i++) {
		entry = (SslExtension *)list->entries->data[i];
		if (entry == NULL) {
			sslPushError(0x2c, 0x75, 0x6d, NULL, 0);
			status = 0x2711;
			break;
		}
		if (entry->type == type) {
			sslDestroyExtension(entry);
			sk_delete(list->entries, i);
			break;
		}
	}
	return status;
}

int sslExtensionListNew(SslExtensionList **result)
{
	int status = 0;
	SslExtensionList *list;
	unsigned int i;

	if (result == NULL) {
		sslPushError(0x2c, 0x71, 0x23, NULL, 0);
		status = 0x2721;
	} else {
		list = (SslExtensionList *)sslAllocate(sizeof(SslExtensionList));
		if (list == NULL) {
			sslPushError(0x2c, 0x71, 0x21, NULL, 0);
			status = 0x2715;
		} else {
			for (i = 0; i < sizeof(SslExtensionList); i++) {
				((unsigned char *)list)[i] = 0;
			}
			list->entries = sk_new(sslExtensionCompare);
			if (list->entries == NULL) {
				sslFree(list);
				status = 0x2715;
			} else {
				list->pending_index = (unsigned int)-1;
				*result = list;
			}
		}
	}
	return status;
}

int sslSetupRenegotiationInfoExtension(SSL *s, int is_server)
{
	int ret = 0;
	SslExtension *ext = NULL;
	SslExtensionData verify_data = { 0, NULL };
	SslExtensionList **list_ptr;
	SceUInt32 state;

	if (is_server == 0) {
		list_ptr = &s->extensions;
	} else {
		list_ptr = &s->session->extensions;
	}
	if (*list_ptr == NULL) {
		if (sslExtensionListNew(list_ptr) != 0) {
			goto end;
		}
	} else {
		sslExtensionListRemove(*list_ptr, 0xff01);
	}
	if (s->read_crypto.cipher_algorithm != NULL) {
		verify_data.length = s->method->ssl3_enc->finish_mac_length;
		verify_data.data = &s->s3->tmp.finish_md[verify_data.length];
	}
	if (sslCreateRenegotiationInfoExtension(s->library, &verify_data, &ext) != 0) {
		goto end;
	}
	if (is_server != 0) {
		state = 2;
		sslExtensionSetProperty(ext, 3, &state);
	}
	if (sslExtensionListAdd(*list_ptr, ext) == 0) {
		ret = 1;
	}
end:
	sslDestroyExtension(ext);
	return ret;
}

int sslExtensionListGetProperty(const SslExtensionList *list, int selector, int *result)
{
	int status = 0;

	if ((list == NULL) || (result == NULL)) {
		sslPushError(0x2c, 0x73, 0x23, NULL, 0);
		status = 0x2721;
	} else if (selector == 0) {
		*result = list->entries->num;
	} else {
		sslPushError(0x2c, 0x73, 0x0e, NULL, 0);
		status = 0x271b;
	}
	return status;
}

int sslExtensionListGetEntry(
	const SslExtensionList *list,
	int index,
	SslExtension **result)
{
	int status = 0;

	if ((list == NULL) || (result == NULL)) {
		sslPushError(0x2c, 0x76, 0x23, NULL, 0);
		status = 0x2721;
	} else if (index > list->entries->num) {
		sslPushError(0x2c, 0x76, 0x71, NULL, 0);
		status = 0x2723;
	} else {
		*result = (SslExtension *)list->entries->data[index];
	}
	return status;
}

int sslExtensionCompare(const SslExtension *const *a, const SslExtension *const *b)
{
	int ret = 0;

	if ((*a)->type != (*b)->type) {
		if ((*a)->type > (*b)->type) {
			ret = 1;
		} else {
			ret = -1;
		}
	}
	return ret;
}

void sslExtensionFree(SslExtension *extension)
{
	sslDestroyExtension(extension);
}

int sslExtensionGetProperty(const SslExtension *ext, int selector, void *result)
{
	int status = 0;

	if ((ext == NULL) || (result == NULL)) {
		sslPushError(0x2c, 0x67, 0x23, NULL, 0);
		return 0x2721;
	}
	switch (selector) {
	case 0:
		*(SceUInt32 *)result = ext->type;
		break;
	case 1:
		((SslExtensionData *)result)->length = ext->length;
		((SslExtensionData *)result)->data = ext->data;
		break;
	case 2:
		*(SceUInt32 *)result = ext->boolean_option;
		break;
	case 3:
		*(SceUInt32 *)result = ext->state;
		break;
	case 4:
		*(int *)result = ext->alert;
		break;
	case 5:
		((SslExtensionHandler *)result)->callback = ext->handlers[0].callback;
		((SslExtensionHandler *)result)->parameters[0] = ext->handlers[0].parameters[0];
		((SslExtensionHandler *)result)->parameters[1] = ext->handlers[0].parameters[1];
		break;
	case 6:
		((SslExtensionHandler *)result)->callback = ext->handlers[1].callback;
		((SslExtensionHandler *)result)->parameters[0] = ext->handlers[1].parameters[0];
		((SslExtensionHandler *)result)->parameters[1] = ext->handlers[1].parameters[1];
		break;
	default:
		sslPushError(0x2c, 0x67, 0x0e, NULL, 0);
		status = 0x271b;
		break;
	}
	return status;
}

int sslExtensionListFindByType(
	const SslExtensionList *list,
	SceUInt32 type,
	SslExtension **result)
{
	int status = 0x2718;
	int ret;
	int i;
	SceUInt32 entry_type;
	SslExtension *entry;

	if ((list == NULL) || (result == NULL)) {
		sslPushError(0x2c, 0x77, 0x23, NULL, 0);
		status = 0x2721;
	} else {
		for (i = 0; i < list->entries->num; i++) {
			entry = (SslExtension *)list->entries->data[i];
			ret = sslExtensionGetProperty(entry, 0, &entry_type);
			if (ret != 0) {
				status = ret;
				break;
			}
			if (entry_type == type) {
				status = 0;
				*result = entry;
				break;
			}
		}
	}
	return status;
}

int sslExtensionListContainsType(const SslExtensionList *list, SceUInt32 type)
{
	int status = 0;
	int found = 0;
	int i;
	SceUInt32 entry_type;

	if (list == NULL) {
		sslPushError(0x2c, 0x78, 0x23, NULL, 0);
		return 0x2721;
	}
	for (i = 0; i < list->entries->num; i++) {
		status = sslExtensionGetProperty(
			(SslExtension *)list->entries->data[i],
			0,
			&entry_type);
		if (status != 0) {
			break;
		}
		if (entry_type == type) {
			found = 1;
			break;
		}
	}
	if ((found == 0) && (status == 0)) {
		status = 0x2718;
	}
	return status;
}

int sslExtensionListEncode(
	const SslExtensionList *list,
	SceUInt32 max_len,
	unsigned char *buf,
	SceUInt32 *out_len)
{
	int status = 0;
	int total_len = 0;
	SslExtensionData ext_data = { 0, NULL };
	int ext_type;
	SslExtension *entry;
	int count;
	int i;
	int pos;

	if ((list == NULL) || (out_len == NULL)) {
		sslPushError(0x2c, 0x79, 0x23, NULL, 0);
		status = 0x2721;
		goto end;
	}
	status = sslExtensionListGetProperty(list, 0, &count);
	if ((status != 0) || (count == 0)) {
		goto end;
	}
	for (i = 0; i < count; i++) {
		status = sslExtensionListGetEntry(list, i, &entry);
		if (status != 0) {
			goto end;
		}
		if (entry != NULL) {
			if (sslExtensionGetProperty(entry, 0, &ext_type) != 0) {
				goto end;
			}
			if (sslExtensionGetProperty(entry, 1, &ext_data) != 0) {
				goto end;
			}
			pos = total_len;
			total_len = pos + (int)ext_data.length + 4;
			if (i == 0) {
				total_len = pos + (int)ext_data.length + 6;
				pos += 2;
			}
			if (buf != NULL) {
				if ((int)max_len < total_len) {
					sslPushError(0x2c, 0x79, 0x72, NULL, 0);
					status = 0x2720;
					goto end;
				}
				buf[pos] = (unsigned char)(ext_type >> 8);
				buf[pos + 1] = (unsigned char)ext_type;
				buf[pos + 2] = (unsigned char)(((int)ext_data.length) >> 8);
				buf[pos + 3] = (unsigned char)ext_data.length;
				if (ext_data.data != NULL) {
					sslCopyMemory(
						&buf[pos + 4],
						ext_data.data,
						ext_data.length);
				}
			}
		}
	}
	if (buf != NULL) {
		int body_len = total_len - 2;
		buf[1] = (unsigned char)body_len;
		buf[0] = (unsigned char)(body_len >> 8);
	}
end:
	*out_len = (SceUInt32)total_len;
	return status;
}

int sslExtensionListCheckMandatory(
	const SslExtensionList *sent,
	const SslExtensionList *received)
{
	int status = 0;
	int i;
	SceUInt32 mandatory = 0;
	SceUInt32 ext_type;
	SslExtension *entry;

	if (sent == NULL) {
		sslPushError(0x2c, 0x7c, 0x23, NULL, 0);
		return 0x2721;
	}
	if (sent->entries == NULL) {
		sslPushError(0x2c, 0x7c, 0x75, NULL, 0);
		return 0x2711;
	}
	for (i = 0; i < sent->entries->num; i++) {
		entry = (SslExtension *)sent->entries->data[i];
		status = sslExtensionGetProperty(entry, 2, &mandatory);
		if (status != 0) {
			break;
		}
		if ((mandatory & 1) != 0) {
			if (received == NULL) {
				status = 0x2718;
				break;
			}
			status = sslExtensionGetProperty(entry, 0, &ext_type);
			if (status != 0) {
				break;
			}
			status = sslExtensionListContainsType(received, ext_type);
			if (status != 0) {
				break;
			}
		}
	}
	return status;
}

static __inline int sslExtensionListFreeInline(SslExtensionList *list)
{
	if (list != NULL) {
		sk_pop_free(list->entries, (void (*)())sslExtensionFree);
		sslFree(list);
	}
	return 0;
}

static __inline int sslGetSessionExtensionsInline(
	SSL_SESSION *session,
	SslExtensionList **result)
{
	int status = 0;

	if ((session == NULL) || (result == NULL)) {
		status = 0x2721;
	} else {
		*result = session->extensions;
	}
	return status;
}

static __inline int sslSetSessionExtensionsInline(
	SSL_SESSION *session,
	SslExtensionList *extensions)
{
	if ((session == NULL) || (extensions == NULL)) {
		return 0x2721;
	}
	sslExtensionListFreeInline(session->extensions);
	return sslExtensionListCopy(extensions, &session->extensions);
}

int sslExtensionListProcess(
	SslExtensionList *list,
	const unsigned char *data,
	SSL *s,
	int *alert)
{
	int status = 0;
	SslExtension *found_ext = NULL;
	int local_alert = -1;
	SslExtension *new_ext = NULL;
	SslExtensionList *rlist;
	SslExtensionList *sess_ext = NULL;
	SslExtensionHandler handler = { NULL, { 0, 0 } };
	SslExtensionData ext_data = { 0, NULL };
	int count1 = 0;
	SceUInt32 state = 0;
	int count2 = 0;
	int rcount = 0;
	int resuming = ((int)list->pending_index >= 0);
	int is_server;
	int i;
	SceUInt32 total_len;
	const unsigned char *p;
	const unsigned char *start_p;
	const unsigned char *end_p;
	SceUInt32 ext_type;
	SceUInt32 ext_len;
	const unsigned char *body_p;
	const unsigned char *next_p;

	if ((list == NULL) || (s == NULL) || ((data == NULL) && !resuming)) {
		sslPushError(0x2c, 0x7a, 0x23, NULL, 0);
		status = 0x2721;
		goto end;
	}
	is_server = s->server;
	if (!resuming) {
		total_len = ((SceUInt32)data[0] << 8) | data[1];
		p = data + 2;

		if (total_len > 0xffff) {
			sslPushError(0x2c, 0x7a, 0x73, NULL, 0);
			local_alert = 0x32;
			goto end;
		}
		status = sslExtensionListNew(&rlist);
		if (status != 0) {
			local_alert = 0x50;
			goto end;
		}
		start_p = p;
		if (total_len != 0) {
			end_p = p + total_len;

			do {
				ext_type = ((SceUInt32)p[0] << 8) | p[1];
				ext_len = ((SceUInt32)p[2] << 8) | p[3];
				body_p = p + 4;

				if ((ext_len > 0xffff) || ((body_p + ext_len) > end_p)) {
					sslPushError(0x2c, 0x7a, 0x73, NULL, 0);
					local_alert = 0x32;
					goto end;
				}
				next_p = body_p + ext_len;
				status = sslExtensionListFindByType(list, ext_type, &found_ext);
				if (status != 0) {
					if (status == 0x2718) {
						if (!is_server) {
							sslPushError(0x2c, 0x7a, 0x74, NULL, 0);
							local_alert = 0x6e;
							goto end;
						}
						status = 0;
					} else {
						local_alert = 0x50;
						goto end;
					}
				} else {
					status = sslExtensionCopy(found_ext, &new_ext);
					if (status != 0) {
						local_alert = 0x50;
						goto end;
					}
					ext_data.length = ext_len;
					ext_data.data = (unsigned char *)body_p;
					status = sslExtensionSetProperty(new_ext, 1, &ext_data);
					if (status != 0) {
						local_alert = 0x50;
						goto end;
					}
					status = sslExtensionListAdd(rlist, new_ext);
					if (status != 0) {
						local_alert = 0x50;
						goto end;
					}
					sslDestroyExtension(new_ext);
					new_ext = NULL;
					rcount++;
				}
				p = next_p;
			} while (total_len > (SceUInt32)(p - start_p));
		}
		status = sslExtensionListGetProperty(list, 0, &count1);
		if (status != 0) {
			local_alert = 0x50;
			goto end;
		}
		status = sslExtensionListGetProperty(rlist, 0, &count2);
		if (status != 0) {
			local_alert = 0x50;
			goto end;
		}
		if (count1 != count2) {
			status = sslExtensionListCheckMandatory(list, rlist);
			if (status != 0) {
				if (status == 0x2718) {
					local_alert = 0x32;
				} else {
					local_alert = 0x50;
				}
				goto end;
			}
		}
	} else {
		status = sslGetSessionExtensionsInline(s->session, &sess_ext);
		if (status != 0) {
			local_alert = 0x50;
			goto end;
		}
		status = sslExtensionListCopy(sess_ext, &rlist);
		if (status != 0) {
			local_alert = 0x50;
			goto end;
		}
		status = sslExtensionListGetProperty(rlist, 0, &rcount);
		if (status != 0) {
			local_alert = 0x50;
			goto end;
		}
	}
	if (rcount != 0) {
		for (i = 0; rcount > i; i++) {
			status = sslExtensionListGetEntry(rlist, i, &found_ext);
			if (status != 0) {
				local_alert = 0x50;
				goto end;
			}
			status = sslExtensionGetProperty(found_ext, 3, &state);
			if (status != 0) {
				local_alert = 0x50;
				goto end;
			}
			if (state == 2) {
				continue;
			}
			status = sslExtensionGetProperty(found_ext, 5, &handler);
			if (status != 0) {
				local_alert = 0x50;
				goto end;
			}
			if (handler.callback == NULL) {
				local_alert = 0x50;
				status = 0x2711;
				goto end;
			}
			status = handler.callback(found_ext, s, handler.parameters[0]);
			if (status != 0) {
				sslExtensionGetProperty(found_ext, 4, &local_alert);
				if (local_alert != 0) {
					goto end;
				}
				sslExtensionGetProperty(found_ext, 3, &state);
				if (state == 1) {
					list->pending_index = i;
					local_alert = -1;
					break;
				}
				local_alert = 0x50;
				goto end;
			}
		}
		if (rcount != 0) {
			if (sslSetSessionExtensionsInline(s->session, rlist) != 0) {
				local_alert = 0x50;
				status = 0x2711;
			}
		}
	}
end:
	if (new_ext != NULL) {
		sslDestroyExtension(new_ext);
		new_ext = NULL;
	}
	sslExtensionListFreeInline(rlist);
	*alert = local_alert;
	return status;
}

int sslExtensionListRunClientHandlers(const SslExtensionList *list, SSL *s)
{
	int status = 0;
	int alert = -1;
	SslExtension *entry = NULL;
	SslExtensionHandler handler = { NULL, { 0, 0 } };
	int count;
	int i;

	if ((list == NULL) || (s == NULL)) {
		sslPushError(0x2c, 0x7b, 0x23, NULL, 0);
		return 0x2721;
	}
	count = list->entries->num;
	if (count != 0) {
		for (i = 0; i < count; i++) {
			status = sslExtensionListGetEntry(list, i, &entry);
			if (status != 0) {
				break;
			}
			if (entry != NULL) {
				status = sslExtensionGetProperty(entry, 5, &handler);
				if (status != 0) {
					break;
				}
				status = handler.callback(entry, s, handler.parameters[0]);
				if (status != 0) {
					sslExtensionGetProperty(entry, 4, &alert);
					break;
				}
			}
		}
	}
	return status;
}

static int sslHandleRenegotiationInfoExtension(SslExtension *ext, SSL *s)
{
	int status = 0;
	SslExtensionData ext_data = { 0, NULL };
	int alert = 0;
	int is_server;
	SceUInt32 state;

	if ((ext == NULL) || (s == NULL)) {
		sslPushError(0x2c, 0x80, 0x23, NULL, 0);
		status = 0x2721;
		goto end;
	}
	status = sslExtensionGetProperty(ext, 1, &ext_data);
	if (status != 0) {
		alert = 0x50;
		goto end;
	}
	is_server = s->server;
	if (s->read_crypto.cipher_algorithm != NULL) {
		if (is_server) {
			SslExtensionData server_data = { 0, NULL };
			unsigned int finish_len;
			unsigned int server_len;

			s->state = SSL3_ST_SR_CLNT_HELLO_RENEGOTIATE;
			finish_len = s->method->ssl3_enc->finish_mac_length;
			status = 0x2711;
			if (((finish_len + 1) != ext_data.length) ||
			    (*(unsigned char *)ext_data.data != (unsigned char)finish_len) ||
			    (sslCompareMemory(
				     (const unsigned char *)ext_data.data + 1,
				     s->s3->tmp.finish_md,
				     finish_len) != 0)) {
				sslPushError(0x2c, 0x84, 0x67, NULL, 0);
				alert = 0x50;
			} else {
				server_len = finish_len * 2;
				server_data.length = server_len + 1;
				server_data.data = sslAllocate(server_data.length);
				if (server_data.data == NULL) {
					sslPushError(0x2c, 0x84, 0x21, NULL, 0);
					alert = 0x50;
					status = 0x2715;
				} else {
					*(unsigned char *)server_data.data =
						(unsigned char)server_len;
					sslCopyMemory(
						(unsigned char *)server_data.data + 1,
						s->s3->tmp.finish_md,
						server_len);
					status = sslExtensionSetProperty(ext, 1, &server_data);
					if (status != 0) {
						alert = 0x50;
					} else {
						status = 0;
					}
				}
			}
			if (server_data.data != NULL) {
				sslFree(server_data.data);
			}
		} else {
			unsigned int finish_len = s->method->ssl3_enc->finish_mac_length;
			unsigned char *server_md = s->s3->tmp.finish_md;
			unsigned char *client_md = &server_md[finish_len];

			if ((((finish_len * 2) + 1) != ext_data.length) ||
			    (*(unsigned char *)ext_data.data !=
			     (unsigned char)(finish_len * 2)) ||
			    (sslCompareMemory(
				     (const unsigned char *)ext_data.data + 1,
				     client_md,
				     finish_len) != 0) ||
			    (sslCompareMemory(
				     (const unsigned char *)ext_data.data + 1 + finish_len,
				     server_md,
				     finish_len) != 0)) {
				sslPushError(0x2c, 0x84, 0x67, NULL, 0);
				alert = 0x50;
				status = 0x2711;
			} else {
				status = 0;
			}
		}
		if (status != 0) {
			goto end;
		}
	} else {
		if (ext_data.length != 1) {
			sslPushError(0x2c, 0x84, 0x7b, NULL, 0);
			alert = 0x50;
			goto end;
		}
	}
	state = 2;
	status = sslExtensionSetProperty(ext, 3, &state);
	if (status != 0) {
		alert = 0x50;
	}
end:
	if (alert != 0) {
		sslExtensionSetProperty(ext, 4, &alert);
	}
	return status;
}

STUB(SceSslInternal_133ED36D)
STUB(SceSslInternal_7D30F0A6)
STUB(SceSslInternal_A374CD7B)
STUB(SceSslInternal_AC1DBE40)
STUB(SceSslInternal_DEC6DD27)
STUB(SceSslInternal_FB51AE89)
STUB(SceSslInternal_FEFE761C)
