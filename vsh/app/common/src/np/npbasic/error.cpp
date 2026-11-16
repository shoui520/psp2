/*
 * SceNpBasic: the error codes of the NP service as the library's own.
 */
#include "npbasic_internal.h"

int npBasicConvertError(int error)
{
	unsigned int code = (unsigned int)error;

	if (code >= 0x80551B00 && code < 0x80551C00)
		return error;
	if (code >= 0x80551C00 && code < 0x80551D00) {
		if (code == 0x80551C06)
			return NP_BASIC_ERROR_SIGNED_OUT;
		return error;
	}
	if (code >= 0x80551A00 && code < 0x80551B00) {
		if (error > (int)0x80551A07 && error <= (int)0x80551A14) {
			switch (code - 0x80551A08) {
			case 0:
				return NP_BASIC_ERROR_SIGNED_OUT;
			case 1:
				return NP_BASIC_ERROR_NOT_ONLINE;
			case 7:
				return NP_BASIC_ERROR_DATA_NOT_FOUND;
			case 2: case 3: case 4: case 5: case 6:
			case 8: case 9: case 10: case 11:
				return error;
			case 12:
				return NP_BASIC_ERROR_NO_NP_COMM_ID_IN_PARAM_SFO;
			}
		}
		return error;
	}
	if (code >= 0x80551500 && code < 0x80551600) {
		if (code == 0x80551514)
			return NP_BASIC_ERROR_SIGNED_OUT;
		return error;
	}
	if (code >= 0x80551400 && code < 0x80551500)
		return error;
	if (code >= 0x80550A00 && code < 0x80550B00)
		return error;
	if (code >= 0x80558B00 && code < 0x80558C00 && code == 0x80558B0A)
		return NP_BASIC_ERROR_DATA_NOT_FOUND;
	return error;
}
