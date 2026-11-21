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
