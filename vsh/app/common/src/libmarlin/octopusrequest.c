/*
 * The DRM core's command dispatch: a parsed request's handler called and its reply written
 * as the response. The file's name is ours: Sony's binary calls the handlers and the error
 * map 0x8102C64C whole here, so this was another file than theirs.
 */
#include "mln_internal.h"

/* a parsed request handled by its command's handler, and the handler's reply written
 * as the response */
int MlnImpl_81031A40(unsigned int command, MlnServiceReply *reply, MlnBuffer **response,
		     MlnBuffer **result)
{
	int status = 0;
	MlnServiceArgs *args = NULL;
	int handle;
	int err, ret;

	if (response == NULL || reply == NULL) {
		ret = 4;
		goto out;
	}
	switch (command) {
	case 1024:
		ret = MlnImpl_8102C664(reply, &args, &status);
		if (ret != 0)
			goto out;
		ret = MlnImpl_810273C4(MLN_HEAP_GENERAL, 1024, status, args, response);
		if (status != 0)
			break;
		if (ret == 0)
			goto done;
		err = MlnImpl_8102701C(MLN_HEAP_GENERAL, args->items, &handle);
		if (err != 0) {
			ret = MlnImpl_8102C64C(err);
			goto out;
		}
		MlnImpl_8104A9CC(handle);
		goto done;
	case 1025:
		ret = MlnImpl_8102C77C(reply, &args, &status);
		break;
#ifndef MLN_PLAYBACK
	case 1026:
		ret = MlnImpl_8102C7FC(reply, &args, &status);
		break;
	case 1029:
		ret = MlnImpl_8102C870(reply, &args, &status);
		break;
#endif
	case 1030:
		ret = MlnImpl_8102C95C(reply, &args, &status);
		break;
#ifndef MLN_PLAYBACK
	case 1031:
		ret = MlnImpl_8102CAAC(reply, &args, &status);
		break;
	case 1032:
		ret = MlnImpl_8102CBF8(reply, &args, &status);
		break;
	case 1033:
		ret = MlnImpl_8102F9A0(1033, reply, &args, &status);
		break;
	case 1034:
		ret = MlnImpl_8102CCA4(reply, &args, &status);
		break;
	case 1035:
		ret = MlnImpl_8102CDB4(reply, &args, &status);
		break;
	case 1037:
		ret = MlnImpl_8102CEEC(reply, &args, &status, result);
		break;
	case 1038:
		ret = MlnImpl_8102CFB0(reply, &args, &status);
		break;
	case 1039:
		ret = MlnImpl_8102F9A0(1039, reply, &args, &status);
		break;
	case 1040:
		ret = MlnImpl_8102D05C(reply, &args, &status, result);
		break;
#endif
	case 1041:
		ret = MlnImpl_8102D138(reply, &args, &status);
		break;
#ifndef MLN_PLAYBACK
	case 1042:
		ret = MlnImpl_8102D1D4(reply, &args, &status, result);
		break;
	case 1050:
		ret = MlnImpl_8102D240(reply, &args, &status);
		if (ret != 0)
			goto out;
		ret = MlnImpl_810273C4(MLN_HEAP_GENERAL, 1050, status, args, response);
		if (status != 0)
			break;
		if (ret == 0)
			goto done;
		err = MlnImpl_8102701C(MLN_HEAP_GENERAL, args->items, &handle);
		if (err != 0) {
			ret = MlnImpl_8102C64C(err);
			goto out;
		}
		MlnImpl_8104DAA4(handle);
		goto done;
	case 1051:
		ret = MlnImpl_8102D454(reply, &args, &status);
		break;
	case 1052:
		ret = MlnImpl_8102D4D4(reply, &args, &status);
		break;
	case 1053:
		ret = MlnImpl_8102D6F8(reply, &args, &status, result);
		break;
	case 1054:
		ret = MlnImpl_8102D8B8(reply, &args, &status, result);
		break;
	case 1055:
		ret = MlnImpl_8102DB4C(reply, &args, &status, result);
		break;
	case 1056:
		ret = MlnImpl_8102DD64(reply, &args, &status, result);
		break;
	case 1057:
		ret = MlnImpl_8102DFC8(reply, &args, &status, result);
		break;
	case 1058:
		ret = MlnImpl_8102E188(reply, &args, &status, result);
		break;
	case 1059:
		ret = MlnImpl_8102E468(reply, &args, &status, result);
		break;
	case 1061:
		ret = MlnImpl_8102E628(reply, &args, &status);
		break;
	case 1062:
		ret = MlnImpl_8102E8A0(reply, &args, &status);
		break;
#endif
	case 1065:
		ret = MlnImpl_8102EBD0(reply, &args, &status, result);
		break;
	case 1066:
		ret = MlnImpl_8102ED08(reply, &args, &status);
		if (ret != 0)
			goto out;
		ret = MlnImpl_810273C4(MLN_HEAP_GENERAL, 1066, status, args, response);
		if (status != 0)
			break;
		if (ret == 0)
			goto done;
		err = MlnImpl_8102701C(MLN_HEAP_GENERAL, args->items, &handle);
		if (err != 0) {
			ret = MlnImpl_8102C64C(err);
			goto out;
		}
		MlnImpl_8104B1C8(handle);
		goto done;
	case 1067:
		ret = MlnImpl_8102EED4(reply, &args, &status);
		break;
	case 1068:
		ret = MlnImpl_8102EF48(1068, reply, &args, &status, result);
		break;
	case 1069:
		ret = MlnImpl_8102EF48(1069, reply, &args, &status, result);
		break;
	case 1070:
		ret = MlnImpl_8102EF48(1070, reply, &args, &status, result);
		break;
#ifndef MLN_PLAYBACK
	case 1071:
		ret = MlnImpl_8102F1B0(reply, &args, &status, result);
		break;
#endif
	case 1072:
		ret = MlnImpl_8102F408(reply, &args, &status, result);
		break;
	case 1073:
		ret = MlnImpl_8102F5E4(reply, &args, &status);
		break;
	case 1074:
		ret = MlnImpl_8102F83C(reply, &args, &status);
		break;
#ifndef MLN_PLAYBACK
	case 1089:
		ret = MlnImpl_8102F9A0(1089, reply, &args, &status);
		break;
	case 1090:
		ret = MlnImpl_8102FE68(reply, &args, &status);
		if (ret != 0)
			goto out;
		ret = MlnImpl_810273C4(MLN_HEAP_GENERAL, 1090, status, args, response);
		if (status != 0)
			break;
		if (ret == 0)
			goto done;
		err = MlnImpl_8102701C(MLN_HEAP_GENERAL, args->items, &handle);
		if (err != 0) {
			ret = MlnImpl_8102C64C(err);
			goto out;
		}
		MlnImpl_810505B8(handle);
		goto done;
	case 1091:
		ret = MlnImpl_81030648(reply, &args, &status);
		break;
	case 1092:
		ret = MlnImpl_810306C8(reply, &args, &status);
		break;
	case 1093:
		ret = MlnImpl_81030858(reply, &args, &status, result);
		break;
	case 1094:
		ret = MlnImpl_81030A24(reply, &args, &status, result);
		break;
	case 1095:
		ret = MlnImpl_81030BD0(reply, &args, &status, result);
		break;
	case 1096:
		ret = MlnImpl_81030D90(reply, &args, &status, result);
		break;
	case 1097:
		ret = MlnImpl_8102FA24(reply, &args, &status);
		if (ret != 0)
			goto out;
		ret = MlnImpl_810273C4(MLN_HEAP_GENERAL, 1097, status, args, response);
		if (status != 0)
			break;
		if (ret == 0)
			goto done;
		MlnImpl_81052608();
		goto out;
	case 1098:
		ret = MlnImpl_8102FA70(reply, &args, &status);
		break;
	case 1099:
		ret = MlnImpl_8102FAAC(reply, &args, &status);
		break;
	case 1100:
		ret = MlnImpl_8102FC18(reply, &args, &status, result);
		break;
#endif
	case 1105:
		ret = MlnImpl_81030FA8(reply, &args, &status);
		break;
	case 1106:
		ret = MlnImpl_8103110C(reply, &args, &status, result);
		break;
	case 1107:
		ret = MlnImpl_810316BC(reply, &args, &status, result);
		break;
	case 1108:
		ret = MlnImpl_8103149C(reply, &args, &status, result);
		break;
#ifndef MLN_PLAYBACK
	case 1109:
		ret = MlnImpl_8102E7A8(reply, &args, &status);
		break;
#endif
	case 1110:
		ret = MlnImpl_81031288(reply, &args, &status);
		break;
#ifndef MLN_PLAYBACK
	case 1111:
		ret = MlnImpl_8102FCCC(reply, &args, &status);
		break;
	case 1112:
		ret = MlnImpl_8102FDB4(reply, &args, &status, result);
		break;
#endif
	case 1113:
		ret = MlnImpl_810317AC(reply, &args, &status);
		break;
	default:
		ret = 6;
		goto out;
	}
	if (ret != 0)
		goto out;
done:
	if (*result == NULL && (status == 9 || status == 10 || status == 11 || status == 15 ||
				status == 16 || status == 68))
		MlnImpl_81043A60(1, result);
	if (*response != NULL) {
		ret = 0;
		goto out;
	}
	ret = MlnImpl_810273C4(MLN_HEAP_GENERAL, command, status, args, response);
	if (ret != 0)
		ret = MlnImpl_8102C64C(ret);
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	return ret;
}
