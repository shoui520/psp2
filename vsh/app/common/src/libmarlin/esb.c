/*
 * The values an action's extended status block carries (integers, reals, strings, dates,
 * parameters, extended parameters, resources and lists), and the output controls read from
 * its parameters. The file's name is ours.
 */
#include "mln_internal.h"

/* a parameter with this name */
#define MLN_PARAMETER_IS(parameter, text)					\
	((parameter)->name != NULL &&						\
	 (parameter)->name->size == sizeof(text) - 1 &&			\
	 MlnMemcmp((parameter)->name->data, (text), sizeof(text) - 1) == 0)

/*
 * An output control from its parameters, each an integer parameter: BasicCCI (EPN, CCI,
 * ImageConstraintToken, DigitalOnlyToken, APS) or DTCP (RetentionMoveMode, RetentionState,
 * EPN, DTCP_CCI, ImageConstraintToken, APS). Another control or parameter is marked unknown,
 * and with report set in *flags too. No parameters: no control.
 */
__attribute__((noinline)) int MlnOutputControlParse(int heap, const MlnValueList *parameters,
						    const MlnBuffer *id, int report,
						    unsigned int *flags, MlnOutputControl **out)
{
	MlnOutputControl *control = NULL;
	const MlnParameter *parameter;
	const MlnValue *item;
	int i, ret;

	if (flags == NULL || out == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	if (parameters == NULL || parameters->count == 0) {
		*out = NULL;
		ret = MLN_OK;
		goto out;
	}
	control = MlnAllocate(heap, sizeof(*control));
	if (control == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	MlnMemset(control, 0, sizeof(*control));
	if (id == NULL)
		goto unknown;

	if (id->size == 8 && MlnMemcmp(id->data, "BasicCCI", 8) == 0) {
		control->kind = MLN_OUTPUT_CONTROL_BASIC_CCI;
		for (i = 0; i < parameters->count; i++) {
			item = parameters->items[i];
			if (item->type != MLN_VALUE_PARAMETER)
				goto format;
			parameter = item->data.parameter;
			if (parameter->value->type != MLN_VALUE_INTEGER)
				goto format;
			if (MLN_PARAMETER_IS(parameter, "EPN")) {
				control->flags |= MLN_OUTPUT_CONTROL_EPN;
				control->basic.epn = parameter->value->data.integer;
			} else if (MLN_PARAMETER_IS(parameter, "CCI")) {
				control->flags |= MLN_OUTPUT_CONTROL_CCI;
				control->basic.cci = parameter->value->data.integer;
			} else if (MLN_PARAMETER_IS(parameter, "ImageConstraintToken")) {
				control->flags |= MLN_OUTPUT_CONTROL_IMAGE_CONSTRAINT;
				control->basic.imageConstraintToken = parameter->value->data.integer;
			} else if (MLN_PARAMETER_IS(parameter, "DigitalOnlyToken")) {
				control->flags |= MLN_OUTPUT_CONTROL_DIGITAL_ONLY;
				control->basic.digitalOnlyToken = parameter->value->data.integer;
			} else if (MLN_PARAMETER_IS(parameter, "APS")) {
				control->flags |= MLN_OUTPUT_CONTROL_APS;
				control->basic.aps = parameter->value->data.integer;
			} else {
				control->flags |= MLN_OUTPUT_CONTROL_UNKNOWN;
				if (report)
					*flags |= MLN_OUTPUT_CONTROL_UNKNOWN;
			}
		}
	} else if (id->size == 4 && MlnMemcmp(id->data, "DTCP", 4) == 0) {
		control->kind = MLN_OUTPUT_CONTROL_DTCP;
		for (i = 0; i < parameters->count; i++) {
			item = parameters->items[i];
			if (item->type != MLN_VALUE_PARAMETER)
				goto format;
			parameter = item->data.parameter;
			if (parameter->value->type != MLN_VALUE_INTEGER)
				goto format;
			if (MLN_PARAMETER_IS(parameter, "RetentionMoveMode")) {
				control->flags |= MLN_OUTPUT_CONTROL_RETENTION_MOVE_MODE;
				control->dtcp.retentionMoveMode = parameter->value->data.integer;
			} else if (MLN_PARAMETER_IS(parameter, "RetentionState")) {
				control->flags |= MLN_OUTPUT_CONTROL_RETENTION_STATE;
				control->dtcp.retentionState = parameter->value->data.integer;
			} else if (MLN_PARAMETER_IS(parameter, "EPN")) {
				control->flags |= MLN_OUTPUT_CONTROL_EPN;
				control->dtcp.epn = parameter->value->data.integer;
			} else if (MLN_PARAMETER_IS(parameter, "DTCP_CCI")) {
				control->flags |= MLN_OUTPUT_CONTROL_CCI;
				control->dtcp.cci = parameter->value->data.integer;
			} else if (MLN_PARAMETER_IS(parameter, "ImageConstraintToken")) {
				control->flags |= MLN_OUTPUT_CONTROL_IMAGE_CONSTRAINT;
				control->dtcp.imageConstraintToken = parameter->value->data.integer;
			} else if (MLN_PARAMETER_IS(parameter, "APS")) {
				control->flags |= MLN_OUTPUT_CONTROL_APS;
				control->dtcp.aps = parameter->value->data.integer;
			} else {
				control->flags |= MLN_OUTPUT_CONTROL_UNKNOWN;
				if (report)
					*flags |= MLN_OUTPUT_CONTROL_UNKNOWN;
			}
		}
	} else {
unknown:
		if (report)
			*flags |= MLN_OUTPUT_CONTROL_UNKNOWN;
	}
	*out = control;
	control = NULL;
	ret = MLN_OK;
	goto out;

format:
	ret = MLN_ERROR_FORMAT;
out:
	MlnFree(heap, control);
	return ret;
}

/* a copy of a name buffer (none: none) */
#define MLN_COPY_NAME(heap, to, from, fail)					\
	do {									\
		MlnBuffer *name_ = NULL;					\
		if ((from) != NULL) {						\
			name_ = MlnBufferCreate((heap), (from)->size, (from)->data); \
			if (name_ == NULL) {					\
				MlnBufferDestroy((heap), name_);		\
				goto fail;					\
			}							\
			(to) = name_;						\
			name_ = NULL;						\
		} else {							\
			(to) = NULL;						\
		}							\
		MlnBufferDestroy((heap), name_);				\
	} while (0)

/* a deep copy of a value (none: none) */
__attribute__((noinline)) int MlnValueCopy(int heap, const MlnValue *value, MlnValue **out)
{
	MlnValue *copy = NULL;
	MlnParameter *parameter;
	const MlnParameter *sourceParameter;
	MlnExtendedParameter *extended;
	const MlnExtendedParameter *sourceExtended;
	MlnValueList *list;
	const MlnValueList *sourceList;
	int ret, i;

	if (value == NULL) {
		*out = NULL;
		ret = MLN_OK;
		goto out;
	}
	copy = MlnAllocate(heap, sizeof(*copy));
	if (copy == NULL)
		goto no_memory;
	MlnMemset(copy, 0, sizeof(*copy));
	copy->type = value->type;
	switch (value->type) {
	case MLN_VALUE_INTEGER:
	case MLN_VALUE_REAL:
	case MLN_VALUE_DATE:
		copy->data.integer = value->data.integer;
		break;
	case MLN_VALUE_STRING:
	case MLN_VALUE_RESOURCE:
		MLN_COPY_NAME(heap, copy->data.buffer, value->data.buffer, no_memory);
		break;
	case MLN_VALUE_PARAMETER:
		copy->data.parameter = MlnAllocate(heap, sizeof(*parameter));
		if (copy->data.parameter == NULL)
			goto no_memory;
		MlnMemset(copy->data.parameter, 0, sizeof(*parameter));
		sourceParameter = value->data.parameter;
		parameter = copy->data.parameter;
		if (sourceParameter == NULL)
			break;
		MLN_COPY_NAME(heap, parameter->name, sourceParameter->name, no_memory);
		ret = MlnValueCopy(heap, sourceParameter->value, &parameter->value);
		if (ret != MLN_OK)
			goto out;
		break;
	case MLN_VALUE_EXTENDED_PARAMETER:
		sourceExtended = value->data.extended;
		if (sourceExtended == NULL) {
			extended = NULL;
		} else {
			extended = MlnAllocate(heap, sizeof(*extended));
			if (extended == NULL) {
				ret = MLN_ERROR_NO_MEMORY;
				goto release_extended;
			}
			MlnMemset(extended, 0, sizeof(*extended));
			MLN_COPY_NAME(heap, extended->name, sourceExtended->name,
				      extended_no_memory);
			ret = MlnValueCopy(heap, sourceExtended->value, &extended->value);
			if (ret != MLN_OK)
				goto release_extended;
			extended->flags = sourceExtended->flags;
		}
		copy->data.extended = extended;
		extended = NULL;
		MlnExtendedParameterFree(heap, extended);
		break;
	case MLN_VALUE_LIST:
		sourceList = value->data.list;
		if (sourceList == NULL) {
			list = NULL;
		} else {
			list = MlnAllocate(heap, (sourceList->count + 1) * sizeof(list->items[0]));
			if (list == NULL) {
				ret = MLN_ERROR_NO_MEMORY;
				goto release_list;
			}
			MlnMemset(list->items, 0, sourceList->count * sizeof(list->items[0]));
			list->count = sourceList->count;
			for (i = 0; i < sourceList->count; i++) {
				ret = MlnValueCopy(heap, sourceList->items[i], &list->items[i]);
				if (ret != MLN_OK)
					goto release_list;
			}
		}
		copy->data.list = list;
		list = NULL;
		MlnValueListFree(heap, list);
		break;
	default:
		ret = MLN_ERROR_UNSUPPORTED;
		goto out;
	}
	*out = copy;
	copy = NULL;
	ret = MLN_OK;
	goto out;

extended_no_memory:
	ret = MLN_ERROR_NO_MEMORY;
release_extended:
	MlnExtendedParameterFree(heap, extended);
	goto out;
release_list:
	MlnValueListFree(heap, list);
	goto out;
no_memory:
	ret = MLN_ERROR_NO_MEMORY;
out:
	MlnValueFree(heap, copy);
	return ret;
}

__attribute__((noinline)) int MlnRightsInfoFree(int heap, MlnRightsInfo *info)
{
	MlnObligation *obligation;
	MlnPermission *permission;
	MlnCallback *callback;
	MlnBuffer **ids;
	MlnDateTime **times;
	unsigned int i;

	if (info == NULL)
		return MLN_OK;
	if (info->obligations != NULL) {
		for (i = 0; i < info->obligationCount; i++) {
			obligation = &info->obligations[i];
			if (obligation == NULL)
				continue;
			switch (obligation->type) {
			case MLN_OBLIGATION_RUN_AGENT_ON_PEER:
				MlnBufferDestroy(heap, obligation->controlId);
				MlnBufferDestroy(heap, obligation->data.agentId);
				MlnBufferDestroy(heap, obligation->contextId);
				if (obligation->parameters != NULL)
					MlnValueListFree(heap, obligation->parameters);
				break;
			case MLN_OBLIGATION_OUTPUT_CONTROL:
				MlnBufferDestroy(heap, obligation->controlId);
				MlnFree(heap, obligation->data.control);
				break;
			case MLN_OBLIGATION_METER_PLAY_DURATION:
				MlnBufferDestroy(heap, obligation->controlId);
				MlnBufferDestroy(heap, obligation->data.agentId);
				break;
			}
		}
		MlnFree(heap, info->obligations);
	}
	if (info->permissions != NULL) {
		for (i = 0; i < info->permissionCount; i++) {
			permission = &info->permissions[i];
			if (permission != NULL &&
			    permission->type == MLN_PERMISSION_OUTPUT_CONTROL_OVERRIDE) {
				MlnBufferDestroy(heap, permission->controlId);
				MlnFree(heap, permission->control);
			}
		}
		MlnFree(heap, info->permissions);
	}
	if (info->callbacks != NULL) {
		for (i = 0; i < info->callbackCount; i++) {
			callback = &info->callbacks[i];
			if (callback == NULL)
				continue;
			if (callback->type == MLN_CALLBACK_ON_TIME && callback->when.time != NULL)
				MlnFree(heap, callback->when.time);
			if (callback->routine != NULL)
				MlnBufferDestroy(heap, callback->routine);
		}
		MlnFree(heap, info->callbacks);
	}
	MlnBufferDestroy(heap, info->description);
	MlnBufferDestroy(heap, info->shortDescription);
	MlnBufferDestroy(heap, info->longDescription);
	ids = info->ids;
	if (ids != NULL) {
		for (i = 0; i < info->idCount; i++)
			MlnBufferDestroy(heap, ids[i]);
		MlnFree(heap, ids);
	}
	/* by pointer: the counts, not the arrays, bound the loops */
	times = info->notBefore;
	for (i = 0; i < info->notBeforeKind; i++)
		MlnFree(heap, times[i]);
	times = info->notAfter;
	for (i = 0; i < info->notAfterKind; i++)
		MlnFree(heap, times[i]);
	MlnFree(heap, info->notDuring[0]);
	MlnFree(heap, info->notDuring[1]);
	MlnBufferDestroy(heap, info->membership);
	MlnBufferDestroy(heap, info->identity);
	MlnBufferDestroy(heap, info->deviceType);
	MlnBufferDestroy(heap, info->deviceFeature);
	MlnBufferDestroy(heap, info->deviceId);
	MlnBufferDestroy(heap, info->custom.string);
	MlnFree(heap, info->custom.date);
	MlnBufferDestroy(heap, info->custom.resource);
	MlnBufferDestroy(heap, info->custom.text);
	MlnFree(heap, info);
	return MLN_OK;
}

/* a date (minutes since 1970) as a new broken-down time */
#define MLN_NEW_TIME(heap, to, minutes, fail)					\
	do {									\
		unsigned int minutes_ = (minutes);				\
		MlnDateTime *time_ = MlnAllocate((heap), sizeof(*time_));	\
		if (time_ == NULL) {						\
			MlnFree((heap), time_);					\
			goto fail;						\
		}								\
		MlnMemset(time_, 0, sizeof(*time_));				\
		MlnTimeFromMinutes(&minutes_, time_);				\
		(to) = time_;							\
		time_ = NULL;							\
		MlnFree((heap), time_);						\
	} while (0)

/* an item of a list as a parameter: a plain one, or an extended one and its flags */
#define MLN_ENTRY(item, entry, flags, other)					\
	do {									\
		if ((item)->type == MLN_VALUE_PARAMETER) {			\
			(entry) = (item)->data.parameter;			\
		} else if ((item)->type == MLN_VALUE_EXTENDED_PARAMETER) {	\
			(entry) = (MlnParameter *)(item)->data.extended;	\
			flags;							\
		} else {							\
			goto other;						\
		}							\
	} while (0)

/* a list value of these item types */
#define MLN_TYPE(list, i)	((list)->items[i]->type)

/*
 * What a control grants, from its evaluation's extended status block: the parameters the
 * block's flags say it carries (Obligations and Callbacks; the Generic, Temporal, Spatial,
 * Group, Device and Counter constraints), and Description and Permissions in any block.
 */
__attribute__((noinline)) int MlnRightsInfoParse(int heap, unsigned int size, const void *data,
						 MlnRightsInfo **out)
{
	MlnEsb *esb = NULL;
	MlnRightsInfo *info = NULL;
	MlnBuffer **ids, *converted;
	MlnValueList *list, *items, *copy;
	const MlnValueList *source;
	MlnParameter *entry, *sub;
	MlnValue *item, *value;
	MlnObligation *obligation;
	MlnCallback *callback;
	MlnPermission *permission;
	const MlnBuffer *feature, *event;
	unsigned int esbFlags, critical, subCritical, n;
	int count, subCount, nodes, i, j, k;
	int ret;

	if (size == 0 || out == NULL || data == NULL)
		return MLN_ERROR_INVALID_ARGUMENT;
	ret = MlnEsbDecode(heap, size, data, &esb);
	if (ret != MLN_OK)
		goto out;
	info = MlnAllocate(heap, sizeof(*info));
	if (info == NULL)
		goto no_memory;
	MlnMemset(info, 0, sizeof(*info));
	list = esb->parameters;
	count = list->count;
	esbFlags = esb->localFlags;
	critical = 0;
	for (i = 0; i < count; i++) {
		item = list->items[i];
		MLN_ENTRY(item, entry, critical = item->data.extended->flags, next);
		value = entry->value;

		if (MLN_PARAMETER_IS(entry, "Description") && value->type == MLN_VALUE_LIST) {
			items = value->data.list;
			subCount = items->count;
			for (j = 0; j < subCount; j++) {
				item = items->items[j];
				MLN_ENTRY(item, sub, , next_description);
				subCritical = item->type == MLN_VALUE_EXTENDED_PARAMETER ?
					item->data.extended->flags & 1 : 0;
				if (sub == NULL)
					continue;
				if (MLN_PARAMETER_IS(sub, "Default") &&
				    sub->value->type == MLN_VALUE_RESOURCE) {
					MLN_COPY_NAME(heap, info->description, sub->value->data.buffer,
						      no_memory);
					info->descriptionFlags |= 1;
				} else if (MLN_PARAMETER_IS(sub, "Short") &&
					   sub->value->type == MLN_VALUE_RESOURCE) {
					MLN_COPY_NAME(heap, info->shortDescription,
						      sub->value->data.buffer, no_memory);
					info->descriptionFlags |= 2;
				} else if (MLN_PARAMETER_IS(sub, "Long") &&
					   sub->value->type == MLN_VALUE_RESOURCE) {
					MLN_COPY_NAME(heap, info->longDescription,
						      sub->value->data.buffer, no_memory);
					info->descriptionFlags |= 4;
				} else if (subCritical) {
					info->flags |= 1;
				}
next_description:
				;
			}
		} else if (MLN_PARAMETER_IS(entry, "GenericConstraints") && (esbFlags & 0x04) &&
			   value->type == MLN_VALUE_LIST) {
			items = value->data.list;
			subCount = items->count;
			ids = NULL;
			if (subCount > 0) {
				nodes = 0;
				for (j = 0; j < subCount; j++) {
					item = items->items[j];
					MLN_ENTRY(item, sub, , next_generic);
					subCritical = item->type == MLN_VALUE_EXTENDED_PARAMETER ?
						item->data.extended->flags & 1 : 0;
					if (sub == NULL)
						continue;
					if (MLN_PARAMETER_IS(sub, "NodeReachabilityRequired") &&
					    sub->value->type == MLN_VALUE_STRING) {
						nodes++;
					} else if (MLN_PARAMETER_IS(sub, "RequiredLicenseStatus") &&
						   sub->value->type == MLN_VALUE_INTEGER) {
						info->requiredLicenseStatus = sub->value->data.integer;
						info->idFlags |= 2;
					} else if (subCritical) {
						info->flags |= 1;
					}
next_generic:
					;
				}
				if (nodes != 0) {
					ids = MlnAllocate(heap, nodes * sizeof(ids[0]));
					if (ids == NULL)
						goto free_ids;
					MlnMemset(ids, 0, nodes * sizeof(ids[0]));
					k = 0;
					for (j = 0; j < subCount; j++) {
						item = items->items[j];
						if (item->type != MLN_VALUE_PARAMETER &&
						    item->type != MLN_VALUE_EXTENDED_PARAMETER)
							continue;
						sub = item->data.parameter;
						if (sub == NULL ||
						    !MLN_PARAMETER_IS(sub, "NodeReachabilityRequired") ||
						    sub->value->type != MLN_VALUE_STRING)
							continue;
						MLN_COPY_NAME(heap, ids[k], sub->value->data.buffer,
							      free_ids);
						k++;
					}
					info->idCount = nodes;
					info->ids = ids;
					ids = NULL;
					info->idFlags |= 1;
				}
			}
			MlnFree(heap, ids);
		} else if (MLN_PARAMETER_IS(entry, "TemporalConstraints") && (esbFlags & 0x08) &&
			   value->type == MLN_VALUE_LIST) {
			items = value->data.list;
			subCount = items->count;
			for (j = 0; j < subCount; j++) {
				item = items->items[j];
				MLN_ENTRY(item, sub, , next_temporal);
				subCritical = item->type == MLN_VALUE_EXTENDED_PARAMETER ?
					item->data.extended->flags & 1 : 0;
				if (sub == NULL)
					continue;
				if (MLN_PARAMETER_IS(sub, "NotBefore") &&
				    sub->value->type == MLN_VALUE_DATE) {
					n = info->notBeforeKind;
					if (n > 1)
						goto unsupported;
					info->notBeforeKind = n + 1;
					MLN_NEW_TIME(heap, info->notBefore[n], sub->value->data.integer,
						     no_memory);
					info->constraintFlags |= MLN_RIGHTS_NOT_BEFORE;
				} else if (MLN_PARAMETER_IS(sub, "NotAfter") &&
					   sub->value->type == MLN_VALUE_DATE) {
					n = info->notAfterKind;
					if (n > 1)
						goto unsupported;
					info->notAfterKind = n + 1;
					MLN_NEW_TIME(heap, info->notAfter[n], sub->value->data.integer,
						     no_memory);
					info->constraintFlags |= MLN_RIGHTS_NOT_AFTER;
				} else if (MLN_PARAMETER_IS(sub, "NotDuring") &&
					   sub->value->type == MLN_VALUE_LIST) {
					source = sub->value->data.list;
					if (source->count != 2 || MLN_TYPE(source, 0) != MLN_VALUE_DATE ||
					    MLN_TYPE(source, 1) != MLN_VALUE_DATE)
						goto unsupported;
					MLN_NEW_TIME(heap, info->notDuring[0],
						     source->items[0]->data.integer, no_memory);
					MLN_NEW_TIME(heap, info->notDuring[1],
						     source->items[1]->data.integer, no_memory);
					info->constraintFlags |= MLN_RIGHTS_NOT_DURING;
				} else if (MLN_PARAMETER_IS(sub, "NotLongerThan") &&
					   sub->value->type == MLN_VALUE_INTEGER) {
					info->notLongerThan = sub->value->data.integer;
					info->constraintFlags |= MLN_RIGHTS_NOT_LONGER_THAN;
				} else if (MLN_PARAMETER_IS(sub, "NotMoreThan") &&
					   sub->value->type == MLN_VALUE_INTEGER) {
					info->notMoreThan = sub->value->data.integer;
					info->constraintFlags |= MLN_RIGHTS_NOT_MORE_THAN;
				} else if (subCritical) {
					info->flags |= 1;
				}
next_temporal:
				;
			}
		} else if (MLN_PARAMETER_IS(entry, "SpatialConstraints") && (esbFlags & 0x10) &&
			   value->type == MLN_VALUE_LIST) {
			/* nothing is kept of them */
		} else if (MLN_PARAMETER_IS(entry, "GroupConstraints") && (esbFlags & 0x20) &&
			   value->type == MLN_VALUE_LIST) {
			items = value->data.list;
			subCount = items->count;
			for (j = 0; j < subCount; j++) {
				item = items->items[j];
				MLN_ENTRY(item, sub, , next_group);
				subCritical = item->type == MLN_VALUE_EXTENDED_PARAMETER ?
					item->data.extended->flags & 1 : 0;
				if (sub == NULL)
					continue;
				if (MLN_PARAMETER_IS(sub, "MembershipRequired") &&
				    sub->value->type == MLN_VALUE_RESOURCE) {
					MLN_COPY_NAME(heap, info->membership, sub->value->data.buffer,
						      no_memory);
					info->groupFlags |= 1;
				} else if (MLN_PARAMETER_IS(sub, "IdentityRequired") &&
					   sub->value->type == MLN_VALUE_RESOURCE) {
					MLN_COPY_NAME(heap, info->identity, sub->value->data.buffer,
						      no_memory);
					info->groupFlags |= 2;
				} else if (subCritical) {
					info->flags |= 1;
				}
next_group:
				;
			}
		} else if (MLN_PARAMETER_IS(entry, "DeviceConstraints") && (esbFlags & 0x40) &&
			   value->type == MLN_VALUE_LIST) {
			items = value->data.list;
			subCount = items->count;
			for (j = 0; j < subCount; j++) {
				item = items->items[j];
				MLN_ENTRY(item, sub, , next_device);
				subCritical = item->type == MLN_VALUE_EXTENDED_PARAMETER ?
					item->data.extended->flags & 1 : 0;
				if (sub == NULL)
					continue;
				if (MLN_PARAMETER_IS(sub, "DeviceTypeRequired") &&
				    sub->value->type == MLN_VALUE_RESOURCE) {
					MLN_COPY_NAME(heap, info->deviceType, sub->value->data.buffer,
						      no_memory);
					info->deviceFlags |= MLN_DEVICE_TYPE;
				} else if (MLN_PARAMETER_IS(sub, "DeviceFeatureRequired") &&
					   sub->value->type == MLN_VALUE_RESOURCE) {
					feature = sub->value->data.buffer;
					if (feature != NULL) {
						if (feature->size == 59 && MlnMemcmp(feature->data,
						    "urn:marlin:broadband:client:capabilities:license-suspension",
						    59) == 0) {
							info->deviceFlags |= MLN_DEVICE_LICENSE_SUSPENSION;
							continue;
						}
						if (feature->size == 60 && MlnMemcmp(feature->data,
						    "urn:marlin:broadband:client:capabilities:meter-play-duration",
						    60) == 0) {
							info->deviceFlags |= MLN_DEVICE_METER_PLAY_DURATION;
							continue;
						}
						if (feature->size == 48 && MlnMemcmp(feature->data,
						    "urn:marlin:core:client:capabilities:trusted-time",
						    48) == 0) {
							info->deviceFlags |= MLN_DEVICE_TRUSTED_TIME;
							continue;
						}
					}
					MLN_COPY_NAME(heap, info->deviceFeature, feature, no_memory);
					info->deviceFlags |= MLN_DEVICE_FEATURE;
				} else if (MLN_PARAMETER_IS(sub, "DeviceIdRequired") &&
					   sub->value->type == MLN_VALUE_STRING) {
					MLN_COPY_NAME(heap, info->deviceId, sub->value->data.buffer,
						      no_memory);
					info->deviceFlags |= MLN_DEVICE_ID;
				} else if (MLN_PARAMETER_IS(sub, "urn:marlin:core:version-major") &&
					   sub->value->type == MLN_VALUE_INTEGER) {
					info->versionMajor = sub->value->data.integer;
					info->deviceFlags |= MLN_DEVICE_VERSION_MAJOR;
				} else if (MLN_PARAMETER_IS(sub, "urn:marlin:core:version-minor") &&
					   sub->value->type == MLN_VALUE_INTEGER) {
					info->versionMinor = sub->value->data.integer;
					info->deviceFlags |= MLN_DEVICE_VERSION_MINOR;
				} else if (MLN_PARAMETER_IS(sub,
					   "urn:marlin:broadband:control:constraint:dcs-freshness-interval") &&
					   sub->value->type == MLN_VALUE_INTEGER) {
					info->dcsFreshnessInterval = sub->value->data.integer;
					info->deviceFlags |= MLN_DEVICE_DCS_FRESHNESS;
				} else if (subCritical) {
					info->flags |= 1;
				}
next_device:
				;
			}
		} else if (MLN_PARAMETER_IS(entry, "CounterConstraints") && (esbFlags & 0x80) &&
			   value->type == MLN_VALUE_LIST) {
			items = value->data.list;
			if (items == NULL)
				goto invalid;
			for (j = 0; j < items->count; j++) {
				item = items->items[j];
				if (item->type != MLN_VALUE_PARAMETER &&
				    item->type != MLN_VALUE_EXTENDED_PARAMETER)
					continue;
				sub = item->data.parameter;
				if (sub == NULL)
					continue;
				if (sub->name == NULL || sub->value == NULL)
					goto invalid;
				if (sub->name->size != 11 ||
				    MlnMemcmp(sub->name->data, "RepeatCount", 11) != 0 ||
				    sub->value->type != MLN_VALUE_LIST)
					continue;
				source = sub->value->data.list;
				if (source == NULL || source->count != 2 || source->items[0] == NULL ||
				    source->items[1] == NULL || MLN_TYPE(source, 0) != MLN_VALUE_INTEGER ||
				    MLN_TYPE(source, 1) != MLN_VALUE_INTEGER)
					goto invalid;
				info->repeatCount[0] = source->items[0]->data.integer;
				info->repeatCount[1] = source->items[1]->data.integer;
				info->counterFlags |= 1;
				break;
			}
		} else if (MLN_PARAMETER_IS(entry, "Obligations") && (esbFlags & 0x01) &&
			   value->type == MLN_VALUE_LIST) {
			items = value->data.list;
			converted = NULL;
			if (items == NULL)
				goto invalid;
			subCount = items->count;
			info->obligationCount = 0;
			if (subCount <= 0)
				goto next;
			info->obligations = MlnAllocate(heap, subCount * sizeof(MlnObligation));
			if (info->obligations == NULL)
				goto no_memory;
			info->obligationCount = subCount;
			MlnMemset(info->obligations, 0, subCount * sizeof(MlnObligation));
			for (j = 0; j < subCount; j++) {
				item = items->items[j];
				obligation = &info->obligations[j];
				MLN_ENTRY(item, sub, obligation->flags = critical |
					  item->data.extended->flags, next_obligation);
				if (MLN_PARAMETER_IS(sub, "RunAgentOnPeer") &&
				    sub->value->type == MLN_VALUE_LIST) {
					source = sub->value->data.list;
					if (source->count != 5 || MLN_TYPE(source, 0) != MLN_VALUE_STRING ||
					    MLN_TYPE(source, 1) != MLN_VALUE_STRING ||
					    MLN_TYPE(source, 2) != MLN_VALUE_INTEGER ||
					    MLN_TYPE(source, 3) != MLN_VALUE_STRING ||
					    MLN_TYPE(source, 4) != MLN_VALUE_LIST)
						goto unsupported;
					obligation->type = MLN_OBLIGATION_RUN_AGENT_ON_PEER;
					MLN_COPY_NAME(heap, obligation->controlId,
						      source->items[0]->data.buffer, no_memory);
					MLN_COPY_NAME(heap, obligation->data.agentId,
						      source->items[1]->data.buffer, no_memory);
					obligation->instanceId = source->items[2]->data.integer;
					MLN_COPY_NAME(heap, obligation->contextId,
						      source->items[3]->data.buffer, no_memory);
					source = source->items[4]->data.list;
					if (source != NULL) {
						copy = MlnAllocate(heap, (source->count + 1) *
								   sizeof(copy->items[0]));
						if (copy == NULL) {
							ret = MLN_ERROR_NO_MEMORY;
							goto free_copy;
						}
						MlnMemset(copy->items, 0,
							  source->count * sizeof(copy->items[0]));
						copy->count = source->count;
						for (k = 0; k < source->count; k++) {
							ret = MlnValueCopy(heap, source->items[k],
									   &copy->items[k]);
							if (ret != MLN_OK)
								goto free_copy;
						}
					} else {
						copy = NULL;
					}
					obligation->parameters = copy;
					copy = NULL;
					MlnValueListFree(heap, copy);
				} else if (MLN_PARAMETER_IS(sub,
					   "urn:marlin:broadband:obligation:meter-play-duration") &&
					   sub->value->type == MLN_VALUE_LIST) {
					source = sub->value->data.list;
					if (source->count != 2 || MLN_TYPE(source, 0) != MLN_VALUE_STRING ||
					    MLN_TYPE(source, 1) != MLN_VALUE_STRING)
						goto unsupported;
					obligation->type = MLN_OBLIGATION_METER_PLAY_DURATION;
					MLN_COPY_NAME(heap, obligation->controlId,
						      source->items[0]->data.buffer, no_memory);
					MLN_COPY_NAME(heap, obligation->data.agentId,
						      source->items[1]->data.buffer, no_memory);
				} else if (MLN_PARAMETER_IS(sub, "OutputControl") &&
					   sub->value->type == MLN_VALUE_LIST) {
					source = sub->value->data.list;
					if (source->count != 2 || MLN_TYPE(source, 0) != MLN_VALUE_STRING ||
					    MLN_TYPE(source, 1) != MLN_VALUE_LIST)
						goto unsupported;
					obligation->type = MLN_OBLIGATION_OUTPUT_CONTROL;
					MLN_COPY_NAME(heap, obligation->controlId,
						      source->items[0]->data.buffer, no_memory);
					ret = MlnOutputControlParse(heap, source->items[1]->data.list,
								    obligation->controlId,
								    obligation->flags & 1, &info->flags,
								    &obligation->data.control);
					if (ret != MLN_OK)
						goto out;
				} else if (MLN_PARAMETER_IS(sub, "Custom") &&
					   sub->value->type == MLN_VALUE_LIST) {
					source = sub->value->data.list;
					if (source->count != 6 || MLN_TYPE(source, 0) != MLN_VALUE_INTEGER ||
					    MLN_TYPE(source, 1) != MLN_VALUE_REAL ||
					    MLN_TYPE(source, 2) != MLN_VALUE_STRING ||
					    MLN_TYPE(source, 3) != MLN_VALUE_DATE ||
					    MLN_TYPE(source, 4) != MLN_VALUE_RESOURCE ||
					    MLN_TYPE(source, 5) != MLN_VALUE_BYTE_ARRAY)
						goto unsupported;
					obligation->type = MLN_OBLIGATION_CUSTOM;
					info->custom.integer = source->items[0]->data.integer;
					info->custom.real = source->items[1]->data.integer;
					MLN_COPY_NAME(heap, info->custom.string,
						      source->items[2]->data.buffer, no_memory);
					MLN_NEW_TIME(heap, info->custom.date,
						     source->items[3]->data.integer, no_memory);
					MLN_COPY_NAME(heap, info->custom.resource,
						      source->items[4]->data.buffer, no_memory);
					ret = MlnImpl_81026814(heap, source->items[5]->data.buffer, NULL,
							       NULL, &converted);
					if (ret != MLN_OK)
						goto out;
					MLN_COPY_NAME(heap, info->custom.text, converted, no_memory);
					MlnBufferDestroy(heap, converted);
				} else if (obligation->flags & 1) {
					info->flags |= 1;
				}
next_obligation:
				;
			}
		} else if (MLN_PARAMETER_IS(entry, "Callbacks") && (esbFlags & 0x02) &&
			   value->type == MLN_VALUE_LIST) {
			items = value->data.list;
			subCount = items->count;
			info->callbackCount = 0;
			if (subCount <= 0)
				goto next;
			info->callbacks = MlnAllocate(heap, subCount * sizeof(MlnCallback));
			if (info->callbacks == NULL)
				goto no_memory;
			info->callbackCount = subCount;
			MlnMemset(info->callbacks, 0, subCount * sizeof(MlnCallback));
			for (j = 0; j < subCount; j++) {
				item = items->items[j];
				callback = &info->callbacks[j];
				MLN_ENTRY(item, sub, callback->flags = critical |
					  item->data.extended->flags, next_callback);
				source = sub->value->data.list;
				if (MLN_PARAMETER_IS(sub, "OnAccept") &&
				    sub->value->type == MLN_VALUE_LIST) {
					callback->type = MLN_CALLBACK_ON_ACCEPT;
				} else if (MLN_PARAMETER_IS(sub, "OnTime") &&
					   sub->value->type == MLN_VALUE_LIST) {
					if (source->count != 2 || MLN_TYPE(source, 0) != MLN_VALUE_DATE)
						goto unsupported;
					callback->type = MLN_CALLBACK_ON_TIME;
					MLN_NEW_TIME(heap, callback->when.time,
						     source->items[0]->data.integer, no_memory);
					source = source->items[1]->data.list;
				} else if (MLN_PARAMETER_IS(sub, "OnTimeElapsed") &&
					   sub->value->type == MLN_VALUE_LIST) {
					if (source->count != 2 || MLN_TYPE(source, 0) != MLN_VALUE_INTEGER)
						goto unsupported;
					callback->type = MLN_CALLBACK_ON_TIME_ELAPSED;
					callback->when.value = source->items[0]->data.integer;
					source = source->items[1]->data.list;
				} else if (MLN_PARAMETER_IS(sub, "OnEvent") &&
					   sub->value->type == MLN_VALUE_LIST) {
					if (source->count != 4 || MLN_TYPE(source, 0) != MLN_VALUE_STRING ||
					    MLN_TYPE(source, 1) != MLN_VALUE_INTEGER ||
					    MLN_TYPE(source, 2) != MLN_VALUE_INTEGER)
						goto unsupported;
					callback->type = MLN_CALLBACK_ON_EVENT;
					event = source->items[0]->data.buffer;
					/* Sony's tests OnPlay twice, around OnStop */
					if (event != NULL && event->size == 6 &&
					    MlnMemcmp(event->data, "OnPlay", 6) == 0)
						callback->when.value = MLN_CALLBACK_EVENT_PLAY;
					else if (event != NULL && event->size == 6 &&
						 MlnMemcmp(event->data, "OnStop", 6) == 0)
						callback->when.value = MLN_CALLBACK_EVENT_STOP;
					else if (event != NULL && event->size == 6 &&
						 MlnMemcmp(event->data, "OnPlay", 6) == 0)
						callback->when.value = MLN_CALLBACK_EVENT_PLAY;
					else if (event != NULL && event->size == 10 &&
						 MlnMemcmp(event->data, "OnTimecode", 10) == 0)
						callback->when.value = MLN_CALLBACK_EVENT_TIMECODE;
					else if (event != NULL && event->size == 6 &&
						 MlnMemcmp(event->data, "OnSeek", 6) == 0)
						callback->when.value = MLN_CALLBACK_EVENT_SEEK;
					else if (callback->flags & 1)
						info->flags |= 1;
					callback->eventStart = source->items[1]->data.integer;
					callback->eventEnd = source->items[2]->data.integer;
					source = source->items[3]->data.list;
				} else if (MLN_PARAMETER_IS(sub, "OnAgentCompletion") &&
					   sub->value->type == MLN_VALUE_LIST) {
					if (source->count != 2 || MLN_TYPE(source, 0) != MLN_VALUE_INTEGER)
						goto unsupported;
					callback->type = MLN_CALLBACK_ON_AGENT_COMPLETION;
					callback->when.value = source->items[0]->data.integer;
					source = source->items[1]->data.list;
				} else {
					if (callback->flags & 1)
						info->flags |= 1;
					continue;
				}

				/* the routine to call: its type, name and cookie */
				if (callback == NULL || source == NULL || source->count != 3 ||
				    MLN_TYPE(source, 0) != MLN_VALUE_INTEGER ||
				    MLN_TYPE(source, 1) != MLN_VALUE_STRING ||
				    MLN_TYPE(source, 2) != MLN_VALUE_INTEGER)
					goto unsupported;
				callback->routineType = source->items[0]->data.integer;
				MLN_COPY_NAME(heap, callback->routine, source->items[1]->data.buffer,
					      unsupported);
				callback->cookie = source->items[2]->data.integer;
next_callback:
				;
			}
		} else if (MLN_PARAMETER_IS(entry, "Permissions") &&
			   value->type == MLN_VALUE_LIST) {
			items = value->data.list;
			if (items == NULL)
				goto invalid;
			subCount = items->count;
			info->permissionCount = 0;
			if (subCount <= 0)
				goto next;
			info->permissions = MlnAllocate(heap, subCount * sizeof(MlnPermission));
			if (info->permissions == NULL)
				goto no_memory;
			info->permissionCount = subCount;
			MlnMemset(info->permissions, 0, subCount * sizeof(MlnPermission));
			for (j = 0; j < subCount; j++) {
				item = items->items[j];
				permission = &info->permissions[j];
				MLN_ENTRY(item, sub, permission->flags = critical |
					  item->data.extended->flags, next_permission);
				if (MLN_PARAMETER_IS(sub, "OutputControlOverride") &&
				    sub->value->type == MLN_VALUE_LIST) {
					source = sub->value->data.list;
					if (source->count != 2 || MLN_TYPE(source, 0) != MLN_VALUE_STRING ||
					    MLN_TYPE(source, 1) != MLN_VALUE_LIST)
						goto unsupported;
					MLN_COPY_NAME(heap, permission->controlId,
						      source->items[0]->data.buffer, no_memory);
					ret = MlnOutputControlParse(heap, source->items[1]->data.list,
								    permission->controlId,
								    permission->flags & 1, &info->flags,
								    &permission->control);
					if (ret != MLN_OK)
						goto out;
					permission->type = MLN_PERMISSION_OUTPUT_CONTROL_OVERRIDE;
				} else if (permission->flags & 1) {
					info->flags |= 1;
				}
next_permission:
				;
			}
		}
next:
		;
	}
	*out = info;
	info = NULL;
	ret = MLN_OK;
	goto out;

free_copy:
	MlnValueListFree(heap, copy);
	goto out;
free_ids:
	MlnFree(heap, ids);
	goto no_memory;
invalid:
	ret = MLN_ERROR_INVALID_ARGUMENT;
	goto out;
unsupported:
	ret = MLN_ERROR_UNSUPPORTED;
	goto out;
no_memory:
	ret = MLN_ERROR_NO_MEMORY;
out:
	if (esb != NULL)
		MlnEsbFree(heap, esb);
	if (info != NULL)
		MlnRightsInfoFree(heap, info);
	return ret;
}

/* a status block's global flags: 0, or what this library cannot act on */
__attribute__((noinline)) int MlnEsbCheckGlobalFlags(int heap, const MlnEsb *esb)
{
	if (esb == NULL)
		return MLN_ERROR_INVALID_ARGUMENT;
	if (esb->globalFlags & MLN_ESB_GLOBAL_UNSUPPORTED)
		return MLN_ERROR_UNSUPPORTED;
	return MLN_OK;
}

static inline __attribute__((always_inline)) void MlnValueListFreeInline(int heap,
									 MlnValueList *list)
{
	int count, i;

	if (list == NULL)
		return;
	count = list->count;
	for (i = 0; i < count; i++) {
		if (list->items[i] != NULL)
			MlnValueFree(heap, list->items[i]);
	}
	MlnFree(heap, list);
}

/* a parameter or an extended parameter: they begin with the same name and value */
static inline __attribute__((always_inline)) void MlnParameterFreeInline(int heap,
									 MlnParameter *parameter)
{
	if (parameter == NULL)
		return;
	if (parameter->name != NULL)
		MlnBufferDestroy(heap, parameter->name);
	if (parameter->value != NULL)
		MlnValueFree(heap, parameter->value);
	MlnFree(heap, parameter);
}

__attribute__((noinline)) int MlnValueFree(int heap, MlnValue *value)
{
	if (value == NULL)
		return MLN_OK;
	switch (value->type) {
	case MLN_VALUE_STRING:
		if (value->data.buffer != NULL)
			MlnBufferDestroy(heap, value->data.buffer);
		break;
	case MLN_VALUE_PARAMETER:
		MlnParameterFreeInline(heap, value->data.parameter);
		break;
	case MLN_VALUE_EXTENDED_PARAMETER:
		MlnParameterFreeInline(heap, (MlnParameter *)value->data.extended);
		break;
	case MLN_VALUE_RESOURCE:
		if (value->data.buffer != NULL)
			MlnBufferDestroy(heap, value->data.buffer);
		break;
	case MLN_VALUE_BYTE_ARRAY:
		if (value->data.buffer != NULL)
			MlnBufferDestroy(heap, value->data.buffer);
		break;
	case MLN_VALUE_LIST:
		MlnValueListFreeInline(heap, value->data.list);
		break;
	}
	MlnFree(heap, value);
	return MLN_OK;
}

__attribute__((noinline)) int MlnValueListFree(int heap, MlnValueList *list)
{
	MlnValueListFreeInline(heap, list);
	return MLN_OK;
}

__attribute__((noinline)) int MlnExtendedParameterFree(int heap, MlnExtendedParameter *extended)
{
	MlnParameterFreeInline(heap, (MlnParameter *)extended);
	return MLN_OK;
}

__attribute__((noinline)) int MlnParameterFree(int heap, MlnParameter *parameter)
{
	MlnParameterFreeInline(heap, parameter);
	return MLN_OK;
}

/* The encoding is big-endian: a word for each value's type and size, then its data. */
#define MLN_ESB_WORD(p)		((unsigned int)((const unsigned char *)(p))[0] << 24 |	\
				 (unsigned int)((const unsigned char *)(p))[1] << 16 |	\
				 (unsigned int)((const unsigned char *)(p))[2] << 8 |	\
				 (unsigned int)((const unsigned char *)(p))[3])

/* a list: its count, then each value */
__attribute__((noinline)) int MlnValueListDecode(int heap, const unsigned int **cursor,
						 MlnValueList **out)
{
	const unsigned int *p = *cursor;
	MlnValueList *list;
	int count, i;
	int ret;

	count = MLN_ESB_WORD(p);
	list = MlnAllocate(heap, (count + 1) * sizeof(list->items[0]));
	if (list == NULL)
		return MLN_ERROR_NO_MEMORY;
	list->count = count;
	MlnMemset(list->items, 0, count * sizeof(list->items[0]));
	*cursor = p + 1;
	for (i = 0; i < count; i++) {
		ret = MlnValueDecode(heap, cursor, &list->items[i]);
		if (ret != MLN_OK) {
			MlnValueListFreeInline(heap, list);
			return ret;
		}
	}
	*out = list;
	return MLN_OK;
}

/* a value: integers, reals and dates of 4 bytes, strings and resources without their NUL */
__attribute__((noinline)) int MlnValueDecode(int heap, const unsigned int **cursor, MlnValue **out)
{
	const unsigned int *p = *cursor, *start, *name;
	MlnParameter *parameter;
	MlnExtendedParameter *extended;
	MlnValue *value;
	unsigned int type, size, nameSize;
	int ret;

	value = MlnAllocate(heap, sizeof(*value));
	if (value == NULL)
		return MLN_ERROR_NO_MEMORY;
	MlnMemset(value, 0, sizeof(*value));
	start = p;
	type = MLN_ESB_WORD(p);
	value->type = type;
	size = MLN_ESB_WORD(p + 1);
	p += 2;
	switch (type) {
	case MLN_VALUE_INTEGER:
	case MLN_VALUE_DATE:
	case MLN_VALUE_REAL:
		if (size != 4)
			goto format;
		value->data.integer = MLN_ESB_WORD(start + 2);
		p = start + 3;
		break;
	case MLN_VALUE_STRING:
	case MLN_VALUE_RESOURCE:
		value->data.buffer = MlnBufferCreate(heap, size != 0 ? size - 1 : 0,
						     size != 0 ? p : NULL);
		if (value->data.buffer == NULL)
			goto no_memory;
		p = (const unsigned int *)((const char *)p + size);
		break;
	case MLN_VALUE_BYTE_ARRAY:
		value->data.buffer = MlnBufferCreate(heap, size, size != 0 ? p : NULL);
		if (value->data.buffer == NULL)
			goto no_memory;
		p = (const unsigned int *)((const char *)p + size);
		break;
	case MLN_VALUE_PARAMETER:
		parameter = MlnAllocate(heap, sizeof(*parameter));
		if (parameter == NULL)
			goto no_memory;
		parameter->name = NULL;
		parameter->value = NULL;
		name = start + 3;
		nameSize = MLN_ESB_WORD(start + 2);
		goto name;
	case MLN_VALUE_EXTENDED_PARAMETER:
		extended = MlnAllocate(heap, sizeof(*extended));
		parameter = (MlnParameter *)extended;
		if (extended == NULL)
			goto no_memory;
		extended->name = NULL;
		extended->value = NULL;
		extended->flags = MLN_ESB_WORD(start + 2);
		name = start + 4;
		nameSize = MLN_ESB_WORD(start + 3);
name:
		parameter->name = MlnBufferCreate(heap, nameSize - 1, name);
		if (parameter->name == NULL) {
			ret = MLN_ERROR_NO_MEMORY;
		} else {
			p = (const unsigned int *)((const char *)name + nameSize);
			ret = MlnValueDecode(heap, &p, &parameter->value);
			if (ret == MLN_OK) {
				value->data.parameter = parameter;
				break;
			}
		}
		MlnParameterFreeInline(heap, parameter);
		goto fail;
	case MLN_VALUE_LIST:
		ret = MlnValueListDecode(heap, &p, &value->data.list);
		if (ret != MLN_OK)
			goto fail;
		break;
	default:
format:
		ret = MLN_ERROR_FORMAT;
		goto fail;
	}
	*cursor = p;
	*out = value;
	return MLN_OK;

no_memory:
	ret = MLN_ERROR_NO_MEMORY;
fail:
	MlnValueFree(heap, value);
	return ret;
}

/* a status block: its six header words, then its parameters */
__attribute__((noinline)) int MlnEsbDecode(int heap, unsigned int size, const void *data,
					   MlnEsb **out)
{
	const unsigned int *p = data;
	MlnEsb *esb;
	int ret;

	esb = MlnAllocate(heap, sizeof(*esb));
	if (esb == NULL)
		return MLN_ERROR_NO_MEMORY;
	MlnMemset(esb, 0, sizeof(*esb));
	esb->globalFlags = MLN_ESB_WORD(p);
	esb->category = MLN_ESB_WORD(p + 1);
	esb->subCategory = MLN_ESB_WORD(p + 2);
	esb->localFlags = MLN_ESB_WORD(p + 3);
	esb->cacheDurationType = MLN_ESB_WORD(p + 4);
	esb->cacheDurationValue = MLN_ESB_WORD(p + 5);
	p += 6;
	ret = MlnValueListDecode(heap, &p, &esb->parameters);
	if (ret != MLN_OK) {
		MlnValueListFreeInline(heap, esb->parameters);
		MlnFree(heap, esb);
	} else {
		*out = esb;
	}
	return ret;
}

__attribute__((noinline)) int MlnEsbFree(int heap, MlnEsb *esb)
{
	if (esb == NULL)
		return MLN_OK;
	MlnValueListFreeInline(heap, esb->parameters);
	MlnFree(heap, esb);
	return MLN_OK;
}
